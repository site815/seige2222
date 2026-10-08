#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "GameFramework/GameUserSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"

namespace
{
const FLinearColor Ink(.022f,.044f,.064f,1), Panel(.035f,.067f,.09f,.97f), Mint(.24f,.92f,.74f,1), Muted(.57f,.7f,.76f,1), Amber(1,.7f,.27f,1);
}

ASeigeGameMode::ASeigeGameMode()
{
    PrimaryActorTick.bCanEverTick=true;
    PlayerControllerClass=ASeigeController::StaticClass();
    HUDClass=ASeigeHUD::StaticClass();
    DefaultPawnClass=nullptr;
    ScenarioSlots.Init(TEXT("empty"),9); ScenarioSlots[4]=TEXT("player");
}
void ASeigeGameMode::BeginPlay()
{
    Super::BeginPlay();
    if(!LoadGraphicsSettings()||!LoadWeatherSettings()||!LoadBuildingVisuals())
    {
        GraphicsSettingsValid=false;Notice=Error;Screen=TEXT("main");
        if(FParse::Param(FCommandLine::Get(),TEXT("GraphicsBenchmark"))||FParse::Param(FCommandLine::Get(),TEXT("UiSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("WorldReview"))||FParse::Param(FCommandLine::Get(),TEXT("DisplaySmoke")))
        {UE_LOG(LogTemp,Error,TEXT("Automated presentation cannot start: %s"),*Error);FPlatformMisc::RequestExitWithStatus(false,1);}
        return;
    }
    BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/M_Colony.M_Colony"));
    if(!BaseMaterial) BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Camera=GetWorld()->SpawnActor<ACameraActor>();
    Camera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Perspective;
    Camera->GetCameraComponent()->bConstrainAspectRatio=false;
    Camera->GetCameraComponent()->SetFieldOfView(CameraFov);
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureBias=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureBias=ExposureBias;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_ColorSaturation=true;
    Camera->GetCameraComponent()->PostProcessSettings.ColorSaturation=FVector4(ColorSaturation,ColorSaturation,ColorSaturation,1);
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AmbientOcclusionIntensity=true;
    Camera->GetCameraComponent()->PostProcessSettings.AmbientOcclusionIntensity=AmbientOcclusionIntensity;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AmbientOcclusionRadius=true;
    Camera->GetCameraComponent()->PostProcessSettings.AmbientOcclusionRadius=AmbientOcclusionRadiusCm;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_BloomIntensity=true;
    Camera->GetCameraComponent()->PostProcessSettings.BloomIntensity=BloomIntensity;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    // Filmic grading: a touch more contrast, cooler shadows, warmer highlights and
    // a soft vignette. These are authored constants; the sun/sky ratio supplies
    // the actual shadow depth so the grade never has to fake it.
    {
        auto& Post=Camera->GetCameraComponent()->PostProcessSettings;
        Post.bOverride_ColorContrast=true;Post.ColorContrast=FVector4(ColorContrast,ColorContrast,ColorContrast,1);
        Post.bOverride_VignetteIntensity=true;Post.VignetteIntensity=VignetteIntensity;
        Post.bOverride_ColorGainShadows=true;Post.ColorGainShadows=FVector4(.97f,.99f,1.04f,1);
        Post.bOverride_ColorGainHighlights=true;Post.ColorGainHighlights=FVector4(1.02f,1.f,.965f,1);
        Post.bOverride_AmbientOcclusionPower=true;Post.AmbientOcclusionPower=2.2f;
        Post.bOverride_BloomThreshold=true;Post.BloomThreshold=.8f;
    }
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->SetViewTarget(Camera);
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,3000),FRotator(-SunElevation,-28,0));
    Sun->GetLightComponent()->SetIntensity(SunIntensity);
    auto* SunComponent=Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
    WeatherSun=SunComponent;
    SunComponent->SetMobility(EComponentMobility::Movable);
    SunComponent->ForwardShadingPriority=1;
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->LightSourceAngle=SunSourceAngle;
    // Warm daylight; the weather pass still multiplies its dawn/dusk tint on top.
    SunComponent->bUseTemperature=true;SunComponent->Temperature=SunTemperatureKelvin;
    SunComponent->bCastCloudShadows=true;
    SunComponent->CloudShadowStrength=CloudShadowStrength;
    SunComponent->CloudShadowOnSurfaceStrength=CloudShadowStrength;
    SunComponent->CloudShadowExtent=10;
    SunComponent->CloudShadowMapResolutionScale=CloudShadowResolutionScale;
    SunComponent->DynamicShadowDistanceMovableLight=120000;
    Sun->GetLightComponent()->MarkRenderStateDirty();
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.985f,.955f));
    auto* Atmosphere=GetWorld()->SpawnActor<AActor>();
    auto* AtmosphereComponent=NewObject<USkyAtmosphereComponent>(Atmosphere);
    AtmosphereComponent->SetMieScatteringScale(AtmosphereMieScale);
    AtmosphereComponent->SetAerialPespectiveViewDistanceScale(AtmosphereAerialPerspectiveScale);
    Atmosphere->SetRootComponent(AtmosphereComponent); AtmosphereComponent->RegisterComponent();
    if(auto* CloudMaterial=LoadObject<UMaterialInterface>(nullptr,*CloudMaterialPath,nullptr,LOAD_NoWarn))
    {
        auto* Clouds=GetWorld()->SpawnActor<AActor>();
        auto* CloudComponent=NewObject<UVolumetricCloudComponent>(Clouds);
        Clouds->SetRootComponent(CloudComponent);
        CloudComponent->SetLayerBottomAltitude(1.2f);CloudComponent->SetLayerHeight(3.f);
        CloudComponent->SetMaterial(CloudMaterial);CloudComponent->RegisterComponent();
    }
    auto* Sky=GetWorld()->SpawnActor<ASkyLight>();
    WeatherSky=Sky->GetLightComponent();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(SkyIntensity);
    // A scene capture queued during BeginPlay may wait for shaders until dawn
    // has already switched the sun off. Stable authored radiance avoids baking
    // a black sky; weather varies its intensity/tint without runtime recapture.
    Sky->GetLightComponent()->SourceType=SLS_SpecifiedCubemap;
    Sky->GetLightComponent()->SetCubemap(WeatherAmbientCubemap);
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(false);
    // A faint green-brown ground bounce instead of a black lower hemisphere keeps
    // undersides of canopies and hulls from going flat black under the sun.
    if(SkyLowerHemisphereLuminance>0)
    {
        Sky->GetLightComponent()->bLowerHemisphereIsBlack=false;
        Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(.42f,.46f,.30f)*SkyLowerHemisphereLuminance);
    }
    Sky->GetLightComponent()->MarkRenderStateDirty();
    if(FogDensity>0)
    {
        // Distance haze: exponential height fog with a sky-coloured inscattering
        // term and a warm directional lobe toward the sun. Volumetric fog stays
        // off; its cost at native resolution is not justified for a top-down view.
        auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>();
        auto* FogComponent=Fog->GetComponent();
        FogComponent->SetFogDensity(FogDensity);
        FogComponent->SetFogHeightFalloff(FogHeightFalloff);
        FogComponent->SetFogInscatteringColor(FLinearColor(.47f,.56f,.70f)*FogInscatteringLuminance);
        FogComponent->SetDirectionalInscatteringColor(FLinearColor(1.f,.86f,.66f)*FogInscatteringLuminance*.6f);
        FogComponent->SetDirectionalInscatteringExponent(6.f);
        FogComponent->SetDirectionalInscatteringStartDistance(FogStartDistanceMeters*100.f);
        FogComponent->SetStartDistance(FogStartDistanceMeters*100.f);
        FogComponent->SetFogMaxOpacity(FogMaxOpacity);
        FogComponent->SetVolumetricFog(false);
    }
    ResetColony();
    ReturnToMainMenu(); Zoom=DefaultZoom;
    InitializeDisplaySettings();
    if(FParse::Param(FCommandLine::Get(),TEXT("PrototypeSmoke"))||FParse::Param(FCommandLine::Get(),TEXT("PrototypeScreenshot")))
    {
        Screen=TEXT("landing");
        ConfirmLanding(HomePosition());
    }
    UpdateCamera();
}
void ASeigeGameMode::ResetColony()
{
    StartScenario();
}
void ASeigeGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(IsPreparingScenario())TickScenarioPreparation();
    if(!Ready)
    {
        if(IsPreparingScenario())
        {RenderClock+=DeltaSeconds;if(FParse::Param(FCommandLine::Get(),TEXT("UiSmoke")))RunPresentationSmoke();}
        return;
    }
    if(CompanionView&&(Sim.Escaped||Sim.Failed||Observer))ExitCompanionView();
    if(CompanionView)Speed=1;
    if(Screen==TEXT("playing")&&!Paused)
    {
        const double Step=Sim.FixedStepSeconds(); Accumulator+=FMath::Min(DeltaSeconds,.25f)*Speed;
        while(Accumulator>=Step)
        {
            BindScenarioCalendar();
            CaptureSimulationPresentation();
            if(Observer&&CenterBrain) CenterBrain->Tick(Sim,Step); else Sim.Tick(Step);
            for(auto& N:Neighbors) if(N.Brain) N.Brain->Tick(N.Sim,Step);
            for(auto& N:Neighbors)Sim.Combat.TickExternalSector(Sim,N.Sim,N.Index,Step);
            ScenarioCalendar.Advance(Step);
            BindScenarioCalendar();
            Accumulator-=Step;
        }
    }
    RenderClock+=DeltaSeconds;
    UpdateWeather(DeltaSeconds);
    RefreshEnvironment();
    SyncVisuals();
    if(FParse::Param(FCommandLine::Get(),TEXT("GraphicsBenchmark")))RunGraphicsBenchmark(DeltaSeconds);
    if(FParse::Param(FCommandLine::Get(),TEXT("UiSmoke"))) RunPresentationSmoke();
    if(FParse::Param(FCommandLine::Get(),TEXT("WorldReview"))) RunWorldReview();
    if(FParse::Param(FCommandLine::Get(),TEXT("DisplaySmoke"))) RunDisplaySmoke();
    if(!ScreenshotRequested && RenderClock>8 && FParse::Param(FCommandLine::Get(),TEXT("PrototypeScreenshot")))
    {
        ScreenshotRequested=true;
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Prototype.png")),true,false);
        UE_LOG(LogTemp,Display,TEXT("SEIGE_SCREENSHOT requested"));
    }
    if(RenderClock>15 && FParse::Param(FCommandLine::Get(),TEXT("PrototypeSmoke")))
    {
        UE_LOG(LogTemp,Display,TEXT("SEIGE_SMOKE_OK time=%.1f population=%d buildings=%d"),Sim.Time,Sim.Population,Sim.Buildings.Num());
        const FString Report=FString::Printf(TEXT("{\"ready\":true,\"time\":%.2f,\"population\":%d,\"buildings\":%d,\"wave\":%d}"),Sim.Time,Sim.Population,Sim.Buildings.Num(),Sim.Wave);
        FFileHelper::SaveStringToFile(Report,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SmokeReport.json")));
        FPlatformMisc::RequestExit(false);
    }
}
UMaterialInterface* ASeigeGameMode::Material(FLinearColor Color)
{
    FString Key=Color.ToString();
    if(auto* Found=Materials.Find(Key)) return *Found;
    auto* M=UMaterialInstanceDynamic::Create(BaseMaterial,this);
    M->SetVectorParameterValue(TEXT("Tint"),Color);
    M->SetVectorParameterValue(TEXT("Color"),Color);
    Materials.Add(Key,M); return M;
}
void ASeigeGameMode::Part(AActor* Actor,const FString& Shape,FVector Offset,FVector Scale3,FLinearColor Color,FRotator Rotation)
{
    auto* Mesh=NewObject<UStaticMeshComponent>(Actor);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"),*Shape,*Shape)));
    Mesh->SetMaterial(0,Material(Color));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetupAttachment(Actor->GetRootComponent());
    Mesh->SetRelativeLocation(Offset); Mesh->SetRelativeScale3D(Scale3); Mesh->SetRelativeRotation(Rotation);
    Mesh->RegisterComponent(); Actor->AddInstanceComponent(Mesh);
}
AActor* ASeigeGameMode::Visual(const FString& Key,const FString& Kind,FVector Location,FLinearColor Color,float Size)
{
    if(auto* Found=Visuals.Find(Key)){ (*Found)->SetActorLocation(Location); (*Found)->SetActorHiddenInGame(false); return *Found; }
    auto* Actor=GetWorld()->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(Actor); Actor->SetRootComponent(Root); Root->RegisterComponent();
    Actor->SetActorLocation(Location);
    const FString MeshName=TEXT("SM_")+Kind;
    auto* Imported=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Art/%s.%s"),*MeshName,*MeshName));
    if(Imported)
    {
        auto* Mesh=NewObject<UStaticMeshComponent>(Actor); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(Imported); Mesh->SetupAttachment(Root);
        const bool Building=Kind!=TEXT("Robot")&&Kind!=TEXT("Bug")&&Kind!=TEXT("Shuttle");
        if(Building)Mesh->ComponentTags.Add(TEXT("BuildingBody"));
        Mesh->SetCollisionEnabled(Building?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
        Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        if(Building)Mesh->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
        Mesh->RegisterComponent(); Actor->AddInstanceComponent(Mesh);
        const FVector Bounds=Imported->GetBounds().BoxExtent;
        const double Extent=(Kind==TEXT("Robot")||Kind==TEXT("Bug"))?Bounds.GetMax():FMath::Max(Bounds.X,Bounds.Y);
        Mesh->SetRelativeScale3D(FVector(Size/FMath::Max(Extent*2,1.0)));
    }
    else if(Kind==TEXT("Robot"))
    {
        Part(Actor,TEXT("Sphere"),FVector(0,0,48),FVector(.55,.48,.8),FLinearColor(.87,.97,1));
        Part(Actor,TEXT("Sphere"),FVector(0,-22,63),FVector(.38,.1,.19),Ink);
        Part(Actor,TEXT("Sphere"),FVector(0,-28,65),FVector(.23,.05,.05),Mint);
    }
    else if(Kind==TEXT("Bug"))
    {
        Part(Actor,TEXT("Sphere"),FVector(0,0,45),FVector(.9,1.4,.55),Color);
        for(int i=0;i<6;i++) Part(Actor,TEXT("Cone"),FVector(i%2?60:-60,(i/2-1)*40,23),FVector(.16,.18,.8),FLinearColor(.17,.05,.15),FRotator(i%2?55:-55,0,0));
    }
    else if(Kind==TEXT("Shuttle"))
    {
        Part(Actor,TEXT("Cylinder"),FVector(0,0,Size*.8),FVector(Size*.006,Size*.006,Size*.014),Color);
        Part(Actor,TEXT("Cone"),FVector(0,0,Size*1.65),FVector(Size*.006,Size*.006,Size*.006),Color);
        for(double X:{-1.,1.})for(double Y:{-1.,1.})Part(Actor,TEXT("Cube"),FVector(X*Size*.3,Y*Size*.3,Size*.08),FVector(Size*.0015,Size*.0015,Size*.0016),Muted);
    }
    else
    {
        Actor->Tags.Add(TEXT("PrimitiveFallback"));
        Part(Actor,TEXT("Cylinder"),FVector(0,0,20),FVector(Size/80,Size/80,.35),FLinearColor(.13,.26,.31));
        Part(Actor,TEXT("Cube"),FVector(0,0,Size*.35),FVector(Size/110,Size/110,Size/150),Color);
        Part(Actor,TEXT("Sphere"),FVector(0,0,Size*.65),FVector(Size/115,Size/115,.5),FLinearColor(.87,.96,1));
        Part(Actor,TEXT("Cube"),FVector(0,-Size*.47,Size*.42),FVector(Size/140,.06,.22),Mint);
    }
    Visuals.Add(Key,Actor); return Actor;
}
void ASeigeGameMode::SyncVisuals()
{
    if(!FApp::CanEverRender()) return;
    if(RegionMapAlpha()>=1.f){for(auto& V:Visuals)V.Value->SetActorHiddenInGame(true);return;}
    TSet<FString> Live;
    const bool SceneVisible=(MenuOpen||Screen==TEXT("playing")||Screen==TEXT("landing"));
    auto Sync=[&](FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,bool Show)
    {
        if(!Show) return;
        const auto* Snapshot=PresentationSnapshot(Colony);
        const FSeigeRenderSnapshot EmptySnapshot;
        const FSeigeRenderSnapshot& RenderState=Snapshot?*Snapshot:EmptySnapshot;
        const double Alpha=PresentationAlpha(),AnimationTime=RenderSimulationTime(Colony);
        for(const auto& B:Colony.Buildings)
        {
            if(B.Health<=0) continue;
            const auto* D=Colony.Definition(B); if(!D||D->Role==TEXT("wall")) continue;
            const auto* Appearance=B.UpgradeTarget.IsEmpty()?D:Colony.BuildingDefs.Find(B.UpgradeTarget);if(!Appearance)Appearance=D;
            const FString Key=Prefix+FString::Printf(TEXT("building_%d"),B.Id);
            const FVector2D P=B.Position+Offset;
            if(!Observer&&!Offset.IsNearlyZero()&&!IsWorldVisible(P)) continue;
            Live.Add(Key);
            FString Kind=BuildingVisualKind(*Appearance); if(Kind.IsEmpty()) Kind=TEXT("Factory"); Kind[0]=FChar::ToUpper(Kind[0]);
            const FName DefinitionTag(*(FString(TEXT("definition_"))+Appearance->Id));
            if(auto* Existing=Visuals.FindRef(Key).Get())if(!Existing->ActorHasTag(DefinitionTag)){Existing->Destroy();Visuals.Remove(Key);}
            if(!Visuals.Contains(Key)) ClearSceneryAt(P,D->ReservedFootprint);
            Visual(Key,Kind,RenderPosition(P),Appearance->Color,Appearance->Footprint*2.f*RenderScale)->Tags.AddUnique(DefinitionTag);
            SyncConstructionVisuals(Colony,B,*Appearance,P,Key,Live);

            SyncInventoryVisuals(Colony,B,*D,P,Key,Live);

            SyncBuildingPlot(Colony,B,*D,P,Key,Live);
        }
        SyncRoadVisuals(Colony,Offset,Prefix,Live);
        SyncWallVisuals(Colony,Offset,Prefix,Live);
        SyncCombatVisuals(Colony,Offset,Prefix,Live);
        SyncWorkerAgents(Colony,Offset,Prefix,Live);
        for(const auto& E:Colony.Enemies)
        {
            const FVector2D LocalP=Snapshot?Snapshot->Enemy(E,Alpha):E.Position;
            const FVector2D P=LocalP+Offset;
            if(!Observer&&!IsWorldVisible(E.Position+Offset))continue;
            if(!Observer&&!IsWorldVisible(P)) continue;
            const FString Key=Prefix+FString::Printf(TEXT("enemy_%d"),E.Id); Live.Add(Key);
            auto* A=Visual(Key,TEXT("Bug"),RenderPosition(P),FLinearColor(.4f,.08f,.17f),210);
            const auto* Attacked=Colony.FindBuilding(E.TargetBuildingId);
            FVector2D Target=Attacked?Attacked->Position:Colony.Buildings.IsEmpty()?LocalP+FVector2D(1,0):Colony.Buildings[0].Position;
            A->SetActorRotation(FVector(Target-LocalP,0).Rotation());
            // Real melee events drive the lunge; an idle blocked enemy does not
            // pretend to attack, and a repaired target still shows incoming hits.
            const double SinceAttack=AnimationTime-E.LastAttackTime;
            if(E.LastAttackTime>=0&&SinceAttack>=0&&SinceAttack<.35)
                A->AddActorWorldOffset(FVector((Target-LocalP).GetSafeNormal(),0)*FMath::Sin(SinceAttack/.35*PI)*18.);
        }
    };
    const bool AwaitingLanding=Screen==TEXT("landing")||(MenuOpen&&MenuReturnScreen==TEXT("landing"));
    Sync(Sim,FVector2D::ZeroVector,TEXT("home_"),SceneVisible&&!AwaitingLanding&&DetailedSectorIndex()==4);
    for(auto& N:Neighbors) Sync(N.Sim,N.Offset,FString::Printf(TEXT("zone_%d_"),N.Index),SceneVisible&&DetailedSectorIndex()==N.Index);
    if(SceneVisible&&!AwaitingLanding&&DetailedSectorIndex()!=4)SyncCombatVisuals(Sim,FVector2D::ZeroVector,TEXT("home_"),Live);
    SyncPlacementGhost(Live);
    SyncRoadPlacementGhost(Live);
    SyncCompanionVisuals(Live);
    SyncWallGhost(Live);
    for(auto It=Visuals.CreateIterator();It;++It) if(!Live.Contains(It.Key()))
    {
        const FString OriginalPrefix=TEXT("construction_original_")+It.Value()->GetPathName()+TEXT(".");
        for(auto MaterialIt=Materials.CreateIterator();MaterialIt;++MaterialIt)if(MaterialIt.Key().StartsWith(OriginalPrefix))MaterialIt.RemoveCurrent();
        It.Value()->Destroy();It.RemoveCurrent();
    }
}
void ASeigeGameMode::ClickWorld()
{
    if(!Ready||!CursorOnWorld||IsRegionMap()||CompanionView) return;
    if(Screen==TEXT("landing")) { ConfirmLanding(CursorWorld); return; }
    if(Screen!=TEXT("playing")) return;
    if(FleetOrderActive)
    {
        if(!Observer&&DetailedSectorIndex()==4)Notice=Sim.Combat.OrderFleet(Sim,SelectedFleetId,TEXT("move"),CursorWorld,0,Error)?TEXT("Fleet moving; units choose their own routes and targets."):Error;
        FleetOrderActive=false;return;
    }
    if(WallPlacementActive){ClickWallPlan();return;}
    if(IsRoadToolActive())
    {
        if(Observer||DetailedSectorIndex()!=4)return;
        if(RoadPlacementActive)
        {
            const FVector2D Point=SnapRoadCursor(CursorWorld);
            if(!RoadHasStart){RoadStart=Point;RoadHasStart=true;Notice=TEXT("Choose the road destination. Endpoints snap to building edge ports and existing roads.");}
            else if(Sim.PlaceRoad(RoadStart,Point,Error)){SelectedRoadId=Sim.Roads.Last().Id;RoadHasStart=false;RefreshTransportScenery();Notice=TEXT("Road construction queued: workers must deliver materials and build the corridor.");}
            else Notice=Error;
            return;
        }
        SelectedRoadId=0;
        double Best=TNumericLimits<double>::Max();
        for(const auto& Road:Sim.Roads)
        {
            if(Road.Health<=0)continue;
            const FVector2D Delta=Road.B-Road.A;
            const double T=FMath::Clamp(FVector2D::DotProduct(CursorWorld-Road.A,Delta)/FMath::Max(Delta.SizeSquared(),1.),0.,1.);
            const double Distance=FVector2D::Distance(CursorWorld,Road.A+Delta*T);
            const auto* Tier=Sim.TransportTiers.Find(Road.Tier.IsEmpty()?Road.TargetTier:Road.Tier);
            if(Tier&&Distance<FMath::Max(20.,Tier->WidthMeters*.6/Sim.MetersPerWorldUnit())&&Distance<Best){SelectedRoadId=Road.Id;Best=Distance;}
        }
        if(SelectedRoadId>0)BeginRoadUpgrade();else Notice=TEXT("Select the surface of a completed transport corridor.");
        return;
    }
    if(!SelectedBuild.IsEmpty())
    {
        if(Observer||DetailedSectorIndex()!=4)return;
        if(Sim.PlaceBuilding(SelectedBuild,CursorWorld,Error)) Notice=TEXT("Construction queued. Workers deliver materials, prepare foundations and assemble the building."); else Notice=Error;
        return;
    }
    SelectedId=SelectedRoadId=SelectedCompanionId=0; double Distance=TNumericLimits<double>::Max();
    if(!Observer&&DetailedSectorIndex()==4)for(const auto& V:Sim.Combat.Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&FVector2D::Distance(CursorWorld,V.Position)<Sim.Combat.Chassis[V.ChassisId].RadiusMeters/Sim.MetersPerWorldUnit())
    {SelectedFleetId=V.FleetId;Notice=FString::Printf(TEXT("Fleet %d selected. Open its command-center fleet panel to issue a mission."),SelectedFleetId);return;}
    for(const auto& Dog:Sim.Companions.Dogs)if(!Dog.Evacuated&&FVector2D::Distance(CursorWorld,Dog.Position)<FMath::Max(22.,CameraViewZoom()*.01)){SelectedCompanionId=Dog.Id;return;}
    if(const auto* Viewed=ViewedSimulation())for(const auto& B:Viewed->Buildings)
    {
        const auto* D=Viewed->Definition(B);const FVector2D Local=CursorWorld-B.Position-DetailedSectorOffset();
        if(D&&B.Health>0&&(Observer||DetailedSectorIndex()==4||IsWorldVisible(B.Position+DetailedSectorOffset()))&&FMath::Abs(Local.X)<=D->ReservedFootprint&&FMath::Abs(Local.Y)<=D->ReservedFootprint&&Local.Size()<Distance)
        {Distance=Local.Size();SelectedId=B.Id;}
    }
    if(!SelectedId&&DetailedSectorIndex()==4)for(const auto& Road:Sim.Roads)
    {
        if(Road.Health<=0)continue;
        const FVector2D Delta=Road.B-Road.A;const double T=FMath::Clamp(FVector2D::DotProduct(CursorWorld-Road.A,Delta)/FMath::Max(Delta.SizeSquared(),1.),0.,1.);
        const double D=FVector2D::Distance(CursorWorld,Road.A+Delta*T);
        const auto* Tier=Sim.TransportTiers.Find(Road.Tier.IsEmpty()?Road.TargetTier:Road.Tier);
        if(Tier&&D<FMath::Max(20.,Tier->WidthMeters*.6/Sim.MetersPerWorldUnit())&&D<Distance){SelectedRoadId=Road.Id;Distance=D;}
    }
}
bool ASeigeGameMode::SelectBuildingRay(const FVector& Origin,const FVector& Direction)
{
    if(!Ready||IsRegionMap()||Screen!=TEXT("playing")||!SelectedBuild.IsEmpty()||IsRoadToolActive()||WallPlacementActive||FleetOrderActive)return false;
    FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(SeigeBuildingPick),true);
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin+Direction.GetSafeNormal()*Sim.WorldHalfSize*RenderScale*24,ECC_Visibility,Params))return false;
    FVector TerrainHit;
    if(TraceGroundRay(Origin,Direction,TerrainHit)&&FVector::DistSquared(Origin,TerrainHit)+25<FVector::DistSquared(Origin,Hit.ImpactPoint))return false;
    const FString Prefix=DetailedSectorIndex()==4?TEXT("home_"):FString::Printf(TEXT("zone_%d_"),DetailedSectorIndex());
    if(const auto* Viewed=ViewedSimulation())for(const auto& B:Viewed->Buildings)
        if(B.Health>0&&Visuals.FindRef(Prefix+FString::Printf(TEXT("building_%d"),B.Id))==Hit.GetActor()){SelectedId=B.Id;SelectedRoadId=SelectedCompanionId=0;return true;}
    return false;
}
