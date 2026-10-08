#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"

namespace
{
double Cross(FVector2D A,FVector2D B){return A.X*B.Y-A.Y*B.X;}
bool OnSegment(FVector2D P,FVector2D A,FVector2D B)
{
    const FVector2D D=B-A;const double T=FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),UE_DOUBLE_SMALL_NUMBER);
    return T>=-1.e-7&&T<=1+1.e-7&&FVector2D::Distance(P,A+D*FMath::Clamp(T,0.,1.))<.01;
}
// Open AABB intersection: boundary contacts are legal walking links.
bool IntersectsPlot(FVector2D A,FVector2D B,FVector2D Center,double Radius)
{
    double Low=0,High=1;const FVector2D D=B-A;
    for(int Axis=0;Axis<2;++Axis)
    {
        const double P=Axis?A.Y:A.X,V=Axis?D.Y:D.X,C=Axis?Center.Y:Center.X;
        if(FMath::Abs(V)<1.e-9){if(P<=C-Radius+1.e-5||P>=C+Radius-1.e-5)return false;continue;}
        double T0=(C-Radius-P)/V,T1=(C+Radius-P)/V;if(T0>T1)Swap(T0,T1);
        Low=FMath::Max(Low,T0);High=FMath::Min(High,T1);if(High-Low<=1.e-8)return false;
    }
    return High>1.e-8&&Low<1-1.e-8;
}
}

bool FSeigeSimulation::LoadTransport(const TSharedPtr<FJsonObject>& Document,FString& Error)
{
    const TSharedPtr<FJsonObject>* O=nullptr;
    if(!Document->TryGetObjectField(TEXT("transport"),O)||!O||!O->IsValid()){Error=TEXT("Missing transport definition");return false;}
    Transport=*O;
    for(const TCHAR* Key:{TEXT("meters_per_world_unit"),TEXT("worker_walk_kmh"),TEXT("access_clearance"),TEXT("path_clearance"),TEXT("snap_distance"),TEXT("minimum_segment_meters"),TEXT("maximum_segment_meters"),TEXT("max_segments")})
    {double N=0;if(!Transport->TryGetNumberField(Key,N)||!FMath::IsFinite(N)||N<=0){Error=FString(TEXT("Invalid transport field: "))+Key;return false;}}
    FString RepairResource;if(!Transport->TryGetStringField(TEXT("repair_resource"),RepairResource)||!Resources.Contains(RepairResource)||Resources[RepairResource].Discrete){Error=TEXT("Road repair requires a continuous physical material");return false;}
    for(const TCHAR* Key:{TEXT("repair_service_range_meters"),TEXT("repair_health_per_second"),TEXT("repair_health_per_unit"),TEXT("repair_energy_kwh_per_health"),TEXT("repair_buffer_units")})
    {double N=0;if(!Transport->TryGetNumberField(Key,N)||!FMath::IsFinite(N)||N<=0){Error=FString(TEXT("Invalid road repair field: "))+Key;return false;}}
    double Unpowered=0;if(!Transport->TryGetNumberField(TEXT("unpowered_speed_multiplier"),Unpowered)||!FMath::IsFinite(Unpowered)||Unpowered<1){Error=TEXT("Invalid unpowered road speed");return false;}
    double Priority=0;if(!Transport->TryGetNumberField(TEXT("construction_staffing_priority"),Priority)||!FMath::IsFinite(Priority)||Priority<0||Priority>1000||Priority!=FMath::FloorToDouble(Priority)){Error=TEXT("Invalid road construction staffing priority");return false;}
    const double Max=Transport->GetNumberField(TEXT("max_segments"));
    if(Max!=FMath::FloorToDouble(Max)||Max>512||Transport->GetNumberField(TEXT("maximum_segment_meters"))<Transport->GetNumberField(TEXT("minimum_segment_meters"))||Transport->GetNumberField(TEXT("access_clearance"))<=Transport->GetNumberField(TEXT("path_clearance")))
    {Error=TEXT("Inconsistent transport limits");return false;}
    const TArray<TSharedPtr<FJsonValue>>* Tiers=nullptr;
    if(!Transport->TryGetArrayField(TEXT("tiers"),Tiers)||Tiers->IsEmpty()){Error=TEXT("Transport tiers missing");return false;}
    for(const auto& V:*Tiers)
    {
        const auto D=V->AsObject();FSeigeTransportTier T;const TSharedPtr<FJsonObject>* Costs=nullptr;double CrewCount=0;
        if(!D||!D->TryGetStringField(TEXT("id"),T.Id)||T.Id.IsEmpty()||TransportTiers.Contains(T.Id)||!D->TryGetStringField(TEXT("name"),T.Name)||T.Name.IsEmpty()||!D->TryGetStringField(TEXT("next_tier"),T.NextTier)||!D->TryGetNumberField(TEXT("speed_multiplier"),T.SpeedMultiplier)||!D->TryGetNumberField(TEXT("width_meters"),T.WidthMeters)||!D->TryGetNumberField(TEXT("construction_seconds_per_100_meters"),T.ConstructionSecondsPer100Meters)||!D->TryGetNumberField(TEXT("construction_workers"),CrewCount)||!D->TryGetObjectField(TEXT("cost_per_100_meters"),Costs))
        {Error=TEXT("Invalid transport tier");return false;}
        if(!D->TryGetStringField(TEXT("visual"),T.Visual)||(T.Visual!=TEXT("road")&&T.Visual!=TEXT("rail")&&T.Visual!=TEXT("vacuum"))){Error=TEXT("Invalid transport visual");return false;}
        if(!D->TryGetNumberField(TEXT("minimum_construction_seconds"),T.MinimumConstructionSeconds)||!FMath::IsFinite(T.MinimumConstructionSeconds)||T.MinimumConstructionSeconds<=0||!FMath::IsFinite(T.SpeedMultiplier)||T.SpeedMultiplier<1||!FMath::IsFinite(T.WidthMeters)||T.WidthMeters<=0||!FMath::IsFinite(T.ConstructionSecondsPer100Meters)||T.ConstructionSecondsPer100Meters<=0||CrewCount<1||CrewCount>128||CrewCount!=FMath::FloorToDouble(CrewCount)){Error=TEXT("Invalid transport tier ranges");return false;}
        if(!D->TryGetNumberField(TEXT("idle_kw_per_100_meters"),T.IdleKWPer100Meters)||!FMath::IsFinite(T.IdleKWPer100Meters)||T.IdleKWPer100Meters<0){Error=TEXT("Invalid road passive energy demand");return false;}
        if(!D->TryGetNumberField(TEXT("health_per_meter"),T.HealthPerMeter)||!FMath::IsFinite(T.HealthPerMeter)||T.HealthPerMeter<=0){Error=TEXT("Invalid transport durability");return false;}
        T.ConstructionWorkers=int32(CrewCount);double Total=0;
        for(const auto& P:(*Costs)->Values){const FString Id(P.Key);double N=0;if(!Resources.Contains(Id)||!P.Value->TryGetNumber(N)||!FMath::IsFinite(N)||N<0){Error=TEXT("Invalid road cost");return false;}T.CostPer100Meters.Add(Id,N);Total+=N;}
        if(Total<=0){Error=TEXT("Road tier must consume materials");return false;}TransportTiers.Add(T.Id,T);
    }
    FString Initial;
    if(!Transport->TryGetStringField(TEXT("initial_tier"),Initial)||!TransportTiers.Contains(Initial)){Error=TEXT("Unknown initial road tier");return false;}
    TSet<FString> Seen;FString Id=Initial;
    while(!Id.IsEmpty())
    {
        if(Seen.Contains(Id)||!TransportTiers.Contains(Id)){Error=TEXT("Road tier chain is cyclic or missing");return false;}
        Seen.Add(Id);const auto& Tier=TransportTiers[Id];
        if(!Tier.NextTier.IsEmpty()&&(!TransportTiers.Contains(Tier.NextTier)||TransportTiers[Tier.NextTier].SpeedMultiplier<=Tier.SpeedMultiplier||TransportTiers[Tier.NextTier].WidthMeters<Tier.WidthMeters)){Error=TEXT("Road upgrade must retain width and increase speed");return false;}
        Id=Tier.NextTier;
    }
    if(Seen.Num()!=TransportTiers.Num()){Error=TEXT("Unreachable transport tier");return false;}
    return true;
}
double FSeigeSimulation::WalkingSpeed() const{return Transport?Transport->GetNumberField(TEXT("worker_walk_kmh"))/3.6/MetersPerWorldUnit():0;}
double FSeigeSimulation::MetersPerWorldUnit() const{return Transport?Transport->GetNumberField(TEXT("meters_per_world_unit")):0;}
FVector2D FSeigeSimulation::BuildingAccessPoint(const FSeigeBuilding& B) const
{FVector2D WallPort;if(Walls.AccessPoint(*this,B.Id,WallPort))return WallPort;const auto* D=Definition(B);return D?B.Position+D->AccessPort*(D->ReservedFootprint+Transport->GetNumberField(TEXT("access_clearance"))):B.Position;}
FVector2D FSeigeSimulation::RoadAccessPoint(const FSeigeTransportSegment& R) const{return (R.A+R.B)*.5;}
FSeigeTransportSegment* FSeigeSimulation::FindRoad(int32 Id){return Roads.FindByPredicate([Id](const auto& R){return R.Id==Id;});}
const FSeigeTransportSegment* FSeigeSimulation::FindRoad(int32 Id) const{return Roads.FindByPredicate([Id](const auto& R){return R.Id==Id;});}
TMap<FString,double> FSeigeSimulation::RoadCost(FVector2D A,FVector2D B,const FString& Tier) const
{TMap<FString,double> Cost;if(const auto* D=TransportTiers.Find(Tier))for(const auto& P:D->CostPer100Meters)Cost.Add(P.Key,P.Value*FVector2D::Distance(A,B)*MetersPerWorldUnit()/100.);return Cost;}
FString FSeigeSimulation::InitialRoadTier() const{return Transport?Transport->GetStringField(TEXT("initial_tier")):FString();}
double FSeigeSimulation::MinimumRoadLength() const{return Transport?Transport->GetNumberField(TEXT("minimum_segment_meters"))/MetersPerWorldUnit():0;}
double FSeigeSimulation::RoadConstructionSeconds(const FSeigeTransportSegment& R) const
{const auto& D=TransportTiers[R.TargetTier];return FMath::Max(D.MinimumConstructionSeconds,D.ConstructionSecondsPer100Meters*FVector2D::Distance(R.A,R.B)*MetersPerWorldUnit()/100.);}
FVector2D FSeigeSimulation::SnapRoadPoint(FVector2D P) const
{
    double Best=Transport->GetNumberField(TEXT("snap_distance"));FVector2D Result=P;
    auto Check=[&](FVector2D Candidate){const double Distance=FVector2D::Distance(P,Candidate);if(Distance<Best){Best=Distance;Result=Candidate;}};
    for(const auto& B:Buildings)if(B.Health>0)Check(BuildingAccessPoint(B));
    for(const auto& R:Roads)if(R.Health>0){Check(R.A);Check(R.B);}return Result;
}
bool FSeigeSimulation::ClearWalkingLine(FVector2D A,FVector2D B,double Clearance) const
{if(!Environment.SegmentDry(A,B,Clearance))return false;for(const auto& Building:Buildings)if(Building.Health>0&&IntersectsPlot(A,B,Building.Position,Definition(Building)->ReservedFootprint+Clearance))return false;return true;}
double FSeigeSimulation::RouteSpeedMultiplier(FVector2D A,FVector2D B) const
{double Speed=1;for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(A,R.A,R.B)&&OnSegment(B,R.A,R.B))Speed=FMath::Max(Speed,Energy.RoadPowered(R.Id)?TransportTiers[R.Tier].SpeedMultiplier:Transport->GetNumberField(TEXT("unpowered_speed_multiplier")));return Speed;}
int32 FSeigeSimulation::CourierRoadId(const FSeigeCourier& C) const
{if(!C.Route.IsValidIndex(C.NextWaypoint))return 0;for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(C.Position,R.A,R.B)&&OnSegment(C.Route[C.NextWaypoint],R.A,R.B))return R.Id;return 0;}
bool FSeigeSimulation::FindRoadRoute(FVector2D From,FVector2D To,TArray<FVector2D>& Route,const FSeigeBuilding* ProspectivePlot) const
{double Width=0;for(const auto& P:TransportTiers)Width=FMath::Max(Width,P.Value.WidthMeters*.5/MetersPerWorldUnit());return FindRoute(From,To,Route,Width+Transport->GetNumberField(TEXT("path_clearance")),true,ProspectivePlot);}
bool FSeigeSimulation::ClearRoadLine(FVector2D From,FVector2D To,const FSeigeBuilding* ProspectivePlot) const
{
    double Clearance=0;for(const auto& P:TransportTiers)Clearance=FMath::Max(Clearance,P.Value.WidthMeters*.5/MetersPerWorldUnit());Clearance+=Transport->GetNumberField(TEXT("path_clearance"));
    const auto* Def=ProspectivePlot?Definition(*ProspectivePlot):nullptr;
    return ClearWalkingLine(From,To,Clearance)&&(!ProspectivePlot||(Def&&!IntersectsPlot(From,To,ProspectivePlot->Position,Def->ReservedFootprint+Clearance)));
}
bool FSeigeSimulation::FindRoute(FVector2D From,FVector2D To,TArray<FVector2D>& Route,double ClearanceOverride,bool RoadPlan,const FSeigeBuilding* ProspectivePlot) const
{
    Route.Empty();if(!Environment.CanStand(From)||!Environment.CanStand(To))return false;if(FVector2D::Distance(From,To)<.001){Route.Add(To);return true;}
    const double Clearance=ClearanceOverride>=0?ClearanceOverride:Transport->GetNumberField(TEXT("path_clearance"));
    const auto* ProspectiveDef=ProspectivePlot?Definition(*ProspectivePlot):nullptr;
    if(ProspectivePlot&&!ProspectiveDef)return false;
    auto ClearLine=[&](FVector2D A,FVector2D B)
    {return ClearWalkingLine(A,B,Clearance)&&(!ProspectivePlot||!IntersectsPlot(A,B,ProspectivePlot->Position,ProspectiveDef->ReservedFootprint+Clearance));};
    const double MinimumRoad=Transport->GetNumberField(TEXT("minimum_segment_meters"))/MetersPerWorldUnit(),MaximumRoad=Transport->GetNumberField(TEXT("maximum_segment_meters"))/MetersPerWorldUnit();
    auto ExistingRoad=[&](FVector2D A,FVector2D B){for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(A,R.A,R.B)&&OnSegment(B,R.A,R.B))return true;return false;};
    if(Roads.IsEmpty()&&!RoadPlan&&ClearLine(From,To)){Route.Add(To);return true;}
    FString Topology=Environment.Fingerprint+FString::Printf(TEXT(":%.6f:%.6f;"),Environment.WorldOffset.X,Environment.WorldOffset.Y);for(const auto& B:Buildings)if(B.Health>0)Topology+=FString::Printf(TEXT("b%d:%.6f:%.6f:%.6f;"),B.Id,B.Position.X,B.Position.Y,Definition(B)->ReservedFootprint);
    for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty())Topology+=FString::Printf(TEXT("r%d:%s:%.6f:%.6f:%.6f:%.6f;"),R.Id,*R.Tier,R.A.X,R.A.Y,R.B.X,R.B.Y);
    if(Topology!=CachedRouteTopology){CachedRouteTopology=Topology;CachedRoutes.Empty();}
    FString Key=FString::Printf(TEXT("%.9f:%.9f:%.9f:%.9f:%.6f:%d"),From.X,From.Y,To.X,To.Y,Clearance,RoadPlan?1:0);
    // Preview a reserved plot without changing live buildings, IDs or stock.
    // Its geometry is part of the query key, never a cached result for the live topology.
    if(ProspectivePlot)Key+=FString::Printf(TEXT(":preview:%.9f:%.9f:%.6f"),ProspectivePlot->Position.X,ProspectivePlot->Position.Y,ProspectiveDef->ReservedFootprint);
    if(const auto* Existing=CachedRoutes.Find(Key)){Route=*Existing;return !Route.IsEmpty();}
    TArray<FVector2D> Points={From,To};
    auto Add=[&](FVector2D P){for(const auto& Existing:Points)if(FVector2D::Distance(P,Existing)<.01)return;Points.Add(P);};
    // Geography waypoints are shared by workers, road plans and vehicles. Keep
    // the graph bounded to this colony; water never becomes a hidden shortcut.
    if(!Environment.SegmentDry(From,To,Clearance))for(FVector2D P:Environment.RoutingWaypoints(Clearance))
        if(FMath::Abs(P.X)+Clearance<WorldHalfSize&&FMath::Abs(P.Y)+Clearance<WorldHalfSize)Add(P);
    for(const auto& B:Buildings)if(B.Health>0)
    {const double R=Definition(B)->ReservedFootprint+Clearance;for(int X:{-1,1})for(int Y:{-1,1})Add(B.Position+FVector2D(X*R,Y*R));}
    if(ProspectivePlot){const double R=ProspectiveDef->ReservedFootprint+Clearance;for(int X:{-1,1})for(int Y:{-1,1})Add(ProspectivePlot->Position+FVector2D(X*R,Y*R));}
    for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()){Add(R.A);Add(R.B);}
    // Crossing segments share an actual junction; every junction splits both weighted edges.
    for(int I=0;I<Roads.Num();++I)if(Roads[I].Health>0&&!Roads[I].Tier.IsEmpty())for(int J=I+1;J<Roads.Num();++J)if(Roads[J].Health>0&&!Roads[J].Tier.IsEmpty())
    {const auto& A=Roads[I];const auto& B=Roads[J];const FVector2D U=A.B-A.A,V=B.B-B.A;const double Den=Cross(U,V);if(FMath::Abs(Den)>1.e-8){const double T=Cross(B.A-A.A,V)/Den,S=Cross(B.A-A.A,U)/Den;if(T>=0&&T<=1&&S>=0&&S<=1)Add(A.A+U*T);}}
    TArray<double> Distance;Distance.Init(TNumericLimits<double>::Max(),Points.Num());Distance[0]=0;
    TArray<int32> Previous;Previous.Init(INDEX_NONE,Points.Num());TArray<bool> Visited;Visited.Init(false,Points.Num());
    for(int Iteration=0;Iteration<Points.Num();++Iteration)
    {
        int Current=INDEX_NONE;double Best=TNumericLimits<double>::Max();for(int I=0;I<Points.Num();++I)if(!Visited[I]&&Distance[I]<Best){Current=I;Best=Distance[I];}
        if(Current==INDEX_NONE)break;if(Current==1)break;Visited[Current]=true;
        for(int Other=0;Other<Points.Num();++Other)if(!Visited[Other]&&Other!=Current&&ClearLine(Points[Current],Points[Other]))
        {const double Length=FVector2D::Distance(Points[Current],Points[Other]);
            if(RoadPlan&&Length+1.e-8<MinimumRoad&&!ExistingRoad(Points[Current],Points[Other]))continue;
            const double Cost=Length/RouteSpeedMultiplier(Points[Current],Points[Other]);if(Best+Cost<Distance[Other]){Distance[Other]=Best+Cost;Previous[Other]=Current;}}
    }
    if(Previous[1]==INDEX_NONE)return false;TArray<FVector2D> Reverse;for(int I=1;I!=0;I=Previous[I]){if(I==INDEX_NONE)return false;Reverse.Add(Points[I]);}
    FVector2D Last=From;
    for(int I=Reverse.Num()-1;I>=0;--I){const FVector2D End=Reverse[I];const int Parts=RoadPlan&&!ExistingRoad(Last,End)?FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Last,End)/MaximumRoad)):1;for(int J=1;J<=Parts;++J)Route.Add(FMath::Lerp(Last,End,double(J)/Parts));Last=End;}
    if(CachedRoutes.Num()>4096)CachedRoutes.Empty();CachedRoutes.Add(Key,Route);return true;
}
bool FSeigeSimulation::WalkRoute(FVector2D& Position,const TArray<FVector2D>& Route,int32& Next,double Seconds) const
{
    while(Route.IsValidIndex(Next)&&Seconds>UE_DOUBLE_SMALL_NUMBER)
    {const FVector2D End=Route[Next];const double Distance=FVector2D::Distance(Position,End),Speed=WalkingSpeed()*RouteSpeedMultiplier(Position,End);if(Distance<=Speed*Seconds+.000001){Position=End;Seconds=FMath::Max(0.,Seconds-Distance/Speed);++Next;}else{Position+=(End-Position)/Distance*Speed*Seconds;Seconds=0;}}
    return Next>=Route.Num()&&!Route.IsEmpty();
}
bool FSeigeSimulation::CanPlaceRoad(FVector2D A,FVector2D B,FString& Error,bool CheckMaterials) const
{
    const auto* C=Core();if(!Transport||!C||C->Health<=0||C->IsConstructing||Escaped||Failed){Error=TEXT("An operating command core is required");return false;}
    for(FVector2D P:{A,B})if(Number(TEXT("placement_requires_visibility"))>0&&!IsVisible(P)){Error=TEXT("Road endpoints require live coverage inside the sector");return false;}
    if(!CanPlaceRoadGeometry(A,B,Error))return false;
    if(CheckMaterials)for(const auto& P:RoadCost(A,B,InitialRoadTier()))if(ConstructionAvailable(P.Key)+UE_DOUBLE_SMALL_NUMBER<P.Value){Error=TEXT("Insufficient unreserved road construction materials");return false;}
    Error.Empty();return true;
}
bool FSeigeSimulation::CanPlaceRoadGeometry(FVector2D A,FVector2D B,FString& Error) const
{
    const auto* C=Core();if(!Transport||!C){Error=TEXT("Road requires a command core");return false;}
    const auto& Tier=TransportTiers[Transport->GetStringField(TEXT("initial_tier"))];const double Length=FVector2D::Distance(A,B)*MetersPerWorldUnit(),HalfWidth=Tier.WidthMeters*.5/MetersPerWorldUnit();
    if(!FMath::IsFinite(A.X)||!FMath::IsFinite(A.Y)||!FMath::IsFinite(B.X)||!FMath::IsFinite(B.Y)||Length<Transport->GetNumberField(TEXT("minimum_segment_meters"))||Length>Transport->GetNumberField(TEXT("maximum_segment_meters"))){Error=TEXT("Road length is outside the allowed range");return false;}
    if(Roads.Num()>=Transport->GetNumberField(TEXT("max_segments"))){Error=TEXT("Road segment limit reached");return false;}
    for(FVector2D P:{A,B})if(FMath::Abs(P.X)+HalfWidth>WorldHalfSize||FMath::Abs(P.Y)+HalfWidth>WorldHalfSize){Error=TEXT("Road endpoints must fit inside the sector");return false;}
    if(!Environment.SegmentDry(A,B,HalfWidth)){Error=TEXT("Road requires dry land; bridges are not available");return false;}
    if(!ClearWalkingLine(A,B,HalfWidth)){Error=TEXT("Road intersects a reserved building plot");return false;}
    for(const auto& R:Roads)if(R.Health>0)
    {
        const FVector2D Direction=(B-A).GetSafeNormal();
        if(FMath::Abs(Cross(R.A-A,Direction))<.01&&FMath::Abs(Cross(R.B-A,Direction))<.01)
        {double Lo=FVector2D::DotProduct(R.A-A,Direction),Hi=FVector2D::DotProduct(R.B-A,Direction);if(Lo>Hi)Swap(Lo,Hi);if(FMath::Min(Hi,FVector2D::Distance(A,B))-FMath::Max(0.,Lo)>.01){Error=TEXT("Upgrade the existing road instead of overlapping it");return false;}}
    }
    TArray<FVector2D> Path;if(!FindRoute(BuildingAccessPoint(*C),(A+B)*.5,Path)){Error=TEXT("Workers cannot reach this road site");return false;}
    Error.Empty();return true;
}
bool FSeigeSimulation::PlaceRoad(FVector2D A,FVector2D B,FString& Error)
{if(!CanPlaceRoad(A,B,Error))return false;FSeigeTransportSegment R;R.Id=NextId++;R.A=A;R.B=B;R.TargetTier=Transport->GetStringField(TEXT("initial_tier"));R.MaxHealth=FVector2D::Distance(A,B)*MetersPerWorldUnit()*TransportTiers[R.TargetTier].HealthPerMeter;R.Health=R.MaxHealth;R.BuilderPosition=BuildingAccessPoint(*Core());FindRoute(R.BuilderPosition,RoadAccessPoint(R),R.BuilderRoute);Roads.Add(R);AllocateWorkers();AddEvent(TEXT("Road construction queued"));return true;}
bool FSeigeSimulation::CanUpgradeRoad(int32 Id,FString& Error) const
{
    const auto* R=FindRoad(Id);if(!R||R->Health<=0||R->IsConstructing||R->Tier.IsEmpty()||Escaped||Failed){Error=TEXT("Select a completed road to upgrade");return false;}
    const FString Next=TransportTiers[R->Tier].NextTier;if(Next.IsEmpty()){Error=TEXT("Transport corridor is already fully upgraded");return false;}
    if(!ClearWalkingLine(R->A,R->B,TransportTiers[Next].WidthMeters*.5/MetersPerWorldUnit())){Error=TEXT("Upgraded corridor intersects a reserved plot");return false;}
    for(const auto& P:RoadCost(R->A,R->B,Next))if(ConstructionAvailable(P.Key)+UE_DOUBLE_SMALL_NUMBER<P.Value){Error=TEXT("Insufficient unreserved upgrade materials");return false;}
    Error.Empty();return true;
}
bool FSeigeSimulation::UpgradeRoad(int32 Id,FString& Error)
{if(!CanUpgradeRoad(Id,Error))return false;auto& R=*FindRoad(Id);for(const auto& P:R.InstalledMaterials)R.PreviousTierMaterials.FindOrAdd(P.Key)+=P.Value;R.TargetTier=TransportTiers[R.Tier].NextTier;R.IsConstructing=true;R.ConstructionProgress=0;R.BuildersOnSite=R.TravellingBuilders=0;R.InstalledMaterials.Empty();R.ConstructionMaterials.Empty();R.BuilderPosition=BuildingAccessPoint(*Core());R.BuilderNextWaypoint=0;FindRoute(R.BuilderPosition,RoadAccessPoint(R),R.BuilderRoute);AllocateWorkers();AddEvent(TEXT("Transport corridor upgrade queued; existing service remains open"));return true;}
double FSeigeSimulation::IncomingRoad(int32 Id,const FString& Resource) const
{double Result=0;for(const auto& C:Couriers)if(C.RoadTargetId==Id&&(Resource.IsEmpty()||C.Resource==Resource))Result+=C.Amount+C.ReservedAmount;return Result;}

namespace
{
double RoadServiceDistance(FVector2D P,const FSeigeTransportSegment& R)
{const FVector2D V=R.B-R.A;return FVector2D::Distance(P,R.A+V*FMath::Clamp(FVector2D::DotProduct(P-R.A,V)/FMath::Max(V.SizeSquared(),1.e-12),0.,1.));}
}
double FSeigeSimulation::RoadRepairDemand(const FSeigeBuilding& B,const FString& Resource) const
{
    if(!Transport||Resource!=Transport->GetStringField(TEXT("repair_resource"))||B.Health<=0||B.IsConstructing||!B.Enabled||B.Workers<=0)return 0;
    for(const auto& R:Roads)if(R.Health>0&&R.Health<R.MaxHealth&&!R.IsConstructing&&Energy.RoadConnectedToBuilding(R.Id,B.Id)&&RoadServiceDistance(BuildingAccessPoint(B),R)*MetersPerWorldUnit()<=Transport->GetNumberField(TEXT("repair_service_range_meters")))return Transport->GetNumberField(TEXT("repair_buffer_units"));
    return 0;
}
void FSeigeSimulation::StepRoadRepairs(double Seconds)
{
    const FString Material=Transport->GetStringField(TEXT("repair_resource"));const double Range=Transport->GetNumberField(TEXT("repair_service_range_meters")),Rate=Transport->GetNumberField(TEXT("repair_health_per_second")),PerUnit=Transport->GetNumberField(TEXT("repair_health_per_unit")),EnergyPerHealth=Transport->GetNumberField(TEXT("repair_energy_kwh_per_health"));
    for(auto& R:Roads)
    {
        if(R.Health<=0||R.IsConstructing||R.Health>=R.MaxHealth)continue;
        TArray<int32> Services;for(const auto& B:Buildings)if(B.Workers>0&&WorkFraction(B)>0&&Energy.RoadConnectedToBuilding(R.Id,B.Id)&&RoadServiceDistance(BuildingAccessPoint(B),R)*MetersPerWorldUnit()<=Range)Services.Add(B.Id);
        Services.Sort([&](int32 A,int32 B){const double DA=RoadServiceDistance(BuildingAccessPoint(*FindBuilding(A)),R),DB=RoadServiceDistance(BuildingAccessPoint(*FindBuilding(B)),R);return DA==DB?A<B:DA<DB;});
        for(int32 Id:Services)
        {
            auto& B=*FindBuilding(Id);const double Repair=FMath::Min(FMath::Min3(R.MaxHealth-R.Health,Rate*Seconds*WorkFraction(B),Spendable(B,Material)*PerUnit),Energy.Info(*this,B.Id).StoredKWh/EnergyPerHealth);
            if(Repair<=1.e-9||!Energy.Consume(*this,B.Id,Repair*EnergyPerHealth))continue;
            B.Inventory.FindOrAdd(Material)=FMath::Max(0.,B.Inventory.FindRef(Material)-Repair/PerUnit);R.Health=FMath::Min(R.MaxHealth,R.Health+Repair);break;
        }
    }
}
