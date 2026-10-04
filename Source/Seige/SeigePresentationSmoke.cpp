#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HighResScreenshot.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Camera/PlayerCameraManager.h"

// Explicit opt-in render/interaction smoke. It never loads or writes player saves.
void ASeigeGameMode::RunPresentationSmoke()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("UiSmoke")))return;
    static const double Times[]={2,3,4,5,7,8,10,11,12,13,14,15,17,18,19,20,21,22,23,24,26,28,30,32,34,36,38,40};
    if(PresentationSmokeStage>=UE_ARRAY_COUNT(Times)||RenderClock<Times[PresentationSmokeStage])return;
    const int32 Stage=PresentationSmokeStage++;
    auto* Controller=Cast<ASeigeController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Hud=Controller?Cast<ASeigeHUD>(Controller->GetHUD()):nullptr;
    auto Require=[&](bool Condition,const TCHAR* Message)
    {
        if(!Condition){++SmokeFailures;UE_LOG(LogTemp,Error,TEXT("UI_SMOKE_ASSERT stage=%d: %s"),Stage,Message);}
        return Condition;
    };
    auto Capture=[&](const TCHAR* Name)
    {
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Review-v03"));
        if(!Require(IFileManager::Get().MakeDirectory(*Directory,true),TEXT("Could not create screenshot directory")))return;
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,FString(Name)+TEXT(".png")),true,false);
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_CAPTURE %s stage=%d screen=%s"),Name,Stage,*Screen);
    };
    auto ClickAction=[&](const FString& Action)
    {
        if(!Require(Controller&&Hud,TEXT("Player controller or HUD is missing")))return false;
        const auto* Region=Hud->Ui.HitRegions.FindByPredicate([&](const FSeigeButton& R){return R.Action==Action;});
        if(!Region){++SmokeFailures;UE_LOG(LogTemp,Error,TEXT("UI_SMOKE_ASSERT stage=%d: Missing cached action %s on %s"),Stage,*Action,*Screen);return false;}
        const FVector2D Point=(Region->Position+Region->Size*.5)*Hud->Ui.Scale;
        Controller->HandlePrimaryClick(Point.X,Point.Y);
        return true;
    };
    auto WorldClick=[&](FVector2D Position)
    {
        if(!Require(Controller&&Hud,TEXT("Cannot route a world click without controller and HUD")))return;
        UpdateCamera();if(Controller->PlayerCameraManager)Controller->PlayerCameraManager->UpdateCamera(0);
        FVector2D Pixel;
        if(!Require(Controller->ProjectWorldLocationToScreen(RenderPosition(Position),Pixel),TEXT("Ground target could not project to screen")))return;
        const bool Hit=Controller->UpdateCursorFromScreen(Pixel.X,Pixel.Y);
        FVector RayOrigin=FVector::ZeroVector,RayDirection=FVector::ZeroVector;Controller->ScreenRay(Pixel,RayOrigin,RayDirection);
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_PICK stage=%d screen=%s target=%s pixel=%s hit=%d cursor=%s delta=%.8f origin=%s direction=%s zoom=%.2f yaw=%.2f pitch=%.2f ui=%s"),
            Stage,*Screen,*Position.ToString(),*Pixel.ToString(),Hit?1:0,*CursorWorld.ToString(),Hit?FVector2D::Distance(CursorWorld,Position):-1.,*RayOrigin.ToString(),*RayDirection.ToString(),Zoom,CameraYaw,CameraPitch,*Hud->Ui.HitTest(Pixel.X,Pixel.Y));
        if(!Require(Hit,TEXT("Perspective screen ray did not hit terrain")))return;
        if(!Require(FVector2D::Distance(CursorWorld,Position)<.2,TEXT("Perspective terrain pick did not roundtrip")))return;
        Controller->HandlePrimaryClick(Pixel.X,Pixel.Y);
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_CLICK stage=%d screen=%s cursor=%s buildings=%d selected=%d notice=%s"),Stage,*Screen,*CursorWorld.ToString(),Sim.Buildings.Num(),SelectedId,*Notice);
    };
    UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_STAGE %d t=%.2f screen=%s"),Stage,RenderClock,*Screen);
    switch(Stage)
    {
    case 0:
        Require(Screen==TEXT("main"),TEXT("Application did not open at the main menu"));Capture(TEXT("main"));break;
    case 1:
        ClickAction(TEXT("screen:scenario"));Require(Screen==TEXT("scenario"),TEXT("Single-player action did not open scenario setup"));break;
    case 2: Capture(TEXT("scenario"));break;
    case 3:
        ClickAction(TEXT("start-scenario"));Require(Screen==TEXT("landing")&&Ready,TEXT("Human scenario did not enter ready landing mode"));break;
    case 4:
        Require(Sim.Time==0,TEXT("Scenario time advanced before human landing"));Capture(TEXT("landing"));break;
    case 5:
        WorldClick(FVector2D::ZeroVector);Require(Screen==TEXT("playing")&&!Observer,TEXT("Core world click did not start human play"));break;
    case 6: Capture(TEXT("play"));break;
    case 7:
        if(Require(Hud!=nullptr,TEXT("Construction shortcut requires HUD")))Hud->HandleShortcut(EKeys::B);
        Require(Hud&&Hud->Ui.BuildOpen,TEXT("B did not open construction catalog"));break;
    case 8: Capture(TEXT("build"));break;
    case 9:
        if(Require(Hud!=nullptr,TEXT("Building shortcuts require HUD"))){Hud->HandleShortcut(EKeys::L);Hud->HandleShortcut(EKeys::S);}
        Require(!SelectedBuild.IsEmpty()&&Hud&&!Hud->Ui.BuildOpen,TEXT("Category/build shortcut chain did not select a blueprint"));break;
    case 10:
        WorldClick(FVector2D(700,0));Require(Sim.Buildings.Num()==2,TEXT("World click did not construct the selected sensor"));
        if(Sim.Buildings.Num()==2)Require(Sim.Definition(Sim.Buildings.Last())&&Sim.Definition(Sim.Buildings.Last())->Role==TEXT("sensor"),TEXT("Shortcut constructed the wrong building role"));break;
    case 11:
    {
        Require(Sim.Buildings.Num()==2,TEXT("Constructed building disappeared before capture"));
        SelectedBuild.Empty();CameraYaw=35;CameraPitch=25;Zoom=1700;UpdateCamera();
        if(Controller&&Controller->PlayerCameraManager)Controller->PlayerCameraManager->UpdateCamera(0);
        FVector2D RoofPixel;
        if(Require(Controller&&Controller->ProjectWorldLocationToScreen(RenderPosition(HomePosition(),1000),RoofPixel),TEXT("Could not project the visible command-center upper floor")))
        {
            FVector RayOrigin,RayDirection;
            Require(Controller->ScreenRay(RoofPixel,RayOrigin,RayDirection)&&SelectBuildingRay(RayOrigin,RayDirection),TEXT("Visible building geometry did not answer a selection ray"));
            SelectedId=0;Controller->HandlePrimaryClick(RoofPixel.X,RoofPixel.Y);Require(SelectedId==Sim.Buildings[0].Id,TEXT("Clicking the tall command center at low tilt did not select it"));
        }
        SelectedId=0;CameraYaw=135;CameraPitch=52;Zoom=DefaultZoom;UpdateCamera();Capture(TEXT("placed"));break;
    }
    case 12: Zoom=Sim.WorldHalfSize*12;UpdateCamera();break;
    case 13: Capture(TEXT("overview"));break;
    case 14: ShowScreen(TEXT("credits"));break;
    case 15:
        Require(Screen==TEXT("credits"),TEXT("Credits screen was not displayed"));Capture(TEXT("credits"));break;
    case 16: ReturnToMainMenu();break;
    case 17:
        ClickAction(TEXT("screen:scenario"));Require(Screen==TEXT("scenario"),TEXT("Scenario setup could not be reopened"));break;
    case 18:
        ClickAction(TEXT("slot:4"));ClickAction(TEXT("slot:4"));
        ClickAction(TEXT("slot:0"));ClickAction(TEXT("slot:0"));ClickAction(TEXT("slot:2"));
        Require(ScenarioSlots.Num()==9&&ScenarioSlots[4]==TEXT("developed")&&ScenarioSlots[0]==TEXT("developed")&&ScenarioSlots[2]==TEXT("starting"),TEXT("Scenario cell clicks did not configure the requested AI types"));break;
    case 19:
        ClickAction(TEXT("start-scenario"));Require(Ready&&Observer&&Screen==TEXT("playing")&&Neighbors.Num()==2,TEXT("AI scenario did not start observation with two neighbors"));break;
    case 20: Zoom=5500;UpdateCamera();break;
    case 21:
        Require(Observer&&Neighbors.Num()==2,TEXT("Observer scenario state changed unexpectedly"));Capture(TEXT("observer"));break;
    case 22: Zoom=1300;CameraYaw=35;CameraPitch=35;UpdateCamera();break;
    case 23: Capture(TEXT("closeup"));WorldClick(HomePosition());break;
    case 24: CameraYaw=220;CameraPitch=65;Zoom=3000;UpdateCamera();break;
    case 25: Capture(TEXT("rotated"));WorldClick(HomePosition());break;
    case 26: CameraYaw=135;CameraPitch=52;Zoom=DefaultZoom;UpdateCamera();break;
    case 27:
    {
        Require(Ready&&Observer&&Neighbors.Num()==2&&Screen==TEXT("playing"),TEXT("Final observer state is invalid"));
        const float Dt=GetWorld()?GetWorld()->GetDeltaSeconds():0;
        auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("ready"),Ready);Report->SetNumberField(TEXT("failures"),SmokeFailures);
        Report->SetBoolField(TEXT("observer"),Observer);Report->SetNumberField(TEXT("neighbors"),Neighbors.Num());Report->SetNumberField(TEXT("buildings"),Sim.Buildings.Num());
        Report->SetNumberField(TEXT("fps"),Dt>0?1.0/Dt:0);Report->SetNumberField(TEXT("presentation_seconds"),RenderClock);Report->SetNumberField(TEXT("simulation_seconds"),Sim.Time);
        Report->SetStringField(TEXT("screen"),Screen);Report->SetNumberField(TEXT("completed_stages"),PresentationSmokeStage);
        FString Json;const FString Filename=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("PresentationSmoke.json"));
        if(!FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json))||!FFileHelper::SaveStringToFile(Json,*Filename,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {++SmokeFailures;UE_LOG(LogTemp,Error,TEXT("UI_SMOKE_ASSERT could not save presentation report"));}
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_COMPLETE failures=%d observer=%d neighbors=%d buildings=%d"),SmokeFailures,Observer?1:0,Neighbors.Num(),Sim.Buildings.Num());
        FPlatformMisc::RequestExitWithStatus(false,SmokeFailures==0?0:1);break;
    }
    default: break;
    }
}
