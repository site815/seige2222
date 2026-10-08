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
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags AIFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
FString AIRules() { return FPaths::Combine(FPaths::ProjectDir(), TEXT("Rules")); }
FString AIDirectory() { return FPaths::Combine(FPaths::ProjectDir(), TEXT("AIFILES")); }
FString AIOutput(const FString& Name) { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/ScenarioAI"), Name); }
bool RecoveryBufferRules(const FString& Name,const FString& BuildingId,int32 Containers,FString& Directory)
{
    // Preserve the regression's gross-stock-versus-reserved-stock condition
    // when authored building mass changes. Only this isolated rules copy gets
    // a larger, still real per-container repair buffer; live balance is intact.
    FString Raw;TSharedPtr<FJsonObject> Buildings,Policy;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(AIRules(),TEXT("buildings.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Buildings))return false;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(AIRules(),TEXT("policies.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Policy))return false;
    const auto Values=Policy->GetObjectField(TEXT("policies"));const FString Resource=Values->GetStringField(TEXT("repair_resource"));double Cost=0;
    for(const auto& Value:Buildings->GetArrayField(TEXT("buildings")))if(Value->AsObject()->GetStringField(TEXT("id"))==BuildingId)Cost=Value->AsObject()->GetObjectField(TEXT("cost"))->GetNumberField(Resource);
    if(Cost<=0)return false;Values->SetNumberField(TEXT("repair_buffer_units"),FMath::Max(Values->GetNumberField(TEXT("repair_buffer_units")),FMath::CeilToDouble(Cost/Containers)+1));
    Directory=AIOutput(Name+TEXT("-rules"));IFileManager::Get().MakeDirectory(*Directory,true);TArray<FString> Files;
    IFileManager::Get().FindFiles(Files,*FPaths::Combine(AIRules(),TEXT("*.json")),true,false);
    for(const auto& File:Files)if(IFileManager::Get().Copy(*FPaths::Combine(Directory,File),*FPaths::Combine(AIRules(),File))!=COPY_OK)return false;
    Raw.Empty();return FJsonSerializer::Serialize(Policy.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw))&&FFileHelper::SaveStringToFile(Raw,*FPaths::Combine(Directory,TEXT("policies.json")));
}
bool FinishCoreDeployment(FSeigeSimulation& Colony,FString& Error,FSeigeScenarioAI* Brain=nullptr)
{
    // Construction duration starts after actual hatch unloading and crew travel.
    // Exercise those paid steps instead of assuming a duration-only instant setup.
    for(int32 I=0;I<400&&Colony.Buildings[0].IsConstructing;++I)
    {if(Brain)Brain->Tick(Colony,10);else Colony.Tick(10);}
    if(Colony.Buildings[0].IsConstructing){Error=TEXT("Finite landed workers did not finish the command deployment fixture");return false;}
    return true;
}
bool InstallDecisionFixtureRoad(FSeigeSimulation& Colony,FVector2D A,FVector2D B,FString& Error)
{
    // Isolated funding tests install their existing grid explicitly. Survival
    // tests still perform every physical construction step after initialization.
    if(!Colony.PlaceRoad(A,B,Error))return false;
    auto& Road=Colony.Roads.Last();Road.IsConstructing=false;Road.ConstructionProgress=1;Road.Tier=Road.TargetTier;
    Road.InstalledMaterials=Colony.RoadCost(A,B,Road.Tier);Road.ConstructionMaterials.Empty();Road.Builders=Road.BuildersOnSite=Road.TravellingBuilders=0;
    Colony.Energy.Invalidate();Colony.Energy.Tick(Colony,0);return true;
}

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
    if(!WriteAIConfig(Invalid,TEXT("colony_ai.json"),[](auto Object){Object->GetObjectField(TEXT("economy"))->SetNumberField(TEXT("fuel_import_refill_fraction"),0);}))return false;
    TestFalse(TEXT("Zero fuel import low-water mark is rejected"),AI.Initialize(Colony,AIRules(),Invalid,false,Error));
    if(!WriteAIConfig(Invalid,TEXT("colony_ai.json"),[](auto Object){Object->GetObjectField(TEXT("economy"))->SetStringField(TEXT("core_replication_policy"),TEXT("free_replication"));}))return false;
    TestFalse(TEXT("Unknown command replication policy is rejected"),AI.Initialize(Colony,AIRules(),Invalid,false,Error));
    if(!WriteAIConfig(Invalid,TEXT("colony_ai.json"),[](auto Object){Object->GetObjectField(TEXT("economy"))->SetStringField(TEXT("export_policy"),TEXT("sell_everything"));}))return false;
    TestFalse(TEXT("Unknown surplus export policy is rejected"),AI.Initialize(Colony,AIRules(),Invalid,false,Error));
    if (!WriteAIConfig(Invalid, TEXT("colony_ai.json"), [](auto Object) { Object->GetArrayField(TEXT("build_targets"))[0]->AsObject()->SetStringField(TEXT("definition"), TEXT("unknown_building")); })) return false;
    TestFalse(TEXT("Unknown building reference is rejected"), AI.Initialize(Colony, AIRules(), Invalid, false, Error));
    const TArray<TFunction<void(TSharedPtr<FJsonObject>)>> BadManifests={
        [](auto O){O->GetArrayField(TEXT("buildings"))[0]->AsObject()->GetObjectField(TEXT("inventory"))->SetNumberField(TEXT("alloy"),999999999);},
        [](auto O){O->GetArrayField(TEXT("buildings"))[1]->AsObject()->SetStringField(TEXT("definition"),TEXT("unknown"));},
        [](auto O){O->GetArrayField(TEXT("buildings"))[1]->AsObject()->SetArrayField(TEXT("offset_meters"),{MakeShared<FJsonValueNumber>(0),MakeShared<FJsonValueNumber>(0)});},
        [](auto O){O->GetArrayField(TEXT("buildings"))[0]->AsObject()->SetNumberField(TEXT("operators"),99);},
        [](auto O){O->GetArrayField(TEXT("buildings"))[0]->AsObject()->GetObjectField(TEXT("inventory"))->SetNumberField(TEXT("stored_workers"),.5);},
        [](auto O){O->SetNumberField(TEXT("idle_workers"),0);},
        [](auto O){O->SetNumberField(TEXT("age_seconds"),0);}
    };
    for(const auto& Edit:BadManifests)
    {
        if(!WriteAIConfig(Invalid,TEXT("developed_start.json"),Edit))return false;
        TestFalse(TEXT("Malformed established state is rejected"),AI.Initialize(Colony,AIRules(),Invalid,true,Error));
        TestFalse(TEXT("Malformed established state has an actionable reason"),Error.IsEmpty());
        TestEqual(TEXT("Rejected manifest preserves active colony time"),Colony.Time,OriginalTime);
        TestEqual(TEXT("Rejected manifest preserves existing colony entities"),Colony.Buildings.Num(),1);
    }
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
    TestTrue(TEXT("Established scenario has authored post-deployment age"), Developed.Time>=60);
    for(const auto& B:Developed.Buildings)if(B.Health>0)TestFalse(TEXT("Developed buildings actually completed construction"),B.IsConstructing);
    TestEqual(TEXT("Scenario initialization does not fabricate delivery history"),Developed.DeliveredUnits,0.);
    TestTrue(TEXT("Scenario initialization does not fabricate production history"),Developed.ProducedUnits.IsEmpty());
    FString PlanText;TSharedPtr<FJsonObject> Plan;
    if(!FFileHelper::LoadFileToString(PlanText,*FPaths::Combine(AIDirectory(),TEXT("colony_ai.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(PlanText),Plan))return false;
    const int32 CarriedVehicles=Starting.Combat.Vehicles.Num();
    const double Alloy = Starting.Buildings[0].Inventory.FindRef(TEXT("alloy"));
    if(!FinishCoreDeployment(Starting,Error,&StartAI)){AddError(Error);return false;}
    StartAI.Tick(Starting,3*Plan->GetNumberField(TEXT("decision_interval_seconds"))+Starting.FixedStepSeconds());
    TestTrue(TEXT("Normal AI action queues a construction site"), Starting.Buildings.Num() > 1);
    TestTrue(TEXT("AI cannot instantly finish a new site"),Starting.Buildings.Last().IsConstructing);
    TestTrue(TEXT("AI construction consumes real core inventory"), Starting.Buildings[0].Inventory.FindRef(TEXT("alloy")) < Alloy);
    TestEqual(TEXT("AI guarding creates no extra vehicles"),Starting.Combat.Vehicles.Num(),CarriedVehicles);
    TestTrue(TEXT("AI deploys its actual carried guard fleet to defend the paid construction site"),Starting.Combat.Fleets.ContainsByPredicate([&](const auto& F){return F.Mission==TEXT("defense")&&F.Destination.Equals(Starting.BuildingAccessPoint(Starting.Buildings.Last()),.01);}));
    TestTrue(TEXT("Living AI guards are physically deployed rather than firing from inside the shuttle"),Starting.Combat.Vehicles.ContainsByPredicate([](const auto& V){return V.Health>0&&!V.Embarked&&!V.Evacuated;}));
    // An isolated tactical fork keeps the no-grant economic viability run intact.
    FSeigeSimulation ThreatProbe=Starting;FSeigeScenarioAI ThreatBrain=StartAI;
    const int32 ProtectedId=ThreatProbe.Buildings.Last().Id;
    bool NewSensor=false;
    for(int32 I=0;I<48&&!NewSensor;++I)
    {
        const double Angle=(I%16)*UE_TWO_PI/16,Radius=1200+(I/16)*300;
        const FVector2D At=ThreatProbe.Buildings[0].Position+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;
        if(ThreatProbe.CanPlaceBuilding(TEXT("sensor"),At,Error))NewSensor=ThreatProbe.PlaceBuilding(TEXT("sensor"),At,Error);
    }
    if(!NewSensor){AddError(TEXT("Tactical fixture cannot place its newer paid sensor: ")+Error);return false;}
    const auto* Protected=ThreatProbe.FindBuilding(ProtectedId);FSeigeEnemy Contact;Contact.Id=1000000;Contact.Health=1000000;Contact.Position=Protected->Position+(Protected->Position-ThreatProbe.Buildings[0].Position).GetSafeNormal()*300;ThreatProbe.Enemies.Add(Contact);
    TestTrue(TEXT("Tactical probe uses a contact visible to actual colony sensors"),ThreatProbe.IsVisible(Contact.Position));
    ThreatBrain.Tick(ThreatProbe,Plan->GetNumberField(TEXT("decision_interval_seconds")));
    TestTrue(TEXT("Visible danger takes guard priority over a newer paid construction site"),ThreatProbe.Combat.Fleets.ContainsByPredicate([&](const auto& F){return F.Mission==TEXT("defense")&&F.Destination.Equals(ThreatProbe.BuildingAccessPoint(*ThreatProbe.FindBuilding(ProtectedId)),.01);}));
    // This is bounded live operation, not a guarantee of a complete industry
    // or unlimited survival. Full-history diagnostics remain explicitly opt-in.
    const int32 StartingBodies=Starting.Workers.Bodies.Num();
    StartAI.Tick(Starting,12000);
    DevelopedAI.Tick(Developed,300);
    TestFalse(TEXT("Starting colony remains playable through bounded early growth"),Starting.Failed);
    TestTrue(TEXT("Starting AI grows through material-paid worker assembly"),Starting.Workers.Bodies.Num()>StartingBodies);
    TestTrue(TEXT("Starting AI uses real deliveries and completed grid roads"),Starting.DeliveredUnits>0&&Starting.Roads.ContainsByPredicate([](const auto& R){return !R.IsConstructing&&R.Health>0;}));
    TestTrue(TEXT("Starting AI completes actual paid installations"),Starting.Buildings.ContainsByPredicate([&](const auto& B){return B.Id!=Starting.Buildings[0].Id&&!B.IsConstructing&&B.Health>0;}));
    TestTrue(TEXT("Established industry consumes real inputs and produces after loading"),Developed.ProducedUnits.FindRef(TEXT("components"))>0);
    TestTrue(TEXT("Established operation uses physical worker deliveries"),Developed.DeliveredUnits>0);
    TestFalse(TEXT("Bounded established continuation remains playable"),Developed.Failed);
    FString SnapshotError;
    TestTrue(TEXT("Starting continuation remains serializable"),Starting.Save(AIOutput(TEXT("starting_final.json")),SnapshotError));
    TestTrue(TEXT("Established continuation remains serializable"),Developed.Save(AIOutput(TEXT("developed_final.json")),SnapshotError));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIGuardServiceTest, "Seige.AI.GuardServiceAndCompactCoverage", AIFlags)
bool FSeigeAIGuardServiceTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    const FString Config=AIOutput(TEXT("compact-coverage"));
    // A one-sensor paid plan isolates placement from long industrial preparation.
    if(!WriteAIConfig(Config,TEXT("colony_ai.json"),[](auto Object)
    {auto Target=MakeShared<FJsonObject>();Target->SetStringField(TEXT("definition"),TEXT("sensor"));Target->SetNumberField(TEXT("count"),1);Target->SetNumberField(TEXT("placement_index"),0);Object->SetArrayField(TEXT("build_targets"),{MakeShared<FJsonValueObject>(Target)});}))return false;
    if(!Brain.Initialize(Colony,AIRules(),Config,false,Error,false,false)){AddError(Error);return false;}
    FString Raw;TSharedPtr<FJsonObject> Plan;if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(Config,TEXT("colony_ai.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Plan))return false;
    const double Cadence=Plan->GetNumberField(TEXT("decision_interval_seconds"));
    if(!FinishCoreDeployment(Colony,Error,&Brain)){AddError(Error);return false;}
    Brain.Tick(Colony,3*Cadence+Colony.FixedStepSeconds());
    const auto* Sensor=Colony.Buildings.FindByPredicate([](const auto& B){return B.DefId==TEXT("sensor")&&B.Health>0;});
    if(!TestNotNull(TEXT("AI pays for a sensor site"),Sensor))return false;
    const double Radius=Plan->GetObjectField(TEXT("placement"))->GetNumberField(TEXT("defense_distance"));
    TestTrue(TEXT("Perimeter search uses one core-relative radius rather than doubling an unused-deposit offset"),FMath::IsNearlyEqual(FVector2D::Distance(Sensor->Position,Colony.Buildings[0].Position),Radius,.01));
    const FVector2D Service=Colony.BuildingAccessPoint(Colony.Buildings[0]);
    const int32 FleetId=Colony.Combat.Fleets[0].Id;
    const double Recharge=Plan->GetObjectField(TEXT("guard_service"))->GetNumberField(TEXT("recharge_below_fraction"));
    for(auto& V:Colony.Combat.Vehicles)V.BatteryKWh=Colony.Combat.Chassis[V.ChassisId].BatteryKWh*Recharge*.5;
    Brain.Tick(Colony,Cadence);
    auto Servicing=[&](const FSeigeSimulation& Sim){const auto* Fleet=Sim.Combat.Fleets.FindByPredicate([&](const auto& F){return F.Id==FleetId;});return Fleet&&Fleet->Mission==TEXT("move")&&Fleet->Aggression==TEXT("defensive")&&Fleet->Destination.Equals(Service,.01);};
    TestTrue(TEXT("Low batteries order a fixed physical return with defensive fire enabled"),Servicing(Colony));
    const double Before=Colony.Combat.Vehicles[0].BatteryKWh;
    Brain.Tick(Colony,300);
    TestTrue(TEXT("Returned guard receives real local grid charge"),Colony.Combat.Vehicles[0].BatteryKWh>Before);
    TestTrue(TEXT("Charging order is held below the resume threshold while other work continues"),Servicing(Colony));
    // A durable isolated target lets both fixed core guns and charging guards
    // fire in this single tick; remove it before testing ordinary persistence.
    Colony.Enemies.Add({100099,Colony.Combat.Vehicles[0].Position+FVector2D(400,0),10000});
    Colony.Combat.ShotEvents.Empty();
    for(auto& V:Colony.Combat.Vehicles)for(auto& Cooldown:V.Cooldowns)Cooldown=0;
    Colony.Combat.Tick(Colony,.05);
    TestTrue(TEXT("Charging guards still pay for a clear in-range defensive shot"),Colony.Combat.ShotEvents.ContainsByPredicate([](const auto& Shot){return Shot.OwnerKind==TEXT("vehicle");}));
    TestTrue(TEXT("A visible enemy does not replace the fixed charging destination with pursuit"),Servicing(Colony));
    Colony.Enemies.Empty();
    TestTrue(TEXT("Service order survives strict simulation persistence"),Colony.Save(AIOutput(TEXT("guard-service.json")),Error));
    FSeigeSimulation Restored;FSeigeScenarioAI RestoredBrain;
    if(!RestoredBrain.Initialize(Restored,AIRules(),Config,false,Error,false,false)||!Restored.Load(AIOutput(TEXT("guard-service.json")),Error)){AddError(Error);return false;}
    RestoredBrain.Tick(Restored,Cadence);TestTrue(TEXT("Reloaded AI keeps charging rather than losing hysteresis"),Servicing(Restored));
    FSeigeSimulation Emergency=Restored;FSeigeScenarioAI EmergencyBrain=RestoredBrain;
    for(auto& V:Emergency.Combat.Vehicles)V.BatteryKWh=Emergency.Combat.Chassis[V.ChassisId].BatteryKWh*(Recharge+.05);
    Emergency.Enemies.Add({100199,FVector2D(Emergency.WorldHalfSize-100,Emergency.WorldHalfSize-100),10000});
    if(!TestFalse(TEXT("Threat fixture outside live coverage is actually hidden"),Emergency.IsVisible(Emergency.Enemies[0].Position)))return false;
    TestFalse(TEXT("An unseen contact cannot interrupt charging"),EmergencyBrain.GuardWorksite(Emergency));
    TestTrue(TEXT("Partial batteries keep the quiet charging order"),Servicing(Emergency));
    const auto* EmergencySensor=Emergency.Buildings.FindByPredicate([](const auto& B){return B.DefId==TEXT("sensor")&&B.Health>0;});
    if(!EmergencySensor)return false;
    const FVector2D DefenseStation=Emergency.BuildingAccessPoint(*EmergencySensor);
    const FVector2D Outward=(EmergencySensor->Position-Emergency.Buildings[0].Position).GetSafeNormal();
    Emergency.Enemies[0].Position=EmergencySensor->Position+Outward*(Emergency.Definition(*EmergencySensor)->Footprint+200);
    if(!TestTrue(TEXT("The attack on the actual sensor site is visible"),Emergency.IsVisible(Emergency.Enemies[0].Position)))return false;
    Emergency.Combat.Vehicles.Last().BatteryKWh=Emergency.Combat.Chassis[Emergency.Combat.Vehicles.Last().ChassisId].BatteryKWh*Recharge;
    TestFalse(TEXT("One guard at the return threshold keeps the whole fleet charging"),EmergencyBrain.GuardWorksite(Emergency));
    Emergency.Combat.Vehicles.Last().BatteryKWh=Emergency.Combat.Chassis[Emergency.Combat.Vehicles.Last().ChassisId].BatteryKWh*(Recharge+.05);
    const double EmergencyEnergy=Emergency.Combat.Vehicles[0].BatteryKWh+Emergency.Combat.Vehicles.Last().BatteryKWh;
    TestTrue(TEXT("Visible danger interrupts charging once every guard has its return reserve"),EmergencyBrain.GuardWorksite(Emergency));
    const auto& EmergencyFleet=Emergency.Combat.Fleets[0];
    TestTrue(TEXT("The response is defensive station protection at the threatened building"),EmergencyFleet.Mission==TEXT("defense")&&EmergencyFleet.Aggression==TEXT("defensive")&&EmergencyFleet.Destination.Equals(DefenseStation,.01));
    TestEqual(TEXT("Changing a charging order does not grant battery energy"),Emergency.Combat.Vehicles[0].BatteryKWh+Emergency.Combat.Vehicles.Last().BatteryKWh,EmergencyEnergy);
    bool VehicleFired=false;
    for(int32 I=0;I<1200&&!VehicleFired;++I)
    {Emergency.Tick(.05);VehicleFired=Emergency.Combat.ShotEvents.ContainsByPredicate([](const auto& Shot){return Shot.OwnerKind==TEXT("vehicle");});}
    TestTrue(TEXT("Interrupted guards physically obtain an ordinary paid defensive shot"),VehicleFired);
    Emergency.Enemies.Empty();Emergency.Combat.Tick(Emergency,.05);
    if(!Emergency.Save(AIOutput(TEXT("guard-emergency.json")),Error)){AddError(Error);return false;}
    FSeigeSimulation ReloadedEmergency;FSeigeScenarioAI ReloadedEmergencyBrain;
    if(!ReloadedEmergencyBrain.Initialize(ReloadedEmergency,AIRules(),Config,false,Error,false,false)||!ReloadedEmergency.Load(AIOutput(TEXT("guard-emergency.json")),Error)){AddError(Error);return false;}
    TestTrue(TEXT("The interrupted service order survives strict save/load as normal defense"),ReloadedEmergency.Combat.Fleets[0].Mission==TEXT("defense")&&ReloadedEmergency.Combat.Fleets[0].Destination.Equals(DefenseStation,.01));
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
    {TArray<TSharedPtr<FJsonValue>> Targets;for(const TCHAR* Id:{TEXT("worker_factory"),TEXT("robot_service_bay")}){auto Target=MakeShared<FJsonObject>();Target->SetStringField(TEXT("definition"),Id);Target->SetNumberField(TEXT("count"),1);Target->SetNumberField(TEXT("placement_index"),Targets.Num());Targets.Add(MakeShared<FJsonValueObject>(Target));}Object->SetArrayField(TEXT("build_targets"),Targets);}))return false;
    if(!Brain.Initialize(Colony,AIRules(),Config,false,Error,false,false)){AddError(Error);return false;}
    if(!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    const FVector2D Home=Colony.Buildings[0].Position;
    // Real paid construction overloads the initial support allowance. The
    // ordinary ordered plan would wait forever on its first factory target.
    if(!Colony.PlaceBuilding(TEXT("worker_factory"),Home+FVector2D(1400,0),Error)||
       !Colony.PlaceBuilding(TEXT("sensor"),Home+FVector2D(0,1400),Error)||
       !Colony.PlaceBuilding(TEXT("sensor"),Home+FVector2D(0,-1400),Error)){AddError(Error);return false;}
    Colony.Tick(6000);
    const int32 FactoryId=Colony.Buildings[1].Id;
    TestTrue(TEXT("Fixture has genuine jobs exceeding service capacity"),Colony.TotalJobs>Colony.RobotSupportCapacity);
    TestTrue(TEXT("Finite workers cannot fill every demanded job within starter support"),Colony.Population<=Colony.RobotSupportCapacity&&Colony.Employed<Colony.TotalJobs);
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
    if(Colony.FindBuilding(FactoryId)->IsConstructing||Colony.FindBuilding(FactoryId)->Workers<Colony.Definition(*Colony.FindBuilding(FactoryId))->Jobs||Colony.RobotSupportCapacity<Colony.TotalJobs)
    {
        const auto* Factory=Colony.FindBuilding(FactoryId);const auto& Core=Colony.Buildings[0];FString Diagnostic;
        Colony.Save(AIOutput(TEXT("support-recovery-incomplete.json")),Diagnostic);
        AddInfo(FString::Printf(TEXT("Recovery endpoint: time=%.0f factoryprogress=%.4f operators=%d/%d builders=%d/%d active=%d jobs=%d support=%d core recipe=%s committed=%s progress=%.4f status=%s"),Colony.Time,Factory->ConstructionProgress,Factory->Workers,Colony.Definition(*Factory)->Jobs,Factory->BuildersOnSite,Factory->Builders,Colony.Population,Colony.TotalJobs,Colony.RobotSupportCapacity,*Core.SelectedRecipe,*Core.CommittedRecipe,Core.Progress,*Brain.GetStatus()));
        AddInfo(FString::Printf(TEXT("Recovery grid: generated=%.3fkW demand=%.3fkW stored=%.4fkWh"),Colony.Energy.Info(Colony).GenerationKW,Colony.Energy.Info(Colony).DemandKW,Colony.Energy.Info(Colony).StoredKWh));
    }
    TestTrue(TEXT("Actual service capacity permits the blocked factory to finish and staff"),!Colony.FindBuilding(FactoryId)->IsConstructing&&Colony.FindBuilding(FactoryId)->Workers==Colony.Definition(*Colony.FindBuilding(FactoryId))->Jobs&&Colony.RobotSupportCapacity>=Colony.TotalJobs);
    if(!Service)return false;
    const int32 ServiceId=Service->Id;
    FSeigeSimulation Outage=Colony;FSeigeScenarioAI RecoveryBrain=Brain;
    FSeigeAIBuildTarget LaterExpansion;LaterExpansion.Definition=TEXT("robot_service_bay");LaterExpansion.Count=2;LaterExpansion.PlacementIndex=RecoveryBrain.Targets.Num();RecoveryBrain.Targets.Add(LaterExpansion);
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
    FString FixtureRules;if(!RecoveryBufferRules(TEXT("support-trade"),TEXT("robot_service_bay"),3,FixtureRules)){AddError(TEXT("Cannot prepare isolated repair-buffer rules"));return false;}
    if(!Brain.Initialize(Colony,FixtureRules,AIDirectory(),false,Error,false,false)){AddError(Error);return false;}
    if(!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    const FVector2D Home=Colony.Buildings[0].Position;
    if(!Colony.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)||!Colony.PlaceBuilding(TEXT("conductor_works"),Home+FVector2D(0,1400),Error)){AddError(Error);return false;}
    // A decision-level recovery fixture: retain normal bills/reservations but
    // isolate an existing port/factory after its support bay has been lost.
    for(auto& B:Colony.Buildings)if(B.IsConstructing){B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Colony.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
    for(int32 I=0;I<64;++I)
    {FVector2D A,B;FString Name;if(!Brain.NextPowerRoad(Colony,A,B,Name,Error))break;if(!InstallDecisionFixtureRoad(Colony,A,B,Error)){AddError(Error);return false;}}
    for(const auto& Building:Colony.Buildings)TestTrue(TEXT("Support-import fixture has no unpaid grid prerequisite"),Colony.IsRoadGridConnected(Colony.Buildings[0].Id,Building.Id));
    Colony.Tick(Colony.FixedStepSeconds());
    FString Raw;TSharedPtr<FJsonObject> Policy;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(FixtureRules,TEXT("policies.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Policy))return false;
    const FString Material=Policy->GetObjectField(TEXT("policies"))->GetStringField(TEXT("repair_resource"));
    const double Buffer=Policy->GetObjectField(TEXT("policies"))->GetNumberField(TEXT("repair_buffer_units"));
    for(auto& B:Colony.Buildings){B.Inventory.FindOrAdd(Material)=Buffer;B.Inventory.Remove(TEXT("iron_ore"));}
    const auto* Recovery=Brain.SupportRecoveryTarget(Colony);TestNotNull(TEXT("Lost capacity requires the configured support prerequisite"),Recovery);if(!Recovery)return false;
    TestTrue(TEXT("Gross stock masks a genuine construction deficit"),Colony.TotalStock(Material)>=Colony.BuildingDefs[Recovery->Definition].Cost[Material]&&Colony.ConstructionAvailable(Material)<Colony.BuildingDefs[Recovery->Definition].Cost[Material]);
    const int32 PortId=Colony.Buildings[1].Id;
    Colony.Credits=Colony.TradeQuote(Material,Brain.ImportBatch,true);const double BeforeCredits=Colony.Credits;
    FSeigeSimulation Unfunded=Colony;Unfunded.Credits=0;
    FSeigeSimulation CapacityImport=Colony;FSeigeScenarioAI CapacityBrain=Brain;
    const double Capacity=CapacityImport.Trade.Definition(CapacityImport.FindBuilding(PortId)->DefId)->CapacityKg;
    const double RecoveryCost=Colony.BuildingDefs[Recovery->Definition].Cost[Material];
    CapacityBrain.ImportBatch=RecoveryCost*2;CapacityImport.Resources[Material].UnitMassKg=Capacity/(RecoveryCost*.5);
    CapacityImport.Credits=CapacityImport.TradeQuote(Material,CapacityBrain.ImportBatch,true);const double CapacityCredits=CapacityImport.Credits;
    TestTrue(TEXT("Oversized configured import is capped to a legal real shipment"),CapacityBrain.ManageTrade(CapacityImport));
    const auto& ImportOrder=CapacityImport.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("Capped import fills the port mass allowance without exceeding it"),ImportOrder.Buy&&FMath::IsNearlyEqual(ImportOrder.Quantity*CapacityImport.Resources[ImportOrder.Resource].UnitMassKg,Capacity,1.e-8));
    TestTrue(TEXT("Capacity-sized import still pays its complete quote"),FMath::IsNearlyEqual(CapacityImport.Credits+ImportOrder.PriceCredits,CapacityCredits,1.e-8));
    FSeigeSimulation IncomingImport=Colony;
    auto* Carrier=IncomingImport.Workers.Bodies.FindByPredicate([](const auto& W){return W.State==TEXT("active")&&W.DeliveryId==0;});
    if(!Carrier){AddError(TEXT("Import-space fixture needs an existing worker"));return false;}
    Carrier->Activity=TEXT("idle");Carrier->BuildingId=Carrier->RoadId=0;Carrier->Route.Empty();Carrier->NextWaypoint=0;
    if(!IncomingImport.Workers.Dispatch(IncomingImport,IncomingImport.Buildings[0].Id,PortId,0,TEXT("carbon"),1,false)){AddError(TEXT("Real inbound cargo must claim the fixture's port space"));return false;}
    auto& SpaceDefinition=IncomingImport.BuildingDefs[IncomingImport.FindBuilding(PortId)->DefId];
    const double ExpectedPartial=FMath::Min(10.,RecoveryCost*.1),CommittedLitres=SpaceDefinition.StorageCapacity-IncomingImport.StorageRoom(*IncomingImport.FindBuilding(PortId));
    SpaceDefinition.StorageCapacity=CommittedLitres+ExpectedPartial*IncomingImport.Resources[Material].LitresPerUnit;
    const double SpaceCredits=IncomingImport.Credits;
    TestTrue(TEXT("AI sizes a paid partial import around real inbound cargo commitments"),Brain.ManageTrade(IncomingImport));
    const auto& PartialOrder=IncomingImport.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("The valid partial batch uses only genuinely unreserved port volume"),PartialOrder.Buy&&PartialOrder.Resource==Material&&FMath::IsNearlyEqual(PartialOrder.Quantity,ExpectedPartial,1.e-8));
    TestTrue(TEXT("Space-limited import pays its actual price"),FMath::IsNearlyEqual(SpaceCredits-IncomingImport.Credits,IncomingImport.TradeQuote(Material,ExpectedPartial,true),1.e-8));
    TestTrue(TEXT("AI orders the missing support material before continually consumed recipe inputs"),Brain.ManageTrade(Colony));
    TestTrue(TEXT("Recovery order buys the physically usable prerequisite"),Colony.FindBuilding(PortId)->Shipment.Buy&&Colony.FindBuilding(PortId)->Shipment.Resource==Material);
    TestTrue(TEXT("The priority import spends actual credits"),Colony.Credits<BeforeCredits);
    const auto* Node=Brain.ExportNode(Unfunded);if(!Node){AddError(TEXT("Missing export fixture node"));return false;}
    Unfunded.Buildings[0].Inventory.FindOrAdd(Node->Resource)=Brain.ExportBatch*2;
    FSeigeSimulation CapacityExport=Unfunded;CapacityBrain.SurplusExportPolicy=TEXT("local_raw_only");CapacityBrain.ExportBatch=1000;CapacityExport.Resources[Node->Resource].UnitMassKg=2;
    TestTrue(TEXT("Export capacity uses cargo mass rather than assuming every unit is one kilogram"),CapacityBrain.ManageTrade(CapacityExport));
    const auto& ExportOrder=CapacityExport.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("Two-kilogram cargo reserves exactly half as many export units"),!ExportOrder.Buy&&FMath::IsNearlyEqual(ExportOrder.Quantity,Capacity/2,1.e-8));
    TestEqual(TEXT("Export ordering preserves goods until actual loading and departure"),CapacityExport.TotalStock(Node->Resource),Unfunded.TotalStock(Node->Resource));
    Brain.SurplusExportPolicy=TEXT("local_raw_only");
    TestTrue(TEXT("An unfunded recovery earns credits through an ordinary export"),Brain.ManageTrade(Unfunded));
    TestTrue(TEXT("No free credit or material is awarded for recovery"),!Unfunded.FindBuilding(PortId)->Shipment.Buy&&Unfunded.FindBuilding(PortId)->Shipment.Resource==Node->Resource&&Unfunded.Credits==0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIGenericTradeRecoveryTest,"Seige.AI.ConstructionRecoveryImportPriority",AIFlags)
bool FSeigeAIGenericTradeRecoveryTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    FString FixtureRules;if(!RecoveryBufferRules(TEXT("construction-trade"),TEXT("alloy_refinery"),4,FixtureRules)){AddError(TEXT("Cannot prepare isolated repair-buffer rules"));return false;}
    if(!Brain.Initialize(Colony,FixtureRules,AIDirectory(),false,Error,false,false)){AddError(Error);return false;}
    if(!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
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
    // Then let a real body walk to the service job. The finite scheduler may
    // borrow a spare core operator; an extra manufactured body is not required.
    Colony.Energy.Invalidate();double WorkerSeconds=0;
    for(const auto& Recipe:Colony.Recipes)if(Recipe.Value.WorkerOutput>0)WorkerSeconds=FMath::Max(WorkerSeconds,Recipe.Value.Seconds*Colony.Definition(Colony.Buildings[0])->RecipeTimeMultiplier);
    for(double Elapsed=0;Elapsed<WorkerSeconds*2+60;++Elapsed)
    {Colony.Tick(1);if(Colony.RobotSupportCapacity>=Colony.TotalJobs)break;}
    TestTrue(TEXT("Service fixture is physically connected to the command power grid"),Colony.IsRoadGridConnected(Colony.Buildings[0].Id,Colony.Buildings[3].Id));
    TestTrue(TEXT("Service staffing belongs to a real on-site worker identity"),Colony.Workers.Bodies.ContainsByPredicate([&](const auto& Body){return Body.State==TEXT("active")&&Body.Activity==TEXT("operate")&&Body.BuildingId==Colony.Buildings[3].Id&&Body.Outdoor;}));
    for(int32 I=0;I<64;++I)
    {FVector2D A,B;FString Name;if(!Brain.NextPowerRoad(Colony,A,B,Name,Error))break;if(!InstallDecisionFixtureRoad(Colony,A,B,Error)){AddError(Error);return false;}}
    for(const auto& Building:Colony.Buildings)TestTrue(TEXT("Refinery-import fixture has no unpaid grid prerequisite"),Colony.IsRoadGridConnected(Colony.Buildings[0].Id,Building.Id));
    Brain.Targets.Empty();for(const TCHAR* Id:{TEXT("alloy_refinery"),TEXT("component_works")}){FSeigeAIBuildTarget Target;Target.Definition=Id;Target.Count=1;Target.PlacementIndex=Brain.Targets.Num();Brain.Targets.Add(Target);}
    // Isolate the installed specialist from command replication. The conductor
    // recipe also produces alloys, so its paid ore bill can recover this site.
    // A separate producer-disabled fork below tests finished-material imports.
    Brain.CoreReplicationRecipes.Empty();
    TestTrue(TEXT("Funding fixture has adequate genuine support capacity"),Colony.RobotSupportCapacity>=Colony.TotalJobs);
    FSeigeSimulation Waiting=Colony;
    if(!Waiting.PlaceBuilding(TEXT("alloy_refinery"),Home+FVector2D(-1400,1400),Error)){AddError(Error);return false;}
    TestNull(TEXT("An already paid construction target blocks downstream funding"),Brain.NextConstructionTarget(Waiting));
    auto& Existing=Waiting.Buildings.Last();Existing.IsConstructing=false;Existing.ConstructionProgress=1;Existing.Workers=0;
    TestNull(TEXT("An earlier understaffed target also blocks downstream funding"),Brain.NextConstructionTarget(Waiting));
    FString Raw;TSharedPtr<FJsonObject> Policy;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(FixtureRules,TEXT("policies.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Policy))return false;
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
    TestTrue(TEXT("Guard orders do not starve paid inputs for an installed reconstruction supplier"),IndependentCycle.Buildings[1].Shipment.Buy&&IndependentCycle.Buildings[1].Shipment.Resource==TEXT("iron_ore")&&IndependentCycle.Credits<PaidCredits);
    TestTrue(TEXT("The specialist can produce the actual missing construction material"),Colony.Recipes[Colony.Definition(Colony.Buildings[2])->Recipe].Outputs.FindRef(Material)>0);
    TestTrue(TEXT("Upstream funding pays the full real quote without granting reconstruction goods"),FMath::IsNearlyEqual(PaidCredits-IndependentCycle.Credits,IndependentCycle.Buildings[1].Shipment.PriceCredits,1.e-8)&&IndependentCycle.Buildings[1].Inventory.FindRef(TEXT("iron_ore"))==0);
    TestEqual(TEXT("Independent tactical scheduling adds no defensive units"),IndependentCycle.Combat.Vehicles.Num(),ExistingVehicles);
    FSeigeSimulation ReserveProbe=Colony,RoutineProbe=Colony;FSeigeScenarioAI ReserveBrain=Brain,RoutineBrain=Brain;
    ReserveBrain.Targets.Empty();ReserveBrain.ReserveTargets.Empty();ReserveBrain.ReserveTargets.Add(Material,ReserveProbe.TotalStock(Material));
    TestTrue(TEXT("Repair buffers meet the gross reserve number but provide no spare construction reserve"),ReserveProbe.TotalStock(Material)>=ReserveBrain.ReserveTargets[Material]&&ReserveProbe.ConstructionAvailable(Material)<ReserveBrain.ReserveTargets[Material]);
    TestTrue(TEXT("Idle-plan AI replenishes a genuine paid construction reserve"),ReserveBrain.ManageTrade(ReserveProbe));
    TestTrue(TEXT("Gross repair-buffer stock does not suppress the reserve import"),ReserveProbe.Buildings[1].Shipment.Buy&&ReserveProbe.Buildings[1].Shipment.Resource==Material);
    RoutineBrain.Targets.Empty();RoutineBrain.ReserveTargets.Empty();RoutineProbe.Credits=RoutineProbe.TradeQuote(TEXT("iron_ore"),RoutineBrain.ImportBatch,true);
    TestTrue(TEXT("Routine recipe inputs still use their ordinary physical shortage"),RoutineBrain.ManageTrade(RoutineProbe));
    TestTrue(TEXT("Reserve correction preserves the recipe-input import"),RoutineProbe.Buildings[1].Shipment.Buy&&RoutineProbe.Buildings[1].Shipment.Resource==TEXT("iron_ore"));
    FSeigeSimulation NoProducer=Colony;NoProducer.Buildings[2].Enabled=false;
    const double DirectCredits=NoProducer.Credits,DirectStock=NoProducer.TotalStock(Material);
    TestTrue(TEXT("Without an enabled supplier the missing reconstruction material is imported"),Brain.ManageTrade(NoProducer));
    TestTrue(TEXT("The direct recovery path still buys stock that repair buffers cannot provide"),NoProducer.Buildings[1].Shipment.Buy&&NoProducer.Buildings[1].Shipment.Resource==Material);
    TestTrue(TEXT("Direct recovery pays credits and preserves physical stock until arrival"),NoProducer.Credits<DirectCredits&&NoProducer.TotalStock(Material)==DirectStock);
    TestTrue(TEXT("An enabled coproduct supplier is funded before buying its finished output"),Brain.ManageTrade(Colony));
    TestEqual(TEXT("The actual supplier input is selected"),Colony.Buildings[1].Shipment.Resource,FString(TEXT("iron_ore")));
    const auto& Supplier=Colony.Recipes[Colony.Definition(Colony.Buildings[2])->Recipe];
    const double UsefulOre=FMath::CeilToDouble(FMath::Max(0.,Colony.BuildingDefs[Target->Definition].Cost[Material]-Colony.ConstructionAvailable(Material))/Supplier.Outputs[Material])*Supplier.Inputs.FindRef(TEXT("iron_ore"));
    const double ExpectedOre=FMath::Min3(UsefulOre,Brain.ImportBatch,Colony.Trade.Definition(Colony.Buildings[1].DefId)->CapacityKg/Colony.Resources[TEXT("iron_ore")].UnitMassKg);
    TestTrue(TEXT("The supplier receives a useful port-sized batch capped by the reconstruction bill"),Colony.Buildings[1].Shipment.Buy&&FMath::IsNearlyEqual(Colony.Buildings[1].Shipment.Quantity,ExpectedOre,1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIBulkFeedstockTest,"Seige.AI.UsefulBulkFeedstockImports",AIFlags)
bool FSeigeAIBulkFeedstockTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Base;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Base,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Base,Error)){AddError(Error);return false;}
    const FVector2D Home=Base.Buildings[0].Position;
    if(!Base.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)||!Base.PlaceBuilding(TEXT("alloy_refinery"),Home+FVector2D(0,-1400),Error)){AddError(Error);return false;}
    for(auto& B:Base.Buildings)if(B.IsConstructing){B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Base.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
    for(int32 I=0;I<64;++I){FVector2D A,B;FString Name;if(!Brain.NextPowerRoad(Base,A,B,Name,Error))break;if(!InstallDecisionFixtureRoad(Base,A,B,Error)){AddError(Error);return false;}}
    const int32 PortId=Base.Buildings[1].Id;
    for(const auto& B:Base.Buildings)if(!TestTrue(TEXT("Bulk-import fixture has real paid grid connections"),Base.IsRoadGridConnected(Base.Buildings[0].Id,B.Id)))return false;
    for(auto& B:Base.Buildings)
    {
        for(const auto& Input:B.ProductionInputs)B.Inventory.FindOrAdd(Input.Key)+=Input.Value;
        B.ProductionInputs.Empty();B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.ProductionReservedLitres=0;B.Progress=0;
        B.Inventory.Remove(TEXT("iron_ore"));B.Inventory.Remove(TEXT("alloy"));
    }
    if(!Base.SetProductionRecipe(Base.Buildings[0].Id,TEXT("smelt_alloy"),Error)){AddError(Error);return false;}
    Brain.Targets.Empty();Brain.ReserveTargets.Empty();FSeigeAIBuildTarget Target;Target.Definition=TEXT("substrate_works");Target.Count=1;Brain.Targets.Add(Target);
    const FString Ore=TEXT("iron_ore");const auto& Recipe=Base.Recipes[TEXT("smelt_alloy")];
    const double Capacity=Base.Trade.Definition(Base.FindBuilding(PortId)->DefId)->CapacityKg/Base.Resources[Ore].UnitMassKg;
    const double Bill=Base.BuildingDefs[Target.Definition].Cost.FindRef(TEXT("alloy"));
    const double Useful=FMath::CeilToDouble(Bill/Recipe.Outputs.FindRef(TEXT("alloy")))*Recipe.Inputs[Ore];
    const double Full=FMath::Min3(Useful,Capacity,Brain.ImportBatch);
    Base.Credits=Base.TradeQuote(Ore,Full,true);const int32 Bodies=Base.Workers.Bodies.Num();
    for(int32 Case=0;Case<3;++Case)
    {
        FSeigeSimulation S=Base;double Expected=Full;
        if(Case==1){Expected=Full*.25;S.Credits=S.TradeQuote(Ore,Expected,true);}
        if(Case==2){Expected=Recipe.Inputs[Ore]*3;S.BuildingDefs[S.FindBuilding(PortId)->DefId].StorageCapacity=S.StorageUsed(*S.FindBuilding(PortId))+Expected*S.Resources[Ore].LitresPerUnit;}
        const double Credits=S.Credits;
        TestTrue(TEXT("Useful feedstock admits a paid shipment under mass, credit and volume limits"),Brain.ManageTrade(S));
        const auto& Order=S.FindBuilding(PortId)->Shipment;
        TestTrue(TEXT("Actual purchased amount matches the limiting physical or financial bound"),Order.Buy&&Order.Resource==Ore&&FMath::IsNearlyEqual(Order.Quantity,Expected,1.e-8));
        TestTrue(TEXT("Every admitted unit reserves its quote without immediate goods or bodies"),FMath::IsNearlyEqual(Credits-S.Credits,S.TradeQuote(Ore,Expected,true),1.e-8)&&S.TotalStock(Ore)==0&&S.Workers.Bodies.Num()==Bodies);
    }
    // Small editable bills and a material without an installed producer must
    // not inherit an unrelated one-tonne replenishment target.
    FSeigeSimulation Tail=Base;Tail.BuildingDefs[Target.Definition].Cost={{TEXT("alloy"),Recipe.Outputs.FindRef(TEXT("alloy"))}};
    TestTrue(TEXT("A final one-batch output bill can still be funded"),Brain.ManageTrade(Tail));
    TestTrue(TEXT("Coproduct output is not counted twice across core and refinery"),Tail.FindBuilding(PortId)->Shipment.Resource==Ore&&FMath::IsNearlyEqual(Tail.FindBuilding(PortId)->Shipment.Quantity,Recipe.Inputs[Ore],1.e-8));
    FSeigeSimulation Unsupported=Base;Unsupported.BuildingDefs[Target.Definition].Cost={{TEXT("crystalline"),2}};Unsupported.Credits=Unsupported.TradeQuote(TEXT("crystalline"),2,true);
    TestTrue(TEXT("A legal material without a producing recipe retains its ordinary direct bill"),Brain.ManageTrade(Unsupported));
    TestTrue(TEXT("Unrelated raw material is neither overbought nor treated as ore feedstock"),Unsupported.FindBuilding(PortId)->Shipment.Resource==TEXT("crystalline")&&FMath::IsNearlyEqual(Unsupported.FindBuilding(PortId)->Shipment.Quantity,2.,1.e-8));
    FSeigeSimulation Refill=Base;FSeigeScenarioAI RefillBrain=Brain;RefillBrain.Targets.Empty();RefillBrain.ReserveTargets.Empty();Refill.Buildings[0].Enabled=false;
    const auto& Factory=Refill.Buildings[2];const double RefillTarget=FMath::Max(Recipe.Inputs[Ore]*Brain.RecipeInputBuffer,Refill.ProductionInputBuffer(Factory,Recipe.Id,Ore));
    TestTrue(TEXT("The ordinary refill honors the actual two-haul raw buffer beyond the old four-cycle amount"),RefillTarget>Recipe.Inputs[Ore]*Brain.RecipeInputBuffer&&RefillBrain.ManageTrade(Refill));
    TestTrue(TEXT("With no outstanding output bill, paid trade funds the real raw operating buffer"),Refill.FindBuilding(PortId)->Shipment.Buy&&Refill.FindBuilding(PortId)->Shipment.Resource==Ore&&FMath::IsNearlyEqual(Refill.FindBuilding(PortId)->Shipment.Quantity,RefillTarget,1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAISurplusExportTest,"Seige.AI.ProtectedManufacturedSurplusExports",AIFlags)
bool FSeigeAISurplusExportTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Base;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Base,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Base,Error)){AddError(Error);return false;}
    const FVector2D Home=Base.Buildings[0].Position;
    if(!Base.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)){AddError(Error);return false;}
    const int32 PortId=Base.Buildings.Last().Id;
    // Installed-port fixture: retain the real landed body identities and paid
    // grid geometry, then isolate an existing physical stock and its sale.
    for(auto& B:Base.Buildings)if(B.IsConstructing){B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Base.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
    for(int32 I=0;I<64;++I){FVector2D A,B;FString Name;if(!Brain.NextPowerRoad(Base,A,B,Name,Error))break;if(!InstallDecisionFixtureRoad(Base,A,B,Error)){AddError(Error);return false;}}
    Base.Couriers.Empty();int32 BodyIndex=0;
    for(auto& W:Base.Workers.Bodies)if(W.State==TEXT("active"))
    {W.Activity=TEXT("operate");W.BuildingId=BodyIndex++==0?PortId:Base.Buildings[0].Id;W.RoadId=W.DeliveryId=W.ContainerId=0;W.ContainerKind.Empty();W.Route.Empty();W.NextWaypoint=0;W.Position=Base.BuildingAccessPoint(*Base.FindBuilding(W.BuildingId));W.Outdoor=true;}
    for(auto& B:Base.Buildings){B.Inventory.Empty();B.ProductionInputs.Empty();B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.ProductionReservedLitres=0;B.Progress=0;}
    Base.Workers.RefreshMetrics(Base);Base.Energy.Invalidate();Base.Energy.Tick(Base,0);
    Brain.Targets.Empty();Brain.ReserveTargets={{TEXT("conductors"),20}};
    for(int32 Count:{1,2}){FSeigeAIBuildTarget T;T.Definition=TEXT("conductor_works");T.Count=Count;Brain.Targets.Add(T);}
    const FString Goods=TEXT("conductors"),Raw=Brain.ExportNode(Base)->Resource;
    const double Future=2*Base.BuildingDefs[TEXT("conductor_works")].Cost[Goods];
    const double SafeBuffer=Base.OperatingBuffer(Goods)+Brain.ReserveTargets[Goods];
    Base.Buildings[0].Inventory={{Goods,Future+SafeBuffer+500},{Raw,1000},{TEXT("alloy"),12},{TEXT("components"),30}};
    Base.Credits=0;const double Stock=Base.TotalStock(Goods);const int32 Bodies=Base.Workers.Bodies.Num();
    TestTrue(TEXT("Repeated cumulative targets protect two final bills, not their sum"),FMath::IsNearlyEqual(Brain.ManufacturedExportSurplus(Base,Goods),500.,1.e-8));
    TestEqual(TEXT("Stored workers never enter the manufactured-surplus path"),Brain.ManufacturedExportSurplus(Base,TEXT("stored_workers")),0.);
    FSeigeSimulation Producer=Base;if(!Producer.SetProductionRecipe(Producer.Buildings[0].Id,TEXT("make_circuits"),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Selected production inputs retain their operating and AI refill buffers"),Brain.ManufacturedExportSurplus(Producer,Goods)<=500-Producer.ProductionInputs(Producer.Buildings[0],TEXT("make_circuits"))[Goods]*Brain.RecipeInputBuffer);
    FSeigeSimulation Queued=Base;FSeigeBuilding Site;Site.Id=1000000;Site.DefId=TEXT("conductor_works");Site.Health=Queued.BuildingDefs[Site.DefId].Health;Site.IsConstructing=true;Site.Enabled=false;
    Site.InstalledMaterials.Add(Goods,20);Site.ConstructionMaterials.Add(Goods,30);Queued.Buildings[0].Inventory[Goods]-=50;Queued.Buildings.Add(Site);
    TestTrue(TEXT("Installed and on-site material retain the same protected total through a paused paid site"),FMath::IsNearlyEqual(Brain.ManufacturedExportSurplus(Queued,Goods),500.,1.e-8));
    FSeigeSimulation Replacement=Base;Site.IsConstructing=false;Site.Health=1;Site.Inventory.Add(Goods,30);Site.ConstructionMaterials.Empty();Replacement.Buildings.Add(Site);
    const double BeforeLoss=Brain.ManufacturedExportSurplus(Replacement,Goods);Replacement.Buildings.Last().Health=0;
    TestTrue(TEXT("Destruction restores the complete planned replacement bill before another export"),FMath::IsNearlyEqual(BeforeLoss-Brain.ManufacturedExportSurplus(Replacement,Goods),Base.BuildingDefs[Site.DefId].Cost[Goods]+30,1.e-8));
    FSeigeSimulation WorkerReserve=Base;WorkerReserve.Buildings[0].Inventory.Add(TEXT("components"),1000);
    const double BeforeTarget=Brain.ManufacturedExportSurplus(WorkerReserve,TEXT("components"));
    if(!WorkerReserve.SetWorkerSurplusTarget(2,Error)){AddError(Error);return false;}
    TestTrue(TEXT("An explicit worker target retains its real assembly inputs"),BeforeTarget-Brain.ManufacturedExportSurplus(WorkerReserve,TEXT("components"))>=2*WorkerReserve.Recipes[TEXT("assemble_robot")].Inputs[TEXT("components")]-1.e-8);
    TestTrue(TEXT("Useful higher-value manufactured stock queues an ordinary sale"),Brain.ManageTrade(Base));
    const auto Order=Base.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("Shipment value chooses only the unreserved 500kg instead of the lower-value raw flight"),!Order.Buy&&Order.Resource==Goods&&FMath::IsNearlyEqual(Order.Quantity,500.,1.e-8));
    TestTrue(TEXT("Queueing a sale grants no credits, removes no goods and creates no bodies"),Base.Credits==0&&Base.TotalStock(Goods)==Stock&&Base.Workers.Bodies.Num()==Bodies&&Order.GoodsEscrow==0);
    FSeigeSimulation LostStock=Base;LostStock.Buildings[0].Inventory.Remove(Goods);
    TestTrue(TEXT("A manufactured order with lost stock and no remaining producer is cancellable"),Brain.ManageTrade(LostStock));
    TestTrue(TEXT("Cancellation releases the blocked port without granting replacement goods or money"),LostStock.FindBuilding(PortId)->Shipment.Resource.IsEmpty()&&LostStock.Credits==0&&LostStock.TotalStock(Goods)==0);
    FSeigeSimulation Replenishing=Base;Replenishing.Buildings[0].Inventory.Remove(Goods);Replenishing.Buildings[0].Inventory.Add(TEXT("iron_ore"),5);
    if(!Replenishing.SetProductionRecipe(Replenishing.Buildings[0].Id,TEXT("draw_conductors"),Error)){AddError(Error);return false;}
    TestTrue(TEXT("The surviving selected supplier can actually pay a replacement batch"),Replenishing.CanCommitProduction(Replenishing.Buildings[0],TEXT("draw_conductors")));
    TestFalse(TEXT("A surviving selected producer keeps its ordinary waiting export"),Brain.ManageTrade(Replenishing));
    TestEqual(TEXT("A replenishable order is not silently replaced"),Replenishing.FindBuilding(PortId)->Shipment.Resource,Goods);
    FSeigeSimulation RawFallback=Base;RawFallback.CancelPendingExport(PortId,Error);RawFallback.Buildings[0].Inventory[Goods]=Future+SafeBuffer;
    TestTrue(TEXT("Ordinary raw exports remain available when all manufactured stock is protected"),Brain.ManageTrade(RawFallback));
    TestTrue(TEXT("Fallback does not sell the future bill"),!RawFallback.FindBuilding(PortId)->Shipment.Buy&&RawFallback.FindBuilding(PortId)->Shipment.Resource==Raw);
    const double Delivered=Base.DeliveredUnits;
    for(int32 I=0;I<300&&!Base.FindBuilding(PortId)->Shipment.Resource.IsEmpty();++I)Base.Tick(10);
    TestTrue(TEXT("The sale physically hauls cargo and finishes its timed flight"),Base.DeliveredUnits>Delivered&&Base.FindBuilding(PortId)->Shipment.Resource.IsEmpty());
    TestTrue(TEXT("Only the quoted proceeds arrive and the actual sold mass leaves stock"),FMath::IsNearlyEqual(Base.Credits,Order.PriceCredits,1.e-8)&&FMath::IsNearlyEqual(Base.TotalStock(Goods),Stock-Order.Quantity,1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIRoadTradePriorityTest,"Seige.AI.RoadGridImportPriority",AIFlags)
bool FSeigeAIRoadTradePriorityTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    const FVector2D Home=Colony.Buildings[0].Position;
    if(!Colony.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)){AddError(Error);return false;}
    // Isolate the next order at an existing port. No simulation tick, free
    // delivery, or auto-completed road is used to establish the assertion.
    auto& Port=Colony.Buildings.Last();const int32 PortId=Port.Id;
    Port.IsConstructing=false;Port.ConstructionProgress=1;Port.InstalledMaterials=Colony.Definition(Port)->Cost;
    Port.ConstructionMaterials.Empty();Port.Builders=Port.BuildersOnSite=Port.TravellingBuilders=0;
    Colony.Energy.Invalidate();Colony.Energy.Tick(Colony,0);
    Brain.Targets.Empty();FSeigeAIBuildTarget Later;Later.Definition=TEXT("alloy_refinery");Later.Count=1;Brain.Targets.Add(Later);
    Brain.ReserveTargets.Empty();Brain.ImportBatch=100000;
    FVector2D A,B;FString Target;
    if(!Brain.NextPowerRoad(Colony,A,B,Target,Error)){AddError(TEXT("No legal road funding fixture: ")+Error);return false;}
    const auto Bill=Colony.RoadCost(A,B,Colony.InitialRoadTier());const FString Missing=TEXT("conductors");
    if(!TestTrue(TEXT("The planned connector has a real conductor bill"),Bill.FindRef(Missing)>0))return false;
    for(auto& Building:Colony.Buildings)Building.Inventory.Remove(Missing);
    for(const auto& Item:Bill)if(Item.Key!=Missing)Colony.Buildings[0].Inventory.FindOrAdd(Item.Key)+=Item.Value;
    Colony.Credits=Colony.TradeQuote(Missing,Bill[Missing],true);const double CreditsBefore=Colony.Credits;
    TestTrue(TEXT("An unfunded road still passes every geometry and worker-access check"),Colony.CanPlaceRoad(A,B,Error,false));
    TestFalse(TEXT("Ordinary road placement still rejects the missing physical bill"),Colony.CanPlaceRoad(A,B,Error));
    const int32 RoadsBefore=Colony.Roads.Num();bool Waiting=false;
    TestFalse(TEXT("The AI cannot construct an unpaid connector"),Brain.ConnectPowerRoad(Colony,Waiting));
    TestTrue(TEXT("The unfunded connector keeps expansion waiting"),Waiting);
    FVector2D RepeatA,RepeatB;FString RepeatTarget;
    TestTrue(TEXT("Funding and construction retain the same legal segment"),Brain.NextPowerRoad(Colony,RepeatA,RepeatB,RepeatTarget,Error)&&RepeatA.Equals(A,.0001)&&RepeatB.Equals(B,.0001));
    TestTrue(TEXT("Trade orders the connector prerequisite before the later refinery"),Brain.ManageTrade(Colony));
    const auto& Order=Colony.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("The order buys exactly the missing actual road bill"),Order.Buy&&Order.Resource==Missing&&FMath::IsNearlyEqual(Order.Quantity,Bill[Missing],1.e-8));
    TestTrue(TEXT("Every imported unit reserves its real credit quote"),FMath::IsNearlyEqual(CreditsBefore-Colony.Credits,Colony.TradeQuote(Missing,Bill[Missing],true),1.e-8));
    TestEqual(TEXT("Planning and importing do not create an unpaid road"),Colony.Roads.Num(),RoadsBefore);
    TestEqual(TEXT("Ordered imports are not instantly added to local stock"),Colony.TotalStock(Missing),0.);
    // Geometry-only planning is not permission to ignore friendly structures.
    TestFalse(TEXT("The material-independent query still rejects a route through the core"),Colony.CanPlaceRoad(Home-FVector2D(1000,0),Home+FVector2D(1000,0),Error,false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIConnectedPlacementTest,"Seige.AI.RoadConnectableBuildingPlacement",AIFlags)
bool FSeigeAIConnectedPlacementTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Colony.Initialize(AIRules(),Error,false,false)||!Brain.LoadConfig(Colony,AIDirectory(),Error)||!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    const auto& Core=Colony.Buildings[0];const FVector2D Home=Core.Position,Start=Colony.BuildingAccessPoint(Core);
    const auto& Factory=Colony.BuildingDefs[TEXT("worker_factory")];
    FSeigeBuilding Preview;Preview.Id=INDEX_NONE;Preview.DefId=Factory.Id;Preview.Health=Factory.Health;
    const double AccessClearance=FVector2D::Distance(Colony.BuildingAccessPoint(Preview),Preview.Position)-Factory.ReservedFootprint;
    double RoadHalfWidth=0;for(const auto& Tier:Colony.TransportTiers)RoadHalfWidth=FMath::Max(RoadHalfWidth,Tier.Value.WidthMeters*.5/Colony.MetersPerWorldUnit());
    // The entrance is in a legal building gap wide enough for a person but
    // narrower than the authored maximum-tier road. No seed coordinates needed.
    Preview.Position=Home-FVector2D(Colony.Definition(Core)->ReservedFootprint+Factory.ReservedFootprint+AccessClearance+RoadHalfWidth*.5,0);
    const FVector2D BadPosition=Preview.Position,End=Colony.BuildingAccessPoint(Preview);
    if(!TestTrue(TEXT("The isolated narrow-gap plot passes ordinary placement rules"),Colony.CanPlaceBuilding(Factory.Id,BadPosition,Error))){AddError(Error);return false;}
    TArray<FVector2D> Walking,Road;
    TestTrue(TEXT("Actual workers can walk around the prospective reserved plot to its entrance"),Colony.FindRoute(Start,End,Walking,-1,false,&Preview));
    TestFalse(TEXT("A full-width future road cannot reach that entrance"),Colony.FindRoadRoute(Start,End,Road,&Preview));
    const int32 BuildingsBefore=Colony.Buildings.Num(),RoadsBefore=Colony.Roads.Num();const double AlloyBefore=Colony.ConstructionAvailable(TEXT("alloy"));
    TestFalse(TEXT("AI refuses to purchase the electrically unreachable building"),Brain.PlaceConnectedBuilding(Colony,Factory.Id,BadPosition,Error));
    TestEqual(TEXT("Rejected preflight creates no construction site"),Colony.Buildings.Num(),BuildingsBefore);
    TestEqual(TEXT("Rejected preflight creates no road"),Colony.Roads.Num(),RoadsBefore);
    TestEqual(TEXT("Rejected preflight reserves no material"),Colony.ConstructionAvailable(TEXT("alloy")),AlloyBefore);
    if(!TestTrue(TEXT("The same search chooses another legal plot"),Brain.BuildNear(Colony,Factory.Id,BadPosition,UE_PI,Brain.RingStep))){AddError(Brain.GetStatus());return false;}
    const auto& Built=Colony.Buildings.Last();
    TestFalse(TEXT("The committed plot is not the rejected narrow entrance"),Built.Position.Equals(BadPosition,.01));
    TestEqual(TEXT("Search commits exactly one physical construction site"),Colony.Buildings.Num(),BuildingsBefore+1);
    TestTrue(TEXT("Accepted site reserves its authored material bill"),FMath::IsNearlyEqual(AlloyBefore-Colony.ConstructionAvailable(TEXT("alloy")),Factory.Cost.FindRef(TEXT("alloy")),1.e-6));
    FVector2D A,B;bool NeedsSegment=false;
    if(!TestTrue(TEXT("After committing the real plot its full grid route remains legal"),Brain.FindPowerConnection(Colony,Built,nullptr,A,B,NeedsSegment,Error)&&NeedsSegment)){AddError(Error);return false;}
    TestTrue(TEXT("The first connector still uses an ordinary paid construction order"),Colony.PlaceRoad(A,B,Error));
    TestEqual(TEXT("Only the requested road is created"),Colony.Roads.Num(),RoadsBefore+1);
    TestTrue(TEXT("Neither site is granted completed infrastructure"),Colony.Buildings.Last().IsConstructing&&Colony.Roads.Last().IsConstructing);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIFuelSupplyTest,"Seige.AI.GeneratorFuelImportPriority",AIFlags)
bool FSeigeAIFuelSupplyTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    const FVector2D Home=Colony.Buildings[0].Position;
    if(!Colony.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)||!Colony.PlaceBuilding(TEXT("fuel_generator"),Home+FVector2D(0,1400),Error)){AddError(Error);return false;}
    // Isolate trade scheduling at normally purchased, installed facilities;
    // this fixture does not replace paid end-to-end survival coverage.
    for(auto& B:Colony.Buildings)if(B.IsConstructing)
    {B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Colony.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
    const int32 PortId=Colony.Buildings[1].Id,GeneratorId=Colony.Buildings[2].Id;
    const auto* Generator=Colony.Energy.Definition(Colony.FindBuilding(GeneratorId)->DefId);
    if(!TestTrue(TEXT("Fixture generator requires actual physical fuel"),Generator&&!Generator->FuelResource.IsEmpty()))return false;
    const FString Fuel=Generator->FuelResource;const double Buffer=Colony.Energy.FuelDemand(Colony.FindBuilding(GeneratorId)->DefId,Fuel)*Brain.FuelImportBufferCycles;
    for(auto& B:Colony.Buildings)B.Inventory.Remove(Fuel);
    Colony.Buildings[0].Inventory.Add(Fuel,Buffer*.25);
    Brain.Targets.Empty();FSeigeAIBuildTarget Later;Later.Definition=TEXT("alloy_refinery");Later.Count=1;Brain.Targets.Add(Later);Brain.ImportBatch=100000;
    FSeigeSimulation SmallDeficit=Colony;FSeigeScenarioAI SmallBrain=Brain;
    SmallDeficit.Buildings[0].Inventory.FindOrAdd(Fuel)=Buffer-.0628;
    for(auto& B:SmallDeficit.Buildings)B.Inventory.Remove(TEXT("components"));
    SmallBrain.Targets[0].Definition=TEXT("robot_service_bay");
    SmallDeficit.Credits=SmallDeficit.TradeQuote(TEXT("components"),1000,true);
    const double SmallCredits=SmallDeficit.Credits,SmallFuel=SmallDeficit.TotalStock(Fuel);
    TestTrue(TEXT("A tiny fuel deficit leaves the port available for paid worker-support inputs"),SmallBrain.ManageTrade(SmallDeficit));
    const auto& SupportOrder=SmallDeficit.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("Fuel above the configured low-water mark cannot monopolize the next import"),SupportOrder.Buy&&SupportOrder.Resource==TEXT("components"));
    TestTrue(TEXT("Competing support import pays its complete price without creating fuel"),FMath::IsNearlyEqual(SmallCredits-SmallDeficit.Credits,SupportOrder.PriceCredits,1.e-8)&&SmallDeficit.TotalStock(Fuel)==SmallFuel);
    const double Deficit=Buffer-Colony.TotalStock(Fuel);Colony.Credits=Colony.TradeQuote(Fuel,Deficit,true);const double Before=Colony.Credits;
    FSeigeSimulation Unfunded=Colony;Unfunded.Credits=0;
    TestTrue(TEXT("An installed generator's missing operating fuel outranks future construction"),Brain.ManageTrade(Colony));
    const auto& Order=Colony.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("Low fuel refills the external trade reserve less existing physical stock"),Order.Buy&&Order.Resource==Fuel&&FMath::IsNearlyEqual(Order.Quantity,Deficit,1.e-8));
    TestTrue(TEXT("Fuel purchase reserves its full real credit price"),FMath::IsNearlyEqual(Before-Colony.Credits,Colony.TradeQuote(Fuel,Deficit,true),1.e-8));
    TestEqual(TEXT("An order does not instantly refuel the generator"),Colony.FindBuilding(GeneratorId)->Inventory.FindRef(Fuel),0.);
    TestEqual(TEXT("Buying fuel does not create extra goods before arrival"),Colony.TotalStock(Fuel),Buffer*.25);
    Brain.ManageTrade(Unfunded);
    TestFalse(TEXT("An unfunded colony cannot get a free fuel import"),Unfunded.FindBuilding(PortId)->Shipment.Buy&&!Unfunded.FindBuilding(PortId)->Shipment.Resource.IsEmpty());
    TestEqual(TEXT("No credits are granted for a generator shortage"),Unfunded.Credits,0.);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIReplicatorBootstrapTest,"Seige.AI.PaidCoreReplicationBootstrap",AIFlags)
bool FSeigeAIReplicatorBootstrapTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    auto& Core=Colony.Buildings[0];const int32 CoreId=Core.Id,Bodies=Colony.Workers.Bodies.Num();
    // Isolate the existing core and finite landed ore, refunding any unfinished
    // fixture batch before creating a genuine construction-material shortage.
    for(const auto& P:Core.ProductionInputs)Core.Inventory.FindOrAdd(P.Key)+=P.Value;
    Core.ProductionInputs.Empty();Core.ProductionCommitted=false;Core.CommittedRecipe.Empty();Core.ProductionReservedLitres=0;Core.Progress=0;
    Core.Inventory.Remove(TEXT("alloy"));
    Brain.Targets.Empty();Brain.ReserveTargets.Empty();FSeigeAIBuildTarget Target;Target.Definition=TEXT("alloy_refinery");Target.Count=1;Brain.Targets.Add(Target);
    const double Ore=Colony.TotalStock(TEXT("iron_ore")),Energy=Colony.Energy.ConsumedKWh,Credits=Colony.Credits,Time=Colony.Time;
    TestTrue(TEXT("A genuine construction shortage selects a funded command recipe"),Brain.ManageCoreProduction(Colony));
    TestEqual(TEXT("Finite starter metal becomes construction alloys before importing finished goods"),Core.SelectedRecipe,FString(TEXT("smelt_alloy")));
    TestTrue(TEXT("Recipe selection grants no stock, energy, time or workers"),Colony.TotalStock(TEXT("alloy"))==0&&Colony.TotalStock(TEXT("iron_ore"))==Ore&&Colony.Energy.ConsumedKWh==Energy&&Colony.Time==Time&&Colony.Workers.Bodies.Num()==Bodies);
    FSeigeSimulation NoEnergy=Colony;NoEnergy.Buildings[0].BatteryEnergyKWh=0;NoEnergy.Energy.Tick(NoEnergy,0);
    TestFalse(TEXT("The batch cannot commit without its actual stored energy"),NoEnergy.CanCommitProduction(NoEnergy.Buildings[0],TEXT("smelt_alloy")));
    const auto& Recipe=Colony.Recipes[TEXT("smelt_alloy")];const double OutputBefore=Colony.ProducedUnits.FindRef(TEXT("alloy"));
    for(int32 I=0;I<20000&&Colony.ProducedUnits.FindRef(TEXT("alloy"))==OutputBefore;++I)Colony.Tick(Colony.FixedStepSeconds());
    TestTrue(TEXT("Real operators finish the selected paid batch"),Colony.ProducedUnits.FindRef(TEXT("alloy"))>=OutputBefore+Recipe.Outputs.FindRef(TEXT("alloy")));
    TestTrue(TEXT("Production consumes the authored ore and emits its coproduct"),FMath::IsNearlyEqual(Colony.TotalStock(TEXT("iron_ore")),Ore-Recipe.Inputs.FindRef(TEXT("iron_ore")),1.e-8)&&Colony.ProducedUnits.FindRef(TEXT("conductors"))>=Recipe.Outputs.FindRef(TEXT("conductors")));
    TestTrue(TEXT("Core energy multiplier is actually paid without free credits or worker creation"),Colony.Energy.ConsumedKWh-Energy>=Colony.ProductionEnergy(*Colony.FindBuilding(CoreId),Recipe.Id)&&Colony.Credits==Credits&&Colony.Workers.Bodies.Num()==Bodies);
    FSeigeSimulation Committed=Colony;auto& Busy=Committed.Buildings[0];
    Busy.ProductionCommitted=true;Busy.CommittedRecipe=TEXT("assemble_robot");Busy.SelectedRecipe=TEXT("assemble_robot");
    TestFalse(TEXT("Planning never replaces an already committed worker batch"),Brain.ManageCoreProduction(Committed));
    TestEqual(TEXT("The worker batch and its selected recipe remain intact"),Busy.SelectedRecipe,FString(TEXT("assemble_robot")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAISelectedRecipeTradeTest,"Seige.AI.SelectedProductionInputImports",AIFlags)
bool FSeigeAISelectedRecipeTradeTest::RunTest(const FString& Parameters)
{
    for(bool WithFactory:{false,true})
    {
        FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
        if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
        const FVector2D Home=Colony.Buildings[0].Position;
        if(!Colony.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)||(WithFactory&&!Colony.PlaceBuilding(TEXT("worker_factory"),Home+FVector2D(0,-1400),Error))){AddError(Error);return false;}
        const int32 PortId=Colony.Buildings[1].Id;
        // Isolate the next funding decision at installed, normally purchased
        // sites. The elapsed-work/endurance tests remain separate.
        for(auto& B:Colony.Buildings)if(B.IsConstructing)
        {B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Colony.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
        Colony.Energy.Invalidate();Colony.Energy.Tick(Colony,0);
        for(int32 I=0;I<64;++I)
        {FVector2D A,B;FString Name;if(!Brain.NextPowerRoad(Colony,A,B,Name,Error))break;if(!InstallDecisionFixtureRoad(Colony,A,B,Error)){AddError(Error);return false;}}
        for(const auto& B:Colony.Buildings)if(!TestTrue(TEXT("Recipe-import fixture has a completed paid-grid prerequisite"),Colony.IsRoadGridConnected(Colony.Buildings[0].Id,B.Id)))return false;
        for(auto& B:Colony.Buildings)
        {
            // Return an unrelated in-flight fixture batch before selecting the
            // shortage. No production completes during the funding assertions.
            for(const auto& P:B.ProductionInputs)B.Inventory.FindOrAdd(P.Key)+=P.Value;
            B.ProductionInputs.Empty();B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.ProductionReservedLitres=0;B.Progress=0;B.Inventory.Remove(TEXT("components"));
        }
        Colony.BuildingDefs[Colony.CoreDefinition].RecipeInputMultiplier=2;
        Brain.Targets.Empty();Brain.ReserveTargets.Empty();Brain.ImportBatch=10000;
        if(!Colony.SetWorkerSurplusTarget(1,Error)){AddError(Error);return false;}
        TestTrue(TEXT("Core worker selection has no fixed definition recipe"),Colony.Definition(Colony.Buildings[0])->Recipe.IsEmpty());
        TestEqual(TEXT("Core has a real selected worker batch to fund"),Colony.ActiveProductionRecipe(Colony.Buildings[0]),FString(TEXT("assemble_robot")));
        if(WithFactory)TestEqual(TEXT("Dedicated factory also exposes its selected worker batch"),Colony.ActiveProductionRecipe(Colony.Buildings[2]),FString(TEXT("assemble_robot")));
        const double PerBody=Colony.Recipes[TEXT("assemble_robot")].Inputs.FindRef(TEXT("components"));
        const double Expected=PerBody*(2+(WithFactory?1:0))*Brain.RecipeInputBuffer;
        Colony.Credits=Colony.TradeQuote(TEXT("components"),Expected,true);const double Before=Colony.Credits;
        FSeigeSimulation Unfunded=Colony;Unfunded.Credits=0;
        TestTrue(TEXT("AI orders selected worker inputs without a generic reserve target"),Brain.ManageTrade(Colony));
        const auto& Shipment=Colony.FindBuilding(PortId)->Shipment;
        TestTrue(TEXT("The import includes every selected producer and its input multiplier"),Shipment.Buy&&Shipment.Resource==TEXT("components")&&FMath::IsNearlyEqual(Shipment.Quantity,Expected,1.e-8));
        TestTrue(TEXT("The full real quote is reserved from credits"),FMath::IsNearlyEqual(Before-Colony.Credits,Before,1.e-8)&&FMath::IsNearlyEqual(Shipment.PriceCredits,Before,1.e-8));
        TestEqual(TEXT("An import order creates no instantly usable components"),Colony.TotalStock(TEXT("components")),0.);
        Brain.ManageTrade(Unfunded);
        TestFalse(TEXT("Missing credits cannot grant a worker-input import"),Unfunded.FindBuilding(PortId)->Shipment.Buy&&!Unfunded.FindBuilding(PortId)->Shipment.Resource.IsEmpty());
        TestEqual(TEXT("Unfunded planning grants no credits"),Unfunded.Credits,0.);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAISupportPendingRoadTest,"Seige.AI.SupportRecoveryWhileRoadPending",AIFlags)
bool FSeigeAISupportPendingRoadTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    const FVector2D Home=Colony.Buildings[0].Position,Port=Colony.BuildingAccessPoint(Colony.Buildings[0]);
    if(!Colony.PlaceBuilding(TEXT("worker_factory"),Home+FVector2D(1400,0),Error)||!Colony.PlaceRoad(Port,Port+FVector2D(0,500),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Real queued construction exceeds current worker support"),Colony.TotalJobs>Colony.RobotSupportCapacity);
    const int32 Before=Colony.Buildings.Num(),RoadId=Colony.Roads.Last().Id;
    const double Available=Colony.ConstructionAvailable(TEXT("alloy"));
    TestTrue(TEXT("Pending road does not block the configured paid support prerequisite"),Brain.MakeDecision(Colony));
    if(!TestEqual(TEXT("One recovery bay is queued"),Colony.Buildings.Num(),Before+1))return false;
    const auto& Bay=Colony.Buildings.Last();
    TestTrue(TEXT("Recovery is an ordinary unfinished service site"),Colony.Definition(Bay)->Role==TEXT("service")&&Bay.IsConstructing);
    TestTrue(TEXT("Recovery reserves its entire authored construction bill"),FMath::IsNearlyEqual(Available-Colony.ConstructionAvailable(TEXT("alloy")),Colony.Definition(Bay)->Cost.FindRef(TEXT("alloy")),1.e-6));
    Brain.MakeDecision(Colony);
    TestEqual(TEXT("Waiting recovery does not queue duplicate support"),Colony.Buildings.Num(),Before+1);
    TestTrue(TEXT("No pending infrastructure is finished for free"),Colony.FindRoad(RoadId)->IsConstructing&&Colony.Buildings.Last().IsConstructing);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIImpossibleExportTest,"Seige.AI.LostProducerExportRecovery",AIFlags)
bool FSeigeAIImpossibleExportTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),false,Error,false,false)||!FinishCoreDeployment(Colony,Error)){AddError(Error);return false;}
    const auto* Node=Brain.ExportNode(Colony);if(!Node)return false;const FString Resource=Node->Resource;const FVector2D NodePosition=Node->Position,Home=Colony.Buildings[0].Position;
    if(!Colony.PlaceBuilding(TEXT("trading_port"),Home+FVector2D(1400,0),Error)||!Colony.PlaceBuilding(TEXT("extraction_mine"),NodePosition,Error)){AddError(Error);return false;}
    const int32 PortId=Colony.Buildings[1].Id,MineId=Colony.Buildings[2].Id;
    // Decision fixture: installed paid facilities and a finite harvested lot.
    // No elapsed production or free rebuilding is used by the assertions.
    for(auto& B:Colony.Buildings)if(B.IsConstructing)
    {B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Colony.Definition(B)->Cost;B.ConstructionMaterials.Empty();B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;}
    Colony.Energy.Invalidate();Colony.Energy.Tick(Colony,0);
    for(int32 I=0;I<64;++I)
    {FVector2D A,B;FString Name;if(!Brain.NextPowerRoad(Colony,A,B,Name,Error))break;if(!InstallDecisionFixtureRoad(Colony,A,B,Error)){AddError(Error);return false;}}
    if(!TestTrue(TEXT("Recovery port has its real grid connection"),Colony.IsRoadGridConnected(Colony.Buildings[0].Id,PortId)))return false;
    for(auto& B:Colony.Buildings)B.Inventory.Remove(Resource);
    constexpr double ExportAmount=100,SurvivingGoods=8;
    Colony.FindBuilding(MineId)->Inventory.Add(Resource,ExportAmount);Colony.FindBuilding(PortId)->Inventory.Add(Resource,SurvivingGoods);
    if(!Colony.TryTrade(PortId,Resource,ExportAmount,false,Error)){AddError(Error);return false;}
    FSeigeSimulation Replenishable=Colony;Replenishable.FindBuilding(MineId)->Inventory.Remove(Resource);
    TestFalse(TEXT("A living producer is allowed to replenish an awaiting export"),Brain.ManageTrade(Replenishable));
    TestEqual(TEXT("A viable pending order is preserved"),Replenishable.FindBuilding(PortId)->Shipment.Quantity,ExportAmount);
    FSeigeSimulation Departed=Colony;Departed.FindBuilding(PortId)->Shipment.Departed=true;Departed.FindBuilding(PortId)->Shipment.GoodsEscrow=ExportAmount;
    TestFalse(TEXT("Already departed or escrowed exports cannot be cancelled"),Departed.CancelPendingExport(PortId,Error));
    Colony.OnBuildingDestroyed(MineId);
    Brain.Targets.Empty();FSeigeAIBuildTarget Rebuild;Rebuild.Definition=TEXT("extraction_mine");Rebuild.Count=1;Brain.Targets.Add(Rebuild);Brain.ReserveTargets.Empty();
    for(auto& B:Colony.Buildings)B.Inventory.Remove(TEXT("alloy"));
    const double Bill=Colony.BuildingDefs[Rebuild.Definition].Cost.FindRef(TEXT("alloy"));
    const double ImportAmount=FMath::Min(Bill,Brain.ImportBatch);Colony.Credits=Colony.TradeQuote(TEXT("alloy"),ImportAmount,true);const double CreditsBefore=Colony.Credits;
    const int32 BuildingsBefore=Colony.Buildings.Num(),WorkersBefore=Colony.Workers.Bodies.Num(),CouriersBefore=Colony.Couriers.Num();
    TestTrue(TEXT("Lost producer and insufficient surviving goods release the impossible export"),Brain.ManageTrade(Colony));
    TestTrue(TEXT("Only the pending order is cleared"),Colony.FindBuilding(PortId)->Shipment.Resource.IsEmpty());
    TestEqual(TEXT("Cancellation grants no export earnings or refund"),Colony.Credits,CreditsBefore);
    TestEqual(TEXT("Surviving physical goods are retained"),Colony.TotalStock(Resource),SurvivingGoods);
    TestEqual(TEXT("Cancellation does not alter the worker ledger"),Colony.Workers.Bodies.Num(),WorkersBefore);
    TestEqual(TEXT("Cancellation does not remove physical delivery tasks"),Colony.Couriers.Num(),CouriersBefore);
    const double AlloyBeforeImport=Colony.TotalStock(TEXT("alloy"));
    TestTrue(TEXT("The released port can pay for the actual mine rebuild bill"),Brain.ManageTrade(Colony));
    const auto& Import=Colony.FindBuilding(PortId)->Shipment;
    TestTrue(TEXT("Recovery buys the missing construction material with its full quote"),Import.Buy&&Import.Resource==TEXT("alloy")&&FMath::IsNearlyEqual(Import.Quantity,ImportAmount,1.e-8)&&FMath::IsNearlyEqual(CreditsBefore-Colony.Credits,Import.PriceCredits,1.e-8));
    TestFalse(TEXT("Paid imports cannot be cancelled through the export-only API"),Colony.CancelPendingExport(PortId,Error));
    TestEqual(TEXT("Recovery grants no completed replacement or instant imported goods"),Colony.Buildings.Num(),BuildingsBefore);
    TestEqual(TEXT("Ordering the import adds no instant usable alloy"),Colony.TotalStock(TEXT("alloy")),AlloyBeforeImport);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIDevelopedNeighborRecoveryTest,"Seige.AI.DevelopedNeighborSeedRecovery",AIFlags)
bool FSeigeAIDevelopedNeighborRecoveryTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Colony;FSeigeScenarioAI Brain;
    const double Started=FPlatformTime::Seconds();
    if(!Brain.Initialize(Colony,AIRules(),AIDirectory(),true,Error,true,true,-2027807669)){AddError(Error);return false;}
    AddInfo(FString::Printf(TEXT("Established exact-seed initialization: %.4f wall seconds, %d buildings, %d roads, %d worker identities"),FPlatformTime::Seconds()-Started,Colony.Buildings.Num(),Colony.Roads.Num(),Colony.Workers.Bodies.Num()));
    TestTrue(TEXT("Signed neighbor seed loads a validated established state immediately"),Brain.IsReady()&&!Brain.IsPreparing()&&Colony.Time==60&&!Colony.Failed&&!Colony.Escaped);
    TestEqual(TEXT("Authored established plan has core plus 23 installations"),Colony.Buildings.Num(),24);
    TestEqual(TEXT("Regression retains the exact signed generation seed"),Colony.GenerationSeed,-2027807669);
    TestEqual(TEXT("No simulated delivery history is fabricated"),Colony.DeliveredUnits,0.);
    TestTrue(TEXT("No simulated production history is fabricated"),Colony.ProducedUnits.IsEmpty());
    TSet<FString> Identities;int32 Active=0,Stored=0;
    for(const auto& W:Colony.Workers.Bodies){TestFalse(TEXT("Established workers have unique identities"),Identities.Contains(W.Id));Identities.Add(W.Id);Active+=W.State==TEXT("active");Stored+=W.State==TEXT("stored");}
    TestEqual(TEXT("All 44 active bodies are present"),Active,44);TestEqual(TEXT("All four packed bodies are present"),Stored,4);
    for(const auto& B:Colony.Buildings){TestFalse(TEXT("Initial structures are completed scenario assets"),B.IsConstructing);TestTrue(TEXT("Every initial facility has a connected real road grid"),Colony.IsRoadGridConnected(Colony.Buildings[0].Id,B.Id));TestTrue(TEXT("Cargo fits the local building"),Colony.StorageUsed(B)<=Colony.Definition(B)->StorageCapacity+1.e-8);}
    const FString SavePath=AIOutput(TEXT("established-initial.json"));if(!Colony.Save(SavePath,Error)){AddError(Error);return false;}
    FSeigeSimulation Restored;FSeigeScenarioAI Resume;
    if(!Resume.Initialize(Restored,AIRules(),AIDirectory(),false,Error,true,true,-2027807669)||!Restored.Load(SavePath,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Save/load preserves established worker identities"),Restored.Workers.Bodies.Num(),Colony.Workers.Bodies.Num());
    for(int32 I=0;I<Colony.Workers.Bodies.Num();++I)TestEqual(TEXT("Worker ID survives restoration"),Restored.Workers.Bodies[I].Id,Colony.Workers.Bodies[I].Id);
    const double Credits=Colony.Credits;const auto* Port=Colony.Buildings.FindByPredicate([&](const auto& B){return Colony.Definition(B)->Role==TEXT("trade");});if(!Port){AddError(TEXT("Manifest trade port missing"));return false;}
    const int32 PortId=Port->Id;const double Ore=Colony.TotalStock(TEXT("iron_ore"));
    TestTrue(TEXT("Established port starts an ordinary paid import"),Colony.TryTrade(PortId,TEXT("iron_ore"),1,true,Error));
    TestTrue(TEXT("Import immediately reserves real credits"),Colony.Credits<Credits);
    TestEqual(TEXT("Ordering imports grants no instant cargo"),Colony.TotalStock(TEXT("iron_ore")),Ore);
    Colony.Tick(300);
    TestTrue(TEXT("Live established continuation settles its real shipment"),Colony.FindBuilding(PortId)->Shipment.Resource.IsEmpty());
    Brain.Tick(Colony,60);
    TestTrue(TEXT("Live continuation manufactures rather than receiving a simulated-history reward"),Colony.ProducedUnits.FindRef(TEXT("components"))>0);
    TestTrue(TEXT("Live continuation physically routes goods"),Colony.DeliveredUnits>0);
    TestFalse(TEXT("Bounded live operation does not end command"),Colony.Failed||Colony.Escaped);
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
    FString Error;FSeigeSimulation Synchronous,Incremental;FSeigeScenarioAI SyncBrain,FrameBrain;
    if(!SyncBrain.Initialize(Synchronous,AIRules(),AIDirectory(),true,Error,false,false)||!FrameBrain.BeginInitialize(Incremental,AIRules(),AIDirectory(),true,Error,false,false)){AddError(Error);return false;}
    bool Complete=false;
    TestTrue(TEXT("Established async adapter returns a ready candidate without simulation growth"),FrameBrain.AdvancePreparation(Incremental,.25,Complete,Error)&&Complete&&FrameBrain.IsReady());
    const FString A=AIOutput(TEXT("established-sync.json")),B=AIOutput(TEXT("established-incremental.json"));
    if(!Synchronous.Save(A,Error)||!Incremental.Save(B,Error)){AddError(Error);return false;}
    FString Left,Right;FFileHelper::LoadFileToString(Left,*A);FFileHelper::LoadFileToString(Right,*B);
    TestEqual(TEXT("The same manifest/seed gives deterministic complete state through both interfaces"),Left,Right);

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
