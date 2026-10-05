#pragma once
#include "SeigeSimulation.h"

// Presentation-only history. No extra simulation entities, timers or inventory.
struct FSeigeRenderSnapshot
{
    double Time = 0;
    bool Valid = false;
    TMap<int32,FVector2D> Couriers, Enemies;
    TMap<int32,double> Construction;
    void Capture(const FSeigeSimulation& Sim)
    {
        Valid=true;Time=Sim.Time;Couriers.Reset();Enemies.Reset();Construction.Reset();
        for(const auto& C:Sim.Couriers)Couriers.Add(C.Id,C.Position);
        for(const auto& E:Sim.Enemies)Enemies.Add(E.Id,E.Position);
        for(const auto& B:Sim.Buildings)Construction.Add(B.Id,B.ConstructionProgress);
    }
    double RenderTime(const FSeigeSimulation& Sim,double Alpha) const
    {return Valid&&Time<=Sim.Time?FMath::Lerp(Time,Sim.Time,FMath::Clamp(Alpha,0.,1.)):Sim.Time;}
    FVector2D Courier(const FSeigeCourier& C,double Alpha) const
    {const auto* P=Couriers.Find(C.Id);return Valid&&P?FMath::Lerp(*P,C.Position,FMath::Clamp(Alpha,0.,1.)):C.Position;}
    FVector2D CourierAtLoadingPorts(const FSeigeSimulation& Sim,const FSeigeCourier& C,double Alpha,double Clearance) const
    {
        FVector2D P=Courier(C,Alpha);
        const auto* Source=Sim.FindBuilding(C.SourceId);const auto* Target=Sim.FindBuilding(C.TargetId);
        auto Outside=[&](const FSeigeBuilding* Building,FVector2D Direction)
        {
            const auto* Definition=Building?Sim.Definition(*Building):nullptr;
            if(!Definition)return;
            const double Radius=Definition->Footprint+FMath::Max(0.,Clearance);
            const FVector2D Local=P-Building->Position;
            if(FMath::Abs(Local.X)>Radius||FMath::Abs(Local.Y)>Radius)return;
            if(Direction.IsNearlyZero())Direction=Local;
            if(Direction.IsNearlyZero())Direction=FVector2D(1,0);
            // Match the square visible footprint, including diagonal approaches.
            Direction/=FMath::Max(FMath::Abs(Direction.X),FMath::Abs(Direction.Y));
            P=Building->Position+Direction*Radius;
        };
        if(Source&&Target&&Source!=Target)Outside(Source,Target->Position-Source->Position);
        if(Target)Outside(Target,Source&&Source!=Target?Source->Position-Target->Position:P-Target->Position);
        // This changes only display position. Center-to-center travel, inventory
        // removal/arrival and full-destination waiting remain simulation-owned.
        return P;
    }
    FVector2D Enemy(const FSeigeEnemy& E,double Alpha) const
    {const auto* P=Enemies.Find(E.Id);return Valid&&P?FMath::Lerp(*P,E.Position,FMath::Clamp(Alpha,0.,1.)):E.Position;}
    double Progress(const FSeigeBuilding& B,double Alpha) const
    {const auto* P=Construction.Find(B.Id);return Valid&&P&&B.IsConstructing?FMath::Lerp(*P,B.ConstructionProgress,FMath::Clamp(Alpha,0.,1.)):B.ConstructionProgress;}
};
