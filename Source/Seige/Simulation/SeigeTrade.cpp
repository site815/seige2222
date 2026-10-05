#include "SeigeTrade.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"
namespace{bool N(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double& V,double Min=0){return O&&O->TryGetNumberField(K,V)&&FMath::IsFinite(V)&&V>=Min;}}
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
    if(Buy){if(Sim.Credits<Quote(R,Q,true)){Error=TEXT("Not enough credits; export local goods first");return false;}if(Sim.Occupied(*B)+Q*Resource->LitresPerUnit>Sim.Definition(*B)->StorageCapacity+1.e-8){Error=TEXT("Trading port has insufficient import storage");return false;}}
    else{double Available=0;for(const auto& S:Sim.Buildings)if(S.Health>0&&!S.IsConstructing)Available+=S.Id==Id?FMath::Max(0.,S.Inventory.FindRef(R)-Sim.Demand(S,R,false)):FMath::Max(0.,Sim.Spendable(S,R)-Sim.Demand(S,R,false));
        if(R==Sim.TextRule(TEXT("inactive_worker_resource"))){Available=0;for(const auto& S:Sim.Buildings)if(S.Health>0&&!S.IsConstructing)Available+=FMath::Max(0.,S.Inventory.FindRef(R)-(S.DisassemblyQueued-(S.DisassemblyCommitted?1:0))-Demand(Sim,S.Id,R));Available=FMath::Max(0.,Available-Sim.WorkerSurplusTarget-FMath::Max(0,FMath::Min(Sim.TotalJobs,Sim.RobotSupportCapacity)-Sim.Population));}
        if(Available+1.e-8<Q){Error=TEXT("Not enough unreserved goods for export");return false;}if(Q*Resource->LitresPerUnit>Sim.Definition(*B)->StorageCapacity){Error=TEXT("Export exceeds local port storage");return false;}}
    Error.Empty();return true;
}
bool FSeigeTradeSystem::TryTrade(FSeigeSimulation& Sim,int32 Id,const FString& R,double Q,bool Buy,FString& Error)
{if(!CanTrade(Sim,Id,R,Q,Buy,Error))return false;auto* B=Sim.FindBuilding(Id);B->Shipment={};B->Shipment.Resource=R;B->Shipment.Quantity=Q;B->Shipment.Buy=Buy;B->Shipment.PriceCredits=Quote(R,Q,Buy);if(Buy)Sim.Credits-=B->Shipment.PriceCredits;Sim.AddEvent(Buy?TEXT("Import ordered; credits reserved"):TEXT("Export ordered; awaiting local cargo"));return true;}
double FSeigeTradeSystem::Demand(const FSeigeSimulation& Sim,int32 Id,const FString& R)const{const auto* B=Sim.FindBuilding(Id);return B&&!B->Shipment.Resource.IsEmpty()&&!B->Shipment.Buy&&!B->Shipment.Departed&&B->Shipment.Resource==R?B->Shipment.Quantity:0;}
void FSeigeTradeSystem::Tick(FSeigeSimulation& Sim,double Seconds)
{
    for(auto& B:Sim.Buildings){auto& S=B.Shipment;if(S.Resource.IsEmpty()||B.Health<=0||B.IsConstructing||!B.Enabled)continue;const auto* D=Ports.Find(B.DefId);if(!D)continue;const double F=Sim.WorkFraction(B);if(F<=0){B.Status=TEXT("Trade awaiting workers or grid power");continue;}
        if(!S.Departed){if(!S.Buy&&B.Inventory.FindRef(S.Resource)+1.e-8<S.Quantity){B.Status=TEXT("Awaiting export cargo deliveries");continue;}if(!Sim.Energy.Consume(Sim,B.Id,D->EnergyKWh)){B.Status=TEXT("Awaiting shipment energy");continue;}if(!S.Buy){B.Inventory.FindOrAdd(S.Resource)=FMath::Max(0.,B.Inventory.FindRef(S.Resource)-S.Quantity);S.GoodsEscrow=S.Quantity;}S.Departed=true;}
        S.Progress=FMath::Min(1.,S.Progress+Seconds*F/D->ShipmentSeconds);B.Status=TEXT("External trade shipment in flight");
        if(S.Progress>=1.){if(S.Buy)B.Inventory.FindOrAdd(S.Resource)+=S.Quantity;else Sim.Credits+=S.PriceCredits;Sim.AddEvent((S.Buy?TEXT("Import received: "):TEXT("Export paid: "))+Sim.Resources[S.Resource].Name);S={};}
    }
}
