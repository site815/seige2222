#include "SeigeGameMode.h"

bool ASeigeGameMode::IsWorldVisible(FVector2D P) const
{
    if(Sim.IsVisible(P))return true;
    const double Span=Sim.WorldHalfSize*2;
    const int32 X=FMath::FloorToInt((P.X+Span*.5)/Span),Y=FMath::FloorToInt((P.Y+Span*.5)/Span);
    if(X< -1||X>1||Y< -1||Y>1)return false;
    return Sim.Combat.IsVisibleInSector((Y+1)*3+X+1,P-FVector2D(X,Y)*Span);
}

bool ASeigeGameMode::IsRegionMap() const
{
    return (Screen==TEXT("playing")||Screen==TEXT("landing"))&&CameraViewZoom()>=RegionMapZoom;
}
float ASeigeGameMode::RegionMapAlpha() const
{
    if(Screen!=TEXT("playing")&&Screen!=TEXT("landing"))return 0;
    return FMath::SmoothStep(RegionMapZoom-RegionMapTransitionWidth*.5f,RegionMapZoom+RegionMapTransitionWidth*.5f,CameraViewZoom());
}
int32 ASeigeGameMode::DetailedSectorIndex() const
{
    const double Span=FMath::Max(1.,Sim.WorldHalfSize*2);
    const int32 X=FMath::Clamp(FMath::FloorToInt((CameraCenter.X+Span*.5)/Span),-1,1);
    const int32 Y=FMath::Clamp(FMath::FloorToInt((CameraCenter.Y+Span*.5)/Span),-1,1);
    return (Y+1)*3+X+1;
}
FVector2D ASeigeGameMode::DetailedSectorOffset() const
{
    const int32 Index=DetailedSectorIndex();
    return FVector2D(Index%3-1,Index/3-1)*Sim.WorldHalfSize*2;
}
const FSeigeSimulation* ASeigeGameMode::ViewedSimulation() const
{
    const int32 Index=DetailedSectorIndex();
    if(Index==4)return &Sim;
    for(const auto& N:Neighbors)if(N.Index==Index)return &N.Sim;
    return nullptr;
}
void ASeigeGameMode::FocusSector(int32 Index,bool SmoothTransition)
{
    if(Index<0||Index>8||(Screen==TEXT("landing")&&Index!=4))return;
    if(SmoothTransition&&IsRegionMap())RegionViewHistory.Add({CameraCenter,Zoom,CameraYaw,CameraPitch,SelectedId,SelectedRoadId,SelectedCompanionId});
    CameraCenter=FVector(FVector2D(Index%3-1,Index/3-1)*Sim.WorldHalfSize*2,0);
    if(Index==4)CameraCenter=FVector(HomePosition(),0);
    else if(Observer)for(const auto& N:Neighbors)if(N.Index==Index)for(const auto& B:N.Sim.Buildings)
        if(const auto* D=N.Sim.Definition(B))if(D->Role==TEXT("core")){CameraCenter=FVector(B.Position+N.Offset,0);break;}
    Zoom=DefaultZoom*2;SelectedId=0;SelectedBuild.Empty();
    // User map navigation uses the same damped zoom as the wheel. Direct setup
    // calls retain their deterministic snap for loading and verification.
    if(!SmoothTransition)UpdateCamera();
}
void ASeigeGameMode::ToggleRegionSurvey()
{
    if(IsRegionMap()){if(!RestoreRegionView())FocusSector(4,true);return;}
    if(!RegionViewHistory.IsEmpty()&&RegionViewHistory.Last().Zoom>=RegionMapZoom){RestoreRegionView();return;}
    RegionViewHistory.Add({CameraCenter,Zoom,CameraYaw,CameraPitch,SelectedId,SelectedRoadId,SelectedCompanionId});
    SelectedId=SelectedRoadId=SelectedCompanionId=0;SelectedBuild.Empty();
    CancelRoadTool();CancelWallTool();CameraCenter=FVector::ZeroVector;Zoom=MaximumZoom;
}
bool ASeigeGameMode::RestoreRegionView()
{
    if(RegionViewHistory.IsEmpty())return false;
    const auto Saved=RegionViewHistory.Pop();
    CameraCenter=Saved.Center;Zoom=Saved.Zoom;CameraYaw=Saved.Yaw;CameraPitch=Saved.Pitch;
    SelectedId=Saved.Building;SelectedRoadId=Saved.Road;SelectedCompanionId=Saved.Companion;
    return true;
}
FVector ASeigeGameMode::WorkerWorldPosition(const FSeigeWorker& Worker,int32 Sector) const
{
    const FString Prefix=Sector==4?FString(TEXT("home_")):FString::Printf(TEXT("zone_%d_"),Sector);
    if(const auto* Actor=Visuals.FindRef(Prefix+TEXT("worker_")+Worker.Id).Get())
        if(!Actor->IsHidden())return Actor->GetActorLocation();
    const FVector2D Offset=FVector2D(Sector%3-1,Sector/3-1)*Sim.WorldHalfSize*2;
    return RenderPosition(Worker.Position+Offset,30);
}
void ASeigeGameMode::ApplyOrbitDrag(FVector2D Pixels)
{
    if(Pixels.ContainsNaN()||IsRegionMap())return;
    CameraYaw=FRotator::ClampAxis(CameraYaw+Pixels.X*OrbitYawPerPixel);
    CameraPitch=FMath::Clamp(CameraPitch+Pixels.Y*OrbitPitchPerPixel,MinimumCameraPitch,MaximumCameraPitch);
}
float ASeigeGameMode::WoodlandDensity(FVector2D P) const
{
    // Broad continuous woodland surrounds a winding meadow, with irregular edges.
    const double Span=Sim.WorldHalfSize*2;
    const FVector2D Offset(FMath::RoundToDouble(P.X/Span)*Span,FMath::RoundToDouble(P.Y/Span)*Span);
    const FVector2D Local=P-Offset;
    const double Winding=FMath::Sin(Local.X/2700)*650+FMath::Sin(Local.X/7100)*950;
    const double Edge=FMath::Abs(Local.Y-Winding)+FMath::PerlinNoise2D(P/600+FVector2D(6,13))*380;
    const double Meadow=FMath::SmoothStep(1050.,2050.,Edge);
    const double Patch=FMath::PerlinNoise2D(P/4400+FVector2D(4,19));
    return FMath::Clamp(Meadow*FMath::SmoothStep(-.4,.05,Patch),0.,1.);
}
