#pragma once
#include "CoreMinimal.h"
#include "SeigeEnergy.h"
#include "SeigeTrade.h"
#include "SeigeCompanions.h"
#include "SeigeCombat.h"
#include "SeigeWalls.h"
#include "SeigeWorkers.h"
#include "SeigeWorldCalendar.h"
#include "SeigeEnvironment.h"

struct FSeigeResourceDef
{
    FString Id, Name, StockpileVisual, Class, Unit;
    double UnitMassKg=1, LitresPerUnit=1;
    bool Discrete=false;
    FLinearColor Color = FLinearColor::White;
    int32 Tier = 0;
};
struct FSeigeRecipeDef
{
    FString Id;
    TMap<FString, double> Inputs, Outputs;
    double Seconds = 1;
    double EnergyKWh = 0;
    int32 WorkerOutput = 0;
};
struct FSeigeBuildingDef
{
    FString Id, Name, Description, Visual, Recipe, Role, WeaponName;
    FLinearColor Color = FLinearColor::White;
    TMap<FString, double> Cost;
    int32 Jobs = 0;
    double Health = 1, Footprint = 100, SensorRange = 0, AttackRange = 0, DamagePerSecond = 0;
    TMap<FString,double> ExtractionRates;
    double StorageCapacity = 0;
    // Logical square half-widths: current body versus permanently reserved plot.
    double ReservedFootprint = 100;
    FVector2D AccessPort = FVector2D(1,0);
    bool DeploymentDefense = false;
    double DamagePerShot = 0, ReloadSeconds = 0, PowerUsageKW = 0, PowerGenerationKW = 0;
    double ConstructionSeconds = 1;
    int32 ConstructionWorkers = 1, RobotSupportCapacity = 0, StaffingPriority = 0;
    FString InventoryPresentation, WorkerActivity;
    FString NextUpgrade;
    TMap<FString,double> UpgradeCost;
    FString Family, WorkforceMode;
    int32 Level=1;
    TArray<FString> AllowedRecipes;
    double RecipeTimeMultiplier=1, RecipeEnergyMultiplier=1, RecipeInputMultiplier=1;
    bool StoresInactiveWorkers=false;
};
struct FSeigeBuilding
{
    int32 Id = 0;
    FString DefId, Status;
    int32 DepositId = 0;
    FVector2D Position = FVector2D::ZeroVector;
    double Health = 0, Progress = 0, WeaponCooldown = 0, LastShotTime = -1;
    FVector2D LastShotPosition = FVector2D::ZeroVector;
    bool Enabled = true;
    int32 Workers = 0;
    TMap<FString, double> Inventory;
    bool IsConstructing = false, MaintenanceSupplied = true;
    double ConstructionProgress = 1;
    int32 Builders = 0, BuildersOnSite = 0, TravellingBuilders = 0, SupportedRobots = 0;
    FVector2D BuilderPosition = FVector2D::ZeroVector;
    TArray<FVector2D> BuilderRoute;
    int32 BuilderNextWaypoint = 0, BuilderRouteRevision = 0;
    TMap<FString, double> ConstructionMaterials, InstalledMaterials;
    FString UpgradeTarget;
    TMap<FString,double> PreviousLevelMaterials, ProductionInputs;
    bool ProductionCommitted=false;
    double BatteryEnergyKWh=0, ProductionReservedLitres=0;
    FSeigeTradeShipment Shipment;
    FString SelectedRecipe, CommittedRecipe;
    int32 WorkerExportTarget=0, DisassemblyQueued=0;
    bool DisassemblyCommitted=false;
    double DisassemblyProgress=0, DisassemblyReservedLitres=0;
};
struct FSeigeTransportTier
{
    FString Id, Name, NextTier, Visual;
    double SpeedMultiplier = 1, WidthMeters = 1, ConstructionSecondsPer100Meters = 1, MinimumConstructionSeconds = 1;
    int32 ConstructionWorkers = 1;
    TMap<FString,double> CostPer100Meters;
    double IdleKWPer100Meters=0;
    double HealthPerMeter=1;
};
struct FSeigeTransportSegment
{
    int32 Id = 0;
    FVector2D A = FVector2D::ZeroVector, B = FVector2D::ZeroVector;
    FString Tier, TargetTier;
    bool IsConstructing = true;
    double ConstructionProgress = 0;
    int32 Builders = 0, BuildersOnSite = 0, TravellingBuilders = 0;
    FVector2D BuilderPosition = FVector2D::ZeroVector;
    TArray<FVector2D> BuilderRoute;
    int32 BuilderNextWaypoint = 0, BuilderRouteRevision = 0;
    TMap<FString,double> ConstructionMaterials, InstalledMaterials, PreviousTierMaterials;
    double Health=1, MaxHealth=1;
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
    bool ForConstruction = false;
    int32 RoadTargetId = 0, NextWaypoint = 0, RouteRevision = 0;
    TArray<FVector2D> Route;
    FString WorkerId, Phase=TEXT("pickup");
    double ReservedAmount=0, PhaseSeconds=0;
    bool SourceDeployment=false, SelfTransfer=false;
};
struct FSeigeEnemy
{
    int32 Id = 0;
    FVector2D Position = FVector2D::ZeroVector;
    double Health = 1;
    // Derived presentation state: recomputed by combat, not save authority.
    int32 TargetBuildingId = 0;
    double LastAttackTime = -1;
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
    bool Initialize(const FString& RulesDirectory, FString& Error, bool bBackgroundBugs = true, bool bPeriodicAttacks = true, int32 SeedOverride=INDEX_NONE, FVector2D WorldOffset=FVector2D::ZeroVector);
    bool SetInitialCorePosition(FVector2D Position, FString& Error);
    bool CanSetInitialCorePosition(FVector2D Position, FString& Error) const;
    void Tick(double Seconds);
    bool PlaceBuilding(const FString& DefinitionId, FVector2D Position, FString& Error);
    // Review tooling only (graphics showcase): adds a completed, unpaid, unstaffed
    // building without placement checks. Gameplay never calls this.
    int32 AddReviewBuilding(const FString& DefinitionId, FVector2D Position);
    // Review tooling only: real stored identities (with their inventory units) in a
    // building that stores inactive workers, held there by its export target.
    bool AddReviewStoredWorkers(int32 BuildingId, int32 Count);
    // Review tooling only: one completed wall section of the given level between
    // two points, registered with the wall system like a committed segment.
    int32 AddReviewWall(const FString& DefinitionId, FVector2D A, FVector2D B, bool InsideLeft);
    bool CanPlaceBuilding(const FString& DefinitionId, FVector2D Position, FString& Error) const;
    void ToggleBuilding(int32 Id);
    bool Save(const FString& Filename, FString& Error) const;
    bool Load(const FString& Filename, FString& Error);
    double TotalStock(const FString& Resource) const;
    bool IsVisible(FVector2D Position) const;
    const FSeigeBuildingDef* Definition(const FSeigeBuilding& Building) const;
    const FSeigeNode* ExtractionNode(const FString& DefinitionId,FVector2D Position) const;
    FString ExtractionResource(const FSeigeBuilding& Building) const;
    double ExtractionRate(const FSeigeBuilding& Building) const;
    FSeigeBuilding* FindBuilding(int32 Id);
    const FSeigeBuilding* FindBuilding(int32 Id) const;
    void LaunchShuttle();
    void TriggerWave();
    void AddEvent(const FString& Text);
    FString ObjectiveText() const;
    FString WorkforceStatus() const;
    double FixedStepSeconds() const;
    double WalkingSpeed() const;
    double MetersPerWorldUnit() const;
    FVector2D BuildingAccessPoint(const FSeigeBuilding& Building) const;
    FVector2D RoadAccessPoint(const FSeigeTransportSegment& Road) const;
    FString ConstructionStage(const FSeigeBuilding& Building) const;
    double ConstructionPhaseProgress(const FSeigeBuilding& Building, int32 PhaseIndex) const;
    bool CanUpgradeBuilding(int32 Id,FString& Error) const;
    // Level-1 blueprint an upgraded definition grows from (itself for level 1).
    FString BaseBlueprint(const FString& DefinitionId) const;
    // Materials installed by the levels below DefinitionId: what an in-place
    // upgrade to it keeps as PreviousLevelMaterials.
    TMap<FString,double> PreviousLevelBill(const FString& DefinitionId) const;
    bool UpgradeBuilding(int32 Id,FString& Error);
    void OnBuildingDestroyed(int32 Id);
    void DamageRoad(int32 Id,double Amount);
    bool SetProductionRecipe(int32 BuildingId,const FString& RecipeId,FString& Error);
    TArray<FString> ProductionOptions(int32 BuildingId) const;
    FString ActiveProductionRecipe(const FSeigeBuilding& Building) const;
    TMap<FString,double> ProductionInputs(const FSeigeBuilding& Building,const FString& RecipeId) const;
    double ProductionInputBuffer(const FSeigeBuilding& Building,const FString& RecipeId,const FString& Resource) const;
    double ProductionSeconds(const FSeigeBuilding& Building,const FString& RecipeId) const;
    double ProductionEnergy(const FSeigeBuilding& Building,const FString& RecipeId) const;
    bool CanCommitProduction(const FSeigeBuilding& Building,const FString& RecipeId) const;
    int32 InactiveWorkerCount() const;
    int32 WorkerReserveTarget() const;
    double DisassemblyEnergyKWh() const;
    bool SetWorkerSurplusTarget(int32 Target,FString& Error);
    bool SetPortWorkerTarget(int32 BuildingId,int32 Target,FString& Error);
    bool CanDisassembleWorkers(int32 BuildingId,int32 Count,FString& Error) const;
    bool DisassembleWorkers(int32 BuildingId,int32 Count,FString& Error);
    bool CanTrade(int32 PortId,const FString& Resource,double Quantity,bool Buy,FString& Error) const{return Trade.CanTrade(*this,PortId,Resource,Quantity,Buy,Error);}
    bool TryTrade(int32 PortId,const FString& Resource,double Quantity,bool Buy,FString& Error){return Trade.TryTrade(*this,PortId,Resource,Quantity,Buy,Error);}
    bool CancelPendingExport(int32 PortId,FString& Error){return Trade.CancelPendingExport(*this,PortId,Error);}
    double TradeQuote(const FString& Resource,double Quantity,bool Buy,int32 PortId=0) const{return Trade.Quote(Resource,Quantity,Buy);}
    double InventoryLitres(const TMap<FString,double>& Inventory) const;
    double InventoryMassKg(const TMap<FString,double>& Inventory) const;
    double StorageUsed(const FSeigeBuilding& Building) const{return Occupied(Building);}
    double StorageRoom(const FSeigeBuilding& Building,int32 ArrivingCourierId=0,const FString& ArrivingWorkerId=FString()) const;
    const TMap<FString,double>& ConstructionCost(const FSeigeBuilding& Building) const;
    double ConstructionSeconds(const FSeigeBuilding& Building) const;
    int32 RequiredBuilders(const FSeigeBuilding& Building) const;
    FSeigeEnergySystem Energy;
    FSeigeTradeSystem Trade;
    FSeigeCompanionSystem Companions;
    FSeigeCombatSystem Combat;
    FSeigeWallSystem Walls;
    double Credits=0;
    int32 GenerationSeed=0;
    bool GenerateResourceNodesForSeed(int32 Seed,TArray<FSeigeNode>& OutNodes,FString& Error,FVector2D WorldOffset=FVector2D::ZeroVector) const;
    // Planning may omit affordability only; PlaceRoad always performs the full check.
    bool CanPlaceRoad(FVector2D A, FVector2D B, FString& Error, bool CheckMaterials=true) const;
    bool PlaceRoad(FVector2D A, FVector2D B, FString& Error);
    bool CanUpgradeRoad(int32 Id, FString& Error) const;
    bool UpgradeRoad(int32 Id, FString& Error);
    FSeigeTransportSegment* FindRoad(int32 Id);
    const FSeigeTransportSegment* FindRoad(int32 Id) const;
    TMap<FString,double> RoadCost(FVector2D A, FVector2D B, const FString& Tier) const;
    FString InitialRoadTier() const;
    double MinimumRoadLength() const; // Simulation world units, matching route coordinates.
    double RoadConstructionSeconds(const FSeigeTransportSegment& Road) const;
    bool FindRoute(FVector2D From, FVector2D To, TArray<FVector2D>& Route, double ClearanceOverride=-1, bool RoadPlan=false, const FSeigeBuilding* ProspectivePlot=nullptr) const;
    bool FindRoadRoute(FVector2D From,FVector2D To,TArray<FVector2D>& Route,const FSeigeBuilding* ProspectivePlot=nullptr) const;
    bool ClearRoadLine(FVector2D From,FVector2D To,const FSeigeBuilding* ProspectivePlot=nullptr) const;
    bool IsRoadGridConnected(int32 A,int32 B) const {const auto X=Energy.Info(*this,A),Y=Energy.Info(*this,B);return X.Connected&&Y.Connected&&X.ComponentId==Y.ComponentId;}
    double RouteSpeedMultiplier(FVector2D A, FVector2D B) const;
    int32 CourierRoadId(const FSeigeCourier& Courier) const;
    FVector2D SnapRoadPoint(FVector2D Position) const;
    TMap<FString,FSeigeTransportTier> TransportTiers;
    TArray<FSeigeTransportSegment> Roads;
    double ConstructionAvailable(const FString& Resource) const;
    double OperatingBuffer(const FString& Resource) const;
    double OperatingEfficiency() const { return WorkforceEfficiency; }
    bool HasActiveWork(const FSeigeBuilding& Building) const;
    TArray<FSeigeBuildingInfoRow> BuildingInfo(const FString& DefinitionId, int32 BuildingId = 0, double CentimetersPerUnit = 1) const;

    TMap<FString, FSeigeResourceDef> Resources;
    TMap<FString, FSeigeRecipeDef> Recipes;
    TMap<FString, FSeigeBuildingDef> BuildingDefs;
    TArray<FString> BuildMenu;
    TArray<FSeigeBuilding> Buildings;
    TArray<FSeigeNode> Nodes;
    TArray<FSeigeCourier> Couriers;
    FSeigeWorkerSystem Workers;
    FSeigeWorldCalendar Calendar;
    FSeigeEnvironment Environment;
    double DeploymentElapsed=0;
    bool DeploymentGrounded=false, DeploymentHatchOpen=false;
    TArray<FSeigeEnemy> Enemies;
    TArray<FSeigeEvent> Events;
    FString Title, RulesVersion, CoreDefinition, RulesPath;
    // Scenario settings are chosen before initialization and restored with the save.
    bool BackgroundBugsEnabled = true, PeriodicAttacksEnabled = true;
    double Time = 0, NextWaveTime = 0, WorldHalfSize = 5000;
    int32 Population = 0, TotalJobs = 0, Employed = 0, Wave = 0, LostCouriers = 0;
    int32 RobotSupportCapacity = 0, SupportedPopulation = 0;
    int32 WorkerSurplusTarget=0, WorkersDisassembled=0;
    double DeliveredUnits = 0;
    bool Escaped = false, Won = false, Failed = false;
    TMap<FString, double> ShuttleCargo;
    TMap<FString, double> ProducedUnits;

private:
    friend class FSeigeScenarioAI;
    bool CanPlaceBuildingGeometry(const FString& DefinitionId,FVector2D Position,FString& Error) const;
    bool CanPlaceRoadGeometry(FVector2D A,FVector2D B,FString& Error) const;
    friend class FSeigeEnergySystem;
    friend class FSeigeTradeSystem;
    friend class FSeigeCombatSystem;
    friend class FSeigeWallSystem;
    friend class FSeigeWorkerSystem;
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSeigeTransportNetworkTest;
    friend class FSeigeReplicatorWorkforceTest;
    friend class FSeigeStoredWorkerLifecycleTest;
    friend class FSeigeRobotSupportTest;
    friend class FSeigeRoadRepairTest;
    friend class FSeigeExtractionMineTest;
    friend class FSeigeMineIncomingStorageTest;
    friend class FSeigeLogisticsPriorityTest;
    friend class FSeigeConstructionSourceTest;
    friend class FSeigeRawInputBufferTest;
    friend class FSeigeEnemyContactTest;
    friend class FSeigeFirstPlayableTimelineDiagnostic;
#endif
    TSharedPtr<class FJsonObject> Policy, Scenario, Transport;
    int32 NextId = 1;
    int32 TransportRevision = 1;
    double PopulationClock = 0, DispatchClock = 0, UpkeepClock = 0;
    double WorkerStoreClock=0, WorkerReactivateClock=0;
    double NextRoamTime = 0;
    double WorkforceEfficiency = 1;
    FRandomStream Random;
    TMap<FString, double> CoreReserves;
    FString RulesFingerprint;
    mutable FString CachedRouteTopology;
    mutable TMap<FString,TArray<FVector2D>> CachedRoutes;
    double Number(const FString& Key) const;
    FString TextRule(const FString& Key) const;
    void AllocateWorkers();
    void StepProduction(double Seconds);
    void StepLogistics(double Seconds);
    void StepCombat(double Seconds);
    void StepPopulation(double Seconds);
    void StepConstruction(double Seconds);
    void UpdateSupport();
    bool LoadWorkforce(FString& Error);
    void StepWorkerDisassembly(double Seconds);
    bool StepRecipe(FSeigeBuilding& Building,double Seconds);
    int32 WorkersNeeded() const;
    int32 PendingWorkers() const;
    double ProductionOutputLitres(const FSeigeRecipeDef& Recipe) const;
    TMap<FString,double> DisassemblyOutputs() const;
    double DisassemblyAvailable(const FSeigeBuilding& Building) const;
    double ConstructionReserved(const FString& Resource) const;
    double Spendable(const FSeigeBuilding& Building,const FString& Resource) const;
    bool HasSpendable(const FSeigeBuilding& Building,const TMap<FString,double>& Amounts) const;
    void CheckObjectives();
    FSeigeBuilding* Core();
    const FSeigeBuilding* Core() const;
    double Demand(const FSeigeBuilding& Building, const FString& Resource, bool IncludeCoreReserve) const;
    double DeliveryDemand(const FSeigeBuilding& Building,const FString& Resource,const FString& Category) const;
    TArray<FString> DeliveryPriorities;
    double Incoming(int32 Target, const FString& Resource) const;
    double Occupied(const FSeigeBuilding& Building) const;
    double WorkFraction(const FSeigeBuilding& Building) const;
    void SpawnEnemies(int32 Count);
    bool LoadTransport(const TSharedPtr<FJsonObject>& Document, FString& Error);
    bool WalkRoute(FVector2D& Position, const TArray<FVector2D>& Route, int32& NextWaypoint, double Seconds) const;
    bool ClearWalkingLine(FVector2D A, FVector2D B, double Clearance = 0) const;
    double IncomingRoad(int32 Id, const FString& Resource) const;
    void StepRoadConstruction(double Seconds);
    void StepRoadRepairs(double Seconds);
    double RoadRepairDemand(const FSeigeBuilding& Building,const FString& Resource) const;
    void UpdateConstructionCrew(FVector2D Destination,int32 Assigned,int32& OnSite,int32& Travelling,FVector2D& Position,TArray<FVector2D>& Route,int32& NextWaypoint,int32& RouteRevision,double Seconds,bool Deployment = false);
};
