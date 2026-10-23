#pragma once
#include "CoreMinimal.h"

struct FSeigeWaterSample
{
    bool Present=false;
    double Surface=0,Depth=0,Shore=0;
};
struct FSeigeRiverPoint {FVector2D Position=FVector2D::ZeroVector;double Height=0;bool RoutingAnchor=true;};
struct FSeigeRiver {TArray<FSeigeRiverPoint> Points;};
struct FSeigeFord {FVector2D Center=FVector2D::ZeroVector;double Radius=900,Blend=450,Depth=2;};
struct FSeigeLake
{
    FVector2D Center=FVector2D::ZeroVector,Radii=FVector2D(1,1);
    double Height=0,Depth=1,ShoreVariation=0,ShorePhase=0;
    // Exact polygon shared by the rendered shoreline and physical queries.
    TArray<FVector2D> Shoreline;
    double RadialScale(FVector2D UnitDirection) const;
    double RadiusAt(FVector2D World) const;
    void BuildShoreline();
};
struct FSeigeCliff
{
    FVector2D Center=FVector2D::ZeroVector,Radii=FVector2D(1,1);
    double Height=0,EdgeRatio=.2,AngleDegrees=0,Irregularity=.12;
    double Elevation(FVector2D World) const;
};

// Physical geography shared by headless colony routing and rendered terrain.
// All distances/heights are simulation units; no resource deposits are created.
struct FSeigeEnvironment
{
    bool Enabled=false;
    FVector2D WorldOffset=FVector2D::ZeroVector;
    TArray<FSeigeRiverPoint> River;
    TArray<FSeigeRiver> AdditionalRivers;
    TArray<FSeigeFord> Fords;
    TArray<FSeigeLake> Lakes;
    TArray<FSeigeCliff> Cliffs;
    double RiverHalfWidth=180,RiverDepth=40,BankWidth=320,BankHeight=20;
    double ValleyWidth=8000,BaseElevation=-350,NorthSlope=.004,MacroAmplitude=500;
    FString Fingerprint;
    bool Load(const FString& Filename,FString& Error);
    FSeigeWaterSample WaterAt(FVector2D Local) const;
    bool CanStand(FVector2D Local,double Radius=0) const;
    bool SegmentDry(FVector2D A,FVector2D B,double Radius=0) const;
    bool CanTraverse(FVector2D Local,double Radius=0) const;
    bool SegmentTraversable(FVector2D A,FVector2D B,double Radius=0) const;
    double RiverDepthAt(FVector2D World) const;
    bool RiverProfileAt(FVector2D World,double MaximumDistance,double& Distance,double& Surface) const;
    TArray<FVector2D> RoutingWaypoints(double Radius=0) const;
    double ShapeHeight(FVector2D Local,double Base) const;
    template<typename Visitor> void ForEachRiver(Visitor&& Visit) const
    {Visit(River);for(const auto& R:AdditionalRivers)Visit(R.Points);}
};
