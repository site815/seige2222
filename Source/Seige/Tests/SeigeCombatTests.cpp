#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
bool Start(FSeigeSimulation& S,FString& Error)
{
    if(!S.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false))return false;
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(S.Buildings[0].IsConstructing){Error=TEXT("Combat fixture's finite landed crew could not deploy the command core");return false;}
    S.Tick(20);return true;
}
FString State(const FSeigeCombatSystem& C){auto O=MakeShared<FJsonObject>();C.Save(O);FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));return Text;}
FString PlaceCarrierFixture(FSeigeSimulation& S,int32 BodyIndex,FSeigeCourier Cargo)
{
    // Explicit battle setup repositions an existing body; the cargo marker
    // cannot stand in for a second, unaccounted worker.
    auto& Body=S.Workers.Bodies[BodyIndex];Body.State=TEXT("active");Body.Activity=TEXT("delivery");Body.BuildingId=Body.RoadId=Body.ContainerId=0;
    Body.ContainerKind.Empty();Body.Position=Cargo.Position;Body.Outdoor=true;Body.DeliveryId=Cargo.Id;Body.Route.Empty();Body.NextWaypoint=0;
    Cargo.WorkerId=Body.Id;Cargo.Phase=TEXT("carrying");Cargo.SourceId=Cargo.TargetId=S.Buildings[0].Id;Cargo.ReservedAmount=0;
    S.Couriers.Add(Cargo);S.Workers.RefreshMetrics(S);return Body.Id;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatMountTest,"Seige.Combat.LoadoutAndFiniteFactory",Flags)
bool FSeigeCombatMountTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;if(!Start(S,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Twelve chassis"),S.Combat.Chassis.Num(),12);
    TArray<FString> Mixed={TEXT("kinetic_medium"),TEXT("laser_medium")};for(int I=0;I<8;++I)Mixed.Add(TEXT("laser_small"));
    TestTrue(TEXT("One large area accepts two medium plus eight small"),S.Combat.ValidateLoadout(TEXT("mech_large"),Mixed,Error));Mixed.Add(TEXT("laser_small"));TestFalse(TEXT("Extra frontal area rejected"),S.Combat.ValidateLoadout(TEXT("mech_large"),Mixed,Error));
    TestFalse(TEXT("A small chassis cannot carry a large module"),S.Combat.ValidateLoadout(TEXT("wheeled_small"),{TEXT("laser_large")},Error));
    auto& B=S.Buildings[0];
    // Isolate fabrication from the core's automatic worker recipe without
    // inventing operators or leaving a second batch's escrow in the fixture.
    for(const auto& P:B.ProductionInputs)B.Inventory.FindOrAdd(P.Key)+=P.Value;
    B.ProductionInputs.Empty();B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.ProductionReservedLitres=B.Progress=0;B.Inventory.Empty();
    int32 Station=0;for(auto& Body:S.Workers.Bodies)if(Body.State==TEXT("active"))
    {const bool Operating=Station<S.Definition(B)->Jobs;Body.Activity=Operating?TEXT("operate"):TEXT("idle");Body.BuildingId=Operating?B.Id:0;Body.RoadId=Body.DeliveryId=Body.ContainerId=0;Body.ContainerKind.Empty();Body.Route.Empty();Body.NextWaypoint=0;Body.StationSlot=Station++;Body.Position=S.BuildingAccessPoint(B)+FVector2D(0,20*Station);Body.Outdoor=true;}
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);
    const int32 Before=S.Combat.Vehicles.Num();TestFalse(TEXT("No free chassis without local materials"),S.Combat.QueueVehicle(S,B.Id,TEXT("wheeled_small"),{TEXT("laser_small")},Error));
    TestTrue(TEXT("Failed purchase retains a courier delivery plan"),S.Combat.FabricationPlans.Contains(B.Id));TestTrue(TEXT("Demand includes weapon circuits"),S.Combat.Demand(S,B.Id,TEXT("circuits"))>S.Combat.Chassis[TEXT("wheeled_small")].Cost[TEXT("circuits")]);
    B.Inventory=S.Combat.Chassis[TEXT("wheeled_small")].Cost;for(const auto& P:S.Combat.Weapons[TEXT("laser_small")].Cost)B.Inventory.FindOrAdd(P.Key)+=P.Value;B.BatteryEnergyKWh=20;S.Energy.Tick(S,0);
    const double Mass=S.InventoryMassKg(B.Inventory),Energy=B.BatteryEnergyKWh;
    TestTrue(TEXT("Local bill and stored energy can begin construction"),S.Combat.QueueVehicle(S,B.Id,TEXT("wheeled_small"),{TEXT("laser_small")},Error));
    TestEqual(TEXT("Queueing does not instantly spawn a chassis"),S.Combat.Vehicles.Num(),Before);TestTrue(TEXT("Materials paid once"),S.InventoryMassKg(B.Inventory)<Mass);TestTrue(TEXT("Stored energy paid"),B.BatteryEnergyKWh<Energy);TestEqual(TEXT("One paid job"),S.Combat.Fabrication.Num(),1);
    if(S.Combat.Fabrication.Num()){S.Combat.Tick(S,.05);TestTrue(TEXT("Work advances gradually"),S.Combat.Fabrication[0].ProgressSeconds>0&&S.Combat.Fabrication[0].ProgressSeconds<S.Combat.Fabrication[0].RequiredSeconds);TestTrue(TEXT("Paid work in progress remains in commodity ledger"),S.Combat.CargoStock(TEXT("alloy"))>0);}
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatProjectileTest,"Seige.Combat.ProjectilesProtectionAndPersistence",Flags)
bool FSeigeCombatProjectileTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation A;if(!Start(A,Error)){AddError(Error);return false;}
    for(auto& B:A.Buildings)B.Enabled=false;A.Combat.Vehicles.Empty();
    // A kinetic round crosses a living friendly structure before the intended
    // enemy. Continuous segment collision must hit the obstruction first.
    const auto Core=A.Buildings[0].Position;FSeigeEnemy E;E.Id=8099;E.Position=Core+FVector2D(500,0);E.Health=60;A.Enemies.Add(E);
    FSeigeProjectile P;P.Id=9000;P.OwnerId=8999;P.OwnerKind=TEXT("vehicle");P.TargetEnemyId=E.Id;P.WeaponId=TEXT("kinetic_small");P.Position=P.PreviousPosition=Core-FVector2D(500,0);P.Velocity=FVector2D(350/A.MetersPerWorldUnit(),0);P.RemainingMeters=100;
    A.Combat.Weapons[TEXT("kinetic_small")].ShieldMultiplier=3.;A.Combat.Projectiles.Add(P);const double Shield=A.Combat.BuildingState[A.Buildings[0].Id].Shield;A.Combat.Tick(A,.1);
    TestEqual(TEXT("Swept collision consumed round at foreground building"),A.Combat.Projectiles.Num(),0);TestTrue(TEXT("Unintended friendly shield damage occurred"),A.Combat.BuildingState[A.Buildings[0].Id].Shield<Shield);TestEqual(TEXT("Enemy behind obstruction untouched"),A.Enemies[0].Health,60.);TestTrue(TEXT("Actual projectile weapon multiplier is authoritative"),FMath::Abs(Shield-A.Combat.BuildingState[A.Buildings[0].Id].Shield-3.*A.Combat.Weapons[TEXT("kinetic_small")].Damage)<1.e-8);
    auto& Protection=A.Combat.BuildingState[A.Buildings[0].Id];Protection.Shield=100;Protection.Armor=100;A.Combat.DamageBuilding(A,A.Buildings[0].Id,10,TEXT("kinetic"));const double KineticShield=100-Protection.Shield;
    Protection.Shield=100;A.Combat.DamageBuilding(A,A.Buildings[0].Id,10,TEXT("energy"));TestTrue(TEXT("Kinetics damage shields more than lasers"),KineticShield>100-Protection.Shield);
    Protection.Shield=0;Protection.Armor=100;A.Combat.DamageBuilding(A,A.Buildings[0].Id,10,TEXT("plasma"));const double PlasmaArmor=100-Protection.Armor;Protection.Armor=100;A.Combat.DamageBuilding(A,A.Buildings[0].Id,10,TEXT("kinetic"));TestTrue(TEXT("Plasma damages armor more than kinetics"),PlasmaArmor>100-Protection.Armor);
    // Use a normal source-owned projectile ID for saved-state validation.
    FSeigeSimulation C;if(!Start(C,Error)){AddError(Error);return false;}C.Combat.Vehicles.Empty();auto& State0=C.Combat.BuildingState[C.Buildings[0].Id];State0.Weapons={TEXT("plasma_small")};State0.Cooldowns={0};
    C.PeriodicAttacksEnabled=true;C.TriggerWave();C.Enemies.SetNum(1);C.Enemies[0].Id=8199;C.Enemies[0].Position=C.Buildings[0].Position+FVector2D(1000,0);C.Combat.Tick(C,.05);
    TestTrue(TEXT("Slow plasma remains in flight"),C.Combat.Projectiles.Num()>0);auto Root=MakeShared<FJsonObject>();C.Combat.Save(Root);FSeigeSimulation D=C;
    TestTrue(TEXT("Combat state loads atomically"),D.Combat.Load(Root,D,Error));C.Combat.Tick(C,.2);D.Combat.Tick(D,.2);TestEqual(TEXT("Projectile/RNG continuation is deterministic"),State(C.Combat),State(D.Combat));
    auto Bad=Root->GetObjectField(TEXT("combat"));Bad->SetNumberField(TEXT("next_id"),0);const FString Before=State(D.Combat);TestFalse(TEXT("Malformed ID state rejected"),D.Combat.Load(Root,D,Error));TestEqual(TEXT("Rejected load preserves live state"),State(D.Combat),Before);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatWorldTest,"Seige.Combat.PrivateerTravelCargoAndShuttle",Flags)
bool FSeigeCombatWorldTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation Home,Neighbor;if(!Start(Home,Error)||!Start(Neighbor,Error)){AddError(Error);return false;}
    Home.Buildings[0].IsConstructing=false;Home.Buildings[0].ConstructionProgress=1;Home.Buildings[0].ConstructionMaterials.Empty();
    Neighbor.Buildings[0].Enabled=false;Neighbor.Combat.Vehicles.Empty();
    Home.Combat.SetSectorResolver([&Neighbor](int32 Index)->const FSeigeSimulation*{return Index==5?&Neighbor:nullptr;});
    TestTrue(TEXT("Initial fleet is physically carried aboard parked shuttle"),Home.Combat.Vehicles[0].Embarked);
    const int32 FleetId=Home.Combat.Fleets[0].Id,VehicleId=Home.Combat.Vehicles[0].Id;Home.Combat.Vehicles.SetNum(1);
    auto& Vehicle=Home.Combat.Vehicles[0];Vehicle.Position=FVector2D(Home.WorldHalfSize-1,0);Vehicle.Destination=Vehicle.Position;Vehicle.Weapons.Empty();Vehicle.Cooldowns.Empty();
    const FVector2D Destination(-Home.WorldHalfSize+500,0);const double BeforeBattery=Vehicle.BatteryKWh;
    TestFalse(TEXT("Privateer cannot target its own colony"),Home.Combat.OrderPrivateer(Home,Home,FleetId,4,Destination,Error));
    TestTrue(TEXT("Occupied rally point normalizes to a walkable plot perimeter"),Home.Combat.OrderPrivateer(Home,Neighbor,FleetId,5,Neighbor.Buildings[0].Position,Error));TestTrue(TEXT("Rally point is outside actual reserved plot"),FMath::Max(FMath::Abs(Home.Combat.Fleets[0].Destination.X),FMath::Abs(Home.Combat.Fleets[0].Destination.Y))>Neighbor.Definition(Neighbor.Buildings[0])->ReservedFootprint);
    TestTrue(TEXT("A real neighbor accepts a privateer order"),Home.Combat.OrderPrivateer(Home,Neighbor,FleetId,5,Destination,Error));
    TestFalse(TEXT("Order deploys carried vehicles"),Vehicle.Embarked);TestEqual(TEXT("Order does not teleport vehicle"),Vehicle.SectorIndex,4);
    FSeigeCourier Cargo;Cargo.Id=98765;Cargo.Position=FVector2D(-Home.WorldHalfSize+90,0);Cargo.Resource=TEXT("circuits");Cargo.Amount=10;const FString CarrierId=PlaceCarrierFixture(Neighbor,0,Cargo);
    Home.Combat.Tick(Home,.05);Home.Combat.TickExternalSector(Home,Neighbor,5,.05);
    TestEqual(TEXT("Real movement crosses adjacent sector boundary"),Vehicle.SectorIndex,5);TestTrue(TEXT("Travel consumes finite battery"),Vehicle.BatteryKWh<BeforeBattery);
    const double Captured=Vehicle.Inventory.FindRef(TEXT("circuits"));TestTrue(TEXT("Nearby courier cargo can be captured"),Captured>0);
    TestTrue(TEXT("Capture conserves physical commodity quantity"),FMath::Abs(Captured+Neighbor.Couriers[0].Amount-10)<1.e-8);
    TestEqual(TEXT("Cargo theft does not manufacture or destroy its living carrier"),Neighbor.Workers.Find(CarrierId)->State,FString(TEXT("active")));
    auto Saved=MakeShared<FJsonObject>();Home.Combat.Save(Saved);FSeigeSimulation Restored=Home;
    TestTrue(TEXT("Foreign position and captured cargo load"),Restored.Combat.Load(Saved,Restored,Error));TestEqual(TEXT("Foreign state preserved exactly"),State(Home.Combat),State(Restored.Combat));
    TestFalse(TEXT("Cannot board a fleet still deployed abroad"),Home.Combat.BoardFleet(Home,FleetId,Error));
    const FVector2D Dock=Home.BuildingAccessPoint(Home.Buildings[0]);const double HomeStock=Home.Buildings[0].Inventory.FindRef(TEXT("circuits"));
    TestTrue(TEXT("Return is an ordinary home defense order"),Home.Combat.OrderFleet(Home,FleetId,TEXT("defense"),Dock,0,Error));
    TestEqual(TEXT("Return order does not teleport"),Vehicle.SectorIndex,5);
    Home.Combat.Save(Saved);Restored=Home;TestTrue(TEXT("A new order can be saved before another movement tick"),Restored.Combat.Load(Saved,Restored,Error));
    // The physical river adds a legal detour around its source. Budget this
    // fixture by sector distance and authored vehicle speed, not the former
    // flat-world500s straight-line assumption; movement and battery stay real.
    const double ReturnSeconds=4*Home.WorldHalfSize*Home.MetersPerWorldUnit()/(Home.Combat.Chassis[Vehicle.ChassisId].SpeedKmh/3.6)+30;
    for(double Elapsed=0;Elapsed<ReturnSeconds&&(Vehicle.SectorIndex!=4||Vehicle.ReturningCargo);Elapsed+=.05)Home.Combat.Tick(Home,.05);
    if(Vehicle.ReturningCargo)AddInfo(FString::Printf(TEXT("Return fixture still pending: %s position=(%.2f,%.2f), battery=%.3f, waypoints=%d/%d"),*Vehicle.Status,Vehicle.Position.X,Vehicle.Position.Y,Vehicle.BatteryKWh,Vehicle.NextWaypoint,Vehicle.Route.Num()));
    TestEqual(TEXT("Surviving unit returns physically"),Vehicle.SectorIndex,4);TestFalse(TEXT("Captured cargo unloaded at local service port"),Vehicle.ReturningCargo);
    TestTrue(TEXT("Captured goods reach home stock"),Home.Buildings[0].Inventory.FindRef(TEXT("circuits"))>=HomeStock+Captured-1.e-8);
    TestTrue(TEXT("Returned fleet can board"),Home.Combat.BoardFleet(Home,FleetId,Error));
    Home.Combat.FindVehicle(VehicleId)->Inventory.Add(TEXT("circuits"),1);Home.Combat.EvacuateShuttle();Home.Escaped=true;
    TestTrue(TEXT("Only boarded vehicle evacuates"),Home.Combat.FindVehicle(VehicleId)->Evacuated);TestEqual(TEXT("Evacuated cargo is separate from colony ledger"),Home.Combat.CargoStock(TEXT("circuits")),0.);TestEqual(TEXT("Shuttle fleet manifest retains physical cargo"),Home.Combat.ShuttleFleetCargoStock(TEXT("circuits")),1.);
    Home.Combat.Save(Saved);Restored=Home;TestTrue(TEXT("Evacuated manifest survives loading"),Restored.Combat.Load(Saved,Restored,Error));
    FSeigeSimulation Transit;if(!Start(Transit,Error)){AddError(Error);return false;}Transit.Combat.SetSectorResolver([&Neighbor](int32 Index)->const FSeigeSimulation*{return Index==5?&Neighbor:nullptr;});
    auto& Returning=Transit.Combat.Vehicles[0];Returning.Embarked=false;Returning.SectorIndex=5;Returning.Position=FVector2D(Neighbor.Definition(Neighbor.Buildings[0])->ReservedFootprint+1000,0);Returning.Destination=Returning.Position;Returning.Weapons.Empty();Returning.Cooldowns.Empty();
    TestTrue(TEXT("Foreign return order accepted"),Transit.Combat.OrderFleet(Transit,Returning.FleetId,TEXT("defense"),Transit.BuildingAccessPoint(Transit.Buildings[0]),0,Error));
    bool EnteredPlot=false;const double Plot=Neighbor.Definition(Neighbor.Buildings[0])->ReservedFootprint;
    for(int I=0;I<1500;++I){Transit.Combat.Tick(Transit,.05);if(Returning.SectorIndex==5&&FMath::Abs(Returning.Position.X)<Plot-.001&&FMath::Abs(Returning.Position.Y)<Plot-.001)EnteredPlot=true;}
    TestFalse(TEXT("Returning fleet never phases through foreign building plot"),EnteredPlot);TestTrue(TEXT("Return route actually passes the obstruction"),Returning.Position.X<-Plot);

    // Isolate a finite storage berth shared by a returning vehicle and one
    // real carrier. Both manifests come from existing local alloy, so no
    // capacity rejection or delivery may change the colony's total stock.
    FSeigeSimulation Capacity;if(!Start(Capacity,Error)){AddError(Error);return false;}
    auto& Store=Capacity.Buildings[0];auto& Hauler=Capacity.Combat.Vehicles[0];
    Hauler.Embarked=false;Hauler.FleetId=0;Hauler.Position=Hauler.Destination=Capacity.BuildingAccessPoint(Store);Hauler.Weapons.Empty();Hauler.Cooldowns.Empty();
    const double TotalAlloy=Capacity.TotalStock(TEXT("alloy"));
    if(!TestTrue(TEXT("Vehicle loads existing physical alloy"),Capacity.Combat.TransferCargo(Capacity,Hauler.Id,Store.Id,TEXT("alloy"),1,true,Error)))return false;
    const double VehicleAlloy=Hauler.Inventory.FindRef(TEXT("alloy"));
    const int32 BodyIndex=Capacity.Workers.Bodies.IndexOfByPredicate([](const auto& W){return W.State==TEXT("active")&&W.DeliveryId==0;});
    if(!TestTrue(TEXT("Capacity fixture has an existing unassigned-to-cargo body"),BodyIndex!=INDEX_NONE))return false;
    Store.Inventory.FindOrAdd(TEXT("alloy"))-=1;
    FSeigeCourier Incoming;Incoming.Id=98766;Incoming.Position=Hauler.Position;Incoming.Resource=TEXT("alloy");Incoming.Amount=1;
    PlaceCarrierFixture(Capacity,BodyIndex,Incoming);
    Capacity.BuildingDefs[Store.DefId].StorageCapacity=Capacity.StorageUsed(Store)+Capacity.Resources[TEXT("alloy")].LitresPerUnit;
    const double StoreBefore=Store.Inventory.FindRef(TEXT("alloy"));
    TestFalse(TEXT("Manual vehicle unload cannot consume a promised courier berth"),Capacity.Combat.TransferCargo(Capacity,Hauler.Id,Store.Id,TEXT("alloy"),1,false,Error));
    Hauler.ReturningCargo=true;Capacity.Combat.Tick(Capacity,.05);
    TestEqual(TEXT("Automatic vehicle unload preserves the incoming berth"),Hauler.Inventory.FindRef(TEXT("alloy")),VehicleAlloy);
    TestEqual(TEXT("Rejected unloading leaves local stock unchanged"),Store.Inventory.FindRef(TEXT("alloy")),StoreBefore);
    TestTrue(TEXT("Capacity rejection conserves local, carried and vehicle alloy"),FMath::IsNearlyEqual(Capacity.TotalStock(TEXT("alloy")),TotalAlloy,1.e-8));
    for(int32 I=0;I<40&&Capacity.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Incoming.Id;});++I)Capacity.Workers.Tick(Capacity,.1);
    TestFalse(TEXT("The reserved carrier unloads rather than deadlocking on its own claim"),Capacity.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Incoming.Id;}));
    TestEqual(TEXT("Incoming goods arrive exactly once"),Store.Inventory.FindRef(TEXT("alloy")),StoreBefore+1);
    Capacity.Combat.Tick(Capacity,.05);
    TestEqual(TEXT("Vehicle still waits while arrived cargo occupies the berth"),Hauler.Inventory.FindRef(TEXT("alloy")),VehicleAlloy);
    if(!TestTrue(TEXT("Loading local stock physically frees a berth"),Capacity.Combat.TransferCargo(Capacity,Hauler.Id,Store.Id,TEXT("alloy"),1,true,Error)))return false;
    Capacity.Combat.Tick(Capacity,.05);
    TestTrue(TEXT("Automatic unloading fills only the newly freed berth"),FMath::IsNearlyEqual(Hauler.Inventory.FindRef(TEXT("alloy")),VehicleAlloy,1.e-8));
    TestTrue(TEXT("Actual local occupancy never exceeds capacity"),Capacity.StorageUsed(Store)<=Capacity.Definition(Store)->StorageCapacity+1.e-8);
    TestTrue(TEXT("Arrival and subsequent unloading conserve total alloy"),FMath::IsNearlyEqual(Capacity.TotalStock(TEXT("alloy")),TotalAlloy,1.e-8));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatLootPriorityTest,"Seige.Combat.LootTierScarcityAndPersistence",Flags)
bool FSeigeCombatLootPriorityTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation BaseHome,BaseNeighbor;
    auto Prepare=[&](const FString& Directory)
    {
        if(!BaseHome.Initialize(Directory,Error,false,false)||!BaseNeighbor.Initialize(Directory,Error,false,false)){AddError(Error);return false;}
        for(auto* S:{&BaseHome,&BaseNeighbor})
        {auto& B=S->Buildings[0];B.IsConstructing=false;B.ConstructionProgress=1;B.ConstructionMaterials.Empty();B.Inventory.Empty();B.Enabled=false;}
        BaseNeighbor.Combat.Vehicles.Empty();BaseNeighbor.Combat.BuildingState[BaseNeighbor.Buildings[0].Id].Shield=0;
        BaseHome.Combat.Vehicles.SetNum(1);auto& Vehicle=BaseHome.Combat.Vehicles[0];
        Vehicle.Position=BaseNeighbor.BuildingAccessPoint(BaseNeighbor.Buildings[0]);Vehicle.Destination=Vehicle.Position;Vehicle.SectorIndex=5;Vehicle.Inventory.Empty();Vehicle.Weapons.Empty();Vehicle.Cooldowns.Empty();
        if(!BaseHome.Combat.OrderPrivateer(BaseHome,BaseNeighbor,Vehicle.FleetId,5,Vehicle.Position,Error)){AddError(Error);return false;}
        BaseHome.Combat.Fleets[0].Aggression=TEXT("passive");return true;
    };
    const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));if(!Prepare(Rules))return false;
    auto Check=[&](const TCHAR* Label,const TMap<FString,double>& Owned,const TMap<FString,double>& Available,const FString& Expected,bool InboundComponents=false)
    {
        FSeigeSimulation Home=BaseHome,Neighbor=BaseNeighbor;Home.Buildings[0].Inventory=Owned;Neighbor.Buildings[0].Inventory=Available;
        if(InboundComponents){FSeigeCourier Courier;Courier.Id=98765;Courier.Resource=TEXT("components");Courier.Amount=20;Home.Couriers.Add(Courier);}
        Home.Combat.TickExternalSector(Home,Neighbor,5,.025);
        const auto& Cargo=Home.Combat.Vehicles[0].Inventory;const double Captured=Cargo.FindRef(Expected);
        TestTrue(FString(Label)+TEXT(": preferred cargo receives the finite fractional loading budget"),Captured>0&&Captured<1);
        for(const auto& P:Available)
        {
            TestTrue(FString(Label)+TEXT(": physical source and vehicle cargo are conserved for ")+P.Key,FMath::Abs(Neighbor.Buildings[0].Inventory.FindRef(P.Key)+Cargo.FindRef(P.Key)-P.Value)<1.e-8);
            if(P.Key!=Expected)TestEqual(FString(Label)+TEXT(": lower-priority or indivisible cargo stays aboard source ")+P.Key,Cargo.FindRef(P.Key),0.);
        }
        auto Saved=MakeShared<FJsonObject>();Home.Combat.Save(Saved);FSeigeSimulation Restored=Home,RestoredNeighbor=Neighbor;
        if(!Restored.Combat.Load(Saved,Restored,Error)){AddError(Error);return;}
        TestEqual(FString(Label)+TEXT(": fractional cargo and loading credit survive persistence"),State(Restored.Combat),State(Home.Combat));
        Home.Combat.TickExternalSector(Home,Neighbor,5,.025);Restored.Combat.TickExternalSector(Restored,RestoredNeighbor,5,.025);
        TestEqual(FString(Label)+TEXT(": resumed loading is deterministic"),State(Restored.Combat),State(Home.Combat));
        for(const auto& P:Available)TestEqual(FString(Label)+TEXT(": resumed source inventory is deterministic ")+P.Key,RestoredNeighbor.Buildings[0].Inventory.FindRef(P.Key),Neighbor.Buildings[0].Inventory.FindRef(P.Key));
    };
    Check(TEXT("Tier before scarcity"),{{TEXT("components"),100}},{{TEXT("alloy"),10},{TEXT("components"),10}},TEXT("components"));
    Check(TEXT("Scarcity before resource ID"),{{TEXT("batteries"),10}},{{TEXT("batteries"),10},{TEXT("components"),10}},TEXT("components"));
    Check(TEXT("Home stock includes physical inbound cargo"),{{TEXT("batteries"),10}},{{TEXT("batteries"),10},{TEXT("components"),10}},TEXT("batteries"),true);
    Check(TEXT("Equal tier and quantity use stable resource ID"),{},{{TEXT("components"),10},{TEXT("batteries"),10}},TEXT("batteries"));
    Check(TEXT("Workers cannot be fractionally captured"),{{TEXT("components"),10}},{{TEXT("stored_workers"),2},{TEXT("components"),10}},TEXT("components"));
    // Exercise the authored policy through the real loader, not a private
    // comparator override. Only this test's isolated rules copy is modified.
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/CombatLootRules"));IFileManager::Get().MakeDirectory(*Directory,true);
    TArray<FString> Files;IFileManager::Get().FindFiles(Files,*FPaths::Combine(Rules,TEXT("*.json")),true,false);
    for(const auto& File:Files)if(IFileManager::Get().Copy(*FPaths::Combine(Directory,File),*FPaths::Combine(Rules,File))!=COPY_OK){AddError(TEXT("Cannot copy isolated loot rules"));return false;}
    const FString Path=FPaths::Combine(Directory,TEXT("combat.json"));FString Raw;TSharedPtr<FJsonObject> Config;
    if(!FFileHelper::LoadFileToString(Raw,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Config)){AddError(TEXT("Cannot read isolated combat rules"));return false;}
    auto WriteCriteria=[&](const TArray<FString>& Criteria)
    {TArray<TSharedPtr<FJsonValue>> Values;for(const auto& Value:Criteria)Values.Add(MakeShared<FJsonValueString>(Value));Config->SetArrayField(TEXT("loot_priority"),Values);FString Text;return FJsonSerializer::Serialize(Config,TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*Path);};
    if(!WriteCriteria({TEXT("owned_quantity_asc"),TEXT("tier_desc"),TEXT("resource_id_asc")})||!Prepare(Directory))return false;
    Check(TEXT("External ordering can prioritize scarcity before tier"),{{TEXT("components"),100}},{{TEXT("alloy"),10},{TEXT("components"),10}},TEXT("alloy"));
    FSeigeSimulation Invalid;
    if(!WriteCriteria({TEXT("tier_desc"),TEXT("tier_desc"),TEXT("resource_id_asc")}))return false;
    TestFalse(TEXT("Duplicate loot criteria are rejected"),Invalid.Initialize(Directory,Error,false,false));
    if(!WriteCriteria({TEXT("unknown_priority"),TEXT("owned_quantity_asc"),TEXT("resource_id_asc")}))return false;
    TestFalse(TEXT("Unknown loot criteria are rejected"),Invalid.Initialize(Directory,Error,false,false));
    if(!WriteCriteria({TEXT("tier_desc"),TEXT("owned_quantity_asc")}))return false;
    TestFalse(TEXT("Missing deterministic tie criterion is rejected"),Invalid.Initialize(Directory,Error,false,false));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatShotCostTest,"Seige.Combat.ShotCostsSplashAndRefitDemand",Flags)
bool FSeigeCombatShotCostTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;if(!Start(S,Error)){AddError(Error);return false;}S.Combat.Vehicles.Empty();
    auto& Core=S.Buildings[0];auto& Mount=S.Combat.BuildingState[Core.Id];Mount.Weapons={TEXT("kinetic_small")};Mount.Cooldowns={0};
    const auto& Weapon=S.Combat.Weapons[TEXT("kinetic_small")];Core.Inventory.Remove(Weapon.Ammo);const double Battery=Core.BatteryEnergyKWh;
    FSeigeEnemy E;E.Id=8099;E.Position=Core.Position+FVector2D(1000,0);E.Health=100;S.Enemies.Add(E);
    TestEqual(TEXT("Fire-control reports the same missing-ammunition gate used for firing"),S.Combat.BuildingFireStatus(S,Core.Id),FString(TEXT("Waiting for local ammunition")));
    S.Combat.Tick(S,.05);TestEqual(TEXT("No ammunition means no kinetic shot"),S.Combat.ShotsFired,0.);TestEqual(TEXT("An unfired shot spends no electricity"),Core.BatteryEnergyKWh,Battery);
    Core.Inventory.Add(Weapon.Ammo,Weapon.AmmoPerShot);S.Combat.Tick(S,.05);
    TestEqual(TEXT("One paid shot fired"),S.Combat.ShotsFired,1.);TestEqual(TEXT("Physical ammo consumed once"),Core.Inventory.FindRef(Weapon.Ammo),0.);
    TestTrue(TEXT("Shot electricity deducted from grid battery"),FMath::Abs(Core.BatteryEnergyKWh-(Battery-Weapon.EnergyKWh))<1.e-8);
    Core.Enabled=false;TestEqual(TEXT("A disabled weapon platform reports its actual operating gate"),S.Combat.BuildingFireStatus(S,Core.Id),FString(TEXT("Disabled")));Core.Enabled=true;
    TestTrue(TEXT("Refit plan creates real module-part demand"),S.Combat.SetRefitPlan(S,Core.Id,{TEXT("plasma_large")},Error));TestTrue(TEXT("Refit delivery request includes circuits"),S.Combat.Demand(S,Core.Id,TEXT("circuits"))>=S.Combat.Weapons[TEXT("plasma_large")].Cost[TEXT("circuits")]);
    auto Saved=MakeShared<FJsonObject>();S.Combat.Save(Saved);FSeigeSimulation Copy=S;TestTrue(TEXT("Pending refit deliveries persist"),Copy.Combat.Load(Saved,Copy,Error));TestEqual(TEXT("Refit plan state matches"),State(S.Combat),State(Copy.Combat));
    FSeigeSimulation Splash;if(!Start(Splash,Error)){AddError(Error);return false;}Splash.Buildings[0].Enabled=false;Splash.Combat.Vehicles.Empty();
    E.Position=FVector2D(2000,0);E.Health=100;Splash.Enemies.Add(E);E.Id=8199;E.Position=FVector2D(2010,0);Splash.Enemies.Add(E);
    FSeigeProjectile P;P.Id=9900;P.OwnerId=9901;P.OwnerKind=TEXT("vehicle");P.WeaponId=TEXT("missile_small");P.Position=P.PreviousPosition=FVector2D(1000,0);P.Velocity=FVector2D(Splash.Combat.Weapons[P.WeaponId].SpeedMetersSecond/Splash.MetersPerWorldUnit(),0);P.RemainingMeters=100;Splash.Combat.Projectiles.Add(P);Splash.Combat.Tick(Splash,1);
    TestTrue(TEXT("Missile damages direct collision target"),Splash.Enemies[0].Health<100);TestTrue(TEXT("Splash also damages nearby unintended target"),Splash.Enemies[1].Health<100);TestTrue(TEXT("Splash attenuates with actual distance"),Splash.Enemies[1].Health>Splash.Enemies[0].Health);
    FSeigeSimulation Victim;if(!Start(Victim,Error)){AddError(Error);return false;}Victim.Buildings[0].Enabled=false;Victim.Combat.Vehicles.Empty();
    FSeigeCourier Courier;Courier.Id=90100;Courier.Resource=TEXT("alloy");Courier.Amount=1;Courier.Position=FVector2D(2000,0);const FString FirstCarrier=PlaceCarrierFixture(Victim,0,Courier);Courier.Id=90101;Courier.Position=FVector2D(2010,0);const FString SecondCarrier=PlaceCarrierFixture(Victim,1,Courier);const int32 PopulationBeforeSplash=Victim.Population;
    P.SectorIndex=5;Splash.Combat.Projectiles.Add(P);Splash.Combat.TickExternalSector(Splash,Victim,5,1);
    TestEqual(TEXT("Missile splash also destroys struck and nearby courier cargo"),Victim.Couriers.Num(),0);
    TestEqual(TEXT("Direct hit destroys the actual carrier identity"),Victim.Workers.Find(FirstCarrier)->State,FString(TEXT("destroyed")));
    TestEqual(TEXT("Splash destroys the second actual carrier identity"),Victim.Workers.Find(SecondCarrier)->State,FString(TEXT("destroyed")));
    TestEqual(TEXT("Killed carriers leave the active workforce exactly once"),Victim.Population,PopulationBeforeSplash-2);

    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatFriendlyFireTest,"Seige.Combat.AutonomousClearFire",Flags)
bool FSeigeCombatFriendlyFireTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;if(!Start(S,Error)){AddError(Error);return false;}S.Combat.Vehicles.Empty();
    FSeigeBuilding Solar;Solar.Id=9000;Solar.DefId=TEXT("solar_array");Solar.Position=FVector2D(1000,0);Solar.Health=S.BuildingDefs[Solar.DefId].Health;Solar.Enabled=false;S.Buildings.Add(Solar);
    auto& State0=S.Combat.BuildingState[S.Buildings[0].Id];State0.Weapons={TEXT("laser_large"),TEXT("laser_large")};State0.Cooldowns={1000,0};S.Combat.Weapons[TEXT("laser_large")].AccuracyDegrees=0;
    FSeigeEnemy E;E.Id=8099;E.Position=FVector2D(2000,0);E.Health=100;S.Enemies.Add(E);
    TestTrue(TEXT("Fire-control dossier explains a friendly blocker"),S.Combat.BuildingFireStatus(S,S.Buildings[0].Id).Contains(TEXT("blocks the shot")));
    const double Energy=S.Buildings[0].BatteryEnergyKWh;S.Combat.Tick(S,.05);
    TestEqual(TEXT("Autonomous core holds fire through friendly solar"),S.Combat.ShotsFired,0.);TestEqual(TEXT("No deliberate friendly damage"),S.Buildings[1].Health,Solar.Health);TestEqual(TEXT("Holding fire costs no shot energy"),S.Buildings[0].BatteryEnergyKWh,Energy);
    E.Id=8199;E.Position=FVector2D(0,2200);S.Enemies.Add(E);
    TestEqual(TEXT("An alternative clear target makes the shared fire-control query ready"),S.Combat.BuildingFireStatus(S,S.Buildings[0].Id),FString(TEXT("Ready to fire")));S.Combat.Tick(S,.05);
    TestEqual(TEXT("Core chooses farther unobstructed target"),S.Combat.ShotsFired,1.);TestEqual(TEXT("Blocked enemy remains untouched"),S.Enemies[0].Health,100.);TestTrue(TEXT("Clear target receives real laser hit"),S.Enemies[1].Health<100);TestEqual(TEXT("Solar survives alternative target acquisition"),S.Buildings[1].Health,Solar.Health);
    TestEqual(TEXT("A paid hitscan has one transient beam event"),S.Combat.ShotEvents.Num(),1);
    if(S.Combat.ShotEvents.Num()==1)
    {
        const auto& Shot=S.Combat.ShotEvents[0];TestEqual(TEXT("Beam identifies the actual weapon"),Shot.WeaponId,FString(TEXT("laser_large")));
        TestEqual(TEXT("Beam identifies the ready second socket, not the first weapon of its family"),Shot.WeaponSlot,1);
        TestEqual(TEXT("Local beam retains owner sector"),Shot.OwnerSector,4);
        TestTrue(TEXT("Presentation metadata leaves simulation start at the authoritative building position"),Shot.Start.Equals(S.Buildings[0].Position));
    }
    FSeigeSimulation Mobile;if(!Start(Mobile,Error)){AddError(Error);return false;}Mobile.Buildings[0].Enabled=false;Mobile.Combat.Vehicles.SetNum(1);auto& Shooter=Mobile.Combat.Vehicles[0];Shooter.Embarked=false;Shooter.Position=FVector2D(1000,0);Mobile.Combat.Fleets[0].Destination=Shooter.Position;Solar.Position=FVector2D(1800,0);Mobile.Buildings.Add(Solar);
    E.Id=8099;E.Position=FVector2D(2400,0);E.Health=100;Mobile.Enemies.Add(E);Mobile.Combat.Tick(Mobile,.05);TestEqual(TEXT("Vehicle holds fire through friendly solar"),Mobile.Combat.ShotsFired,0.);
    FSeigeSimulation Flank=Mobile;const FVector2D BeforeFlank=Flank.Combat.Vehicles[0].Position;
    for(int I=0;I<300&&Flank.Combat.ShotsFired==0;++I)Flank.Combat.Tick(Flank,.05);
    TestTrue(TEXT("Defensive guard physically routes around obstructing plot"),FVector2D::Distance(BeforeFlank,Flank.Combat.Vehicles[0].Position)>Flank.BuildingDefs[TEXT("solar_array")].Footprint);
    TestTrue(TEXT("Guard reaches a clear firing position instead of idling"),Flank.Combat.ShotsFired>0);TestEqual(TEXT("Flanking does not shoot the protected solar array"),Flank.Buildings[1].Health,Solar.Health);
    FSeigeSimulation VehicleCover=Mobile;VehicleCover.Buildings.RemoveAt(1);VehicleCover.Combat.Vehicles[0].Position=FVector2D(1000,0);VehicleCover.Combat.Vehicles[0].Route.Empty();VehicleCover.Combat.Vehicles[0].NextWaypoint=0;
    for(const auto& WeaponId:VehicleCover.Combat.Vehicles[0].Weapons)VehicleCover.Combat.Weapons[WeaponId].AccuracyDegrees=0;
    FSeigeVehicle Ally=VehicleCover.Combat.Vehicles[0];Ally.Id=9900;Ally.FleetId=0;Ally.Position=FVector2D(1600,0);Ally.Weapons.Empty();Ally.Cooldowns.Empty();Ally.Route.Empty();Ally.BatteryKWh=0;VehicleCover.Combat.Vehicles.Add(Ally);
    const double AllyHealth=Ally.Health,AllyShield=Ally.Shield;
    for(int32 I=0;I<300&&VehicleCover.Combat.ShotsFired==0;++I)VehicleCover.Combat.Tick(VehicleCover,.05);
    TestTrue(TEXT("Defensive guard also sidesteps a stationary friendly vehicle"),VehicleCover.Combat.ShotsFired>0);
    TestEqual(TEXT("A friendly vehicle remains a real shot blocker during flanking"),VehicleCover.Combat.Vehicles[1].Health,AllyHealth);
    TestEqual(TEXT("Flanking never spends the ally's shield"),VehicleCover.Combat.Vehicles[1].Shield,AllyShield);
    FSeigeSimulation Passive=Mobile;Passive.Combat.Fleets[0].Destination=Passive.Combat.Vehicles[0].Position;Passive.Combat.Vehicles[0].Route.Empty();Passive.Combat.Vehicles[0].NextWaypoint=0;const FVector2D PassiveStart=Passive.Combat.Vehicles[0].Position;Passive.Combat.SetAggression(Passive.Combat.Fleets[0].Id,TEXT("passive"),Error);
    for(int I=0;I<20;++I)Passive.Combat.Tick(Passive,.05);TestTrue(TEXT("Passive guard does not flank or pursue threats"),Passive.Combat.Vehicles[0].Position.Equals(PassiveStart,.001));TestEqual(TEXT("Passive guard holds weapons"),Passive.Combat.ShotsFired,0.);
    E.Id=8199;E.Position=FVector2D(1000,1500);Mobile.Enemies.Add(E);Mobile.Combat.Tick(Mobile,.05);TestEqual(TEXT("Vehicle chooses a clear alternative"),Mobile.Combat.ShotsFired,1.);TestEqual(TEXT("Vehicle preserves intervening friendly structure"),Mobile.Buildings[1].Health,Solar.Health);
    // A foreign defender uses the same clear-fire rule against raiders.
    FSeigeSimulation Raider,Defender;if(!Start(Raider,Error)||!Start(Defender,Error)){AddError(Error);return false;}Defender.Combat.Vehicles.Empty();Solar.Position=FVector2D(1000,0);Defender.Buildings.Add(Solar);
    auto& DefenderMount=Defender.Combat.BuildingState[Defender.Buildings[0].Id];DefenderMount.Weapons={TEXT("laser_large")};DefenderMount.Cooldowns={0};Defender.Combat.Weapons[TEXT("laser_large")].AccuracyDegrees=0;
    auto& V=Raider.Combat.Vehicles[0];V.Embarked=false;V.SectorIndex=5;V.Position=FVector2D(2000,0);V.Weapons.Empty();V.Cooldowns.Empty();const double Shield=V.Shield;
    Raider.Combat.TickExternalSector(Raider,Defender,5,.05);TestEqual(TEXT("Foreign defense also holds through friendly building"),V.Shield,Shield);TestEqual(TEXT("Foreign solar is not deliberately shot"),Defender.Buildings[1].Health,Solar.Health);
    Defender.Buildings.RemoveAt(1);Raider.Combat.TickExternalSector(Raider,Defender,5,.05);
    TestEqual(TEXT("Unblocked foreign defender emits a real beam event"),Raider.Combat.ShotEvents.Num(),1);
    if(Raider.Combat.ShotEvents.Num()==1)
    {
        const auto& Shot=Raider.Combat.ShotEvents[0];TestEqual(TEXT("Foreign beam does not alias an identically numbered home building"),Shot.OwnerSector,5);
        TestEqual(TEXT("Foreign beam preserves socket index"),Shot.WeaponSlot,0);TestEqual(TEXT("Foreign beam preserves weapon identity"),Shot.WeaponId,FString(TEXT("laser_large")));
    }
    return true;
}
#endif
