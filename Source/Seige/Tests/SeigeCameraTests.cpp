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
    Reject(TEXT("Horizontal sky rays miss"),Above,FVector::ForwardVector);
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
    const float Yaws[]={0,90,135,270},Pitches[]={25,52,75},Zooms[]={G.MinimumZoom,G.DefaultZoom,float(G.Sim.WorldHalfSize*12)};
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
            TestTrue(TEXT("Orbit camera stays at least 300 rendered centimeters above terrain"),Position.Z>=G.GroundHeight(Local)*G.RenderScale+300-.02);
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
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeNearestGroundRayTest,"Seige.Camera.NearestTerrainCrossing",CameraFlags)
bool FSeigeNearestGroundRayTest::RunTest(const FString& Parameters)
{
    FCameraWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    const FVector2D Starts[]={FVector2D(-20000,-15000),FVector2D(20000,-12000),FVector2D(-14000,18000)};
    const double Yaws[]={15,150,-65};int32 Compared=0;
    for(int32 Ray=0;Ray<UE_ARRAY_COUNT(Starts);++Ray)
    {
        const FVector Origin(Starts[Ray],G.GroundHeight(Starts[Ray])+240);
        const double Angle=FMath::DegreesToRadians(Yaws[Ray]);
        const FVector Direction=FVector(FMath::Cos(Angle),FMath::Sin(Angle),-.06).GetSafeNormal();
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
    TestEqual(TEXT("All independent shallow-ray cases were compared"),Compared,3);
    return true;
}
#endif
