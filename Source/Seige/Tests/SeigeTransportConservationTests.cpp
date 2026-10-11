#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Simulation/SeigeSimulation.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeTransportConservationTest,"Seige.NextUpdate.AutonomousTransportConservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeTransportConservationTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!TestTrue(TEXT("Rules initialize"),S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201))){AddError(Error);return false;}
    TestEqual(TEXT("Exactly one unarmed transport is aboard the landing shuttle"),S.TransportVehicles.EmbarkedCount(),1);
    TestEqual(TEXT("No armed starter vehicles"),S.Combat.Vehicles.Num(),0);
    TestEqual(TEXT("Bulk mass capacity is fifty worker loads"),S.TransportVehicles.HaulUnits(S,TEXT("alloy")),S.Workers.HaulUnits(S,TEXT("alloy"))*50);
    TestEqual(TEXT("Bulk liquid capacity is fifty worker loads"),S.TransportVehicles.HaulUnits(S,TEXT("hydrocarbons")),S.Workers.HaulUnits(S,TEXT("hydrocarbons"))*50);
    TestEqual(TEXT("Stored bodies cannot be converted to anonymous cargo"),S.TransportVehicles.HaulUnits(S,TEXT("stored_workers")),0.);
    auto& Core=S.Buildings[0];Core.IsConstructing=false;Core.ConstructionProgress=1;Core.ConstructionMaterials.Empty();Core.InstalledMaterials=S.ConstructionCost(Core);
    const int32 CoreId=Core.Id,DepotId=S.AddReviewBuilding(TEXT("depot"),FVector2D(1400,0));
    S.Time=20;S.NextWaveTime=S.NextRoamTime=100000;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    if(!TestTrue(TEXT("Authored fixture seeds real core and depot crews"),S.Workers.SeedEstablished(S,{{CoreId,5},{DepotId,1}},0,Error))){AddError(Error);return false;}
    auto Advance=[&](FSeigeSimulation& World,double Seconds){for(double T=0;T<Seconds-1.e-8;T+=.1){World.Time+=.1;World.Workers.Tick(World,.1);World.TransportVehicles.Tick(World,.1);}};
    const int32 Bodies=S.Workers.OwnedCount();const FVector2D Hatch=S.TransportVehicles.Bodies[0].Position;
    Advance(S,.1);TestTrue(TEXT("Vehicle exits continuously, never appears directly at a remote depot"),FVector2D::Distance(S.TransportVehicles.Bodies[0].Position,Hatch)*S.MetersPerWorldUnit()<1);
    Advance(S,30);if(!TestTrue(TEXT("Vehicle physically leaves hatch before hauling"),S.TransportVehicles.Available(S)))return false;
    const double Before=S.TotalStock(TEXT("alloy")),SourceBefore=S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy"));
    if(!TestTrue(TEXT("Dispatch reserves a real bulk batch"),S.TransportVehicles.Dispatch(S,CoreId,DepotId,0,TEXT("alloy"),400,false)))return false;
    const int32 VehicleId=S.TransportVehicles.Bodies[0].Id;
    TestEqual(TEXT("Reservation leaves goods at source until worker loads them"),S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),SourceBefore);
    Advance(S,1);TestEqual(TEXT("Bulk loading is not instantaneous"),S.Couriers[0].Amount,0.);
    bool SawLoading=false,SawUnloading=false,Saved=false;const FString Save=FPaths::ProjectSavedDir()/TEXT("TransportFocusedSave.json");
    for(int32 I=0;I<6000&&S.TransportVehicles.Find(VehicleId)->DeliveryId;++I)
    {
        Advance(S,.1);const auto* V=S.TransportVehicles.Find(VehicleId);SawLoading|=V->Status==TEXT("Worker loading cargo");SawUnloading|=V->Status==TEXT("Worker unloading cargo");
        TestTrue(TEXT("Stock remains conserved while reserved, loaded and delivered"),FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy")),Before,1.e-6));
        if(!Saved&&SawLoading&&!S.Couriers.IsEmpty()&&S.Couriers[0].Amount>0)
        {
            if(!TestTrue(TEXT("Save accepts owned vehicle cargo"),S.Save(Save,Error))){AddError(Error);break;}
            FSeigeSimulation Restored;if(!Restored.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201)||!Restored.Load(Save,Error)){AddError(Error);break;}
            TestEqual(TEXT("Save keeps stable vehicle identity and actual cargo"),Restored.TransportVehicles.Find(VehicleId)->DeliveryId,V->DeliveryId);
            TestTrue(TEXT("Save/load conserves cargo"),FMath::IsNearlyEqual(Restored.TotalStock(TEXT("alloy")),Before,1.e-6));
            S=MoveTemp(Restored);S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});Saved=true;
        }
    }
    TestTrue(TEXT("Both endpoint workers complete timed handling"),SawLoading&&SawUnloading);
    TestTrue(TEXT("Mid-delivery save/load completed"),Saved);
    TestEqual(TEXT("Transport delivered exactly its 400 kg batch"),S.FindBuilding(DepotId)->Inventory.FindRef(TEXT("alloy")),400.);
    TestEqual(TEXT("Transport never manufactures drivers or builders"),S.Workers.OwnedCount(),Bodies);
    TestEqual(TEXT("Delivery leaves no orphan task"),S.Couriers.Num(),0);
    if(TestTrue(TEXT("Empty transport can return to the sole shuttle bay"),S.TransportVehicles.BoardShuttle(S,VehicleId,Error)))
    {for(int I=0;I<2000&&!S.TransportVehicles.Find(VehicleId)->Embarked;++I)Advance(S,.1);S.LaunchShuttle();TestTrue(TEXT("One physically boarded vehicle escapes"),S.TransportVehicles.Find(VehicleId)->Evacuated);}
    IFileManager::Get().Delete(*Save,false,true);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeMobilityRouteCacheTest,"Seige.NextUpdate.MobilityRouteCache",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeMobilityRouteCacheTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!TestTrue(TEXT("Route-cache rules initialize"),S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201))){AddError(Error);return false;}
    // Isolate terrain/cache behavior from a colony's operating schedules.
    S.Buildings.Empty();S.Roads.Empty();FVector2D A,B;bool Found=false;
    for(int X=-4;X<=4&&!Found;++X)for(int Y=-4;Y<=4&&!Found;++Y)
    {A=FVector2D(X*2000,Y*2000);B=A+FVector2D(500,0);Found=S.Environment.CanStand(A)&&S.Environment.CanStand(B)&&S.Environment.SegmentDry(A,B,25);}
    if(!TestTrue(TEXT("A dry test corridor exists"),Found))return false;
    int32 Samples=0;bool Blocked=true;
    S.Mobility.SetTerrainSampler([&](FVector2D){++Samples;return Blocked?std::numeric_limits<double>::quiet_NaN():0.;});
    TArray<FVector2D> Route;
    TestFalse(TEXT("Unavailable terrain rejects a route"),S.FindMobilityRoute(TEXT("transport"),A,B,Route));
    const int32 FirstSamples=Samples;
    TestTrue(TEXT("First query examined authoritative terrain"),FirstSamples>0);
    for(int I=0;I<20;++I)TestFalse(TEXT("Repeated failed route remains blocked"),S.FindMobilityRoute(TEXT("transport"),A,B,Route));
    TestEqual(TEXT("Identical failed requests do not resample the visibility graph"),Samples,FirstSamples);
    Blocked=false;S.Mobility.InvalidateTerrain();
    TestTrue(TEXT("Terrain revision invalidates cached failure"),S.FindMobilityRoute(TEXT("transport"),A,B,Route));
    const int32 OpenSamples=Samples;
    TestTrue(TEXT("Unchanged valid route can be reused"),S.FindMobilityRoute(TEXT("transport"),A,B,Route));
    TestEqual(TEXT("Valid route reuse does not resample terrain"),Samples,OpenSamples);
    Blocked=true;S.Mobility.InvalidateTerrain();
    TestFalse(TEXT("Terrain revision also invalidates cached success"),S.FindMobilityRoute(TEXT("transport"),A,B,Route));
    return true;
}
#endif
