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
    bool Initialize(FSeigeSimulation& Colony, const FString& RulesDirectory, const FString& AIDirectory, bool bDeveloped, FString& Error);
    void Tick(FSeigeSimulation& Colony, double Seconds);
    FString GetConfigFingerprint() const { return ConfigFingerprint; }
    FString GetStatus() const { return Status; }
    bool IsReady() const { return Ready; }

private:
    bool Ready = false;
    FString ConfigFingerprint, Status, SensorDefinition;
    double DecisionInterval = 1, RingStart = 0, RingStep = 1, RingLimit = 1, NodeClearance = 0, SensorOverlap = 0, DefenseDistance = 0;
    int32 Angles = 0, MaxActions = 0, MaxSensors = 0, DevelopedSetupLimit = 0;
    TArray<FSeigeAIBuildTarget> Targets;
    bool LoadConfig(const FSeigeSimulation& Colony, const FString& Directory, FString& Error);
    bool MakeDecision(FSeigeSimulation& Colony);
    bool BuildNear(FSeigeSimulation& Colony, const FString& Definition, FVector2D Anchor, double StartingAngle);
    bool ExtendSensors(FSeigeSimulation& Colony, FVector2D Destination);
    int32 CountLive(const FSeigeSimulation& Colony, const FString& Definition) const;
};
