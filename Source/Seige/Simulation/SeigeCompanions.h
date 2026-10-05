#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;
class FJsonObject;

struct FSeigeCompanion
{
    int32 Id=1;
    FVector2D Position=FVector2D::ZeroVector,Home=FVector2D::ZeroVector,Target=FVector2D::ZeroVector;
    TArray<FVector2D> Route;
    int32 NextWaypoint=0;
    double FedUntil=0,NextMeal=0,RestUntil=0,Heading=0,DistanceWalked=0,FoodConsumedKg=0;
    bool Moving=false,Evacuated=false;
};

// Companions are not workforce. Physical food is consumed from nearby stores;
// rendering and possession never manufacture population, food or simulation time.
class SEIGE_API FSeigeCompanionSystem
{
public:
    bool Initialize(const FString& RulesDirectory,const FSeigeSimulation& Sim,FString& Error);
    void Tick(FSeigeSimulation& Sim,double Seconds);
    void ShiftHome(FVector2D Delta);
    double EfficiencyAt(FVector2D Position) const;
    bool Save(const TSharedPtr<FJsonObject>& Object) const;
    bool Load(const TSharedPtr<FJsonObject>& Object,const FSeigeSimulation& Sim,FString& Error);
    bool SetControlled(int32 Id);
    void SetControlDirection(FVector2D Direction){ControlDirection=Direction.GetClampedToMaxSize(1.);}
    FSeigeCompanion* Find(int32 Id);
    const FSeigeCompanion* Find(int32 Id) const;
    TArray<FSeigeCompanion> Dogs;
    FString Name=TEXT("Rex"),FoodResource,MeshPath,WalkAnimation,IdleAnimation;
    double EyeHeightCm=52,ViewFov=85,LookSensitivity=.13,WalkKmh=5,MoraleBonus=.05,MoraleRadiusMeters=60,FoodPerMealKg=.1,MealIntervalSeconds=1800;
    int32 ControlledId=0;
    double WalkCycleMeters=.6018518518518519;
    double BodyRadiusMeters=.3,MaximumSlopeGrade=.8;
private:
    double MetresPerUnit=.06,RoamRadiusMeters=35,FeedRadiusMeters=100,RestSeconds=8,FedDurationSeconds=2400,Clock=0;
    FVector2D ControlDirection=FVector2D::ZeroVector;
    FRandomStream Random;
};
