#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionViewTest,"Seige.Companions.ThirdPersonControls",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCompanionViewTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper World;if(!World.CreateTestWorld(EWorldType::Game)){World.ForwardErrorMessages(this);return false;}
    FURL Url;Url.AddOption(*FString::Printf(TEXT("game=%s"),*ASeigeGameMode::StaticClass()->GetPathName()));
    if(!World.GetTestWorld()->SetGameMode(Url))return false;
    auto* G=Cast<ASeigeGameMode>(World.GetTestWorld()->GetAuthGameMode());if(!G)return false;
    if(!G->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),G->Error,false,false)){AddError(G->Error);return false;}
    if(!G->LoadGraphicsSettings()){AddError(G->Error);return false;}
    G->Sim.Companions.Dogs[0].Aboard=false;G->Sim.Companions.Dogs[0].Position=G->Sim.BuildingAccessPoint(G->Sim.Buildings[0]);
    G->Ready=true;G->Screen=TEXT("playing");G->Speed=10;G->CameraCenter=FVector(1400,800,0);G->Zoom=15000;G->CameraYaw=23;G->CameraPitch=61;
    const FVector CameraBefore=G->CameraCenter;const float ZoomBefore=G->Zoom;
    TSet<FString> Live;G->SyncCompanionVisuals(Live);
    auto* DogActor=G->Visuals.FindRef(TEXT("companion_1")).Get();
    TestNotNull(TEXT("Companion presentation actor exists"),DogActor);if(!DogActor)return false;
    auto* DogMesh=DogActor->FindComponentByClass<USkeletalMeshComponent>();
    if(!TestNotNull(TEXT("Rex uses his actual skeletal mesh"),DogMesh))return false;
    TestFalse(TEXT("Companion starts visible in colony view"),DogActor->IsHidden());
    auto* Controller=World.GetTestWorld()->SpawnActor<ASeigeController>();
    if(!TestNotNull(TEXT("Rex button uses the real player controller"),Controller))return false;
    Controller->ClientSetHUD_Implementation(ASeigeHUD::StaticClass());
    auto* Hud=Cast<ASeigeHUD>(Controller->GetHUD());
    if(!TestNotNull(TEXT("Rex button uses the real owned HUD"),Hud))return false;
    Hud->SetCanvas(nullptr,nullptr);
    auto ClickRoam=[&]()
    {
        G->SelectedCompanionId=1;Hud->Ui.Scale=1;
        // Match the real panel/button ordering, then go through the controller's
        // between-draw input path rather than calling EnterCompanionView directly.
        Hud->Ui.HitRegions={{FVector2D(100,100),FVector2D(416,420),TEXT("panel"),TEXT("")},
            {FVector2D(120,458),FVector2D(376,42),TEXT("roam-rex"),TEXT("")}};
        Controller->HandlePrimaryClick(200,475);
    };
    ClickRoam();TestTrue(TEXT("Rex button enters third person in a player colony"),G->CompanionView);
    TestTrue(TEXT("Entering third person clears stale colony button hit regions"),Hud->Ui.HitRegions.IsEmpty());
    TestFalse(TEXT("Entering third person keeps Rex visible immediately"),DogActor->IsHidden());
    TestEqual(TEXT("Possession sets 1x speed"),G->Speed,1.f);TestFalse(TEXT("Possession resumes world"),G->Paused);
    G->CycleGameSpeed(1);TestEqual(TEXT("Cannot accelerate while roaming"),G->Speed,1.f);
    G->CycleGameSpeed(-1);TestEqual(TEXT("Cannot reverse cycle out of 1x"),G->Speed,1.f);
    auto* Dog=G->Sim.Companions.Find(1);TestNotNull(TEXT("Rex exists"),Dog);if(!Dog)return false;
    const FTransform View=G->CameraTransform();
    const FVector Target=G->RenderPosition(Dog->Position,G->CompanionCameraTargetHeight);
    TestTrue(TEXT("Follow camera stands behind and above Rex"),FVector::Distance(View.GetLocation(),Target)>150&&View.GetLocation().Z>Target.Z);
    TestTrue(TEXT("Follow camera centers the visible dog"),View.GetRotation().GetForwardVector().Equals((Target-View.GetLocation()).GetSafeNormal(),.001));
    auto* Blocker=World.GetTestWorld()->SpawnActor<AActor>();
    auto* Box=NewObject<UBoxComponent>(Blocker);Blocker->SetRootComponent(Box);Box->SetBoxExtent(FVector(25));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Box->RegisterComponent();Blocker->SetActorLocation(FMath::Lerp(Target,View.GetLocation(),.5));
    TestTrue(TEXT("A building between Rex and camera shortens the follow arm"),FVector::Distance(G->CameraTransform().GetLocation(),Target)<FVector::Distance(View.GetLocation(),Target)-25);
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);Blocker->Destroy();
    const FVector2D WalkStart=Dog->Position;
    G->CompanionYaw=G->CompanionSteeringYaw=(FVector(Dog->Position-Dog->Home,0)).Rotation().Yaw;
    G->MoveCompanion(1,0);G->Sim.Companions.Tick(G->Sim,.2);G->ResetSimulationPresentation();G->SyncCompanionVisuals(Live);
    TestTrue(TEXT("Walk input physically moves Rex"),Dog->Moving&&FVector2D::Distance(Dog->Position,WalkStart)>0);
    TestFalse(TEXT("Walking Rex remains visible after visual synchronization"),DogActor->IsHidden());
    TestNotNull(TEXT("Dedicated run gait is loaded"),G->CompanionRun.Get());
    TestNotNull(TEXT("Idle/walk/run blend asset is loaded"),G->CompanionLocomotion.Get());
    if(G->CompanionLocomotion)
    {
        TArray<FBlendSampleData> Samples;int32 Triangulation=INDEX_NONE;
        TestTrue(TEXT("Locomotion blend evaluates actual walk/run samples"),G->CompanionLocomotion->GetSamplesFromBlendInput(FVector(2.6,0,0),Samples,Triangulation,true)&&Samples.Num()>=2);
    }
    TestTrue(TEXT("Walking uses Rex's blended locomotion"),DogMesh->GetSingleNodeInstance()&&DogMesh->GetSingleNodeInstance()->GetAnimationAsset()==G->CompanionLocomotion.Get());
    const FTransform WalkingView=G->CameraTransform();const FVector WalkingTarget=G->RenderPosition(Dog->Position,G->CompanionCameraTargetHeight);
    TestTrue(TEXT("Camera follows Rex's new physical position"),WalkingView.GetRotation().GetForwardVector().Equals((WalkingTarget-WalkingView.GetLocation()).GetSafeNormal(),.001));
    G->MoveCompanion(0,0);G->Sim.Companions.Tick(G->Sim,.5);
    const FVector2D Standing=Dog->Position;const double HeadingBefore=Dog->Heading;
    const float YawBefore=G->CompanionYaw;G->LookCompanion(FVector2D(100,10000));G->Sim.Companions.Tick(G->Sim,.2);
    TestTrue(TEXT("Ordinary mouse input turns Rex while standing"),FMath::Abs(FMath::FindDeltaAngleDegrees(HeadingBefore,Dog->Heading))>1&&Dog->Position.Equals(Standing,.001));
    TestTrue(TEXT("Mouse steering also turns the follow camera"),G->CompanionYaw!=YawBefore);
    TestEqual(TEXT("Downward orbit stays bounded"),G->CompanionPitch,G->CompanionCameraMinPitch);
    G->LookCompanion(FVector2D(0,-10000));TestEqual(TEXT("Upward orbit stays bounded"),G->CompanionPitch,G->CompanionCameraMaxPitch);
    const FVector LowCamera=G->CameraTransform().GetLocation();
    TestTrue(TEXT("Upward orbit cannot put the camera underground"),LowCamera.Z>=G->GroundHeight(FVector2D(LowCamera)/G->RenderScale)*G->RenderScale+G->CompanionCameraClearance-.01);
    const float SteeringBefore=G->CompanionSteeringYaw;const double FacingBefore=Dog->Heading;
    G->LookCompanion(FVector2D(600,0),true);G->Sim.Companions.Tick(G->Sim,.2);
    TestEqual(TEXT("Alt mouse does not change steering direction"),G->CompanionSteeringYaw,SteeringBefore);
    TestTrue(TEXT("Alt mouse only orbits the camera, not Rex"),FMath::Abs(FMath::FindDeltaAngleDegrees(FacingBefore,Dog->Heading))<.001&&FMath::Abs(FMath::FindDeltaAngleDegrees(G->CompanionYaw,G->CompanionSteeringYaw))>60);
    const double BeforeReturn=FMath::Abs(FMath::FindDeltaAngleDegrees(G->CompanionYaw,G->CompanionSteeringYaw));
    G->LookCompanion(FVector2D::ZeroVector,false);G->UpdateCamera(.1f);
    TestTrue(TEXT("Releasing Alt smoothly returns camera without redirecting Rex"),FMath::Abs(FMath::FindDeltaAngleDegrees(G->CompanionYaw,G->CompanionSteeringYaw))<BeforeReturn&&G->CompanionSteeringYaw==SteeringBefore);
    G->MoveCompanion(1,0,true);for(int32 I=0;I<12;++I)G->Sim.Companions.Tick(G->Sim,.2);
    TestTrue(TEXT("Shift movement reaches a faster physical gait at 1x"),Dog->SpeedMetersPerSecond>G->Sim.Companions.WalkKmh/3.6&&G->Speed==1);
    G->ResetSimulationPresentation();G->SyncCompanionVisuals(Live);
    FVector BlendInput,Filtered;DogMesh->GetSingleNodeInstance()->GetBlendSpaceState(BlendInput,Filtered);
    TestTrue(TEXT("Running animation receives actual movement speed"),BlendInput.X>G->Sim.Companions.WalkKmh/3.6);
    // Evaluate the real skeletal pose: a valid asset reference alone would not
    // catch a frozen blend space or incorrect normalized-time sampling.
    DogMesh->GetSingleNodeInstance()->SetPosition(.1f,false);DogMesh->TickAnimation(.016f,false);DogMesh->RefreshBoneTransforms();
    const FVector PawA=DogMesh->GetBoneLocation(TEXT("fore_paw_L"),EBoneSpaces::ComponentSpace);
    DogMesh->GetSingleNodeInstance()->SetPosition(.6f,false);DogMesh->TickAnimation(.016f,false);DogMesh->RefreshBoneTransforms();
    const FVector PawB=DogMesh->GetBoneLocation(TEXT("fore_paw_L"),EBoneSpaces::ComponentSpace);
    TestTrue(TEXT("Distance-driven running actually animates Rex's paws"),FVector::Distance(PawA,PawB)>1);
    DogMesh->TickAnimation(.2f,false);DogMesh->RefreshBoneTransforms();
    TestTrue(TEXT("With unchanged simulation phase a paused paw does not animate"),DogMesh->GetBoneLocation(TEXT("fore_paw_L"),EBoneSpaces::ComponentSpace).Equals(PawB,.001));
    FSeigeRenderSnapshot TurnSnapshot;Dog->Heading=179;TurnSnapshot.Capture(G->Sim);Dog->Heading=-179;
    TestTrue(TEXT("Visible turns interpolate through the short angle at render rate"),FMath::Abs(FMath::FindDeltaAngleDegrees(TurnSnapshot.CompanionHeading(*Dog,.5),180.))<.001);
    G->ExitCompanionView();TestFalse(TEXT("Exit restores colony mode"),G->CompanionView);TestEqual(TEXT("Exit releases simulated dog"),G->Sim.Companions.ControlledId,0);
    TestFalse(TEXT("Exiting third person keeps Rex visible"),DogActor->IsHidden());
    TestTrue(TEXT("Original colony focus restored"),G->CameraCenter.Equals(CameraBefore));TestEqual(TEXT("Original survey zoom restored"),G->Zoom,ZoomBefore);
    G->CenterBrain=MakeShared<FSeigeScenarioAI>();
    if(!G->CenterBrain->Initialize(G->Sim,FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES")),false,G->Error,false,false,815))
    {AddError(G->Error);return false;}
    Dog=G->Sim.Companions.Find(1);if(!Dog)return false;
    Dog->Aboard=false;Dog->Position=G->Sim.BuildingAccessPoint(G->Sim.Buildings[0]);
    G->Observer=true;G->FocusCompanion(1);TestTrue(TEXT("Observer can focus Rex before roaming"),G->CameraCenter.Equals(FVector(Dog->Position,0))&&!G->CompanionView);
    const auto* CenterBrainBefore=G->CenterBrain.Get();G->Paused=true;G->Speed=20;
    G->WallPlacementActive=true;G->FleetOrderActive=true;
    ClickRoam();TestTrue(TEXT("The same Rex button works in an AI-center observation scenario"),G->CompanionView);
    TestEqual(TEXT("Observed Rex resumes at 1x"),G->Speed,1.f);TestFalse(TEXT("Observed Rex resumes from pause"),G->Paused);
    TestTrue(TEXT("Roaming cancels pending construction and fleet targeting"),!G->WallPlacementActive&&!G->FleetOrderActive);
    G->Tick(0);
    TestTrue(TEXT("Next game tick retains third-person observation"),G->CompanionView&&G->Sim.Companions.ControlledId==1);
    TestTrue(TEXT("Roaming preserves observer restrictions and the existing colony AI"),G->Observer&&G->CenterBrain.Get()==CenterBrainBefore);
    Hud->HandleShortcut(EKeys::SpaceBar);TestTrue(TEXT("Space pauses Rex's world"),G->Paused);
    Hud->HandleShortcut(EKeys::SpaceBar);TestFalse(TEXT("Space resumes Rex's world"),G->Paused);
    Hud->HandleShortcut(EKeys::Escape);TestFalse(TEXT("HUD Escape exits Rex mode"),G->CompanionView);
    Hud->HandleShortcut(EKeys::B);TestFalse(TEXT("Returning from Rex never grants observer building controls"),Hud->Ui.BuildOpen);
    G->Observer=false;
    Dog->Retreating=true;ClickRoam();TestFalse(TEXT("Stale Roam button cannot interrupt a safety retreat"),G->CompanionView);
    TestTrue(TEXT("Unavailable safety state gives readable feedback"),G->Notice.Contains(TEXT("shelter")));Dog->Retreating=false;
    Dog->Aboard=true;ClickRoam();TestFalse(TEXT("Rex cannot roam while aboard the shuttle"),G->CompanionView);Dog->Aboard=false;
    Dog->ExitingShuttle=true;ClickRoam();TestFalse(TEXT("Rex must finish leaving the shuttle before control"),G->CompanionView);Dog->ExitingShuttle=false;
    G->Screen=TEXT("landing");TestFalse(TEXT("Rex cannot roam before landing"),G->EnterCompanionView());G->Screen=TEXT("playing");
    Dog->Evacuated=true;TestFalse(TEXT("Evacuated dog cannot be possessed"),G->EnterCompanionView());return true;
}
#endif
