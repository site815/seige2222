#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;
class FJsonObject;
struct FSeigeCourier;

struct FSeigeTransportVehicleDef
{
    FString Name;
    double CargoMassKg=0,CargoLitres=0,BodyMassKg=0,Health=0,BatteryKWh=0,InitialBatteryKWh=0;
    double TravelKWhPerKm=0,ChargingKW=0,HandlingSecondsPerDroneLoad=0,BuildSeconds=0,BuildEnergyKWh=0,ServiceMeters=0,MinimumDepartureChargeFraction=0;
    int32 ShuttleVehicleCapacity=1,MaximumQueue=1;
    FVector BodySizeMeters=FVector::ZeroVector;
    TMap<FString,double> BuildCost,Factories;
};
struct FSeigeTransportVehicle
{
    int32 Id=0,DeliveryId=0,NextWaypoint=0,RouteRevision=0;
    double Health=0,BatteryKWh=0,DistanceMeters=0,RetryAt=0;
    bool Embarked=false,Evacuated=false,ReturningToShuttle=false;
    FVector2D Position=FVector2D::ZeroVector,Heading=FVector2D(1,0);
    FString HandlerWorkerId,Status;
    TArray<FVector2D> Route;
};
struct FSeigeTransportFabrication
{
    int32 Id=0,FactoryId=0;
    bool Committed=false;
    double ProgressSeconds=0;
};

// Cargo has exactly one owner: the referenced courier's amount/reservation.
// Vehicle bodies never duplicate that inventory, and handlers remain workers.
class SEIGE_API FSeigeTransportVehicleSystem
{
public:
    TArray<FSeigeTransportVehicle> Bodies;
    TArray<FSeigeTransportFabrication> Jobs;
    bool Initialize(const FString& Directory,FSeigeSimulation& Sim,FString& Error);
    const FSeigeTransportVehicleDef& Definition() const{return Def;}
    const FString& GetFingerprint() const{return Fingerprint;}
    FSeigeTransportVehicle* Find(int32 Id);
    const FSeigeTransportVehicle* Find(int32 Id) const;
    int32 LiveCount() const;
    int32 EmbarkedCount() const;
    double HaulUnits(const FSeigeSimulation& Sim,const FString& Resource) const;
    bool Available(const FSeigeSimulation& Sim) const;
    bool Dispatch(FSeigeSimulation& Sim,int32 Source,int32 Target,int32 Road,const FString& Resource,double Amount,bool Construction,bool Deployment=false);
    void Tick(FSeigeSimulation& Sim,double Seconds);
    void ShiftHome(FVector2D Delta);
    void OnBuildingDestroyed(FSeigeSimulation& Sim,int32 BuildingId);
    void Damage(FSeigeSimulation& Sim,int32 VehicleId,double Amount);
    void CompleteEmptyDelivery(FSeigeSimulation& Sim,int32 VehicleId);
    bool BoardShuttle(FSeigeSimulation& Sim,int32 VehicleId,FString& Error);
    int32 Evacuate(FSeigeSimulation& Sim,int32 Capacity);
    bool CanQueueVehicle(const FSeigeSimulation& Sim,int32 FactoryId,FString& Error) const;
    bool QueueVehicle(FSeigeSimulation& Sim,int32 FactoryId,FString& Error);
    double Demand(const FSeigeSimulation& Sim,int32 FactoryId,const FString& Resource) const;
    double CommittedStock(const FString& Resource) const;
    bool Save(const TSharedPtr<FJsonObject>& Root) const;
    bool Load(const TSharedPtr<FJsonObject>& Root,FSeigeSimulation& Sim,FString& Error);
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSeigeCargoMovementParityTest;
    friend class FSeigeRoadMovementRegressionTest;
    friend class FSeigeRoadlessCrewAccessTest;
#endif
    FSeigeTransportVehicleDef Def;
    FString Fingerprint;
    int32 NextId=1,NextJobId=1;
    TSet<FString> RejectedServices;
    int32 ServiceGeometryRevision=INDEX_NONE,ServiceTerrainRevision=INDEX_NONE;
    void RefreshServiceFailures(const FSeigeSimulation& Sim);
    void ReleaseHandler(FSeigeSimulation& Sim,FSeigeTransportVehicle& V);
    bool FindServiceRoute(FSeigeSimulation& Sim,FVector2D From,FVector2D Port,FVector2D Heading,TArray<FVector2D>& Route) const;
    bool Move(FSeigeSimulation& Sim,FSeigeTransportVehicle& V,FVector2D Destination,double Seconds);
    void StepDelivery(FSeigeSimulation& Sim,FSeigeTransportVehicle& V,FSeigeCourier& C,double Seconds);
    void FinishDelivery(FSeigeSimulation& Sim,FSeigeTransportVehicle& V,int32 CourierId);
    void StepFabrication(FSeigeSimulation& Sim,double Seconds);
};
