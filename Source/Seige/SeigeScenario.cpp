#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

FString ASeigeGameMode::DataDirectory(const TCHAR* Folder) const
{
    const FString Project=FPaths::Combine(FPaths::ProjectDir(),Folder);
    return IFileManager::Get().DirectoryExists(*Project)?Project:FPaths::Combine(FPlatformProcess::BaseDir(),Folder);
}
FVector2D ASeigeGameMode::HomePosition() const
{
    for(const auto& B:Sim.Buildings) if(const auto* D=Sim.Definition(B)) if(D->Role==TEXT("core")) return B.Position;
    return FVector2D::ZeroVector;
}
void ASeigeGameMode::ShowScreen(const FString& NewScreen)
{
    if(NewScreen==TEXT("settings")||NewScreen==TEXT("credits")) ReturnScreen=Screen;
    Screen=NewScreen;
    SelectedBuild.Empty(); SelectedId=0;
}
void ASeigeGameMode::ReturnToMainMenu()
{
    Screen=TEXT("main"); SelectedBuild.Empty(); SelectedId=0;
    Notice=Ready?TEXT("Single player runs entirely offline."):(Error.IsEmpty()?TEXT("Game definitions could not be loaded."):Error);
}
void ASeigeGameMode::CycleScenarioSlot(int32 Index)
{
    if(!ScenarioSlots.IsValidIndex(Index)) return;
    FString& Value=ScenarioSlots[Index];
    if(Index==4) Value=Value==TEXT("player")?TEXT("starting"):Value==TEXT("starting")?TEXT("developed"):TEXT("player");
    else Value=Value==TEXT("empty")?TEXT("starting"):Value==TEXT("starting")?TEXT("developed"):TEXT("empty");
}
bool ASeigeGameMode::InitializeScenario(FString& Reason)
{
    FSeigeSimulation NewCenter;
    TSharedPtr<FSeigeScenarioAI> NewCenterBrain;
    TArray<FSeigeNeighbor> NewNeighbors;
    const bool NewObserver=ScenarioSlots[4]!=TEXT("player");
    if(NewObserver)
    {
        NewCenterBrain=MakeShared<FSeigeScenarioAI>();
        if(!NewCenterBrain->Initialize(NewCenter,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),ScenarioSlots[4]==TEXT("developed"),Reason)) return false;
    }
    else if(!NewCenter.Initialize(DataDirectory(TEXT("Rules")),Reason)) return false;
    for(int32 Index=0;Index<9;Index++)
    {
        if(Index==4||ScenarioSlots[Index]==TEXT("empty")) continue;
        FSeigeNeighbor N; N.Index=Index; N.Type=ScenarioSlots[Index];
        N.Offset=FVector2D(Index%3-1,Index/3-1)*NewCenter.WorldHalfSize*2;
        N.Brain=MakeShared<FSeigeScenarioAI>();
        if(!N.Brain->Initialize(N.Sim,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),N.Type==TEXT("developed"),Reason)) return false;
        NewNeighbors.Add(MoveTemp(N));
    }
    Sim=MoveTemp(NewCenter); CenterBrain=MoveTemp(NewCenterBrain); Neighbors=MoveTemp(NewNeighbors); Observer=NewObserver;
    return true;
}
void ASeigeGameMode::StartScenario()
{
    if(!GraphicsSettingsValid){Notice=Error.IsEmpty()?TEXT("Correct Graphics/scene.json and restart the game."):Error;return;}
    if(!InitializeScenario(Error)) { Notice=Error; return; }
    for(auto& Pair:Visuals) if(Pair.Value) Pair.Value->Destroy();
    Visuals.Empty();
    for(auto It=Materials.CreateIterator();It;++It)if(It.Key().StartsWith(TEXT("construction_original_")))It.RemoveCurrent();
    Ready=true; SelectedId=0; SelectedBuild.Empty(); WinAcknowledged=false;
    Accumulator=0; Speed=1; Paused=false;
    Screen=Observer?TEXT("playing"):TEXT("landing");
    CameraCenter=FVector(HomePosition(),0); Zoom=DefaultZoom*2;
    Notice=Observer?TEXT("OBSERVATION MODE | AI colonies follow the same industry and defense rules."):TEXT("CHOOSE YOUR COMMAND CENTER | Time is paused. Survey deposits, then click a landing site.");
    CreateLandscape(); SyncVisuals(); UpdateCamera();
}
bool ASeigeGameMode::CanLand(FVector2D Position,FString& Reason) const
{
    if(!Sim.CanSetInitialCorePosition(Position,Reason)) return false;
    Reason=TEXT("Click to land here and start the scenario."); return true;
}
void ASeigeGameMode::ConfirmLanding(FVector2D Position)
{
    if(Screen!=TEXT("landing")) return;
    if(!CanLand(Position,Error)||!Sim.SetInitialCorePosition(Position,Error)) { Notice=Error; return; }
    Screen=TEXT("playing"); Paused=false; CameraCenter=FVector(Position,0); Zoom=DefaultZoom;
    Notice=TEXT("SHUTTLE LANDING | Robots are deploying the command center. Time is running.");
    CreateLandscape(); SyncVisuals(); UpdateCamera();
}
void ASeigeGameMode::SetGraphicsQuality(int32 Quality)
{
    GraphicsQuality=FMath::Clamp(Quality,0,3);
    if(auto* Settings=UGameUserSettings::GetGameUserSettings()) { Settings->SetOverallScalabilityLevel(GraphicsQuality); Settings->ApplySettings(false); Settings->SaveSettings(); }
}
void ASeigeGameMode::SetFullscreen(bool Enabled)
{
    Fullscreen=Enabled;
    if(auto* Settings=UGameUserSettings::GetGameUserSettings()) { Settings->SetFullscreenMode(Enabled?EWindowMode::WindowedFullscreen:EWindowMode::Windowed); Settings->ApplySettings(false); Settings->SaveSettings(); }
}
