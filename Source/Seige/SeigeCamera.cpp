#include "SeigeGameMode.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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
    float Forest=0,NearForest=0;
    if(!Read(TEXT("world_centimeters_per_unit"),1,20,RenderScale)||
       !Read(TEXT("camera_fov"),35,80,CameraFov)||!Read(TEXT("camera_pitch"),25,75,CameraPitch)||
       !Read(TEXT("camera_yaw"),-360,360,CameraYaw)||!Read(TEXT("default_zoom"),900,20000,DefaultZoom)||
       !Read(TEXT("minimum_zoom"),300,2000,MinimumZoom)||!Read(TEXT("forest_candidates"),1000,200000,Forest)||
       !Read(TEXT("near_forest_candidates"),100,30000,NearForest))return false;
    if(MinimumZoom>DefaultZoom){Error=TEXT("Minimum camera zoom exceeds default zoom");return false;}
    if(FMath::FloorToFloat(Forest)!=Forest||FMath::FloorToFloat(NearForest)!=NearForest){Error=TEXT("Forest candidate counts must be integers");return false;}
    ForestCandidates=FMath::RoundToInt(Forest);NearForestCandidates=FMath::RoundToInt(NearForest);
    const TSharedPtr<FJsonObject>* Assets=nullptr;
    if(!Root->TryGetObjectField(TEXT("nature_assets"),Assets)){Error=TEXT("Missing nature asset definitions");return false;}
    for(const FString Key:{TEXT("OakA"),TEXT("OakB"),TEXT("PineA"),TEXT("PineB"),TEXT("Shrub"),TEXT("Grass"),TEXT("RockA"),TEXT("RockB")})
    {
        FString Path;
        if(!(*Assets)->TryGetStringField(Key,Path)||!Path.StartsWith(TEXT("/Game/"))){Error=TEXT("Missing or invalid nature asset path: ")+Key;return false;}
        NatureAssets.Add(Key,Path);
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
    const FRotator Aim(-FMath::Clamp(CameraPitch,25.f,75.f),CameraYaw,0);
    FVector Position=Target-Aim.Vector()*Distance;
    const FVector2D Logical(Position.X/RenderScale,Position.Y/RenderScale);
    if(FMath::Abs(Logical.X)<=Sim.WorldHalfSize*3&&FMath::Abs(Logical.Y)<=Sim.WorldHalfSize*3)
        Position.Z=FMath::Max(Position.Z,GroundHeight(Logical)*RenderScale+300);
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
    if(Direction.Z>=-.00001)return false;
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
    const double Step=FMath::Clamp((Sim.WorldHalfSize*2/256)*.4/FMath::Max(FMath::Abs(Direction.X),FMath::Max(FMath::Abs(Direction.Y),.05)),20.,500.);
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
