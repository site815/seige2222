#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags CameraFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FCameraWorld : FTestWorldWrapper
{
    ASeigeGameMode* Game=nullptr;
    bool Prepare(FAutomationTestBase& Test)
    {
        if(!CreateTestWorld(EWorldType::Game)){ForwardErrorMessages(&Test);return false;}
        FURL Url;Url.AddOption(*FString::Printf(TEXT("game=%s"),*ASeigeGameMode::StaticClass()->GetPathName()));
        if(!GetTestWorld()->SetGameMode(Url)){Test.AddError(TEXT("Cannot create camera test game mode"));return false;}
        Game=Cast<ASeigeGameMode>(GetTestWorld()->GetAuthGameMode());
        if(!Game||!Game->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Game->Error))
        {Test.AddError(Game?Game->Error:TEXT("Missing camera test game mode"));return false;}
        Game->Ready=true;Game->Screen=TEXT("playing");
        // Exercise the same triangle-height cache as rendering without creating
        // scene actors, materials or a viewport in the native test world.
        Game->RebuildTerrainHeights();return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeBoundedGroundRayTest,"Seige.Camera.BoundedTerrainRay",CameraFlags)
bool FSeigeBoundedGroundRayTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    const double Half=G.Sim.WorldHalfSize,Extent=Half*3,CoarseStep=Half*2/128,Epsilon=.001;
    TArray<FVector2D> Points={FVector2D::ZeroVector,FVector2D(2100,-1600),FVector2D(-Extent*.85,Extent*.8)};
    const double AlongValues[]={-Half*.37,CoarseStep*31.5,-CoarseStep*17.5};
    for(int32 Axis=0;Axis<2;++Axis)for(double Sign:{-1.,1.})for(double Along:AlongValues)
    {
        const FVector2D Seam=Axis==0?FVector2D(Sign*Half,Along):FVector2D(Along,Sign*Half);
        const FVector2D Normal=Axis==0?FVector2D(1,0):FVector2D(0,1);
        const double Before=G.GroundHeight(Seam-Normal*Epsilon),After=G.GroundHeight(Seam+Normal*Epsilon);
        TestTrue(TEXT("Mixed-resolution sector seams have no height jump on either side"),FMath::Abs(Before-After)<.01);
        const double Lower=FMath::FloorToDouble(Along/CoarseStep)*CoarseStep;
        const FVector2D A=Axis==0?FVector2D(Sign*Half,Lower):FVector2D(Lower,Sign*Half);
        const FVector2D B=A+(Axis==0?FVector2D(0,CoarseStep):FVector2D(CoarseStep,0));
        const double Shared=FMath::Lerp(G.GroundHeight(A),G.GroundHeight(B),(Along-Lower)/CoarseStep);
        TestTrue(TEXT("Shared edge agrees with the common coarse-grid vertices"),FMath::Abs(G.GroundHeight(Seam)-Shared)<.001);
        Points.Add(Seam);Points.Add(Seam-Normal*Epsilon);Points.Add(Seam+Normal*Epsilon);
    }
    for(const FVector2D& Point:Points)
    {
        const FVector Expected=G.RenderPosition(Point);
        FVector Hit;
        const FVector Origin=Expected+FVector(0,0,8000*G.RenderScale);
        if(TestTrue(TEXT("Downward ray enters the bounded world and finds terrain"),G.TraceGroundRay(Origin,FVector(0,0,-7),Hit)))
        {
            TestTrue(TEXT("Terrain hit respects logical-to-world rendering scale"),Hit.Equals(Expected,.02));
            TestTrue(TEXT("The returned point is forward along the ray"),Hit.Z<Origin.Z);
        }
    }
    const FVector Ground=G.RenderPosition(FVector2D::ZeroVector),Above=Ground+FVector(0,0,1000);
    auto Reject=[&](const TCHAR* Label,const FVector& Origin,const FVector& Direction)
    {
        FVector Hit(123,456,789);
        TestFalse(Label,G.TraceGroundRay(Origin,Direction,Hit));
        TestTrue(TEXT("Rejected rays do not overwrite the caller's hit position"),Hit.Equals(FVector(123,456,789)));
    };
    Reject(TEXT("Upward rays cannot select ground behind the camera"),Above,FVector::UpVector);
    Reject(TEXT("Horizontal sky rays above the terrain bounds miss"),FVector(0,0,6001*G.RenderScale),FVector::ForwardVector);
    Reject(TEXT("Zero-length directions miss"),Above,FVector::ZeroVector);
    Reject(TEXT("Ground below the ray origin is not fabricated for an underground camera"),Ground-FVector(0,0,100),FVector(0,0,-1));
    Reject(TEXT("A ray outside the neighborhood cannot hit its infinite height function"),FVector((Extent+100)*G.RenderScale,0,20000),FVector(0,0,-1));
    FVector Invalid=Above;Invalid.X=std::numeric_limits<double>::quiet_NaN();
    Reject(TEXT("NaN origin is rejected"),Invalid,FVector(0,0,-1));
    Invalid=FVector(0,0,-1);Invalid.Y=std::numeric_limits<double>::infinity();
    Reject(TEXT("Infinite direction is rejected"),Above,Invalid);
    G.RenderScale=0;Reject(TEXT("Invalid zero rendering scale is rejected"),Above,FVector(0,0,-1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePerspectiveCameraTest,"Seige.Camera.PerspectiveTransformAndPan",CameraFlags)
bool FSeigePerspectiveCameraTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    G.CameraCenter=FVector(1300,-700,0);
    const float Yaws[]={0,90,135,270},Pitches[]={G.MinimumCameraPitch,52,G.MaximumCameraPitch},Zooms[]={G.MinimumZoom,G.DefaultZoom,float(G.Sim.WorldHalfSize*12)};
    for(float Yaw:Yaws)for(float Pitch:Pitches)for(float Width:Zooms)
    {
        G.CameraYaw=Yaw;G.CameraPitch=Pitch;G.Zoom=Width;
        const FTransform Transform=G.CameraTransform();const FVector Position=Transform.GetLocation();
        const FVector Target=G.RenderPosition(FVector2D(G.CameraCenter),70);
        const FVector Aim=Transform.GetRotation().GetForwardVector();
        TestFalse(TEXT("Camera transform stays finite across supported orbit and overview limits"),Transform.ContainsNaN());
        TestTrue(TEXT("Camera remains aimed at its terrain focus"),FVector::DotProduct(Aim,(Target-Position).GetSafeNormal())>1-1.e-8);
        const double MinimumDistance=Width*G.RenderScale/(2*FMath::Tan(FMath::DegreesToRadians(G.CameraFov*.5)));
        TestTrue(TEXT("Perspective zoom has the requested width-equivalent camera distance or extra ground clearance"),FVector::Distance(Position,Target)>=MinimumDistance-.02);
        const FVector2D Local(Position.X/G.RenderScale,Position.Y/G.RenderScale);
        if(FMath::Abs(Local.X)<=G.Sim.WorldHalfSize*3&&FMath::Abs(Local.Y)<=G.Sim.WorldHalfSize*3)
            TestTrue(TEXT("Orbit camera respects configured ground clearance in rendered centimeters"),Position.Z>=G.GroundHeight(Local)*G.RenderScale+G.CameraGroundClearance-.02);
        FVector Hit;
        if(TestTrue(TEXT("Camera center direction intersects bounded terrain"),G.TraceGroundRay(Position,Aim,Hit)))
            TestTrue(TEXT("Camera ray returns a point on the same ground surface used for placement"),FMath::Abs(Hit.Z-G.GroundHeight(FVector2D(Hit)/G.RenderScale)*G.RenderScale)<.05);
    }
    G.CameraYaw=0;
    TestTrue(TEXT("At yaw zero forward follows positive X"),G.CameraPanDirection(1,0).Equals(FVector2D(1,0),1.e-6));
    TestTrue(TEXT("At yaw zero right follows positive Y"),G.CameraPanDirection(0,1).Equals(FVector2D(0,1),1.e-6));
    G.CameraYaw=90;
    TestTrue(TEXT("Forward rotates with camera yaw"),G.CameraPanDirection(1,0).Equals(FVector2D(0,1),1.e-6));
    TestTrue(TEXT("Right rotates with camera yaw"),G.CameraPanDirection(0,1).Equals(FVector2D(-1,0),1.e-6));
    TestTrue(TEXT("Diagonal panning cannot move faster than a single axis"),FMath::IsNearlyEqual(G.CameraPanDirection(1,1).Size(),1.,1.e-6));
    TestTrue(TEXT("No input leaves the focus stationary"),G.CameraPanDirection(0,0).IsNearlyZero());
    G.CameraPitch=52;G.Zoom=G.MinimumZoom;const FTransform Closest=G.CameraTransform();
    G.Zoom=0;TestTrue(TEXT("Camera transform clamps distance below the minimum"),G.CameraTransform().Equals(Closest,.02));

    // Home's level foundation isolates the deliberate close-zoom pitch change
    // from the separate clearance correction required on a nearby hillside.
    G.CameraCenter=FVector(G.HomePosition(),0);G.CameraYaw=135;G.CameraPitch=67;G.Zoom=G.DefaultZoom;
    const FTransform OriginalOrbit=G.CameraTransform();const float StoredPitch=G.CameraPitch;
    TestTrue(TEXT("Normal zoom uses the chosen orbit angle"),FMath::Abs(OriginalOrbit.Rotator().Pitch+StoredPitch)<.001);
    G.Zoom=G.MinimumZoom;const FTransform GroundView=G.CameraTransform();
    const FVector GroundPosition=GroundView.GetLocation();
    const double Clearance=GroundPosition.Z-G.GroundHeight(FVector2D(GroundPosition)/G.RenderScale)*G.RenderScale;
    TestTrue(TEXT("Closest zoom keeps the configured camera ground clearance"),Clearance>=G.CameraGroundClearance-.02);
    TestTrue(TEXT("Closest zoom reaches a near-ground view rather than remaining high above the colony"),Clearance<=G.CameraGroundClearance+100);
    TestTrue(TEXT("Closest zoom lowers the viewing angle toward the configured minimum"),FMath::Abs(GroundView.Rotator().Pitch+G.MinimumCameraPitch)<1);
    TestEqual(TEXT("Automatic close-zoom tilt preserves the user's stored orbit angle"),G.CameraPitch,StoredPitch);
    FVector GroundHit;
    if(TestTrue(TEXT("Near-ground center ray still finds the forward terrain surface"),G.TraceGroundRay(GroundPosition,GroundView.GetRotation().GetForwardVector(),GroundHit)))
        TestTrue(TEXT("Near-ground selection agrees with the cached visible surface"),FMath::Abs(GroundHit.Z-G.GroundHeight(FVector2D(GroundHit)/G.RenderScale)*G.RenderScale)<.05);
    G.Zoom=G.DefaultZoom*.45f;
    TestTrue(TEXT("Zooming beyond the close-view transition restores the stored orbit angle"),FMath::Abs(G.CameraTransform().Rotator().Pitch+StoredPitch)<.001);
    G.Zoom=G.DefaultZoom;
    TestTrue(TEXT("Zooming back out restores the original orbit transform"),G.CameraTransform().Equals(OriginalOrbit,.02));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeNearestGroundRayTest,"Seige.Camera.NearestTerrainCrossing",CameraFlags)
bool FSeigeNearestGroundRayTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    const FVector2D Starts[]={FVector2D(-20000,-15000),FVector2D(20000,-12000),FVector2D(-14000,18000),
        FVector2D(1200,600),FVector2D(-3100,900),FVector2D(2400,-2300)};
    const double Yaws[]={15,150,-65,25,-35,145};int32 Compared=0;
    for(int32 Ray=0;Ray<UE_ARRAY_COUNT(Starts);++Ray)
    {
        // The close cases graze the fine focused-sector relief. A later valley
        // intersection must not replace an earlier small ridge crossing.
        const FVector Origin(Starts[Ray],G.GroundHeight(Starts[Ray])+(Ray<3?240:25));
        const double Angle=FMath::DegreesToRadians(Yaws[Ray]);
        const FVector Direction=FVector(FMath::Cos(Angle),FMath::Sin(Angle),Ray<3?-.06:-.02).GetSafeNormal();
        // Independent fine sampling establishes the first crossing, rather than
        // assuming the far end of a shallow ray is the surface the player sees.
        double Previous=0,Expected=-1;
        for(double T=2;T<=50000;T+=2)
        {
            const FVector P=Origin+Direction*T;
            if(FMath::Abs(P.X)>G.Sim.WorldHalfSize*3||FMath::Abs(P.Y)>G.Sim.WorldHalfSize*3)break;
            if(P.Z<=G.GroundHeight(FVector2D(P)))
            {
                double A=Previous,B=T;
                for(int32 I=0;I<18;++I){const double Mid=(A+B)*.5;const FVector M=Origin+Direction*Mid;if(M.Z>G.GroundHeight(FVector2D(M)))A=Mid;else B=Mid;}
                Expected=(A+B)*.5;break;
            }
            Previous=T;
        }
        if(!TestTrue(TEXT("Shallow-ray fixture has an in-bounds terrain crossing"),Expected>=0))continue;
        FVector Hit;
        if(TestTrue(TEXT("Shallow camera ray resolves a forward surface hit"),G.TraceGroundRay(Origin*G.RenderScale,Direction,Hit)))
        {
            ++Compared;
            const double Distance=FVector::DotProduct(Hit/G.RenderScale-Origin,Direction);
            TestTrue(TEXT("Picking chooses the nearest visible ground crossing, not a later hillside"),FMath::Abs(Distance-Expected)<.1);
        }
    }
    TestEqual(TEXT("All independent shallow-ray cases were compared"),Compared,int32(UE_ARRAY_COUNT(Starts)));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAboveHorizonGroundRayTest,"Seige.Camera.AboveHorizonTerrain",CameraFlags)
bool FSeigeAboveHorizonGroundRayTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    const FVector2D Start(1300,450);
    const FVector Origin(Start,G.GroundHeight(Start)+30);
    for(double Rise:{0.,.08})
    {
        const FVector Direction=FVector(1,0,Rise).GetSafeNormal();
        double Previous=0,Expected=-1;
        // Independent fine sampling proves that these are visible uphill hits,
        // including the upward ray that the previous sign check discarded.
        for(double T=2;T<=5000;T+=2)
        {
            const FVector P=Origin+Direction*T;
            if(P.Z<=G.GroundHeight(FVector2D(P)))
            {
                double A=Previous,B=T;
                for(int32 I=0;I<18;++I)
                {const double Mid=(A+B)*.5;const FVector M=Origin+Direction*Mid;if(M.Z>G.GroundHeight(FVector2D(M)))A=Mid;else B=Mid;}
                Expected=(A+B)*.5;break;
            }
            Previous=T;
        }
        if(!TestTrue(TEXT("The above-horizon fixture has a forward terrain crossing"),Expected>0))continue;
        FVector Hit;
        if(TestTrue(Rise>0?TEXT("Upward view ray selects the visible hillside"):TEXT("Horizontal view ray selects the visible hillside"),G.TraceGroundRay(Origin*G.RenderScale,Direction,Hit)))
        {
            const double Distance=FVector::DotProduct(Hit/G.RenderScale-Origin,Direction);
            TestTrue(TEXT("Above-horizon picking returns the nearest visible crossing"),FMath::Abs(Distance-Expected)<.1);
            TestTrue(TEXT("Above-horizon hit agrees with the cached rendered terrain"),FMath::Abs(Hit.Z-G.GroundHeight(FVector2D(Hit)/G.RenderScale)*G.RenderScale)<.05);
            if(Rise>0)TestTrue(TEXT("The selected uphill terrain is above the camera"),Hit.Z>Origin.Z*G.RenderScale);
        }
    }
    FVector SkyHit(123,456,789);
    TestFalse(TEXT("A vertical ray into clear sky still misses"),G.TraceGroundRay(Origin*G.RenderScale,FVector::UpVector,SkyHit));
    TestTrue(TEXT("A clear-sky miss preserves the caller's hit value"),SkyHit.Equals(FVector(123,456,789)));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompactBuildingPadTest,"Seige.Camera.CompactBuildingPads",CameraFlags)
bool FSeigeCompactBuildingPadTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    if(!G.Sim.SetInitialCorePosition(FVector2D(613,487),G.Error)){AddError(G.Error);return false;}
    G.Sim.Tick(G.Sim.BuildingDefs[G.Sim.CoreDefinition].ConstructionSeconds+G.Sim.FixedStepSeconds());
    if(!G.Sim.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-713,319),G.Error))
    {AddError(G.Error);return false;}
    const double Step=G.Sim.WorldHalfSize*2/G.DetailedTerrainResolution;
    TArray<FVector2D> Outside,DepositSamples;
    for(const auto& B:G.Sim.Buildings)if(const auto* D=G.Sim.Definition(B))
    {
        // Two cells beyond the compact analytical influence also exclude any
        // interpolation triangle that touches a modified foundation vertex.
        const double Radius=D->Footprint*G.CorePadOuterRatio+2*Step;
        for(double X:{-1.,0.,1.})for(double Y:{-1.,0.,1.})if(X!=0||Y!=0)
            Outside.Add(B.Position+FVector2D(X,Y)*Radius);
    }
    for(const auto& N:G.Sim.Nodes)
    {
        DepositSamples.Add(N.Position);
        DepositSamples.Add(N.Position+FVector2D(90,45));
        DepositSamples.Add(N.Position-FVector2D(90,45));
    }
    G.Screen=TEXT("landing");G.RebuildTerrainHeights();
    TArray<double> NaturalOutside,NaturalDeposits;
    for(const FVector2D& P:Outside)NaturalOutside.Add(G.GroundHeight(P));
    for(const FVector2D& P:DepositSamples)NaturalDeposits.Add(G.GroundHeight(P));
    double DepositRelief=0;
    for(int32 I=0;I<NaturalDeposits.Num();I+=3)
        DepositRelief=FMath::Max(DepositRelief,FMath::Max(FMath::Abs(NaturalDeposits[I]-NaturalDeposits[I+1]),FMath::Abs(NaturalDeposits[I]-NaturalDeposits[I+2])));
    TestTrue(TEXT("Natural deposit fixture contains measurable relief"),DepositRelief>.1);

    G.Screen=TEXT("playing");G.RebuildTerrainHeights();
    for(const auto& B:G.Sim.Buildings)if(const auto* D=G.Sim.Definition(B))
    {
        const double Level=G.GroundHeight(B.Position);
        for(double X:{-1.,0.,1.})for(double Y:{-1.,0.,1.})
        {
            const FVector2D P=B.Position+FVector2D(X,Y)*D->Footprint;
            TestTrue(*FString::Printf(TEXT("%s cached foundation is level at footprint (%g,%g)"),*B.DefId,X,Y),
                FMath::Abs(G.GroundHeight(P)-Level)<.001);
            FVector Hit;
            if(TestTrue(TEXT("Building foundation remains pickable on the rendered surface"),
                G.TraceGroundRay(G.RenderPosition(P)+FVector(0,0,6000),FVector(0,0,-1),Hit)))
                TestTrue(TEXT("Foundation ray and cached surface agree"),Hit.Equals(G.RenderPosition(P),.02));
        }
    }
    for(int32 I=0;I<Outside.Num();++I)
        TestTrue(TEXT("Terrain outside compact pad influence recovers its natural cached height"),FMath::Abs(G.GroundHeight(Outside[I])-NaturalOutside[I])<.001);

    G.Screen=TEXT("landing");G.Sim.Nodes.Reset();G.RebuildTerrainHeights();
    for(int32 I=0;I<DepositSamples.Num();++I)
        TestTrue(TEXT("An unbuilt deposit cannot flatten the natural terrain"),FMath::Abs(G.GroundHeight(DepositSamples[I])-NaturalDeposits[I])<.001);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeIncrementalSectorSeamTest,"Seige.Camera.IncrementalSectorSeams",CameraFlags)
bool FSeigeIncrementalSectorSeamTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    const double Half=G.Sim.WorldHalfSize,CoarseStep=Half*2/128,Epsilon=.001,EdgeY=CoarseStep;
    // This legal sensor location reaches the shared boundary, while its narrow
    // pad is much smaller than the coarse edge segment influenced by its height.
    // At Y=0 the natural height terms almost cancel the X slope; one coarse step
    // north produces a substantial changed vertex instead of a submillimeter case.
    if(!G.Sim.SetInitialCorePosition(FVector2D(Half-500,EdgeY),G.Error))
    {AddError(G.Error);return false;}
    G.RebuildTerrainHeights();
    TArray<FVector2D> Samples;
    for(double Side:{-Epsilon,0.,Epsilon})Samples.Add(FVector2D(Half+Side,EdgeY));
    for(double Sign:{-1.,1.})for(double Fraction:{.25,.5,.75,.95})
        for(double Side:{-Epsilon,0.,Epsilon})Samples.Add(FVector2D(Half+Side,EdgeY+Sign*Fraction*CoarseStep));
    TArray<double> Natural;
    for(const auto& P:Samples)Natural.Add(G.GroundHeight(P));
    G.Sim.Tick(G.Sim.BuildingDefs[G.Sim.CoreDefinition].ConstructionSeconds+G.Sim.FixedStepSeconds());
    if(!G.Sim.PlaceBuilding(TEXT("sensor"),FVector2D(Half-100,EdgeY),G.Error))
    {AddError(G.Error);return false;}
    const int32 SensorId=G.Sim.Buildings.Last().Id;
    G.RefreshBuildingPads();
    TArray<double> Added;
    for(const auto& P:Samples)Added.Add(G.GroundHeight(P));
    double MaximumChange=0;
    for(int32 I=0;I<Added.Num();++I)MaximumChange=FMath::Max(MaximumChange,FMath::Abs(Added[I]-Natural[I]));
    AddInfo(FString::Printf(TEXT("Boundary vertex change %.6f logical units; maximum sampled change %.6f"),FMath::Abs(Added[1]-Natural[1]),MaximumChange));
    TestTrue(TEXT("The new foundation changes the actual coarse edge vertex by more than one logical unit"),FMath::Abs(Added[1]-Natural[1])>1);
    TestTrue(TEXT("The new foundation measurably changes the shared edge fixture"),MaximumChange>1);
    for(int32 I=0;I<Added.Num();I+=3)
        TestTrue(TEXT("Incremental construction preserves agreement across the mixed-resolution seam"),FMath::Abs(Added[I]-Added[I+2])<.01);
    G.RebuildTerrainHeights();
    for(int32 I=0;I<Samples.Num();++I)
        TestTrue(TEXT("Incremental edge heights equal a complete terrain rebuild"),FMath::Abs(Added[I]-G.GroundHeight(Samples[I]))<.001);
    if(auto* Sensor=G.Sim.FindBuilding(SensorId))Sensor->Health=0;
    else {AddError(TEXT("Boundary sensor disappeared from the test colony"));return false;}
    G.RefreshBuildingPads();
    TArray<double> Removed;
    for(int32 I=0;I<Samples.Num();++I)
    {
        Removed.Add(G.GroundHeight(Samples[I]));
        TestTrue(TEXT("Removing the boundary foundation restores its previous surface"),FMath::Abs(Removed[I]-Natural[I])<.001);
    }
    G.RebuildTerrainHeights();
    for(int32 I=0;I<Samples.Num();++I)
        TestTrue(TEXT("Incremental foundation removal equals a complete rebuild at the seam"),FMath::Abs(Removed[I]-G.GroundHeight(Samples[I]))<.001);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRegionAndOrbitTest,"Seige.Camera.RegionFocusAndOrbit",CameraFlags)
bool FSeigeRegionAndOrbitTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    G.Zoom=G.DefaultZoom;G.CameraYaw=350;G.CameraPitch=52;
    const FVector Before=G.CameraCenter;
    G.ApplyOrbitDrag(FVector2D(100,20));
    TestTrue(TEXT("100 raw pixels rotate 22 degrees through the yaw wrap"),FMath::IsNearlyEqual(G.CameraYaw,12.f,.001f));
    TestTrue(TEXT("Pitch follows raw mouse displacement without frame time scaling"),FMath::IsNearlyEqual(G.CameraPitch,55.6f,.001f));
    TestTrue(TEXT("Orbit holds its world focus"),Before.Equals(G.CameraCenter));
    G.ApplyOrbitDrag(FVector2D(0,10000));TestEqual(TEXT("Pitch stops at the configured upper orbit limit"),G.CameraPitch,G.MaximumCameraPitch);
    G.ApplyOrbitDrag(FVector2D(0,-10000));TestEqual(TEXT("Low orbit stops at the configured lower orbit limit"),G.CameraPitch,G.MinimumCameraPitch);
    G.Zoom=G.RegionMapZoom;TestTrue(TEXT("Threshold enters cartographic map"),G.IsRegionMap());
    const float Yaw=G.CameraYaw;G.ApplyOrbitDrag(FVector2D(100,10));TestEqual(TEXT("Map cannot be accidentally orbited"),G.CameraYaw,Yaw);
    G.FocusSector(0);TestEqual(TEXT("Focusing a neighbor selects its coordinate space"),G.DetailedSectorIndex(),0);
    TestNull(TEXT("Empty sector exposes no copied home simulation"),G.ViewedSimulation());
    TestFalse(TEXT("Sector focus exits map view"),G.IsRegionMap());
    G.FocusSector(4);TestTrue(TEXT("Home simulation is restored"),G.ViewedSimulation()==&G.Sim);
    G.Screen=TEXT("landing");G.FocusSector(8);TestEqual(TEXT("Deployment cannot move to a neighbor"),G.DetailedSectorIndex(),4);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeHiddenTerrainTest,"Seige.Camera.HiddenNeighborTerrain",CameraFlags)
bool FSeigeHiddenTerrainTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    G.FocusSector(0);G.RebuildTerrainHeights();
    const FVector2D Offset=G.DetailedSectorOffset();
    const FVector2D P=Offset+FVector2D(250,250);const double Empty=G.GroundHeight(P);
    if(!G.Sim.SetInitialCorePosition(FVector2D(613,487),G.Error)){AddError(G.Error);return false;}
    G.RebuildTerrainHeights();
    TestTrue(TEXT("Moving the home core cannot create a plateau in an empty neighboring sector"),FMath::IsNearlyEqual(Empty,G.GroundHeight(P),.001));
    FSeigeNeighbor N;N.Index=0;N.Offset=Offset;N.Type=TEXT("starting");
    if(!N.Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),G.Error)){AddError(G.Error);return false;}
    G.Neighbors.Add(MoveTemp(N));G.RebuildTerrainHeights();
    TestTrue(TEXT("Unknown neighbor core cannot leave a detectable terrain plateau"),FMath::IsNearlyEqual(Empty,G.GroundHeight(P),.001));
    G.Observer=true;G.RebuildTerrainHeights();
    TestTrue(TEXT("Observer's known core uses the building plateau"),FMath::Abs(Empty-G.GroundHeight(P))>.1);
    return true;
}
#endif
