#include "SeigeGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

// Opt-in visual/lifecycle review. Initial landing and construction run through the
// ordinary simulation. Later calendar jumps are explicitly visual-only fixtures;
// this harness never opens or changes a player's save.
void ASeigeGameMode::RunWorldReview()
{
    if(FParse::Param(FCommandLine::Get(),TEXT("TerrainReview")))
    {
        static TWeakObjectPtr<ASeigeGameMode> TerrainReviewOwner;static int32 Step=0;static double Since=0;static bool Requested=false;static TArray<FString> Shots,Failures;
        if(TerrainReviewOwner.Get()!=this){TerrainReviewOwner=this;Step=0;Since=RenderClock;Requested=false;Shots.Reset();Failures.Reset();}
        if(!Ready||RenderClock-Since<2||Step>17)return;
        auto Frame=[&](int32 Sector,FVector2D Center,float Distance,float Pitch,float Yaw)
        {FocusSector(Sector);CameraCenter=FVector(Center,0);Zoom=Distance;RenderedZoom=Distance;CameraPitch=Pitch;CameraYaw=Yaw;UpdateCamera();RefreshEnvironment();};
        auto Capture=[&](const TCHAR* Name)
        {
            if(!IsSceneryStreamingReady()&&RenderClock-Since<90)return false;
            if(!IsSceneryStreamingReady())Failures.Add(FString(TEXT("Scenery did not settle: "))+Name);
            const FString Folder=FPaths::ProjectSavedDir()/TEXT("Screenshots/Terrain-v0106");IFileManager::Get().MakeDirectory(*Folder,true);
            FScreenshotRequest::RequestScreenshot(Folder/(FString(Name)+TEXT(".png")),true,false);Shots.Add(Name);
            UE_LOG(LogTemp,Display,TEXT("TERRAIN_REVIEW_CAPTURE %s"),Name);return true;
        };
        if(Step==0)
        {
            if(!Requested)
            {
                ScenarioSlots.Init(TEXT("empty"),9);ScenarioSlots[4]=TEXT("player");ScenarioSeedOverride=815;
                ScenarioBackgroundBugs=ScenarioPeriodicAttacks=false;StartScenario();Requested=true;Since=RenderClock;return;
            }
            if(Screen!=TEXT("landing"))return;
            // Scenario creation completes asynchronously and resets observer /
            // camera state. Only frame views after that initialization is done.
            Observer=true;Screen=TEXT("playing");Paused=true;Notice.Empty();
            ScenarioCalendar.SetElapsedMicroseconds(900000000);BindScenarioCalendar();UpdateWeather(0);
            Frame(4,FVector2D(14000,-8500),16000,56,135);
        }
        if(Step==1&&!Capture(TEXT("01_irregular_lake_valley")))return;
        if(Step==2)Frame(4,FVector2D(14800,14500),12000,48,155);
        if(Step==3&&!Capture(TEXT("02_stream_in_lower_ground")))return;
        if(Step==4)Frame(4,FVector2D(-13500,9000),18500,32,113);
        if(Step==5&&!Capture(TEXT("03_fractured_escarpment")))return;
        if(Step==6)Frame(6,FVector2D(-54000,49000),18000,56,135);
        if(Step==7&&!Capture(TEXT("04_neighbor_lake")))return;
        if(Step==8){RegionMapZoom=350000;Frame(4,FVector2D::ZeroVector,260000,80,90);}
        if(Step==9&&!Capture(TEXT("05_neighborhood_drainage")))return;
        if(Step==10)Frame(4,FVector2D(14000,0),2000,28,160);
        if(Step==11&&!Capture(TEXT("06_shallow_ford_oblique")))return;
        if(Step==12)Frame(4,FVector2D(15000,14000),1100,25,180);
        if(Step==13&&!Capture(TEXT("07_stream_valley_oblique")))return;
        if(Step==14)Frame(4,FVector2D(-13500,8500),2400,25,113);
        if(Step==15&&!Capture(TEXT("08_scarp_face_oblique")))return;
        if(Step==17)
        {
            auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("scope"),TEXT("Rendered geography-only fixture: paused, no player saves, three full-region river valleys and irregular lake/scarp views, plus close oblique ford/stream/scarp detail."));
            Report->SetNumberField(TEXT("failures"),Failures.Num());TArray<TSharedPtr<FJsonValue>> I,F;
            for(const auto& V:Shots)I.Add(MakeShared<FJsonValueString>(V));for(const auto& V:Failures)F.Add(MakeShared<FJsonValueString>(V));Report->SetArrayField(TEXT("screenshots"),I);Report->SetArrayField(TEXT("assertion_failures"),F);
            FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));FFileHelper::SaveStringToFile(Json,*(FPaths::ProjectSavedDir()/TEXT("TerrainReview-v0106.json")));
            FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()?0:1);
        }
        ++Step;Since=RenderClock;return;
    }
    static TWeakObjectPtr<ASeigeGameMode> ReviewOwner;
    static int32 Stage=0;static double Since=0;static TArray<FString> Failures,Images,InitialIds;
    if(ReviewOwner.Get()!=this){ReviewOwner=this;Stage=0;Since=RenderClock;Failures.Reset();Images.Reset();InitialIds.Reset();}
    if(Stage>37||RenderClock-Since<1.5||(Stage==0&&RenderClock-Since<30))return;
    auto Check=[&](bool Good,const TCHAR* Why){if(!Good){Failures.Add(Why);UE_LOG(LogTemp,Error,TEXT("WORLD_REVIEW %s"),Why);}};
    auto Capture=[&](const TCHAR* Name)
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Review-v09"));
        IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,FString(Name)+TEXT(".png")),true,false);Images.Add(Name);
        UE_LOG(LogTemp,Display,TEXT("WORLD_REVIEW_CAPTURE stage=%d %s age=%.2f"),Stage,Name,Sim.Time);
    };
    auto Core=[&](){return Sim.Buildings.FindByPredicate([&](const FSeigeBuilding& B){return B.DefId==Sim.CoreDefinition&&B.Health>0;});};
    auto* PC=UGameplayStatics::GetPlayerController(this,0);auto* HUD=PC?Cast<ASeigeHUD>(PC->GetHUD()):nullptr;
    auto Frame=[&](FVector2D Center,float Distance,float Pitch=52){CameraCenter=FVector(Center,0);Zoom=Distance;CameraPitch=Pitch;CameraYaw=135;UpdateCamera();RefreshEnvironment();};
    auto Date=[&](int32 Day,double Fraction)
    {
        const auto& R=ScenarioCalendar.GetRules();ScenarioCalendar.SetElapsedMicroseconds(Day*(R.DaylightMicroseconds+R.NightMicroseconds)+int64(Fraction*(R.DaylightMicroseconds+R.NightMicroseconds)));
        BindScenarioCalendar();UpdateWeather(0);
    };
    // Capture and mutation use different frames: screenshot requests are fulfilled
    // at end-of-frame, after HUD rendering. Do not close a panel in its capture stage.
    if(Stage==0){Check(Ready,TEXT("Game definitions load"));Capture(TEXT("01_main_menu"));}
    if(Stage==1)ShowScreen(TEXT("scenario"));
    if(Stage==2)Capture(TEXT("02_scenario"));
    if(Stage==3){ScenarioSlots.Init(TEXT("empty"),9);ScenarioSlots[4]=TEXT("player");ScenarioSeedOverride=815;ScenarioBackgroundBugs=ScenarioPeriodicAttacks=false;StartScenario();}
    if(Stage==4){Check(Screen==TEXT("landing"),TEXT("Human scenario starts paused at site selection"));Capture(TEXT("03_site_survey"));}
    if(Stage==5)
    {
        FString Why;FVector2D Landing=FVector2D::ZeroVector;bool Found=CanLand(Landing,Why);
        for(int32 I=1;I<=16&&!Found;++I){Landing=FVector2D(I*450,0);Found=CanLand(Landing,Why);}
        Check(Found,TEXT("Find a valid dry landing plot"));if(Found)ConfirmLanding(Landing);
        Speed=1;Paused=false;Frame(Landing,6000);for(const auto& W:Sim.Workers.Bodies)InitialIds.Add(W.Id);
    }
    if(Stage==6){Capture(TEXT("04_vertical_descent"));Check(!Sim.DeploymentGrounded,TEXT("Shuttle still in vertical descent"));}
    if(Stage==7)
    {
        if(Sim.DeploymentElapsed<23&&RenderClock-Since<50)return;
        const auto* Visible=Sim.Workers.Bodies.FindByPredicate([&](const auto& W){return W.State==TEXT("active")&&W.Outdoor&&FVector2D::Distance(W.Position,HomePosition())>Sim.BuildingDefs[Sim.CoreDefinition].Footprint*.9;});
        if(!Visible&&RenderClock-Since<65)return;
        Check(Sim.DeploymentHatchOpen,TEXT("Landing opens physical cargo hatch"));Check(Visible!=nullptr,TEXT("A real worker physically exits the shuttle into view"));
        Paused=true;if(Visible){Frame(Visible->Position,280,25);CameraYaw=180;UpdateCamera();}
    }
    if(Stage==8)Capture(TEXT("05_workers_exiting"));
    if(Stage==9){Paused=false;Speed=10;Frame(HomePosition(),6000);}
    if(Stage==10)Speed=10;
    if(Stage==11)
    {
        // Completion does not teleport builders into operating jobs. Keep the
        // ordinary simulation running until a real operator reaches the core.
        if(Core()&&(Core()->IsConstructing||Core()->Workers<1||Sim.Energy.Info(Sim,Core()->Id).GenerationKW<=0)&&RenderClock-Since<900&&!Sim.Failed)return;
        Check(Core()&&!Core()->IsConstructing,TEXT("Finite real workers finish command deployment at10x"));
        Check(Core()&&Core()->Workers>=1&&Sim.Energy.Info(Sim,Core()->Id).GenerationKW>0,TEXT("Deployed fusion core has a physically arrived operator and generates power"));
        int32 Expected=0,Drawn=0;for(const auto& W:Sim.Workers.Bodies)if(W.State==TEXT("active")&&W.Outdoor){++Expected;const auto* A=Visuals.FindRef(TEXT("home_worker_")+W.Id).Get();if(A&&!A->IsHidden())++Drawn;}
        Check(Expected==Drawn,TEXT("Every outdoor worker has exactly its persistent body actor"));
        for(const auto& Pair:Visuals)Check(!Pair.Key.StartsWith(TEXT("home_courier_"))&&!Pair.Key.Contains(TEXT("_builder_"))&&!Pair.Key.Contains(TEXT("_operator_")),TEXT("No phantom aggregate worker actors"));
        for(const auto& Id:InitialIds)Check(Sim.Workers.Find(Id)!=nullptr,TEXT("Initial worker identities survive deployment"));
        Paused=true;Date(0,.25);
    }
    if(Stage==12)Capture(TEXT("06_deployed_colony"));
    if(Stage==13){if(HUD)HUD->ProcessShortcut(EKeys::B,*this);}
    if(Stage==14)Capture(TEXT("07_build_portraits"));
    if(Stage==15){if(HUD)HUD->ProcessShortcut(EKeys::B,*this);SelectedId=Core()?Core()->Id:0;}
    if(Stage==16)
    {
        bool Clicked=false;
        if(HUD)for(const auto& R:HUD->Ui.HitRegions)if(R.Action==TEXT("info-section:Weapons")){HUD->ProcessClick((R.Position.X+R.Size.X*.5)*HUD->Ui.Scale,(R.Position.Y+R.Size.Y*.5)*HUD->Ui.Scale,*this);Clicked=true;break;}
        Check(Clicked,TEXT("Command center exposes weapons dossier"));
    }
    if(Stage==17)Capture(TEXT("08_command_weapons"));
    if(Stage==18){SelectedId=0;Date(0,.75);}
    if(Stage==19){Check(!ScenarioCalendar.Sample().IsDay,TEXT("Night fixture uses night solar phase"));Capture(TEXT("09_night"));}
    if(Stage==20)Date(100,.25);
    if(Stage==21){Check(SnowCoverage()>.5&&Snowflakes&&Snowflakes->GetInstanceCount()==SnowflakeCount,TEXT("Midwinter has snow cover and bounded snowfall"));Capture(TEXT("10_winter"));}
    if(Stage==22){Observer=true;Date(0,.25);Frame(FVector2D(14000,-8500),13000,55);}
    if(Stage==23){if(!IsSceneryStreamingReady()&&RenderClock-Since<60)return;Check(IsSceneryStreamingReady(),TEXT("Lake scenery became ready within 60 seconds"));Capture(TEXT("11_lake_and_stream"));}
    if(Stage==24)Frame(FVector2D(-13500,9000),11500,38);
    if(Stage==25){if(!IsSceneryStreamingReady()&&RenderClock-Since<60)return;Check(IsSceneryStreamingReady(),TEXT("Cliff scenery became ready within 60 seconds"));Capture(TEXT("12_cliffs"));}
    if(Stage==26){if(!Sim.Nodes.IsEmpty())Frame(Sim.Nodes[0].Position,2100,55);}
    if(Stage==27)
    {
        if(!IsSceneryStreamingReady()&&RenderClock-Since<60)return;
        Check(IsSceneryStreamingReady(),TEXT("Deposit scenery became ready within 60 seconds"));
        int32 Outcrops=0;
        if(Foliage){TArray<UInstancedStaticMeshComponent*> Sets;Foliage->GetComponents(Sets);for(const auto* Set:Sets)if(Set->ComponentHasTag(TEXT("seige_deposit")))Outcrops+=Set->GetInstanceCount();}
        Check(Outcrops>0,TEXT("Revealed deposits acquire physical outcrop instances without a sector reload"));
        Capture(TEXT("13_deposit_geology"));
    }
    if(Stage==28){Observer=false;Frame(HomePosition(),6000);ToggleGameMenu();}
    if(Stage==29)Capture(TEXT("14_pause_menu"));
    if(Stage==30)ToggleGameMenu();
    if(Stage==31)Capture(TEXT("15_final_colony"));
    if(Stage==32){Frame(HomePosition()+FVector2D(170,0),600,25);}
    if(Stage==33)Capture(TEXT("16_cargo_hatch"));
    if(Stage==34){Frame(HomePosition(),2600,45);}
    if(Stage==35)Capture(TEXT("17_shuttle_closeup"));
    if(Stage==36){Frame(HomePosition(),6000);}
    if(Stage==37)
    {
        auto Report=MakeShared<FJsonObject>();Report->SetStringField(TEXT("scope"),TEXT("Opt-in rendered lifecycle/UI/environment review. Weather phases are isolated visual fixtures, not a 100-day gameplay soak."));
        Report->SetNumberField(TEXT("stages"),38);Report->SetNumberField(TEXT("failures"),Failures.Num());Report->SetNumberField(TEXT("colony_simulation_seconds"),Sim.Time);
        TArray<TSharedPtr<FJsonValue>> F,I;for(const auto& V:Failures)F.Add(MakeShared<FJsonValueString>(V));for(const auto& V:Images)I.Add(MakeShared<FJsonValueString>(V));
        Report->SetArrayField(TEXT("assertion_failures"),F);Report->SetArrayField(TEXT("screenshots"),I);FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json));
        FFileHelper::SaveStringToFile(Json,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("WorldReview-v09.json")));
        UE_LOG(LogTemp,Display,TEXT("WORLD_REVIEW_COMPLETE failures=%d"),Failures.Num());FPlatformMisc::RequestExitWithStatus(false,Failures.IsEmpty()?0:1);
    }
    ++Stage;Since=RenderClock;
}
