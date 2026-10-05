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
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersion.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Scalability.h"

// Opt-in, repeatable graphics measurement. No player saves or settings are written.
// Each static camera gets four seconds to settle and five seconds of wall-frame samples.
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
    enum class EPhase {Warmup,Sample,Capture,AwaitCapture};
    static EPhase Phase=EPhase::Warmup;
    static double Started=0,Previous=0;
    static FString Name;
    static TArray<double> Frames;
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
        for(TCHAR& C:Name)if(!FChar::IsAlnum(C)&&C!=TEXT('-')&&C!=TEXT('_'))C=TEXT('_');
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkFullGrassShadows")))GrassShadowDistanceMeters=500;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkFullGrassLighting")))GrassDistanceFieldLighting=true;
        if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkGrassOpaqueBeyond80")))GrassProgrammableDistanceMeters=80;
    }
    auto BeginView=[&]()
    {
        CameraCenter=Views[View].Center;CameraYaw=Views[View].Yaw;CameraPitch=Views[View].Pitch;Zoom=Views[View].Distance;
        UpdateCamera();RefreshEnvironment();SyncVisuals();
        if(GroundCover&&FParse::Param(FCommandLine::Get(),TEXT("BenchmarkHideSward")))
        {
            TArray<UHierarchicalInstancedStaticMeshComponent*> Components;GroundCover->GetComponents(Components);
            for(auto* Component:Components)if(Component->ComponentHasTag(TEXT("seige_sward")))Component->SetVisibility(false);
        }
        // Synchronous scene/ground-cover setup is outside this view's warmup.
        Frames.Reset();Phase=EPhase::Warmup;Started=Previous=FPlatformTime::Seconds();
    };
    if(View<0)
    {
        if(RenderClock<2)return;
        // Boot CSV capture and ordinary launches can resolve different saved
        // user settings. Pin the benchmark only; never persist these levels.
        Scalability::FQualityLevels BenchmarkQuality;
        BenchmarkQuality.SetFromSingleQualityLevel(3);
        BenchmarkQuality.ResolutionQuality=100.f;
        Scalability::SetQualityLevels(BenchmarkQuality,true);
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
        View=0;BeginView();
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
    if(Phase==EPhase::Warmup)
    {
        if(Now-Started>=4){Phase=EPhase::Sample;Started=Previous=Now;}
        return;
    }
    if(Phase==EPhase::Sample)
    {
        const double FrameMS=(Now-Previous)*1000;Previous=Now;
        if(FrameMS>0)Frames.Add(FrameMS);
        if(Now-Started<5)return;
        TSharedPtr<FJsonObject> Row=MakeShared<FJsonObject>();double Sum=0;
        for(double MS:Frames)Sum+=MS;Frames.Sort();
        Row->SetStringField(TEXT("view"),Views[View].Name);Row->SetNumberField(TEXT("samples"),Frames.Num());
        Row->SetNumberField(TEXT("sample_seconds"),Sum/1000);
        Row->SetNumberField(TEXT("mean_fps"),Sum>0?Frames.Num()*1000/Sum:0);
        Row->SetNumberField(TEXT("mean_frame_ms"),Frames.Num()?Sum/Frames.Num():0);
        Row->SetNumberField(TEXT("p95_frame_ms"),Frames.Num()?Frames[FMath::Clamp(FMath::CeilToInt(Frames.Num()*.95)-1,0,Frames.Num()-1)]:0);
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
    ++View;
    if(View<UE_ARRAY_COUNT(Views))
    {
        BeginView();return;
    }
    TSharedPtr<FJsonObject> Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("name"),Name);Report->SetArrayField(TEXT("views"),Results);
    Report->SetNumberField(TEXT("grass_shadow_distance_m"),GrassShadowDistanceMeters);
    Report->SetNumberField(TEXT("grass_programmable_distance_m"),GrassProgrammableDistanceMeters);
    Report->SetNumberField(TEXT("configured_nanite_max_pixels_per_edge"),NaniteMaxPixelsPerEdge);
    Report->SetBoolField(TEXT("diagnostic_nanite_baseline"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkNaniteBaseline")));
    Report->SetBoolField(TEXT("grass_distance_field_lighting"),GrassDistanceFieldLighting);
    Report->SetNumberField(TEXT("ground_cover_candidates"),GroundCoverCandidates);
    Report->SetBoolField(TEXT("diagnostic_sward_hidden"),FParse::Param(FCommandLine::Get(),TEXT("BenchmarkHideSward")));
    int32 Width=0,Height=0;if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->GetViewportSize(Width,Height);
    Report->SetNumberField(TEXT("width"),Width);Report->SetNumberField(TEXT("height"),Height);Report->SetStringField(TEXT("engine"),FEngineVersion::Current().ToString());
    auto Quality=MakeShared<FJsonObject>();
    for(const TCHAR* CVar:{TEXT("sg.ResolutionQuality"),TEXT("sg.ViewDistanceQuality"),TEXT("sg.AntiAliasingQuality"),TEXT("sg.ShadowQuality"),TEXT("sg.GlobalIlluminationQuality"),TEXT("sg.ReflectionQuality"),TEXT("sg.PostProcessQuality"),TEXT("sg.TextureQuality"),TEXT("sg.EffectsQuality"),TEXT("sg.FoliageQuality"),TEXT("sg.ShadingQuality"),TEXT("sg.LandscapeQuality"),TEXT("r.ScreenPercentage"),TEXT("r.ScreenPercentage.Default.Desktop.Mode"),TEXT("r.SecondaryScreenPercentage.GameViewport"),TEXT("r.Nanite.MaxPixelsPerEdge"),TEXT("r.Nanite.PrimaryRaster.TimeBudgetMs"),TEXT("r.VSync"),TEXT("t.MaxFPS"),TEXT("r.DynamicRes.OperationMode")})
        if(const auto* Variable=IConsoleManager::Get().FindConsoleVariable(CVar))Quality->SetNumberField(CVar,Variable->GetFloat());
    Quality->SetNumberField(TEXT("runtime_resolution_quality"),Scalability::GetQualityLevels().ResolutionQuality);
    Report->SetObjectField(TEXT("quality"),Quality);
    Report->SetStringField(TEXT("method"),TEXT("Wall frame timings; benchmark-only Epic quality level 3 and 100 percent resolution quality, never saved; fresh player core deployed through normal construction at origin, then paused with eight empty neighbors; at least 4s warmup after synchronous view setup, then at least 5s of complete frame intervals; screenshot requested on a later frame after sampling and completed before advancing camera."));
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
