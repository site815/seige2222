#include "SeigeGameMode.h"
#include "Simulation/SeigeResourceGeneration.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags RegionFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
TArray<FString> EmptyNeighborhood()
{
    TArray<FString> Slots;Slots.Init(TEXT("empty"),9);Slots[4]=TEXT("player");return Slots;
}
bool SameNodes(const TArray<FSeigeNode>& A,const TArray<FSeigeNode>& B)
{
    if(A.Num()!=B.Num())return false;
    for(int32 I=0;I<A.Num();++I)if(A[I].Id!=B[I].Id||A[I].Resource!=B[I].Resource||A[I].Position!=B[I].Position)return false;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeEmptyRegionResourcesTest,"Seige.Regions.EmptyResourceCatalog",RegionFlags)
bool FSeigeEmptyRegionResourcesTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation Center;FString Error;
    if(!Center.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false))
    {AddError(Error);return false;}
    const int32 Buildings=Center.Buildings.Num(),Population=Center.Population,Couriers=Center.Couriers.Num();
    const auto CenterNodes=Center.Nodes;
    const auto Slots=EmptyNeighborhood();TArray<FSeigeRegionResources> First,Again;
    if(!TestTrue(TEXT("Empty regions generate without a colony"),ASeigeGameMode::GenerateEmptyRegionResources(Center,Slots,First,Error)))
    {AddError(Error);return false;}
    TestEqual(TEXT("Eight unsettled neighbors have separate catalogs"),First.Num(),8);
    TestTrue(TEXT("Repeated generation succeeds"),ASeigeGameMode::GenerateEmptyRegionResources(Center,Slots,Again,Error));
    for(int32 I=0;I<First.Num();++I)
    {
        const auto& Region=First[I];int32 Standard=0,Rare=0;TSet<FString> Types;
        TestEqual(TEXT("Every empty region has five deposits"),Region.Nodes.Num(),5);
        TestEqual(TEXT("Each catalog records its stable sector salt"),Region.Seed,SeigeSectorResourceSeed(Center.GenerationSeed,Region.Index));
        for(const auto& Node:Region.Nodes)
        {
            const auto* Definition=Center.Resources.Find(Node.Resource);
            if(!TestNotNull(TEXT("Generated deposit refers to an existing resource"),Definition))continue;
            Standard+=Definition->Class==TEXT("standard");Rare+=Definition->Class==TEXT("rare");Types.Add(Node.Resource);
            TestTrue(TEXT("Resource remains inside inner 75 percent of sector area"),FMath::Abs(Node.Position.X)<=Center.WorldHalfSize*FMath::Sqrt(.75)&&FMath::Abs(Node.Position.Y)<=Center.WorldHalfSize*FMath::Sqrt(.75));
        }
        TestEqual(TEXT("Three standard deposits"),Standard,3);TestEqual(TEXT("Two rare deposits"),Rare,2);
        TestEqual(TEXT("Five distinct resource types"),Types.Num(),5);
        if(Again.IsValidIndex(I))TestTrue(TEXT("Same seed and rules regenerate exactly"),SameNodes(Region.Nodes,Again[I].Nodes));
        TestFalse(TEXT("Sector salt avoids copied home positions"),SameNodes(Region.Nodes,CenterNodes));
    }
    TestEqual(TEXT("Resource generation creates no buildings"),Center.Buildings.Num(),Buildings);
    TestEqual(TEXT("Resource generation creates no workforce"),Center.Population,Population);
    TestEqual(TEXT("Resource generation creates no couriers"),Center.Couriers.Num(),Couriers);
    TestTrue(TEXT("Home deposits remain unchanged"),SameNodes(Center.Nodes,CenterNodes));
    auto Mixed=Slots;Mixed[0]=TEXT("starting");Mixed[8]=TEXT("developed");
    TestTrue(TEXT("Occupied neighbors use their own colony catalogs"),ASeigeGameMode::GenerateEmptyRegionResources(Center,Mixed,Again,Error));
    TestEqual(TEXT("Occupied neighbors are excluded from empty data"),Again.Num(),6);
    TestFalse(TEXT("No empty catalog duplicates an occupied neighbor"),Again.ContainsByPredicate([](const FSeigeRegionResources& R){return R.Index==0||R.Index==8;}));
    Mixed[4]=TEXT("empty");const int32 Previous=Again.Num();
    TestFalse(TEXT("Invalid center fails transactionally"),ASeigeGameMode::GenerateEmptyRegionResources(Center,Mixed,Again,Error));
    TestEqual(TEXT("Failed generation preserves previous catalog"),Again.Num(),Previous);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRegionResourceVisibilityTest,"Seige.Regions.DepositVisibility",RegionFlags)
bool FSeigeRegionResourceVisibilityTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper World;if(!World.CreateTestWorld(EWorldType::Game)){World.ForwardErrorMessages(this);return false;}
    auto* Game=World.GetTestWorld()->SpawnActor<ASeigeGameMode>();
    if(!TestNotNull(TEXT("Region test game mode"),Game))return false;
    FString Error;if(!Game->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    Game->ScenarioSlots=EmptyNeighborhood();Game->Screen=TEXT("playing");
    if(!Game->GenerateEmptyRegionResources(Game->Sim,Game->ScenarioSlots,Game->EmptyRegionResources,Error)){AddError(Error);return false;}
    const auto* Nodes=Game->RegionNodes(0);
    if(!TestNotNull(TEXT("An unsettled sector exposes data without ViewedSimulation"),Nodes)||Nodes->IsEmpty())return false;
    Game->CameraCenter=FVector(-Game->Sim.WorldHalfSize*2,-Game->Sim.WorldHalfSize*2,0);
    TestNull(TEXT("Empty region still has no colony simulation"),Game->ViewedSimulation());
    TestFalse(TEXT("Panning to an empty sector does not reveal its deposits"),Game->IsRegionResourceVisible(0,(*Nodes)[0]));
    Game->Screen=TEXT("landing");
    TestFalse(TEXT("Home landing survey does not expose neighboring resources"),Game->IsRegionResourceVisible(0,(*Nodes)[0]));
    TestTrue(TEXT("Landing survey reveals the home resource layout"),Game->IsRegionResourceVisible(4,Game->Sim.Nodes[0]));
    Game->Screen=TEXT("playing");Game->Observer=true;
    TestTrue(TEXT("Observer can inspect empty-region deposits"),Game->IsRegionResourceVisible(0,(*Nodes)[0]));
    Game->Observer=false;
    auto* Core=Game->Sim.Buildings.FindByPredicate([&](const FSeigeBuilding& B){const auto* D=Game->Sim.Definition(B);return D&&D->Role==TEXT("core");});
    if(!TestNotNull(TEXT("Home core exists only for visibility fixture"),Core))return false;
    // Put the player's live sensor directly at the neighbor node in this focused
    // fixture; visibility must use world coordinates rather than neighbor-local.
    Core->Position=(*Nodes)[0].Position+FVector2D(-Game->Sim.WorldHalfSize*2,-Game->Sim.WorldHalfSize*2);
    TestTrue(TEXT("Player sensor coverage reveals a neighboring deposit"),Game->IsRegionResourceVisible(0,(*Nodes)[0]));
    Core->Enabled=false;
    TestFalse(TEXT("Disabling the player's sensor hides that deposit again"),Game->IsRegionResourceVisible(0,(*Nodes)[0]));
    TestNull(TEXT("Invalid sector index has no catalog"),Game->RegionNodes(9));
    return true;
}
#endif
