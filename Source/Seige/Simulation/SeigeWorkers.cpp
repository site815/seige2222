#include "SeigeWorkers.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"

double FSeigeWorkerSystem::Number(const TCHAR* Key) const{return Policy->GetNumberField(Key);}
FSeigeWorker* FSeigeWorkerSystem::Find(const FString& Id){return Bodies.FindByPredicate([&](const auto& W){return W.Id==Id;});}
const FSeigeWorker* FSeigeWorkerSystem::Find(const FString& Id) const{return Bodies.FindByPredicate([&](const auto& W){return W.Id==Id;});}
int32 FSeigeWorkerSystem::StoredAt(int32 Id) const{int32 N=0;for(const auto& W:Bodies)if(W.State==TEXT("stored")&&W.ContainerKind==TEXT("building")&&W.ContainerId==Id)++N;return N;}
bool FSeigeWorkerSystem::Initialize(const TSharedPtr<FJsonObject>& Rules,FSeigeSimulation& S,FString& Error)
{
    Policy=Rules;FString Selector;
    if(!Policy||!Policy->TryGetStringField(TEXT("scheduler_policy"),Selector)||Selector!=TEXT("finite_shared_pool")){Error=TEXT("Unknown finite worker scheduler");return false;}
    for(const TCHAR* Key:{TEXT("haul_mass_kg"),TEXT("haul_volume_litres"),TEXT("logistics_workers"),TEXT("logistics_facilities_per_worker"),TEXT("logistics_max_workers"),TEXT("core_minimum_operators"),TEXT("loading_seconds"),TEXT("unloading_seconds"),TEXT("route_retry_seconds"),TEXT("deployment_ground_seconds"),TEXT("deployment_hatch_seconds"),TEXT("hatch_exit_spacing_seconds"),TEXT("core_hatch_fraction"),TEXT("idle_return_seconds"),TEXT("workstation_spacing_meters"),TEXT("body_radius_meters")})
    {double V=0;if(!Policy->TryGetNumberField(Key,V)||!FMath::IsFinite(V)||V<=0){Error=FString(TEXT("Invalid worker policy: "))+Key;return false;}}
    for(const TCHAR* Key:{TEXT("logistics_workers"),TEXT("logistics_facilities_per_worker"),TEXT("logistics_max_workers"),TEXT("core_minimum_operators")})if(Number(Key)!=FMath::FloorToDouble(Number(Key))||Number(Key)>1000){Error=TEXT("Worker scheduling counts must be bounded integers");return false;}
    if(!Policy->TryGetStringField(TEXT("logistics_scaling_policy"),LogisticsScalingPolicy)||(LogisticsScalingPolicy!=TEXT("fixed")&&LogisticsScalingPolicy!=TEXT("completed_facilities"))||Number(TEXT("logistics_max_workers"))<Number(TEXT("logistics_workers")))
    {Error=TEXT("Invalid logistics scaling policy or capacity");return false;}
    const TArray<TSharedPtr<FJsonValue>>* Excluded=nullptr;LogisticsExcludedRoles.Empty();
    if(!Policy->TryGetArrayField(TEXT("logistics_excluded_roles"),Excluded)){Error=TEXT("Logistics excluded roles must be an array");return false;}
    for(const auto& Value:*Excluded)
    {FString Role;if(!Value->TryGetString(Role)||LogisticsExcludedRoles.Contains(Role)){Error=TEXT("Invalid or repeated logistics excluded role");return false;}bool Known=false;for(const auto& Pair:S.BuildingDefs)Known|=Pair.Value.Role==Role;if(!Known){Error=TEXT("Unknown logistics excluded role: ")+Role;return false;}LogisticsExcludedRoles.Add(Role);}
    if(Number(TEXT("core_hatch_fraction"))>1){Error=TEXT("Worker hatch must be inside the level-one core body");return false;}
    if(BodyRadiusMeters()<.1||BodyRadiusMeters()>5){Error=TEXT("Worker body radius must be between 0.1 and 5 metres");return false;}
    Origin=FString::Printf(TEXT("%08x"),uint32(S.GenerationSeed));NextSerial=1;DeploymentElapsed=RecyclingWasteKg=0;Bodies.Empty();DeploymentStock.Empty();
    auto* C=S.Core();if(!C){Error=TEXT("Worker landing requires a command core");return false;}
    DeploymentStock=MoveTemp(C->ConstructionMaterials);C->ConstructionMaterials.Empty();
    return SeedInitial(S,S.Population,Error);
}
int32 FSeigeWorkerSystem::LogisticsJobs(const FSeigeSimulation& S) const
{
    if(!Policy)return 0;
    int32 Facilities=0;
    if(LogisticsScalingPolicy==TEXT("completed_facilities"))for(const auto& B:S.Buildings)
        if(B.Health>0&&B.Enabled&&!B.IsConstructing)if(const auto* D=S.Definition(B))
            if(!LogisticsExcludedRoles.Contains(D->Role))++Facilities;
    return FMath::Min(int32(Number(TEXT("logistics_max_workers"))),int32(Number(TEXT("logistics_workers")))+Facilities/int32(Number(TEXT("logistics_facilities_per_worker"))));
}
bool FSeigeWorkerSystem::SeedInitial(FSeigeSimulation& S,int32 Count,FString& Error)
{
    if(S.Time>0||Count<0||Count>100000){Error=TEXT("Initial worker manifests can only be authored before simulation starts");return false;}
    Bodies.Empty();NextSerial=1;auto* C=S.Core();if(!C)return false;
    for(int32 I=0;I<Count;++I){FSeigeWorker W;W.Id=Origin+TEXT(":")+FString::FromInt(NextSerial++);W.Position=C->Position;W.ContainerId=C->Id;W.DepartureAt=Number(TEXT("deployment_ground_seconds"))+Number(TEXT("deployment_hatch_seconds"))+I*Number(TEXT("hatch_exit_spacing_seconds"));Bodies.Add(W);}
    const FString Resource=S.TextRule(TEXT("inactive_worker_resource"));
    for(const auto& B:S.Buildings)NewStored(S,B.Id,FMath::RoundToInt(B.Inventory.FindRef(Resource)));
    RefreshMetrics(S);Error.Empty();return true;
}
void FSeigeWorkerSystem::NewStored(FSeigeSimulation& S,int32 Id,int32 Count)
{
    const auto* B=S.FindBuilding(Id);if(!B)return;
    for(int32 I=0;I<Count;++I){FSeigeWorker W;W.Id=Origin+TEXT(":")+FString::FromInt(NextSerial++);W.State=TEXT("stored");W.Activity=TEXT("stored");W.ContainerId=Id;W.Position=S.BuildingAccessPoint(*B);Bodies.Add(W);}
}
bool FSeigeWorkerSystem::SeedEstablished(FSeigeSimulation& S,const TMap<int32,int32>& OperatorsByBuilding,int32 IdleWorkers,FString& Error)
{
    const auto* Core=S.Core();
    if(!Policy||Origin.IsEmpty()||!Core||Core->Health<=0||!Core->Enabled||Core->IsConstructing||!FMath::IsFinite(S.Time)||S.Time<DeploymentDescentSeconds()+DeploymentHatchSeconds()||IdleWorkers<0||IdleWorkers>100000||!S.Couriers.IsEmpty()||S.WorkersDisassembled||DeploymentElapsed>0)
    {Error=TEXT("Established worker manifests require a fresh completed scenario candidate");return false;}
    for(const auto& W:Bodies)if(W.State!=TEXT("active")||W.Activity!=TEXT("aboard"))
    {Error=TEXT("Established initialization cannot replace workers that have entered play");return false;}
    int64 Active=IdleWorkers,Stored=0,Capacity=0;
    for(const auto& P:OperatorsByBuilding)
    {
        const auto* B=S.FindBuilding(P.Key);const auto* D=B?S.Definition(*B):nullptr;
        if(!D||B->Health<=0||!B->Enabled||B->IsConstructing||P.Value<0||P.Value>D->Jobs)
        {Error=TEXT("Established operator assignment exceeds a live completed building's jobs");return false;}
        Active+=P.Value;
    }
    const FString Resource=S.TextRule(TEXT("inactive_worker_resource"));TArray<int32> BuildingIds;
    for(const auto& B:S.Buildings)
    {
        const auto* D=S.Definition(B);const double Packed=B.Inventory.FindRef(Resource);
        if(!D||B.Health<=0||B.IsConstructing||B.ProductionCommitted||B.DisassemblyCommitted||!B.Shipment.Resource.IsEmpty()||!FMath::IsFinite(Packed)||Packed<0||Packed!=FMath::FloorToDouble(Packed)||Packed>100000||(Packed>0&&!D->StoresInactiveWorkers)||S.Occupied(B)>D->StorageCapacity+1.e-6)
        {Error=TEXT("Established worker stock requires valid completed containers without in-flight work");return false;}
        Stored+=int64(Packed);BuildingIds.Add(B.Id);
        const int32 Assigned=OperatorsByBuilding.FindRef(B.Id);
        if(B.Enabled&&(D->WorkforceMode==TEXT("full_staff")?Assigned>=D->Jobs:D->Jobs==0||Assigned>0))Capacity+=D->RobotSupportCapacity;
    }
    if(Active+Stored>100000||Active>Capacity||OperatorsByBuilding.FindRef(Core->Id)<FMath::Min(S.Definition(*Core)->Jobs,int32(Number(TEXT("core_minimum_operators")))))
    {Error=TEXT("Established worker population exceeds staffed support capacity or lacks core operators");return false;}
    for(const auto& P:DeploymentStock)if(P.Value>Core->InstalledMaterials.FindRef(P.Key)+1.e-6)
    {Error=TEXT("Established core must account for its installed deployment material");return false;}
    BuildingIds.Sort();TArray<FSeigeWorker> Pending;Pending.Reserve(int32(Active+Stored));int32 Serial=1;
    const double Radius=BodyRadiusMeters()/S.MetersPerWorldUnit();
    auto AddActive=[&](int32 BuildingId,int32 Slot,bool Operating)
    {
        FSeigeWorker W;W.Id=Origin+TEXT(":")+FString::FromInt(Serial++);W.Activity=Operating?TEXT("operate"):TEXT("idle");W.BuildingId=BuildingId;W.StationSlot=Slot;W.Position=WorkPosition(S,W);W.ContainerId=0;W.ContainerKind.Empty();W.Outdoor=true;W.RouteRevision=S.TransportRevision;
        if(FMath::Abs(W.Position.X)+Radius>S.WorldHalfSize||FMath::Abs(W.Position.Y)+Radius>S.WorldHalfSize||!S.ClearWalkingLine(S.BuildingAccessPoint(*S.FindBuilding(BuildingId)),W.Position,Radius))
        {Error=TEXT("Established worker workstation is obstructed or outside dry sector ground");return false;}
        for(const auto& Other:Pending)if(Other.State==TEXT("active")&&FVector2D::DistSquared(W.Position,Other.Position)<FMath::Square(2*Radius)-1.e-6)
        {Error=TEXT("Established worker workstations overlap physical bodies");return false;}
        if(!Operating)W.BuildingId=0;Pending.Add(MoveTemp(W));return true;
    };
    for(int32 Id:BuildingIds)for(int32 I=0;I<OperatorsByBuilding.FindRef(Id);++I)if(!AddActive(Id,I,true))return false;
    for(int32 I=0;I<IdleWorkers;++I)if(!AddActive(Core->Id,OperatorsByBuilding.FindRef(Core->Id)+I,false))return false;
    for(int32 Id:BuildingIds)for(int32 I=0;I<int32(S.FindBuilding(Id)->Inventory.FindRef(Resource));++I)
    {FSeigeWorker W;W.Id=Origin+TEXT(":")+FString::FromInt(Serial++);W.State=TEXT("stored");W.Activity=TEXT("stored");W.ContainerId=Id;W.Position=S.BuildingAccessPoint(*S.FindBuilding(Id));Pending.Add(MoveTemp(W));}
    // Commit only after the complete manifest, physical stations and all berth
    // counts validate. Candidate inventory is never created or consumed here.
    Bodies=MoveTemp(Pending);NextSerial=Serial;DeploymentStock.Empty();DeploymentElapsed=DeploymentDescentSeconds()+DeploymentHatchSeconds();RecyclingWasteKg=0;RefreshMetrics(S);Error.Empty();return true;
}
FVector2D FSeigeWorkerSystem::HatchPoint(const FSeigeSimulation& S) const
{
    const auto* C=S.Core();if(!C)return FVector2D::ZeroVector;double Radius=S.Definition(*C)->Footprint;
    for(const auto& P:S.BuildingDefs)if(P.Value.Role==TEXT("core")&&P.Value.Level==1){Radius=P.Value.Footprint;break;}
    return C->Position+FVector2D(Radius*Number(TEXT("core_hatch_fraction")),0);
}
void FSeigeWorkerSystem::ShiftHome(FVector2D Delta){for(auto& W:Bodies){W.Position+=Delta;for(auto& P:W.Route)P+=Delta;}}
void FSeigeWorkerSystem::Release(FSeigeWorker& W){W.Activity=TEXT("idle");W.BuildingId=W.RoadId=W.DeliveryId=0;W.Route.Empty();W.NextWaypoint=0;W.PhaseSeconds=0;W.ContainerId=0;W.ContainerKind.Empty();W.Outdoor=true;}
FVector2D FSeigeWorkerSystem::WorkPosition(const FSeigeSimulation& S,const FSeigeWorker& W) const
{
    const double Separation=Number(TEXT("workstation_spacing_meters"))/S.MetersPerWorldUnit();
    if(const auto* B=S.FindBuilding(W.BuildingId)){const auto& Port=S.Definition(*B)->AccessPort;return S.BuildingAccessPoint(*B)+FVector2D(-Port.Y,Port.X)*((W.StationSlot+1)*Separation);}
    if(const auto* R=S.FindRoad(W.RoadId)){const FVector2D Along=(R->B-R->A).GetSafeNormal();return S.RoadAccessPoint(*R)+Along*((W.StationSlot+1)*Separation);}
    return W.Position;
}
void FSeigeWorkerSystem::RouteTo(FSeigeSimulation& S,FSeigeWorker& W,FVector2D Destination)
{
    W.Route.Empty();W.NextWaypoint=0;W.RouteRevision=S.TransportRevision;W.RetryAt=S.Time+Number(TEXT("route_retry_seconds"));
    const auto* C=S.Core();const FVector2D Port=C?S.BuildingAccessPoint(*C):FVector2D::ZeroVector,Hatch=HatchPoint(S);
    // Only the own shuttle hatch connector may traverse its reserved forecourt.
    // All exterior route segments still use the ordinary collision/road solver.
    auto InsideConnector=[&](FVector2D P){return C&&FMath::Abs(P.Y-C->Position.Y)<1.e-5&&P.X>=C->Position.X-1.e-5&&P.X<=Port.X+1.e-5;};
    FVector2D From=W.Position,To=Destination;TArray<FVector2D> Prefix,Suffix,Middle;
    if(InsideConnector(From)){if(!S.Environment.SegmentDry(From,Port,0))return;Prefix.Add(From);Prefix.Add(Port);From=Port;}
    if(InsideConnector(To)){if(!S.Environment.SegmentDry(Port,To,0))return;Suffix.Add(Port);Suffix.Add(To);To=Port;}
    if(FVector2D::DistSquared(From,To)<1.e-10)Middle.Add(To);else if(!S.FindRoute(From,To,Middle))return;
    W.Route=MoveTemp(Prefix);for(const auto& P:Middle)if(W.Route.IsEmpty()||FVector2D::DistSquared(W.Route.Last(),P)>1.e-10)W.Route.Add(P);for(const auto& P:Suffix)if(W.Route.IsEmpty()||FVector2D::DistSquared(W.Route.Last(),P)>1.e-10)W.Route.Add(P);
}
bool FSeigeWorkerSystem::Move(FSeigeSimulation& S,FSeigeWorker& W,FVector2D Destination,double Seconds)
{
    if(FVector2D::DistSquared(W.Position,Destination)<1.e-8)return true;
    if((W.Route.IsEmpty()&&S.Time>=W.RetryAt)||W.RouteRevision!=S.TransportRevision)RouteTo(S,W,Destination);
    const FVector2D Before=W.Position;const bool Arrived=S.WalkRoute(W.Position,W.Route,W.NextWaypoint,Seconds);
    if(!W.Position.Equals(Before,1.e-8))W.Heading=(W.Position-Before).GetSafeNormal();return Arrived;
}
FSeigeWorker* FSeigeWorkerSystem::IdleWorker(FSeigeSimulation& S,bool Borrow,bool IncludeIdle)
{
    if(IncludeIdle)for(auto& W:Bodies)if(W.State==TEXT("active")&&W.Activity==TEXT("idle"))return &W;
    if(Borrow)
    {
        // Incoming replacements do not operate the generator yet. Lending the
        // last on-site operator before they arrive would interrupt core power.
        const auto* C=S.Core();int32 CoreOperators=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&W.BuildingId==(C?C->Id:0)&&W.Activity==TEXT("operate"))++CoreOperators;
        if(CoreOperators>Number(TEXT("core_minimum_operators")))for(int32 I=Bodies.Num()-1;I>=0;--I){auto& W=Bodies[I];if(W.State==TEXT("active")&&W.BuildingId==C->Id&&W.Activity==TEXT("operate")){Release(W);return &W;}}
    }
    return nullptr;
}
void FSeigeWorkerSystem::RefreshMetrics(FSeigeSimulation& S) const
{
    if(!Policy)return;S.Population=S.Employed=S.TotalJobs=0;
    for(auto& B:S.Buildings){B.Workers=B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;B.BuilderRoute.Empty();B.BuilderNextWaypoint=0;if(B.Health>0&&B.Enabled)S.TotalJobs+=B.IsConstructing?S.RequiredBuilders(B):S.Definition(B)->Jobs;}
    for(auto& R:S.Roads){R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;R.BuilderRoute.Empty();R.BuilderNextWaypoint=0;if(R.Health>0&&R.IsConstructing)S.TotalJobs+=S.TransportTiers[R.TargetTier].ConstructionWorkers;}
    S.TotalJobs+=LogisticsJobs(S);
    for(const auto& W:Bodies)if(W.State==TEXT("active"))
    {
        ++S.Population;if(W.Activity!=TEXT("idle")&&W.Activity!=TEXT("aboard"))++S.Employed;
        if(auto* B=S.FindBuilding(W.BuildingId))
        {if(W.Activity==TEXT("operate"))++B->Workers;if(W.Activity==TEXT("build")||W.Activity==TEXT("to_build")){++B->Builders;if(W.Activity==TEXT("build"))++B->BuildersOnSite;else{++B->TravellingBuilders;B->BuilderPosition=W.Position;B->BuilderRoute=W.Route;B->BuilderNextWaypoint=W.NextWaypoint;B->BuilderRouteRevision=W.RouteRevision;}}}
        if(auto* R=S.FindRoad(W.RoadId))if(W.Activity==TEXT("road_build")||W.Activity==TEXT("to_road")){++R->Builders;if(W.Activity==TEXT("road_build"))++R->BuildersOnSite;else{++R->TravellingBuilders;R->BuilderPosition=W.Position;R->BuilderRoute=W.Route;R->BuilderNextWaypoint=W.NextWaypoint;R->BuilderRouteRevision=W.RouteRevision;}}
    }
    S.DeploymentElapsed=DeploymentElapsed;S.DeploymentGrounded=DeploymentElapsed>=Number(TEXT("deployment_ground_seconds"));S.DeploymentHatchOpen=DeploymentElapsed>=Number(TEXT("deployment_ground_seconds"))+Number(TEXT("deployment_hatch_seconds"));S.UpdateSupport();
}
void FSeigeWorkerSystem::Schedule(FSeigeSimulation& S)
{
    const int32 LogisticsReserve=LogisticsJobs(S);
    int32 WorkingHaulers=0;
    for(const auto& C:S.Couriers)if(!C.SelfTransfer&&C.Phase!=TEXT("done"))
        if(const auto* W=Find(C.WorkerId))if(W->State==TEXT("active")&&W->Activity==TEXT("delivery")&&W->DeliveryId==C.Id)++WorkingHaulers;
    const bool FreightPending=S.Couriers.ContainsByPredicate([](const auto& C){return C.Phase!=TEXT("done");});
    auto FullySupplied=[](const TMap<FString,double>& Bill,const TMap<FString,double>& Installed,const TMap<FString,double>& OnSite)
    {for(const auto& P:Bill)if(Installed.FindRef(P.Key)+OnSite.FindRef(P.Key)+1.e-8<P.Value)return false;return true;};
    bool ConstructionCrewActive=Bodies.ContainsByPredicate([](const auto& W){return W.State==TEXT("active")&&(W.Activity==TEXT("to_build")||W.Activity==TEXT("build")||W.Activity==TEXT("to_road")||W.Activity==TEXT("road_build"));});
    struct Job{int32 Building=0,Road=0,Count=0,Priority=0;FString Activity;bool SuppliedConstruction=false;};TArray<Job> Jobs;
    for(const auto& B:S.Buildings)if(B.Health>0&&B.Enabled)
    {
        const auto* D=S.Definition(B);int32 Have=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&W.BuildingId==B.Id&&(W.Activity==TEXT("operate")||W.Activity==TEXT("to_job")||W.Activity==TEXT("build")||W.Activity==TEXT("to_build")))++Have;
        const int32 Need=B.IsConstructing?S.RequiredBuilders(B):D->Jobs;
        if(Need>Have){Job J;J.Building=B.Id;J.Count=Need-Have;J.Priority=D->StaffingPriority;J.Activity=B.IsConstructing?TEXT("to_build"):TEXT("to_job");J.SuppliedConstruction=B.IsConstructing&&FullySupplied(S.ConstructionCost(B),B.InstalledMaterials,B.ConstructionMaterials);if(D->Role==TEXT("core")&&!B.IsConstructing){J.Count=FMath::Max(0,int32(Number(TEXT("core_minimum_operators")))-Have);J.Priority=MAX_int32;}else if(D->Role==TEXT("service")&&!B.IsConstructing)J.Priority=MAX_int32-1;if(J.Count)Jobs.Add(J);}
    }
    for(const auto& R:S.Roads)if(R.Health>0&&R.IsConstructing){int32 Have=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&W.RoadId==R.Id)++Have;Job J;J.Road=R.Id;J.Count=S.TransportTiers[R.TargetTier].ConstructionWorkers-Have;J.Priority=int32(S.Transport->GetNumberField(TEXT("construction_staffing_priority")));J.Activity=TEXT("to_road");J.SuppliedConstruction=FullySupplied(S.RoadCost(R.A,R.B,R.TargetTier),R.InstalledMaterials,R.ConstructionMaterials);if(J.Count>0)Jobs.Add(J);}
    Jobs.StableSort([](const Job& A,const Job& B){const bool AC=A.Priority>=MAX_int32-1,BC=B.Priority>=MAX_int32-1;if(AC!=BC)return AC;return AC?A.Priority>B.Priority:A.Priority<B.Priority;});
    for(const auto& J:Jobs)for(int32 I=0;I<J.Count;++I)
    {
        int32 Idle=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&W.Activity==TEXT("idle"))++Idle;
        const bool Critical=J.Priority>=MAX_int32-1;
        // A fully delivered road may be the only way to power more support.
        // With no outstanding freight, lend only the adaptive reserve above
        // the configured base; retain real haulers for subsequent deliveries.
        const int32 Reserve=J.SuppliedConstruction&&!FreightPending?FMath::Min(LogisticsReserve,int32(Number(TEXT("logistics_workers")))):LogisticsReserve;
        // If that reserve itself is the last available body, permit one
        // reachable, fully funded job to finish rather than deadlocking every
        // worker behind the support capacity that this construction can unlock.
        // Other idle haulers remain available whenever the population permits.
        // Spare arrived core operators are lent before the hauling reserve, so
        // the loan only applies when no operator above the minimum remains.
        bool SpareOperator=false;if(const auto* Core=S.Core()){int32 Operators=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&W.BuildingId==Core->Id&&W.Activity==TEXT("operate"))++Operators;SpareOperator=Operators>Number(TEXT("core_minimum_operators"));}
        const bool ProgressLoan=J.SuppliedConstruction&&!FreightPending&&!ConstructionCrewActive&&!SpareOperator&&Idle>0&&Idle+WorkingHaulers<=Reserve&&S.OperatingEfficiency()>0;
        // The reserve includes bodies already hauling. A finished delivery can
        // join construction while other haulers keep logistics running; intact
        // stored bodies relocating themselves are not available haulers.
        // Only idle bodies or spare arrived core operators are reassigned.
        auto* W=IdleWorker(S,true,Critical||Idle+WorkingHaulers>Reserve||ProgressLoan);if(!W)break;
        if(ProgressLoan)
        {
            FSeigeWorker Assignment=*W;Assignment.BuildingId=J.Building;Assignment.RoadId=J.Road;Assignment.Activity=J.Activity;Assignment.PhaseSeconds=0;Assignment.StationSlot=0;
            const FVector2D Destination=WorkPosition(S,Assignment);RouteTo(S,Assignment,Destination);
            if(Assignment.Route.IsEmpty()&&!Assignment.Position.Equals(Destination,1.e-8))break;
            *W=MoveTemp(Assignment);ConstructionCrewActive=true;continue;
        }
        W->BuildingId=J.Building;W->RoadId=J.Road;W->Activity=J.Activity;W->PhaseSeconds=0;
        W->StationSlot=0;while(Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W->Id&&Other.State==TEXT("active")&&Other.BuildingId==J.Building&&Other.RoadId==J.Road&&Other.StationSlot==W->StationSlot;}))++W->StationSlot;
        RouteTo(S,*W,WorkPosition(S,*W));
        ConstructionCrewActive|=J.Activity==TEXT("to_build")||J.Activity==TEXT("to_road");
    }
    // Spare operators improve proportional command production, but remain the
    // first workers borrowed for a real pending delivery.
    auto* C=S.Core();if(C&&!C->IsConstructing&&C->Health>0){int32 Have=0;for(const auto& W:Bodies)if(W.BuildingId==C->Id&&(W.Activity==TEXT("operate")||W.Activity==TEXT("to_job")))++Have;while(Have<S.Definition(*C)->Jobs){int32 Idle=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&W.Activity==TEXT("idle"))++Idle;if(Idle+WorkingHaulers<=LogisticsReserve)break;auto* W=IdleWorker(S,false);if(!W)break;W->BuildingId=C->Id;W->Activity=TEXT("to_job");W->StationSlot=0;while(Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W->Id&&Other.BuildingId==C->Id&&Other.StationSlot==W->StationSlot;}))++W->StationSlot;RouteTo(S,*W,WorkPosition(S,*W));++Have;}}
}
double FSeigeWorkerSystem::HaulUnits(const FSeigeSimulation& S,const FString& Resource) const
{const auto* D=S.Resources.Find(Resource);if(!D||!Policy)return 0;double V=FMath::Min(Number(TEXT("haul_mass_kg"))/D->UnitMassKg,Number(TEXT("haul_volume_litres"))/D->LitresPerUnit);return D->Discrete?FMath::FloorToDouble(V):V;}
double FSeigeWorkerSystem::PickupReserved(const FSeigeSimulation& S,int32 Source,const FString& Resource,bool Deployment) const
{double N=0;for(const auto& C:S.Couriers)if(C.SourceId==Source&&C.Resource==Resource&&C.SourceDeployment==Deployment)N+=C.ReservedAmount;return N;}
bool FSeigeWorkerSystem::Dispatch(FSeigeSimulation& S,int32 SourceId,int32 Target,int32 Road,const FString& Resource,double Amount,bool Construction,bool Deployment)
{
    auto* Source=S.FindBuilding(SourceId);if(!Source||!S.DeploymentHatchOpen)return false;
    const bool Self=Resource==S.TextRule(TEXT("inactive_worker_resource"));
    if(!Road){const auto* Destination=S.FindBuilding(Target);if(!Destination||Destination->Health<=0)return false;Amount=FMath::Min(Amount,S.StorageRoom(*Destination)/S.Resources[Resource].LitresPerUnit);}
    Amount=FMath::Min(Amount,Self?1.:HaulUnits(S,Resource));if(S.Resources[Resource].Discrete)Amount=FMath::FloorToDouble(Amount+1.e-9);if(Amount<=1.e-8)return false;
    FSeigeWorker* W=nullptr;
    if(Self){int32 Active=0;for(auto& Body:Bodies){if(Body.State==TEXT("active"))++Active;if(!W&&Body.State==TEXT("stored")&&Body.ContainerKind==TEXT("building")&&Body.ContainerId==SourceId)W=&Body;}if(!W||Active>=S.RobotSupportCapacity)return false;}
    else W=IdleWorker(S,true);if(!W)return false;
    FSeigeCourier C;C.Id=S.NextId++;C.WorkerId=W->Id;C.SourceId=SourceId;C.TargetId=Target;C.RoadTargetId=Road;C.Resource=Resource;C.ReservedAmount=Amount;C.ForConstruction=Construction;C.SourceDeployment=Deployment;C.SelfTransfer=Self;C.Position=W->Position;
    if(Self){Source->Inventory.FindOrAdd(Resource)-=1;W->State=TEXT("active");W->Outdoor=false;C.Phase=TEXT("reactivate_transfer");C.ReservedAmount=0;}
    W->Activity=TEXT("delivery");W->DeliveryId=C.Id;W->BuildingId=W->RoadId=0;W->PhaseSeconds=0;W->Route.Empty();W->NextWaypoint=0;W->RetryAt=0;S.Couriers.Add(C);
    // Self-relocation activates the existing body immediately. Publish that
    // occupancy before another transfer or an immediate save can observe it.
    if(Self)RefreshMetrics(S);return true;
}
void FSeigeWorkerSystem::StepDelivery(FSeigeSimulation& S,FSeigeWorker& W,FSeigeCourier& C,double Seconds)
{
    auto* Source=S.FindBuilding(C.SourceId);auto* Target=S.FindBuilding(C.TargetId);auto* Road=S.FindRoad(C.RoadTargetId);
    if((C.RoadTargetId&&(!Road||Road->Health<=0))||(!C.RoadTargetId&&(!Target||Target->Health<=0)))
    {
        if(C.Amount<=0&&!C.SelfTransfer){C.ReservedAmount=0;C.Phase=TEXT("done");Release(W);return;}
        Target=Source&&Source->Health>0?Source:S.Core();if(!Target||Target->Health<=0)return;C.TargetId=Target->Id;C.RoadTargetId=0;C.ForConstruction=false;W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;
    }
    if(C.Phase==TEXT("reactivate_transfer")){C.PhaseSeconds+=Seconds;if(C.PhaseSeconds<S.Number(TEXT("worker_reactivate_seconds")))return;W.Outdoor=true;W.ContainerId=0;W.ContainerKind.Empty();C.Phase=TEXT("carrying");C.PhaseSeconds=0;}
    if(C.Phase==TEXT("pickup")||C.Phase==TEXT("loading"))
    {
        if(!Source||Source->Health<=0){C.ReservedAmount=0;C.Phase=TEXT("done");Release(W);return;}
        const FVector2D Pickup=C.SourceDeployment||Source==S.Core()&&Source->IsConstructing?HatchPoint(S):S.BuildingAccessPoint(*Source);
        if(C.Phase==TEXT("pickup")){if(!Move(S,W,Pickup,Seconds))return;C.Phase=TEXT("loading");C.PhaseSeconds=0;return;}
        C.PhaseSeconds+=Seconds;if(C.PhaseSeconds<Number(TEXT("loading_seconds")))return;
        auto& Stock=C.SourceDeployment?DeploymentStock:Source->Inventory;const double Amount=FMath::Min(C.ReservedAmount,Stock.FindRef(C.Resource));
        Stock.FindOrAdd(C.Resource)-=Amount;C.Amount=Amount;C.ReservedAmount=0;C.PhaseSeconds=0;C.Phase=Amount>1.e-8?TEXT("carrying"):TEXT("done");W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;if(C.Phase==TEXT("done"))Release(W);return;
    }
    const FVector2D Destination=C.RoadTargetId?S.RoadAccessPoint(*Road):S.BuildingAccessPoint(*Target);
    if(C.Phase==TEXT("carrying")){if(!Move(S,W,Destination,Seconds))return;C.Phase=TEXT("unloading");C.PhaseSeconds=0;return;}
    if(C.Phase==TEXT("unloading"))
    {
        C.PhaseSeconds+=Seconds;if(C.PhaseSeconds<Number(TEXT("unloading_seconds")))return;
        if(C.SelfTransfer)
        {if(S.StorageRoom(*Target,C.Id)+1.e-8<S.Resources[C.Resource].LitresPerUnit)return;Target->Inventory.FindOrAdd(C.Resource)+=1;W.State=TEXT("stored");W.Activity=TEXT("stored");W.ContainerId=Target->Id;W.ContainerKind=TEXT("building");W.Outdoor=false;W.DeliveryId=0;W.Route.Empty();W.NextWaypoint=0;C.Phase=TEXT("done");return;}
        const double Capacity=C.RoadTargetId?C.Amount:S.StorageRoom(*Target,C.Id)/S.Resources[C.Resource].LitresPerUnit;
        double Amount=FMath::Min(C.Amount,Capacity);if(S.Resources[C.Resource].Discrete)Amount=FMath::FloorToDouble(Amount+1.e-9);
        if(C.RoadTargetId)Road->ConstructionMaterials.FindOrAdd(C.Resource)+=Amount;else if(C.ForConstruction)Target->ConstructionMaterials.FindOrAdd(C.Resource)+=Amount;else Target->Inventory.FindOrAdd(C.Resource)+=Amount;
        C.Amount-=Amount;S.DeliveredUnits+=Amount;if(C.Amount<=1.e-8){C.Amount=0;C.Phase=TEXT("done");Release(W);}
    }
}
void FSeigeWorkerSystem::Tick(FSeigeSimulation& S,double Seconds)
{
    if(!Policy)return;DeploymentElapsed+=Seconds;RefreshMetrics(S);
    for(auto& W:Bodies)
    {
        if(W.State!=TEXT("active"))continue;
        if(W.Activity==TEXT("aboard"))
        {if(DeploymentElapsed<W.DepartureAt)continue;W.Activity=TEXT("exit");W.Outdoor=false;RouteTo(S,W,S.BuildingAccessPoint(*S.Core()));}
        if(W.Activity==TEXT("exit"))
        {const auto* C=S.Core();if(!C)continue;const bool Arrived=Move(S,W,S.BuildingAccessPoint(*C),Seconds);W.Outdoor=W.Position.X>=HatchPoint(S).X;if(Arrived)Release(W);continue;}
        if(W.Activity==TEXT("delivery"))
        {auto* C=S.Couriers.FindByPredicate([&](const auto& V){return V.Id==W.DeliveryId;});if(!C){Release(W);continue;}StepDelivery(S,W,*C,Seconds);C->Position=W.Position;C->Route=W.Route;C->NextWaypoint=W.NextWaypoint;C->RouteRevision=W.RouteRevision;continue;}
        if(W.Activity==TEXT("to_job")||W.Activity==TEXT("operate")||W.Activity==TEXT("to_build")||W.Activity==TEXT("build"))
        {auto* B=S.FindBuilding(W.BuildingId);const bool Build=W.Activity==TEXT("to_build")||W.Activity==TEXT("build");if(!B||B->Health<=0||!B->Enabled||B->IsConstructing!=Build){Release(W);continue;}if(W.Activity==TEXT("to_job")||W.Activity==TEXT("to_build")){if(Move(S,W,WorkPosition(S,W),Seconds))W.Activity=Build?TEXT("build"):TEXT("operate");}continue;}
        if(W.Activity==TEXT("to_road")||W.Activity==TEXT("road_build"))
        {auto* R=S.FindRoad(W.RoadId);if(!R||R->Health<=0||!R->IsConstructing){Release(W);continue;}if(W.Activity==TEXT("to_road")&&Move(S,W,WorkPosition(S,W),Seconds))W.Activity=TEXT("road_build");continue;}
        if(W.Activity==TEXT("reactivate")){W.PhaseSeconds+=Seconds;if(W.PhaseSeconds>=S.Number(TEXT("worker_reactivate_seconds")))Release(W);continue;}
        if(W.Activity==TEXT("return")){const auto* C=S.Core();if(!C||C->Health<=0||Move(S,W,S.BuildingAccessPoint(*C),Seconds))Release(W);continue;}
        if(W.Activity==TEXT("to_recycle"))
        {
            auto* B=S.FindBuilding(W.ContainerId);if(!B||B->Health<=0||B->IsConstructing){Release(W);continue;}
            if(!Move(S,W,S.BuildingAccessPoint(*B),Seconds))continue;
            const double Volume=S.InventoryLitres(S.DisassemblyOutputs());
            if(S.Population<=FMath::Max(S.TotalJobs,int32(S.Number(TEXT("minimum_population"))))){Release(W);continue;}
            if(B->DisassemblyCommitted||B->DisassemblyQueued||S.WorkFraction(*B)<=0||Volume>S.StorageRoom(*B,0,W.Id)+1.e-8||!S.Energy.Consume(S,B->Id,S.DisassemblyEnergyKWh()))continue;
            B->DisassemblyQueued=1;B->DisassemblyCommitted=true;B->DisassemblyProgress=0;B->DisassemblyReservedLitres=Volume;W.State=TEXT("disassembling");W.Activity=TEXT("disassembling");W.Outdoor=false;W.Route.Empty();W.NextWaypoint=0;continue;
        }
        if(W.Activity==TEXT("to_store")||W.Activity==TEXT("store"))
        {auto* B=S.FindBuilding(W.ContainerId);if(!B||B->Health<=0){Release(W);continue;}if(W.Activity==TEXT("to_store")){if(Move(S,W,S.BuildingAccessPoint(*B),Seconds)){W.Activity=TEXT("store");W.PhaseSeconds=0;}continue;}W.PhaseSeconds+=Seconds;if(W.PhaseSeconds>=S.Number(TEXT("worker_store_seconds"))&&S.Resources[S.TextRule(TEXT("inactive_worker_resource"))].LitresPerUnit<=S.StorageRoom(*B,0,W.Id)+1.e-8){B->Inventory.FindOrAdd(S.TextRule(TEXT("inactive_worker_resource")))+=1;W.State=TEXT("stored");W.Activity=TEXT("stored");W.Outdoor=false;W.Route.Empty();W.NextWaypoint=0;}continue;}
        if(W.Activity==TEXT("idle")){W.PhaseSeconds+=Seconds;if(W.PhaseSeconds>=Number(TEXT("idle_return_seconds")))if(const auto* C=S.Core())if(FVector2D::DistSquared(W.Position,S.BuildingAccessPoint(*C))>1.e-8){W.Activity=TEXT("return");RouteTo(S,W,S.BuildingAccessPoint(*C));}}
    }
    S.Couriers.RemoveAll([](const auto& C){return C.Phase==TEXT("done");});RefreshMetrics(S);
    const int32 Target=FMath::Min(S.RobotSupportCapacity,FMath::Max(S.TotalJobs,int32(S.Number(TEXT("minimum_population")))));
    int32 Need=Target-S.Population;
    for(auto& W:Bodies)if(Need>0&&W.State==TEXT("stored")&&W.ContainerKind==TEXT("building"))
    {auto* B=S.FindBuilding(W.ContainerId);const FString Resource=S.TextRule(TEXT("inactive_worker_resource"));if(!B||B->Health<=0||B->DisassemblyQueued||B->Inventory.FindRef(Resource)-FMath::Max(double(B->WorkerExportTarget),S.Trade.Demand(S,B->Id,Resource))<1)continue;B->Inventory.FindOrAdd(Resource)-=1;W.State=TEXT("active");W.Activity=TEXT("reactivate");W.PhaseSeconds=0;--Need;}
    if(S.Population>FMath::Max(S.TotalJobs,int32(S.Number(TEXT("minimum_population")))))
    {
        int32 Outbound=0;for(const auto& Body:Bodies)if(Body.State==TEXT("active")&&(Body.Activity==TEXT("to_store")||Body.Activity==TEXT("store")||Body.Activity==TEXT("to_recycle")))++Outbound;
        if(S.Population-Outbound>FMath::Max(S.TotalJobs,int32(S.Number(TEXT("minimum_population")))))if(auto* W=IdleWorker(S,false))
        {
            for(auto& B:S.Buildings)if(B.Health>0&&!B.IsConstructing&&S.Definition(B)->StoresInactiveWorkers)
            {const double Volume=S.Resources[S.TextRule(TEXT("inactive_worker_resource"))].LitresPerUnit;if(Volume<=S.StorageRoom(B)+1.e-8){W->Activity=TEXT("to_store");W->ContainerId=B.Id;W->ContainerKind=TEXT("building");RouteTo(S,*W,S.BuildingAccessPoint(B));break;}}
            if(W->Activity==TEXT("idle")&&S.Policy->GetBoolField(TEXT("auto_disassemble_storage_full")))if(auto* C=S.Core())if(!C->IsConstructing&&!C->DisassemblyCommitted&&S.InventoryLitres(S.DisassemblyOutputs())<=S.StorageRoom(*C)+1.e-8){W->Activity=TEXT("to_recycle");W->ContainerId=C->Id;W->ContainerKind=TEXT("building");RouteTo(S,*W,S.BuildingAccessPoint(*C));}
        }
    }
    Schedule(S);RefreshMetrics(S);
}
bool FSeigeWorkerSystem::BeginDisassembly(FSeigeSimulation& S,int32 Building)
{for(auto& W:Bodies)if(W.State==TEXT("stored")&&W.ContainerKind==TEXT("building")&&W.ContainerId==Building){W.State=TEXT("disassembling");W.Activity=TEXT("disassembling");return true;}return false;}
void FSeigeWorkerSystem::FinishDisassembly(FSeigeSimulation& S,int32 Building){for(auto& W:Bodies)if(W.State==TEXT("disassembling")&&W.ContainerId==Building){RecyclingWasteKg+=S.Resources[S.TextRule(TEXT("inactive_worker_resource"))].UnitMassKg-S.InventoryMassKg(S.DisassemblyOutputs());W.State=TEXT("disassembled");W.Activity=TEXT("terminal");return;}}
void FSeigeWorkerSystem::KillCourier(FSeigeSimulation& S,int32 Id)
{auto* C=S.Couriers.FindByPredicate([&](const auto& V){return V.Id==Id;});if(!C)return;if(auto* W=Find(C->WorkerId)){W->State=TEXT("destroyed");W->Activity=TEXT("terminal");W->Outdoor=false;W->DeliveryId=0;W->Route.Empty();W->NextWaypoint=0;}S.Couriers.RemoveAll([&](const auto& V){return V.Id==Id;});++S.LostCouriers;RefreshMetrics(S);}
void FSeigeWorkerSystem::OnBuildingDestroyed(FSeigeSimulation& S,int32 Id)
{
    const auto* B=S.FindBuilding(Id);const bool Command=B&&S.Definition(*B)->Role==TEXT("core");
    for(auto& W:Bodies)
    {
        const bool Live=W.State==TEXT("active")||W.State==TEXT("stored")||W.State==TEXT("disassembling")||W.State==TEXT("shipment");
        const bool Contained=Live&&(W.ContainerKind==TEXT("building")||W.ContainerKind==TEXT("shipment"))&&W.ContainerId==Id&&!W.Outdoor;
        if(Contained)
        {
            const bool Aboard=Command&&W.State==TEXT("active")&&(W.Activity==TEXT("aboard")||W.Activity==TEXT("exit"));
            if(W.DeliveryId){const int32 Task=W.DeliveryId;S.Couriers.RemoveAll([&](const auto& C){return C.Id==Task;});++S.LostCouriers;}
            W.State=Aboard?TEXT("evacuated"):TEXT("destroyed");W.Activity=TEXT("terminal");W.DeliveryId=W.BuildingId=W.RoadId=0;W.Route.Empty();W.NextWaypoint=0;
        }
        else if(W.BuildingId==Id)Release(W);
    }
    // Uncollected material perishes with its source. Cancel its claim now,
    // including when core loss ends the simulation before another worker tick.
    // The exterior worker stays alive in place; already carried cargo remains
    // on its existing body and follows normal destination/return handling.
    for(int32 I=S.Couriers.Num()-1;I>=0;--I)
    {
        const auto& C=S.Couriers[I];
        if(C.SourceId!=Id||C.SelfTransfer||C.Amount>0||C.ReservedAmount<=0)continue;
        if(auto* W=Find(C.WorkerId))if(W->State==TEXT("active"))Release(*W);
        S.Couriers.RemoveAt(I);
    }
    RefreshMetrics(S);
}
void FSeigeWorkerSystem::Evacuate(){for(auto& W:Bodies)if(W.State==TEXT("active")&&!W.Outdoor&&(W.Activity==TEXT("aboard")||W.Activity==TEXT("exit"))){W.State=TEXT("evacuated");W.Activity=TEXT("terminal");}}
bool FSeigeWorkerSystem::MoveStored(int32 Source,const FString& Kind,int32 Id,int32 Count)
{if(StoredAt(Source)<Count)return false;for(auto& W:Bodies)if(Count>0&&W.State==TEXT("stored")&&W.ContainerKind==TEXT("building")&&W.ContainerId==Source){W.State=Kind==TEXT("building")?TEXT("stored"):Kind;W.Activity=W.State;W.ContainerKind=Kind;W.ContainerId=Id;--Count;}return true;}
bool FSeigeWorkerSystem::ReceiveStored(const FSeigeSimulation& S,const FString& Kind,int32 Id,int32 Building,int32 Count)
{const auto* B=S.FindBuilding(Building);if(!B)return false;int32 Have=0;for(const auto& W:Bodies)if(W.State==Kind&&W.ContainerKind==Kind&&W.ContainerId==Id)++Have;if(Have<Count)return false;for(auto& W:Bodies)if(Count>0&&W.State==Kind&&W.ContainerKind==Kind&&W.ContainerId==Id){W.State=TEXT("stored");W.Activity=TEXT("stored");W.ContainerKind=TEXT("building");W.ContainerId=Building;W.Position=S.BuildingAccessPoint(*B);--Count;}return true;}
bool FSeigeWorkerSystem::TransferStoredTo(FSeigeWorkerSystem& Other,const FString& FromKind,int32 FromId,const FString& ToKind,int32 ToId,int32 Count)
{TArray<int32> Indices;for(int32 I=0;I<Bodies.Num()&&Indices.Num()<Count;++I)if(Bodies[I].ContainerKind==FromKind&&Bodies[I].ContainerId==FromId&&(Bodies[I].State==TEXT("stored")||Bodies[I].State==TEXT("vehicle")))Indices.Add(I);if(Indices.Num()!=Count)return false;for(int32 I:Indices)if(Other.Find(Bodies[I].Id))return false;for(int32 I=Indices.Num()-1;I>=0;--I){FSeigeWorker W=Bodies[Indices[I]];W.ContainerKind=ToKind;W.ContainerId=ToId;W.State=ToKind==TEXT("building")?TEXT("stored"):ToKind;W.Activity=W.State;Other.Bodies.Add(W);Bodies.RemoveAt(Indices[I]);}return true;}
int32 FSeigeWorkerSystem::RoadId(const FSeigeSimulation& S,const FSeigeWorker& W) const
{FSeigeCourier C;C.Position=W.Position;C.Route=W.Route;C.NextWaypoint=W.NextWaypoint;return S.CourierRoadId(C);}
