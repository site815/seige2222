#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/SeigeSimulation.h"
#include "SeigeGameMode.generated.h"

class FSeigeScenarioAI;
struct FSeigeNeighbor
{
    int32 Index=0;
    FVector2D Offset=FVector2D::ZeroVector;
    FString Type;
    FSeigeSimulation Sim;
    TSharedPtr<FSeigeScenarioAI> Brain;
};

struct FSeigeTerrainTile
{
    FVector2D Offset=FVector2D::ZeroVector;
    int32 Resolution=0;
    TArray<float> Heights;
};

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
    float Zoom=3500;
    float RenderScale=6,CameraYaw=135,CameraPitch=52,CameraFov=55;
    float DefaultZoom=3500,MinimumZoom=120;
    int32 DetailedTerrainResolution=1024;
    float RollingTerrainWavelength=3200,RollingTerrainAmplitude=360;
    float MicroTerrainWavelength=360,MicroTerrainAmplitude=18;
    float CorePadInnerRatio=1.15f,CorePadOuterRatio=1.8f;
    FVector2D RidgeCenter=FVector2D(1900,-1400);
    float RidgeAngleDegrees=35,RidgeWidth=1900,RidgeLength=6500,RidgeHeight=550;
    float RegionMapZoom=40000,OrbitYawPerPixel=.45f,OrbitPitchPerPixel=.35f;
    FString TerrainMaterialPath=TEXT("/Game/Art/NatureV04/M_TerrainV04.M_TerrainV04");
    int32 ForestCandidates=85000,NearForestCandidates=8500;
    int32 GroundCoverCandidates=350000;
    float GrassScaleMin=1.f,GrassScaleMax=1.3f;
    float MinimumCameraPitch=8,MaximumCameraPitch=80,CameraGroundClearance=160;
    float SunIntensity=5.2f,SkyIntensity=1.3f,CloudShadowStrength=.6f;
    FString CloudMaterialPath=TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst");
    TMap<FString,FString> NatureAssets;
    FVector2D CursorWorld=FVector2D::ZeroVector;
    bool CursorOnWorld=false;
    FString Screen=TEXT("main"),ReturnScreen=TEXT("main");
    TArray<FString> ScenarioSlots;
    TArray<FSeigeNeighbor> Neighbors;
    bool Observer=false,Fullscreen=false;
    int32 GraphicsQuality=2;
    void ShowScreen(const FString& NewScreen);
    void StartScenario();
    void ConfirmLanding(FVector2D Position);
    void CycleScenarioSlot(int32 Index);
    void ReturnToMainMenu();
    void SetGraphicsQuality(int32 Quality);
    void SetFullscreen(bool Enabled);
    bool CanLand(FVector2D Position,FString& Reason) const;
    FVector2D HomePosition() const;
    void ClickWorld();
    void ResetColony();
    void SaveGame();
    void LoadGame();
    void UpdateCamera();
    double GroundHeight(FVector2D Position) const;
    FVector RenderPosition(FVector2D Position,float HeightOffset=0) const;
    bool TraceGroundRay(const FVector& Origin,const FVector& Direction,FVector& Hit) const;
    bool SelectBuildingRay(const FVector& Origin,const FVector& Direction);
    void RebuildTerrainHeights();
    FTransform CameraTransform() const;
    FVector2D CameraPanDirection(float Forward,float Right) const;
    void ApplyOrbitDrag(FVector2D Pixels);
    bool IsRegionMap() const;
    int32 DetailedSectorIndex() const;
    FVector2D DetailedSectorOffset() const;
    const FSeigeSimulation* ViewedSimulation() const;
    void FocusSector(int32 Index);
    float WoodlandDensity(FVector2D WorldLogical) const;
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSeigeIncrementalSectorSeamTest;
#endif
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    UPROPERTY() TObjectPtr<AActor> Landscape;
    UPROPERTY() TObjectPtr<AActor> Foliage;
    UPROPERTY() TObjectPtr<AActor> GroundCover;
    UPROPERTY() TObjectPtr<class UMaterialInterface> BaseMaterial;
    UPROPERTY() TMap<FString, TObjectPtr<AActor>> Visuals;
    UPROPERTY() TMap<FString, TObjectPtr<class UMaterialInstanceDynamic>> Materials;
    double Accumulator=0;
    double RenderClock=0;
    bool ScreenshotRequested=false;
    bool GraphicsSettingsValid=true;
    int32 PresentationSmokeStage=0,SmokeFailures=0;
    void RunPresentationSmoke();
    TSharedPtr<FSeigeScenarioAI> CenterBrain;
    FString DataDirectory(const TCHAR* Folder) const;
    bool InitializeScenario(FString& Reason);
    UMaterialInterface* Material(FLinearColor Color);
    AActor* Visual(const FString& Key, const FString& Kind, FVector Location, FLinearColor Color, float Size);
    void Part(AActor* Actor,const FString& Shape,FVector Offset,FVector Scale,FLinearColor Color,FRotator Rotation=FRotator::ZeroRotator);
    void CreateLandscape();
    void RefreshEnvironment();
    void RefreshBuildingPads();
    FString TerrainPadSignature;
    TMap<FString,FVector4> TerrainPadBounds;
    void CreateFoliage();
    void CreateGroundCover();
    int32 RenderedSector=-1;
    FVector2D FoliageCenter=FVector2D(1.e10,1.e10);
    double TerrainHeight(FVector2D Position) const;
    bool LoadGraphicsSettings();
    TArray<FSeigeTerrainTile> TerrainTiles;
    void ClearSceneryAt(FVector2D Position,float Radius);
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
    void HandlePrimaryClick(float ScreenX,float ScreenY);
    bool ScreenRay(const FVector2D& ScreenPosition,FVector& WorldOrigin,FVector& WorldDirection) const;
    bool UpdateCursorFromScreen(float ScreenX,float ScreenY);
    void BeginOrbitGesture(float X,float Y);
    void OrbitGestureDelta(float X,float Y);
    void EndOrbitGesture();
private:
    bool OrbitActive=false;
    FVector2D OrbitCursor;
    TSet<FKey> ConsumedKeysUntilRelease;
};

struct FSeigeButton { FVector2D Position,Size; FString Action,Tooltip; };
struct FSeigeMenuEntry { FString Definition,Shortcut; };
struct FSeigeMenuGroup { FString Id,Name,Shortcut,Description; TArray<FSeigeMenuEntry> Entries; };
struct FSeigeCredit { FString Heading,Text; };
struct FSeigeSummaryResource { FString Resource,Label; };
struct FSeigeUiState
{
    float ViewportWidth=1600,ViewportHeight=900,Scale=1;
    bool BuildOpen=false,ColonyOpen=false,GroupFocused=false;
    FString Category,HoverPanel;
    TArray<FSeigeMenuGroup> Categories;
    TArray<FSeigeCredit> Credits;
    TArray<FSeigeSummaryResource> SummaryResources;
    TArray<FSeigeButton> HitRegions;
    FString HitTest(float ScreenX,float ScreenY) const;
    void CloseMenus();
};
UCLASS()
class SEIGE_API ASeigeHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    bool Click(float X,float Y);
    bool ProcessClick(float X,float Y,ASeigeGameMode& GameMode);
    bool HandleShortcut(const FKey& Key);
    bool ProcessShortcut(const FKey& Key,ASeigeGameMode& GameMode);
    bool BlocksCameraKeys() const;
    bool IsPointerOverUI() const;
    bool LoadInterface(const FString& Directory,FString& Error);
    FSeigeUiState Ui;
private:
    float Scale=1;
    float SmoothedFps=0;
    float NoticeVisibleSeconds=0;
    bool InterfaceLoaded=false,InterfaceAttempted=false;
    FString InterfaceError,Version,LastNotice;
    FString BuildingInfoSection;
    int32 BuildingInfoPage=0;
    void DrawBuildingInfo(ASeigeGameMode& GameMode,float Width,float Height);
    void DrawRegionMap(ASeigeGameMode& GameMode,float Width,float Height);
    void Box(float X,float Y,float W,float H,FLinearColor Color);
    void Label(const FString& Text,float X,float Y,float Size,FLinearColor Color=FLinearColor::White);
    void Button(const FString& Text,const FString& Action,float X,float Y,float W,float H,bool Active=false,const FString& Tooltip=TEXT(""));
    void Region(const FString& Action,float X,float Y,float W,float H,const FString& Tooltip=TEXT(""));
    void Wrapped(const FString& Text,float X,float& Y,float Width,float Size,FLinearColor Color);
    TArray<FString> WrapLines(const FString& Text,float Width,float Size) const;
    float DrawNotice(ASeigeGameMode& GameMode,float Width,float Height);
    void Icon(const FString& Visual,float X,float Y,float Size,FLinearColor Color);
    void Frame(float X,float Y,float W,float H);
    void Description(const FSeigeBuildingDef& Def,ASeigeGameMode& GameMode,float X,float Y,float Width);
    bool ExecuteAction(const FString& Action,ASeigeGameMode& GameMode);
    bool DrawFrontend(ASeigeGameMode& GameMode,float Width,float Height);
};
