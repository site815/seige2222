#include "SeigeGameMode.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"

bool ASeigeGameMode::EnterCompanionView(int32 Id)
{
    const auto* Dog=Sim.Companions.Find(Id);
    if(!Ready||Screen!=TEXT("playing")||MenuOpen){Notice=TEXT("Return to the active colony to roam as Rex.");return false;}
    if(Sim.Escaped||Sim.Failed||!Dog||Dog->Evacuated){Notice=TEXT("Rex is no longer available in this colony.");return false;}
    if(!Sim.Companions.SetControlled(Id))
    {
        Notice=Dog->Dead?TEXT("Rex has died and cannot be controlled."):
            Dog->Aboard?TEXT("Rex will be available after the shuttle lands and he walks outside."):
            Dog->Retreating||Dog->Sheltered?TEXT("Rex is taking shelter. Roaming resumes once he is healed and safely outside."):
            TEXT("Wait for Rex to finish leaving the shuttle before roaming.");
        return false;
    }
    if(!CompanionView)
    {
        SavedColonyCamera=CameraCenter;SavedColonyZoom=Zoom;SavedColonyYaw=CameraYaw;SavedColonyPitch=CameraPitch;
    }
    // Roaming controls only the companion. An observed center colony keeps its
    // AI controller and observer restrictions throughout the companion visit.
    CompanionView=true;SelectedCompanionId=Id;CompanionYaw=CompanionSteeringYaw=Dog->Heading;CompanionPitch=CompanionCameraDefaultPitch;CompanionFreeLook=false;
    Sim.Companions.SetControlFacing(CompanionSteeringYaw);
    Sim.Companions.SetMovementConstraint([this](FVector2D From,FVector2D To,double RadiusMeters)
    {
        const double Run=FVector2D::Distance(From,To);
        if(Run>UE_DOUBLE_SMALL_NUMBER&&FMath::Abs(GroundHeight(To)-GroundHeight(From))>Run*Sim.Companions.MaximumSlopeGrade)return false;
        for(const auto& N:Neighbors)for(const auto& B:N.Sim.Buildings)if(B.Health>0)if(const auto* Def=N.Sim.Definition(B))
        {
            const FVector2D Delta=To-(B.Position+N.Offset);
            const double Radius=Def->ReservedFootprint+RadiusMeters/Sim.MetersPerWorldUnit();
            if(FMath::Abs(Delta.X)<Radius&&FMath::Abs(Delta.Y)<Radius)return false;
        }
        return true;
    });
    // Keep the animated dog visible from the third-person follow camera.
    if(auto* Actor=Visuals.FindRef(FString::Printf(TEXT("companion_%d"),Id)).Get())Actor->SetActorHiddenInGame(false);
    Speed=1;Paused=false;Accumulator=0;SelectedId=SelectedRoadId=0;SelectedBuild.Empty();CancelRoadTool();CancelWallTool();FleetOrderActive=false;
    ResetSimulationPresentation();CameraCenter=FVector(Dog->Position,0);Zoom=MinimumZoom;RenderedZoom=MinimumZoom;
    Notice=TEXT("REX | WASD moves, mouse steers, Shift runs, Alt + mouse looks around. Esc returns. Playback stays at 1x.");
    UpdateCamera();return true;
}
void ASeigeGameMode::ExitCompanionView()
{
    const int32 ControlledId=Sim.Companions.ControlledId;
    Sim.Companions.SetControlled(0);
    Sim.Companions.SetMovementConstraint({});
    if(!CompanionView)return;
    CompanionView=false;CompanionFreeLook=false;CameraCenter=SavedColonyCamera;Zoom=SavedColonyZoom;RenderedZoom=Zoom;CameraYaw=SavedColonyYaw;CameraPitch=SavedColonyPitch;
    if(auto* Actor=Visuals.FindRef(FString::Printf(TEXT("companion_%d"),ControlledId)).Get())Actor->SetActorHiddenInGame(false);
    UpdateCamera();
}
void ASeigeGameMode::FocusCompanion(int32 Id)
{
    if(Screen!=TEXT("playing")||Sim.Escaped||Sim.Failed)return;
    if(const auto* D=Sim.Companions.Find(Id))if(!D->Evacuated)
    {
        ExitCompanionView();CancelRoadTool();CancelWallTool();FleetOrderActive=false;SelectedId=SelectedRoadId=0;SelectedBuild.Empty();SelectedCompanionId=Id;
        // A useful elevated close view keeps a moving dog in frame at 10x;
        // ground-level minimum zoom can put the camera behind a nearby ridge.
        CameraCenter=FVector(D->Aboard||D->Sheltered||D->Dead?D->Home:D->Position,0);
        Zoom=FMath::Max(MinimumZoom*3,DefaultZoom*.35f);CameraPitch=FMath::Min(MaximumCameraPitch,65.f);
        Notice=D->Dead?TEXT("Rex has died. Workers move at half speed during their month of mourning."):D->Aboard?TEXT("Rex is aboard the landing shuttle."):D->Sheltered?TEXT("Rex is healing safely inside the shuttle."):TEXT("Rex is near the orbital command module.");
    }
}
void ASeigeGameMode::LookCompanion(FVector2D Pixels,bool FreeLook)
{
    if(!CompanionView||Pixels.ContainsNaN())return;
    CompanionFreeLook=FreeLook;
    const double YawDelta=Pixels.X*Sim.Companions.LookSensitivity;
    CompanionYaw=FRotator::ClampAxis(CompanionYaw+YawDelta);
    if(!FreeLook)
    {
        CompanionSteeringYaw=FRotator::ClampAxis(CompanionSteeringYaw+YawDelta);
        Sim.Companions.SetControlFacing(CompanionSteeringYaw);
    }
    CompanionPitch=FMath::Clamp(CompanionPitch-Pixels.Y*Sim.Companions.LookSensitivity,CompanionCameraMinPitch,CompanionCameraMaxPitch);
}
void ASeigeGameMode::MoveCompanion(float Forward,float Right,bool Sprint)
{
    if(!CompanionView)return;
    const auto* D=Sim.Companions.Find(Sim.Companions.ControlledId);if(!D)return;
    const double Angle=FMath::DegreesToRadians(CompanionSteeringYaw);
    FVector2D Direction(FMath::Cos(Angle)*Forward-FMath::Sin(Angle)*Right,FMath::Sin(Angle)*Forward+FMath::Cos(Angle)*Right);
    Direction=Direction.GetClampedToMaxSize(1.);
    // The simulation integrates acceleration and body-aligned turns, then checks
    // each actual stride with the world constraint installed on entry.
    const bool MovingAllowed=Screen==TEXT("playing")&&!Paused;
    Sim.Companions.SetControlSprint(MovingAllowed&&Sprint);
    Sim.Companions.SetControlDirection(MovingAllowed?Direction:FVector2D::ZeroVector);
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
    if(CompanionView&&Sim.Companions.ControlledId==0){ExitCompanionView();Notice=TEXT("Rex is hurt or in danger and is returning to the shuttle.");}
    if(Screen==TEXT("landing")||(MenuOpen&&MenuReturnScreen==TEXT("landing"))||Sim.Escaped||Sim.Failed)return;
    if(Screen!=TEXT("playing")&&!MenuOpen)return;
    const FString AssetKey=Sim.Companions.MeshPath+TEXT("|")+Sim.Companions.WalkAnimation+TEXT("|")+Sim.Companions.RunAnimation+TEXT("|")+Sim.Companions.IdleAnimation+TEXT("|")+Sim.Companions.LocomotionBlendSpace;
    if(CompanionVisualAssetKey!=AssetKey)
    {
        CompanionVisualAssetKey=AssetKey;
        CompanionMesh=LoadObject<USkeletalMesh>(nullptr,*Sim.Companions.MeshPath,nullptr,LOAD_NoWarn);
        CompanionWalk=LoadObject<UAnimSequence>(nullptr,*Sim.Companions.WalkAnimation,nullptr,LOAD_NoWarn);
        CompanionRun=LoadObject<UAnimSequence>(nullptr,*Sim.Companions.RunAnimation,nullptr,LOAD_NoWarn);
        CompanionIdle=LoadObject<UAnimSequence>(nullptr,*Sim.Companions.IdleAnimation,nullptr,LOAD_NoWarn);
        CompanionLocomotion=LoadObject<UBlendSpace>(nullptr,*Sim.Companions.LocomotionBlendSpace,nullptr,LOAD_NoWarn);
        CompanionPoseStates.Reset();
    }
    if(!CompanionMesh)return;
    for(const auto& D:Sim.Companions.Dogs)
    {
        if(!Sim.Companions.IsOutside(D))continue;
        const auto* Snapshot=PresentationSnapshot(Sim);
        const double Alpha=PresentationAlpha();
        const FVector2D P=Snapshot?Snapshot->Companion(D,Alpha):D.Position;
        const double Heading=Snapshot?Snapshot->CompanionHeading(D,Alpha):D.Heading;
        const double SpeedMeters=Snapshot?Snapshot->CompanionSpeed(D,Alpha):D.SpeedMetersPerSecond;
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
        // Match the body's support plane to the terrain instead of skating
        // horizontally through slopes. Heading itself interpolates at render rate.
        const double Yaw=FMath::DegreesToRadians(Heading),Probe=CompanionGroundProbe/RenderScale;
        const FVector2D Forward(FMath::Cos(Yaw),FMath::Sin(Yaw)),Right(-Forward.Y,Forward.X);
        const double Pitch=FMath::RadiansToDegrees(FMath::Atan2(GroundHeight(P+Forward*Probe)-GroundHeight(P-Forward*Probe),Probe*2));
        const double Roll=-FMath::RadiansToDegrees(FMath::Atan2(GroundHeight(P+Right*Probe)-GroundHeight(P-Right*Probe),Probe*2));
        Actor->SetActorLocation(RenderPosition(P,2));Actor->SetActorRotation(FRotator(FMath::Clamp(Pitch,-double(CompanionMaximumGroundTilt),double(CompanionMaximumGroundTilt)),Heading,FMath::Clamp(Roll,-double(CompanionMaximumGroundTilt),double(CompanionMaximumGroundTilt))));
        Actor->SetActorHiddenInGame(false);
        if(Mesh)
        {
            const double Travel=Snapshot?Snapshot->CompanionDistance(D,Alpha):D.DistanceWalked;
            const double Time=RenderSimulationTime(Sim);
            const double RunWeight=FMath::Clamp((SpeedMeters-Sim.Companions.WalkKmh/3.6)/FMath::Max(.01,(Sim.Companions.RunKmh-Sim.Companions.WalkKmh)/3.6),0.,1.);
            const double Stride=FMath::Lerp(Sim.Companions.WalkCycleMeters,Sim.Companions.RunCycleMeters,RunWeight);
            auto& Pose=CompanionPoseStates.FindOrAdd(D.Id);
            if(!Pose.Valid||Travel<Pose.Travel||Time<Pose.Time){Pose.Phase=0;Pose.Travel=Travel;Pose.Time=Time;Pose.Valid=true;}
            const double DeltaTravel=FMath::Max(0.,Travel-Pose.Travel);
            // Phase follows actual travel, so stops, acceleration and playback
            // speed do not turn into sliding feet or a sped-up walking clip.
            Pose.Phase=FMath::Fmod(Pose.Phase+DeltaTravel/Stride+(SpeedMeters<.01?FMath::Max(0.,Time-Pose.Time)/FMath::Max(.01,CompanionIdle?CompanionIdle->GetPlayLength():3.):0.),1.);
            Pose.Travel=Travel;Pose.Time=Time;
            UAnimationAsset* Animation=CompanionLocomotion.Get();
            if(!Animation)Animation=SpeedMeters>.01?(RunWeight>.5&&CompanionRun?CompanionRun.Get():CompanionWalk.Get()):CompanionIdle.Get();
            if(Animation)
            {
                if(!Mesh->GetSingleNodeInstance()||Mesh->GetSingleNodeInstance()->GetAnimationAsset()!=Animation)Mesh->PlayAnimation(Animation,true);
                auto* Instance=Mesh->GetSingleNodeInstance();
                if(CompanionLocomotion){Instance->SetBlendSpacePosition(FVector(SpeedMeters,0,0));Instance->SetPosition(Pose.Phase,false);}
                else if(auto* Sequence=Cast<UAnimSequence>(Animation))Instance->SetPosition(Pose.Phase*Sequence->GetPlayLength(),false);
                Mesh->SetPlayRate(0);
            }
        }
    }
}
