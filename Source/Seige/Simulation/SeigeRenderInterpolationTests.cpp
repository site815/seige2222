#include "SeigeRenderInterpolation.h"
#include "SeigeWorksiteLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRenderInterpolationTest,"Seige.Simulation.RenderInterpolationByIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeRenderInterpolationTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation S;S.Time=10;
    FSeigeCourier C;C.Id=12;C.Position=FVector2D(100,200);C.Resource=TEXT("cargo");C.Amount=4;S.Couriers.Add(C);
    FSeigeEnemy E;E.Id=19;E.Position=FVector2D(-40,20);S.Enemies.Add(E);
    FSeigeBuilding B;B.Id=3;B.IsConstructing=true;B.ConstructionProgress=.2;S.Buildings.Add(B);
    FSeigeRenderSnapshot Previous;Previous.Capture(S);
    S.Time=10.05;S.Couriers[0].Position+=FVector2D(20,0);S.Enemies[0].Position+=FVector2D(0,10);S.Buildings[0].ConstructionProgress=.3;
    const auto Quarter=Previous.Courier(S.Couriers[0],.25),ThreeQuarters=Previous.Courier(S.Couriers[0],.75);
    TestTrue(TEXT("Two display frames within one fixed step have different positions"),Quarter.Equals(FVector2D(105,200))&&ThreeQuarters.Equals(FVector2D(115,200)));
    TestTrue(TEXT("Enemy motion interpolates the same authoritative step"),Previous.Enemy(S.Enemies[0],.5).Equals(FVector2D(-40,25)));
    TestTrue(TEXT("Builder time is fractional between fixed simulation ticks"),FMath::IsNearlyEqual(Previous.RenderTime(S,.25),10.0125));
    TestTrue(TEXT("Construction reveal follows the same interpolated progress"),FMath::IsNearlyEqual(Previous.Progress(S.Buildings[0],.5),.25));
    TestEqual(TEXT("Paused repeated display does not advance its clock"),Previous.RenderTime(S,.25),Previous.RenderTime(S,.25));
    FSeigeCourier New;New.Id=99;New.Position=FVector2D(900,900);S.Couriers.Insert(New,0);
    TestTrue(TEXT("Array insertions cannot interpolate a different courier identity"),Previous.Courier(S.Couriers[1],.5).Equals(FVector2D(110,200)));
    TestTrue(TEXT("Newly spawned courier starts at its actual position"),Previous.Courier(S.Couriers[0],.1).Equals(New.Position));
    TestEqual(TEXT("Rendering neither consumes nor invents cargo"),S.Couriers[1].Amount,4.);
    S.Time=0;S.Couriers[1].Position=FVector2D(700,800);Previous.Capture(S);
    TestTrue(TEXT("Reset after load or new scenario discards old motion"),Previous.Courier(S.Couriers[1],0).Equals(FVector2D(700,800)));
    TestEqual(TEXT("Reset builder time cannot inherit old session time"),Previous.RenderTime(S,.5),0.);
    FSeigeBuildingDef D;D.Id=TEXT("test_loading_building");D.Footprint=100;S.BuildingDefs.Add(D.Id,D);
    S.Buildings.Reset();FSeigeBuilding Source;Source.Id=101;Source.DefId=D.Id;Source.Position=FVector2D::ZeroVector;
    FSeigeBuilding Target=Source;Target.Id=102;Target.Position=FVector2D(1000,0);S.Buildings.Add(Source);S.Buildings.Add(Target);
    FSeigeCourier Cargo;Cargo.Id=103;Cargo.SourceId=Source.Id;Cargo.TargetId=Target.Id;Cargo.Position=Source.Position;Cargo.Amount=8;
    FSeigeRenderSnapshot Ports;
    TestTrue(TEXT("Dispatch is visible at the exterior loading port"),Ports.CourierAtLoadingPorts(S,Cargo,.5,10).Equals(FVector2D(110,0)));
    Cargo.Position=FVector2D(500,0);
    TestTrue(TEXT("Open route display keeps actual cargo position"),Ports.CourierAtLoadingPorts(S,Cargo,.5,10).Equals(Cargo.Position));
    Cargo.Position=FVector2D(960,0);
    TestTrue(TEXT("Cargo waits visibly outside until actual arrival"),Ports.CourierAtLoadingPorts(S,Cargo,.5,10).Equals(FVector2D(890,0)));
    TestTrue(TEXT("Loading-port display does not alter authoritative movement"),Cargo.Position.Equals(FVector2D(960,0)));
    TestEqual(TEXT("Loading-port hold cannot deliver or consume goods"),Cargo.Amount,8.);
    S.Buildings[1].Position=FVector2D(1000,1000);Cargo.Position=FVector2D(980,980);
    TestTrue(TEXT("Diagonal dock holds clear the square footprint corner"),Ports.CourierAtLoadingPorts(S,Cargo,.5,10).Equals(FVector2D(890,890)));
    S.Buildings[1].Position=FVector2D(1000,0);Cargo.Position=FVector2D(500,0);S.Couriers.Reset();S.Couriers.Add(Cargo);Ports.Capture(S);
    Cargo.Position=FVector2D(520,0);
    TestTrue(TEXT("Port rendering retains frame interpolation on the open route"),Ports.CourierAtLoadingPorts(S,Cargo,.25,10).Equals(FVector2D(505,0)));
    // This diagonal neighbor is legal under circular building spacing, but its
    // square footprint covers one of the preferred exterior stockyard slots.
    TArray<FSeigeWorksiteBounds> Occupied;Occupied.Add({FVector2D(230,-230),140});
    FVector2D Yard,Repeat;
    TestTrue(TEXT("Stockyard finds an exterior slot beside a diagonal neighbor"),SeigeFindExteriorPosition(FVector2D::ZeroVector,115,4,18,Occupied,Yard));
    SeigeFindExteriorPosition(FVector2D::ZeroVector,115,4,18,Occupied,Repeat);
    TestTrue(TEXT("Exterior placement is deterministic"),Yard.Equals(Repeat));
    for(int32 Slot=0;Slot<9;++Slot)
    {
        if(!TestTrue(TEXT("Available resources obtain distinct yard positions"),SeigeFindExteriorPosition(FVector2D::ZeroVector,115,Slot,18,Occupied,Yard)))break;
        TestTrue(TEXT("Pile remains outside its own footprint"),FMath::Max(FMath::Abs(Yard.X),FMath::Abs(Yard.Y))>=133);
        for(const auto& Area:Occupied)
        {
            const FVector2D Gap=Yard-Area.Position;
            TestTrue(TEXT("Pile geometry cannot overlap known buildings or earlier piles"),FMath::Abs(Gap.X)>=Area.HalfWidth+18||FMath::Abs(Gap.Y)>=Area.HalfWidth+18);
        }
        Occupied.Add({Yard,18});
    }
    TArray<FSeigeWorksiteBounds> Enclosed;Enclosed.Add({FVector2D::ZeroVector,10000});
    TestFalse(TEXT("Impossible exterior layout is omitted instead of intersecting geometry"),SeigeFindExteriorPosition(FVector2D::ZeroVector,115,0,18,Enclosed,Yard));
    return true;
}
#endif
