#include "SeigeGameMode.h"
#include "Engine/World.h"
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
    Game.CursorWorld = FVector2D(700, 0);
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
    Hud.HandleShortcut(EKeys::Add);TestEqual(TEXT("Numpad plus selects 5x after 1x"),Game.Speed,5.f);
    Hud.HandleShortcut(EKeys::Equals);TestEqual(TEXT("Keyboard plus selects 10x after 5x"),Game.Speed,10.f);
    Hud.HandleShortcut(EKeys::Add);TestEqual(TEXT("Plus wraps 10x to 1x"),Game.Speed,1.f);
    Hud.HandleShortcut(EKeys::Hyphen);TestEqual(TEXT("Keyboard minus wraps 1x to 10x"),Game.Speed,10.f);
    Hud.HandleShortcut(EKeys::Subtract);TestEqual(TEXT("Numpad minus selects 5x after 10x"),Game.Speed,5.f);
    Hud.HandleShortcut(EKeys::SpaceBar);TestTrue(TEXT("Space pauses without resetting speed"),Game.Paused&&Game.Speed==5);
    Hud.HandleShortcut(EKeys::SpaceBar);TestFalse(TEXT("Space resumes the same speed"),Game.Paused);
    Game.SelectedBuild=TEXT("sensor");Hud.HandleShortcut(EKeys::Escape);
    TestTrue(TEXT("Escape first cancels a blueprint without opening the game menu"),Game.SelectedBuild.IsEmpty()&&!Game.MenuOpen&&Game.Screen==TEXT("playing"));
    Hud.HandleShortcut(EKeys::Escape);
    TestTrue(TEXT("A subsequent Escape opens the game menu"),Game.MenuOpen&&Game.Screen==TEXT("game-menu")&&Game.Paused);
    Hud.HandleShortcut(EKeys::Add);TestEqual(TEXT("Speed shortcuts do not change a paused menu's saved speed"),Game.Speed,5.f);
    Game.CursorOnWorld=true;Game.CursorWorld=FVector2D(700,0);const int32 Buildings=Game.Sim.Buildings.Num();
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
    Game.CursorWorld = FVector2D(700, 0);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFloatingDockTest, "Seige.Interaction.FloatingDockAndRegionIsolation", InteractionFlags)
bool FSeigeFloatingDockTest::RunTest(const FString& Parameters)
{
    FInteractionWorld World;if(!World.Prepare(*this))return false;
    auto& G=*World.Game;auto& Hud=*World.Hud;G.CursorOnWorld=true;G.CursorWorld=FVector2D(700,0);G.SelectedBuild=TEXT("sensor");
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
#endif
