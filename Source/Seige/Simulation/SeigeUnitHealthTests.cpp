#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags HealthFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
bool HealthStart(FSeigeSimulation& S,FAutomationTestBase& T)
{FString Error;if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){T.AddError(Error);return false;}return true;}
void ExposeWorker(FSeigeWorker& W,FVector2D P)
{W.State=TEXT("active");W.Activity=TEXT("idle");W.Position=P;W.Outdoor=true;W.ContainerId=W.BuildingId=W.RoadId=W.DeliveryId=W.TransportVehicleId=W.ChargingBuildingId=0;W.ContainerKind.Empty();W.SuspendedActivity.Empty();W.Route.Empty();W.NextWaypoint=0;}
void Shoot(FSeigeSimulation& S,FVector2D From,const FString& Weapon)
{FSeigeProjectile P;P.Id=99999;P.OwnerId=99998;P.OwnerKind=TEXT("enemy");P.WeaponId=Weapon;P.Position=P.PreviousPosition=From;P.Velocity=FVector2D(S.Combat.Weapons[Weapon].SpeedMetersSecond/S.MetersPerWorldUnit(),0);P.RemainingMeters=100;S.Combat.Projectiles.Add(P);for(int32 Step=0;Step<200&&!S.Combat.Projectiles.IsEmpty();++Step)S.Combat.Tick(S,.05);}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeUnitPhysicalDamageTest,"Seige.Units.WorkerAndDogPhysicalDamage",HealthFlags)
bool FSeigeUnitPhysicalDamageTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!HealthStart(S,*this))return false;S.Buildings[0].Enabled=false;S.Combat.Vehicles.Empty();
    auto& Dog=S.Companions.Dogs[0];Dog.Aboard=false;Dog.Position=FVector2D(3000,3000);
    auto& W=S.Workers.Bodies[0];ExposeWorker(W,Dog.Position+FVector2D(0,1/S.MetersPerWorldUnit()));
    auto& K=S.Combat.Weapons[TEXT("kinetic_small")];K.Damage=5;K.SplashMeters=0;
    Shoot(S,Dog.Position-FVector2D(100,0),K.Id);
    TestEqual(TEXT("Swept projectile hits Rex's actual body"),Dog.Health,S.Companions.MaximumHealth-5);
    TestEqual(TEXT("A non-splash hit does not damage adjacent worker"),W.Health,S.Workers.MaximumHealth());
    auto& P=S.Combat.Weapons[TEXT("plasma_small")];P.Damage=10;P.SplashMeters=5;
    Shoot(S,Dog.Position-FVector2D(100,0),P.Id);
    TestTrue(TEXT("Splash also damages a nearby physical worker"),W.Health<S.Workers.MaximumHealth());
    TestTrue(TEXT("Direct and splash dog damage share the same HP ledger"),Dog.Health<S.Companions.MaximumHealth-5);
    const double Health=W.Health;Dog.Sheltered=true;Dog.Retreating=false;Dog.Position=S.Workers.HatchPoint(S);
    K.Damage=2;Shoot(S,W.Position-FVector2D(100,0),K.Id);
    TestEqual(TEXT("Worker direct hit removes configured HP"),W.Health,Health-2);
    const auto DogHealth=Dog.Health;S.Companions.Damage(S,Dog.Id,1000);
    TestEqual(TEXT("Sheltered body is excluded from damage"),Dog.Health,DogHealth);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerHealthPersistenceTest,"Seige.Units.WorkerHealthPersistenceAndDeath",HealthFlags)
bool FSeigeWorkerHealthPersistenceTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!HealthStart(S,*this))return false;auto& W=S.Workers.Bodies[0];const FString Id=W.Id;ExposeWorker(W,FVector2D(2000,2000));
    S.Workers.Damage(S,Id,25);TestEqual(TEXT("Surviving damage retains a unique live body"),W.Health,S.Workers.MaximumHealth()-25);
    const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/worker-health-v9.json"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);FString Error;
    TestTrue(TEXT("Wounded worker saves"),S.Save(Path,Error));FSeigeSimulation Loaded;if(!HealthStart(Loaded,*this))return false;
    TestTrue(TEXT("Wounded worker loads"),Loaded.Load(Path,Error));if(!Error.IsEmpty())AddError(Error);
    if(const auto* Body=Loaded.Workers.Find(Id))TestEqual(TEXT("Load retains exact worker HP"),Body->Health,W.Health);else AddError(TEXT("Reload lost worker identity"));
    S.Workers.Damage(S,Id,1000);TestTrue(TEXT("Lethal worker hit destroys one body"),W.State==TEXT("destroyed")&&W.Health==0&&S.Population==4);
    TestTrue(TEXT("Dead worker saves"),S.Save(Path,Error));TestTrue(TEXT("Dead worker reloads without regeneration"),Loaded.Load(Path,Error));
    if(const auto* Body=Loaded.Workers.Find(Id))TestTrue(TEXT("No resurrection or duplicate after load"),Body->Health==0&&Body->State==TEXT("destroyed")&&Loaded.Workers.Bodies.Num()==S.Workers.Bodies.Num());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerCargoDamageTest,"Seige.Units.DeadCourierAndLoaderConservation",HealthFlags)
bool FSeigeWorkerCargoDamageTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!HealthStart(S,*this))return false;auto& W=S.Workers.Bodies[0];ExposeWorker(W,FVector2D(1000,1000));
    FSeigeCourier C;C.Id=10001;C.SourceId=C.TargetId=S.Buildings[0].Id;C.Resource=TEXT("alloy");C.Amount=10;C.WorkerId=W.Id;C.Position=W.Position;C.Phase=TEXT("carrying");S.Couriers.Add(C);W.Activity=TEXT("delivery");W.DeliveryId=C.Id;
    const int32 Lost=S.LostCouriers;S.Workers.Damage(S,W.Id,1000);
    TestTrue(TEXT("Dead courier relinquishes its real cargo task once"),S.Couriers.IsEmpty()&&W.DeliveryId==0&&S.LostCouriers==Lost+1);
    S.Workers.Damage(S,W.Id,1000);TestEqual(TEXT("Duplicate damage cannot double-count lost cargo"),S.LostCouriers,Lost+1);
    if(!S.TransportVehicles.Bodies.IsEmpty())
    {
        auto& Loader=S.Workers.Bodies[1];ExposeWorker(Loader,FVector2D(2000,2000));auto& V=S.TransportVehicles.Bodies[0];
        Loader.Activity=TEXT("handling");Loader.TransportVehicleId=V.Id;V.HandlerWorkerId=Loader.Id;const double Before=V.Health;
        S.Workers.Damage(S,Loader.Id,1000);TestTrue(TEXT("Dead loader releases a surviving transport's handler claim"),V.HandlerWorkerId.IsEmpty()&&V.Health==Before&&Loader.TransportVehicleId==0);
    }
    return true;
}
#endif
