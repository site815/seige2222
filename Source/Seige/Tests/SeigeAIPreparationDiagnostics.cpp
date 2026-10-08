#include "AI/SeigeScenarioAI.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
TSharedPtr<FJsonObject> AIPreparationSample(const FSeigeSimulation& Sim,const FSeigeScenarioAI& Brain)
{
    auto Incoming=[&](int32 Id,const FString& Resource){double Total=0;for(const auto& C:Sim.Couriers)if(C.TargetId==Id&&C.RoadTargetId==0&&C.Resource==Resource)Total+=C.Amount+C.ReservedAmount+(C.SelfTransfer?1:0);return Total;};
    auto Sample=MakeShared<FJsonObject>();Sample->SetNumberField(TEXT("time"),Sim.Time);Sample->SetStringField(TEXT("status"),Brain.GetStatus());
    Sample->SetNumberField(TEXT("population"),Sim.Population);Sample->SetNumberField(TEXT("jobs"),Sim.TotalJobs);Sample->SetNumberField(TEXT("couriers"),Sim.Couriers.Num());
    Sample->SetNumberField(TEXT("credits"),Sim.Credits);
    auto Activities=MakeShared<FJsonObject>();for(const auto& W:Sim.Workers.Bodies)if(W.State==TEXT("active")){double Count=0;Activities->TryGetNumberField(W.Activity,Count);Activities->SetNumberField(W.Activity,Count+1);}Sample->SetObjectField(TEXT("activities"),Activities);
    TArray<TSharedPtr<FJsonValue>> Generators,Ports,Roads,Services,Workers;
    for(const auto& W:Sim.Workers.Bodies)if(W.State==TEXT("active"))
    {auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("id"),W.Id);V->SetStringField(TEXT("activity"),W.Activity);V->SetNumberField(TEXT("building"),W.BuildingId);V->SetNumberField(TEXT("road"),W.RoadId);V->SetNumberField(TEXT("delivery"),W.DeliveryId);Workers.Add(MakeShared<FJsonValueObject>(V));}
    for(const auto& B:Sim.Buildings)if(B.Health>0)
    {
        if(const auto* E=Sim.Energy.Definition(B.DefId);E&&!E->FuelResource.IsEmpty())
        {
            auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("id"),B.Id);V->SetStringField(TEXT("definition"),B.DefId);V->SetStringField(TEXT("fuel"),E->FuelResource);
            V->SetNumberField(TEXT("local"),B.Inventory.FindRef(E->FuelResource));V->SetNumberField(TEXT("buffer"),Sim.Energy.FuelDemand(B.DefId,E->FuelResource));V->SetNumberField(TEXT("incoming"),Incoming(B.Id,E->FuelResource));V->SetNumberField(TEXT("workers"),B.Workers);V->SetNumberField(TEXT("required_workers"),Sim.Definition(B)->Jobs);V->SetNumberField(TEXT("staffing_fraction"),FMath::Min(1.,double(B.Workers)/FMath::Max(1,Sim.Definition(B)->Jobs)));V->SetStringField(TEXT("status"),B.Status);
            const auto Grid=Sim.Energy.Info(Sim,B.Id);V->SetNumberField(TEXT("grid_generation_kw"),Grid.GenerationKW);V->SetNumberField(TEXT("grid_stored_kwh"),Grid.StoredKWh);V->SetNumberField(TEXT("power_fraction"),Grid.PowerFraction);
            TArray<TSharedPtr<FJsonValue>> Sources;
            for(const auto& Source:Sim.Buildings)if(Source.Id!=B.Id&&Source.Health>0&&Source.Inventory.FindRef(E->FuelResource)>0)
            {
                auto P=MakeShared<FJsonObject>();P->SetNumberField(TEXT("id"),Source.Id);P->SetNumberField(TEXT("fuel"),Source.Inventory.FindRef(E->FuelResource));
                FVector2D Previous=Sim.BuildingAccessPoint(Source);TArray<FVector2D> Route;const bool Reachable=Sim.FindRoute(Previous,Sim.BuildingAccessPoint(B),Route);double Length=0,Seconds=0;
                for(const auto& Next:Route){const double D=FVector2D::Distance(Previous,Next);Length+=D;Seconds+=D/(Sim.WalkingSpeed()*Sim.RouteSpeedMultiplier(Previous,Next));Previous=Next;}
                P->SetBoolField(TEXT("reachable"),Reachable);P->SetNumberField(TEXT("route_meters"),Length*Sim.MetersPerWorldUnit());P->SetNumberField(TEXT("current_one_way_seconds"),Seconds);Sources.Add(MakeShared<FJsonValueObject>(P));
            }
            V->SetArrayField(TEXT("sources"),Sources);Generators.Add(MakeShared<FJsonValueObject>(V));
        }
        if(Sim.Definition(B)->RobotSupportCapacity>0)
        {auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("id"),B.Id);V->SetNumberField(TEXT("workers"),B.Workers);V->SetNumberField(TEXT("capacity"),Sim.Definition(B)->RobotSupportCapacity);V->SetNumberField(TEXT("supported_here"),B.SupportedRobots);V->SetBoolField(TEXT("maintenance_supplied"),B.MaintenanceSupplied);auto Stock=MakeShared<FJsonObject>();for(const auto& P:B.Inventory)Stock->SetNumberField(P.Key,P.Value);V->SetObjectField(TEXT("inventory"),Stock);Services.Add(MakeShared<FJsonValueObject>(V));}
        if(!B.Shipment.Resource.IsEmpty())
        {auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("id"),B.Id);V->SetStringField(TEXT("resource"),B.Shipment.Resource);V->SetBoolField(TEXT("buy"),B.Shipment.Buy);V->SetBoolField(TEXT("departed"),B.Shipment.Departed);V->SetNumberField(TEXT("quantity"),B.Shipment.Quantity);V->SetNumberField(TEXT("local"),B.Inventory.FindRef(B.Shipment.Resource));V->SetNumberField(TEXT("incoming"),Incoming(B.Id,B.Shipment.Resource));Ports.Add(MakeShared<FJsonValueObject>(V));}
    }
    for(const auto& R:Sim.Roads)if(R.Health>0&&R.IsConstructing)
    {auto V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("id"),R.Id);V->SetNumberField(TEXT("progress"),R.ConstructionProgress);V->SetNumberField(TEXT("builders"),R.Builders);V->SetNumberField(TEXT("on_site"),R.BuildersOnSite);Roads.Add(MakeShared<FJsonValueObject>(V));}
    Sample->SetArrayField(TEXT("generators"),Generators);Sample->SetArrayField(TEXT("shipments"),Ports);Sample->SetArrayField(TEXT("unfinished_roads"),Roads);Sample->SetArrayField(TEXT("support"),Services);Sample->SetArrayField(TEXT("workers"),Workers);return Sample;
}
}

// Opt-in evidence collection, deliberately outside release acceptance tests.
// Success confirms the requested snapshot was captured, not colony readiness.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIPreparationSnapshotDiagnostic,"Diagnostics.AIPreparationSnapshot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIPreparationSnapshotDiagnostic::RunTest(const FString&)
{
    int32 Seed=0;double Stop=0;
    if(!FParse::Value(FCommandLine::Get(),TEXT("SeigeSnapshotSeed="),Seed)||!FParse::Value(FCommandLine::Get(),TEXT("SeigeSnapshotSeconds="),Stop)||!FMath::IsFinite(Stop)||Stop<=0)
    {AddError(TEXT("Requires -SeigeSnapshotSeed=<seed> -SeigeSnapshotSeconds=<time within authored preparation limit>"));return false;}
    const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),AI=FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES"));
    FString Raw,Error;TSharedPtr<FJsonObject> Config;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(AI,TEXT("colony_ai.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Config)){AddError(TEXT("Cannot read the existing AI decision cadence"));return false;}
    double Chunk=0;if(!Config->TryGetNumberField(TEXT("decision_interval_seconds"),Chunk)||!FMath::IsFinite(Chunk)||Chunk<=0){AddError(TEXT("Invalid existing AI decision cadence"));return false;}FSeigeSimulation Sim;FSeigeScenarioAI Brain;
    if(!Brain.BeginInitialize(Sim,Rules,AI,true,Error,true,true,Seed)){AddError(Error);return false;}
    if(Stop>Brain.PreparationLimit()){AddError(TEXT("Snapshot time cannot extend the authored developed preparation limit"));return false;}
    const FString Stem=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Diagnostics"),FString::Printf(TEXT("ai-preloss-%d-%.0f"),Seed,Stop));
    TArray<TSharedPtr<FJsonValue>> Samples;TMap<int32,TSharedPtr<FJsonObject>> Deliveries;double NextSample=FMath::Max(0.,Stop-6000.);
    while(Sim.Time+UE_DOUBLE_SMALL_NUMBER<Stop&&!Sim.Failed&&!Sim.Escaped)
    {
        const double Before=Sim.Time;Brain.Tick(Sim,FMath::Min(Chunk,Stop-Sim.Time));if(Sim.Time<=Before){AddError(TEXT("Diagnostic simulation stopped advancing"));return false;}
        // Observe after normal decision-sized chunks. These timestamps have
        // that sampling resolution; no task or movement state is changed.
        for(const auto& C:Sim.Couriers)
        {
            auto& D=Deliveries.FindOrAdd(C.Id);
            if(!D){D=MakeShared<FJsonObject>();D->SetNumberField(TEXT("id"),C.Id);D->SetNumberField(TEXT("source"),C.SourceId);D->SetNumberField(TEXT("target"),C.TargetId);D->SetNumberField(TEXT("road"),C.RoadTargetId);D->SetStringField(TEXT("resource"),C.Resource);D->SetStringField(TEXT("worker"),C.WorkerId);D->SetNumberField(TEXT("first_observed"),Sim.Time);D->SetNumberField(TEXT("initial_load"),C.Amount+C.ReservedAmount);}
            D->SetNumberField(TEXT("last_observed"),Sim.Time);D->SetStringField(TEXT("last_phase"),C.Phase);D->SetNumberField(TEXT("remaining_load"),C.Amount+C.ReservedAmount);D->SetNumberField(TEXT("remaining_waypoints"),C.Route.Num()-C.NextWaypoint);D->SetNumberField(TEXT("x"),C.Position.X);D->SetNumberField(TEXT("y"),C.Position.Y);
        }
        if(Sim.Time+UE_DOUBLE_SMALL_NUMBER>=NextSample)
        {
            const int32 Index=Samples.Num();Samples.Add(MakeShared<FJsonValueObject>(AIPreparationSample(Sim,Brain)));NextSample+=1000.;
            if(Index==0||Index==2)if(!Sim.Save(Stem+FString::Printf(TEXT("-at-%.0f.json"),Sim.Time),Error)){AddError(Error);return false;}
        }
    }
    if(!Sim.Save(Stem+TEXT(".json"),Error)){AddError(Error);return false;}
    auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("purpose"),TEXT("Diagnostic only; does not establish developed readiness"));Report->SetNumberField(TEXT("sampling_seconds"),Chunk);Report->SetArrayField(TEXT("samples"),Samples);
    TArray<int32> Ids;Deliveries.GetKeys(Ids);Ids.Sort();TArray<TSharedPtr<FJsonValue>> Records;for(int32 Id:Ids)Records.Add(MakeShared<FJsonValueObject>(Deliveries[Id]));Report->SetArrayField(TEXT("deliveries"),Records);
    Raw.Empty();if(!FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Raw))||!FFileHelper::SaveStringToFile(Raw,*(Stem+TEXT("-logistics.json")))){AddError(TEXT("Cannot save logistics diagnostic"));return false;}
    AddInfo(FString::Printf(TEXT("Diagnostic snapshot at %.2f, failed=%d, status=%s, output=%s.json"),Sim.Time,Sim.Failed?1:0,*Brain.GetStatus(),*Stem));
    TestTrue(TEXT("The requested pre-loss state was actually reached"),FMath::Abs(Sim.Time-Stop)<1.e-6);return true;
}
#endif
