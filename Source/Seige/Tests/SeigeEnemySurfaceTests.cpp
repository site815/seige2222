#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeEnemySurfaceTest,"Seige.Combat.EnemyPursuitRespectsBuildingSurfaces",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeEnemySurfaceTest::RunTest(const FString&)
{
    FSeigeSimulation Base;FString Error;
    if(!Base.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false)){AddError(Error);return false;}
    for(int32 I=0;I<400&&Base.Buildings[0].IsConstructing;++I)Base.Tick(10);
    if(!TestFalse(TEXT("The surface fixture starts after ordinary shuttle deployment"),Base.Buildings[0].IsConstructing))return false;
    // Explicit geometry fixture: suppress unrelated defenders and moving crews.
    // The real starter budget and threat settings are exercised by FirstPlayable.
    Base.Environment.Enabled=false;Base.Buildings[0].Enabled=false;Base.Companions.Dogs.Empty();Base.Workers.Bodies.Empty();Base.Combat.Vehicles.Empty();Base.TransportVehicles.Bodies.Empty();
    const FVector2D Center(2000,0);const int32 Port=Base.AddReviewBuilding(TEXT("trading_port"),Center);
    const double Radius=Base.BuildingDefs[TEXT("trading_port")].Footprint+Base.Combat.EnemyBodyRadiusMeters()/Base.MetersPerWorldUnit();
    const double Step=.05,MaximumStep=Base.Number(TEXT("enemy_speed"))*Step;
    FSeigeWorker Worker;Worker.Id=TEXT("surface-test-worker");Worker.State=TEXT("active");Worker.Activity=TEXT("operate");Worker.Outdoor=true;Worker.Health=100;Worker.CrewBuildingId=Port;
    Worker.Position=Center+FVector2D(-288,11);Base.Workers.Bodies.Add(Worker);
    FSeigeEnemy Enemy;Enemy.Id=199;Enemy.Health=45;Enemy.Position=Center+FVector2D(-154,236);Base.Enemies.Add(Enemy);
    TestTrue(TEXT("The worker is nearer than the building center, reproducing corner-cut pursuit"),FVector2D::Distance(Enemy.Position,Worker.Position)<FVector2D::Distance(Enemy.Position,Center));
    FSeigeSimulation Pursuit=Base;bool StayedOutside=true,BoundedSpeed=true;
    for(int32 I=0;I<100;++I)
    {
        const FVector2D Before=Pursuit.Enemies[0].Position;Pursuit.Time+=Step;Pursuit.StepCombat(Step);
        const FVector2D Offset=Pursuit.Enemies[0].Position-Center;
        StayedOutside&=FMath::Max(FMath::Abs(Offset.X),FMath::Abs(Offset.Y))>=Radius-1.e-5;
        BoundedSpeed&=FVector2D::Distance(Before,Pursuit.Enemies[0].Position)<=MaximumStep+1.e-6;
    }
    TestTrue(TEXT("Pursuing a worker never cuts through the intervening structure"),StayedOutside);
    TestTrue(TEXT("Surface contact never moves an enemy faster than its authored speed"),BoundedSpeed);
    TestTrue(TEXT("The blocking structure takes the ordinary melee damage"),Pursuit.FindBuilding(Port)->Health<Base.FindBuilding(Port)->Health);
    TestEqual(TEXT("The worker behind the obstruction is not attacked through the wall"),Pursuit.Workers.Bodies[0].Health,100.);

    FSeigeSimulation Legacy=Base;Legacy.Enemies[0].Position=Center+FVector2D(72,-198);
    const FVector2D Embedded=Legacy.Enemies[0].Position;Legacy.StepCombat(Step);
    TestTrue(TEXT("A saved overlap walks outward instead of teleporting to the surface"),FVector2D::Distance(Embedded,Legacy.Enemies[0].Position)>0&&FVector2D::Distance(Embedded,Legacy.Enemies[0].Position)<=MaximumStep+1.e-6);
    TestTrue(TEXT("One short step cannot skip the actual remaining distance to safety"),FMath::Abs(Legacy.Enemies[0].Position.Y-Center.Y)<Radius);
    for(int32 I=0;I<100;++I){Legacy.Time+=Step;Legacy.StepCombat(Step);}
    const FVector2D Recovered=Legacy.Enemies[0].Position-Center;
    TestTrue(TEXT("The original living enemy physically exits the legacy overlap"),Legacy.Enemies.Num()==1&&Legacy.Enemies[0].Id==199&&Legacy.Enemies[0].Health==45&&FMath::Max(FMath::Abs(Recovered.X),FMath::Abs(Recovered.Y))>=Radius-1.e-5);

    FSeigeSimulation Placement=Base;Placement.Buildings[0].Enabled=true;Placement.Enemies.Empty();const FVector2D Plot(1300,900);
    if(!TestTrue(TEXT("The unoccupied comparison plot is otherwise legal"),Placement.CanPlaceBuilding(TEXT("turret"),Plot,Error))){AddError(Error);return false;}
    Enemy.Position=Plot;Placement.Enemies.Add(Enemy);
    TestFalse(TEXT("New construction cannot enclose a visible living enemy"),Placement.CanPlaceBuilding(TEXT("turret"),Plot,Error));
    TestTrue(TEXT("The occupancy refusal explains the actual obstruction"),Error.Contains(TEXT("enemy")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeConstructionCoverTest,"Seige.Combat.ConstructionCoverMatchesEnclosure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeConstructionCoverTest::RunTest(const FString&)
{
    FSeigeSimulation Base;FString Error;
    if(!Base.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false)){AddError(Error);return false;}
    for(int32 I=0;I<400&&Base.Buildings[0].IsConstructing;++I)Base.Tick(10);
    if(!TestFalse(TEXT("Cover fixture starts after paid deployment"),Base.Buildings[0].IsConstructing))return false;
    const int32 Core=Base.Buildings[0].Id;const FVector2D Origin=Base.Buildings[0].Position;
    for(auto& W:Base.Workers.Bodies)W.Position=Origin+FVector2D(5000,5000);
    for(auto& V:Base.TransportVehicles.Bodies)V.Position=Origin+FVector2D(5000,-5000);
    for(auto& D:Base.Companions.Dogs)D.Position=Origin+FVector2D(-5000,-5000);
    Base.Buildings[0].Workers=Base.Definition(Base.Buildings[0])->Jobs;
    Base.Buildings[0].BatteryEnergyKWh=Base.Energy.Definition(Base.Buildings[0].DefId)->BatteryCapacityKWh;
    for(auto& W:Base.Combat.Weapons)W.Value.AccuracyDegrees=0;
    Base.Energy.Invalidate();Base.Energy.Tick(Base,0);
    const int32 Site=Base.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(600,0));
    Base.FindBuilding(Site)->IsConstructing=true;Base.FindBuilding(Site)->ConstructionProgress=0;
    FSeigeEnemy Enemy;Enemy.Id=8099;Enemy.Health=100000;Enemy.Position=Origin+FVector2D(1100,0);Base.Enemies.Add(Enemy);
    for(double Progress:{0.,.1,.4,.7,1.})
    {
        FSeigeSimulation S=Base;auto* B=S.FindBuilding(Site);B->ConstructionProgress=Progress;B->IsConstructing=Progress<1;
        const bool Opaque=Progress>=.7;const FString Label=FString::Printf(TEXT("Construction %.0f%%"),Progress*100);
        TestEqual(Label+TEXT(" derives opaque cover from the rendered enclosure phase"),S.BuildingBlocksProjectiles(*B),Opaque);
        FVector2D Aim;TestEqual(Label+TEXT(" acquisition matches the visible shell"),S.Combat.BuildingWeaponAim(S,Core,0,Aim),!Opaque);
        const double Health=B->Health,Shots=S.Combat.ShotsFired;
        S.Combat.Tick(S,.1);
        TestEqual(Label+TEXT(" actual defensive fire agrees with acquisition"),S.Combat.ShotsFired>Shots,!Opaque);
        TestEqual(Label+TEXT(" actual projectiles reach the enemy only through the open site"),S.Enemies[0].Health<Enemy.Health,!Opaque);
        TestEqual(Label+TEXT(" defensive fire does not hit a nonexistent finished shell"),S.FindBuilding(Site)->Health,Health);
    }
    // An open site is still destructible: aimed hostile fire and splash target
    // its foundations/materials instead of granting construction invulnerability.
    FSeigeSimulation Attack=Base;Attack.Buildings[0].Enabled=false;Attack.Enemies.Empty();
    const auto& W=Attack.Combat.Weapons[TEXT("bug_plasma")];FSeigeProjectile P;
    P.Id=99001;P.OwnerId=99002;P.OwnerKind=TEXT("enemy");P.TargetKind=TEXT("building");P.TargetEnemyId=Site;P.WeaponId=W.Id;
    P.Position=P.PreviousPosition=Origin+FVector2D(1100,0);P.Velocity=FVector2D(-1,0)*W.SpeedMetersSecond/Attack.MetersPerWorldUnit();P.RemainingMeters=W.RangeMeters;
    Attack.Combat.Projectiles.Add(P);const double Health=Attack.FindBuilding(Site)->Health;
    for(int32 I=0;I<200&&!Attack.Combat.Projectiles.IsEmpty();++I)Attack.Combat.Tick(Attack,.05);
    TestTrue(TEXT("Deliberately aimed plasma still damages an unfinished foundation"),Attack.FindBuilding(Site)->Health<Health);
    FSeigeBuilding Shuttle=Base.Buildings[0];Shuttle.IsConstructing=true;Shuttle.ConstructionProgress=0;
    TestTrue(TEXT("The intact deploying shuttle never becomes transparent"),Base.BuildingBlocksProjectiles(Shuttle));
    return true;
}
#endif
