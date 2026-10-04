#include "SeigeGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
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
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "DrawDebugHelpers.h"

namespace
{
const FLinearColor Ink(.022f,.044f,.064f,1), Panel(.035f,.067f,.09f,.97f), Mint(.24f,.92f,.74f,1), Muted(.57f,.7f,.76f,1), Amber(1,.7f,.27f,1);
FString SavePath() { return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames/Colony.json")); }
}

ASeigeGameMode::ASeigeGameMode()
{
    PrimaryActorTick.bCanEverTick=true;
    PlayerControllerClass=ASeigeController::StaticClass();
    HUDClass=ASeigeHUD::StaticClass();
    DefaultPawnClass=nullptr;
}
void ASeigeGameMode::BeginPlay()
{
    Super::BeginPlay();
    BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/M_Colony.M_Colony"));
    if(!BaseMaterial) BaseMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    Camera=GetWorld()->SpawnActor<ACameraActor>();
    Camera->GetCameraComponent()->ProjectionMode=ECameraProjectionMode::Orthographic;
    Camera->GetCameraComponent()->bConstrainAspectRatio=false;
    Camera->GetCameraComponent()->bAutoCalculateOrthoPlanes=false;
    Camera->GetCameraComponent()->SetOrthoNearClipPlane(-20000);
    Camera->GetCameraComponent()->SetOrthoFarClipPlane(40000);
    Camera->GetCameraComponent()->SetOrthoWidth(Zoom);
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureMethod=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureMethod=EAutoExposureMethod::AEM_Manual;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureBias=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureBias=-1;
    Camera->GetCameraComponent()->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure=true;
    Camera->GetCameraComponent()->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure=false;
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->SetViewTarget(Camera);
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,3000),FRotator(-55,-35,0));
    Sun->GetLightComponent()->SetIntensity(4);
    Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->ForwardShadingPriority=1;
    Sun->GetLightComponent()->MarkRenderStateDirty();
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1,.91f,.8f));
    auto* Fill=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,3000),FRotator(-35,140,0));
    Fill->GetLightComponent()->SetIntensity(1.8f);
    Fill->GetLightComponent()->SetCastShadows(false);
    Fill->GetLightComponent()->SetLightColor(FLinearColor(.5f,.73f,1));
    ResetColony();
    UpdateCamera();
}
void ASeigeGameMode::ResetColony()
{
    for(auto& Pair:Visuals) if(Pair.Value) Pair.Value->Destroy();
    Visuals.Empty();
    FString Directory=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));
    if(!IFileManager::Get().DirectoryExists(*Directory)) Directory=FPaths::Combine(FPlatformProcess::BaseDir(),TEXT("Rules"));
    Ready=Sim.Initialize(Directory,Error);
    SelectedId=0; SelectedBuild.Empty(); Paused=false; WinAcknowledged=false; Accumulator=0;
    Notice=Ready?TEXT("FIRST LANDING  |  Build extractors on deposits, then connect a production chain."):Error;
    if(!Ready) { UE_LOG(LogTemp,Error,TEXT("RULES FAILED: %s"),*Error); }
    else { UE_LOG(LogTemp,Display,TEXT("SEIGE_READY: %s; %d building definitions"),*Sim.Title,Sim.BuildingDefs.Num()); }
    SyncVisuals();
    if(Ready) CreateLandscape();
}
void ASeigeGameMode::SaveGame()
{
    if(!Ready) { Notice=TEXT("Cannot save while the rule set is invalid."); return; }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(SavePath()),true);
    Notice=Sim.Save(SavePath(),Error)?TEXT("Colony saved. F9 restores this snapshot."):Error;
}
void ASeigeGameMode::LoadGame()
{
    if(!Ready) { Notice=TEXT("Correct the rule files and restart before loading."); return; }
    if(Sim.Load(SavePath(),Error))
    {
        for(auto& Pair:Visuals) if(Pair.Value) Pair.Value->Destroy();
        Visuals.Empty(); Accumulator=0; WinAcknowledged=false;
        Notice=TEXT("Colony restored."); SelectedId=0; SelectedBuild.Empty(); SyncVisuals();
    }
    else Notice=Error;
}
void ASeigeGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!Ready) return;
    if(!Paused){ const double Step=Sim.FixedStepSeconds(); Accumulator+=FMath::Min(DeltaSeconds,.25f)*Speed; while(Accumulator>=Step){ Sim.Tick(Step); Accumulator-=Step; } }
    RenderClock+=DeltaSeconds;
    SyncVisuals();
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
void ASeigeGameMode::UpdateCamera()
{
    if(!Camera) return;
    Camera->SetActorLocation(CameraCenter+FVector(6300,-6300,9000));
    Camera->SetActorRotation(FRotator(-45,135,0));
    Camera->GetCameraComponent()->SetOrthoWidth(Zoom);
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
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->RegisterComponent(); Actor->AddInstanceComponent(Mesh);
        const double Extent=Imported->GetBounds().BoxExtent.GetMax();
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
void ASeigeGameMode::CreateLandscape()
{
    if(Landscape) Landscape->Destroy();
    auto* Ground=GetWorld()->SpawnActor<AActor>(); auto* Root=NewObject<USceneComponent>(Ground); Ground->SetRootComponent(Root); Root->RegisterComponent();
    Landscape=Ground;
    Part(Ground,TEXT("Cube"),FVector(0,0,-65),FVector(240,240,1),FLinearColor(.08,.22,.24));
    FRandomStream R(2222);
    for(int i=0;i<380;i++)
    {
        FVector Pos(R.FRandRange(-6500,6500),R.FRandRange(-6500,6500),0);
        if(Pos.Size2D()<550) continue;
        bool NearDeposit=false; for(const auto& N:Sim.Nodes) if(FVector2D::Distance(FVector2D(Pos),N.Position)<330) NearDeposit=true;
        if(NearDeposit) continue;
        FLinearColor C=i%3?FLinearColor(.11,.38,.4):FLinearColor(.43,.16,.48);
        const float S=R.FRandRange(.4,1.6);
        Part(Ground,i%4?TEXT("Cone"):TEXT("Sphere"),Pos+FVector(0,0,60*S),FVector(.8*S,.8*S,1.6*S),C);
        if(i%4==0) Part(Ground,TEXT("Sphere"),Pos+FVector(0,0,125*S),FVector(1.5*S,1.5*S,.3*S),FLinearColor(.12,.65,.66));
    }
    for(const auto& N:Sim.Nodes)
    {
        const auto* Def=Sim.Resources.Find(N.Resource); FLinearColor C=Def?Def->Color:Mint;
        for(int i=0;i<7;i++) Part(Ground,TEXT("Cone"),FVector(N.Position.X+R.FRandRange(-120,120),N.Position.Y+R.FRandRange(-120,120),40),FVector(.65,.65,1.5),C,FRotator(R.FRandRange(-20,20),0,0));
        Part(Ground,TEXT("Cylinder"),FVector(N.Position,2),FVector(3.5,3.5,.04),C*.55f);
    }
}
void ASeigeGameMode::SyncVisuals()
{
    TSet<FString> Live;
    for(const auto& B:Sim.Buildings) if(B.Health>0) Live.Add(FString::Printf(TEXT("building_%d"),B.Id));
    for(const auto& C:Sim.Couriers) Live.Add(FString::Printf(TEXT("courier_%d"),C.Id));
    for(const auto& E:Sim.Enemies) Live.Add(FString::Printf(TEXT("enemy_%d"),E.Id));
    for(auto It=Visuals.CreateIterator();It;++It) if(!Live.Contains(It.Key())) { It.Value()->Destroy(); It.RemoveCurrent(); }
    for(auto& Pair:Visuals) Pair.Value->SetActorHiddenInGame(true);
    for(const auto& B:Sim.Buildings)
    {
        if(B.Health<=0) continue;
        auto* D=Sim.Definition(B); if(!D) continue;
        FString Kind=D->Visual; if(Kind.IsEmpty()) Kind=TEXT("Factory");
        Kind[0]=FChar::ToUpper(Kind[0]);
        Visual(FString::Printf(TEXT("building_%d"),B.Id),Kind,FVector(B.Position,0),D->Color,D->Footprint*1.5f);
        if(B.Enabled && B.Workers>=D->Jobs && D->DamagePerSecond>0 && FMath::Fmod(RenderClock,.3)<.12)
        {
            const FSeigeEnemy* Target=nullptr; double Closest=D->AttackRange;
            for(const auto& E:Sim.Enemies){ double Dist=FVector2D::Distance(B.Position,E.Position); if(Dist<Closest && Sim.IsVisible(E.Position)){Closest=Dist;Target=&E;} }
            if(Target) DrawDebugLine(GetWorld(),FVector(B.Position,D->Footprint*.7),FVector(Target->Position,50),FColor(140,245,235),false,0,0,3);
        }
    }
    for(const auto& C:Sim.Couriers)
    {
        auto* A=Visual(FString::Printf(TEXT("courier_%d"),C.Id),TEXT("Robot"),FVector(C.Position,12+FMath::Sin(RenderClock*4+C.Id)*5),Mint,85);
        if(auto* B=Sim.FindBuilding(C.TargetId)) A->SetActorRotation(FVector(B->Position-C.Position,0).Rotation());
    }
    for(const auto& E:Sim.Enemies)
    {
        if(!Sim.IsVisible(E.Position)) continue;
        auto* A=Visual(FString::Printf(TEXT("enemy_%d"),E.Id),TEXT("Bug"),FVector(E.Position,0),FLinearColor(.4,.08,.17),150);
        A->SetActorRotation(FVector(-E.Position,0).Rotation());
    }
    if(!SelectedBuild.IsEmpty() && CursorOnWorld)
    {
        FString Why; bool Valid=Sim.CanPlaceBuilding(SelectedBuild,CursorWorld,Why);
        const auto* D=Sim.BuildingDefs.Find(SelectedBuild);
        DrawDebugCircle(GetWorld(),FVector(CursorWorld,15),D?D->Footprint:100,40,Valid?FColor::Green:FColor::Red,false,0,0,3,FVector(1,0,0),FVector(0,1,0),false);
    }
    if(auto* B=Sim.FindBuilding(SelectedId)) if(auto* D=Sim.Definition(*B))
    {
        DrawDebugCircle(GetWorld(),FVector(B->Position,18),D->Footprint+20,40,FColor(99,242,208),false,0,0,3,FVector(1,0,0),FVector(0,1,0),false);
        if(D->AttackRange>0) DrawDebugCircle(GetWorld(),FVector(B->Position,12),D->AttackRange,80,FColor(255,180,80),false,0,0,1,FVector(1,0,0),FVector(0,1,0),false);
        if(D->SensorRange>0) DrawDebugCircle(GetWorld(),FVector(B->Position,10),D->SensorRange,80,FColor(80,170,220),false,0,0,1,FVector(1,0,0),FVector(0,1,0),false);
    }
}
void ASeigeGameMode::ClickWorld()
{
    if(!Ready||!CursorOnWorld) return;
    if(!SelectedBuild.IsEmpty())
    {
        if(Sim.PlaceBuilding(SelectedBuild,CursorWorld,Error)) Notice=TEXT("Building online. Staffing and deliveries are automatic."); else Notice=Error;
        return;
    }
    SelectedId=0; double Distance=350;
    for(const auto& B:Sim.Buildings) if(B.Health>0 && FVector2D::Distance(B.Position,CursorWorld)<Distance){ Distance=FVector2D::Distance(B.Position,CursorWorld); SelectedId=B.Id; }
}
ASeigeController::ASeigeController(){ bShowMouseCursor=true; PrimaryActorTick.bCanEverTick=true; }
void ASeigeController::BeginPlay(){ Super::BeginPlay(); FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); SetInputMode(Mode); }
void ASeigeController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt); auto* G=Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()); if(!G) return;
    FVector Origin,Direction; G->CursorOnWorld=DeprojectMousePositionToWorld(Origin,Direction)&&FMath::Abs(Direction.Z)>.0001;
    if(G->CursorOnWorld) G->CursorWorld=FVector2D(Origin+Direction*(-Origin.Z/Direction.Z));
    const float Step=G->Zoom*.65f*FMath::Min(Dt,.1f);
    if(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up)) G->CameraCenter+=FVector(-1,1,0)*Step;
    if(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down)) G->CameraCenter+=FVector(1,-1,0)*Step;
    if(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)) G->CameraCenter+=FVector(1,1,0)*Step;
    if(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)) G->CameraCenter+=FVector(-1,-1,0)*Step;
    if(WasInputKeyJustPressed(EKeys::MouseScrollUp)) G->Zoom=FMath::Max(2500.f,G->Zoom*.88f);
    if(WasInputKeyJustPressed(EKeys::MouseScrollDown)) G->Zoom=FMath::Min(16000.f,G->Zoom*1.12f);
    if(WasInputKeyJustPressed(EKeys::Home)){ G->CameraCenter=FVector::ZeroVector; G->Zoom=6500; }
    if(WasInputKeyJustPressed(EKeys::SpaceBar)) G->Paused=!G->Paused;
    if(WasInputKeyJustPressed(EKeys::F5)) G->SaveGame();
    if(WasInputKeyJustPressed(EKeys::F9)) G->LoadGame();
    if(WasInputKeyJustPressed(EKeys::RightMouseButton)||WasInputKeyJustPressed(EKeys::Escape)){ G->SelectedBuild.Empty(); G->SelectedId=0; }
    if(WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        float X,Y; GetMousePosition(X,Y); auto* UI=Cast<ASeigeHUD>(GetHUD()); if(!UI||!UI->Click(X,Y)) G->ClickWorld();
    }
    G->UpdateCamera();
}
void ASeigeHUD::Box(float X,float Y,float W,float H,FLinearColor C){ DrawRect(C,X*Scale,Y*Scale,W*Scale,H*Scale); }
void ASeigeHUD::Label(const FString& Text,float X,float Y,float Size,FLinearColor C)
{
    float TW,TH; Canvas->StrLen(GEngine->GetLargeFont(),TEXT("Ag"),TW,TH);
    DrawText(Text,C,X*Scale,Y*Scale,GEngine->GetLargeFont(),Size/FMath::Max(TH,1.f)*Scale,false);
}
void ASeigeHUD::Button(const FString& Text,const FString& Action,float X,float Y,float W,float H,bool Active)
{
    Box(X,Y,W,H,Active?FLinearColor(.13,.4,.36):FLinearColor(.065,.115,.145));
    Box(X,Y,Active?3:1,H,Active?Mint:FLinearColor(.13,.23,.27));
    Label(Text,X+10,Y+H*.25f,15,Active?Mint:FLinearColor(.87,.94,.95));
    Buttons.Add({FVector2D(X,Y),FVector2D(W,H),Action});
}
bool ASeigeHUD::Click(float X,float Y)
{
    auto* G=Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()); if(!G) return false;
    X/=Scale; Y/=Scale;
    for(const auto& B:Buttons) if(X>=B.Position.X&&X<=B.Position.X+B.Size.X&&Y>=B.Position.Y&&Y<=B.Position.Y+B.Size.Y)
    {
        if(B.Action==TEXT("pause")) G->Paused=!G->Paused;
        else if(B.Action==TEXT("speed")) G->Speed=G->Speed==1?3:1;
        else if(B.Action==TEXT("save")) G->SaveGame();
        else if(B.Action==TEXT("load")) G->LoadGame();
        else if(B.Action==TEXT("reset")) G->ResetColony();
        else if(B.Action==TEXT("continue")) G->WinAcknowledged=true;
        else if(B.Action==TEXT("toggle")) G->Sim.ToggleBuilding(G->SelectedId);
        else if(B.Action==TEXT("escape")){ G->Sim.LaunchShuttle(); G->Notice=TEXT("Shuttle launched. Only cargo already aboard leaves with you."); }
        else if(B.Action.StartsWith(TEXT("build:"))){ G->SelectedBuild=B.Action.RightChop(6); G->SelectedId=0; }
        return true;
    }
    if(G->Sim.Escaped || G->Sim.Failed || (G->Sim.Won && !G->WinAcknowledged)) return true;
    return Y<112||Y>Canvas->SizeY/Scale-180||X<300;
}
void ASeigeHUD::DrawHUD()
{
    Super::DrawHUD(); if(!Canvas) return;
    auto* G=Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()); if(!G) return;
    Scale=FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f); float W=Canvas->SizeX/Scale,H=Canvas->SizeY/Scale;
    Buttons.Reset(); Box(0,0,W,106,Ink); Box(0,104,W,2,Mint);
    Label(TEXT("seige2222"),24,15,30); Label(TEXT("FIRST LANDING  /  SINGLE-PLAYER PROTOTYPE"),26,53,12,Mint);
    Label(FString::Printf(TEXT("ROBOTS  %d / %d jobs"),G->Sim.Population,G->Sim.TotalJobs),365,20,18);
    Label(FString::Printf(TEXT("OPEN JOBS  %d     CARRIERS  %d"),FMath::Max(0,G->Sim.TotalJobs-G->Sim.Employed),G->Sim.Couriers.Num()),365,51,13,Muted);
    Label(FString::Printf(TEXT("PULSE %d   |   NEXT %.0fs"),G->Sim.Wave,FMath::Max(0.,G->Sim.NextWaveTime-G->Sim.Time)),680,20,18,Amber);
    Label(FString::Printf(TEXT("COLONY TIME  %02d:%02d"),int(G->Sim.Time)/60,int(G->Sim.Time)%60),680,51,13,Muted);
    Button(G->Paused?TEXT("Resume"):TEXT("Pause"),TEXT("pause"),W-385,17,90,34,G->Paused);
    Button(G->Speed==1?TEXT("1x speed"):TEXT("3x speed"),TEXT("speed"),W-285,17,90,34,G->Speed>1);
    Button(TEXT("Save F5"),TEXT("save"),W-185,17,80,34); Button(TEXT("Load F9"),TEXT("load"),W-95,17,80,34);
    Label(G->Notice.Left(170),24,80,13,Muted);
    if(!G->Ready){ Box(200,200,W-400,220,Panel); Label(TEXT("RULE FILE ERROR"),240,230,24,Amber); Label(G->Error,240,280,16); Button(TEXT("Reload corrected rules"),TEXT("reset"),240,350,300,40); return; }
    Box(16,124,272,H-320,Panel); Box(16,124,3,32,Mint); Label(TEXT("COLONY OVERVIEW"),32,139,15,Mint);
    float Y=172; TArray<FString> ResourceKeys; G->Sim.Resources.GetKeys(ResourceKeys); ResourceKeys.Sort();
    for(const FString& Key:ResourceKeys)
    {
        const auto& D=G->Sim.Resources[Key]; Box(32,Y+6,5,10,D.Color); Label(D.Name.Left(20),46,Y,14,Muted); Label(FString::Printf(TEXT("%.0f"),G->Sim.TotalStock(Key)),232,Y,14); Y+=23;
    }
    Y+=10; Label(TEXT("FIRST PLAYABLE OBJECTIVE"),32,Y,12,Mint); Y+=25;
    TArray<FString> Goals; G->Sim.ObjectiveText().ParseIntoArray(Goals,TEXT(" | "),true);
    for(const FString& Goal:Goals) { Label(Goal,32,Y,12); Y+=22; }
    Label(TEXT("Goods travel with visible robots."),32,Y,12,Muted); Y+=22;
    Label(TEXT("Protect routes, not just buildings."),32,Y,12,Muted);
    if(auto* B=G->Sim.FindBuilding(G->SelectedId))
    {
        if(auto* D=G->Sim.Definition(*B))
        {
            const float SX=W-310; Box(SX,124,294,340,Panel); Label(D->Name,SX+18,143,21); Label(B->Status.Left(35),SX+18,181,14,Mint);
            Label(FString::Printf(TEXT("HULL  %.0f / %.0f"),B->Health,D->Health),SX+18,210,15);
            Label(FString::Printf(TEXT("WORKERS  %d / %d"),B->Workers,D->Jobs),SX+18,235,15);
            Label(TEXT("LOCAL STOCKPILE"),SX+18,275,12,Muted); float IY=300;
            for(const auto& Stock:B->Inventory) if(Stock.Value>.1&&IY<395){ Label(FString::Printf(TEXT("%s  %.1f"),*Stock.Key,Stock.Value),SX+18,IY,13); IY+=21; }
            Button(B->Enabled?TEXT("Turn building off"):TEXT("Turn building on"),TEXT("toggle"),SX+18,411,258,34,!B->Enabled);
        }
    }
    auto* PC=GetOwningPlayerController();
    // Canvas feedback is available in Shipping builds, unlike editor debug drawing.
    auto WorldLine=[&](FVector A,FVector B,FLinearColor Color,float Thickness)
    {
        FVector2D P,Q;
        if(!PC || !PC->ProjectWorldLocationToScreen(A,P) || !PC->ProjectWorldLocationToScreen(B,Q)) return;
        auto InWorld=[&](FVector2D V){return V.X/Scale>300 && V.Y/Scale>110 && V.Y/Scale<H-185;};
        if(InWorld(P)&&InWorld(Q)) DrawLine(P.X,P.Y,Q.X,Q.Y,Color,Thickness*Scale);
    };
    auto WorldCircle=[&](FVector2D Center,double Radius,FLinearColor Color)
    {
        for(int32 I=0;I<64;I++)
        {
            const double A=I*UE_TWO_PI/64,B=(I+1)*UE_TWO_PI/64;
            WorldLine(FVector(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius,12),FVector(Center+FVector2D(FMath::Cos(B),FMath::Sin(B))*Radius,12),Color,1.5f);
        }
    };
    if(!G->SelectedBuild.IsEmpty() && G->CursorOnWorld)
    {
        FString Why; const auto* D=G->Sim.BuildingDefs.Find(G->SelectedBuild);
        const bool Valid=G->Sim.CanPlaceBuilding(G->SelectedBuild,G->CursorWorld,Why);
        if(D) WorldCircle(G->CursorWorld,D->Footprint,Valid?Mint:FLinearColor(1,.2f,.2f));
    }
    if(auto* B=G->Sim.FindBuilding(G->SelectedId)) if(const auto* D=G->Sim.Definition(*B))
    {
        WorldCircle(B->Position,D->Footprint+20,Mint);
        if(D->AttackRange>0) WorldCircle(B->Position,D->AttackRange,Amber);
        if(D->SensorRange>0) WorldCircle(B->Position,D->SensorRange,FLinearColor(.2f,.6f,1));
    }
    for(const auto& B:G->Sim.Buildings)
    {
        const auto* D=G->Sim.Definition(B);
        if(!D || B.Health<=0 || !B.Enabled || B.Workers<D->Jobs || D->DamagePerSecond<=0) continue;
        const FSeigeEnemy* Target=nullptr; double Closest=D->AttackRange;
        for(const auto& E:G->Sim.Enemies) if(G->Sim.IsVisible(E.Position))
        {
            const double Distance=FVector2D::Distance(B.Position,E.Position);
            if(Distance<Closest){Closest=Distance;Target=&E;}
        }
        if(Target && FMath::Fmod(G->Sim.Time,.3)<.12) WorldLine(FVector(B.Position,D->Footprint*.7),FVector(Target->Position,50),Mint,2);
    }
    for(const auto& N:G->Sim.Nodes)
    {
        FVector2D Screen; if(PC&&PC->ProjectWorldLocationToScreen(FVector(N.Position,190),Screen))
        {
            const auto* R=G->Sim.Resources.Find(N.Resource); if(R&&Screen.Y/Scale>112&&Screen.Y/Scale<H-190&&Screen.X/Scale>310)
            { Box(Screen.X/Scale-68,Screen.Y/Scale,136,25,FLinearColor(.025,.06,.08,.9)); Label(R->Name,Screen.X/Scale-60,Screen.Y/Scale+5,12,R->Color); }
        }
    }
    if(!G->SelectedBuild.IsEmpty())
    {
        const auto* D=G->Sim.BuildingDefs.Find(G->SelectedBuild); if(D)
        {
            Box(320,H-278,W-650,87,Panel); Label(D->Name+TEXT("  |  ")+D->Description.Left(70),334,H-268,13,Mint);
            FString Cost=TEXT("COST   "); for(const auto& C:D->Cost) Cost+=FString::Printf(TEXT("%s %.0f   "),*C.Key,C.Value);
            Label(Cost,334,H-244,12,Muted);
            FString RecipeText;
            if(const auto* Recipe=G->Sim.Recipes.Find(D->Recipe))
            {
                for(const auto& Input:Recipe->Inputs) RecipeText+=FString::Printf(TEXT("%.0f %s + "),Input.Value,*G->Sim.Resources[Input.Key].Name);
                RecipeText.RemoveFromEnd(TEXT(" + ")); RecipeText+=TEXT("  >  ");
                for(const auto& Output:Recipe->Outputs) RecipeText+=FString::Printf(TEXT("%.0f %s "),Output.Value,*G->Sim.Resources[Output.Key].Name);
                RecipeText+=FString::Printf(TEXT(" / %.0fs"),Recipe->Seconds);
            }
            else RecipeText=FString::Printf(TEXT("%d workers  |  Automatic operation and repairs"),D->Jobs);
            Label(RecipeText,334,H-219,12,Muted);
        }
    }
    Box(0,H-178,W,178,Ink); Label(TEXT("CONSTRUCTION"),24,H-165,13,Mint);
    Label(TEXT("Click a blueprint, then terrain  /  extractors need a matching deposit  /  right-click cancels"),190,H-165,13,Muted);
    const float BW=(W-48)/7.f;
    for(int32 i=0;i<G->Sim.BuildMenu.Num();i++)
    {
        const FString& Id=G->Sim.BuildMenu[i]; auto* D=G->Sim.BuildingDefs.Find(Id); if(D) Button(D->Name,TEXT("build:")+Id,24+(i%7)*BW,H-139+(i/7)*49,BW-8,41,G->SelectedBuild==Id);
    }
    Label(TEXT("WASD / arrows: pan    Wheel: zoom    Home: colony    Space: pause    F5 / F9: save / load"),24,H-25,12,Muted);
    Button(TEXT("Eject"),TEXT("escape"),W-192,H-32,74,25); Button(TEXT("Restart"),TEXT("reset"),W-107,H-32,83,25);
    if(G->Sim.Events.Num()) Label(G->Sim.Events.Last().Text.Left(130),320,120,14,Amber);
    if((G->Sim.Won&&!G->WinAcknowledged)||G->Sim.Escaped||G->Sim.Failed)
    {
        Buttons.Reset();
        const bool Victory=G->Sim.Won&&!G->Sim.Escaped&&!G->Sim.Failed;
        Box(W/2-310,H/2-110,620,180,Ink); Label(Victory?TEXT("FIRST LANDING COMPLETE"):TEXT("COLONY EVACUATED"),W/2-275,H/2-80,28,Mint);
        Label(Victory?TEXT("Your industry survived. Continue building or restart."):TEXT("The shuttle escaped. Restart to establish a new colony."),W/2-275,H/2-32,15);
        if(Victory) Button(TEXT("Continue building"),TEXT("continue"),W/2-275,H/2+9,266,37);
        Button(TEXT("Establish a new colony"),TEXT("reset"),Victory?W/2+9:W/2-275,H/2+9,Victory?266:550,37);
    }
}
