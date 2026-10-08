#pragma once
#include "CoreMinimal.h"

class FSeigeSimulation;
class FJsonObject;
struct FSeigeBuilding;
struct FSeigeCourier;

// One record is one manufactured body. Cargo tasks and workstations reference
// this identity; neither logistics nor rendering manufactures another worker.
struct FSeigeWorker
{
    FString Id, State=TEXT("active"), Activity=TEXT("aboard");
    FVector2D Position=FVector2D::ZeroVector, Heading=FVector2D(1,0);
    int32 BuildingId=0, RoadId=0, ContainerId=0, DeliveryId=0;
    int32 StationSlot=0;
    FString ContainerKind=TEXT("building");
    TArray<FVector2D> Route;
    int32 NextWaypoint=0, RouteRevision=0;
    double PhaseSeconds=0, RetryAt=0;
    double DepartureAt=0;
    bool Outdoor=false;
};

class SEIGE_API FSeigeWorkerSystem
{
public:
    TArray<FSeigeWorker> Bodies;
    TMap<FString,double> DeploymentStock;
    double DeploymentElapsed=0;
    double RecyclingWasteKg=0;
    double DeploymentDescentSeconds() const{return Policy?Number(TEXT("deployment_ground_seconds")):0;}
    double DeploymentHatchSeconds() const{return Policy?Number(TEXT("deployment_hatch_seconds")):0;}
    double BodyRadiusMeters() const{return Policy?Number(TEXT("body_radius_meters")):0;}
    int32 LogisticsJobs(const FSeigeSimulation& Sim) const;
    bool Initialize(const TSharedPtr<FJsonObject>& Rules,FSeigeSimulation& Sim,FString& Error);
    bool SeedInitial(FSeigeSimulation& Sim,int32 Count,FString& Error);
    // Explicit scenario authoring only: completed facilities and their stocks
    // already exist; no runtime hiring, delivery or manufacturing is skipped.
    bool SeedEstablished(FSeigeSimulation& Sim,const TMap<int32,int32>& OperatorsByBuilding,int32 IdleWorkers,FString& Error);
    void Tick(FSeigeSimulation& Sim,double Seconds);
    void RefreshMetrics(FSeigeSimulation& Sim) const;
    void ShiftHome(FVector2D Delta);
    FVector2D HatchPoint(const FSeigeSimulation& Sim) const;
    double HaulUnits(const FSeigeSimulation& Sim,const FString& Resource) const;
    double PickupReserved(const FSeigeSimulation& Sim,int32 Source,const FString& Resource,bool Deployment=false) const;
    bool Dispatch(FSeigeSimulation& Sim,int32 Source,int32 Target,int32 Road,const FString& Resource,double Amount,bool Construction,bool Deployment=false);
    void NewStored(FSeigeSimulation& Sim,int32 BuildingId,int32 Count);
    bool BeginDisassembly(FSeigeSimulation& Sim,int32 BuildingId);
    void FinishDisassembly(FSeigeSimulation& Sim,int32 BuildingId);
    void KillCourier(FSeigeSimulation& Sim,int32 DeliveryId);
    void OnBuildingDestroyed(FSeigeSimulation& Sim,int32 BuildingId);
    void Evacuate();
    // Inventory callers move manifests alongside the numeric cargo quantity.
    bool MoveStored(int32 Source,const FString& ToKind,int32 ToId,int32 Count);
    bool ReceiveStored(const FSeigeSimulation& Sim,const FString& FromKind,int32 FromId,int32 BuildingId,int32 Count);
    bool TransferStoredTo(FSeigeWorkerSystem& Other,const FString& FromKind,int32 FromId,const FString& ToKind,int32 ToId,int32 Count);
    int32 StoredAt(int32 BuildingId) const;
    FSeigeWorker* Find(const FString& Id);
    const FSeigeWorker* Find(const FString& Id) const;
    int32 RoadId(const FSeigeSimulation& Sim,const FSeigeWorker& Body) const;
    bool Save(const TSharedPtr<FJsonObject>& Root) const;
    bool Load(const TSharedPtr<FJsonObject>& Root,FSeigeSimulation& Sim,FString& Error);
    FString Fingerprint;
private:
    TSharedPtr<FJsonObject> Policy;
    FString LogisticsScalingPolicy;
    TSet<FString> LogisticsExcludedRoles;
    FString Origin;
    int32 NextSerial=1;
    double Number(const TCHAR* Key) const;
    FSeigeWorker* IdleWorker(FSeigeSimulation& Sim,bool BorrowOperator,bool IncludeIdle=true);
    void Schedule(FSeigeSimulation& Sim);
    void RouteTo(FSeigeSimulation& Sim,FSeigeWorker& W,FVector2D Destination);
    bool Move(FSeigeSimulation& Sim,FSeigeWorker& W,FVector2D Destination,double Seconds);
    void StepDelivery(FSeigeSimulation& Sim,FSeigeWorker& W,FSeigeCourier& C,double Seconds);
    void Release(FSeigeWorker& W);
    FVector2D WorkPosition(const FSeigeSimulation& Sim,const FSeigeWorker& Body) const;
};
