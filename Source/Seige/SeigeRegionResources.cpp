#include "SeigeGameMode.h"
#include "Simulation/SeigeResourceGeneration.h"

bool ASeigeGameMode::GenerateEmptyRegionResources(const FSeigeSimulation& Center,
    const TArray<FString>& Slots,TArray<FSeigeRegionResources>& Output,FString& Reason)
{
    if(Slots.Num()!=9)
    {Reason=TEXT("Regional resources require exactly nine scenario slots");return false;}
    TArray<FSeigeRegionResources> Generated;
    for(int32 Index=0;Index<Slots.Num();++Index)
    {
        const FString& Type=Slots[Index];
        if(Type!=TEXT("starting")&&Type!=TEXT("developed")&&Type!=(Index==4?TEXT("player"):TEXT("empty")))
        {Reason=TEXT("Regional resources contain an invalid scenario slot");return false;}
        if(Type!=TEXT("empty"))continue;
        FSeigeRegionResources Region;
        Region.Index=Index;Region.Seed=SeigeSectorResourceSeed(Center.GenerationSeed,Index);
        // Only immutable resource data is generated. An unsettled region never
        // initializes a simulation, command core, population, AI or inventory.
        const FVector2D Offset=FVector2D(Index%3-1,Index/3-1)*Center.WorldHalfSize*2;
        if(!Center.GenerateResourceNodesForSeed(Region.Seed,Region.Nodes,Reason,Offset))return false;
        Generated.Add(MoveTemp(Region));
    }
    Output=MoveTemp(Generated);Reason.Empty();return true;
}

const TArray<FSeigeNode>* ASeigeGameMode::RegionNodes(int32 Index) const
{
    if(Index<0||Index>8)return nullptr;
    if(Index==4)return &Sim.Nodes;
    for(const auto& Region:Neighbors)if(Region.Index==Index)return &Region.Sim.Nodes;
    for(const auto& Region:EmptyRegionResources)if(Region.Index==Index)return &Region.Nodes;
    return nullptr;
}

bool ASeigeGameMode::IsRegionResourceVisible(int32 Index,const FSeigeNode& Node) const
{
    if(Index<0||Index>8||Node.Position.ContainsNaN()||
        FMath::Abs(Node.Position.X)>Sim.WorldHalfSize||FMath::Abs(Node.Position.Y)>Sim.WorldHalfSize)return false;
    if(Observer)return true;
    if(Index==4&&Screen==TEXT("landing"))return true;
    const FVector2D Offset=FVector2D(Index%3-1,Index/3-1)*Sim.WorldHalfSize*2;
    // Coverage belongs to the player's colony in world coordinates. Looking at
    // an empty or AI region, or owning no local simulation there, reveals nothing.
    return IsWorldVisible(Node.Position+Offset);
}
