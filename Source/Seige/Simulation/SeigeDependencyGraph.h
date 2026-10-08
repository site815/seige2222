#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;

// The building <-> resource dependency graph derived from the loaded rules:
// extraction (mine -> raw resource), production (recipe inputs -> building ->
// outputs, worker output), ammunition (resource -> tower) and chassis materials
// (resource -> vehicle factory). Layered left to right by longest path so the
// HUD can draw it as a tree and tests can check that every manufactured
// resource has a producer and that nothing depends on itself.
struct FSeigeDependencyNode
{
    FString Id,Name,Role;      // Role: resource class/tier text for resources, building role for buildings
    bool Building=false;
    int32 Tier=0,Layer=0,Order=0;
};
struct FSeigeDependencyEdge
{
    enum class EKind : uint8 {Extraction,Production,Workers,Ammunition,Chassis};
    int32 From=0,To=0;
    EKind Kind=EKind::Production;
    double Amount=0;           // units per batch where the rules give one
};
struct FSeigeDependencyGraph
{
    TArray<FSeigeDependencyNode> Nodes;
    TArray<FSeigeDependencyEdge> Edges;
    TArray<TArray<int32>> Layers;          // node indices per layer, in drawing order
    TArray<FString> OutsideChain;          // buildings with no production role (power, storage, sensing, trade)
    void Build(const FSeigeSimulation& Sim);
    int32 Find(const FString& Key) const;  // "r:<resource>" or "b:<building>"
    TSet<int32> Upstream(int32 Node) const;
    TSet<int32> Downstream(int32 Node) const;
    TArray<FString> UnproducedResources() const;   // manufactured resources no building makes
    bool HasCycle() const;
private:
    TMap<FString,int32> Index;
    TArray<TArray<int32>> Out,In;
    int32 Node(const FString& Key,const FString& Name,bool Building,int32 Tier,const FString& Role);
    void Link(int32 From,int32 To,FSeigeDependencyEdge::EKind Kind,double Amount);
    void Layer();
};
