#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Simulation/SeigeSimulation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeTransportRecoveryTest,"Seige.NextUpdate.TransportAlternateDepotRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeTransportRecoveryTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!TestTrue(TEXT("Recovery rules initialize"),S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201))){AddError(Error);return false;}
    auto& Core=S.Buildings[0];Core.IsConstructing=false;Core.ConstructionProgress=1;Core.ConstructionMaterials.Empty();Core.InstalledMaterials=S.ConstructionCost(Core);
    const int32 CoreId=Core.Id,SourceId=S.AddReviewBuilding(TEXT("depot"),FVector2D(1400,0)),TargetId=S.AddReviewBuilding(TEXT("depot"),FVector2D(2800,0));
    const FVector2D SourcePosition=S.FindBuilding(SourceId)->Position,TargetPosition=S.FindBuilding(TargetId)->Position;
    S.Time=20;S.NextWaveTime=100000;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    if(!TestTrue(TEXT("Real crews operate all three depots"),S.Workers.SeedEstablished(S,{{CoreId,5},{SourceId,1},{TargetId,1}},0,Error))){AddError(Error);return false;}
    // Move existing paid starting stock to the source; no fixture goods are minted.
    const double Batch=400.;S.FindBuilding(CoreId)->Inventory.FindOrAdd(TEXT("alloy"))-=Batch;S.FindBuilding(SourceId)->Inventory.FindOrAdd(TEXT("alloy"))+=Batch;
    auto Advance=[&](FSeigeSimulation& World,double Seconds){for(double T=0;T<Seconds-1.e-8;T+=.1){World.Time+=.1;World.Workers.Tick(World,.1);World.TransportVehicles.Tick(World,.1);}};
    Advance(S,30);
    const int32 VehicleId=S.TransportVehicles.Bodies[0].Id,WorkerCount=S.Workers.OwnedCount();
    const double StockBefore=S.TotalStock(TEXT("alloy")),CoreBefore=S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),DeliveredBefore=S.DeliveredUnits;
    if(!TestTrue(TEXT("Truck reserves the existing batch"),S.TransportVehicles.Dispatch(S,SourceId,TargetId,0,TEXT("alloy"),Batch,false)))return false;
    const int32 CourierId=S.TransportVehicles.Find(VehicleId)->DeliveryId;
    auto Cargo=[&](){return S.Couriers.FindByPredicate([&](const auto& C){return C.Id==CourierId;});};
    bool LoadedOutside=false;
    for(int32 I=0;I<6000&&Cargo();++I)
    {
        Advance(S,.1);const auto* C=Cargo();const auto* V=S.TransportVehicles.Find(VehicleId);
        if(C&&C->Amount==Batch&&FVector2D::Distance(V->Position,SourcePosition)>450&&FVector2D::Distance(V->Position,TargetPosition)>450){LoadedOutside=true;break;}
    }
    if(!TestTrue(TEXT("Real loader fills the truck, which drives away from source"),LoadedOutside))return false;
    TestEqual(TEXT("Loaded stock left its original depot"),S.FindBuilding(SourceId)->Inventory.FindRef(TEXT("alloy")),0.);
    // Newly graded terrain isolates both original service ports. The vehicle is
    // outside the two steep annuli; an actual flat route to the core remains.
    auto ChangedTerrain=[SourcePosition,TargetPosition](FVector2D P)
    {
        auto Island=[](double D){const double T=FMath::Clamp((D-300.)/100.,0.,1.);return 1000.*(1.-T*T*(3.-2.*T));};
        return Island(FVector2D::Distance(P,SourcePosition))+Island(FVector2D::Distance(P,TargetPosition));
    };
    S.Mobility.SetTerrainSampler(ChangedTerrain);
    bool Recovered=false,Saved=false,SawUnloading=false;
    const FString Save=FPaths::ProjectSavedDir()/TEXT("TransportRecoverySave.json"),Legacy=FPaths::ProjectSavedDir()/TEXT("TransportRecoveryLegacy.json"),Invalid=FPaths::ProjectSavedDir()/TEXT("TransportRecoveryInvalid.json");
    for(int32 I=0;I<10000&&Cargo();++I)
    {
        const FVector2D Previous=S.TransportVehicles.Find(VehicleId)->Position;Advance(S,.1);
        const auto* C=Cargo();const auto* V=S.TransportVehicles.Find(VehicleId);
        TestTrue(TEXT("Recovery moves the original body continuously"),FVector2D::Distance(Previous,V->Position)*S.MetersPerWorldUnit()<=2.);
        TestTrue(TEXT("Every recovery step conserves the sole physical cargo ledger"),FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy")),StockBefore,1.e-6));
        SawUnloading|=V->Status==TEXT("Worker unloading cargo");
        if(C&&C->RecoveryCargo)
        {
            Recovered=true;TestEqual(TEXT("Fallback selects reachable core, not the inaccessible source"),C->TargetId,CoreId);
            TestEqual(TEXT("Recovery retains original source identity"),C->SourceId,SourceId);
            if(!Saved)
            {
                TestEqual(TEXT("No goods unload before the recovered truck arrives"),S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),CoreBefore);
                if(!TestTrue(TEXT("Save accepts physical recovery in progress"),S.Save(Save,Error))){AddError(Error);break;}
                FString Text;TSharedPtr<FJsonObject> Document;
                if(!FFileHelper::LoadFileToString(Text,*Save)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Document)){AddError(TEXT("Cannot inspect saved recovery fixture"));break;}
                // This subsystem fixture does not tick invasion scheduling.
                // Advance its disabled event deadline, leaving every physical
                // worker, cargo, vehicle, route and storage value untouched.
                Document->SetNumberField(TEXT("next_roam_time"),S.Time+1000.);
                Text.Reset();FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Save);
                FSeigeSimulation Restored;
                if(!Restored.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201)||!Restored.Load(Save,Error)){AddError(Error);break;}
                const auto* RestoredCargo=Restored.Couriers.FindByPredicate([&](const auto& Item){return Item.Id==CourierId;});
                TestTrue(TEXT("Snapshot preserves recovery flag and original source"),RestoredCargo&&RestoredCargo->RecoveryCargo&&RestoredCargo->SourceId==SourceId);
                TestEqual(TEXT("Snapshot preserves vehicle ownership of the same courier"),Restored.TransportVehicles.Find(VehicleId)->DeliveryId,CourierId);
                TestTrue(TEXT("Snapshot preserves cargo without duplication"),FMath::IsNearlyEqual(Restored.TotalStock(TEXT("alloy")),StockBefore,1.e-6));
                const auto Row=Document->GetArrayField(TEXT("couriers"))[0]->AsObject();
                // A pre-flag snapshot can still represent an original-source
                // return; loading must infer recovery without crediting delivery.
                Row->RemoveField(TEXT("recovery_cargo"));Row->SetNumberField(TEXT("target"),SourceId);
                Text.Reset();FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Legacy);
                FSeigeSimulation Old;Old.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,2201);
                TestTrue(TEXT("Legacy return cargo remains loadable"),Old.Load(Legacy,Error));
                const auto* OldCargo=Old.Couriers.FindByPredicate([&](const auto& Item){return Item.Id==CourierId;});
                TestTrue(TEXT("Legacy same-source return is marked as recovery"),OldCargo&&OldCargo->RecoveryCargo);
                Row->SetStringField(TEXT("recovery_cargo"),TEXT("yes"));
                Text.Reset();FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Invalid);
                TestFalse(TEXT("Malformed recovery flags are rejected"),Old.Load(Invalid,Error));
                Row->SetBoolField(TEXT("recovery_cargo"),true);Row->SetNumberField(TEXT("transport_vehicle_id"),0);
                Text.Reset();FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Invalid);
                TestFalse(TEXT("Worker parcels cannot claim truck recovery state"),Old.Load(Invalid,Error));
                S=MoveTemp(Restored);S.Mobility.SetTerrainSampler(ChangedTerrain);Saved=true;
            }
        }
    }
    TestTrue(TEXT("An unreachable source does not trap loaded cargo forever"),Recovered);
    TestTrue(TEXT("Recovery survives save/load and actually unloads with a worker"),Saved&&SawUnloading);
    TestEqual(TEXT("Physical return completes without an orphan courier"),S.Couriers.Num(),0);
    TestEqual(TEXT("The reachable core receives the entire original batch"),S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),CoreBefore+Batch);
    TestEqual(TEXT("Unreachable requested depot receives no imaginary goods"),S.FindBuilding(TargetId)->Inventory.FindRef(TEXT("alloy")),0.);
    TestEqual(TEXT("A recovered load earns no successful delivery credit"),S.DeliveredUnits,DeliveredBefore);
    TestEqual(TEXT("Recovery never creates a replacement worker"),S.Workers.OwnedCount(),WorkerCount);
    TestEqual(TEXT("Recovery uses the original vehicle identity"),S.TransportVehicles.Bodies[0].Id,VehicleId);
    for(const auto& Path:{Save,Legacy,Invalid})IFileManager::Get().Delete(*Path,false,true);
    return true;
}
#endif
