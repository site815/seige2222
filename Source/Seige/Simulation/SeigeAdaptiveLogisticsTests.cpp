#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags AdaptiveLogisticsFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString AdaptiveRules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
bool AdaptiveWorkerPolicy(TSharedPtr<FJsonObject>& Policy)
{FString Json;return FFileHelper::LoadFileToString(Json,*FPaths::Combine(AdaptiveRules(),TEXT("workers.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Policy);}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAdaptiveLogisticsPolicyTest,"Seige.Workers.AdaptiveLogisticsPolicy",AdaptiveLogisticsFlags)
bool FSeigeAdaptiveLogisticsPolicyTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;if(!S.Initialize(AdaptiveRules(),Error,false,false)){AddError(Error);return false;}
    const int32 InitialBodies=S.Workers.Bodies.Num();
    TestEqual(TEXT("Building crews add no separate hauling jobs"),S.Workers.LogisticsJobs(S),0);
    // Classification-only records exercise current demand without pretending to
    // manufacture workers. The paid-growth test below covers real admission.
    auto Facility=[&](const FString& Definition)
    {FSeigeBuilding B;B.Id=100+S.Buildings.Num();B.DefId=Definition;B.Health=S.BuildingDefs[Definition].Health;B.IsConstructing=false;B.Enabled=true;S.Buildings.Add(B);};
    for(int32 I=0;I<40;++I)Facility(TEXT("solar_array"));
    for(int32 I=0;I<12;++I)Facility(TEXT("wall_segment"));
    Facility(TEXT("robot_service_bay"));
    S.Workers.RefreshMetrics(S);
    TestEqual(TEXT("Large installations retain only authored workplace vacancies"),S.TotalJobs,S.Definition(S.Buildings[0])->Jobs+40*S.BuildingDefs[TEXT("solar_array")].Jobs+12*S.BuildingDefs[TEXT("wall_segment")].Jobs+S.BuildingDefs[TEXT("robot_service_bay")].Jobs);
    TestEqual(TEXT("Facility counts do not synthesize logistics staff"),S.Workers.LogisticsJobs(S),0);
    TestEqual(TEXT("Deriving demand never creates physical bodies"),S.Workers.Bodies.Num(),InitialBodies);
    S.Buildings.Last().Enabled=false;S.Workers.RefreshMetrics(S);
    TestEqual(TEXT("Disabling a workplace removes its open demand"),S.TotalJobs,S.Definition(S.Buildings[0])->Jobs+40*S.BuildingDefs[TEXT("solar_array")].Jobs+12*S.BuildingDefs[TEXT("wall_segment")].Jobs);
    TestEqual(TEXT("Disabling a workplace never removes a physical body"),S.Workers.Bodies.Num(),InitialBodies);
    TSharedPtr<FJsonObject> Policy;if(!AdaptiveWorkerPolicy(Policy)){AddError(TEXT("Cannot read worker policy fixture"));return false;}
    Policy->SetStringField(TEXT("logistics_scaling_policy"),TEXT("fixed"));
    if(!S.Workers.Initialize(Policy,S,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Legacy fixed-policy data cannot reintroduce phantom hauling staff"),S.Workers.LogisticsJobs(S),0);
    Policy->SetStringField(TEXT("logistics_scaling_policy"),TEXT("completed_facilities"));Policy->SetNumberField(TEXT("logistics_max_workers"),3);
    if(!S.Workers.Initialize(Policy,S,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Legacy scaling bounds cannot override permanent building crews"),S.Workers.LogisticsJobs(S),0);
    Policy->SetNumberField(TEXT("logistics_facilities_per_worker"),0);
    TestFalse(TEXT("Native loading rejects a zero scaling divisor"),S.Workers.Initialize(Policy,S,Error));
    Policy->SetNumberField(TEXT("logistics_facilities_per_worker"),4);Policy->SetNumberField(TEXT("logistics_max_workers"),.5);
    TestFalse(TEXT("Native loading rejects fractional or below-base scheduling limits"),S.Workers.Initialize(Policy,S,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAdaptiveLogisticsGrowthTest,"Seige.Workers.AdaptiveLogisticsPaidGrowth",AdaptiveLogisticsFlags)
bool FSeigeAdaptiveLogisticsGrowthTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!S.Initialize(AdaptiveRules(),Error,false,false)){AddError(Error);return false;}
    for(int32 I=0;I<1000&&(S.Buildings[0].IsConstructing||S.Population<S.TotalJobs||S.InactiveWorkerCount()<S.WorkerReserveTarget()||S.Buildings[0].ProductionCommitted);++I)S.Tick(10);
    if(!TestFalse(TEXT("Finite landed workers finish actual core deployment"),S.Buildings[0].IsConstructing)||!TestEqual(TEXT("The isolated core has its actual paid workforce"),S.Population,S.TotalJobs))return false;
    const int32 Core=S.Buildings[0].Id;
    for(auto& Dog:S.Companions.Dogs){Dog.Position=Dog.Target=S.Buildings[0].Position+FVector2D(-1500,-1500);Dog.Route.Empty();Dog.NextWaypoint=0;Dog.Moving=false;Dog.RestUntil=S.Time;Dog.MotionSpeedMetersPerSecond=Dog.SpeedMetersPerSecond=0;S.Companions.SetControlled(Dog.Id);}
    // Install isolated demand fixtures from the genuine starter kit. This skips
    // only their already-covered construction duration, never worker production,
    // cargo cost, support admission, energy or movement.
    auto Install=[&](const FString& Definition,FVector2D Position,int32& Id)
    {
        if(!S.PlaceBuilding(Definition,Position,Error))return false;
        auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);
        for(const auto& P:Cost)if(S.FindBuilding(Core)->Inventory.FindRef(P.Key)<P.Value){Error=TEXT("Finite kit cannot pay logistics fixture");return false;}
        for(const auto& P:Cost)S.FindBuilding(Core)->Inventory.FindOrAdd(P.Key)-=P.Value;
        B.InstalledMaterials=Cost;B.IsConstructing=false;B.ConstructionProgress=1;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;Id=B.Id;return true;
    };
    int32 Service=0,A=0,B=0,C=0;
    if(!Install(TEXT("robot_service_bay"),FVector2D(1300,0),Service)||!Install(TEXT("solar_array"),FVector2D(0,1300),A)||!Install(TEXT("solar_array"),FVector2D(0,-1300),B)||!Install(TEXT("solar_array"),FVector2D(-1300,0),C)){AddError(Error);return false;}
    S.Energy.Invalidate();S.Energy.Tick(S,0);S.Workers.RefreshMetrics(S);
    const int32 Before=S.Workers.Bodies.Num(),PopulationBefore=S.Population,Goal=S.TotalJobs;
    const double CircuitsBefore=S.TotalStock(TEXT("circuits")),BatteriesBefore=S.TotalStock(TEXT("batteries"));
    const auto WorkerInputs=S.ProductionInputs(*S.FindBuilding(Core),TEXT("assemble_robot"));
    TestEqual(TEXT("Four completed facilities share owned crews without extra logistics jobs"),S.Workers.LogisticsJobs(S),0);
    TestEqual(TEXT("Derived vacancies do not create a worker instantly"),S.Population,PopulationBefore);
    TestEqual(TEXT("Every vacancy belongs to an actual workplace"),Goal,S.Definition(*S.FindBuilding(Core))->Jobs+S.Definition(*S.FindBuilding(Service))->Jobs+3*S.BuildingDefs[TEXT("solar_array")].Jobs);
    for(int32 I=0;I<1200&&(S.Population<S.TotalJobs||S.InactiveWorkerCount()<S.WorkerReserveTarget()||S.FindBuilding(Core)->ProductionCommitted);++I)S.Tick(10);
    if(!TestEqual(TEXT("Paid manufacture and physical reactivation fill the expanded job ledger"),S.Population,Goal))return false;
    const int32 Added=S.Workers.Bodies.Num()-Before;
    TestEqual(TEXT("Refilling the used reserve manufactures exactly the new workplace demand"),Added,Goal-PopulationBefore);
    TestTrue(TEXT("Assembly debits the authored circuits for every new body"),FMath::IsNearlyEqual(CircuitsBefore-S.TotalStock(TEXT("circuits")),Added*WorkerInputs.FindRef(TEXT("circuits")),1.e-6));
    TestTrue(TEXT("Assembly debits the authored batteries for every new body"),FMath::IsNearlyEqual(BatteriesBefore-S.TotalStock(TEXT("batteries")),Added*WorkerInputs.FindRef(TEXT("batteries")),1.e-6));
    TestTrue(TEXT("Additional active workers remain inside real serviced capacity"),S.Population<=S.RobotSupportCapacity);
    const int32 BodyCount=S.Workers.Bodies.Num();
    if(!S.Workers.Dispatch(S,Core,A,0,TEXT("alloy"),1,false)){AddError(TEXT("A physically available worker must accept the reduction probe"));return false;}
    const int32 Delivery=S.Couriers.Last().Id;const FString Identity=S.Couriers.Last().WorkerId;const FVector2D Position=S.Workers.Find(Identity)->Position;
    S.FindBuilding(Service)->Enabled=false;S.Workers.RefreshMetrics(S);
    TestEqual(TEXT("A disabled facility does not create hidden hauling demand"),S.Workers.LogisticsJobs(S),0);
    TestEqual(TEXT("A demand reduction cannot delete a manufactured body"),S.Workers.Bodies.Num(),BodyCount);
    TestTrue(TEXT("A demand reduction cannot cancel or teleport an accepted pickup"),S.Workers.Find(Identity)->DeliveryId==Delivery&&S.Workers.Find(Identity)->Position.Equals(Position)&&S.Couriers.Last().ReservedAmount==1);
    S.Tick(S.FixedStepSeconds());
    TestTrue(TEXT("Existing live hauling continues during a workplace disable"),S.Workers.Find(Identity)->Activity==TEXT("delivery")&&S.Workers.Find(Identity)->DeliveryId==Delivery);
    const FString Save=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/WorkerAgents/adaptive-logistics.json"));
    if(!S.Save(Save,Error)||!Loaded.Initialize(AdaptiveRules(),Error,false,false)||!Loaded.Load(Save,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Reload derives the same logistics demand from saved facilities"),Loaded.Workers.LogisticsJobs(Loaded),S.Workers.LogisticsJobs(S));
    TestEqual(TEXT("Reload preserves every manufactured identity"),Loaded.Workers.Bodies.Num(),S.Workers.Bodies.Num());
    TestTrue(TEXT("Reload retains the exact live pickup and its physical position"),Loaded.Workers.Find(Identity)&&Loaded.Workers.Find(Identity)->DeliveryId==Delivery&&Loaded.Workers.Find(Identity)->Position.Equals(S.Workers.Find(Identity)->Position));
    return true;
}
#endif
