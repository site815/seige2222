#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRoadlessGridTest,"Seige.Energy.RoadlessColonyGrid",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeRoadlessGridTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Neighbor;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)||!Neighbor.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    TestFalse(TEXT("Current rules disable roads"),S.RoadsEnabled());
    auto& Core=S.Buildings[0];Core.IsConstructing=false;Core.BatteryEnergyKWh=30;
    const int32 CoreId=Core.Id;
    FSeigeBuilding Depot;Depot.Id=990;Depot.DefId=TEXT("depot");Depot.Health=S.BuildingDefs[Depot.DefId].Health;
    Depot.Position=FVector2D(7000,7000);Depot.Enabled=true;Depot.IsConstructing=false;S.Buildings.Add(Depot);
    S.Energy.Invalidate();S.Energy.Tick(S,0);Neighbor.Energy.Tick(Neighbor,0);
    const auto Grid=S.Energy.Info(S,Depot.Id);
    TestTrue(TEXT("Remote live building joins its colony grid without a road"),Grid.Connected);
    TestEqual(TEXT("Roadless depot and core share the same grid"),Grid.ComponentId,S.Energy.Info(S,CoreId).ComponentId);
    TestEqual(TEXT("No road was created as a hidden prerequisite"),S.Roads.Num(),0);
    const double Before=Grid.StoredKWh,NeighborBefore=Neighbor.Energy.Info(Neighbor).StoredKWh,Generated=S.Energy.GeneratedKWh,Consumed=S.Energy.ConsumedKWh;
    TestTrue(TEXT("Remote production can draw real stored charge"),S.Energy.Consume(S,Depot.Id,2));
    TestTrue(TEXT("Shared storage pays the transaction exactly once"),FMath::IsNearlyEqual(S.Energy.Info(S).StoredKWh,Before-2,1.e-9));
    TestEqual(TEXT("Consumption ledger records the transaction"),S.Energy.ConsumedKWh,Consumed+2);
    TestEqual(TEXT("Topology does not create energy"),S.Energy.GeneratedKWh,Generated);
    TestEqual(TEXT("Neighbor battery remains separate"),Neighbor.Energy.Info(Neighbor).StoredKWh,NeighborBefore);
    TestFalse(TEXT("Shared grid cannot spend nonexistent energy"),S.Energy.Consume(S,Depot.Id,Before+1));
    S.FindBuilding(Depot.Id)->Health=0;S.Energy.Invalidate();S.Energy.Tick(S,0);
    TestFalse(TEXT("Destroyed consumer leaves the grid"),S.Energy.Info(S,Depot.Id).Connected);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFuelReservationTest,"Seige.Energy.GeneratorRespectsPhysicalPickupReservations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeFuelReservationTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false)){AddError(Error);return false;}
    const int32 Generator=S.AddReviewBuilding(TEXT("fuel_generator"),FVector2D(1500,1500));
    auto* B=S.FindBuilding(Generator);B->Workers=S.BuildingDefs[B->DefId].Jobs;B->Inventory.Add(TEXT("fuel"),3.);
    // Isolate the energy consumer from routing: an accepted pickup promise
    // must remain physically collectible until the courier arrives.
    FSeigeCourier Promise;Promise.Id=999991;Promise.SourceId=Generator;Promise.TargetId=S.Buildings[0].Id;Promise.Resource=TEXT("fuel");Promise.ReservedAmount=2.5;Promise.Phase=TEXT("to_pickup");S.Couriers.Add(Promise);
    S.Energy.Invalidate();S.Energy.Tick(S,600);
    TestTrue(TEXT("Generator can burn only unreserved fuel"),FMath::IsNearlyEqual(S.FindBuilding(Generator)->Inventory.FindRef(TEXT("fuel")),2.5,1.e-9));
    S.Energy.Tick(S,600);
    TestTrue(TEXT("Further ticks preserve the entire promised pickup"),FMath::IsNearlyEqual(S.FindBuilding(Generator)->Inventory.FindRef(TEXT("fuel")),2.5,1.e-9));
    S.Couriers.Empty();S.Energy.Tick(S,600);
    TestTrue(TEXT("Released pickup fuel becomes usable again"),S.FindBuilding(Generator)->Inventory.FindRef(TEXT("fuel"))<2.5);
    return true;
}
#endif
