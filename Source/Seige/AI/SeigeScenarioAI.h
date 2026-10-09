#pragma once
#include "CoreMinimal.h"
#include "Simulation/SeigeSimulation.h"

struct FSeigeAIBuildTarget
{
    FString Definition;
    int32 Count = 0;
    int32 PlacementIndex = 0;
    // Optional targets never hold back later work while their bill is short.
    bool Optional = false;
};
struct FSeigeEstablishedBuilding
{
    FString Definition, Anchor, Recipe;
    FVector2D OffsetMeters=FVector2D::ZeroVector;
    TMap<FString,double> Inventory;
    int32 Operators=0;
    double BatteryKWh=0;
};

// A deterministic colony controller. Owns no simulation and gives no resources during play.
// Initialize then load the colony snapshot to resume; decision cadence derives from Sim.Time.
class SEIGE_API FSeigeScenarioAI
{
public:
    bool Initialize(FSeigeSimulation& Colony, const FString& RulesDirectory, const FString& AIDirectory, bool bDeveloped, FString& Error, bool bBackgroundBugs = true, bool bPeriodicAttacks = true, int32 SeedOverride = INDEX_NONE, FVector2D WorldOffset=FVector2D::ZeroVector);
    bool BeginInitialize(FSeigeSimulation& Candidate,const FString& RulesDirectory,const FString& AIDirectory,bool bDeveloped,FString& Error,bool bBackgroundBugs=true,bool bPeriodicAttacks=true,int32 SeedOverride=INDEX_NONE,FVector2D WorldOffset=FVector2D::ZeroVector);
    bool AdvancePreparation(FSeigeSimulation& Candidate,double MaxWallMilliseconds,bool& Complete,FString& Error);
    bool IsPreparing() const {return Preparing;}
    double PreparationLimit() const {return DevelopedSetupSeconds;}
    void Tick(FSeigeSimulation& Colony, double Seconds);
    FString GetConfigFingerprint() const { return ConfigFingerprint; }
    FString GetStatus() const { return Status; }
    bool IsReady() const { return Ready&&!Preparing; }
    bool IncludesTarget(const FSeigeSimulation& Colony,const FString& Definition) const;

private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSeigeAISupportRecoveryTest;
    friend class FSeigeAIIncrementalPreparationTest;
    friend class FSeigeAISupportTradeRecoveryTest;
    friend class FSeigeAIGenericTradeRecoveryTest;
    friend class FSeigeAIRoadTradePriorityTest;
    friend class FSeigeAIGuardServiceTest;
    friend class FSeigeAIFuelSupplyTest;
    friend class FSeigeAIConnectedPlacementTest;
    friend class FSeigeAISelectedRecipeTradeTest;
    friend class FSeigeAISupportPendingRoadTest;
    friend class FSeigeAIImpossibleExportTest;
    friend class FSeigeAIReplicatorBootstrapTest;
    friend class FSeigeAIBulkFeedstockTest;
    friend class FSeigeAISurplusExportTest;
    friend class FSeigeAIDevelopedContinuationDiagnostic;
    friend class FSeigeAICoveredPlacementTest;
    friend class FSeigeAIUpgradeTest;
#endif
    bool Ready = false;
    bool Preparing=false;
    double PreparationChunkRemaining=0;
    int32 PreparationBestCompleted=0;
    double PreparationLastReportTime=0;
    FString ConfigFingerprint, Status, SensorDefinition, DecisionSchedulingPolicy;
    double DecisionInterval = 1, RingStart = 0, RingStep = 1, RingLimit = 1, NodeClearance = 0, SensorOverlap = 0, DefenseDistance = 0;
    FString DefenseCoveragePolicy;
    int32 CoverageSamples=0;
    double CoverageProbeMeters=0;
    TSet<FString> CoverageExcludedRoles;
    double DevelopedSetupSeconds = 0;
    FString DevelopedInitialization, EstablishedRoadTier;
    double EstablishedAge=0,EstablishedCredits=0;
    int32 EstablishedIdle=0,EstablishedRotations=4;
    TArray<FSeigeEstablishedBuilding> EstablishedBuildings;
    bool LoadEstablishedPreset(const FSeigeSimulation& Colony,const TSharedPtr<FJsonObject>& Preset,FString& Error);
    bool InitializeEstablished(FSeigeSimulation& Colony,FString& Error);
    bool TryEstablishedLayout(FSeigeSimulation& Candidate,int32 DepositId,FVector2D CorePosition,double Rotation,FString& Error);
    int32 Angles = 0, MaxActions = 0, MaxSensors = 0, DevelopedSetupLimit = 0;
    TArray<FSeigeAIBuildTarget> Targets;
    FString SolarDefinition,TradeDefinition;
    double ExportBatch=20,ImportBatch=10;
    double RecipeInputBuffer=4,CreditBufferBatches=3;
    double FuelImportBufferCycles=1,FuelImportRefillFraction=1;
    TArray<FString> CoreReplicationRecipes;
    FString BulkInputPolicy,SurplusExportPolicy;
    double GuardRechargeBelow=0,GuardResumeAbove=0;
    TMap<FString,double> ReserveTargets;
    TArray<FString> UpgradeFamilies;
    int32 MaxConcurrentUpgrades=0;
    double UpgradeQuietRadius=0;
    bool LoadConfig(const FSeigeSimulation& Colony, const FString& Directory, FString& Error);
    bool MakeDecision(FSeigeSimulation& Colony);
    void RunDecisionCycle(FSeigeSimulation& Colony);
    void ReportPreparationMilestone(const FSeigeSimulation& Colony);
    bool BuildNear(FSeigeSimulation& Colony, const FString& Definition, FVector2D Anchor, double StartingAngle, double FirstRadius=-1, bool PreferShortRoad=false);
    int32 PlotDefenseCoverage(const FSeigeSimulation& Colony,const FSeigeBuildingDef& Definition,FVector2D Position) const;
    bool PlaceConnectedBuilding(FSeigeSimulation& Colony,const FString& Definition,FVector2D Position,FString& Error);
    bool FindPowerConnection(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuilding* ProspectivePlot,FVector2D& OutA,FVector2D& OutB,bool& NeedsSegment,FString& Error) const;
    bool ExtendSensors(FSeigeSimulation& Colony, FVector2D Destination);
    int32 CountLive(const FSeigeSimulation& Colony, const FString& Definition) const;
    // True for the target definition itself or a higher level of its family,
    // so an upgraded building still satisfies its plan entry.
    bool Meets(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FString& Definition) const;
    bool ManageUpgrades(FSeigeSimulation& Colony);
    const FSeigeNode* ExportNode(const FSeigeSimulation& Colony) const;
    bool NextPowerRoad(const FSeigeSimulation& Colony,FVector2D& OutA,FVector2D& OutB,FString& TargetName,FString& Error) const;
    bool ConnectPowerRoad(FSeigeSimulation& Colony,bool& Waiting);
    bool ManageTrade(FSeigeSimulation& Colony);
    bool ManageCoreProduction(FSeigeSimulation& Colony);
    TMap<FString,double> ProductionGoals(const FSeigeSimulation& Colony) const;
    TMap<FString,double> UsefulFeedstockTargets(const FSeigeSimulation& Colony,const TMap<FString,double>& Goals) const;
    double ManufacturedExportSurplus(const FSeigeSimulation& Colony,const FString& Resource,double NextRoadBill=0) const;
    bool GuardWorksite(FSeigeSimulation& Colony);
    bool RecoverWorkerSupport(FSeigeSimulation& Colony,bool& Waiting);
    const FSeigeAIBuildTarget* SupportRecoveryTarget(const FSeigeSimulation& Colony) const;
    const FSeigeAIBuildTarget* NextConstructionTarget(const FSeigeSimulation& Colony) const;
    bool DevelopmentComplete(const FSeigeSimulation& Colony) const;
    bool FinishPreparation(FSeigeSimulation& Colony,FString& Error);
};
