#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags FrontendFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
struct FFrontendWorld : FTestWorldWrapper
{
    ASeigeGameMode* Game=nullptr;
    ASeigeController* Controller=nullptr;
    ASeigeHUD* Hud=nullptr;
    bool Prepare(FAutomationTestBase& Test)
    {
        if(!CreateTestWorld(EWorldType::Game)){ForwardErrorMessages(&Test);return false;}
        FURL Url;Url.AddOption(*FString::Printf(TEXT("game=%s"),*ASeigeGameMode::StaticClass()->GetPathName()));
        if(!GetTestWorld()->SetGameMode(Url)){Test.AddError(TEXT("Could not create frontend test game mode"));return false;}
        Game=Cast<ASeigeGameMode>(GetTestWorld()->GetAuthGameMode());
        Controller=GetTestWorld()->SpawnActor<ASeigeController>();
        if(!Game||!Controller){Test.AddError(TEXT("Frontend fixture actors were not created"));return false;}
        Controller->ClientSetHUD_Implementation(ASeigeHUD::StaticClass());Hud=Cast<ASeigeHUD>(Controller->GetHUD());
        if(!Hud){Test.AddError(TEXT("Frontend HUD was not created"));return false;}
        Hud->SetCanvas(nullptr,nullptr);FString Error;
        if(!Hud->LoadInterface(FPaths::Combine(FPaths::ProjectDir(),TEXT("Interface")),Error)){Test.AddError(Error);return false;}
        return true;
    }
    void ClickAction(const FString& Action)
    {
        Hud->Ui.Scale=1;
        Hud->Ui.HitRegions={{FVector2D(100,100),FVector2D(200,50),Action,TEXT("")}};
        Controller->HandlePrimaryClick(150,125);
        Hud->Ui.HitRegions.Reset();
    }
};
int32 CountCores(const FSeigeSimulation& Sim)
{int32 Count=0;for(const auto& B:Sim.Buildings)if(const auto* D=Sim.Definition(B))if(D->Role==TEXT("core"))++Count;return Count;}
FString StateText(const FSeigeSimulation& Sim,const FString& Name,FAutomationTestBase& Test)
{
    const FString Filename=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Frontend"),Name+TEXT(".json"));
    FString Error,Text;
    if(!Sim.Save(Filename,Error)||!FFileHelper::LoadFileToString(Text,*Filename))Test.AddError(TEXT("Could not compare frontend simulation snapshot: ")+Error);
    return Text;
}
// Preserve a user's existing save pointer even when an assertion fails. Generation folders are
// immutable snapshots in ignored Saved; tests do not remove user generations or working saves.
struct FSavePointerGuard
{
    FString Filename=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames/Scenario.json"));
    TArray<uint8> Original;
    bool Existed=false,Valid=false;
    FSavePointerGuard(){Existed=IFileManager::Get().FileExists(*Filename);Valid=!Existed||FFileHelper::LoadFileToArray(Original,*Filename);}
    ~FSavePointerGuard(){if(!Valid)return;if(Existed)FFileHelper::SaveArrayToFile(Original,*Filename);else IFileManager::Get().Delete(*Filename,false,true,true);}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFrontendLandingTest,"Seige.Frontend.ConfigurationAndLanding",FrontendFlags)
bool FSeigeFrontendLandingTest::RunTest(const FString& Parameters)
{
    FFrontendWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    TestEqual(TEXT("Fresh launch starts at main menu"),G.Screen,FString(TEXT("main")));
    TestEqual(TEXT("Nine configured sector slots"),G.ScenarioSlots.Num(),9);
    for(int32 I=0;I<9;++I)TestEqual(FString::Printf(TEXT("Default sector %d is human center or empty neighbor"),I),G.ScenarioSlots[I],FString(I==4?TEXT("player"):TEXT("empty")));
    W.ClickAction(TEXT("screen:scenario"));TestEqual(TEXT("Single-player menu opens scenario configuration"),G.Screen,FString(TEXT("scenario")));
    W.ClickAction(TEXT("slot:0"));TestEqual(TEXT("Empty neighbor cycles to starting AI"),G.ScenarioSlots[0],FString(TEXT("starting")));
    W.ClickAction(TEXT("slot:0"));TestEqual(TEXT("Starting neighbor cycles to developed AI"),G.ScenarioSlots[0],FString(TEXT("developed")));
    W.ClickAction(TEXT("slot:0"));TestEqual(TEXT("Developed neighbor cycles to empty"),G.ScenarioSlots[0],FString(TEXT("empty")));
    W.ClickAction(TEXT("slot:4"));TestEqual(TEXT("Human center cycles to starting AI"),G.ScenarioSlots[4],FString(TEXT("starting")));
    W.ClickAction(TEXT("slot:4"));TestEqual(TEXT("Starting center cycles to developed AI"),G.ScenarioSlots[4],FString(TEXT("developed")));
    W.ClickAction(TEXT("slot:4"));TestEqual(TEXT("Developed center cycles back to human"),G.ScenarioSlots[4],FString(TEXT("player")));
    G.RoadPlacementActive=true;G.RoadHasStart=true;G.SelectedRoadId=123;
    W.ClickAction(TEXT("start-scenario"));
    if(!TestTrue(TEXT("Scenario initialization succeeds"),G.Ready)){AddError(G.Error);return false;}
    TestTrue(TEXT("A new scenario clears previous road selection and unfinished placement"),!G.IsRoadToolActive()&&!G.RoadHasStart&&G.SelectedRoadId==0);
    TestEqual(TEXT("Human scenario first enters landing"),G.Screen,FString(TEXT("landing")));
    const double Before=G.Sim.Time;G.Tick(.2f);G.Tick(.2f);
    TestEqual(TEXT("Landing does not advance colony simulation"),G.Sim.Time,Before);
    TestEqual(TEXT("Landing has exactly one reserved central core"),CountCores(G.Sim),1);
    TestEqual(TEXT("Level-one command center uses the parked shuttle visual"),G.Sim.BuildingDefs[G.Sim.CoreDefinition].Visual,FString(TEXT("shuttle")));
    TestEqual(TEXT("Core level two retains the expanded command-center visual"),G.Sim.BuildingDefs[TEXT("command_core_2")].Visual,FString(TEXT("core")));
    G.CursorOnWorld=true;G.CursorWorld=FVector2D(G.Sim.WorldHalfSize*2,0);W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("An invalid landing click does not start time"),G.Screen,FString(TEXT("landing")));
    G.CursorWorld=FVector2D(700,0);W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("A valid world click begins play"),G.Screen,FString(TEXT("playing")));
    TestTrue(TEXT("Core occupies the chosen landing site"),G.HomePosition().Equals(FVector2D(700,0)));
    TestEqual(TEXT("Landing relocates the reserved core without duplicating it"),CountCores(G.Sim),1);
    G.Tick(.2f);TestTrue(TEXT("Time begins after landing"),G.Sim.Time>Before);
    W.ClickAction(TEXT("screen:settings"));const double SettingsTime=G.Sim.Time;G.Tick(.2f);
    TestEqual(TEXT("Settings pause the scenario timeline"),G.Sim.Time,SettingsTime);
    W.ClickAction(TEXT("back-screen"));TestEqual(TEXT("Settings return to the paused game menu"),G.Screen,FString(TEXT("game-menu")));
    W.ClickAction(TEXT("resume-game"));TestEqual(TEXT("Resume returns to the colony"),G.Screen,FString(TEXT("playing")));
    G.Sim.Failed=true;W.ClickAction(TEXT("main-menu"));
    TestEqual(TEXT("A colony-loss modal can return to the main menu"),G.Screen,FString(TEXT("main")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeGameMenuTest,"Seige.Frontend.GameMenuPauseAndSave",FrontendFlags)
bool FSeigeGameMenuTest::RunTest(const FString& Parameters)
{
    FSavePointerGuard SaveGuard;if(!SaveGuard.Valid){AddError(TEXT("Could not preserve existing save pointer"));return false;}
    FFrontendWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    G.ScenarioSlots[0]=TEXT("starting");G.StartScenario();G.ConfirmLanding(FVector2D(700,0));
    if(!TestTrue(TEXT("Menu fixture begins a live colony"),G.Ready&&G.Screen==TEXT("playing")&&G.Neighbors.Num()==1))return false;
    G.Speed=5;G.Tick(.2f);
    W.ClickAction(TEXT("game-menu"));
    TestTrue(TEXT("The direct Menu action pauses running gameplay"),G.MenuOpen&&G.Paused&&G.Screen==TEXT("game-menu"));
    const double CenterTime=G.Sim.Time,NeighborTime=G.Neighbors[0].Sim.Time;G.Tick(.2f);
    TestTrue(TEXT("The menu pauses both the colony and its independent neighbor"),G.Sim.Time==CenterTime&&G.Neighbors[0].Sim.Time==NeighborTime);
    W.ClickAction(TEXT("screen:settings"));W.Hud->HandleShortcut(EKeys::SpaceBar);G.Tick(.2f);
    TestTrue(TEXT("Settings and Space cannot resume an open game menu"),G.Screen==TEXT("settings")&&G.MenuOpen&&G.Paused&&G.Sim.Time==CenterTime);
    W.Hud->HandleShortcut(EKeys::Escape);
    TestEqual(TEXT("Escape from settings returns to the paused game menu"),G.Screen,FString(TEXT("game-menu")));
    W.Hud->HandleShortcut(EKeys::F5);
    TestTrue(TEXT("Saving from the game menu succeeds without resuming"),G.Notice.Contains(TEXT("Entire scenario saved"))&&G.Paused&&G.MenuOpen);
    FString SavedText;TSharedPtr<FJsonObject> Saved;
    if(!FFileHelper::LoadFileToString(SavedText,*SaveGuard.Filename)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SavedText),Saved)){AddError(TEXT("Menu save metadata was not readable"));return false;}
    TestFalse(TEXT("A menu save records the earlier running state rather than its temporary pause"),Saved->GetBoolField(TEXT("paused")));
    TestEqual(TEXT("Menu save retains the chosen speed"),Saved->GetNumberField(TEXT("speed")),5.);
    W.Hud->HandleShortcut(EKeys::F10);
    TestTrue(TEXT("F10 closes the menu and restores the running state"),!G.MenuOpen&&!G.Paused&&G.Screen==TEXT("playing")&&G.Speed==5);
    G.Tick(.2f);TestTrue(TEXT("Simulation advances after closing the menu"),G.Sim.Time>CenterTime);
    G.Paused=true;W.Hud->HandleShortcut(EKeys::F10);W.ClickAction(TEXT("screen:credits"));W.Hud->HandleShortcut(EKeys::F10);
    TestTrue(TEXT("F10 from a menu subpage preserves an already paused colony"),G.Screen==TEXT("playing")&&!G.MenuOpen&&G.Paused);
    G.Paused=false;W.Hud->HandleShortcut(EKeys::F10);W.ClickAction(TEXT("load"));
    TestTrue(TEXT("Loading from the menu restores the saved running state and closes the menu"),G.Screen==TEXT("playing")&&!G.MenuOpen&&!G.Paused&&G.Speed==5);
    G.StartScenario();const double LandingTime=G.Sim.Time;W.Hud->HandleShortcut(EKeys::Escape);
    TestTrue(TEXT("Escape opens the game menu before landing"),G.MenuOpen&&G.Paused&&G.MenuReturnScreen==TEXT("landing"));
    W.ClickAction(TEXT("resume-game"));G.Tick(.2f);
    TestTrue(TEXT("Resume returns to the landing survey without starting time"),G.Screen==TEXT("landing")&&!G.MenuOpen&&G.Sim.Time==LandingTime);
    W.Hud->HandleShortcut(EKeys::F10);W.Hud->HandleShortcut(EKeys::F9);
    TestTrue(TEXT("F9 loads from the pre-landing game menu just like its Load button"),G.Screen==TEXT("playing")&&!G.MenuOpen&&!G.Paused&&G.Speed==5&&G.Sim.Time==CenterTime);
    FString GraphicsText;TSharedPtr<FJsonObject> Graphics;
    if(!FFileHelper::LoadFileToString(GraphicsText,*FPaths::Combine(FPaths::ProjectDir(),TEXT("Graphics/scene.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(GraphicsText),Graphics)){AddError(TEXT("Authored camera fixture was not readable"));return false;}
    G.CameraCenter=FVector(50000,-50000,0);G.Zoom=G.MaximumZoom;G.CameraYaw=320;G.CameraPitch=15;
    W.ClickAction(TEXT("main-menu"));
    TestTrue(TEXT("Returning from a distant view restores the authored main-menu backdrop"),G.Screen==TEXT("main")&&G.CameraCenter.IsNearlyZero()&&G.Zoom==G.DefaultZoom&&FMath::IsNearlyEqual(double(G.CameraYaw),Graphics->GetNumberField(TEXT("camera_yaw")),.0001)&&FMath::IsNearlyEqual(double(G.CameraPitch),Graphics->GetNumberField(TEXT("camera_pitch")),.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFrontendObserverTest,"Seige.Frontend.AICenterObservation",FrontendFlags)
bool FSeigeFrontendObserverTest::RunTest(const FString& Parameters)
{
    FFrontendWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    G.ScenarioSlots[4]=TEXT("starting");G.ScenarioSlots[0]=TEXT("starting");G.ScenarioSlots[8]=TEXT("developed");G.StartScenario();
    if(!TestTrue(TEXT("AI observer scenario initializes"),G.Ready)){AddError(G.Error);return false;}
    TestTrue(TEXT("AI-controlled center selects observer mode"),G.Observer);TestEqual(TEXT("Observer skips human landing"),G.Screen,FString(TEXT("playing")));
    TestEqual(TEXT("Only configured occupied neighbors are instantiated"),G.Neighbors.Num(),2);
    TestTrue(TEXT("Occupied regions use different deterministic generation seeds"),G.Neighbors[0].Sim.GenerationSeed!=G.Sim.GenerationSeed&&G.Neighbors[1].Sim.GenerationSeed!=G.Sim.GenerationSeed&&G.Neighbors[0].Sim.GenerationSeed!=G.Neighbors[1].Sim.GenerationSeed);
    const double CenterBefore=G.Sim.Time;
    TArray<double> NeighborTimes;for(const auto& N:G.Neighbors)NeighborTimes.Add(N.Sim.Time);
    for(int32 I=0;I<12;++I)G.Tick(.2f);
    TestTrue(TEXT("Center AI colony advances"),G.Sim.Time>CenterBefore);
    for(int32 I=0;I<G.Neighbors.Num();++I){TestTrue(TEXT("Independent neighbor simulation advances"),G.Neighbors[I].Sim.Time>NeighborTimes[I]);TestEqual(TEXT("Each occupied sector has only one core"),CountCores(G.Neighbors[I].Sim),1);}
    W.Hud->HandleShortcut(EKeys::B);TestFalse(TEXT("Observer cannot open construction"),W.Hud->Ui.BuildOpen);
    G.Sim.Won=true;G.WinAcknowledged=false;W.ClickAction(TEXT("pause"));
    TestTrue(TEXT("A center AI objective never blocks observer controls"),G.Paused);
    W.ClickAction(TEXT("pause"));G.Sim.Won=false;
    const int32 CoreCount=CountCores(G.Sim);G.ConfirmLanding(FVector2D(1000,1000));TestEqual(TEXT("Observer cannot create a second core through landing"),CountCores(G.Sim),CoreCount);
    G.ReturnToMainMenu();G.ScenarioSlots[4]=TEXT("developed");G.StartScenario();
    TestTrue(TEXT("Developed center remains observable"),G.Observer&&G.Ready&&G.Screen==TEXT("playing"));
    TestTrue(TEXT("Developed center begins with constructed industry"),G.Sim.Buildings.Num()>1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeNeighborhoodSaveTest,"Seige.Frontend.NeighborhoodSaveLoad",FrontendFlags)
bool FSeigeNeighborhoodSaveTest::RunTest(const FString& Parameters)
{
    FSavePointerGuard SaveGuard;if(!SaveGuard.Valid){AddError(TEXT("Could not preserve existing save pointer; test stopped before modifying saves"));return false;}
    FFrontendWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    G.ScenarioSlots[0]=TEXT("starting");G.ScenarioSlots[8]=TEXT("developed");G.StartScenario();G.ConfirmLanding(FVector2D(700,0));
    if(!TestTrue(TEXT("Save scenario is ready and playing"),G.Ready&&G.Screen==TEXT("playing"))){AddError(G.Error);return false;}
    for(int32 I=0;I<64;++I)G.Tick(.2f);
    G.CameraCenter=FVector(3000,-1200,0);G.Zoom=27000;G.CameraYaw=224;G.CameraPitch=67;G.Speed=5;G.Paused=true;G.WinAcknowledged=true;
    const FString CenterBefore=StateText(G.Sim,TEXT("center-before"),*this);
    TMap<int32,FString> NeighborBefore;for(const auto& N:G.Neighbors)NeighborBefore.Add(N.Index,StateText(N.Sim,TEXT("neighbor-before-")+FString::FromInt(N.Index),*this));
    G.SaveGame();FString MetadataText;
    if(!TestTrue(TEXT("Scenario save completes successfully"),G.Notice.Contains(TEXT("Entire scenario saved")))){AddError(G.Notice);return false;}
    if(!TestTrue(TEXT("Scenario save metadata is written"),FFileHelper::LoadFileToString(MetadataText,*SaveGuard.Filename)))return false;
    G.Paused=false;G.Speed=1;for(int32 I=0;I<20;++I)G.Tick(.2f);G.CameraCenter=FVector::ZeroVector;G.Zoom=6500;G.CameraYaw=10;G.CameraPitch=30;G.ScenarioSlots[0]=TEXT("empty");
    G.LoadGame();
    TestEqual(TEXT("Center complete state restores exactly"),StateText(G.Sim,TEXT("center-restored"),*this),CenterBefore);
    TestEqual(TEXT("Saved neighbor count restores"),G.Neighbors.Num(),NeighborBefore.Num());
    for(const auto& N:G.Neighbors)TestEqual(TEXT("Neighbor full simulation snapshot restores"),StateText(N.Sim,TEXT("neighbor-restored-")+FString::FromInt(N.Index),*this),NeighborBefore.FindRef(N.Index));
    TestEqual(TEXT("Scenario slot configuration restores"),G.ScenarioSlots[0],FString(TEXT("starting")));
    TestTrue(TEXT("Camera focus, orbit, zoom, speed, pause and objective acknowledgment restore"),G.CameraCenter.Equals(FVector(3000,-1200,0))&&G.Zoom==27000&&G.CameraYaw==224&&G.CameraPitch==67&&G.Speed==5&&G.Paused&&G.WinAcknowledged);
    TSharedPtr<FJsonObject> Metadata;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MetadataText),Metadata)){AddError(TEXT("Could not read emitted metadata"));return false;}
    TestEqual(TEXT("Neighborhood saves use strict version-5 metadata"),Metadata->GetNumberField(TEXT("format")),5.);
    const auto Fingerprints=Metadata->GetArrayField(TEXT("neighbor_ai"));
    if(!TestTrue(TEXT("Metadata contains neighbor identities"),Fingerprints.Num()>0))return false;
    Fingerprints[0]->AsObject()->SetStringField(TEXT("ai"),TEXT("corrupted-fingerprint"));
    FString Corrupt;FJsonSerializer::Serialize(Metadata.ToSharedRef(),TJsonWriterFactory<>::Create(&Corrupt));FFileHelper::SaveStringToFile(Corrupt,*SaveGuard.Filename);
    G.LoadGame();
    TestTrue(TEXT("Corrupt AI metadata reports load rejection"),G.Notice.Contains(TEXT("AI files have changed")));
    TestEqual(TEXT("Rejected metadata leaves center simulation unchanged"),StateText(G.Sim,TEXT("center-rejected"),*this),CenterBefore);
    TestEqual(TEXT("Rejected metadata leaves neighbors unchanged"),G.Neighbors.Num(),NeighborBefore.Num());
    for(const auto& N:G.Neighbors)TestEqual(TEXT("Rejected metadata preserves each neighbor"),StateText(N.Sim,TEXT("neighbor-rejected-")+FString::FromInt(N.Index),*this),NeighborBefore.FindRef(N.Index));
    TestTrue(TEXT("Rejected metadata leaves camera and gameplay flags unchanged"),G.CameraCenter.Equals(FVector(3000,-1200,0))&&G.CameraYaw==224&&G.CameraPitch==67&&G.Paused&&G.Speed==5&&G.Screen==TEXT("playing"));
    auto FreshMetadata=[&]()
    {
        TSharedPtr<FJsonObject> Result;
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MetadataText),Result))AddError(TEXT("Cannot recreate original save metadata"));
        return Result;
    };
    auto WriteMetadata=[&](const TSharedPtr<FJsonObject>& Value)
    {
        FString Json;
        return Value&&FJsonSerializer::Serialize(Value.ToSharedRef(),TJsonWriterFactory<>::Create(&Json))&&FFileHelper::SaveStringToFile(Json,*SaveGuard.Filename);
    };
    for(int32 Case=0;Case<6;++Case)
    {
        auto Invalid=FreshMetadata();if(!Invalid)return false;
        if(Case==0)Invalid->SetNumberField(TEXT("camera_yaw"),361);
        else if(Case==1)Invalid->SetStringField(TEXT("camera_yaw"),TEXT("invalid"));
        else if(Case==2)Invalid->SetNumberField(TEXT("camera_pitch"),G.MinimumCameraPitch-1);
        else if(Case==3)Invalid->SetNumberField(TEXT("camera_pitch"),G.MaximumCameraPitch+1);
        else if(Case==4)Invalid->RemoveField(TEXT("camera_yaw"));
        else Invalid->RemoveField(TEXT("camera_pitch"));
        if(!TestTrue(TEXT("Invalid-camera fixture is written"),WriteMetadata(Invalid)))return false;
        G.LoadGame();
        TestTrue(TEXT("Malformed or out-of-range saved orientation is rejected"),G.Notice.Contains(TEXT("camera orientation is invalid")));
        TestTrue(TEXT("Rejected camera metadata cannot alter the live view or timeline"),G.CameraYaw==224&&G.CameraPitch==67&&G.Zoom==27000&&G.Paused&&G.Speed==5);
        TestEqual(TEXT("Rejected camera metadata leaves simulation state unchanged"),StateText(G.Sim,TEXT("camera-rejected"),*this),CenterBefore);
    }
    for(int32 Case=0;Case<8;++Case)
    {
        auto Invalid=FreshMetadata();if(!Invalid)return false;
        if(Case==0)Invalid->SetNumberField(TEXT("format"),4);
        else if(Case==1)Invalid->SetNumberField(TEXT("speed"),3);
        else if(Case==2)Invalid->SetNumberField(TEXT("speed"),0);
        else if(Case==3)Invalid->SetStringField(TEXT("speed"),TEXT("5"));
        else if(Case==4)Invalid->SetNumberField(TEXT("zoom"),0);
        else if(Case==5)Invalid->SetNumberField(TEXT("camera_x"),G.Sim.WorldHalfSize*3);
        else if(Case==6)Invalid->RemoveField(TEXT("speed"));
        else Invalid->RemoveField(TEXT("camera_x"));
        if(!TestTrue(TEXT("Invalid strict-format fixture is written"),WriteMetadata(Invalid)))return false;
        G.LoadGame();
        TestFalse(TEXT("Legacy or invalid required metadata cannot restore"),G.Notice.Contains(TEXT("Scenario restored")));
        if(Case==0)TestTrue(TEXT("Older format clearly requests a new scenario"),G.Notice.Contains(TEXT("Start a new scenario")));
        TestTrue(TEXT("Rejected strict metadata preserves the camera and time controls"),G.CameraCenter.Equals(FVector(3000,-1200,0))&&G.CameraYaw==224&&G.CameraPitch==67&&G.Zoom==27000&&G.Paused&&G.Speed==5);
        TestEqual(TEXT("Rejected strict metadata preserves the center"),StateText(G.Sim,TEXT("strict-metadata-rejected"),*this),CenterBefore);
        for(const auto& N:G.Neighbors)TestEqual(TEXT("Rejected strict metadata preserves each neighbor"),StateText(N.Sim,TEXT("strict-neighbor-rejected-")+FString::FromInt(N.Index),*this),NeighborBefore.FindRef(N.Index));
    }
    auto CloseView=FreshMetadata();if(!CloseView)return false;CloseView->SetNumberField(TEXT("zoom"),1200);
    if(!TestTrue(TEXT("Valid close-view format-5 fixture is written"),WriteMetadata(CloseView)))return false;
    G.LoadGame();TestTrue(TEXT("Complete format-5 saves restore a valid close camera"),G.Notice.Contains(TEXT("Scenario restored"))&&G.Zoom==1200&&G.CameraYaw==224&&G.CameraPitch==67);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeScenarioPreparationTest,"Seige.Frontend.ScenarioPreparationCancellationAndAtomicCommit",FrontendFlags)
bool FSeigeScenarioPreparationTest::RunTest(const FString& Parameters)
{
    FFrontendWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    G.StartScenario();G.ConfirmLanding(FVector2D(700,0));
    const FString Original=StateText(G.Sim,TEXT("preparation-original"),*this);
    G.ShowScreen(TEXT("scenario"));G.ScenarioSlots[0]=TEXT("developed");
    G.BeginScenarioPreparation();
    TestTrue(TEXT("Explicit preparation immediately returns control with a preparing screen"),G.IsPreparingScenario()&&G.Screen==TEXT("preparing"));
    G.TickScenarioPreparation(.1);G.TickScenarioPreparation(.1);
    TestFalse(TEXT("Preparation exposes useful progress status"),G.ScenarioPreparationStatus().IsEmpty());
    TestTrue(TEXT("Preparation progress stays bounded"),G.ScenarioPreparationProgress()>=0&&G.ScenarioPreparationProgress()<=1);
    TestEqual(TEXT("Partial developed setup does not publish candidate state"),StateText(G.Sim,TEXT("preparation-in-progress"),*this),Original);
    G.CancelScenarioPreparation();
    TestTrue(TEXT("Cancellation returns to the originating configuration screen"),!G.IsPreparingScenario()&&G.Screen==TEXT("scenario"));
    TestEqual(TEXT("Cancellation leaves the active simulation exactly intact"),StateText(G.Sim,TEXT("preparation-cancelled"),*this),Original);
    TestTrue(TEXT("Cancellation publishes no partially prepared neighbors"),G.Neighbors.IsEmpty());
    G.ScenarioSlots[0]=TEXT("starting");G.ScenarioBackgroundBugs=false;G.ScenarioPeriodicAttacks=false;
    G.BeginScenarioPreparation();
    for(int I=0;I<100&&G.IsPreparingScenario();++I)G.TickScenarioPreparation(2);
    TestTrue(TEXT("Completed candidate commits the entire neighborhood together"),!G.IsPreparingScenario()&&G.Screen==TEXT("landing")&&G.Neighbors.Num()==1&&G.Neighbors[0].Index==0);
    TestTrue(TEXT("Published center and neighbor inherit the captured scenario choices"),!G.Sim.BackgroundBugsEnabled&&!G.Sim.PeriodicAttacksEnabled&&!G.Neighbors[0].Sim.BackgroundBugsEnabled&&!G.Neighbors[0].Sim.PeriodicAttacksEnabled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeScenarioThreatSettingsTest,"Seige.Frontend.ScenarioThreatSettings",FrontendFlags)
bool FSeigeScenarioThreatSettingsTest::RunTest(const FString& Parameters)
{
    FSavePointerGuard SaveGuard;if(!SaveGuard.Valid){AddError(TEXT("Could not preserve existing save pointer"));return false;}
    FFrontendWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    TestTrue(TEXT("New scenario threat choices both default on"),G.ScenarioBackgroundBugs&&G.ScenarioPeriodicAttacks);
    FString LastMetadata;
    for(int32 Combination=0;Combination<4;++Combination)
    {
        const bool Background=(Combination&1)!=0,Periodic=(Combination&2)!=0;
        G.ReturnToMainMenu();W.ClickAction(TEXT("screen:scenario"));
        if(G.ScenarioBackgroundBugs!=Background)W.ClickAction(TEXT("scenario-threat:background"));
        if(G.ScenarioPeriodicAttacks!=Periodic)W.ClickAction(TEXT("scenario-threat:periodic"));
        TestTrue(TEXT("Real scenario button routes set independent choices"),G.ScenarioBackgroundBugs==Background&&G.ScenarioPeriodicAttacks==Periodic);
        G.ScenarioSlots[0]=TEXT("starting");G.ScenarioSlots[8]=TEXT("developed");
        G.ScenarioSlots[4]=Combination==3?TEXT("developed"):Combination==1?TEXT("starting"):TEXT("player");
        W.ClickAction(TEXT("start-scenario"));
        if(!TestTrue(TEXT("All four combinations start a complete neighborhood"),G.Ready&&G.Neighbors.Num()==2&&G.Screen!=(TEXT("scenario")))){AddError(G.Notice);return false;}
        if(G.Screen==TEXT("landing"))G.ConfirmLanding(FVector2D(700,0));
        TestTrue(TEXT("Human and AI centers receive the chosen threats"),G.Sim.BackgroundBugsEnabled==Background&&G.Sim.PeriodicAttacksEnabled==Periodic);
        for(const auto& N:G.Neighbors)
        {
            TestTrue(TEXT("Starting and developed neighbors receive the same settings"),N.Sim.BackgroundBugsEnabled==Background&&N.Sim.PeriodicAttacksEnabled==Periodic);
            if(N.Type==TEXT("developed"))
            {
                TestTrue(TEXT("Developed preparation actually advances the simulation"),N.Sim.Time>0&&N.Sim.DeliveredUnits>0);
                TestEqual(TEXT("Periodic waves honor the selection during preparation, not just afterward"),N.Sim.Wave>0,Periodic);
                if(!Background&&!Periodic)TestTrue(TEXT("Peaceful developed preparation creates no bugs"),N.Sim.Enemies.IsEmpty());
            }
        }
        W.ClickAction(TEXT("scenario-threat:background"));W.ClickAction(TEXT("scenario-threat:periodic"));
        TestTrue(TEXT("Stale scenario controls cannot change an active game"),G.ScenarioBackgroundBugs==Background&&G.ScenarioPeriodicAttacks==Periodic&&G.Sim.BackgroundBugsEnabled==Background&&G.Sim.PeriodicAttacksEnabled==Periodic);
        G.SaveGame();
        if(!TestTrue(TEXT("Each selected combination saves"),G.Notice.Contains(TEXT("Entire scenario saved")))){AddError(G.Notice);return false;}
        if(!FFileHelper::LoadFileToString(LastMetadata,*SaveGuard.Filename))return false;
        G.ReturnToMainMenu();G.ScenarioBackgroundBugs=!Background;G.ScenarioPeriodicAttacks=!Periodic;
        G.LoadGame();
        if(!TestTrue(TEXT("Load restores scenario choices instead of current menu choices"),G.Notice.Contains(TEXT("Scenario restored"))&&G.ScenarioBackgroundBugs==Background&&G.ScenarioPeriodicAttacks==Periodic))AddError(TEXT("Threat load rejected: ")+G.Notice);
        TestTrue(TEXT("Saved center preserves the chosen threats"),G.Sim.BackgroundBugsEnabled==Background&&G.Sim.PeriodicAttacksEnabled==Periodic);
        for(const auto& N:G.Neighbors)TestTrue(TEXT("Saved neighbors preserve the chosen threats"),N.Sim.BackgroundBugsEnabled==Background&&N.Sim.PeriodicAttacksEnabled==Periodic);
    }
    const FString CenterBefore=StateText(G.Sim,TEXT("threat-center-before"),*this);
    TMap<int32,FString> NeighborBefore;for(const auto& N:G.Neighbors)NeighborBefore.Add(N.Index,StateText(N.Sim,TEXT("threat-neighbor-before-")+FString::FromInt(N.Index),*this));
    auto ReadOriginal=[&](){TSharedPtr<FJsonObject> Object;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(LastMetadata),Object);return Object;};
    auto WriteJson=[](const TSharedPtr<FJsonObject>& Object,const FString& Filename)
    {FString Raw;return Object&&FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw))&&FFileHelper::SaveStringToFile(Raw,*Filename);};
    auto AssertUnchanged=[&]()
    {
        TestTrue(TEXT("Rejected load preserves active settings and screen"),G.ScenarioBackgroundBugs&&G.ScenarioPeriodicAttacks&&G.Screen==TEXT("playing"));
        TestEqual(TEXT("Rejected threat metadata cannot mutate the center"),StateText(G.Sim,TEXT("threat-center-after"),*this),CenterBefore);
        for(const auto& N:G.Neighbors)TestEqual(TEXT("Rejected threat metadata cannot mutate neighbors"),StateText(N.Sim,TEXT("threat-neighbor-after-")+FString::FromInt(N.Index),*this),NeighborBefore.FindRef(N.Index));
    };
    for(int32 Variant=0;Variant<6;++Variant)
    {
        auto Object=ReadOriginal();if(!Object)return false;
        if(Variant==0)Object->RemoveField(TEXT("background_bugs"));
        if(Variant==1)Object->SetStringField(TEXT("background_bugs"),TEXT("true"));
        if(Variant==2)Object->SetNumberField(TEXT("periodic_attacks"),1);
        if(Variant==3)Object->SetField(TEXT("periodic_attacks"),MakeShared<FJsonValueNull>());
        if(Variant==4)Object->SetBoolField(TEXT("background_bugs"),false);
        if(Variant==5){Object->RemoveField(TEXT("background_bugs"));Object->RemoveField(TEXT("periodic_attacks"));}
        if(!WriteJson(Object,SaveGuard.Filename))return false;
        G.LoadGame();
        TestTrue(TEXT("Malformed or inconsistent scenario threat metadata is rejected"),G.Notice.Contains(TEXT("threat settings")));
        AssertUnchanged();
    }
    auto Original=ReadOriginal();if(!Original)return false;
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames/Scenarios"),Original->GetStringField(TEXT("generation")));
    const FString NeighborFile=FPaths::Combine(Directory,TEXT("sector_0.json"));
    FString NeighborText;TSharedPtr<FJsonObject> Neighbor;
    if(!FFileHelper::LoadFileToString(NeighborText,*NeighborFile)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(NeighborText),Neighbor))return false;
    Neighbor->SetBoolField(TEXT("periodic_attacks"),false);
    if(!WriteJson(Original,SaveGuard.Filename)||!WriteJson(Neighbor,NeighborFile))return false;
    G.LoadGame();TestTrue(TEXT("A disagreeing child snapshot is rejected atomically"),G.Notice.Contains(TEXT("saved neighbor")));AssertUnchanged();
    if(!FFileHelper::SaveStringToFile(NeighborText,*NeighborFile))return false;
    Neighbor->RemoveField(TEXT("background_bugs"));Neighbor->RemoveField(TEXT("periodic_attacks"));
    if(!WriteJson(Neighbor,NeighborFile))return false;
    G.LoadGame();TestFalse(TEXT("A child snapshot missing required threat settings cannot restore"),G.Notice.Contains(TEXT("Scenario restored")));AssertUnchanged();
    if(!FFileHelper::SaveStringToFile(NeighborText,*NeighborFile))return false;
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeDisplayAntialiasingTest,"Seige.Frontend.DisplayAntialiasingSelection",FrontendFlags)
bool FSeigeDisplayAntialiasingTest::RunTest(const FString& Parameters)
{
    FFrontendWorld W;if(!W.Prepare(*this))return false;auto& G=*W.Game;
    auto* Method=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
    if(!TestNotNull(TEXT("Engine antialiasing selector exists"),Method))return false;
    struct FRestoreGraphics
    {
        Scalability::FQualityLevels Levels=Scalability::GetQualityLevels();
        IConsoleVariable* Method=nullptr;FString Value;EConsoleVariableFlags Priority;
        explicit FRestoreGraphics(IConsoleVariable* Variable):Method(Variable),Value(Variable->GetString()),Priority(static_cast<EConsoleVariableFlags>(Variable->GetFlags()&ECVF_SetByMask)){}
        ~FRestoreGraphics(){Scalability::SetQualityLevels(Levels,true);Method->ClearFlags(ECVF_SetByMask);Method->Set(*Value,Priority);}
    } Restore(Method);
    TestEqual(TEXT("Default full-resolution profile selects TAA"),G.NativeAntialiasing,2);
    TestEqual(TEXT("Default reduced-resolution profile selects TSR"),G.UpscalingAntialiasing,4);
    // NullRHI does not resize an actual viewport. Exercise the same profile method
    // against the engine's authoritative resolution setting; DisplaySmoke covers
    // the real window/settings route. Restore every changed global on return.
    Method->ClearFlags(ECVF_SetByMask);
    for(const float Resolution:{100.f,75.f,100.f})
    {
        auto Levels=Scalability::GetQualityLevels();Levels.ResolutionQuality=Resolution;
        Scalability::SetQualityLevels(Levels,true);G.ApplyMediumPreset();
        TestEqual(*FString::Printf(TEXT("Medium applies the correct engine AA method at %.0f percent"),Resolution),Method->GetInt(),Resolution==75.f?4:2);
        TestTrue(TEXT("Reapplying Medium preserves the selected rendering resolution"),FMath::IsNearlyEqual(Scalability::GetQualityLevels().ResolutionQuality,Resolution,.01f));
    }
    Method->Set(1,ECVF_SetByConsole);G.ApplyMediumPreset();
    TestEqual(TEXT("An explicit diagnostic console override retains priority"),Method->GetInt(),1);
    return true;
}

#endif
