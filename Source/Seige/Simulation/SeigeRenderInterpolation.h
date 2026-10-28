#pragma once
#include "SeigeSimulation.h"

// Presentation-only history. No extra simulation entities, timers or inventory.
struct FSeigeRenderSnapshot
{
    double Time = 0;
    bool Valid = false;
    TMap<int32,FVector2D> Couriers, Enemies, Builders, RoadBuilders, Companions, Vehicles;
    TMap<int32,int32> VehicleSectors;
    TMap<int32,FVector2D> TransportPositions;
    TMap<int32,double> CompanionHeadings,CompanionSpeeds,CompanionTravel;
    TMap<FString,FVector2D> WorkerPositions;
    TMap<int32,double> Construction;
    void Capture(const FSeigeSimulation& Sim)
    {
        Valid=true;Time=Sim.Time;Couriers.Reset();Enemies.Reset();Construction.Reset();Builders.Reset();RoadBuilders.Reset();
        for(const auto& C:Sim.Couriers)Couriers.Add(C.Id,C.Position);
        WorkerPositions.Reset();for(const auto& W:Sim.Workers.Bodies)WorkerPositions.Add(W.Id,W.Position);
        for(const auto& E:Sim.Enemies)Enemies.Add(E.Id,E.Position);
        for(const auto& B:Sim.Buildings){Construction.Add(B.Id,B.ConstructionProgress);Builders.Add(B.Id,B.BuilderPosition);}
        for(const auto& R:Sim.Roads)RoadBuilders.Add(R.Id,R.BuilderPosition);
        Companions.Reset();CompanionHeadings.Reset();CompanionSpeeds.Reset();CompanionTravel.Reset();
        for(const auto& D:Sim.Companions.Dogs){Companions.Add(D.Id,D.Position);CompanionHeadings.Add(D.Id,D.Heading);CompanionSpeeds.Add(D.Id,D.SpeedMetersPerSecond);CompanionTravel.Add(D.Id,D.DistanceWalked);}
        Vehicles.Reset();VehicleSectors.Reset();for(const auto& V:Sim.Combat.Vehicles){Vehicles.Add(V.Id,V.Position);VehicleSectors.Add(V.Id,V.SectorIndex);}
        TransportPositions.Reset();for(const auto& V:Sim.TransportVehicles.Bodies)TransportPositions.Add(V.Id,V.Position);
    }
    double RenderTime(const FSeigeSimulation& Sim,double Alpha) const
    {return Valid&&Time<=Sim.Time?FMath::Lerp(Time,Sim.Time,FMath::Clamp(Alpha,0.,1.)):Sim.Time;}
    FVector2D Courier(const FSeigeCourier& C,double Alpha) const
    {const auto* P=Couriers.Find(C.Id);return Valid&&P?FMath::Lerp(*P,C.Position,FMath::Clamp(Alpha,0.,1.)):C.Position;}
    FVector2D Worker(const FSeigeWorker& W,double Alpha) const
    {const auto* P=WorkerPositions.Find(W.Id);return Valid&&P?FMath::Lerp(*P,W.Position,FMath::Clamp(Alpha,0.,1.)):W.Position;}
    FVector2D CourierAtLoadingPorts(const FSeigeSimulation& Sim,const FSeigeCourier& C,double Alpha,double Clearance) const
    {
        // Routes now physically start/end at edge ports. Presentation must not
        // clamp to a different location or fake travel speed near a building.
        return Courier(C,Alpha);
    }
    FVector2D Builder(const FSeigeBuilding& B,double Alpha) const
    {const auto* P=Builders.Find(B.Id);return Valid&&P?FMath::Lerp(*P,B.BuilderPosition,FMath::Clamp(Alpha,0.,1.)):B.BuilderPosition;}
    FVector2D RoadBuilder(const FSeigeTransportSegment& R,double Alpha) const
    {const auto* P=RoadBuilders.Find(R.Id);return Valid&&P?FMath::Lerp(*P,R.BuilderPosition,FMath::Clamp(Alpha,0.,1.)):R.BuilderPosition;}
    FVector2D Enemy(const FSeigeEnemy& E,double Alpha) const
    {const auto* P=Enemies.Find(E.Id);return Valid&&P?FMath::Lerp(*P,E.Position,FMath::Clamp(Alpha,0.,1.)):E.Position;}
    FVector2D Companion(const FSeigeCompanion& D,double Alpha) const
    {const auto* P=Companions.Find(D.Id);return Valid&&P?FMath::Lerp(*P,D.Position,FMath::Clamp(Alpha,0.,1.)):D.Position;}
    double CompanionHeading(const FSeigeCompanion& D,double Alpha) const
    {const auto* H=CompanionHeadings.Find(D.Id);return Valid&&H?*H+FMath::FindDeltaAngleDegrees(*H,D.Heading)*FMath::Clamp(Alpha,0.,1.):D.Heading;}
    double CompanionSpeed(const FSeigeCompanion& D,double Alpha) const
    {const auto* S=CompanionSpeeds.Find(D.Id);return Valid&&S?FMath::Lerp(*S,D.SpeedMetersPerSecond,FMath::Clamp(Alpha,0.,1.)):D.SpeedMetersPerSecond;}
    double CompanionDistance(const FSeigeCompanion& D,double Alpha) const
    {const auto* T=CompanionTravel.Find(D.Id);return Valid&&T?FMath::Lerp(*T,D.DistanceWalked,FMath::Clamp(Alpha,0.,1.)):D.DistanceWalked;}
    FVector2D Vehicle(const FSeigeVehicle& V,double Alpha) const
    {const auto* P=Vehicles.Find(V.Id);return Valid&&P&&VehicleSectors.FindRef(V.Id)==V.SectorIndex?FMath::Lerp(*P,V.Position,FMath::Clamp(Alpha,0.,1.)):V.Position;}
    FVector2D Transport(const FSeigeTransportVehicle& V,double Alpha) const
    {const auto* P=TransportPositions.Find(V.Id);return Valid&&P?FMath::Lerp(*P,V.Position,FMath::Clamp(Alpha,0.,1.)):V.Position;}
    double Progress(const FSeigeBuilding& B,double Alpha) const
    {const auto* P=Construction.Find(B.Id);return Valid&&P&&B.IsConstructing?FMath::Lerp(*P,B.ConstructionProgress,FMath::Clamp(Alpha,0.,1.)):B.ConstructionProgress;}
};
