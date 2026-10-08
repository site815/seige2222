#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeDefensiveChargingReserveTest,"Seige.Combat.DefensiveEnergyBeforeVehicleCharging",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeDefensiveChargingReserveTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation Base;
    if(!Base.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    for(int32 I=0;I<400&&Base.Buildings[0].IsConstructing;++I)Base.Tick(10);
    if(!TestFalse(TEXT("Real landed crew completes the fixture core"),Base.Buildings[0].IsConstructing))return false;
    // Isolate electrical dispatch from production and worker travel. Reassign
    // existing bodies; no new workers, gameplay materials or generation rates.
    const int32 CoreId=Base.Buildings[0].Id;
    for(auto& Body:Base.Workers.Bodies)if(Body.State==TEXT("active"))
    {Body.Activity=TEXT("operate");Body.BuildingId=CoreId;Body.RoadId=Body.DeliveryId=Body.ContainerId=0;Body.Route.Empty();Body.NextWaypoint=0;Body.Position=Base.BuildingAccessPoint(Base.Buildings[0]);}
    Base.Workers.RefreshMetrics(Base);Base.Calendar.SetElapsedMicroseconds(2700000000);
    Base.Combat.Vehicles.SetNum(1);auto& Vehicle=Base.Combat.Vehicles[0];Vehicle.Embarked=false;Vehicle.FleetId=0;Vehicle.Weapons.Empty();Vehicle.Cooldowns.Empty();Vehicle.BatteryKWh=0;Vehicle.Position=Base.BuildingAccessPoint(Base.Buildings[0])+FVector2D(0,100);
    Vehicle.Shield=Base.Combat.Chassis[Vehicle.ChassisId].Shield;
    auto& Platform=Base.Combat.BuildingState[CoreId];Platform.Weapons={TEXT("laser_small")};Platform.Cooldowns={0};Platform.LastDamageTime=Base.Time;
    Base.Buildings[0].BatteryEnergyKWh=0;Base.Energy.Invalidate();Base.Energy.Tick(Base,0);
    const double Step=Base.FixedStepSeconds(),ShotCost=Base.Combat.Weapons[TEXT("laser_small")].EnergyKWh;
    auto Advance=[&](FSeigeSimulation& S){S.Time+=Step;S.Energy.Tick(S,Step);S.Combat.Tick(S,Step);S.Calendar.Advance(Step);};
    auto AddThreat=[&](FSeigeSimulation& S){FSeigeEnemy Enemy;Enemy.Id=8099;Enemy.Health=10000;Enemy.Position=S.Buildings[0].Position+FVector2D(-1000,0);S.Enemies.Add(Enemy);};

    FSeigeSimulation Defense=Base;AddThreat(Defense);const double Shots=Defense.Combat.ShotsFired,Spent=Defense.Combat.EnergySpentKWh;
    bool Accumulated=false;
    for(int32 I=0;I<400&&Defense.Combat.ShotsFired==Shots;++I)
    {Advance(Defense);Accumulated|=Defense.Energy.Info(Defense,CoreId).StoredKWh>0&&Defense.Combat.ShotsFired==Shots;}
    TestTrue(TEXT("Nighttime fusion accumulates a shot instead of feeding smaller charge withdrawals"),Accumulated);
    TestEqual(TEXT("A valid defensive target receives one actual paid shot"),Defense.Combat.ShotsFired,Shots+1);
    TestTrue(TEXT("Shot deducts the exact external weapon electricity"),FMath::IsNearlyEqual(Defense.Combat.EnergySpentKWh-Spent,ShotCost,1.e-9));
    TestEqual(TEXT("Insufficient surplus does not charge the docked vehicle before its defended shot"),Defense.Combat.Vehicles[0].BatteryKWh,0.);

    FSeigeSimulation Shield=Base;AddThreat(Shield);Shield.Buildings[0].BatteryEnergyKWh=ShotCost;
    auto& ShieldState=Shield.Combat.BuildingState[CoreId];ShieldState.Shield-=1;ShieldState.LastDamageTime=-1;
    const double BeforeShield=ShieldState.Shield;Shield.Time+=Step;Shield.Energy.Tick(Shield,0);Shield.Combat.Tick(Shield,Step);
    TestEqual(TEXT("Shield recovery cannot consume the last charged defensive shot"),Shield.Combat.ShotsFired,Shots+1);
    TestEqual(TEXT("Pending shot takes priority over optional shield regeneration"),ShieldState.Shield,BeforeShield);
    TestTrue(TEXT("Shot consumes exactly the finite stored charge"),FMath::IsNearlyZero(Shield.Energy.Info(Shield,CoreId).StoredKWh,1.e-9));

    FSeigeSimulation Quiet=Base;Quiet.Buildings[0].BatteryEnergyKWh=ShotCost*2;Advance(Quiet);
    TestTrue(TEXT("No detected threat leaves ordinary vehicle charging available"),Quiet.Combat.Vehicles[0].BatteryKWh>0);
    FSeigeSimulation Blocked=Base;AddThreat(Blocked);Blocked.Buildings[0].BatteryEnergyKWh=ShotCost*2;
    FSeigeBuilding Obstruction;Obstruction.Id=900001;Obstruction.DefId=TEXT("sensor");Obstruction.Position=Blocked.Buildings[0].Position+FVector2D(-600,0);Obstruction.Health=Blocked.BuildingDefs[Obstruction.DefId].Health;Obstruction.IsConstructing=false;Obstruction.ConstructionProgress=1;Blocked.Buildings.Add(Obstruction);Blocked.Energy.Invalidate();Advance(Blocked);
    TestEqual(TEXT("Known friendly obstruction prevents the shot"),Blocked.Combat.ShotsFired,Shots);
    TestTrue(TEXT("An obstructed target does not reserve energy from charging"),Blocked.Combat.Vehicles[0].BatteryKWh>0);
    FSeigeSimulation Reloading=Base;AddThreat(Reloading);Reloading.Buildings[0].BatteryEnergyKWh=ShotCost*2;
    Reloading.Combat.BuildingState[CoreId].Cooldowns[0]=Reloading.Combat.Weapons[TEXT("laser_small")].ReloadSeconds;Advance(Reloading);
    TestEqual(TEXT("Not-yet-due weapons create no reserve"),Reloading.Energy.ReservedForDefense(CoreId),0.);
    TestTrue(TEXT("Reloading leaves surplus charging available"),Reloading.Combat.Vehicles[0].BatteryKWh>0);


    // Explicit disconnected electrical fixture: a completed factory and battery
    // on their own road. Its finite charge must not be held for the home gun.
    FSeigeSimulation Separate=Base;AddThreat(Separate);
    FSeigeBuilding Factory;Factory.Id=900002;Factory.DefId=TEXT("vehicle_factory");Factory.Position=FVector2D(8000,5000);Factory.Health=Separate.BuildingDefs[Factory.DefId].Health;Factory.IsConstructing=false;Factory.ConstructionProgress=1;
    FSeigeBuilding Battery;Battery.Id=900003;Battery.DefId=TEXT("battery_bank");Battery.Position=FVector2D(9000,5000);Battery.Health=Separate.BuildingDefs[Battery.DefId].Health;Battery.IsConstructing=false;Battery.ConstructionProgress=1;Battery.BatteryEnergyKWh=1;
    Separate.Buildings.Add(Factory);Separate.Buildings.Add(Battery);
    FSeigeTransportSegment Road;Road.Id=900004;Road.A=Separate.BuildingAccessPoint(Factory);Road.B=Separate.BuildingAccessPoint(Battery);Road.Tier=Road.TargetTier=TEXT("road");Road.Health=Road.MaxHealth=1000;Road.IsConstructing=false;Road.ConstructionProgress=1;Separate.Roads.Add(Road);
    auto RemoteVehicle=Separate.Combat.Vehicles[0];RemoteVehicle.Id=900005;RemoteVehicle.Position=Separate.BuildingAccessPoint(Factory);Separate.Combat.Vehicles.Add(RemoteVehicle);
    Separate.Energy.Invalidate();Advance(Separate);
    TestNotEqual(TEXT("Fixture service points really belong to different power components"),Separate.Energy.Info(Separate,CoreId).ComponentId,Separate.Energy.Info(Separate,Factory.Id).ComponentId);
    TestEqual(TEXT("Home reserve waits for actual generation"),Separate.Combat.Vehicles[0].BatteryKWh,0.);
    TestTrue(TEXT("Unrelated disconnected battery still pays for its local vehicle charging"),Separate.Combat.Vehicles[1].BatteryKWh>0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeDefensiveTransactionsTest,"Seige.Combat.DefensiveEnergyTransactionsAndPersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeDefensiveTransactionsTest::RunTest(const FString&)
{
    const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));FString Error;FSeigeSimulation S,Loaded;
    if(!S.Initialize(Rules,Error,false,false)||!S.SetWorkerSurplusTarget(0,Error)){AddError(Error);return false;}
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("Real starter workers finish deployment before the transaction fixture"),S.Buildings[0].IsConstructing))return false;
    S.Tick(20);const int32 CoreId=S.Buildings[0].Id;const FString Recipe=TEXT("smelt_alloy");
    // Deployment may already have committed a worker batch against temporary
    // construction demand. Finish its paid work; do not erase escrow or refund it.
    for(int32 I=0;I<80000&&S.Buildings[0].ProductionCommitted;++I)S.Tick(S.FixedStepSeconds());
    if(!TestFalse(TEXT("The earlier paid deployment batch finishes through normal production"),S.Buildings[0].ProductionCommitted))return false;
    if(!S.SetProductionRecipe(CoreId,Recipe,Error)){AddError(Error);return false;}
    // Controlled electrical starting condition, retaining real bodies, stock,
    // normal recipe transactions and a legitimately allocated threat ID.
    S.PeriodicAttacksEnabled=true;S.TriggerWave();S.PeriodicAttacksEnabled=false;
    if(!TestTrue(TEXT("Configured wave supplies a real enemy"),S.Enemies.Num()>0))return false;
    S.Enemies.SetNum(1);S.Enemies[0].Position=S.Buildings[0].Position+FVector2D(-1500,0);
    auto& Platform=S.Combat.BuildingState[CoreId];Platform.Weapons={TEXT("laser_small")};Platform.Cooldowns={0};Platform.LastDamageTime=S.Time;
    const double Step=S.FixedStepSeconds(),Shot=S.Combat.Weapons[TEXT("laser_small")].EnergyKWh;
    const double Batch=S.ProductionEnergy(S.Buildings[0],Recipe);const auto Inputs=S.ProductionInputs(S.Buildings[0],Recipe);
    S.Buildings[0].BatteryEnergyKWh=Batch;S.Energy.Invalidate();S.Energy.Tick(S,0);
    TestTrue(TEXT("Batch is otherwise affordable with this finite battery"),S.Energy.CanConsume(S,CoreId,Batch));
    S.Energy.RefreshDefensiveReserve(S,Step);
    TestTrue(TEXT("Reserve is the actual due weapon bill"),FMath::IsNearlyEqual(S.Energy.ReservedForDefense(CoreId),Shot,1.e-9));
    TestFalse(TEXT("Production cannot claim electricity reserved for the pending valid shot"),S.Energy.CanConsume(S,CoreId,Batch));
    TestTrue(TEXT("Reserved energy remains spendable by actual defensive firing"),S.Energy.CanConsume(S,CoreId,Shot,ESeigeEnergyPurpose::DefensiveShot));

    const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Combat/defensive-reserve.json"));
    if(!S.Save(Path,Error)||!Loaded.Initialize(Rules,Error,false,false)||!Loaded.Load(Path,Error)){AddError(Error);return false;}
    FString Raw;TSharedPtr<FJsonObject> Document;
    if(!FFileHelper::LoadFileToString(Raw,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Document)){AddError(TEXT("Unable to inspect saved reserve fixture"));return false;}
    TestEqual(TEXT("Energy saves contain only the three physical ledger totals, never a second reserve balance"),Document->GetObjectField(TEXT("energy"))->Values.Num(),3);
    TestTrue(TEXT("Loading reconstructs reserve from current targets, work and cooldowns"),FMath::IsNearlyEqual(Loaded.Energy.ReservedForDefense(CoreId),Shot,1.e-9));
    TestEqual(TEXT("Reconstructing reserve consumes no extra energy"),Loaded.Energy.ConsumedKWh,S.Energy.ConsumedKWh);
    const double Iron=S.Buildings[0].Inventory.FindRef(TEXT("iron_ore")),Shots=S.Combat.ShotsFired,CombatSpent=S.Combat.EnergySpentKWh;
    S.Tick(Step);Loaded.Tick(Step);
    TestFalse(TEXT("Actual production step waits without claiming an underfunded batch"),S.Buildings[0].ProductionCommitted);
    TestEqual(TEXT("Waiting recipe retains its actual local inputs"),S.Buildings[0].Inventory.FindRef(TEXT("iron_ore")),Iron);
    TestEqual(TEXT("Actual simulation step fires its paid defensive shot"),S.Combat.ShotsFired,Shots+1);
    TestTrue(TEXT("Combat ledger records only the authored shot bill"),FMath::IsNearlyEqual(S.Combat.EnergySpentKWh-CombatSpent,Shot,1.e-9));
    TestEqual(TEXT("Load continuation fires identically"),Loaded.Combat.ShotsFired,S.Combat.ShotsFired);
    TestTrue(TEXT("Load continuation preserves physical battery and energy ledger"),FMath::IsNearlyEqual(Loaded.Energy.Info(Loaded,CoreId).StoredKWh,S.Energy.Info(S,CoreId).StoredKWh,1.e-9)&&FMath::IsNearlyEqual(Loaded.Energy.ConsumedKWh,S.Energy.ConsumedKWh,1.e-9));

    S.Enemies.Empty();S.Buildings[0].BatteryEnergyKWh=Batch;const double BeforeConsumed=S.Energy.ConsumedKWh;
    const double BeforeStored=S.Energy.Info(S,CoreId).StoredKWh,BeforeGenerated=S.Energy.GeneratedKWh,BeforeSpilled=S.Energy.SpilledKWh;
    S.Tick(Step);
    TestEqual(TEXT("A quiet step releases the derived reservation"),S.Energy.ReservedForDefense(CoreId),0.);
    TestTrue(TEXT("Normal recipe commits when its real local bill is affordable"),S.Buildings[0].ProductionCommitted);
    TestEqual(TEXT("Committed recipe deducts exactly its configured iron input"),S.Buildings[0].Inventory.FindRef(TEXT("iron_ore")),Iron-Inputs.FindRef(TEXT("iron_ore")));
    TestTrue(TEXT("Recipe energy plus ordinary passive use is charged"),S.Energy.ConsumedKWh-BeforeConsumed>=Batch);
    TestTrue(TEXT("Stored plus generated equals consumed, remaining and spilled electricity"),FMath::IsNearlyEqual(BeforeStored+S.Energy.GeneratedKWh-BeforeGenerated,S.Energy.Info(S,CoreId).StoredKWh+S.Energy.ConsumedKWh-BeforeConsumed+S.Energy.SpilledKWh-BeforeSpilled,1.e-8));
    return true;
}
#endif
