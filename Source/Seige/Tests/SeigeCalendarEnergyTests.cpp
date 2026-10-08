#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCalendarEnergy,"Seige.Calendar.SolarNightAndConstantFusion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCalendarEnergy::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    // Isolate generation, not bootstrap: one completed unstaffed solar array.
    const auto Core=S.Buildings[0];S.Buildings.Empty();
    FSeigeBuilding Solar;Solar.Id=100;Solar.DefId=TEXT("solar_array");Solar.Health=S.BuildingDefs[Solar.DefId].Health;Solar.Enabled=true;Solar.IsConstructing=false;Solar.ConstructionProgress=1;
    S.Buildings.Add(Solar);S.Energy.Invalidate();
    S.Calendar.SetElapsedMicroseconds(900000000);S.Energy.Tick(S,0);
    TestEqual(TEXT("Noon solar query uses authored peak kW"),S.Energy.Info(S).GenerationKW,S.Energy.Definition(TEXT("solar_array"))->GenerationKW);
    S.Calendar.SetElapsedMicroseconds(1800000000);const double Generated=S.Energy.GeneratedKWh;S.Energy.Tick(S,1800);
    TestEqual(TEXT("Entire night produces no solar electricity"),S.Energy.GeneratedKWh,Generated);
    TestEqual(TEXT("Night query is zero"),S.Energy.Info(S).GenerationKW,0.0);
    S.Calendar.SetElapsedMicroseconds(0);const double Start=S.Energy.GeneratedKWh;S.Energy.Tick(S,3600);
    TestTrue(TEXT("Energy ledger integrates the solar curve over day and night"),FMath::IsNearlyEqual(S.Energy.GeneratedKWh-Start,25/UE_DOUBLE_PI,1.e-9));
    S.Buildings={Core};S.Buildings[0].IsConstructing=true;S.Energy.Invalidate();S.Calendar.SetElapsedMicroseconds(2700000000);S.Energy.Tick(S,0);
    TestEqual(TEXT("Installed core fusion remains available at night"),S.Energy.Info(S).GenerationKW,S.Energy.Definition(S.CoreDefinition)->GenerationKW);
    return true;
}
