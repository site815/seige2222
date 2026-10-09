#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAICoveredPlacementTest,"Seige.AI.CoveredCivilianPlacement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAICoveredPlacementTest::RunTest(const FString& Parameters)
{
    const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Directory=FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES"));
    FSeigeSimulation Colony;FSeigeScenarioAI Brain;FString Error;
    if(!Brain.Initialize(Colony,Rules,Directory,false,Error,false,false)){AddError(Error);return false;}
    for(int32 Step=0;Step<400&&Colony.Buildings[0].IsConstructing;++Step)Colony.Tick(10);
    if(!TestFalse(TEXT("Finite crew performs the normal paid core deployment"),Colony.Buildings[0].IsConstructing))return false;
    // Isolate placement geometry from seed-dependent water/deposits and moving
    // guards. Installed towers below are an explicit geometry fixture, not an
    // assertion that the bootstrap economy manufactured a free perimeter.
    Colony.Environment.Enabled=false;Colony.Nodes.Empty();Colony.Combat.Vehicles.Empty();
    const FVector2D Home=Colony.Buildings[0].Position;const double Unit=Colony.MetersPerWorldUnit();
    auto AddTower=[&](int32 Id,FVector2D Meters)
    {
        FSeigeBuilding B;B.Id=Id;B.DefId=TEXT("turret");B.Position=Home+Meters/Unit;B.Health=Colony.BuildingDefs[B.DefId].Health;
        B.ConstructionProgress=1;B.Enabled=true;Colony.Buildings.Add(B);
        FSeigeBuildingCombatState State;State.Id=Id;State.Definition=B.DefId;State.Weapons={TEXT("laser_small")};State.Cooldowns={0};Colony.Combat.BuildingState.Add(Id,State);
    };
    AddTower(100001,FVector2D(-25.3,-61));AddTower(100002,FVector2D(61,-25.3));
    AddTower(100003,FVector2D(61,25.3));AddTower(100004,FVector2D(-61,25.3));
    Brain.RingStart=1700;Brain.RingLimit=2300;Brain.RingStep=300;
    const auto& Solar=Colony.BuildingDefs[TEXT("solar_array")];
    FSeigeSimulation Legacy=Colony,Preferred=Colony;FSeigeScenarioAI LegacyBrain=Brain;
    LegacyBrain.DefenseCoveragePolicy=TEXT("first_legal");
    if(!TestTrue(TEXT("The first legal exposed site remains a feasible paid order"),LegacyBrain.BuildNear(Legacy,Solar.Id,Home,UE_PI*1.25))){AddError(LegacyBrain.GetStatus());return false;}
    const FVector2D Exposed=Legacy.Buildings.Last().Position;const int32 ExposedCoverage=Brain.PlotDefenseCoverage(Colony,Solar,Exposed);
    const double BeforeAlloy=Preferred.TotalStock(TEXT("alloy")),BeforeAvailable=Preferred.ConstructionAvailable(TEXT("alloy"));const int32 BeforeBodies=Preferred.Workers.Bodies.Num();
    if(!TestTrue(TEXT("Coverage preference finds a legal civilian plot without granting infrastructure"),Brain.BuildNear(Preferred,Solar.Id,Home,UE_PI*1.25))){AddError(Brain.GetStatus());return false;}
    const auto& Built=Preferred.Buildings.Last();
    TestTrue(TEXT("A safer reachable plot outranks rebuilding the exposed first slot"),Brain.PlotDefenseCoverage(Colony,Solar,Built.Position)>ExposedCoverage);
    TestFalse(TEXT("Safer placement moves away from the exposed reconstruction site"),Built.Position.Equals(Exposed,.01));
    TestEqual(TEXT("Only one construction site is committed"),Preferred.Buildings.Num(),Colony.Buildings.Num()+1);
    TestTrue(TEXT("The chosen plot still requires physical construction"),Built.IsConstructing&&Built.ConstructionProgress==0);
    TestTrue(TEXT("The normal authored alloy bill is reserved"),FMath::IsNearlyEqual(BeforeAvailable-Preferred.ConstructionAvailable(TEXT("alloy")),Solar.Cost.FindRef(TEXT("alloy")),1.e-6));
    TestEqual(TEXT("Placement neither grants nor teleports alloy"),Preferred.TotalStock(TEXT("alloy")),BeforeAlloy);
    TestEqual(TEXT("Placement adds no worker bodies"),Preferred.Workers.Bodies.Num(),BeforeBodies);
    FVector2D A,B;bool NeedsSegment=false;
    TestTrue(TEXT("The selected building has a feasible ordinary road connection"),Brain.FindPowerConnection(Preferred,Built,nullptr,A,B,NeedsSegment,Error));

    // Prospective buildings must screen their own far approaches. Without this
    // check a range-only planner incorrectly credits the core with full cover.
    FSeigeSimulation CoreOnly=Colony;CoreOnly.Buildings.SetNum(1);CoreOnly.Combat.BuildingState.Empty();
    FSeigeBuildingCombatState CoreGun;CoreGun.Id=CoreOnly.Buildings[0].Id;CoreGun.Definition=CoreOnly.CoreDefinition;CoreGun.Weapons={TEXT("laser_large")};CoreOnly.Combat.BuildingState.Add(CoreGun.Id,CoreGun);
    const FVector2D Probe=Home+FVector2D(1100,0);const int32 OneGun=Brain.PlotDefenseCoverage(CoreOnly,Solar,Probe);
    TestTrue(TEXT("The prospective square blocks its own rear approach despite weapon range"),OneGun>0&&OneGun<Brain.CoverageSamples);
    CoreOnly.Combat.Weapons[TEXT("laser_large")].RangeMeters=1;
    TestEqual(TEXT("Actual equipped range bounds coverage"),Brain.PlotDefenseCoverage(CoreOnly,Solar,Probe),0);
    CoreOnly.Combat.Weapons[TEXT("laser_large")].RangeMeters=Colony.Combat.Weapons[TEXT("laser_large")].RangeMeters;
    CoreOnly.Buildings[0].Enabled=false;
    TestEqual(TEXT("Disabled fixed weapons grant no cover"),Brain.PlotDefenseCoverage(CoreOnly,Solar,Probe),0);
    CoreOnly.Buildings[0].Enabled=true;
    FSeigeBuilding Blocker;Blocker.Id=100010;Blocker.DefId=Solar.Id;Blocker.Position=Home+FVector2D(650,0);Blocker.Health=Solar.Health;CoreOnly.Buildings.Add(Blocker);
    TestTrue(TEXT("Known friendly structures reduce actual clear firing approaches"),Brain.PlotDefenseCoverage(CoreOnly,Solar,Probe)<OneGun);
    CoreOnly.Buildings.Last().Health=0;
    TestEqual(TEXT("Destroyed structures no longer occlude combat geometry"),Brain.PlotDefenseCoverage(CoreOnly,Solar,Probe),OneGun);
    FSeigeEnemy Unseen;Unseen.Id=100011;Unseen.Health=45;Unseen.Position=Home+FVector2D(25000,25000);CoreOnly.Enemies.Add(Unseen);
    TestEqual(TEXT("Hidden enemy positions never influence the preference"),Brain.PlotDefenseCoverage(CoreOnly,Solar,Probe),OneGun);

    // Armed perimeter plots turn toward approaches no existing gun reaches: a
    // body west of the core screens its own far side from the core's lasers, so
    // the next tower leaves the authored east bearing for a western one.
    {
        const auto& Tower=Colony.BuildingDefs[TEXT("turret")];const double East=0;
        FSeigeSimulation Screened=Colony;Screened.Buildings.SetNum(1);Screened.Combat.BuildingState.Empty();Screened.Combat.BuildingState.Add(CoreGun.Id,CoreGun);
        TestTrue(TEXT("With nothing exposed the authored tower bearing stands"),FMath::IsNearlyEqual(Brain.DefenseBearing(Screened,Tower,Home,East,Brain.DefenseDistance),East));
        FSeigeBuilding Screen;Screen.Id=100020;Screen.DefId=Solar.Id;Screen.Position=Home+FVector2D(-650,0);Screen.Health=Solar.Health;Screened.Buildings.Add(Screen);
        const double Bearing=Brain.DefenseBearing(Screened,Tower,Home,East,Brain.DefenseDistance);
        TestTrue(TEXT("A tower turns toward the far side a building screens from the core"),FMath::Cos(Bearing)<-.3);
        FSeigeScenarioAI LegacyBearing=Brain;LegacyBearing.DefenseCoveragePolicy=TEXT("first_legal");
        TestTrue(TEXT("first_legal keeps the authored tower bearing"),FMath::IsNearlyEqual(LegacyBearing.DefenseBearing(Screened,Tower,Home,East,Brain.DefenseDistance),East));
    }

    FSeigeSimulation Unarmed=Colony;Unarmed.Combat.BuildingState.Empty();FSeigeSimulation UnarmedLegacy=Unarmed;
    TestTrue(TEXT("Bootstrap is still allowed with no fixed armed platform"),Brain.BuildNear(Unarmed,Solar.Id,Home,UE_PI*1.25));
    TestTrue(TEXT("The fallback has an ordinary first legal comparison"),LegacyBrain.BuildNear(UnarmedLegacy,Solar.Id,Home,UE_PI*1.25));
    TestTrue(TEXT("Equal zero coverage preserves deterministic authored search order"),Unarmed.Buildings.Last().Position.Equals(UnarmedLegacy.Buildings.Last().Position,.01));
    FSeigeSimulation Defense=Colony,DefenseLegacy=Colony;
    if(!TestTrue(TEXT("Perimeter placement remains feasible"),Brain.BuildNear(Defense,TEXT("turret"),Home,UE_PI*.5))||!LegacyBrain.BuildNear(DefenseLegacy,TEXT("turret"),Home,UE_PI*.5))return false;
    TestTrue(TEXT("Defense exclusion preserves the authored perimeter spread"),Defense.Buildings.Last().Position.Equals(DefenseLegacy.Buildings.Last().Position,.01));

    const FString Invalid=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/ScenarioAI/invalid-coverage"));IFileManager::Get().MakeDirectory(*Invalid,true);
    if(IFileManager::Get().Copy(*FPaths::Combine(Invalid,TEXT("developed_start.json")),*FPaths::Combine(Directory,TEXT("developed_start.json")))!=COPY_OK)return false;
    FString Raw;TSharedPtr<FJsonObject> Config;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(Directory,TEXT("colony_ai.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Config))return false;
    Config->GetObjectField(TEXT("placement"))->SetNumberField(TEXT("coverage_samples"),0);Raw.Empty();
    if(!FJsonSerializer::Serialize(Config.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw))||!FFileHelper::SaveStringToFile(Raw,*FPaths::Combine(Invalid,TEXT("colony_ai.json"))))return false;
    FSeigeScenarioAI InvalidBrain;Error.Empty();TestFalse(TEXT("Unbounded or missing coverage samples are rejected"),InvalidBrain.LoadConfig(Colony,Invalid,Error));TestTrue(TEXT("Bad sample count is diagnosed"),Error.Contains(TEXT("coverage_samples")));
    return true;
}
#endif
