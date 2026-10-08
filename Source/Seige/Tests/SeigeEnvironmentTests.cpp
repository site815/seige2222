#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeEnvironmentHydrologyTest,"Seige.Environment.ConnectedWaterAndDryRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeEnvironmentHydrologyTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));
    if(!S.Initialize(Rules,Error,false,false)){AddError(Error);return false;}
    const auto& E=S.Environment;TestTrue(TEXT("Shared physical geography is active"),E.Enabled);
    for(int32 I=1;I<E.River.Num();++I)
    {
        TestTrue(TEXT("Connected reaches never flow uphill"),E.River[I].Height<=E.River[I-1].Height);
        const auto P=FMath::Lerp(E.River[I-1].Position,E.River[I].Position,.5);const auto W=E.WaterAt(P);
        TestTrue(TEXT("A river's actual rendered bed lies below its shared water surface"),W.Present&&E.ShapeHeight(P,400)<W.Surface);
        TestFalse(TEXT("Workers cannot take a straight shortcut through the channel"),E.CanStand(P,1));
        const FVector2D Direction=(E.River[I].Position-E.River[I-1].Position).GetSafeNormal(),Side(-Direction.Y,Direction.X);
        const int32 Samples=FMath::Max(2,FMath::CeilToInt(FVector2D::Distance(E.River[I-1].Position,E.River[I].Position)/100));
        for(int32 J=0;J<=Samples;++J)for(double Across:{-.75,0.,.75})
        {
            const FVector2D Channel=FMath::Lerp(E.River[I-1].Position,E.River[I].Position,double(J)/Samples)+Side*E.RiverHalfWidth*Across;
            const auto Water=E.WaterAt(Channel);
            if(Water.Present&&E.ShapeHeight(Channel,400)>Water.Surface+1.e-6)
            {AddError(FString::Printf(TEXT("A dry bank dams the connected channel at %.2f,%.2f"),Channel.X,Channel.Y));return false;}
        }
    }
    for(int32 Sector=0;Sector<9;++Sector)
    {
        const FVector2D Offset=FVector2D(Sector%3-1,Sector/3-1)*S.WorldHalfSize*2;TArray<FSeigeNode> Nodes;
        if(!S.GenerateResourceNodesForSeed(321+Sector,Nodes,Error,Offset)){AddError(Error);return false;}
        FSeigeEnvironment Regional=E;Regional.WorldOffset=Offset;
        TestEqual(TEXT("Water creates no additional extractable deposits"),Nodes.Num(),5);
        for(const auto& N:Nodes)TestTrue(TEXT("All deposits including groundwater leave dry room for the extraction mine"),Regional.CanStand(N.Position,100));
        const FVector2D Local(850,-1500);TestEqual(TEXT("Neighbor's physical water query uses the same world origin as rendering"),Regional.WaterAt(Local).Present,E.WaterAt(Local+Offset).Present);
    }
    if(!E.Lakes.IsEmpty())
    {
        const auto L=E.Lakes[0];FString Reason;TestFalse(TEXT("Landing inside the visible lake is rejected"),S.CanSetInitialCorePosition(L.Center,Reason));
        // Isolate a lake to verify the actual route graph, not merely a query.
        S.Environment.River.Empty();const FVector2D A=L.Center-FVector2D(L.Radii.X+500,0),B=L.Center+FVector2D(L.Radii.X+500,0);TArray<FVector2D> Route;
        TestFalse(TEXT("Direct lake crossing is blocked"),S.Environment.SegmentDry(A,B,8));
        if(TestTrue(TEXT("Workers find a dry path around a lake"),S.FindRoute(A,B,Route)))
        {FVector2D Previous=A;for(const auto& P:Route){TestTrue(TEXT("Every planned walking segment respects the water body"),S.Environment.SegmentDry(Previous,P,8));Previous=P;}}
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeEnvironmentPersistenceTest,"Seige.Environment.OffsetAndProfilePersistence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeEnvironmentPersistenceTest::RunTest(const FString& Parameters)
{
    FString Error;const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));FSeigeSimulation S,Same,Other;
    const FVector2D Offset(0,-60000);if(!S.Initialize(Rules,Error,false,false,6123,Offset)||!Same.Initialize(Rules,Error,false,false,6123,Offset)||!Other.Initialize(Rules,Error,false,false,6123)){AddError(Error);return false;}
    const FString File=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/environment-offset.json"));
    if(!S.Save(File,Error)){AddError(Error);return false;}
    TestTrue(TEXT("Matching region restores its exact dry deposit layout"),Same.Load(File,Error));
    TestFalse(TEXT("A save cannot relocate physical geography into another region"),Other.Load(File,Error));
    TestEqual(TEXT("Rejected region load preserves live offset"),Other.Environment.WorldOffset,FVector2D::ZeroVector);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeEnvironmentCacheTest,"Seige.Environment.SectorCacheMatchesFullRebuild",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeEnvironmentCacheTest::RunTest(const FString& Parameters)
{
    struct FEnvironmentWorld : FTestWorldWrapper
    {
        ASeigeGameMode* Create(FAutomationTestBase& Test)
        {
            if(!CreateTestWorld(EWorldType::Game)){ForwardErrorMessages(&Test);return nullptr;}
            FURL Url;Url.AddOption(*FString::Printf(TEXT("game=%s"),*ASeigeGameMode::StaticClass()->GetPathName()));
            if(!GetTestWorld()->SetGameMode(Url))return nullptr;return Cast<ASeigeGameMode>(GetTestWorld()->GetAuthGameMode());
        }
    } World;
    auto* G=World.Create(*this);
    if(!G||!G->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),G->Error)){AddError(G?G->Error:TEXT("No test game mode"));return false;}
    G->Ready=true;G->Screen=TEXT("playing");G->RebuildTerrainHeights();
    TArray<FVector2D> Points;for(int32 X=-4;X<=4;++X)for(int32 Y=-4;Y<=4;++Y)Points.Add(FVector2D(X*17513.,Y*17029.));
    const double Half=G->Sim.WorldHalfSize;for(double Y:{-14000.,0.,19000.})for(double Delta:{-.001,0.,.001})Points.Add(FVector2D(Half+Delta,Y));
    for(int32 Sector:{5,4})
    {
        G->FocusSector(Sector);G->RebuildTerrainHeights(true);TArray<double> Retained;
        for(const auto& P:Points)Retained.Add(G->GroundHeight(P));G->RebuildTerrainHeights();
        for(int32 I=0;I<Points.Num();++I)TestTrue(TEXT("Retaining the seven unchanged tiles preserves the exact full-rebuild triangle surface"),FMath::Abs(Retained[I]-G->GroundHeight(Points[I]))<.00001);
    }
    return true;
}
#endif
