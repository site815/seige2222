#include "SeigeGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"

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
    float Forest=0,NearForest=0,GroundCandidates=0,TerrainResolution=0,RidgeX=0,RidgeY=0,NeighborForest=0;
    if(!Read(TEXT("world_centimeters_per_unit"),1,20,RenderScale)||
       !Read(TEXT("nanite_max_pixels_per_edge"),.5,4,NaniteMaxPixelsPerEdge)||
       !Read(TEXT("camera_fov"),35,80,CameraFov)||!Read(TEXT("camera_pitch"),5,85,CameraPitch)||
       !Read(TEXT("minimum_camera_pitch"),5,25,MinimumCameraPitch)||!Read(TEXT("maximum_camera_pitch"),60,85,MaximumCameraPitch)||
       !Read(TEXT("camera_ground_clearance_cm"),100,500,CameraGroundClearance)||
       !Read(TEXT("sun_intensity"),.1,20,SunIntensity)||!Read(TEXT("sky_intensity"),.1,5,SkyIntensity)||
       !Read(TEXT("cloud_shadow_strength"),0,1,CloudShadowStrength)||
       !Read(TEXT("camera_yaw"),-360,360,CameraYaw)||!Read(TEXT("default_zoom"),900,20000,DefaultZoom)||
       !Read(TEXT("minimum_zoom"),60,2000,MinimumZoom)||!Read(TEXT("forest_candidates"),1000,200000,Forest)||
       !Read(TEXT("near_forest_candidates"),100,30000,NearForest)||
       !Read(TEXT("grass_shadow_distance_m"),0,500,GrassShadowDistanceMeters)||
       !Read(TEXT("grass_programmable_distance_m"),0,900,GrassProgrammableDistanceMeters)||
       !Read(TEXT("neighboring_forest_candidates_per_sector"),0,12000,NeighborForest)||
       !Read(TEXT("region_map_zoom"),20000,90000,RegionMapZoom)||
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
    if(!Root->TryGetStringField(TEXT("cloud_material"),CloudMaterialPath)||!(CloudMaterialPath.StartsWith(TEXT("/Game/"))||CloudMaterialPath==TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
    {Error=TEXT("Invalid cloud material path");return false;}
    if(CameraPitch<MinimumCameraPitch||CameraPitch>MaximumCameraPitch){Error=TEXT("Default camera pitch lies outside orbit limits");return false;}
    if(MinimumZoom>DefaultZoom){Error=TEXT("Minimum camera zoom exceeds default zoom");return false;}
    if(MinimumZoom>=DefaultZoom*.45f){Error=TEXT("Minimum camera zoom must lie below the close-view transition");return false;}
    if(FMath::FloorToFloat(Forest)!=Forest||FMath::FloorToFloat(NearForest)!=NearForest||FMath::FloorToFloat(GroundCandidates)!=GroundCandidates||FMath::FloorToFloat(NeighborForest)!=NeighborForest){Error=TEXT("Vegetation candidate counts must be integers");return false;}
    if(!Root->TryGetBoolField(TEXT("neighboring_forest_shadow"),NeighborForestShadows)){Error=TEXT("Invalid neighboring forest shadow flag");return false;}
    if(!Root->TryGetBoolField(TEXT("grass_distance_field_lighting"),GrassDistanceFieldLighting)){Error=TEXT("Invalid grass distance-field lighting flag");return false;}
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
    if(auto* NaniteEdge=IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.MaxPixelsPerEdge")))
    {
        // External project data must not override higher-priority command-line,
        // console or platform choices. Avoid a rejected lower-priority Set.
        if((NaniteEdge->GetFlags()&ECVF_SetByMask)<=ECVF_SetByProjectSetting)
            NaniteEdge->Set(NaniteMaxPixelsPerEdge,ECVF_SetByProjectSetting);
    }
    Zoom=DefaultZoom;return true;
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
FTransform ASeigeGameMode::CameraTransform() const
{
    const FVector Target=RenderPosition(FVector2D(CameraCenter),70);
    const double Distance=FMath::Clamp(double(Zoom),double(MinimumZoom),Sim.WorldHalfSize*12)*RenderScale/(2*FMath::Tan(FMath::DegreesToRadians(CameraFov*.5)));
    // Close zoom lowers the view gradually to show actual ground detail. The
    // chosen orbit angle remains intact, returning as the camera pulls back.
    const double CloseBlend=FMath::SmoothStep(double(MinimumZoom),double(DefaultZoom)*.45,double(Zoom));
    const double Pitch=FMath::Lerp(double(MinimumCameraPitch),double(FMath::Clamp(CameraPitch,MinimumCameraPitch,MaximumCameraPitch)),CloseBlend);
    const FRotator Aim(-Pitch,CameraYaw,0);
    FVector Position=Target-Aim.Vector()*Distance;
    const FVector2D Logical(Position.X/RenderScale,Position.Y/RenderScale);
    if(FMath::Abs(Logical.X)<=Sim.WorldHalfSize*3&&FMath::Abs(Logical.Y)<=Sim.WorldHalfSize*3)
        Position.Z=FMath::Max(Position.Z,GroundHeight(Logical)*RenderScale+CameraGroundClearance);
    return FTransform((Target-Position).Rotation(),Position);
}
void ASeigeGameMode::UpdateCamera()
{
    if(!Camera)return;
    Camera->SetActorTransform(CameraTransform());
    Camera->GetCameraComponent()->SetFieldOfView(CameraFov);
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
