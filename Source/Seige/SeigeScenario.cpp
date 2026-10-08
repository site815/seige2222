#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "Simulation/SeigeResourceGeneration.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"

struct FSeigeScenarioPreparation
{
    TArray<FString> Slots;
    FString ReturnScreen;
    bool Background=true,Periodic=true,Observer=false,SlotActive=false;
    int32 Cursor=0;
    TArray<int32> Order={4,0,1,2,3,5,6,7,8};
    FSeigeSimulation Center;
    TSharedPtr<FSeigeScenarioAI> CenterBrain;
    TArray<FSeigeNeighbor> Neighbors;
    FSeigeNeighbor ActiveNeighbor;
};

namespace
{
bool CanApplyDisplayPreferences()
{
    const TCHAR* Command=FCommandLine::Get();
    return FApp::CanEverRender()&&!FParse::Param(Command,TEXT("ForceRes"))&&!FParse::Param(Command,TEXT("UiSmoke"))&&!FParse::Param(Command,TEXT("WorldReview"))&&!FParse::Param(Command,TEXT("GraphicsBenchmark"));
}
bool CanSaveDisplayPreferences()
{
    return CanApplyDisplayPreferences()&&!FParse::Param(FCommandLine::Get(),TEXT("NoSaveDisplay"));
}
}

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
    if(IsPreparingScenario()&&NewScreen!=TEXT("preparing"))CancelScenarioPreparation();
    if((NewScreen==TEXT("settings")||NewScreen==TEXT("credits"))&&(Screen==TEXT("playing")||Screen==TEXT("landing")))ToggleGameMenu();
    if(NewScreen==TEXT("settings")||NewScreen==TEXT("credits")) ReturnScreen=Screen;
    Screen=NewScreen;
    SelectedBuild.Empty(); SelectedId=0;
}
void ASeigeGameMode::ToggleGameMenu()
{
    if(MenuOpen){ResumeGameMenu();return;}
    if(Screen!=TEXT("playing")&&Screen!=TEXT("landing"))return;
    CancelRoadTool();CancelWallTool();SelectedRoadId=0;
    MenuReturnScreen=Screen;PauseBeforeMenu=Paused;MenuOpen=true;Paused=true;
    Screen=TEXT("game-menu");SelectedBuild.Empty();SelectedId=0;
}
void ASeigeGameMode::ResumeGameMenu()
{
    if(!MenuOpen)return;
    Screen=MenuReturnScreen;Paused=PauseBeforeMenu;MenuOpen=false;
}
void ASeigeGameMode::ReturnToMainMenu()
{
    if(IsPreparingScenario())CancelScenarioPreparation();
    ExitCompanionView();SelectedCompanionId=0;
    CancelRoadTool();CancelWallTool();SelectedRoadId=0;
    MenuOpen=false;
    Screen=TEXT("main"); SelectedBuild.Empty(); SelectedId=0;
    CameraCenter=FVector::ZeroVector;Zoom=DefaultZoom;
    CameraYaw=GetDefault<ASeigeGameMode>()->CameraYaw;CameraPitch=GetDefault<ASeigeGameMode>()->CameraPitch;
    // Restore the authored opening composition rather than inheriting a map or
    // close inspection camera. Read only the angles; do not reload scene assets.
    FString Json;TSharedPtr<FJsonObject> Graphics;
    if(FFileHelper::LoadFileToString(Json,*FPaths::Combine(DataDirectory(TEXT("Graphics")),TEXT("scene.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Graphics)&&Graphics)
    {
        double Angle=0;
        if(Graphics->TryGetNumberField(TEXT("camera_yaw"),Angle)&&FMath::IsFinite(Angle)&&Angle>=-360&&Angle<=360)CameraYaw=Angle;
        if(Graphics->TryGetNumberField(TEXT("camera_pitch"),Angle)&&FMath::IsFinite(Angle)&&Angle>=MinimumCameraPitch&&Angle<=MaximumCameraPitch)CameraPitch=Angle;
    }
    UpdateCamera();
    Notice=Ready?TEXT("Single player runs entirely offline."):(Error.IsEmpty()?TEXT("Game definitions could not be loaded."):Error);
}
void ASeigeGameMode::CycleScenarioSlot(int32 Index)
{
    if(IsPreparingScenario())return;
    if(!ScenarioSlots.IsValidIndex(Index)) return;
    FString& Value=ScenarioSlots[Index];
    if(Index==4) Value=Value==TEXT("player")?TEXT("starting"):Value==TEXT("starting")?TEXT("developed"):TEXT("player");
    else Value=Value==TEXT("empty")?TEXT("starting"):Value==TEXT("starting")?TEXT("developed"):TEXT("empty");
}
void ASeigeGameMode::ToggleScenarioThreat(const FString& Threat)
{
    if(Screen!=TEXT("scenario"))return;
    if(Threat==TEXT("background"))ScenarioBackgroundBugs=!ScenarioBackgroundBugs;
    else if(Threat==TEXT("periodic"))ScenarioPeriodicAttacks=!ScenarioPeriodicAttacks;
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
        if(!NewCenterBrain->Initialize(NewCenter,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),ScenarioSlots[4]==TEXT("developed"),Reason,ScenarioBackgroundBugs,ScenarioPeriodicAttacks)) return false;
    }
    else if(!NewCenter.Initialize(DataDirectory(TEXT("Rules")),Reason,ScenarioBackgroundBugs,ScenarioPeriodicAttacks)) return false;
    for(int32 Index=0;Index<9;Index++)
    {
        if(Index==4||ScenarioSlots[Index]==TEXT("empty")) continue;
        FSeigeNeighbor N; N.Index=Index; N.Type=ScenarioSlots[Index];
        N.Offset=FVector2D(Index%3-1,Index/3-1)*NewCenter.WorldHalfSize*2;
        N.Brain=MakeShared<FSeigeScenarioAI>();
        if(!N.Brain->Initialize(N.Sim,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),N.Type==TEXT("developed"),Reason,ScenarioBackgroundBugs,ScenarioPeriodicAttacks,SeigeSectorResourceSeed(NewCenter.GenerationSeed,Index),N.Offset)) return false;
        NewNeighbors.Add(MoveTemp(N));
    }
    TArray<FSeigeRegionResources> NewEmptyRegions;
    if(!GenerateEmptyRegionResources(NewCenter,ScenarioSlots,NewEmptyRegions,Reason))return false;
    ExitCompanionView();
    Sim=MoveTemp(NewCenter); CenterBrain=MoveTemp(NewCenterBrain); Neighbors=MoveTemp(NewNeighbors); Observer=NewObserver;
    ResetScenarioCalendar();
    EmptyRegionResources=MoveTemp(NewEmptyRegions);ConfigureCombatTerrain();SelectedFleetId=0;FleetOrderActive=false;
    return true;
}
void ASeigeGameMode::StartScenario()
{
    if(!GraphicsSettingsValid){Notice=Error.IsEmpty()?TEXT("Correct Graphics/scene.json and restart the game."):Error;return;}
    if(FApp::CanEverRender()&&ScenarioSlots.Contains(TEXT("developed"))){BeginScenarioPreparation();return;}
    if(IsPreparingScenario())CancelScenarioPreparation();
    if(!InitializeScenario(Error)) { Notice=Error; return; }
    FinishScenarioStart();
}
void ASeigeGameMode::FinishScenarioStart()
{
    for(auto& Pair:Visuals) if(Pair.Value) Pair.Value->Destroy();
    Visuals.Empty();
    for(auto It=Materials.CreateIterator();It;++It)if(It.Key().StartsWith(TEXT("construction_original_")))It.RemoveCurrent();
    CancelRoadTool();CancelWallTool();SelectedRoadId=0;
    Ready=true; SelectedId=0; SelectedBuild.Empty(); WinAcknowledged=false;
    Accumulator=0; Speed=1; Paused=false;MenuOpen=false;
    Screen=Observer?TEXT("playing"):TEXT("landing");
    CameraCenter=FVector(HomePosition(),0); Zoom=DefaultZoom*2;
    Notice=Observer?TEXT("OBSERVATION MODE | AI colonies follow the same industry and defense rules."):TEXT("CHOOSE YOUR COMMAND CENTER | Time is paused. Survey deposits, then click a landing site.");
    ResetSimulationPresentation();CreateLandscape(); SyncVisuals(); UpdateCamera();
}
bool ASeigeGameMode::IsPreparingScenario() const{return ScenarioPreparation.IsValid();}
void ASeigeGameMode::BeginScenarioPreparation()
{
    if(!GraphicsSettingsValid){Notice=Error.IsEmpty()?TEXT("Correct Graphics/scene.json and restart the game."):Error;return;}
    if(IsPreparingScenario())return;
    if(ScenarioSlots.Num()!=9){Notice=TEXT("Scenario requires exactly nine region choices");return;}
    for(int32 I=0;I<9;++I)if(ScenarioSlots[I]!=TEXT("starting")&&ScenarioSlots[I]!=TEXT("developed")&&ScenarioSlots[I]!=(I==4?TEXT("player"):TEXT("empty")))
    {Notice=TEXT("Invalid scenario region choice");return;}
    ScenarioPreparation=MakeShared<FSeigeScenarioPreparation>();auto& P=*ScenarioPreparation;
    P.Slots=ScenarioSlots;P.Background=ScenarioBackgroundBugs;P.Periodic=ScenarioPeriodicAttacks;P.Observer=P.Slots[4]!=TEXT("player");P.ReturnScreen=Screen;
    Screen=TEXT("preparing");Notice=TEXT("Loading established colonies and validating their supplies, workforce and defenses. You can cancel.");
}
void ASeigeGameMode::CancelScenarioPreparation()
{
    if(!ScenarioPreparation)return;
    Screen=ScenarioPreparation->ReturnScreen;ScenarioPreparation.Reset();Notice=TEXT("Scenario preparation cancelled. Your current colony is unchanged.");
}
double ASeigeGameMode::ScenarioPreparationProgress() const
{
    if(!ScenarioPreparation)return 0;const auto& P=*ScenarioPreparation;double Fraction=0;
    if(P.SlotActive&&P.Order.IsValidIndex(P.Cursor))
    {const bool Center=P.Order[P.Cursor]==4;const auto Brain=Center?P.CenterBrain:P.ActiveNeighbor.Brain;const auto& Candidate=Center?P.Center:P.ActiveNeighbor.Sim;if(Brain&&Brain->PreparationLimit()>0)Fraction=FMath::Clamp(Candidate.Time/Brain->PreparationLimit(),0.,1.);}
    return FMath::Clamp((P.Cursor+Fraction)/P.Order.Num(),0.,1.);
}
FString ASeigeGameMode::ScenarioPreparationStatus() const
{
    if(!ScenarioPreparation)return {};const auto& P=*ScenarioPreparation;
    if(!P.Order.IsValidIndex(P.Cursor))return TEXT("Preparing the region map");
    const int32 Index=P.Order[P.Cursor];const auto Brain=Index==4?P.CenterBrain:P.ActiveNeighbor.Brain;
    return FString::Printf(TEXT("Region %d of 9 | %s\n%s"),P.Cursor+1,*P.Slots[Index],Brain?*Brain->GetStatus():TEXT("Loading region definitions"));
}
void ASeigeGameMode::TickScenarioPreparation(double BudgetMilliseconds)
{
    if(!ScenarioPreparation||!FMath::IsFinite(BudgetMilliseconds)||BudgetMilliseconds<=0)return;
    const double Deadline=FPlatformTime::Seconds()+BudgetMilliseconds/1000.;
    auto Fail=[&](const FString& Reason){const FString Back=ScenarioPreparation->ReturnScreen;ScenarioPreparation.Reset();Screen=Back;Error=Reason;Notice=Reason;};
    while(ScenarioPreparation&&FPlatformTime::Seconds()<Deadline)
    {
        auto& P=*ScenarioPreparation;
        if(P.Cursor>=P.Order.Num())
        {
            TArray<FSeigeRegionResources> Empty;FString Reason;
            if(!GenerateEmptyRegionResources(P.Center,P.Slots,Empty,Reason)){Fail(Reason);return;}
            ExitCompanionView();Sim=MoveTemp(P.Center);CenterBrain=MoveTemp(P.CenterBrain);Neighbors=MoveTemp(P.Neighbors);Observer=P.Observer;
            ScenarioSlots=P.Slots;ScenarioBackgroundBugs=P.Background;ScenarioPeriodicAttacks=P.Periodic;EmptyRegionResources=MoveTemp(Empty);
            ResetScenarioCalendar();
            ScenarioPreparation.Reset();ConfigureCombatTerrain();SelectedFleetId=0;FleetOrderActive=false;FinishScenarioStart();return;
        }
        const int32 Index=P.Order[P.Cursor];const FString Type=P.Slots[Index];
        if(Type==TEXT("empty")){++P.Cursor;continue;}
        if(!P.SlotActive)
        {
            FString Reason;
            if(Index==4&&Type==TEXT("player"))
            {if(!P.Center.Initialize(DataDirectory(TEXT("Rules")),Reason,P.Background,P.Periodic)){Fail(Reason);return;}++P.Cursor;continue;}
            if(Index==4)P.CenterBrain=MakeShared<FSeigeScenarioAI>();
            else{P.ActiveNeighbor=FSeigeNeighbor();P.ActiveNeighbor.Index=Index;P.ActiveNeighbor.Type=Type;P.ActiveNeighbor.Offset=FVector2D(Index%3-1,Index/3-1)*P.Center.WorldHalfSize*2;P.ActiveNeighbor.Brain=MakeShared<FSeigeScenarioAI>();}
            auto Brain=Index==4?P.CenterBrain:P.ActiveNeighbor.Brain;auto& Candidate=Index==4?P.Center:P.ActiveNeighbor.Sim;
            if(!Brain->BeginInitialize(Candidate,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),Type==TEXT("developed"),Reason,P.Background,P.Periodic,Index==4?INDEX_NONE:SeigeSectorResourceSeed(P.Center.GenerationSeed,Index),Index==4?FVector2D::ZeroVector:P.ActiveNeighbor.Offset)){Fail(Reason);return;}
            P.SlotActive=true;
        }
        auto Brain=Index==4?P.CenterBrain:P.ActiveNeighbor.Brain;auto& Candidate=Index==4?P.Center:P.ActiveNeighbor.Sim;bool Complete=!Brain->IsPreparing();FString Reason;
        const double Remaining=(Deadline-FPlatformTime::Seconds())*1000.;
        if(!Complete&&Remaining<=0)return;
        if(!Complete&&!Brain->AdvancePreparation(Candidate,Remaining,Complete,Reason)){Fail(Reason);return;}
        if(!Complete)return;
        if(Index!=4)P.Neighbors.Add(MoveTemp(P.ActiveNeighbor));P.SlotActive=false;++P.Cursor;
    }
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
    Notice=TEXT("SHUTTLE LANDING | Workers are deploying the command center. Time is running.");
    ResetSimulationPresentation();CreateLandscape(); SyncVisuals(); UpdateCamera();
}
void ASeigeGameMode::SetGraphicsQuality(int32 Quality)
{
    GraphicsQuality=1;ApplyMediumPreset();
}
void ASeigeGameMode::SetFullscreen(bool Enabled)
{
    Fullscreen=Enabled;
    if(!CanApplyDisplayPreferences())return;
    if(auto* Settings=UGameUserSettings::GetGameUserSettings())
    {
        Settings->SetFullscreenMode(Enabled?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);
        const FIntPoint Desktop=Settings->GetDesktopResolution();
        Settings->SetScreenResolution(Enabled&&Desktop.X>0&&Desktop.Y>0?Desktop:WindowResolution);
        Settings->ApplyResolutionSettings(false);Settings->ConfirmVideoMode();
        Settings->SetResolutionScaleValueEx(RenderResolutionPercent);Settings->ApplyNonResolutionSettings();ApplyMediumPreset();if(CanSaveDisplayPreferences())Settings->SaveSettings();
    }
}
void ASeigeGameMode::InitializeDisplaySettings()
{
    auto* Settings=UGameUserSettings::GetGameUserSettings();if(!Settings)return;
    int32 Version=0;GConfig->GetInt(TEXT("Seige.Display"),TEXT("Version"),Version,GGameUserSettingsIni);
    GConfig->GetInt(TEXT("Seige.Display"),TEXT("WindowWidth"),WindowResolution.X,GGameUserSettingsIni);
    GConfig->GetInt(TEXT("Seige.Display"),TEXT("WindowHeight"),WindowResolution.Y,GGameUserSettingsIni);
    if(WindowResolution.X<960||WindowResolution.Y<540)WindowResolution=FIntPoint(1600,900);
    float Normalized=1,Minimum=50,Maximum=100;
    Settings->GetResolutionScaleInformationEx(Normalized,RenderResolutionPercent,Minimum,Maximum);
    RenderResolutionPercent=Version<1?100:FMath::Clamp(RenderResolutionPercent,50.f,100.f);
    Fullscreen=Version<1||Settings->GetFullscreenMode()!=EWindowMode::Windowed;
    ApplyMediumPreset();GraphicsQuality=1;
    // Automated capture sizes remain explicit; a normal launch adopts native
    // borderless on first use and never enters exclusive fullscreen.
    if(CanApplyDisplayPreferences())
    {
        SetFullscreen(Fullscreen);
        if(CanSaveDisplayPreferences()){GConfig->SetInt(TEXT("Seige.Display"),TEXT("Version"),1,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
    }
}
void ASeigeGameMode::SetRenderResolutionPercent(float Percent)
{
    if(!FMath::IsFinite(Percent))return;
    RenderResolutionPercent=FMath::Clamp(Percent,50.f,100.f);
    if(CanApplyDisplayPreferences())if(auto* Settings=UGameUserSettings::GetGameUserSettings())
    {Settings->SetResolutionScaleValueEx(RenderResolutionPercent);Settings->ApplyNonResolutionSettings();ApplyMediumPreset();if(CanSaveDisplayPreferences())Settings->SaveSettings();}
}
void ASeigeGameMode::CycleWindowResolution(int32 Direction)
{
    if(Fullscreen)return;
    const FIntPoint Choices[]={FIntPoint(1280,720),FIntPoint(1600,900),FIntPoint(1920,1080),FIntPoint(2560,1440),FIntPoint(3840,2160)};
    FIntPoint Desktop(3840,2160);if(auto* Settings=UGameUserSettings::GetGameUserSettings()){const auto Size=Settings->GetDesktopResolution();if(Size.X>0&&Size.Y>0)Desktop=Size;}
    TArray<FIntPoint> Available;for(const auto& Choice:Choices)if(Choice.X<=Desktop.X&&Choice.Y<=Desktop.Y)Available.Add(Choice);
    if(Available.IsEmpty())Available.Add(Desktop);
    int32 Current=Available.IndexOfByKey(WindowResolution);if(Current==INDEX_NONE)Current=0;
    WindowResolution=Available[(Current+(Direction<0?-1:1)+Available.Num())%Available.Num()];
    if(CanApplyDisplayPreferences())
    {if(CanSaveDisplayPreferences()){GConfig->SetInt(TEXT("Seige.Display"),TEXT("WindowWidth"),WindowResolution.X,GGameUserSettingsIni);GConfig->SetInt(TEXT("Seige.Display"),TEXT("WindowHeight"),WindowResolution.Y,GGameUserSettingsIni);}SetFullscreen(false);if(CanSaveDisplayPreferences())GConfig->Flush(false,GGameUserSettingsIni);}
}
FIntPoint ASeigeGameMode::EffectiveRenderResolution() const
{
    const FIntPoint Size=DisplayResolution();
    return FIntPoint(FMath::RoundToInt(Size.X*RenderResolutionPercent/100.f),FMath::RoundToInt(Size.Y*RenderResolutionPercent/100.f));
}
FIntPoint ASeigeGameMode::DisplayResolution() const
{
    FIntPoint Size=WindowResolution;
    if(const auto* PC=UGameplayStatics::GetPlayerController(this,0)){int32 X=0,Y=0;PC->GetViewportSize(X,Y);if(X>0&&Y>0)Size=FIntPoint(X,Y);}
    return Size;
}
void ASeigeGameMode::CycleGameSpeed(int32 Direction)
{
    if(CompanionView){Speed=1;Notice=TEXT("Rex roams at 1x. Press Esc to return to colony speed controls.");return;}
    if(GameSpeeds.IsEmpty())return;
    // Pause is a selectable step, while Speed always retains the last running
    // rate so Space can resume it and existing saves keep a nonzero speed.
    int32 Index=Paused?0:GameSpeeds.IndexOfByKey(FMath::RoundToInt(Speed))+1;
    const int32 Count=GameSpeeds.Num()+1;
    Index=(Index+(Direction<0?-1:1)+Count)%Count;
    Paused=Index==0;
    if(!Paused)Speed=GameSpeeds[Index-1];
}
bool ASeigeGameMode::IsSupportedGameSpeed(double Value) const
{
    return FMath::IsFinite(Value)&&Value>=1&&Value<=10&&Value==FMath::FloorToDouble(Value)&&GameSpeeds.Contains(static_cast<int32>(Value));
}

void ASeigeGameMode::ResetScenarioCalendar()
{
    ScenarioCalendar.Configure(Sim.Calendar.GetRules());
    BindScenarioCalendar();
    Sim.Energy.Tick(Sim,0);
    for(auto& N:Neighbors)N.Sim.Energy.Tick(N.Sim,0);
}
void ASeigeGameMode::BindScenarioCalendar()
{
    Sim.Calendar.SetElapsedMicroseconds(ScenarioCalendar.ElapsedMicroseconds());
    for(auto& N:Neighbors)N.Sim.Calendar.SetElapsedMicroseconds(ScenarioCalendar.ElapsedMicroseconds());
}
FString ASeigeGameMode::CalendarLabel() const
{
    const auto Sample=ScenarioCalendar.Sample();
    const TCHAR* Names[]={TEXT("Spring"),TEXT("Summer"),TEXT("Autumn"),TEXT("Winter")};
    const int32 Minute=FMath::FloorToInt(Sample.CycleFraction*1440.);
    return FString::Printf(TEXT("%s %d | %02d:%02d | %s"),Names[Sample.SeasonIndex],Sample.DayOfSeason,(Minute/60+6)%24,Minute%60,Sample.IsDay?TEXT("Daylight"):TEXT("Night"));
}
