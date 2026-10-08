#include "SeigeDependencyGraph.h"
#include "SeigeSimulation.h"

int32 FSeigeDependencyGraph::Node(const FString& Key,const FString& Name,bool Building,int32 Tier,const FString& Role)
{
    if(const int32* Existing=Index.Find(Key))return *Existing;
    FSeigeDependencyNode N;N.Id=Key.RightChop(2);N.Name=Name;N.Building=Building;N.Tier=Tier;N.Role=Role;
    const int32 Added=Nodes.Add(N);Index.Add(Key,Added);Out.AddDefaulted();In.AddDefaulted();return Added;
}
void FSeigeDependencyGraph::Link(int32 From,int32 To,FSeigeDependencyEdge::EKind Kind,double Amount)
{
    if(From<0||To<0||From==To)return;
    for(const auto& E:Edges)if(E.From==From&&E.To==To){return;}
    FSeigeDependencyEdge E;E.From=From;E.To=To;E.Kind=Kind;E.Amount=Amount;Edges.Add(E);Out[From].Add(To);In[To].Add(From);
}
int32 FSeigeDependencyGraph::Find(const FString& Key) const{const int32* Found=Index.Find(Key);return Found?*Found:INDEX_NONE;}

void FSeigeDependencyGraph::Build(const FSeigeSimulation& Sim)
{
    Nodes.Reset();Edges.Reset();Layers.Reset();OutsideChain.Reset();Index.Reset();Out.Reset();In.Reset();
    TArray<FString> ResourceIds;Sim.Resources.GetKeys(ResourceIds);ResourceIds.Sort();
    for(const FString& Id:ResourceIds){const auto& R=Sim.Resources[Id];Node(TEXT("r:")+Id,R.Name,false,R.Tier,R.Class);}
    auto Resource=[&](const FString& Id){return Find(TEXT("r:")+Id);};
    TArray<FString> Blueprints=Sim.BuildMenu;Blueprints.Sort();
    if(!Sim.CoreDefinition.IsEmpty())Blueprints.Insert(Sim.CoreDefinition,0);
    for(const FString& Id:Blueprints)
    {
        const auto* D=Sim.BuildingDefs.Find(Id);if(!D||D->Role==TEXT("wall"))continue;
        bool InChain=false;
        TArray<FString> RecipeIds;if(!D->Recipe.IsEmpty())RecipeIds.Add(D->Recipe);for(const FString& R:D->AllowedRecipes)RecipeIds.AddUnique(R);
        if(D->Role==TEXT("core"))RecipeIds.Reset();   // the universal replicator would connect everything; shown as a note instead
        const int32 B=Node(TEXT("b:")+Id,D->Name,true,D->Level,D->Role);
        for(const auto& Pair:D->ExtractionRates){Link(B,Resource(Pair.Key),FSeigeDependencyEdge::EKind::Extraction,Pair.Value);InChain=true;}
        for(const FString& RecipeId:RecipeIds)
        {
            const auto* R=Sim.Recipes.Find(RecipeId);if(!R)continue;
            for(const auto& Pair:R->Inputs){Link(Resource(Pair.Key),B,FSeigeDependencyEdge::EKind::Production,Pair.Value);InChain=true;}
            for(const auto& Pair:R->Outputs){Link(B,Resource(Pair.Key),FSeigeDependencyEdge::EKind::Production,Pair.Value);InChain=true;}
            if(R->WorkerOutput>0&&Resource(TEXT("stored_workers"))>=0){Link(B,Resource(TEXT("stored_workers")),FSeigeDependencyEdge::EKind::Workers,R->WorkerOutput);InChain=true;}
        }
        if(const auto* Platform=Sim.Combat.BuildingPlatforms.Find(Id))
            for(const FString& WeaponId:Platform->Weapons)
                if(const auto* Weapon=Sim.Combat.Weapons.Find(WeaponId))if(!Weapon->Ammo.IsEmpty()&&Resource(Weapon->Ammo)>=0){Link(Resource(Weapon->Ammo),B,FSeigeDependencyEdge::EKind::Ammunition,Weapon->AmmoPerShot);InChain=true;}
        if(const auto* Factory=Sim.Combat.Factories.Find(Id))
        {
            TMap<FString,double> Materials;
            for(const auto& Pair:Sim.Combat.Chassis)
                if((Factory->Family==TEXT("all")||Pair.Value.Family==Factory->Family)&&Pair.Value.Tier<=Factory->MaximumTier)
                    for(const auto& Cost:Pair.Value.Cost)Materials.FindOrAdd(Cost.Key)=FMath::Max(Materials.FindOrAdd(Cost.Key),Cost.Value);
            if(D->Role!=TEXT("core"))for(const auto& Pair:Materials){Link(Resource(Pair.Key),B,FSeigeDependencyEdge::EKind::Chassis,Pair.Value);InChain=true;}
        }
        if(!InChain&&D->Role!=TEXT("core")&&D->Role!=TEXT("defense")){OutsideChain.Add(D->Name);Nodes.RemoveAt(B);Index.Remove(TEXT("b:")+Id);Out.RemoveAt(B);In.RemoveAt(B);}
    }
    Layer();
}

void FSeigeDependencyGraph::Layer()
{
    // Longest path from the sources over a copy of the edge list with any back
    // edges dropped (rules are validated acyclic elsewhere; this stays robust).
    const int32 N=Nodes.Num();TArray<int32> Depth;Depth.Init(0,N);TArray<int32> Remaining;Remaining.Init(0,N);
    for(const auto& E:Edges)++Remaining[E.To];
    TArray<int32> Queue;for(int32 I=0;I<N;++I)if(Remaining[I]==0)Queue.Add(I);
    TArray<bool> Done;Done.Init(false,N);
    while(Queue.Num())
    {
        const int32 Current=Queue.Pop(EAllowShrinking::No);Done[Current]=true;
        for(int32 Next:Out[Current]){Depth[Next]=FMath::Max(Depth[Next],Depth[Current]+1);if(--Remaining[Next]==0)Queue.Add(Next);}
    }
    for(int32 I=0;I<N;++I)if(!Done[I])Depth[I]=0;   // part of a cycle: left at the front
    int32 MaxDepth=0;for(int32 I=0;I<N;++I)MaxDepth=FMath::Max(MaxDepth,Depth[I]);
    for(int32 I=0;I<N;++I)
    {
        // Pure consumers with no inputs (energy towers) and resources nobody makes
        // sit in the last column rather than among the sources.
        if(Nodes[I].Building&&Nodes[I].Role==TEXT("core"))Depth[I]=0;   // the landing kit is where every chain starts
        else if(In[I].Num()==0&&Out[I].Num()==0)Depth[I]=Nodes[I].Building?MaxDepth:FMath::Max(0,MaxDepth-1);
        else if(Nodes[I].Building&&In[I].Num()==0&&Nodes[I].Role==TEXT("defense"))Depth[I]=MaxDepth;
        Nodes[I].Layer=FMath::Clamp(Depth[I],0,FMath::Max(MaxDepth,0));
    }
    Layers.SetNum(MaxDepth+1);
    for(int32 I=0;I<N;++I)Layers[Nodes[I].Layer].Add(I);
    // Initial order: resources by tier then name, buildings by name.
    for(auto& Column:Layers)Column.Sort([&](int32 A,int32 B){const auto& X=Nodes[A];const auto& Y=Nodes[B];if(X.Building!=Y.Building)return !X.Building;if(X.Tier!=Y.Tier)return X.Tier<Y.Tier;return X.Name<Y.Name;});
    // Two barycenter sweeps reduce crossings.
    TArray<double> Position;Position.Init(0,N);
    auto Assign=[&](){for(const auto& Column:Layers)for(int32 K=0;K<Column.Num();++K)Position[Column[K]]=K;};
    for(int32 Sweep=0;Sweep<2;++Sweep)
    {
        Assign();
        for(int32 L=1;L<Layers.Num();++L)
        {
            TMap<int32,double> Key;
            for(int32 Idx:Layers[L]){double Sum=0;int32 Count=0;for(int32 P:In[Idx])if(Nodes[P].Layer<L){Sum+=Position[P];++Count;}Key.Add(Idx,Count?Sum/Count:Position[Idx]);}
            Layers[L].StableSort([&](int32 A,int32 B){return Key[A]<Key[B];});
        }
        Assign();
        for(int32 L=Layers.Num()-2;L>=0;--L)
        {
            TMap<int32,double> Key;
            for(int32 Idx:Layers[L]){double Sum=0;int32 Count=0;for(int32 S:Out[Idx])if(Nodes[S].Layer>L){Sum+=Position[S];++Count;}Key.Add(Idx,Count?Sum/Count:Position[Idx]);}
            Layers[L].StableSort([&](int32 A,int32 B){return Key[A]<Key[B];});
        }
    }
    for(const auto& Column:Layers)for(int32 K=0;K<Column.Num();++K)Nodes[Column[K]].Order=K;
}

TSet<int32> FSeigeDependencyGraph::Upstream(int32 Start) const
{
    TSet<int32> Seen;TArray<int32> Stack;if(Start<0||Start>=Nodes.Num())return Seen;Stack.Add(Start);
    while(Stack.Num()){const int32 C=Stack.Pop(EAllowShrinking::No);if(Seen.Contains(C))continue;Seen.Add(C);for(int32 P:In[C])Stack.Add(P);}
    Seen.Remove(Start);return Seen;
}
TSet<int32> FSeigeDependencyGraph::Downstream(int32 Start) const
{
    TSet<int32> Seen;TArray<int32> Stack;if(Start<0||Start>=Nodes.Num())return Seen;Stack.Add(Start);
    while(Stack.Num()){const int32 C=Stack.Pop(EAllowShrinking::No);if(Seen.Contains(C))continue;Seen.Add(C);for(int32 S:Out[C])Stack.Add(S);}
    Seen.Remove(Start);return Seen;
}
TArray<FString> FSeigeDependencyGraph::UnproducedResources() const
{
    TArray<FString> Missing;
    for(int32 I=0;I<Nodes.Num();++I)if(!Nodes[I].Building&&Nodes[I].Tier>0&&In[I].Num()==0)Missing.Add(Nodes[I].Id);
    return Missing;
}
bool FSeigeDependencyGraph::HasCycle() const
{
    const int32 N=Nodes.Num();TArray<int32> Remaining;Remaining.Init(0,N);for(const auto& E:Edges)++Remaining[E.To];
    TArray<int32> Queue;for(int32 I=0;I<N;++I)if(Remaining[I]==0)Queue.Add(I);int32 Visited=0;
    while(Queue.Num()){const int32 C=Queue.Pop(EAllowShrinking::No);++Visited;for(int32 S:Out[C])if(--Remaining[S]==0)Queue.Add(S);}
    return Visited<N;
}
