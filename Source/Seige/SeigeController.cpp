#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "SceneView.h"
ASeigeController::ASeigeController(){bShowMouseCursor=true;PrimaryActorTick.bCanEverTick=true;}
void ASeigeController::BeginPlay(){Super::BeginPlay();FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);SetInputMode(Mode);}
void ASeigeController::HandlePrimaryClick(float X,float Y)
{
    int32 Width=0,Height=0;GetViewportSize(Width,Height);
    if(!FMath::IsFinite(X)||!FMath::IsFinite(Y)||X<0||Y<0||(Width>0&&X>Width)||(Height>0&&Y>Height))return;
    auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;if(!G)return;
    if(GetLocalPlayer())UpdateCursorFromScreen(X,Y);
    auto* UI=Cast<ASeigeHUD>(GetHUD());if(UI&&UI->Click(X,Y))return;
    if(G->IsRegionMap()||OrbitActive)return;
    FVector Origin,Direction;
    if(GetLocalPlayer()&&ScreenRay(FVector2D(X,Y),Origin,Direction)&&G->SelectBuildingRay(Origin,Direction))return;
    G->ClickWorld();
}
bool ASeigeController::ScreenRay(const FVector2D& ScreenPosition,FVector& WorldOrigin,FVector& WorldDirection) const
{
    const ULocalPlayer* Local=GetLocalPlayer();
    if(ScreenPosition.ContainsNaN()||!Local||!Local->ViewportClient||!Local->ViewportClient->Viewport)return false;
    FSceneViewProjectionData Projection;
    if(!Local->GetProjectionData(Local->ViewportClient->Viewport,Projection))return false;
    const FIntRect Rect=Projection.GetConstrainedViewRect();
    if(Rect.Width()<=0||Rect.Height()<=0||ScreenPosition.X<Rect.Min.X||ScreenPosition.Y<Rect.Min.Y||ScreenPosition.X>Rect.Max.X||ScreenPosition.Y>Rect.Max.Y)return false;
    // The engine's screen deprojection snaps to whole pixels. Keep subpixel input
    // so a projected target remains the same terrain point even at region scale.
    const double X=2*(ScreenPosition.X-Rect.Min.X)/Rect.Width()-1;
    const double Y=1-2*(ScreenPosition.Y-Rect.Min.Y)/Rect.Height();
    const FMatrix Inverse=Projection.ComputeViewProjectionMatrix().InverseFast();
    const FVector4 Near=Inverse.TransformFVector4(FVector4(X,Y,1,1));
    const FVector4 Far=Inverse.TransformFVector4(FVector4(X,Y,.01,1));
    if(!FMath::IsFinite(Near.W)||!FMath::IsFinite(Far.W)||FMath::Abs(Near.W)<1.e-12||FMath::Abs(Far.W)<1.e-12)return false;
    const FVector Origin=FVector(Near.X,Near.Y,Near.Z)/Near.W;
    const FVector End=FVector(Far.X,Far.Y,Far.Z)/Far.W;
    if(Origin.ContainsNaN()||End.ContainsNaN())return false;
    const FVector Direction=(End-Origin).GetSafeNormal();
    if(Direction.IsNearlyZero()||Direction.ContainsNaN())return false;
    WorldOrigin=Origin;WorldDirection=Direction;return true;
}
bool ASeigeController::UpdateCursorFromScreen(float X,float Y)
{
    auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;
    if(!G||G->IsRegionMap())return false;
    FVector Origin,Direction,Hit;
    G->CursorOnWorld=ScreenRay(FVector2D(X,Y),Origin,Direction)&&G->TraceGroundRay(Origin,Direction,Hit);
    if(G->CursorOnWorld)G->CursorWorld=FVector2D(Hit)/G->RenderScale;
    return G->CursorOnWorld;
}
void ASeigeController::BeginOrbitGesture(float X,float Y)
{
    OrbitCursor=FVector2D(X,Y);OrbitActive=true;bShowMouseCursor=false;
    FInputModeGameOnly Mode;Mode.SetConsumeCaptureMouseDown(false);SetInputMode(Mode);
}
void ASeigeController::OrbitGestureDelta(float X,float Y)
{
    if(!OrbitActive)return;
    if(auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr)G->ApplyOrbitDrag(FVector2D(X,Y));
}
void ASeigeController::EndOrbitGesture()
{
    if(!OrbitActive)return;
    OrbitActive=false;bShowMouseCursor=true;
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);SetInputMode(Mode);
    SetMouseLocation(FMath::RoundToInt(OrbitCursor.X),FMath::RoundToInt(OrbitCursor.Y));
}
void ASeigeController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;if(!G)return;
    auto* UI=Cast<ASeigeHUD>(GetHUD());
    bool Consumed=false;
    for(auto It=ConsumedKeysUntilRelease.CreateIterator();It;++It)if(!IsInputKeyDown(*It))It.RemoveCurrent();
    TArray<FKey> Keys={EKeys::Escape,EKeys::RightMouseButton,EKeys::SpaceBar,EKeys::F5,EKeys::F9,EKeys::F10,EKeys::Add,EKeys::Subtract,EKeys::Equals,EKeys::Hyphen};
    for(TCHAR Letter=TEXT('A');Letter<=TEXT('Z');++Letter)Keys.Add(FKey(FName(*FString::Chr(Letter))));
    for(const FKey& Key:Keys)if(WasInputKeyJustPressed(Key))
    {
        if(UI){const bool Handled=UI->HandleShortcut(Key);Consumed=Handled||Consumed;if(Handled)ConsumedKeysUntilRelease.Add(Key);}
        else if(Key==EKeys::Escape||Key==EKeys::RightMouseButton){G->SelectedBuild.Empty();G->SelectedId=0;Consumed=true;}
    }
    const bool CameraScreen=G->Screen==TEXT("playing")||G->Screen==TEXT("landing");
    if(OrbitActive&&(!IsInputKeyDown(EKeys::MiddleMouseButton)||!CameraScreen||G->IsRegionMap()))EndOrbitGesture();
    if(CameraScreen&&!G->IsRegionMap()&&WasInputKeyJustPressed(EKeys::MiddleMouseButton)&&!(UI&&UI->IsPointerOverUI()))
    {float X=0,Y=0;if(GetMousePosition(X,Y))BeginOrbitGesture(X,Y);}
    if(OrbitActive){float X=0,Y=0;GetInputMouseDelta(X,Y);OrbitGestureDelta(X,-Y);}
    const bool Blocked=!CameraScreen||Consumed||!ConsumedKeysUntilRelease.IsEmpty()||(UI&&UI->BlocksCameraKeys());
    if(!Blocked&&!G->IsRegionMap())
    {
        const float Step=G->Zoom*.65f*FMath::Min(Dt,.1f);
        const float Forward=(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up)?1.f:0.f)-(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down)?1.f:0.f);
        const float Right=(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)?1.f:0.f)-(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)?1.f:0.f);
        G->CameraCenter+=FVector(G->CameraPanDirection(Forward,Right),0)*Step;
        if(IsInputKeyDown(EKeys::Q))G->CameraYaw-=55*Dt;
        if(IsInputKeyDown(EKeys::E))G->CameraYaw+=55*Dt;
        G->CameraYaw=FRotator::ClampAxis(G->CameraYaw);
    }
    if(CameraScreen&&(G->IsRegionMap()||!(UI&&UI->IsPointerOverUI())))
    {
        if(WasInputKeyJustPressed(EKeys::MouseScrollUp))
        {
            float X=0,Y=0;FString Action;
            if(G->IsRegionMap()&&UI&&GetMousePosition(X,Y))Action=UI->Ui.HitTest(X,Y);
            if(Action.StartsWith(TEXT("focus-sector:")))G->FocusSector(FCString::Atoi(*Action.Mid(13)),true);
            else G->Zoom=FMath::Max(G->MinimumZoom,G->Zoom*.88f);
        }
        if(WasInputKeyJustPressed(EKeys::MouseScrollDown))G->Zoom=FMath::Min(G->MaximumZoom,G->Zoom*1.12f);
    }
    const double Limit=G->Sim.WorldHalfSize*2.8;
    G->CameraCenter.X=FMath::Clamp(G->CameraCenter.X,-Limit,Limit);G->CameraCenter.Y=FMath::Clamp(G->CameraCenter.Y,-Limit,Limit);
    if(!Blocked&&WasInputKeyJustPressed(EKeys::Home)){G->CameraCenter=FVector(G->HomePosition(),0);G->Zoom=G->DefaultZoom;G->CameraYaw=135;G->CameraPitch=52;}
    G->UpdateCamera(Dt);
    if(PlayerCameraManager)PlayerCameraManager->UpdateCamera(Dt);
    float X=0,Y=0;G->CursorOnWorld=false;
    if(!OrbitActive&&GetMousePosition(X,Y)){UpdateCursorFromScreen(X,Y);if(WasInputKeyJustPressed(EKeys::LeftMouseButton))HandlePrimaryClick(X,Y);}
}
