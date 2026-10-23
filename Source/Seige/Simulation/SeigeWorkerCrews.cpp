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
        const double Threshold=Number(TEXT("charge_reserve_fraction"))+Number(TEXT("charge_reserve_stagger_fraction"))*(GetTypeHash(W.Id)%101)/100.;
        // Begin planning before the normal reserve is spent on travel and the
        // existing queue. This cheap lower bound avoids route searches on
        // every fixed step while a healthy battery is nowhere near due.
        const double Speed=FMath::Max(S.WalkingSpeed(),UE_DOUBLE_SMALL_NUMBER);
        double Earliest=DBL_MAX;
        for(const auto& B:S.Buildings)if(ChargerBerths(S,B.Id)>0)
        {
            if(!S.Energy.CanConsume(S,B.Id,ChargeSlice))continue;
            if(S.Definition(B)->Role==TEXT("service")&&B.Id!=W.CrewBuildingId&&!HasAttendant(B.Id))continue;
            int32 Queue=0;for(const auto& Other:Bodies)if(Other.ChargingBuildingId==B.Id)++Queue;
            Earliest=FMath::Min(Earliest,FVector2D::Distance(W.Position,S.BuildingAccessPoint(B))/Speed+(Queue+1)*Session);
        }
        if(Earliest==DBL_MAX)Earliest=0;
        if(W.BatteryKWh>W.BatteryCapacityKWh*Threshold+Drain*Earliest)return false;
        // Finish attending the current short session before taking this same
        // berth. Otherwise the sole attendant and its customer can deadlock.
        if(W.Activity==TEXT("operate")&&W.BatteryKWh>0)
            if(const auto* Home=S.FindBuilding(W.CrewBuildingId))if(S.Definition(*Home)->Role==TEXT("service"))
                if(W.BatteryKWh>Drain*(Session*2)&&Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W.Id&&Other.ChargingBuildingId==Home->Id&&(Other.Activity==TEXT("charging")||Other.Activity==TEXT("berth_entry")&&Other.BatteryKWh>0);}))return false;
        // Never abandon an accepted vehicle load/unload in place; relinquish
        // the temporary handler so a different physical crew member can help.
        if(W.TransportVehicleId){W.TransportVehicleId=0;Release(W);}
        int32 Best=0,BestSlot=0;double Distance=TNumericLimits<double>::Max();
        for(const auto& B:S.Buildings)if(ChargerBerths(S,B.Id)>0)
        {
            // A finished station may still be awaiting its grid road. Never
            // park that road's material carrier at an unpowered charger.
            if(!S.Energy.CanConsume(S,B.Id,ChargeSlice))continue;
            const bool OwnStation=B.Id==W.CrewBuildingId&&S.Definition(B)->Role==TEXT("service");
            if(S.Definition(B)->Role==TEXT("service")&&!OwnStation&&!HasAttendant(B.Id))continue;
            const auto Port=S.BuildingAccessPoint(B),Access=S.Definition(B)->AccessPort;
            int32 Slot=1;while(Bodies.ContainsByPredicate([&](const auto& Other){return Other.Id!=W.Id&&Other.ChargingBuildingId==B.Id&&Other.ChargeQueueSlot==Slot;}))++Slot;
            const double Side=(Slot%2?1.:-1.)*((Slot+1)/2)*Number(TEXT("charging_queue_spacing_meters"))/S.MetersPerWorldUnit();
            const FVector2D QueuePosition=Port+FVector2D(-Access.Y,Access.X)*Side;
            TArray<FVector2D> Route,DockRoute;
            // The actual queue offset can sit on a different bank than the
            // access port. Both physical legs must be traversable.
            if((!S.FindRoute(W.Position,QueuePosition,Route)&&!W.Position.Equals(QueuePosition,1.e-5))||(!S.FindRoute(QueuePosition,Port,DockRoute)&&!QueuePosition.Equals(Port,1.e-5)))continue;
            double Length=0;FVector2D Previous=W.Position;for(const auto& Point:Route){Length+=FVector2D::Distance(Previous,Point);Previous=Point;}
            Previous=QueuePosition;for(const auto& Point:DockRoute){Length+=FVector2D::Distance(Previous,Point);Previous=Point;}
            int32 Queue=0;for(const auto& Other:Bodies)if(Other.ChargingBuildingId==B.Id)++Queue;
            const double Cost=Length+(Queue+1)*Session*Speed;
            if(OwnStation||Cost<Distance){Best=B.Id;BestSlot=Slot;Distance=Cost;}
            // A station attendant uses its own self-service berth. Sending it
            // to a neighbor creates cyclic dependencies between attendants.
            if(OwnStation)break;
        }
        if(!Best)return W.BatteryKWh<=0;
        W.ChargingBuildingId=Best;W.ChargeQueueSlot=BestSlot;
        W.SuspendedActivity=W.Activity;W.Activity=TEXT("to_charge");W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;
    }
    auto* Charger=S.FindBuilding(W.ChargingBuildingId);
    if(!Charger||ChargerBerths(S,W.ChargingBuildingId)<=0)
    {ResumeAfterCharging(S,W);return true;}
    if(W.BatteryKWh>0&&!S.Energy.CanConsume(S,Charger->Id,ChargeSlice))
    {ResumeAfterCharging(S,W);return true;}
    const auto Port=S.BuildingAccessPoint(*Charger);
    const auto Access=S.Definition(*Charger)->AccessPort;
    const double Side=(W.ChargeQueueSlot%2?1.:-1.)*((W.ChargeQueueSlot+1)/2)*Number(TEXT("charging_queue_spacing_meters"))/S.MetersPerWorldUnit();
    const FVector2D QueuePosition=Port+FVector2D(-Access.Y,Access.X)*Side;
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
