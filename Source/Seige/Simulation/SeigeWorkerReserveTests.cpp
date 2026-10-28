#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags WorkerReserveFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
void ScheduleOnly(FSeigeSimulation& S)
{
    const double Step=S.FixedStepSeconds();S.Time+=Step;S.Workers.Tick(S,Step);S.Calendar.Advance(Step);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerReserveProgressTest,"Seige.Workers.SuppliedRoadUsesOwnedCrew",WorkerReserveFlags)
bool FSeigeWorkerReserveProgressTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false)){AddError(Error);return false;}
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("Real starter crew first completes the landed core"),S.Buildings[0].IsConstructing))return false;
    // Only this compatibility fixture enables roads. The shipped scenarios
    // remain road-free; current hauling uses permanently owned building crew.
    S.SetRoadsEnabledForTest(true);
    const int32 CoreId=S.Buildings[0].Id,BodyCount=S.Workers.Bodies.Num();
    const FVector2D Port=S.BuildingAccessPoint(S.Buildings[0]);
    if(!S.PlaceRoad(Port,Port+FVector2D(400,0),Error)){AddError(Error);return false;}
    const int32 RoadId=S.Roads.Last().Id;
    const auto Bill=S.RoadCost(S.Roads.Last().A,S.Roads.Last().B,S.Roads.Last().TargetTier);
    int32 Slot=0;
    for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active"))
    {
        W.CrewBuildingId=W.BuildingId=CoreId;W.Activity=TEXT("operate");W.Outdoor=true;
        W.RoadId=W.DeliveryId=W.ContainerId=W.TransportVehicleId=W.ChargingBuildingId=W.ChargeQueueSlot=0;
        W.ContainerKind.Empty();W.Route.Empty();W.NextWaypoint=0;W.PhaseSeconds=W.RetryAt=0;W.SuspendedActivity.Empty();
        W.StationSlot=Slot++;W.Position=Port+FVector2D(0,Slot*20);
    }
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);
    TestEqual(TEXT("Hauling adds no phantom logistics population"),S.Workers.LogisticsJobs(S),0);
    TestEqual(TEXT("The real core owns every operating body in this fixture"),S.Workers.AssignedCount(CoreId),S.Population);
    FSeigeSimulation Empty=S;ScheduleOnly(Empty);
    TestEqual(TEXT("An unfunded road does not tie up builders"),Empty.FindRoad(RoadId)->Builders,0);
    // Stage the accepted road's own finite bill at its actual worksite. The
    // physical hauling regressions separately exercise pickup and unloading.
    for(const auto& P:Bill)
    {
        if(!TestTrue(TEXT("The remaining finite landing kit pays the road bill"),S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)>=P.Value))return false;
        S.FindBuilding(CoreId)->Inventory.FindOrAdd(P.Key)-=P.Value;
        S.FindRoad(RoadId)->ConstructionMaterials.Add(P.Key,P.Value);
    }
    TMap<FString,FVector2D> Positions;for(const auto& W:S.Workers.Bodies)Positions.Add(W.Id,W.Position);
    const int32 CrewCap=S.TransportTiers[S.FindRoad(RoadId)->TargetTier].ConstructionWorkers;
    ScheduleOnly(S);TSet<FString> Builders;
    for(const auto& W:S.Workers.Bodies)if(W.RoadId==RoadId)
    {
        Builders.Add(W.Id);TestEqual(TEXT("A borrowed builder retains its permanent owner"),W.CrewBuildingId,CoreId);
        TestTrue(TEXT("Scheduling starts a physical walk without relocating the worker"),W.Activity==TEXT("to_road")&&W.Position.Equals(Positions[W.Id],1.e-8));
    }
    TestEqual(TEXT("The supplied road receives its authored number of actual builders"),Builders.Num(),CrewCap);
    TestTrue(TEXT("The core retains an actual on-site operator"),S.FindBuilding(CoreId)->Workers>=1);
    TestEqual(TEXT("Assignment creates no additional worker identities"),S.Workers.Bodies.Num(),BodyCount);
    const double Duration=S.RoadConstructionSeconds(*S.FindRoad(RoadId))*CrewCap;
    // Advance physical crew travel and construction only. This fixture isolates
    // task lending from unrelated worker fabrication and trade decisions.
    for(int32 I=0;I<FMath::CeilToInt((Duration+600)/S.FixedStepSeconds())&&S.FindRoad(RoadId)->IsConstructing;++I)
    {
        const double Step=S.FixedStepSeconds();S.Time+=Step;S.Calendar.Advance(Step);S.Energy.Tick(S,Step);
        S.Workers.Tick(S,Step);S.Workers.RefreshMetrics(S);S.StepConstruction(Step);
    }
    TestFalse(TEXT("Owned crew physically completes the paid road"),S.FindRoad(RoadId)->IsConstructing);
    ScheduleOnly(S);
    for(const auto& Id:Builders)
    {
        const auto* W=S.Workers.Find(Id);
        TestTrue(TEXT("The same builder is released with its original ownership"),W&&W->RoadId==0&&W->CrewBuildingId==CoreId);
    }
    TestEqual(TEXT("Completion keeps the finite original body ledger"),S.Workers.Bodies.Num(),BodyCount);
    for(const auto& P:Bill)TestTrue(TEXT("Installed material exactly equals the paid delivered bill"),FMath::IsNearlyEqual(S.FindRoad(RoadId)->InstalledMaterials.FindRef(P.Key),P.Value,1.e-6));
    return true;
}
#endif
