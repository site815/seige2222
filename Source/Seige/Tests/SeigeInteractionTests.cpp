#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#if WITH_EDITOR
#include "StaticMeshCompiler.h"
#endif
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags InteractionFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

struct FInteractionWorld : FTestWorldWrapper
{
    ASeigeGameMode* Game = nullptr;
    ASeigeController* Controller = nullptr;
    ASeigeHUD* Hud = nullptr;

    bool Prepare(FAutomationTestBase& Test)
    {
        if (!CreateTestWorld(EWorldType::Game))
        {
            ForwardErrorMessages(&Test);
            return false;
        }
        FURL Url;
        Url.AddOption(*FString::Printf(TEXT("game=%s"), *ASeigeGameMode::StaticClass()->GetPathName()));
        if (!Test.TestTrue(TEXT("Test world creates the real game mode"), GetTestWorld()->SetGameMode(Url))) return false;
        Game = Cast<ASeigeGameMode>(GetTestWorld()->GetAuthGameMode());
        if (!Test.TestNotNull(TEXT("Authoritative colony game mode exists"), Game)) return false;
        Game->Screen = TEXT("playing");
        Game->Ready = Game->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(), TEXT("Rules")), Game->Error);
        if (!Test.TestTrue(TEXT("Colony rules initialize for interaction test"), Game->Ready))
        {
            Test.AddError(Game->Error);
            return false;
        }
        Game->Sim.Tick(Game->Sim.BuildingDefs[Game->Sim.CoreDefinition].ConstructionSeconds+Game->Sim.FixedStepSeconds());
        Controller = GetTestWorld()->SpawnActor<ASeigeController>();
        if (!Test.TestNotNull(TEXT("Real player controller exists"), Controller)) return false;
        // No LocalPlayer/net connection exists in this headless fixture. Invoke the
        // engine's normal client implementation directly to create its owned HUD.
        Controller->ClientSetHUD_Implementation(ASeigeHUD::StaticClass());
        Hud = Cast<ASeigeHUD>(Controller->GetHUD());
        if (!Test.TestNotNull(TEXT("Controller owns the real colony HUD"), Hud)) return false;
        // Canvas is only valid during rendering. Deliberately never begin a render pass.
        Hud->SetCanvas(nullptr, nullptr);
        FString Error;
        if (!Test.TestTrue(TEXT("External interface definition loads"), Hud->LoadInterface(FPaths::Combine(FPaths::ProjectDir(), TEXT("Interface")), Error)))
        {
            Test.AddError(Error);
            return false;
        }
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCommandShuttlePickTest,"Seige.Interaction.CommandShuttleGeometrySelection",InteractionFlags)
bool FSeigeCommandShuttlePickTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;if(!World.Prepare(*this))return false;auto& G=*World.Game;
    G.BaseMaterial=UMaterial::GetDefaultMaterial(MD_Surface);G.RebuildTerrainHeights();
    const auto& Core=G.Sim.Buildings[0];const auto& Definition=*G.Sim.Definition(Core);
    if(!TestEqual(TEXT("Fixture uses the level-one parked command shuttle"),Definition.Visual,FString(TEXT("shuttle"))))return false;
    const FString Key=FString::Printf(TEXT("home_building_%d"),Core.Id);
    const double Size=Definition.Footprint*2*G.RenderScale;
    auto* Body=G.Visual(Key,TEXT("Shuttle"),G.RenderPosition(Core.Position),Definition.Color,Size);
    TArray<UStaticMeshComponent*> Hulls;Body->GetComponents(Hulls);
    if(!TestTrue(TEXT("The actual command shuttle mesh is available for geometry picking"),Hulls.Num()==1&&Hulls[0]->GetStaticMesh()&&Hulls[0]->GetStaticMesh()->GetName()==TEXT("SM_Shuttle")))return false;
#if WITH_EDITOR
    FStaticMeshCompilingManager::Get().FinishCompilation({Hulls[0]->GetStaticMesh()});
#endif
    TSet<FString> Live;G.SyncConstructionVisuals(G.Sim,Core,Definition,Core.Position,Key,Live);
    // Runtime has ordinary world frames between creation and a player click.
    // This synchronous fixture must finish deferred query-body creation itself.
    if(auto* Physics=World.GetTestWorld()->GetPhysicsScene())
    {
        Physics->ProcessDeferredCreatePhysicsState();
        Physics->ProcessAsyncPhysicsStateJobs(true);
    }
    TestTrue(TEXT("Command hull participates in selection queries"),Hulls[0]->GetCollisionEnabled()==ECollisionEnabled::QueryOnly&&Hulls[0]->GetCollisionResponseToChannel(ECC_Visibility)==ECR_Block);
    FVector Center,Extent;Body->GetActorBounds(false,Center,Extent);
    const FVector Direction=FRotator(-25,35,0).Vector();
    const FVector Origin=Center-Direction*Size*2;
    TestTrue(TEXT("A low-angle ray hits the real parked hull, without ground-footprint fallback"),G.SelectBuildingRay(Origin,Direction));
    TestEqual(TEXT("The geometry hit selects the command building"),G.SelectedId,Core.Id);
    auto* Decorative=G.Visual(Key+TEXT("_shuttle"),TEXT("Shuttle"),G.RenderPosition(Core.Position)+FVector(Size*3,0,0),Definition.Color,Size*.2);
    TArray<UStaticMeshComponent*> DecorativeMeshes;Decorative->GetComponents(DecorativeMeshes);
    for(auto* Mesh:DecorativeMeshes)TestTrue(TEXT("Decorative docked shuttles do not intercept selection rays"),Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!Mesh->ComponentHasTag(TEXT("BuildingBody")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeControllerClickTest, "Seige.Interaction.ControllerClickBetweenDraws", InteractionFlags)
bool FSeigeControllerClickTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;
    if (!World.Prepare(*this)) return false;
    auto& Game = *World.Game;
    const int32 CoreId = Game.Sim.Buildings[0].Id;
    Game.CursorOnWorld = true;
    Game.CursorWorld = Game.Sim.Buildings[0].Position;

    // Uses the same controller handler as PlayerTick, not a simulation-only shortcut.
    // The original release crashed here by reading Canvas->SizeY between DrawHUD calls.
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("Click between render passes reaches world selection without a canvas"), Game.SelectedId, CoreId);

    const int32 Before = Game.Sim.Buildings.Num();
    Game.SelectedBuild = TEXT("sensor");
    Game.CursorWorld = FVector2D(1100, 0);
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("The normal controller click places a selected blueprint"), Game.Sim.Buildings.Num(), Before + 1);
    TestEqual(TEXT("Placement used the selected building definition"), Game.Sim.Buildings.Last().DefId, FString(TEXT("sensor")));

    Game.CursorOnWorld = false;
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("A click without a world intersection does not place a building"), Game.Sim.Buildings.Num(), Before + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeMenuShortcutTest, "Seige.Interaction.BuildMenuShortcuts", InteractionFlags)
bool FSeigeMenuShortcutTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;
    if (!World.Prepare(*this)) return false;
    auto& Hud = *World.Hud;
    auto& Game = *World.Game;
    TestTrue(TEXT("B opens construction before any DrawHUD"), Hud.HandleShortcut(EKeys::B));
    TestTrue(TEXT("Construction catalog is open"), Hud.Ui.BuildOpen);
    TestTrue(TEXT("Open catalog suppresses camera letter keys"), Hud.BlocksCameraKeys());
    TestTrue(TEXT("L selects the external logistics category"), Hud.HandleShortcut(EKeys::L));
    TestEqual(TEXT("Logistics category selected"), Hud.Ui.Category, FString(TEXT("logistics")));
    TestTrue(TEXT("S chooses the sensor within logistics"), Hud.HandleShortcut(EKeys::S));
    TestEqual(TEXT("Keyboard routing selects the sensor blueprint"), Game.SelectedBuild, FString(TEXT("sensor")));
    TestFalse(TEXT("Choosing a blueprint closes the catalog"), Hud.Ui.BuildOpen);
    TestFalse(TEXT("Camera keys become available after catalog closes"), Hud.BlocksCameraKeys());
    TestTrue(TEXT("Escape cancels placement"), Hud.HandleShortcut(EKeys::Escape));
    TestTrue(TEXT("No blueprint remains after cancel"), Game.SelectedBuild.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeSpeedAndMenuShortcutTest, "Seige.Interaction.SpeedAndGameMenuShortcuts", InteractionFlags)
bool FSeigeSpeedAndMenuShortcutTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;if(!World.Prepare(*this))return false;
    auto& Game=*World.Game;auto& Hud=*World.Hud;
    Game.Speed=1;Game.Paused=false;
    Hud.HandleShortcut(EKeys::Add);TestEqual(TEXT("Numpad plus selects 5x after 1x"),Game.Speed,5.f);
    Hud.HandleShortcut(EKeys::Equals);TestEqual(TEXT("Keyboard plus selects 10x after 5x"),Game.Speed,10.f);
    Hud.HandleShortcut(EKeys::Add);TestTrue(TEXT("Plus enters Paused after 10x and retains that running rate"),Game.Paused&&Game.Speed==10);
    Hud.HandleShortcut(EKeys::Equals);TestTrue(TEXT("Plus leaves Paused at 1x"),!Game.Paused&&Game.Speed==1);
    Hud.HandleShortcut(EKeys::Hyphen);TestTrue(TEXT("Minus enters Paused before 1x"),Game.Paused&&Game.Speed==1);
    Hud.HandleShortcut(EKeys::Subtract);TestTrue(TEXT("Minus wraps Paused to 10x"),!Game.Paused&&Game.Speed==10);
    Hud.HandleShortcut(EKeys::Subtract);TestEqual(TEXT("Numpad minus selects 5x after 10x"),Game.Speed,5.f);
    Hud.HandleShortcut(EKeys::SpaceBar);TestTrue(TEXT("Space pauses without resetting speed"),Game.Paused&&Game.Speed==5);
    Hud.HandleShortcut(EKeys::SpaceBar);TestFalse(TEXT("Space resumes the same speed"),Game.Paused);
    Game.Speed=10;Hud.HandleShortcut(EKeys::Add);Hud.HandleShortcut(EKeys::SpaceBar);
    TestTrue(TEXT("Space resumes 10x after the speed cycle entered pause"),!Game.Paused&&Game.Speed==10);
    Game.Speed=5;
    Game.SelectedBuild=TEXT("sensor");Hud.HandleShortcut(EKeys::Escape);
    TestTrue(TEXT("Escape first cancels a blueprint without opening the game menu"),Game.SelectedBuild.IsEmpty()&&!Game.MenuOpen&&Game.Screen==TEXT("playing"));
    Hud.HandleShortcut(EKeys::Escape);
    TestTrue(TEXT("A subsequent Escape opens the game menu"),Game.MenuOpen&&Game.Screen==TEXT("game-menu")&&Game.Paused);
    Hud.HandleShortcut(EKeys::Add);TestEqual(TEXT("Speed shortcuts do not change a paused menu's saved speed"),Game.Speed,5.f);
    Game.CursorOnWorld=true;Game.CursorWorld=FVector2D(1100,0);const int32 Buildings=Game.Sim.Buildings.Num();
    Hud.Ui.Scale=1;Hud.Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),TEXT("build:sensor"),TEXT("")}};
    World.Controller->HandlePrimaryClick(150,125);
    TestTrue(TEXT("A stale blueprint button cannot build through the game menu"),Game.SelectedBuild.IsEmpty()&&Game.Sim.Buildings.Num()==Buildings);
    Hud.HandleShortcut(EKeys::F10);Hud.HandleShortcut(EKeys::B);TestTrue(TEXT("Build catalog opens after resuming"),Hud.Ui.BuildOpen);
    Hud.HandleShortcut(EKeys::F10);
    TestTrue(TEXT("F10 opens the game menu directly even from the construction catalog"),Game.MenuOpen&&!Hud.Ui.BuildOpen&&Game.Paused);
    Hud.HandleShortcut(EKeys::Escape);TestTrue(TEXT("Escape resumes the original running state"),!Game.MenuOpen&&!Game.Paused&&Game.Speed==5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeUiClickIsolationTest, "Seige.Interaction.UiClickIsolation", InteractionFlags)
bool FSeigeUiClickIsolationTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;
    if (!World.Prepare(*this)) return false;
    auto& Hud = *World.Hud;
    auto& Game = *World.Game;
    Game.CursorOnWorld = true;
    Game.CursorWorld = FVector2D(1100, 0);
    Game.SelectedBuild = TEXT("sensor");
    const int32 Before = Game.Sim.Buildings.Num();

    Hud.HandleShortcut(EKeys::B);
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("Catalog consumes world clicks instead of placing behind it"), Game.Sim.Buildings.Num(), Before);
    Hud.Ui.CloseMenus();
    Game.SelectedBuild = TEXT("sensor");

    // Hit regions store logical coordinates; input coordinates are physical pixels.
    Hud.Ui.Scale = 2;
    Hud.Ui.HitRegions = {{FVector2D(100, 100), FVector2D(80, 30), TEXT("pause"), TEXT("")}};
    World.Controller->HandlePrimaryClick(250, 225);
    TestTrue(TEXT("Cached scaled hit region executes its action without Canvas"), Game.Paused);
    TestEqual(TEXT("Button click never falls through to construction"), Game.Sim.Buildings.Num(), Before);
    Hud.Ui.Scale = 1;
    Hud.Ui.HitRegions.Reset();

    Game.Sim.Won = true;
    Game.WinAcknowledged = false;
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("Victory modal blocks world input before its next draw"), Game.Sim.Buildings.Num(), Before);

    // The simulation can end after the previous frame cached ordinary menu hit regions.
    Hud.Ui.HitRegions = {{FVector2D(800, 400), FVector2D(200, 200), TEXT("build:turret"), TEXT("")}};
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("Victory rejects stale underlying build actions"), Game.SelectedBuild, FString(TEXT("sensor")));
    TestEqual(TEXT("Victory never places a building through a stale menu"), Game.Sim.Buildings.Num(), Before);

    Game.Sim.Won = false;
    Game.Sim.Failed = true;
    Hud.Ui.HitRegions.Reset();
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("Failure modal consumes clicks"), Game.Sim.Buildings.Num(), Before);
    Game.Sim.Failed = false;
    Game.Ready = false;
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("Invalid rules cannot trigger world construction"), Game.Sim.Buildings.Num(), Before);
    Game.Ready = true;
    Game.Screen = TEXT("main");
    World.Controller->HandlePrimaryClick(900, 500);
    TestEqual(TEXT("Main menu consumes clicks instead of constructing behind it"), Game.Sim.Buildings.Num(), Before);
    Game.Screen = TEXT("playing");
    Game.Observer = true;
    Game.SelectedBuild.Empty();
    TestTrue(TEXT("Observer consumes construction shortcut"), Hud.HandleShortcut(EKeys::B));
    TestFalse(TEXT("Observer never opens player construction catalog"), Hud.Ui.BuildOpen);
    Hud.Ui.HitRegions = {{FVector2D(800, 400), FVector2D(200, 200), TEXT("build:turret"), TEXT("")}};
    World.Controller->HandlePrimaryClick(900, 500);
    TestTrue(TEXT("Observer rejects stale player construction actions"), Game.SelectedBuild.IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeSelectedCoreCommandsTest, "Seige.Interaction.SelectedCoreCommands", InteractionFlags)
bool FSeigeSelectedCoreCommandsTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;if(!World.Prepare(*this))return false;
    auto& G=*World.Game;auto& Hud=*World.Hud;
    const auto* Core=G.Sim.Buildings.FindByPredicate([&](const FSeigeBuilding& B){return B.DefId==G.Sim.CoreDefinition;});
    if(!TestNotNull(TEXT("Core command fixture has its command center"),Core))return false;const int32 CoreId=Core->Id;
    auto Click=[&](const TCHAR* Action)
    {
        Hud.Ui.Scale=1;Hud.Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),Action,TEXT("")}};
        World.Controller->HandlePrimaryClick(150,125);Hud.Ui.HitRegions.Reset();
    };
    auto Reject=[&](const TCHAR* Reason){Click(TEXT("escape"));TestFalse(Reason,G.Sim.Escaped);};
    G.SelectedId=0;Reject(TEXT("An old general escape button cannot launch without selection"));
    Click(TEXT("colony-menu"));TestTrue(TEXT("Removed Colony menu action cannot open a game menu or issue a command"),!G.MenuOpen&&!G.Sim.Escaped);
    G.SelectedId=MAX_int32;Reject(TEXT("A stale or missing building selection cannot launch"));
    if(!G.Sim.PlaceBuilding(TEXT("sensor"),FVector2D(1100,0),G.Error)){AddError(G.Error);return false;}
    G.SelectedId=G.Sim.Buildings.Last().Id;Reject(TEXT("Selecting another own building cannot expose core commands"));
    G.SelectedId=CoreId;G.Observer=true;Reject(TEXT("Observer selection cannot command the center"));G.Observer=false;
    G.CameraCenter=FVector(G.Sim.WorldHalfSize*2,0,0);Reject(TEXT("A neighbor-local ID cannot command the home core"));G.CameraCenter=FVector::ZeroVector;
    G.Zoom=G.MaximumZoom;Reject(TEXT("Regional-map selection cannot launch the shuttle"));G.Zoom=G.DefaultZoom;
    G.SelectedBuild=TEXT("sensor");Reject(TEXT("An active blueprint cannot reuse a stale core command"));G.SelectedBuild.Empty();
    G.BeginRoadPlacement();G.SelectedId=CoreId;Reject(TEXT("An active road tool cannot reuse a stale core command"));G.CancelRoadTool();
    G.SelectedId=CoreId;const double CoreHealth=G.Sim.FindBuilding(CoreId)->Health;G.Sim.FindBuilding(CoreId)->Health=0;
    Reject(TEXT("A destroyed selected core cannot issue a manual command"));G.Sim.FindBuilding(CoreId)->Health=CoreHealth;
    G.ToggleGameMenu();G.SelectedId=CoreId;Reject(TEXT("A paused game-menu surface consumes stale escape commands"));G.ResumeGameMenu();
    G.SelectedId=CoreId;Click(TEXT("escape"));
    TestTrue(TEXT("The selected live home command center can launch its shuttle"),G.Sim.Escaped);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRoadShortcutTest, "Seige.Interaction.RoadToolsAndIsolation", InteractionFlags)
bool FSeigeRoadShortcutTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;if(!World.Prepare(*this))return false;
    auto& G=*World.Game;auto& Hud=*World.Hud;
    Hud.HandleShortcut(EKeys::B);Hud.HandleShortcut(EKeys::L);Hud.HandleShortcut(EKeys::R);
    TestTrue(TEXT("B L R opens road construction through the external catalog"),G.IsRoadToolActive()&&!Hud.Ui.BuildOpen&&G.SelectedBuild.IsEmpty());
    Hud.HandleShortcut(EKeys::Escape);
    TestTrue(TEXT("Escape cancels the road tool before opening a menu"),!G.IsRoadToolActive()&&!G.MenuOpen);
    Hud.HandleShortcut(EKeys::B);Hud.HandleShortcut(EKeys::L);Hud.HandleShortcut(EKeys::U);
    TestTrue(TEXT("B L U opens existing-road upgrade selection"),G.IsRoadToolActive()&&!Hud.Ui.BuildOpen);
    Hud.HandleShortcut(EKeys::RightMouseButton);TestFalse(TEXT("Right click cancels upgrade targeting"),G.IsRoadToolActive());
    G.SelectedRoadId=123;Hud.HandleShortcut(EKeys::Escape);
    TestTrue(TEXT("Escape clears road selection before the game menu"),G.SelectedRoadId==0&&!G.MenuOpen);
    auto ClickRoad=[&]()
    {
        Hud.Ui.Scale=1;Hud.Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),TEXT("build:road"),TEXT("")}};
        World.Controller->HandlePrimaryClick(150,125);Hud.Ui.HitRegions.Reset();
    };
    G.Observer=true;ClickRoad();TestFalse(TEXT("Observer cannot activate a stale road construction button"),G.IsRoadToolActive());G.Observer=false;
    G.CameraCenter=FVector(G.Sim.WorldHalfSize*2,0,0);ClickRoad();TestFalse(TEXT("Neighbor viewing cannot activate home road construction"),G.IsRoadToolActive());G.CameraCenter=FVector::ZeroVector;
    G.BeginRoadPlacement();Hud.HandleShortcut(EKeys::F10);
    TestTrue(TEXT("Opening the game menu cancels active road targeting"),G.MenuOpen&&!G.IsRoadToolActive());
    ClickRoad();TestFalse(TEXT("The game menu consumes stale road catalog clicks"),G.IsRoadToolActive());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFloatingDockTest, "Seige.Interaction.FloatingDockAndRegionIsolation", InteractionFlags)
bool FSeigeFloatingDockTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;if(!World.Prepare(*this))return false;
    auto& G=*World.Game;auto& Hud=*World.Hud;G.CursorOnWorld=true;G.CursorWorld=FVector2D(1100,0);G.SelectedBuild=TEXT("sensor");
    const int32 Buildings=G.Sim.Buildings.Num();
    Hud.Ui.Scale=1.5f;Hud.Ui.HitRegions={{FVector2D(453,817),FVector2D(132,58),TEXT("build-menu"),TEXT("")}};
    World.Controller->HandlePrimaryClick(750,1260);
    TestTrue(TEXT("Floating bottom dock opens construction between drawing passes"),Hud.Ui.BuildOpen);
    TestEqual(TEXT("Bottom dock never places through its UI rectangle"),G.Sim.Buildings.Num(),Buildings);
    Hud.HandleShortcut(EKeys::L);Hud.HandleShortcut(EKeys::S);
    TestEqual(TEXT("Bottom dock retains B/category/blueprint keyboard flow"),G.SelectedBuild,FString(TEXT("sensor")));
    G.Zoom=G.Sim.WorldHalfSize*12;Hud.Ui.Scale=1;
    Hud.Ui.HitRegions={{FVector2D(500,500),FVector2D(200,100),TEXT("build:turret"),TEXT("")}};
    World.Controller->HandlePrimaryClick(550,550);
    TestEqual(TEXT("A stale blueprint click cannot issue construction in regional view"),G.SelectedBuild,FString(TEXT("sensor")));
    TestTrue(TEXT("Regional view consumes B without opening construction"),Hud.HandleShortcut(EKeys::B));
    TestFalse(TEXT("Regional map keeps local construction catalog closed"),Hud.Ui.BuildOpen);
    TestEqual(TEXT("Regional UI input does not mutate the colony"),G.Sim.Buildings.Num(),Buildings);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerCommandUiTest,"Seige.Interaction.WorkerProductionAndReserveControls",InteractionFlags)
bool FSeigeWorkerCommandUiTest::RunTest(const FString& Parameters)
{
    FInteractionWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;auto& H=*W.Hud;
    const int32 CoreId=G.Sim.Buildings[0].Id;G.SelectedId=CoreId;
    auto Click=[&](const TCHAR* Action){H.Ui.Scale=1;H.Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),Action,TEXT("")}};W.Controller->HandlePrimaryClick(150,125);H.Ui.HitRegions.Reset();};
    const auto Options=G.Sim.ProductionOptions(CoreId);if(!TestTrue(TEXT("Core exposes selectable manufacturing outputs"),Options.Num()>1))return false;
    const FString Before=G.Sim.FindBuilding(CoreId)->SelectedRecipe;const double Stock=G.Sim.TotalStock(TEXT("components"));const int32 Population=G.Sim.Population;
    Click(TEXT("command:recipe-next"));TestNotEqual(TEXT("Production button changes selected output without Canvas"),G.Sim.FindBuilding(CoreId)->SelectedRecipe,Before);
    TestEqual(TEXT("Selecting a recipe cannot award or consume physical parts"),G.Sim.TotalStock(TEXT("components")),Stock);
    Click(TEXT("command:reserve-more"));TestEqual(TEXT("Worker HUD increases whole inactive reserve target"),G.Sim.WorkerSurplusTarget,1);
    Click(TEXT("command:reserve-less"));Click(TEXT("command:reserve-less"));TestEqual(TEXT("Reserve decrement is bounded at zero"),G.Sim.WorkerSurplusTarget,0);
    Click(TEXT("command:disassemble"));TestEqual(TEXT("No stored surplus means no active workers are deleted"),G.Sim.Population,Population);
    const FString Selected=G.Sim.FindBuilding(CoreId)->SelectedRecipe;G.Observer=true;Click(TEXT("command:recipe-next"));Click(TEXT("command:reserve-more"));
    TestEqual(TEXT("Observer cannot change selected production"),G.Sim.FindBuilding(CoreId)->SelectedRecipe,Selected);TestEqual(TEXT("Observer cannot change reserve"),G.Sim.WorkerSurplusTarget,0);G.Observer=false;
    G.CameraCenter=FVector(G.Sim.WorldHalfSize*2,0,0);Click(TEXT("command:reserve-more"));TestEqual(TEXT("Neighbor view cannot issue home workforce commands"),G.Sim.WorkerSurplusTarget,0);G.CameraCenter=FVector::ZeroVector;
    G.ToggleGameMenu();Click(TEXT("command:reserve-more"));TestEqual(TEXT("Paused game menu consumes stale worker actions"),G.Sim.WorkerSurplusTarget,0);G.ResumeGameMenu();
    G.Sim.Won=true;G.WinAcknowledged=false;Click(TEXT("command:reserve-more"));TestEqual(TEXT("Outcome modal consumes stale worker actions"),G.Sim.WorkerSurplusTarget,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePortWorkerUiTest,"Seige.Interaction.TradingPortWorkerTargets",InteractionFlags)
bool FSeigePortWorkerUiTest::RunTest(const FString& Parameters)
{
    FInteractionWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;auto& H=*W.Hud;
    if(!G.Sim.PlaceBuilding(TEXT("trading_port"),FVector2D(1300,-700),G.Error)){AddError(G.Error);return false;}
    const int32 PortId=G.Sim.Buildings.Last().Id;G.SelectedId=PortId;
    auto Click=[&](const TCHAR* Action){H.Ui.Scale=1;H.Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),Action,TEXT("")}};W.Controller->HandlePrimaryClick(150,125);H.Ui.HitRegions.Reset();};
    Click(TEXT("trade:reserve-more"));TestEqual(TEXT("Selected own port has a separate worker target"),G.Sim.FindBuilding(PortId)->WorkerExportTarget,1);
    TestEqual(TEXT("Port target does not overwrite colony target"),G.Sim.WorkerSurplusTarget,0);
    G.SelectedId=G.Sim.Buildings[0].Id;Click(TEXT("trade:reserve-more"));TestEqual(TEXT("A stale port action cannot modify a nonselected port"),G.Sim.FindBuilding(PortId)->WorkerExportTarget,1);
    G.SelectedId=PortId;G.Observer=true;Click(TEXT("trade:reserve-more"));TestEqual(TEXT("Observer cannot change a port worker target"),G.Sim.FindBuilding(PortId)->WorkerExportTarget,1);G.Observer=false;
    G.SelectedBuild=TEXT("sensor");Click(TEXT("trade:reserve-more"));TestEqual(TEXT("Placement consumes stale trade actions"),G.Sim.FindBuilding(PortId)->WorkerExportTarget,1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWallToolUiTest,"Seige.Interaction.WallPlanInputsWithoutCanvas",InteractionFlags)
bool FSeigeWallToolUiTest::RunTest(const FString& Parameters)
{
    FInteractionWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;auto& H=*W.Hud;
    TestFalse(TEXT("Wall tool is not a nonexistent ordinary blueprint"),G.Sim.BuildingDefs.Contains(TEXT("wall")));
    H.HandleShortcut(EKeys::B);H.HandleShortcut(EKeys::L);H.HandleShortcut(EKeys::W);
    TestTrue(TEXT("B L W resolves custom wall entry with no building pointer or Canvas"),G.WallPlacementActive&&!H.Ui.BuildOpen&&G.SelectedBuild.IsEmpty());
    const int32 Before=G.Sim.Buildings.Num();G.CursorOnWorld=true;G.CursorWorld=FVector2D(1300,-700);W.Controller->HandlePrimaryClick(900,500);
    G.CursorWorld=FVector2D(1700,-700);W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("World clicks add two physical planning joints"),G.WallJoints.Num(),2);TestEqual(TEXT("A wall preview does not spend materials or spawn buildings"),G.Sim.Buildings.Num(),Before);
    const bool Inside=G.WallInsideLeft;H.HandleShortcut(EKeys::E);TestNotEqual(TEXT("E reverses the planned inside"),G.WallInsideLeft,Inside);
    H.HandleShortcut(EKeys::BackSpace);TestEqual(TEXT("Backspace undoes the latest joint"),G.WallJoints.Num(),1);
    G.SelectedWallJoint=0;H.HandleShortcut(EKeys::Delete);TestEqual(TEXT("Delete removes the selected planned joint"),G.WallJoints.Num(),0);
    H.HandleShortcut(EKeys::Enter);TestTrue(TEXT("An invalid empty plan stays editable and does not construct"),G.WallPlacementActive&&G.Sim.Buildings.Num()==Before);
    H.HandleShortcut(EKeys::Escape);TestTrue(TEXT("Escape cancels wall planning before opening menu"),!G.WallPlacementActive&&!G.MenuOpen);
    G.Observer=true;H.HandleShortcut(EKeys::B);TestFalse(TEXT("Observer cannot open construction or wall planning"),H.Ui.BuildOpen||G.WallPlacementActive);G.Observer=false;
    G.BeginWallPlacement();H.HandleShortcut(EKeys::F10);TestTrue(TEXT("F10 pauses and clears a wall preview"),G.MenuOpen&&!G.WallPlacementActive);
    G.ResumeGameMenu();G.BeginWallPlacement();
    const FVector2D Core=G.Sim.Buildings[0].Position;
    G.WallJoints={Core+FVector2D(-300,-1000),Core+FVector2D(300,-1000)};
    FSeigeWallPlan Plan;FString Error;
    if(!G.Sim.Walls.Plan(G.Sim,G.WallJoints,G.WallInsideLeft,Plan,Error)){AddError(Error);return false;}
    const int32 Sections=G.Sim.Walls.Segments.Num();
    const double Available=G.Sim.ConstructionAvailable(TEXT("alloy"));
    G.Sim.Won=true;G.WinAcknowledged=false;H.HandleShortcut(EKeys::Enter);
    TestEqual(TEXT("Victory modal blocks committing a valid wall plan"),G.Sim.Buildings.Num(),Before);
    TestEqual(TEXT("Victory modal creates no wall sections"),G.Sim.Walls.Segments.Num(),Sections);
    TestEqual(TEXT("Victory modal reserves no construction materials"),G.Sim.ConstructionAvailable(TEXT("alloy")),Available);
    TestTrue(TEXT("Blocked wall plan remains available after acknowledging victory"),G.WallPlacementActive&&G.WallJoints.Num()==2);
    G.WinAcknowledged=true;H.HandleShortcut(EKeys::Enter);
    TestEqual(TEXT("Acknowledged victory permits the same valid paid wall plan"),G.Sim.Walls.Segments.Num(),Sections+Plan.Segments.Num());
    TestTrue(TEXT("Committed plan reserves its actual material bill"),FMath::IsNearlyEqual(G.Sim.ConstructionAvailable(TEXT("alloy")),Available-Plan.Cost.FindRef(TEXT("alloy")),1.e-8));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePreparationUiTest,"Seige.Interaction.PreparationCancelAndInputIsolation",InteractionFlags)
bool FSeigePreparationUiTest::RunTest(const FString& Parameters)
{
    FInteractionWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;auto& H=*W.Hud;
    G.Screen=TEXT("scenario");G.ScenarioSlots.Init(TEXT("empty"),9);G.ScenarioSlots[4]=TEXT("player");G.ScenarioSlots[0]=TEXT("developed");
    const double Time=G.Sim.Time,Alloy=G.Sim.TotalStock(TEXT("alloy"));const int32 Buildings=G.Sim.Buildings.Num();const bool Paused=G.Paused;
    G.BeginScenarioPreparation();
    if(!TestTrue(TEXT("Preparation enters its input-isolated screen"),G.IsPreparingScenario()&&G.Screen==TEXT("preparing")))return false;
    for(const auto& Key:{EKeys::B,EKeys::F5,EKeys::F9,EKeys::F10,EKeys::SpaceBar,EKeys::Enter})TestTrue(TEXT("Preparation consumes gameplay and menu shortcuts"),H.HandleShortcut(Key));
    auto Click=[&](const TCHAR* Action){H.Ui.Scale=1;H.Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),Action,TEXT("")}};W.Controller->HandlePrimaryClick(150,125);H.Ui.HitRegions.Reset();};
    Click(TEXT("main-menu"));Click(TEXT("start-scenario"));Click(TEXT("command:reserve-more"));Click(TEXT("slot:0"));
    G.CursorOnWorld=true;G.CursorWorld=G.Sim.Buildings[0].Position;G.SelectedBuild=TEXT("sensor");
    W.Controller->HandlePrimaryClick(900,500);
    TestTrue(TEXT("Stale actions and world clicks keep preparation active"),G.IsPreparingScenario()&&G.Screen==TEXT("preparing")&&!H.Ui.BuildOpen&&!G.MenuOpen);
    TestEqual(TEXT("Preparation controls cannot alter pause state"),G.Paused,Paused);
    TestEqual(TEXT("Preparation controls cannot change colony reserves"),G.Sim.WorkerSurplusTarget,0);
    TestEqual(TEXT("Preparation controls preserve captured region choice"),G.ScenarioSlots[0],FString(TEXT("developed")));
    Click(TEXT("cancel-preparation"));
    TestTrue(TEXT("Cancel button works before a render pass"),!G.IsPreparingScenario()&&G.Screen==TEXT("scenario"));
    G.SelectedBuild.Empty();G.BeginScenarioPreparation();TestTrue(TEXT("Escape is handled during preparation"),H.HandleShortcut(EKeys::Escape));
    TestTrue(TEXT("Escape cancels and returns to scenario setup"),!G.IsPreparingScenario()&&G.Screen==TEXT("scenario"));
    TestEqual(TEXT("Cancelled preparation leaves active simulation time unchanged"),G.Sim.Time,Time);
    TestEqual(TEXT("Cancelled preparation creates no colony buildings"),G.Sim.Buildings.Num(),Buildings);
    TestEqual(TEXT("Cancelled preparation consumes no active colony stock"),G.Sim.TotalStock(TEXT("alloy")),Alloy);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatUiTest,"Seige.Interaction.CombatPlansPaymentsAndOwnership",InteractionFlags)
bool FSeigeCombatUiTest::RunTest(const FString& Parameters)
{
    FInteractionWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;auto& H=*W.Hud;auto& C=G.Sim.Combat;
    const int32 CoreId=G.Sim.Buildings[0].Id;G.SelectedId=CoreId;
    auto Click=[&](const TCHAR* Action){H.Ui.Scale=1;H.Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),Action,TEXT("")}};W.Controller->HandlePrimaryClick(150,125);H.Ui.HitRegions.Reset();};
    Click(TEXT("combat:request"));TestFalse(TEXT("A stale hidden combat action cannot install a supply plan"),C.FabricationPlans.Contains(CoreId));
    Click(TEXT("combat:open"));Click(TEXT("combat:tab:factory"));Click(TEXT("combat:hull-next"));Click(TEXT("combat:weapon-next"));Click(TEXT("combat:add"));
    const double Alloy=G.Sim.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),Energy=C.EnergySpentKWh;const int32 Jobs=C.Fabrication.Num();
    Click(TEXT("combat:request"));const auto* Plan=C.FabricationPlans.Find(CoreId);
    if(!TestNotNull(TEXT("Core can request a physical chassis and weapon bill through its UI"),Plan))return false;
    TestEqual(TEXT("The draft contains the requested module count"),Plan->Weapons.Num(),1);
    TestEqual(TEXT("Requesting parts cannot start assembly"),C.Fabrication.Num(),Jobs);
    TestEqual(TEXT("Requesting parts cannot spend or award alloy"),G.Sim.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),Alloy);
    TestEqual(TEXT("Requesting parts cannot spend grid energy"),C.EnergySpentKWh,Energy);
    Click(TEXT("combat:queue"));TestEqual(TEXT("An unaffordable medium hull does not bypass physical delivery"),C.Fabrication.Num(),Jobs);
    TestEqual(TEXT("Rejected assembly preserves actual stock"),G.Sim.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),Alloy);
    Click(TEXT("combat:hull-prev"));Click(TEXT("combat:weapon-next"));Click(TEXT("combat:add"));Click(TEXT("combat:request"));
    Plan=C.FabricationPlans.Find(CoreId);if(!TestNotNull(TEXT("Affordable small hull replaces the draft plan"),Plan))return false;
    const auto Hull=C.Chassis[Plan->ChassisId];const auto Weapon=C.Weapons[Plan->Weapons[0]];
    Click(TEXT("combat:queue"));if(!TestEqual(TEXT("Local paid stock and energy start a real timed job"),C.Fabrication.Num(),Jobs+1)){AddError(G.Notice);return false;}
    TestEqual(TEXT("Assembly debits the exact hull and weapon bill"),G.Sim.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),Alloy-Hull.Cost.FindRef(TEXT("alloy"))-Weapon.Cost.FindRef(TEXT("alloy")));
    TestEqual(TEXT("Assembly debits the configured transaction energy"),C.EnergySpentKWh,Energy+Hull.BuildKWh);
    TestFalse(TEXT("A committed paid job consumes its material-request plan"),C.FabricationPlans.Contains(CoreId));
    Click(TEXT("combat:tab:outfit"));Click(TEXT("combat:outfit-platform"));Click(TEXT("combat:refit-request"));
    TestTrue(TEXT("A selected home platform can request delivered refit parts"),C.RefitPlans.Contains(CoreId));
    const auto OriginalWeapons=C.BuildingState[CoreId].Weapons;const double BeforeRefit=G.Sim.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy"));
    G.CameraCenter=FVector(G.Sim.WorldHalfSize*2,0,0);Click(TEXT("combat:request"));Click(TEXT("combat:queue"));Click(TEXT("combat:refit"));
    TestEqual(TEXT("Viewing a neighbor cannot queue a home factory through an aliased ID"),C.Fabrication.Num(),Jobs+1);
    TestTrue(TEXT("Viewing a neighbor cannot refit a home platform"),C.BuildingState[CoreId].Weapons==OriginalWeapons);
    if(C.Fleets.Num()){G.SelectedFleetId=C.Fleets[0].Id;Click(TEXT("combat:defense"));TestTrue(TEXT("Own fleet can return to home defense while viewing a neighbor"),C.Fleets[0].Mission==TEXT("defense")&&C.Fleets[0].DestinationSector==4);}
    G.CameraCenter=FVector::ZeroVector;G.Observer=true;Click(TEXT("combat:refit"));TestTrue(TEXT("Observer cannot install a cached home outfit"),C.BuildingState[CoreId].Weapons==OriginalWeapons);G.Observer=false;
    Click(TEXT("combat:refit"));TestTrue(TEXT("Home outfit installation changes the actual platform modules"),C.BuildingState[CoreId].Weapons.Num()==1&&C.BuildingState[CoreId].Weapons[0]==Weapon.Id);
    TestEqual(TEXT("Installation consumes actual delivered weapon materials"),G.Sim.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),BeforeRefit-Weapon.Cost.FindRef(TEXT("alloy")));
    Click(TEXT("combat:close"));Click(TEXT("combat:queue"));TestEqual(TEXT("Closing the combat panel invalidates its cached assembly action"),C.Fabrication.Num(),Jobs+1);
    return true;
}
#endif
