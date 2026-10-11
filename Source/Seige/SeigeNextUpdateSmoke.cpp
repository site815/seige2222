#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "Engine/World.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "HighResScreenshot.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

// Explicit, bounded presentation fixture. This does not load/write player saves,
// simulate a long bootstrap, or replace the full gameplay acceptance route.
void ASeigeGameMode::RunNextUpdateSmoke()
{
    static TWeakObjectPtr<ASeigeGameMode> SmokeOwner;
    static int32 Stage=0,Assertions=0,FactoryId=0,PortId=0,TradeSteps=0;
    static double Started=0,Since=0,InitialTime=0,LastTick=0,LongestTickGap=0;
    static int32 ObservedTicks=0;
    static TArray<TSharedPtr<FJsonValue>> StageTimings;
    static FString WorkerId;
    static FVector BeforeMap;
    static float BeforeZoom=0;
    static TArray<FString> Failures,Images;
    const double Now=FPlatformTime::Seconds();
    if(SmokeOwner.Get()!=this){SmokeOwner=this;Stage=Assertions=FactoryId=PortId=TradeSteps=0;Started=Since=LastTick=Now;LongestTickGap=0;ObservedTicks=0;StageTimings.Reset();Failures.Reset();Images.Reset();WorkerId.Empty();}
    if(Stage<0)return;
    LongestTickGap=FMath::Max(LongestTickGap,Now-LastTick);LastTick=Now;++ObservedTicks;
    auto* PC=Cast<ASeigeController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Hud=PC?Cast<ASeigeHUD>(PC->GetHUD()):nullptr;
    auto Check=[&](bool Valid,const FString& Message)
    {++Assertions;if(!Valid){Failures.Add(FString::Printf(TEXT("Stage %d: %s"),Stage,*Message));UE_LOG(LogTemp,Error,TEXT("NEXT_UPDATE_SMOKE %s"),*Failures.Last());}return Valid;};
    auto Finish=[&]()
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/NextUpdate"));
        for(const auto& Image:Images)Check(IFileManager::Get().FileSize(*FPaths::Combine(Folder,Image+TEXT(".png")))>0,TEXT("Capture exists: ")+Image);
        auto Report=MakeShared<FJsonObject>();
        Report->SetStringField(TEXT("scope"),TEXT("Bounded UI playcheck at 10x using the established-colony preset, one review-only factory, and a visual-only night phase. No save files, full acceptance suite, balance proof or graphics parity claim."));
        Report->SetBoolField(TEXT("passed"),Failures.IsEmpty()&&Stage==23);
        Report->SetNumberField(TEXT("assertions"),Assertions);Report->SetNumberField(TEXT("failures"),Failures.Num());
        Report->SetNumberField(TEXT("completed_stages"),Stage);Report->SetNumberField(TEXT("elapsed_seconds"),FPlatformTime::Seconds()-Started);
        Report->SetNumberField(TEXT("observed_frames"),ObservedTicks);Report->SetNumberField(TEXT("longest_frame_gap_seconds"),LongestTickGap);
        Report->SetArrayField(TEXT("stage_timings"),StageTimings);
        auto FrameCosts=MakeShared<FJsonObject>();FrameCosts->SetNumberField(TEXT("frames"),NextUpdateTiming.Frames);
        FrameCosts->SetNumberField(TEXT("fixed_simulation_total_seconds"),NextUpdateTiming.FixedSimulation);FrameCosts->SetNumberField(TEXT("fixed_simulation_max_seconds"),NextUpdateTiming.MaxFixedSimulation);
        FrameCosts->SetNumberField(TEXT("render_sync_total_seconds"),NextUpdateTiming.RenderSync);FrameCosts->SetNumberField(TEXT("render_sync_max_seconds"),NextUpdateTiming.MaxRenderSync);
        FrameCosts->SetNumberField(TEXT("preparation_total_seconds"),NextUpdateTiming.Preparation);FrameCosts->SetNumberField(TEXT("preparation_max_seconds"),NextUpdateTiming.MaxPreparation);
        Report->SetObjectField(TEXT("cpu_frame_costs"),FrameCosts);
        Report->SetBoolField(TEXT("scenario_ready"),Ready);Report->SetBoolField(TEXT("scenario_preparing"),IsPreparingScenario());
        Report->SetNumberField(TEXT("worker_bodies"),Sim.Workers.Bodies.Num());Report->SetNumberField(TEXT("buildings"),Sim.Buildings.Num());Report->SetNumberField(TEXT("couriers"),Sim.Couriers.Num());
        Report->SetNumberField(TEXT("simulation_speed"),Speed);Report->SetNumberField(TEXT("simulation_seconds_advanced"),Sim.Time-InitialTime);
        TArray<TSharedPtr<FJsonValue>> F,I;for(const auto& V:Failures)F.Add(MakeShared<FJsonValueString>(V));for(const auto& V:Images)I.Add(MakeShared<FJsonValueString>(V));
        Report->SetArrayField(TEXT("assertion_failures"),F);Report->SetArrayField(TEXT("screenshots"),I);
        FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json));
        const bool Written=FFileHelper::SaveStringToFile(Json,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("NextUpdateSmoke.json")));
        const bool Passed=Written&&Failures.IsEmpty()&&Stage==23;Stage=-1;
        UE_LOG(LogTemp,Display,TEXT("NEXT_UPDATE_SMOKE_COMPLETE passed=%d assertions=%d failures=%d"),Passed?1:0,Assertions,Failures.Num());
        FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
    };
    if(Now-Started>55){Check(false,TEXT("55-second wall-clock watchdog expired"));Finish();return;}
    const double Delay=Stage==1?3.:Stage==22?2.5:1.;
    if(Now-Since<Delay||FScreenshotRequest::IsScreenshotRequested())return;
    if(!Check(Hud&&PC,TEXT("Controller and rendered HUD are available"))){Finish();return;}
    Hud->SmokeLabelCapture=true;
    auto Label=[&](const FString& Part){return Hud->SmokeLabels.ContainsByPredicate([&](const FString& V){return V.Contains(Part);});};
    auto Click=[&](const FString& Action)
    {
        const auto* R=Hud->Ui.HitRegions.FindByPredicate([&](const FSeigeButton& V){return V.Action==Action;});
        if(!Check(R!=nullptr,TEXT("Rendered action exists: ")+Action))return false;
        const FVector2D P=(R->Position+R->Size*.5)*Hud->Ui.Scale;
        PC->SetMouseLocation(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y));PC->HandlePrimaryClick(P.X,P.Y);return true;
    };
    auto Capture=[&](const TCHAR* Name)
    {
        const FString Folder=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/NextUpdate"));
        Check(IFileManager::Get().MakeDirectory(*Folder,true),TEXT("Capture directory is writable"));
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Folder,FString(Name)+TEXT(".png")),true,false);Images.Add(Name);
    };
    auto PointAtWorker=[&]()
    {
        const auto* Worker=Sim.Workers.Find(WorkerId);FVector2D Pixel;
        if(!Check(Worker&&Worker->Outdoor,TEXT("Inspected worker is physically outdoors")))return false;
        if(!Check(PC->ProjectWorldLocationToScreen(WorkerWorldPosition(*Worker,4)+FVector(0,0,35),Pixel),TEXT("Worker projects into the actual viewport")))return false;
        PC->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));return true;
    };
    switch(Stage)
    {
    case 0:
    {
        ScenarioSlots.Init(TEXT("empty"),9);ScenarioSlots[4]=TEXT("developed");ScenarioBackgroundBugs=ScenarioPeriodicAttacks=false;
        if(!Check(InitializeScenario(Error),TEXT("Established UI fixture initializes: ")+Error)){Finish();return;}
        FinishScenarioStart();Observer=false;CenterBrain.Reset();Speed=10;Paused=false;InitialTime=Sim.Time;
        RegionViewHistory.Reset();Hud->Ui.CloseMenus();Hud->ProgressionOpen=false;
        for(const auto& B:Sim.Buildings)if(Sim.Definition(B)->Role==TEXT("trade")){PortId=B.Id;break;}
        FactoryId=Sim.AddReviewBuilding(TEXT("vehicle_factory"),HomePosition()+FVector2D(3600,2600));
        Check(FactoryId>0&&PortId>0,TEXT("Factory and trading-port fixture available"));
        const auto& Calendar=ScenarioCalendar.GetRules();ScenarioCalendar.SetElapsedMicroseconds(Calendar.DaylightMicroseconds/2);BindScenarioCalendar();UpdateWeather(0);
        Notice.Empty();break;
    }
    case 1:
    {
        const auto* Worker=Sim.Workers.Bodies.FindByPredicate([](const auto& W){return W.State==TEXT("active")&&W.Outdoor&&W.Activity==TEXT("operate")&&W.CrewBuildingId>0;});
        if(!Check(Worker!=nullptr,TEXT("A staffed outdoor worker exists"))){Finish();return;}
        WorkerId=Worker->Id;CameraCenter=FVector(Worker->Position,0);Zoom=1400;CameraPitch=55;UpdateCamera();
        Check(Sim.Workers.OwnedCount()>=Sim.Workers.ActiveCount()+Sim.Workers.StoredCount(),TEXT("Worker headline counts do not duplicate active/stored subsets"));break;
    }
    case 2: PointAtWorker();break;
    case 3:
    {
        Check(Label(TEXT("Permanent crew:"))&&Label(TEXT("Battery ")),TEXT("Hover renders crew and battery details"));
        if(PointAtWorker()){float X=0,Y=0;PC->GetMousePosition(X,Y);PC->HandlePrimaryClick(X,Y);}PC->SetMouseLocation(2,2);break;
    }
    case 4:
        Check(Label(TEXT("WORKER / ")+WorkerId)&&Label(TEXT("Selected / Esc")),TEXT("Click pins the same physical worker inspector"));
        for(const auto* Heading:{TEXT("Credits"),TEXT("Energy"),TEXT("Workers"),TEXT("Raw"),TEXT("Basic"),TEXT("Advanced")})Check(Hud->SmokeLabels.Contains(Heading),FString(TEXT("Thin HUD heading: "))+Heading);
        Capture(TEXT("01_worker_and_hud"));break;
    case 5:
    {
        Hud->ProcessShortcut(EKeys::Escape,*this);
        const auto* R=Hud->Ui.HitRegions.FindByPredicate([](const auto& V){return V.Action==TEXT("summary:group:raw");});
        if(Check(R!=nullptr,TEXT("Raw hover entry exists"))){const auto P=(R->Position+R->Size*.5)*Hud->Ui.Scale;PC->SetMouseLocation(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y));}break;
    }
    case 6:
    {
        const auto* R=Hud->Ui.HitRegions.FindByPredicate([](const auto& V){return V.Action==TEXT("resource-detail:water");});
        if(Check(R!=nullptr,TEXT("Nested individual material entry exists"))){const auto P=(R->Position+R->Size*.5)*Hud->Ui.Scale;PC->SetMouseLocation(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y));}break;
    }
    case 7:
        Check(Label(TEXT("free for new construction"))&&Label(TEXT("Trade per")),TEXT("Nested material detail renders stock availability and batch quote"));
        PC->SetMouseLocation(2,2);Hud->Ui.CloseMenus();BeforeMap=CameraCenter;BeforeZoom=Zoom;ToggleRegionSurvey();UpdateCamera();break;
    case 8:
        Check(IsRegionMap(),TEXT("Regional survey view opens"));Hud->ProcessShortcut(EKeys::P,*this);break;
    case 9:
        Check(Hud->ProgressionOpen&&IsRegionMap()&&Label(TEXT("PRODUCTION CHAIN")),TEXT("Production chain overlays the regional map without changing its view"));
        Check(Hud->ChainGraph.Find(TEXT("b:")+Sim.CoreDefinition)>=0,TEXT("Worker-only core appears in production dependency graph"));Capture(TEXT("02_map_chain"));break;
    case 10:
        Hud->ProcessShortcut(EKeys::Escape,*this);Check(!Hud->ProgressionOpen&&IsRegionMap(),TEXT("Closing chain retains the underlying map"));
        FocusSector(4,true);UpdateCamera();break;
    case 11:
        Hud->ProcessShortcut(EKeys::Escape,*this);UpdateCamera();Check(IsRegionMap(),TEXT("Deselecting focused region restores the prior map"));break;
    case 12:
        ToggleRegionSurvey();UpdateCamera();Check(CameraCenter.Equals(BeforeMap,.01)&&FMath::IsNearlyEqual(Zoom,BeforeZoom,.01f),TEXT("Leaving map restores the original local camera"));
        SelectedId=FactoryId;CameraCenter=FVector(HomePosition(),0);Zoom=DefaultZoom*2;UpdateCamera();PC->SetMouseLocation(2,2);break;
    case 13: Click(TEXT("combat:open"));break;
    case 14: Click(TEXT("combat:tab:factory"));break;
    case 15:
        Check(Label(TEXT("COMPLETE ASSEMBLY BILL")),TEXT("Fabrication material bill reaches renderer"));Click(TEXT("combat:add"));break;
    case 16:
        Check(Label(TEXT("Required / local stock"))&&Label(TEXT("Total material mass:")),TEXT("Complete equipment bill includes quantities, local stocks and mass"));
        Check(Label(TEXT("per shot")),TEXT("Weapon shot requirements are rendered"));Capture(TEXT("03_equipment_bill"));break;
    case 17:
        Click(TEXT("combat:close"));SelectedId=PortId;PC->SetMouseLocation(2,2);break;
    case 18:
        if(!Hud->SmokeLabels.Contains(TEXT("Water"))&&TradeSteps++<24){Click(TEXT("trade:next"));Since=Now;return;}
        Check(Hud->SmokeLabels.Contains(TEXT("Water")),TEXT("Water selected to exercise a real sub-cent unit price"));
        Check(Label(TEXT("EXTERNAL TRADE"))&&Label(TEXT("Reference quote /")),TEXT("Trading port displays explicit reference batch size"));
        Check(Sim.TradeQuote(TEXT("water"),1,true)>0&&Sim.TradeQuote(TEXT("water"),1,true)<.01,TEXT("Fixture includes a positive price below 0.01 credits per unit"));
        Check(Label(TEXT("<0.01 Cr")),TEXT("Tiny positive transaction has an explicit less-than label"));
        Check(!Hud->SmokeLabels.ContainsByPredicate([](const FString& V){return (V.StartsWith(TEXT("Import "))||V.StartsWith(TEXT("Export ")))&&V.Contains(TEXT("0.00 Cr"));}),TEXT("Tiny positive transactions are never labeled zero credits"));
        {const auto& Calendar=ScenarioCalendar.GetRules();ScenarioCalendar.SetElapsedMicroseconds(Calendar.DaylightMicroseconds+Calendar.NightMicroseconds/2);BindScenarioCalendar();UpdateWeather(0);}break;
    case 19: break; // let actual night lights and exposure settle across frames
    case 20:
    {
        TArray<AActor*> Lights;UGameplayStatics::GetAllActorsOfClass(this,APointLight::StaticClass(),Lights);int32 Lit=0;
        for(auto* A:Lights)if(const auto* L=Cast<APointLight>(A))if(L->PointLightComponent->Intensity>0&&L->PointLightComponent->IsVisible())++Lit;
        Check(!ScenarioCalendar.Sample().IsDay&&Lit>0,TEXT("Night phase has active in-world local lighting"));Capture(TEXT("04_night_trade"));break;
    }
    case 21:
        Check(Speed==10&&!Paused&&Sim.Time>InitialTime,TEXT("Normal 10x simulation advances throughout the UI check"));
        Check(Images.Num()==4,TEXT("Four representative captures were requested"));break;
    case 22: break; // bounded capture flush, no workload loop
    case 23: Finish();return;
    }
    const double Done=FPlatformTime::Seconds();
    auto Timing=MakeShared<FJsonObject>();Timing->SetNumberField(TEXT("stage"),Stage);Timing->SetNumberField(TEXT("start_seconds"),Now-Started);Timing->SetNumberField(TEXT("wait_seconds"),Now-Since);Timing->SetNumberField(TEXT("execution_seconds"),Done-Now);Timing->SetNumberField(TEXT("simulation_seconds"),Sim.Time-InitialTime);
    StageTimings.Add(MakeShared<FJsonValueObject>(Timing));
    UE_LOG(LogTemp,Display,TEXT("NEXT_UPDATE_SMOKE_STAGE stage=%d wall=%.3f wait=%.3f execution=%.3f sim=%.3f frames=%d"),Stage,Done-Started,Now-Since,Done-Now,Sim.Time-InitialTime,ObservedTicks);
    ++Stage;Since=Done;
}
