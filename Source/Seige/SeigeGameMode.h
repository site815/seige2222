#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/SeigeSimulation.h"
#include "SeigeGameMode.generated.h"

UCLASS()
class SEIGE_API ASeigeGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASeigeGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    FSeigeSimulation Sim;
    FString Error, Notice, SelectedBuild;
    int32 SelectedId=0;
    bool Paused=false, Ready=false, WinAcknowledged=false;
    float Speed=1;
    FVector CameraCenter=FVector::ZeroVector;
    float Zoom=6500;
    FVector2D CursorWorld=FVector2D::ZeroVector;
    bool CursorOnWorld=false;
    void ClickWorld();
    void ResetColony();
    void SaveGame();
    void LoadGame();
    void UpdateCamera();
private:
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    UPROPERTY() TObjectPtr<AActor> Landscape;
    UPROPERTY() TObjectPtr<class UMaterialInterface> BaseMaterial;
    UPROPERTY() TMap<FString, TObjectPtr<AActor>> Visuals;
    UPROPERTY() TMap<FString, TObjectPtr<class UMaterialInstanceDynamic>> Materials;
    double Accumulator=0;
    double RenderClock=0;
    bool ScreenshotRequested=false;
    UMaterialInterface* Material(FLinearColor Color);
    AActor* Visual(const FString& Key, const FString& Kind, FVector Location, FLinearColor Color, float Size);
    void Part(AActor* Actor,const FString& Shape,FVector Offset,FVector Scale,FLinearColor Color,FRotator Rotation=FRotator::ZeroRotator);
    void CreateLandscape();
    void SyncVisuals();
};

UCLASS()
class SEIGE_API ASeigeController : public APlayerController
{
    GENERATED_BODY()
public:
    ASeigeController();
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
};

struct FSeigeButton { FVector2D Position,Size; FString Action; };
UCLASS()
class SEIGE_API ASeigeHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    bool Click(float X,float Y);
private:
    TArray<FSeigeButton> Buttons;
    float Scale=1;
    void Box(float X,float Y,float W,float H,FLinearColor Color);
    void Label(const FString& Text,float X,float Y,float Size,FLinearColor Color=FLinearColor::White);
    void Button(const FString& Text,const FString& Action,float X,float Y,float W,float H,bool Active=false);
};
