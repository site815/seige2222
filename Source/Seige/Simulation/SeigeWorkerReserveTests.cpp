#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags WorkerReserveFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
bool WorkerReserveFixture(FSeigeSimulation& S,int32& RoadId,int32& ServiceId,FString& Error)
{
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false))return false;
    for(int32 I=0;I<1000&&(S.Buildings[0].IsConstructing||S.Population<7);++I)S.Tick(10);
    if(S.Buildings[0].IsConstructing||S.Population!=7||!S.Couriers.IsEmpty()){Error=TEXT("Finite kit failed to deploy and manufacture the seventh fixture body");return false;}
    const int32 CoreId=S.Buildings[0].Id;
    auto Install=[&](const FString& Id,FVector2D Position,int32& Out)
    {
        if(!S.PlaceBuilding(Id,Position,Error))return false;auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);
        for(const auto& P:Cost)if(S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)<P.Value){Error=TEXT("Finite kit cannot fund reserve fixture");return false;}
        for(const auto& P:Cost)S.FindBuilding(CoreId)->Inventory.FindOrAdd(P.Key)-=P.Value;
        B.InstalledMaterials=Cost;B.ConstructionMaterials.Empty();B.IsConstructing=false;B.ConstructionProgress=1;Out=B.Id;return true;
    };
    // Existing paid facilities isolate the scheduling boundary. Every body was
    // manufactured normally above; local fixture materials debit the real kit.
    const FVector2D Home=S.Buildings[0].Position;int32 Factory=0,SensorA=0,SensorB=0;
    if(!Install(TEXT("robot_service_bay"),Home+FVector2D(1400,0),ServiceId)||!Install(TEXT("worker_factory"),Home+FVector2D(0,1400),Factory)||!Install(TEXT("sensor"),Home+FVector2D(-1400,0),SensorA)||!Install(TEXT("sensor"),Home+FVector2D(0,-1400),SensorB))return false;
    const FVector2D Port=S.BuildingAccessPoint(*S.FindBuilding(CoreId));TArray<FVector2D> Route;
    if(!S.FindRoadRoute(Port,S.BuildingAccessPoint(*S.FindBuilding(ServiceId)),Route)){Error=TEXT("Reserve fixture requires an ordinary legal support road");return false;}
    FVector2D Previous=Port;
    for(const auto& End:Route)
    {
        if(Previous.Equals(End,.01))continue;
        if(!S.PlaceRoad(Previous,End,Error))return false;auto& Road=S.Roads.Last();const auto Cost=S.RoadCost(Road.A,Road.B,Road.TargetTier);
        for(const auto& P:Cost)if(S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)<P.Value){Error=TEXT("Finite kit cannot deliver the road bill");return false;}
        for(const auto& P:Cost)S.FindBuilding(CoreId)->Inventory.FindOrAdd(P.Key)-=P.Value;
        Road.ConstructionMaterials=Cost;RoadId=Road.Id;Previous=End;
    }
    if(!RoadId){Error=TEXT("Reserve fixture produced no support road");return false;}
    for(auto& R:S.Roads)if(R.Id!=RoadId){R.InstalledMaterials=MoveTemp(R.ConstructionMaterials);R.Tier=R.TargetTier;R.IsConstructing=false;R.ConstructionProgress=1;}
    const TArray<int32> Jobs={CoreId,ServiceId,Factory,SensorA,SensorB};int32 Index=0;
    for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active"))
    {
        W.BuildingId=W.RoadId=W.DeliveryId=W.ContainerId=0;W.ContainerKind.Empty();W.Route.Empty();W.NextWaypoint=0;W.PhaseSeconds=W.RetryAt=0;W.Outdoor=true;W.Position=Port;W.Activity=TEXT("idle");
        if(Jobs.IsValidIndex(Index)){W.BuildingId=Jobs[Index];W.Activity=TEXT("operate");W.Position=S.BuildingAccessPoint(*S.FindBuilding(W.BuildingId));}++Index;
    }
    S.Energy.Invalidate();S.Workers.RefreshMetrics(S);S.Energy.Tick(S,0);S.Workers.RefreshMetrics(S);return true;
}
void WorkerReserveSchedule(FSeigeSimulation& S){const double Step=S.FixedStepSeconds();S.Time+=Step;S.Workers.Tick(S,Step);S.Calendar.Advance(Step);}
int32 WorkerReserveIdle(const FSeigeSimulation& S){int32 N=0;for(const auto& W:S.Workers.Bodies)if(W.State==TEXT("active")&&W.Activity==TEXT("idle"))++N;return N;}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerReserveProgressTest,"Seige.Workers.SuppliedRoadBorrowsIdleReserve",WorkerReserveFlags)
bool FSeigeWorkerReserveProgressTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;int32 RoadId=0,ServiceId=0;if(!WorkerReserveFixture(S,RoadId,ServiceId,Error)){AddError(Error);return false;}
    const int32 CoreId=S.Buildings[0].Id,BodyCount=S.Workers.Bodies.Num();const auto Bill=S.RoadCost(S.FindRoad(RoadId)->A,S.FindRoad(RoadId)->B,S.FindRoad(RoadId)->TargetTier);
    TestEqual(TEXT("Four completed facilities reserve two real logistics workers"),S.Workers.LogisticsJobs(S),2);
    TestEqual(TEXT("Exactly two existing idle bodies are held at the reserve boundary"),WorkerReserveIdle(S),2);
    TestTrue(TEXT("Disconnected support prevents the demanded workforce from being admitted"),S.RobotSupportCapacity<S.TotalJobs&&!S.IsRoadGridConnected(CoreId,ServiceId));
    FSeigeSimulation Partial=S;auto It=Bill.CreateConstIterator();const FString Resource=It.Key();const double Undelivered=It.Value()*.5;
    Partial.FindRoad(RoadId)->ConstructionMaterials.FindOrAdd(Resource)-=Undelivered;Partial.FindBuilding(CoreId)->Inventory.FindOrAdd(Resource)+=Undelivered;
    WorkerReserveSchedule(Partial);
    TestEqual(TEXT("An incomplete bill keeps its needed hauling reserve"),Partial.FindRoad(RoadId)->Builders,0);
    TestEqual(TEXT("Incomplete work does not consume either idle hauling body"),WorkerReserveIdle(Partial),2);
    FSeigeSimulation Freight=S;
    if(!Freight.Workers.Dispatch(Freight,CoreId,ServiceId,0,TEXT("alloy"),1,false)){AddError(TEXT("Existing idle body must accept a real freight claim"));return false;}
    const FString Carrier=Freight.Couriers[0].WorkerId;const int32 Delivery=Freight.Couriers[0].Id;
    WorkerReserveSchedule(Freight);
    TestEqual(TEXT("Outstanding freight retains the ordinary adaptive reserve"),Freight.FindRoad(RoadId)->Builders,0);
    TestTrue(TEXT("Reserve lending cannot preempt a real carrier or its pickup"),Freight.Workers.Find(Carrier)->DeliveryId==Delivery&&Freight.Couriers.Num()==1);
    TSet<FString> IdleIds;TMap<FString,FVector2D> Positions;for(const auto& W:S.Workers.Bodies){Positions.Add(W.Id,W.Position);if(W.Activity==TEXT("idle"))IdleIds.Add(W.Id);}
    WorkerReserveSchedule(S);const auto* Builder=S.Workers.Bodies.FindByPredicate([&](const auto& W){return W.RoadId==RoadId;});
    if(!TestNotNull(TEXT("The fully supplied road receives one of the real reserved bodies"),Builder))return false;
    const FString BuilderId=Builder->Id;
    TestTrue(TEXT("The idle body receives a walking assignment without teleporting"),IdleIds.Contains(BuilderId)&&Builder->Activity==TEXT("to_road")&&Builder->Position.Equals(Positions[BuilderId],1.e-8));
    TestEqual(TEXT("One configured base hauler remains available"),WorkerReserveIdle(S),1);
    TestEqual(TEXT("The arrived minimum core operator remains at work"),S.FindBuilding(CoreId)->Workers,1);
    TestEqual(TEXT("Reserve lending manufactures no additional identities"),S.Workers.Bodies.Num(),BodyCount);
    const double Duration=S.RoadConstructionSeconds(*S.FindRoad(RoadId))*S.TransportTiers[S.FindRoad(RoadId)->TargetTier].ConstructionWorkers;
    for(int32 I=0;I<FMath::CeilToInt((Duration+600)/10)&&S.FindRoad(RoadId)->IsConstructing;++I)S.Tick(10);
    TestFalse(TEXT("A single proportional crew can physically finish the fully supplied road"),S.FindRoad(RoadId)->IsConstructing);
    S.Tick(10);TestTrue(TEXT("Actual road completion reconnects the staffed support facility"),S.IsRoadGridConnected(CoreId,ServiceId)&&S.RobotSupportCapacity>8);
    TestEqual(TEXT("Completion retains the same finite set of worker identities"),S.Workers.Bodies.Num(),BodyCount);
    TestTrue(TEXT("The same builder leaves the completed road through ordinary scheduling"),S.Workers.Find(BuilderId)&&S.Workers.Find(BuilderId)->RoadId==0);
    for(const auto& P:Bill)TestTrue(TEXT("The paid road installs precisely its delivered material"),FMath::IsNearlyEqual(S.FindRoad(RoadId)->InstalledMaterials.FindRef(P.Key),P.Value,1.e-6));

    // One idle body is also a liveness boundary: a rigid base reserve must not
    // keep a fully supplied, reachable site permanently at zero construction.
    FSeigeSimulation Single=Partial;Single.FindBuilding(CoreId)->Inventory.FindOrAdd(Resource)-=Undelivered;Single.FindRoad(RoadId)->ConstructionMaterials.FindOrAdd(Resource)+=Undelivered;
    auto* Other=Single.Workers.Bodies.FindByPredicate([](const auto& W){return W.Activity==TEXT("idle");});
    const auto* Factory=Single.Buildings.FindByPredicate([&](const auto& B){return Single.Definition(B)->Role==TEXT("worker_factory");});
    Other->Activity=TEXT("operate");Other->BuildingId=Factory->Id;Other->Position=Single.BuildingAccessPoint(*Factory);Single.Workers.RefreshMetrics(S);
    Single.Workers.RefreshMetrics(Single);TestEqual(TEXT("Single-body boundary has exactly one physically free worker"),WorkerReserveIdle(Single),1);
    WorkerReserveSchedule(Single);const auto* SingleBuilder=Single.Workers.Bodies.FindByPredicate([&](const auto& W){return W.RoadId==RoadId;});
    if(!TestNotNull(TEXT("One reachable fully supplied job may borrow the last idle body"),SingleBuilder))return false;
    const FString SingleId=SingleBuilder->Id;
    for(int32 I=0;I<FMath::CeilToInt((Duration+600)/10)&&Single.FindRoad(RoadId)->IsConstructing;++I)Single.Tick(10);
    TestFalse(TEXT("The last-reserve loan completes rather than deadlocking support"),Single.FindRoad(RoadId)->IsConstructing);
    Single.Tick(10);TestTrue(TEXT("The same body is released and available for subsequent real work"),Single.Workers.Find(SingleId)&&Single.Workers.Find(SingleId)->RoadId==0);
    TestEqual(TEXT("The last-reserve fallback also creates no body"),Single.Workers.Bodies.Num(),BodyCount);
    return true;
}
#endif
