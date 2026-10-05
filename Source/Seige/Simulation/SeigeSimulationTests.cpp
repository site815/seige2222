#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
    TestFalse(TEXT("Mismatched extractor cannot mine another material"),S.CanPlaceBuilding(TEXT("extract_iron_ore"),S.Nodes[1].Position,Error));
    TestTrue(TEXT("Matching extractor placement accepted"),S.PlaceBuilding(TEXT("extract_iron_ore"),S.Nodes[0].Position,Error));
    TestFalse(TEXT("Same deposit cannot host another extractor"),S.CanPlaceBuilding(TEXT("extract_iron_ore"),S.Nodes[0].Position+FVector2D(40,40),Error));
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
    FString GoodRecipes,GoodBuildings;
    FFileHelper::LoadFileToString(GoodRecipes,*FPaths::Combine(TestRules(),TEXT("recipes.json")));
    FFileHelper::LoadFileToString(GoodBuildings,*FPaths::Combine(TestRules(),TEXT("buildings.json")));
    FFileHelper::SaveStringToFile(GoodRecipes,*FPaths::Combine(BadRules,TEXT("recipes.json")));
    for(int32 Case=0;Case<5;++Case)
    {
        TSharedPtr<FJsonObject> Document;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GoodBuildings),Document);
        const auto Core=Document->GetArrayField(TEXT("buildings"))[0]->AsObject();
        if(Case==0)Core->SetNumberField(TEXT("reload_seconds"),0);
        if(Case==1)Core->SetNumberField(TEXT("reload_seconds"),.001);
        if(Case==2)Core->SetNumberField(TEXT("power_usage_kw"),1);
        if(Case==3)Core->SetNumberField(TEXT("damage_per_second"),999);
        if(Case==4)Core->RemoveField(TEXT("damage_per_shot"));
        FString Invalid;FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Invalid));
        FFileHelper::SaveStringToFile(Invalid,*FPaths::Combine(BadRules,TEXT("buildings.json")));
        TestFalse(*FString::Printf(TEXT("Runtime rejects inconsistent weapon/power rule variant %d"),Case),S.Initialize(BadRules,Error));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWeaponCadenceTest,"Seige.Simulation.WeaponCadenceAndPersistence",TestFlags)
bool FSeigeWeaponCadenceTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation A,B;
    if(!A.Initialize(TestRules(),Error)){AddError(Error);return false;}
    A.TriggerWave();A.Enemies.SetNum(1);A.Enemies[0].Position=FVector2D(500,0);
    const double Health=A.Enemies[0].Health,Damage=A.BuildingDefs[A.CoreDefinition].DamagePerShot;
    A.Tick(.05);
    if(!TestEqual(TEXT("A ready weapon applies one whole shot, not continuous tick damage"),A.Enemies.Num(),1))return false;
    TestEqual(TEXT("Shot damage agrees with the loaded weapon rule"),A.Enemies[0].Health,Health-Damage);
    TestTrue(TEXT("Firing starts a reload cycle"),A.Buildings[0].WeaponCooldown>0);
    if(!A.Save(TestSave(TEXT("weapon-mid-reload")),Error)||!B.Initialize(TestRules(),Error)||!B.Load(TestSave(TEXT("weapon-mid-reload")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Save/load preserves the active reload"),B.Buildings[0].WeaponCooldown,A.Buildings[0].WeaponCooldown);
    TestEqual(TEXT("Save/load preserves the actual shot event"),B.Buildings[0].LastShotTime,A.Buildings[0].LastShotTime);
    A.Tick(.8);B.Tick(.8);
    TestEqual(TEXT("A target takes no additional damage before reload finishes"),A.Enemies[0].Health,Health-Damage);
    TestEqual(TEXT("Loading does not grant a free shot"),B.Enemies[0].Health,A.Enemies[0].Health);
    A.Tick(.2);B.Tick(.2);
    TestEqual(TEXT("The next full shot defeats the target after the reload interval"),A.Enemies.Num(),0);
    if(!A.Save(TestSave(TEXT("weapon-a")),Error)||!B.Save(TestSave(TEXT("weapon-b")),Error)){AddError(Error);return false;}
    FString SA,SB;FFileHelper::LoadFileToString(SA,*TestSave(TEXT("weapon-a")));FFileHelper::LoadFileToString(SB,*TestSave(TEXT("weapon-b")));
    TestEqual(TEXT("Reloaded and uninterrupted combat states remain identical"),SA,SB);
    FSeigeSimulation C;if(!C.Initialize(TestRules(),Error)){AddError(Error);return false;}
    auto& Weapon=C.BuildingDefs[C.CoreDefinition];Weapon.DamagePerShot=1;Weapon.ReloadSeconds=.17;Weapon.DamagePerSecond=1/.17;
    C.TriggerWave();C.Enemies.SetNum(1);C.Enemies[0].Position=FVector2D(500,0);
    C.Tick(1);
    TestEqual(TEXT("Non-step-multiple reload retains elapsed time (six shots in one second including ready shot)"),C.Enemies[0].Health,Health-6);
    C.Enemies.Empty();C.Tick(1);TestEqual(TEXT("Idle weapon stores only one ready shot"),C.Buildings[0].WeaponCooldown,0.);
    C.TriggerWave();C.Enemies.SetNum(1);C.Enemies[0].Position=FVector2D(500,0);C.Tick(.05);
    TestEqual(TEXT("No idle-time damage backlog is released"),C.Enemies[0].Health,Health-1);
    C.Population=0;const double Before=C.Enemies[0].Health,Cooldown=C.Buildings[0].WeaponCooldown;C.Tick(.2);
    TestEqual(TEXT("Unstaffed weapons do not fire"),C.Enemies[0].Health,Before);
    TestEqual(TEXT("Unstaffed weapons pause reload progress"),C.Buildings[0].WeaponCooldown,Cooldown);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeBuildingInfoTest,"Seige.Simulation.BuildingInformation",TestFlags)
bool FSeigeBuildingInfoTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;if(!S.Initialize(TestRules(),Error)){AddError(Error);return false;}
    auto Value=[](const TArray<FSeigeBuildingInfoRow>& Rows,const FString& Section,const FString& Label)
    {const auto* Row=Rows.FindByPredicate([&](const auto& R){return R.Section==Section&&R.Label==Label;});return Row?Row->Value:FString();};
    for(const auto& Pair:S.BuildingDefs)
    {
        const auto Rows=S.BuildingInfo(Pair.Key,0,6);
        TSet<FString> Sections;for(const auto& Row:Rows)Sections.Add(Row.Section);
        TestEqual(TEXT("Every blueprint exposes all six information sections"),Sections.Num(),6);
        TestEqual(TEXT("Absent power demand is explicit"),Value(Rows,TEXT("Power"),TEXT("Power consumption")),FString(TEXT("0 kW")));
        TestEqual(TEXT("Absent power generation is explicit"),Value(Rows,TEXT("Power"),TEXT("Power generation")),FString(TEXT("0 kW")));
        TestFalse(TEXT("Every definition reports shot damage"),Value(Rows,TEXT("Weapons"),TEXT("Damage per shot")).IsEmpty());
        TestFalse(TEXT("Every definition reports reload time"),Value(Rows,TEXT("Weapons"),TEXT("Reload time")).IsEmpty());
        if(Pair.Value.DamagePerShot==0)
        {
            TestEqual(TEXT("Unarmed buildings say unarmed"),Value(Rows,TEXT("Weapons"),TEXT("Weapon")),FString(TEXT("Unarmed")));
            TestEqual(TEXT("Unarmed DPS is displayed as zero"),Value(Rows,TEXT("Weapons"),TEXT("Nominal DPS")),FString(TEXT("0 health/s")));
        }
    }
    if(!S.PlaceBuilding(TEXT("component_works"),FVector2D(700,0),Error)){AddError(Error);return false;}
    auto& B=S.Buildings.Last();B.Inventory.Add(TEXT("conductors"),3);
    const auto Live=S.BuildingInfo(B.DefId,B.Id,6);
    TestEqual(TEXT("Required but missing input stock remains visible as zero"),Value(Live,TEXT("Resources"),S.Resources[TEXT("circuits")].Name),FString(TEXT("0 units")));
    TestEqual(TEXT("Local inventory uses actual instance stock"),Value(Live,TEXT("Resources"),S.Resources[TEXT("conductors")].Name),FString(TEXT("3 units")));
    TestTrue(TEXT("Recipe inputs are shown with their quantities"),Value(Live,TEXT("Production"),TEXT("Inputs per cycle")).Contains(TEXT("2 ")+S.Resources[TEXT("alloy")].Name));
    TestEqual(TEXT("Unknown instances cannot show another building's stock"),S.BuildingInfo(B.DefId,S.Buildings[0].Id,6).Num(),0);
    const auto Core=S.BuildingInfo(S.CoreDefinition,S.Buildings[0].Id,6);
    TestEqual(TEXT("Displayed attack range uses the caller's rendering scale"),Value(Core,TEXT("Weapons"),TEXT("Attack range")),FString(TEXT("72 m")));
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
    // Protect each resource approach with both visibility and firepower. The northwest
    // defense covers the nearby iron/copper pair; southwest and southeast cover the
    // other two deposits. All costs and robot-production delays use the shipped rules.
    if(!Build(TEXT("sensor"),-900,950) || !Build(TEXT("sensor"),-1100,-750) || !Build(TEXT("sensor"),800,-1000)) return false;
    S.Tick(15.1);
    if(!Build(TEXT("turret"),-1450,1225) || !Build(TEXT("turret"),-1400,-1150) || !Build(TEXT("turret"),1250,-1250)) return false;
    if(!Build(TEXT("extract_iron_ore"),-1800,800) || !Build(TEXT("extract_copper_ore"),-1100,1650) || !Build(TEXT("extract_silica"),1500,-1550) || !Build(TEXT("extract_carbon"),-1700,-1300)) return false;
    if(!Build(TEXT("alloy_refinery"),-600,150) || !Build(TEXT("conductor_works"),-600,-350) || !Build(TEXT("substrate_works"),650,0) || !Build(TEXT("circuit_works"),650,-500) || !Build(TEXT("component_works"),0,700)) return false;
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
