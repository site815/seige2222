#include "SeigeTransportVehicles.h"
#include "SeigeSimulation.h"
#include "SeigeCombatJson.h"
using namespace SeigeCombatJson;
bool FSeigeTransportVehicleSystem::Save(const TSharedPtr<FJsonObject>& Root) const
{
    O Doc=MakeShared<FJsonObject>();Doc->SetNumberField(TEXT("next_id"),NextId);Doc->SetNumberField(TEXT("next_job"),NextJobId);
    TArray<TSharedPtr<FJsonValue>> List;
    for(const auto& V:Bodies)
    {
        O Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("id"),V.Id);Row->SetNumberField(TEXT("delivery"),V.DeliveryId);Row->SetNumberField(TEXT("health"),V.Health);Row->SetNumberField(TEXT("battery_kwh"),V.BatteryKWh);Row->SetNumberField(TEXT("distance_meters"),V.DistanceMeters);Row->SetNumberField(TEXT("retry_at"),V.RetryAt);
        Row->SetBoolField(TEXT("embarked"),V.Embarked);Row->SetBoolField(TEXT("evacuated"),V.Evacuated);Row->SetBoolField(TEXT("returning"),V.ReturningToShuttle);Row->SetStringField(TEXT("handler"),V.HandlerWorkerId);Row->SetStringField(TEXT("status"),V.Status);Point(Row,TEXT("position"),V.Position);Point(Row,TEXT("heading"),V.Heading);Row->SetNumberField(TEXT("next_waypoint"),V.NextWaypoint);
        TArray<TSharedPtr<FJsonValue>> Route;for(const auto& P:V.Route){O R=MakeShared<FJsonObject>();Point(R,TEXT("p"),P);Route.Add(MakeShared<FJsonValueObject>(R));}Row->SetArrayField(TEXT("route"),Route);List.Add(MakeShared<FJsonValueObject>(Row));
    }
    Doc->SetArrayField(TEXT("bodies"),List);List.Reset();
    for(const auto& J:Jobs){O Row=MakeShared<FJsonObject>();Row->SetNumberField(TEXT("id"),J.Id);Row->SetNumberField(TEXT("factory"),J.FactoryId);Row->SetBoolField(TEXT("committed"),J.Committed);Row->SetNumberField(TEXT("progress_seconds"),J.ProgressSeconds);List.Add(MakeShared<FJsonValueObject>(Row));}
    Doc->SetArrayField(TEXT("jobs"),List);Root->SetObjectField(TEXT("transport_vehicles"),Doc);return true;
}
bool FSeigeTransportVehicleSystem::Load(const TSharedPtr<FJsonObject>& Root,FSeigeSimulation& S,FString& Error)
{
    const O* Doc=nullptr;const TArray<TSharedPtr<FJsonValue>> *List=nullptr,*Queue=nullptr;
    if(!Root->TryGetObjectField(TEXT("transport_vehicles"),Doc)||!Int(*Doc,TEXT("next_id"),NextId,1)||!Int(*Doc,TEXT("next_job"),NextJobId,1)||!(*Doc)->TryGetArrayField(TEXT("bodies"),List)||List->Num()>100000||!(*Doc)->TryGetArrayField(TEXT("jobs"),Queue)||Queue->Num()>Def.MaximumQueue){Error=TEXT("Invalid transport ledger header");return false;}
    Bodies.Reset();Jobs.Reset();TSet<int32> Ids,Claims;TSet<FString> Handlers;int32 Max=0,Embarked=0;
    for(const auto& Item:*List)
    {
        const O Row=Item->AsObject();FSeigeTransportVehicle V;const TArray<TSharedPtr<FJsonValue>>* Route=nullptr;
        if(!Int(Row,TEXT("id"),V.Id,1)||Ids.Contains(V.Id)||!Int(Row,TEXT("delivery"),V.DeliveryId)||!Num(Row,TEXT("health"),V.Health,0,Def.Health)||!Num(Row,TEXT("battery_kwh"),V.BatteryKWh,0,Def.BatteryKWh)||!Num(Row,TEXT("distance_meters"),V.DistanceMeters)||!Num(Row,TEXT("retry_at"),V.RetryAt)||!Row->TryGetBoolField(TEXT("embarked"),V.Embarked)||!Row->TryGetBoolField(TEXT("evacuated"),V.Evacuated)||!Row->TryGetBoolField(TEXT("returning"),V.ReturningToShuttle)||!Str(Row,TEXT("handler"),V.HandlerWorkerId)||!Str(Row,TEXT("status"),V.Status)||!Point(Row,TEXT("position"),V.Position,S.WorldHalfSize)||!Point(Row,TEXT("heading"),V.Heading,1.000001)||!Int(Row,TEXT("next_waypoint"),V.NextWaypoint)||!Row->TryGetArrayField(TEXT("route"),Route)||Route->Num()>8192||V.NextWaypoint>Route->Num())
        {Error=TEXT("Invalid saved transport body");return false;}
        for(const auto& R:*Route){FVector2D P;if(!Point(R->AsObject(),TEXT("p"),P,S.WorldHalfSize)){Error=TEXT("Invalid transport route");return false;}V.Route.Add(P);}
        V.RouteRevision=S.TransportRevision;Ids.Add(V.Id);Max=FMath::Max(Max,V.Id);
        if((V.Health<=0||V.Evacuated||V.Embarked||V.ReturningToShuttle)&&V.DeliveryId){Error=TEXT("Unavailable transport owns cargo work");return false;}
        if(V.Evacuated&&(!S.Escaped||!V.Embarked)){Error=TEXT("Transport evacuation disagrees with shuttle state");return false;}
        if(V.Embarked&&V.Health>0)++Embarked;
        const auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==V.DeliveryId;});
        if(V.DeliveryId)
        {
            if(!C||C->TransportVehicleId!=V.Id||Claims.Contains(C->Id)||!C->WorkerId.IsEmpty()||C->SourceDeployment||C->SelfTransfer||C->Resource==S.TextRule(TEXT("inactive_worker_resource"))||C->Amount+C->ReservedAmount>HaulUnits(S,C->Resource)+1.e-7||!C->Position.Equals(V.Position,.0001)||!OneOf(C->Phase,{TEXT("pickup"),TEXT("loading"),TEXT("carrying"),TEXT("unloading")})||(C->Amount>0&&C->ReservedAmount>0)||(C->Amount<=0&&C->ReservedAmount<=0))
            {Error=TEXT("Transport cargo identity, capacity or phase mismatch");return false;}Claims.Add(C->Id);
        }
        if(!V.HandlerWorkerId.IsEmpty())
        {
            if(!V.DeliveryId||Handlers.Contains(V.HandlerWorkerId)||!C||(C->Phase!=TEXT("loading")&&C->Phase!=TEXT("unloading")&&C->Phase!=TEXT("pickup")&&C->Phase!=TEXT("carrying"))){Error=TEXT("Invalid transport handler claim");return false;}Handlers.Add(V.HandlerWorkerId);
        }
        Bodies.Add(V);
    }
    if(NextId<=Max){Error=TEXT("Transport identity allocator would duplicate a body");return false;}
    for(const auto& C:S.Couriers)if(C.TransportVehicleId&&!Claims.Contains(C.Id)){Error=TEXT("Cargo has no owning transport body");return false;}
    for(const auto& V:S.Combat.Vehicles)if(V.Health>0&&V.Embarked)++Embarked;
    if(Embarked>Def.ShuttleVehicleCapacity){Error=TEXT("Shuttle vehicle bay exceeds its one-vehicle capacity");return false;}
    Ids.Reset();Max=0;TSet<int32> PaidFactories;
    for(const auto& Item:*Queue)
    {
        const O Row=Item->AsObject();FSeigeTransportFabrication J;
        if(!Int(Row,TEXT("id"),J.Id,1)||Ids.Contains(J.Id)||!Int(Row,TEXT("factory"),J.FactoryId,1)||!Row->TryGetBoolField(TEXT("committed"),J.Committed)||!Num(Row,TEXT("progress_seconds"),J.ProgressSeconds,0,Def.BuildSeconds)||J.ProgressSeconds>=Def.BuildSeconds||(!J.Committed&&J.ProgressSeconds!=0))
        {Error=TEXT("Invalid transport fabrication escrow");return false;}
        const auto* B=S.FindBuilding(J.FactoryId);if(!B||B->Health<=0||!Def.Factories.Contains(B->DefId)||(J.Committed&&PaidFactories.Contains(B->Id))){Error=TEXT("Transport fabrication has an invalid factory or duplicate paid job");return false;}
        if(J.Committed)PaidFactories.Add(B->Id);Ids.Add(J.Id);Max=FMath::Max(Max,J.Id);Jobs.Add(J);
    }
    if(NextJobId<=Max){Error=TEXT("Transport fabrication allocator would duplicate a job");return false;}Error.Empty();return true;
}
