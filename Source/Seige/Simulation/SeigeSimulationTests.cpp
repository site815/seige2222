#include "SeigeSimulation.h"
#include "AI/SeigeScenarioAI.h"
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
bool Deploy(FSeigeSimulation& S,FString& Error)
{
    if(!S.Initialize(TestRules(),Error))return false;
    S.Tick(S.BuildingDefs[S.CoreDefinition].ConstructionSeconds+S.FixedStepSeconds());
    return !S.Buildings[0].IsConstructing;
}
bool FinishSites(FSeigeSimulation& S,double Limit=180)
{
    for(double Elapsed=0;Elapsed<Limit&&!S.Escaped;Elapsed+=S.FixedStepSeconds())
    {
        bool Pending=false;for(const auto& B:S.Buildings)if(B.Health>0&&B.IsConstructing)Pending=true;
        if(!Pending)return true;S.Tick(S.FixedStepSeconds());
    }
    return false;
}
constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRulesTest,"Seige.Simulation.RuleValidation",TestFlags)
bool FSeigeRulesTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!TestTrue(TEXT("Current rules initialize"),Deploy(S,Error))) { AddError(Error); return false; }
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
    for(int32 Case=0;Case<8;++Case)
    {
        TSharedPtr<FJsonObject> Document;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GoodBuildings),Document);
        const auto Core=Document->GetArrayField(TEXT("buildings"))[0]->AsObject();
        if(Case==0)Core->SetNumberField(TEXT("reload_seconds"),0);
        if(Case==1)Core->SetNumberField(TEXT("reload_seconds"),.001);
        if(Case==2)Core->SetNumberField(TEXT("power_usage_kw"),1);
        if(Case==3)Core->SetNumberField(TEXT("damage_per_second"),999);
        if(Case==4)Core->RemoveField(TEXT("damage_per_shot"));
        if(Case==5)Core->SetNumberField(TEXT("construction_seconds"),0);
        if(Case==6)Core->SetNumberField(TEXT("construction_workers"),0);
        if(Case==7)Core->SetNumberField(TEXT("robot_support_capacity"),1);
        FString Invalid;FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Invalid));
        FFileHelper::SaveStringToFile(Invalid,*FPaths::Combine(BadRules,TEXT("buildings.json")));
        TestFalse(*FString::Printf(TEXT("Runtime rejects invalid weapon/power/construction/support variant %d"),Case),S.Initialize(BadRules,Error));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWeaponCadenceTest,"Seige.Simulation.WeaponCadenceAndPersistence",TestFlags)
bool FSeigeWeaponCadenceTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation A,B;
    if(!Deploy(A,Error)){AddError(Error);return false;}
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
    FSeigeSimulation C;if(!Deploy(C,Error)){AddError(Error);return false;}
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
    FString Error;FSeigeSimulation S;if(!Deploy(S,Error)){AddError(Error);return false;}
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
    if(!S.PlaceBuilding(TEXT("component_works"),FVector2D(700,0),Error)||!FinishSites(S)){AddError(Error);return false;}
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
    if (!Deploy(S,Error)) { AddError(Error); return false; }
    if (!S.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-700,0),Error)) { AddError(Error); return false; }
    if(!TestTrue(TEXT("Factory finishes using delivered materials and builders"),FinishSites(S)))return false;
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
    if (!Deploy(S,Error) || !S.PlaceBuilding(TEXT("sensor"),FVector2D(700,0),Error)) { AddError(Error); return false; }
    const int32 Start=S.Population, SensorId=S.Buildings.Last().Id;
    TestEqual(TEXT("Construction creates two builder vacancies"),S.TotalJobs-S.Employed,2);
    if(!TestTrue(TEXT("Sensor finishes with automatically produced builders"),FinishSites(S)))return false;
    S.Tick(13);
    TestEqual(TEXT("Construction surplus retires to the operating job count"),S.Population,Start+1);
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
    if (!Deploy(A,Error) || !A.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-700,0),Error)) { AddError(Error); return false; }
    A.Buildings[0].Inventory.Add(TEXT("iron_ore"),8); A.Buildings[0].Inventory.Add(TEXT("carbon"),4); A.Population=8; A.Tick(.4);
    if (!TestTrue(TEXT("Snapshot has in-transit cargo"),A.Couriers.Num()>0) || !A.Save(TestSave(TEXT("roundtrip")),Error) || !B.Initialize(TestRules(),Error) || !B.Load(TestSave(TEXT("roundtrip")),Error)) { AddError(Error); return false; }
    TestEqual(TEXT("Time restored"),A.Time,B.Time); TestEqual(TEXT("Cargo restored"),A.Couriers.Num(),B.Couriers.Num());
    A.Tick(110); B.Tick(110);
    TestEqual(TEXT("Same next wave schedule"),A.NextWaveTime,B.NextWaveTime); TestEqual(TEXT("Same courier count"),A.Couriers.Num(),B.Couriers.Num());
    TestEqual(TEXT("Same enemy count and random sequence"),A.Enemies.Num(),B.Enemies.Num());
    if (!A.Save(TestSave(TEXT("roundtrip-a")),Error) || !B.Save(TestSave(TEXT("roundtrip-b")),Error)) { AddError(Error); return false; }
    FString SA,SB; FFileHelper::LoadFileToString(SA,*TestSave(TEXT("roundtrip-a"))); FFileHelper::LoadFileToString(SB,*TestSave(TEXT("roundtrip-b")));
    TestEqual(TEXT("Continued complete simulation states are identical"),SA,SB);
    const double Before=B.Time; FString Corrupt=SB; Corrupt.ReplaceInline(TEXT("\"save_format\": 2"),TEXT("\"save_format\": 999"));
    FFileHelper::SaveStringToFile(Corrupt,*TestSave(TEXT("incompatible")));
    TestFalse(TEXT("Incompatible save rejected"),B.Load(TestSave(TEXT("incompatible")),Error)); TestEqual(TEXT("Rejected save leaves running colony untouched"),B.Time,Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFailureTest,"Seige.Simulation.CoreLossAndShuttle",TestFlags)
bool FSeigeFailureTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!Deploy(S,Error)) { AddError(Error); return false; }
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
    FString Error;FSeigeSimulation S;FSeigeScenarioAI Controller;
    if(!Controller.Initialize(S,TestRules(),FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES")),false,Error)){AddError(Error);return false;}
    // The controller uses only ordinary placement/toggle commands. No inventory, population,
    // construction-progress or threat overrides: the entire shipped bootstrap must survive.
    for(int32 I=0;I<180&&!S.Won&&!S.Escaped;++I)Controller.Tick(S,5);
    AddInfo(S.ObjectiveText()); AddInfo(S.WorkforceStatus());
    TestTrue(TEXT("Shipped first-playable scenario is winnable with only normal build actions"),S.Won);
    TestFalse(TEXT("Winning colony still stands"),S.Failed);
    TestTrue(TEXT("Actual physical deliveries occurred"),S.DeliveredUnits>0);
    TestTrue(TEXT("Scaled alien pulse occurred during test"),S.Wave>0);
    for(const FSeigeBuilding& B:S.Buildings) if(B.Health<=0) AddInfo(TEXT("Lost building during scenario: ")+B.DefId);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeConstructionTest,"Seige.Simulation.PhysicalConstructionAndReservations",TestFlags)
bool FSeigeConstructionTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S,Loaded;
    if(!S.Initialize(TestRules(),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Core begins as a shuttle deployment"),S.Buildings[0].IsConstructing);
    TestFalse(TEXT("Undeployed core has no active sensors"),S.IsVisible(FVector2D::ZeroVector));
    TestFalse(TEXT("Cannot order buildings during deployment"),S.PlaceBuilding(TEXT("sensor"),FVector2D(700,0),Error));
    S.Tick(2);
    TestTrue(TEXT("Builders make partial deployment progress"),S.Buildings[0].ConstructionProgress>0&&S.Buildings[0].ConstructionProgress<1);
    if(!S.Save(TestSave(TEXT("deploying")),Error)||!Loaded.Initialize(TestRules(),Error)||!Loaded.Load(TestSave(TEXT("deploying")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Deployment progress survives loading"),Loaded.Buildings[0].ConstructionProgress,S.Buildings[0].ConstructionProgress);
    if(!FinishSites(S)||!FinishSites(Loaded))return false;
    TestTrue(TEXT("Deployed core provides normal visibility"),S.IsVisible(FVector2D::ZeroVector));
    // Finite stock can fund one sensor plus protected operating buffers, never two.
    S.Buildings[0].Inventory[TEXT("alloy")]=14;S.Buildings[0].Inventory[TEXT("circuits")]=7;
    const double AlloyBefore=S.TotalStock(TEXT("alloy"));
    if(!TestTrue(TEXT("Affordable order queues a site"),S.PlaceBuilding(TEXT("sensor"),FVector2D(700,0),Error)))return false;
    const int32 Site=S.Buildings.Last().Id;
    TestEqual(TEXT("Ordering does not consume or teleport stock"),S.TotalStock(TEXT("alloy")),AlloyBefore);
    TestEqual(TEXT("Reserved materials remain physically at source"),S.FindBuilding(Site)->ConstructionMaterials.Num(),0);
    TestFalse(TEXT("Second order cannot double-spend queued reservations"),S.PlaceBuilding(TEXT("sensor"),FVector2D(-700,0),Error));
    S.Tick(.4);
    TestTrue(TEXT("Construction dispatch creates tagged physical cargo"),S.Couriers.ContainsByPredicate([](const auto& C){return C.ForConstruction;}));
    TestEqual(TEXT("Shipping conserves construction material"),S.TotalStock(TEXT("alloy")),AlloyBefore);
    TestEqual(TEXT("Builders wait for physical materials"),S.FindBuilding(Site)->ConstructionProgress,0.);
    TestFalse(TEXT("Site has no operational sensor range"),S.IsVisible(FVector2D(2350,0)));
    if(!S.Save(TestSave(TEXT("construction-cargo")),Error)||!Loaded.Load(TestSave(TEXT("construction-cargo")),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Saved in-flight cargo retains construction destination"),Loaded.Couriers.ContainsByPredicate([](const auto& C){return C.ForConstruction;}));
    S.Tick(30);Loaded.Tick(30);
    TestFalse(TEXT("Delivered materials and automatic builders finish the site"),S.FindBuilding(Site)->IsConstructing);
    TestTrue(TEXT("Only the completed staffed sensor extends coverage"),S.IsVisible(FVector2D(2350,0)));
    TestEqual(TEXT("Finished building embodies its physical material cost"),S.TotalStock(TEXT("alloy")),AlloyBefore-S.BuildingDefs[TEXT("sensor")].Cost[TEXT("alloy")]);
    if(!S.Save(TestSave(TEXT("construction-a")),Error)||!Loaded.Save(TestSave(TEXT("construction-b")),Error)){AddError(Error);return false;}
    FString A,B;FFileHelper::LoadFileToString(A,*TestSave(TEXT("construction-a")));FFileHelper::LoadFileToString(B,*TestSave(TEXT("construction-b")));
    TestEqual(TEXT("Construction continues deterministically after loading"),A,B);
    TSharedPtr<FJsonObject> Invalid;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(B),Invalid);
    Invalid->GetArrayField(TEXT("buildings"))[1]->AsObject()->SetNumberField(TEXT("construction_progress"),2);
    FString Raw;FJsonSerializer::Serialize(Invalid.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw));FFileHelper::SaveStringToFile(Raw,*TestSave(TEXT("construction-invalid")));
    const double Time=Loaded.Time;
    TestFalse(TEXT("Corrupt construction progress is rejected"),Loaded.Load(TestSave(TEXT("construction-invalid")),Error));
    TestEqual(TEXT("Rejected construction load is atomic"),Loaded.Time,Time);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRobotSupportTest,"Seige.Simulation.RobotSupportCapacityAndLocalMaintenance",TestFlags)
bool FSeigeRobotSupportTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;if(!Deploy(S,Error)){AddError(Error);return false;}
    const int32 CoreCapacity=S.RobotSupportCapacity;
    // Ordinary job demand exceeds the starter core's support capacity.
    for(const FVector2D P:{FVector2D(700,0),FVector2D(-700,0),FVector2D(0,700)})
        if(!S.PlaceBuilding(TEXT("sensor"),P,Error)){AddError(Error);return false;}
    S.Tick(45);
    TestEqual(TEXT("Population growth stops at actual service capacity"),S.Population,CoreCapacity);
    if(!S.PlaceBuilding(TEXT("robot_service_bay"),FVector2D(0,-700),Error)){AddError(Error);return false;}
    const int32 BayId=S.Buildings.Last().Id;
    TestEqual(TEXT("A service construction site grants no capacity"),S.RobotSupportCapacity,CoreCapacity);
    if(!FinishSites(S)){AddError(TEXT("Service expansion did not finish"));return false;}
    S.Tick(10);
    TestEqual(TEXT("Completed service bay expands real capacity"),S.RobotSupportCapacity,CoreCapacity+S.BuildingDefs[TEXT("robot_service_bay")].RobotSupportCapacity);
    TestTrue(TEXT("Open jobs can now grow beyond starter capacity"),S.Population>CoreCapacity);
    TestTrue(TEXT("Additional robots are allocated to the new service bay"),S.FindBuilding(BayId)->SupportedRobots>0);
    S.Buildings[0].Inventory.Add(TEXT("components"),2);S.FindBuilding(BayId)->Inventory.Remove(TEXT("components"));
    S.Couriers.RemoveAll([](const auto& C){return C.Resource==TEXT("components");});
    S.Tick(11);
    TestTrue(TEXT("Core can maintain its crew without exporting its protected buffer"),S.Buildings[0].MaintenanceSupplied);
    TestFalse(TEXT("Service upkeep requires its own local supplies"),S.FindBuilding(BayId)->MaintenanceSupplied);
    TestTrue(TEXT("Maintenance shortage has a real workforce consequence"),S.OperatingEfficiency()<1);
    S.FindBuilding(BayId)->Inventory.Add(TEXT("components"),5);S.Tick(11);
    TestTrue(TEXT("Delivered local components restore service at the next interval"),S.FindBuilding(BayId)->MaintenanceSupplied);
    S.ToggleBuilding(BayId);
    TestEqual(TEXT("Disabling a bay removes its usable capacity"),S.RobotSupportCapacity,CoreCapacity);
    TestTrue(TEXT("Unsupported robots remain and operate at shortage efficiency"),S.SupportedPopulation<S.Population&&S.OperatingEfficiency()<1);
    return true;
}
#endif
