#include "SeigeSimulation.h"
#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags EstablishedWorkerFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString EstablishedWorkerRules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
FSeigeBuilding* EstablishedWorkerCore(FSeigeSimulation& S)
{return S.Buildings.FindByPredicate([&](const auto& B){const auto* D=S.Definition(B);return D&&D->Role==TEXT("core");});}
FString EstablishedLedger(const FSeigeWorkerSystem& Workers)
{auto O=MakeShared<FJsonObject>();Workers.Save(O);FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));return Json;}
void EstablishedCoreFixture(FSeigeSimulation& S)
{
    // Explicit scenario authoring, not a paid-gameplay shortcut: the static
    // installation and its loose cargo are separate entries in the manifest.
    auto& Core=*EstablishedWorkerCore(S);Core.InstalledMaterials=S.ConstructionCost(Core);Core.ConstructionMaterials.Empty();Core.IsConstructing=false;Core.ConstructionProgress=1;
    // This isolated worker-API fixture is never saved or advanced. Full scenario
    // clocks are authored by the real initializer in the persistence test.
    S.Time=60;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeEstablishedWorkerManifestTest,"Seige.Workers.EstablishedManifestAndPersistence",EstablishedWorkerFlags)
bool FSeigeEstablishedWorkerManifestTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Twin;FSeigeScenarioAI Brain,TwinBrain;
    const FString AIDirectory=FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES"));
    if(!Brain.Initialize(S,EstablishedWorkerRules(),AIDirectory,true,Error,false,false)||!TwinBrain.Initialize(Twin,EstablishedWorkerRules(),AIDirectory,true,Error,false,false)){AddError(Error);return false;}
    FString Raw;TSharedPtr<FJsonObject> Manifest;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(AIDirectory,TEXT("developed_start.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Manifest)){AddError(TEXT("Cannot inspect the authored established manifest"));return false;}
    const auto& Rows=Manifest->GetArrayField(TEXT("buildings"));int32 Active=int32(Manifest->GetNumberField(TEXT("idle_workers"))),Packed=0;
    if(!TestEqual(TEXT("The initializer retains every explicitly authored local container"),S.Buildings.Num(),Rows.Num()))return false;
    for(int32 I=0;I<Rows.Num();++I)
    {
        const auto Row=Rows[I]->AsObject(),Inventory=Row->GetObjectField(TEXT("inventory"));const auto& Building=S.Buildings[I];Active+=int32(Row->GetNumberField(TEXT("operators")));
        double Count=0;Inventory->TryGetNumberField(TEXT("stored_workers"),Count);Packed+=int32(Count);
        TestEqual(TEXT("Each container has one physical identity for every authored packed worker"),S.Workers.StoredAt(Building.Id),int32(Count));
        TestEqual(TEXT("Initialization creates no additional inventory entries"),Building.Inventory.Num(),Inventory->Values.Num());
        for(const auto& P:Inventory->Values)TestEqual(TEXT("Worker seeding neither consumes nor grants authored local cargo"),Building.Inventory.FindRef(FString(P.Key)),P.Value->AsNumber());
    }
    auto* Core=EstablishedWorkerCore(S);
    TestEqual(TEXT("Identical explicit manifests produce identical stable body identities and positions"),EstablishedLedger(S.Workers),EstablishedLedger(Twin.Workers));
    TestEqual(TEXT("Operators and idle haulers share the authoritative active population"),S.Population,Active);
    TestEqual(TEXT("No courier or extra delivery bodies are synthesized"),S.Couriers.Num(),0);
    TestEqual(TEXT("All active and stored bodies are in the same identity ledger"),S.Workers.Bodies.Num(),Active+Packed);
    TestTrue(TEXT("Established workers are already grounded with the physical hatch open"),S.DeploymentGrounded&&S.DeploymentHatchOpen&&S.Workers.DeploymentStock.IsEmpty());
    TSet<FString> Ids;TArray<FVector2D> ActivePositions;
    for(const auto& W:S.Workers.Bodies)
    {
        Ids.Add(W.Id);
        if(W.State==TEXT("active"))
        {
            TestTrue(TEXT("Established active workers occupy visible physical workstations"),W.Outdoor&&(W.Activity==TEXT("operate")||W.Activity==TEXT("idle")));
            TArray<FVector2D> Route;const auto* Workplace=W.BuildingId?S.FindBuilding(W.BuildingId):Core;
            TestTrue(TEXT("Workstations are reachable outside reserved plots and water"),Workplace&&S.FindRoute(S.BuildingAccessPoint(*Workplace),W.Position,Route,S.Workers.BodyRadiusMeters()/S.MetersPerWorldUnit()));
            for(const auto& P:ActivePositions)TestTrue(TEXT("Each active worker has a distinct non-overlapping station"),FVector2D::Distance(P,W.Position)*S.MetersPerWorldUnit()>=2*S.Workers.BodyRadiusMeters()-1.e-6);
            ActivePositions.Add(W.Position);
        }
        else TestTrue(TEXT("Packed workers remain inside their real inventory container"),W.State==TEXT("stored")&&!W.Outdoor&&S.FindBuilding(W.ContainerId)!=nullptr);
    }
    TestEqual(TEXT("No worker identity is duplicated"),Ids.Num(),S.Workers.Bodies.Num());
    const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/WorkerAgents/established-manifest.json"));FSeigeSimulation Loaded;
    if(!S.Save(Path,Error)||!Loaded.Initialize(EstablishedWorkerRules(),Error,false,false)||!Loaded.Load(Path,Error)){AddError(Error);return false;}
    TestEqual(TEXT("The ordinary atomic save/load retains the complete worker ledger"),EstablishedLedger(Loaded.Workers),EstablishedLedger(S.Workers));
    S.Tick(10);Loaded.Tick(10);
    TestEqual(TEXT("Normal finite scheduling resumes deterministically after load"),EstablishedLedger(Loaded.Workers),EstablishedLedger(S.Workers));
    TestEqual(TEXT("No landing replay or hidden replacement crew appears after load"),Loaded.Workers.Bodies.Num(),Ids.Num());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeEstablishedWorkerAtomicTest,"Seige.Workers.EstablishedManifestRejectsInvalidState",EstablishedWorkerFlags)
bool FSeigeEstablishedWorkerAtomicTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;if(!S.Initialize(EstablishedWorkerRules(),Error,false,false)){AddError(Error);return false;}
    auto* Core=EstablishedWorkerCore(S);const int32 CoreId=Core->Id,Operators=S.Definition(*Core)->Jobs;TMap<int32,int32> Assignment;Assignment.Add(CoreId,Operators);
    const FString LandingLedger=EstablishedLedger(S.Workers);const double LandingMass=S.InventoryMassKg(Core->Inventory);
    TestFalse(TEXT("A new-player landing cannot be silently converted into an established workforce"),S.Workers.SeedEstablished(S,Assignment,1,Error));
    TestEqual(TEXT("Rejected initialization leaves initial hatch IDs and carried deployment kit intact"),EstablishedLedger(S.Workers),LandingLedger);
    TestEqual(TEXT("The finite initial loose kit is unchanged"),S.InventoryMassKg(Core->Inventory),LandingMass);
    EstablishedCoreFixture(S);const FString Before=EstablishedLedger(S.Workers);
    Assignment[CoreId]=Operators+1;TestFalse(TEXT("A station manifest cannot exceed actual jobs"),S.Workers.SeedEstablished(S,Assignment,1,Error));Assignment[CoreId]=Operators;
    TestEqual(TEXT("Rejected assignments do not partially replace the body ledger"),EstablishedLedger(S.Workers),Before);
    Core->Inventory.Add(TEXT("stored_workers"),.5);TestFalse(TEXT("Packed body manifests require whole identities"),S.Workers.SeedEstablished(S,Assignment,1,Error));Core->Inventory.Remove(TEXT("stored_workers"));
    TestEqual(TEXT("A fractional packed-worker error retains the original ledger"),EstablishedLedger(S.Workers),Before);
    TestFalse(TEXT("Explicit active bodies still require sufficient staffed support"),S.Workers.SeedEstablished(S,Assignment,100,Error));
    TestEqual(TEXT("Support rejection creates no partial worker population"),EstablishedLedger(S.Workers),Before);
    if(!S.Workers.SeedEstablished(S,Assignment,1,Error)){AddError(Error);return false;}
    const FString Established=EstablishedLedger(S.Workers);
    TestFalse(TEXT("An established worker ledger cannot be reset after entering play"),S.Workers.SeedEstablished(S,Assignment,1,Error));
    TestEqual(TEXT("Rejected reseeding preserves every existing identity"),EstablishedLedger(S.Workers),Established);
    return true;
}
#endif
