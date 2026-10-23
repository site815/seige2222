#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "AI/SeigeScenarioAI.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
bool LogisticsFixture(FSeigeSimulation& S,int32& Port,int32& Generator,FString& Error,int32* EarlierSensor=nullptr)
{
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false))return false;
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(S.Buildings[0].IsConstructing||!S.Couriers.IsEmpty()){Error=TEXT("Paid deployment must finish before dispatch probe");return false;}
    auto Install=[&](const FString& Def,FVector2D P,int32& Id)
    {
        if(!S.PlaceBuilding(Def,P,Error))return false;auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);
        for(const auto& V:Cost){if(S.Buildings[0].Inventory.FindRef(V.Key)<V.Value){Error=TEXT("Finite kit cannot fund isolated logistics destinations");return false;}S.Buildings[0].Inventory.FindOrAdd(V.Key)-=V.Value;}
        B.InstalledMaterials=Cost;B.ConstructionProgress=1;B.IsConstructing=false;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;Id=B.Id;return true;
    };
    if(EarlierSensor&&!Install(TEXT("sensor"),FVector2D(0,-1300),*EarlierSensor))return false;
    if(!Install(TEXT("trading_port"),FVector2D(1300,0),Port)||!Install(TEXT("fuel_generator"),FVector2D(0,1300),Generator))return false;
    int32 Index=0;
    for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active"))
    {
        W.BuildingId=W.RoadId=W.DeliveryId=W.ContainerId=0;W.ContainerKind.Empty();W.Route.Empty();W.NextWaypoint=0;W.Outdoor=true;W.Position=S.BuildingAccessPoint(S.Buildings[0]);
        if(Index==0){W.Activity=TEXT("operate");W.BuildingId=S.Buildings[0].Id;}
        else if(Index==1)W.Activity=TEXT("idle");
        else if(Index==2){W.Activity=TEXT("operate");W.BuildingId=Port;}
        else if(Index==3){W.Activity=TEXT("operate");W.BuildingId=Generator;}
        else W.Activity=TEXT("return");
        ++Index;
    }
    S.Workers.RefreshMetrics(S);return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeLogisticsPriorityTest,"Seige.Workers.EssentialDispatchAndFiniteTails",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeLogisticsPriorityTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation Base;int32 Port=0,Generator=0;
    if(!LogisticsFixture(Base,Port,Generator,Error)){AddError(Error);return false;}
    const int32 Core=Base.Buildings[0].Id,BodyCount=Base.Workers.Bodies.Num();
    const FString Repair=Base.TextRule(TEXT("repair_resource")),Fuel=Base.Energy.Definition(Base.FindBuilding(Generator)->DefId)->FuelResource;
    const double Buffer=Base.Energy.FuelDemand(Base.FindBuilding(Generator)->DefId,Fuel),RepairBuffer=Base.Number(TEXT("repair_buffer_units"));
    auto Transfer=[&](FSeigeSimulation& S,int32 Target,const FString& Resource,double Amount){S.FindBuilding(Core)->Inventory.FindOrAdd(Resource)-=Amount;S.FindBuilding(Target)->Inventory.FindOrAdd(Resource)+=Amount;};
    Transfer(Base,Port,Repair,40);Transfer(Base,Generator,Repair,RepairBuffer);
    // This already accepted export is a dispatch fixture. Its goods remain in
    // real local inventories and are never escrowed or credited by the probe.
    auto& Shipment=Base.FindBuilding(Port)->Shipment;Shipment.Resource=Repair;Shipment.Quantity=50;Shipment.PriceCredits=Base.TradeQuote(Repair,50,false);
    FSeigeSimulation Essential=Base;const double FuelBefore=Essential.TotalStock(Fuel);
    Essential.StepLogistics(Essential.Number(TEXT("dispatch_interval")));
    if(!TestEqual(TEXT("One physically available body takes exactly one task"),Essential.Couriers.Num(),1))return false;
    TestTrue(TEXT("A later-built empty generator receives fuel before an older port export"),Essential.Couriers[0].TargetId==Generator&&Essential.Couriers[0].Resource==Fuel&&FMath::IsNearlyEqual(Essential.Couriers[0].ReservedAmount,Buffer,1.e-8));
    TestEqual(TEXT("Dispatch leaves fuel at its physical pickup source"),Essential.TotalStock(Fuel),FuelBefore);
    TestEqual(TEXT("Priority creates no new worker"),Essential.Workers.Bodies.Num(),BodyCount);
    const int32 Delivery=Essential.Couriers[0].Id;const FString Carrier=Essential.Couriers[0].WorkerId;
    Essential.StepLogistics(Essential.Number(TEXT("dispatch_interval")));
    TestTrue(TEXT("A later scheduling pass does not preempt the accepted pickup"),Essential.Couriers.Num()==1&&Essential.Couriers[0].Id==Delivery&&Essential.Couriers[0].WorkerId==Carrier);

    FSeigeSimulation Mixed=Base;Transfer(Mixed,Generator,Fuel,Buffer);
    Mixed.StepLogistics(Mixed.Number(TEXT("dispatch_interval")));
    if(!TestEqual(TEXT("The free hauler can serve the finite order once essential buffers are full"),Mixed.Couriers.Num(),1))return false;
    const double Expected=RepairBuffer+Shipment.Quantity-40;
    TestTrue(TEXT("Shared-resource repair buffer and finite export count local stock once"),Mixed.Couriers[0].TargetId==Port&&Mixed.Couriers[0].Resource==Repair&&FMath::IsNearlyEqual(Mixed.Couriers[0].ReservedAmount,Expected,1.e-8));
    for(auto& W:Mixed.Workers.Bodies)if(W.State==TEXT("active")&&W.Activity==TEXT("return")){W.Activity=TEXT("idle");break;}
    Mixed.StepLogistics(Mixed.Number(TEXT("dispatch_interval")));
    TestEqual(TEXT("An additional free body cannot reserve the same cumulative incoming material twice"),Mixed.Couriers.Num(),1);

    FSeigeSimulation Tail=Base;Transfer(Tail,Generator,Fuel,Buffer*.99);Transfer(Tail,Port,Repair,Expected-.25);
    Tail.StepLogistics(Tail.Number(TEXT("dispatch_interval")));
    if(!TestEqual(TEXT("Tiny routine fuel usage leaves the worker available for useful finite work"),Tail.Couriers.Num(),1))return false;
    TestTrue(TEXT("The last fractional finite-order load is never stranded by the refill threshold"),Tail.Couriers[0].TargetId==Port&&FMath::IsNearlyEqual(Tail.Couriers[0].ReservedAmount,.25,1.e-8));

    FSeigeSimulation Low=Base;const double Fraction=Low.Number(TEXT("delivery_refill_trigger_fraction"));Transfer(Low,Generator,Fuel,Buffer*Fraction);
    Low.StepLogistics(Low.Number(TEXT("dispatch_interval")));
    TestTrue(TEXT("The low-water mark refills toward the full authored fuel target"),Low.Couriers.Num()==1&&Low.Couriers[0].TargetId==Generator&&FMath::IsNearlyEqual(Low.Couriers[0].ReservedAmount,Buffer*(1-Fraction),1.e-8));

    FSeigeSimulation Reordered=Base;Reordered.DeliveryPriorities.Swap(Reordered.DeliveryPriorities.Find(TEXT("fuel")),Reordered.DeliveryPriorities.Find(TEXT("trade")));
    Reordered.StepLogistics(Reordered.Number(TEXT("dispatch_interval")));
    TestTrue(TEXT("The validated editable category order controls actual scheduling"),Reordered.Couriers.Num()==1&&Reordered.Couriers[0].TargetId==Port);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeConstructionSourceTest,"Seige.Workers.ConstructionUsesBulkBeforeOperatingBuffers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeConstructionSourceTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;int32 Port=0,Generator=0,Sensor=0;
    if(!LogisticsFixture(S,Port,Generator,Error,&Sensor)){AddError(Error);return false;}
    const int32 CoreId=S.Buildings[0].Id,BodyCount=S.Workers.Bodies.Num();const FString Resource=S.TextRule(TEXT("repair_resource"));
    const double Buffer=S.Number(TEXT("repair_buffer_units"));
    auto Transfer=[&](int32 Id,const FString& R,double N){S.FindBuilding(CoreId)->Inventory.FindOrAdd(R)-=N;S.FindBuilding(Id)->Inventory.FindOrAdd(R)+=N;};
    Transfer(Sensor,Resource,Buffer);Transfer(Generator,Resource,Buffer);
    const FString Fuel=S.Energy.Definition(S.FindBuilding(Generator)->DefId)->FuelResource;Transfer(Generator,Fuel,S.Energy.FuelDemand(S.FindBuilding(Generator)->DefId,Fuel));
    Transfer(Port,Resource,S.FindBuilding(CoreId)->Inventory.FindRef(Resource)-Buffer);
    if(!S.PlaceBuilding(TEXT("sensor"),FVector2D(-1300,0),Error)){AddError(Error);return false;}
    const int32 Site=S.Buildings.Last().Id;const double Before=S.TotalStock(Resource),BulkBefore=S.FindBuilding(Port)->Inventory.FindRef(Resource);
    const double Expected=FMath::Min(S.ConstructionCost(*S.FindBuilding(Site)).FindRef(Resource),S.Workers.HaulUnits(S,Resource));
    S.StepLogistics(S.Number(TEXT("dispatch_interval")));
    if(!TestEqual(TEXT("The one available existing body receives one construction load"),S.Couriers.Num(),1))return false;
    const int32 Task=S.Couriers[0].Id;const FString Carrier=S.Couriers[0].WorkerId;
    TestTrue(TEXT("A full load comes from later bulk storage instead of the older small repair buffer"),S.Couriers[0].SourceId==Port&&S.Couriers[0].TargetId==Site&&S.Couriers[0].ForConstruction&&FMath::IsNearlyEqual(S.Couriers[0].ReservedAmount,Expected,1.e-8));
    TestEqual(TEXT("The old sensor retains its working repair stock"),S.FindBuilding(Sensor)->Inventory.FindRef(Resource),Buffer);
    TestEqual(TEXT("A pickup claim never deducts or grants physical stock"),S.TotalStock(Resource),Before);
    const double Available=S.ConstructionAvailable(Resource);
    TestTrue(TEXT("Accepted construction and its pickup lower free stock below gross cargo"),Available<S.TotalStock(Resource));
    for(double Threshold:{1.,Available,Available+1.})
        TestEqual(TEXT("Threshold queries retain the full stock answer despite accepted bills, pickups and operating buffers"),S.ConstructionAvailable(Resource,Threshold)+1.e-8>=Threshold,Available+1.e-8>=Threshold);
    for(int32 I=0;I<20000&&S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Task;});++I){const double Step=S.FixedStepSeconds();S.Time+=Step;S.Workers.Tick(S,Step);S.Calendar.Advance(Step);}
    TestFalse(TEXT("The same carrier physically finishes its full bulk delivery"),S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Task;}));
    TestTrue(TEXT("The paid construction site receives the full load"),FMath::IsNearlyEqual(S.FindBuilding(Site)->ConstructionMaterials.FindRef(Resource),Expected,1.e-8));
    TestTrue(TEXT("Only the actual bulk source is debited"),FMath::IsNearlyEqual(S.FindBuilding(Port)->Inventory.FindRef(Resource),BulkBefore-Expected,1.e-8)&&S.FindBuilding(Sensor)->Inventory.FindRef(Resource)==Buffer);
    TestTrue(TEXT("Moving the accepted bill conserves every material unit"),FMath::IsNearlyEqual(S.TotalStock(Resource),Before,1.e-7));
    TestTrue(TEXT("Source selection uses the same body without a parallel hauling population"),S.Workers.Bodies.Num()==BodyCount&&S.Workers.Find(Carrier)!=nullptr);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeReachableSupplyTest,"Seige.Workers.ReachableSupplyAfterBlockedNearest",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeReachableSupplyTest::RunTest(const FString&)
{
    FSeigeSimulation Base;FString Error;
    if(!Base.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)){AddError(Error);return false;}
    Base.Environment.Enabled=false;Base.Buildings[0].IsConstructing=false;Base.Buildings[0].ConstructionProgress=1;
    Base.Workers.DeploymentElapsed=Base.Workers.DeploymentDescentSeconds()+Base.Workers.DeploymentHatchSeconds();
    Base.DeploymentHatchOpen=Base.DeploymentGrounded=true;Base.DeliveryPriorities={TEXT("trade")};
    const int32 Core=Base.Buildings[0].Id;const FVector2D Origin=Base.Buildings[0].Position;
    const int32 Port=Base.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(2000,0));
    const int32 Isolated=Base.AddReviewBuilding(TEXT("depot"),Origin+FVector2D(2000,1500));
    if(!TestTrue(TEXT("Reachability fixture has two real supply locations and a trade destination"),Port>0&&Isolated>0&&Base.Workers.Bodies.Num()>=5))return false;
    Base.Mobility.SetTerrainSampler([Origin](FVector2D P){return P.Y>Origin.Y+800.?100000.:0.;});
    const FString Goods=TEXT("alloy");const double Quantity=40,IsolatedQuantity=100;
    if(!TestTrue(TEXT("All fixture cargo comes from the finite landing stock"),Base.FindBuilding(Core)->Inventory.FindRef(Goods)>IsolatedQuantity+Quantity))return false;
    Base.FindBuilding(Core)->Inventory.FindOrAdd(Goods)-=IsolatedQuantity;Base.FindBuilding(Isolated)->Inventory.Add(Goods,IsolatedQuantity);
    auto& Order=Base.FindBuilding(Port)->Shipment;Order.Resource=Goods;Order.Quantity=Quantity;Order.PriceCredits=Base.TradeQuote(Goods,Quantity,false);
    for(int32 I=0;I<Base.Workers.Bodies.Num();++I)
    {
        auto& W=Base.Workers.Bodies[I];const int32 Home=I==3?Isolated:I==4?Port:Core;
        W.Activity=TEXT("operate");W.BuildingId=W.CrewBuildingId=Home;W.RoadId=W.DeliveryId=W.TransportVehicleId=W.ContainerId=0;
        W.ContainerKind.Empty();W.Outdoor=true;W.Position=Base.BuildingAccessPoint(*Base.FindBuilding(Home))+FVector2D(0,20);W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;
    }
    Base.Workers.RefreshMetrics(Base);Base.DeploymentHatchOpen=Base.DeploymentGrounded=true;
    TArray<FVector2D> Route;
    TestFalse(TEXT("Closer stock is separated by an impassable cliff"),Base.FindRoute(Base.BuildingAccessPoint(*Base.FindBuilding(Isolated)),Base.BuildingAccessPoint(*Base.FindBuilding(Port)),Route));
    TestTrue(TEXT("Farther finite supply has a physical route"),Base.FindRoute(Base.BuildingAccessPoint(*Base.FindBuilding(Core)),Base.BuildingAccessPoint(*Base.FindBuilding(Port)),Route));
    for(bool UseTruck:{false,true})
    {
        FSeigeSimulation S=Base;auto& Truck=S.TransportVehicles.Bodies[0];Truck.Embarked=!UseTruck;Truck.Status=UseTruck?TEXT("Available"):TEXT("Aboard shuttle");Truck.Position=S.BuildingAccessPoint(*S.FindBuilding(Core));
        const double TotalBefore=S.TotalStock(Goods),CoreBefore=S.FindBuilding(Core)->Inventory.FindRef(Goods);const int32 BodiesBefore=S.Workers.Bodies.Num();
        TMap<FString,FVector2D> Positions;for(const auto& W:S.Workers.Bodies)Positions.Add(W.Id,W.Position);
        S.StepLogistics(S.Number(TEXT("dispatch_interval")));
        if(!TestEqual(TEXT("Only one real pickup is accepted after skipping the isolated nearest source"),S.Couriers.Num(),1))return false;
        const auto Task=S.Couriers[0];
        TestTrue(TEXT("The ordinary trade load uses farther reachable supply"),Task.SourceId==Core&&Task.TargetId==Port&&Task.Resource==Goods&&!Task.ForConstruction&&Task.Amount==0&&Task.ReservedAmount==Quantity);
        TestEqual(TEXT("Both real worker hauling and autonomous vehicle hauling exercise fallback"),Task.TransportVehicleId!=0,UseTruck);
        TestTrue(TEXT("Rejected source retains every kg and holds no reservation"),S.FindBuilding(Isolated)->Inventory.FindRef(Goods)==IsolatedQuantity&&S.Workers.PickupReserved(S,Isolated,Goods)==0&&S.TotalStock(Goods)==TotalBefore);
        for(const auto& W:S.Workers.Bodies)TestTrue(TEXT("Dispatch does not relocate workers"),W.Position.Equals(Positions[W.Id],1.e-8));
        bool Conserved=true;
        for(int32 I=0;I<20000&&S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Task.Id;});++I)
        {const double Step=S.FixedStepSeconds();S.Time+=Step;S.Workers.Tick(S,Step);S.TransportVehicles.Tick(S,Step);Conserved&=FMath::IsNearlyEqual(S.TotalStock(Goods),TotalBefore,1.e-7);}
        TestFalse(TEXT("The accepted carrier physically completes its delivery"),S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Task.Id;}));
        TestTrue(TEXT("Only the reachable stock is moved to the intended port"),FMath::IsNearlyEqual(S.FindBuilding(Core)->Inventory.FindRef(Goods),CoreBefore-Quantity,1.e-7)&&S.FindBuilding(Port)->Inventory.FindRef(Goods)==Quantity&&S.FindBuilding(Isolated)->Inventory.FindRef(Goods)==IsolatedQuantity);
        TestTrue(TEXT("Source fallback conserves goods throughout actual movement"),Conserved);
        TestEqual(TEXT("Source fallback uses only the existing finite workers"),S.Workers.Bodies.Num(),BodiesBefore);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRawInputBufferTest,"Seige.Workers.RawInputBuffersUsePhysicalLoads",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeRawInputBufferTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation Base;int32 Port=0,Generator=0;
    if(!LogisticsFixture(Base,Port,Generator,Error)){AddError(Error);return false;}
    const int32 Core=Base.Buildings[0].Id;
    if(!Base.PlaceBuilding(TEXT("alloy_refinery"),FVector2D(-1300,0),Error)){AddError(Error);return false;}
    const int32 Refinery=Base.Buildings.Last().Id;auto* B=Base.FindBuilding(Refinery);const auto Bill=Base.ConstructionCost(*B);
    // Isolate dispatch from construction time using a genuinely paid facility.
    // Every tested parcel below comes from the remaining finite landed kit.
    for(const auto& P:Bill){if(Base.FindBuilding(Core)->Inventory.FindRef(P.Key)<P.Value){AddError(TEXT("Finite kit cannot fund the refinery fixture"));return false;}Base.FindBuilding(Core)->Inventory.FindOrAdd(P.Key)-=P.Value;}
    B->InstalledMaterials=Bill;B->ConstructionProgress=1;B->IsConstructing=false;B->Builders=B->BuildersOnSite=B->TravellingBuilders=0;
    auto Transfer=[&](FSeigeSimulation& S,int32 Source,int32 Target,const FString& Resource,double Amount)
    {if(S.FindBuilding(Source)->Inventory.FindRef(Resource)+1.e-8<Amount){AddError(TEXT("The dispatch fixture cannot grant missing stock"));return false;}S.FindBuilding(Source)->Inventory.FindOrAdd(Resource)-=Amount;S.FindBuilding(Target)->Inventory.FindOrAdd(Resource)+=Amount;return true;};
    const FString Repair=Base.TextRule(TEXT("repair_resource"));
    for(int32 Id:{Port,Generator,Refinery})if(!Transfer(Base,Core,Id,Repair,Base.Number(TEXT("repair_buffer_units"))))return false;
    const FString Fuel=Base.Energy.Definition(Base.FindBuilding(Generator)->DefId)->FuelResource;
    if(!Transfer(Base,Core,Generator,Fuel,Base.Energy.FuelDemand(Base.FindBuilding(Generator)->DefId,Fuel)))return false;
    const FString Resource=TEXT("iron_ore"),Recipe=TEXT("smelt_alloy");
    if(!Transfer(Base,Core,Port,Resource,Base.FindBuilding(Core)->Inventory.FindRef(Resource)))return false;
    Base.Workers.RefreshMetrics(Base);const int32 BodyCount=Base.Workers.Bodies.Num();
    const double Haul=Base.Workers.HaulUnits(Base,Resource),Target=Base.ProductionInputBuffer(*B,Recipe,Resource);
    TestEqual(TEXT("The authored ore payload obeys the real forty-kilogram carrier limit"),Haul,40.);
    TestEqual(TEXT("Raw recipe input buffers cover two full physical loads"),Target,2*Haul);
    TestEqual(TEXT("Manufactured recipe inputs retain their existing cycle buffer"),Base.ProductionInputBuffer(Base.Buildings[0],TEXT("make_components"),TEXT("alloy")),2*Base.Number(TEXT("delivery_buffer_cycles")));
    TestEqual(TEXT("Rare raw recipe inputs use the same physical-load policy"),Base.ProductionInputBuffer(Base.Buildings[0],TEXT("make_ai_chips"),TEXT("copper_ore")),2*Base.Workers.HaulUnits(Base,TEXT("copper_ore")));
    TestEqual(TEXT("A resource absent from the recipe gains no artificial demand"),Base.ProductionInputBuffer(*B,Recipe,TEXT("water")),0.);

    FSeigeSimulation S=Base;const double Before=S.TotalStock(Resource),SourceBefore=S.FindBuilding(Port)->Inventory.FindRef(Resource);
    S.StepLogistics(S.Number(TEXT("dispatch_interval")));
    if(!TestEqual(TEXT("Only one existing available body accepts the first full load"),S.Couriers.Num(),1))return false;
    TestTrue(TEXT("The production task reserves a full ore load from real port inventory"),S.Couriers[0].SourceId==Port&&S.Couriers[0].TargetId==Refinery&&S.Couriers[0].Resource==Resource&&S.Couriers[0].ReservedAmount==Haul&&S.Couriers[0].Amount==0);
    const FString FirstBody=S.Couriers[0].WorkerId;const FVector2D FirstPosition=S.Workers.Find(FirstBody)->Position;
    TestEqual(TEXT("Accepting the pickup leaves goods at their actual source"),S.FindBuilding(Port)->Inventory.FindRef(Resource),SourceBefore);
    bool SecondBody=false;for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active")&&W.Activity==TEXT("return")){W.Activity=TEXT("idle");SecondBody=true;break;}
    if(!TestTrue(TEXT("The second load uses another existing finite body"),SecondBody))return false;
    S.StepLogistics(S.Number(TEXT("dispatch_interval")));
    TestTrue(TEXT("Local plus incoming accounting fills the target without duplicate cargo claims"),S.Couriers.Num()==2&&S.Incoming(Refinery,Resource)==Target&&S.Couriers[1].ReservedAmount==Haul&&S.Couriers[1].WorkerId!=FirstBody);
    TestEqual(TEXT("Additional dispatch cannot teleport the accepted carrier"),S.Workers.Find(FirstBody)->Position,FirstPosition);
    for(int32 I=0;I<20000&&!S.Couriers.IsEmpty();++I){const double Step=S.FixedStepSeconds();S.Time+=Step;S.Workers.Tick(S,Step);S.Calendar.Advance(Step);for(const auto& C:S.Couriers)if(C.Amount>S.Workers.HaulUnits(S,C.Resource)+1.e-8){AddError(TEXT("Physical raw delivery exceeded its carrier's mass/volume limit"));return false;}}
    TestTrue(TEXT("Both finite workers physically arrive and unload their real ore"),S.Couriers.IsEmpty()&&FMath::IsNearlyEqual(S.FindBuilding(Refinery)->Inventory.FindRef(Resource),Target,1.e-8));
    TestTrue(TEXT("Physical delivery debits the source once and conserves total material"),FMath::IsNearlyEqual(S.FindBuilding(Port)->Inventory.FindRef(Resource),SourceBefore-Target,1.e-8)&&FMath::IsNearlyEqual(S.TotalStock(Resource),Before,1.e-8));
    TestEqual(TEXT("A larger buffer creates no extra delivery workers"),S.Workers.Bodies.Num(),BodyCount);

    FSeigeSimulation High=Base;const double Trigger=Target*High.Number(TEXT("delivery_refill_trigger_fraction"));
    if(!Transfer(High,Port,Refinery,Resource,Trigger+1))return false;
    High.StepLogistics(High.Number(TEXT("dispatch_interval")));
    TestFalse(TEXT("The existing low-water rule suppresses raw refills above half the larger target"),High.Couriers.ContainsByPredicate([&](const auto& C){return C.TargetId==Refinery&&C.Resource==Resource;}));
    FSeigeSimulation Low=Base;if(!Transfer(Low,Port,Refinery,Resource,Trigger))return false;
    Low.StepLogistics(Low.Number(TEXT("dispatch_interval")));
    TestTrue(TEXT("At the unchanged low-water mark one full load refills toward the target"),Low.Couriers.ContainsByPredicate([&](const auto& C){return C.TargetId==Refinery&&C.Resource==Resource&&FMath::IsNearlyEqual(C.ReservedAmount,Haul,1.e-8);}));
    Base.Resources[Resource].LitresPerUnit=2;
    TestEqual(TEXT("A volume-limited raw input uses the smaller real payload"),Base.ProductionInputBuffer(*B,Recipe,Resource),60.);
    Base.Resources[Resource].Discrete=true;
    TestEqual(TEXT("Discrete cargo is excluded from raw bulk buffers"),Base.ProductionInputBuffer(*B,Recipe,Resource),15.);
    Base.Resources[Resource].Discrete=false;Base.Policy->SetNumberField(TEXT("delivery_raw_input_buffer_loads"),0);
    TestEqual(TEXT("An externally configured zero restores the original recipe-cycle target"),Base.ProductionInputBuffer(*B,Recipe,Resource),15.);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeDispatchEligibilityTest,"Seige.Workers.DispatchEligibilityAndSelfTransfer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeDispatchEligibilityTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;FSeigeScenarioAI Brain;
    if(!Brain.Initialize(S,FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES")),true,Error,false,false)){AddError(Error);return false;}
    const int32 CoreId=S.Buildings[0].Id;
    auto* Port=S.Buildings.FindByPredicate([&](const auto& B){return S.Definition(B)->Role==TEXT("trade");});
    if(!TestNotNull(TEXT("Established scenario supplies a real trade destination"),Port))return false;
    const int32 PortId=Port->Id;const FString Body=S.TextRule(TEXT("inactive_worker_resource"));
    bool KeptCoreOperator=false;TMap<FString,FVector2D> Positions;
    for(auto& W:S.Workers.Bodies)
    {
        Positions.Add(W.Id,W.Position);
        if(W.State!=TEXT("active"))continue;
        if(W.Activity==TEXT("idle"))W.Activity=TEXT("return");
        if(W.BuildingId==CoreId&&W.Activity==TEXT("operate"))
        {if(KeptCoreOperator)W.Activity=TEXT("to_job");else KeptCoreOperator=true;}
    }
    S.Workers.RefreshMetrics(S);
    TestFalse(TEXT("Incoming replacements cannot be lent as arrived core operators"),S.Workers.HasDispatchWorker(S,TEXT("alloy")));
    TestTrue(TEXT("An existing stored body may relocate when all ordinary haulers are busy"),S.Workers.HasDispatchWorker(S,Body));
    FSeigeSimulation Closed=S;Closed.DeploymentHatchOpen=false;
    TestFalse(TEXT("An unopened shuttle cannot dispatch ordinary freight"),Closed.Workers.HasDispatchWorker(Closed,TEXT("alloy")));
    TestFalse(TEXT("An unopened shuttle cannot dispatch a stored worker either"),Closed.Workers.HasDispatchWorker(Closed,Body));
    FSeigeSimulation Borrow=S;
    auto* Incoming=Borrow.Workers.Bodies.FindByPredicate([&](const auto& W){return W.State==TEXT("active")&&W.BuildingId==CoreId&&W.Activity==TEXT("to_job");});
    if(!TestNotNull(TEXT("Fixture retains the actual incoming core replacement"),Incoming))return false;
    Incoming->Activity=TEXT("operate");
    TestTrue(TEXT("A second arrived core operator becomes a legal hauler"),Borrow.Workers.HasDispatchWorker(Borrow,TEXT("alloy")));
    TestTrue(TEXT("Ordinary dispatch can borrow that body"),Borrow.Workers.Dispatch(Borrow,CoreId,PortId,0,TEXT("alloy"),1,false));
    TestFalse(TEXT("Dispatch cannot lend the final arrived core operator"),Borrow.Workers.HasDispatchWorker(Borrow,TEXT("alloy")));
    const int32 Count=S.Workers.Bodies.Num(),Active=S.Population;const double Before=S.TotalStock(Body);
    if(!S.SetPortWorkerTarget(PortId,1,Error)){AddError(Error);return false;}
    S.StepLogistics(S.Number(TEXT("dispatch_interval")));
    int32 PortTasks=0;TSet<FString> Assigned;
    for(const auto& C:S.Couriers)
    {
        if(C.TargetId==PortId)++PortTasks;Assigned.Add(C.WorkerId);
        TestTrue(TEXT("With no hauler, every permitted task relocates its own stored body"),C.SelfTransfer&&C.Resource==Body);
    }
    if(!TestEqual(TEXT("The finite trading target receives exactly one body claim"),PortTasks,1))return false;
    TestEqual(TEXT("Every simultaneous relocation uses a different existing body"),Assigned.Num(),S.Couriers.Num());
    TestEqual(TEXT("Self relocation creates no body"),S.Workers.Bodies.Num(),Count);
    TestEqual(TEXT("Self relocation debits exactly its existing packed bodies"),S.TotalStock(Body),Before-S.Couriers.Num());
    TestEqual(TEXT("Those same bodies become active during relocation"),S.Population,Active+S.Couriers.Num());
    for(const auto& W:S.Workers.Bodies)TestTrue(TEXT("Eligibility and dispatch do not teleport any worker"),W.Position.Equals(Positions[W.Id],1.e-8));
    return true;
}
#endif
