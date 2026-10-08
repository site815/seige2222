#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRoadRepairTest,"Seige.Simulation.RoadWearRepairUsesLocalService",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeRoadRepairTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    // Deploy the real finite crew; only the roads are focused installed fixtures.
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("Physical starter deployment completes"),S.Buildings[0].IsConstructing))return false;
    S.Tick(20);auto& Core=S.Buildings[0];
    S.AllocateWorkers();S.Energy.Invalidate();S.Energy.Tick(S,0);
    const FVector2D Port=S.BuildingAccessPoint(Core);
    auto InstallRoad=[&](FVector2D A,FVector2D B)->int32
    {
        if(!S.PlaceRoad(A,B,Error)){AddError(Error);return 0;}
        auto& R=S.Roads.Last();R.InstalledMaterials=S.RoadCost(A,B,R.TargetTier);
        for(const auto& P:R.InstalledMaterials)Core.Inventory.FindOrAdd(P.Key)-=P.Value;
        R.Tier=R.TargetTier;R.ConstructionProgress=1;R.IsConstructing=false;R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;
        ++S.TransportRevision;return R.Id;
    };
    const int32 Near=InstallRoad(Port,FVector2D(1500,0)),Far=InstallRoad(FVector2D(1500,0),FVector2D(2200,0));
    const int32 Disconnected=InstallRoad(FVector2D(900,100),FVector2D(1500,100));
    if(!Near||!Far||!Disconnected)return false;
    S.AllocateWorkers();S.Energy.Invalidate();S.Energy.Tick(S,0);
    const FString Material=S.Transport->GetStringField(TEXT("repair_resource"));
    const double Rate=S.Transport->GetNumberField(TEXT("repair_health_per_second")),PerUnit=S.Transport->GetNumberField(TEXT("repair_health_per_unit")),EnergyPerHealth=S.Transport->GetNumberField(TEXT("repair_energy_kwh_per_health"));
    for(int32 Id:{Near,Far,Disconnected})S.DamageRoad(Id,Rate*10);
    TestTrue(TEXT("Remote road shares the grid but not physical service reach"),S.Energy.RoadConnectedToBuilding(Far,Core.Id));
    TestFalse(TEXT("A nearby parallel road is a separate grid component"),S.Energy.RoadConnectedToBuilding(Disconnected,Core.Id));
    TestEqual(TEXT("Worn road requests the configured local repair buffer"),S.RoadRepairDemand(Core,Material),S.Transport->GetNumberField(TEXT("repair_buffer_units")));
    const double Before=S.FindRoad(Near)->Health,Remote=S.FindRoad(Far)->Health,Separate=S.FindRoad(Disconnected)->Health;
    const double Stock=Core.Inventory.FindRef(Material),Energy=S.Energy.ConsumedKWh,Expected=Rate*S.WorkFraction(Core);
    if(!TestTrue(TEXT("Real deployed operators can perform nonzero service work"),Expected>0))return false;
    S.StepRoadRepairs(1);
    const double Restored=S.FindRoad(Near)->Health-Before;
    TestTrue(TEXT("Staffed local service restores the authored rate"),FMath::IsNearlyEqual(Restored,Expected,1.e-7));
    TestTrue(TEXT("Road repair consumes exactly the local physical material"),FMath::IsNearlyEqual(Stock-Core.Inventory.FindRef(Material),Restored/PerUnit,1.e-7));
    TestTrue(TEXT("Road repair charges exact component energy"),FMath::IsNearlyEqual(S.Energy.ConsumedKWh-Energy,Restored*EnergyPerHealth,1.e-8));
    TestEqual(TEXT("Grid connection cannot teleport maintenance beyond physical reach"),S.FindRoad(Far)->Health,Remote);
    TestEqual(TEXT("Nearby service cannot borrow power from a disconnected component"),S.FindRoad(Disconnected)->Health,Separate);
    const double LocalStock=Core.Inventory.FindRef(Material);Core.Inventory.Add(Material,0);
    FSeigeCourier InTransit;InTransit.Resource=Material;InTransit.Amount=LocalStock;S.Couriers.Add(InTransit);
    const double Unrepaired=S.FindRoad(Near)->Health;
    TestTrue(TEXT("Physical material remains elsewhere in colony's ledger"),S.TotalStock(Material)>0);
    S.StepRoadRepairs(1);TestEqual(TEXT("Undelivered cargo cannot repair a road"),S.FindRoad(Near)->Health,Unrepaired);
    S.Couriers.Empty();Core.Inventory.Add(Material,LocalStock);
    const double Battery=Core.BatteryEnergyKWh;Core.BatteryEnergyKWh=0;
    S.StepRoadRepairs(1);TestEqual(TEXT("Materials alone cannot bypass the repair energy cost"),S.FindRoad(Near)->Health,Unrepaired);
    Core.BatteryEnergyKWh=Battery;Core.Enabled=false;
    S.StepRoadRepairs(1);TestEqual(TEXT("Disabled service cannot perform repairs"),S.FindRoad(Near)->Health,Unrepaired);Core.Enabled=true;
    const FVector2D ReusablePlot=FMath::Lerp(Port,FVector2D(1500,0),.5);
    TestFalse(TEXT("A living road still reserves its physical corridor against building plots"),S.CanPlaceBuilding(TEXT("sensor"),ReusablePlot,Error));
    S.DamageRoad(Near,S.FindRoad(Near)->MaxHealth);S.Energy.Tick(S,0);S.StepRoadRepairs(1);
    TestEqual(TEXT("Destroyed roads require rebuilding and never resurrect through wear repair"),S.FindRoad(Near)->Health,0.);
    TestTrue(TEXT("A destroyed road no longer blocks an otherwise legal reserved plot"),S.CanPlaceBuilding(TEXT("sensor"),ReusablePlot,Error));
    return true;
}
#endif
