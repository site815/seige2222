#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeTransportPaymentRoundingTest,"Seige.Transport.FabricationPaymentRoundingPersists",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeTransportPaymentRoundingTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;const FString Rules=FPaths::ProjectDir()/TEXT("Rules");
    if(!S.Initialize(Rules,Error,false,false)){AddError(Error);return false;}
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("Finite landing crew completes normal deployment"),S.Buildings[0].IsConstructing))return false;
    const int32 Core=S.Buildings[0].Id,Factory=S.AddReviewBuilding(TEXT("vehicle_factory"),S.Buildings[0].Position+FVector2D(1800,0));
    S.AddReviewBuilding(TEXT("battery_bank"),S.Buildings[0].Position+FVector2D(0,1800));
    // Installed infrastructure isolates payment precision. All vehicle material
    // comes from the finite kit and all operators are existing physical bodies.
    const auto Def=S.TransportVehicles.Definition();
    for(const auto& P:Def.BuildCost)
    {
        if(!TestTrue(TEXT("The finite kit pays the complete vehicle ingredient"),S.FindBuilding(Core)->Inventory.FindRef(P.Key)>=P.Value))return false;
        S.FindBuilding(Core)->Inventory.FindOrAdd(P.Key)-=P.Value;S.FindBuilding(Factory)->Inventory.Add(P.Key,P.Value);
    }
    for(int32 I=0;I<S.Definition(*S.FindBuilding(Factory))->Jobs;++I)
    {
        auto& W=S.Workers.Bodies[I];W.CrewBuildingId=W.BuildingId=Factory;W.Activity=TEXT("operate");W.Outdoor=true;W.ContainerKind.Empty();
        W.ContainerId=W.RoadId=W.DeliveryId=W.ChargingBuildingId=W.TransportVehicleId=0;W.Route.Empty();W.NextWaypoint=0;W.StationSlot=I;
        W.Position=S.BuildingAccessPoint(*S.FindBuilding(Factory))+FVector2D(0,20*(I+1));
    }
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,7200);
    if(!TestTrue(TEXT("Real grid generation funds the assembly energy"),S.Energy.Info(S,Factory).StoredKWh>=Def.BuildEnergyKWh))return false;
    if(!S.TransportVehicles.QueueVehicle(S,Factory,Error)){AddError(Error);return false;}
    const int32 Vehicles=S.TransportVehicles.LiveCount(),Workers=S.Workers.Bodies.Num();const double Energy=S.Energy.ConsumedKWh;
    S.FindBuilding(Factory)->Inventory[TEXT("batteries")]-=1.e-6;S.FindBuilding(Core)->Inventory.FindOrAdd(TEXT("batteries"))+=1.e-6;
    S.TransportVehicles.Tick(S,0);
    TestFalse(TEXT("A real shortage outside payment tolerance cannot commit"),S.TransportVehicles.Jobs[0].Committed);
    TestEqual(TEXT("Rejected payment spends no assembly energy"),S.Energy.ConsumedKWh,Energy);
    const double Refill=1.e-6-1.e-12;
    S.FindBuilding(Core)->Inventory[TEXT("batteries")]-=Refill;S.FindBuilding(Factory)->Inventory[TEXT("batteries")]+=Refill;
    TMap<FString,double> Before;for(const auto& P:Def.BuildCost)Before.Add(P.Key,S.TotalStock(P.Key));
    S.TransportVehicles.Tick(S,0);
    if(!TestTrue(TEXT("A delivered bill within floating-point tolerance commits once"),S.TransportVehicles.Jobs.Num()==1&&S.TransportVehicles.Jobs[0].Committed))return false;
    TestEqual(TEXT("Payment consumes exactly the authored energy"),S.Energy.ConsumedKWh-Energy,Def.BuildEnergyKWh);
    TestEqual(TEXT("Payment does not instantly create a vehicle"),S.TransportVehicles.LiveCount(),Vehicles);
    for(const auto& P:Def.BuildCost)
    {
        TestEqual(TEXT("Accepted debit leaves nonnegative empty local input"),S.FindBuilding(Factory)->Inventory.FindRef(P.Key),0.);
        TestTrue(TEXT("Every paid ingredient remains in fabrication escrow within numerical tolerance"),FMath::IsNearlyEqual(S.TotalStock(P.Key),Before[P.Key],1.e-9));
        TestEqual(TEXT("Escrow contains the exact authored ingredient"),S.TransportVehicles.CommittedStock(P.Key),P.Value);
    }
    const FString Path=FPaths::ProjectSavedDir()/TEXT("Automation/Transport/fabrication-rounding.json");FSeigeSimulation Loaded;
    if(!S.Save(Path,Error)||!Loaded.Initialize(Rules,Error,false,false)||!Loaded.Load(Path,Error)){AddError(TEXT("Strict paid-job save/reload: ")+Error);return false;}
    TestTrue(TEXT("Reload retains the one paid commitment"),Loaded.TransportVehicles.Jobs.Num()==1&&Loaded.TransportVehicles.Jobs[0].Committed);
    Loaded.TransportVehicles.Tick(Loaded,Def.BuildSeconds*2);
    TestEqual(TEXT("Only the paid job produces one additional vehicle"),Loaded.TransportVehicles.LiveCount(),Vehicles+1);
    TestEqual(TEXT("Completed assembly clears its payment escrow"),Loaded.TransportVehicles.Jobs.Num(),0);
    if(!Loaded.TransportVehicles.QueueVehicle(Loaded,Factory,Error)){AddError(Error);return false;}
    Loaded.TransportVehicles.Tick(Loaded,Def.BuildSeconds*2);
    TestTrue(TEXT("An unfunded second job remains uncommitted"),Loaded.TransportVehicles.Jobs.Num()==1&&!Loaded.TransportVehicles.Jobs[0].Committed);
    TestEqual(TEXT("No free additional vehicle is created"),Loaded.TransportVehicles.LiveCount(),Vehicles+1);
    TestEqual(TEXT("Fabrication never invents operators"),Loaded.Workers.Bodies.Num(),Workers);
    if(!Loaded.Save(Path,Error)||!S.Load(Path,Error)){AddError(TEXT("Strict completed-body save/reload: ")+Error);return false;}
    TestEqual(TEXT("The completed body survives strict reload exactly once"),S.TransportVehicles.LiveCount(),Vehicles+1);
    return true;
}
#endif
