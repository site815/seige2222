#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerDestructionLifecycleTest,"Seige.Workers.DestructionDuringChargingAndExport",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeWorkerDestructionLifecycleTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    S.Buildings[0].IsConstructing=false;S.Buildings[0].ConstructionProgress=1;
    S.Buildings[0].InstalledMaterials=S.Workers.DeploymentStock;S.Workers.DeploymentStock.Empty();
    const int32 CoreId=S.Buildings[0].Id,Owner=S.AddReviewBuilding(TEXT("extraction_mine"),FVector2D(600,0));
    auto& Worker=S.Workers.Bodies[0];const FString WorkerId=Worker.Id;
    Worker.Activity=TEXT("charge_queue");Worker.SuspendedActivity=TEXT("operate");Worker.CrewBuildingId=Worker.BuildingId=Owner;
    Worker.ChargingBuildingId=CoreId;Worker.ChargeQueueSlot=1;Worker.ContainerId=0;Worker.ContainerKind.Empty();Worker.Outdoor=true;
    Worker.Position=S.BuildingAccessPoint(*S.FindBuilding(CoreId))+FVector2D(0,20);
    const auto Position=Worker.Position;const int32 Owned=S.Workers.OwnedCount();
    S.OnBuildingDestroyed(Owner);
    TestEqual(TEXT("Owner destruction preserves charging trip"),Worker.Activity,FString(TEXT("charge_queue")));
    TestEqual(TEXT("Charger remains claimed"),Worker.ChargingBuildingId,CoreId);
    TestEqual(TEXT("Only the suspended job is cancelled"),Worker.SuspendedActivity,FString(TEXT("idle")));
    TestEqual(TEXT("Destroyed owner releases permanent crew"),Worker.CrewBuildingId,0);
    TestTrue(TEXT("Worker remains at its physical queue position"),Worker.Position.Equals(Position));
    TestEqual(TEXT("Owner destruction does not duplicate or remove the body"),S.Workers.OwnedCount(),Owned);
    auto State=MakeShared<FJsonObject>();S.Workers.Save(State);auto Reload=S.Workers;
    TestTrue(TEXT("Charging survivor manifest reloads"),Reload.Load(State,S,Error));
    if(!Error.IsEmpty())AddError(Error);

    const int32 Owner2=S.AddReviewBuilding(TEXT("extraction_mine"),FVector2D(-600,0));
    const int32 Charger=S.AddReviewBuilding(TEXT("robot_service_bay"),FVector2D(0,600));
    auto* W=S.Workers.Find(WorkerId);
    W->Activity=TEXT("charging");W->SuspendedActivity=TEXT("operate");W->CrewBuildingId=W->BuildingId=Owner2;
    W->ChargingBuildingId=Charger;W->Position=S.BuildingAccessPoint(*S.FindBuilding(Charger));
    const auto ChargerPosition=W->Position;
    S.OnBuildingDestroyed(Charger);
    TestEqual(TEXT("Charger destruction requires travel back before work"),W->Activity,FString(TEXT("to_job")));
    TestEqual(TEXT("Destroyed charger claim is cleared"),W->ChargingBuildingId,0);
    TestTrue(TEXT("Charger destruction never teleports the worker"),W->Position.Equals(ChargerPosition));

    // A disabled charger uses the same return contract for carried goods.
    const int32 Charger2=S.AddReviewBuilding(TEXT("robot_service_bay"),FVector2D(0,-600));
    FSeigeCourier Cargo;Cargo.Id=900001;Cargo.WorkerId=WorkerId;Cargo.SourceId=CoreId;Cargo.TargetId=Owner2;
    Cargo.Resource=TEXT("iron_ore");Cargo.Amount=1;Cargo.Phase=TEXT("unloading");Cargo.PhaseSeconds=1;
    W->Activity=TEXT("charging");W->SuspendedActivity=TEXT("delivery");W->BuildingId=0;W->DeliveryId=Cargo.Id;
    W->ChargingBuildingId=Charger2;W->Position=S.BuildingAccessPoint(*S.FindBuilding(Charger2));Cargo.Position=W->Position;
    S.Couriers.Add(Cargo);S.FindBuilding(Charger2)->Enabled=false;S.Time+=.1;S.Workers.Tick(S,.1);
    const auto* Returned=S.Couriers.FindByPredicate([&](const auto& C){return C.Id==Cargo.Id;});
    TestTrue(TEXT("Interrupted unloading must travel with cargo before unloading again"),Returned&&Returned->Phase==TEXT("carrying")&&Returned->Amount==1&&Returned->PhaseSeconds==0);

    const int32 Port=S.AddReviewBuilding(TEXT("trading_port"),FVector2D(900,900));
    auto* Trade=S.FindBuilding(Port);const FString Resource=TEXT("stored_workers");
    S.Workers.NewStored(S,Port,1);TestTrue(TEXT("Existing worker enters export escrow"),S.Workers.MoveStored(Port,TEXT("shipment"),Port,1));
    Trade->Shipment.Resource=Resource;Trade->Shipment.Quantity=Trade->Shipment.GoodsEscrow=1;Trade->Shipment.Departed=true;
    const int32 BeforeLoss=S.Workers.OwnedCount();S.OnBuildingDestroyed(Port);
    TestEqual(TEXT("Destroyed escrow loses exactly its existing worker"),S.Workers.OwnedCount(),BeforeLoss-1);
    TestFalse(TEXT("No orphaned worker remains in deleted shipment"),S.Workers.Bodies.ContainsByPredicate([&](const auto& B){return B.State==TEXT("shipment")&&B.ContainerId==Port;}));
    TestEqual(TEXT("Destroyed port does not recreate worker stock"),S.FindBuilding(Port)->Inventory.FindRef(Resource),0.);
    return true;
}
#endif
