#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIUpgradeTest,"Seige.AI.VoluntaryUpgrades",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIUpgradeTest::RunTest(const FString& Parameters)
{
    const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Directory=FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES"));
    FSeigeSimulation Colony;FSeigeScenarioAI Brain;FString Error;
    if(!Brain.Initialize(Colony,Rules,Directory,false,Error,false,false)){AddError(Error);return false;}
    for(int32 Step=0;Step<400&&Colony.Buildings[0].IsConstructing;++Step)Colony.Tick(10);
    if(!TestFalse(TEXT("Finite crew performs the normal paid core deployment"),Colony.Buildings[0].IsConstructing))return false;
    TestTrue(TEXT("The shipped plan names upgrade families"),Brain.UpgradeFamilies.Contains(TEXT("solar_array"))&&Brain.MaxConcurrentUpgrades>=1);
    TestTrue(TEXT("The AI-chip works is an optional plan entry"),Brain.Targets.ContainsByPredicate([](const FSeigeAIBuildTarget& T){return T.Definition==TEXT("ai_chip_works")&&T.Optional;}));

    // Installed fixtures (as in the placement tests): one completed array and
    // just enough core stock for its level-2 bill above the AI reserve targets.
    auto Stock=[&](const TMap<FString,double>& Bill){for(const auto& P:Bill){const double Need=P.Value+Brain.ReserveTargets.FindRef(P.Key)+1-Colony.ConstructionAvailable(P.Key);if(Need>0)Colony.Buildings[0].Inventory.FindOrAdd(P.Key)+=Need;}};
    const int32 SolarId=Colony.AddReviewBuilding(TEXT("solar_array"),Colony.Buildings[0].Position+FVector2D(1400,0));
    if(!TestTrue(TEXT("Fixture solar array exists"),SolarId>0))return false;
    Stock(Colony.BuildingDefs[TEXT("solar_array")].UpgradeCost);
    const double AlloyBefore=Colony.TotalStock(TEXT("alloy"));const int32 BodiesBefore=Colony.Workers.Bodies.Num();
    if(!TestTrue(TEXT("A spare level-2 bill starts one ordinary upgrade"),Brain.ManageUpgrades(Colony))){AddError(Brain.GetStatus());return false;}
    const FSeigeBuilding* Solar=Colony.FindBuilding(SolarId);
    TestEqual(TEXT("The array upgrades to the next level"),Solar->UpgradeTarget,FString(TEXT("solar_array_2")));
    TestTrue(TEXT("The upgrade is physical construction, not an instant swap"),Solar->IsConstructing&&Solar->DefId==TEXT("solar_array"));
    TestEqual(TEXT("Ordering an upgrade conserves stock"),Colony.TotalStock(TEXT("alloy")),AlloyBefore);
    TestEqual(TEXT("Ordering an upgrade adds no workers"),Colony.Workers.Bodies.Num(),BodiesBefore);
    TestFalse(TEXT("Only the configured number of upgrades runs at once"),Brain.ManageUpgrades(Colony));
    TestEqual(TEXT("An upgrading array still meets its plan entry"),Brain.CountLive(Colony,TEXT("solar_array")),1);

    // A finished level-2 array keeps satisfying the level-1 target, so the
    // ordered plan never rebuilds what it upgraded; the reverse never holds.
    FSeigeBuilding* Finished=Colony.FindBuilding(SolarId);
    Finished->DefId=TEXT("solar_array_2");Finished->UpgradeTarget.Empty();Finished->IsConstructing=false;Finished->ConstructionProgress=1;
    TestTrue(TEXT("Level 2 meets a level-1 plan entry"),Brain.Meets(Colony,*Finished,TEXT("solar_array")));
    TestEqual(TEXT("Counts follow the family, not the exact id"),Brain.CountLive(Colony,TEXT("solar_array")),1);
    Finished->DefId=TEXT("solar_array");
    TestFalse(TEXT("Level 1 does not meet a level-2 plan entry"),Brain.Meets(Colony,*Finished,TEXT("solar_array_2")));
    Finished->DefId=TEXT("solar_array_2");

    // Towers stop firing while they upgrade: none starts with an enemy inside
    // the quiet radius; the same tower upgrades once the approach is clear.
    Brain.MaxConcurrentUpgrades=4;
    const int32 TowerId=Colony.AddReviewBuilding(TEXT("turret"),Colony.Buildings[0].Position+FVector2D(-1400,0));
    Stock(Colony.BuildingDefs[TEXT("turret")].UpgradeCost);
    FSeigeEnemy Raider;Raider.Id=424242;Raider.Position=Colony.Buildings[0].Position+FVector2D(Brain.UpgradeQuietRadius*.5,0);Raider.Health=10;Colony.Enemies.Add(Raider);
    TestFalse(TEXT("No tower upgrade with an enemy inside the quiet radius"),Brain.ManageUpgrades(Colony));
    TestTrue(TEXT("The tower is still armed"),Colony.FindBuilding(TowerId)->UpgradeTarget.IsEmpty());
    Colony.Enemies.Empty();
    TestTrue(TEXT("The tower upgrades once the approach is quiet"),Brain.ManageUpgrades(Colony)&&Colony.FindBuilding(TowerId)->UpgradeTarget==TEXT("turret_2"));
    return true;
}
#endif
