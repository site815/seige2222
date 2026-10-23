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
    static int32 Stage=0,Assertions=0,FactoryId=0,PortId=0,TradeSteps=0,InspectedVehicleId=0,HoverSiteId=0;
    static double Started=0,Since=0,InitialTime=0,LastTick=0,LongestTickGap=0;
    static int32 ObservedTicks=0;
    static TArray<TSharedPtr<FJsonValue>> StageTimings;
    static FString WorkerId;
    static FSeigeInspectionRef StockRef;
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
        Report->SetStringField(TEXT("scope"),TEXT("Bounded UI playcheck at 10x using an established-colony preset, review-only factory/construction site, visual-only night phase, and paused stockpile inspection/depletion fixture. No player saves, full acceptance suite, balance proof or graphics parity claim."));
        Report->SetBoolField(TEXT("passed"),Failures.IsEmpty()&&Stage==38);
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
        const bool Passed=Written&&Failures.IsEmpty()&&Stage==38;Stage=-1;
        UE_LOG(LogTemp,Display,TEXT("NEXT_UPDATE_SMOKE_COMPLETE passed=%d assertions=%d failures=%d"),Passed?1:0,Assertions,Failures.Num());
        FPlatformMisc::RequestExitWithStatus(false,Passed?0:1);
    };
    if(Now-Started>75){Check(false,TEXT("75-second wall-clock watchdog expired"));Finish();return;}
    // Follow the actual moving transport during this hover assertion. At 10x it
    // can travel out from under a stationary cursor between fixture stages.
    if(Stage==27&&PC)
    {
        TArray<FSeigeInspectionCandidate> Candidates;GatherInspectionCandidates(Candidates);
        if(const auto* V=Candidates.FindByPredicate([&](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Transport&&C.Ref.Id==InspectedVehicleId&&C.Ref.OwnerSector==4;}))
        {FVector2D Pixel=FVector2D::ZeroVector;if(PC->ProjectWorldLocationToScreen(V->Bounds.GetCenter(),Pixel))PC->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));}
    }
    const double Delay=Stage==1?3.:Stage==24?2.5:1.;
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
        const auto* Worker=Sim.Workers.Find(WorkerId);FVector2D Pixel=FVector2D::ZeroVector;
        if(!Check(Worker&&Worker->Outdoor,TEXT("Inspected worker is physically outdoors")))return false;
        if(!Check(PC->ProjectWorldLocationToScreen(WorkerWorldPosition(*Worker,4)+FVector(0,0,35),Pixel),TEXT("Worker projects into the actual viewport")))return false;
        PC->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));return true;
    };
    switch(Stage)
    {
    case 0:
    {
        ScenarioSeedOverride=815;ScenarioSlots.Init(TEXT("empty"),9);ScenarioSlots[4]=TEXT("developed");ScenarioBackgroundBugs=ScenarioPeriodicAttacks=false;
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
    {
        Check(Label(TEXT("WORKER / ")+WorkerId)&&Label(TEXT("Selected / Esc")),TEXT("Click pins the same physical worker inspector"));
        for(const auto* Heading:{TEXT("Credits"),TEXT("Energy"),TEXT("Workers"),TEXT("Raw"),TEXT("Basic"),TEXT("Advanced")})Check(Hud->SmokeLabels.Contains(Heading),FString(TEXT("Thin HUD heading: "))+Heading);
        const float ExpectedWidth=Hud->Ui.ViewportHeight,UiScale=Hud->Ui.Scale;
        auto Casing=[&](bool Bottom){return Hud->Ui.HitRegions.FindByPredicate([&](const FSeigeButton& R){return R.Action==TEXT("panel")&&FMath::Abs(R.Size.X*UiScale-ExpectedWidth)<.1f&&(R.Position.Y*UiScale>Hud->Ui.ViewportHeight*.5f)==Bottom;});};
        const auto* Top=Casing(false);const auto* Dock=Casing(true);
        Check(Top&&Dock,TEXT("Both rendered HUD casings are exactly the physical screen height in width"));
        if(Top&&Dock)
        {
            Check(FMath::Abs(Top->Position.X-Dock->Position.X)<.1f,TEXT("Upper resource HUD and bottom dock share the same horizontal alignment"));
            for(const auto* Action:{TEXT("build-menu"),TEXT("region-map"),TEXT("progression"),TEXT("summary:objective"),TEXT("focus-rex"),TEXT("game-menu"),TEXT("pause"),TEXT("speed"),TEXT("summary:threats")})
            {
                const auto* Control=Hud->Ui.HitRegions.FindByPredicate([&](const FSeigeButton& R){return R.Action==Action;});
                Check(Control&&Control->Position.X>=Dock->Position.X&&Control->Position.Y>=Dock->Position.Y&&Control->Position.X+Control->Size.X<=Dock->Position.X+Dock->Size.X+.1f&&Control->Position.Y+Control->Size.Y<=Dock->Position.Y+Dock->Size.Y+.1f,FString(TEXT("Action is retained inside compact dock: "))+Action);
                Check(Control&&FMath::IsNearlyEqual(Control->Position.Y,Dock->Position.Y+8),FString(TEXT("Single-row dock control: "))+Action);
            }
        }
        const auto* SpeedControl=Hud->Ui.HitRegions.FindByPredicate([](const auto& V){return V.Action==TEXT("speed");});
        const auto* Timer=Hud->Ui.HitRegions.FindByPredicate([](const auto& V){return V.Action==TEXT("summary:threats");});
        Check(SpeedControl&&Timer&&FMath::IsNearlyEqual(SpeedControl->Position.Y,Timer->Position.Y)&&Timer->Position.X>=SpeedControl->Position.X+SpeedControl->Size.X,
            TEXT("Whole timer shares the bottom dock immediately after speed controls"));
        Capture(TEXT("01_worker_and_hud"));break;
    }
    case 5:
    {
        Hud->ProcessShortcut(EKeys::Escape,*this);Hud->ProcessShortcut(EKeys::B,*this);
        const auto* R=Hud->Ui.HitRegions.FindByPredicate([](const auto& V){return V.Action==TEXT("summary:group:raw");});
        if(Check(R!=nullptr,TEXT("Raw hover entry exists"))){const auto P=(R->Position+R->Size*.5)*Hud->Ui.Scale;PC->SetMouseLocation(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y));}break;
    }
    case 6:
    {
        Check(Hud->Ui.BuildOpen&&Label(TEXT("CONSTRUCTION"))&&Label(TEXT("RAW / INVENTORY")),TEXT("Top resource hover remains visible while construction menu stays open"));
        Capture(TEXT("09_build_and_resources"));
        const auto* Card=Hud->Ui.HitRegions.FindByPredicate([](const auto& V){return V.Action==TEXT("summary:group:raw");});
        if(Card)
        {
            const auto Bridge=FVector2D(Card->Position.X+Card->Size.X*.5,Card->Position.Y+Card->Size.Y+4)*Hud->Ui.Scale;
            Hud->Ui.UpdateHoverPanel(Bridge.X,Bridge.Y,true);
            Check(Hud->Ui.HoverPanel==TEXT("group:raw"),TEXT("Pointer crosses the top casing padding into its resource ledger without closing it"));
        }
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
        Check(Images.Num()==5,TEXT("Five gameplay captures were requested"));Click(TEXT("game-menu"));break;
    case 22: Click(TEXT("screen:credits"));break;
    case 23:
        Check(Screen==TEXT("credits")&&Label(TEXT("licence unresolved")),TEXT("Credits visibly record the provisional Rex asset exception"));
        Capture(TEXT("05_credits"));Check(Images.Num()==6,TEXT("Six representative captures were requested"));break;
    case 24: break; // bounded capture flush, no workload loop
    case 25:
    {
        ResumeGameMenu();Screen=TEXT("playing");MenuOpen=false;Paused=false;Speed=10;
        SelectedId=SelectedCompanionId=SelectedRoadId=0;Hud->ClearUnitInspection();Hud->Ui.CloseMenus();
        const auto* V=Sim.TransportVehicles.Bodies.FindByPredicate([](const auto& V){return V.Health>0&&!V.Embarked&&!V.Evacuated;});
        if(Check(V!=nullptr,TEXT("A physical autonomous transport is available"))){InspectedVehicleId=V->Id;CameraCenter=FVector(V->Position,0);Zoom=1400;UpdateCamera();}
        const auto& Calendar=ScenarioCalendar.GetRules();ScenarioCalendar.SetElapsedMicroseconds(Calendar.DaylightMicroseconds/2);BindScenarioCalendar();UpdateWeather(0);break;
    }
    case 26:
    {
        TArray<FSeigeInspectionCandidate> Candidates;GatherInspectionCandidates(Candidates);
        const auto* V=Candidates.FindByPredicate([&](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Transport&&C.Ref.Id==InspectedVehicleId&&C.Ref.OwnerSector==4;});FVector2D Pixel=FVector2D::ZeroVector;
        if(Check(V&&PC->ProjectWorldLocationToScreen(V->Bounds.GetCenter(),Pixel),TEXT("Transport inspection uses the visible rendered vehicle")))PC->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));break;
    }
    case 27:
    {
        Check(Label(TEXT("HP "))&&Label(TEXT("Cargo capacity"))&&Label(TEXT("Battery ")),TEXT("Transport hover shows actual health bar, battery and cargo details"));
        float X=0,Y=0;PC->GetMousePosition(X,Y);PC->HandlePrimaryClick(X,Y);
        Check(Hud->HasUnitInspection(),TEXT("Clicking the transport pins its inspector"));PC->SetMouseLocation(2,2);
        Capture(TEXT("06_transport_inspection"));break;
    }
    case 28:
        Check(Label(TEXT("Selected / Esc"))&&Label(TEXT("Cargo capacity")),TEXT("Vehicle details stay pinned after cursor leaves"));
        Hud->ProcessShortcut(EKeys::Escape,*this);Check(!Hud->HasUnitInspection(),TEXT("Esc clears a selected vehicle without opening the menu"));
        HoverSiteId=Sim.AddReviewBuilding(TEXT("sensor"),HomePosition()+FVector2D(2200,-2100));
        if(auto* B=Sim.FindBuilding(HoverSiteId)){B->IsConstructing=true;B->ConstructionProgress=.42;CameraCenter=FVector(B->Position,0);Zoom=2200;UpdateCamera();}break;
    case 29:
    {
        TArray<FSeigeInspectionCandidate> Candidates;GatherInspectionCandidates(Candidates);
        const auto* B=Candidates.FindByPredicate([&](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Building&&C.Ref.Id==HoverSiteId&&C.Ref.OwnerSector==4;});FVector2D Pixel=FVector2D::ZeroVector;
        if(Check(B&&PC->ProjectWorldLocationToScreen(B->Bounds.GetCenter(),Pixel),TEXT("Construction site projects through the unified hover picker")))PC->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));break;
    }
    case 30:
        Check(Label(TEXT("HP "))&&Label(TEXT("Construction ")),TEXT("Building hover renders health above and construction below"));Capture(TEXT("07_building_hover"));break;
    case 31:
        WallPlacementActive=true;FleetOrderActive=true;
        PC->SetMouseLocation(2,2);Click(TEXT("focus-rex"));UpdateCamera();break;
    case 32:
        Check(!WallPlacementActive&&!FleetOrderActive,TEXT("Rex focus cancels prior wall and fleet targeting modes"));
        Check(SelectedCompanionId==1&&Label(TEXT("REX"))&&Label(TEXT("Health ")),TEXT("Bottom Rex button focuses the companion and opens real health and lifecycle details"));
        Check(!IsRegionMap()&&Zoom==FMath::Max(MinimumZoom*3,DefaultZoom*.35f),TEXT("Rex focus changes to the elevated local close camera"));Capture(TEXT("08_rex_focus"));break;
    case 33:
    {
        SelectedId=SelectedCompanionId=0;Hud->ClearUnitInspection();Hud->Ui.CloseMenus();Paused=true;
        Check(!Sim.RoadsEnabled()&&Sim.Roads.IsEmpty(),TEXT("Current scenario has no road prerequisites or hidden road network"));
        TArray<FSeigeInspectionCandidate> Candidates;GatherInspectionCandidates(Candidates);
        const auto* Pile=Candidates.FindByPredicate([](const auto& C){return C.Ref.Kind==ESeigeInspectKind::Stockpile&&C.Ref.OwnerSector==4&&!C.Ref.ConstructionStock;});
        if(Check(Pile!=nullptr,TEXT("Actual outdoor inventory has a rendered stockpile")))
        {StockRef=Pile->Ref;CameraCenter=FVector(FVector2D(Pile->Bounds.GetCenter())/RenderScale,0);Zoom=1400;UpdateCamera();}break;
    }
    case 34:
    {
        TArray<FSeigeInspectionCandidate> Candidates;GatherInspectionCandidates(Candidates);
        const auto* Pile=Candidates.FindByPredicate([&](const auto& C){return C.Ref==StockRef;});FVector2D Pixel;
        if(Check(Pile&&PC->ProjectWorldLocationToScreen(Pile->Bounds.GetCenter(),Pixel),TEXT("Ground stock projects through the unit picker")))PC->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));break;
    }
    case 35:
    {
        Check(Label(TEXT("Quantity "))&&Label(TEXT("Mass "))&&Label(TEXT("Reserved ")),TEXT("Ground stock hover shows actual quantity, mass, volume and reservations"));
        float X=0,Y=0;PC->GetMousePosition(X,Y);PC->HandlePrimaryClick(X,Y);
        Check(Hud->HasUnitInspection(),TEXT("Clicking physical stock pins its inspector"));PC->SetMouseLocation(2,2);Capture(TEXT("10_ground_stock"));break;
    }
    case 36:
        Check(Label(TEXT("Selected / Esc"))&&Label(TEXT("Quantity ")),TEXT("Ground stock details remain pinned after cursor leaves"));
        // Remove only this review fixture's pile to prove stale actors cannot
        // keep a deleted inventory selection alive; no player save is involved.
        if(auto* B=Sim.FindBuilding(StockRef.Id))B->Inventory.Add(StockRef.ResourceId,0);break;
    case 37:
        Check(!Hud->HasUnitInspection(),TEXT("Exhausted stock automatically clears pinned inspection"));Paused=false;break;
    case 38: Finish();return;
    }
    const double Done=FPlatformTime::Seconds();
    auto Timing=MakeShared<FJsonObject>();Timing->SetNumberField(TEXT("stage"),Stage);Timing->SetNumberField(TEXT("start_seconds"),Now-Started);Timing->SetNumberField(TEXT("wait_seconds"),Now-Since);Timing->SetNumberField(TEXT("execution_seconds"),Done-Now);Timing->SetNumberField(TEXT("simulation_seconds"),Sim.Time-InitialTime);
    StageTimings.Add(MakeShared<FJsonValueObject>(Timing));
    UE_LOG(LogTemp,Display,TEXT("NEXT_UPDATE_SMOKE_STAGE stage=%d wall=%.3f wait=%.3f execution=%.3f sim=%.3f frames=%d"),Stage,Done-Started,Now-Since,Done-Now,Sim.Time-InitialTime,ObservedTicks);
    ++Stage;Since=Done;
}
