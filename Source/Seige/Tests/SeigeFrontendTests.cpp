#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
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
    W.ClickAction(TEXT("start-scenario"));
    if(!TestTrue(TEXT("Scenario initialization succeeds"),G.Ready)){AddError(G.Error);return false;}
    TestEqual(TEXT("Human scenario first enters landing"),G.Screen,FString(TEXT("landing")));
    const double Before=G.Sim.Time;G.Tick(.2f);G.Tick(.2f);
    TestEqual(TEXT("Landing does not advance colony simulation"),G.Sim.Time,Before);
    TestEqual(TEXT("Landing has exactly one reserved central core"),CountCores(G.Sim),1);
    G.CursorOnWorld=true;G.CursorWorld=FVector2D(G.Sim.WorldHalfSize*2,0);W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("An invalid landing click does not start time"),G.Screen,FString(TEXT("landing")));
    G.CursorWorld=FVector2D(700,0);W.Controller->HandlePrimaryClick(900,500);
    TestEqual(TEXT("A valid world click begins play"),G.Screen,FString(TEXT("playing")));
    TestTrue(TEXT("Core occupies the chosen landing site"),G.HomePosition().Equals(FVector2D(700,0)));
    TestEqual(TEXT("Landing relocates the reserved core without duplicating it"),CountCores(G.Sim),1);
    G.Tick(.2f);TestTrue(TEXT("Time begins after landing"),G.Sim.Time>Before);
    W.ClickAction(TEXT("screen:settings"));const double SettingsTime=G.Sim.Time;G.Tick(.2f);
    TestEqual(TEXT("Settings pause the scenario timeline"),G.Sim.Time,SettingsTime);
    W.ClickAction(TEXT("back-screen"));TestEqual(TEXT("Settings return to the previous gameplay screen"),G.Screen,FString(TEXT("playing")));
    G.Sim.Failed=true;W.ClickAction(TEXT("main-menu"));
    TestEqual(TEXT("A colony-loss modal can return to the main menu"),G.Screen,FString(TEXT("main")));
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
    G.CameraCenter=FVector(3000,-1200,0);G.Zoom=27000;G.CameraYaw=224;G.CameraPitch=67;G.Speed=3;G.Paused=true;G.WinAcknowledged=true;
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
    TestTrue(TEXT("Camera focus, orbit, zoom, speed, pause and objective acknowledgment restore"),G.CameraCenter.Equals(FVector(3000,-1200,0))&&G.Zoom==27000&&G.CameraYaw==224&&G.CameraPitch==67&&G.Speed==3&&G.Paused&&G.WinAcknowledged);
    TSharedPtr<FJsonObject> Metadata;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MetadataText),Metadata)){AddError(TEXT("Could not read emitted metadata"));return false;}
    const auto Fingerprints=Metadata->GetArrayField(TEXT("neighbor_ai"));
    if(!TestTrue(TEXT("Metadata contains neighbor identities"),Fingerprints.Num()>0))return false;
    Fingerprints[0]->AsObject()->SetStringField(TEXT("ai"),TEXT("corrupted-fingerprint"));
    FString Corrupt;FJsonSerializer::Serialize(Metadata.ToSharedRef(),TJsonWriterFactory<>::Create(&Corrupt));FFileHelper::SaveStringToFile(Corrupt,*SaveGuard.Filename);
    G.LoadGame();
    TestTrue(TEXT("Corrupt AI metadata reports load rejection"),G.Notice.Contains(TEXT("AI files have changed")));
    TestEqual(TEXT("Rejected metadata leaves center simulation unchanged"),StateText(G.Sim,TEXT("center-rejected"),*this),CenterBefore);
    TestEqual(TEXT("Rejected metadata leaves neighbors unchanged"),G.Neighbors.Num(),NeighborBefore.Num());
    for(const auto& N:G.Neighbors)TestEqual(TEXT("Rejected metadata preserves each neighbor"),StateText(N.Sim,TEXT("neighbor-rejected-")+FString::FromInt(N.Index),*this),NeighborBefore.FindRef(N.Index));
    TestTrue(TEXT("Rejected metadata leaves camera and gameplay flags unchanged"),G.CameraCenter.Equals(FVector(3000,-1200,0))&&G.CameraYaw==224&&G.CameraPitch==67&&G.Paused&&G.Speed==3&&G.Screen==TEXT("playing"));
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
    for(int32 Case=0;Case<4;++Case)
    {
        auto Invalid=FreshMetadata();if(!Invalid)return false;
        if(Case==0)Invalid->SetNumberField(TEXT("camera_yaw"),361);
        else if(Case==1)Invalid->SetStringField(TEXT("camera_yaw"),TEXT("invalid"));
        else if(Case==2)Invalid->SetNumberField(TEXT("camera_pitch"),G.MinimumCameraPitch-1);
        else Invalid->SetNumberField(TEXT("camera_pitch"),G.MaximumCameraPitch+1);
        if(!TestTrue(TEXT("Invalid-camera fixture is written"),WriteMetadata(Invalid)))return false;
        G.LoadGame();
        TestTrue(TEXT("Malformed or out-of-range saved orientation is rejected"),G.Notice.Contains(TEXT("camera orientation is invalid")));
        TestTrue(TEXT("Rejected camera metadata cannot alter the live view or timeline"),G.CameraYaw==224&&G.CameraPitch==67&&G.Zoom==27000&&G.Paused&&G.Speed==3);
        TestEqual(TEXT("Rejected camera metadata leaves simulation state unchanged"),StateText(G.Sim,TEXT("camera-rejected"),*this),CenterBefore);
    }
    auto Legacy=FreshMetadata();if(!Legacy)return false;
    Legacy->RemoveField(TEXT("camera_yaw"));Legacy->RemoveField(TEXT("camera_pitch"));Legacy->SetNumberField(TEXT("zoom"),1200);
    if(!TestTrue(TEXT("Legacy format-2 camera fixture is written"),WriteMetadata(Legacy)))return false;
    G.CameraYaw=10;G.CameraPitch=30;G.LoadGame();
    TestTrue(TEXT("Format-2 saves without orbit fields still load"),G.Notice.Contains(TEXT("Scenario restored")));
    TestTrue(TEXT("Legacy saves use stable default angles instead of the current view"),G.CameraYaw==GetDefault<ASeigeGameMode>()->CameraYaw&&G.CameraPitch==GetDefault<ASeigeGameMode>()->CameraPitch);
    TestEqual(TEXT("New close perspective zoom survives loading below the old orthographic minimum"),G.Zoom,1200.f);
    TestEqual(TEXT("Camera migration does not change the saved economy"),StateText(G.Sim,TEXT("camera-migrated"),*this),CenterBefore);
    Legacy->SetNumberField(TEXT("zoom"),0);
    if(!TestTrue(TEXT("Below-minimum zoom fixture is written"),WriteMetadata(Legacy)))return false;
    G.LoadGame();TestEqual(TEXT("Loading enforces the current configured minimum zoom"),G.Zoom,G.MinimumZoom);
    return true;
}
#endif
