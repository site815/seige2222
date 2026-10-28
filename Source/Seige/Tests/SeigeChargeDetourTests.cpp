#include "Simulation/SeigeSimulation.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeChargeDetourTest,"Seige.Workers.ChargingPlansForRealDetours",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeChargeDetourTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,108)){AddError(Error);return false;}
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("Normal finite workers completed landing"),S.Buildings[0].IsConstructing))return false;
    // A geometry-only installed scenario: the real worker stands across a
    // long ridge. It can return on its full battery, but not after waiting
    // until a misleading short straight-line estimate says charging is due.
    const FVector2D Shift=FVector2D(-1000,0)-S.Buildings[0].Position;
    S.Buildings[0].Position+=Shift;for(auto& Body:S.Workers.Bodies)Body.Position+=Shift;
    S.Environment.Enabled=true;S.Environment.WorldOffset=FVector2D::ZeroVector;
    S.Environment.River.Empty();S.Environment.AdditionalRivers.Empty();S.Environment.Fords.Empty();S.Environment.Cliffs.Empty();S.Environment.Lakes.Empty();
    FSeigeLake Lake;Lake.Center=FVector2D(0,14500);Lake.Radii=FVector2D(300,300);Lake.BuildShoreline();S.Environment.Lakes.Add(Lake);
    auto TerrainSamples=MakeShared<int64>(0);
    S.Mobility.SetTerrainSampler([TerrainSamples](FVector2D P){++*TerrainSamples;return FMath::Abs(P.Y)<12000?500.*FMath::Max(0.,1.-FMath::Abs(P.X)/100.):0.;});
    auto& W=S.Workers.Bodies.Last();const FString WorkerId=W.Id;
    W.State=TEXT("active");W.Activity=TEXT("idle");W.Position=FVector2D(1000,0);W.CrewBuildingId=W.BuildingId=W.ContainerId=W.DeliveryId=W.TransportVehicleId=0;W.ContainerKind.Empty();W.Outdoor=true;W.Route.Empty();W.NextWaypoint=0;W.ChargingBuildingId=0;
    W.BatteryKWh=W.BatteryCapacityKWh*.9;
    const double Day=double(S.Calendar.GetRules().DaylightMicroseconds+S.Calendar.GetRules().NightMicroseconds)/1.e6;
    const double Session=S.Workers.ChargeSessionSeconds(S),Drain=S.Workers.BatteryCapacity()/(Day-Session);
    const double Threshold=S.Workers.Number(TEXT("charge_reserve_fraction"))+S.Workers.Number(TEXT("charge_reserve_stagger_fraction"))*(GetTypeHash(W.Id)%101)/100.;
    const double OldTrigger=W.BatteryCapacityKWh*Threshold+Drain*(FVector2D::Distance(W.Position,S.BuildingAccessPoint(S.Buildings[0]))/S.WalkingSpeed()+Session);
    S.Time+=2;S.Energy.Tick(S,2);S.Workers.StepBattery(S,W,2);
    TestTrue(TEXT("A healthy body caches an actual charger route"),W.ChargePlanBuilding==S.Buildings[0].Id&&W.ChargePlanTravelSeconds>300);
    const int64 AfterPlanning=*TerrainSamples;
    for(int32 I=0;I<100;++I){S.Time+=1;S.Energy.Tick(S,1);S.Workers.StepBattery(S,W,1);}
    TestEqual(TEXT("Stationary healthy bodies do not repeat terrain routing each tick"),*TerrainSamples,AfterPlanning);
    TestEqual(TEXT("A cached estimate does not reserve a physical berth"),W.ChargingBuildingId,0);

    // Save between authored planning decisions. Rebuilding a derived cache
    // must not phase-shift departure, queue admission or consumed energy.
    S.Time+=.75;S.Energy.Tick(S,.75);S.Workers.StepBattery(S,W,.75);
    // The serialized worker ledger deliberately contains no planning cache.
    auto Saved=MakeShared<FJsonObject>();S.Workers.Save(Saved);
    FSeigeSimulation Loaded=S;
    if(!TestTrue(TEXT("A worker ledger with a derived plan reloads normally"),Loaded.Workers.Load(Saved,Loaded,Error))){AddError(Error);return false;}
    const auto* Restored=Loaded.Workers.Find(WorkerId);
    TestTrue(TEXT("Reload discards cached routes and rechecks current geometry"),Restored&&Restored->ChargePlanTime<0&&Restored->ChargePlanBuilding==0);

    bool Departed=false,Finished=false,Deterministic=true;double DepartureBattery=0,MinimumBattery=W.BatteryKWh,Distance=0;
    const int32 BeforeBodies=S.Workers.Bodies.Num();const double BeforeConsumption=S.Energy.ConsumedKWh;
    for(int32 I=0;I<10000&&!Finished;++I)
    {
        const FVector2D Before=W.Position;S.Time+=1;S.Energy.Tick(S,1);S.Workers.StepBattery(S,W,1);
        Loaded.Time+=1;Loaded.Energy.Tick(Loaded,1);auto* Reloaded=Loaded.Workers.Find(WorkerId);Loaded.Workers.StepBattery(Loaded,*Reloaded,1);
        if(W.Activity!=Reloaded->Activity||W.ChargingBuildingId!=Reloaded->ChargingBuildingId||W.BatteryKWh!=Reloaded->BatteryKWh||W.Position!=Reloaded->Position||S.Energy.ConsumedKWh!=Loaded.Energy.ConsumedKWh)Deterministic=false;
        const double Step=FVector2D::Distance(Before,W.Position);Distance+=Step;
        if(Step>S.WalkingSpeed()+1.e-6){AddError(TEXT("Charging body moved farther than one physical second permits"));return false;}
        if(!Departed&&W.ChargingBuildingId){Departed=true;DepartureBattery=W.BatteryKWh;}
        MinimumBattery=FMath::Min(MinimumBattery,W.BatteryKWh);
        Finished=Departed&&W.ChargingBuildingId==0;
    }
    TestTrue(TEXT("Actual detour cost triggers departure before the old straight-line threshold"),Departed&&DepartureBattery>OldTrigger+.01);
    TestTrue(TEXT("The same physical worker reaches a berth and finishes charging"),Finished&&W.Position.Equals(S.BuildingAccessPoint(S.Buildings[0]),1.e-5));
    TestTrue(TEXT("The long return never consumes the battery to zero"),MinimumBattery>0);
    TestTrue(TEXT("The worker walked the real detour rather than teleporting"),Distance>24000);
    TestEqual(TEXT("Charging creates no replacement body"),S.Workers.Bodies.Num(),BeforeBodies);
    TestTrue(TEXT("Refill used the actual grid energy ledger"),S.Energy.ConsumedKWh>BeforeConsumption&&FMath::IsNearlyEqual(W.BatteryKWh,W.BatteryCapacityKWh,1.e-8));
    TestTrue(TEXT("Discarding a derived charger cache preserves every physical charging step and exact grid consumption"),Deterministic);

    // A changed terrain revision invalidates the plan immediately. A body
    // already too depleted for the known detour must not promise a trip/berth.
    W.Activity=TEXT("idle");W.Position=FVector2D(1000,0);W.BatteryKWh=.001;
    S.Mobility.InvalidateTerrain();const FVector2D BeforeRejected=W.Position;const double BeforeRejectedCharge=W.BatteryKWh;
    S.Time+=2;S.Workers.StepBattery(S,W,2);
    TestTrue(TEXT("An unaffordable charging trip keeps the body and berth uncommitted"),W.ChargingBuildingId==0&&W.Position.Equals(BeforeRejected,1.e-8));
    TestTrue(TEXT("Rejected travel never grants rescue energy"),W.BatteryKWh<=BeforeRejectedCharge);
    return true;
}
#endif
