#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Simulation/SeigeSimulation.h"

namespace {
bool EstablishedCore(FSeigeSimulation& S,FString& Error)
{
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201))return false;
    auto& Core=S.Buildings[0];Core.IsConstructing=false;Core.ConstructionProgress=1;
    Core.InstalledMaterials=S.BuildingDefs[Core.DefId].Cost;Core.ConstructionMaterials.Empty();
    S.Time=20;return S.Workers.SeedEstablished(S,{{Core.Id,5}},0,Error);
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeOwnedCrewConservationTest,"Seige.NextUpdate.OwnedCrewConservation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeOwnedCrewConservationTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!TestTrue(*Error,EstablishedCore(S,Error)))return false;
    const int32 CoreId=S.Buildings[0].Id;const auto Initial=S.Workers.Bodies;
    TArray<int32> Sites;
    for(auto P:{FVector2D(1400,0),FVector2D(0,1400),FVector2D(-1400,0)})Sites.Add(S.AddReviewBuilding(TEXT("solar_array"),P));
    const double PartsBefore=S.TotalStock(TEXT("components"));
    S.Tick(.1);
    TestEqual(TEXT("Crew transfers cannot manufacture bodies"),S.Workers.OwnedCount(),5);
    TestEqual(TEXT("Three new one-crew sites borrow three core members"),S.Workers.AssignedCount(CoreId),2);
    for(int32 Id:Sites)TestEqual(TEXT("New site owns exactly one existing worker"),S.Workers.AssignedCount(Id),1);
    for(const auto& W:Initial)TestNotNull(TEXT("All five landing identities survive"),S.Workers.Find(W.Id));
    double Limit=2200;while(S.Workers.OwnedCount()==5&&Limit>0){S.Tick(.5);Limit-=.5;}
    TestEqual(TEXT("One paid core batch creates exactly one replacement"),S.Workers.OwnedCount(),6);
    TestTrue(TEXT("Replacement takes simulation time"),S.Time>600);
    TestTrue(TEXT("Parts consumed in real production escrow"),S.TotalStock(TEXT("components"))<PartsBefore);
    TSet<FString> Ids;for(const auto& W:S.Workers.Bodies){TestFalse(TEXT("Body identity remains unique"),Ids.Contains(W.Id));Ids.Add(W.Id);}
    const int32 BeforeLoss=S.Workers.OwnedCount();S.OnBuildingDestroyed(Sites[0]);
    TestEqual(TEXT("Building destruction preserves surviving crew"),S.Workers.OwnedCount(),BeforeLoss);
    S.Tick(.1);
    TestTrue(TEXT("Released worker reused by core before another fabrication"),S.Workers.AssignedCount(CoreId)>=3);
    const FString Save=FPaths::ProjectSavedDir()/TEXT("OwnedCrewFocusedSave.json");
    TestTrue(*Error,S.Save(Save,Error));FSeigeSimulation Restored;
    if(TestTrue(*Error,Restored.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201))&&TestTrue(*Error,Restored.Load(Save,Error)))
    {TestEqual(TEXT("Save/load preserves every owned physical body"),Restored.Workers.OwnedCount(),S.Workers.OwnedCount());for(const auto& W:S.Workers.Bodies)TestNotNull(TEXT("Identity survives save/load"),Restored.Workers.Find(W.Id));}
    IFileManager::Get().Delete(*Save,false,true);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePhysicalChargingTest,"Seige.NextUpdate.PhysicalChargingAndSurplus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigePhysicalChargingTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!TestTrue(*Error,EstablishedCore(S,Error)))return false;
    const int32 CoreId=S.Buildings[0].Id;const auto Port=S.BuildingAccessPoint(S.Buildings[0]);
    TestEqual(TEXT("Landing manifest has five drones"),S.Workers.OwnedCount(),5);
    TestEqual(TEXT("Exactly one physical charging berth"),S.Workers.ChargerBerths(S,CoreId),1);
    TestEqual(TEXT("Ten calendar minutes in the current one-hour day"),S.Workers.ChargeSessionSeconds(S),25.);
    TArray<FString> Low;
    for(int32 I=0;I<2;++I){auto& W=S.Workers.Bodies[I];W.BatteryKWh=W.BatteryCapacityKWh*.1;W.Position=Port+FVector2D(0,(I+1)*40);W.Activity=TEXT("idle");W.BuildingId=0;Low.Add(W.Id);}
    const int32 Owned=S.Workers.OwnedCount();bool SawQueue=false,SawCharge=false,SawTravel=false;
    for(int32 I=0;I<1200;++I)
    {
        S.Tick(.1);int32 Charging=0;for(const auto& W:S.Workers.Bodies){Charging+=W.Activity==TEXT("charging")||W.Activity==TEXT("berth_entry")?1:0;SawQueue|=W.Activity==TEXT("charge_queue");SawCharge|=W.Activity==TEXT("charging");SawTravel|=W.Activity==TEXT("to_charge");}
        if(Charging>1){AddError(TEXT("Physical berth overbooked"));break;}
    }
    TestTrue(TEXT("Bodies physically travel and queue before charging"),SawTravel&&SawQueue&&SawCharge);
    for(const auto& Id:Low)TestTrue(TEXT("Each queued worker receives real battery energy"),S.Workers.Find(Id)->BatteryKWh>S.Workers.BatteryCapacity()*.8);
    TestEqual(TEXT("Charging never changes the body count"),S.Workers.OwnedCount(),Owned);
    // A real intact manufactured/imported body arrives as inventory. Releasing
    // it from storage without a vacancy must not invent an operating slot.
    S.AddReviewStoredWorkers(CoreId,1);auto* Extra=S.Workers.Bodies.FindByPredicate([](const auto& W){return W.State==TEXT("stored");});
    TestNotNull(TEXT("Surplus is a physical stored identity"),Extra);if(!Extra)return false;
    const FString ExtraId=Extra->Id;S.Buildings[0].Inventory.FindOrAdd(TEXT("stored_workers"))-=1;Extra->State=TEXT("active");Extra->Activity=TEXT("idle");Extra->Outdoor=true;Extra->ContainerId=0;Extra->ContainerKind.Empty();
    S.BuildingDefs[S.Buildings[0].DefId].StorageCapacity=S.StorageUsed(S.Buildings[0]);
    S.Tick(5);
    TestEqual(TEXT("Full storage does not recycle or erase an idle surplus"),S.Workers.Find(ExtraId)->State,FString(TEXT("active")));
    TestEqual(TEXT("Surplus does not exceed core crew cap"),S.Workers.AssignedCount(CoreId),5);
    TestEqual(TEXT("No automatic recycling"),S.WorkersDisassembled,0);return true;
}
#endif
