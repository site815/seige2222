#include "SeigeWorkers.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"

int32 FSeigeWorkerSystem::AssignedCount(int32 Id) const { int32 N=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&W.CrewBuildingId==Id)++N;return N; }
int32 FSeigeWorkerSystem::ActiveCount() const { int32 N=0;for(const auto& W:Bodies)if(W.State==TEXT("active"))++N;return N; }
int32 FSeigeWorkerSystem::StoredCount() const { int32 N=0;for(const auto& W:Bodies)if(W.State==TEXT("stored"))++N;return N; }
int32 FSeigeWorkerSystem::OwnedCount() const { int32 N=0;for(const auto& W:Bodies)if(W.State==TEXT("active")||W.State==TEXT("stored")||W.State==TEXT("shipment")||W.State==TEXT("vehicle")||W.State==TEXT("disassembling"))++N;return N; }
int32 FSeigeWorkerSystem::IdleSurplusCount() const { int32 N=0;for(const auto& W:Bodies)if(W.State==TEXT("active")&&!W.CrewBuildingId)++N;return N; }

bool FSeigeWorkerSystem::CanLendCoreCrew(const FSeigeSimulation& S,const FSeigeWorker& W) const
{
    const auto* Core=S.FindBuilding(W.CrewBuildingId);
    if(!Core||S.Definition(*Core)->Role!=TEXT("core")||Core->IsConstructing)return true;
    const int32 Required=FMath::Min(S.Definition(*Core)->Jobs,int32(Number(TEXT("core_minimum_operators"))));
    int32 Remaining=0;
    for(const auto& Other:Bodies)
        if(Other.Id!=W.Id&&Other.State==TEXT("active")&&Other.CrewBuildingId==Core->Id&&Other.BatteryKWh>0&&!Other.ChargingBuildingId&&!Other.TransportVehicleId&&!Other.DeliveryId&&
            Other.Activity==TEXT("operate"))++Remaining;
    return Remaining>=Required;
}
double FSeigeWorkerSystem::BatteryCapacity() const {return Policy?Number(TEXT("battery_capacity_kwh")):0;}
double FSeigeWorkerSystem::RecoveryFraction() const {return Policy?Number(TEXT("core_recovery_fraction")):0;}
void FSeigeWorkerSystem::InitializeBattery(FSeigeWorker& W,int32 Sequence) const
{
    W.Health=MaximumHealth();W.BatteryCapacityKWh=BatteryCapacity();
    // The landing manifest includes charged batteries. Stagger initial trips;
    // later fabrication supplies one fully charged battery through its recipe.
    W.BatteryKWh=W.BatteryCapacityKWh*(1.-Number(TEXT("initial_charge_stagger_fraction"))*(Sequence%11)/10.);
}
double FSeigeWorkerSystem::ChargeSessionSeconds(const FSeigeSimulation& S) const
{
    const auto& Calendar=S.Calendar.GetRules();
    return double(Calendar.DaylightMicroseconds+Calendar.NightMicroseconds)/1.e6*Number(TEXT("charge_minutes_per_day"))/(24.*60.);
}
int32 FSeigeWorkerSystem::ChargerBerths(const FSeigeSimulation& S,int32 Id) const
{
    const auto* B=S.FindBuilding(Id);const auto* D=B?S.Definition(*B):nullptr;
    if(!D||B->Health<=0||!B->Enabled||(B->IsConstructing&&D->Role!=TEXT("core")))return 0;
    return D->Role==TEXT("core")?int32(Number(TEXT("core_charging_berths"))):D->Role==TEXT("service")?int32(Number(TEXT("station_charging_berths"))):0;
}
FVector2D FSeigeWorkerSystem::ChargingQueuePosition(const FSeigeSimulation& S,const FSeigeBuilding& Charger,int32 Slot) const
{
    const auto Access=S.Definition(Charger)->AccessPort;
    const double Side=(Slot%2?1.:-1.)*((Slot+1)/2)*Number(TEXT("charging_queue_spacing_meters"))/S.MetersPerWorldUnit();
    return S.BuildingAccessPoint(Charger)+FVector2D(-Access.Y,Access.X)*Side;
}
double FSeigeWorkerSystem::ChargingTravelSeconds(const FSeigeSimulation& S,FVector2D From,FVector2D To,double Limit,const FSeigeBuilding* ProspectivePlot) const
{
    if(From.Equals(To,1.e-5))return 0.;
    const auto* Profile=S.Mobility.Profile(TEXT("drone"));if(!Profile)return DBL_MAX;
    const double SampleLength=Profile->SampleMeters/S.MetersPerWorldUnit();
    TArray<FVector2D> Route;if(!S.FindRoute(From,To,Route,-1,false,ProspectivePlot))return DBL_MAX;
    double Seconds=0.;int32 Samples=0;
    for(const auto& Point:Route)
    {
        while(!From.Equals(Point,1.e-5))
        {
            // Mobility.Speed deliberately samples only the next authored
            // interval. Walk each interval here too, with a bounded search
            // budget; treating a whole long edge as flat is optimistic.
            if(++Samples>4096)return DBL_MAX;
            const double Distance=FVector2D::Distance(From,Point);
            const double Step=FMath::Min(Distance,SampleLength);
            const FVector2D Next=From+(Point-From)*(Step/Distance);
            const double Speed=S.Mobility.Speed(S,TEXT("drone"),From,Next)/S.MetersPerWorldUnit()*S.Companions.WorkerSpeedMultiplier(S);
            if(Speed<=0)return DBL_MAX;
            Seconds+=Step/Speed;if(Seconds>Limit)return DBL_MAX;
            From=Next;
        }
        From=Point;
    }
    return Seconds;
}
bool FSeigeWorkerSystem::WorkstationHasChargingAccess(const FSeigeSimulation& S,FVector2D Position,const FSeigeBuilding* ProspectivePlot,int32 AdditionalQueued) const
{
    const double DaySeconds=double(S.Calendar.GetRules().DaylightMicroseconds+S.Calendar.GetRules().NightMicroseconds)/1.e6;
    const double Session=ChargeSessionSeconds(S);
    const double ReserveSeconds=Number(TEXT("charge_reserve_fraction"))*FMath::Max(1.,DaySeconds-Session);
    for(const auto& B:S.Buildings)if(ChargerBerths(S,B.Id)>0)
    {
        // The core's automatic berth is available during bootstrap. Ordinary
        // charging stations require their existing, real permanent attendant.
        if(S.Definition(B)->Role==TEXT("service")&&AssignedCount(B.Id)<1)continue;
        if(!S.Energy.CanConsume(S,B.Id,BatteryCapacity()/Session))continue;
        int32 Queue=0;for(const auto& W:Bodies)if(W.ChargingBuildingId==B.Id)++Queue;
        int32 Slot=0;
        for(int32 Candidate=0;Candidate<=AdditionalQueued;++Candidate)
        {do{++Slot;}while(Bodies.ContainsByPredicate([&](const auto& W){return W.ChargingBuildingId==B.Id&&W.ChargeQueueSlot==Slot;}));}
        const FVector2D Waiting=ChargingQueuePosition(S,B,Slot),Port=S.BuildingAccessPoint(B);
        const double Walk=ChargingTravelSeconds(S,Position,Waiting,ReserveSeconds,ProspectivePlot),Dock=ChargingTravelSeconds(S,Waiting,Port,ReserveSeconds,ProspectivePlot);
        // Use actual terrain travel plus a conservative serial queue. No
        // stagger bonus, future charger, road or energy grant is assumed.
        if(Walk!=DBL_MAX&&Dock!=DBL_MAX&&Walk+Dock+(Queue+AdditionalQueued+1)*Session<ReserveSeconds)return true;
    }
    return false;
}
void FSeigeWorkerSystem::AssignCrews(FSeigeSimulation& S)
{
    auto FreeTask=[](const FSeigeWorker& W){return W.Activity==TEXT("idle")||W.Activity==TEXT("return")||W.Activity==TEXT("operate")||W.Activity==TEXT("to_job")||W.Activity==TEXT("to_store")||W.Activity==TEXT("store");};
    for(auto& W:Bodies)if(W.State==TEXT("active")&&W.CrewBuildingId)
    {
        const auto* B=S.FindBuilding(W.CrewBuildingId);
        if(!B||B->Health<=0){W.CrewBuildingId=0;if(FreeTask(W))Release(W);}
    }
    // Retain crews through upgrades and temporary power/enable shortages.
    // New facilities receive existing unassigned bodies, then stored bodies,
    // then core generalists. Only fabrication increases total ownership.
    TArray<int32> Targets;for(const auto& B:S.Buildings)if(B.Health>0&&(!B.IsConstructing||S.Definition(B)->Role==TEXT("core")))Targets.Add(B.Id);
    Targets.StableSort([&](int32 A,int32 B){const bool AC=S.Definition(*S.FindBuilding(A))->Role==TEXT("core"),BC=S.Definition(*S.FindBuilding(B))->Role==TEXT("core");return AC!=BC?!AC:A<B;});
    for(int32 Id:Targets)
    {
        auto* B=S.FindBuilding(Id);const auto* D=S.Definition(*B);
        int32 Have=AssignedCount(Id);
        // A disabled facility keeps its crew; changing a modded capacity can
        // release excess bodies, never erase them or allow cap overflow.
        for(int32 I=Bodies.Num()-1;Have>D->Jobs&&I>=0;--I)if(Bodies[I].State==TEXT("active")&&Bodies[I].CrewBuildingId==Id&&FreeTask(Bodies[I])){Bodies[I].CrewBuildingId=0;Release(Bodies[I]);--Have;}
        while(Have<D->Jobs)
        {
            FSeigeWorker* Candidate=nullptr;
            for(auto& W:Bodies)if(W.State==TEXT("active")&&!W.CrewBuildingId&&FreeTask(W)){Candidate=&W;break;}
            if(!Candidate)for(auto& W:Bodies)if(W.State==TEXT("stored")&&W.ContainerKind==TEXT("building"))
            {
                auto* Container=S.FindBuilding(W.ContainerId);const FString Resource=S.TextRule(TEXT("inactive_worker_resource"));
                if(!Container||Container->Health<=0||Container->Inventory.FindRef(Resource)-S.Trade.Demand(S,Container->Id,Resource)<1)continue;
                Container->Inventory.FindOrAdd(Resource)-=1;W.State=TEXT("active");W.Activity=TEXT("reactivate");W.PhaseSeconds=0;Candidate=&W;break;
            }
            if(!Candidate&&D->Role!=TEXT("core"))if(const auto* Core=S.Core())
                for(auto& W:Bodies)if(W.State==TEXT("active")&&W.CrewBuildingId==Core->Id&&FreeTask(W)&&CanLendCoreCrew(S,W)){Candidate=&W;break;}
            if(!Candidate)break;
            if(Candidate->Activity!=TEXT("reactivate"))Release(*Candidate);
            Candidate->CrewBuildingId=Id;++Have;
        }
    }
}
FSeigeWorker* FSeigeWorkerSystem::TaskWorker(FSeigeSimulation& S,int32 Source,int32 Target,bool Construction,const TSet<FString>* Excluded)
{
    auto Available=[&](const FSeigeWorker& W)
    {
        if(Excluded&&Excluded->Contains(W.Id))return false;
        if(W.State!=TEXT("active")||W.BatteryKWh<=0||W.ChargingBuildingId||W.TransportVehicleId||!(W.Activity==TEXT("idle")||W.Activity==TEXT("operate")||W.Activity==TEXT("to_job")))return false;
        // A station cannot lend its last attendant while accepted customers
        // rely on that person to charge. The customer is a physical body, not
        // a promise that can disappear when a construction delivery starts.
        const auto* Home=S.FindBuilding(W.CrewBuildingId);
        if(Home&&S.Definition(*Home)->Role==TEXT("service")&&Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W.Id&&Other.ChargingBuildingId==Home->Id;}))
            return Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W.Id&&Other.CrewBuildingId==Home->Id&&Other.Activity==TEXT("operate")&&Other.BatteryKWh>0&&!Other.ChargingBuildingId;});
        return true;
    };
    // The receiving or shipping building lends one of its own real crew.
    for(int32 Owner:{Source,Target})for(auto& W:Bodies)if(W.CrewBuildingId==Owner&&Available(W)&&CanLendCoreCrew(S,W))return &W;
    if(Construction)for(auto& W:Bodies)
    {
        if(Excluded&&Excluded->Contains(W.Id))continue;
        const auto* Home=S.FindBuilding(W.CrewBuildingId);if(!Home)continue;
        const auto& Role=S.Definition(*Home)->Role;
        if(Role!=TEXT("core")&&Role!=TEXT("construction"))continue;
        if(Available(W)&&CanLendCoreCrew(S,W))return &W;
        // Builders waiting for a load may fetch it. They cease construction
        // while absent; there is no permanent invisible hauling reserve.
        if(W.State==TEXT("active")&&W.BatteryKWh>0&&!W.ChargingBuildingId&&!W.TransportVehicleId&&(W.Activity==TEXT("build")||W.Activity==TEXT("road_build")))
        {
            const auto* Site=S.FindBuilding(W.BuildingId);const auto* Road=S.FindRoad(W.RoadId);
            bool Waiting=false;
            if(Site)for(const auto& P:S.ConstructionCost(*Site))if(P.Value>0&&Site->ConstructionMaterials.FindRef(P.Key)<=1.e-8)Waiting=true;
            if(Road)for(const auto& P:S.RoadCost(Road->A,Road->B,Road->TargetTier))if(P.Value>0&&Road->ConstructionMaterials.FindRef(P.Key)<=1.e-8)Waiting=true;
            if(Waiting&&CanLendCoreCrew(S,W))return &W;
        }
    }
    return nullptr;
}
FString FSeigeWorkerSystem::AcquireTransportHandler(FSeigeSimulation& S,int32 Vehicle,int32 Building,int32 Road,FVector2D Position,bool Construction)
{
    for(const auto& W:Bodies)if(W.TransportVehicleId==Vehicle&&W.State==TEXT("active")&&(W.Activity==TEXT("to_handle")||W.Activity==TEXT("handling")))return W.Id;
    // Test every eligible physical worker. An inaccessible first worker must
    // not starve all other reachable operators at this cargo door.
    TSet<FString> Tried;
    while(auto* W=TaskWorker(S,Building,Building,Construction||Road!=0,&Tried))
    {
        Tried.Add(W->Id);FSeigeWorker Candidate=*W;RouteTo(S,Candidate,Position);
        if(Candidate.Route.IsEmpty()&&!Candidate.Position.Equals(Position,1.e-5))continue;
        Release(*W);W->TransportVehicleId=Vehicle;W->HandlingPosition=Position;W->Activity=TEXT("to_handle");
        W->Route=MoveTemp(Candidate.Route);W->NextWaypoint=0;W->RouteRevision=Candidate.RouteRevision;W->RetryAt=Candidate.RetryAt;return W->Id;
    }
    return {};
}
bool FSeigeWorkerSystem::TransportHandlingReady(const FString& Id,int32 Vehicle) const
{const auto* W=Find(Id);return W&&W->State==TEXT("active")&&W->TransportVehicleId==Vehicle&&W->Activity==TEXT("handling")&&W->BatteryKWh>0&&W->Position.Equals(W->HandlingPosition,1.e-6);}
void FSeigeWorkerSystem::ReleaseTransportHandler(const FString& Id,int32 Vehicle)
{if(auto* W=Find(Id))if(W->TransportVehicleId==Vehicle)Release(*W);}

void FSeigeWorkerSystem::ResumeAfterCharging(FSeigeSimulation& S,FSeigeWorker& W)
{
    W.ChargingBuildingId=0;W.ChargeQueueSlot=0;W.Activity=W.SuspendedActivity;W.SuspendedActivity.Empty();
    W.PhaseSeconds=0;W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;
    // Aborted charging and completed charging have the same spatial contract:
    // travel back to the workstation or cargo endpoint before doing work.
    if(W.Activity==TEXT("operate"))W.Activity=TEXT("to_job");
    else if(W.Activity==TEXT("build"))W.Activity=TEXT("to_build");
    else if(W.Activity==TEXT("road_build"))W.Activity=TEXT("to_road");
    else if(W.Activity==TEXT("store"))W.Activity=TEXT("to_store");
    // A saved or interrupted charging task can return after another real body
    // has occupied its old construction slot. Do not exceed authored crew caps.
    if(W.Activity==TEXT("to_build"))
    {
        const auto* Site=S.FindBuilding(W.BuildingId);int32 OtherBuilders=0;
        for(const auto& Other:Bodies)if(Other.Id!=W.Id&&Other.BuildingId==W.BuildingId&&(Other.Activity==TEXT("build")||Other.Activity==TEXT("to_build")))++OtherBuilders;
        if(!Site||!Site->IsConstructing||OtherBuilders>=S.RequiredBuilders(*Site))Release(W);
    }
    else if(W.Activity==TEXT("to_road"))
    {
        const auto* Road=S.FindRoad(W.RoadId);int32 OtherBuilders=0;
        for(const auto& Other:Bodies)if(Other.Id!=W.Id&&Other.RoadId==W.RoadId&&(Other.Activity==TEXT("road_build")||Other.Activity==TEXT("to_road")))++OtherBuilders;
        const auto* Tier=Road?S.TransportTiers.Find(Road->TargetTier):nullptr;
        if(!S.RoadsEnabled()||!Road||!Road->IsConstructing||!Tier||OtherBuilders>=Tier->ConstructionWorkers)Release(W);
    }
    if(W.DeliveryId)
    {
        if(auto* C=S.Couriers.FindByPredicate([&](const auto& V){return V.Id==W.DeliveryId;}))
        {
            if(C->Phase==TEXT("loading"))C->Phase=TEXT("pickup");
            if(C->Phase==TEXT("unloading"))C->Phase=TEXT("carrying");
            C->PhaseSeconds=0;C->Route.Empty();C->NextWaypoint=0;C->Position=W.Position;
        }
        else Release(W);
    }
    if(W.Activity.IsEmpty())Release(W);
}

bool FSeigeWorkerSystem::StepBattery(FSeigeSimulation& S,FSeigeWorker& W,double Seconds)
{
    if(W.State!=TEXT("active")||W.Activity==TEXT("aboard"))return false;
    const double DaySeconds=double(S.Calendar.GetRules().DaylightMicroseconds+S.Calendar.GetRules().NightMicroseconds)/1.e6;
    const double Session=ChargeSessionSeconds(S),Drain=BatteryCapacity()/FMath::Max(1.,DaySeconds-Session);
    const double ChargeSlice=BatteryCapacity()*Seconds/Session;
    auto HasAttendant=[&](int32 Id){return Bodies.ContainsByPredicate([&](const auto& Other){return Other.CrewBuildingId==Id&&Other.BuildingId==Id&&Other.State==TEXT("active")&&Other.Activity==TEXT("operate")&&Other.BatteryKWh>0&&!Other.ChargingBuildingId&&!Other.DeliveryId;});};
    const bool Charging=W.ChargingBuildingId!=0;
    if(W.Activity!=TEXT("charging"))W.BatteryKWh=FMath::Max(0.,W.BatteryKWh-Drain*Seconds);
    if(W.BatteryKWh<=0&&W.DeliveryId)
        if(auto* Courier=S.Couriers.FindByPredicate([&](const auto& C){return C.Id==W.DeliveryId;}))
            if(!Courier->SelfTransfer&&Courier->Amount<=1.e-8&&Courier->ReservedAmount>0)
            {
                // Nothing has left the source yet. A powerless carrier must
                // not hold the only materials for an entire road indefinitely.
                // Carried cargo and the stranded physical body stay in place.
                Courier->ReservedAmount=0;Courier->Phase=TEXT("done");Release(W);
            }
    if(!Charging)
    {
        // Planning is derived state, so losing its cache during load must not
        // shift the next recharge decision. Use the authored retry cadence on
        // the saved simulation clock, not time since this cache was created.
        // Exact routes below still confirm every actual charging departure.
        const double Cadence=Number(TEXT("route_retry_seconds"));
        const double ClockTolerance=1.e-7;
        if(FMath::FloorToDouble((S.Time+ClockTolerance)/Cadence)==FMath::FloorToDouble((S.Time-Seconds+ClockTolerance)/Cadence))return W.BatteryKWh<=0;
        const double Threshold=Number(TEXT("charge_reserve_fraction"))+Number(TEXT("charge_reserve_stagger_fraction"))*(GetTypeHash(W.Id)%101)/100.;
        auto Eligible=[&](const FSeigeBuilding& B)
        {
            return ChargerBerths(S,B.Id)>0&&S.Energy.CanConsume(S,B.Id,ChargeSlice)&&
                (S.Definition(B)->Role!=TEXT("service")||B.Id==W.CrewBuildingId||HasAttendant(B.Id));
        };
        auto FreeSlot=[&](int32 Id)
        {int32 Slot=1;while(Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W.Id&&Other.ChargingBuildingId==Id&&Other.ChargeQueueSlot==Slot;}))++Slot;return Slot;};
        auto QueueSeconds=[&](int32 Id)
        {int32 Queue=0;for(const auto& Other:Bodies)if(Other.Id!=W.Id&&Other.ChargingBuildingId==Id)++Queue;return (Queue+1)*Session;};
        const double SpeedScale=S.Companions.WorkerSpeedMultiplier(S);
        const bool SameWorld=W.ChargePlanTime>=0&&W.ChargePlanTime<=S.Time&&W.ChargePlanTopology==S.TransportRevision&&W.ChargePlanTerrain==S.Mobility.GetTerrainRevision()&&W.ChargePlanCrew==W.CrewBuildingId&&W.ChargePlanSpeed==SpeedScale;
        const auto* Cached=SameWorld?S.FindBuilding(W.ChargePlanBuilding):nullptr;
        bool Valid=Cached&&Eligible(*Cached)&&FreeSlot(Cached->Id)==W.ChargePlanSlot;
        auto Plan=[&]()
        {
            W.ChargePlanBuilding=W.ChargePlanSlot=0;W.ChargePlanTravelSeconds=DBL_MAX;
            double BestCost=DBL_MAX;
            for(const auto& B:S.Buildings)if(Eligible(B))
            {
                const bool OwnStation=B.Id==W.CrewBuildingId&&S.Definition(B)->Role==TEXT("service");
                const int32 Slot=FreeSlot(B.Id);const auto Waiting=ChargingQueuePosition(S,B,Slot);
                const double Limit=W.BatteryCapacityKWh/Drain;
                const double Walk=ChargingTravelSeconds(S,W.Position,Waiting,Limit),Dock=ChargingTravelSeconds(S,Waiting,S.BuildingAccessPoint(B),Limit);
                if(Walk==DBL_MAX||Dock==DBL_MAX)continue;
                const double Cost=Walk+Dock+QueueSeconds(B.Id);if(Cost>Limit)continue;
                if(OwnStation||Cost<BestCost){W.ChargePlanBuilding=B.Id;W.ChargePlanSlot=Slot;W.ChargePlanTravelSeconds=Walk+Dock;BestCost=Cost;}
                // Retain the attendant's existing self-service preference.
                if(OwnStation)break;
            }
            W.ChargePlanPosition=W.Position;W.ChargePlanTime=S.Time;W.ChargePlanRetryAt=(FMath::FloorToDouble((S.Time+ClockTolerance)/Cadence)+1)*Cadence;
            W.ChargePlanTopology=S.TransportRevision;W.ChargePlanTerrain=S.Mobility.GetTerrainRevision();W.ChargePlanCrew=W.CrewBuildingId;W.ChargePlanSpeed=SpeedScale;
            return W.ChargePlanBuilding>0;
        };
        bool Fresh=false;
        if(!Valid)
        {
            // Failed/unavailable plans retry on the existing authored cadence,
            // never running terrain searches for every fixed-step body update.
            if(SameWorld&&S.Time+ClockTolerance<W.ChargePlanRetryAt)return W.BatteryKWh<=0;
            Valid=Plan();Fresh=true;
            if(!Valid)return W.BatteryKWh<=0;
        }
        else
        {
            // Without topology changes the body can retrace its actual walk.
            // This conservative bound makes moving bodies return early; a new
            // exact route is needed only as they approach their charge reserve.
            if(!W.Position.Equals(W.ChargePlanPosition,1.e-5))
            {
                const auto* P=S.Mobility.Profile(TEXT("drone"));
                W.ChargePlanTravelSeconds+=FMath::Max(0.,S.Time-W.ChargePlanTime)/FMath::Max(.0001,P?FMath::Min(1.,P->RoughMultiplier):1.);
            }
            W.ChargePlanPosition=W.Position;W.ChargePlanTime=S.Time;
        }
        double ReturnSeconds=W.ChargePlanTravelSeconds+QueueSeconds(W.ChargePlanBuilding);
        if(W.BatteryKWh>W.BatteryCapacityKWh*Threshold+Drain*ReturnSeconds)return false;
        if(!Fresh)
        {
            if(S.Time+ClockTolerance<W.ChargePlanRetryAt)return W.BatteryKWh<=0;
            if(!Plan())return W.BatteryKWh<=0;
            ReturnSeconds=W.ChargePlanTravelSeconds+QueueSeconds(W.ChargePlanBuilding);
            if(W.BatteryKWh>W.BatteryCapacityKWh*Threshold+Drain*ReturnSeconds)return false;
        }
        // Terrain changes or a genuinely out-of-range job can make departure
        // unaffordable. Do not commit a charging trip or promise a berth; the
        // body retains its task/cargo and needs a nearer powered charger.
        if(W.BatteryKWh<=Drain*ReturnSeconds)return W.BatteryKWh<=0;
        // Finish attending the current short session before taking this same
        // berth. Otherwise the sole attendant and its customer can deadlock.
        if(W.Activity==TEXT("operate")&&W.BatteryKWh>0)
            if(const auto* Home=S.FindBuilding(W.CrewBuildingId))if(S.Definition(*Home)->Role==TEXT("service"))
                if(W.BatteryKWh>Drain*(Session*2)&&Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W.Id&&Other.ChargingBuildingId==Home->Id&&(Other.Activity==TEXT("charging")||Other.Activity==TEXT("berth_entry")&&Other.BatteryKWh>0);}))return false;
        // Never abandon an accepted vehicle load/unload in place; relinquish
        // the temporary handler so a different physical crew member can help.
        if(W.TransportVehicleId){W.TransportVehicleId=0;Release(W);}
        W.ChargingBuildingId=W.ChargePlanBuilding;W.ChargeQueueSlot=W.ChargePlanSlot;W.ChargePlanTime=-1;
        W.SuspendedActivity=W.Activity;W.Activity=TEXT("to_charge");W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;
    }
    auto* Charger=S.FindBuilding(W.ChargingBuildingId);
    if(!Charger||ChargerBerths(S,W.ChargingBuildingId)<=0)
    {ResumeAfterCharging(S,W);return true;}
    if(W.BatteryKWh>0&&!S.Energy.CanConsume(S,Charger->Id,ChargeSlice))
    {ResumeAfterCharging(S,W);return true;}
    const auto Port=S.BuildingAccessPoint(*Charger);
    const FVector2D QueuePosition=ChargingQueuePosition(S,*Charger,W.ChargeQueueSlot);
    if(W.Activity==TEXT("to_charge"))
    {
        if(W.BatteryKWh>0&&Move(S,W,QueuePosition,Seconds)){W.Activity=TEXT("charge_queue");W.PhaseSeconds=0;}
    }
    else if(W.Activity==TEXT("charge_queue"))
    {
        int32 Occupied=0;for(const auto& Other:Bodies)if(Other.Id!=W.Id&&Other.ChargingBuildingId==Charger->Id&&(Other.Activity==TEXT("charging")||Other.Activity==TEXT("berth_entry")&&Other.BatteryKWh>0))++Occupied;
        const bool Core=S.Definition(*Charger)->Role==TEXT("core");
        const bool Attendant=W.CrewBuildingId==Charger->Id;
        const bool AttendantWaiting=Bodies.ContainsByPredicate([&](const auto& Other){return Other.CrewBuildingId==Charger->Id&&Other.ChargingBuildingId==Charger->Id;});
        const bool ReadyAttendant=Bodies.ContainsByPredicate([&](const auto& Other){return Other.CrewBuildingId==Charger->Id&&Other.BuildingId==Charger->Id&&Other.State==TEXT("active")&&Other.Activity==TEXT("operate")&&!Other.ChargingBuildingId&&!Other.DeliveryId&&Other.BatteryKWh>Drain*(Session*3);});
        // Queue selection is deterministic; one berth includes the attendant.
        // A fresh guest must leave its real attendant enough energy to finish
        // the session and still enter the berth itself afterwards.
        if(W.BatteryKWh>0&&Occupied<ChargerBerths(S,Charger->Id)&&(Core||Attendant||ReadyAttendant&&!AttendantWaiting)){W.Activity=TEXT("berth_entry");W.PhaseSeconds=0;W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;}
    }
    else if(W.Activity==TEXT("berth_entry"))
    {
        // A body that depleted before docking cannot hold an admission slot.
        // It waits where it stopped, retaining its cargo and suspended task.
        if(W.BatteryKWh<=0){W.Activity=TEXT("charge_queue");W.PhaseSeconds=0;W.Route.Empty();W.NextWaypoint=0;}
        else if(Move(S,W,Port,Seconds)){W.Activity=TEXT("charging");W.PhaseSeconds=0;}
    }
    else if(W.Activity==TEXT("charging"))
    {
        const auto* D=S.Definition(*Charger);
        const bool Attended=D->Role==TEXT("core")||HasAttendant(Charger->Id)||W.CrewBuildingId==Charger->Id;
        const double Amount=FMath::Min(W.BatteryCapacityKWh-W.BatteryKWh,BatteryCapacity()*Seconds/Session);
        // The core's automated recovery berth and a station's self-service
        // attendant session cannot deadlock the only charging operator.
        if(Attended&&S.Energy.Consume(S,Charger->Id,Amount)){W.BatteryKWh+=Amount;W.PhaseSeconds+=Seconds;W.Health=FMath::Min(MaximumHealth(),W.Health+Number(TEXT("charging_repair_hp_per_second"))*Seconds);}
        if(W.BatteryKWh+1.e-9>=W.BatteryCapacityKWh&&W.PhaseSeconds+1.e-9>=Session)
        {
            W.BatteryKWh=W.BatteryCapacityKWh;ResumeAfterCharging(S,W);
        }
    }
    if(W.DeliveryId)if(auto* C=S.Couriers.FindByPredicate([&](const auto& V){return V.Id==W.DeliveryId;})){C->Position=W.Position;C->Route=W.Route;C->NextWaypoint=W.NextWaypoint;C->RouteRevision=W.RouteRevision;}
    return true;
}
