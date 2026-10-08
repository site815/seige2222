#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "Simulation/SeigeSimulation.h"
#include "Simulation/SeigeRenderInterpolation.h"
#include "SeigeGameMode.generated.h"

class FSeigeScenarioAI;
class UTexture2D;
struct FSeigeSceneryStreamState;
struct FSeigeScenarioPreparation;
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
struct FSeigeRegionResources
{
    int32 Index=0,Seed=0;
    TArray<FSeigeNode> Nodes;
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
    FSeigeWorldCalendar ScenarioCalendar;
    void ResetScenarioCalendar();
    void BindScenarioCalendar();
    FString CalendarLabel() const;
    bool LoadWeatherSettings();
    // Optional Graphics/building_visuals.json: building id -> mesh kind (SM_<Kind>, T_Building_<Kind>)
    // so families can get distinct art without editing the fingerprinted Rules.
    bool LoadBuildingVisuals();
    TMap<FString,FString> BuildingVisualOverrides;
    FString BuildingVisualKind(const FSeigeBuildingDef& Definition) const;
    void UpdateWeather(float DeltaSeconds=0);
    double SnowCoverage() const;
    float NightSkyFraction=.5f,NightExposureOffsetEV=2.f,SunriseSoftness=.12f,WinterAccumulationFraction=.08f,WinterMeltFraction=.12f,MaximumSnowCoverage=.92f;
    float SnowRadiusMeters=32,SnowHeightMeters=24,SnowFallMetersPerSecond=1.4f,SnowflakeSizeCentimeters=2.2f;
    float SunDirectionUpdateDegrees=.15f,SunShadowUpdateSeconds=.5f;
    double LastWeatherSunUpdate=-1;
    bool HasWeatherSunDirection=false;
    int32 SnowflakeCount=384;
    FString SnowCollectionPath,SnowflakeMaterialPath,AmbientCubemapPath;
    FString Error, Notice, SelectedBuild;
    int32 SelectedId=0;
    int32 SelectedCompanionId=0;
    bool CompanionView=false;
    float CompanionYaw=0,CompanionPitch=0;
    bool EnterCompanionView(int32 Id=1);
    void ExitCompanionView();
    void FocusCompanion(int32 Id=1);
    void MoveCompanion(float Forward,float Right);
    void LookCompanion(FVector2D Pixels);
    FVector2D CompanionRenderPosition() const;
    int32 SelectedRoadId=0;
    int32 SelectedFleetId=0;
    bool FleetOrderActive=false;
    bool WallPlacementActive=false,WallInsideLeft=true;
    TArray<FVector2D> WallJoints;
    int32 SelectedWallJoint=INDEX_NONE;
    void BeginWallPlacement();
    void CancelWallTool();
    void ClickWallPlan();
    bool WallShortcut(const FKey& Key);
    bool RoadPlacementActive=false,RoadUpgradeActive=false,RoadHasStart=false;
    FVector2D RoadStart=FVector2D::ZeroVector;
    bool Paused=false, Ready=false, WinAcknowledged=false;
    float Speed=1;
    FVector CameraCenter=FVector::ZeroVector;
    float Zoom=3500;
    float RenderScale=6,CameraYaw=135,CameraPitch=52,CameraFov=55;
    float NaniteMaxPixelsPerEdge=3.f;
    float NaniteSurveyPixelsPerEdge=3,NaniteSurveyStartZoom=7000,NaniteSurveyEndZoom=50000;
    float DefaultZoom=3500,MinimumZoom=120,MaximumZoom=360000;
    float CameraZoomResponse=9,RenderedZoom=-1;
    int32 DetailedTerrainResolution=1024;
    float RollingTerrainWavelength=3200,RollingTerrainAmplitude=360;
    float MicroTerrainWavelength=360,MicroTerrainAmplitude=18;
    float CorePadInnerRatio=1.15f,CorePadOuterRatio=1.8f;
    FVector2D RidgeCenter=FVector2D(1900,-1400);
    float RidgeAngleDegrees=35,RidgeWidth=1900,RidgeLength=6500,RidgeHeight=550;
    float RegionMapZoom=180000,RegionMapTransitionWidth=40000,OrbitYawPerPixel=.22f,OrbitPitchPerPixel=.18f;
    FString TerrainMaterialPath=TEXT("/Game/Art/NatureV04/M_TerrainV04.M_TerrainV04");
    int32 ForestCandidates=85000,NearForestCandidates=8500;
    int32 GroundCoverCandidates=350000;
    float GrassScaleMin=1.f,GrassScaleMax=1.3f;
    float GrassShadowDistanceMeters=100;
    float GrassProgrammableDistanceMeters=0,ForestProgrammableDistanceMeters=0;
    float GrassDetailDistanceMeters=45,GrassLodTransitionMeters=35,GrassStreamRadiusMeters=540,GrassStreamBudgetMs=2;
    int32 GrassStreamCellsPerFrame=32;
    float ForestDetailDistanceMeters=300,ForestLodTransitionMeters=120;
    FString GrassProxyAsset=TEXT("/Game/Art/NatureV07/SM_GrassProxy.SM_GrassProxy");
    FString BroadleafProxyAsset=TEXT("/Game/Art/NatureV07/SM_BroadleafProxy.SM_BroadleafProxy");
    FString ConiferProxyAsset=TEXT("/Game/Art/NatureV07/SM_ConiferProxy.SM_ConiferProxy");
    bool GrassDistanceFieldLighting=false;
    int32 NeighborForestCandidates=4500;
    bool NeighborForestShadows=false;
    float MinimumCameraPitch=8,MaximumCameraPitch=80,CameraGroundClearance=160;
    float SunIntensity=5.2f,SkyIntensity=1.3f,CloudShadowStrength=.6f;
    float SunSourceAngle=.6f,CloudShadowResolutionScale=1;
    float ExposureBias=.25f,ColorSaturation=1.1f,AmbientOcclusionIntensity=.5f,SunElevation=52;
    float FogDensity=0,FogStartDistanceMeters=1000,AtmosphereMieScale=.2f,AtmosphereAerialPerspectiveScale=.15f,BloomIntensity=.03f;
    // v0.9.1 presentation controls (Graphics/scene.json). Defaults reproduce the
    // v0.9 look when a key is absent from an older profile.
    float ColorContrast=1.f,VignetteIntensity=0,AmbientOcclusionRadiusCm=120,SunTemperatureKelvin=6500;
    float FogHeightFalloff=.15f,FogInscatteringLuminance=.5f,FogMaxOpacity=1.f,SkyLowerHemisphereLuminance=0;
    bool GrassFarProxy=true;
    bool SkyRealtimeCapture=false;
    TMap<FString,int32> MediumQualityGroups;
    TMap<FString,float> MediumRenderSettings;
    int32 NativeAntialiasing=2,UpscalingAntialiasing=4;
    FString CloudMaterialPath=TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst");
    TMap<FString,FString> NatureAssets;
    FVector2D CursorWorld=FVector2D::ZeroVector;
    bool CursorOnWorld=false;
    FString Screen=TEXT("main"),ReturnScreen=TEXT("main");
    TArray<FString> ScenarioSlots;
    bool ScenarioBackgroundBugs=true,ScenarioPeriodicAttacks=true;
    TArray<FSeigeNeighbor> Neighbors;
    TArray<FSeigeRegionResources> EmptyRegionResources;
    static bool GenerateEmptyRegionResources(const FSeigeSimulation& Center,const TArray<FString>& Slots,TArray<FSeigeRegionResources>& Output,FString& Error);
    const TArray<FSeigeNode>* RegionNodes(int32 Index) const;
    bool IsRegionResourceVisible(int32 Index,const FSeigeNode& Node) const;
    bool Observer=false,Fullscreen=true;
    int32 GraphicsQuality=1;
    bool MenuOpen=false,PauseBeforeMenu=false;
    FString MenuReturnScreen=TEXT("playing");
    float RenderResolutionPercent=100;
    FIntPoint WindowResolution=FIntPoint(1600,900);
    TArray<int32> GameSpeeds={1,5,10};
    void ShowScreen(const FString& NewScreen);
    void StartScenario();
    void BeginScenarioPreparation();
    void TickScenarioPreparation(double BudgetMilliseconds=6);
    void CancelScenarioPreparation();
    bool IsPreparingScenario() const;
    double ScenarioPreparationProgress() const;
    FString ScenarioPreparationStatus() const;
    void ConfirmLanding(FVector2D Position);
    void CycleScenarioSlot(int32 Index);
    void ToggleScenarioThreat(const FString& Threat);
    void ReturnToMainMenu();
    void SetGraphicsQuality(int32 Quality);
    void SetFullscreen(bool Enabled);
    void ToggleGameMenu();
    void ResumeGameMenu();
    void InitializeDisplaySettings();
    void SetRenderResolutionPercent(float Percent);
    void CycleWindowResolution(int32 Direction);
    FIntPoint EffectiveRenderResolution() const;
    FIntPoint DisplayResolution() const;
    void CycleGameSpeed(int32 Direction=1);
    bool IsSupportedGameSpeed(double Value) const;
    bool CanLand(FVector2D Position,FString& Reason) const;
    FVector2D HomePosition() const;
    void ClickWorld();
    void BeginRoadPlacement();
    void BeginRoadUpgrade();
    void CancelRoadTool();
    bool IsRoadToolActive() const {return RoadPlacementActive||RoadUpgradeActive;}
    FVector2D SnapRoadCursor(FVector2D Position) const;
    void ResetColony();
    void SaveGame();
    void LoadGame();
    void UpdateCamera(float DeltaSeconds=0);
    void ApplyMediumPreset();
    float CameraViewZoom() const;
    float RegionMapAlpha() const;
    double GroundHeight(FVector2D Position) const;
    FVector RenderPosition(FVector2D Position,float HeightOffset=0) const;
    bool TraceGroundRay(const FVector& Origin,const FVector& Direction,FVector& Hit) const;
    bool SelectBuildingRay(const FVector& Origin,const FVector& Direction);
    void RebuildTerrainHeights(bool ReuseUnchangedTiles=false);
    void RefreshTransportScenery();
    bool IsSceneryStreamingReady() const;
    int32 PendingSceneryCells() const;
    int32 PendingVisibleSceneryCells() const;
    int32 PendingNearSceneryCells() const;
    FTransform CameraTransform(float ZoomOverride=-1) const;
    FVector2D CameraPanDirection(float Forward,float Right) const;
    void ApplyOrbitDrag(FVector2D Pixels);
    bool IsRegionMap() const;
    bool IsWorldVisible(FVector2D Position) const;
    int32 DetailedSectorIndex() const;
    FVector2D DetailedSectorOffset() const;
    const FSeigeSimulation* ViewedSimulation() const;
    void FocusSector(int32 Index,bool SmoothTransition=false);
    float WoodlandDensity(FVector2D WorldLogical) const;
private:
#if WITH_DEV_AUTOMATION_TESTS
    friend class FSeigeIncrementalSectorSeamTest;
    friend class FSeigeTransportClearanceTest;
    friend class FSeigeCompanionViewTest;
    friend class FSeigeCommandShuttlePickTest;
#endif
    UPROPERTY() TObjectPtr<class UDirectionalLightComponent> WeatherSun;
    UPROPERTY() TObjectPtr<class USkyLightComponent> WeatherSky;
    UPROPERTY() TObjectPtr<class UTextureCube> WeatherAmbientCubemap;
    UPROPERTY() TObjectPtr<class UMaterialParameterCollection> WeatherCollection;
    UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Snowflakes;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    FString CompanionVisualAssetKey;
    UPROPERTY() TObjectPtr<class USkeletalMesh> CompanionMesh;
    UPROPERTY() TObjectPtr<class UAnimSequence> CompanionWalk;
    UPROPERTY() TObjectPtr<class UAnimSequence> CompanionIdle;
    FVector SavedColonyCamera=FVector::ZeroVector;
    float SavedColonyZoom=3500,SavedColonyYaw=135,SavedColonyPitch=52;
    UPROPERTY() TObjectPtr<AActor> Landscape;
    UPROPERTY() TObjectPtr<AActor> Foliage;
    UPROPERTY() TObjectPtr<AActor> GroundCover;
    UPROPERTY() TMap<FString,TObjectPtr<class UStaticMesh>> SceneryMeshReferences;
    UPROPERTY() TObjectPtr<class UMaterialInterface> BaseMaterial;
    UPROPERTY() TMap<FString, TObjectPtr<AActor>> Visuals;
    UPROPERTY() TMap<FString, TObjectPtr<class UMaterialInstanceDynamic>> Materials;
    double Accumulator=0;
    double RenderClock=0;
    bool ScreenshotRequested=false;
    bool GraphicsSettingsValid=true;
    int32 PresentationSmokeStage=0,SmokeFailures=0;
    void RunPresentationSmoke();
    void RunWorldReview();
    void RunDisplaySmoke();
    void RunGraphicsBenchmark(float DeltaSeconds);
    TSharedPtr<FSeigeScenarioAI> CenterBrain;
    FString DataDirectory(const TCHAR* Folder) const;
    bool InitializeScenario(FString& Reason);
    void FinishScenarioStart();
    TSharedPtr<FSeigeScenarioPreparation> ScenarioPreparation;
    UMaterialInterface* Material(FLinearColor Color);
    AActor* Visual(const FString& Key, const FString& Kind, FVector Location, FLinearColor Color, float Size);
    void Part(AActor* Actor,const FString& Shape,FVector Offset,FVector Scale,FLinearColor Color,FRotator Rotation=FRotator::ZeroRotator);
    void CreateLandscape(bool SectorTransition=false);
    void CreateEnvironmentWater();
    void RefreshEnvironment();
    void RefreshBuildingPads();
    FString TerrainPadSignature;
    FString RoadGhostSignature;
    FString BuildingPlotGhostSignature;
    TMap<FString,FVector4> TerrainPadBounds;
    void CreateFoliage(int32 PreviousSector=INDEX_NONE);
    void RefreshDepositGeology(bool Force=false);
    FString DepositVisibilitySignature;
    void CreateGroundCover();
    TSharedPtr<FSeigeSceneryStreamState> SceneryStream;
    int32 RenderedSector=-1;
    FVector2D FoliageCenter=FVector2D(1.e10,1.e10);
    double TerrainHeight(FVector2D Position) const;
    bool LoadGraphicsSettings();
    TArray<FSeigeTerrainTile> TerrainTiles;
    void ClearSceneryAt(FVector2D Position,float Radius);
    void SyncVisuals();
    void SyncCompanionVisuals(TSet<FString>& Live);
    void SyncWallVisuals(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live);
    void SyncWallGhost(TSet<FString>& Live);
    void SyncCombatVisuals(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live);
    void ConfigureCombatTerrain();
    void SyncRoadVisuals(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live);
    void SyncRoadPlacementGhost(TSet<FString>& Live);
    void SyncBuildingPlot(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D Position,const FString& Key,TSet<FString>& Live);
    UMaterialInterface* ConstructionMaterial(FLinearColor Color,bool Reveal=false);
    void SyncPlacementGhost(TSet<FString>& Live);
    void SyncConstructionVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live);
    void SetConstructionReveal(AActor* Actor,double Progress);
    void SyncServiceVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live);
    TMap<int32,FSeigeRenderSnapshot> PresentationSnapshots;
    void CaptureSimulationPresentation();
    void ResetSimulationPresentation();
    const FSeigeRenderSnapshot* PresentationSnapshot(const FSeigeSimulation& Colony) const;
    double PresentationAlpha() const;
    double RenderSimulationTime(const FSeigeSimulation& Colony) const;
    double RenderConstructionProgress(const FSeigeSimulation& Colony,const FSeigeBuilding& Building) const;
    void SyncWorkerVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live);
    void SyncWorkerAgents(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live);
    void SyncInventoryVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live);
    void SyncStockpile(const FSeigeResourceDef& Resource,double Amount,FVector2D Position,const FString& Key,TSet<FString>& Live);
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
    bool CompanionMouseCaptured=false;
    FVector2D OrbitCursor;
    TSet<FKey> ConsumedKeysUntilRelease;
};

struct FSeigeButton { FVector2D Position,Size; FString Action,Tooltip; };
struct FSeigeMenuEntry { FString Definition,Shortcut; };
struct FSeigeMenuGroup { FString Id,Name,Shortcut,Description; TArray<FSeigeMenuEntry> Entries; };
struct FSeigeCredit { FString Heading,Text; };
struct FSeigeSummaryResource { FString Resource,Label; };
struct FSeigeResourceGroup { FString Id,Label; TArray<FSeigeSummaryResource> Entries; };
struct FSeigeDepositLabelState { FVector2D Offset=FVector2D(14,-14); bool Initialized=false; };
struct FSeigeUiState
{
    float ViewportWidth=1600,ViewportHeight=900,Scale=1,ContentTop=178;
    bool BuildOpen=false,GroupFocused=false;
    FString Category,HoverPanel;
    TArray<FSeigeMenuGroup> Categories;
    TArray<FSeigeCredit> Credits;
    TArray<FSeigeResourceGroup> ResourceGroups;
    FString Title=TEXT("SEIGE"),Eyebrow=TEXT("FIRST LANDING"),Tagline=TEXT("A foothold in the wilderness.");
    TArray<int32> SpeedSteps={1,5,10};
    TArray<FSeigeButton> HitRegions;
    FString HitTest(float ScreenX,float ScreenY) const;
    void UpdateHoverPanel(float ScreenX,float ScreenY,bool Enabled);
    TArray<FBox2D> ResourceCardBounds(float LogicalWidth) const;
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
    bool ProgressionOpen=false;   // production-chain panel (P / dock button); review captures set it directly
private:
    float Scale=1;
    float DrawOpacity=1;
    float SmoothedFps=0;
    float NoticeVisibleSeconds=0;
    bool InterfaceLoaded=false,InterfaceAttempted=false;
    FString InterfaceError,Version,LastNotice;
    FString BuildingInfoSection;
    int32 BuildingInfoPage=0;
    TMap<FString,FSeigeDepositLabelState> DepositLabels;
    UPROPERTY(Transient) TMap<FString,TObjectPtr<UTexture2D>> PortraitTextures;
    FVector LabelCameraPosition=FVector(1.e10,1.e10,1.e10);
    FRotator LabelCameraRotation=FRotator::ZeroRotator;
    FVector2D LabelViewport=FVector2D::ZeroVector;
    float LabelStillSeconds=0;
    void DrawBuildingInfo(ASeigeGameMode& GameMode,float Width,float Height);
    void DrawTradeInfo(ASeigeGameMode& GameMode,float Width,float Height);
    void DrawCommandInfo(ASeigeGameMode& GameMode,float Width,float Height);
    bool HandleCommandAction(const FString& Action,ASeigeGameMode& GameMode);
    void DrawWorkforceControls(ASeigeGameMode& GameMode,float X,float Y,float Width);
    void DrawWallPlan(ASeigeGameMode& GameMode,float Width,float Height);
    void DrawCombatInfo(ASeigeGameMode& GameMode,float Width,float Height);
    bool HandleCombatAction(const FString& Action,ASeigeGameMode& GameMode);
    bool CombatPanelOpen=false;
    void DrawProgression(ASeigeGameMode& GameMode,float Width,float Height);
    FString CombatTab=TEXT("fleet"),ChosenChassis,ChosenWeapon,LoadoutContext;
    int32 OutfitVehicleId=0;
    bool OutfitBuilding=false;
    TArray<FString> DraftWeapons;
    bool HandleTradeAction(const FString& Action,ASeigeGameMode& GameMode);
    FString TradeResourceSelection;
    double TradeQuantity=10;
    void DrawRegionMap(ASeigeGameMode& GameMode,float Width,float Height);
    void Box(float X,float Y,float W,float H,FLinearColor Color);
    void Label(const FString& Text,float X,float Y,float Size,FLinearColor Color=FLinearColor::White);
    FVector2D MeasureLabel(const FString& Text,float Size) const;
    void Button(const FString& Text,const FString& Action,float X,float Y,float W,float H,bool Active=false,const FString& Tooltip=TEXT(""));
    void Region(const FString& Action,float X,float Y,float W,float H,const FString& Tooltip=TEXT(""));
    void Wrapped(const FString& Text,float X,float& Y,float Width,float Size,FLinearColor Color);
    TArray<FString> WrapLines(const FString& Text,float Width,float Size) const;
    float DrawNotice(ASeigeGameMode& GameMode,float Width,float Height);
    void Icon(const FString& Visual,float X,float Y,float Size,FLinearColor Color);
    bool Portrait(const FString& Name,float X,float Y,float Width,float Height,float Opacity=1);
    void Frame(float X,float Y,float W,float H);
    void Description(const FSeigeBuildingDef& Def,ASeigeGameMode& GameMode,float X,float Y,float Width);
    bool ExecuteAction(const FString& Action,ASeigeGameMode& GameMode);
    bool DrawFrontend(ASeigeGameMode& GameMode,float Width,float Height);
};
