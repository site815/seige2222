#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCalendarEnergy,"Seige.Calendar.SolarNightAndConstantFusion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCalendarEnergy::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    // Isolate generation, not bootstrap. Solar still needs real operators.
    const auto Core=S.Buildings[0];const auto InitialBodies=S.Workers.Bodies;S.Buildings.Empty();
    FSeigeBuilding Solar;Solar.Id=100;Solar.DefId=TEXT("solar_array");Solar.Health=S.BuildingDefs[Solar.DefId].Health;Solar.Enabled=true;Solar.IsConstructing=false;Solar.ConstructionProgress=1;
    S.Buildings.Add(Solar);S.Energy.Invalidate();
    const auto Calendar=S.Calendar.GetRules();const double DaySeconds=double(Calendar.DaylightMicroseconds)/1.e6,NightSeconds=double(Calendar.NightMicroseconds)/1.e6;
    S.Calendar.SetElapsedMicroseconds(Calendar.DaylightMicroseconds/2);S.Energy.Tick(S,0);
    TestEqual(TEXT("Unstaffed solar cannot generate electricity"),S.Energy.Info(S).GenerationKW,0.0);
    const int32 Jobs=S.Definition(S.Buildings[0])->Jobs;
    if(!TestTrue(TEXT("The finite landed manifest can staff the solar fixture"),S.Workers.Bodies.Num()>=Jobs))return false;
    for(int32 I=0;I<Jobs;++I){auto& W=S.Workers.Bodies[I];W.State=TEXT("active");W.Activity=TEXT("operate");W.BuildingId=W.CrewBuildingId=Solar.Id;W.ContainerId=W.ChargingBuildingId=W.TransportVehicleId=W.DeliveryId=W.RoadId=0;W.ContainerKind.Empty();W.SuspendedActivity.Empty();W.Outdoor=true;W.StationSlot=I;W.Position=S.BuildingAccessPoint(Solar)+FVector2D(0,20*I);W.Route.Empty();W.NextWaypoint=0;}
    S.Workers.RefreshMetrics(S);S.Energy.Tick(S,0);
    TestEqual(TEXT("Noon solar query uses authored peak kW"),S.Energy.Info(S).GenerationKW,S.Energy.Definition(TEXT("solar_array"))->GenerationKW);
    S.Calendar.SetElapsedMicroseconds(Calendar.DaylightMicroseconds);const double Generated=S.Energy.GeneratedKWh;S.Energy.Tick(S,NightSeconds);
    TestEqual(TEXT("Entire night produces no solar electricity"),S.Energy.GeneratedKWh,Generated);
    TestEqual(TEXT("Night query is zero"),S.Energy.Info(S).GenerationKW,0.0);
    S.Calendar.SetElapsedMicroseconds(0);const double Start=S.Energy.GeneratedKWh;S.Energy.Tick(S,DaySeconds+NightSeconds);
    const double Expected=S.Energy.Definition(TEXT("solar_array"))->GenerationKW*2.*DaySeconds/(3600.*UE_DOUBLE_PI);
    TestTrue(TEXT("Energy ledger integrates the solar curve over day and night"),FMath::IsNearlyEqual(S.Energy.GeneratedKWh-Start,Expected,1.e-9));
    S.Buildings={Core};S.Workers.Bodies=InitialBodies;S.Workers.RefreshMetrics(S);S.Buildings[0].IsConstructing=true;S.Energy.Invalidate();S.Calendar.SetElapsedMicroseconds(Calendar.DaylightMicroseconds+Calendar.NightMicroseconds/2);S.Energy.Tick(S,0);
    TestEqual(TEXT("Installed core fusion remains available at night"),S.Energy.Info(S).GenerationKW,S.Energy.Definition(S.CoreDefinition)->GenerationKW);
    return true;
}
