#include "SeigeGameMode.h"

bool ASeigeGameMode::IsRegionMap() const
{
    return (Screen==TEXT("playing")||Screen==TEXT("landing"))&&Zoom>=RegionMapZoom;
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
void ASeigeGameMode::FocusSector(int32 Index)
{
    if(Index<0||Index>8||(Screen==TEXT("landing")&&Index!=4))return;
    CameraCenter=FVector(FVector2D(Index%3-1,Index/3-1)*Sim.WorldHalfSize*2,0);
    if(Index==4)CameraCenter=FVector(HomePosition(),0);
    else if(Observer)for(const auto& N:Neighbors)if(N.Index==Index)for(const auto& B:N.Sim.Buildings)
        if(const auto* D=N.Sim.Definition(B))if(D->Role==TEXT("core")){CameraCenter=FVector(B.Position+N.Offset,0);break;}
    Zoom=DefaultZoom*2;SelectedId=0;SelectedBuild.Empty();
    UpdateCamera();
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
