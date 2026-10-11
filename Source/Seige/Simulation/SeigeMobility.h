#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;

struct FSeigeMobilityProfile
{
    double BodyRadiusMeters=0,ClearanceMeters=0,AccessMeters=0,SpeedKmh=0,MaximumSlope=0,RoughSlope=0,RoughMultiplier=0,SampleMeters=0;
    bool AllowEasy=true,AllowRough=false,AllowWater=false,AllowRoad=true;
};

// Shared authoritative mobility policy for every physical unit family.
class SEIGE_API FSeigeMobilityRules
{
public:
    bool Load(const FString& Directory,FString& Error);
    bool MovementAllowed(const FSeigeSimulation& Sim,const FString& Profile,FVector2D From,FVector2D To,double AdditionalRadiusMeters=0) const;
    double Speed(const FSeigeSimulation& Sim,const FString& Profile,FVector2D From,FVector2D To,double BaseKmhOverride=0) const;
    double ClearanceMeters(const FString& Profile) const;
    double AccessMeters(const FString& Profile) const;
    void SetTerrainSampler(TFunction<double(FVector2D)> Sampler){TerrainHeight=MoveTemp(Sampler);++TerrainRevision;}
    int32 GetTerrainRevision() const{return TerrainRevision;}
    void InvalidateTerrain(){++TerrainRevision;}
    const FString& GetFingerprint() const{return Fingerprint;}
    const FSeigeMobilityProfile* Profile(const FString& Id) const{return Profiles.Find(Id);}
private:
    TMap<FString,FSeigeMobilityProfile> Profiles;
    FString Fingerprint;
    TFunction<double(FVector2D)> TerrainHeight;
    int32 TerrainRevision=0;
    double Grade(const FSeigeSimulation& Sim,FVector2D From,FVector2D To,double SampleMeters) const;
};
