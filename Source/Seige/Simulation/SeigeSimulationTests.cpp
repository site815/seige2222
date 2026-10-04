#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
FString TestRules() { return FPaths::Combine(FPaths::ProjectDir(), TEXT("Rules")); }
FString TestSave(const FString& Name) { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Name + TEXT(".json")); }
constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRulesTest,"Seige.Simulation.RuleValidation",TestFlags)
bool FSeigeRulesTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!TestTrue(TEXT("Current rules initialize"),S.Initialize(TestRules(),Error))) { AddError(Error); return false; }
    TestFalse(TEXT("Cannot build a second core"),S.CanPlaceBuilding(S.CoreDefinition,FVector2D(700,0),Error));
    TestFalse(TEXT("Mismatched extractor cannot mine another material"),S.CanPlaceBuilding(TEXT("extract_iron_ore"),FVector2D(1450,800),Error));
    TestTrue(TEXT("Matching extractor placement accepted"),S.PlaceBuilding(TEXT("extract_iron_ore"),FVector2D(-1500,600),Error));
    TestFalse(TEXT("Same deposit cannot host another extractor"),S.CanPlaceBuilding(TEXT("extract_iron_ore"),FVector2D(-1450,640),Error));
    TestFalse(TEXT("Missing directory fails visibly"),S.Initialize(FPaths::Combine(TestRules(),TEXT("missing-rules")),Error));
    TestFalse(TEXT("Missing rules produce a diagnostic"),Error.IsEmpty());
    const FString BadRules = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"),TEXT("BadRules"));
    IFileManager::Get().MakeDirectory(*BadRules,true);
    for (const FString& Name : {FString(TEXT("resources")),FString(TEXT("recipes")),FString(TEXT("buildings")),FString(TEXT("policies")),FString(TEXT("scenario"))})
    {
        FString Text; FFileHelper::LoadFileToString(Text,*FPaths::Combine(TestRules(),Name+TEXT(".json")));
        if (Name == TEXT("recipes")) Text.ReplaceInline(TEXT("\"iron_ore\""),TEXT("\"nonexistent_item\""));
        FFileHelper::SaveStringToFile(Text,*FPaths::Combine(BadRules,Name+TEXT(".json")));
    }
    TestFalse(TEXT("Unknown recipe reference rejected by runtime"),S.Initialize(BadRules,Error));
    TestTrue(TEXT("Unknown reference diagnostic names bad item"),Error.Contains(TEXT("nonexistent_item")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePhysicalDeliveryTest,"Seige.Simulation.PhysicalDelivery",TestFlags)
bool FSeigePhysicalDeliveryTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!S.Initialize(TestRules(),Error)) { AddError(Error); return false; }
    if (!S.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-700,0),Error)) { AddError(Error); return false; }
    S.Population=8; S.Buildings[0].Inventory.Add(TEXT("iron_ore"),8); S.Buildings[0].Inventory.Add(TEXT("carbon"),4);
    // A very fast recipe cannot consume resources remotely before the physical couriers arrive.
    S.Recipes[TEXT("smelt_alloy")].Seconds=.01;
    const double IronBefore=S.TotalStock(TEXT("iron_ore")), CarbonBefore=S.TotalStock(TEXT("carbon"));
    S.Tick(.4);
    TestTrue(TEXT("Input couriers dispatched"),S.Couriers.Num()>0);
    TestEqual(TEXT("Iron conserved across dispatch"),S.TotalStock(TEXT("iron_ore")),IronBefore);
    TestEqual(TEXT("Carbon conserved across dispatch"),S.TotalStock(TEXT("carbon")),CarbonBefore);
    TestEqual(TEXT("Unrelated copper not inflated by other cargo"),S.TotalStock(TEXT("copper_ore")),0.0);
    TestEqual(TEXT("No production before input delivery"),S.ProducedUnits.FindRef(TEXT("alloy")),0.0);
    TestEqual(TEXT("Remote factory still has no iron"),S.Buildings[1].Inventory.FindRef(TEXT("iron_ore")),0.0);
    S.Tick(3);
    TestTrue(TEXT("Production starts after local delivery"),S.ProducedUnits.FindRef(TEXT("alloy"))>0);
    TestTrue(TEXT("Fast data-defined recipes retain normalized progress"),S.Buildings[1].Progress<1);
    TestTrue(TEXT("Delivered units tracked"),S.DeliveredUnits>0);
    const double Used=S.ProducedUnits.FindRef(TEXT("alloy"));
    TestTrue(TEXT("Iron transformed according to recipe"),FMath::IsNearlyEqual(S.TotalStock(TEXT("iron_ore"))+Used,IronBefore));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkforceTest,"Seige.Simulation.AutomaticWorkforceAndRepair",TestFlags)
bool FSeigeWorkforceTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!S.Initialize(TestRules(),Error) || !S.PlaceBuilding(TEXT("sensor"),FVector2D(700,0),Error)) { AddError(Error); return false; }
    const int32 Start=S.Population, SensorId=S.Buildings.Last().Id;
    TestEqual(TEXT("New sensor creates an open job"),S.TotalJobs-S.Employed,1);
    S.Tick(6);
    TestEqual(TEXT("Core automatically produces required robot"),S.Population,Start+1);
    TestEqual(TEXT("All jobs automatically filled"),S.Employed,S.TotalJobs);
    S.ToggleBuilding(SensorId); S.Tick(13);
    TestEqual(TEXT("Disabled building removes job demand and surplus retires"),S.Population,Start);
    FSeigeBuilding* B=S.FindBuilding(SensorId); B->Health-=50; B->Inventory.Add(TEXT("alloy"),2);
    const double Before=B->Health; S.Tick(1);
    TestTrue(TEXT("Repair is automatic on disabled structures"),S.FindBuilding(SensorId)->Health>Before);
    TestTrue(TEXT("Repair consumes local material"),S.FindBuilding(SensorId)->Inventory.FindRef(TEXT("alloy"))<2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePersistenceTest,"Seige.Simulation.SaveLoadDeterminism",TestFlags)
bool FSeigePersistenceTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation A,B;
    if (!A.Initialize(TestRules(),Error) || !A.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-700,0),Error)) { AddError(Error); return false; }
    A.Buildings[0].Inventory.Add(TEXT("iron_ore"),8); A.Buildings[0].Inventory.Add(TEXT("carbon"),4); A.Population=8; A.Tick(.4);
    if (!TestTrue(TEXT("Snapshot has in-transit cargo"),A.Couriers.Num()>0) || !A.Save(TestSave(TEXT("roundtrip")),Error) || !B.Initialize(TestRules(),Error) || !B.Load(TestSave(TEXT("roundtrip")),Error)) { AddError(Error); return false; }
    TestEqual(TEXT("Time restored"),A.Time,B.Time); TestEqual(TEXT("Cargo restored"),A.Couriers.Num(),B.Couriers.Num());
    A.Tick(110); B.Tick(110);
    TestEqual(TEXT("Same next wave schedule"),A.NextWaveTime,B.NextWaveTime); TestEqual(TEXT("Same courier count"),A.Couriers.Num(),B.Couriers.Num());
    TestEqual(TEXT("Same enemy count and random sequence"),A.Enemies.Num(),B.Enemies.Num());
    if (!A.Save(TestSave(TEXT("roundtrip-a")),Error) || !B.Save(TestSave(TEXT("roundtrip-b")),Error)) { AddError(Error); return false; }
    FString SA,SB; FFileHelper::LoadFileToString(SA,*TestSave(TEXT("roundtrip-a"))); FFileHelper::LoadFileToString(SB,*TestSave(TEXT("roundtrip-b")));
    TestEqual(TEXT("Continued complete simulation states are identical"),SA,SB);
    const double Before=B.Time; FString Corrupt=SB; Corrupt.ReplaceInline(TEXT("\"save_format\": 1"),TEXT("\"save_format\": 999"));
    FFileHelper::SaveStringToFile(Corrupt,*TestSave(TEXT("incompatible")));
    TestFalse(TEXT("Incompatible save rejected"),B.Load(TestSave(TEXT("incompatible")),Error)); TestEqual(TEXT("Rejected save leaves running colony untouched"),B.Time,Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFailureTest,"Seige.Simulation.CoreLossAndShuttle",TestFlags)
bool FSeigeFailureTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!S.Initialize(TestRules(),Error)) { AddError(Error); return false; }
    S.Buildings[0].Health=1; S.Buildings[0].Inventory.Remove(TEXT("alloy"));
    S.ShuttleCargo.Add(TEXT("circuits"),3);
    for(int32 I=0;I<8;++I) { FSeigeEnemy E; E.Id=1000+I; E.Health=45; E.Position=FVector2D(20,20); S.Enemies.Add(E); }
    S.Tick(1);
    TestTrue(TEXT("Core loss ends colony command"),S.Failed); TestTrue(TEXT("Shuttle launches automatically"),S.Escaped);
    TestEqual(TEXT("Only preloaded shuttle cargo escapes"),S.ShuttleCargo.FindRef(TEXT("circuits")),3.0);
    TestEqual(TEXT("Core components are not magically loaded"),S.ShuttleCargo.FindRef(TEXT("components")),0.0);
    const double EndTime=S.Time; S.Tick(10); TestEqual(TEXT("Escaped prototype stops simulation"),S.Time,EndTime);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePlayableTest,"Seige.Simulation.FirstPlayableSolvable",TestFlags)
bool FSeigePlayableTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!S.Initialize(TestRules(),Error)) { AddError(Error); return false; }
    auto Build=[&](const TCHAR* Id,double X,double Y)->bool { if(!S.PlaceBuilding(Id,FVector2D(X,Y),Error)) { AddError(FString(Id)+TEXT(": ")+Error); return false; } return true; };
    // All costs, staffing, delivery times and threats use shipped rules; no inventory or health edits.
    if(!Build(TEXT("sensor"),-100,-1500)) return false;
    S.Tick(5.1);
    if(!Build(TEXT("turret"),-1100,-1050) || !Build(TEXT("turret"),-950,500) || !Build(TEXT("turret"),1150,450) || !Build(TEXT("turret"),1000,-1350)) return false;
    if(!Build(TEXT("extract_iron_ore"),-1500,600) || !Build(TEXT("extract_copper_ore"),1450,800) || !Build(TEXT("extract_silica"),1500,-1550) || !Build(TEXT("extract_carbon"),-1700,-1300)) return false;
    if(!Build(TEXT("alloy_refinery"),-600,150) || !Build(TEXT("conductor_works"),-600,-350) || !Build(TEXT("substrate_works"),650,0) || !Build(TEXT("circuit_works"),650,-500) || !Build(TEXT("component_works"),0,700) || !Build(TEXT("depot"),0,1150)) return false;
    for(int32 I=0;I<90 && !S.Won && !S.Escaped;++I) S.Tick(5);
    AddInfo(S.ObjectiveText()); AddInfo(S.WorkforceStatus());
    TestTrue(TEXT("Shipped first-playable scenario is winnable with only normal build actions"),S.Won);
    TestFalse(TEXT("Winning colony still stands"),S.Failed);
    TestTrue(TEXT("Actual physical deliveries occurred"),S.DeliveredUnits>0);
    TestTrue(TEXT("Scaled alien pulse occurred during test"),S.Wave>0);
    for(const FSeigeBuilding& B:S.Buildings) if(B.Health<=0) AddInfo(TEXT("Lost building during scenario: ")+B.DefId);
    return true;
}
#endif
