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
    S.Buildings[0].IsConstructing=false;return true;
}
void StepDog(FSeigeSimulation& S,double Seconds)
{for(double T=0;T<Seconds-.00001;T+=S.FixedStepSeconds()){S.Time+=S.FixedStepSeconds();S.Companions.Tick(S,S.FixedStepSeconds());}}
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
    FSeigeSimulation S;if(!Prepare(S,*this))return false;auto& Dog=S.Companions.Dogs[0];Dog.Position=FVector2D(2000,2000);
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
    const double Limit=S.WorldHalfSize*3;Dog.Position=FVector2D(Limit-S.Companions.BodyRadiusMeters/S.MetersPerWorldUnit()-1,2000);S.Companions.SetControlDirection(FVector2D(1,0));StepDog(S,10);
    TestTrue(TEXT("Cannot walk outside all nine sectors"),Dog.Position.X<Limit);
    const auto DogJson=State->GetObjectField(TEXT("companions"))->GetArrayField(TEXT("dogs"))[0]->AsObject();DogJson->SetNumberField(TEXT("food_consumed_kg"),-1);
    TestFalse(TEXT("Invalid saved food ledger rejected"),Loaded.Load(State,S,Error));
    S.LaunchShuttle();TestTrue(TEXT("Rex evacuates with the colony"),S.Companions.Dogs[0].Evacuated);return true;
}
#endif
