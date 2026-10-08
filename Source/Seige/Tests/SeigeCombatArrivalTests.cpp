#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeDefensiveCornerArrivalTest,"Seige.Combat.DefensiveCornerArrival",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeDefensiveCornerArrivalTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    // Explicit combat geometry, using one real carried guard. Isolate the
    // narrow arrival condition from terrain, hostile motion and firing spread.
    S.Environment.Enabled=false;S.Combat.Vehicles.SetNum(1);S.Buildings[0].Enabled=false;
    auto& Guard=S.Combat.Vehicles[0];Guard.Embarked=false;Guard.SectorIndex=4;Guard.Route.Empty();Guard.NextWaypoint=0;
    const double Unit=S.MetersPerWorldUnit();const auto& Chassis=S.Combat.Chassis[Guard.ChassisId];
    auto& Weapon=S.Combat.Weapons[Guard.Weapons[0]];Weapon.AccuracyDegrees=0;
    FSeigeBuilding Cover;Cover.Id=990001;Cover.DefId=TEXT("solar_array");Cover.Position=S.Buildings[0].Position+FVector2D(6000,0);Cover.Health=S.BuildingDefs[Cover.DefId].Health;Cover.ConstructionProgress=1;
    S.Buildings.Add(Cover);
    const double Half=S.Definition(Cover)->Footprint,Clearance=Chassis.RadiusMeters/Unit;
    const FVector2D Corner=Cover.Position+FVector2D(Half+Clearance+.001,Half+Clearance+.001);
    const FVector2D Start=Corner-FVector2D(0,.7/Unit);
    Guard.Position=Guard.Destination=Start;Guard.Status=TEXT("On station");Guard.DistanceMeters=0;Guard.Cooldowns.Init(0,Guard.Weapons.Num());
    FSeigeEnemy Enemy;Enemy.Id=990099;Enemy.Health=45;Enemy.Position=Cover.Position+FVector2D(-20/Unit,Half-.2/Unit);S.Enemies.Add(Enemy);
    const double BeforeBattery=Guard.BatteryKWh,BeforeFriendlyHealth=Cover.Health,BeforeEnemyHealth=Enemy.Health,BeforeShots=S.Combat.ShotsFired;
    TestTrue(TEXT("The clear obstacle corner lies inside ordinary one-metre station tolerance"),FVector2D::Distance(Start,Corner)*Unit<1);
    TestTrue(TEXT("The current guard sensor detects the real enemy"),S.IsVisible(Enemy.Position));
    if(!S.Combat.OrderFleet(S,Guard.FleetId,TEXT("move"),Start,0,Error)||!S.Combat.SetAggression(Guard.FleetId,TEXT("defensive"),Error)){AddError(Error);return false;}
    S.Combat.Tick(S,.05);
    TestEqual(TEXT("Actual autonomous fire control rejects the occluded current muzzle"),S.Combat.ShotsFired,BeforeShots);
    TestTrue(TEXT("The ordinary station order stays at its position"),Guard.Position.Equals(Start,1.e-8));
    TestEqual(TEXT("Holding a blocked shot pays no movement or weapon energy"),Guard.BatteryKWh,BeforeBattery);

    FSeigeSimulation Ordinary=S;Ordinary.Enemies.Empty();auto& OrdinaryGuard=Ordinary.Combat.Vehicles[0];
    if(!Ordinary.Combat.OrderFleet(Ordinary,OrdinaryGuard.FleetId,TEXT("move"),Corner,0,Error)){AddError(Error);return false;}
    Ordinary.Combat.Tick(Ordinary,.05);
    TestTrue(TEXT("Ordinary nondefensive station tolerance is unchanged"),OrdinaryGuard.Position.Equals(Start,1.e-8));

    if(!S.Combat.OrderFleet(S,Guard.FleetId,TEXT("defense"),Start,0,Error)){AddError(Error);return false;}
    const double Step=.01;const FVector2D BeforeMovement=Guard.Position;
    S.Combat.Tick(S,Step);
    const double FirstMove=FVector2D::Distance(BeforeMovement,Guard.Position)*Unit;
    TestTrue(TEXT("Defensive arrival makes a real sub-metre step instead of clearing its route"),FirstMove>0&&FirstMove<=Chassis.SpeedKmh/3.6*Step+1.e-6);
    TestEqual(TEXT("The first small step has not prematurely shot through the friendly building"),S.Combat.ShotsFired,BeforeShots);
    for(int32 I=0;I<200&&S.Combat.ShotsFired==BeforeShots;++I)S.Combat.Tick(S,Step);
    TestTrue(TEXT("The guard reaches actual firing geometry and fires normally"),S.Combat.ShotsFired>BeforeShots&&S.Enemies[0].Health<BeforeEnemyHealth);
    TestTrue(TEXT("Flanking remains a bounded physical movement, not a teleport"),Guard.DistanceMeters>0&&Guard.DistanceMeters<=.7+1.e-5&&FMath::IsNearlyEqual(Guard.DistanceMeters,FVector2D::Distance(Start,Guard.Position)*Unit,1.e-6));
    const double ShotCount=S.Combat.ShotsFired-BeforeShots;
    const double ExpectedEnergy=Guard.DistanceMeters*Chassis.TravelKWhPerKm/1000+ShotCount*Weapon.EnergyKWh;
    TestTrue(TEXT("Both travel and the real shot draw their authored finite battery cost"),FMath::IsNearlyEqual(BeforeBattery-Guard.BatteryKWh,ExpectedEnergy,1.e-7));
    TestEqual(TEXT("The friendly building is preserved throughout the flanking shot"),S.FindBuilding(Cover.Id)->Health,BeforeFriendlyHealth);
    TestEqual(TEXT("The original living guard remains the same physical vehicle"),S.Combat.Vehicles.Num(),1);
    return true;
}
#endif
