#include "SeigeGameMode.h"
#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "RenderTimer.h"
#include "DynamicRHI.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Scalability.h"
#include "Engine/Engine.h"

namespace
{
TSharedPtr<FJsonObject> TimingSummary(TArray<double> Samples)
{
    auto Summary=MakeShared<FJsonObject>();
    Summary->SetBoolField(TEXT("available"),!Samples.IsEmpty());
    Summary->SetNumberField(TEXT("samples"),Samples.Num());
    if(Samples.IsEmpty())return Summary;
    double Sum=0;for(double Sample:Samples)Sum+=Sample;
    Samples.Sort();
    auto Percentile=[&](double P){return Samples[FMath::Clamp(FMath::CeilToInt(Samples.Num()*P)-1,0,Samples.Num()-1)];};
    Summary->SetNumberField(TEXT("mean_ms"),Sum/Samples.Num());
    Summary->SetNumberField(TEXT("p95_ms"),Percentile(.95));
    Summary->SetNumberField(TEXT("p99_ms"),Percentile(.99));
    Summary->SetNumberField(TEXT("max_ms"),Samples.Last());
    return Summary;
}
}

// Opt-in, repeatable graphics measurement. No player saves or settings are written.
// Each camera waits for complete scenery, settles for four seconds, then samples five.
void ASeigeGameMode::RunGraphicsBenchmark(float DeltaSeconds)
{
    struct FView {const TCHAR* Name;FVector Center;float Yaw,Pitch,Distance;};
    static const FView Views[]={
        {TEXT("colony"),FVector(0,0,0),135,52,3500},
        {TEXT("meadow"),FVector(1200,400,0),150,25,900},
        {TEXT("ground"),FVector(1300,450,0),35,20,180},
        {TEXT("hills"),FVector(3000,-700,0),155,48,7000},
        {TEXT("boundary"),FVector(29000,0,0),0,60,14000},
        // Companion review view: follows Rex at ground level. Opt-in only
        // (-BenchmarkView=rex); the standard five-view runs never include it.
        {TEXT("rex"),FVector(0,0,0),120,12,150}};
    constexpr int32 StandardViews=5;
    constexpr int32 RexView=5;
    static TWeakObjectPtr<ASeigeGameMode> BenchmarkOwner;
    static int32 View=-1;
    static int32 SelectedView=INDEX_NONE;
    enum class EPhase {AwaitScenery,Warmup,Sample,Capture,AwaitCapture};
    static EPhase Phase=EPhase::Warmup;
    static double Started=0,Previous=0;
    static double SampleSimulationStart=0;
    static FString Name;
    static TArray<double> Frames;
    static TArray<double> GameTimes,RenderTimes,RhiTimes,GpuTimes;
    static bool Orbit=false,Travel=false,SimpleTerrain=false;
    static double MotionStarted=0;
    static double ViewSetupStarted=0,SynchronousSetupSeconds=0,SceneryReadySeconds=0;
    static int32 InitialPendingSceneryCells=0;
    static int32 PendingSceneryAtSampleStart=0,MaxPendingSceneryCells=0;
    static double PendingSceneryCellSum=0;
    static int32 MaxPendingVisible=0,MaxPendingNear=0,FramesWithPendingVisible=0;
    static int32 InitialPendingVisible=-1,InitialPendingNear=-1;
    static double InitialVisibleReadySeconds=-1,InitialNearReadySeconds=-1;
    static double ConsecutiveVisiblePendingSeconds=0,MaxConsecutiveVisiblePendingSeconds=0;
    static int32 SimpleTerrainComponents=0;
    static TArray<TSharedPtr<FJsonValue>> Results;
#if CSV_PROFILER
    static TSharedFuture<FString> CsvWrite;
    static double CsvWriteStarted=0;
#endif
    const double Now=FPlatformTime::Seconds();
    if(BenchmarkOwner.Get()!=this)
    {
        BenchmarkOwner=this;View=-1;Phase=EPhase::Warmup;Frames.Reset();Results.Reset();Previous=Now;
#if CSV_PROFILER
        CsvWrite=TSharedFuture<FString>();
#endif
        Name=TEXT("default");FParse::Value(FCommandLine::Get(),TEXT("BenchmarkName="),Name);
        SelectedView=INDEX_NONE;
        FString RequestedView;
        const bool HasView=FParse::Value(FCommandLine::Get(),TEXT("BenchmarkView="),RequestedView);
        if(HasView||FParse::Param(FCommandLine::Get(),TEXT("BenchmarkView"))||FString(FCommandLine::Get()).Contains(TEXT("-BenchmarkView="),ESearchCase::IgnoreCase))
        {
            for(int32 Index=0;Index<UE_ARRAY_COUNT(Views);++Index)
                if(RequestedView.Equals(Views[Index].Name,ESearchCase::IgnoreCase)){SelectedView=Index;break;}
            if(SelectedView==INDEX_NONE)
            {
                UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK invalid BenchmarkView='%s'; expected colony, meadow, ground, hills, boundary or rex"),*RequestedView);
                View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);return;
            }
        }
        Orbit=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkOrbit"));
        Travel=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkTravel"));
        SimpleTerrain=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkSimpleTerrain"));
        SimpleTerrainComponents=0;
        for(TCHAR& C:Name)if(!FChar::IsAlnum(C)&&C!=TEXT('-')&&C!=TEXT('_'))C=TEXT('_');
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkFullGrassShadows")))GrassShadowDistanceMeters=500;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkFullGrassLighting")))GrassDistanceFieldLighting=true;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkGrassOpaqueBeyond80")))GrassProgrammableDistanceMeters=80;
    }
    auto ApplySwardDiagnostic=[&]()
    {
        const bool Hide=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkHideSward"));
        const bool Opaque=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkOpaqueSward"));
        const bool OnlyA=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkSwardOnlyA"));
        const bool OnlyB=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkSwardOnlyB"));
        if(GroundCover&&(Hide||Opaque||OnlyA||OnlyB))
        {
            TArray<UInstancedStaticMeshComponent*> Components;GroundCover->GetComponents(Components);
            UMaterialInterface* Plain=Opaque?LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/NatureV08/M_GrassProxyV08.M_GrassProxyV08")):nullptr;
            for(auto* Component:Components)if(Component->ComponentHasTag(TEXT("seige_sward")))
            {
                bool IsA=false,IsB=false;
                for(const FName Tag:Component->ComponentTags)
                {
                    const FString Text=Tag.ToString();
                    IsA|=Text.Contains(TEXT(":Grass_"))||Text==TEXT("seige_source:Grass");
                    IsB|=Text.Contains(TEXT(":GrassB_"))||Text==TEXT("seige_source:GrassB");
                }
                if(Hide||(OnlyA&&!IsA)||(OnlyB&&!IsB)){if(Component->IsVisible())Component->SetVisibility(false);continue;}
                // Use the existing opaque *two-sided* grass shader so back-face
                // culling cannot masquerade as missing masked-card coverage.
                // Geometry/transforms remain identical, including new instances.
                if(Plain)for(int32 Slot=0;Slot<Component->GetNumMaterials();++Slot)
                    if(Component->GetMaterial(Slot)!=Plain)Component->SetMaterial(Slot,Plain);
            }
        }
    };
    auto ApplyViewDiagnostics=[&]()
    {
        ApplySwardDiagnostic();
        if(SimpleTerrain&&Landscape)
        {
            // A runtime-only substitution isolates terrain shading cost. Keep
            // the same terrain geometry, grass, lighting and camera trajectory.
            TArray<UPrimitiveComponent*> Components;Landscape->GetComponents(Components);
            SimpleTerrainComponents=0;
            UMaterialInterface* Plain=Material(FLinearColor(.18f,.22f,.13f));
            for(auto* Component:Components)
            {
                for(int32 Slot=0;Slot<Component->GetNumMaterials();++Slot)Component->SetMaterial(Slot,Plain);
                if(Component->GetNumMaterials()>0)++SimpleTerrainComponents;
            }
        }
    };
    auto ObserveInitialCoverage=[&](double CurrentTime)
    {
        // Null state reports zero counters, so zero is meaningful only after
        // CreateGroundCover has initialized its residency/scheduling state.
        if(!SceneryStream.IsValid())return;
        const int32 Visible=PendingVisibleSceneryCells(),Near=PendingNearSceneryCells();
        if(InitialPendingVisible<0){InitialPendingVisible=Visible;InitialPendingNear=Near;}
        if(Visible==0&&InitialVisibleReadySeconds<0)InitialVisibleReadySeconds=CurrentTime-ViewSetupStarted;
        if(Near==0&&InitialNearReadySeconds<0)InitialNearReadySeconds=CurrentTime-ViewSetupStarted;
    };
    auto FollowRex=[&]()
    {
        if(View!=RexView||Sim.Companions.Dogs.IsEmpty())return;
        const auto& Dog=Sim.Companions.Dogs[0];
        const auto* Snapshot=PresentationSnapshot(Sim);
        CameraCenter=FVector(Snapshot?Snapshot->Companion(Dog,PresentationAlpha()):Dog.Position,0);
    };
    auto BeginView=[&]()
    {
        ViewSetupStarted=FPlatformTime::Seconds();
        CameraCenter=Views[View].Center;CameraYaw=Views[View].Yaw;CameraPitch=Views[View].Pitch;Zoom=Views[View].Distance;
        if(View==RexView)
        {
            // Review-only camera overrides. A zoom below the player minimum is
            // allowed here so the companion can be inspected at close range.
            float Value=0;
            if(FParse::Value(FCommandLine::Get(),TEXT("BenchmarkZoom="),Value)&&Value>0){MinimumZoom=FMath::Min(MinimumZoom,Value);Zoom=Value;}
            if(FParse::Value(FCommandLine::Get(),TEXT("BenchmarkYaw="),Value))CameraYaw=Value;
            // The close-range camera blends toward the minimum pitch, so the
            // requested pitch becomes that minimum for the review run.
            if(FParse::Value(FCommandLine::Get(),TEXT("BenchmarkPitch="),Value)){MinimumCameraPitch=FMath::Clamp(Value,1.f,MaximumCameraPitch);CameraPitch=MinimumCameraPitch;}
        }
        FollowRex();
        if(View==0&&FParse::Param(FCommandLine::Get(),TEXT("BenchmarkClearing")))
        {CameraCenter=FVector(HomePosition(),0);CameraPitch=50;}
        UpdateCamera();RefreshEnvironment();SyncVisuals();
        ApplyViewDiagnostics();
        // Synchronous setup and budgeted scenery generation are both excluded.
        // Settling starts only after the final required cell is installed.
        Frames.Reset();GameTimes.Reset();RenderTimes.Reset();RhiTimes.Reset();GpuTimes.Reset();
        Phase=EPhase::AwaitScenery;Started=Previous=MotionStarted=FPlatformTime::Seconds();
        SynchronousSetupSeconds=Started-ViewSetupStarted;SceneryReadySeconds=0;
        InitialPendingSceneryCells=PendingSceneryCells();
        PendingSceneryAtSampleStart=MaxPendingSceneryCells=0;PendingSceneryCellSum=0;
        MaxPendingVisible=MaxPendingNear=FramesWithPendingVisible=0;
        InitialPendingVisible=InitialPendingNear=-1;InitialVisibleReadySeconds=InitialNearReadySeconds=-1;
        ConsecutiveVisiblePendingSeconds=MaxConsecutiveVisiblePendingSeconds=0;
        ObserveInitialCoverage(Started);
    };
    if(View<0)
    {
        if(RenderClock<2)return;
        // Pin the single Medium profile and full resolution independently of
        // saved user choices. The opt-in old-profile run is diagnostic only.
        Scalability::FQualityLevels BenchmarkQuality;
        const bool OldEpic=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkV05Epic"));
        if(OldEpic)
        {
            BenchmarkQuality.SetFromSingleQualityLevel(3);
        }
        else BenchmarkQuality=Scalability::GetQualityLevels();
        BenchmarkQuality.ResolutionQuality=100.f;
        Scalability::SetQualityLevels(BenchmarkQuality,true);
        // Reapply the profile's additional settings after the quality groups:
        // TextureQuality resets r.MaxAnisotropy to its built-in value of 8.
        if(!OldEpic)ApplyMediumPreset();
        if(OldEpic)for(const auto& Setting:{TPair<const TCHAR*,float>(TEXT("r.TSR.ThinGeometryDetection"),0),TPair<const TCHAR*,float>(TEXT("r.TSR.Velocity.WeightClampingSampleCount"),4),TPair<const TCHAR*,float>(TEXT("r.Tonemapper.Sharpen"),0),TPair<const TCHAR*,float>(TEXT("r.MaxAnisotropy"),8)})
            if(auto* Variable=IConsoleManager::Get().FindConsoleVariable(Setting.Key))Variable->Set(Setting.Value,ECVF_SetByConsole);
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkDisableThinGeometry")))
            if(auto* Thin=IConsoleManager::Get().FindConsoleVariable(TEXT("r.TSR.ThinGeometryDetection")))Thin->Set(0,ECVF_SetByConsole);
        // The explicit diagnostic restores the original geometry target even
        // when ordinary play uses a different external project default.
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkNaniteBaseline")))
            if(auto* NaniteEdge=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge")))
                NaniteEdge->Set(1.f,ECVF_SetByConsole);
        const bool Clearing=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkClearing"));
        ScenarioSlots.Init(TEXT("empty"),9);ScenarioSlots[4]=TEXT("player");
        const bool DevelopedNeighbors=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkDevelopedNeighbors"));
        if(DevelopedNeighbors)for(int32 Index=0;Index<9;++Index)if(Index!=4)ScenarioSlots[Index]=TEXT("developed");
        if(Clearing)ScenarioBackgroundBugs=ScenarioPeriodicAttacks=false;
        if(DevelopedNeighbors)
        {
            if(!InitializeScenario(Error))
            {UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK established neighbors failed: %s"),*Error);View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);return;}
            FinishScenarioStart();
        }
        else StartScenario();
        FVector2D Landing=FVector2D::ZeroVector;
        if(Clearing)
        {
            // The same ordinary resource-side landing used by the interaction
            // route, isolated for settled surface review without replaying it.
            bool Found=false;
            for(const auto& Node:Sim.Nodes)if(!Found&&Sim.Resources[Node.Resource].Class==TEXT("standard"))
                for(int32 I=0;I<16&&!Found;++I)
                {const double A=I*UE_TWO_PI/16;const auto Candidate=Node.Position+FVector2D(FMath::Cos(A),FMath::Sin(A))*1100;FString Why;if(CanLand(Candidate,Why)){Landing=Candidate;Found=true;}}
            if(!Found){UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK no valid clearing landing"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
        }
        if(Screen==TEXT("landing"))ConfirmLanding(Landing);
        const FSeigeBuildingDef* CommandDefinition=Sim.BuildingDefs.Find(Sim.CoreDefinition);
        if(Screen==TEXT("playing")&&!Observer&&CommandDefinition)
        {
            const double Deadline=Sim.Time+CommandDefinition->ConstructionSeconds*6;
            auto Core=[&](){return Sim.Buildings.FindByPredicate([&](const FSeigeBuilding& B){return B.DefId==Sim.CoreDefinition&&B.Health>0;});};
            while(Core()&&Core()->IsConstructing&&Sim.Time<Deadline&&!Sim.Failed)Sim.Tick(1.);
        }
        // A graphics comparison uses a documented identical light phase. It does
        // not grant production/cargo or change the colony's elapsed development.
        const auto& CalendarRules=ScenarioCalendar.GetRules();
        const int64 BenchmarkDay=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkWinter"))?int64(CalendarRules.DaysPerSeason)*3+CalendarRules.DaysPerSeason/2:0;
        ScenarioCalendar.SetElapsedMicroseconds(BenchmarkDay*(CalendarRules.DaylightMicroseconds+CalendarRules.NightMicroseconds)+CalendarRules.DaylightMicroseconds/2);BindScenarioCalendar();UpdateWeather(0);
        Paused=false;Speed=10;ResetSimulationPresentation();
        const FSeigeBuilding* Command=Sim.Buildings.FindByPredicate([&](const FSeigeBuilding& Building){return Building.DefId==Sim.CoreDefinition;});
        if(Screen!=TEXT("playing")||Observer||!Command||Command->IsConstructing||Command->Health<=0)
        {
            UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK failed to deploy fresh player core: %s"),*Error);
            View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);return;
        }
        View=SelectedView==INDEX_NONE?0:SelectedView;BeginView();
        return;
    }
    if(View<UE_ARRAY_COUNT(Views)&&(Phase==EPhase::AwaitScenery||Phase==EPhase::Warmup))ObserveInitialCoverage(Now);
    if(View<UE_ARRAY_COUNT(Views)&&(Phase==EPhase::AwaitScenery||(!Orbit&&Phase==EPhase::Warmup&&!IsSceneryStreamingReady())))
    {
        Phase=EPhase::AwaitScenery;
        if(Now-ViewSetupStarted>120)
        {
            UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK %s scenery readiness timed out with %d cells pending"),Views[View].Name,PendingSceneryCells());
            View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);return;
        }
        if(!IsSceneryStreamingReady())return;
        // Budgeted cells did not exist when BeginView first applied diagnostics.
        ApplyViewDiagnostics();
        SceneryReadySeconds=Now-ViewSetupStarted;
        Phase=EPhase::Warmup;Started=Previous=MotionStarted=FPlatformTime::Seconds();
        UE_LOG(LogTemp,Display,TEXT("GRAPHICS_BENCHMARK %s %s: scenery ready after %.3fs; starting 4s settling"),*Name,Views[View].Name,SceneryReadySeconds);
        return;
    }
    if(View>=UE_ARRAY_COUNT(Views))
    {
#if CSV_PROFILER
        // EndCapture is asynchronous and stops on an end-frame tick. Continue
        // ticking until its write future resolves instead of exiting mid-file.
        if(CsvWrite.IsValid())
        {
            if(CsvWrite.IsReady())
            {
                const FString Filename=CsvWrite.Get();
                UE_LOG(LogTemp,Display,TEXT("GRAPHICS_BENCHMARK CSV saved: %s"),*Filename);
                FPlatformMisc::RequestExitWithStatus(false,Filename.IsEmpty()?1:0);
            }
            else if(Now-CsvWriteStarted>30)
            {UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK CSV write timed out"));FPlatformMisc::RequestExitWithStatus(false,1);}
        }
#endif
        return;
    }
    if(Orbit&&(Phase==EPhase::Warmup||Phase==EPhase::Sample))
    {
        // Camera-driven streaming can install further sward cells mid-orbit.
        ApplySwardDiagnostic();
        // One turn per five seconds, independent of frame rate. Start moving
        // during warmup; cross the authored angle near the sample boundary,
        // without resetting the camera or rebuilding the environment there.
        const double Turns=(Now-MotionStarted-4.)/5.;
        CameraYaw=FMath::Fmod(Views[View].Yaw+Turns*360.,360.);
        CameraPitch=FMath::Clamp(float(Views[View].Pitch+8.*FMath::Sin(Turns*2.*PI)),MinimumCameraPitch,MaximumCameraPitch);
        UpdateCamera();
    }
    if(Phase==EPhase::Warmup)
    {
        if(Now-Started>=4)
        {
            // Opt-in GPU pass breakdown. This diagnostic run is not used as a
            // performance baseline because capture itself can stall the GPU.
            if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkGPUProfile")))
            {
                if(auto* ShowUI=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ProfileGPU.ShowUI")))ShowUI->Set(0,ECVF_SetByConsole);
                if(GEngine)GEngine->Exec(GetWorld(),TEXT("profilegpu"));
            }
            Phase=EPhase::Sample;Started=Previous=Now;SampleSimulationStart=Sim.Time;
            PendingSceneryAtSampleStart=PendingSceneryCells();
            CSV_EVENT_GLOBAL(TEXT("SEIGE_BENCH_SAMPLE_BEGIN:%s"),Views[View].Name);
        }
        return;
    }
    if(View==RexView&&(Phase==EPhase::Warmup||Phase==EPhase::Sample)){FollowRex();UpdateCamera();}
    if(Phase==EPhase::Sample)
    {
        if(Travel)
        {
            // Survey translation at36m/s deliberately crosses multiple54m
            // streaming cells. Keep all generated work inside normal budgets.
            CameraCenter=Views[View].Center+FVector((Now-Started)*3600./RenderScale,0,0);
            UpdateCamera();ApplySwardDiagnostic();
        }
        if(!Orbit&&!Travel&&View!=RexView&&!IsSceneryStreamingReady())
        {
            UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK %s scenery became incomplete during sampling (%d cells pending)"),Views[View].Name,PendingSceneryCells());
            View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);return;
        }
        const double FrameMS=(Now-Previous)*1000;Previous=Now;
        if(FrameMS>0)
        {
            Frames.Add(FrameMS);
            const int32 Pending=PendingSceneryCells();
            PendingSceneryCellSum+=Pending;MaxPendingSceneryCells=FMath::Max(MaxPendingSceneryCells,Pending);
            const int32 Visible=PendingVisibleSceneryCells(),Near=PendingNearSceneryCells();
            MaxPendingVisible=FMath::Max(MaxPendingVisible,Visible);MaxPendingNear=FMath::Max(MaxPendingNear,Near);
            if(Visible>0)
            {
                ++FramesWithPendingVisible;ConsecutiveVisiblePendingSeconds+=FrameMS*.001;
                MaxConsecutiveVisiblePendingSeconds=FMath::Max(MaxConsecutiveVisiblePendingSeconds,ConsecutiveVisiblePendingSeconds);
            }
            else ConsecutiveVisiblePendingSeconds=0;
            // Latest completed engine counters may lag the current camera by
            // one or more frames. Zero means unavailable, never zero-cost GPU.
            auto AddCycles=[](TArray<double>& Out,uint32 Cycles){if(Cycles>0)Out.Add(FPlatformTime::ToMilliseconds(Cycles));};
            AddCycles(GameTimes,GGameThreadTime);AddCycles(RenderTimes,GRenderThreadTime);
            AddCycles(RhiTimes,GRHIThreadTime);AddCycles(GpuTimes,RHIGetGPUFrameCycles());
        }
        if(Now-Started<5)return;
        CSV_EVENT_GLOBAL(TEXT("SEIGE_BENCH_SAMPLE_END:%s"),Views[View].Name);
        TSharedPtr<FJsonObject> Row=MakeShared<FJsonObject>();double Sum=0;
        for(double MS:Frames)Sum+=MS;Frames.Sort();
        Row->SetStringField(TEXT("view"),Views[View].Name);Row->SetNumberField(TEXT("samples"),Frames.Num());
        Row->SetNumberField(TEXT("displayed_zoom"),CameraViewZoom());
        Row->SetNumberField(TEXT("camera_yaw"),CameraYaw);Row->SetNumberField(TEXT("camera_pitch"),CameraPitch);
        Row->SetNumberField(TEXT("simple_terrain_components"),SimpleTerrainComponents);
        Row->SetNumberField(TEXT("synchronous_setup_seconds"),SynchronousSetupSeconds);
        Row->SetNumberField(TEXT("scenery_ready_wall_seconds"),SceneryReadySeconds);
        Row->SetNumberField(TEXT("scenery_wait_seconds"),FMath::Max(0.,SceneryReadySeconds-SynchronousSetupSeconds));
        Row->SetNumberField(TEXT("initial_pending_scenery_cells"),InitialPendingSceneryCells);
        Row->SetNumberField(TEXT("pending_scenery_cells_at_sample_start"),PendingSceneryAtSampleStart);
        Row->SetNumberField(TEXT("pending_scenery_cells_max"),MaxPendingSceneryCells);
        Row->SetNumberField(TEXT("pending_scenery_cells_mean"),Frames.Num()?PendingSceneryCellSum/Frames.Num():0);
        Row->SetNumberField(TEXT("pending_scenery_cells_at_sample_end"),PendingSceneryCells());
        Row->SetNumberField(TEXT("pending_visible_cells_max"),MaxPendingVisible);
        Row->SetNumberField(TEXT("pending_near_cells_max"),MaxPendingNear);
        Row->SetNumberField(TEXT("frames_with_pending_visible_cells"),FramesWithPendingVisible);
        Row->SetNumberField(TEXT("max_consecutive_visible_pending_seconds"),MaxConsecutiveVisiblePendingSeconds);
        Row->SetBoolField(TEXT("initial_visibility_counters_available"),InitialPendingVisible>=0);
        Row->SetNumberField(TEXT("initial_pending_visible_cells"),InitialPendingVisible);
        Row->SetNumberField(TEXT("initial_pending_near_cells"),InitialPendingNear);
        Row->SetBoolField(TEXT("initial_visible_coverage_ready"),InitialVisibleReadySeconds>=0);
        Row->SetBoolField(TEXT("initial_near_coverage_ready"),InitialNearReadySeconds>=0);
        if(InitialVisibleReadySeconds>=0)
        {
            Row->SetNumberField(TEXT("initial_visible_ready_wall_seconds"),InitialVisibleReadySeconds);
            Row->SetNumberField(TEXT("initial_visible_wait_seconds"),FMath::Max(0.,InitialVisibleReadySeconds-SynchronousSetupSeconds));
        }
        if(InitialNearReadySeconds>=0)
        {
            Row->SetNumberField(TEXT("initial_near_ready_wall_seconds"),InitialNearReadySeconds);
            Row->SetNumberField(TEXT("initial_near_wait_seconds"),FMath::Max(0.,InitialNearReadySeconds-SynchronousSetupSeconds));
        }
        if(const auto* Edge=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge")))Row->SetNumberField(TEXT("nanite_max_pixels_per_edge"),Edge->GetFloat());
        for(const TCHAR* CVar:{TEXT("r.Shadow.Virtual.SMRT.RayCountDirectional"),TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayDirectional")})
            if(const auto* Value=IConsoleManager::Get().FindConsoleVariable(CVar))Row->SetNumberField(CVar,Value->GetInt());
        Row->SetNumberField(TEXT("sample_seconds"),Sum/1000);
        Row->SetNumberField(TEXT("simulation_seconds_advanced"),Sim.Time-SampleSimulationStart);
        Row->SetNumberField(TEXT("effective_simulation_speed"),Sum>0?(Sim.Time-SampleSimulationStart)*1000/Sum:0);
        Row->SetNumberField(TEXT("mean_fps"),Sum>0?Frames.Num()*1000/Sum:0);
        Row->SetNumberField(TEXT("mean_frame_ms"),Frames.Num()?Sum/Frames.Num():0);
        Row->SetNumberField(TEXT("p95_frame_ms"),Frames.Num()?Frames[FMath::Clamp(FMath::CeilToInt(Frames.Num()*.95)-1,0,Frames.Num()-1)]:0);
        Row->SetNumberField(TEXT("p99_frame_ms"),Frames.Num()?Frames[FMath::Clamp(FMath::CeilToInt(Frames.Num()*.99)-1,0,Frames.Num()-1)]:0);
        Row->SetNumberField(TEXT("max_frame_ms"),Frames.Num()?Frames.Last():0);
        auto Timings=MakeShared<FJsonObject>();
        Timings->SetObjectField(TEXT("wall"),TimingSummary(Frames));
        Timings->SetObjectField(TEXT("game"),TimingSummary(GameTimes));
        Timings->SetObjectField(TEXT("render"),TimingSummary(RenderTimes));
        Timings->SetObjectField(TEXT("rhi"),TimingSummary(RhiTimes));
        Timings->SetObjectField(TEXT("gpu"),TimingSummary(GpuTimes));Row->SetObjectField(TEXT("timings"),Timings);
        Results.Add(MakeShared<FJsonValueObject>(Row));
        UE_LOG(LogTemp,Display,TEXT("GRAPHICS_BENCHMARK %s %s: %.2f FPS, %d samples over %.3f seconds"),*Name,Views[View].Name,Sum>0?Frames.Num()*1000/Sum:0,Frames.Num(),Sum/1000);
        Phase=EPhase::Capture;return;
    }
    if(Phase==EPhase::Capture)
    {
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Benchmark"),Name);
        IFileManager::Get().MakeDirectory(*Directory,true);
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,FString(Views[View].Name)+TEXT(".png")),true,false);
        Phase=EPhase::AwaitCapture;Started=Now;return;
    }
    // Keep the old camera until its post-sample screenshot has been processed.
    // Screenshot cost is neither sampled nor charged to the next view's warmup.
    if(FScreenshotRequest::IsScreenshotRequested())
    {
        if(Now-Started>30)
        {UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK screenshot timed out"));View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);}
        return;
    }
    View=SelectedView==INDEX_NONE?View+1:UE_ARRAY_COUNT(Views);
    if(SelectedView==INDEX_NONE&&View>=StandardViews)View=UE_ARRAY_COUNT(Views);
    if(View<UE_ARRAY_COUNT(Views))
    {
        BeginView();return;
    }
    TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("name"),Name);Report->SetArrayField(TEXT("views"),Results);
    Report->SetStringField(TEXT("selected_view"),SelectedView==INDEX_NONE?TEXT("all"):Views[SelectedView].Name);
    Report->SetNumberField(TEXT("schema_version"),4);
    Report->SetNumberField(TEXT("simulation_speed"),Speed);Report->SetNumberField(TEXT("simulation_seconds"),Sim.Time);
    Report->SetBoolField(TEXT("diagnostic_developed_neighbors"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkDevelopedNeighbors")));
    Report->SetNumberField(TEXT("simulated_neighbor_count"),Neighbors.Num());
    int32 NeighborWorkers=0,NeighborBuildings=0;
    for(const auto& Neighbor:Neighbors){NeighborWorkers+=Neighbor.Sim.Workers.Bodies.Num();NeighborBuildings+=Neighbor.Sim.Buildings.Num();}
    Report->SetNumberField(TEXT("neighbor_worker_bodies"),NeighborWorkers);Report->SetNumberField(TEXT("neighbor_buildings"),NeighborBuildings);
    Report->SetStringField(TEXT("camera_mode"),Travel?TEXT("travel"):Orbit?TEXT("orbit"):TEXT("static"));
    Report->SetNumberField(TEXT("travel_meters_per_second"),Travel?36:0);
    Report->SetStringField(TEXT("camera_path_version"),TEXT("five-views-v1"));
    Report->SetNumberField(TEXT("orbit_degrees_per_second"),Orbit?72:0);
    Report->SetNumberField(TEXT("orbit_pitch_amplitude_degrees"),Orbit?8:0);
    Report->SetNumberField(TEXT("warmup_seconds"),4);Report->SetNumberField(TEXT("sample_seconds_per_view"),5);
    Report->SetStringField(TEXT("warmup_policy"),TEXT("initial-scenery-ready-then-4s-settling"));
    Report->SetStringField(TEXT("streaming_sample_policy"),Travel?TEXT("Travel includes live camera-driven streaming at 36m/s; visible/near backlog and pending duration reported, no sample reset."):Orbit?TEXT("Orbit includes live camera-driven streaming; pending cells reported, no sample reset."):TEXT("Static samples require complete scenery."));
    Report->SetNumberField(TEXT("scenery_readiness_timeout_seconds"),120);
    Report->SetBoolField(TEXT("diagnostic_simple_terrain"),SimpleTerrain);
    Report->SetBoolField(TEXT("diagnostic_gpu_profile"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkGPUProfile")));
    Report->SetStringField(TEXT("thread_timing_method"),TEXT("Latest completed RenderTimer counters exclude idle; GPU uses RHIGetGPUFrameCycles(0). Counters can lag camera samples, and repeated GPU readback values are not de-duplicated. Zero counters are omitted and availability/sample counts reported. These distributions are bottleneck evidence, not synchronized CPU/GPU frame traces."));
    Report->SetNumberField(TEXT("grass_shadow_distance_m"),GrassShadowDistanceMeters);
    Report->SetNumberField(TEXT("grass_programmable_distance_m"),GrassProgrammableDistanceMeters);
    Report->SetNumberField(TEXT("configured_nanite_max_pixels_per_edge"),NaniteMaxPixelsPerEdge);
    Report->SetBoolField(TEXT("diagnostic_nanite_baseline"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkNaniteBaseline")));
    Report->SetBoolField(TEXT("diagnostic_clearing_view"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkClearing")));
    Report->SetBoolField(TEXT("grass_distance_field_lighting"),GrassDistanceFieldLighting);
    Report->SetNumberField(TEXT("ground_cover_candidates"),GroundCoverCandidates);
    Report->SetStringField(TEXT("profile"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkV05Epic"))?TEXT("v0.5 Epic reference"):TEXT("Medium"));
    const bool WinterFixture=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkWinter"));
    Report->SetBoolField(TEXT("diagnostic_winter"),WinterFixture);
    Report->SetStringField(TEXT("lighting_fixture"),WinterFixture?TEXT("Midwinter, midday at benchmark start; advances at 10x during measurements"):TEXT("Spring day 1, midday at benchmark start; advances at 10x during measurements"));
    Report->SetNumberField(TEXT("season_index_at_finish"),ScenarioCalendar.Sample().SeasonIndex);
    Report->SetNumberField(TEXT("snow_coverage_at_finish"),SnowCoverage());
    Report->SetNumberField(TEXT("snowflake_instances_at_finish"),Snowflakes&&Snowflakes->IsVisible()?Snowflakes->GetInstanceCount():0);
    Report->SetNumberField(TEXT("calendar_elapsed_seconds_at_finish"),ScenarioCalendar.ElapsedMicroseconds()/1000000.);
    Report->SetBoolField(TEXT("sky_realtime_capture"),SkyRealtimeCapture);
    Report->SetNumberField(TEXT("fog_density"),FogDensity);
    Report->SetNumberField(TEXT("atmosphere_mie_scale"),AtmosphereMieScale);
    Report->SetNumberField(TEXT("atmosphere_aerial_perspective_scale"),AtmosphereAerialPerspectiveScale);
    Report->SetNumberField(TEXT("sun_source_angle"),SunSourceAngle);
    Report->SetNumberField(TEXT("cloud_shadow_resolution_scale"),CloudShadowResolutionScale);
    Report->SetBoolField(TEXT("diagnostic_sward_hidden"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkHideSward")));
    Report->SetBoolField(TEXT("diagnostic_sward_opaque"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkOpaqueSward")));
    Report->SetBoolField(TEXT("diagnostic_sward_only_a"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkSwardOnlyA")));
    Report->SetBoolField(TEXT("diagnostic_sward_only_b"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkSwardOnlyB")));
    if(GroundCover&&FParse::Param(FCommandLine::Get(),TEXT("BenchmarkDumpSward")))
    {
        // Opt-in post-sample evidence: inspect the actual committed transforms,
        // not a reconstruction of the random generator. No measured frames use
        // this path and normal release reports contain no instance dump.
        auto VectorJson=[](FVector V){return TArray<TSharedPtr<FJsonValue>>{
            MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)};};
        const FTransform CameraPose=CameraTransform(CameraViewZoom());
        auto Dump=MakeShared<FJsonObject>();Dump->SetArrayField(TEXT("camera_position"),VectorJson(CameraPose.GetLocation()));
        Dump->SetArrayField(TEXT("camera_forward"),VectorJson(CameraPose.GetUnitAxis(EAxis::X)));
        Dump->SetNumberField(TEXT("camera_fov"),CameraFov);Dump->SetNumberField(TEXT("render_scale"),RenderScale);
        TArray<TSharedPtr<FJsonValue>> Groups;
        TArray<UProceduralMeshComponent*> TerrainComponents;if(Landscape)Landscape->GetComponents(TerrainComponents);
        auto RenderedHeight=[&](FVector2D P,double& Height)
        {
            for(auto* Terrain:TerrainComponents)
            {
                const auto* Section=Terrain->GetProcMeshSection(0);if(!Section||Section->ProcVertexBuffer.IsEmpty())continue;
                const FTransform ToWorld=Terrain->GetComponentTransform();
                const FBox Box=Section->SectionLocalBox.TransformBy(ToWorld);
                if(P.X<Box.Min.X||P.X>Box.Max.X||P.Y<Box.Min.Y||P.Y>Box.Max.Y)continue;
                const int32 Stride=FMath::RoundToInt(FMath::Sqrt(double(Section->ProcVertexBuffer.Num()))),Cells=Stride-1;
                if(Cells<1||Stride*Stride!=Section->ProcVertexBuffer.Num())continue;
                const FVector Origin=ToWorld.TransformPosition(Section->ProcVertexBuffer[0].Position);
                const FVector Across=ToWorld.TransformPosition(Section->ProcVertexBuffer[1].Position)-Origin;
                const FVector Down=ToWorld.TransformPosition(Section->ProcVertexBuffer[Stride].Position)-Origin;
                if(FMath::Abs(Across.Y)>.0001||FMath::Abs(Down.X)>.0001||Across.X<=0||Down.Y<=0)continue;
                const int32 X=FMath::Clamp(FMath::FloorToInt((P.X-Origin.X)/Across.X),0,Cells-1),Y=FMath::Clamp(FMath::FloorToInt((P.Y-Origin.Y)/Down.Y),0,Cells-1);
                const int32 First=(Y*Cells+X)*6;if(First+5>=Section->ProcIndexBuffer.Num())continue;
                for(int32 Triangle=0;Triangle<2;++Triangle)
                {
                    const FVector A=ToWorld.TransformPosition(Section->ProcVertexBuffer[Section->ProcIndexBuffer[First+Triangle*3]].Position);
                    const FVector B=ToWorld.TransformPosition(Section->ProcVertexBuffer[Section->ProcIndexBuffer[First+Triangle*3+1]].Position);
                    const FVector C=ToWorld.TransformPosition(Section->ProcVertexBuffer[Section->ProcIndexBuffer[First+Triangle*3+2]].Position);
                    const double Den=(B.Y-C.Y)*(A.X-C.X)+(C.X-B.X)*(A.Y-C.Y);if(FMath::Abs(Den)<.000001)continue;
                    const double U=((B.Y-C.Y)*(P.X-C.X)+(C.X-B.X)*(P.Y-C.Y))/Den;
                    const double V=((C.Y-A.Y)*(P.X-C.X)+(A.X-C.X)*(P.Y-C.Y))/Den,W=1-U-V;
                    if(U>=-.000001&&V>=-.000001&&W>=-.000001){Height=U*A.Z+V*B.Z+W*C.Z;return true;}
                }
            }
            return false;
        };
        double WorstMeshCacheDifference=0;int32 RenderedComparisons=0,MissingRenderedSamples=0;
        TArray<UInstancedStaticMeshComponent*> Components;GroundCover->GetComponents(Components);
        for(auto* Component:Components)
        {
            if(!Component->ComponentHasTag(TEXT("seige_sward"))||Component->ComponentHasTag(TEXT("seige_proxy"))||
                !Component->GetStaticMesh()||Component->Bounds.GetBox().ComputeSquaredDistanceToPoint(CameraPose.GetLocation())>FMath::Square(4000.))continue;
            auto Group=MakeShared<FJsonObject>();UStaticMesh* Mesh=Component->GetStaticMesh();const FBox Bounds=Mesh->GetBoundingBox();
            Group->SetStringField(TEXT("mesh"),Mesh->GetPathName());Group->SetBoolField(TEXT("visible"),Component->IsVisible());
            Group->SetArrayField(TEXT("bounds_min"),VectorJson(Bounds.Min));Group->SetArrayField(TEXT("bounds_max"),VectorJson(Bounds.Max));
            Group->SetArrayField(TEXT("component_position"),VectorJson(Component->GetComponentLocation()));
            TArray<TSharedPtr<FJsonValue>> Instances;
            for(int32 I=0;I<Component->GetInstanceCount();++I)
            {
                FTransform Transform;if(!Component->GetInstanceTransform(I,Transform,true)||
                    FVector::DistSquared(Transform.GetLocation(),CameraPose.GetLocation())>FMath::Square(3000.))continue;
                auto Instance=MakeShared<FJsonObject>();Instance->SetArrayField(TEXT("position"),VectorJson(Transform.GetLocation()));
                Instance->SetArrayField(TEXT("scale"),VectorJson(Transform.GetScale3D()));const FQuat Q=Transform.GetRotation();
                Instance->SetArrayField(TEXT("quaternion"),{MakeShared<FJsonValueNumber>(Q.X),MakeShared<FJsonValueNumber>(Q.Y),MakeShared<FJsonValueNumber>(Q.Z),MakeShared<FJsonValueNumber>(Q.W)});
                Instance->SetNumberField(TEXT("pivot_ground_z"),GroundHeight(FVector2D(Transform.GetLocation())/RenderScale)*RenderScale);
                double Rendered=0;
                if(RenderedHeight(FVector2D(Transform.GetLocation()),Rendered))
                {
                    const double Difference=Rendered-GroundHeight(FVector2D(Transform.GetLocation())/RenderScale)*RenderScale;
                    Instance->SetNumberField(TEXT("rendered_ground_z"),Rendered);Instance->SetNumberField(TEXT("rendered_minus_cache_z"),Difference);
                    WorstMeshCacheDifference=FMath::Max(WorstMeshCacheDifference,FMath::Abs(Difference));++RenderedComparisons;
                }
                else ++MissingRenderedSamples;
                Instances.Add(MakeShared<FJsonValueObject>(Instance));
            }
            if(!Instances.IsEmpty()){Group->SetArrayField(TEXT("instances"),Instances);Groups.Add(MakeShared<FJsonValueObject>(Group));}
        }
        Dump->SetNumberField(TEXT("rendered_triangle_comparisons"),RenderedComparisons);
        Dump->SetNumberField(TEXT("missing_rendered_triangle_samples"),MissingRenderedSamples);
        Dump->SetNumberField(TEXT("maximum_absolute_rendered_cache_difference_cm"),WorstMeshCacheDifference);
        Dump->SetArrayField(TEXT("groups"),Groups);Report->SetObjectField(TEXT("diagnostic_sward_instances"),Dump);
    }
    int32 Width=0,Height=0;if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->GetViewportSize(Width,Height);
    Report->SetNumberField(TEXT("width"),Width);Report->SetNumberField(TEXT("height"),Height);Report->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    auto Quality=MakeShared<FJsonObject>();
    for(const TCHAR* CVar:{TEXT("sg.ResolutionQuality"),TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality"),TEXT("sg.LandscapeQuality"),TEXT("r.ScreenPercentage"),TEXT("r.ScreenPercentage.Default.Desktop.Mode"),TEXT("r.SecondaryScreenPercentage.GameViewport"),TEXT("r.Nanite.MaxPixelsPerEdge"),TEXT("r.Nanite.PrimaryRaster.TimeBudgetMs"),TEXT("r.Shadow.Virtual.SMRT.RayCountDirectional"),TEXT("r.Shadow.Virtual.SMRT.SamplesPerRayDirectional"),TEXT("r.VSync"),TEXT("t.MaxFPS"),TEXT("r.DynamicRes.OperationMode")})
        if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(CVar))Quality->SetNumberField(CVar,Variable->GetFloat());
    Quality->SetNumberField(TEXT("runtime_resolution_quality"),Scalability::GetQualityLevels().ResolutionQuality);
    for(const auto& Pair:MediumRenderSettings)if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(*Pair.Key))Quality->SetNumberField(Pair.Key,Variable->GetFloat());
    if(const auto* Method=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))Quality->SetNumberField(TEXT("r.AntiAliasingMethod"),Method->GetInt());
    Report->SetObjectField(TEXT("quality"),Quality);
    Report->SetStringField(TEXT("method"),TEXT("Wall frame timings; named benchmark profile and 100 percent resolution quality, never saved; fresh player core deployed through normal construction at origin, then runs at10x with eight empty neighbors; wait for initial required scenery cells (120s watchdog), then at least 4s settling and at least 5s of complete frame intervals. Orbit and travel samples include camera-driven streaming without resetting; travel translates 36m/s. Both report pending-cell counts and visible/near backlogs, including longest consecutive visible backlog. Initial visible/near-ready times use initialized CPU residency counters and are recorded before the full-ready wait; they are not GPU upload fences or pixel coverage. Static samples require complete scenery. Setup/wait duration is separate. Older schema-2 baselines started 4s warmup directly after synchronous setup. Screenshot cost is excluded."));
    FString Json;FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json));
    if(!FFileHelper::SaveStringToFile(Json,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("GraphicsBenchmark-")+Name+TEXT(".json"))))
    {UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK report write failed"));FPlatformMisc::RequestExitWithStatus(false,1);return;}
#if CSV_PROFILER
    if(FCsvProfiler::IsCapturing())
    {
        CsvWrite=FCsvProfiler::Get()->EndCapture();CsvWriteStarted=FPlatformTime::Seconds();
        if(CsvWrite.IsValid())return;
    }
#endif
    FPlatformMisc::RequestExit(false);
}
