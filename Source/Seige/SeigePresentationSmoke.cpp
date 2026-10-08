#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HighResScreenshot.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Camera/PlayerCameraManager.h"
#include "InputKeyEventArgs.h"

// Explicit opt-in render/interaction smoke. It never loads or writes player saves.
void ASeigeGameMode::RunPresentationSmoke()
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("UiSmoke")))return;
    static const double Times[]={2,3,4,5,7,8,10,11,12,13,14.5,16,17,18,19,20,21,22,23,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,78,80,84,86,90,92,96,98,100,102,104,106,108,114,116,138,158,160,162,163,165,166,167,168,169,170,173,174,176,177,179,180,183,186,188,190,192,194,196,198,200,202,204,206,208,210,212,214,216,218,220,222,224,226,228,230,232,234,236,238,240,242,244,246,248,250,252,254,256,258,260,262};
    static double MenuTime=0,LastStageTime=0,WalkDistance=0,WalkSeconds=0;
    static TMap<FString,FVector2D> LastSimulationPositions;
    static int32 TestRoadId=0;
    static FVector2D SmokeLanding,ExportPosition,RexBefore,SolarPosition,PortPosition,ServicePosition;
    static TArray<FString> AssertionFailures;
    static FString ExportResource,ImportResource;
    static int32 SolarId=0,TradePortId=0,ExtractorId=0;
    static double CreditsBefore=0,ImportBefore=0;
    static bool ExportOrdered=false,ImportOrdered=false;
    static bool PreparationCaptured=false;
    static bool ServiceGhostCaptured=false;
    static double ServiceGhostReadySince=-1;
    static int32 PreActionCapturedStage=INDEX_NONE;
    static bool CommandSelectionReframed=false;
    static int32 BeforeWallBuildings=0,BeforeFactoryJobs=0;static double BeforeWallAlloy=0,BeforePlanAlloy=0,BeforePlanEnergy=0;
    static TWeakObjectPtr<ASeigeGameMode> MotionOwner;
    static TMap<FString,FVector> LastCourierPositions;
    static double LastMotionTime=-1;
    static int32 MotionFramesBetweenTicks=0;
    if(MotionOwner.Get()!=this){MotionOwner=this;LastCourierPositions.Reset();LastMotionTime=-1;MotionFramesBetweenTicks=0;LastStageTime=WalkDistance=WalkSeconds=0;LastSimulationPositions.Reset();TestRoadId=SolarId=TradePortId=ExtractorId=0;SmokeLanding=ExportPosition=RexBefore=FVector2D::ZeroVector;ExportResource.Empty();ImportResource.Empty();ExportOrdered=ImportOrdered=false;PreparationCaptured=ServiceGhostCaptured=false;ServiceGhostReadySince=-1;PreActionCapturedStage=INDEX_NONE;CommandSelectionReframed=false;AssertionFailures.Reset();}
    if(Screen==TEXT("playing")&&!Paused&&Speed==1&&!Observer)
    {
        bool MovedBetweenTicks=false;TMap<FString,FVector> Current;
        for(const auto& Courier:Sim.Workers.Bodies)if(Courier.State==TEXT("active")&&Courier.Outdoor)if(const auto* Actor=Visuals.FindRef(TEXT("home_worker_")+Courier.Id).Get())
        {
            const FVector Position=Actor->GetActorLocation();Current.Add(Courier.Id,Position);
            if(const auto* Previous=LastCourierPositions.Find(Courier.Id))
                MovedBetweenTicks|=Sim.Time==LastMotionTime&&!Position.Equals(*Previous,.001);
        }
        if(Sim.Time>LastMotionTime&&LastMotionTime>=0)
        {
            for(const auto& Courier:Sim.Workers.Bodies)if(const auto* Before=LastSimulationPositions.Find(Courier.Id))
            {
                const double Distance=FVector2D::Distance(*Before,Courier.Position);
                if(Distance>0&&Sim.Workers.RoadId(Sim,Courier)==0){WalkDistance+=Distance*Sim.MetersPerWorldUnit();WalkSeconds+=Sim.Time-LastMotionTime;}
            }
        }
        LastSimulationPositions.Reset();for(const auto& Courier:Sim.Workers.Bodies)LastSimulationPositions.Add(Courier.Id,Courier.Position);
        if(MovedBetweenTicks)++MotionFramesBetweenTicks;
        LastCourierPositions=MoveTemp(Current);LastMotionTime=Sim.Time;
    }
    else{LastCourierPositions.Reset();LastMotionTime=-1;}
    if(PresentationSmokeStage>=UE_ARRAY_COUNT(Times))return;
    // Test orchestration submits the same paid orders as the UI. It never changes
    // progress, inventories, workforce, elapsed simulation time or credit balances.
    auto PowerConnected=[&](int32 Id)
    {
        const auto* Target=Sim.FindBuilding(Id);if(!Target||Target->IsConstructing)return false;
        const auto* Core=Sim.Buildings.FindByPredicate([&](const auto&B){return B.DefId==Sim.CoreDefinition&&B.Health>0;});if(!Core)return false;
        if(Sim.IsRoadGridConnected(Core->Id,Id))return true;
        for(const auto& R:Sim.Roads)if(R.IsConstructing)return false;
        TArray<FVector2D> Starts{Sim.BuildingAccessPoint(*Core)};for(const auto& R:Sim.Roads)if(R.Health>0&&!R.Tier.IsEmpty()){Starts.AddUnique(R.A);Starts.AddUnique(R.B);}
        const FVector2D End=Sim.BuildingAccessPoint(*Target);
        Starts.Sort([&](const FVector2D&A,const FVector2D&B){return FVector2D::DistSquared(A,End)<FVector2D::DistSquared(B,End);});
        for(const auto& Start:Starts)
        {
            TArray<FVector2D> Route;if(!Sim.FindRoadRoute(Start,End,Route))continue;FVector2D Previous=Start;
            for(const auto& Point:Route)
            {
                auto On=[](FVector2D P,FVector2D A,FVector2D B){const auto D=B-A;const double T=FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),1.e-9);return T>=-.00001&&T<=1.00001&&FVector2D::Distance(P,A+D*T)<.01;};
                if(Sim.Roads.ContainsByPredicate([&](const auto&R){return R.Health>0&&!R.Tier.IsEmpty()&&On(Previous,R.A,R.B)&&On(Point,R.A,R.B);})){Previous=Point;continue;}
                FString Why;if(Sim.PlaceRoad(Previous,Point,Why))return false;
                if(Point.Equals(End,.01)&&!Previous.Equals(Point,.01)&&Sim.PlaceRoad(Previous,Point+(Point-Previous).GetSafeNormal()*400,Why))return false;
                break;
            }
        }
        return false;
    };
    const double Delay=PresentationSmokeStage==0?Times[0]:Times[PresentationSmokeStage]-Times[PresentationSmokeStage-1];
    if(RenderClock<LastStageTime+Delay)return;
    // These are real 10x simulation waits, never accelerated construction cheats.
    const bool WaitingForDeploymentWork=PresentationSmokeStage==57&&!Sim.Buildings.IsEmpty()&&Sim.Buildings[0].IsConstructing&&Sim.Buildings[0].ConstructionProgress<=0;
    const bool WaitingForCore=(PresentationSmokeStage==10||PresentationSmokeStage==58)&&!Sim.Buildings.IsEmpty()&&Sim.Buildings[0].IsConstructing;
    const int32 ExpandedSupport=Sim.Buildings.IsEmpty()?0:Sim.Definition(Sim.Buildings[0])->RobotSupportCapacity+Sim.BuildingDefs[TEXT("robot_service_bay")].RobotSupportCapacity;
    const bool WaitingForService=PresentationSmokeStage==61&&Sim.Buildings.Num()>=2&&(!PowerConnected(Sim.Buildings[1].Id)||Sim.RobotSupportCapacity<ExpandedSupport);
    const auto* TestRoad=Sim.FindRoad(TestRoadId);
    const bool WaitingForRoad=(PresentationSmokeStage==80||PresentationSmokeStage==84||PresentationSmokeStage==87)&&TestRoad&&TestRoad->IsConstructing;
    const bool WaitingForSolar=PresentationSmokeStage==95&&SolarId&&(!PowerConnected(SolarId)||Sim.Energy.Info(Sim,SolarId).GenerationKW<=0);
    const bool WaitingForPort=PresentationSmokeStage==98&&TradePortId&&(!PowerConnected(TradePortId)||!Sim.FindBuilding(TradePortId)||Sim.FindBuilding(TradePortId)->Workers<Sim.Definition(*Sim.FindBuilding(TradePortId))->Jobs);
    const bool WaitingForExtractor=PresentationSmokeStage==100&&ExtractorId&&(!PowerConnected(ExtractorId)||Sim.FindBuilding(ExtractorId)->Inventory.FindRef(ExportResource)<10);
    const bool WaitingForExport=PresentationSmokeStage==101&&ExportOrdered&&Sim.FindBuilding(TradePortId)&&(!Sim.FindBuilding(TradePortId)->Shipment.Resource.IsEmpty()||Sim.Credits<=CreditsBefore);
    const bool WaitingForImport=PresentationSmokeStage==103&&ImportOrdered&&Sim.FindBuilding(TradePortId)&&(!Sim.FindBuilding(TradePortId)->Shipment.Resource.IsEmpty()||Sim.TotalStock(ImportResource)<ImportBefore+2);
    if((WaitingForDeploymentWork||WaitingForCore||WaitingForService||WaitingForRoad||WaitingForSolar||WaitingForPort||WaitingForExtractor||WaitingForExport||WaitingForImport)&&RenderClock-LastStageTime<900&&!Sim.Failed&&!Sim.Escaped)return;
    const int32 Stage=PresentationSmokeStage;
    auto* Controller=Cast<ASeigeController>(UGameplayStatics::GetPlayerController(this,0));
    auto* Hud=Controller?Cast<ASeigeHUD>(Controller->GetHUD()):nullptr;
    auto Require=[&](bool Condition,const TCHAR* Message)
    {
        if(!Condition){++SmokeFailures;AssertionFailures.Add(FString::Printf(TEXT("Stage %d: %s; notice=%s; simulation=%s"),Stage,Message,*Notice,*Error));UE_LOG(LogTemp,Error,TEXT("UI_SMOKE_ASSERT stage=%d: %s"),Stage,Message);}
        return Condition;
    };
    auto Capture=[&](const TCHAR* Name)
    {
        const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Review-v09"));
        if(!Require(IFileManager::Get().MakeDirectory(*Directory,true),TEXT("Could not create screenshot directory")))return;
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory,FString(Name)+TEXT(".png")),true,false);
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_CAPTURE %s stage=%d screen=%s"),Name,Stage,*Screen);
    };
    if(Stage==20&&IsPreparingScenario())
    {
        if(!PreparationCaptured){Require(Screen==TEXT("preparing"),TEXT("Developed preparation must retain a responsive progress screen"));Capture(TEXT("scenario_preparing"));PreparationCaptured=true;}
        return;
    }
    if(Stage==59&&!ServiceGhostCaptured)
    {
        // Stage 58 changes zoom after this frame's scenery update. Retain that
        // transition separately, then review the identical camera and blueprint
        // after streaming and temporal history settle. Simulation keeps running.
        if(IsSceneryStreamingReady())
        {if(ServiceGhostReadySince<0)ServiceGhostReadySince=RenderClock;}
        else ServiceGhostReadySince=-1;
        const bool Settled=ServiceGhostReadySince>=0&&RenderClock-ServiceGhostReadySince>=2;
        if(!Settled&&RenderClock-LastStageTime<120)return;
        Require(Settled,TEXT("Service ghost scenery must become ready and settle for two seconds within 120 seconds"));
        Capture(TEXT("service_ghost"));ServiceGhostCaptured=true;
        // Keep this capture's ghost/UI state for the normal inter-stage delay;
        // placement and its camera change happen in a later rendered frame.
        LastStageTime=RenderClock;return;
    }
    if(Stage==88&&!CommandSelectionReframed)
    {
        // The road inspection camera can leave the core behind the resource
        // cards. Pan home and let the real HUD redraw before selecting it;
        // retain the ordinary world/UI hit-test guard below.
        CameraCenter=FVector(HomePosition(),0);Zoom=DefaultZoom;UpdateCamera();
        CommandSelectionReframed=true;LastStageTime=RenderClock;return;
    }
    const TCHAR* PreActionCapture=Stage==23?TEXT("closeup"):Stage==25?TEXT("rotated"):
        Stage==89?TEXT("command_controls"):Stage==90?TEXT("rex_selected"):nullptr;
    if(PreActionCapture&&PreActionCapturedStage!=Stage)
    {
        // Screenshots are written at frame end. Hold the current view before
        // clicking into a different dossier or moving into first-person mode.
        Capture(PreActionCapture);PreActionCapturedStage=Stage;
        LastStageTime=RenderClock;return;
    }
    ++PresentationSmokeStage;LastStageTime=RenderClock;
    auto ClickAction=[&](const FString& Action)
    {
        if(!Require(Controller&&Hud,TEXT("Player controller or HUD is missing")))return false;
        const auto* Region=Hud->Ui.HitRegions.FindByPredicate([&](const FSeigeButton& R){return R.Action==Action;});
        if(!Region){++SmokeFailures;AssertionFailures.Add(FString::Printf(TEXT("Stage %d: missing action %s on %s"),Stage,*Action,*Screen));UE_LOG(LogTemp,Error,TEXT("UI_SMOKE_ASSERT stage=%d: Missing cached action %s on %s"),Stage,*Action,*Screen);return false;}
        const FVector2D Point=(Region->Position+Region->Size*.5)*Hud->Ui.Scale;
        Controller->SetMouseLocation(FMath::RoundToInt(Point.X),FMath::RoundToInt(Point.Y));
        Controller->HandlePrimaryClick(Point.X,Point.Y);
        return true;
    };
    auto WorldClick=[&](FVector2D Position)
    {
        if(!Require(Controller&&Hud,TEXT("Cannot route a world click without controller and HUD")))return;
        UpdateCamera();if(Controller->PlayerCameraManager)Controller->PlayerCameraManager->UpdateCamera(0);
        FVector2D Pixel;
        if(!Require(Controller->ProjectWorldLocationToScreen(RenderPosition(Position),Pixel),TEXT("Ground target could not project to screen")))return;
        const bool Hit=Controller->UpdateCursorFromScreen(Pixel.X,Pixel.Y);
        FVector RayOrigin=FVector::ZeroVector,RayDirection=FVector::ZeroVector;Controller->ScreenRay(Pixel,RayOrigin,RayDirection);
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_PICK stage=%d screen=%s target=%s pixel=%s hit=%d cursor=%s delta=%.8f origin=%s direction=%s zoom=%.2f yaw=%.2f pitch=%.2f ui=%s"),
            Stage,*Screen,*Position.ToString(),*Pixel.ToString(),Hit?1:0,*CursorWorld.ToString(),Hit?FVector2D::Distance(CursorWorld,Position):-1.,*RayOrigin.ToString(),*RayDirection.ToString(),Zoom,CameraYaw,CameraPitch,*Hud->Ui.HitTest(Pixel.X,Pixel.Y));
        if(!Require(Hit,TEXT("Perspective screen ray did not hit terrain")))return;
        if(!Require(FVector2D::Distance(CursorWorld,Position)<.2,TEXT("Perspective terrain pick did not roundtrip")))return;
        if(!Require(Hud->Ui.HitTest(Pixel.X,Pixel.Y).IsEmpty(),TEXT("World placement target is covered by cached UI; settle UI before clicking")))return;
        Controller->SetMouseLocation(FMath::RoundToInt(Pixel.X),FMath::RoundToInt(Pixel.Y));
        Controller->HandlePrimaryClick(Pixel.X,Pixel.Y);
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_CLICK stage=%d screen=%s cursor=%s buildings=%d selected=%d notice=%s"),Stage,*Screen,*CursorWorld.ToString(),Sim.Buildings.Num(),SelectedId,*Notice);
    };
    auto ChooseSite=[&](const FString& Definition,FVector2D Preferred,FVector2D& Result)
    {
        FString Why;
        if(Sim.CanPlaceBuilding(Definition,Preferred,Why)){Result=Preferred;return true;}
        for(double Radius=1200;Radius<=2100;Radius+=300)for(int32 I=0;I<24;++I)
        {
            const double A=I*UE_TWO_PI/24;const auto Candidate=HomePosition()+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius;
            if(FVector2D::Distance(Candidate,ExportPosition)<600)continue;
            if(Sim.CanPlaceBuilding(Definition,Candidate,Why)){Result=Candidate;return true;}
        }
        Error=Why;return false;
    };
    UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_STAGE %d t=%.2f screen=%s"),Stage,RenderClock,*Screen);
    switch(Stage)
    {
    case 0:
        Require(Screen==TEXT("main"),TEXT("Application did not open at the main menu"));Capture(TEXT("main"));break;
    case 1:
        ClickAction(TEXT("screen:scenario"));Require(Screen==TEXT("scenario"),TEXT("Single-player action did not open scenario setup"));break;
    case 2:
        Require(ScenarioBackgroundBugs&&ScenarioPeriodicAttacks,TEXT("New scenarios default both threat types on"));
        ClickAction(TEXT("scenario-threat:background"));ClickAction(TEXT("scenario-threat:periodic"));
        Require(!ScenarioBackgroundBugs&&!ScenarioPeriodicAttacks,TEXT("Independent scenario threat buttons must turn both types off"));
        Capture(TEXT("scenario"));break;
    case 3:
        ClickAction(TEXT("start-scenario"));Require(Screen==TEXT("landing")&&Ready,TEXT("Human scenario did not enter ready landing mode"));
        Require(!Sim.BackgroundBugsEnabled&&!Sim.PeriodicAttacksEnabled,TEXT("New colony must inherit disabled threats"));break;
    case 4:
        Require(Sim.Time==0,TEXT("Scenario time advanced before human landing"));Capture(TEXT("landing"));break;
    case 5:
        WorldClick(FVector2D::ZeroVector);Speed=10;Require(Screen==TEXT("playing")&&!Observer,TEXT("Core world click did not start human play"));break;
    case 6: Capture(TEXT("play"));break;
    case 7:
        if(Require(Hud!=nullptr,TEXT("Construction shortcut requires HUD")))Hud->HandleShortcut(EKeys::B);
        Require(Hud&&Hud->Ui.BuildOpen,TEXT("B did not open construction catalog"));break;
    case 8: Capture(TEXT("build"));break;
    case 9:
        if(Require(Hud!=nullptr,TEXT("Building shortcuts require HUD"))){Hud->HandleShortcut(EKeys::L);Hud->HandleShortcut(EKeys::S);}
        Require(!SelectedBuild.IsEmpty()&&Hud&&!Hud->Ui.BuildOpen,TEXT("Category/build shortcut chain did not select a blueprint"));break;
    case 10:
    {
        // v0.9 deploys the core physically over several seconds; placement needs
        // its live coverage, so let ordinary 10x time finish the deployment first.
        if(const auto* CoreDef=Sim.BuildingDefs.Find(Sim.CoreDefinition)){const double Deadline=Sim.Time+CoreDef->ConstructionSeconds*6;while(!Sim.Buildings.IsEmpty()&&Sim.Buildings[0].IsConstructing&&Sim.Time<Deadline&&!Sim.Failed)Sim.Tick(1.);}
        FVector2D Site;Require(ChooseSite(SelectedBuild,FVector2D(1100,0),Site),TEXT("Sensor world click needs a legal unoccupied plot"));
        WorldClick(Site);Require(Sim.Buildings.Num()==2,TEXT("World click did not construct the selected sensor"));
        if(Sim.Buildings.Num()==2)Require(Sim.Definition(Sim.Buildings.Last())&&Sim.Definition(Sim.Buildings.Last())->Role==TEXT("sensor"),TEXT("Shortcut constructed the wrong building role"));break;
    }
    case 11:
    {
        Require(Sim.Buildings.Num()==2,TEXT("Constructed building disappeared before capture"));
        SelectedBuild.Empty();CameraYaw=35;CameraPitch=25;Zoom=1700;UpdateCamera();
        if(Controller&&Controller->PlayerCameraManager)Controller->PlayerCameraManager->UpdateCamera(0);
        FVector BodyCenter=RenderPosition(HomePosition()),BodyExtent;const auto* CoreVisual=Visuals.FindRef(FString::Printf(TEXT("home_building_%d"),Sim.Buildings[0].Id)).Get();
        if(CoreVisual)CoreVisual->GetActorBounds(false,BodyCenter,BodyExtent);
        FVector2D RoofPixel;
        if(Require(CoreVisual&&Controller&&Controller->ProjectWorldLocationToScreen(BodyCenter,RoofPixel),TEXT("Could not project the actual parked command-shuttle body")))
        {
            // The landed hull stands on legs, so the bounds centre can fall in
            // the open space under the fuselage at a low tilt. Probe the hull
            // from its centre upwards and click the first pixel that answers.
            FVector RayOrigin,RayDirection;bool Answered=false;
            for(const double Lift:{0.,.35,.6,.8,-.25})
            {
                FVector2D Pixel;if(!Controller->ProjectWorldLocationToScreen(BodyCenter+FVector(0,0,BodyExtent.Z*Lift),Pixel))continue;
                SelectedId=0;if(Controller->ScreenRay(Pixel,RayOrigin,RayDirection)&&SelectBuildingRay(RayOrigin,RayDirection)){RoofPixel=Pixel;Answered=true;break;}
            }
            Require(Answered,TEXT("Visible building geometry did not answer a selection ray"));
            SelectedId=0;Controller->SetMouseLocation(FMath::RoundToInt(RoofPixel.X),FMath::RoundToInt(RoofPixel.Y));Controller->HandlePrimaryClick(RoofPixel.X,RoofPixel.Y);Require(SelectedId==Sim.Buildings[0].Id,TEXT("Clicking the parked command shuttle at low tilt did not select it"));
        }
        SelectedId=0;CameraYaw=135;CameraPitch=52;Zoom=DefaultZoom;UpdateCamera();Capture(TEXT("placed"));break;
    }
    case 12: Zoom=Sim.WorldHalfSize*12;UpdateCamera();break;
    case 13: Capture(TEXT("overview"));break;
    case 14: ShowScreen(TEXT("credits"));break;
    case 15:
        Require(Screen==TEXT("credits"),TEXT("Credits screen was not displayed"));Capture(TEXT("credits"));break;
    case 16: ReturnToMainMenu();break;
    case 17:
        ClickAction(TEXT("screen:scenario"));Require(Screen==TEXT("scenario"),TEXT("Scenario setup could not be reopened"));break;
    case 18:
        ClickAction(TEXT("scenario-threat:background"));ClickAction(TEXT("scenario-threat:periodic"));
        Require(ScenarioBackgroundBugs&&ScenarioPeriodicAttacks,TEXT("Threat buttons must also restore both types"));
        ClickAction(TEXT("slot:4"));ClickAction(TEXT("slot:4"));
        ClickAction(TEXT("slot:0"));ClickAction(TEXT("slot:0"));ClickAction(TEXT("slot:2"));
        Require(ScenarioSlots.Num()==9&&ScenarioSlots[4]==TEXT("developed")&&ScenarioSlots[0]==TEXT("developed")&&ScenarioSlots[2]==TEXT("starting"),TEXT("Scenario cell clicks did not configure the requested AI types"));break;
    case 19:
        PreparationCaptured=false;ClickAction(TEXT("start-scenario"));Require(IsPreparingScenario()&&Screen==TEXT("preparing"),TEXT("Developed AI must enter bounded scenario preparation"));break;
    case 20:
        Require(Ready&&Observer&&Screen==TEXT("playing")&&Neighbors.Num()==2,TEXT("Prepared AI scenario did not start observation with two neighbors"));Speed=10;Zoom=5500;UpdateCamera();break;
    case 21:
        Require(Observer&&Neighbors.Num()==2,TEXT("Observer scenario state changed unexpectedly"));Capture(TEXT("observer"));break;
    case 22: Zoom=1300;CameraYaw=35;CameraPitch=35;UpdateCamera();break;
    case 23: WorldClick(HomePosition());break;
    case 24: CameraYaw=220;CameraPitch=65;Zoom=3000;UpdateCamera();break;
    case 25: WorldClick(HomePosition());break;
    case 26: CameraYaw=135;CameraPitch=52;Zoom=DefaultZoom;UpdateCamera();break;
    case 27: WorldClick(HomePosition());Require(SelectedId==Sim.Buildings[0].Id,TEXT("Observer core must be inspectable"));break;
    case 28: ClickAction(TEXT("info-section:Weapons"));break;
    case 29: Capture(TEXT("weapons"));break;
    case 30: ClickAction(TEXT("info-section:Power"));break;
    case 31: Capture(TEXT("power"));break;
    case 32: ClickAction(TEXT("region-map"));Require(Zoom==MaximumZoom,TEXT("Region dock must begin zooming into cartographic view"));break;
    case 33: Capture(TEXT("regional_ai"));break;
    case 34: ClickAction(TEXT("focus-sector:0"));break;
    case 35:
        Require(DetailedSectorIndex()==0&&!IsRegionMap()&&ViewedSimulation()!=&Sim,TEXT("Map click did not focus the selected neighbor"));
        Capture(TEXT("neighbor"));break;
    case 36:
        if(Controller)
        {
            const FVector Focus=CameraCenter;const float Yaw=CameraYaw;
            Controller->BeginOrbitGesture(800,450);Controller->OrbitGestureDelta(120,10);Controller->EndOrbitGesture();UpdateCamera();
            Require(FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,CameraYaw)-120*OrbitYawPerPixel)<.01,TEXT("Captured middle drag did not apply full mouse sensitivity"));
            Require(CameraCenter.Equals(Focus),TEXT("Orbit drag moved the world focus"));
        }
        break;
    case 37: Capture(TEXT("neighbor_orbit"));break;
    case 38: FocusSector(4);break;
    case 39:
        for(const auto& B:Sim.Buildings)if(const auto* D=Sim.Definition(B))if(D->Role==TEXT("sensor"))
        {CameraCenter=FVector(B.Position,0);Zoom=DefaultZoom;WorldClick(B.Position);Require(SelectedId==B.Id,TEXT("Unarmed sensor could not be inspected"));break;}
        Require(SelectedId>0,TEXT("No unarmed sensor in observer scenario"));break;
    case 40: ClickAction(TEXT("info-section:Weapons"));break;
    case 41: Capture(TEXT("unarmed"));break;
    case 42: ClickAction(TEXT("info-section:Power"));break;
    case 43: Capture(TEXT("unarmed_power"));break;
    case 44:
        FocusSector(4);Paused=false;Speed=10;SelectedId=0;CameraCenter=FVector(0,-900,0);CameraYaw=35;CameraPitch=40;Zoom=2600;UpdateCamera();break;
    case 45: Capture(TEXT("terrain_overlook"));break;
    case 46: CameraCenter=FVector(1200,400,0);CameraYaw=150;CameraPitch=25;Zoom=900;UpdateCamera();break;
    case 47: Capture(TEXT("meadow_mid"));break;
    case 48: CameraCenter=FVector(1300,450,0);CameraYaw=35;CameraPitch=20;Zoom=180;UpdateCamera();break;
    case 49: Capture(TEXT("meadow_ground"));break;
    case 50: CameraCenter=FVector(3000,-700,0);CameraYaw=155;CameraPitch=48;Zoom=7000;UpdateCamera();break;
    case 51: Capture(TEXT("terrain_hills"));break;
    case 52: SelectedId=0;CameraCenter=FVector(29000,0,0);CameraYaw=0;CameraPitch=60;Zoom=14000;UpdateCamera();break;
    case 53: Capture(TEXT("boundary"));break;
    case 54:
    {
        ReturnToMainMenu();ScenarioSlots.Init(TEXT("empty"),9);ScenarioSlots[4]=TEXT("player");ScenarioBackgroundBugs=ScenarioPeriodicAttacks=false;CameraYaw=135;CameraPitch=50;StartScenario();
        bool Found=false;
        for(const auto& Node:Sim.Nodes)if(!Found&&Sim.Resources[Node.Resource].Class==TEXT("standard"))for(int32 I=0;I<16&&!Found;++I)
        {const double A=I*UE_TWO_PI/16;const FVector2D Candidate=Node.Position+FVector2D(FMath::Cos(A),FMath::Sin(A))*1100;FString Why;if(CanLand(Candidate,Why)){SmokeLanding=Candidate;ExportPosition=Node.Position;ExportResource=Node.Resource;Found=true;}}
        Require(Found,TEXT("A generated standard resource must allow a legal nearby landing"));CameraCenter=FVector(SmokeLanding,0);UpdateCamera();break;
    }
    case 55:
        if(Controller)Controller->SetMouseLocation(800,450);
        CursorOnWorld=true;CursorWorld=SmokeLanding;SyncVisuals();
        Require(Visuals.Contains(TEXT("placement_")+Sim.CoreDefinition),TEXT("Landing must show the final core footprint as a translucent mesh"));Capture(TEXT("landing_ghost"));break;
    case 56: WorldClick(SmokeLanding);Speed=10;Require(Screen==TEXT("playing")&&Sim.Buildings[0].IsConstructing,TEXT("Landing must begin shuttle deployment"));Zoom=1500;UpdateCamera();break;
    case 57:
        Require(Sim.DeploymentGrounded&&Sim.DeploymentHatchOpen&&Sim.Buildings[0].IsConstructing&&Sim.Buildings[0].ConstructionProgress>0&&Sim.Buildings[0].ConstructionProgress<1,TEXT("Grounded shuttle must open its hatch and receive real worker installation work"));
        Require(Sim.Buildings[0].BuildersOnSite>0&&Sim.Workers.Bodies.ContainsByPredicate([](const auto& W){return W.State==TEXT("active")&&W.Outdoor&&W.Activity==TEXT("build");}),TEXT("Deployment progress requires actual workers outside the shuttle"));Capture(TEXT("shuttle_deployment"));break;
    case 58:
        Require(!Sim.Buildings[0].IsConstructing,TEXT("Carried core must finish deployment"));
        Zoom=DefaultZoom;UpdateCamera();
        if(Hud){Hud->HandleShortcut(EKeys::B);Hud->HandleShortcut(EKeys::L);Hud->HandleShortcut(EKeys::C);}
        Require(SelectedBuild==TEXT("robot_service_bay"),TEXT("B L C must select worker service hub"));
        Require(ChooseSite(SelectedBuild,HomePosition()+FVector2D(1300,500),ServicePosition),TEXT("Worker service hub needs a legal unoccupied dry plot"));
        if(Controller)
        {
            if(Controller->PlayerCameraManager)Controller->PlayerCameraManager->UpdateCamera(0);
            FVector2D GhostPixel;
            if(Controller->ProjectWorldLocationToScreen(RenderPosition(ServicePosition),GhostPixel))Controller->SetMouseLocation(FMath::RoundToInt(GhostPixel.X),FMath::RoundToInt(GhostPixel.Y));
        }
        CursorOnWorld=true;CursorWorld=ServicePosition;SyncVisuals();
        Require(Visuals.Contains(TEXT("placement_robot_service_bay")),TEXT("Service blueprint must display a mesh ghost"));Capture(TEXT("service_ghost_transition"));break;
    case 59:
        WorldClick(ServicePosition);SelectedBuild.Empty();
        Require(Sim.Buildings.Num()==2&&Sim.Buildings.Last().IsConstructing,TEXT("Service building must start as a construction site"));
        if(Sim.Buildings.Num()==2)SelectedId=Sim.Buildings.Last().Id;
        CameraCenter=FVector(ServicePosition,0);Zoom=1800;UpdateCamera();break;
    case 60:
        Require(Sim.Buildings.Num()==2&&Sim.Buildings.Last().IsConstructing,TEXT("Workers must not create a service building instantly"));
        if(Sim.Buildings.Num()==2){SelectedId=Sim.Buildings.Last().Id;Require(Sim.RobotSupportCapacity==Sim.Definition(Sim.Buildings[0])->RobotSupportCapacity,TEXT("Unfinished service hub cannot supply charging capacity"));}
        ClickAction(TEXT("info-section:Overview"));Capture(TEXT("worker_construction"));break;
    case 61:
        Require(Sim.Buildings.Num()==2&&!Sim.Buildings.Last().IsConstructing,TEXT("Delivered materials and workers must finish the service hub"));
        Require(Sim.IsRoadGridConnected(Sim.Buildings[0].Id,Sim.Buildings.Last().Id)&&Sim.RobotSupportCapacity==ExpandedSupport,TEXT("Completed road-powered service hub must expand worker support capacity"));ClickAction(TEXT("info-section:Maintenance"));Capture(TEXT("service_complete"));break;
    case 62:
        if(Hud)Hud->HandleShortcut(EKeys::F10);MenuTime=Sim.Time;
        Require(MenuOpen&&Screen==TEXT("game-menu")&&Paused,TEXT("F10 must open the paused game menu directly"));break;
    case 63:
        Require(Sim.Time==MenuTime,TEXT("The local scenario must stop while its game menu is open"));Capture(TEXT("game_menu"));break;
    case 64: SetRenderResolutionPercent(100);ClickAction(TEXT("screen:settings"));Require(Screen==TEXT("settings")&&MenuOpen,TEXT("Game menu must expose settings"));break;
    case 65: Capture(TEXT("settings"));break;
    case 66:
        ClickAction(TEXT("render-scale:-10"));Require(RenderResolutionPercent==90,TEXT("Rendering resolution control must decrease independently of display mode"));
        ClickAction(TEXT("render-scale:10"));Require(RenderResolutionPercent==100,TEXT("Rendering resolution control must restore native rendering"));break;
    case 67: ClickAction(TEXT("back-screen"));Require(Screen==TEXT("game-menu"),TEXT("Settings Back must return to the in-game menu"));break;
    case 68:
        if(Hud)Hud->HandleShortcut(EKeys::F10);
        Require(!MenuOpen&&Screen==TEXT("playing")&&!Paused,TEXT("Closing the game menu must restore the earlier running state"));break;
    case 69:
        Speed=1;Paused=false;
        if(Hud)
        {
            Hud->HandleShortcut(EKeys::Add);Require(Speed==5&&!Paused,TEXT("Plus selects5x after1x"));
            Hud->HandleShortcut(EKeys::Add);Require(Speed==10&&!Paused,TEXT("Plus selects10x after5x"));
            Hud->HandleShortcut(EKeys::Add);Require(Paused,TEXT("Plus wraps10x to pause"));
            Hud->HandleShortcut(EKeys::Add);Require(Speed==1&&!Paused,TEXT("Plus resumes1x from pause"));
            Hud->HandleShortcut(EKeys::Subtract);Require(Paused,TEXT("Minus reaches pause from1x"));
            Hud->HandleShortcut(EKeys::Subtract);Require(Speed==10&&!Paused,TEXT("Minus wraps pause to10x"));
            Hud->HandleShortcut(EKeys::SpaceBar);Require(Paused&&Speed==10,TEXT("Space pauses without losing10x"));
            Hud->HandleShortcut(EKeys::SpaceBar);Require(!Paused&&Speed==10,TEXT("Space restores10x"));
        }
        break;
    case 70: SelectedId=0;CameraCenter=FVector::ZeroVector;CameraYaw=135;CameraPitch=52;Zoom=150000;UpdateCamera();break;
    case 71:
        Require(!IsRegionMap()&&RegionMapAlpha()==0,TEXT("Whole-sector survey must remain an unobscured 3D view"));Capture(TEXT("sector_survey"));break;
    case 72: Zoom=RegionMapZoom+RegionMapTransitionWidth*.25f;UpdateCamera();break;
    case 73:
        Require(RegionMapAlpha()>0&&RegionMapAlpha()<1,TEXT("The grid map must blend into the landscape"));Capture(TEXT("map_transition"));break;
    case 74: Zoom=MaximumZoom;UpdateCamera();break;
    case 75: Require(IsRegionMap()&&RegionMapAlpha()==1,TEXT("The outer zoom must reach the full map"));Capture(TEXT("full_map"));break;
    case 76:
    {
        FocusSector(4);CameraCenter=FVector(HomePosition()+FVector2D(-200,300),0);Zoom=3500;UpdateCamera();Paused=false;Speed=1;
        LastCourierPositions.Reset();LastSimulationPositions.Reset();LastMotionTime=-1;MotionFramesBetweenTicks=0;WalkDistance=WalkSeconds=0;
        // The only1x probe measures true walking and sub-tick presentation.
        // A local sensor site works on every generated resource subset.
        FVector2D Site;const bool Placed=ChooseSite(TEXT("sensor"),HomePosition()+FVector2D(-1100,700),Site)&&Sim.PlaceBuilding(TEXT("sensor"),Site,Error);
        Require(Placed,TEXT("Walking probe needs a real material-delivery job"));break;
    }
    case 77: Capture(TEXT("service_work"));break;
    case 78:
    {
        Require(MotionFramesBetweenTicks>10,TEXT("Visible1x worker movement must update between fixed simulation ticks"));
        Require(WalkSeconds>0&&FMath::IsNearlyEqual(WalkDistance/WalkSeconds*3.6,5.,.1),TEXT("Observed off-road cargo worker walking speed must be5km/h"));
        int32 ActiveBodies=0;TSet<FString> VisibleBodies;for(const auto& W:Sim.Workers.Bodies)if(W.State==TEXT("active")){++ActiveBodies;if(W.Outdoor){const FString Key=TEXT("home_worker_")+W.Id;VisibleBodies.Add(Key);const auto* Actor=Visuals.FindRef(Key).Get();Require(Actor&&!Actor->IsHidden(),TEXT("Each outdoor worker identity must have exactly its corresponding visible body"));}}
        Require(ActiveBodies==Sim.Population&&Sim.Couriers.Num()<=ActiveBodies,TEXT("Delivery tasks must share the real worker population"));
        for(const auto& VisualEntry:Visuals)if(IsValid(VisualEntry.Value.Get())&&!VisualEntry.Value->IsHidden())
        {const auto& Key=VisualEntry.Key;if(Key.StartsWith(TEXT("home_worker_"))&&!Key.EndsWith(TEXT("_cargo"))&&!Key.EndsWith(TEXT("_carrier"))&&!Key.EndsWith(TEXT("_tool")))Require(VisibleBodies.Contains(Key),TEXT("Worker rendering cannot invent an unaccounted body"));Require(!Key.StartsWith(TEXT("home_courier_")),TEXT("Legacy courier rendering cannot duplicate the worker body"));}
        Speed=10;Paused=false;SelectedId=0;CameraCenter=FVector(HomePosition()+FVector2D(1000,0),0);Zoom=2000;UpdateCamera();break;
    }
    case 79:
    {
        if(Hud){Hud->HandleShortcut(EKeys::B);Hud->HandleShortcut(EKeys::L);Hud->HandleShortcut(EKeys::R);}
        Require(RoadPlacementActive,TEXT("B L R must activate road construction"));
        const int32 Before=Sim.Roads.Num();const FVector2D Port=Sim.BuildingAccessPoint(Sim.Buildings[0]);FVector2D End=Port+FVector2D(300,0);
        for(int32 I=0;I<12;++I){const double A=(I%2?1.:-1.)*FMath::CeilToDouble(I/2.)*.12;const auto Candidate=Port+FVector2D(FMath::Cos(A),FMath::Sin(A))*300;FString Why;if(Sim.CanPlaceRoad(Port,Candidate,Why)){End=Candidate;break;}}
        WorldClick(Port);WorldClick(End);
        Require(Sim.Roads.Num()==Before+1&&Sim.Roads.Last().IsConstructing,TEXT("Two world clicks must create a material-delivery road worksite"));
        if(Sim.Roads.Num()>Before)TestRoadId=Sim.Roads.Last().Id;CancelRoadTool();Capture(TEXT("road_construction"));break;
    }
    case 80:
        Require(Sim.FindRoad(TestRoadId)&&!Sim.FindRoad(TestRoadId)->IsConstructing&&Sim.FindRoad(TestRoadId)->Tier==TEXT("road"),TEXT("Workers must complete the2x road"));break;
    case 81:
        if(const auto* Road=Sim.FindRoad(TestRoadId))WorldClick((Road->A+Road->B)*.5);
        Require(SelectedRoadId==TestRoadId&&SelectedId==0,TEXT("Clicking road surface must select the transport dossier"));Capture(TEXT("road_complete"));break;
    case 82:
        ClickAction(TEXT("build:upgrade_road"));
        Require(Sim.FindRoad(TestRoadId)&&Sim.FindRoad(TestRoadId)->IsConstructing&&Sim.FindRoad(TestRoadId)->Tier==TEXT("road"),TEXT("Rail upgrade must retain working road until completion"));break;
    case 83: Capture(TEXT("rail_construction"));break;
    case 84:
        Require(Sim.FindRoad(TestRoadId)&&!Sim.FindRoad(TestRoadId)->IsConstructing&&Sim.FindRoad(TestRoadId)->Tier==TEXT("road_rail"),TEXT("Rail upgrade must complete through workers and materials"));Capture(TEXT("rail_complete"));break;
    case 85: ClickAction(TEXT("build:upgrade_road"));break;
    case 86: Capture(TEXT("tube_construction"));break;
    case 87:
        Require(Sim.FindRoad(TestRoadId)&&!Sim.FindRoad(TestRoadId)->IsConstructing&&Sim.FindRoad(TestRoadId)->Tier==TEXT("road_rail_vacuum"),TEXT("Vacuum upgrade must complete through workers and materials"));Capture(TEXT("tube_complete"));break;
    case 88:
        WorldClick(HomePosition());Require(SelectedId==Sim.Buildings[0].Id,TEXT("The own core must expose colony commands"));break;
    case 89:
        Require(Hud&&!Hud->Ui.HitRegions.ContainsByPredicate([](const FSeigeButton& B){return B.Action==TEXT("colony-menu");}),TEXT("There must be no obsolete colony-menu button"));
        ClickAction(TEXT("focus-rex"));Require(SelectedCompanionId>0,TEXT("The selected core must expose Find Rex"));break;
    case 90:
        ClickAction(TEXT("roam-rex"));Require(CompanionView&&Speed==1&&!Paused,TEXT("Roam as Rex must enter first person at 1x"));break;
    case 91:
        if(const auto* Dog=Sim.Companions.Find(Sim.Companions.ControlledId))
        {
            RexBefore=Dog->Position;const auto Away=(Dog->Position-HomePosition()).GetSafeNormal();CompanionYaw=FMath::RadiansToDegrees(FMath::Atan2(Away.Y,Away.X));
            if(Controller)Controller->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Pressed,1,false,FPlatformTime::Cycles64()));
        }
        Require(CompanionView&&Speed==1,TEXT("Rex view must retain 1x playback"));Capture(TEXT("rex_first_person"));break;
    case 92:
        if(Controller)Controller->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::W,IE_Released,0,false,FPlatformTime::Cycles64()));
        if(const auto* Dog=Sim.Companions.Find(Sim.Companions.ControlledId))Require(FVector2D::Distance(RexBefore,Dog->Position)>1,TEXT("WASD must move the actual Rex simulation body"));else Require(false,TEXT("Controlled Rex disappeared"));
        Capture(TEXT("rex_walk"));break;
    case 93:
        if(Hud)Hud->HandleShortcut(EKeys::Escape);
        Require(!CompanionView&&Screen==TEXT("playing"),TEXT("Escape must restore the colony view"));Speed=10;Paused=false;SelectedCompanionId=0;
        Require(ChooseSite(TEXT("solar_array"),HomePosition()+FVector2D(1300,-650),SolarPosition),TEXT("Solar needs a legal paid construction site"));
        CameraCenter=FVector(SolarPosition,0);Zoom=DefaultZoom;UpdateCamera();
        if(Hud){Hud->HandleShortcut(EKeys::B);Hud->HandleShortcut(EKeys::L);Hud->HandleShortcut(EKeys::P);}
        Require(SelectedBuild==TEXT("solar_array"),TEXT("B L P must select solar generation"));break;
    case 94:
        WorldClick(SolarPosition);SelectedBuild.Empty();
        if(Sim.Buildings.Last().DefId==TEXT("solar_array"))SolarId=Sim.Buildings.Last().Id;
        Require(SolarId>0,TEXT("Solar generation must be placed through the UI"));break;
    case 95:
        Require(SolarId&&PowerConnected(SolarId)&&Sim.Energy.Info(Sim,SolarId).GenerationKW>0,TEXT("Solar generation must operate on the completed road grid"));SelectedId=SolarId;Capture(TEXT("solar_grid"));break;
    case 96:
        Require(ChooseSite(TEXT("trading_port"),HomePosition()+FVector2D(0,1400),PortPosition),TEXT("Trading port needs a legal paid construction site"));
        CameraCenter=FVector(PortPosition,0);Zoom=DefaultZoom;UpdateCamera();
        if(Hud){Hud->HandleShortcut(EKeys::B);Hud->HandleShortcut(EKeys::L);Hud->HandleShortcut(EKeys::T);}
        Require(SelectedBuild==TEXT("trading_port"),TEXT("B L T must select the external trading port"));break;
    case 97:
        WorldClick(PortPosition);SelectedBuild.Empty();
        if(Sim.Buildings.Last().DefId==TEXT("trading_port"))TradePortId=Sim.Buildings.Last().Id;
        Require(TradePortId>0,TEXT("Trading port must begin actual paid construction"));break;
    case 98:
        Require(TradePortId&&PowerConnected(TradePortId),TEXT("Completed trading port must share the road grid"));SelectedId=TradePortId;
        CameraCenter=FVector(Sim.FindBuilding(TradePortId)?Sim.FindBuilding(TradePortId)->Position:HomePosition(),0);Zoom=2400;UpdateCamera();Capture(TEXT("trading_port"));break;
    case 99:
        SelectedId=0;Require(Sim.PlaceBuilding(TEXT("extraction_mine"),ExportPosition,Error),TEXT("The Extraction Mine must bind to an actual generated deposit"));
        if(Sim.Buildings.Last().DefId==TEXT("extraction_mine"))ExtractorId=Sim.Buildings.Last().Id;
        Require(ExtractorId>0,TEXT("Local export source was not queued"));break;
    case 100:
        Require(ExtractorId&&PowerConnected(ExtractorId)&&Sim.FindBuilding(ExtractorId)->Inventory.FindRef(ExportResource)>=10,TEXT("Paid extractor must physically produce saleable goods in its own inventory"));
        CreditsBefore=Sim.Credits;Require(CreditsBefore==0,TEXT("No credits may be granted before the first external export"));
        ExportOrdered=Sim.TryTrade(TradePortId,ExportResource,10,false,Error);Require(ExportOrdered,TEXT("Actual local goods must queue an export"));SelectedId=TradePortId;Capture(TEXT("export_queued"));break;
    case 101:
        Require(Sim.Credits>CreditsBefore&&Sim.FindBuilding(TradePortId)&&Sim.FindBuilding(TradePortId)->Shipment.Resource.IsEmpty(),TEXT("Credits arrive only after the physical export shipment"));Capture(TEXT("export_paid"));break;
    case 102:
    {
        ImportResource.Empty();TArray<FString> Keys;Sim.Resources.GetKeys(Keys);Keys.Sort();
        for(const auto& Id:Keys)if(Sim.Resources[Id].Class==TEXT("standard")&&!Sim.Nodes.ContainsByPredicate([&](const auto&N){return N.Resource==Id;})){ImportResource=Id;break;}
        Require(!ImportResource.IsEmpty(),TEXT("Exactly one standard type must be absent from this region"));ImportBefore=Sim.TotalStock(ImportResource);
        ImportOrdered=Sim.TryTrade(TradePortId,ImportResource,2,true,Error);Require(ImportOrdered,TEXT("Export earnings must buy a physically missing standard input"));Capture(TEXT("import_queued"));break;
    }
    case 103:
        Require(Sim.FindBuilding(TradePortId)&&Sim.FindBuilding(TradePortId)->Shipment.Resource.IsEmpty()&&Sim.TotalStock(ImportResource)>=ImportBefore+2,TEXT("Paid import must arrive as physical goods"));Capture(TEXT("import_received"));break;
    case 104:
        SelectedCompanionId=0;CameraCenter=FVector(HomePosition(),0);Zoom=DefaultZoom;UpdateCamera();WorldClick(HomePosition());Require(SelectedId==Sim.Buildings[0].Id,TEXT("Core can be reselected after trade and Rex controls"));break;
    case 105:
        Paused=true;ClickAction(TEXT("command:reserve-more"));Require(Sim.WorkerSurplusTarget==1,TEXT("Command UI must change the inactive worker production target"));Capture(TEXT("worker_reserve"));break;
    case 106:
        ClickAction(TEXT("command:reserve-less"));Require(Sim.WorkerSurplusTarget==0,TEXT("Command target must return to zero"));
        CameraCenter=FVector(PortPosition,0);Zoom=DefaultZoom;UpdateCamera();WorldClick(PortPosition);Require(SelectedId==TradePortId,TEXT("Select own trading port for worker exports"));break;
    case 107:
        ClickAction(TEXT("trade:reserve-more"));Require(Sim.FindBuilding(TradePortId)&&Sim.FindBuilding(TradePortId)->WorkerExportTarget==1,TEXT("Trading port must have an independent worker export target"));Capture(TEXT("worker_export_target"));break;
    case 108:
        ClickAction(TEXT("trade:reserve-less"));Require(Sim.FindBuilding(TradePortId)&&Sim.FindBuilding(TradePortId)->WorkerExportTarget==0,TEXT("Trading target must return to zero"));
        SelectedId=0;CameraCenter=FVector(HomePosition()+FVector2D(1400,-1000),0);Zoom=DefaultZoom;UpdateCamera();
        BeforeWallBuildings=Sim.Buildings.Num();BeforeWallAlloy=Sim.TotalStock(TEXT("alloy"));
        if(Hud){Hud->HandleShortcut(EKeys::B);Hud->HandleShortcut(EKeys::L);Hud->HandleShortcut(EKeys::W);}
        Require(WallPlacementActive,TEXT("External wall tool must open without an ordinary building definition"));break;
    case 109:
        WorldClick(HomePosition()+FVector2D(1200,-1000));WorldClick(HomePosition()+FVector2D(1600,-1000));
        Require(WallJoints.Num()==2,TEXT("Two actual world clicks must establish a wall preview"));
        {const bool Inside=WallInsideLeft;if(Hud)Hud->HandleShortcut(EKeys::E);Require(WallInsideLeft!=Inside,TEXT("E must reverse the planned inside/outside"));}
        Capture(TEXT("wall_preview"));break;
    case 110:
        if(Hud)Hud->HandleShortcut(EKeys::Escape);
        Require(!WallPlacementActive&&!MenuOpen&&Sim.Buildings.Num()==BeforeWallBuildings&&Sim.TotalStock(TEXT("alloy"))==BeforeWallAlloy,TEXT("Canceling a wall preview must not build, spend materials or open the menu"));
        CameraCenter=FVector(HomePosition(),0);Zoom=DefaultZoom;UpdateCamera();WorldClick(HomePosition());Require(SelectedId==Sim.Buildings[0].Id,TEXT("Reselect own command center for universal chassis production"));break;
    case 111: ClickAction(TEXT("combat:open"));break;
    case 112: ClickAction(TEXT("combat:tab:factory"));break;
    case 113:
    {
        BeforeFactoryJobs=Sim.Combat.Fabrication.Num();BeforePlanAlloy=Sim.TotalStock(TEXT("alloy"));BeforePlanEnergy=Sim.Combat.EnergySpentKWh;
        ClickAction(TEXT("combat:hull-next"));ClickAction(TEXT("combat:weapon-next"));ClickAction(TEXT("combat:add"));ClickAction(TEXT("combat:request"));
        const auto* Plan=Sim.Combat.FabricationPlans.Find(SelectedId);
        Require(Plan&&Plan->Weapons.Num()==1,TEXT("Factory UI must preserve the selected chassis and one weapon module in its material request"));
        Require(Sim.Combat.Fabrication.Num()==BeforeFactoryJobs&&Sim.TotalStock(TEXT("alloy"))==BeforePlanAlloy&&Sim.Combat.EnergySpentKWh==BeforePlanEnergy,TEXT("Requesting materials must not bypass paid assembly or consume stock and energy"));Capture(TEXT("vehicle_material_plan"));break;
    }
    case 114: ClickAction(TEXT("combat:close"));break;
    case 115:
        ClickAction(TEXT("escape"));Require(Sim.Escaped,TEXT("The selected own core must launch its escape shuttle"));break;
    case 116:
    {
        Require(Ready&&!Observer&&Neighbors.Num()==0&&Screen==TEXT("playing"),TEXT("Final construction scenario state is invalid"));
        Require(MotionFramesBetweenTicks>10,TEXT("Visible 1x identified worker movement must update between fixed simulation ticks"));
        const float Dt=GetWorld()?GetWorld()->GetDeltaSeconds():0;
        auto Report=MakeShared<FJsonObject>();Report->SetBoolField(TEXT("ready"),Ready);Report->SetNumberField(TEXT("failures"),SmokeFailures);
        TArray<TSharedPtr<FJsonValue>> FailureValues;for(const auto& Message:AssertionFailures)FailureValues.Add(MakeShared<FJsonValueString>(Message));Report->SetArrayField(TEXT("assertion_failures"),FailureValues);
        Report->SetBoolField(TEXT("observer"),Observer);Report->SetNumberField(TEXT("neighbors"),Neighbors.Num());Report->SetNumberField(TEXT("buildings"),Sim.Buildings.Num());
        Report->SetNumberField(TEXT("fps"),Dt>0?1.0/Dt:0);Report->SetNumberField(TEXT("presentation_seconds"),RenderClock);Report->SetNumberField(TEXT("simulation_seconds"),Sim.Time);
        Report->SetStringField(TEXT("screen"),Screen);Report->SetNumberField(TEXT("completed_stages"),PresentationSmokeStage);
        Report->SetNumberField(TEXT("courier_motion_frames_between_ticks_at_1x"),MotionFramesBetweenTicks);
        Report->SetNumberField(TEXT("worker_motion_frames_between_ticks_at_1x"),MotionFramesBetweenTicks);
        Report->SetNumberField(TEXT("main_test_speed"),10);Report->SetNumberField(TEXT("walking_probe_speed"),1);
        Report->SetNumberField(TEXT("measured_worker_walking_kmh"),WalkSeconds>0?WalkDistance/WalkSeconds*3.6:0);
        Report->SetNumberField(TEXT("transport_segments"),Sim.Roads.Num());
        Report->SetNumberField(TEXT("resource_deposits"),Sim.Nodes.Num());Report->SetNumberField(TEXT("credits_after_paid_trade"),Sim.Credits);
        Report->SetStringField(TEXT("exported_resource"),ExportResource);Report->SetStringField(TEXT("imported_missing_standard"),ImportResource);
        FString Json;const FString Filename=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("PresentationSmoke.json"));
        if(!FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Json))||!FFileHelper::SaveStringToFile(Json,*Filename,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {++SmokeFailures;UE_LOG(LogTemp,Error,TEXT("UI_SMOKE_ASSERT could not save presentation report"));}
        UE_LOG(LogTemp,Display,TEXT("UI_SMOKE_COMPLETE failures=%d observer=%d neighbors=%d buildings=%d"),SmokeFailures,Observer?1:0,Neighbors.Num(),Sim.Buildings.Num());
        FPlatformMisc::RequestExitWithStatus(false,SmokeFailures==0?0:1);break;
    }
    default: break;
    }
    // Shipping logging is disabled. Keep bounded, explicit progress evidence
    // so a failed input step is observable before the long paid-economy route
    // finishes. This is diagnostic only and never changes simulation state.
    auto Progress=MakeShared<FJsonObject>();Progress->SetStringField(TEXT("scope"),TEXT("in_progress"));
    Progress->SetNumberField(TEXT("completed_stages"),PresentationSmokeStage);Progress->SetNumberField(TEXT("failures"),SmokeFailures);
    Progress->SetNumberField(TEXT("presentation_seconds"),RenderClock);Progress->SetStringField(TEXT("screen"),Screen);
    TArray<TSharedPtr<FJsonValue>> Messages;for(const auto& Message:AssertionFailures)Messages.Add(MakeShared<FJsonValueString>(Message));Progress->SetArrayField(TEXT("assertion_failures"),Messages);
    FString ProgressJson;if(FJsonSerializer::Serialize(Progress,TJsonWriterFactory<>::Create(&ProgressJson)))
        FFileHelper::SaveStringToFile(ProgressJson,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("PresentationSmokeProgress.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
