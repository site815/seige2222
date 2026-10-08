#include "SeigeTrade.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"
namespace
{
bool N(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double& V,double Min=0){return O&&O->TryGetNumberField(K,V)&&FMath::IsFinite(V)&&V>=Min;}
}
double FSeigeTradeSystem::ExportableStock(const FSeigeSimulation& Sim,const FSeigeBuilding& B,const FString& Resource,bool OwnShipment) const
{
    const double ExportDemand=Sim.Trade.Demand(Sim,B.Id,Resource);
    // A port may sell its worker export buffer. The colony reserve is checked
    // separately; queued recycling and other shipments still retain their stock.
    const double OtherDemand=Resource==Sim.TextRule(TEXT("inactive_worker_resource"))
        ? FMath::Max(0,B.DisassemblyQueued-(B.DisassemblyCommitted?1:0))+(OwnShipment?0.:ExportDemand)
        : FMath::Max(0.,Sim.Demand(B,Resource,false)-(OwnShipment?ExportDemand:0.));
    // Accepted construction bills and physical workers walking to a pickup
    // claim the same local inventory, including inventory already at this port.
    return FMath::Max(0.,Sim.Spendable(B,Resource)-OtherDemand);
}
bool FSeigeTradeSystem::Initialize(const TSharedPtr<FJsonObject>& Doc,const FSeigeSimulation& Sim,FString& Error)
{
    Prices.Empty();Ports.Empty();const TSharedPtr<FJsonObject>* O=nullptr,*Ps=nullptr,*Ds=nullptr;
    if(!Doc->TryGetObjectField(TEXT("trade"),O)||!(*O)->TryGetObjectField(TEXT("prices"),Ps)||!(*O)->TryGetObjectField(TEXT("ports"),Ds)){Error=TEXT("Missing trade policy");return false;}
    for(const auto& P:(*Ps)->Values){const FString Id(P.Key);FSeigeTradePrice V;if(!Sim.Resources.Contains(Id)||!N(P.Value->AsObject(),TEXT("buy_credits"),V.BuyCredits,1.e-9)||!N(P.Value->AsObject(),TEXT("sell_credits"),V.SellCredits,1.e-9)||V.SellCredits>V.BuyCredits){Error=TEXT("Invalid external trade price: ")+Id;return false;}Prices.Add(Id,V);}
    for(const auto& P:(*Ds)->Values){const FString Id(P.Key);FSeigeTradePortDefinition V;double Level=0;if(!Sim.BuildingDefs.Contains(Id)||Sim.BuildingDefs[Id].Role!=TEXT("trade")||!N(P.Value->AsObject(),TEXT("level"),Level,1)||Level>3||Level!=FMath::FloorToDouble(Level)||!N(P.Value->AsObject(),TEXT("capacity_kg"),V.CapacityKg,1.e-9)||!N(P.Value->AsObject(),TEXT("shipment_seconds"),V.ShipmentSeconds,1.e-9)||!N(P.Value->AsObject(),TEXT("energy_kwh"),V.EnergyKWh)){Error=TEXT("Invalid trade port: ")+Id;return false;}V.Level=int32(Level);Ports.Add(Id,V);}
    if(Prices.Num()!=Sim.Resources.Num()||Ports.Num()!=3){Error=TEXT("Trade requires a price for every cargo and three port levels");return false;}return true;
}
double FSeigeTradeSystem::Quote(const FString& Id,double Q,bool Buy)const{const auto* P=Prices.Find(Id);return P&&FMath::IsFinite(Q)&&Q>0?Q*(Buy?P->BuyCredits:P->SellCredits):0;}
bool FSeigeTradeSystem::CanTrade(const FSeigeSimulation& Sim,int32 Id,const FString& R,double Q,bool Buy,FString& Error)const
{
    if(Sim.Escaped||Sim.Failed){Error=TEXT("Trading is unavailable after colony evacuation");return false;}
    const auto* B=Sim.FindBuilding(Id);const auto* D=B?Ports.Find(B->DefId):nullptr;const auto* Resource=Sim.Resources.Find(R);
    if(!B||!D||B->Health<=0||B->IsConstructing||!B->Enabled){Error=TEXT("A completed enabled trading port is required");return false;}
    if(!Resource||!Prices.Contains(R)||(Resource->Discrete&&Q!=FMath::FloorToDouble(Q))||!FMath::IsFinite(Q)||Q<=0||Q*Resource->UnitMassKg>D->CapacityKg+1.e-8){Error=TEXT("Shipment exceeds port cargo capacity or has an invalid resource");return false;}
    if(!B->Shipment.Resource.IsEmpty()){Error=TEXT("This trading port already has a shipment");return false;}
    if(Buy){if(Sim.Credits<Quote(R,Q,true)){Error=TEXT("Not enough credits; export local goods first");return false;}if(Q*Resource->LitresPerUnit>Sim.StorageRoom(*B)+1.e-8){Error=TEXT("Trading port has insufficient unreserved import storage");return false;}}
    else{double Available=0;for(const auto& S:Sim.Buildings)if(S.Health>0&&!S.IsConstructing)Available+=ExportableStock(Sim,S,R);
        if(R==Sim.TextRule(TEXT("inactive_worker_resource")))Available=FMath::Max(0.,Available-Sim.WorkerSurplusTarget-FMath::Max(0,FMath::Min(Sim.TotalJobs,Sim.RobotSupportCapacity)-Sim.Population));
        if(Available+1.e-8<Q){Error=TEXT("Not enough unreserved goods for export");return false;}if(Q*Resource->LitresPerUnit>Sim.Definition(*B)->StorageCapacity){Error=TEXT("Export exceeds local port storage");return false;}}
    Error.Empty();return true;
}
bool FSeigeTradeSystem::TryTrade(FSeigeSimulation& Sim,int32 Id,const FString& R,double Q,bool Buy,FString& Error)
{if(!CanTrade(Sim,Id,R,Q,Buy,Error))return false;auto* B=Sim.FindBuilding(Id);B->Shipment={};B->Shipment.Resource=R;B->Shipment.Quantity=Q;B->Shipment.Buy=Buy;B->Shipment.PriceCredits=Quote(R,Q,Buy);if(Buy)Sim.Credits-=B->Shipment.PriceCredits;Sim.AddEvent(Buy?TEXT("Import ordered; credits reserved"):TEXT("Export ordered; awaiting local cargo"));return true;}

bool FSeigeTradeSystem::CancelPendingExport(FSeigeSimulation& Sim,int32 Id,FString& Error)
{
    auto* B=Sim.FindBuilding(Id);
    if(Sim.Escaped||Sim.Failed||!B||B->Health<=0||!Ports.Contains(B->DefId)||B->Shipment.Resource.IsEmpty()||B->Shipment.Buy||B->Shipment.Departed||B->Shipment.GoodsEscrow>0)
    {Error=TEXT("Only an export still awaiting local cargo can be cancelled");return false;}
    // No money or cargo has left yet. Existing delivery bodies and their cargo
    // keep their ordinary destination; cancellation only releases future demand.
    B->Shipment={};Error.Empty();Sim.AddEvent(TEXT("Pending export cancelled; local goods retained"));return true;
}
double FSeigeTradeSystem::Demand(const FSeigeSimulation& Sim,int32 Id,const FString& R)const{const auto* B=Sim.FindBuilding(Id);return B&&!B->Shipment.Resource.IsEmpty()&&!B->Shipment.Buy&&!B->Shipment.Departed&&B->Shipment.Resource==R?B->Shipment.Quantity:0;}
void FSeigeTradeSystem::Tick(FSeigeSimulation& Sim,double Seconds)
{
    for(auto& B:Sim.Buildings){auto& S=B.Shipment;if(S.Resource.IsEmpty()||B.Health<=0||B.IsConstructing||!B.Enabled)continue;const auto* D=Ports.Find(B.DefId);if(!D)continue;const double F=Sim.WorkFraction(B);if(F<=0){B.Status=TEXT("Trade awaiting workers or grid power");continue;}
        if(!S.Departed){if(!S.Buy&&ExportableStock(Sim,B,S.Resource,true)+1.e-8<S.Quantity){B.Status=TEXT("Awaiting unreserved export cargo deliveries");continue;}if(!Sim.Energy.Consume(Sim,B.Id,D->EnergyKWh)){B.Status=TEXT("Awaiting shipment energy");continue;}if(!S.Buy){if(S.Resource==Sim.TextRule(TEXT("inactive_worker_resource"))&&!Sim.Workers.MoveStored(B.Id,TEXT("shipment"),B.Id,FMath::RoundToInt(S.Quantity)))continue;B.Inventory.FindOrAdd(S.Resource)=FMath::Max(0.,B.Inventory.FindRef(S.Resource)-S.Quantity);S.GoodsEscrow=S.Quantity;}S.Departed=true;}
        S.Progress=FMath::Min(1.,S.Progress+Seconds*F/D->ShipmentSeconds);B.Status=TEXT("External trade shipment in flight");
        if(S.Progress>=1.){if(S.Buy){B.Inventory.FindOrAdd(S.Resource)+=S.Quantity;if(S.Resource==Sim.TextRule(TEXT("inactive_worker_resource")))Sim.Workers.NewStored(Sim,B.Id,FMath::RoundToInt(S.Quantity));}else{Sim.Credits+=S.PriceCredits;if(S.Resource==Sim.TextRule(TEXT("inactive_worker_resource")))for(auto& W:Sim.Workers.Bodies)if(W.State==TEXT("shipment")&&W.ContainerId==B.Id){W.State=TEXT("exported");W.Activity=TEXT("terminal");W.ContainerKind=TEXT("external");}}Sim.AddEvent((S.Buy?TEXT("Import received: "):TEXT("Export paid: "))+Sim.Resources[S.Resource].Name);S={};}
    }
}
