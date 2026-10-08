#include "SeigeGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureCube.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

bool ASeigeGameMode::LoadWeatherSettings()
{
    FString Raw;TSharedPtr<FJsonObject> Doc;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(DataDirectory(TEXT("Graphics")),TEXT("weather.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Doc)||!Doc)
    {Error=TEXT("Could not read Graphics/weather.json");return false;}
    double Version=0,Count=0;
    if(!Doc->TryGetNumberField(TEXT("version"),Version)||Version!=1){Error=TEXT("Unsupported weather configuration version");return false;}
    auto Number=[&](const TCHAR* Key,double Min,double Max,float& Out){double Value=0;if(!Doc->TryGetNumberField(Key,Value)||!FMath::IsFinite(Value)||Value<Min||Value>Max){Error=FString::Printf(TEXT("weather.%s must be in [%g,%g]"),Key,Min,Max);return false;}Out=float(Value);return true;};
    if(!Number(TEXT("sun_direction_update_degrees"),.01,2,SunDirectionUpdateDegrees)||!Number(TEXT("sun_shadow_update_seconds"),.05,10,SunShadowUpdateSeconds)||!Number(TEXT("night_sky_intensity_fraction"),.05,1,NightSkyFraction)||!Number(TEXT("night_exposure_offset_ev"),0,4,NightExposureOffsetEV)||!Number(TEXT("sunrise_sunset_softness"),.01,.5,SunriseSoftness)||
       !Number(TEXT("winter_accumulation_fraction"),.001,.49,WinterAccumulationFraction)||!Number(TEXT("winter_melt_fraction"),.001,.49,WinterMeltFraction)||
       !Number(TEXT("maximum_snow_coverage"),0,1,MaximumSnowCoverage)||!Number(TEXT("snow_radius_meters"),5,100,SnowRadiusMeters)||
       !Number(TEXT("snow_height_meters"),5,100,SnowHeightMeters)||!Number(TEXT("snow_fall_meters_per_second"),.1,10,SnowFallMetersPerSecond)||
       !Number(TEXT("snowflake_size_centimeters"),.1,10,SnowflakeSizeCentimeters)||!Doc->TryGetNumberField(TEXT("snowflake_count"),Count)||Count<0||Count>2048||Count!=FMath::FloorToDouble(Count)){if(Error.IsEmpty())Error=TEXT("Invalid snowflake count");return false;}
    if(!Doc->TryGetStringField(TEXT("snow_collection"),SnowCollectionPath)||!SnowCollectionPath.StartsWith(TEXT("/Game/"))||!Doc->TryGetStringField(TEXT("snowflake_material"),SnowflakeMaterialPath)||!SnowflakeMaterialPath.StartsWith(TEXT("/Game/")))
    {Error=TEXT("Weather assets require /Game paths");return false;}
    if(!Doc->TryGetStringField(TEXT("ambient_cubemap"),AmbientCubemapPath)||!AmbientCubemapPath.StartsWith(TEXT("/Game/")))
    {Error=TEXT("Weather ambient_cubemap requires a /Game asset");return false;}
    WeatherAmbientCubemap=LoadObject<UTextureCube>(nullptr,*AmbientCubemapPath,nullptr,LOAD_NoWarn);
    if(!WeatherAmbientCubemap){Error=TEXT("Could not load the authored weather ambient cubemap: ")+AmbientCubemapPath;return false;}
    SnowflakeCount=int32(Count);return true;
}

double ASeigeGameMode::SnowCoverage() const
{
    const auto Date=ScenarioCalendar.Sample();
    if(Date.SeasonIndex!=3)return 0;
    const double In=FMath::Clamp(Date.SeasonFraction/WinterAccumulationFraction,0.,1.);
    const double Out=FMath::Clamp((1-Date.SeasonFraction)/WinterMeltFraction,0.,1.);
    const double T=FMath::Min(In,Out);
    return MaximumSnowCoverage*T*T*(3-2*T);
}

void ASeigeGameMode::UpdateWeather(float DeltaSeconds)
{
    if(!FApp::CanEverRender()||!GetWorld())return;
    FSeigeWorldCalendar PresentationCalendar=ScenarioCalendar;
    if(Screen==TEXT("playing")&&!Paused)
        PresentationCalendar.Advance(FMath::RoundToDouble(FMath::Clamp(Accumulator,0.,Sim.FixedStepSeconds())*1000000.)/1000000.);
    const auto Date=PresentationCalendar.Sample();
    const double Light=FMath::SmoothStep(0.,double(SunriseSoftness),Date.SolarFactor);
    // Keep the authored daytime exposure exactly. A deterministic dark-phase
    // lift makes terrain readable without automatic metering or extra lights.
    if(Camera)Camera->GetCameraComponent()->PostProcessSettings.AutoExposureBias=ExposureBias+NightExposureOffsetEV*float(1-Light);
    if(WeatherSun)
    {
        const double Elevation=Date.IsDay?SunElevation*Date.SolarFactor:-SunElevation;
        const FRotator Direction(-Elevation,-118+Date.CycleFraction*360,0);
        // Movable-light rotation invalidates shadow pages. Smooth intensity can
        // update per frame while direction uses bounded authored angular steps.
        if(Light>0&&(!HasWeatherSunDirection||(RenderClock-LastWeatherSunUpdate>=SunShadowUpdateSeconds&&!WeatherSun->GetComponentRotation().Equals(Direction,SunDirectionUpdateDegrees))))
        {WeatherSun->SetWorldRotation(Direction);LastWeatherSunUpdate=RenderClock;HasWeatherSunDirection=true;}
        WeatherSun->SetIntensity(SunIntensity*Light);
        WeatherSun->SetLightColor(FMath::Lerp(FLinearColor(1,.63f,.38f),FLinearColor(1,.985f,.955f),float(FMath::Sqrt(Date.SolarFactor))));
    }
    if(WeatherSky)
    {
        WeatherSky->SetIntensity(SkyIntensity*FMath::Lerp(NightSkyFraction,1.f,float(Light)));
        WeatherSky->SetLightColor(FMath::Lerp(FLinearColor(.55f,.66f,1),FLinearColor::White,float(Light)));
    }
    if(!WeatherCollection)WeatherCollection=LoadObject<UMaterialParameterCollection>(nullptr,*SnowCollectionPath,nullptr,LOAD_NoWarn);
    const float Coverage=float(SnowCoverage());
    if(WeatherCollection)if(auto* Instance=GetWorld()->GetParameterCollectionInstance(WeatherCollection))Instance->SetScalarParameterValue(TEXT("SnowCoverage"),Coverage);
    if(Coverage<=.001f||!Camera||SnowflakeCount<=0)
    {if(Snowflakes)Snowflakes->SetVisibility(false);return;}
    if(!Snowflakes)
    {
        auto* SnowActor=GetWorld()->SpawnActor<AActor>();
        Snowflakes=NewObject<UInstancedStaticMeshComponent>(SnowActor);
        SnowActor->SetRootComponent(Snowflakes);Snowflakes->SetMobility(EComponentMobility::Movable);
        Snowflakes->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        if(auto* Material=LoadObject<UMaterialInterface>(nullptr,*SnowflakeMaterialPath,nullptr,LOAD_NoWarn))Snowflakes->SetMaterial(0,Material);
        Snowflakes->SetCollisionEnabled(ECollisionEnabled::NoCollision);Snowflakes->SetCastShadow(false);Snowflakes->SetAffectDistanceFieldLighting(false);Snowflakes->SetAffectDynamicIndirectLighting(false);Snowflakes->RegisterComponent();
        for(int32 I=0;I<SnowflakeCount;++I)Snowflakes->AddInstance(FTransform::Identity);
    }
    Snowflakes->SetVisibility(true);
    // Cosmetic flakes follow the authoritative clock and freeze with pause. No actors per flake.
    const double Seconds=PresentationCalendar.ElapsedMicroseconds()/1000000.;
    const FVector Focus=Camera->GetActorLocation();
    TArray<FTransform> Transforms;Transforms.Reserve(SnowflakeCount);
    FRandomStream Random(9127);
    for(int32 I=0;I<SnowflakeCount;++I)
    {
        const float X=Random.FRandRange(-SnowRadiusMeters,SnowRadiusMeters)*100;
        const float Y=Random.FRandRange(-SnowRadiusMeters,SnowRadiusMeters)*100;
        const double Phase=Random.FRand()*SnowHeightMeters;
        const double Z=FMath::Fmod(Phase-Seconds*SnowFallMetersPerSecond,SnowHeightMeters);
        const FVector Position=Focus+FVector(X+FMath::Sin(Seconds*.4+I)*45,Y,(Z<0?Z+SnowHeightMeters:Z)*100-SnowHeightMeters*50);
        Transforms.Emplace(FQuat::Identity,Position,FVector(SnowflakeSizeCentimeters/100));
    }
    Snowflakes->BatchUpdateInstancesTransforms(0,Transforms,true,true,true);
}
