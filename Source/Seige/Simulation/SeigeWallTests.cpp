#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
bool Deploy(FSeigeSimulation& S,FString& Error){if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false))return false;for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);if(S.Buildings[0].IsConstructing){Error=TEXT("Physical worker deployment did not complete");return false;}S.Tick(20);return true;}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWallPlanTest,"Seige.Simulation.Walls.PreviewCommitAndAtomicRejection",Flags)
bool FSeigeWallPlanTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const auto Core=S.Buildings[0].Position;const TArray<FVector2D> Joints={Core+FVector2D(-300,-1000),Core+FVector2D(300,-1000)};
    FSeigeWallPlan Plan;const int32 Before=S.Buildings.Num();const double Metal=S.TotalStock(TEXT("alloy"));
    if(!S.Walls.Plan(S,Joints,true,Plan,Error)){AddError(Error);return false;}
    TestEqual(TEXT("36-metre edge subdivides into six actual build sites"),Plan.Segments.Num(),6);
    TestEqual(TEXT("Preview does not create buildings"),S.Buildings.Num(),Before);
    if(!S.Walls.Commit(S,Joints,true,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Commit creates all sections atomically"),S.Buildings.Num(),Before+6);
    TestEqual(TEXT("No delivered cargo is teleported or consumed at commit"),S.TotalStock(TEXT("alloy")),Metal);
    for(const auto& W:S.Walls.Segments){const auto* B=S.FindBuilding(W.BuildingId);TestTrue(TEXT("New wall is a real construction site"),B&&B->IsConstructing&&B->ConstructionProgress==0);FVector2D P;TestTrue(TEXT("Inside loading point is outside wall body"),S.Walls.AccessPoint(S,W.BuildingId,P)&&FMath::Abs(P.Y-B->Position.Y)>S.Definition(*B)->ReservedFootprint);}
    const int32 Committed=S.Buildings.Num();TestFalse(TEXT("Duplicate geometry rejected"),S.Walls.Commit(S,Joints,true,Error));TestEqual(TEXT("Rejected plan adds nothing"),S.Buildings.Num(),Committed);
    TestFalse(TEXT("Core plot cannot be walled through"),S.Walls.Commit(S,{Core-FVector2D(800,0),Core+FVector2D(800,0)},true,Error));
    TestEqual(TEXT("Plot collision preserves all buildings"),S.Buildings.Num(),Committed);
    const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/walls.json"));FSeigeSimulation Loaded;
    if(!S.Save(Path,Error)||!Loaded.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error)||!Loaded.Load(Path,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Committed geometry persists"),Loaded.Walls.Segments.Num(),6);TestTrue(TEXT("Inside designation persists"),Loaded.Walls.Segments[0].InsideLeft);
    TestTrue(TEXT("Geometry survives deterministic continuation"),Loaded.Walls.Segments[0].A.Equals(S.Walls.Segments[0].A));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWallGeometryTest,"Seige.Simulation.Walls.InvalidCoordinatesCrossingsAndSides",Flags)
bool FSeigeWallGeometryTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}const auto Core=S.Buildings[0].Position;FSeigeWallPlan P;
    TestFalse(TEXT("Outside-sector plan rejected before subdivision"),S.Walls.Plan(S,{FVector2D(1.e100,0),FVector2D(0,0)},true,P,Error));
    TestFalse(TEXT("One joint is not a wall"),S.Walls.Plan(S,{Core},true,P,Error));
    TestFalse(TEXT("Self crossing rejected"),S.Walls.Plan(S,{Core+FVector2D(-300,-1400),Core+FVector2D(300,-900),Core+FVector2D(-300,-900),Core+FVector2D(300,-1400)},true,P,Error));
    if(!S.Walls.Commit(S,{Core+FVector2D(-300,-1000),Core+FVector2D(300,-1000)},false,Error)){AddError(Error);return false;}
    const auto& W=S.Walls.Segments[0];FVector2D Access;S.Walls.AccessPoint(S,W.BuildingId,Access);TestTrue(TEXT("Flipped side changes real worker delivery approach"),Access.Y<S.FindBuilding(W.BuildingId)->Position.Y);
    auto Root=MakeShared<FJsonObject>();S.Walls.Save(Root);Root->GetArrayField(TEXT("wall_segments"))[0]->AsObject()->SetNumberField(TEXT("ax"),0);const auto Before=S.Walls.Segments[0].A;
    TestFalse(TEXT("Corrupted geometry inconsistent with body is rejected"),S.Walls.Load(Root,S,Error));TestTrue(TEXT("Malformed geometry load is atomic"),S.Walls.Segments[0].A.Equals(Before));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWallOccupancyTest,"Seige.Simulation.Walls.OccupiedCorridor",Flags)
bool FSeigeWallOccupancyTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const auto Core=S.Buildings[0].Position;const TArray<FVector2D> Joints={Core+FVector2D(-300,-1000),Core+FVector2D(300,-1000)};
    const auto At=Core+FVector2D(-250,-1000);FSeigeWallPlan P;const int32 Before=S.Buildings.Num();
    if(S.Workers.Bodies.IsEmpty()){AddError(TEXT("Fixture requires real landed worker bodies"));return false;}
    auto& Body=S.Workers.Bodies[0];const auto Original=Body;
    Body.Position=At;Body.State=TEXT("active");Body.Activity=TEXT("idle");Body.BuildingId=Body.RoadId=Body.DeliveryId=0;Body.Outdoor=true;
    TestFalse(TEXT("An individual idle outdoor worker blocks the wall even without an aggregate crew counter"),S.Walls.Plan(S,Joints,true,P,Error));
    Body.Position=At+FVector2D(0,S.BuildingDefs[TEXT("wall_segment")].ReservedFootprint+S.Workers.BodyRadiusMeters()/S.MetersPerWorldUnit()*.5);
    TestFalse(TEXT("The wall plot also respects the physical body radius beyond its pivot"),S.Walls.Plan(S,Joints,true,P,Error));
    Body.Outdoor=false;TestTrue(TEXT("A worker inside a building does not reserve outdoor wall ground"),S.Walls.Plan(S,Joints,true,P,Error));
    Body=Original;
    FSeigeCourier C;C.Position=At;S.Couriers.Add(C);
    TestFalse(TEXT("Wall cannot trap a courier"),S.Walls.Commit(S,Joints,true,Error));
    TestEqual(TEXT("Occupied corridor rejection is atomic"),S.Buildings.Num(),Before);S.Couriers.Empty();
    S.Buildings[0].TravellingBuilders=1;S.Buildings[0].BuilderPosition=At;
    TestFalse(TEXT("Wall cannot trap travelling construction crew"),S.Walls.Plan(S,Joints,true,P,Error));S.Buildings[0].TravellingBuilders=0;
    FSeigeTransportSegment R;R.Health=1;R.TravellingBuilders=1;R.BuilderPosition=At;R.A=Core+FVector2D(1300,1200);R.B=Core+FVector2D(1400,1200);S.Roads.Add(R);
    TestFalse(TEXT("Wall cannot trap travelling road crew"),S.Walls.Plan(S,Joints,true,P,Error));S.Roads.Empty();
    if(S.Combat.Vehicles.IsEmpty()){AddError(TEXT("Fixture requires the initial defense force"));return false;}
    auto& V=S.Combat.Vehicles[0];V.Position=At;V.SectorIndex=4;V.Embarked=false;
    TestFalse(TEXT("Wall cannot trap a deployed vehicle"),S.Walls.Plan(S,Joints,true,P,Error));V.Embarked=true;
    TestTrue(TEXT("Embarked vehicles do not block ground construction"),S.Walls.Plan(S,Joints,true,P,Error));
    return true;
}
#endif
