#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;
class FJsonObject;

struct FSeigeWallSegment
{
    int32 BuildingId=0;
    FVector2D A=FVector2D::ZeroVector,B=FVector2D::ZeroVector;
    bool InsideLeft=true;
};
struct FSeigeWallPlan
{
    TArray<FSeigeWallSegment> Segments;
    TMap<FString,double> Cost;
    double LengthMeters=0;
};
// Walls reuse real building construction, delivery, upgrades and damage.
// Only the committed geometry is stored here; preview editing is presentation.
class SEIGE_API FSeigeWallSystem
{
public:
    bool Initialize(const FString& Directory,const FSeigeSimulation& Sim,FString& Error);
    bool Plan(const FSeigeSimulation& Sim,const TArray<FVector2D>& Joints,bool InsideLeft,FSeigeWallPlan& Out,FString& Error) const;
    bool Commit(FSeigeSimulation& Sim,const TArray<FVector2D>& Joints,bool InsideLeft,FString& Error);
    const FSeigeWallSegment* Find(int32 BuildingId) const;
    bool AccessPoint(const FSeigeSimulation& Sim,int32 BuildingId,FVector2D& Out) const;
    bool Save(const TSharedPtr<FJsonObject>& Root) const;
    bool Load(const TSharedPtr<FJsonObject>& Root,const FSeigeSimulation& Sim,FString& Error);
    TArray<FSeigeWallSegment> Segments;
    TArray<FString> LevelDefinitions;
    TArray<double> HeightMeters;
    double SegmentLengthMeters=6,MinimumEdgeMeters=1,WidthMeters=1.2,AccessClearanceMeters=1.5,JointPickRadiusMeters=4;
    int32 MaximumJoints=64,MaximumSegments=256;
};
