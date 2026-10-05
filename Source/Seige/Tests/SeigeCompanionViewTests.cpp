#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCompanionViewTest,"Seige.Companions.FirstPersonControls",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCompanionViewTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper World;if(!World.CreateTestWorld(EWorldType::Game)){World.ForwardErrorMessages(this);return false;}
    FURL Url;Url.AddOption(*FString::Printf(TEXT("game=%s"),*ASeigeGameMode::StaticClass()->GetPathName()));
    if(!World.GetTestWorld()->SetGameMode(Url))return false;
    auto* G=Cast<ASeigeGameMode>(World.GetTestWorld()->GetAuthGameMode());if(!G)return false;
    if(!G->Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),G->Error,false,false)){AddError(G->Error);return false;}
    G->Ready=true;G->Screen=TEXT("playing");G->Speed=10;G->CameraCenter=FVector(1400,800,0);G->Zoom=15000;G->CameraYaw=23;G->CameraPitch=61;
    const FVector CameraBefore=G->CameraCenter;const float ZoomBefore=G->Zoom;
    auto* DogActor=World.GetTestWorld()->SpawnActor<AActor>();
    TestNotNull(TEXT("Companion presentation actor exists"),DogActor);if(!DogActor)return false;
    G->Visuals.Add(TEXT("companion_1"),DogActor);
    TestFalse(TEXT("Companion starts visible in colony view"),DogActor->IsHidden());
    TestTrue(TEXT("Rex can be possessed in single player"),G->EnterCompanionView());
    TestTrue(TEXT("Entering first person hides the body immediately without a visual tick"),DogActor->IsHidden());
    TestEqual(TEXT("Possession sets 1x speed"),G->Speed,1.f);TestFalse(TEXT("Possession resumes world"),G->Paused);
    G->CycleGameSpeed(1);TestEqual(TEXT("Cannot accelerate while roaming"),G->Speed,1.f);
    G->CycleGameSpeed(-1);TestEqual(TEXT("Cannot reverse cycle out of 1x"),G->Speed,1.f);
    auto* Dog=G->Sim.Companions.Find(1);TestNotNull(TEXT("Rex exists"),Dog);if(!Dog)return false;
    const FTransform View=G->CameraTransform();
    TestTrue(TEXT("Camera lies at dog eye height"),View.GetLocation().Equals(G->RenderPosition(Dog->Position,G->Sim.Companions.EyeHeightCm),.001));
    G->LookCompanion(FVector2D(100,10000));TestTrue(TEXT("Look input changes heading"),G->CompanionYaw!=0);TestTrue(TEXT("Look pitch stays bounded"),FMath::Abs(G->CompanionPitch)<=75);
    G->ExitCompanionView();TestFalse(TEXT("Exit restores colony mode"),G->CompanionView);TestEqual(TEXT("Exit releases simulated dog"),G->Sim.Companions.ControlledId,0);
    TestFalse(TEXT("Exiting first person restores the body immediately without a visual tick"),DogActor->IsHidden());
    TestTrue(TEXT("Original colony focus restored"),G->CameraCenter.Equals(CameraBefore));TestEqual(TEXT("Original survey zoom restored"),G->Zoom,ZoomBefore);
    G->Observer=true;TestFalse(TEXT("Observer cannot take over AI dog"),G->EnterCompanionView());G->Observer=false;
    G->Screen=TEXT("landing");TestFalse(TEXT("Rex cannot roam before landing"),G->EnterCompanionView());G->Screen=TEXT("playing");
    Dog->Evacuated=true;TestFalse(TEXT("Evacuated dog cannot be possessed"),G->EnterCompanionView());return true;
}
#endif
