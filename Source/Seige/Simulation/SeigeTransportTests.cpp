#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags TransportTestFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString Rules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
FString SaveFile(const FString& Name){return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Transport"),Name+TEXT(".json"));}
void Advance(FSeigeSimulation& S,double Duration){for(double Remaining=Duration;Remaining>UE_DOUBLE_SMALL_NUMBER;){const double Chunk=FMath::Min(10.,Remaining);S.Tick(Chunk);Remaining-=Chunk;}}
bool Deploy(FSeigeSimulation& S,FString& Error){if(!S.Initialize(Rules(),Error,false,false))return false;Advance(S,S.BuildingDefs[S.CoreDefinition].ConstructionSeconds+S.FixedStepSeconds());return !S.Buildings[0].IsConstructing;}
// Pay for the crew through the slow core replicator before measuring walking
// and road assembly; worker-manufacture latency has its own lifecycle tests.
bool PrepareStoredCrew(FSeigeSimulation& S,FString& Error)
{
    int32 Count=S.BuildingDefs[TEXT("sensor")].ConstructionWorkers;
    for(const auto& Tier:S.TransportTiers)Count=FMath::Max(Count,Tier.Value.ConstructionWorkers);
    if(!S.SetWorkerSurplusTarget(Count,Error))return false;
    FString WorkerRecipe;
    for(const auto& Id:S.ProductionOptions(S.Buildings[0].Id))if(S.Recipes[Id].WorkerOutput>0){WorkerRecipe=Id;break;}
    if(WorkerRecipe.IsEmpty()){Error=TEXT("Core must offer worker assembly");return false;}
    const double Limit=S.ProductionSeconds(S.Buildings[0],WorkerRecipe)*(Count+1)+60;
    for(double Elapsed=0;Elapsed<Limit&&S.InactiveWorkerCount()<Count;Elapsed+=10)S.Tick(10);
    if(S.InactiveWorkerCount()<Count){Error=TEXT("Core could not manufacture the paid transport-test crew");return false;}
    return S.SetWorkerSurplusTarget(0,Error);
}
double InstalledAlloy(const FSeigeSimulation& S){double Result=0;for(const auto& B:S.Buildings)Result+=B.InstalledMaterials.FindRef(TEXT("alloy"));for(const auto& R:S.Roads)Result+=R.InstalledMaterials.FindRef(TEXT("alloy"))+R.PreviousTierMaterials.FindRef(TEXT("alloy"));return Result;}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigePhasedWorkTest,"Seige.Simulation.WorkerTravelAndPhasedConstruction",TransportTestFlags)
bool FSeigePhasedWorkTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation A,B;if(!Deploy(A,Error)||!PrepareStoredCrew(A,Error)){AddError(Error);return false;}
    const auto& CoreDef=A.BuildingDefs[A.CoreDefinition];
    TestEqual(TEXT("Command body begins at level-one dimensions"),CoreDef.Footprint,240.);
    TestEqual(TEXT("Command reserves nine times the starting body area"),CoreDef.ReservedFootprint/CoreDef.Footprint,3.);
    TestFalse(TEXT("Reserved future command plot cannot be occupied"),A.CanPlaceBuilding(TEXT("sensor"),FVector2D(700,0),Error));
    if(!A.PlaceBuilding(TEXT("sensor"),FVector2D(1200,0),Error)){AddError(Error);return false;}
    const int32 SiteId=A.Buildings.Last().Id;const double Material=A.TotalStock(TEXT("alloy"))+InstalledAlloy(A);
    const FVector2D Start=A.FindBuilding(SiteId)->BuilderPosition;
    // Observe a fraction of the actual walk, after a stored body activates.
    // A fixed 65s probe can already be beyond arrival on this shorter route.
    for(double Wait=0;Wait<60&&A.FindBuilding(SiteId)->TravellingBuilders==0;Wait+=A.FixedStepSeconds())A.Tick(A.FixedStepSeconds());
    const auto* Departing=A.FindBuilding(SiteId);
    if(!TestTrue(TEXT("Stored crew reactivates and is dispatched"),Departing->TravellingBuilders>0))return false;
    double RemainingDistance=0;FVector2D Previous=Departing->BuilderPosition;
    for(int32 I=Departing->BuilderNextWaypoint;I<Departing->BuilderRoute.Num();++I){RemainingDistance+=FVector2D::Distance(Previous,Departing->BuilderRoute[I]);Previous=Departing->BuilderRoute[I];}
    Advance(A,RemainingDistance/A.WalkingSpeed()*.25);
    const auto* Site=A.FindBuilding(SiteId);
    TestTrue(TEXT("Assigned crew physically leaves the command access port"),FVector2D::Distance(Site->BuilderPosition,Start)>0);
    TestEqual(TEXT("Travelling crew cannot perform on-site work"),Site->BuildersOnSite,0);
    TestEqual(TEXT("Delivered materials alone do not advance construction"),Site->ConstructionProgress,0.);
    if(!A.Save(SaveFile(TEXT("crew-travelling")),Error)||!B.Initialize(Rules(),Error)||!B.Load(SaveFile(TEXT("crew-travelling")),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Saved crew remains at its actual travel position"),B.FindBuilding(SiteId)->BuilderPosition.Equals(Site->BuilderPosition,1.e-8));
    Advance(A,150);Advance(B,150);Site=A.FindBuilding(SiteId);
    TestTrue(TEXT("On-site crew performs partial construction"),Site->BuildersOnSite>0&&Site->ConstructionProgress>0&&Site->ConstructionProgress<1);
    TestTrue(TEXT("Installed material increases while physical site stock decreases"),Site->InstalledMaterials.FindRef(TEXT("alloy"))>0&&Site->ConstructionMaterials.FindRef(TEXT("alloy"))>0);
    TestTrue(TEXT("Stock plus incorporated material is conserved"),FMath::IsNearlyEqual(A.TotalStock(TEXT("alloy"))+InstalledAlloy(A),Material,1.e-6));
    if(!A.Save(SaveFile(TEXT("crew-a")),Error)||!B.Save(SaveFile(TEXT("crew-b")),Error)){AddError(Error);return false;}
    FString Left,Right;FFileHelper::LoadFileToString(Left,*SaveFile(TEXT("crew-a")));FFileHelper::LoadFileToString(Right,*SaveFile(TEXT("crew-b")));
    TestEqual(TEXT("Travel and phased work continue deterministically after loading"),Left,Right);
    A.ToggleBuilding(SiteId);
    if(!A.Save(SaveFile(TEXT("crew-paused-between-steps")),Error)||!B.Load(SaveFile(TEXT("crew-paused-between-steps")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Pausing then immediately saving keeps crew assignment valid without a fixed tick"),B.FindBuilding(SiteId)->BuildersOnSite,0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeTransportNetworkTest,"Seige.Simulation.RoadNetworkSpeedUpgradeAndPersistence",TransportTestFlags)
bool FSeigeTransportNetworkTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S,Loaded;if(!Deploy(S,Error)||!PrepareStoredCrew(S,Error)){AddError(Error);return false;}
    TestTrue(TEXT("Default walking pace converts to exactly five kilometres per hour"),FMath::IsNearlyEqual(S.WalkingSpeed()*S.MetersPerWorldUnit()*3.6,5.,1.e-9));
    TArray<FVector2D> Route;
    if(!TestTrue(TEXT("Walking path can go around command reserved plot"),S.FindRoute(FVector2D(-1100,0),FVector2D(1100,0),Route)))return false;
    double Length=0;FVector2D P(-1100,0);for(const auto& Waypoint:Route){TestTrue(TEXT("Every path leg avoids reserved plots"),S.ClearWalkingLine(P,Waypoint));Length+=FVector2D::Distance(P,Waypoint);P=Waypoint;}
    TestTrue(TEXT("Path does not walk straight through command building"),Length>2200);
    const FVector2D A=S.BuildingAccessPoint(S.Buildings[0]),B(2200,0);
    TestFalse(TEXT("Road cannot be placed across reserved command plot"),S.CanPlaceRoad(FVector2D(-1000,0),FVector2D(1000,0),Error));
    const double Material=S.TotalStock(TEXT("alloy"))+InstalledAlloy(S);
    if(!S.PlaceRoad(A,B,Error)){AddError(Error);return false;}const int32 RoadId=S.Roads.Last().Id;
    TestEqual(TEXT("Unfinished road confers no transport benefit"),S.RouteSpeedMultiplier(A,B),1.);
    TestTrue(TEXT("Road order reserves rather than consuming physical material"),FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy"))+InstalledAlloy(S),Material));
    Advance(S,1200);
    if(!TestFalse(TEXT("Workers complete the material-supplied road"),S.FindRoad(RoadId)->IsConstructing))return false;
    for(int32 TierIndex=0;TierIndex<3;++TierIndex)
    {
        const double Expected=FMath::Pow(2.,TierIndex+1);
        TestEqual(TEXT("Completed road tier provides its authored multiplier"),S.RouteSpeedMultiplier(A,B),Expected);
        Route.Empty();if(!TestTrue(TEXT("Route planner uses connected road endpoints"),S.FindRoute(A,B,Route)))return false;
        P=A;int32 Next=0;S.WalkRoute(P,Route,Next,1.);
        TestTrue(TEXT("Actual route traversal advances at walking pace times the tier multiplier"),FMath::IsNearlyEqual(FVector2D::Distance(A,P),S.WalkingSpeed()*Expected,1.e-5));
        if(TierIndex==2)break;
        if(!S.UpgradeRoad(RoadId,Error)){AddError(Error);return false;}
        TestEqual(TEXT("Upgrade keeps previous transport service active"),S.RouteSpeedMultiplier(A,B),Expected);
        Advance(S,150);
        if(!S.Save(SaveFile(TEXT("road-upgrading")),Error)||!Loaded.Initialize(Rules(),Error)||!Loaded.Load(SaveFile(TEXT("road-upgrading")),Error)){AddError(Error);return false;}
        TestEqual(TEXT("Upgrade target survives save/load"),Loaded.FindRoad(RoadId)->TargetTier,S.FindRoad(RoadId)->TargetTier);
        const double Duration=S.TransportTiers[S.FindRoad(RoadId)->TargetTier].ConstructionSecondsPer100Meters*FVector2D::Distance(A,B)*S.MetersPerWorldUnit()/100+500;
        Advance(S,Duration);Advance(Loaded,Duration);
        if(!TestFalse(TEXT("Upgraded corridor completes physically"),S.FindRoad(RoadId)->IsConstructing))return false;
        if(!S.Save(SaveFile(TEXT("road-a")),Error)||!Loaded.Save(SaveFile(TEXT("road-b")),Error)){AddError(Error);return false;}
        FString Left,Right;FFileHelper::LoadFileToString(Left,*SaveFile(TEXT("road-a")));FFileHelper::LoadFileToString(Right,*SaveFile(TEXT("road-b")));
        TestEqual(TEXT("Road upgrades and cargo continue deterministically after loading"),Left,Right);
    }
    TestTrue(TEXT("All transport tiers conserve stored plus installed construction material"),FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy"))+InstalledAlloy(S),Material,1.e-5));
    TestFalse(TEXT("Maximum corridor tier cannot upgrade again"),S.UpgradeRoad(RoadId,Error));
    if(!S.Save(SaveFile(TEXT("valid")),Error))return false;
    FString Raw;FFileHelper::LoadFileToString(Raw,*SaveFile(TEXT("valid")));TSharedPtr<FJsonObject> Json;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Json);
    Json->GetArrayField(TEXT("roads"))[0]->AsObject()->SetStringField(TEXT("target_tier"),TEXT("teleporter"));
    Raw.Empty();FJsonSerializer::Serialize(Json.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw));FFileHelper::SaveStringToFile(Raw,*SaveFile(TEXT("invalid")));
    const double Before=S.Time;TestFalse(TEXT("Unknown saved transport tier is rejected"),S.Load(SaveFile(TEXT("invalid")),Error));TestEqual(TEXT("Failed road load is atomic"),S.Time,Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRoadPlanLengthsTest,"Seige.Simulation.RoadPlansRespectConstructionLengths",TransportTestFlags)
bool FSeigeRoadPlanLengthsTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const FVector2D Start(1000,0),Near(1050,0);TArray<FVector2D> Route;
    TestFalse(TEXT("A three-metre new link is too short to build"),S.CanPlaceRoad(Start,Near,Error));
    if(!TestTrue(TEXT("Road planner finds a legal detour rather than returning an unbuildable short edge"),S.FindRoadRoute(Start,Near,Route)))return false;
    TestTrue(TEXT("Short port connection is resolved with more than one valid segment"),Route.Num()>1);
    FVector2D Previous=Start;for(const auto& P:Route){TestTrue(TEXT("Every newly planned detour segment can actually be ordered"),S.CanPlaceRoad(Previous,P,Error));Previous=P;}
    const double VehicleRadius=58.;
    if(!TestTrue(TEXT("Vehicle clearance permits a short direct movement without road construction constraints"),S.FindRoute(Start,Near,Route,VehicleRadius)))return false;
    TestEqual(TEXT("A three-metre vehicle move stays direct"),Route.Num(),1);
    TestTrue(TEXT("Short vehicle route ends at the actual requested point"),Route.Last().Equals(Near,1.e-8));
    // Exercise the cache at identical endpoints and clearance for both route modes.
    // A real paid corridor keeps these calls on the cached graph path, rather
    // than the no-road straight-line optimization.
    if(!S.PlaceRoad(FVector2D(1600,300),FVector2D(2000,300),Error)){AddError(Error);return false;}
    auto& Existing=S.Roads.Last();Existing.InstalledMaterials=S.RoadCost(Existing.A,Existing.B,Existing.TargetTier);
    for(const auto& P:Existing.InstalledMaterials)S.Buildings[0].Inventory.FindOrAdd(P.Key)-=P.Value;
    Existing.Tier=Existing.TargetTier;Existing.IsConstructing=false;Existing.ConstructionProgress=1;Existing.Builders=Existing.BuildersOnSite=Existing.TravellingBuilders=0;
    if(!S.FindRoute(Start,Near,Route,VehicleRadius))return false;
    TestEqual(TEXT("Short vehicle move remains direct on the cached route graph"),Route.Num(),1);
    if(!TestTrue(TEXT("Explicit road-planning mode still enforces the minimum segment"),S.FindRoute(Start,Near,Route,VehicleRadius,true)))return false;
    TestTrue(TEXT("A cached vehicle shortcut cannot contaminate a road construction plan"),Route.Num()>1);
    if(!S.FindRoute(Start,Near,Route,VehicleRadius))return false;
    TestEqual(TEXT("A cached road detour cannot contaminate a vehicle movement route"),Route.Num(),1);
    if(!TestTrue(TEXT("A long clear corridor can be divided into buildable-length pieces"),S.FindRoadRoute(Start,FVector2D(25000,0),Route)))return false;
    Previous=Start;for(const auto& P:Route){const double Metres=FVector2D::Distance(Previous,P)*S.MetersPerWorldUnit();TestTrue(TEXT("Planned pieces obey configured current minimum and maximum lengths"),Metres>=6-1.e-8&&Metres<=500+1.e-8);Previous=P;}
    return true;
}
#endif
