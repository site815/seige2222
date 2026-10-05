#pragma once
#include "CoreMinimal.h"
#include "Simulation/SeigeSimulation.h"

struct FSeigeAIBuildTarget
{
    FString Definition;
    int32 Count = 0;
};

// A deterministic colony controller. Owns no simulation and gives no resources during play.
// Initialize then load the colony snapshot to resume; decision cadence derives from Sim.Time.
class SEIGE_API FSeigeScenarioAI
{
public:
    bool Initialize(FSeigeSimulation& Colony, const FString& RulesDirectory, const FString& AIDirectory, bool bDeveloped, FString& Error, bool bBackgroundBugs = true, bool bPeriodicAttacks = true, int32 SeedOverride = INDEX_NONE);
    bool BeginInitialize(FSeigeSimulation& Candidate,const FString& RulesDirectory,const FString& AIDirectory,bool bDeveloped,FString& Error,bool bBackgroundBugs=true,bool bPeriodicAttacks=true,int32 SeedOverride=INDEX_NONE);
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
    friend class FSeigeAIDevelopedContinuationDiagnostic;
#endif
    bool Ready = false;
    bool Preparing=false;
    double PreparationChunkRemaining=0;
    int32 PreparationBestCompleted=0;
    double PreparationLastReportTime=0;
    FString ConfigFingerprint, Status, SensorDefinition, DecisionSchedulingPolicy;
    double DecisionInterval = 1, RingStart = 0, RingStep = 1, RingLimit = 1, NodeClearance = 0, SensorOverlap = 0, DefenseDistance = 0;
    double DevelopedSetupSeconds = 0;
    int32 Angles = 0, MaxActions = 0, MaxSensors = 0, DevelopedSetupLimit = 0;
    TArray<FSeigeAIBuildTarget> Targets;
    FString SolarDefinition,TradeDefinition;
    double ExportBatch=20,ImportBatch=10;
    double RecipeInputBuffer=4,CreditBufferBatches=3;
    double GuardRechargeBelow=0,GuardResumeAbove=0;
    TMap<FString,double> ReserveTargets;
    bool LoadConfig(const FSeigeSimulation& Colony, const FString& Directory, FString& Error);
    bool MakeDecision(FSeigeSimulation& Colony);
    void RunDecisionCycle(FSeigeSimulation& Colony);
    void ReportPreparationMilestone(const FSeigeSimulation& Colony);
    bool BuildNear(FSeigeSimulation& Colony, const FString& Definition, FVector2D Anchor, double StartingAngle, double FirstRadius=-1);
    bool ExtendSensors(FSeigeSimulation& Colony, FVector2D Destination);
    int32 CountLive(const FSeigeSimulation& Colony, const FString& Definition) const;
    const FSeigeNode* ExportNode(const FSeigeSimulation& Colony) const;
    bool ConnectPowerRoad(FSeigeSimulation& Colony,bool& Waiting);
    bool ManageTrade(FSeigeSimulation& Colony);
    bool GuardWorksite(FSeigeSimulation& Colony);
    bool RecoverWorkerSupport(FSeigeSimulation& Colony,bool& Waiting);
    const FSeigeAIBuildTarget* SupportRecoveryTarget(const FSeigeSimulation& Colony) const;
    const FSeigeAIBuildTarget* NextConstructionTarget(const FSeigeSimulation& Colony) const;
    bool DevelopmentComplete(const FSeigeSimulation& Colony) const;
    bool FinishPreparation(FSeigeSimulation& Colony,FString& Error);
};
