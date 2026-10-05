#pragma once
#include "CoreMinimal.h"

struct FSeigeResourceDef
{
    FString Id, Name;
    FLinearColor Color = FLinearColor::White;
    int32 Tier = 0;
};
struct FSeigeRecipeDef
{
    FString Id;
    TMap<FString, double> Inputs, Outputs;
    double Seconds = 1;
};
struct FSeigeBuildingDef
{
    FString Id, Name, Category, Description, Visual, Recipe, ExtractResource, Role, WeaponName;
    FLinearColor Color = FLinearColor::White;
    TMap<FString, double> Cost;
    int32 Jobs = 0;
    double Health = 1, Footprint = 100, SensorRange = 0, AttackRange = 0, DamagePerSecond = 0, ExtractRate = 0;
    double StorageCapacity = 0;
    double DamagePerShot = 0, ReloadSeconds = 0, PowerUsageKW = 0, PowerGenerationKW = 0;
};
struct FSeigeBuilding
{
    int32 Id = 0;
    FString DefId, Status;
    FVector2D Position = FVector2D::ZeroVector;
    double Health = 0, Progress = 0, WeaponCooldown = 0, LastShotTime = -1;
    FVector2D LastShotPosition = FVector2D::ZeroVector;
    bool Enabled = true;
    int32 Workers = 0;
    TMap<FString, double> Inventory;
};
struct FSeigeNode
{
    int32 Id = 0;
    FString Resource;
    FVector2D Position = FVector2D::ZeroVector;
};
struct FSeigeCourier
{
    int32 Id = 0, SourceId = 0, TargetId = 0;
    FString Resource;
    double Amount = 0;
    FVector2D Position = FVector2D::ZeroVector;
};
struct FSeigeEnemy
{
    int32 Id = 0;
    FVector2D Position = FVector2D::ZeroVector;
    double Health = 1;
};
struct FSeigeEvent
{
    double Time = 0;
    FString Text;
};
struct FSeigeBuildingInfoRow
{
    FString Section, Label, Value;
};

// Simulation knows no rendering or input. Game-specific definitions and policies are loaded from Rules/*.json.
class SEIGE_API FSeigeSimulation
{
public:
    bool Initialize(const FString& RulesDirectory, FString& Error);
    bool SetInitialCorePosition(FVector2D Position, FString& Error);
    bool CanSetInitialCorePosition(FVector2D Position, FString& Error) const;
    void Tick(double Seconds);
    bool PlaceBuilding(const FString& DefinitionId, FVector2D Position, FString& Error);
    bool CanPlaceBuilding(const FString& DefinitionId, FVector2D Position, FString& Error) const;
    void ToggleBuilding(int32 Id);
    bool Save(const FString& Filename, FString& Error) const;
    bool Load(const FString& Filename, FString& Error);
    double TotalStock(const FString& Resource) const;
    bool IsVisible(FVector2D Position) const;
    const FSeigeBuildingDef* Definition(const FSeigeBuilding& Building) const;
    FSeigeBuilding* FindBuilding(int32 Id);
    const FSeigeBuilding* FindBuilding(int32 Id) const;
    void LaunchShuttle();
    void TriggerWave();
    void AddEvent(const FString& Text);
    FString ObjectiveText() const;
    FString WorkforceStatus() const;
    double FixedStepSeconds() const;
    TArray<FSeigeBuildingInfoRow> BuildingInfo(const FString& DefinitionId, int32 BuildingId = 0, double CentimetersPerUnit = 1) const;

    TMap<FString, FSeigeResourceDef> Resources;
    TMap<FString, FSeigeRecipeDef> Recipes;
    TMap<FString, FSeigeBuildingDef> BuildingDefs;
    TArray<FString> BuildMenu;
    TArray<FSeigeBuilding> Buildings;
    TArray<FSeigeNode> Nodes;
    TArray<FSeigeCourier> Couriers;
    TArray<FSeigeEnemy> Enemies;
    TArray<FSeigeEvent> Events;
    FString Title, RulesVersion, CoreDefinition, RulesPath;
    double Time = 0, NextWaveTime = 0, WorldHalfSize = 5000;
    int32 Population = 0, TotalJobs = 0, Employed = 0, Wave = 0, LostCouriers = 0;
    double DeliveredUnits = 0;
    bool Escaped = false, Won = false, Failed = false;
    TMap<FString, double> ShuttleCargo;
    TMap<FString, double> ProducedUnits;

private:
    TSharedPtr<class FJsonObject> Policy, Scenario;
    int32 NextId = 1;
    double PopulationClock = 0, DispatchClock = 0, UpkeepClock = 0;
    double NextRoamTime = 0;
    double WorkforceEfficiency = 1;
    FRandomStream Random;
    TMap<FString, double> CoreReserves;
    FString RulesFingerprint;
    double Number(const FString& Key) const;
    FString TextRule(const FString& Key) const;
    void AllocateWorkers();
    void StepProduction(double Seconds);
    void StepLogistics(double Seconds);
    void StepCombat(double Seconds);
    void StepPopulation(double Seconds);
    void CheckObjectives();
    FSeigeBuilding* Core();
    const FSeigeBuilding* Core() const;
    double Demand(const FSeigeBuilding& Building, const FString& Resource, bool IncludeCoreReserve) const;
    double Incoming(int32 Target, const FString& Resource) const;
    double Occupied(const FSeigeBuilding& Building) const;
    double WorkFraction(const FSeigeBuilding& Building) const;
    void SpawnEnemies(int32 Count);
};
