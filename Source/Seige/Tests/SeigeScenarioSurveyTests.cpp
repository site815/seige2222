#include "SeigeGameMode.h"
#include "Simulation/SeigeResourceGeneration.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags SurveyFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
bool StartSurveyScenario(ASeigeGameMode& Game,FAutomationTestBase& Test)
{
    Game.Ready=false;Game.StartScenario();
    if(!Game.Ready){Test.AddError(Game.Error);return false;}
    return true;
}
struct FSurveyWorld : FTestWorldWrapper
{
    ASeigeGameMode* Game=nullptr;
    ASeigeController* Controller=nullptr;
    bool Prepare(FAutomationTestBase& Test)
    {
        if(!CreateTestWorld(EWorldType::Game)){ForwardErrorMessages(&Test);return false;}
        FURL Url;Url.AddOption(*FString::Printf(TEXT("game=%s"),*ASeigeGameMode::StaticClass()->GetPathName()));
        if(!GetTestWorld()->SetGameMode(Url)){Test.AddError(TEXT("Could not create scenario survey game mode"));return false;}
        Game=Cast<ASeigeGameMode>(GetTestWorld()->GetAuthGameMode());Controller=GetTestWorld()->SpawnActor<ASeigeController>();
        if(!Game||!Controller){Test.AddError(TEXT("Could not create scenario survey actors"));return false;}
        Game->ScenarioSlots.Init(TEXT("empty"),9);Game->ScenarioSlots[4]=TEXT("player");Game->ScenarioSeedOverride=815;
        Game->ScenarioBackgroundBugs=Game->ScenarioPeriodicAttacks=false;
        if(!StartSurveyScenario(*Game,Test))return false;
        Game->Ready=true;Game->Screen=TEXT("playing");return true;
    }
};
bool SameLayout(const TArray<FSeigeNode>& A,const TArray<FSeigeNode>& B)
{
    if(A.Num()!=B.Num())return false;
    for(int32 I=0;I<A.Num();++I)if(A[I].Resource!=B[I].Resource||A[I].Position!=B[I].Position)return false;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeBuildingClickAfterSurveyTest,"Seige.Interaction.BuildingClickAfterRegionSurvey",SurveyFlags)
bool FSeigeBuildingClickAfterSurveyTest::RunTest(const FString& Parameters)
{
    FSurveyWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    G.Zoom=G.MaximumZoom;G.RenderedZoom=-1;G.FocusSector(4,true);
    if(!TestEqual(TEXT("Focusing a sector saves the region-map return view"),G.RegionViewHistory.Num(),1))return false;
    const float DetailedZoom=G.Zoom;
    const int32 CoreId=G.Sim.Buildings[0].Id;
    TestTrue(TEXT("Fixture uses a construction site without a rendered query body"),G.Sim.Buildings[0].IsConstructing);
    G.CursorOnWorld=true;G.CursorWorld=G.Sim.Buildings[0].Position;
    W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("First construction-site click selects the building"),G.SelectedId,CoreId);
    TestEqual(TEXT("Building selection does not zoom out"),G.Zoom,DetailedZoom);
    TestEqual(TEXT("Building selection retains the explicit map return bookmark"),G.RegionViewHistory.Num(),1);
    G.SelectedId=0;G.Sim.Buildings[0].IsConstructing=false;G.Sim.Buildings[0].ConstructionProgress=1;
    W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("A completed footprint also selects without a query mesh"),G.SelectedId,CoreId);
    G.CursorWorld=FVector2D(G.Sim.WorldHalfSize*.9,G.Sim.WorldHalfSize*.9);
    W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("An empty-ground click clears building selection"),G.SelectedId,0);
    W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("Repeated empty-ground clicks do not navigate to regions"),G.Zoom,DetailedZoom);
    TestTrue(TEXT("Explicit back navigation still works"),G.RestoreRegionView());
    TestEqual(TEXT("Explicit back restores the regional view"),G.Zoom,G.MaximumZoom);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeHomeSurveyPersistsTest,"Seige.Regions.HomeSurveyPersistsAfterLanding",SurveyFlags)
bool FSeigeHomeSurveyPersistsTest::RunTest(const FString& Parameters)
{
    FSurveyWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    TestEqual(TEXT("Peaceful player scenario has five home deposits"),G.Sim.Nodes.Num(),5);
    G.Screen=TEXT("landing");const auto Before=G.Sim.Nodes;
    for(const auto& Node:Before)TestTrue(TEXT("Landing charts each home deposit"),G.IsRegionResourceVisible(4,Node));
    G.ConfirmLanding(FVector2D(700,0));
    if(!TestEqual(TEXT("Player lands successfully"),G.Screen,FString(TEXT("playing"))))return false;
    int32 OutsideLiveCoverage=0;
    for(const auto& Node:G.Sim.Nodes)
    {
        if(!G.IsWorldVisible(Node.Position))++OutsideLiveCoverage;
        TestTrue(TEXT("Charted deposit remains visible during deployment"),G.IsRegionResourceVisible(4,Node));
    }
    TestTrue(TEXT("Regression covers deposits outside live sensor coverage"),OutsideLiveCoverage>0);
    G.Sim.Buildings[0].Enabled=false;
    for(const auto& Node:G.Sim.Nodes)TestTrue(TEXT("Sensor power loss does not erase surveyed geology"),G.IsRegionResourceVisible(4,Node));
    const auto* Neighbor=G.RegionNodes(0);
    if(!TestNotNull(TEXT("Empty neighbor still has resource data"),Neighbor))return false;
    for(const auto& Node:*Neighbor)TestFalse(TEXT("Survey does not reveal remote deposits"),G.IsRegionResourceVisible(0,Node));
    TestTrue(TEXT("Landing changes no deposit positions or resource choices"),SameLayout(Before,G.Sim.Nodes));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeScenarioResourceSeedTest,"Seige.Regions.ScenarioResourceSeedsAndThreatIndependence",SurveyFlags)
bool FSeigeScenarioResourceSeedTest::RunTest(const FString& Parameters)
{
    FSurveyWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    TArray<TArray<FSeigeNode>> Baseline;
    for(int32 Index=0;Index<9;++Index)Baseline.Add(*G.RegionNodes(Index));
    for(int32 Mask=0;Mask<4;++Mask)
    {
        G.ScenarioBackgroundBugs=(Mask&1)!=0;G.ScenarioPeriodicAttacks=(Mask&2)!=0;
        if(!StartSurveyScenario(G,*this))return false;
        for(int32 Index=0;Index<9;++Index)
        {
            const auto* Nodes=G.RegionNodes(Index);if(!TestNotNull(TEXT("Every scenario sector owns a deposit catalog"),Nodes))continue;
            TestTrue(TEXT("Threat toggles do not alter geological layouts"),SameLayout(Baseline[Index],*Nodes));
            int32 Standard=0,Rare=0;TSet<FString> Distinct;
            for(const auto& Node:*Nodes)
            {
                Distinct.Add(Node.Resource);const auto& Def=G.Sim.Resources[Node.Resource];Standard+=Def.Class==TEXT("standard");Rare+=Def.Class==TEXT("rare");
                const double Limit=G.Sim.WorldHalfSize*FMath::Sqrt(.75);
                TestTrue(TEXT("Deposits stay inside the inner 75 percent of regional area"),FMath::Abs(Node.Position.X)<=Limit&&FMath::Abs(Node.Position.Y)<=Limit);
            }
            TestEqual(TEXT("Three standard deposits remain available"),Standard,3);TestEqual(TEXT("Two rare deposits remain available"),Rare,2);
            TestEqual(TEXT("No duplicate resource type within a region"),Distinct.Num(),5);
        }
    }
    G.ScenarioBackgroundBugs=G.ScenarioPeriodicAttacks=false;G.ScenarioSeedOverride=816;
    if(!StartSurveyScenario(G,*this))return false;
    TestFalse(TEXT("A different scenario seed changes the home layout"),SameLayout(Baseline[4],G.Sim.Nodes));
    const auto SavedNodes=G.Sim.Nodes;const int32 SavedSeed=G.Sim.GenerationSeed;
    const FString Save=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/ScenarioSurvey/seed-roundtrip.json"));FString Error;
    if(!G.Sim.Save(Save,Error)){AddError(Error);return false;}
    FSeigeSimulation Loaded;
    if(!Loaded.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error)||!Loaded.Load(Save,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Save/load preserves the exact new-game seed"),Loaded.GenerationSeed,SavedSeed);
    TestTrue(TEXT("Save/load does not reroll deposits"),SameLayout(SavedNodes,Loaded.Nodes));
    G.ScenarioSeedOverride=INDEX_NONE;
    if(!StartSurveyScenario(G,*this))return false;
    const int32 FirstRandomSeed=G.Sim.GenerationSeed;const auto FirstRandomNodes=G.Sim.Nodes;
    if(!StartSurveyScenario(G,*this))return false;
    int32 ReplaySeed=INDEX_NONE;
    if(FParse::Value(FCommandLine::Get(),TEXT("ScenarioSeed="),ReplaySeed)&&ReplaySeed!=INDEX_NONE)
    {
        TestEqual(TEXT("Explicit command-line replay seed overrides the fresh-game draw"),G.Sim.GenerationSeed,ReplaySeed);
        TestTrue(TEXT("Explicit replay preserves layout between new games"),SameLayout(FirstRandomNodes,G.Sim.Nodes));
    }
    else
    {
        TestTrue(TEXT("Ordinary new games receive independent seeds"),G.Sim.GenerationSeed!=FirstRandomSeed);
        TestFalse(TEXT("Ordinary new games do not reuse the same deposit layout"),SameLayout(FirstRandomNodes,G.Sim.Nodes));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePreparedScenarioSeedTest,"Seige.Regions.PreparedScenarioCapturesSeed",SurveyFlags)
bool FSeigePreparedScenarioSeedTest::RunTest(const FString& Parameters)
{
    FSurveyWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    const auto Expected=G.Sim.Nodes;G.Screen=TEXT("scenario");G.BeginScenarioPreparation();
    if(!TestTrue(TEXT("Incremental scenario preparation begins"),G.IsPreparingScenario()))return false;
    G.ScenarioSeedOverride=816;G.ScenarioBackgroundBugs=G.ScenarioPeriodicAttacks=true;
    G.RegionViewHistory.Add({FVector::ZeroVector,G.MaximumZoom,135,52,0,0,0});
    for(int32 I=0;I<100&&G.IsPreparingScenario();++I)G.TickScenarioPreparation(50);
    if(!TestTrue(TEXT("Incremental preparation finishes"),!G.IsPreparingScenario()&&G.Screen==TEXT("landing"))){AddError(G.Error);return false;}
    TestEqual(TEXT("Preparation retains its original seed"),G.Sim.GenerationSeed,815);
    TestTrue(TEXT("Preparation retains its original peaceful threat choices"),!G.Sim.BackgroundBugsEnabled&&!G.Sim.PeriodicAttacksEnabled);
    TestTrue(TEXT("Incremental and immediate preparation generate the same seeded layout"),SameLayout(Expected,G.Sim.Nodes));
    TestTrue(TEXT("New scenario clears prior game map bookmarks"),G.RegionViewHistory.IsEmpty());
    return true;
}
#endif

