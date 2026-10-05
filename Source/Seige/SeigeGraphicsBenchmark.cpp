#include "SeigeGameMode.h"
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
        {TEXT("boundary"),FVector(29000,0,0),0,60,14000}};
    static TWeakObjectPtr<ASeigeGameMode> BenchmarkOwner;
    static int32 View=-1;
    static int32 SelectedView=INDEX_NONE;
    enum class EPhase {AwaitScenery,Warmup,Sample,Capture,AwaitCapture};
    static EPhase Phase=EPhase::Warmup;
    static double Started=0,Previous=0;
    static FString Name;
    static TArray<double> Frames;
    static TArray<double> GameTimes,RenderTimes,RhiTimes,GpuTimes;
    static bool Orbit=false,SimpleTerrain=false;
    static double MotionStarted=0;
    static double ViewSetupStarted=0,SynchronousSetupSeconds=0,SceneryReadySeconds=0;
    static int32 InitialPendingSceneryCells=0;
    static int32 PendingSceneryAtSampleStart=0,MaxPendingSceneryCells=0;
    static double PendingSceneryCellSum=0;
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
                UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK invalid BenchmarkView='%s'; expected colony, meadow, ground, hills or boundary"),*RequestedView);
                View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);return;
            }
        }
        Orbit=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkOrbit"));
        SimpleTerrain=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkSimpleTerrain"));
        SimpleTerrainComponents=0;
        for(TCHAR& C:Name)if(!FChar::IsAlnum(C)&&C!=TEXT('-')&&C!=TEXT('_'))C=TEXT('_');
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkFullGrassShadows")))GrassShadowDistanceMeters=500;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkFullGrassLighting")))GrassDistanceFieldLighting=true;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkGrassOpaqueBeyond80")))GrassProgrammableDistanceMeters=80;
    }
    auto ApplySwardDiagnostic=[&]()
    {
        if(GroundCover&&FParse::Param(FCommandLine::Get(),TEXT("BenchmarkHideSward")))
        {
            TArray<UInstancedStaticMeshComponent*> Components;GroundCover->GetComponents(Components);
            for(auto* Component:Components)if(Component->IsVisible()&&Component->ComponentHasTag(TEXT("seige_sward")))Component->SetVisibility(false);
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
    auto BeginView=[&]()
    {
        ViewSetupStarted=FPlatformTime::Seconds();
        CameraCenter=Views[View].Center;CameraYaw=Views[View].Yaw;CameraPitch=Views[View].Pitch;Zoom=Views[View].Distance;
        UpdateCamera();RefreshEnvironment();SyncVisuals();
        ApplyViewDiagnostics();
        // Synchronous setup and budgeted scenery generation are both excluded.
        // Settling starts only after the final required cell is installed.
        Frames.Reset();GameTimes.Reset();RenderTimes.Reset();RhiTimes.Reset();GpuTimes.Reset();
        Phase=EPhase::AwaitScenery;Started=Previous=MotionStarted=FPlatformTime::Seconds();
        SynchronousSetupSeconds=Started-ViewSetupStarted;SceneryReadySeconds=0;
        InitialPendingSceneryCells=PendingSceneryCells();
        PendingSceneryAtSampleStart=MaxPendingSceneryCells=0;PendingSceneryCellSum=0;
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
        ScenarioSlots.Init(TEXT("empty"),9);ScenarioSlots[4]=TEXT("player");StartScenario();
        if(Screen==TEXT("landing"))ConfirmLanding(FVector2D::ZeroVector);
        const FSeigeBuildingDef* CommandDefinition=Sim.BuildingDefs.Find(Sim.CoreDefinition);
        if(Screen==TEXT("playing")&&!Observer&&CommandDefinition)
            Sim.Tick(CommandDefinition->ConstructionSeconds+Sim.FixedStepSeconds());
        Paused=true;
        const FSeigeBuilding* Command=Sim.Buildings.FindByPredicate([&](const FSeigeBuilding& Building){return Building.DefId==Sim.CoreDefinition;});
        if(Screen!=TEXT("playing")||Observer||!Command||Command->IsConstructing||Command->Health<=0)
        {
            UE_LOG(LogTemp,Error,TEXT("GRAPHICS_BENCHMARK failed to deploy fresh player core: %s"),*Error);
            View=UE_ARRAY_COUNT(Views);FPlatformMisc::RequestExitWithStatus(false,1);return;
        }
        View=SelectedView==INDEX_NONE?0:SelectedView;BeginView();
        return;
    }
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
            Phase=EPhase::Sample;Started=Previous=Now;
            PendingSceneryAtSampleStart=PendingSceneryCells();
            CSV_EVENT_GLOBAL(TEXT("SEIGE_BENCH_SAMPLE_BEGIN:%s"),Views[View].Name);
        }
        return;
    }
    if(Phase==EPhase::Sample)
    {
        if(!Orbit&&!IsSceneryStreamingReady())
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
        if(const auto* Edge=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge")))Row->SetNumberField(TEXT("nanite_max_pixels_per_edge"),Edge->GetFloat());
        Row->SetNumberField(TEXT("sample_seconds"),Sum/1000);
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
    if(View<UE_ARRAY_COUNT(Views))
    {
        BeginView();return;
    }
    TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("name"),Name);Report->SetArrayField(TEXT("views"),Results);
    Report->SetStringField(TEXT("selected_view"),SelectedView==INDEX_NONE?TEXT("all"):Views[SelectedView].Name);
    Report->SetNumberField(TEXT("schema_version"),3);
    Report->SetStringField(TEXT("camera_mode"),Orbit?TEXT("orbit"):TEXT("static"));
    Report->SetStringField(TEXT("camera_path_version"),TEXT("five-views-v1"));
    Report->SetNumberField(TEXT("orbit_degrees_per_second"),Orbit?72:0);
    Report->SetNumberField(TEXT("orbit_pitch_amplitude_degrees"),Orbit?8:0);
    Report->SetNumberField(TEXT("warmup_seconds"),4);Report->SetNumberField(TEXT("sample_seconds_per_view"),5);
    Report->SetStringField(TEXT("warmup_policy"),TEXT("initial-scenery-ready-then-4s-settling"));
    Report->SetStringField(TEXT("streaming_sample_policy"),Orbit?TEXT("Orbit includes live camera-driven streaming; pending cells reported, no sample reset."):TEXT("Static samples require complete scenery."));
    Report->SetNumberField(TEXT("scenery_readiness_timeout_seconds"),120);
    Report->SetBoolField(TEXT("diagnostic_simple_terrain"),SimpleTerrain);
    Report->SetStringField(TEXT("thread_timing_method"),TEXT("Latest completed RenderTimer counters exclude idle; GPU uses RHIGetGPUFrameCycles(0). Counters can lag camera samples, and repeated GPU readback values are not de-duplicated. Zero counters are omitted and availability/sample counts reported. These distributions are bottleneck evidence, not synchronized CPU/GPU frame traces."));
    Report->SetNumberField(TEXT("grass_shadow_distance_m"),GrassShadowDistanceMeters);
    Report->SetNumberField(TEXT("grass_programmable_distance_m"),GrassProgrammableDistanceMeters);
    Report->SetNumberField(TEXT("configured_nanite_max_pixels_per_edge"),NaniteMaxPixelsPerEdge);
    Report->SetBoolField(TEXT("diagnostic_nanite_baseline"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkNaniteBaseline")));
    Report->SetBoolField(TEXT("grass_distance_field_lighting"),GrassDistanceFieldLighting);
    Report->SetNumberField(TEXT("ground_cover_candidates"),GroundCoverCandidates);
    Report->SetStringField(TEXT("profile"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkV05Epic"))?TEXT("v0.5 Epic reference"):TEXT("Medium"));
    Report->SetBoolField(TEXT("sky_realtime_capture"),SkyRealtimeCapture);
    Report->SetNumberField(TEXT("fog_density"),FogDensity);
    Report->SetNumberField(TEXT("atmosphere_mie_scale"),AtmosphereMieScale);
    Report->SetNumberField(TEXT("atmosphere_aerial_perspective_scale"),AtmosphereAerialPerspectiveScale);
    Report->SetNumberField(TEXT("sun_source_angle"),SunSourceAngle);
    Report->SetNumberField(TEXT("cloud_shadow_resolution_scale"),CloudShadowResolutionScale);
    Report->SetBoolField(TEXT("diagnostic_sward_hidden"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkHideSward")));
    int32 Width=0,Height=0;if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->GetViewportSize(Width,Height);
    Report->SetNumberField(TEXT("width"),Width);Report->SetNumberField(TEXT("height"),Height);Report->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    auto Quality=MakeShared<FJsonObject>();
    for(const TCHAR* CVar:{TEXT("sg.ResolutionQuality"),TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality"),TEXT("sg.LandscapeQuality"),TEXT("r.ScreenPercentage"),TEXT("r.ScreenPercentage.Default.Desktop.Mode"),TEXT("r.SecondaryScreenPercentage.GameViewport"),TEXT("r.Nanite.MaxPixelsPerEdge"),TEXT("r.Nanite.PrimaryRaster.TimeBudgetMs"),TEXT("r.VSync"),TEXT("t.MaxFPS"),TEXT("r.DynamicRes.OperationMode")})
        if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(CVar))Quality->SetNumberField(CVar,Variable->GetFloat());
    Quality->SetNumberField(TEXT("runtime_resolution_quality"),Scalability::GetQualityLevels().ResolutionQuality);
    for(const auto& Pair:MediumRenderSettings)if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(*Pair.Key))Quality->SetNumberField(Pair.Key,Variable->GetFloat());
    if(const auto* Method=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))Quality->SetNumberField(TEXT("r.AntiAliasingMethod"),Method->GetInt());
    Report->SetObjectField(TEXT("quality"),Quality);
    Report->SetStringField(TEXT("method"),TEXT("Wall frame timings; named benchmark profile and 100 percent resolution quality, never saved; fresh player core deployed through normal construction at origin, then paused with eight empty neighbors; wait for initial required scenery cells (120s watchdog), then at least 4s settling and at least 5s of complete frame intervals. Orbit samples include camera-driven streaming and report pending-cell mean/max/start/end without resetting; static samples require complete scenery. Setup/wait duration is separate. Older schema-2 baselines started 4s warmup directly after synchronous setup. Screenshot cost is excluded."));
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
