#include "SeigeGameMode.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

bool ASeigeGameMode::EnterCompanionView(int32 Id)
{
    const auto* Dog=Sim.Companions.Find(Id);
    if(!Ready||Screen!=TEXT("playing")||Observer||Sim.Escaped||Sim.Failed||!Dog||Dog->Evacuated)return false;
    if(!CompanionView)
    {
        SavedColonyCamera=CameraCenter;SavedColonyZoom=Zoom;SavedColonyYaw=CameraYaw;SavedColonyPitch=CameraPitch;
    }
    if(!Sim.Companions.SetControlled(Id))return false;
    CompanionView=true;SelectedCompanionId=Id;CompanionYaw=Dog->Heading;CompanionPitch=0;
    // Input can enter this mode after SyncVisuals. Hide the body before moving
    // the camera into its head, rather than waiting for the next scenery tick.
    if(auto* Actor=Visuals.FindRef(FString::Printf(TEXT("companion_%d"),Id)).Get())Actor->SetActorHiddenInGame(true);
    Speed=1;Paused=false;Accumulator=0;SelectedId=SelectedRoadId=0;SelectedBuild.Empty();CancelRoadTool();
    ResetSimulationPresentation();CameraCenter=FVector(Dog->Position,0);Zoom=MinimumZoom;RenderedZoom=MinimumZoom;
    Notice=TEXT("ROAM AS REX | WASD walks, mouse looks. Esc returns to the colony. Playback stays at 1x.");
    UpdateCamera();return true;
}
void ASeigeGameMode::ExitCompanionView()
{
    const int32 ControlledId=Sim.Companions.ControlledId;
    Sim.Companions.SetControlled(0);
    if(!CompanionView)return;
    CompanionView=false;CameraCenter=SavedColonyCamera;Zoom=SavedColonyZoom;RenderedZoom=Zoom;CameraYaw=SavedColonyYaw;CameraPitch=SavedColonyPitch;
    if(auto* Actor=Visuals.FindRef(FString::Printf(TEXT("companion_%d"),ControlledId)).Get())Actor->SetActorHiddenInGame(false);
    UpdateCamera();
}
void ASeigeGameMode::FocusCompanion(int32 Id)
{
    if(Screen!=TEXT("playing")||Observer||Sim.Escaped||Sim.Failed)return;
    if(const auto* D=Sim.Companions.Find(Id))if(!D->Evacuated)
    {
        ExitCompanionView();CancelRoadTool();SelectedId=SelectedRoadId=0;SelectedBuild.Empty();SelectedCompanionId=Id;
        CameraCenter=FVector(D->Position,0);Zoom=MinimumZoom*3;
    }
}
void ASeigeGameMode::LookCompanion(FVector2D Pixels)
{
    if(!CompanionView||Pixels.ContainsNaN())return;
    CompanionYaw=FRotator::ClampAxis(CompanionYaw+Pixels.X*Sim.Companions.LookSensitivity);
    CompanionPitch=FMath::Clamp(CompanionPitch-Pixels.Y*Sim.Companions.LookSensitivity,-75.f,75.f);
}
void ASeigeGameMode::MoveCompanion(float Forward,float Right)
{
    if(!CompanionView)return;
    const auto* D=Sim.Companions.Find(Sim.Companions.ControlledId);if(!D)return;
    const double Angle=FMath::DegreesToRadians(CompanionYaw);
    FVector2D Direction(FMath::Cos(Angle)*Forward-FMath::Sin(Angle)*Right,FMath::Sin(Angle)*Forward+FMath::Cos(Angle)*Right);
    Direction=Direction.GetClampedToMaxSize(1.);
    // The simulation owns actual walking speed. This world adapter additionally
    // rejects steep slopes and neighboring colony walls using terrain/world data.
    const double Step=Sim.Companions.WalkKmh/3.6/Sim.MetersPerWorldUnit()*Sim.FixedStepSeconds();
    const FVector2D Next=D->Position+Direction*Step;
    if(!Direction.IsNearlyZero())
    {
        const double Run=FVector2D::Distance(Next,D->Position);
        if(FMath::Abs(GroundHeight(Next)-GroundHeight(D->Position))>Run*Sim.Companions.MaximumSlopeGrade)Direction=FVector2D::ZeroVector;
        for(const auto& N:Neighbors)for(const auto& B:N.Sim.Buildings)if(B.Health>0)if(const auto* Def=N.Sim.Definition(B))
        {
            const FVector2D Delta=Next-(B.Position+N.Offset);
            const double Radius=Def->Footprint+Sim.Companions.BodyRadiusMeters/Sim.MetersPerWorldUnit();
            if(FMath::Abs(Delta.X)<Radius&&FMath::Abs(Delta.Y)<Radius)Direction=FVector2D::ZeroVector;
        }
    }
    Sim.Companions.SetControlDirection(Screen==TEXT("playing")&&!Paused?Direction:FVector2D::ZeroVector);
}
FVector2D ASeigeGameMode::CompanionRenderPosition() const
{
    const auto* Dog=Sim.Companions.Find(Sim.Companions.ControlledId);
    if(!Dog)return FVector2D(CameraCenter);
    const auto* Snapshot=PresentationSnapshot(Sim);
    return Snapshot?Snapshot->Companion(*Dog,PresentationAlpha()):Dog->Position;
}
void ASeigeGameMode::SyncCompanionVisuals(TSet<FString>& Live)
{
    if(Screen==TEXT("landing")||(MenuOpen&&MenuReturnScreen==TEXT("landing"))||Sim.Escaped||Sim.Failed)return;
    if(Screen!=TEXT("playing")&&!MenuOpen)return;
    const FString AssetKey=Sim.Companions.MeshPath+TEXT("|")+Sim.Companions.WalkAnimation+TEXT("|")+Sim.Companions.IdleAnimation;
    if(CompanionVisualAssetKey!=AssetKey)
    {
        CompanionVisualAssetKey=AssetKey;
        CompanionMesh=LoadObject<USkeletalMesh>(nullptr,*Sim.Companions.MeshPath,nullptr,LOAD_NoWarn);
        CompanionWalk=LoadObject<UAnimSequence>(nullptr,*Sim.Companions.WalkAnimation,nullptr,LOAD_NoWarn);
        CompanionIdle=LoadObject<UAnimSequence>(nullptr,*Sim.Companions.IdleAnimation,nullptr,LOAD_NoWarn);
    }
    if(!CompanionMesh)return;
    for(const auto& D:Sim.Companions.Dogs)
    {
        if(D.Evacuated)continue;
        const auto* Snapshot=PresentationSnapshot(Sim);
        const FVector2D P=Snapshot?Snapshot->Companion(D,PresentationAlpha()):D.Position;
        if(FVector2D::Distance(P,FVector2D(CameraCenter))*Sim.MetersPerWorldUnit()>600)continue;
        const FString Key=FString::Printf(TEXT("companion_%d"),D.Id);Live.Add(Key);
        auto* Actor=Visuals.FindRef(Key).Get();USkeletalMeshComponent* Mesh=nullptr;
        if(!Actor)
        {
            Actor=GetWorld()->SpawnActor<AActor>();Mesh=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Mesh);
            Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetSkeletalMeshAsset(CompanionMesh);
            Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            Mesh->RegisterComponent();Actor->AddInstanceComponent(Mesh);Visuals.Add(Key,Actor);
        }
        else Mesh=Actor->FindComponentByClass<USkeletalMeshComponent>();
        Actor->SetActorLocation(RenderPosition(P,2));Actor->SetActorRotation(FRotator(0,D.Heading,0));
        Actor->SetActorHiddenInGame(CompanionView&&Sim.Companions.ControlledId==D.Id);
        if(Mesh)
        {
            auto* Animation=D.Moving?CompanionWalk.Get():CompanionIdle.Get();
            if(Animation)
            {
                if(!Mesh->GetSingleNodeInstance()||Mesh->GetSingleNodeInstance()->GetAnimationAsset()!=Animation)Mesh->PlayAnimation(Animation,true);
                // Explicit render-time sampling stays smooth at 1x and freezes
                // with pause; animation never advances on an independent clock.
                const double Duration=FMath::Max(.01,Animation->GetPlayLength());
                const double Travel=FMath::Max(0.,D.DistanceWalked-FVector2D::Distance(P,D.Position)*Sim.MetersPerWorldUnit());
                const double Phase=D.Moving?Travel/Sim.Companions.WalkCycleMeters*Duration:RenderSimulationTime(Sim);
                Mesh->SetPosition(FMath::Fmod(Phase,Duration),false);
                Mesh->SetPlayRate(0);
            }
        }
    }
}
