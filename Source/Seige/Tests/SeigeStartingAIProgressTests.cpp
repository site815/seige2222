#include "AI/SeigeScenarioAI.h"
#include "Simulation/SeigeResourceGeneration.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "SeigeGameMode.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS
// Narrow fixture access: production scenario/terrain internals stay private.
struct FSeigeStartingAIHarness
{
    static bool LoadSettings(ASeigeGameMode& G){return G.LoadGraphicsSettings();}
    static bool Initialize(ASeigeGameMode& G,FString& Error){return G.InitializeScenario(Error);}
    static FSeigeScenarioAI* CenterAI(ASeigeGameMode& G){return G.CenterBrain.Get();}
    static void RefreshTerrain(ASeigeGameMode& G){G.SimulationTerrainHeight(G.HomePosition());}
    static void RebindTerrain(ASeigeGameMode& G){G.ConfigureCombatTerrain();}
    static double Height(ASeigeGameMode& G,FVector2D P){return G.SimulationTerrainHeight(P);}
};
namespace
{
const FSeigeBuilding* StartingCore(const FSeigeSimulation& S)
{
    return S.Buildings.FindByPredicate([&](const auto& B){return B.Health>0&&S.Definition(B)->Role==TEXT("core");});
}
TSharedRef<FJsonObject> AIProgressState(const FSeigeSimulation& S,const FSeigeScenarioAI& AI,ASeigeGameMode* Game)
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("time"),S.Time);O->SetStringField(TEXT("ai_status"),AI.GetStatus());
    O->SetNumberField(TEXT("seed"),S.GenerationSeed);O->SetNumberField(TEXT("population"),S.Population);O->SetNumberField(TEXT("jobs"),S.TotalJobs);
    O->SetNumberField(TEXT("inactive_workers"),S.InactiveWorkerCount());O->SetNumberField(TEXT("worker_reserve"),S.WorkerReserveTarget());O->SetNumberField(TEXT("worker_bodies"),S.Workers.Bodies.Num());
    O->SetNumberField(TEXT("delivered_units"),S.DeliveredUnits);O->SetNumberField(TEXT("credits"),S.Credits);O->SetBoolField(TEXT("failed"),S.Failed);
    TArray<TSharedPtr<FJsonValue>> Buildings,Roads,Workers;
    for(const auto& B:S.Buildings)
    {
        auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("id"),B.Id);J->SetStringField(TEXT("definition"),B.DefId);J->SetStringField(TEXT("status"),B.Status);J->SetNumberField(TEXT("workers"),B.Workers);J->SetNumberField(TEXT("assigned_crew"),S.Workers.AssignedCount(B.Id));
        J->SetBoolField(TEXT("constructing"),B.IsConstructing);J->SetNumberField(TEXT("progress"),B.ConstructionProgress);J->SetNumberField(TEXT("health"),B.Health);J->SetNumberField(TEXT("power"),S.Energy.Fraction(B.Id));
        Buildings.Add(MakeShared<FJsonValueObject>(J));
    }
    for(const auto& R:S.Roads)
    {auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("id"),R.Id);J->SetBoolField(TEXT("constructing"),R.IsConstructing);J->SetNumberField(TEXT("progress"),R.ConstructionProgress);Roads.Add(MakeShared<FJsonValueObject>(J));}
    for(const auto& W:S.Workers.Bodies)
    {
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("id"),W.Id);J->SetStringField(TEXT("activity"),W.Activity);J->SetStringField(TEXT("state"),W.State);J->SetNumberField(TEXT("building"),W.BuildingId);J->SetNumberField(TEXT("road"),W.RoadId);J->SetNumberField(TEXT("x"),W.Position.X);J->SetNumberField(TEXT("y"),W.Position.Y);J->SetNumberField(TEXT("waypoint"),W.NextWaypoint);J->SetNumberField(TEXT("battery_kwh"),W.BatteryKWh);J->SetNumberField(TEXT("crew_building"),W.CrewBuildingId);
        if(Game&&W.State==TEXT("active")&&W.Outdoor)
        {
            TArray<FVector2D> Route;const auto* Core=StartingCore(S);
            J->SetBoolField(TEXT("route_to_core"),Core&&S.FindRoute(W.Position,S.BuildingAccessPoint(*Core),Route));
            TArray<TSharedPtr<FJsonValue>> Directions;
            for(int32 Direction=0;Direction<8;++Direction)
            {
                const FVector2D Unit(FMath::Cos(Direction*UE_PI/4),FMath::Sin(Direction*UE_PI/4));
                auto Probe=MakeShared<FJsonObject>();Probe->SetNumberField(TEXT("angle_degrees"),Direction*45);
                Probe->SetBoolField(TEXT("move_five_meters"),S.Mobility.MovementAllowed(S,TEXT("drone"),W.Position,W.Position+Unit*5/S.MetersPerWorldUnit()));
                double Grade=0;double Previous=FSeigeStartingAIHarness::Height(*Game,W.Position+S.Environment.WorldOffset);
                for(int32 I=1;I<=100;++I)
                {
                    const double H=FSeigeStartingAIHarness::Height(*Game,W.Position+S.Environment.WorldOffset+Unit*(I*.05/S.MetersPerWorldUnit()));
                    Grade=FMath::Max(Grade,FMath::Abs(H-Previous)/(.05/S.MetersPerWorldUnit()));Previous=H;
                }
                Probe->SetNumberField(TEXT("maximum_grade_5cm_samples"),Grade);Directions.Add(MakeShared<FJsonValueObject>(Probe));
            }
            J->SetArrayField(TEXT("local_route_probes"),Directions);
        }
        Workers.Add(MakeShared<FJsonValueObject>(J));
    }
    auto Stocks=MakeShared<FJsonObject>(),Produced=MakeShared<FJsonObject>(),Available=MakeShared<FJsonObject>();
    for(const auto& P:S.Resources){Stocks->SetNumberField(P.Key,S.TotalStock(P.Key));Available->SetNumberField(P.Key,S.ConstructionAvailable(P.Key));}
    for(const auto& P:S.ProducedUnits)Produced->SetNumberField(P.Key,P.Value);
    O->SetArrayField(TEXT("buildings"),Buildings);O->SetArrayField(TEXT("roads"),Roads);O->SetArrayField(TEXT("workers"),Workers);O->SetObjectField(TEXT("stock"),Stocks);O->SetObjectField(TEXT("produced"),Produced);O->SetObjectField(TEXT("construction_available"),Available);
    return O;
}

bool RunStartingProgress(FAutomationTestBase& Test,int32 Seed,int32 Sector,double Hours,bool LiveTerrain,bool RequireIndustry,bool RequireDevelopment=false,const FString& Snapshot=FString())
{
    // Use the actual scenario initializer and its authoritative terrain binding.
    // An isolated simulation only has the rivers/cliffs shaped over a flat base,
    // and cannot reproduce grading, slope and road-access failures in the game.
    FTestWorldWrapper World;ASeigeGameMode* Game=nullptr;
    FSeigeSimulation Isolated;FSeigeScenarioAI IsolatedBrain;
    FSeigeSimulation* Simulation=&Isolated;FSeigeScenarioAI* Brain=&IsolatedBrain;FString Error;
    if(LiveTerrain)
    {
        if(!World.CreateTestWorld(EWorldType::Game)){World.ForwardErrorMessages(&Test);return false;}
        FURL Url;Url.AddOption(*FString::Printf(TEXT("game=%s"),*ASeigeGameMode::StaticClass()->GetPathName()));
        if(!World.GetTestWorld()->SetGameMode(Url)){Test.AddError(TEXT("Cannot create live-terrain AI scenario"));return false;}
        Game=Cast<ASeigeGameMode>(World.GetTestWorld()->GetAuthGameMode());
        if(!Game||!FSeigeStartingAIHarness::LoadSettings(*Game)){Test.AddError(Game?Game->Error:TEXT("Missing AI game fixture"));return false;}
        Game->ScenarioSeedOverride=Seed;Game->ScenarioBackgroundBugs=Game->ScenarioPeriodicAttacks=false;
        for(auto& Slot:Game->ScenarioSlots)Slot=TEXT("empty");
        Game->ScenarioSlots[4]=TEXT("player");Game->ScenarioSlots[Sector]=TEXT("starting");
        if(!FSeigeStartingAIHarness::Initialize(*Game,Error)){Test.AddError(Error);return false;}
        if(Sector==4){Simulation=&Game->Sim;Brain=FSeigeStartingAIHarness::CenterAI(*Game);}
        else
        {
            auto* Neighbor=Game->Neighbors.FindByPredicate([&](const auto& N){return N.Index==Sector;});
            if(!Neighbor){Test.AddError(TEXT("Starting neighbor was not initialized"));return false;}
            Simulation=&Neighbor->Sim;Brain=Neighbor->Brain.Get();
        }
    }
    else
    {
        FSeigeSimulation Settings;
        if(!Settings.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,Seed)){Test.AddError(Error);return false;}
        const FVector2D Offset=FVector2D(Sector%3-1,Sector/3-1)*Settings.WorldHalfSize*2;
        if(!Brain->Initialize(*Simulation,FPaths::ProjectDir()/TEXT("Rules"),FPaths::ProjectDir()/TEXT("AIFILES"),false,Error,false,false,SeigeSectorResourceSeed(Seed,Sector),Offset)){Test.AddError(Error);return false;}
    }
    auto& S=*Simulation;auto& AI=*Brain;
    if(!Snapshot.IsEmpty())
    {
        if(!S.Load(Snapshot,Error)){Test.AddError(TEXT("Cannot restore diagnostic snapshot: ")+Error);return false;}
        if(Game)FSeigeStartingAIHarness::RebindTerrain(*Game);
        Hours+=S.Time/3600;
    }
    const FString Root=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Diagnostics/StartingAI"),FString::Printf(TEXT("seed-%d-sector-%d%s%s"),Seed,Sector,LiveTerrain?TEXT("-terrain"):TEXT(""),Snapshot.IsEmpty()?TEXT(""):TEXT("-restored")));
    IFileManager::Get().MakeDirectory(*Root,true);
    if(Snapshot.IsEmpty())
    {
        Test.TestEqual(TEXT("Starting AI has exactly the finite landing core"),S.Buildings.Num(),1);
        Test.TestEqual(TEXT("Starting AI begins with zero credits"),S.Credits,0.);
    }
    Test.TestFalse(TEXT("Current scenarios have road construction disabled"),S.RoadsEnabled());
    const bool TradeRecovery=!Snapshot.IsEmpty()&&FParse::Param(FCommandLine::Get(),TEXT("StartingAITradeRecovery"));
    int32 RecoveryPort=0;FString RecoveryResource;double RecoveryQuantity=0,RecoveryQuote=0;bool TradeRecovered=false;
    if(TradeRecovery)
    {
        for(const auto& B:S.Buildings)if(B.Health>0&&!B.Shipment.Resource.IsEmpty()&&!B.Shipment.Buy&&!B.Shipment.Departed)
        {RecoveryPort=B.Id;RecoveryResource=B.Shipment.Resource;RecoveryQuantity=B.Shipment.Quantity;RecoveryQuote=B.Shipment.PriceCredits;break;}
        if(!RecoveryPort){Test.AddError(TEXT("Trade recovery diagnostic requires a real accepted export awaiting local cargo"));return false;}
        Test.AddInfo(FString::Printf(TEXT("TRADE_RECOVERY starts time=%.3f port=%d resource=%s quantity=%.9f ordinary_quote=%.9f"),S.Time,RecoveryPort,*RecoveryResource,RecoveryQuantity,RecoveryQuote));
    }
    const int32 InitialBodies=S.Workers.Bodies.Num();int32 PaidWorkerCompletions=0,CompletedTruckDeliveries=0;double TruckDeliveredUnits=0;bool UnpaidBody=false;
    const int32 InitialTransports=S.TransportVehicles.Bodies.Num(),InitialLiveTransports=S.TransportVehicles.LiveCount();int32 PaidTransportCompletions=0;bool UnpaidTransport=false;
    auto Developed=[&]()
    {
        // Opt-in restoration is a diagnostic of paid transport recovery only;
        // release acceptance always starts fresh with the full requirements.
        if(!Snapshot.IsEmpty())return TradeRecovery?TradeRecovered:S.TransportVehicles.LiveCount()>InitialLiveTransports&&PaidTransportCompletions>0;
        return S.ProducedUnits.FindRef(TEXT("components"))>=100&&S.ProducedUnits.FindRef(TEXT("circuits"))>=20&&
            S.ProducedUnits.FindRef(TEXT("conductors"))>=100&&S.ProducedUnits.FindRef(TEXT("substrates"))>=100&&
            S.Buildings.ContainsByPredicate([&](const auto& B){return B.Health>0&&!B.IsConstructing&&S.Definition(B)->Family==TEXT("vehicle_factory");})&&
            S.TransportVehicles.LiveCount()>=2&&PaidTransportCompletions>0&&CompletedTruckDeliveries>=10;
    };
    struct FTruckIntent{int32 Source,Target,Road;};TMap<int32,FTruckIntent> TruckIntents;
    TArray<TSharedPtr<FJsonValue>> Journal;double NextReport=1800;
    auto Report=[&]()
    {
        auto Milestone=AIProgressState(S,AI,Game);double TruckDistance=0;for(const auto& V:S.TransportVehicles.Bodies)TruckDistance+=V.DistanceMeters;
        Milestone->SetNumberField(TEXT("transport_distance_meters"),TruckDistance);Milestone->SetNumberField(TEXT("completed_truck_deliveries"),CompletedTruckDeliveries);Milestone->SetNumberField(TEXT("truck_delivered_units"),TruckDeliveredUnits);
        Milestone->SetNumberField(TEXT("live_transports"),S.TransportVehicles.LiveCount());Milestone->SetNumberField(TEXT("paid_transport_completions"),PaidTransportCompletions);
        Journal.Add(MakeShared<FJsonValueObject>(Milestone));auto Document=MakeShared<FJsonObject>();Document->SetArrayField(TEXT("milestones"),Journal);
        Document->SetBoolField(TEXT("authoritative_game_terrain"),LiveTerrain);Document->SetNumberField(TEXT("initial_bodies"),InitialBodies);Document->SetNumberField(TEXT("paid_worker_completions"),PaidWorkerCompletions);
        // External readers can briefly hold a Windows file handle while a
        // milestone is published. Retry only that I/O, with a tight bound;
        // a persistent write/save error still fails the acceptance gate.
        auto Write=[&](const FString& Name,TFunctionRef<bool()> Operation)
        {
            for(int32 Attempt=0;Attempt<5;++Attempt)
            {Error.Empty();if(Operation())return true;if(Attempt<4)FPlatformProcess::SleepNoStats(.01f);}
            Test.AddError(TEXT("Cannot write AI diagnostic ")+FPaths::Combine(Root,Name)+TEXT(": ")+(Error.IsEmpty()?TEXT("file serialization/write failed after five attempts"):Error));return false;
        };
        FString Text;
        if(!FJsonSerializer::Serialize(Document,TJsonWriterFactory<>::Create(&Text)))Test.AddError(TEXT("Cannot serialize AI progress journal"));
        else Write(TEXT("progress.json"),[&](){return FFileHelper::SaveStringToFile(Text,*FPaths::Combine(Root,TEXT("progress.json")));});
        Write(TEXT("latest.json"),[&](){return S.Save(FPaths::Combine(Root,TEXT("latest.json")),Error);});
        const FString StateFile=FString::Printf(TEXT("state-%06d.json"),FMath::RoundToInt(S.Time));
        Write(StateFile,[&](){return S.Save(FPaths::Combine(Root,StateFile),Error);});
        Test.AddInfo(FString::Printf(TEXT("STARTING_AI seed=%d sector=%d terrain=%d time=%.0f buildings=%d roads=%d bodies=%d delivered=%.1f components=%.1f truck_deliveries=%d truck_m=%.1f status=%s"),Seed,Sector,LiveTerrain,S.Time,S.Buildings.Num(),S.Roads.Num(),S.Workers.Bodies.Num(),S.DeliveredUnits,S.ProducedUnits.FindRef(TEXT("components")),CompletedTruckDeliveries,TruckDistance,*AI.GetStatus()));
    };
    Report();
    const double Step=S.FixedStepSeconds();
    while(S.Time+1.e-6<Hours*3600&&!S.Failed&&!S.Escaped)
    {
        // Match the runtime's topology refresh before each simulation step,
        // without rendering 72,000 frames per hour in this bounded diagnostic.
        if(Game)FSeigeStartingAIHarness::RefreshTerrain(*Game);
        const int32 BeforeBodies=S.Workers.Bodies.Num();int32 CommittedOutput=0;
        const int32 BeforeTransports=S.TransportVehicles.Bodies.Num();int32 CommittedTransportOutput=0;for(const auto& Job:S.TransportVehicles.Jobs)if(Job.Committed)++CommittedTransportOutput;
        for(const auto& B:S.Buildings)if(B.ProductionCommitted)
            if(const auto* Recipe=S.Recipes.Find(B.CommittedRecipe))CommittedOutput+=Recipe->WorkerOutput;
        TSet<int32> ActiveTruckCouriers;
        for(const auto& C:S.Couriers)if(C.TransportVehicleId)
        {
            ActiveTruckCouriers.Add(C.Id);
            if(!TruckIntents.Contains(C.Id))TruckIntents.Add(C.Id,{C.SourceId,C.TargetId,C.RoadTargetId});
        }
        for(auto It=TruckIntents.CreateIterator();It;++It)if(!ActiveTruckCouriers.Contains(It.Key()))It.RemoveCurrent();
        struct FLoadedTruck{int32 Courier,Vehicle,Target,Road;double Amount;FVector2D Endpoint;};TArray<FLoadedTruck,TInlineAllocator<4>> LoadedTrucks;
        for(const auto& C:S.Couriers)if(C.TransportVehicleId&&C.Amount>1.e-8&&C.Phase==TEXT("unloading")&&(C.TargetId!=C.SourceId||C.RoadTargetId))
        {
            // Recovery can retarget freight to a different source depot or
            // the core. Neither is proof of reaching the original worksite.
            const auto* Intent=TruckIntents.Find(C.Id);
            if(!Intent||Intent->Source!=C.SourceId||Intent->Target!=C.TargetId||Intent->Road!=C.RoadTargetId)continue;
            const auto* B=S.FindBuilding(C.TargetId);const auto* R=S.FindRoad(C.RoadTargetId);const auto* V=S.TransportVehicles.Find(C.TransportVehicleId);
            if(V&&!V->Route.IsEmpty()&&S.Workers.TransportHandlingReady(V->HandlerWorkerId,V->Id)&&((R&&R->Health>0)||(B&&B->Health>0)))
            {
                const auto Port=R?S.RoadAccessPoint(*R):S.BuildingAccessPoint(*B);
                if(FVector2D::Distance(V->Route.Last(),Port)*S.MetersPerWorldUnit()<=S.TransportVehicles.Definition().ServiceMeters+1.e-6)
                    LoadedTrucks.Add({C.Id,C.TransportVehicleId,C.TargetId,C.RoadTargetId,C.Amount,V->Route.Last()});
            }
        }
        const auto* PendingTrade=TradeRecovery?S.FindBuilding(RecoveryPort):nullptr;
        const bool OriginalTradeInFlight=PendingTrade&&PendingTrade->Shipment.Departed&&!PendingTrade->Shipment.Buy&&PendingTrade->Shipment.Resource==RecoveryResource&&PendingTrade->Shipment.Quantity==RecoveryQuantity&&PendingTrade->Shipment.PriceCredits==RecoveryQuote;
        const double Before=S.Time,DeliveredBefore=S.DeliveredUnits;const int32 LostBefore=S.LostCouriers;AI.Tick(S,FMath::Min(Step,Hours*3600-S.Time));
        if(OriginalTradeInFlight&&S.Events.ContainsByPredicate([&](const auto& E){return E.Time>Before&&E.Text==TEXT("Export paid: ")+S.Resources[RecoveryResource].Name;}))
        {TradeRecovered=true;Test.AddInfo(FString::Printf(TEXT("TRADE_RECOVERY original physical shipment paid time=%.3f quantity=%.9f credits=%.9f"),S.Time,RecoveryQuantity,RecoveryQuote));}
        if(S.Time<=Before){Test.AddError(TEXT("Starting simulation stopped advancing"));break;}
        const int32 Created=S.Workers.Bodies.Num()-BeforeBodies;
        if(Created>0){PaidWorkerCompletions+=Created;if(Created>CommittedOutput)UnpaidBody=true;}
        const int32 CreatedTransports=S.TransportVehicles.Bodies.Num()-BeforeTransports;
        if(CreatedTransports>0){PaidTransportCompletions+=CreatedTransports;if(CreatedTransports>CommittedTransportOutput)UnpaidTransport=true;}
        for(const auto& Load:LoadedTrucks)
        {
            const auto* Vehicle=S.TransportVehicles.Find(Load.Vehicle);const auto* B=S.FindBuilding(Load.Target);const auto* R=S.FindRoad(Load.Road);
            // An unpicked cancellation, destroyed target or cargo return is
            // not a successful shipment. Require real loaded goods, surviving
            // destination, physical arrival and the delivered-goods ledger.
            if(Vehicle&&Vehicle->Health>0&&!Vehicle->Evacuated&&Vehicle->Position.Equals(Load.Endpoint,1.e-6)&&((R&&R->Health>0)||(B&&B->Health>0))&&S.LostCouriers==LostBefore&&S.DeliveredUnits-DeliveredBefore+1.e-7>=Load.Amount&&!S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Load.Courier;}))
            {++CompletedTruckDeliveries;TruckDeliveredUnits+=Load.Amount;}
        }
        if(S.Time>=NextReport){Report();NextReport=S.Time+1800;}
        if(RequireDevelopment?Developed():RequireIndustry&&S.ProducedUnits.FindRef(TEXT("components"))>=10&&CompletedTruckDeliveries>0)break;
    }
    Report();
    Test.TestFalse(TEXT("Peaceful starting colony retains command"),S.Failed||S.Escaped);
    Test.TestTrue(TEXT("Starting AI develops without constructing any roads"),S.Roads.IsEmpty());
    Test.TestFalse(TEXT("Every new worker comes from an existing paid production batch"),UnpaidBody);
    Test.TestEqual(TEXT("All original and manufactured bodies remain individually accounted for"),S.Workers.Bodies.Num(),InitialBodies+PaidWorkerCompletions);
    Test.TestFalse(TEXT("Every new cargo vehicle comes from a paid fabrication batch"),UnpaidTransport);
    Test.TestEqual(TEXT("All original and assembled transport bodies remain accounted for"),S.TransportVehicles.Bodies.Num(),InitialTransports+PaidTransportCompletions);
    if(RequireDevelopment)Test.TestTrue(Snapshot.IsEmpty()?TEXT("Finite kit develops the specialist chain and builds a real additional cargo vehicle"):TradeRecovery?TEXT("Restored colony physically completes and is paid for its original accepted export"):TEXT("Restored colony physically fabricates a paid additional cargo vehicle"),Developed());
    TSet<FString> Identities;bool Unique=true;for(const auto& W:S.Workers.Bodies){Unique&=!Identities.Contains(W.Id);Identities.Add(W.Id);}
    Test.TestTrue(TEXT("Worker body identities are unique"),Unique);
    if(RequireIndustry)
    {
        Test.TestTrue(TEXT("Finite-kit AI physically constructs and operates component production before the time limit"),S.ProducedUnits.FindRef(TEXT("components"))>=10);
        Test.TestTrue(TEXT("Construction uses physical delivered materials"),S.DeliveredUnits>0);
        Test.TestTrue(TEXT("New permanent crew is manufactured from paid inputs"),PaidWorkerCompletions>0);
        double Distance=0;for(const auto& V:S.TransportVehicles.Bodies)Distance+=V.DistanceMeters;
        Test.TestTrue(TEXT("Real cargo vehicle travels between facilities"),Distance>100);
        Test.TestTrue(TEXT("A real truck completes a loaded delivery to another facility or construction site"),CompletedTruckDeliveries>0&&TruckDeliveredUnits>0);
    }
    if(RequireIndustry||RequireDevelopment)
    {
        FSeigeSimulation Reloaded;
        if(!Reloaded.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,S.GenerationSeed,S.Environment.WorldOffset)||!Reloaded.Load(FPaths::Combine(Root,TEXT("latest.json")),Error))Test.AddError(TEXT("Progress snapshot must pass inventory/body validation: ")+Error);
        else{Test.TestEqual(TEXT("Reload keeps exact worker bodies"),Reloaded.Workers.Bodies.Num(),S.Workers.Bodies.Num());Test.TestEqual(TEXT("Reload keeps exact delivered cargo"),Reloaded.DeliveredUnits,S.DeliveredUnits);}
    }
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeStartingAIProgressDiagnostic,"Diagnostics.StartingAIProgress",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeStartingAIProgressDiagnostic::RunTest(const FString& Parameters)
{
    double Hours=6;FParse::Value(FCommandLine::Get(),TEXT("StartingAIHours="),Hours);
    int32 Seed=815,Sector=4;FParse::Value(FCommandLine::Get(),TEXT("StartingAISeed="),Seed);FParse::Value(FCommandLine::Get(),TEXT("StartingAISector="),Sector);
    if(!FMath::IsFinite(Hours)||Hours<=0||Hours>72||Sector<0||Sector>8){AddError(TEXT("Expected bounded StartingAIHours (0,72], seed and sector0..8"));return false;}
    return RunStartingProgress(*this,Seed,Sector,Hours,FParse::Param(FCommandLine::Get(),TEXT("StartingAILiveTerrain")),false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeStartingAIIndustryTest,"Seige.AI.FiniteKitIndustryOnGameTerrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeStartingAIIndustryTest::RunTest(const FString&)
{
    // Center observer and two neighboring colonies use different deposit rolls
    // and real global slopes. No fixture assets, workers or stocks are granted.
    for(const auto& Case:{FIntPoint(815,4),FIntPoint(2222,0),FIntPoint(4242,8)})
        if(!RunStartingProgress(*this,Case.X,Case.Y,24,true,true))return false;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeStartingAIDevelopmentTest,"Seige.AI.FiniteKitSelfDevelopmentOnGameTerrain",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeStartingAIDevelopmentTest::RunTest(const FString&)
{
    return RunStartingProgress(*this,815,4,72,true,true,true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeStartingAIRestoreDiagnostic,"Diagnostics.StartingAIRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeStartingAIRestoreDiagnostic::RunTest(const FString&)
{
    FString Snapshot;double Hours=6;int32 Seed=815,Sector=4;
    FParse::Value(FCommandLine::Get(),TEXT("StartingAISnapshot="),Snapshot);
    FParse::Value(FCommandLine::Get(),TEXT("StartingAIHours="),Hours);
    FParse::Value(FCommandLine::Get(),TEXT("StartingAISeed="),Seed);
    FParse::Value(FCommandLine::Get(),TEXT("StartingAISector="),Sector);
    if(Snapshot.IsEmpty()||!FMath::IsFinite(Hours)||Hours<=0||Hours>24||Sector<0||Sector>8)
    {AddError(TEXT("Restoration requires explicit StartingAISnapshot, matching seed/sector and up to24 additional simulation hours"));return false;}
    return RunStartingProgress(*this,Seed,Sector,Hours,true,false,true,Snapshot);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIPermanentCrewReadinessTest,"Seige.AI.PermanentCrewKeepsExpansionFunded",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIPermanentCrewReadinessTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!AI.Initialize(S,FPaths::ProjectDir()/TEXT("Rules"),FPaths::ProjectDir()/TEXT("AIFILES"),false,Error,false,false,815)){AddError(Error);return false;}
    // This decision fixture represents a completed port lending its sole actual
    // operator to cargo handling. No new worker body is created for the fixture.
    const int32 Id=S.AddReviewBuilding(TEXT("trading_port"),S.Buildings[0].Position+FVector2D(2000,0));
    auto* Port=S.FindBuilding(Id);if(!Port){AddError(TEXT("No port in crew-readiness fixture"));return false;}
    Port->IsConstructing=false;Port->Workers=0;S.RobotSupportCapacity=100;
    auto& Worker=S.Workers.Bodies[0];Worker.CrewBuildingId=Id;Worker.State=TEXT("active");Worker.Activity=TEXT("handling");
    AI.Targets.Reset();FSeigeAIBuildTarget First;First.Definition=TEXT("trading_port");First.Count=1;AI.Targets.Add(First);
    FSeigeAIBuildTarget Next;Next.Definition=TEXT("extraction_mine");Next.Count=1;AI.Targets.Add(Next);
    const auto* Target=AI.NextConstructionTarget(S);
    TestTrue(TEXT("Port crew handling cargo retains funding for the next mine"),Target&&Target->Definition==TEXT("extraction_mine"));
    Worker.Activity=TEXT("charging");Target=AI.NextConstructionTarget(S);
    TestTrue(TEXT("Port crew charging retains the same expansion bill"),Target&&Target->Definition==TEXT("extraction_mine"));
    Worker.State=TEXT("destroyed");
    TestTrue(TEXT("A lost operator really does require crew replacement before expansion"),AI.NextConstructionTarget(S)==nullptr);
    Worker.State=TEXT("active");Port->IsConstructing=true;
    TestTrue(TEXT("Permanent ownership never skips unfinished construction"),AI.NextConstructionTarget(S)==nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCoreOperatingReserveTest,"Seige.AI.CoreCrewReserveDuringConstruction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCoreOperatingReserveTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)){AddError(Error);return false;}
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    // Deploy every real carried body and finish the shuttle normally first.
    for(int32 I=0;I<400&&StartingCore(S)->IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("The operating reserve must not deadlock initial shuttle deployment"),StartingCore(S)->IsConstructing))return false;
    const int32 InitialBodies=S.Workers.Bodies.Num();const auto Origin=StartingCore(S)->Position;
    for(const auto& Offset:{FVector2D(1500,0),FVector2D(0,1500),FVector2D(-1500,0)})
        if(!S.PlaceBuilding(TEXT("solar_array"),Origin+Offset,Error)){AddError(Error);return false;}
    double Unstaffed=0,LongestUnstaffed=0;bool SawConcurrentWork=false,SawCharging=false;
    // One retained operator is one fifth of the core's rated throughput:
    // allow the real 600-second batch at that rate plus travel/charging.
    for(int32 I=0;I<144000;++I)
    {
        S.Tick(.05);const auto* Core=StartingCore(S);
        Unstaffed=Core->Workers>0?0:Unstaffed+.05;LongestUnstaffed=FMath::Max(LongestUnstaffed,Unstaffed);
        bool Builder=false;for(const auto& W:S.Workers.Bodies){Builder|=W.CrewBuildingId==Core->Id&&(W.Activity==TEXT("build")||W.Activity==TEXT("to_build")||W.Activity==TEXT("delivery"));SawCharging|=W.Activity==TEXT("charging");}
        SawConcurrentWork|=Builder&&Core->Workers>0;
    }
    TestTrue(TEXT("Real core operators remain while other real crew build and haul"),SawConcurrentWork);
    TestTrue(TEXT("The reserve is exercised across real charging handoffs"),SawCharging);
    TestTrue(TEXT("Charging or travel cannot starve the core for an entire production interval"),LongestUnstaffed<180);
    TestTrue(TEXT("Core keeps manufacturing paid worker bodies during expansion"),S.Workers.Bodies.Num()>InitialBodies);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePowerlessCourierClaimsTest,"Seige.AI.PowerlessCourierPreservesCargoAndReleasesUnpickedClaim",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigePowerlessCourierClaimsTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)){AddError(Error);return false;}
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    for(int32 I=0;I<400&&StartingCore(S)->IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("Fixture uses an actually deployed command module"),StartingCore(S)->IsConstructing)||
       !TestTrue(TEXT("Deployment cargo has settled before the controlled fault"),S.Couriers.IsEmpty())||S.Workers.Bodies.Num()<2)return false;
    const int32 CoreId=StartingCore(S)->Id;auto* Core=S.FindBuilding(CoreId);Core->Enabled=false;
    if(!TestTrue(TEXT("Controlled loaded cargo exists in finite source stock"),Core->Inventory.FindRef(TEXT("alloy"))>=10))return false;
    const double TotalBefore=S.TotalStock(TEXT("alloy"));const int32 BodyCount=S.Workers.Bodies.Num();
    // Controlled fault fixture: two original bodies lose power. One only has
    // a source reservation; the other owns ten real kg removed from inventory.
    const int32 PickupId=100000,LoadedId=100001;
    FString PickupWorker,LoadedWorker;FVector2D PickupPosition,LoadedPosition;
    for(int32 I=0;I<2;++I)
    {
        auto& W=S.Workers.Bodies[I];W.BatteryKWh=0;W.Activity=TEXT("delivery");W.DeliveryId=I?LoadedId:PickupId;W.BuildingId=W.RoadId=W.ChargingBuildingId=W.TransportVehicleId=0;W.SuspendedActivity.Empty();
        FSeigeCourier C;C.Id=W.DeliveryId;C.SourceId=C.TargetId=CoreId;C.Resource=TEXT("alloy");C.WorkerId=W.Id;C.Position=W.Position;
        if(I){C.Amount=10;C.Phase=TEXT("carrying");Core->Inventory.FindOrAdd(TEXT("alloy"))-=10;LoadedWorker=W.Id;LoadedPosition=W.Position;}
        else{C.ReservedAmount=10;C.Phase=TEXT("pickup");PickupWorker=W.Id;PickupPosition=W.Position;}
        S.Couriers.Add(C);
    }
    const double SourceBefore=Core->Inventory.FindRef(TEXT("alloy"));S.Tick(.05);
    TestFalse(TEXT("A powerless unpicked promise no longer blocks source materials"),S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==PickupId;}));
    const auto* Loaded=S.Couriers.FindByPredicate([&](const auto& C){return C.Id==LoadedId;});
    if(TestNotNull(TEXT("Actual carried cargo remains with its original physical carrier"),Loaded))
    {TestEqual(TEXT("Loaded mass is unchanged"),Loaded->Amount,10.);TestEqual(TEXT("Carrier identity is unchanged"),Loaded->WorkerId,LoadedWorker);TestTrue(TEXT("Carried goods stay at their physical position"),Loaded->Position.Equals(LoadedPosition,1.e-8));}
    TestEqual(TEXT("Releasing a reservation neither refunds nor consumes unpicked stock"),S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),SourceBefore);
    TestEqual(TEXT("Total source plus carried cargo is conserved"),S.TotalStock(TEXT("alloy")),TotalBefore);
    TestEqual(TEXT("Fault recovery never creates worker bodies"),S.Workers.Bodies.Num(),BodyCount);
    TestTrue(TEXT("Unpicked carrier is not teleported home"),S.Workers.Find(PickupWorker)->Position.Equals(PickupPosition,1.e-8));
    TestEqual(TEXT("No battery power is invented"),S.Workers.Find(PickupWorker)->BatteryKWh,0.);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeChargingConstructionCapTest,"Seige.AI.ChargingBuildersKeepPhysicalCrewCaps",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeChargingConstructionCapTest::RunTest(const FString&)
{
    for(bool RoadCase:{false,true})
    {
        FSeigeSimulation S;FString Error;
        if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)){AddError(Error);return false;}
        S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
        for(int32 I=0;I<400&&StartingCore(S)->IsConstructing;++I)S.Tick(10);
        const auto Origin=StartingCore(S)->Position;int32 Target=0;
        if(RoadCase)
        {
            S.SetRoadsEnabledForTest(true);
            // Controlled long-running road fixture: retain the authored crew
            // cap and material bill, only extend work duration across charging.
            S.TransportTiers[TEXT("road")].ConstructionSecondsPer100Meters=12000;
            const auto Port=S.BuildingAccessPoint(*StartingCore(S));
            if(!S.PlaceRoad(Port,Port+FVector2D(1000,0),Error)){AddError(Error);return false;}Target=S.Roads.Last().Id;
        }
        else
        {
            if(!S.PlaceBuilding(TEXT("component_works"),Origin+FVector2D(1500,0),Error)){AddError(Error);return false;}Target=S.Buildings.Last().Id;
        }
        bool SawSuspended=false,SawResumed=false,Exceeded=false;TSet<FString> ChargedBuilders;
        for(int32 I=0;I<144000;++I)
        {
            S.Tick(.05);
            for(const auto& W:S.Workers.Bodies)if((RoadCase?W.RoadId:W.BuildingId)==Target)
            {
                if(W.ChargingBuildingId){SawSuspended=true;ChargedBuilders.Add(W.Id);}
                else if(ChargedBuilders.Contains(W.Id)&&(W.Activity==TEXT("build")||W.Activity==TEXT("road_build")))SawResumed=true;
            }
            Exceeded|=RoadCase?S.FindRoad(Target)->Builders>S.TransportTiers[S.FindRoad(Target)->TargetTier].ConstructionWorkers:S.FindBuilding(Target)->Builders>S.RequiredBuilders(*S.FindBuilding(Target));
        }
        TestTrue(RoadCase?TEXT("Road builders make real charging trips"):TEXT("Building builders make real charging trips"),SawSuspended);
        TestTrue(RoadCase?TEXT("Road builders physically resume after charging"):TEXT("Building builders physically resume after charging"),SawResumed);
        TestFalse(TEXT("Charging and returning can never oversubscribe the authored construction crew"),Exceeded);
        const FString Path=FPaths::ProjectSavedDir()/TEXT("Diagnostics/StartingAI")/(RoadCase?TEXT("road-crew-charge-save.json"):TEXT("building-crew-charge-save.json"));
        if(!S.Save(Path,Error)){AddError(Error);return false;}
        FSeigeSimulation Loaded;
        if(!Loaded.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)||!Loaded.Load(Path,Error)){AddError(TEXT("Charged construction crew must pass strict save validation: ")+Error);return false;}
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeServiceChargeDependencyTest,"Seige.AI.ServiceAttendantsKeepChargingDependencies",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeServiceChargeDependencyTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)){AddError(Error);return false;}
    S.SetRoadsEnabledForTest(true);
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    for(int32 I=0;I<400&&StartingCore(S)->IsConstructing;++I)S.Tick(10);
    const auto Origin=StartingCore(S)->Position;
    // Explicit staffing fixture using only original landing identities. Two
    // completed bays expose the dependency cycle without manufacturing bodies.
    const int32 A=S.AddReviewBuilding(TEXT("robot_service_bay"),Origin+FVector2D(1500,0));
    const int32 B=S.AddReviewBuilding(TEXT("robot_service_bay"),Origin+FVector2D(0,1500));
    const int32 Core=StartingCore(S)->Id;
    for(int32 Station:{A,B})
    {
        FSeigeTransportSegment Road;Road.Id=90000+Station;Road.A=S.BuildingAccessPoint(*S.FindBuilding(Core));Road.B=S.BuildingAccessPoint(*S.FindBuilding(Station));Road.Tier=Road.TargetTier=TEXT("road");Road.IsConstructing=false;Road.ConstructionProgress=1;S.Roads.Add(Road);
    }
    S.Energy.Invalidate();S.Energy.Tick(S,0);
    auto& Attendant=S.Workers.Bodies[0];const FString Id=Attendant.Id;
    Attendant.CrewBuildingId=Attendant.BuildingId=A;Attendant.Activity=TEXT("operate");Attendant.Position=S.BuildingAccessPoint(*S.FindBuilding(B));Attendant.BatteryKWh=.03;
    auto& Second=S.Workers.Bodies[1];const FString SecondId=Second.Id;Second.CrewBuildingId=Second.BuildingId=B;Second.Activity=TEXT("operate");Second.Position=S.BuildingAccessPoint(*S.FindBuilding(B))+FVector2D(0,20);
    // Charging decisions use absolute saved-clock boundaries. Starting this
    // fixture between two boundaries must not require an immediate decision.
    FString WorkerText;TSharedPtr<FJsonObject> WorkerPolicy;
    if(!FFileHelper::LoadFileToString(WorkerText,*(FPaths::ProjectDir()/TEXT("Rules/workers.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(WorkerText),WorkerPolicy)||!WorkerPolicy.IsValid())
    {AddError(TEXT("Cannot read the authored worker planning cadence"));return false;}
    const double Cadence=WorkerPolicy->GetNumberField(TEXT("route_retry_seconds")),Step=S.FixedStepSeconds();
    for(int32 I=0;I<=FMath::CeilToInt(Cadence/Step)&&S.Workers.Find(Id)->ChargingBuildingId==0;++I)S.Tick(Step);
    TestEqual(TEXT("A low-battery attendant returns to its own self-service berth even if another bay is closer"),S.Workers.Find(Id)->ChargingBuildingId,A);
    // Remove only A's physical connection during its journey. The real body
    // must release that failed charge request at its present position.
    const auto InterruptedPosition=S.Workers.Find(Id)->Position;
    S.Roads.RemoveAll([&](const auto& R){return R.Id==90000+A;});S.Energy.Invalidate();S.Energy.Tick(S,0);S.Tick(.05);
    TestEqual(TEXT("An unpowered bay releases an existing charge request"),S.Workers.Find(Id)->ChargingBuildingId,0);
    TestTrue(TEXT("Power loss does not teleport the worker"),S.Workers.Find(Id)->Position.Equals(InterruptedPosition,1.e-8));
    S.Tick(.05);TestTrue(TEXT("A powerless station is not selected again"),S.Workers.Find(Id)->ChargingBuildingId!=A);
    FSeigeTransportSegment RestoredRoad;RestoredRoad.Id=90000+A;RestoredRoad.A=S.BuildingAccessPoint(*S.FindBuilding(Core));RestoredRoad.B=S.BuildingAccessPoint(*S.FindBuilding(A));RestoredRoad.Tier=RestoredRoad.TargetTier=TEXT("road");RestoredRoad.IsConstructing=false;RestoredRoad.ConstructionProgress=1;S.Roads.Add(RestoredRoad);S.Energy.Invalidate();S.Energy.Tick(S,0);
    TestTrue(TEXT("Restoring the real connection restores charging supply"),S.Energy.CanConsume(S,A,.0001));
    // A waiting physical guest now relies on A. Its attendant cannot be lent
    // to a maintenance shipment while that accepted charging visit exists.
    auto* Operator=S.Workers.Find(Id);Operator->ChargingBuildingId=0;Operator->SuspendedActivity.Empty();Operator->BatteryKWh=.2;Operator->Activity=TEXT("operate");Operator->Position=S.BuildingAccessPoint(*S.FindBuilding(A))+FVector2D(0,20);
    auto& Guest=S.Workers.Bodies[2];Guest.CrewBuildingId=Core;Guest.BuildingId=0;Guest.ChargingBuildingId=A;Guest.ChargeQueueSlot=1;Guest.Activity=TEXT("charge_queue");Guest.SuspendedActivity=TEXT("operate");Guest.Position=S.BuildingAccessPoint(*S.FindBuilding(A))+FVector2D(0,33.333333333);
    // The preceding integration ticks can dispatch a maintenance reservation.
    // Reset that unpicked fixture task before isolating the lending decision.
    for(const auto& C:S.Couriers)if(C.WorkerId==SecondId&&C.Amount>0){AddError(TEXT("Fixture reset must not discard actual carried cargo"));return false;}
    S.Couriers.RemoveAll([&](const auto& C){return C.WorkerId==SecondId;});
    auto* AvailableReceiver=S.Workers.Find(SecondId);AvailableReceiver->DeliveryId=AvailableReceiver->ChargingBuildingId=AvailableReceiver->TransportVehicleId=0;AvailableReceiver->SuspendedActivity.Empty();AvailableReceiver->Activity=TEXT("operate");AvailableReceiver->BuildingId=AvailableReceiver->CrewBuildingId=B;AvailableReceiver->BatteryKWh=.2;
    S.FindBuilding(A)->Inventory.FindOrAdd(TEXT("alloy"))=10;
    const bool Dispatched=S.Workers.Dispatch(S,A,B,0,TEXT("alloy"),1,false,false);
    TestTrue(TEXT("A real alternative receiver crew can collect the maintenance load"),Dispatched);
    TestEqual(TEXT("A relied-upon service attendant remains at its charging station"),S.Workers.Find(Id)->Activity,FString(TEXT("operate")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIBootstrapExportHorizonTest,"Seige.AI.NextPrerequisiteExportsFundMissingParts",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIBootstrapExportHorizonTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)||!AI.LoadConfig(S,FPaths::ProjectDir()/TEXT("AIFILES"),Error)){AddError(Error);return false;}
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    for(int32 I=0;I<400&&StartingCore(S)->IsConstructing;++I)S.Tick(10);
    const int32 CoreId=StartingCore(S)->Id;const auto Origin=StartingCore(S)->Position;
    // Explicit installed-port fixture: its complete material bill and its
    // grid road are paid from the real finite landing stock, never granted.
    const int32 PortId=S.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(1400,0));
    auto Pay=[&](const TMap<FString,double>& Bill)
    {
        for(const auto& P:Bill)if(S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)+1.e-8<P.Value){AddError(TEXT("Finite fixture cannot pay ")+P.Key);return false;}
        for(const auto& P:Bill)S.FindBuilding(CoreId)->Inventory.FindOrAdd(P.Key)-=P.Value;
        return true;
    };
    if(!Pay(S.Definition(*S.FindBuilding(PortId))->Cost))return false;
    FSeigeTransportSegment Grid;Grid.Id=900001;Grid.A=S.BuildingAccessPoint(*S.FindBuilding(CoreId));Grid.B=S.BuildingAccessPoint(*S.FindBuilding(PortId));Grid.Tier=Grid.TargetTier=S.InitialRoadTier();Grid.IsConstructing=false;Grid.ConstructionProgress=1;Grid.InstalledMaterials=S.RoadCost(Grid.A,Grid.B,Grid.Tier);
    if(!Pay(Grid.InstalledMaterials))return false;S.Roads.Add(Grid);
    int32 Index=0;for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active"))
    {W.CrewBuildingId=W.BuildingId=Index++==0?PortId:CoreId;W.Activity=TEXT("operate");W.RoadId=W.DeliveryId=W.ChargingBuildingId=W.TransportVehicleId=0;W.SuspendedActivity.Empty();W.Route.Empty();W.Position=S.BuildingAccessPoint(*S.FindBuilding(W.BuildingId));}
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);
    AI.Targets.Empty();FSeigeAIBuildTarget Next;Next.Definition=TEXT("plasma_tower");Next.Count=1;AI.Targets.Add(Next);
    FSeigeAIBuildTarget Later;Later.Definition=TEXT("alloy_refinery");Later.Count=20;AI.Targets.Add(Later);
    TestEqual(TEXT("A starting colony earns all money through trade"),S.Credits,0.);
    AI.ExportReserveHorizon=TEXT("full_plan");TestEqual(TEXT("The legacy whole-city reserve can lock all finite alloy"),AI.ManufacturedExportSurplus(S,TEXT("alloy")),0.);
    AI.ExportReserveHorizon=TEXT("next_prerequisite");const double Surplus=AI.ManufacturedExportSurplus(S,TEXT("alloy"));
    if(!TestTrue(TEXT("The next-prerequisite horizon releases a real useful shipment"),Surplus>1000))return false;
    FSeigeSimulation SiteCase=S;
    if(!SiteCase.PlaceBuilding(TEXT("solar_array"),Origin+FVector2D(0,1400),Error)){AddError(Error);return false;}
    TestTrue(TEXT("An accepted construction site remains protected outside the planning horizon"),AI.ManufacturedExportSurplus(SiteCase,TEXT("alloy"))<=Surplus-SiteCase.BuildingDefs[TEXT("solar_array")].Cost[TEXT("alloy")]+1.e-8);
    TestTrue(TEXT("The next physical connector also remains protected"),AI.ManufacturedExportSurplus(S,TEXT("alloy"),200)<=Surplus-200+1.e-8);
    // Model already-delivered port stock without changing one gram. The test
    // isolates AI trade decisions and the real timed/paid trade subsystem;
    // the live-terrain industry test separately requires physical truck delivery.
    const double AlloyBefore=S.TotalStock(TEXT("alloy"));
    // The receiver also retains its ordinary repair buffer; exactly one
    // shipment's mass alone is not exportable local stock.
    const double Staged=1000+S.OperatingBuffer(TEXT("alloy"));
    if(!TestTrue(TEXT("Finite source funds the shipment and local repair supplies"),S.FindBuilding(CoreId)->Inventory[TEXT("alloy")]>=Staged))return false;
    S.FindBuilding(CoreId)->Inventory[TEXT("alloy")]-=Staged;S.FindBuilding(PortId)->Inventory.Add(TEXT("alloy"),Staged);
    TestEqual(TEXT("Staging the finite shipment conserves alloy"),S.TotalStock(TEXT("alloy")),AlloyBefore);
    if(!TestTrue(TEXT("A missing prerequisite queues a real sale"),AI.ManageTrade(S)))return false;
    const auto Sale=S.FindBuilding(PortId)->Shipment;
    if(!TestTrue(TEXT("Only surplus alloy is exported"),!Sale.Buy&&Sale.Resource==TEXT("alloy")&&Sale.Quantity>0&&Sale.Quantity<=1000))return false;
    TestEqual(TEXT("Queueing an export creates no credits"),S.Credits,0.);
    for(int32 I=0;I<10000&&!S.FindBuilding(PortId)->Shipment.Resource.IsEmpty();++I){S.Energy.Tick(S,.1);S.Trade.Tick(S,.1);}
    if(!TestTrue(TEXT("Real workers and grid power complete the timed sale"),S.FindBuilding(PortId)->Shipment.Resource.IsEmpty()))return false;
    TestTrue(TEXT("Exactly the quoted proceeds and sold mass settle"),FMath::IsNearlyEqual(S.Credits,Sale.PriceCredits,1.e-8)&&FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy")),AlloyBefore-Sale.Quantity,1.e-8));
    const double Earned=S.Credits,PartsBefore=S.TotalStock(TEXT("ai_chips"));
    if(!TestTrue(TEXT("Earned money purchases the genuinely missing part"),AI.ManageTrade(S)))return false;
    const auto Buy=S.FindBuilding(PortId)->Shipment;
    if(!TestTrue(TEXT("The order funds only the missing prerequisite chips"),Buy.Buy&&Buy.Resource==TEXT("ai_chips")&&FMath::IsNearlyEqual(Buy.Quantity,S.BuildingDefs[Next.Definition].Cost[TEXT("ai_chips")],1.e-8)))return false;
    TestTrue(TEXT("Import charges the earned quote before delivering anything"),FMath::IsNearlyEqual(Earned-S.Credits,Buy.PriceCredits,1.e-8)&&S.TotalStock(TEXT("ai_chips"))==PartsBefore);
    for(int32 I=0;I<10000&&!S.FindBuilding(PortId)->Shipment.Resource.IsEmpty();++I){S.Energy.Tick(S,.1);S.Trade.Tick(S,.1);}
    TestTrue(TEXT("The paid timed import supplies the missing parts"),S.FindBuilding(PortId)->Shipment.Resource.IsEmpty()&&FMath::IsNearlyEqual(S.TotalStock(TEXT("ai_chips")),PartsBefore+Buy.Quantity,1.e-8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeDepletedBerthAdmissionTest,"Seige.AI.DepletedBodiesCannotReserveChargingBerths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeDepletedBerthAdmissionTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)){AddError(Error);return false;}
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    for(int32 I=0;I<400&&StartingCore(S)->IsConstructing;++I)S.Tick(10);
    const int32 CoreId=StartingCore(S)->Id;const auto Port=S.BuildingAccessPoint(*StartingCore(S));const int32 Bodies=S.Workers.Bodies.Num();
    if(!TestTrue(TEXT("Fixture starts with three real landed identities"),Bodies>=3))return false;
    const FString EmptyEntry=S.Workers.Bodies[0].Id,EmptyQueue=S.Workers.Bodies[1].Id,Powered=S.Workers.Bodies[2].Id;
    for(int32 I=0;I<3;++I)
    {
        auto& W=S.Workers.Bodies[I];W.CrewBuildingId=CoreId;W.BuildingId=W.RoadId=W.DeliveryId=W.TransportVehicleId=0;W.ChargingBuildingId=CoreId;W.ChargeQueueSlot=I+1;W.Activity=I==0?TEXT("berth_entry"):TEXT("charge_queue");W.SuspendedActivity=TEXT("operate");W.BatteryKWh=I==2?.05:0.;W.Position=Port+FVector2D(0,(I+1)*25);W.Route.Empty();
    }
    // A real paid load remains on the depleted pre-dock worker.
    if(!TestTrue(TEXT("Finite stock pays the stranded load"),S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy"))>=10))return false;
    S.FindBuilding(CoreId)->Inventory[TEXT("alloy")]-=10;FSeigeCourier Cargo;Cargo.Id=999991;Cargo.SourceId=Cargo.TargetId=CoreId;Cargo.Resource=TEXT("alloy");Cargo.Amount=10;Cargo.Phase=TEXT("carrying");Cargo.WorkerId=EmptyEntry;Cargo.Position=S.Workers.Find(EmptyEntry)->Position;S.Couriers.Add(Cargo);S.Workers.Find(EmptyEntry)->DeliveryId=Cargo.Id;
    const auto StrandedPosition=Cargo.Position;const double Stock=S.TotalStock(TEXT("alloy"));
    S.Workers.Tick(S,.05);
    TestEqual(TEXT("Exhausted pre-dock admission is released"),S.Workers.Find(EmptyEntry)->Activity,FString(TEXT("charge_queue")));
    TestEqual(TEXT("An already empty waiting worker cannot take the berth"),S.Workers.Find(EmptyQueue)->Activity,FString(TEXT("charge_queue")));
    TestEqual(TEXT("A powered waiting worker can claim the same free berth"),S.Workers.Find(Powered)->Activity,FString(TEXT("berth_entry")));
    bool Charged=false;for(int32 I=0;I<10000;++I){S.Energy.Tick(S,.05);S.Workers.Tick(S,.05);if(S.Workers.Find(Powered)->BatteryKWh>.15){Charged=true;break;}}
    TestTrue(TEXT("Other real workers physically dock and recharge despite depleted queue members"),Charged);
    TestTrue(TEXT("The stranded body remains at its actual position"),S.Workers.Find(EmptyEntry)->Position.Equals(StrandedPosition,1.e-8));
    TestEqual(TEXT("No worker identity was manufactured by queue recovery"),S.Workers.Bodies.Num(),Bodies);
    TestEqual(TEXT("All physical cargo remains conserved"),S.TotalStock(TEXT("alloy")),Stock);
    TestEqual(TEXT("No free rescue power was invented"),S.Workers.Find(EmptyEntry)->BatteryKWh,0.);
    return true;
}
#endif

