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
    if(!S.Initialize(TestRules(),Error,false,false))return false;
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(S.Buildings[0].IsConstructing){Error=TEXT("Finite starter workers could not complete physical deployment");return false;}S.Tick(20);return true;
}
double CoreWorkerSeconds(const FSeigeSimulation& S)
{
    const auto& Core=S.Buildings[0];double Seconds=0;
    for(const auto& Id:S.ProductionOptions(Core.Id))if(S.Recipes[Id].WorkerOutput>0)Seconds=FMath::Max(Seconds,S.ProductionSeconds(Core,Id)/S.Recipes[Id].WorkerOutput);
    return Seconds;
}
bool FinishSites(FSeigeSimulation& S,double Limit=0)
{
    if(Limit<=0){Limit=600+FMath::Max(0,S.TotalJobs-S.Population-S.InactiveWorkerCount())*CoreWorkerSeconds(S)*2;for(const auto& B:S.Buildings)if(B.IsConstructing)Limit+=S.Definition(B)->ConstructionSeconds*2;}
    for(double Elapsed=0;Elapsed<Limit&&!S.Escaped;Elapsed+=10)
    {
        bool Pending=false;for(const auto& B:S.Buildings)if(B.Health>0&&B.IsConstructing&&B.Enabled)Pending=true;
        if(!Pending)return true;S.Tick(10);
    }
    return false;
}
bool ConnectPower(FSeigeSimulation& S,int32 Id,FString& Error)
{
    auto* B=S.FindBuilding(Id);if(!B)return false;const bool Enabled=B->Enabled;if(Enabled)S.ToggleBuilding(Id);
    TArray<FVector2D> Route;if(!S.FindRoadRoute(S.BuildingAccessPoint(S.Buildings[0]),S.BuildingAccessPoint(*B),Route))return false;
    FVector2D Last=S.BuildingAccessPoint(S.Buildings[0]);
    for(const auto& Point:Route){if(FVector2D::Distance(Last,Point)<.001)continue;if(!S.PlaceRoad(Last,Point,Error))return false;const int Road=S.Roads.Last().Id;for(int I=0;I<1200&&S.FindRoad(Road)->IsConstructing;++I)S.Tick(10);if(S.FindRoad(Road)->IsConstructing)return false;Last=Point;}
    if(Enabled)S.ToggleBuilding(Id);S.Tick(S.FixedStepSeconds());return S.IsRoadGridConnected(S.Buildings[0].Id,Id);
}
constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRulesTest,"Seige.Simulation.RuleValidation",TestFlags)
bool FSeigeRulesTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if(!S.Initialize(TestRules(),Error,false,false)){AddError(Error);return false;}
    const auto Node=S.Nodes[0];FString Extractor;
    for(const auto& Id:S.BuildMenu)if(S.BuildingDefs[Id].ExtractionRates.Contains(Node.Resource)){Extractor=Id;break;}
    if(!S.SetInitialCorePosition(Node.Position-FVector2D(1200,0),Error)){AddError(Error);return false;}if(!TestTrue(TEXT("Workers physically complete deployment before testing mine orders"),FinishSites(S)))return false;
    TestFalse(TEXT("Cannot build a second core"),S.CanPlaceBuilding(S.CoreDefinition,Node.Position,Error));
    TestFalse(TEXT("Extraction Mine cannot operate without a deposit"),S.CanPlaceBuilding(Extractor,Node.Position+FVector2D(600,0),Error));
    TestTrue(TEXT("Generic mine placement accepted"),S.PlaceBuilding(Extractor,Node.Position,Error));
    TestFalse(TEXT("Same deposit cannot host another extractor"),S.CanPlaceBuilding(Extractor,Node.Position+FVector2D(40,40),Error));
    TestFalse(TEXT("Missing directory fails visibly"),S.Initialize(FPaths::Combine(TestRules(),TEXT("missing-rules")),Error));
    TestFalse(TEXT("Missing rules produce a diagnostic"),Error.IsEmpty());
    const FString BadRules = FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation"),TEXT("BadRules"));
    IFileManager::Get().MakeDirectory(*BadRules,true);
    for (const FString& Name : {FString(TEXT("resources")),FString(TEXT("recipes")),FString(TEXT("buildings")),FString(TEXT("policies")),FString(TEXT("scenario")),FString(TEXT("transport")),FString(TEXT("energy")),FString(TEXT("trade")),FString(TEXT("companions")),FString(TEXT("walls")),FString(TEXT("combat")),FString(TEXT("weapons")),FString(TEXT("chassis")),FString(TEXT("workers")),FString(TEXT("calendar")),FString(TEXT("environment"))})
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
    for(int32 Case=0;Case<10;++Case)
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
        if(Case==8)Core->SetStringField(TEXT("inventory_presentation"),TEXT("nowhere"));
        if(Case==9)Core->SetStringField(TEXT("worker_activity"),TEXT("unbounded_patrol"));
        FString Invalid;FJsonSerializer::Serialize(Document.ToSharedRef(),TJsonWriterFactory<>::Create(&Invalid));
        FFileHelper::SaveStringToFile(Invalid,*FPaths::Combine(BadRules,TEXT("buildings.json")));
        TestFalse(*FString::Printf(TEXT("Runtime rejects invalid weapon/power/construction/support variant %d"),Case),S.Initialize(BadRules,Error));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeBlockedCourierSaveTest,"Seige.Simulation.BlockedCourierPersistenceAndOccupiedPlots",TestFlags)
bool FSeigeBlockedCourierSaveTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const FVector2D Origin=S.Buildings[0].Position,Site=Origin+FVector2D(1400,0),OpenPlot=Origin+FVector2D(0,1400);
    if(!S.PlaceBuilding(TEXT("sensor"),Site,Error)){AddError(Error);return false;}
    for(int I=0;I<4000&&(!S.Couriers.Num()||S.Couriers[0].Amount<=0);++I)S.Tick(S.FixedStepSeconds());
    if(!TestTrue(TEXT("Fixture obtains an actual paid construction delivery"),!S.Couriers.IsEmpty()))return false;
    const int32 CourierId=S.Couriers[0].Id,TargetId=S.Couriers[0].TargetId;
    auto* C=S.Couriers.FindByPredicate([&](const auto& X){return X.Id==CourierId;});
    // Reproduce the legitimate pre-fix waiting state left when a replacement
    // structure enclosed a returning courier. Payload still comes from dispatch.
    auto* Body=S.Workers.Find(C->WorkerId);if(!Body)return false;Body->Position=Site;Body->Route.Empty();Body->NextWaypoint=0;Body->RetryAt=0;C->Position=Site;C->Route.Empty();C->NextWaypoint=0;
    const double Payload=C->Amount;const FString Resource=C->Resource;
    S.Tick(S.FixedStepSeconds());C=S.Couriers.FindByPredicate([&](const auto& X){return X.Id==CourierId;});
    TestTrue(TEXT("A physically blocked courier waits without teleporting through its obstruction"),C&&C->Route.IsEmpty()&&C->Position.Equals(Site)&&C->Amount==Payload);
    const double Stock=S.TotalStock(Resource);
    const FString File=TestSave(TEXT("blocked-courier"));
    if(!S.Save(File,Error)){AddError(Error);return false;}
    FSeigeSimulation Loaded;if(!Loaded.Initialize(TestRules(),Error,false,false)||!Loaded.Load(File,Error)){AddError(Error);return false;}
    const auto* Restored=Loaded.Couriers.FindByPredicate([&](const auto& X){return X.Id==CourierId;});
    TestTrue(TEXT("Strict load preserves empty waiting route and its physical cargo"),Restored&&Restored->Route.IsEmpty()&&Restored->NextWaypoint==0&&Restored->Amount==Payload&&Restored->Position.Equals(Site));
    TestEqual(TEXT("Waiting delivery does not duplicate or discard resource stock"),Loaded.TotalStock(Resource),Stock);
    Loaded.Tick(Loaded.FixedStepSeconds());Restored=Loaded.Couriers.FindByPredicate([&](const auto& X){return X.Id==CourierId;});
    TestTrue(TEXT("Loaded courier remains blocked and conserved while topology is unchanged"),Restored&&Restored->Position.Equals(Site)&&Restored->Amount==Payload);
    Loaded.FindBuilding(TargetId)->Health=0;Loaded.OnBuildingDestroyed(TargetId);Loaded.Tick(Loaded.FixedStepSeconds());
    Restored=Loaded.Couriers.FindByPredicate([&](const auto& X){return X.Id==CourierId;});
    TestTrue(TEXT("Courier retries and walks a return route once the obstruction is gone"),Restored&&!Restored->Route.IsEmpty()&&!Restored->Position.Equals(Site)&&Restored->Amount==Payload);

    FString Raw;TSharedPtr<FJsonObject> Json;if(!FFileHelper::LoadFileToString(Raw,*File)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Json))return false;
    Json->GetArrayField(TEXT("couriers"))[0]->AsObject()->SetNumberField(TEXT("next_waypoint"),1);
    Raw.Empty();FJsonSerializer::Serialize(Json.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw));FFileHelper::SaveStringToFile(Raw,*TestSave(TEXT("invalid-waiting-courier")));
    const double BeforeLoad=Loaded.Time;
    TestFalse(TEXT("An empty route cannot claim a progressed waypoint"),Loaded.Load(TestSave(TEXT("invalid-waiting-courier")),Error));
    TestEqual(TEXT("Rejected waiting-route corruption is atomic"),Loaded.Time,BeforeLoad);

    FSeigeSimulation Probe=S;Probe.Couriers.Empty();
    TestTrue(TEXT("Occupancy probe starts with a legal free reserved plot"),Probe.CanPlaceBuilding(TEXT("sensor"),OpenPlot,Error));
    Probe.Couriers.Add(S.Couriers[0]);Probe.Couriers[0].Position=OpenPlot;
    TestFalse(TEXT("New plots cannot enclose a physical delivery worker"),Probe.CanPlaceBuilding(TEXT("sensor"),OpenPlot,Error));Probe.Couriers.Empty();
    auto& Crew=Probe.Buildings.Last();Crew.TravellingBuilders=1;Crew.BuilderPosition=OpenPlot;
    TestFalse(TEXT("New plots cannot enclose travelling construction crews"),Probe.CanPlaceBuilding(TEXT("sensor"),OpenPlot,Error));Crew.TravellingBuilders=0;
    auto& Vehicle=Probe.Combat.Vehicles[0];Vehicle.Embarked=false;Vehicle.Position=OpenPlot;
    TestFalse(TEXT("New plots cannot enclose a deployed living vehicle"),Probe.CanPlaceBuilding(TEXT("sensor"),OpenPlot,Error));Vehicle.Embarked=true;
    TestTrue(TEXT("After occupants leave, ordinary paid placement remains possible"),Probe.CanPlaceBuilding(TEXT("sensor"),OpenPlot,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWeaponCadenceTest,"Seige.Simulation.WeaponCadenceAndPersistence",TestFlags)
bool FSeigeWeaponCadenceTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation A,B;if(!Deploy(A,Error)){AddError(Error);return false;}
    // A real tower/core loadout is persisted independently of the compatibility
    // display fields. Keep one low-damage laser in this controlled cadence test.
    A.Combat.Vehicles.Empty();auto& State=A.Combat.BuildingState[A.Buildings[0].Id];
    State.Weapons={TEXT("laser_small")};State.Cooldowns={0};
    A.PeriodicAttacksEnabled=true;A.TriggerWave();A.Enemies.SetNum(1);A.Enemies[0].Position=FVector2D(500,0);
    const double Health=A.Enemies[0].Health,Damage=A.Combat.Weapons[TEXT("laser_small")].Damage;
    A.Tick(.05);if(!TestEqual(TEXT("Target survives the small laser shot"),A.Enemies.Num(),1))return false;
    TestEqual(TEXT("Discrete shot uses the combat weapon definition"),A.Enemies[0].Health,Health-Damage);
    TestTrue(TEXT("Authoritative module cooldown is active"),A.Combat.BuildingState[A.Buildings[0].Id].Cooldowns[0]>0);
    if(!A.Save(TestSave(TEXT("weapon-mid-reload")),Error)||!B.Initialize(TestRules(),Error)||!B.Load(TestSave(TEXT("weapon-mid-reload")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Reload duration survives save"),B.Combat.BuildingState[B.Buildings[0].Id].Cooldowns[0],A.Combat.BuildingState[A.Buildings[0].Id].Cooldowns[0]);
    A.Tick(.8);B.Tick(.8);TestEqual(TEXT("No extra shot during reload"),A.Enemies[0].Health,Health-Damage);
    A.Tick(1.5);B.Tick(1.5);TestEqual(TEXT("Loaded combat continues identically"),B.Enemies[0].Health,A.Enemies[0].Health);
    if(!A.Save(TestSave(TEXT("weapon-a")),Error)||!B.Save(TestSave(TEXT("weapon-b")),Error)){AddError(Error);return false;}
    FString SA,SB;FFileHelper::LoadFileToString(SA,*TestSave(TEXT("weapon-a")));FFileHelper::LoadFileToString(SB,*TestSave(TEXT("weapon-b")));TestEqual(TEXT("RNG, fleet and cooldown save continuation"),SA,SB);
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
        TestEqual(TEXT("Power demand reflects external energy definition"),Value(Rows,TEXT("Power"),TEXT("Power consumption")),FString::SanitizeFloat(S.Energy.Definition(Pair.Key)->IdleKW,0)+TEXT(" kW"));
        TestEqual(TEXT("Generation reflects external energy definition"),Value(Rows,TEXT("Power"),S.Energy.Definition(Pair.Key)->GenerationSource==TEXT("solar")?TEXT("Rated generation (daylight peak)"):TEXT("Rated generation")),FString::SanitizeFloat(S.Energy.Definition(Pair.Key)->GenerationKW,0)+TEXT(" kW"));
        const auto* Platform=S.Combat.BuildingPlatforms.Find(Pair.Key);double DPS=0;
        if(Platform)for(int I=0;I<Platform->Weapons.Num();++I){const auto& Weapon=S.Combat.Weapons[Platform->Weapons[I]];DPS+=Weapon.Damage/Weapon.ReloadSeconds;const FString Label=FString::Printf(TEXT("Mount %d - %s"),I+1,*Weapon.Name);const FString Details=Value(Rows,TEXT("Weapons"),Label);TestTrue(TEXT("Every real mount exposes authored shot damage, reload and metre range"),Details.Contains(FString::SanitizeFloat(Weapon.Damage,0)+TEXT(" damage"))&&Details.Contains(FString::SanitizeFloat(Weapon.ReloadSeconds,0)+TEXT(" s"))&&Details.Contains(FString::SanitizeFloat(Weapon.RangeMeters,0)+TEXT(" m")));}
        TestEqual(TEXT("Displayed theoretical DPS sums real equipped modules"),Value(Rows,TEXT("Weapons"),TEXT("Nominal DPS")),FString::SanitizeFloat(DPS,0)+TEXT(" health/s before misses and protection"));
        if(!Platform||Platform->Weapons.IsEmpty())
        {
            TestEqual(TEXT("Unarmed buildings explicitly show zero capability"),Value(Rows,TEXT("Weapons"),TEXT("Weapon")),FString(TEXT("Unarmed (0)")));
            TestEqual(TEXT("Unarmed shot damage is displayed as zero"),Value(Rows,TEXT("Weapons"),TEXT("Damage per shot")),FString(TEXT("0")));
        }
    }
    if(!S.PlaceBuilding(TEXT("component_works"),FVector2D(1200,0),Error)||!FinishSites(S)){AddError(Error);return false;}
    auto& B=S.Buildings.Last();B.Inventory.Add(TEXT("conductors"),3);
    const auto Live=S.BuildingInfo(B.DefId,B.Id,6);
    TestTrue(TEXT("Required missing local input stock remains zero while inbound cargo is listed"),Value(Live,TEXT("Resources"),S.Resources[TEXT("circuits")].Name).StartsWith(TEXT("0 kg")));
    double Inbound=0;for(const auto& Cargo:S.Couriers)if(Cargo.TargetId==B.Id&&Cargo.Resource==TEXT("conductors"))Inbound+=Cargo.Amount+Cargo.ReservedAmount;
    const FString ExpectedStock=TEXT("3 kg")+(Inbound>0?TEXT(" (+")+FString::SanitizeFloat(Inbound,0)+TEXT(" inbound)"):FString());
    TestEqual(TEXT("Local inventory and separately claimed inbound cargo use their actual amounts"),Value(Live,TEXT("Resources"),S.Resources[TEXT("conductors")].Name),ExpectedStock);
    TestTrue(TEXT("Recipe inputs are shown with their quantities"),Value(Live,TEXT("Production"),TEXT("Inputs per cycle")).Contains(TEXT("2 kg ")+S.Resources[TEXT("alloy")].Name));
    TestEqual(TEXT("Unknown instances cannot show another building's stock"),S.BuildingInfo(B.DefId,S.Buildings[0].Id,6).Num(),0);
    const auto Core=S.BuildingInfo(S.CoreDefinition,S.Buildings[0].Id,6);
    TestFalse(TEXT("Live core exposes physical mount capacity"),Value(Core,TEXT("Weapons"),TEXT("Mount area used / capacity")).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePhysicalDeliveryTest,"Seige.Simulation.PhysicalDelivery",TestFlags)
bool FSeigePhysicalDeliveryTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!Deploy(S,Error)) { AddError(Error); return false; }
    if (!S.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-1200,0),Error)) { AddError(Error); return false; }
    if(!TestTrue(TEXT("Factory finishes using delivered materials and builders"),FinishSites(S)))return false;
    if(!ConnectPower(S,S.Buildings[1].Id,Error)){AddError(Error);return false;}
    // The ordinary setup may already have stocked the refinery while its road
    // was being built. Finish those transfers, then return only the test inputs
    // and any paid batch to the source so this probe starts before fresh pickup.
    const int32 Refinery=S.Buildings[1].Id;S.ToggleBuilding(Refinery);
    for(int32 I=0;I<120&&!S.Couriers.IsEmpty();++I)S.Tick(10);
    auto& Factory=*S.FindBuilding(Refinery);
    for(const auto& P:Factory.ProductionInputs)Factory.Inventory.FindOrAdd(P.Key)+=P.Value;
    Factory.ProductionInputs.Empty();Factory.ProductionCommitted=false;Factory.CommittedRecipe.Empty();Factory.Progress=Factory.ProductionReservedLitres=0;
    for(const auto& P:S.Recipes[TEXT("smelt_alloy")].Inputs){S.Buildings[0].Inventory.FindOrAdd(P.Key)+=Factory.Inventory.FindRef(P.Key);Factory.Inventory.Remove(P.Key);}
    S.ToggleBuilding(Refinery);
    // A very fast recipe cannot consume resources remotely before the physical couriers arrive.
    S.Recipes[TEXT("smelt_alloy")].Seconds=.01;
    const double IronBefore=S.TotalStock(TEXT("iron_ore")), CarbonBefore=S.TotalStock(TEXT("carbon")), AlloyBefore=S.ProducedUnits.FindRef(TEXT("alloy"));
    S.Tick(2.1);
    TestTrue(TEXT("Input couriers dispatched"),S.Couriers.Num()>0);
    TestEqual(TEXT("Iron conserved across dispatch"),S.TotalStock(TEXT("iron_ore")),IronBefore);
    TestEqual(TEXT("Carbon conserved across dispatch"),S.TotalStock(TEXT("carbon")),CarbonBefore);
    TestEqual(TEXT("Unrelated copper not inflated by other cargo"),S.TotalStock(TEXT("copper_ore")),0.0);
    TestEqual(TEXT("No production before input delivery"),S.ProducedUnits.FindRef(TEXT("alloy")),AlloyBefore);
    TestEqual(TEXT("Remote factory still has no iron"),S.Buildings[1].Inventory.FindRef(TEXT("iron_ore")),0.0);
    for(int32 I=0;I<120&&S.ProducedUnits.FindRef(TEXT("alloy"))==AlloyBefore;++I)S.Tick(10);
    TestTrue(TEXT("Production starts after local delivery"),S.ProducedUnits.FindRef(TEXT("alloy"))>AlloyBefore);
    TestTrue(TEXT("Fast data-defined recipes retain normalized progress"),S.Buildings[1].Progress<1);
    TestTrue(TEXT("Delivered units tracked"),S.DeliveredUnits>0);
    const double Used=(S.ProducedUnits.FindRef(TEXT("alloy"))-AlloyBefore)*S.Recipes[TEXT("smelt_alloy")].Inputs[TEXT("iron_ore")]/S.Recipes[TEXT("smelt_alloy")].Outputs[TEXT("alloy")];
    TestTrue(TEXT("Iron transformed according to recipe"),FMath::IsNearlyEqual(S.TotalStock(TEXT("iron_ore"))+Used,IronBefore));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkforceTest,"Seige.Simulation.AutomaticWorkforceAndRepair",TestFlags)
bool FSeigeWorkforceTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation S;
    if (!Deploy(S,Error)) { AddError(Error); return false; }
    const int32 Start=S.Population,OriginalJobs=S.TotalJobs;TSet<FString> OriginalIds;for(const auto& W:S.Workers.Bodies)OriginalIds.Add(W.Id);
    if(!S.PlaceBuilding(TEXT("sensor"),FVector2D(1200,0),Error)){AddError(Error);return false;}
    const int32 SensorId=S.Buildings.Last().Id;
    TestEqual(TEXT("A construction order adds its authored builder jobs"),S.TotalJobs-OriginalJobs,S.RequiredBuilders(S.Buildings.Last()));
    if(!TestTrue(TEXT("Existing finite workers physically finish the sensor"),FinishSites(S)))return false;
    for(double Elapsed=0;Elapsed<CoreWorkerSeconds(S)*12+600&&(S.Population<S.TotalJobs||S.FindBuilding(SensorId)->Workers<S.Definition(*S.FindBuilding(SensorId))->Jobs);Elapsed+=10)S.Tick(10);
    TestEqual(TEXT("Paid manufacturing reaches supported operational and logistics demand"),S.Population,S.TotalJobs);
    TestTrue(TEXT("Vacancies create additional actual worker identities"),S.Population>Start&&S.Workers.Bodies.Num()>OriginalIds.Num());
    TestEqual(TEXT("Operating sensor is staffed by workers who arrived"),S.FindBuilding(SensorId)->Workers,S.Definition(*S.FindBuilding(SensorId))->Jobs);
    for(const auto& Id:OriginalIds)TestNotNull(TEXT("Reassignment preserves every original worker identity"),S.Workers.Find(Id));
    S.ToggleBuilding(SensorId);
    for(int32 I=0;I<120&&(S.Population>S.TotalJobs||S.InactiveWorkerCount()==0);++I)S.Tick(10);
    TestEqual(TEXT("Disabled building removes jobs after surplus workers physically return to storage"),S.Population,S.TotalJobs);
    TestTrue(TEXT("Returned surplus remains represented by a stored body"),S.InactiveWorkerCount()>0&&S.Workers.Bodies.ContainsByPredicate([](const auto& W){return W.State==TEXT("stored");}));
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
    if (!Deploy(A,Error) || !A.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-1200,0),Error)) { AddError(Error); return false; }
    A.Buildings[0].Inventory.Add(TEXT("iron_ore"),8); A.Buildings[0].Inventory.Add(TEXT("carbon"),4); A.Tick(2.1);
    if (!TestTrue(TEXT("Snapshot has in-transit cargo"),A.Couriers.Num()>0) || !A.Save(TestSave(TEXT("roundtrip")),Error) || !B.Initialize(TestRules(),Error) || !B.Load(TestSave(TEXT("roundtrip")),Error)) { AddError(Error); return false; }
    TestEqual(TEXT("Time restored"),A.Time,B.Time); TestEqual(TEXT("Cargo restored"),A.Couriers.Num(),B.Couriers.Num());
    A.Tick(110); B.Tick(110);
    TestEqual(TEXT("Same next wave schedule"),A.NextWaveTime,B.NextWaveTime); TestEqual(TEXT("Same courier count"),A.Couriers.Num(),B.Couriers.Num());
    TestEqual(TEXT("Same enemy count and random sequence"),A.Enemies.Num(),B.Enemies.Num());
    if (!A.Save(TestSave(TEXT("roundtrip-a")),Error) || !B.Save(TestSave(TEXT("roundtrip-b")),Error)) { AddError(Error); return false; }
    FString SA,SB; FFileHelper::LoadFileToString(SA,*TestSave(TEXT("roundtrip-a"))); FFileHelper::LoadFileToString(SB,*TestSave(TEXT("roundtrip-b")));
    TestEqual(TEXT("Continued complete simulation states are identical"),SA,SB);
    const double Before=B.Time; FString Corrupt=SB; Corrupt.ReplaceInline(TEXT("\"save_format\": 7"),TEXT("\"save_format\": 999"));
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
    auto& Protection=S.Combat.BuildingState[S.Buildings[0].Id];Protection.Shield=Protection.Armor=0;Protection.LastDamageTime=S.Time;
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
    TSet<int32> Lost;TArray<FString> Losses;
    for(int32 I=0;I<6480&&!S.Won&&!S.Escaped;++I)
    {
        Controller.Tick(S,10);
        for(const auto& B:S.Buildings)if(B.Health<=0&&!Lost.Contains(B.Id)){Lost.Add(B.Id);Losses.Add(FString::Printf(TEXT("%s id=%d at %.0f,%.0f lost at t=%.0f wave=%d population=%d enemies=%d"),*B.DefId,B.Id,B.Position.X,B.Position.Y,S.Time,S.Wave,S.Population,S.Enemies.Num()));}
    }
    AddInfo(S.ObjectiveText()); AddInfo(S.WorkforceStatus());AddInfo(Controller.GetStatus());
    for(const FString& Loss:Losses)AddInfo(Loss);
    S.Save(TestSave(TEXT("first-playable-final")),Error);
    if(!S.Won)for(const auto& B:S.Buildings)if(B.Health>0)AddInfo(FString::Printf(TEXT("%s at %.0f,%.0f: %s; construction %.3f, health %.0f"),*B.DefId,B.Position.X,B.Position.Y,*B.Status,B.ConstructionProgress,B.Health));
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
    if(!S.Initialize(TestRules(),Error,false,false)){AddError(Error);return false;}
    TestTrue(TEXT("Core begins as a shuttle deployment"),S.Buildings[0].IsConstructing);
    TestTrue(TEXT("Landed shuttle keeps deployment sensors and point defense active"),S.IsVisible(FVector2D::ZeroVector));
    TestFalse(TEXT("Cannot order buildings during deployment"),S.PlaceBuilding(TEXT("sensor"),FVector2D(1200,0),Error));
    S.Tick(2);
    TestEqual(TEXT("Descending shuttle has no workers or materials at the worksite yet"),S.Buildings[0].ConstructionProgress,0.);
    for(int32 I=0;I<120&&S.Buildings[0].ConstructionProgress<=0;++I)S.Tick(10);
    TestTrue(TEXT("After exit and delivery, actual builders make partial deployment progress"),S.Buildings[0].ConstructionProgress>0&&S.Buildings[0].ConstructionProgress<1);
    if(!S.Save(TestSave(TEXT("deploying")),Error)||!Loaded.Initialize(TestRules(),Error)||!Loaded.Load(TestSave(TEXT("deploying")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Deployment progress survives loading"),Loaded.Buildings[0].ConstructionProgress,S.Buildings[0].ConstructionProgress);
    if(!FinishSites(S)||!FinishSites(Loaded))return false;
    TestTrue(TEXT("Deployed core provides normal visibility"),S.IsVisible(FVector2D::ZeroVector));
    // Finite stock can fund one sensor plus protected operating buffers, never two.
    for(const auto& P:S.BuildingDefs[TEXT("sensor")].Cost)S.Buildings[0].Inventory[P.Key]=P.Value+S.Buildings[0].Inventory.FindRef(P.Key)-S.ConstructionAvailable(P.Key);
    const double AlloyBefore=S.TotalStock(TEXT("alloy"));
    if(!TestTrue(TEXT("Affordable order queues a site"),S.PlaceBuilding(TEXT("sensor"),FVector2D(1200,0),Error)))return false;
    const int32 Site=S.Buildings.Last().Id;
    TestEqual(TEXT("Ordering does not consume or teleport stock"),S.TotalStock(TEXT("alloy")),AlloyBefore);
    TestEqual(TEXT("Reserved materials remain physically at source"),S.FindBuilding(Site)->ConstructionMaterials.Num(),0);
    TestFalse(TEXT("Second order cannot double-spend queued reservations"),S.PlaceBuilding(TEXT("sensor"),FVector2D(-1200,0),Error));
    S.Tick(2.1);
    TestTrue(TEXT("Construction dispatch creates tagged physical cargo"),S.Couriers.ContainsByPredicate([](const auto& C){return C.ForConstruction;}));
    TestEqual(TEXT("Shipping conserves construction material"),S.TotalStock(TEXT("alloy")),AlloyBefore);
    TestEqual(TEXT("Builders wait for physical materials"),S.FindBuilding(Site)->ConstructionProgress,0.);
    TestFalse(TEXT("Site has no operational sensor range"),S.IsVisible(FVector2D(2800,0)));
    if(!S.Save(TestSave(TEXT("construction-cargo")),Error)||!Loaded.Load(TestSave(TEXT("construction-cargo")),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Saved in-flight cargo retains construction destination"),Loaded.Couriers.ContainsByPredicate([](const auto& C){return C.ForConstruction;}));
    if(!TestTrue(TEXT("Material-supplied site completes after paid crew assembly and construction"),FinishSites(S))||!FinishSites(Loaded))return false;
    TestFalse(TEXT("Delivered materials and automatic builders finish the site"),S.FindBuilding(Site)->IsConstructing);
    TestFalse(TEXT("Completed sensor still requires a connected powered road"),S.IsVisible(FVector2D(2800,0)));
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
    for(const FVector2D P:{FVector2D(1200,0),FVector2D(-1200,0),FVector2D(0,1200)})
        if(!S.PlaceBuilding(TEXT("sensor"),P,Error)){AddError(Error);return false;}
    // Manufacture the missing crew at the current authored core replicator rate.
    // Excess demand remains, so the support ceiling is checked after assembly.
    for(int32 I=0;I<1200&&S.Population<CoreCapacity;++I)S.Tick(10);
    TestEqual(TEXT("Population growth stops at actual service capacity"),S.Population,CoreCapacity);
    if(!S.PlaceBuilding(TEXT("robot_service_bay"),FVector2D(0,-1200),Error)){AddError(Error);return false;}
    const int32 BayId=S.Buildings.Last().Id;
    TestEqual(TEXT("A service construction site grants no capacity"),S.RobotSupportCapacity,CoreCapacity);
    for(auto& Building:S.Buildings)if(Building.DefId==TEXT("sensor")&&Building.Enabled)S.ToggleBuilding(Building.Id);
    if(!FinishSites(S)){AddError(TEXT("Service expansion did not finish"));return false;}
    if(!ConnectPower(S,BayId,Error)){AddError(Error);return false;}
    for(auto& Building:S.Buildings)if(Building.DefId==TEXT("sensor")&&!Building.Enabled)S.ToggleBuilding(Building.Id);
    if(!FinishSites(S)){AddError(TEXT("Expanded supported construction did not finish"));return false;}
    for(int32 I=0;I<1200&&(S.Population<=CoreCapacity||S.FindBuilding(BayId)->SupportedRobots<=0);++I)S.Tick(10);
    TestEqual(TEXT("Completed service bay expands real capacity"),S.RobotSupportCapacity,CoreCapacity+S.BuildingDefs[TEXT("robot_service_bay")].RobotSupportCapacity);
    TestTrue(TEXT("Open jobs can now grow beyond starter capacity"),S.Population>CoreCapacity);
    TestTrue(TEXT("Additional robots are allocated to the new service bay"),S.FindBuilding(BayId)->SupportedRobots>0);
    // Keep the service population working while isolating a single maintenance interval.
    // No outside source can replace missing service supplies before that interval.
    S.FindBuilding(BayId)->Inventory.Remove(TEXT("components"));
    const double Interval=S.Number(TEXT("upkeep_interval"));
    S.Buildings[0].Inventory[TEXT("components")]=S.Buildings[0].SupportedRobots*S.Number(TEXT("upkeep_per_robot"));
    S.StepPopulation(Interval-S.UpkeepClock+S.FixedStepSeconds());
    TestTrue(TEXT("Core can maintain its crew without exporting its protected buffer"),S.Buildings[0].MaintenanceSupplied);
    TestFalse(TEXT("Service upkeep requires its own local supplies"),S.FindBuilding(BayId)->MaintenanceSupplied);
    TestTrue(TEXT("Maintenance shortage has a real workforce consequence"),S.OperatingEfficiency()<1);
    S.FindBuilding(BayId)->Inventory.Add(TEXT("components"),5);S.Buildings[0].Inventory[TEXT("components")]=5;S.StepPopulation(Interval);
    TestTrue(TEXT("Delivered local components restore service at the next interval"),S.FindBuilding(BayId)->MaintenanceSupplied);
    S.ToggleBuilding(BayId);
    TestEqual(TEXT("Disabling a bay removes its usable capacity"),S.RobotSupportCapacity,CoreCapacity);
    TestTrue(TEXT("Unsupported robots remain and operate at shortage efficiency"),S.SupportedPopulation<S.Population&&S.OperatingEfficiency()<1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeVisibleWorkGatesTest,"Seige.Simulation.VisibleWorkUsesOperatingGates",TestFlags)
bool FSeigeVisibleWorkGatesTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;if(!Deploy(S,Error)){AddError(Error);return false;}
    for(int32 I=0;I<1200&&S.HasActiveWork(S.Buildings[0]);++I)S.Tick(10);
    TestFalse(TEXT("Core becomes visually idle once paid startup manufacture finishes"),S.HasActiveWork(S.Buildings[0]));
    // Use an expanding recipe: a shrinking batch may legitimately run in a full
    // store because removing its inputs makes enough room for every output.
    const FSeigeBuildingDef* D=S.BuildingDefs.Find(TEXT("circuit_works"));
    if(!TestNotNull(TEXT("Current rules contain the circuit processor"),D))return false;
    const FString DefinitionId=D->Id;
    if(!S.PlaceBuilding(DefinitionId,FVector2D(1200,0),Error)||!FinishSites(S)){AddError(Error);return false;}
    const int Id=S.Buildings.Last().Id;
    TestFalse(TEXT("A disconnected completed workplace cannot animate production"),S.HasActiveWork(*S.FindBuilding(Id)));
    if(!ConnectPower(S,Id,Error)){AddError(Error);return false;}
    for(int32 I=0;I<120&&S.FindBuilding(Id)->Workers<D->Jobs;++I)S.Tick(10);
    TestEqual(TEXT("Actual workers arrive before checking the production animation gate"),S.FindBuilding(Id)->Workers,D->Jobs);
    auto& B=*S.FindBuilding(Id);B.Inventory.Empty();B.ProductionCommitted=false;B.ProductionInputs.Empty();B.ProductionReservedLitres=0;B.Progress=0;
    TestFalse(TEXT("No local inputs means exterior tools stay idle"),S.HasActiveWork(B));
    const auto Inputs=S.ProductionInputs(B,D->Recipe);const auto& Recipe=S.Recipes[D->Recipe];
    const double RequiredHeadroom=S.InventoryLitres(Recipe.Outputs)-S.InventoryLitres(Inputs);
    if(!TestTrue(TEXT("Circuit recipe needs additional output volume"),RequiredHeadroom>0))return false;
    B.Inventory=Inputs;
    TestTrue(TEXT("Real locally supplied powered production allows work animation"),S.HasActiveWork(B));
    TestTrue(TEXT("The supplied batch can actually commit"),S.CanCommitProduction(B,D->Recipe));
    const FString Stock=Recipe.Outputs.CreateConstIterator().Key();const double LitresPerUnit=S.Resources[Stock].LitresPerUnit;
    B.Inventory.FindOrAdd(Stock)+=S.StorageRoom(B)/LitresPerUnit;
    TestTrue(TEXT("Full-store fixture respects physical capacity"),S.StorageUsed(B)<=D->StorageCapacity+1.e-8);
    TestTrue(TEXT("No unclaimed output room remains"),S.StorageRoom(B)<1.e-8);
    TestFalse(TEXT("Full storage rejects the actual expanding batch"),S.CanCommitProduction(B,D->Recipe));
    TestFalse(TEXT("Full output storage also stops work animation"),S.HasActiveWork(B));
    B.Inventory[Stock]-=(RequiredHeadroom+1.e-6)/LitresPerUnit;
    TestTrue(TEXT("Freeing the required output headroom admits the batch"),S.CanCommitProduction(B,D->Recipe));
    TestTrue(TEXT("The same freed headroom restores work animation"),S.HasActiveWork(B));
    B.Inventory=Inputs;B.IsConstructing=true;
    TestFalse(TEXT("Construction site never shows operating workforce"),S.HasActiveWork(B));
    B.IsConstructing=false;B.Enabled=false;
    TestFalse(TEXT("Disabled production stays idle"),S.HasActiveWork(B));
    return true;
}
#endif
