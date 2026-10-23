#include "SeigeGameMode.h"
#include "SeigeUnitInspection.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags InspectionFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeUnitInspectionGeometryTest,"Seige.Interaction.UnitHoverGeometry",InspectionFlags)
bool FSeigeUnitInspectionGeometryTest::RunTest(const FString&)
{
    const FBox Box(FVector(-1,-1,-1),FVector(1,1,1));double Depth=0;
    TestTrue(TEXT("A vehicle's visible box can be picked along a parallel ground ray"),SeigeInspection::RayBox(FVector(-5,0,0),FVector(1,0,0),Box,Depth));
    TestEqual(TEXT("Depth resolves the front surface"),Depth,4.);
    TestFalse(TEXT("Ray parallel to and outside the hull does not hit"),SeigeInspection::RayBox(FVector(-5,2,0),FVector(1,0,0),Box,Depth));
    TestFalse(TEXT("Behind-camera bounds do not intercept clicks"),SeigeInspection::RayBox(FVector(5,0,0),FVector(1,0,0),Box,Depth));
    const FBox2D Rect(FVector2D(10,10),FVector2D(30,30));
    TArray<FSeigeInspectionProjection> Hits={{2,Rect,40},{7,Rect,12},{9,Rect,70}};
    TestEqual(TEXT("Closest rendered body wins overlapping screen projections"),SeigeInspection::Pick(Hits,FVector2D(20,20)),7);
    TestEqual(TEXT("A hill occludes the body behind its surface"),SeigeInspection::Pick(Hits,FVector2D(20,20),5),INDEX_NONE);
    TestEqual(TEXT("Outside all projected bounds does not select"),SeigeInspection::Pick(Hits,FVector2D(31,20)),INDEX_NONE);
    const auto Bars=SeigeInspection::Bars(Rect,25,100,.4,1.5);
    TestEqual(TEXT("Health bar reflects actual partial health"),Bars.HealthFraction,.25);
    TestEqual(TEXT("Construction bar reflects progress"),Bars.ProgressFraction,.4);
    TestTrue(TEXT("Health is above the whole rendered body"),Bars.Health.Max.Y<Rect.Min.Y);
    TestTrue(TEXT("Construction is below the whole rendered body"),Bars.Progress.Min.Y>Rect.Max.Y);
    TestEqual(TEXT("DPI scaling preserves the same visual bar width"),Bars.Health.GetSize().X,168.);
    TestFalse(TEXT("Units have no construction bar"),SeigeInspection::Bars(Rect,10,20,-1).HasProgress);
    TestEqual(TEXT("Invalid max health never divides by zero"),SeigeInspection::Bars(Rect,10,0,-1).HealthFraction,0.);
    FSeigeInspectionRef A{ESeigeInspectKind::Transport,4,1,{}},B{ESeigeInspectKind::Transport,3,1,{}};
    TestFalse(TEXT("Identical vehicle ids in different owners are different selections"),A==B);
    B=A;B.Kind=ESeigeInspectKind::Vehicle;TestFalse(TEXT("Transport and fleet identities cannot alias"),A==B);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeUnitInspectionVisibilityTest,"Seige.Interaction.UnitHoverVisibility",InspectionFlags)
bool FSeigeUnitInspectionVisibilityTest::RunTest(const FString&)
{
    TestTrue(TEXT("Living outdoor rendered and revealed unit is inspectable"),SeigeInspection::Visible(20,true,false,false,true,true));
    TestFalse(TEXT("Dead body is not inspectable"),SeigeInspection::Visible(0,true,false,false,true,true));
    TestFalse(TEXT("Indoor worker is not inspectable"),SeigeInspection::Visible(20,false,false,false,true,true));
    TestFalse(TEXT("Embarked unit is not inspectable"),SeigeInspection::Visible(20,true,true,false,true,true));
    TestFalse(TEXT("Evacuated unit is not inspectable"),SeigeInspection::Visible(20,true,false,true,true,true));
    TestFalse(TEXT("Unrendered unit is not an invisible clickable proxy"),SeigeInspection::Visible(20,true,false,false,false,true));
    TestFalse(TEXT("Unrevealed unit does not leak through fog"),SeigeInspection::Visible(20,true,false,false,true,false));
    FTestWorldWrapper World;
    if(!World.CreateTestWorld(EWorldType::Game)){World.ForwardErrorMessages(this);return false;}
    auto* G=World.GetTestWorld()->SpawnActor<ASeigeGameMode>();
    if(!TestNotNull(TEXT("Game mode exists"),G))return false;
    FString Error;G->Ready=G->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false);
    if(!TestTrue(TEXT("Rules initialize"),G->Ready)){AddError(Error);return false;}
    G->Screen=TEXT("playing");G->CameraCenter=FVector::ZeroVector;G->Zoom=G->DefaultZoom;G->RenderedZoom=G->DefaultZoom;
    auto AddVisual=[&](const FString& Key,FVector Position)
    {
        auto* A=World.GetTestWorld()->SpawnActor<AActor>();auto* Shape=NewObject<UBoxComponent>(A);A->SetRootComponent(Shape);
        Shape->SetBoxExtent(FVector(100,50,40));Shape->SetHiddenInGame(false);Shape->RegisterComponent();A->AddInstanceComponent(Shape);A->SetActorLocation(Position);G->Visuals.Add(Key,A);return A;
    };
    FSeigeTransportVehicle V;V.Id=991;V.Health=G->Sim.TransportVehicles.Definition().Health;V.Position=FVector2D(500,600);
    G->Sim.TransportVehicles.Bodies.Add(V);
    const FVector ActualRendered(3500,1700,400);auto* Actor=AddVisual(TEXT("home_transport_991"),ActualRendered);
    TArray<FSeigeInspectionCandidate> Candidates;G->GatherInspectionCandidates(Candidates);
    auto Find=[&]()->const FSeigeInspectionCandidate*{return Candidates.FindByPredicate([](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Transport&&C.Ref.Id==991;});};
    if(TestNotNull(TEXT("Transport appears in the same inspection system as workers"),Find()))
    {
        TestTrue(TEXT("Picking uses actual interpolated render position, not simulation position"),Find()->Bounds.GetCenter().Equals(ActualRendered));
        TestEqual(TEXT("Transport health comes from the authoritative body"),Find()->Health,V.Health);
    }
    Actor->SetActorHiddenInGame(true);G->GatherInspectionCandidates(Candidates);TestNull(TEXT("Hidden cached actor cannot be inspected"),Find());
    Actor->SetActorHiddenInGame(false);G->Sim.TransportVehicles.Find(991)->Embarked=true;G->GatherInspectionCandidates(Candidates);TestNull(TEXT("Boarding invalidates a visible cached selection immediately"),Find());
    G->Sim.TransportVehicles.Find(991)->Embarked=false;G->Sim.TransportVehicles.Find(991)->Health=0;G->GatherInspectionCandidates(Candidates);TestNull(TEXT("Destroyed body invalidates cached render actor immediately"),Find());
    auto& Building=G->Sim.Buildings[0];Building.IsConstructing=true;Building.ConstructionProgress=.4;
    AddVisual(FString::Printf(TEXT("home_building_%d"),Building.Id),FVector(1000,0,200));
    G->GatherInspectionCandidates(Candidates);
    const auto* Site=Candidates.FindByPredicate([&](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Building&&C.Ref.Id==Building.Id;});
    if(TestNotNull(TEXT("Construction site participates in hover health and progress"),Site))
    {TestEqual(TEXT("Building progress is simulation progress, not arbitrary elapsed time"),Site->ConstructionProgress,.4);TestEqual(TEXT("Building maximum HP follows its definition"),Site->MaximumHealth,G->Sim.Definition(Building)->Health);}
    FSeigeVehicle FleetVehicle;FleetVehicle.Id=992;FleetVehicle.Health=100;FleetVehicle.ChassisId=TEXT("wheeled_small");FleetVehicle.SectorIndex=3;
    G->Sim.Combat.Vehicles.Add(FleetVehicle);AddVisual(TEXT("home_vehicle_992"),FVector(500,500,200));
    auto FindFleet=[&]()->const FSeigeInspectionCandidate*{return Candidates.FindByPredicate([](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Vehicle&&C.Ref.Id==992;});};
    G->GatherInspectionCandidates(Candidates);TestNull(TEXT("Home-owned fleet in another physical sector is not a ghost unit at home"),FindFleet());
    G->Sim.Combat.FindVehicle(992)->SectorIndex=4;G->GatherInspectionCandidates(Candidates);TestNotNull(TEXT("Returning fleet is inspectable in its current sector"),FindFleet());
    G->Sim.TransportVehicles.Find(991)->Health=V.Health;G->FleetOrderActive=true;G->GatherInspectionCandidates(Candidates);TestEqual(TEXT("Fleet mission placement takes precedence over inspection"),Candidates.Num(),0);
    G->FleetOrderActive=false;G->Screen=TEXT("landing");G->GatherInspectionCandidates(Candidates);TestEqual(TEXT("Landing does not reveal units before deployment"),Candidates.Num(),0);
    G->Screen=TEXT("playing");G->MenuOpen=true;G->GatherInspectionCandidates(Candidates);TestEqual(TEXT("Modal menu blocks world inspection"),Candidates.Num(),0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeGroundStockInspectionTest,"Seige.Interaction.GroundStockInspection",InspectionFlags)
bool FSeigeGroundStockInspectionTest::RunTest(const FString&)
{
    FTestWorldWrapper World;if(!World.CreateTestWorld(EWorldType::Game)){World.ForwardErrorMessages(this);return false;}
    auto* G=World.GetTestWorld()->SpawnActor<ASeigeGameMode>();FString Error;
    if(!G||!G->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    G->Ready=true;G->Screen=TEXT("playing");G->Zoom=G->RenderedZoom=G->DefaultZoom;
    auto& B=G->Sim.Buildings[0];B.IsConstructing=false;G->Sim.BuildingDefs[B.DefId].InventoryPresentation=TEXT("outdoor");
    B.Inventory.Add(TEXT("alloy"),72);const double Before=G->Sim.TotalStock(TEXT("alloy"));
    const FString Prefix=TEXT("home_building_")+FString::FromInt(B.Id);
    auto AddVisual=[&](const FString& Key)
    {
        auto* A=World.GetTestWorld()->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(A);A->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(90,70,30));Box->SetHiddenInGame(false);Box->RegisterComponent();A->AddInstanceComponent(Box);A->SetActorLocation(FVector(1000,500,40));G->Visuals.Add(Key,A);return A;
    };
    auto* Pile=AddVisual(Prefix+TEXT("_inventory_alloy"));TArray<FSeigeInspectionCandidate> Found;
    auto Stock=[&](){G->GatherInspectionCandidates(Found);return Found.FindByPredicate([](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Stockpile;});};
    auto* C=Stock();if(TestNotNull(TEXT("Visible ground inventory is a pickable candidate"),C))
    {TestEqual(TEXT("Quantity resolves actual inventory"),C->StockAmount,72.);TestEqual(TEXT("Resource id is stable"),C->Ref.ResourceId,FString(TEXT("alloy")));TestTrue(TEXT("Picking uses pile bounds"),C->Bounds.GetCenter().Equals(Pile->GetActorLocation()));}
    TestEqual(TEXT("Inspection cannot duplicate stock"),G->Sim.TotalStock(TEXT("alloy")),Before);
    B.Inventory[TEXT("alloy")]=0;TestNull(TEXT("Consumed pile is immediately unpickable even before cached visual cleanup"),Stock());
    B.Inventory[TEXT("alloy")]=12;Pile->SetActorHiddenInGame(true);TestNull(TEXT("Hidden pile is not a selectable proxy"),Stock());Pile->SetActorHiddenInGame(false);
    G->Sim.BuildingDefs[B.DefId].InventoryPresentation=TEXT("indoor");TestNull(TEXT("Indoor stock is not exposed by stale outdoor visuals"),Stock());
    B.IsConstructing=true;B.ConstructionMaterials.Add(TEXT("alloy"),8);AddVisual(Prefix+TEXT("_stock_alloy"));
    C=Stock();if(TestNotNull(TEXT("Delivered construction stock is inspectable"),C))
    {TestEqual(TEXT("Site stock excludes installed and indoor inventory"),C->StockAmount,8.);TestTrue(TEXT("Construction and inventory have distinct selection identities"),C->Ref.ConstructionStock);}
    B.Health=0;TestNull(TEXT("Destroyed owner's stale pile is not selectable"),Stock());
    return true;
}
#endif
