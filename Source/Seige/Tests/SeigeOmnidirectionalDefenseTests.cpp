#include "Simulation/SeigeSimulation.h"
#include "SeigeHardpointLayout.h"
#include "SeigeTurretMount.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeOmniDefenseTest,"Seige.Combat.OmnidirectionalBuildingWeapons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeOmniDefenseTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation Base;
    if(!Base.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    for(int I=0;I<400&&Base.Buildings[0].IsConstructing;++I)Base.Tick(10);
    if(!TestFalse(TEXT("Fixture deploys with the real landed crew"),Base.Buildings[0].IsConstructing))return false;
    // Keep real bodies but move them clear of this authored weapon-range fixture.
    for(auto& W:Base.Workers.Bodies)W.Position=FVector2D(5000,5000);
    for(auto& V:Base.TransportVehicles.Bodies)V.Position=FVector2D(5000,-5000);
    for(auto& D:Base.Companions.Dogs)D.Position=FVector2D(-5000,-5000);
    TArray<FString> Platforms;Base.Combat.BuildingPlatforms.GetKeys(Platforms);Platforms.Sort();
    for(const auto& Id:Platforms)for(int32 Direction=0;Direction<8;++Direction)
    {
        FSeigeSimulation S=Base;const bool Core=S.BuildingDefs[Id].Role==TEXT("core");
        if(Core){S.Buildings[0].DefId=Id;S.Buildings[0].Health=S.BuildingDefs[Id].Health;}
        else
        {
            auto& CoreState=S.Combat.BuildingState[S.Buildings[0].Id];CoreState.Weapons.Empty();CoreState.Cooldowns.Empty();
            FSeigeBuilding B;B.Id=9000;B.DefId=Id;B.Position=FVector2D(1500,0);B.Health=S.BuildingDefs[Id].Health;B.ConstructionProgress=1;B.IsConstructing=false;S.Buildings.Add(B);
        }
        auto& B=Core?S.Buildings[0]:S.Buildings.Last();B.Workers=S.BuildingDefs[Id].Jobs;B.BatteryEnergyKWh=S.Energy.Definition(B.DefId)->BatteryCapacityKWh;
        S.Buildings[0].BatteryEnergyKWh=S.Energy.Definition(S.Buildings[0].DefId)->BatteryCapacityKWh;
        for(const auto& WeaponId:S.Combat.BuildingPlatforms[Id].Weapons)
        {auto& W=S.Combat.Weapons[WeaponId];W.AccuracyDegrees=0;if(!W.Ammo.IsEmpty())B.Inventory.Add(W.Ammo,W.AmmoPerShot*10);}
        S.Energy.Invalidate();S.Energy.Tick(S,0);S.Combat.Tick(S,.001);
        const double Angle=FMath::DegreesToRadians(Direction*45.);FSeigeEnemy Enemy;Enemy.Id=8099;Enemy.Health=100000;
        Enemy.Position=B.Position+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*(Core?1100:700);S.Enemies.Add(Enemy);
        const FString Label=FString::Printf(TEXT("%s bearing %d"),*Id,Direction*45);
        FVector2D Aim;TestTrue(Label+TEXT(" acquires before firing"),S.Combat.BuildingWeaponAim(S,B.Id,0,Aim));
        TestTrue(Label+TEXT(" aims at the actual target"),Aim.Equals(Enemy.Position));
        const double Shots=S.Combat.ShotsFired;
        double FlightSeconds=1.;
        for(const auto& WeaponId:S.Combat.BuildingPlatforms[Id].Weapons){const auto& W=S.Combat.Weapons[WeaponId];if(W.SpeedMetersSecond>0)FlightSeconds=FMath::Max(FlightSeconds,FVector2D::Distance(B.Position,Enemy.Position)*S.MetersPerWorldUnit()/W.SpeedMetersSecond+.25);}
        for(int32 Step=0;Step<FMath::CeilToInt(FlightSeconds/.05);++Step)S.Combat.Tick(S,.05);
        TestTrue(Label+TEXT(" fires all equipped modules without facing restriction"),S.Combat.ShotsFired-Shots>=S.Combat.BuildingPlatforms[Id].Weapons.Num());
        TestTrue(Label+TEXT(" actual beams/projectiles damage the target"),S.Enemies[0].Health<Enemy.Health);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeOmniMountGeometryTest,"Seige.Presentation.OmnidirectionalMountClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeOmniMountGeometryTest::RunTest(const FString&)
{
    const TArray<int32> Sizes={4,2,2,1,1,1,1,1,1,1,1};TArray<FIntVector> Cells;
    if(!TestTrue(TEXT("The unchanged two-bank starting loadout packs"),SeigePackHardpointBanks(Sizes,Cells)))return false;
    const double ShipScale=240.*2.*6./848.586975;
    for(int32 Degrees=0;Degrees<360;Degrees+=15)for(int32 I=0;I<Sizes.Num();++I)
    {
        const FVector Position=SeigeOrbitingBankMount(Cells[I],Sizes[I],ShipScale,Degrees);
        const FVector Direction=FRotator(0,Degrees,0).Vector();
        TestTrue(TEXT("Every module rides outside the pressure hull"),FVector2D(Position).Size()>278.*ShipScale);
        TestTrue(TEXT("Every firing ray points away from the hull in all azimuths"),FVector::DotProduct(Position,Direction)>278.*ShipScale);
        TestTrue(TEXT("The banks keep their authored vertical socket positions"),FMath::IsNearlyEqual(Position.Z,1120.*ShipScale+(Cells[I].Z+Sizes[I]*.5-2.)*50.,.001));
    }
    TestTrue(TEXT("Tower bearing clears the tallest authored level-three mast"),SeigeTowerBearingHeightCm>803.);
    for(int32 Yaw=0;Yaw<360;Yaw+=45)
    {
        const FTransform Barrel(FRotator(-90,0,0),FVector(35,0,0),FVector(.12,.12,.60));
        const FTransform Gimbal(FRotator(-25,Yaw,0),FVector(0,0,34));
        const FVector Muzzle=Gimbal.TransformPosition(Barrel.TransformPosition(FVector(0,0,50)));
        const FVector Forward=FRotator(-25,Yaw,0).Vector();
        TestTrue(TEXT("Beam starts at forward cylinder muzzle after arbitrary yaw and pitch"),(Muzzle-FVector(0,0,34)).GetSafeNormal().Equals(Forward,.0001));
    }
    return true;
}
#endif
