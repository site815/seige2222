#include "AI/SeigeScenarioAI.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
    if(!WriteAIConfig(Invalid,TEXT("colony_ai.json"),[](auto Object){Object->SetStringField(TEXT("decision_scheduling_policy"),TEXT("unlimited_free_actions"));}))return false;
    TestFalse(TEXT("Unknown decision scheduling is rejected"),AI.Initialize(Colony,AIRules(),Invalid,false,Error));
    TestTrue(TEXT("Scheduling diagnostic identifies the editable policy"),Error.Contains(TEXT("decision_scheduling_policy")));
    if(!WriteAIConfig(Invalid,TEXT("colony_ai.json"),[](auto Object){Object->RemoveField(TEXT("decision_scheduling_policy"));}))return false;
    TestFalse(TEXT("Missing decision scheduling is rejected"),AI.Initialize(Colony,AIRules(),Invalid,false,Error));
    if(!WriteAIConfig(Invalid,TEXT("colony_ai.json"),[](auto Object){Object->GetObjectField(TEXT("guard_service"))->SetNumberField(TEXT("resume_above_fraction"),.1);}))return false;
    TestFalse(TEXT("Guard charging requires real hysteresis"),AI.Initialize(Colony,AIRules(),Invalid,false,Error));
    if (!WriteAIConfig(Invalid, TEXT("colony_ai.json"), [](auto Object) { Object->GetArrayField(TEXT("build_targets"))[0]->AsObject()->SetStringField(TEXT("definition"), TEXT("unknown_building")); })) return false;
    TestFalse(TEXT("Unknown building reference is rejected"), AI.Initialize(Colony, AIRules(), Invalid, false, Error));
    if (!WriteAIConfig(Invalid, TEXT("developed_start.json"), [](auto Object) { Object->GetObjectField(TEXT("inventory"))->SetNumberField(TEXT("alloy"), 999999); })) return false;
    TestFalse(TEXT("Over-capacity developed seed is rejected"), AI.Initialize(Colony, AIRules(), Invalid, true, Error));
    if(!WriteAIConfig(Invalid,TEXT("colony_ai.json"),[](auto Object){Object->SetNumberField(TEXT("developed_setup_seconds"),10.003);}))return false;
    TestFalse(TEXT("Insufficient developed preparation budget returns instead of spinning"),AI.Initialize(Colony,AIRules(),Invalid,true,Error));
    TestTrue(TEXT("Preparation failure identifies the unfinished target"),Error.Contains(TEXT("cannot finish")));
    TestEqual(TEXT("Failed preparation preserves the active colony"),Colony.Time,OriginalTime);
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
    TestTrue(TEXT("Established scenario contains genuinely elapsed construction time"), Developed.Time>0);
    for(const auto& B:Developed.Buildings)if(B.Health>0)TestFalse(TEXT("Developed buildings actually completed construction"),B.IsConstructing);
    TestTrue(TEXT("Developed preparation physically delivered materials"),Developed.DeliveredUnits>0);
    FString PlanText;TSharedPtr<FJsonObject> Plan;
    if(!FFileHelper::LoadFileToString(PlanText,*FPaths::Combine(AIDirectory(),TEXT("colony_ai.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PlanText),Plan))return false;
    const int32 CarriedVehicles=Starting.Combat.Vehicles.Num();
    const double Alloy = Starting.Buildings[0].Inventory.FindRef(TEXT("alloy"));
    StartAI.Tick(Starting, Starting.BuildingDefs[Starting.CoreDefinition].ConstructionSeconds+3*Plan->GetNumberField(TEXT("decision_interval_seconds"))+Starting.FixedStepSeconds());
    TestTrue(TEXT("Normal AI action queues a construction site"), Starting.Buildings.Num() > 1);
    TestTrue(TEXT("AI cannot instantly finish a new site"),Starting.Buildings.Last().IsConstructing);
    TestTrue(TEXT("AI construction consumes real core inventory"), Starting.Buildings[0].Inventory.FindRef(TEXT("alloy")) < Alloy);
    TestEqual(TEXT("AI guarding creates no extra vehicles"),Starting.Combat.Vehicles.Num(),CarriedVehicles);
    TestTrue(TEXT("AI deploys its actual carried guard fleet to defend the paid construction site"),Starting.Combat.Fleets.ContainsByPredicate([&](const auto& F){return F.Mission==TEXT("defense")&&F.Destination.Equals(Starting.BuildingAccessPoint(Starting.Buildings.Last()),.01);}));
    TestTrue(TEXT("Living AI guards are physically deployed rather than firing from inside the shuttle"),Starting.Combat.Vehicles.ContainsByPredicate([](const auto& V){return V.Health>0&&!V.Embarked&&!V.Evacuated;}));
    // An isolated tactical fork keeps the no-grant economic viability run intact.
    FSeigeSimulation ThreatProbe=Starting;FSeigeScenarioAI ThreatBrain=StartAI;
    const int32 ProtectedId=ThreatProbe.Buildings.Last().Id;
    if(!ThreatProbe.PlaceBuilding(TEXT("sensor"),ThreatProbe.Buildings[0].Position+FVector2D(0,1200),Error)){AddError(Error);return false;}
    const auto* Protected=ThreatProbe.FindBuilding(ProtectedId);FSeigeEnemy Contact;Contact.Id=1000000;Contact.Health=1000000;Contact.Position=Protected->Position+(Protected->Position-ThreatProbe.Buildings[0].Position).GetSafeNormal()*300;ThreatProbe.Enemies.Add(Contact);
    TestTrue(TEXT("Tactical probe uses a contact visible to actual colony sensors"),ThreatProbe.IsVisible(Contact.Position));
    ThreatBrain.Tick(ThreatProbe,Plan->GetNumberField(TEXT("decision_interval_seconds")));
    TestTrue(TEXT("Visible danger takes guard priority over a newer paid construction site"),ThreatProbe.Combat.Fleets.ContainsByPredicate([&](const auto& F){return F.Mission==TEXT("defense")&&F.Destination.Equals(ThreatProbe.BuildingAccessPoint(*ThreatProbe.FindBuilding(ProtectedId)),.01);}));
    const double PreparationBudget=Plan->GetNumberField(TEXT("developed_setup_seconds"));
    StartAI.Tick(Starting,FMath::Max(0.,PreparationBudget-Starting.Time));
    DevelopedAI.Tick(Developed, 60);
    IFileManager::Get().MakeDirectory(*AIOutput(TEXT("")),true);FString SnapshotError;Starting.Save(AIOutput(TEXT("starting_final.json")),SnapshotError);Developed.Save(AIOutput(TEXT("developed_final.json")),SnapshotError);
    AddInfo(TEXT("Starting AI: ") + Starting.ObjectiveText() + TEXT(" | ") + StartAI.GetStatus());
    TestTrue(TEXT("Starting AI develops the industrial chain through normal actions"), Starting.ProducedUnits.FindRef(TEXT("components")) > 0);
    TestTrue(TEXT("Starting AI reaches the authored playable objective through ordinary paid actions"),Starting.Won);
    TestFalse(TEXT("Starting AI survives its configured preparation budget"),Starting.Failed);
    for(const auto& Value:Plan->GetArrayField(TEXT("build_targets")))
    {
        const auto Target=Value->AsObject();const FString Definition=Target->GetStringField(TEXT("definition"));int32 Completed=0;
        if(!StartAI.IncludesTarget(Starting,Definition))continue;
        for(const auto& B:Starting.Buildings)if(B.Health>0&&!B.IsConstructing&&B.Enabled&&B.DefId==Definition&&B.Workers>=Starting.Definition(B)->Jobs)++Completed;
        AddInfo(FString::Printf(TEXT("Starting target census at %.0fs: %s %d/%d completed and staffed"),Starting.Time,*Definition,Completed,Target->GetIntegerField(TEXT("count"))));
    }
    TestTrue(TEXT("Starting AI uses physical deliveries"), Starting.DeliveredUnits > 0);
    TestTrue(TEXT("Developed AI actually produces goods after setup"), Developed.ProducedUnits.FindRef(TEXT("components")) > 0);
    TestTrue(TEXT("Developed AI uses physical deliveries"), Developed.DeliveredUnits > 0);
    TestTrue(TEXT("Starting AI builds actual power-grid roads"),Starting.Roads.Num()>0);
    int32 Extractors=0;for(const auto&B:Starting.Buildings)if(B.Health>0&&Starting.Definition(B)->Role==TEXT("extractor"))++Extractors;
    TestEqual(TEXT("Bootstrap exports one actual nearby source instead of assuming all deposits"),Extractors,1);
    TestTrue(TEXT("Missing raw inputs arrive through the trading economy"),Starting.TotalStock(TEXT("plastic"))>0||Starting.ProducedUnits.FindRef(TEXT("circuits"))>0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIGuardServiceTest, "Seige.AI.GuardServiceAndCompactCoverage", AIFlags)
bool FSeigeAIGuardServiceTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    const FString Config=AIOutput(TEXT("compact-coverage"));
    // A one-sensor paid plan isolates placement from long industrial preparation.
    if(!WriteAIConfig(Config,TEXT("colony_ai.json"),[](auto Object)
    {auto Target=MakeShared<FJsonObject>();Target->SetStringField(TEXT("definition"),TEXT("sensor"));Target->SetNumberField(TEXT("count"),1);Object->SetArrayField(TEXT("build_targets"),{MakeShared<FJsonValueObject>(Target)});}))return false;
    if(!Brain.Initialize(Colony,AIRules(),Config,false,Error,false,false)){AddError(Error);return false;}
    FString Raw;TSharedPtr<FJsonObject> Plan;if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(Config,TEXT("colony_ai.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Plan))return false;
    const double Cadence=Plan->GetNumberField(TEXT("decision_interval_seconds"));
    Brain.Tick(Colony,Colony.Definition(Colony.Buildings[0])->ConstructionSeconds+3*Cadence+Colony.FixedStepSeconds());
    const auto* Sensor=Colony.Buildings.FindByPredicate([](const auto& B){return B.DefId==TEXT("sensor")&&B.Health>0;});
    if(!TestNotNull(TEXT("AI pays for a sensor site"),Sensor))return false;
    const double Radius=Plan->GetObjectField(TEXT("placement"))->GetNumberField(TEXT("defense_distance"));
    TestTrue(TEXT("Perimeter search uses one core-relative radius rather than doubling an unused-deposit offset"),FMath::IsNearlyEqual(FVector2D::Distance(Sensor->Position,Colony.Buildings[0].Position),Radius,.01));
    const FVector2D Service=Colony.BuildingAccessPoint(Colony.Buildings[0]);
    const int32 FleetId=Colony.Combat.Fleets[0].Id;
    const double Recharge=Plan->GetObjectField(TEXT("guard_service"))->GetNumberField(TEXT("recharge_below_fraction"));
    for(auto& V:Colony.Combat.Vehicles)V.BatteryKWh=Colony.Combat.Chassis[V.ChassisId].BatteryKWh*Recharge*.5;
    Brain.Tick(Colony,Cadence);
    auto Servicing=[&](const FSeigeSimulation& Sim){const auto* Fleet=Sim.Combat.Fleets.FindByPredicate([&](const auto& F){return F.Id==FleetId;});return Fleet&&Fleet->Mission==TEXT("move")&&Fleet->Aggression==TEXT("passive")&&Fleet->Destination.Equals(Service,.01);};
    TestTrue(TEXT("Low batteries order physical return to the core without continuing defensive firing"),Servicing(Colony));
    const double Before=Colony.Combat.Vehicles[0].BatteryKWh;
    Brain.Tick(Colony,300);
    TestTrue(TEXT("Returned guard receives real local grid charge"),Colony.Combat.Vehicles[0].BatteryKWh>Before);
    TestTrue(TEXT("Charging order is held below the resume threshold while other work continues"),Servicing(Colony));
    TestTrue(TEXT("Service order survives strict simulation persistence"),Colony.Save(AIOutput(TEXT("guard-service.json")),Error));
    FSeigeSimulation Restored;FSeigeScenarioAI RestoredBrain;
    if(!RestoredBrain.Initialize(Restored,AIRules(),Config,false,Error,false,false)||!Restored.Load(AIOutput(TEXT("guard-service.json")),Error)){AddError(Error);return false;}
    RestoredBrain.Tick(Restored,Cadence);TestTrue(TEXT("Reloaded AI keeps charging rather than losing hysteresis"),Servicing(Restored));
    const double Resume=Plan->GetObjectField(TEXT("guard_service"))->GetNumberField(TEXT("resume_above_fraction"));
    for(auto& V:Restored.Combat.Vehicles)V.BatteryKWh=Colony.Combat.Chassis[V.ChassisId].BatteryKWh*Resume;
    RestoredBrain.Tick(Restored,Cadence);
    TestTrue(TEXT("Charged guards resume the ordinary defensive mission"),Restored.Combat.Fleets[0].Mission==TEXT("defense")&&Restored.Combat.Fleets[0].Aggression==TEXT("defensive"));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAISupportRecoveryTest, "Seige.AI.WorkerSupportRecovery", AIFlags)
bool FSeigeAISupportRecoveryTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    const FString Config=AIOutput(TEXT("support-recovery"));
    if(!WriteAIConfig(Config,TEXT("colony_ai.json"),[](auto Object)
    {TArray<TSharedPtr<FJsonValue>> Targets;for(const TCHAR* Id:{TEXT("worker_factory"),TEXT("robot_service_bay")}){auto Target=MakeShared<FJsonObject>();Target->SetStringField(TEXT("definition"),Id);Target->SetNumberField(TEXT("count"),1);Targets.Add(MakeShared<FJsonValueObject>(Target));}Object->SetArrayField(TEXT("build_targets"),Targets);}))return false;
    if(!Brain.Initialize(Colony,AIRules(),Config,false,Error,false,false)){AddError(Error);return false;}
    Colony.Tick(Colony.Definition(Colony.Buildings[0])->ConstructionSeconds+Colony.FixedStepSeconds());
    const FVector2D Home=Colony.Buildings[0].Position;
    // Real paid construction overloads the initial support allowance. The
    // ordinary ordered plan would wait forever on its first factory target.
    if(!Colony.PlaceBuilding(TEXT("worker_factory"),Home+FVector2D(1400,0),Error)||
       !Colony.PlaceBuilding(TEXT("sensor"),Home+FVector2D(0,1400),Error)||
       !Colony.PlaceBuilding(TEXT("sensor"),Home+FVector2D(0,-1400),Error)){AddError(Error);return false;}
    Colony.Tick(6000);
    const int32 FactoryId=Colony.Buildings[1].Id;
    TestTrue(TEXT("Fixture has genuine jobs exceeding service capacity"),Colony.TotalJobs>Colony.RobotSupportCapacity);
    TestTrue(TEXT("First ordered factory lacks the workers it needs before recovery"),Colony.FindBuilding(FactoryId)->IsConstructing||Colony.FindBuilding(FactoryId)->Workers<Colony.Definition(*Colony.FindBuilding(FactoryId))->Jobs);
    const double PaidBefore=Colony.DeliveredUnits;
    for(int I=0;I<1200;++I)
    {
        Brain.Tick(Colony,10);
        const auto* Factory=Colony.FindBuilding(FactoryId);
        if(!Factory->IsConstructing&&Factory->Workers==Colony.Definition(*Factory)->Jobs&&Colony.RobotSupportCapacity>=Colony.TotalJobs)break;
    }
    const auto* Service=Colony.Buildings.FindByPredicate([&](const auto& B){return B.Health>0&&Colony.Definition(B)->Role==TEXT("service");});
    TestTrue(TEXT("AI recovers by constructing its configured service bay ahead of the blocked target"),Service&&!Service->IsConstructing&&Service->Workers==Colony.Definition(*Service)->Jobs);
    TestTrue(TEXT("Recovery uses physical paid deliveries"),Colony.DeliveredUnits>PaidBefore);
    TestTrue(TEXT("Actual service capacity permits the blocked factory to finish and staff"),!Colony.FindBuilding(FactoryId)->IsConstructing&&Colony.FindBuilding(FactoryId)->Workers==Colony.Definition(*Colony.FindBuilding(FactoryId))->Jobs&&Colony.RobotSupportCapacity>=Colony.TotalJobs);
    if(!Service)return false;
    const int32 ServiceId=Service->Id;
    FSeigeSimulation Outage=Colony;FSeigeScenarioAI RecoveryBrain=Brain;
    FSeigeAIBuildTarget LaterExpansion;LaterExpansion.Definition=TEXT("robot_service_bay");LaterExpansion.Count=2;RecoveryBrain.Targets.Add(LaterExpansion);
    for(auto& Road:Outage.Roads)Outage.DamageRoad(Road.Id,Road.MaxHealth);
    Outage.Tick(Outage.FixedStepSeconds());
    TestTrue(TEXT("Disconnected living support creates an actual operating-capacity outage"),Outage.RobotSupportCapacity<Outage.TotalJobs);
    const int32 Before=Outage.Buildings.Num();bool Waiting=false;
    TestFalse(TEXT("Temporary outage does not buy a later support expansion when intact capacity is sufficient"),RecoveryBrain.RecoverWorkerSupport(Outage,Waiting));
    TestEqual(TEXT("Outage recovery preserves finite construction reserves"),Outage.Buildings.Num(),Before);
    TestFalse(TEXT("Power restoration remains available to the ordinary AI priorities"),Waiting);
    Outage.FindBuilding(ServiceId)->Health=0;Outage.OnBuildingDestroyed(ServiceId);
    TestTrue(TEXT("Destroying that same support facility still triggers ordinary paid replacement"),RecoveryBrain.RecoverWorkerSupport(Outage,Waiting));
    TestEqual(TEXT("Genuine missing capacity queues exactly one replacement"),Outage.Buildings.Num(),Before+1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAISupportTradeRecoveryTest,"Seige.AI.SupportRecoveryImportPriority",AIFlags)
bool FSeigeAISupportTradeRecoveryTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)){AddError(Error);return false;}
    Colony.Tick(Colony.Definition(Colony.Buildings[0])->ConstructionSeconds+Colony.FixedStepSeconds());
    const FVector2D Home=Colony.Buildings[0].Position;
    if(!Colony.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)||!Colony.PlaceBuilding(TEXT("conductor_works"),Home+FVector2D(0,1400),Error)){AddError(Error);return false;}
    // A decision-level recovery fixture: retain normal bills/reservations but
    // isolate an existing port/factory after its support bay has been lost.
    for(auto& B:Colony.Buildings)if(B.IsConstructing){B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Colony.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
    Colony.Tick(Colony.FixedStepSeconds());
    FString Raw;TSharedPtr<FJsonObject> Policy;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(AIRules(),TEXT("policies.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Policy))return false;
    const FString Material=Policy->GetObjectField(TEXT("policies"))->GetStringField(TEXT("repair_resource"));
    const double Buffer=Policy->GetObjectField(TEXT("policies"))->GetNumberField(TEXT("repair_buffer_units"));
    for(auto& B:Colony.Buildings){B.Inventory.FindOrAdd(Material)=Buffer;B.Inventory.Remove(TEXT("iron_ore"));}
    const auto* Recovery=Brain.SupportRecoveryTarget(Colony);TestNotNull(TEXT("Lost capacity requires the configured support prerequisite"),Recovery);if(!Recovery)return false;
    TestTrue(TEXT("Gross stock masks a genuine construction deficit"),Colony.TotalStock(Material)>=Colony.BuildingDefs[Recovery->Definition].Cost[Material]&&Colony.ConstructionAvailable(Material)<Colony.BuildingDefs[Recovery->Definition].Cost[Material]);
    const int32 PortId=Colony.Buildings[1].Id;
    Colony.Credits=Colony.TradeQuote(Material,Brain.ImportBatch,true);const double BeforeCredits=Colony.Credits;
    FSeigeSimulation Unfunded=Colony;Unfunded.Credits=0;
    TestTrue(TEXT("AI orders the missing support material before continually consumed recipe inputs"),Brain.ManageTrade(Colony));
    TestTrue(TEXT("Recovery order buys the physically usable prerequisite"),Colony.FindBuilding(PortId)->Shipment.Buy&&Colony.FindBuilding(PortId)->Shipment.Resource==Material);
    TestTrue(TEXT("The priority import spends actual credits"),Colony.Credits<BeforeCredits);
    const auto* Node=Brain.ExportNode(Unfunded);if(!Node){AddError(TEXT("Missing export fixture node"));return false;}
    Unfunded.Buildings[0].Inventory.FindOrAdd(Node->Resource)=Brain.ExportBatch*2;
    TestTrue(TEXT("An unfunded recovery earns credits through an ordinary export"),Brain.ManageTrade(Unfunded));
    TestTrue(TEXT("No free credit or material is awarded for recovery"),!Unfunded.FindBuilding(PortId)->Shipment.Buy&&Unfunded.FindBuilding(PortId)->Shipment.Resource==Node->Resource&&Unfunded.Credits==0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIGenericTradeRecoveryTest,"Seige.AI.ConstructionRecoveryImportPriority",AIFlags)
bool FSeigeAIGenericTradeRecoveryTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)){AddError(Error);return false;}
    Colony.Tick(Colony.Definition(Colony.Buildings[0])->ConstructionSeconds+Colony.FixedStepSeconds());
    const FVector2D Home=Colony.Buildings[0].Position;
    if(!Colony.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)||!Colony.PlaceBuilding(TEXT("conductor_works"),Home+FVector2D(0,1400),Error)||!Colony.PlaceBuilding(TEXT("robot_service_bay"),Home+FVector2D(0,-1400),Error)){AddError(Error);return false;}
    // Isolate the funding decision after normal support has been restored.
    // The full signed-seed regression separately exercises elapsed paid work.
    for(auto& B:Colony.Buildings)if(B.IsConstructing){B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Colony.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
    FVector2D Previous=Colony.BuildingAccessPoint(Colony.Buildings[0]);TArray<FVector2D> Route;
    if(!Colony.FindRoadRoute(Previous,Colony.BuildingAccessPoint(Colony.Buildings[3]),Route)){AddError(TEXT("No support fixture grid route"));return false;}
    for(const auto& Point:Route)
    {
        if(!Colony.PlaceRoad(Previous,Point,Error)){AddError(Error);return false;}
        auto& Road=Colony.Roads.Last();Road.IsConstructing=false;Road.ConstructionProgress=1;Road.Tier=Road.TargetTier;Road.InstalledMaterials=Colony.RoadCost(Road.A,Road.B,Road.Tier);Road.ConstructionMaterials.Empty();Road.Builders=Road.BuildersOnSite=Road.TravellingBuilders=0;Previous=Point;
    }
    // The fixture finished road geometry directly, so refresh its grid cache.
    // Then wait for the core to manufacture/activate the service worker with
    // real local materials and batch energy; the landed six all staff the core.
    Colony.Energy.Invalidate();double WorkerSeconds=0;
    for(const auto& Recipe:Colony.Recipes)if(Recipe.Value.WorkerOutput>0)WorkerSeconds=FMath::Max(WorkerSeconds,Recipe.Value.Seconds*Colony.Definition(Colony.Buildings[0])->RecipeTimeMultiplier);
    for(double Elapsed=0;Elapsed<WorkerSeconds*2+60;++Elapsed)
    {Colony.Tick(1);if(Colony.RobotSupportCapacity>=Colony.TotalJobs)break;}
    TestTrue(TEXT("Service fixture is physically connected to the command power grid"),Colony.IsRoadGridConnected(Colony.Buildings[0].Id,Colony.Buildings[3].Id));
    TestTrue(TEXT("The core produced the additional service worker instead of a staffing grant"),Colony.Population>Colony.Definition(Colony.Buildings[0])->Jobs);
    Brain.Targets.Empty();for(const TCHAR* Id:{TEXT("alloy_refinery"),TEXT("component_works")}){FSeigeAIBuildTarget Target;Target.Definition=Id;Target.Count=1;Brain.Targets.Add(Target);}
    TestTrue(TEXT("Funding fixture has adequate genuine support capacity"),Colony.RobotSupportCapacity>=Colony.TotalJobs);
    FSeigeSimulation Waiting=Colony;
    if(!Waiting.PlaceBuilding(TEXT("alloy_refinery"),Home+FVector2D(-1400,1400),Error)){AddError(Error);return false;}
    TestNull(TEXT("An already paid construction target blocks downstream funding"),Brain.NextConstructionTarget(Waiting));
    auto& Existing=Waiting.Buildings.Last();Existing.IsConstructing=false;Existing.ConstructionProgress=1;Existing.Workers=0;
    TestNull(TEXT("An earlier understaffed target also blocks downstream funding"),Brain.NextConstructionTarget(Waiting));
    FString Raw;TSharedPtr<FJsonObject> Policy;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(AIRules(),TEXT("policies.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Policy))return false;
    const FString Material=Policy->GetObjectField(TEXT("policies"))->GetStringField(TEXT("repair_resource"));const double Buffer=Policy->GetObjectField(TEXT("policies"))->GetNumberField(TEXT("repair_buffer_units"));
    for(auto& B:Colony.Buildings){B.Inventory.FindOrAdd(Material)=Buffer;B.Inventory.Remove(TEXT("iron_ore"));}
    const auto* Target=Brain.NextConstructionTarget(Colony);TestTrue(TEXT("The next missing refinery is the actual funding target"),Target&&Target->Definition==TEXT("alloy_refinery"));if(!Target)return false;
    TestTrue(TEXT("Local repair buffers cannot pay the refinery even when total stock covers it"),Colony.TotalStock(Material)>=Colony.BuildingDefs[Target->Definition].Cost[Material]&&Colony.ConstructionAvailable(Material)<Colony.BuildingDefs[Target->Definition].Cost[Material]);
    Colony.Credits=Colony.TradeQuote(Material,Brain.ImportBatch,true);
    FSeigeSimulation IndependentCycle=Colony,SharedCycle=Colony;FSeigeScenarioAI IndependentBrain=Brain,SharedBrain=Brain;
    IndependentBrain.DecisionSchedulingPolicy=TEXT("independent_tactics_trade_construction");SharedBrain.DecisionSchedulingPolicy=TEXT("shared_action_budget");
    const int32 FleetId=IndependentCycle.Combat.Fleets[0].Id;
    TestTrue(TEXT("Scheduling fixture has a carried guard awaiting its first real deployment order"),IndependentCycle.Combat.Vehicles.ContainsByPredicate([&](const auto& V){return V.FleetId==FleetId&&V.Health>0&&V.Embarked;}));
    const double DecisionSeconds=Brain.DecisionInterval-FMath::Fmod(Colony.Time,Brain.DecisionInterval)+Colony.FixedStepSeconds();
    const double PaidCredits=IndependentCycle.Credits;const int32 ExistingVehicles=IndependentCycle.Combat.Vehicles.Num();
    SharedBrain.Tick(SharedCycle,DecisionSeconds);IndependentBrain.Tick(IndependentCycle,DecisionSeconds);
    TestTrue(TEXT("The selected shared policy demonstrates guard orders occupying the sole economic action slot"),SharedCycle.Buildings[1].Shipment.Resource.IsEmpty());
    TestTrue(TEXT("Independent scheduling deploys the existing fleet on the same decision as its paid import"),IndependentCycle.Combat.Vehicles.ContainsByPredicate([&](const auto& V){return V.FleetId==FleetId&&V.Health>0&&!V.Embarked&&!V.Evacuated;}));
    TestTrue(TEXT("Guard orders do not starve a physically needed reconstruction import"),IndependentCycle.Buildings[1].Shipment.Buy&&IndependentCycle.Buildings[1].Shipment.Resource==Material&&IndependentCycle.Credits<PaidCredits);
    TestEqual(TEXT("Independent tactical scheduling adds no defensive units"),IndependentCycle.Combat.Vehicles.Num(),ExistingVehicles);
    FSeigeSimulation ReserveProbe=Colony,RoutineProbe=Colony;FSeigeScenarioAI ReserveBrain=Brain,RoutineBrain=Brain;
    ReserveBrain.Targets.Empty();ReserveBrain.ReserveTargets.Empty();ReserveBrain.ReserveTargets.Add(Material,ReserveProbe.TotalStock(Material));
    TestTrue(TEXT("Repair buffers meet the gross reserve number but provide no spare construction reserve"),ReserveProbe.TotalStock(Material)>=ReserveBrain.ReserveTargets[Material]&&ReserveProbe.ConstructionAvailable(Material)<ReserveBrain.ReserveTargets[Material]);
    TestTrue(TEXT("Idle-plan AI replenishes a genuine paid construction reserve"),ReserveBrain.ManageTrade(ReserveProbe));
    TestTrue(TEXT("Gross repair-buffer stock does not suppress the reserve import"),ReserveProbe.Buildings[1].Shipment.Buy&&ReserveProbe.Buildings[1].Shipment.Resource==Material);
    RoutineBrain.Targets.Empty();RoutineBrain.ReserveTargets.Empty();RoutineProbe.Credits=RoutineProbe.TradeQuote(TEXT("iron_ore"),RoutineBrain.ImportBatch,true);
    TestTrue(TEXT("Routine recipe inputs still use their ordinary physical shortage"),RoutineBrain.ManageTrade(RoutineProbe));
    TestTrue(TEXT("Reserve correction preserves the recipe-input import"),RoutineProbe.Buildings[1].Shipment.Buy&&RoutineProbe.Buildings[1].Shipment.Resource==TEXT("iron_ore"));
    TestTrue(TEXT("Reconstruction imports outrank ongoing factory input deficits"),Brain.ManageTrade(Colony));
    TestTrue(TEXT("Paid import buys the actual reconstruction material"),Colony.Buildings[1].Shipment.Buy&&Colony.Buildings[1].Shipment.Resource==Material);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIDevelopedNeighborRecoveryTest,"Seige.AI.DevelopedNeighborSeedRecovery",AIFlags)
bool FSeigeAIDevelopedNeighborRecoveryTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),true,Error,true,true,-2027807669)){AddError(Error);return false;}
    TestTrue(TEXT("Previously failing signed neighbor seed completes within the unchanged preparation budget"),Brain.IsReady()&&Colony.Time<=Brain.PreparationLimit()+1.e-6&&!Colony.Failed&&!Colony.Escaped);
    TestTrue(TEXT("Developed neighbor uses physical deliveries and real industry"),Colony.DeliveredUnits>0&&Colony.ProducedUnits.FindRef(TEXT("components"))>0);
    TestEqual(TEXT("Regression uses the shipping neighbor's exact generation seed"),Colony.GenerationSeed,-2027807669);
    return true;
}

// Explicit diagnostic, deliberately outside the release acceptance group.
// Continue the exact failed state without redoing its paid history or changing
// the authored setup deadline. Both arguments are required so this cannot
// silently become an empty passing test in an ordinary Seige suite run.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIDevelopedContinuationDiagnostic,"Diagnostics.DevelopedContinuation",AIFlags)
bool FSeigeAIDevelopedContinuationDiagnostic::RunTest(const FString& Parameters)
{
    FString Snapshot;double Hours=0;
    if(!FParse::Value(FCommandLine::Get(),TEXT("SeigeContinuationSnapshot="),Snapshot)||
        !FParse::Value(FCommandLine::Get(),TEXT("SeigeContinuationHours="),Hours)||!FMath::IsFinite(Hours)||Hours<=0)
    {AddError(TEXT("Diagnostic requires -SeigeContinuationSnapshot=<file> and -SeigeContinuationHours=<absolute simulation hours>"));return false;}
    const FString Config=AIOutput(TEXT("continuation-shared-policy"));
    if(!WriteAIConfig(Config,TEXT("colony_ai.json"),[](auto Object){Object->SetStringField(TEXT("decision_scheduling_policy"),TEXT("shared_action_budget"));}))return false;
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),Config,false,Error)||!Colony.Load(Snapshot,Error)){AddError(Error);return false;}
    const double StopTime=Hours*3600,StartTime=Colony.Time;
    if(StopTime<=StartTime){AddError(TEXT("Diagnostic end time must follow the loaded snapshot"));return false;}
    AddInfo(FString::Printf(TEXT("Diagnostic only: shared policy continues seed %d from %.2f to at most %.2f seconds; authored acceptance limit remains %.2f."),Colony.GenerationSeed,StartTime,StopTime,Brain.DevelopedSetupSeconds));
    Brain.ReportPreparationMilestone(Colony);
    while(Colony.Time+UE_DOUBLE_SMALL_NUMBER<StopTime&&!Colony.Failed&&!Colony.Escaped&&!Brain.DevelopmentComplete(Colony))
    {
        const double Before=Colony.Time;Brain.Tick(Colony,FMath::Min(10.,StopTime-Colony.Time));
        Brain.ReportPreparationMilestone(Colony);
        if(Colony.Time<=Before){AddError(TEXT("Continuation stopped advancing"));return false;}
    }
    const FString Output=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Diagnostics"),FString::Printf(TEXT("developed-ai-continuation-%d.json"),Colony.GenerationSeed));
    if(!Colony.Save(Output,Error)){AddError(Error);return false;}
    AddInfo(FString::Printf(TEXT("Diagnostic continuation stopped at %.2f seconds, ready=%d, failed=%d, status=%s, snapshot=%s"),Colony.Time,Brain.DevelopmentComplete(Colony)?1:0,Colony.Failed?1:0,*Brain.GetStatus(),*Output));
    TestTrue(TEXT("Diagnostic continuation reaches every completed, staffed, connected target without grants"),Brain.DevelopmentComplete(Colony));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIIncrementalPreparationTest,"Seige.AI.IncrementalPreparationDeterminism",AIFlags)
bool FSeigeAIIncrementalPreparationTest::RunTest(const FString& Parameters)
{
    const FString Config=AIOutput(TEXT("incremental-preparation"));
    if(!WriteAIConfig(Config,TEXT("colony_ai.json"),[](auto Object)
    {auto Target=MakeShared<FJsonObject>();Target->SetStringField(TEXT("definition"),TEXT("sensor"));Target->SetNumberField(TEXT("count"),1);Object->SetArrayField(TEXT("build_targets"),{MakeShared<FJsonValueObject>(Target)});Object->SetNumberField(TEXT("decision_interval_seconds"),10);}))return false;
    FString Error;FSeigeSimulation Synchronous,Incremental;FSeigeScenarioAI SyncBrain,FrameBrain;
    if(!SyncBrain.BeginInitialize(Synchronous,AIRules(),Config,true,Error,false,false)||!FrameBrain.BeginInitialize(Incremental,AIRules(),Config,true,Error,false,false)){AddError(Error);return false;}
    // Independent pre-incremental reference: one whole ten-second Tick call,
    // then completion/action-limit checks. Do not use Initialize or
    // AdvancePreparation here: both now share the incremental implementation.
    constexpr double LegacyPreparationChunkSeconds=10;
    while(Synchronous.Time+UE_DOUBLE_SMALL_NUMBER<SyncBrain.DevelopedSetupSeconds&&!Synchronous.Failed&&!Synchronous.Escaped)
    {
        const double Before=Synchronous.Time;
        SyncBrain.Tick(Synchronous,FMath::Min(LegacyPreparationChunkSeconds,SyncBrain.DevelopedSetupSeconds-Synchronous.Time));
        if(Synchronous.Time<=Before||Synchronous.Buildings.Num()>SyncBrain.DevelopedSetupLimit+1){AddError(TEXT("Legacy preparation reference did not make legal progress"));return false;}
        if(SyncBrain.DevelopmentComplete(Synchronous))break;
    }
    if(!SyncBrain.FinishPreparation(Synchronous,Error)){AddError(Error);return false;}
    TestTrue(TEXT("Beginning preparation does not synchronously simulate an established colony"),FrameBrain.IsPreparing()&&Incremental.Time==0&&!FrameBrain.IsReady());
    bool Complete=false;int32 Frames=0;
    while(!Complete&&Frames<50000)
    {if(!FrameBrain.AdvancePreparation(Incremental,Frames%2?.25:1,Complete,Error)){AddError(Error);return false;}++Frames;}
    TestTrue(TEXT("Bounded calls eventually complete the same paid development plan"),Complete&&FrameBrain.IsReady()&&Frames>1&&Incremental.DeliveredUnits>0);
    if(!Synchronous.Save(AIOutput(TEXT("preparation-sync.json")),Error)||!Incremental.Save(AIOutput(TEXT("preparation-frames.json")),Error)){AddError(Error);return false;}
    FString A,B;FFileHelper::LoadFileToString(A,*AIOutput(TEXT("preparation-sync.json")));FFileHelper::LoadFileToString(B,*AIOutput(TEXT("preparation-frames.json")));
    TestEqual(TEXT("Frame budgeting preserves the complete state of the original ten-second preparation schedule"),A,B);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIPersistenceTest, "Seige.AI.DeterministicSaveContinuation", AIFlags)
bool FSeigeAIPersistenceTest::RunTest(const FString& Parameters)
{
    FString Error; FSeigeSimulation A, B; FSeigeScenarioAI BrainA, BrainB;
    if (!BrainA.Initialize(A, AIRules(), AIDirectory(), false, Error)) { AddError(Error); return false; }
    BrainA.Tick(A, A.BuildingDefs[A.CoreDefinition].ConstructionSeconds+31.25);
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
