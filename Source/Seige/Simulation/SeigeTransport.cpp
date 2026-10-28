#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"

namespace
{
FString RouteEdgeKey(const FString& Context,FVector2D A,FVector2D B)
{return Context+FString::Printf(TEXT(":%.17g:%.17g:%.17g:%.17g"),A.X,A.Y,B.X,B.Y);}
double Cross(FVector2D A,FVector2D B){return A.X*B.Y-A.Y*B.X;}
FVector2D ProjectToSegment(FVector2D P,FVector2D A,FVector2D B)
{const FVector2D D=B-A;return A+D*FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),UE_DOUBLE_SMALL_NUMBER),0.,1.);}
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
    bool Enabled=false;if(!Transport->TryGetBoolField(TEXT("roads_enabled"),Enabled)){Error=TEXT("Transport roads_enabled must be boolean");return false;}
    if(Transport->HasField(TEXT("worker_walk_kmh"))){Error=TEXT("Worker speed belongs only in mobility.drone.speed_kmh");return false;}
    for(const TCHAR* Key:{TEXT("meters_per_world_unit"),TEXT("access_clearance"),TEXT("path_clearance"),TEXT("snap_distance"),TEXT("minimum_segment_meters"),TEXT("maximum_segment_meters"),TEXT("max_segments")})
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
bool FSeigeSimulation::RoadsEnabled() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if(RoadsEnabledOverride>=0)return RoadsEnabledOverride!=0;
#endif
    return Transport&&Transport->GetBoolField(TEXT("roads_enabled"));
}
double FSeigeSimulation::WalkingSpeed() const{const auto* P=Mobility.Profile(TEXT("drone"));return P?P->SpeedKmh/3.6/MetersPerWorldUnit()*Companions.WorkerSpeedMultiplier(*this):0;}
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
    for(const auto& R:Roads)if(R.Health>0){Check(R.A);Check(R.B);Check(ProjectToSegment(P,R.A,R.B));}return Result;
}
bool FSeigeSimulation::ClearWalkingLine(FVector2D A,FVector2D B,double Clearance) const
{if(!Environment.SegmentTraversable(A,B,Clearance))return false;for(const auto& Building:Buildings)if(Building.Health>0&&IntersectsPlot(A,B,Building.Position,Definition(Building)->ReservedFootprint+Clearance))return false;return true;}
bool FSeigeSimulation::HasRoadLink(FVector2D A,FVector2D B) const
{if(!RoadsEnabled())return false;for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(A,R.A,R.B)&&OnSegment(B,R.A,R.B))return true;return false;}
double FSeigeSimulation::RouteSpeedMultiplier(FVector2D A,FVector2D B) const
{if(!RoadsEnabled())return 1;double Speed=1;for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(A,R.A,R.B)&&OnSegment(B,R.A,R.B))Speed=FMath::Max(Speed,Energy.RoadPowered(R.Id)?TransportTiers[R.Tier].SpeedMultiplier:Transport->GetNumberField(TEXT("unpowered_speed_multiplier")));return Speed;}
int32 FSeigeSimulation::CourierRoadId(const FSeigeCourier& C) const
{if(!RoadsEnabled()||!C.Route.IsValidIndex(C.NextWaypoint))return 0;for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(C.Position,R.A,R.B)&&OnSegment(C.Route[C.NextWaypoint],R.A,R.B))return R.Id;return 0;}
bool FSeigeSimulation::FindRoadRoute(FVector2D From,FVector2D To,TArray<FVector2D>& Route,const FSeigeBuilding* ProspectivePlot) const
{if(!RoadsEnabled()){Route.Empty();return false;}double Width=0;for(const auto& P:TransportTiers)Width=FMath::Max(Width,P.Value.WidthMeters*.5/MetersPerWorldUnit());return FindRoute(From,To,Route,Width+Transport->GetNumberField(TEXT("path_clearance")),true,ProspectivePlot);}
bool FSeigeSimulation::ClearRoadLine(FVector2D From,FVector2D To,const FSeigeBuilding* ProspectivePlot) const
{
    double Clearance=0;for(const auto& P:TransportTiers)Clearance=FMath::Max(Clearance,P.Value.WidthMeters*.5/MetersPerWorldUnit());Clearance+=Transport->GetNumberField(TEXT("path_clearance"));
    const auto* Def=ProspectivePlot?Definition(*ProspectivePlot):nullptr;
    return Environment.SegmentDry(From,To,Clearance)&&ClearWalkingLine(From,To,Clearance)&&(!ProspectivePlot||(Def&&!IntersectsPlot(From,To,ProspectivePlot->Position,Def->ReservedFootprint+Clearance)));
}
bool FSeigeSimulation::FindMobilityRoute(const FString& Profile,FVector2D From,FVector2D To,TArray<FVector2D>& Route,double AdditionalRadiusMeters) const
{const auto* P=Mobility.Profile(Profile);if(!P){Route.Empty();return false;}return FindRoute(From,To,Route,(FMath::Max(P->BodyRadiusMeters,AdditionalRadiusMeters)+P->ClearanceMeters)/MetersPerWorldUnit(),false,nullptr,Profile);}
void FSeigeSimulation::RejectMobilityRouteEdge(const FString& Profile,FVector2D From,FVector2D To,double AdditionalRadiusMeters) const
{
    const auto* P=Mobility.Profile(Profile);if(!P)return;
    const double Clearance=(FMath::Max(P->BodyRadiusMeters,AdditionalRadiusMeters)+P->ClearanceMeters)/MetersPerWorldUnit();
    const FString Context=Profile+FString::Printf(TEXT(":%.9f:%d"),Clearance,0);
    // A physical step is authoritative evidence that this candidate cannot be
    // used. Discard successful paths that may contain it, but retain all other
    // expensive edge checks. Geometry/terrain invalidation clears this failure.
    CachedRoutes.Empty();
    // Dry clearance and absolute grade are bidirectional. Retrying this same
    // rejected physical edge backwards cannot repair the terrain evidence.
    for(const FString& Key:{RouteEdgeKey(Context,From,To),RouteEdgeKey(Context,To,From)})if(!RejectedPhysicalRouteEdges.Contains(Key))
    {
        if(RejectedPhysicalRouteOrder.Num()>=4096)
        {RejectedPhysicalRouteEdges.Remove(RejectedPhysicalRouteOrder[0]);RejectedPhysicalRouteOrder.RemoveAt(0);}
        RejectedPhysicalRouteEdges.Add(Key);RejectedPhysicalRouteOrder.Add(Key);
    }
}
bool FSeigeSimulation::FindRoute(FVector2D From,FVector2D To,TArray<FVector2D>& Route,double ClearanceOverride,bool RoadPlan,const FSeigeBuilding* ProspectivePlot,const FString& MobilityProfile) const
{
    if(RoadPlan&&!RoadsEnabled()){Route.Empty();return false;}
    const auto* Profile=Mobility.Profile(MobilityProfile);const bool WaterAllowed=!RoadPlan&&Profile&&Profile->AllowWater;
    Route.Empty();if(!WaterAllowed&&(RoadPlan?(!Environment.CanStand(From)||!Environment.CanStand(To)):(!Environment.CanTraverse(From)||!Environment.CanTraverse(To))))return false;if(FVector2D::Distance(From,To)<.001){Route.Add(To);return true;}
    const double Clearance=ClearanceOverride>=0?ClearanceOverride:FMath::Max(Transport->GetNumberField(TEXT("path_clearance")),Mobility.ClearanceMeters(MobilityProfile)/MetersPerWorldUnit());
    const auto* ProspectiveDef=ProspectivePlot?Definition(*ProspectivePlot):nullptr;
    if(ProspectivePlot&&!ProspectiveDef)return false;
    auto ClearLine=[&](FVector2D A,FVector2D B)
    {if(RoadPlan&&!Environment.SegmentDry(A,B,Clearance))return false;if(WaterAllowed){for(const auto& Building:Buildings)if(Building.Health>0&&IntersectsPlot(A,B,Building.Position,Definition(Building)->ReservedFootprint+Clearance))return false;}else if(!ClearWalkingLine(A,B,Clearance))return false;return (RoadPlan||Mobility.MovementAllowed(*this,MobilityProfile,A,B))&&(!ProspectivePlot||!IntersectsPlot(A,B,ProspectivePlot->Position,ProspectiveDef->ReservedFootprint+Clearance));};
    const double MinimumRoad=Transport->GetNumberField(TEXT("minimum_segment_meters"))/MetersPerWorldUnit(),MaximumRoad=Transport->GetNumberField(TEXT("maximum_segment_meters"))/MetersPerWorldUnit();
    auto ExistingRoad=[&](FVector2D A,FVector2D B){if(!RoadsEnabled())return false;for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(A,R.A,R.B)&&OnSegment(B,R.A,R.B))return true;return false;};
    FString Topology=Environment.Fingerprint+Mobility.GetFingerprint()+FString::Printf(TEXT(":%.6f:%.6f:%.6f:%d;"),Environment.WorldOffset.X,Environment.WorldOffset.Y,WorldHalfSize,Mobility.GetTerrainRevision());for(const auto& B:Buildings)if(B.Health>0)Topology+=FString::Printf(TEXT("b%d:%.6f:%.6f:%.6f;"),B.Id,B.Position.X,B.Position.Y,Definition(B)->ReservedFootprint);
    if(RoadsEnabled())for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty())Topology+=FString::Printf(TEXT("r%d:%s:%.6f:%.6f:%.6f:%.6f:%d;"),R.Id,*R.Tier,R.A.X,R.A.Y,R.B.X,R.B.Y,Energy.RoadPowered(R.Id)?1:0);
    if(Topology!=CachedRouteTopology){CachedRouteTopology=Topology;CachedRoutes.Empty();CachedRouteEdges.Empty();RejectedPhysicalRouteEdges.Empty();RejectedPhysicalRouteOrder.Empty();}
    FString Key=Mobility.GetFingerprint()+MobilityProfile+FString::Printf(TEXT("%d:%.9f:%.9f:%.9f:%.9f:%.6f:%d"),Mobility.GetTerrainRevision(),From.X,From.Y,To.X,To.Y,Clearance,RoadPlan?1:0);
    // Preview a reserved plot without changing live buildings, IDs or stock.
    // Its geometry is part of the query key, never a cached result for the live topology.
    if(ProspectivePlot)Key+=FString::Printf(TEXT(":preview:%.9f:%.9f:%.6f"),ProspectivePlot->Position.X,ProspectivePlot->Position.Y,ProspectiveDef->ReservedFootprint);
    if(const auto* Existing=CachedRoutes.Find(Key)){Route=*Existing;return !Route.IsEmpty();}
    auto Remember=[&](bool Success){if(CachedRoutes.Num()>4096)CachedRoutes.Empty();CachedRoutes.Add(Key,Route);return Success;};
    // Complete visibility graphs used to resample every terrain edge for every
    // worker/destination, including identical failed requests each simulation
    // step. Reuse exact directed edges without weakening terrain or plot tests.
    if(CachedRouteEdges.Num()>131072)CachedRouteEdges.Empty();
    FString EdgeContext=MobilityProfile+FString::Printf(TEXT(":%.9f:%d"),Clearance,RoadPlan?1:0);
    if(ProspectivePlot)EdgeContext+=FString::Printf(TEXT(":preview:%.9f:%.9f:%.9f"),ProspectivePlot->Position.X,ProspectivePlot->Position.Y,ProspectiveDef->ReservedFootprint);
    auto EdgeCost=[&](FVector2D A,FVector2D B)
    {
        const FString EdgeKey=RouteEdgeKey(EdgeContext,A,B);
        if(RejectedPhysicalRouteEdges.Contains(EdgeKey))return -1.;
        if(const double* Existing=CachedRouteEdges.Find(EdgeKey))return *Existing;
        const double Length=FVector2D::Distance(A,B);
        const double Cost=ClearLine(A,B)&&(!RoadPlan||Length+1.e-8>=MinimumRoad||ExistingRoad(A,B))?
            Length/(RoadPlan?RouteSpeedMultiplier(A,B):FMath::Max(.0001,Mobility.Speed(*this,MobilityProfile,A,B))):-1.;
        CachedRouteEdges.Add(EdgeKey,Cost);return Cost;
    };
    if((!RoadsEnabled()||Roads.IsEmpty())&&!RoadPlan&&EdgeCost(From,To)>=0){Route.Add(To);return Remember(true);}
    TArray<FVector2D> Points={From,To};
    auto Add=[&](FVector2D P){for(const auto& Existing:Points)if(FVector2D::Distance(P,Existing)<.01)return;Points.Add(P);};
    // Geography waypoints are shared by workers, road plans and vehicles. Keep
    // the graph bounded to this colony; water never becomes a hidden shortcut.
    if(!Environment.SegmentDry(From,To,Clearance)||(!RoadPlan&&!Mobility.MovementAllowed(*this,MobilityProfile,From,To)))for(FVector2D P:Environment.RoutingWaypoints(Clearance))
        if(FMath::Abs(P.X)+Clearance<WorldHalfSize&&FMath::Abs(P.Y)+Clearance<WorldHalfSize)Add(P);
    for(const auto& B:Buildings)if(B.Health>0)
    {const double R=Definition(B)->ReservedFootprint+Clearance;for(int X:{-1,1})for(int Y:{-1,1})Add(B.Position+FVector2D(X*R,Y*R));}
    if(ProspectivePlot){const double R=ProspectiveDef->ReservedFootprint+Clearance;for(int X:{-1,1})for(int Y:{-1,1})Add(ProspectivePlot->Position+FVector2D(X*R,Y*R));}
    if(RoadsEnabled())for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty()){Add(R.A);Add(R.B);}
    // Workstations, charging queues and parked cargo vehicles are offset from
    // the road centreline. Connect their actual positions to road interiors;
    // forcing them to visit a distant endpoint made useful corridors invisible
    // to the cost solver. Every connector still passes ordinary body clearance.
    if(RoadsEnabled()&&!RoadPlan)for(const auto& R:Roads)if(R.Health>0&&!R.Tier.IsEmpty())
    {Add(ProjectToSegment(From,R.A,R.B));Add(ProjectToSegment(To,R.A,R.B));}
    // Crossing segments share an actual junction; every junction splits both weighted edges.
    if(RoadsEnabled())for(int I=0;I<Roads.Num();++I)if(Roads[I].Health>0&&!Roads[I].Tier.IsEmpty())for(int J=I+1;J<Roads.Num();++J)if(Roads[J].Health>0&&!Roads[J].Tier.IsEmpty())
    {const auto& A=Roads[I];const auto& B=Roads[J];const FVector2D U=A.B-A.A,V=B.B-B.A;const double Den=Cross(U,V);if(FMath::Abs(Den)>1.e-8){const double T=Cross(B.A-A.A,V)/Den,S=Cross(B.A-A.A,U)/Den;if(T>=0&&T<=1&&S>=0&&S<=1)Add(A.A+U*T);}}
    TArray<double> Distance;Distance.Init(TNumericLimits<double>::Max(),Points.Num());Distance[0]=0;
    TArray<int32> Previous;Previous.Init(INDEX_NONE,Points.Num());TArray<bool> Visited;Visited.Init(false,Points.Num());
    // Euclidean distance divided by the fastest possible speed is an admissible
    // A* bound. It only avoids edges that cannot improve a valid route; every
    // selected edge still passes the same complete terrain/clearance checks.
    double FastestMultiplier=1.;if(RoadsEnabled())for(const auto& Tier:TransportTiers)FastestMultiplier=FMath::Max(FastestMultiplier,Tier.Value.SpeedMultiplier);
    if(RoadsEnabled())FastestMultiplier=FMath::Max(FastestMultiplier,Transport->GetNumberField(TEXT("unpowered_speed_multiplier")));
    const double FastestSpeed=RoadPlan?FastestMultiplier:FMath::Max(.0001,Profile?Profile->SpeedKmh/3.6*FastestMultiplier:.0001);
    TArray<double> Heuristic;for(const auto& P:Points)Heuristic.Add(FVector2D::Distance(P,To)/FastestSpeed);
    for(int Iteration=0;Iteration<Points.Num();++Iteration)
    {
        int Current=INDEX_NONE;double Priority=TNumericLimits<double>::Max();for(int I=0;I<Points.Num();++I)if(!Visited[I]&&Distance[I]+Heuristic[I]<Priority){Current=I;Priority=Distance[I]+Heuristic[I];}
        if(Current==INDEX_NONE)break;if(Current==1)break;Visited[Current]=true;const double Best=Distance[Current];
        for(int Other=0;Other<Points.Num();++Other)if(!Visited[Other]&&Other!=Current)
        {
            const double Lower=FVector2D::Distance(Points[Current],Points[Other])/FastestSpeed;
            if(Best+Lower>=Distance[Other]||Best+Lower+Heuristic[Other]>=Distance[1])continue;
            const double Cost=EdgeCost(Points[Current],Points[Other]);
            if(Cost>=0&&Best+Cost<Distance[Other]){Distance[Other]=Best+Cost;Previous[Other]=Current;}
        }
    }
    const bool HasGraphRoute=Previous[1]!=INDEX_NONE;
    const double BaseStep=Profile?FMath::Max(Profile->SampleMeters/MetersPerWorldUnit(),Clearance*2):0;
    const double Margin=FMath::Max(BaseStep*8,FVector2D::Distance(From,To)*.5);
    const FVector2D Minimum(FMath::Max(-WorldHalfSize+Clearance,FMath::Min(From.X,To.X)-Margin),FMath::Max(-WorldHalfSize+Clearance,FMath::Min(From.Y,To.Y)-Margin));
    const FVector2D Maximum(FMath::Min(WorldHalfSize-Clearance,FMath::Max(From.X,To.X)+Margin),FMath::Min(WorldHalfSize-Clearance,FMath::Max(From.Y,To.Y)+Margin));
    bool LeavesLocalEnvelope=false;
    if(HasGraphRoute&&!RoadPlan&&Profile)for(int32 At=1;At!=0;At=Previous[At])
    {
        const FVector2D P=Points[At];
        if(P.X<Minimum.X||P.X>Maximum.X||P.Y<Minimum.Y||P.Y>Maximum.Y){LeavesLocalEnvelope=true;break;}
    }
    if(!HasGraphRoute||LeavesLocalEnvelope)
    {
        auto FindContour=[&]()
        {
        if(RoadPlan||!Profile)return false;
        // Building/road visibility corners cannot describe a contour across
        // an otherwise open hillside. A distant geography waypoint can also
        // conceal a nearby contour: compare the bounded local grid when that
        // cheap graph leaves its local envelope, retaining the existing valid
        // route if this search cannot improve it. Physical checks are identical.
        // Exact endpoint links preserve body/cargo positions; no grid snapping
        // is applied to the simulation. Results and edges reuse topology caches.
        constexpr int32 MaximumGridNodes=4096;
        if(Minimum.X>Maximum.X||Minimum.Y>Maximum.Y)return false;
        double Step=BaseStep;int32 Columns=0,Rows=0;
        do{Columns=FMath::FloorToInt((Maximum.X-Minimum.X)/Step)+1;Rows=FMath::FloorToInt((Maximum.Y-Minimum.Y)/Step)+1;if(int64(Columns)*Rows>MaximumGridNodes)Step*=1.25;}while(int64(Columns)*Rows>MaximumGridNodes);
        const int32 Cells=Columns*Rows,Start=Cells,Goal=Cells+1;
        auto Location=[&](int32 Index){return Index==Start?From:Index==Goal?To:Minimum+FVector2D(Index%Columns,Index/Columns)*Step;};
        TArray<double> Cost;Cost.Init(TNumericLimits<double>::Max(),Cells+2);Cost[Start]=0;
        TArray<int32> Parent;Parent.Init(INDEX_NONE,Cells+2);TArray<bool> Closed;Closed.Init(false,Cells+2);
        struct FOpenNode{int32 Index;double Priority;};TArray<FOpenNode> Open;
        auto Push=[&](int32 Index,double Priority)
        {int32 At=Open.Add({Index,Priority});while(At>0){const int32 Up=(At-1)/2;if(Open[Up].Priority<=Open[At].Priority)break;Swap(Open[Up],Open[At]);At=Up;}};
        auto Pop=[&]()
        {const int32 Index=Open[0].Index;Open[0]=Open.Last();Open.Pop(EAllowShrinking::No);int32 At=0;while(At*2+1<Open.Num()){int32 Child=At*2+1;if(Child+1<Open.Num()&&Open[Child+1].Priority<Open[Child].Priority)++Child;if(Open[At].Priority<=Open[Child].Priority)break;Swap(Open[At],Open[Child]);At=Child;}return Index;};
        Push(Start,FVector2D::Distance(From,To)/FastestSpeed);
        while(!Open.IsEmpty())
        {
            const int32 Current=Pop();if(Closed[Current])continue;Closed[Current]=true;if(Current==Goal)break;const FVector2D Here=Location(Current);
            auto Link=[&](int32 Next)
            {
                if(Next<0||Next>=Cells+2||Closed[Next])return;const FVector2D There=Location(Next);const double Edge=EdgeCost(Here,There);
                if(Edge<0||Cost[Current]+Edge>=Cost[Next])return;Cost[Next]=Cost[Current]+Edge;Parent[Next]=Current;Push(Next,Cost[Next]+FVector2D::Distance(There,To)/FastestSpeed);
            };
            if(FVector2D::Distance(Here,To)<=Step*2)Link(Goal);
            const int32 X=Current==Start?FMath::RoundToInt((From.X-Minimum.X)/Step):Current%Columns,Y=Current==Start?FMath::RoundToInt((From.Y-Minimum.Y)/Step):Current/Columns;
            for(int32 DY=-1;DY<=1;++DY)for(int32 DX=-1;DX<=1;++DX)
            {if(Current!=Start&&DX==0&&DY==0)continue;const int32 NX=X+DX,NY=Y+DY;if(NX>=0&&NX<Columns&&NY>=0&&NY<Rows)Link(NY*Columns+NX);}
        }
        if(Parent[Goal]==INDEX_NONE||(HasGraphRoute&&Cost[Goal]>=Distance[1]))return false;
        TArray<FVector2D> Contour;for(int32 At=Goal;At!=Start;At=Parent[At]){if(At==INDEX_NONE)return false;Contour.Add(Location(At));}
        // Remove unnecessary grid turns only when the same full edge checks
        // approve the longer chord. The final point remains exactly To.
        FVector2D LastPoint=From;
        for(int32 At=Contour.Num()-1;At>=0;)
        {
            // Keep smoothing linear in the bounded path length even for a
            // serpentine corridor; unrestricted all-future chords are quadratic.
            int32 Furthest=At;const int32 EndLookahead=FMath::Max(0,At-32);
            for(int32 Candidate=At-1;Candidate>=EndLookahead;--Candidate)if(EdgeCost(LastPoint,Contour[Candidate])>=0)Furthest=Candidate;
            Route.Add(Contour[Furthest]);LastPoint=Contour[Furthest];At=Furthest-1;
        }
        return true;
        };
        if(FindContour())return Remember(true);
        if(!HasGraphRoute)return Remember(false);
    }
    TArray<FVector2D> Reverse;for(int I=1;I!=0;I=Previous[I]){if(I==INDEX_NONE)return Remember(false);Reverse.Add(Points[I]);}
    FVector2D Last=From;
    for(int I=Reverse.Num()-1;I>=0;--I){const FVector2D End=Reverse[I];const int Parts=RoadPlan&&!ExistingRoad(Last,End)?FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(Last,End)/MaximumRoad)):1;for(int J=1;J<=Parts;++J)Route.Add(J==Parts?End:FMath::Lerp(Last,End,double(J)/Parts));Last=End;}
    return Remember(true);
}
bool FSeigeSimulation::WalkRoute(FVector2D& Position,const TArray<FVector2D>& Route,int32& Next,double Seconds) const
{
    while(Route.IsValidIndex(Next)&&Seconds>UE_DOUBLE_SMALL_NUMBER)
    {const FVector2D End=Route[Next];const double Distance=FVector2D::Distance(Position,End),Speed=Mobility.Speed(*this,TEXT("drone"),Position,End)/MetersPerWorldUnit()*Companions.WorkerSpeedMultiplier(*this);if(Speed<=0)return false;if(Distance<=Speed*Seconds+.000001){Position=End;Seconds=FMath::Max(0.,Seconds-Distance/Speed);++Next;}else{Position+=(End-Position)/Distance*Speed*Seconds;Seconds=0;}}
    return Next>=Route.Num()&&!Route.IsEmpty();
}
bool FSeigeSimulation::CanPlaceRoad(FVector2D A,FVector2D B,FString& Error,bool CheckMaterials) const
{
    if(!RoadsEnabled()){Error=TEXT("Road construction is disabled in this version");return false;}
    const auto* C=Core();if(!Transport||!C||C->Health<=0||C->IsConstructing||Escaped||Failed){Error=TEXT("An operating command core is required");return false;}
    for(FVector2D P:{A,B})if(Number(TEXT("placement_requires_visibility"))>0&&!IsVisible(P)){Error=TEXT("Road endpoints require live coverage inside the sector");return false;}
    if(!CanPlaceRoadGeometry(A,B,Error))return false;
    if(CheckMaterials)for(const auto& P:RoadCost(A,B,InitialRoadTier()))if(ConstructionAvailable(P.Key)+UE_DOUBLE_SMALL_NUMBER<P.Value){Error=TEXT("Insufficient unreserved road construction materials");return false;}
    Error.Empty();return true;
}
bool FSeigeSimulation::CanPlaceRoadGeometry(FVector2D A,FVector2D B,FString& Error) const
{
    if(!RoadsEnabled()){Error=TEXT("Road construction is disabled in this version");return false;}
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
{if(!CanPlaceRoad(A,B,Error))return false;FSeigeTransportSegment R;R.Id=NextId++;R.A=A;R.B=B;R.TargetTier=Transport->GetStringField(TEXT("initial_tier"));R.MaxHealth=FVector2D::Distance(A,B)*MetersPerWorldUnit()*TransportTiers[R.TargetTier].HealthPerMeter;R.Health=R.MaxHealth;R.BuilderPosition=BuildingAccessPoint(*Core());FindRoute(R.BuilderPosition,RoadAccessPoint(R),R.BuilderRoute);Roads.Add(R);++TransportRevision;AllocateWorkers();AddEvent(TEXT("Road construction queued"));return true;}
bool FSeigeSimulation::CanUpgradeRoad(int32 Id,FString& Error) const
{
    if(!RoadsEnabled()){Error=TEXT("Road construction is disabled in this version");return false;}
    const auto* R=FindRoad(Id);if(!R||R->Health<=0||R->IsConstructing||R->Tier.IsEmpty()||Escaped||Failed){Error=TEXT("Select a completed road to upgrade");return false;}
    const FString Next=TransportTiers[R->Tier].NextTier;if(Next.IsEmpty()){Error=TEXT("Transport corridor is already fully upgraded");return false;}
    const double RoadWidth=TransportTiers[Next].WidthMeters*.5/MetersPerWorldUnit();
    if(!Environment.SegmentDry(R->A,R->B,RoadWidth)||!ClearWalkingLine(R->A,R->B,RoadWidth)){Error=TEXT("Upgraded corridor requires dry land clear of reserved plots");return false;}
    for(const auto& P:RoadCost(R->A,R->B,Next))if(ConstructionAvailable(P.Key)+UE_DOUBLE_SMALL_NUMBER<P.Value){Error=TEXT("Insufficient unreserved upgrade materials");return false;}
    Error.Empty();return true;
}
bool FSeigeSimulation::UpgradeRoad(int32 Id,FString& Error)
{if(!CanUpgradeRoad(Id,Error))return false;auto& R=*FindRoad(Id);for(const auto& P:R.InstalledMaterials)R.PreviousTierMaterials.FindOrAdd(P.Key)+=P.Value;R.TargetTier=TransportTiers[R.Tier].NextTier;R.IsConstructing=true;R.ConstructionProgress=0;R.BuildersOnSite=R.TravellingBuilders=0;R.InstalledMaterials.Empty();R.ConstructionMaterials.Empty();R.BuilderPosition=BuildingAccessPoint(*Core());R.BuilderNextWaypoint=0;FindRoute(R.BuilderPosition,RoadAccessPoint(R),R.BuilderRoute);++TransportRevision;AllocateWorkers();AddEvent(TEXT("Transport corridor upgrade queued; existing service remains open"));return true;}
double FSeigeSimulation::IncomingRoad(int32 Id,const FString& Resource) const
{double Result=0;for(const auto& C:Couriers)if(C.RoadTargetId==Id&&(Resource.IsEmpty()||C.Resource==Resource))Result+=C.Amount+C.ReservedAmount;return Result;}

namespace
{
double RoadServiceDistance(FVector2D P,const FSeigeTransportSegment& R)
{const FVector2D V=R.B-R.A;return FVector2D::Distance(P,R.A+V*FMath::Clamp(FVector2D::DotProduct(P-R.A,V)/FMath::Max(V.SizeSquared(),1.e-12),0.,1.));}
}
double FSeigeSimulation::RoadRepairDemand(const FSeigeBuilding& B,const FString& Resource) const
{
    if(!RoadsEnabled()||!Transport||Resource!=Transport->GetStringField(TEXT("repair_resource"))||B.Health<=0||B.IsConstructing||!B.Enabled||B.Workers<=0)return 0;
    for(const auto& R:Roads)if(R.Health>0&&R.Health<R.MaxHealth&&!R.IsConstructing&&Energy.RoadConnectedToBuilding(R.Id,B.Id)&&RoadServiceDistance(BuildingAccessPoint(B),R)*MetersPerWorldUnit()<=Transport->GetNumberField(TEXT("repair_service_range_meters")))return Transport->GetNumberField(TEXT("repair_buffer_units"));
    return 0;
}
void FSeigeSimulation::StepRoadRepairs(double Seconds)
{
    if(!RoadsEnabled())return;
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
