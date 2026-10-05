#include "SeigeGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Scalability.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

bool ASeigeGameMode::LoadGraphicsSettings()
{
    FString Json;
    TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Json,*FPaths::Combine(DataDirectory(TEXT("Graphics")),TEXT("scene.json")))||
       !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root)
    { Error=TEXT("Cannot read Graphics/scene.json");return false; }
    double Version=0;
    if(!Root->TryGetNumberField(TEXT("version"),Version)||Version!=1){Error=TEXT("Unsupported Graphics/scene.json version");return false;}
    auto Read=[&](const TCHAR* Key,double Min,double Max,float& Value)
    {
        double Number=0;
        if(!Root->TryGetNumberField(Key,Number)||!FMath::IsFinite(Number)||Number<Min||Number>Max)
        {Error=FString::Printf(TEXT("Invalid Graphics/scene.json value: %s"),Key);return false;}
        Value=Number;return true;
    };
    float Forest=0,NearForest=0,GroundCandidates=0,TerrainResolution=0,RidgeX=0,RidgeY=0,NeighborForest=0,StreamCells=0;
    if(!Read(TEXT("world_centimeters_per_unit"),1,20,RenderScale)||
       !Read(TEXT("nanite_max_pixels_per_edge"),.5,4,NaniteMaxPixelsPerEdge)||
       !Read(TEXT("nanite_survey_pixels_per_edge"),.5,4,NaniteSurveyPixelsPerEdge)||
       !Read(TEXT("nanite_survey_start_zoom"),5000,60000,NaniteSurveyStartZoom)||
       !Read(TEXT("nanite_survey_end_zoom"),10000,200000,NaniteSurveyEndZoom)||
       !Read(TEXT("camera_fov"),35,80,CameraFov)||!Read(TEXT("camera_pitch"),5,85,CameraPitch)||
       !Read(TEXT("minimum_camera_pitch"),5,25,MinimumCameraPitch)||!Read(TEXT("maximum_camera_pitch"),60,85,MaximumCameraPitch)||
       !Read(TEXT("camera_ground_clearance_cm"),100,500,CameraGroundClearance)||
       !Read(TEXT("sun_intensity"),.1,20,SunIntensity)||!Read(TEXT("sky_intensity"),.1,5,SkyIntensity)||
       !Read(TEXT("cloud_shadow_strength"),0,1,CloudShadowStrength)||
       !Read(TEXT("sun_source_angle"),.1,5,SunSourceAngle)||
       !Read(TEXT("sun_elevation_degrees"),15,80,SunElevation)||
       !Read(TEXT("exposure_bias"),-2,2,ExposureBias)||
       !Read(TEXT("color_saturation"),.5,1.5,ColorSaturation)||
       !Read(TEXT("ambient_occlusion_intensity"),0,1,AmbientOcclusionIntensity)||
       !Read(TEXT("cloud_shadow_resolution_scale"),.25,2,CloudShadowResolutionScale)||
       !Read(TEXT("fog_density"),0,.01,FogDensity)||
       !Read(TEXT("fog_start_distance_m"),0,10000,FogStartDistanceMeters)||
       !Read(TEXT("atmosphere_mie_scale"),0,2,AtmosphereMieScale)||
       !Read(TEXT("atmosphere_aerial_perspective_scale"),0,3,AtmosphereAerialPerspectiveScale)||
       !Read(TEXT("bloom_intensity"),0,1,BloomIntensity)||
       !Read(TEXT("camera_yaw"),-360,360,CameraYaw)||!Read(TEXT("default_zoom"),900,20000,DefaultZoom)||
       !Read(TEXT("minimum_zoom"),60,2000,MinimumZoom)||!Read(TEXT("forest_candidates"),1000,200000,Forest)||
       !Read(TEXT("maximum_zoom"),180000,720000,MaximumZoom)||
       !Read(TEXT("camera_zoom_response"),1,30,CameraZoomResponse)||
       !Read(TEXT("near_forest_candidates"),100,30000,NearForest)||
       !Read(TEXT("grass_shadow_distance_m"),0,500,GrassShadowDistanceMeters)||
       !Read(TEXT("grass_detail_distance_m"),10,150,GrassDetailDistanceMeters)||
       !Read(TEXT("grass_lod_transition_m"),10,200,GrassLodTransitionMeters)||
       !Read(TEXT("grass_stream_radius_m"),150,1200,GrassStreamRadiusMeters)||
       !Read(TEXT("grass_stream_budget_ms"),.5,8,GrassStreamBudgetMs)||
       !Read(TEXT("grass_stream_cells_per_frame"),1,8,StreamCells)||
       !Read(TEXT("forest_detail_distance_m"),75,1000,ForestDetailDistanceMeters)||
       !Read(TEXT("forest_lod_transition_m"),20,500,ForestLodTransitionMeters)||
       !Read(TEXT("grass_programmable_distance_m"),0,900,GrassProgrammableDistanceMeters)||
       !Read(TEXT("neighboring_forest_candidates_per_sector"),0,12000,NeighborForest)||
       !Read(TEXT("region_map_zoom"),60000,240000,RegionMapZoom)||
       !Read(TEXT("region_map_transition_width"),5000,60000,RegionMapTransitionWidth)||
       !Read(TEXT("orbit_yaw_degrees_per_pixel"),.05,2,OrbitYawPerPixel)||
       !Read(TEXT("orbit_pitch_degrees_per_pixel"),.05,2,OrbitPitchPerPixel)||
       !Read(TEXT("ground_cover_candidates"),10000,400000,GroundCandidates)||
       !Read(TEXT("grass_scale_min"),.1,3,GrassScaleMin)||!Read(TEXT("grass_scale_max"),.1,3,GrassScaleMax)||
       !Read(TEXT("detailed_terrain_resolution"),512,1024,TerrainResolution)||
       !Read(TEXT("rolling_terrain_wavelength"),1000,10000,RollingTerrainWavelength)||
       !Read(TEXT("rolling_terrain_amplitude"),0,1000,RollingTerrainAmplitude)||
       !Read(TEXT("micro_terrain_wavelength"),100,1000,MicroTerrainWavelength)||
       !Read(TEXT("micro_terrain_amplitude"),0,50,MicroTerrainAmplitude)||
       !Read(TEXT("core_pad_inner_ratio"),1,2,CorePadInnerRatio)||
       !Read(TEXT("core_pad_outer_ratio"),1.1,4,CorePadOuterRatio)||
       !Read(TEXT("ridge_center_x"),-90000,90000,RidgeX)||!Read(TEXT("ridge_center_y"),-90000,90000,RidgeY)||
       !Read(TEXT("ridge_angle_degrees"),-360,360,RidgeAngleDegrees)||
       !Read(TEXT("ridge_width"),500,10000,RidgeWidth)||!Read(TEXT("ridge_length"),1000,30000,RidgeLength)||
       !Read(TEXT("ridge_height"),0,1500,RidgeHeight))return false;
    if(!Root->TryGetStringField(TEXT("terrain_material"),TerrainMaterialPath)||!TerrainMaterialPath.StartsWith(TEXT("/Game/")))
    {Error=TEXT("Invalid terrain material path");return false;}
    for(const auto& Entry:{TPair<const TCHAR*,FString*>(TEXT("grass_proxy_asset"),&GrassProxyAsset),TPair<const TCHAR*,FString*>(TEXT("broadleaf_proxy_asset"),&BroadleafProxyAsset),TPair<const TCHAR*,FString*>(TEXT("conifer_proxy_asset"),&ConiferProxyAsset)})
        if(!Root->TryGetStringField(Entry.Key,*Entry.Value)||!Entry.Value->StartsWith(TEXT("/Game/")))
        {Error=TEXT("Invalid scenery proxy asset");return false;}
    if(StreamCells!=FMath::FloorToFloat(StreamCells)||GrassStreamRadiusMeters<=GrassDetailDistanceMeters+GrassLodTransitionMeters)
    {Error=TEXT("Invalid scenery streaming budget or range");return false;}
    GrassStreamCellsPerFrame=FMath::RoundToInt(StreamCells);
    if(!Root->TryGetStringField(TEXT("cloud_material"),CloudMaterialPath)||!(CloudMaterialPath.StartsWith(TEXT("/Game/"))||CloudMaterialPath==TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
    {Error=TEXT("Invalid cloud material path");return false;}
    if(CameraPitch<MinimumCameraPitch||CameraPitch>MaximumCameraPitch){Error=TEXT("Default camera pitch lies outside orbit limits");return false;}
    if(MinimumZoom>DefaultZoom){Error=TEXT("Minimum camera zoom exceeds default zoom");return false;}
    if(RegionMapZoom-RegionMapTransitionWidth*.5f<60000||RegionMapZoom+RegionMapTransitionWidth*.5f>=MaximumZoom)
    {Error=TEXT("Region map transition must follow the sector overview and end before maximum zoom");return false;}
    if(NaniteSurveyPixelsPerEdge<NaniteMaxPixelsPerEdge||NaniteSurveyStartZoom<DefaultZoom||NaniteSurveyEndZoom<=NaniteSurveyStartZoom||NaniteSurveyEndZoom>RegionMapZoom-RegionMapTransitionWidth*.5f)
    {Error=TEXT("Invalid Nanite survey transition");return false;}
    if(MinimumZoom>=DefaultZoom*.45f){Error=TEXT("Minimum camera zoom must lie below the close-view transition");return false;}
    if(FMath::FloorToFloat(Forest)!=Forest||FMath::FloorToFloat(NearForest)!=NearForest||FMath::FloorToFloat(GroundCandidates)!=GroundCandidates||FMath::FloorToFloat(NeighborForest)!=NeighborForest){Error=TEXT("Vegetation candidate counts must be integers");return false;}
    if(!Root->TryGetBoolField(TEXT("neighboring_forest_shadow"),NeighborForestShadows)){Error=TEXT("Invalid neighboring forest shadow flag");return false;}
    if(!Root->TryGetBoolField(TEXT("grass_distance_field_lighting"),GrassDistanceFieldLighting)){Error=TEXT("Invalid grass distance-field lighting flag");return false;}
    if(!Root->TryGetBoolField(TEXT("sky_realtime_capture"),SkyRealtimeCapture)){Error=TEXT("Invalid realtime sky capture flag");return false;}
    NeighborForestCandidates=static_cast<int32>(NeighborForest);
    if(GrassScaleMin>GrassScaleMax){Error=TEXT("Minimum grass scale exceeds maximum");return false;}
    if(TerrainResolution!=512&&TerrainResolution!=1024){Error=TEXT("Detailed terrain resolution must be 512 or 1024");return false;}
    if(CorePadOuterRatio<=CorePadInnerRatio){Error=TEXT("Terrain pad outer ratio must exceed its inner ratio");return false;}
    if(1500+RollingTerrainAmplitude+MicroTerrainAmplitude+RidgeHeight>=6000){Error=TEXT("Terrain amplitudes exceed the camera trace bounds");return false;}
    DetailedTerrainResolution=FMath::RoundToInt(TerrainResolution);RidgeCenter=FVector2D(RidgeX,RidgeY);
    GroundCoverCandidates=FMath::RoundToInt(GroundCandidates);
    ForestCandidates=FMath::RoundToInt(Forest);NearForestCandidates=FMath::RoundToInt(NearForest);
    const TSharedPtr<FJsonObject>* Assets=nullptr;
    if(!Root->TryGetObjectField(TEXT("nature_assets"),Assets)){Error=TEXT("Missing nature asset definitions");return false;}
    for(const FString Key:{TEXT("OakA"),TEXT("OakB"),TEXT("PineA"),TEXT("PineB"),TEXT("Shrub"),TEXT("Grass"),TEXT("GrassB"),TEXT("Wildflowers"),TEXT("RockA"),TEXT("RockB")})
    {
        FString Path;
        if(!(*Assets)->TryGetStringField(Key,Path)||!Path.StartsWith(TEXT("/Game/"))){Error=TEXT("Missing or invalid nature asset path: ")+Key;return false;}
        NatureAssets.Add(Key,Path);
    }
    const TSharedPtr<FJsonObject>* Profile=nullptr;
    if(!Root->TryGetObjectField(TEXT("medium_profile"),Profile)){Error=TEXT("Missing Medium graphics profile");return false;}
    auto ReadAntialiasing=[&](const TCHAR* Key,int32& Target)
    {
        FString Value;
        if(!(*Profile)->TryGetStringField(Key,Value)||(Value!=TEXT("taa")&&Value!=TEXT("tsr")))
        {Error=TEXT("Invalid Medium antialiasing method: ")+FString(Key);return false;}
        Target=Value==TEXT("taa")?2:4;return true;
    };
    if(!ReadAntialiasing(TEXT("native_antialiasing"),NativeAntialiasing)||!ReadAntialiasing(TEXT("upscaling_antialiasing"),UpscalingAntialiasing))return false;
    MediumQualityGroups.Reset();MediumRenderSettings.Reset();
    const TSharedPtr<FJsonObject>* Groups=nullptr;
    if(!(*Profile)->TryGetObjectField(TEXT("quality_groups"),Groups)||(*Groups)->Values.Num()!=11){Error=TEXT("Invalid Medium quality groups");return false;}
    for(const TCHAR* Key:{TEXT("ViewDistance"),TEXT("AntiAliasing"),TEXT("Shadow"),TEXT("GlobalIllumination"),TEXT("Reflection"),TEXT("PostProcess"),TEXT("Texture"),TEXT("Effects"),TEXT("Foliage"),TEXT("Shading"),TEXT("Landscape")})
    {
        double Value=0;
        if(!(*Groups)->TryGetNumberField(Key,Value)||!FMath::IsFinite(Value)||Value<0||Value>3||Value!=FMath::FloorToDouble(Value))
        {Error=TEXT("Invalid Medium quality group: ")+FString(Key);return false;}
        MediumQualityGroups.Add(Key,static_cast<int32>(Value));
    }
    const TSharedPtr<FJsonObject>* Settings=nullptr;
    if(!(*Profile)->TryGetObjectField(TEXT("render_settings"),Settings)||(*Settings)->Values.Num()!=9){Error=TEXT("Invalid Medium rendering settings");return false;}
    struct FSettingRange{const TCHAR* Name;double Min,Max;bool Integer;};
    for(const auto& Range:{FSettingRange{TEXT("r.TSR.History.ScreenPercentage"),100,200,false},FSettingRange{TEXT("r.TSR.ThinGeometryDetection"),0,1,true},FSettingRange{TEXT("r.TSR.ThinGeometryDetection.Coverage.ShadingRange"),0,3,true},FSettingRange{TEXT("r.TSR.Velocity.WeightClampingSampleCount"),1,8,false},FSettingRange{TEXT("r.Tonemapper.Sharpen"),0,1,false},FSettingRange{TEXT("r.MaxAnisotropy"),4,16,true},FSettingRange{TEXT("r.TemporalAA.Quality"),1,2,true},FSettingRange{TEXT("r.TemporalAAFilterSize"),.5,1,false},FSettingRange{TEXT("r.TemporalAACurrentFrameWeight"),.04,.2,false}})
    {
        double Value=0;
        if(!(*Settings)->TryGetNumberField(Range.Name,Value)||!FMath::IsFinite(Value)||Value<Range.Min||Value>Range.Max||(Range.Integer&&Value!=FMath::FloorToDouble(Value)))
        {Error=TEXT("Invalid Medium rendering setting: ")+FString(Range.Name);return false;}
        MediumRenderSettings.Add(Range.Name,Value);
    }
    // Opt-in benchmark controls recreate the previous lighting setup before
    // BeginPlay creates its components. They never write player preferences.
    if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkV05Epic")))
    {
        NaniteMaxPixelsPerEdge=1.5f;
        FogDensity=.0025f;FogStartDistanceMeters=450;AtmosphereMieScale=1;AtmosphereAerialPerspectiveScale=1;
        BloomIntensity=.15f;SunSourceAngle=2;CloudShadowResolutionScale=2;SkyRealtimeCapture=true;
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("BenchmarkRealtimeSky")))SkyRealtimeCapture=true;
    if(auto* NaniteEdge=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge")))
    {
        // External project data must not override higher-priority command-line,
        // console or platform choices. Avoid a rejected lower-priority Set.
        if((NaniteEdge->GetFlags()&ECVF_SetByMask)<=ECVF_SetByProjectSetting)
            NaniteEdge->Set(NaniteMaxPixelsPerEdge,ECVF_SetByProjectSetting);
    }
    Zoom=DefaultZoom;RenderedZoom=-1;return true;
}
void ASeigeGameMode::ApplyMediumPreset()
{
    auto Levels=Scalability::GetQualityLevels();const float Resolution=Levels.ResolutionQuality>0?Levels.ResolutionQuality:100.f;
    Levels.SetFromSingleQualityLevel(2);Levels.ResolutionQuality=Resolution;
    auto Quality=[&](const TCHAR* Key,int32 Fallback){const int32* Found=MediumQualityGroups.Find(Key);return Found?*Found:Fallback;};
    Levels.ViewDistanceQuality=Quality(TEXT("ViewDistance"),2);Levels.AntiAliasingQuality=Quality(TEXT("AntiAliasing"),3);
    Levels.ShadowQuality=Quality(TEXT("Shadow"),2);Levels.GlobalIlluminationQuality=Quality(TEXT("GlobalIllumination"),2);
    Levels.ReflectionQuality=Quality(TEXT("Reflection"),2);Levels.PostProcessQuality=Quality(TEXT("PostProcess"),2);
    Levels.TextureQuality=Quality(TEXT("Texture"),3);Levels.EffectsQuality=Quality(TEXT("Effects"),2);
    Levels.FoliageQuality=Quality(TEXT("Foliage"),3);Levels.ShadingQuality=Quality(TEXT("Shading"),2);Levels.LandscapeQuality=Quality(TEXT("Landscape"),2);
    Scalability::SetQualityLevels(Levels,true);
    for(const auto& Pair:MediumRenderSettings)if(auto* Variable=IConsoleManager::Get().FindConsoleVariable(*Pair.Key))
        // These complete the custom scalability profile. Keeping the same
        // priority avoids rejected group reapplication when display options change.
        if((Variable->GetFlags()&ECVF_SetByMask)<=ECVF_SetByScalability)Variable->Set(Pair.Value,ECVF_SetByScalability);
    // Native TAA avoids an unnecessary high-cost reconstruction pass at 100%.
    // Keep TSR when the user chooses a smaller internal rendering resolution.
    if(auto* Method=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
        if((Method->GetFlags()&ECVF_SetByMask)<=ECVF_SetByProjectSetting)
            Method->Set(Resolution>=99.9f?NativeAntialiasing:UpscalingAntialiasing,ECVF_SetByProjectSetting);
}
FVector ASeigeGameMode::RenderPosition(FVector2D P,float Offset) const
{
    return FVector(P.X*RenderScale,P.Y*RenderScale,GroundHeight(P)*RenderScale+Offset);
}
FVector2D ASeigeGameMode::CameraPanDirection(float Forward,float Right) const
{
    const double Angle=FMath::DegreesToRadians(CameraYaw);
    return FVector2D(FMath::Cos(Angle)*Forward-FMath::Sin(Angle)*Right,FMath::Sin(Angle)*Forward+FMath::Cos(Angle)*Right).GetClampedToMaxSize(1);
}
float ASeigeGameMode::CameraViewZoom() const
{
    return FMath::Clamp(Camera&&RenderedZoom>=0?RenderedZoom:Zoom,MinimumZoom,MaximumZoom);
}
FTransform ASeigeGameMode::CameraTransform(float ZoomOverride) const
{
    const FVector Target=RenderPosition(FVector2D(CameraCenter),70);
    const double ViewZoom=FMath::Clamp(double(ZoomOverride>=0?ZoomOverride:Zoom),double(MinimumZoom),double(MaximumZoom));
    const double Distance=ViewZoom*RenderScale/(2*FMath::Tan(FMath::DegreesToRadians(CameraFov*.5)));
    // Close zoom lowers the view gradually to show actual ground detail. The
    // chosen orbit angle remains intact, returning as the camera pulls back.
    const double CloseBlend=FMath::SmoothStep(double(MinimumZoom),double(DefaultZoom)*.45,ViewZoom);
    const double Pitch=FMath::Lerp(double(MinimumCameraPitch),double(FMath::Clamp(CameraPitch,MinimumCameraPitch,MaximumCameraPitch)),CloseBlend);
    const FRotator Aim(-Pitch,CameraYaw,0);
    FVector Position=Target-Aim.Vector()*Distance;
    const FVector2D Logical(Position.X/RenderScale,Position.Y/RenderScale);
    if(FMath::Abs(Logical.X)<=Sim.WorldHalfSize*3&&FMath::Abs(Logical.Y)<=Sim.WorldHalfSize*3)
        Position.Z=FMath::Max(Position.Z,GroundHeight(Logical)*RenderScale+CameraGroundClearance);
    return FTransform((Target-Position).Rotation(),Position);
}
void ASeigeGameMode::UpdateCamera(float DeltaSeconds)
{
    Zoom=FMath::Clamp(Zoom,MinimumZoom,MaximumZoom);
    if(DeltaSeconds<=0||!FMath::IsFinite(RenderedZoom)||RenderedZoom<MinimumZoom)RenderedZoom=Zoom;
    else
    {
        const float Alpha=1-FMath::Exp(-CameraZoomResponse*FMath::Clamp(DeltaSeconds,0.f,.25f));
        RenderedZoom=FMath::Exp(FMath::Lerp(FMath::Loge(RenderedZoom),FMath::Loge(Zoom),Alpha));
        if(FMath::Abs(RenderedZoom-Zoom)<=FMath::Max(.01f,Zoom*.00001f))RenderedZoom=Zoom;
    }
    if(!Camera)return;
    Camera->SetActorTransform(CameraTransform(RenderedZoom));
    Camera->GetCameraComponent()->SetFieldOfView(CameraFov);
    // Keep the detailed ground view unchanged. Broad surveys allow a coarser
    // Nanite screen-space target without dropping any grass/tree instances.
    static IConsoleVariable* Edge=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge"));
    if(Edge)
        if((Edge->GetFlags()&ECVF_SetByMask)<=ECVF_SetByProjectSetting)
        {
            const float Alpha=FMath::SmoothStep(NaniteSurveyStartZoom,NaniteSurveyEndZoom,RenderedZoom);
            const float Target=FParse::Param(FCommandLine::Get(),TEXT("BenchmarkV05Epic"))?NaniteMaxPixelsPerEdge:FMath::Lerp(NaniteMaxPixelsPerEdge,NaniteSurveyPixelsPerEdge,Alpha);
            if(!FMath::IsNearlyEqual(Edge->GetFloat(),Target,.005f))Edge->Set(Target,ECVF_SetByProjectSetting);
        }
}
bool ASeigeGameMode::TraceGroundRay(const FVector& WorldOrigin,const FVector& WorldDirection,FVector& Hit) const
{
    if(WorldOrigin.ContainsNaN()||WorldDirection.ContainsNaN()||RenderScale<=0||WorldDirection.IsNearlyZero())return false;
    const FVector Origin=WorldOrigin/RenderScale,Direction=WorldDirection.GetSafeNormal();
    // A low camera can see an uphill surface above its own horizon. Horizontal
    // and upward rays must reach the bounded terrain test, just like downward rays.
    const double Extent=Sim.WorldHalfSize*3;
    double Enter=0,Leave=1.e9;
    const FVector Low(-Extent,-Extent,-6000),High(Extent,Extent,6000);
    for(int32 Axis=0;Axis<3;++Axis)
    {
        if(FMath::Abs(Direction[Axis])<1.e-9){if(Origin[Axis]<Low[Axis]||Origin[Axis]>High[Axis])return false;continue;}
        double A=(Low[Axis]-Origin[Axis])/Direction[Axis],B=(High[Axis]-Origin[Axis])/Direction[Axis];
        if(A>B)Swap(A,B);Enter=FMath::Max(Enter,A);Leave=FMath::Min(Leave,B);
    }
    if(Enter>=Leave||Leave<0)return false;
    auto Gap=[&](double T){const FVector P=Origin+Direction*T;return P.Z-GroundHeight(FVector2D(P));};
    double PreviousT=Enter;
    if(Gap(Enter)<0)return false;
    // Walk at less than half a terrain triangle's horizontal width, then refine the
    // first surface crossing. This finds the visible ridge, not ground behind it.
    int32 FinestResolution=DetailedTerrainResolution;
    for(const auto& Tile:TerrainTiles)FinestResolution=FMath::Max(FinestResolution,Tile.Resolution);
    const double FinestSpacing=Sim.WorldHalfSize*2/FMath::Max(1,FinestResolution);
    const double Step=FMath::Clamp(FinestSpacing*.4/FMath::Max(FMath::Abs(Direction.X),FMath::Max(FMath::Abs(Direction.Y),.05)),1.,500.);
    for(double T=FMath::Min(Enter+Step,Leave);T<=Leave;T=FMath::Min(T+Step,Leave))
    {
        const double CurrentGap=Gap(T);
        if(CurrentGap<=0)
        {
            double A=PreviousT,B=T;
            for(int32 I=0;I<22;++I){const double Mid=(A+B)*.5;if(Gap(Mid)>0)A=Mid;else B=Mid;}
            Hit=(Origin+Direction*((A+B)*.5))*RenderScale;return true;
        }
        if(T>=Leave)break;
        PreviousT=T;
    }
    return false;
}
