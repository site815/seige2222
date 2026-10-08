#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;
class FJsonObject;
struct FSeigeBuilding;
struct FSeigeTradeShipment
{
    FString Resource;
    bool Buy=false, Departed=false;
    double Quantity=0, PriceCredits=0, Progress=0, GoodsEscrow=0;
};
struct FSeigeTradePrice {double BuyCredits=0,SellCredits=0;};
struct FSeigeTradePortDefinition {int32 Level=1;double CapacityKg=0,ShipmentSeconds=0,EnergyKWh=0;};
class SEIGE_API FSeigeTradeSystem
{
public:
    bool Initialize(const TSharedPtr<FJsonObject>& Document,const FSeigeSimulation& Sim,FString& Error);
    bool CanTrade(const FSeigeSimulation& Sim,int32 PortId,const FString& Resource,double Quantity,bool Buy,FString& Error) const;
    bool TryTrade(FSeigeSimulation& Sim,int32 PortId,const FString& Resource,double Quantity,bool Buy,FString& Error);
    bool CancelPendingExport(FSeigeSimulation& Sim,int32 PortId,FString& Error);
    double Quote(const FString& Resource,double Quantity,bool Buy) const;
    void Tick(FSeigeSimulation& Sim,double Seconds);
    double Demand(const FSeigeSimulation& Sim,int32 BuildingId,const FString& Resource) const;
    const FSeigeTradePortDefinition* Definition(const FString& Id) const{return Ports.Find(Id);}
    TMap<FString,FSeigeTradePrice> Prices;
private:
    double ExportableStock(const FSeigeSimulation& Sim,const FSeigeBuilding& Building,const FString& Resource,bool OwnShipment=false) const;
    TMap<FString,FSeigeTradePortDefinition> Ports;
};
