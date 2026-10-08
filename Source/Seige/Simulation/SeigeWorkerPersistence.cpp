#include "SeigeWorkers.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"

namespace
{
using WorkerJson=TSharedPtr<FJsonObject>;
void WorkerWritePoint(const WorkerJson& O,const TCHAR* K,FVector2D P){O->SetArrayField(K,{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)});}
bool WorkerReadPoint(const WorkerJson& O,const TCHAR* K,FVector2D& P,double Bounds)
{const TArray<TSharedPtr<FJsonValue>>* A=nullptr;double X=0,Y=0;if(!O||!O->TryGetArrayField(K,A)||A->Num()!=2||!(*A)[0]->TryGetNumber(X)||!(*A)[1]->TryGetNumber(Y)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||FMath::Abs(X)>Bounds||FMath::Abs(Y)>Bounds)return false;P=FVector2D(X,Y);return true;}
bool WorkerReadNumber(const WorkerJson& O,const TCHAR* K,double& V){return O&&O->TryGetNumberField(K,V)&&FMath::IsFinite(V)&&V>=0;}
bool WorkerReadInt(const WorkerJson& O,const TCHAR* K,int32& V){double D=0;if(!WorkerReadNumber(O,K,D)||D>MAX_int32||D!=FMath::FloorToDouble(D))return false;V=int32(D);return true;}
}
bool FSeigeWorkerSystem::Save(const TSharedPtr<FJsonObject>& Root) const
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("origin"),Origin);O->SetNumberField(TEXT("next_serial"),NextSerial);O->SetNumberField(TEXT("deployment_elapsed"),DeploymentElapsed);O->SetNumberField(TEXT("recycling_waste_kg"),RecyclingWasteKg);
    auto Stock=MakeShared<FJsonObject>();for(const auto& P:DeploymentStock)Stock->SetNumberField(P.Key,P.Value);O->SetObjectField(TEXT("deployment_stock"),Stock);
    TArray<TSharedPtr<FJsonValue>> A;for(const auto& W:Bodies)
    {
        auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("id"),W.Id);V->SetStringField(TEXT("state"),W.State);V->SetStringField(TEXT("activity"),W.Activity);V->SetStringField(TEXT("container_kind"),W.ContainerKind);
        V->SetNumberField(TEXT("station_slot"),W.StationSlot);V->SetNumberField(TEXT("departure_at"),W.DepartureAt);V->SetNumberField(TEXT("building"),W.BuildingId);V->SetNumberField(TEXT("road"),W.RoadId);V->SetNumberField(TEXT("container"),W.ContainerId);V->SetNumberField(TEXT("delivery"),W.DeliveryId);V->SetNumberField(TEXT("next_waypoint"),W.NextWaypoint);V->SetNumberField(TEXT("phase_seconds"),W.PhaseSeconds);V->SetNumberField(TEXT("retry_at"),W.RetryAt);V->SetBoolField(TEXT("outdoor"),W.Outdoor);
        WorkerWritePoint(V,TEXT("position"),W.Position);WorkerWritePoint(V,TEXT("heading"),W.Heading);TArray<TSharedPtr<FJsonValue>> Route;for(auto P:W.Route){auto Point=MakeShared<FJsonObject>();WorkerWritePoint(Point,TEXT("p"),P);Route.Add(MakeShared<FJsonValueObject>(Point));}V->SetArrayField(TEXT("route"),Route);A.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("bodies"),A);Root->SetObjectField(TEXT("workers"),O);return true;
}
bool FSeigeWorkerSystem::Load(const TSharedPtr<FJsonObject>& Root,FSeigeSimulation& S,FString& Error)
{
    const TSharedPtr<FJsonObject>* O=nullptr,*Stock=nullptr;const TArray<TSharedPtr<FJsonValue>>* A=nullptr;
    if(!Root->TryGetObjectField(TEXT("workers"),O)||!(*O)->TryGetStringField(TEXT("origin"),Origin)||Origin.IsEmpty()||!WorkerReadInt(*O,TEXT("next_serial"),NextSerial)||NextSerial<1||!WorkerReadNumber(*O,TEXT("recycling_waste_kg"),RecyclingWasteKg)||!WorkerReadNumber(*O,TEXT("deployment_elapsed"),DeploymentElapsed)||DeploymentElapsed>S.Time+1.e-5||!(*O)->TryGetObjectField(TEXT("deployment_stock"),Stock)||!(*O)->TryGetArrayField(TEXT("bodies"),A)||A->Num()>1000000){Error=TEXT("Invalid saved worker ledger header");return false;}
    DeploymentStock.Empty();for(const auto& P:(*Stock)->Values){double Amount=0;const FString Id(P.Key);if(!S.Resources.Contains(Id)||!P.Value->TryGetNumber(Amount)||!FMath::IsFinite(Amount)||Amount<0){Error=TEXT("Invalid onboard deployment material");return false;}DeploymentStock.Add(Id,Amount);}
    const TSet<FString> States={TEXT("active"),TEXT("stored"),TEXT("shipment"),TEXT("vehicle"),TEXT("disassembling"),TEXT("disassembled"),TEXT("destroyed"),TEXT("exported"),TEXT("evacuated")};
    const TSet<FString> Activities={TEXT("aboard"),TEXT("exit"),TEXT("idle"),TEXT("to_job"),TEXT("operate"),TEXT("to_build"),TEXT("build"),TEXT("to_road"),TEXT("road_build"),TEXT("delivery"),TEXT("reactivate"),TEXT("return"),TEXT("to_store"),TEXT("store"),TEXT("to_recycle"),TEXT("stored"),TEXT("shipment"),TEXT("vehicle"),TEXT("disassembling"),TEXT("terminal")};
    Bodies.Empty();TSet<FString> Keys;TSet<int32> DeliveryClaims;int32 Active=0,MaxOwnSerial=0;
    for(const auto& Value:*A)
    {
        const auto V=Value->AsObject();FSeigeWorker W;
        if(!V||!V->TryGetStringField(TEXT("id"),W.Id)||W.Id.IsEmpty()||Keys.Contains(W.Id)||!V->TryGetStringField(TEXT("state"),W.State)||!States.Contains(W.State)||!V->TryGetStringField(TEXT("activity"),W.Activity)||!Activities.Contains(W.Activity)||!V->TryGetStringField(TEXT("container_kind"),W.ContainerKind)||!WorkerReadInt(V,TEXT("station_slot"),W.StationSlot)||!WorkerReadNumber(V,TEXT("departure_at"),W.DepartureAt)||!WorkerReadInt(V,TEXT("building"),W.BuildingId)||!WorkerReadInt(V,TEXT("road"),W.RoadId)||!WorkerReadInt(V,TEXT("container"),W.ContainerId)||!WorkerReadInt(V,TEXT("delivery"),W.DeliveryId)||!WorkerReadInt(V,TEXT("next_waypoint"),W.NextWaypoint)||!WorkerReadNumber(V,TEXT("phase_seconds"),W.PhaseSeconds)||!WorkerReadNumber(V,TEXT("retry_at"),W.RetryAt)||!V->TryGetBoolField(TEXT("outdoor"),W.Outdoor)||!WorkerReadPoint(V,TEXT("position"),W.Position,S.WorldHalfSize*3)||!WorkerReadPoint(V,TEXT("heading"),W.Heading,1.000001))
        {Error=TEXT("Invalid or duplicate saved worker body");return false;}
        FString Prefix,Serial;if(!W.Id.Split(TEXT(":"),&Prefix,&Serial)||!Serial.IsNumeric()||FCString::Atoi(*Serial)<=0){Error=TEXT("Malformed worker origin identity");return false;}if(Prefix==Origin)MaxOwnSerial=FMath::Max(MaxOwnSerial,FCString::Atoi(*Serial));Keys.Add(W.Id);
        const TArray<TSharedPtr<FJsonValue>>* Route=nullptr;if(!V->TryGetArrayField(TEXT("route"),Route)||Route->Num()>100000){Error=TEXT("Invalid worker route");return false;}for(const auto& P:*Route){FVector2D Point;if(!WorkerReadPoint(P->AsObject(),TEXT("p"),Point,S.WorldHalfSize)){Error=TEXT("Invalid worker waypoint");return false;}W.Route.Add(Point);}if(W.NextWaypoint>W.Route.Num()){Error=TEXT("Worker waypoint outside route");return false;}W.RouteRevision=S.TransportRevision;
        if(W.State==TEXT("active"))
        {
            if(W.Activity==TEXT("stored")||W.Activity==TEXT("shipment")||W.Activity==TEXT("vehicle")||W.Activity==TEXT("disassembling")||W.Activity==TEXT("terminal")){Error=TEXT("Active worker has an inactive lifecycle activity");return false;}
            ++Active;if(W.BuildingId&&!S.FindBuilding(W.BuildingId)||W.RoadId&&!S.FindRoad(W.RoadId)){Error=TEXT("Worker references absent job");return false;}
            const bool BuildingWork=W.Activity==TEXT("operate")||W.Activity==TEXT("to_job")||W.Activity==TEXT("build")||W.Activity==TEXT("to_build"),RoadWork=W.Activity==TEXT("road_build")||W.Activity==TEXT("to_road");
            if(BuildingWork!=(W.BuildingId>0)||RoadWork!=(W.RoadId>0)||(W.Activity==TEXT("delivery"))!=(W.DeliveryId>0)){Error=TEXT("Worker task has inconsistent physical ownership");return false;}
            if(W.Activity==TEXT("delivery")){const auto* C=S.Couriers.FindByPredicate([&](const auto& X){return X.Id==W.DeliveryId;});if(!C||C->WorkerId!=W.Id||DeliveryClaims.Contains(C->Id)||!W.Position.Equals(C->Position,1.e-6)){Error=TEXT("Delivery worker manifest or position mismatch");return false;}DeliveryClaims.Add(C->Id);}
            else if(W.DeliveryId){Error=TEXT("Worker has two simultaneous assignments");return false;}
        }
        else if(W.Outdoor||W.DeliveryId){Error=TEXT("Inactive worker cannot walk or own delivery work");return false;}
        else if((W.State==TEXT("stored")||W.State==TEXT("shipment")||W.State==TEXT("vehicle")||W.State==TEXT("disassembling"))?W.Activity!=W.State:W.Activity!=TEXT("terminal")){Error=TEXT("Inactive worker lifecycle and activity disagree");return false;}
        if((W.State==TEXT("stored")||W.State==TEXT("disassembling"))&&(W.ContainerKind!=TEXT("building")||!S.FindBuilding(W.ContainerId)||S.FindBuilding(W.ContainerId)->Health<=0)){Error=TEXT("Stored worker has no live physical container");return false;}
        Bodies.Add(W);
    }
    if(NextSerial<=MaxOwnSerial||Active!=S.Population||DeliveryClaims.Num()!=S.Couriers.Num()){Error=TEXT("Saved worker totals or identity sequence disagree");return false;}
    const FString Resource=S.TextRule(TEXT("inactive_worker_resource"));
    for(const auto& B:S.Buildings)
    {
        int32 Disassembling=0,Shipment=0;for(const auto& W:Bodies)if(W.ContainerId==B.Id){if(W.State==TEXT("disassembling"))++Disassembling;if(W.State==TEXT("shipment"))++Shipment;}
        if(StoredAt(B.Id)!=FMath::RoundToInt(B.Inventory.FindRef(Resource))||Disassembling!=(B.DisassemblyCommitted?1:0)||Shipment!=(B.Shipment.Resource==Resource?FMath::RoundToInt(B.Shipment.GoodsEscrow):0)){Error=TEXT("Stored worker inventory disagrees with body manifest");return false;}
    }
    for(const auto& V:S.Combat.Vehicles){int32 Count=0;for(const auto& W:Bodies)if((W.State==TEXT("vehicle")||W.State==TEXT("evacuated"))&&W.ContainerKind==TEXT("vehicle")&&W.ContainerId==V.Id)++Count;if(Count!=FMath::RoundToInt(V.Inventory.FindRef(Resource))){Error=TEXT("Vehicle worker cargo disagrees with body manifest");return false;}}
    for(const auto& C:S.Couriers)
    {
        if(C.SourceDeployment&&(C.SourceId!=S.Core()->Id||C.TargetId!=S.Core()->Id||!C.ForConstruction||C.SelfTransfer)){Error=TEXT("Deployment cargo has an invalid compartment or destination");return false;}
        if(C.SelfTransfer){if(C.Resource!=Resource||C.Amount!=0||C.ReservedAmount!=0){Error=TEXT("Self-relocating worker is not hauled cargo");return false;}}
        else if(C.Amount+C.ReservedAmount<=0||C.Amount>0&&C.ReservedAmount>0){Error=TEXT("Invalid physical cargo reservation");return false;}
        if(C.Phase!=TEXT("pickup")&&C.Phase!=TEXT("loading")&&C.Phase!=TEXT("carrying")&&C.Phase!=TEXT("unloading")&&C.Phase!=TEXT("reactivate_transfer")){Error=TEXT("Invalid delivery lifecycle phase");return false;}
        if(!C.SelfTransfer&&((C.Phase==TEXT("pickup")||C.Phase==TEXT("loading"))!=(C.ReservedAmount>0))){Error=TEXT("Delivery cargo is in the wrong physical phase");return false;}
        if(C.ReservedAmount>0){const auto* B=S.FindBuilding(C.SourceId);const double StockAmount=C.SourceDeployment?DeploymentStock.FindRef(C.Resource):(B?B->Inventory.FindRef(C.Resource):0);if(PickupReserved(S,C.SourceId,C.Resource,C.SourceDeployment)>StockAmount+1.e-6){Error=TEXT("Delivery pickup reservations exceed physical stock");return false;}}
    }
    const auto* Core=S.Core();if(Core&&Core->Health>0)for(const auto& P:DeploymentStock)if(P.Value+Core->ConstructionMaterials.FindRef(P.Key)+Core->InstalledMaterials.FindRef(P.Key)>S.ConstructionCost(*Core).FindRef(P.Key)+1.e-6){Error=TEXT("Deployment kit exceeds its authored bill");return false;}
    int32 Recycled=0;for(const auto& W:Bodies)if(W.State==TEXT("disassembled"))++Recycled;const double Waste=Recycled*(S.Resources[Resource].UnitMassKg-S.InventoryMassKg(S.DisassemblyOutputs()));if(Recycled!=S.WorkersDisassembled||FMath::Abs(Waste-RecyclingWasteKg)>1.e-6){Error=TEXT("Worker recycling mass ledger disagrees");return false;}
    RefreshMetrics(S);Error.Empty();return true;
}
