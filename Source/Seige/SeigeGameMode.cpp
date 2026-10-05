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
    if(!LoadGraphicsSettings()){GraphicsSettingsValid=false;Notice=Error;Screen=TEXT("main");return;}
    BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/M_Colony.M_Colony"));
    if(!BaseMaterial) BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Camera=GetWorld()->SpawnActor<ACameraActor>();
    Camera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Perspective;
    Camera->GetCameraComponent()->bConstrainAspectRatio=false;
    Camera->GetCameraComponent()->SetFieldOfView(CameraFov);
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureBias=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureBias=-.1f;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AmbientOcclusionIntensity=true;
    Camera->GetCameraComponent()->PostProcessSettings.AmbientOcclusionIntensity=.8f;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AmbientOcclusionRadius=true;
    Camera->GetCameraComponent()->PostProcessSettings.AmbientOcclusionRadius=120;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_BloomIntensity=true;
    Camera->GetCameraComponent()->PostProcessSettings.BloomIntensity=.15f;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->SetViewTarget(Camera);
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,3000),FRotator(-38,-28,0));
    Sun->GetLightComponent()->SetIntensity(SunIntensity);
    auto* SunComponent=Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
    SunComponent->SetMobility(EComponentMobility::Movable);
    SunComponent->ForwardShadingPriority=1;
    SunComponent->SetAtmosphereSunLight(true);
    SunComponent->LightSourceAngle=2.0f;
    SunComponent->bCastCloudShadows=true;
    SunComponent->CloudShadowStrength=CloudShadowStrength;
    SunComponent->CloudShadowOnSurfaceStrength=CloudShadowStrength;
    SunComponent->CloudShadowExtent=10;
    SunComponent->CloudShadowMapResolutionScale=2;
    SunComponent->DynamicShadowDistanceMovableLight=120000;
    Sun->GetLightComponent()->MarkRenderStateDirty();
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.985f,.955f));
    auto* Atmosphere=GetWorld()->SpawnActor<AActor>();
    auto* AtmosphereComponent=NewObject<USkyAtmosphereComponent>(Atmosphere);
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
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(SkyIntensity);
    Sky->GetLightComponent()->SetRealTimeCaptureEnabled(true);
    auto* Fog=GetWorld()->SpawnActor<AExponentialHeightFog>();
    Fog->GetComponent()->SetFogDensity(.0025f);
    Fog->GetComponent()->SetFogHeightFalloff(.15f);
    Fog->GetComponent()->SetFogInscatteringColor(FLinearColor(.45f,.52f,.58f));
    Fog->GetComponent()->SetStartDistance(45000);
    ResetColony();
    ReturnToMainMenu(); Zoom=DefaultZoom;
    if(auto* Settings=UGameUserSettings::GetGameUserSettings())
    {
        GraphicsQuality=FMath::Clamp(Settings->GetOverallScalabilityLevel(),0,3);
        Fullscreen=Settings->GetFullscreenMode()!=EWindowMode::Windowed;
    }
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
    if(!Ready) return;
    if(Screen==TEXT("playing")&&!Paused)
    {
        const double Step=Sim.FixedStepSeconds(); Accumulator+=FMath::Min(DeltaSeconds,.25f)*Speed;
        while(Accumulator>=Step)
        {
            if(Observer&&CenterBrain) CenterBrain->Tick(Sim,Step); else Sim.Tick(Step);
            for(auto& N:Neighbors) if(N.Brain) N.Brain->Tick(N.Sim,Step);
            Accumulator-=Step;
        }
    }
    RenderClock+=DeltaSeconds;
    RefreshEnvironment();
    SyncVisuals();
    if(FParse::Param(FCommandLine::Get(),TEXT("UiSmoke"))) RunPresentationSmoke();
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
        auto* Mesh=NewObject<UStaticMeshComponent>(Actor); Mesh->SetStaticMesh(Imported); Mesh->SetupAttachment(Root);
        const bool Building=Kind!=TEXT("Robot")&&Kind!=TEXT("Bug");
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
    else
    {
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
    if(IsRegionMap()){for(auto& V:Visuals)V.Value->SetActorHiddenInGame(true);return;}
    TSet<FString> Live;
    auto Sync=[&](FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,bool Show)
    {
        if(!Show) return;
        for(const auto& B:Colony.Buildings)
        {
            if(B.Health<=0) continue;
            const auto* D=Colony.Definition(B); if(!D) continue;
            const FString Key=Prefix+FString::Printf(TEXT("building_%d"),B.Id);
            const FVector2D P=B.Position+Offset;
            if(!Observer&&!Offset.IsNearlyZero()&&!Sim.IsVisible(P)) continue;
            Live.Add(Key);
            FString Kind=D->Visual; if(Kind.IsEmpty()) Kind=TEXT("Factory"); Kind[0]=FChar::ToUpper(Kind[0]);
            if(!Visuals.Contains(Key)) ClearSceneryAt(P,D->Footprint);
            Visual(Key,Kind,RenderPosition(P),D->Color,D->Footprint*2.f*RenderScale);
        }
        for(const auto& C:Colony.Couriers)
        {
            const FVector2D P=C.Position+Offset;
            if(!Observer&&!Offset.IsNearlyZero()&&!Sim.IsVisible(P)) continue;
            const FString Key=Prefix+FString::Printf(TEXT("courier_%d"),C.Id); Live.Add(Key);
            auto* A=Visual(Key,TEXT("Robot"),RenderPosition(P,18+FMath::Sin(RenderClock*4+C.Id)*5),Mint,110);
            if(const auto* B=Colony.FindBuilding(C.TargetId)) A->SetActorRotation(FVector(B->Position-C.Position,0).Rotation());
        }
        for(const auto& E:Colony.Enemies)
        {
            const FVector2D P=E.Position+Offset;
            if(!Observer&&!Sim.IsVisible(P)) continue;
            const FString Key=Prefix+FString::Printf(TEXT("enemy_%d"),E.Id); Live.Add(Key);
            auto* A=Visual(Key,TEXT("Bug"),RenderPosition(P),FLinearColor(.4f,.08f,.17f),210);
            FVector2D Target=Colony.Buildings.IsEmpty()?FVector2D::ZeroVector:Colony.Buildings[0].Position;
            A->SetActorRotation(FVector(Target-E.Position,0).Rotation());
        }
    };
    Sync(Sim,FVector2D::ZeroVector,TEXT("home_"),Screen!=TEXT("landing")&&DetailedSectorIndex()==4);
    for(auto& N:Neighbors) Sync(N.Sim,N.Offset,FString::Printf(TEXT("zone_%d_"),N.Index),DetailedSectorIndex()==N.Index);
    for(auto It=Visuals.CreateIterator();It;++It) if(!Live.Contains(It.Key())) { It.Value()->Destroy(); It.RemoveCurrent(); }
}
void ASeigeGameMode::ClickWorld()
{
    if(!Ready||!CursorOnWorld||IsRegionMap()) return;
    if(Screen==TEXT("landing")) { ConfirmLanding(CursorWorld); return; }
    if(Screen!=TEXT("playing")) return;
    if(!SelectedBuild.IsEmpty())
    {
        if(Observer||DetailedSectorIndex()!=4)return;
        if(Sim.PlaceBuilding(SelectedBuild,CursorWorld,Error)) Notice=TEXT("Building online. Staffing and deliveries are automatic."); else Notice=Error;
        return;
    }
    SelectedId=0; double Distance=350;
    if(const auto* Viewed=ViewedSimulation())for(const auto& B:Viewed->Buildings)
        if(B.Health>0&&(Observer||DetailedSectorIndex()==4||Sim.IsVisible(B.Position+DetailedSectorOffset()))&&FVector2D::Distance(B.Position+DetailedSectorOffset(),CursorWorld)<Distance)
        {Distance=FVector2D::Distance(B.Position+DetailedSectorOffset(),CursorWorld);SelectedId=B.Id;}
}
bool ASeigeGameMode::SelectBuildingRay(const FVector& Origin,const FVector& Direction)
{
    if(!Ready||IsRegionMap()||Screen!=TEXT("playing")||!SelectedBuild.IsEmpty())return false;
    FHitResult Hit;FCollisionQueryParams Params(SCENE_QUERY_STAT(SeigeBuildingPick),true);
    if(!GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin+Direction.GetSafeNormal()*Sim.WorldHalfSize*RenderScale*24,ECC_Visibility,Params))return false;
    FVector TerrainHit;
    if(TraceGroundRay(Origin,Direction,TerrainHit)&&FVector::DistSquared(Origin,TerrainHit)+25<FVector::DistSquared(Origin,Hit.ImpactPoint))return false;
    const FString Prefix=DetailedSectorIndex()==4?TEXT("home_"):FString::Printf(TEXT("zone_%d_"),DetailedSectorIndex());
    if(const auto* Viewed=ViewedSimulation())for(const auto& B:Viewed->Buildings)
        if(B.Health>0&&Visuals.FindRef(Prefix+FString::Printf(TEXT("building_%d"),B.Id))==Hit.GetActor()){SelectedId=B.Id;return true;}
    return false;
}
