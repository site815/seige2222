#include "SeigeDependencyGraph.h"
#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeDependencyGraphTest,"Seige.Rules.DependencyGraph",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeDependencyGraphTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    FSeigeDependencyGraph Graph;Graph.Build(S);
    TestTrue(TEXT("Graph has resource and building nodes"),Graph.Nodes.Num()>S.Resources.Num());
    TestFalse(TEXT("Production dependencies are acyclic"),Graph.HasCycle());
    TestTrue(TEXT("Layers cover every node"),Graph.Layers.Num()>=3);
    int32 Counted=0;for(const auto& Column:Graph.Layers)Counted+=Column.Num();
    TestEqual(TEXT("Every node sits in exactly one layer"),Counted,Graph.Nodes.Num());
    // Every manufactured resource a recipe or chassis consumes must be made by some blueprint.
    TArray<FString> Unproduced=Graph.UnproducedResources();
    for(const FString& Id:Unproduced)
    {
        bool Consumed=false;const int32 Node=Graph.Find(TEXT("r:")+Id);
        for(const auto& E:Graph.Edges)if(E.From==Node)Consumed=true;
        if(Consumed)AddError(FString::Printf(TEXT("Resource '%s' is consumed but no blueprint produces it"),*Id));
    }
    const int32 Mine=Graph.Find(TEXT("b:extraction_mine"));
    TestTrue(TEXT("The extraction mine is in the graph"),Mine!=INDEX_NONE);
    if(Mine!=INDEX_NONE)
    {
        const TSet<int32> Reach=Graph.Downstream(Mine);
        for(const auto& N:Graph.Nodes)if(!N.Building&&N.Tier==0)TestTrue(TEXT("Raw resource reachable from the mine: ")+N.Id,Reach.Contains(Graph.Find(TEXT("r:")+N.Id)));
        const int32 Chips=Graph.Find(TEXT("r:ai_chips"));
        if(Chips!=INDEX_NONE)TestTrue(TEXT("AI chips trace back to the mine"),Graph.Upstream(Chips).Contains(Mine));
    }
    return true;
}
#endif
