#include "AI/SeigeScenarioAI.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags AIFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
FString AIRules() { return FPaths::Combine(FPaths::ProjectDir(), TEXT("Rules")); }
FString AIDirectory() { return FPaths::Combine(FPaths::ProjectDir(), TEXT("AIFILES")); }
FString AIOutput(const FString& Name) { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ScenarioAI"), Name); }
bool WriteAIConfig(const FString& Directory, const FString& Filename, const TFunction<void(TSharedPtr<FJsonObject>)>& Edit)
{
    IFileManager::Get().MakeDirectory(*Directory, true);
    for (const TCHAR* Name : {TEXT("colony_ai.json"), TEXT("developed_start.json")})
    {
        FString Raw;
        if (!FFileHelper::LoadFileToString(Raw, *FPaths::Combine(AIDirectory(), Name))) return false;
        if (Filename == Name)
        {
            TSharedPtr<FJsonObject> Object;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Object)) return false;
            Edit(Object); Raw.Empty();
            if (!FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Raw))) return false;
        }
        if (!FFileHelper::SaveStringToFile(Raw, *FPaths::Combine(Directory, Name))) return false;
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIValidationTest, "Seige.AI.ConfigValidation", AIFlags)
bool FSeigeAIValidationTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation Colony; FSeigeScenarioAI AI; FString Error;
    if (!AI.Initialize(Colony, AIRules(), AIDirectory(), false, Error)) { AddError(Error); return false; }
    TestFalse(TEXT("AI fingerprint exists"), AI.GetConfigFingerprint().IsEmpty());
    Colony.Tick(2); const double OriginalTime = Colony.Time;
    TestFalse(TEXT("Missing AI folder is rejected"), AI.Initialize(Colony, AIRules(), AIOutput(TEXT("missing")), false, Error));
    TestEqual(TEXT("Failed initialization preserves the existing colony"), Colony.Time, OriginalTime);
    const FString Invalid = AIOutput(TEXT("invalid"));
    if (!WriteAIConfig(Invalid, TEXT("colony_ai.json"), [](auto Object) { Object->SetNumberField(TEXT("decision_interval_seconds"), 0); })) return false;
    TestFalse(TEXT("Zero decision cadence is rejected"), AI.Initialize(Colony, AIRules(), Invalid, false, Error));
    TestTrue(TEXT("Cadence diagnostic identifies the field"), Error.Contains(TEXT("decision_interval_seconds")));
    if (!WriteAIConfig(Invalid, TEXT("colony_ai.json"), [](auto Object) { Object->GetArrayField(TEXT("build_targets"))[0]->AsObject()->SetStringField(TEXT("definition"), TEXT("unknown_building")); })) return false;
    TestFalse(TEXT("Unknown building reference is rejected"), AI.Initialize(Colony, AIRules(), Invalid, false, Error));
    if (!WriteAIConfig(Invalid, TEXT("developed_start.json"), [](auto Object) { Object->GetObjectField(TEXT("inventory"))->SetNumberField(TEXT("alloy"), 999999); })) return false;
    TestFalse(TEXT("Over-capacity developed seed is rejected"), AI.Initialize(Colony, AIRules(), Invalid, true, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIStartsTest, "Seige.AI.StartingAndDevelopedColonies", AIFlags)
bool FSeigeAIStartsTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation Starting, Developed; FSeigeScenarioAI StartAI, DevelopedAI;
    if (!StartAI.Initialize(Starting, AIRules(), AIDirectory(), false, Error) || !DevelopedAI.Initialize(Developed, AIRules(), AIDirectory(), true, Error))
    { AddError(Error); return false; }
    TestEqual(TEXT("Starting AI begins with only its command core"), Starting.Buildings.Num(), 1);
    TestTrue(TEXT("Developed preset starts with an established colony"), Developed.Buildings.Num() > Starting.Buildings.Num() + 8);
    TestEqual(TEXT("Developed setup does not secretly advance the clock"), Developed.Time, 0.0);
    TestTrue(TEXT("Developed starting stock does not count as manufactured output"), Developed.ProducedUnits.IsEmpty());
    const double Alloy = Starting.Buildings[0].Inventory.FindRef(TEXT("alloy"));
    StartAI.Tick(Starting, 1);
    TestTrue(TEXT("Normal AI action constructs a building"), Starting.Buildings.Num() > 1);
    TestTrue(TEXT("AI construction consumes real core inventory"), Starting.Buildings[0].Inventory.FindRef(TEXT("alloy")) < Alloy);
    StartAI.Tick(Starting, 299);
    DevelopedAI.Tick(Developed, 60);
    AddInfo(TEXT("Starting AI: ") + Starting.ObjectiveText() + TEXT(" | ") + StartAI.GetStatus());
    TestTrue(TEXT("Starting AI develops the industrial chain through normal actions"), Starting.ProducedUnits.FindRef(TEXT("components")) > 0);
    TestTrue(TEXT("Starting AI uses physical deliveries"), Starting.DeliveredUnits > 0);
    TestTrue(TEXT("Developed AI actually produces goods after setup"), Developed.ProducedUnits.FindRef(TEXT("components")) > 0);
    TestTrue(TEXT("Developed AI uses physical deliveries"), Developed.DeliveredUnits > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIPersistenceTest, "Seige.AI.DeterministicSaveContinuation", AIFlags)
bool FSeigeAIPersistenceTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation A, B; FSeigeScenarioAI BrainA, BrainB;
    if (!BrainA.Initialize(A, AIRules(), AIDirectory(), false, Error)) { AddError(Error); return false; }
    BrainA.Tick(A, 31.25);
    IFileManager::Get().MakeDirectory(*AIOutput(TEXT("")), true);
    if (!A.Save(AIOutput(TEXT("resume.json")), Error) || !BrainB.Initialize(B, AIRules(), AIDirectory(), false, Error) || !B.Load(AIOutput(TEXT("resume.json")), Error))
    { AddError(Error); return false; }
    TestEqual(TEXT("Identical AI definitions have the same fingerprint"), BrainA.GetConfigFingerprint(), BrainB.GetConfigFingerprint());
    BrainA.Tick(A, 120); BrainB.Tick(B, 120);
    if (!A.Save(AIOutput(TEXT("a.json")), Error) || !B.Save(AIOutput(TEXT("b.json")), Error)) { AddError(Error); return false; }
    FString Left, Right;
    FFileHelper::LoadFileToString(Left, *AIOutput(TEXT("a.json")));
    FFileHelper::LoadFileToString(Right, *AIOutput(TEXT("b.json")));
    TestEqual(TEXT("Time-derived AI decisions preserve complete state across save/load"), Left, Right);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeLandingSimulationTest, "Seige.AI.InitialCoreAndThreatOrigin", AIFlags)
bool FSeigeLandingSimulationTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation Colony; FString Error;
    if (!Colony.Initialize(AIRules(), Error)) { AddError(Error); return false; }
    const FVector2D Original = Colony.Buildings[0].Position;
    TestFalse(TEXT("Landing cannot overlap a resource source"), Colony.CanSetInitialCorePosition(Colony.Nodes[0].Position, Error));
    TestFalse(TEXT("Landing cannot straddle the sector boundary"), Colony.SetInitialCorePosition(FVector2D(Colony.WorldHalfSize, 0), Error));
    TestEqual(TEXT("Rejected landing preserves core location"), Colony.Buildings[0].Position, Original);
    FVector2D Destination = FVector2D::ZeroVector; bool Found = false;
    for (int32 I = 1; I <= 8; ++I)
    {
        const double Angle = I * UE_TWO_PI / 8;
        const FVector2D Candidate(FMath::Cos(Angle) * Colony.WorldHalfSize * .5, FMath::Sin(Angle) * Colony.WorldHalfSize * .5);
        if (Colony.CanSetInitialCorePosition(Candidate, Error)) { Destination = Candidate; Found = true; break; }
    }
    if (!TestTrue(TEXT("A legal noncentral landing position exists"), Found)) return false;
    TestTrue(TEXT("Fresh core can land away from origin"), Colony.SetInitialCorePosition(Destination, Error));
    Colony.TriggerWave();
    TestTrue(TEXT("A wave has enemies to verify"), Colony.Enemies.Num() > 0);
    FString PolicyText; TSharedPtr<FJsonObject> Policies;
    if (!FFileHelper::LoadFileToString(PolicyText, *FPaths::Combine(AIRules(), TEXT("policies.json"))) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PolicyText), Policies)) return false;
    const double Radius = Policies->GetObjectField(TEXT("policies"))->GetNumberField(TEXT("spawn_radius"));
    for (const auto& Enemy : Colony.Enemies)
    {
        TestTrue(TEXT("Threat spawn remains inside sector bounds"), FMath::Abs(Enemy.Position.X) <= Colony.WorldHalfSize && FMath::Abs(Enemy.Position.Y) <= Colony.WorldHalfSize);
        TestTrue(TEXT("Threat spawn uses the relocated command core"), FVector2D::Distance(Enemy.Position, Destination) <= Radius + UE_KINDA_SMALL_NUMBER);
    }
    Colony.Tick(Colony.FixedStepSeconds());
    TestFalse(TEXT("Core placement cannot move a running colony"), Colony.SetInitialCorePosition(Original, Error));
    return true;
}
#endif
