#include "SeigeSimulation.h"
#include "SeigeRenderInterpolation.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
bool Prepare(FSeigeSimulation& S,FAutomationTestBase& Test)
{
    FString Error;if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){Test.AddError(Error);return false;}
    // Unit fixture isolates companion behavior from colony construction/power.
    S.Buildings[0].IsConstructing=false;S.DeploymentHatchOpen=true;S.Companions.Dogs[0].Aboard=false;S.Companions.Dogs[0].Position=S.BuildingAccessPoint(S.Buildings[0])+FVector2D(0,80);return true;
}
void StepDog(FSeigeSimulation& S,double Seconds)
{for(double T=0;T<Seconds-.00001;T+=S.FixedStepSeconds()){S.Time+=S.FixedStepSeconds();S.Calendar.Advance(S.FixedStepSeconds());S.Companions.Tick(S,S.FixedStepSeconds());}}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionFoodTest,"Seige.Companions.PhysicalFoodAndMorale",Flags)
bool FSeigeCompanionFoodTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!Prepare(S,*this))return false;
    TestEqual(TEXT("Single companion, separate from workforce"),S.Companions.Dogs.Num(),1);
    const int32 Population=S.Population;auto& Dog=S.Companions.Dogs[0];S.Companions.SetControlled(Dog.Id);
    const FString Food=S.Companions.FoodResource;const double Stock=S.Buildings[0].Inventory.FindRef(Food);
    TestTrue(TEXT("Finite landed food exists"),Stock>=S.Companions.FoodPerMealKg);
    StepDog(S,1);TestTrue(TEXT("One meal physically removes organic food"),FMath::IsNearlyEqual(Stock-S.Buildings[0].Inventory.FindRef(Food),S.Companions.FoodPerMealKg,.000001));
    TestTrue(TEXT("Fed dog benefits nearby work"),S.Companions.EfficiencyAt(Dog.Position)>1);
    TestEqual(TEXT("Morale is local"),S.Companions.EfficiencyAt(Dog.Position+FVector2D(S.Companions.MoraleRadiusMeters*2/S.MetersPerWorldUnit(),0)),1.);
    StepDog(S,2);TestTrue(TEXT("No per-tick duplicate meal"),FMath::IsNearlyEqual(Dog.FoodConsumedKg,S.Companions.FoodPerMealKg,.000001));
    Dog.Position=FVector2D(S.WorldHalfSize*2,0);Dog.NextMeal=S.Time;Dog.FedUntil=S.Time;
    StepDog(S,1);TestTrue(TEXT("Remote stock cannot feed dog"),FMath::IsNearlyEqual(Dog.FoodConsumedKg,S.Companions.FoodPerMealKg,.000001));
    TestEqual(TEXT("No food means no morale benefit"),S.Companions.EfficiencyAt(Dog.Position),1.);
    TestEqual(TEXT("Dogs do not create workers"),S.Population,Population);
    Dog.Position=S.Buildings[0].Position;S.Buildings[0].Inventory.FindOrAdd(Food)=.3;
    for(int32 Meal=0;Meal<3;++Meal){Dog.NextMeal=S.Time;StepDog(S,1);}
    TestEqual(TEXT("Decimal meal depletion never leaves negative physical stock"),S.Buildings[0].Inventory.FindRef(Food),0.);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionMovementTest,"Seige.Companions.WalkingInterpolationAndPersistence",Flags)
bool FSeigeCompanionMovementTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!Prepare(S,*this))return false;auto& Dog=S.Companions.Dogs[0];Dog.Position=FVector2D(1000,1000);
    S.Companions.SetControlled(Dog.Id);S.Companions.SetControlDirection(FVector2D(1,1));const FVector2D Before=Dog.Position;
    StepDog(S,10);const double Kmh=FVector2D::Distance(Dog.Position,Before)*S.MetersPerWorldUnit()/10*3.6;
    TestTrue(TEXT("Diagonal input stays at configured walking speed"),FMath::IsNearlyEqual(Kmh,S.Companions.WalkKmh,.0001));
    FSeigeRenderSnapshot Snapshot;Snapshot.Capture(S);const FVector2D Previous=Dog.Position;StepDog(S,S.FixedStepSeconds());
    const auto Mid=Snapshot.Companion(Dog,.5);TestTrue(TEXT("Dog moves smoothly between fixed simulation steps"),!Mid.Equals(Previous)&&!Mid.Equals(Dog.Position));
    auto State=MakeShared<FJsonObject>();S.Companions.Save(State);FSeigeCompanionSystem Loaded;FString Error;
    TestTrue(TEXT("Saved rules initialize companion reader"),Loaded.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),S,Error));
    TestTrue(TEXT("Companion save reloads"),Loaded.Load(State,S,Error));
    TestTrue(TEXT("Reload retains position"),Loaded.Dogs[0].Position.Equals(Dog.Position));
    TestEqual(TEXT("Reload clears transient possession"),Loaded.ControlledId,0);
    TestTrue(TEXT("Reload retains food accounting"),FMath::IsNearlyEqual(Loaded.Dogs[0].FoodConsumedKg,Dog.FoodConsumedKg));
    const double Limit=S.Companions.TerritoryRadiusMeters/S.MetersPerWorldUnit();Dog.Position=FVector2D(Limit-1,0);S.Companions.SetControlDirection(FVector2D(1,0));StepDog(S,10);
    TestTrue(TEXT("Cannot walk beyond 100 m of the shuttle"),Dog.Position.Size()<=Limit);
    const auto DogJson=State->GetObjectField(TEXT("companions"))->GetArrayField(TEXT("dogs"))[0]->AsObject();DogJson->SetNumberField(TEXT("food_consumed_kg"),-1);
    TestFalse(TEXT("Invalid saved food ledger rejected"),Loaded.Load(State,S,Error));
    S.LaunchShuttle();TestTrue(TEXT("Rex evacuates with the colony"),S.Companions.Dogs[0].Evacuated);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionShelterTest,"Seige.Companions.DamageTriggeredShelter",Flags)
bool FSeigeCompanionShelterTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!Prepare(S,*this))return false;auto& Dog=S.Companions.Dogs[0];
    Dog.Position=S.BuildingAccessPoint(S.Buildings[0])+FVector2D(400,0);Dog.RestUntil=1000;
    FSeigeEnemy Enemy;Enemy.Id=999;Enemy.Position=Dog.Position;S.Enemies.Add(Enemy);
    StepDog(S,1);TestTrue(TEXT("Nearby living enemy triggers protective retreat"),Dog.Retreating||Dog.Sheltered);S.Enemies.Empty();Dog.Retreating=false;Dog.Route.Empty();Dog.NextWaypoint=0;
    S.Companions.NotifyBuildingDamaged(S,S.Buildings[0].Position+FVector2D(101/S.MetersPerWorldUnit(),0));
    TestFalse(TEXT("Damage outside the shuttle radius is ignored"),Dog.Retreating);
    S.Companions.SetControlled(Dog.Id);S.Combat.DamageBuilding(S,S.Buildings[0].Id,1);
    TestTrue(TEXT("Actual nearby building damage triggers retreat"),Dog.Retreating);
    TestEqual(TEXT("Retreat releases possession"),S.Companions.ControlledId,0);
    const auto Before=Dog.Position;StepDog(S,S.FixedStepSeconds());
    TestTrue(TEXT("Rex travels physically instead of teleporting inside"),FVector2D::Distance(Before,Dog.Position)*S.MetersPerWorldUnit()<=S.Companions.RunKmh/3.6*S.FixedStepSeconds()+1.e-5);
    StepDog(S,20);TestTrue(TEXT("Rex reaches the real shuttle hatch and shelters"),Dog.Sheltered);
    TestTrue(TEXT("Shelter position is the hatch"),Dog.Position.Equals(S.Workers.HatchPoint(S),.001));
    auto State=MakeShared<FJsonObject>();S.Companions.Save(State);FSeigeCompanionSystem Loaded;FString Error;
    TestTrue(TEXT("Shelter rules initialize"),Loaded.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),S,Error));
    TestTrue(TEXT("Shelter state persists"),Loaded.Load(State,S,Error)&&Loaded.Dogs[0].Sheltered);
    StepDog(S,S.Companions.ShelterSeconds+30);TestFalse(TEXT("Rex returns outside after the quiet interval"),Dog.Sheltered||Dog.Retreating||Dog.ExitingShuttle);
    TestTrue(TEXT("Unpossessed roaming remains inside territory"),FVector2D::Distance(Dog.Position,S.Buildings[0].Position)*S.MetersPerWorldUnit()<=100);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionLifeTest,"Seige.Companions.LandingHealingAndPermanentDeath",Flags)
bool FSeigeCompanionLifeTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    auto& D=S.Companions.Dogs[0];const auto Hatch=S.Workers.HatchPoint(S);
    TestTrue(TEXT("Rex begins aboard at the real hatch"),D.Aboard&&D.Position.Equals(Hatch));
    TestFalse(TEXT("Aboard Rex has no outdoor body or player control"),S.Companions.IsOutside(D)||S.Companions.SetControlled(D.Id));
    StepDog(S,1);TestTrue(TEXT("No roaming before hatch opens"),D.Aboard&&D.Position.Equals(Hatch));
    S.DeploymentHatchOpen=true;const auto Before=D.Position;StepDog(S,S.FixedStepSeconds());
    TestTrue(TEXT("Landing releases a physical exit route"),!D.Aboard&&D.ExitingShuttle);
    TestTrue(TEXT("Exit walks rather than teleports"),FVector2D::Distance(Before,D.Position)*S.MetersPerWorldUnit()<=S.Companions.WalkKmh/3.6*S.FixedStepSeconds()+1.e-5);
    StepDog(S,30);TestFalse(TEXT("Exit completes"),D.ExitingShuttle);
    D.Position=S.BuildingAccessPoint(S.Buildings[0])+FVector2D(200,0);D.Route.Empty();D.NextWaypoint=0;
    S.Companions.SetControlled(D.Id);S.Companions.Damage(S,D.Id,10);
    TestEqual(TEXT("Hit removes actual HP"),D.Health,S.Companions.MaximumHealth-10);
    TestTrue(TEXT("Injury forces retreat and releases first-person control"),D.Retreating&&S.Companions.ControlledId==0);
    StepDog(S,20);TestTrue(TEXT("Physical retreat enters shelter"),D.Sheltered&&D.Position.Equals(Hatch,.001));
    StepDog(S,10);TestEqual(TEXT("Shelter heals to maximum"),D.Health,S.Companions.MaximumHealth);
    S.Companions.Damage(S,D.Id,1000);TestFalse(TEXT("Exterior damage cannot kill sheltered Rex"),D.Dead);
    StepDog(S,S.Companions.ShelterSeconds+10);TestTrue(TEXT("Healed Rex walks back outside after safe interval"),S.Companions.IsOutside(D)&&!D.Retreating&&!D.ExitingShuttle);
    S.Companions.Damage(S,D.Id,S.Companions.MaximumHealth);const int64 Deadline=S.Companions.DepressionUntilCalendarUs;
    TestTrue(TEXT("Lethal hit is permanent and withdraws control"),D.Dead&&D.Health==0&&!S.Companions.SetControlled(D.Id));
    StepDog(S,120);TestTrue(TEXT("Dead Rex never heals or exits on a later tick"),D.Dead&&D.Health==0);
    auto State=MakeShared<FJsonObject>();S.Companions.Save(State);FSeigeCompanionSystem Loaded;
    TestTrue(TEXT("Life-state reader initializes"),Loaded.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),S,Error));
    TestTrue(TEXT("Death and grief persist together"),Loaded.Load(State,S,Error)&&Loaded.Dogs[0].Dead&&Loaded.DepressionUntilCalendarUs==Deadline);
    TestEqual(TEXT("Every worker movement uses half-speed grief multiplier"),Loaded.WorkerSpeedMultiplier(S),.5);
    S.Calendar.SetElapsedMicroseconds(Deadline-1);TestEqual(TEXT("Grief persists one microsecond before deadline"),Loaded.WorkerSpeedMultiplier(S),.5);
    S.Calendar.SetElapsedMicroseconds(Deadline);TestEqual(TEXT("Grief expires exactly after thirty calendar days"),Loaded.WorkerSpeedMultiplier(S),1.);
    State->GetObjectField(TEXT("companions"))->GetArrayField(TEXT("dogs"))[0]->AsObject()->SetBoolField(TEXT("dead"),false);
    TestFalse(TEXT("A save cannot revive a zero-HP dog"),Loaded.Load(State,S,Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionGriefMovementTest,"Seige.Companions.GriefSlowsPhysicalWorkers",Flags)
bool FSeigeCompanionGriefMovementTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!Prepare(S,*this))return false;
    const FVector2D A(1000,1000),B(2000,1000);TArray<FVector2D> Route={B};int32 Next=0;auto Normal=A;
    S.WalkRoute(Normal,Route,Next,1);S.Companions.Damage(S,1,1000);Next=0;auto Sad=A;S.WalkRoute(Sad,Route,Next,1);
    TestTrue(TEXT("Actual offroad displacement is halved"),FMath::IsNearlyEqual(FVector2D::Distance(A,Sad)*2,FVector2D::Distance(A,Normal),1.e-6));
    const auto& Rules=S.Calendar.GetRules();TestEqual(TEXT("Month is exactly thirty full daylight/night cycles"),S.Companions.DepressionUntilCalendarUs-S.Calendar.ElapsedMicroseconds(),int64(30)*(Rules.DaylightMicroseconds+Rules.NightMicroseconds));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionSafeGroundTest,"Seige.Companions.ControlledRouteEligibility",Flags)
bool FSeigeCompanionSafeGroundTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;if(!Prepare(S,*this))return false;auto& D=S.Companions.Dogs[0];
    const double Reserved=S.Definition(S.Buildings[0])->ReservedFootprint;
    D.Position=S.Buildings[0].Position+FVector2D(Reserved+100,100);S.Companions.SetControlled(D.Id);S.Companions.SetControlDirection(FVector2D(-1,0));StepDog(S,10);
    TestTrue(TEXT("Manual walking cannot enter the future core footprint and strand retreat"),D.Position.X>=S.Buildings[0].Position.X+Reserved);
    S.Companions.Damage(S,D.Id,1);const auto Before=D.Position;StepDog(S,S.FixedStepSeconds());
    TestTrue(TEXT("Retreat from reserve edge is physical"),FVector2D::Distance(Before,D.Position)*S.MetersPerWorldUnit()<=S.Companions.RunKmh/3.6*S.FixedStepSeconds()+1.e-6);
    StepDog(S,30);TestTrue(TEXT("Reserve-edge retreat reaches hatch"),D.Sheltered);
    D.Sheltered=false;D.Retreating=false;D.Position=FVector2D(1000,1000);D.Route.Empty();D.NextWaypoint=0;
    S.Environment.Enabled=true;S.Environment.River.Empty();S.Environment.Lakes.Empty();FSeigeLake Lake;Lake.Center=FVector2D(1100,1000);Lake.Radii=FVector2D(70,70);Lake.Height=100;Lake.Depth=50;S.Environment.Lakes.Add(Lake);
    S.Companions.SetControlled(D.Id);S.Companions.SetControlDirection(FVector2D(1,0));StepDog(S,5);
    TestTrue(TEXT("Controlled motion retains dry route eligibility"),S.Environment.CanStand(D.Position,S.Companions.BodyRadiusMeters/S.MetersPerWorldUnit()));
    return true;
}

#endif
