#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAutomaticWorkerReserveTest,"Seige.Workers.AutomaticCommandReserve",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAutomaticWorkerReserveTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;const auto Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));
    if(!S.Initialize(Rules,Error,false,false)){AddError(Error);return false;}
    const int32 Bodies=S.Workers.Bodies.Num(),Active=S.Population;
    TestEqual(TEXT("Landing needs five real spares beyond the five active command workers"),S.WorkerSurplusTarget(),5);
    TestEqual(TEXT("The automatic reserve creates no free stored bodies"),S.Workers.StoredCount(),0);
    TestEqual(TEXT("Assembly demand includes jobs plus spare capacity"),S.WorkersNeeded(),5);
    S.Buildings[0].UpgradeTarget=TEXT("command_core_2");
    TestEqual(TEXT("An unfinished upgrade does not grant its future reserve capacity"),S.WorkerSurplusTarget(),5);
    S.Buildings[0].UpgradeTarget.Empty();
    for(const FString Id:{FString(TEXT("command_core_2")),FString(TEXT("command_core_3"))})
    {
        FSeigeSimulation Upgraded=S;Upgraded.Buildings[0].DefId=Id;Upgraded.CoreDefinition=Id;Upgraded.Workers.RefreshMetrics(Upgraded);
        const int32 Capacity=Upgraded.BuildingDefs[Id].Jobs;
        TestEqual(TEXT("Completed command level supplies its authored spare capacity"),Upgraded.WorkerSurplusTarget(),Capacity);
        TestEqual(TEXT("Upgrading changes demand without inventing identities"),Upgraded.Workers.Bodies.Num(),Bodies);
        TestEqual(TEXT("Upgrading does not pull existing workers into storage"),Upgraded.Population,Active);
        TestEqual(TEXT("Demand covers vacancies plus the new spare reserve"),Upgraded.WorkersNeeded(),Capacity*2-Active);
    }
    FSeigeSimulation Modified=S;Modified.BuildingDefs[Modified.CoreDefinition].Jobs=7;
    TestEqual(TEXT("Reserve derives from external command capacity rather than hardcoded levels"),Modified.WorkerSurplusTarget(),7);
    FSeigeSimulation Trade=S;const int32 Port=Trade.AddReviewBuilding(TEXT("trading_port"),FVector2D(2000,2000));
    if(!Trade.SetPortWorkerTarget(Port,3,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Trade stock does not change command reserve"),Trade.WorkerSurplusTarget(),5);
    TestEqual(TEXT("Trade reserve is additional to the command's protected spares"),Trade.WorkerReserveTarget(),8);
    TestEqual(TEXT("Changing trade stock grants no bodies"),Trade.Workers.Bodies.Num(),Bodies);
    const auto Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Workforce/automatic-reserve.json"));
    if(!S.Save(Path,Error)||!Loaded.Initialize(Rules,Error,false,false)||!Loaded.Load(Path,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Save/load derives the same immutable command reserve"),Loaded.WorkerSurplusTarget(),5);
    FString Raw;TSharedPtr<FJsonObject> Doc;
    if(!FFileHelper::LoadFileToString(Raw,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Doc))return false;
    Doc->SetNumberField(TEXT("worker_surplus_target"),99);Raw.Empty();FJsonSerializer::Serialize(Doc.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw));FFileHelper::SaveStringToFile(Raw,*Path);
    TestFalse(TEXT("A save cannot restore an obsolete manually selected reserve"),Loaded.Load(Path,Error));
    TestEqual(TEXT("Rejected reserve override is atomic"),Loaded.WorkerSurplusTarget(),5);

    // Drive real paid batches against the automatic cap. The elapsed assembly
    // time is supplied to the production subsystem; no worker identity is granted.
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(!TestFalse(TEXT("Finite original workers complete deployment"),S.Buildings[0].IsConstructing))return false;
    for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active")&&!W.DeliveryId&&!W.ChargingBuildingId&&!W.TransportVehicleId)
    {W.CrewBuildingId=W.BuildingId=S.Buildings[0].Id;W.Activity=TEXT("operate");W.RoadId=0;W.Route.Empty();W.NextWaypoint=0;}
    S.Workers.RefreshMetrics(S);auto& Core=S.Buildings[0];const auto Recipe=S.TextRule(TEXT("population_recipe"));
    const auto Inputs=S.ProductionInputs(Core,Recipe);const double BeforeParts=Core.Inventory.FindRef(TEXT("components"))+Core.ProductionInputs.FindRef(TEXT("components"));
    const int32 BeforeBodies=S.Workers.Bodies.Num(),BeforeStored=S.Workers.StoredCount();
    for(int32 I=0;I<S.WorkerSurplusTarget()+1&&S.WorkersNeeded()>0;++I)
    {
        const double Duration=S.ProductionSeconds(Core,Recipe)/FMath::Max(.001,S.WorkFraction(Core));
        S.Energy.Tick(S,Duration);S.StepRecipe(Core,Duration+.001);
    }
    const int32 Made=S.Workers.Bodies.Num()-BeforeBodies;
    TestEqual(TEXT("Paid manufacturing fills exactly the automatic reserve"),S.Workers.StoredCount(),S.WorkerSurplusTarget());
    TestEqual(TEXT("New identities correspond exactly to newly stored bodies"),Made,S.Workers.StoredCount()-BeforeStored);
    TestTrue(TEXT("Manufacturing pays each body's configured components"),FMath::IsNearlyEqual(BeforeParts-Core.Inventory.FindRef(TEXT("components")),Made*Inputs.FindRef(TEXT("components")),1.e-8));
    TestEqual(TEXT("Filling spare storage never removes active workers"),S.Population,Active);
    TestEqual(TEXT("The satisfied target stops automatic manufacturing"),S.WorkersNeeded(),0);
    TestFalse(TEXT("The five protected spares cannot be disassembled"),S.CanDisassembleWorkers(Core.Id,1,Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCargoMovementParityTest,"Seige.Workers.CargoSpeedParity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCargoMovementParityTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation Base;if(!Base.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    Base.Environment.Enabled=false;Base.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    const FVector2D A(4000,4000),B(6000,4000);const double Unit=Base.MetersPerWorldUnit();
    auto WorkerSpeed=[&](bool Loaded,double Grade,bool Grieving)
    {
        FSeigeSimulation S=Base;S.Mobility.SetTerrainSampler([Grade](FVector2D P){return P.X*Grade;});
        if(Grieving){auto& Dog=S.Companions.Dogs[0];Dog.Aboard=false;Dog.Sheltered=false;Dog.Position=A;S.Companions.Damage(S,Dog.Id,1000);}
        auto& W=S.Workers.Bodies[0];W.State=TEXT("active");W.Position=A;W.Outdoor=true;W.Activity=Loaded?TEXT("delivery"):TEXT("to_job");W.Route={B};W.NextWaypoint=0;W.RouteRevision=S.TransportRevision;
        if(Loaded){FSeigeCourier C;C.Id=999;C.WorkerId=W.Id;C.Position=A;C.Resource=TEXT("alloy");C.Amount=S.Workers.HaulUnits(S,C.Resource);C.Phase=TEXT("carrying");S.Couriers.Add(C);W.DeliveryId=C.Id;}
        S.Workers.Move(S,W,B,1.);return FVector2D::Distance(A,W.Position)*Unit*3.6;
    };
    auto VehicleSpeed=[&](bool Loaded,double Grade,double& BatteryCost)
    {
        FSeigeSimulation S=Base;S.Mobility.SetTerrainSampler([Grade](FVector2D P){return P.X*Grade;});
        auto& V=S.TransportVehicles.Bodies[0];V.Embarked=false;V.Position=A;V.Route={B};V.NextWaypoint=0;V.RouteRevision=S.TransportRevision;
        if(Loaded){FSeigeCourier C;C.Id=999;C.TransportVehicleId=V.Id;C.Position=A;C.Resource=TEXT("alloy");C.Amount=S.TransportVehicles.Definition().CargoMassKg;C.Phase=TEXT("carrying");S.Couriers.Add(C);V.DeliveryId=C.Id;}
        const double Before=V.BatteryKWh;S.TransportVehicles.Move(S,V,B,1.);BatteryCost=Before-V.BatteryKWh;return FVector2D::Distance(A,V.Position)*Unit*3.6;
    };
    TestTrue(TEXT("Empty worker physically moves at ten km/h"),FMath::IsNearlyEqual(WorkerSpeed(false,0,false),10.,1.e-8));
    TestTrue(TEXT("Fully laden worker has identical ten km/h movement"),FMath::IsNearlyEqual(WorkerSpeed(true,0,false),10.,1.e-8));
    double EmptyCost=0,LoadedCost=0;
    TestTrue(TEXT("Empty transport physically moves at thirty km/h"),FMath::IsNearlyEqual(VehicleSpeed(false,0,EmptyCost),30.,1.e-8));
    TestTrue(TEXT("Fully laden transport has identical thirty km/h movement"),FMath::IsNearlyEqual(VehicleSpeed(true,0,LoadedCost),30.,1.e-8));
    TestTrue(TEXT("Travel energy remains distance-based with no hidden cargo penalty"),FMath::IsNearlyEqual(EmptyCost,LoadedCost,1.e-10));
    TestTrue(TEXT("Loaded and empty workers share the authored rough-terrain penalty"),FMath::IsNearlyEqual(WorkerSpeed(false,.16,false),8.,1.e-8)&&FMath::IsNearlyEqual(WorkerSpeed(true,.16,false),8.,1.e-8));
    TestTrue(TEXT("Loaded and empty transports share the authored rough-terrain penalty"),FMath::IsNearlyEqual(VehicleSpeed(false,.16,EmptyCost),7.5,1.e-8)&&FMath::IsNearlyEqual(VehicleSpeed(true,.16,LoadedCost),7.5,1.e-8));
    TestTrue(TEXT("Grief halves both loaded and empty worker movement"),FMath::IsNearlyEqual(WorkerSpeed(false,0,true),5.,1.e-8)&&FMath::IsNearlyEqual(WorkerSpeed(true,0,true),5.,1.e-8));
    TestTrue(TEXT("Reported walking speed uses the same mobility authority"),FMath::IsNearlyEqual(Base.WalkingSpeed()*Unit*3.6,10.,1.e-8));
    return true;
}
#endif
