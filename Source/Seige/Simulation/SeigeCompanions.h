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
    double FedUntil=0,NextMeal=0,RestUntil=0,Heading=0,DistanceWalked=0,FoodConsumedKg=0,ShelterUntil=0;
    double Health=0;
    // Transient locomotion, never persisted as player input in a save.
    double SpeedMetersPerSecond=0,MotionSpeedMetersPerSecond=0;
    bool Moving=false,Evacuated=false,Retreating=false,Sheltered=false,ExitingShuttle=false,Aboard=true,Dead=false;
};

// Companions are not workforce. Physical food is consumed from nearby stores;
// rendering and possession never manufacture population, food or simulation time.
class SEIGE_API FSeigeCompanionSystem
{
public:
    bool Initialize(const FString& RulesDirectory,const FSeigeSimulation& Sim,FString& Error);
    void Tick(FSeigeSimulation& Sim,double Seconds);
    void ShiftHome(FVector2D Delta);
    void NotifyBuildingDamaged(const FSeigeSimulation& Sim,FVector2D Position);
    double EfficiencyAt(FVector2D Position) const;
    void Damage(FSeigeSimulation& Sim,int32 Id,double Amount);
    void Alert(const FSeigeSimulation& Sim,FSeigeCompanion& Dog);
    double WorkerSpeedMultiplier(const FSeigeSimulation& Sim) const;
    static bool IsOutside(const FSeigeCompanion& Dog){return Dog.Health>0&&!Dog.Dead&&!Dog.Aboard&&!Dog.Sheltered&&!Dog.Evacuated;}
    int64 DepressionUntilCalendarUs=0;
    double MaximumHealth=25,DangerRadiusMeters=35,HealPerSecond=5,DepressionDays=30,DepressionSpeedMultiplier=.5;
    bool Save(const TSharedPtr<FJsonObject>& Object) const;
    bool Load(const TSharedPtr<FJsonObject>& Object,const FSeigeSimulation& Sim,FString& Error);
    bool SetControlled(int32 Id);
    void SetControlDirection(FVector2D Direction){ControlDirection=Direction.GetClampedToMaxSize(1.);}
    void SetControlSprint(bool Sprint){ControlSprint=ControlledId>0&&Sprint;}
    void SetControlFacing(double Yaw){if(FMath::IsFinite(Yaw))ControlFacing=FMath::UnwindDegrees(Yaw);}
    void SetMovementConstraint(TFunction<bool(FVector2D,FVector2D,double)> Constraint){MovementConstraint=MoveTemp(Constraint);}
    FSeigeCompanion* Find(int32 Id);
    const FSeigeCompanion* Find(int32 Id) const;
    TArray<FSeigeCompanion> Dogs;
    FString Name=TEXT("Rex"),FoodResource,MeshPath,WalkAnimation,RunAnimation,IdleAnimation,LocomotionBlendSpace;
    double EyeHeightCm=52,ViewFov=85,LookSensitivity=.13,WalkKmh=5,MoraleBonus=.05,MoraleRadiusMeters=60,FoodPerMealKg=.1,MealIntervalSeconds=1800;
    int32 ControlledId=0;
    double WalkCycleMeters=.7,RunCycleMeters=1.3;
    double BodyRadiusMeters=.3,MaximumSlopeGrade=.8;
    double TerritoryRadiusMeters=100,DamageAlertRadiusMeters=100,ShelterSeconds=60,RunKmh=14;
    double AccelerationMetersPerSecondSquared=3,DecelerationMetersPerSecondSquared=5,TurnDegreesPerSecond=240;
private:
    void ResetControl();
    double MetresPerUnit=.06,RoamRadiusMeters=35,FeedRadiusMeters=100,RestSeconds=8,FedDurationSeconds=2400,Clock=0;
    FVector2D ControlDirection=FVector2D::ZeroVector;
    double ControlFacing=0;
    bool ControlSprint=false;
    TFunction<bool(FVector2D,FVector2D,double)> MovementConstraint;
    FRandomStream Random;
};
