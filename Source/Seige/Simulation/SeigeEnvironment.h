#pragma once
#include "CoreMinimal.h"

struct FSeigeWaterSample
{
    bool Present=false;
    double Surface=0,Depth=0,Shore=0;
};
struct FSeigeRiverPoint {FVector2D Position=FVector2D::ZeroVector;double Height=0;};
struct FSeigeLake {FVector2D Center=FVector2D::ZeroVector,Radii=FVector2D(1,1);double Height=0,Depth=1;};
struct FSeigeCliff {FVector2D Center=FVector2D::ZeroVector,Radii=FVector2D(1,1);double Height=0,EdgeRatio=.2;};

// Physical geography shared by headless colony routing and rendered terrain.
// All distances/heights are simulation units; no resource deposits are created.
struct FSeigeEnvironment
{
    bool Enabled=false;
    FVector2D WorldOffset=FVector2D::ZeroVector;
    TArray<FSeigeRiverPoint> River;
    TArray<FSeigeLake> Lakes;
    TArray<FSeigeCliff> Cliffs;
    double RiverHalfWidth=180,RiverDepth=40,BankWidth=320,BankHeight=20;
    FString Fingerprint;
    bool Load(const FString& Filename,FString& Error);
    FSeigeWaterSample WaterAt(FVector2D Local) const;
    bool CanStand(FVector2D Local,double Radius=0) const;
    bool SegmentDry(FVector2D A,FVector2D B,double Radius=0) const;
    TArray<FVector2D> RoutingWaypoints(double Radius=0) const;
    double ShapeHeight(FVector2D Local,double Base) const;
};
