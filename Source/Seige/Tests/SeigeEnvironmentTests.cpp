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
        S.Environment.River.Empty();S.Environment.AdditionalRivers.Empty();const FVector2D A=L.Center-FVector2D(L.Radii.X+500,0),B=L.Center+FVector2D(L.Radii.X+500,0);TArray<FVector2D> Route;
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRegionalDrainageTest,"Seige.Environment.RegionalValleysAndEscarpments",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeRegionalDrainageTest::RunTest(const FString& Parameters)
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
    if(!G||!G->Sim.Initialize(FPaths::ProjectDir()/TEXT("Rules"),G->Error,false,false)||!G->LoadGraphicsSettings())
    {AddError(G?G->Error:TEXT("No regional drainage fixture"));return false;}
    const auto& E=G->Sim.Environment;TSet<int32> WetSectors;int32 Channels=0,ReachSamples=0,CurveSamples=0,RouteAnchors=0;
    E.ForEachRiver([&](const auto& River)
    {
        if(River.Num()<2)return;++Channels;
        for(const auto& P:River){++CurveSamples;if(P.RoutingAnchor)++RouteAnchors;}
        for(int32 End:{0,River.Num()-1})TestTrue(TEXT("Regional streams continue beyond the map instead of starting as round pools"),FMath::Max(FMath::Abs(River[End].Position.X),FMath::Abs(River[End].Position.Y))>G->Sim.WorldHalfSize*3);
        for(int32 I=1;I<River.Num();++I)
        {
            TestTrue(TEXT("Every drainage reach is downhill or lake-level"),River[I].Height<=River[I-1].Height);
            const FVector2D Direction=(River[I].Position-River[I-1].Position).GetSafeNormal(),Side(-Direction.Y,Direction.X);
            const int32 Samples=FMath::Max(2,FMath::CeilToInt((River[I].Position-River[I-1].Position).Size()/1000));
            for(int32 J=1;J<Samples;++J)
            {
                const FVector2D P=FMath::Lerp(River[I-1].Position,River[I].Position,double(J)/Samples);
                const int32 X=FMath::FloorToInt((P.X+G->Sim.WorldHalfSize)/(G->Sim.WorldHalfSize*2)),Y=FMath::FloorToInt((P.Y+G->Sim.WorldHalfSize)/(G->Sim.WorldHalfSize*2));
                if(X<-1||X>1||Y<-1||Y>1)continue;WetSectors.Add((Y+1)*3+X+1);++ReachSamples;
                const auto W=E.WaterAt(P);
                TestTrue(TEXT("The actual regional terrain puts water over a submerged bed"),W.Present&&G->SimulationTerrainHeight(P)<W.Surface);
                for(double Sign:{-1.,1.})
                {
                    const FVector2D Bank=P+Side*Sign*(E.RiverHalfWidth+E.BankWidth*.7);
                    if(!E.WaterAt(Bank).Present)TestTrue(TEXT("A dry river bank rises above the water, never floating below an elevated ribbon"),G->SimulationTerrainHeight(Bank)>W.Surface);
                    const FVector2D Valley=P+Side*Sign*(E.RiverHalfWidth+E.ValleyWidth*.9);
                    if(!E.WaterAt(Valley).Present)TestTrue(TEXT("River lies lower than its broad surrounding valley"),G->SimulationTerrainHeight(Valley)>W.Surface);
                }
            }
        }
    });
    TestEqual(TEXT("Drainage has three independently authored rivers"),Channels,3);
    TestEqual(TEXT("Surface water occurs in every region of the 3x3 neighborhood"),WetSectors.Num(),9);
    TestTrue(TEXT("Hydrology covers hundreds of real-terrain samples"),ReachSamples>300);
    TestTrue(TEXT("Shared river curve rounds authored bends without inflating the pathfinding graph"),CurveSamples>RouteAnchors*2&&CurveSamples<RouteAnchors*8&&RouteAnchors<60);
    for(const auto& Lake:E.Lakes)
    {
        double Minimum=10,Maximum=0;
        for(int32 I=0;I<Lake.Shoreline.Num();++I)
        {
            const FVector2D Q=Lake.Shoreline[I],P=Lake.Center+Q*Lake.Radii;Minimum=FMath::Min(Minimum,Q.Size());Maximum=FMath::Max(Maximum,Q.Size());
            TestTrue(TEXT("Rendered polygon and wet/dry query share the same shoreline"),FMath::Abs(Lake.RadiusAt(P)-1)<1.e-6);
            TestTrue(TEXT("Just inside the irregular lake is submerged"),E.WaterAt(Lake.Center+Q*Lake.Radii*.99).Present);
            const auto Inside=Lake.Center+Q*Lake.Radii*.95;
            TestTrue(TEXT("Lake basin stays below its horizontal water level"),G->SimulationTerrainHeight(Inside)<Lake.Height);
        }
        TestTrue(TEXT("Pond shore is irregular rather than a perfect ellipse"),Maximum-Minimum>.12);
        // Test the actual polygon perimeter, including a concave shoreline bay.
        FSeigeEnvironment Isolated=E;Isolated.River.Empty();Isolated.AdditionalRivers.Empty();Isolated.Lakes={Lake};
        for(int32 I=0;I<Lake.Shoreline.Num();I+=7)
        {
            const FVector2D Q=Lake.Shoreline[I]*Lake.Radii;
            TestFalse(TEXT("Short crossing at any irregular shore is physically blocked"),Isolated.SegmentDry(Lake.Center+Q*.95,Lake.Center+Q*1.05));
            TestTrue(TEXT("Dry points beyond the polygon are not blocked by an oversized ellipse"),Isolated.CanStand(Lake.Center+Q*1.05));
        }
    }
    for(const auto& Cliff:E.Cliffs)
    {
        const double A=FMath::DegreesToRadians(Cliff.AngleDegrees);const FVector2D Along(FMath::Cos(A),FMath::Sin(A)),Across(-Along.Y,Along.X);
        const double Step=10,Face=-.25*Cliff.Radii.Y;
        const double FrontGrade=FMath::Abs(Cliff.Elevation(Cliff.Center+Across*(Face+Step))-Cliff.Elevation(Cliff.Center+Across*(Face-Step)))/(2*Step);
        const double BackAt=.7*Cliff.Radii.Y;
        const double BackGrade=FMath::Abs(Cliff.Elevation(Cliff.Center+Across*(BackAt+Step))-Cliff.Elevation(Cliff.Center+Across*(BackAt-Step)))/(2*Step);
        TestTrue(TEXT("Escarpment has a steep fault face and gentler back slope"),FrontGrade>BackGrade*2);
        TestTrue(TEXT("Rock ridge continues along its length instead of a round hill rim"),Cliff.Elevation(Cliff.Center+Along*(Cliff.Radii.X*.45))>Cliff.Height*.6);
        TestEqual(TEXT("Eroded scarp ends return to the surrounding terrain"),Cliff.Elevation(Cliff.Center+Along*(Cliff.Radii.X*1.2)),0.);
    }
    TestEqual(TEXT("Each regional river has several natural crossings"),E.Fords.Num(),9);
    for(const auto& Ford:E.Fords)
    {
        FVector2D Direction(0,1);double Nearest=TNumericLimits<double>::Max();
        E.ForEachRiver([&](const auto& R){for(int32 I=1;I<R.Num();++I)
        {
            const FVector2D D=R[I].Position-R[I-1].Position;const double T=FMath::Clamp(FVector2D::DotProduct(Ford.Center-R[I-1].Position,D)/D.SizeSquared(),0.,1.);
            const double Distance=FVector2D::Distance(Ford.Center,R[I-1].Position+D*T);if(Distance<Nearest){Nearest=Distance;Direction=D.GetSafeNormal();}
        }});
        const FVector2D Side(-Direction.Y,Direction.X);const double Reach=E.RiverHalfWidth+E.BankWidth+100;
        const FVector2D A=Ford.Center-Side*Reach,B=Ford.Center+Side*Reach;
        TestFalse(TEXT("Ford stays water and cannot host building or deposit plots"),E.CanStand(Ford.Center,20));
        TestFalse(TEXT("Strict dry-only query still rejects the wet crossing"),E.SegmentDry(A,B,20));
        TestTrue(TEXT("A body with real clearance can traverse the whole shallow shelf"),E.SegmentTraversable(A,B,20));
        TestTrue(TEXT("A worker standing mid-ford can replan without a dry-only endpoint failure"),E.CanTraverse(Ford.Center,20));
        const auto W=E.WaterAt(Ford.Center);TestTrue(TEXT("Ford is continuous shallow water above a raised gravel bed"),W.Present&&W.Depth<=Ford.Depth+1.e-6&&W.Depth>0&&FMath::IsNearlyEqual(G->SimulationTerrainHeight(Ford.Center),W.Surface-W.Depth,.001));
        const FVector2D Deep=Ford.Center+Direction*(Ford.Radius+Ford.Blend+900);
        TestFalse(TEXT("Ordinary deep river remains physically impassable"),E.SegmentTraversable(Deep-Side*Reach,Deep+Side*Reach,20));
        TestFalse(TEXT("A long diagonal cannot use one ford intersection to tunnel through deep water"),E.SegmentTraversable(A,Deep+Side*Reach,20));
        const FVector2D Offset(FMath::FloorToDouble((Ford.Center.X+G->Sim.WorldHalfSize)/(G->Sim.WorldHalfSize*2))*G->Sim.WorldHalfSize*2,FMath::FloorToDouble((Ford.Center.Y+G->Sim.WorldHalfSize)/(G->Sim.WorldHalfSize*2))*G->Sim.WorldHalfSize*2);
        FSeigeSimulation Regional;FString Error;if(!Regional.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815,Offset)){AddError(Error);return false;}
        Regional.Mobility.SetTerrainSampler([&,Offset](FVector2D P){return G->SimulationTerrainHeight(P+Offset);});
        TArray<FVector2D> Route;TestTrue(TEXT("Transport route solver can cross every regional natural ford"),Regional.FindMobilityRoute(TEXT("transport"),A-Offset,B-Offset,Route));
        const int32 Steps=FMath::CeilToInt((B-A).Size()/5.);FVector2D Previous=A;
        for(int32 I=1;I<=Steps;++I)
        {
            const FVector2D Next=FMath::Lerp(A,B,double(I)/Steps);
            if(!Regional.Mobility.MovementAllowed(Regional,TEXT("transport"),Previous-Offset,Next-Offset))
            {AddError(FString::Printf(TEXT("Actual short transport step blocked across ford %.2f,%.2f at %.2f,%.2f"),Ford.Center.X,Ford.Center.Y,Next.X,Next.Y));return false;}Previous=Next;
        }
    }
    // Actual seed2222/sector0 failed here after a long edge passed its coarse
    // grade check. Crossing an adjacent river-reach bisector switched surface
    // projection by ~2cm and repeatedly stopped workers at this exact bank point.
    const FVector2D Reported(-49740.8004728249,-57598.53950845421),Travel=FVector2D(2918.348428262166,-3260.159205025987).GetSafeNormal();
    FSeigeSimulation BankColony;FString BankError;const FVector2D BankOffset(-60000,-60000);
    if(!BankColony.Initialize(FPaths::ProjectDir()/TEXT("Rules"),BankError,false,false,2222,BankOffset)){AddError(BankError);return false;}
    BankColony.Mobility.SetTerrainSampler([&,BankOffset](FVector2D P){return G->SimulationTerrainHeight(P+BankOffset);});
    for(int32 I=-200;I<200;++I)
    {
        const FVector2D A=Reported+Travel*(I*.1),B=Reported+Travel*((I+1)*.1);
        const double Grade=FMath::Abs(G->SimulationTerrainHeight(B)-G->SimulationTerrainHeight(A))/.1;
        if(Grade>.18||!BankColony.Mobility.MovementAllowed(BankColony,TEXT("drone"),A-BankOffset,B-BankOffset)||
            !BankColony.Mobility.MovementAllowed(BankColony,TEXT("transport"),A-BankOffset,B-BankOffset))
        {AddError(FString::Printf(TEXT("Reported riverbank bisector still blocks a physical .6cm step at %.8f,%.8f grade %.8f"),B.X,B.Y,Grade));return false;}
    }
    AddInfo(FString::Printf(TEXT("Shared drainage validated: %d reaches sampled across %d sectors, %d irregular lake polygons, %d oriented scarps"),ReachSamples,WetSectors.Num(),E.Lakes.Num(),E.Cliffs.Num()));
    return true;
}
#endif
