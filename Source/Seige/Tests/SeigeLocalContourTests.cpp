#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeLocalContourTest,"Seige.Transport.LocalContourBeatsDistantGeographyDetour",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeLocalContourTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,108)){AddError(Error);return false;}
    // Geometry-only fixture: a nearby steep knoll blocks the direct segment.
    // A distant lake contributes otherwise valid visibility-graph waypoints.
    S.Buildings.Empty();S.Roads.Empty();S.Environment.River.Empty();S.Environment.AdditionalRivers.Empty();S.Environment.Fords.Empty();S.Environment.Cliffs.Empty();S.Environment.Lakes.Empty();S.Environment.WorldOffset=FVector2D::ZeroVector;S.Environment.Enabled=true;
    FSeigeLake Lake;Lake.Center=FVector2D(0,8000);Lake.Radii=FVector2D(300,300);Lake.BuildShoreline();S.Environment.Lakes.Add(Lake);
    const FVector2D A(-500,0),B(500,0);
    S.Mobility.SetTerrainSampler([](FVector2D P){return 200.*FMath::Max(0.,1.-P.Size()/100.);});
    TestFalse(TEXT("The direct chord crosses impassable slope"),S.Mobility.MovementAllowed(S,TEXT("drone"),A,B));
    TArray<FVector2D> Route;
    if(!TestTrue(TEXT("A bounded local contour reaches the actual destination"),S.FindRoute(A,B,Route)))return false;
    double Length=0;FVector2D Last=A;
    for(const auto& P:Route)
    {
        Length+=FVector2D::Distance(Last,P);
        TestTrue(TEXT("Each selected contour chord passes full physical mobility checks"),S.Mobility.MovementAllowed(S,TEXT("drone"),Last,P));
        TestTrue(TEXT("The shorter contour never clips the water body"),S.Environment.SegmentTraversable(Last,P));
        Last=P;
    }
    TestTrue(TEXT("The short walk does not visit a lake kilometres away"),Length<2000);
    TestTrue(TEXT("The physical destination remains exact"),Last.Equals(B,1.e-8));
    // If the local envelope really is blocked, retain a valid long graph path.
    S.Mobility.SetTerrainSampler([](FVector2D P){return FMath::Abs(P.Y)<2500?500.*FMath::Max(0.,1.-FMath::Abs(P.X)/100.):0.;});
    if(TestTrue(TEXT("A legitimate distant detour survives failed local refinement"),S.FindRoute(A,B,Route)))
    {Length=0;Last=A;for(const auto& P:Route){Length+=FVector2D::Distance(Last,P);TestTrue(TEXT("Fallback segments retain physical clearance"),S.Mobility.MovementAllowed(S,TEXT("drone"),Last,P));Last=P;}TestTrue(TEXT("The impassable local barrier really requires leaving the envelope"),Length>5000);}
    S.Mobility.SetTerrainSampler([](FVector2D){return std::numeric_limits<double>::quiet_NaN();});
    TestFalse(TEXT("Neither graph nor contour can bypass unavailable terrain"),S.FindRoute(A,B,Route));
    TestTrue(TEXT("Unreachable route returns no usable waypoints"),Route.IsEmpty());
    return true;
}
#endif
