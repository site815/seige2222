#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"

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
    for(int32 I=0;I<20000&&S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Task;});++I){const double Step=S.FixedStepSeconds();S.Time+=Step;S.Workers.Tick(S,Step);S.Calendar.Advance(Step);}
    TestFalse(TEXT("The same carrier physically finishes its full bulk delivery"),S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Task;}));
    TestTrue(TEXT("The paid construction site receives the full load"),FMath::IsNearlyEqual(S.FindBuilding(Site)->ConstructionMaterials.FindRef(Resource),Expected,1.e-8));
    TestTrue(TEXT("Only the actual bulk source is debited"),FMath::IsNearlyEqual(S.FindBuilding(Port)->Inventory.FindRef(Resource),BulkBefore-Expected,1.e-8)&&S.FindBuilding(Sensor)->Inventory.FindRef(Resource)==Buffer);
    TestTrue(TEXT("Moving the accepted bill conserves every material unit"),FMath::IsNearlyEqual(S.TotalStock(Resource),Before,1.e-7));
    TestTrue(TEXT("Source selection uses the same body without a parallel hauling population"),S.Workers.Bodies.Num()==BodyCount&&S.Workers.Find(Carrier)!=nullptr);
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
#endif
