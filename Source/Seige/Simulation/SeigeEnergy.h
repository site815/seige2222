#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;
class FJsonObject;
enum class ESeigeEnergyPurpose { Optional, DefensiveShot };
struct FSeigeEnergyDefinition
{
    double GenerationKW=0, BatteryCapacityKWh=0, InitialBatteryKWh=0, IdleKW=0;
    double FuelUnitsPerKWh=0, FuelBufferSeconds=0;
    FString FuelResource;
    FString GenerationSource=TEXT("constant");
    bool SelfStart=false, RequiresRoadGrid=true;
    int32 Priority=0;
};
struct FSeigeEnergyInfo
{
    int32 ComponentId=INDEX_NONE;
    bool Connected=false;
    double GenerationKW=0, DemandKW=0, SuppliedKW=0, StoredKWh=0, CapacityKWh=0, PowerFraction=0;
};
class SEIGE_API FSeigeEnergySystem
{
public:
    bool Initialize(const TSharedPtr<FJsonObject>& Document,FSeigeSimulation& Sim,FString& Error);
    void Tick(FSeigeSimulation& Sim,double Seconds);
    double Fraction(int32 BuildingId) const;
    bool RoadPowered(int32 RoadId) const;
    bool RoadConnectedToBuilding(int32 RoadId,int32 BuildingId) const;
    double FuelDemand(const FString& DefinitionId,const FString& Resource) const;
    bool CanConsume(const FSeigeSimulation& Sim,int32 BuildingId,double KWh,ESeigeEnergyPurpose Purpose=ESeigeEnergyPurpose::Optional) const;
    bool Consume(FSeigeSimulation& Sim,int32 BuildingId,double KWh,ESeigeEnergyPurpose Purpose=ESeigeEnergyPurpose::Optional);
    void RefreshDefensiveReserve(FSeigeSimulation& Sim,double Seconds);
    double ReservedForDefense(int32 BuildingId) const;
    FSeigeEnergyInfo Info(const FSeigeSimulation& Sim,int32 BuildingId=0) const;
    const FSeigeEnergyDefinition* Definition(const FString& Id) const {return Definitions.Find(Id);}
    void Invalidate(){TopologyRevision=0;DefensiveReserve.Empty();}
    void Save(const TSharedPtr<FJsonObject>& Object) const;
    bool Load(const TSharedPtr<FJsonObject>& Object,FSeigeSimulation& Sim,FString& Error);
    double WorkerKW=0, GeneratedKWh=0, ConsumedKWh=0, SpilledKWh=0;
private:
    struct FGrid {TArray<int32> Buildings,Roads;FSeigeEnergyInfo State;};
    TMap<FString,FSeigeEnergyDefinition> Definitions;
    TArray<FGrid> Grids;
    TMap<int32,int32> BuildingGrid,RoadGrid;
    TMap<int32,double> Fractions,RoadFractions;
    // Derived from current targets/cooldowns; no additional saved energy ledger.
    TMap<int32,double> DefensiveReserve;
    FString DispatchPolicy;
    double ConnectionToleranceMeters=0, MinimumOperatingFraction=1;
    int32 TopologyRevision=0;
    bool Ready=false;
    void Rebuild(FSeigeSimulation& Sim);
    double Stored(const FSeigeSimulation& Sim,const FGrid& Grid) const;
    double Capacity(const FSeigeSimulation& Sim,const FGrid& Grid) const;
    void Distribute(FSeigeSimulation& Sim,const FGrid& Grid,double Energy) const;
};
