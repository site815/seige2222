#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace {
constexpr EAutomationTestFlags EconomyFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString Rules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
FString File(const FString& Name){return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Economy"),Name+TEXT(".json"));}
bool Deploy(FSeigeSimulation& S,FString& Error)
{if(!S.Initialize(Rules(),Error,false,false))return false;for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);if(S.Buildings[0].IsConstructing){Error=TEXT("Physical starter deployment did not complete");return false;}S.Tick(20);return true;}
// Isolated subsystem setup reassigns existing physical bodies. It never changes
// Population directly, creates anonymous workers, or bypasses runtime authority.
bool FixtureStaff(FSeigeSimulation& S,int32 Id)
{
    auto* Building=S.FindBuilding(Id);if(!Building)return false;const auto* Def=S.Definition(*Building);int32 Need=Def->Jobs;
    for(auto& W:S.Workers.Bodies)if(Need>0&&W.State==TEXT("active")&&!W.DeliveryId&&W.BuildingId!=Id)
    {
        if(W.BuildingId==S.Buildings[0].Id){int32 CoreCount=0;for(const auto& X:S.Workers.Bodies)if(X.State==TEXT("active")&&X.BuildingId==S.Buildings[0].Id&&(X.Activity==TEXT("operate")||X.Activity==TEXT("to_job")))++CoreCount;if(CoreCount<=1)continue;}
        else if(W.Activity!=TEXT("idle")&&W.Activity!=TEXT("return"))continue;
        W.Activity=TEXT("operate");W.BuildingId=Id;W.RoadId=W.DeliveryId=W.ContainerId=0;W.ContainerKind.Empty();W.StationSlot=Def->Jobs-Need;W.Route.Empty();W.NextWaypoint=0;W.PhaseSeconds=0;W.Outdoor=Def->InventoryPresentation==TEXT("outdoor");
        W.Position=S.BuildingAccessPoint(*Building);--Need;
    }
    S.Workers.RefreshMetrics(S);return Need==0;
}
// Focused subsystem fixture: legal paid placement, then install its bill immediately.
// Construction timing and no-grant scenario viability are tested separately.
int32 FixtureBuilding(FSeigeSimulation& S,const FString& Id,FVector2D P,FString& Error)
{
    FVector2D At=P;bool Valid=S.CanPlaceBuilding(Id,At,Error);
    for(int32 Ring=1;Ring<=4&&!Valid;++Ring)for(int32 Direction=0;Direction<8&&!Valid;++Direction)
    {const double Angle=Direction*UE_TWO_PI/8;At=P+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Ring*250;Valid=S.CanPlaceBuilding(Id,At,Error);}
    if(!Valid||!S.PlaceBuilding(Id,At,Error))return 0;auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);
    for(const auto& V:Cost)if(S.Buildings[0].Inventory.FindRef(V.Key)<V.Value){Error=TEXT("Fixture lacks real materials");return 0;}
    for(const auto& V:Cost)S.Buildings[0].Inventory.FindOrAdd(V.Key)-=V.Value;
    B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Cost;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;
    const int32 Result=B.Id;if(!FixtureStaff(S,Result)){Error=TEXT("Fixture lacks available real worker bodies");return 0;}S.Energy.Invalidate();return Result;
}
bool FixtureRoad(FSeigeSimulation& S,int A,int B,FString& Error){TArray<FVector2D> Route;FVector2D Last=S.BuildingAccessPoint(*S.FindBuilding(A));if(!S.FindRoadRoute(Last,S.BuildingAccessPoint(*S.FindBuilding(B)),Route))return false;for(const auto& P:Route){if((P-Last).IsNearlyZero())continue;
    // A second paid connector may share an existing trunk; never purchase or
    // place an overlapping duplicate just because the route traverses it.
    const bool Existing=S.Roads.ContainsByPredicate([&](const auto& R){if(R.Health<=0||R.IsConstructing)return false;const FVector2D V=R.B-R.A;const double Length2=V.SizeSquared();if(Length2<=0)return false;auto On=[&](FVector2D Point){return (Point-(R.A+V*FMath::Clamp(FVector2D::DotProduct(Point-R.A,V)/Length2,0.,1.))).SizeSquared()<1.e-8;};return On(Last)&&On(P);});
    if(Existing){Last=P;continue;}if(!S.PlaceRoad(Last,P,Error))return false;auto& R=S.Roads.Last();const auto Cost=S.RoadCost(R.A,R.B,R.TargetTier);for(const auto& V:Cost)if(S.Buildings[0].Inventory.FindRef(V.Key)<V.Value){Error=TEXT("Fixture lacks real road materials");return false;}for(const auto& V:Cost)S.Buildings[0].Inventory.FindOrAdd(V.Key)-=V.Value;R.InstalledMaterials=Cost;R.IsConstructing=false;R.ConstructionProgress=1;R.Tier=R.TargetTier;R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;Last=P;}S.Energy.Invalidate();S.Energy.Tick(S,0);return true;}
bool Json(const FString& Path,TSharedPtr<FJsonObject>& O){FString Raw;return FFileHelper::LoadFileToString(Raw,*Path)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),O);}
bool Write(const FString& Path,const TSharedPtr<FJsonObject>& O){FString Raw;return FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw))&&FFileHelper::SaveStringToFile(Raw,*Path);}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeIndependentGridTest,"Seige.Simulation.Economy.IndependentRoadGrids",EconomyFlags)
bool FSeigeIndependentGridTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const int Solar=FixtureBuilding(S,TEXT("solar_array"),FVector2D(-1400,-1300),Error),Consumer=FixtureBuilding(S,TEXT("sensor"),FVector2D(-800,-1600),Error);
    if(!Solar||!Consumer){AddError(Error);return false;}
    S.Energy.Tick(S,1);TestEqual(TEXT("Disconnected nongenerator has no operating electricity"),S.Energy.Fraction(Consumer),0.);
    if(!FixtureRoad(S,Solar,Consumer,Error)){AddError(Error);return false;}
    TestTrue(TEXT("Remote solar and consumer share their own road component"),S.IsRoadGridConnected(Solar,Consumer));
    TestFalse(TEXT("Independent remote outpost does not require or join the command grid"),S.IsRoadGridConnected(S.Buildings[0].Id,Consumer));
    const double BeforeStored=S.Energy.Info(S).StoredKWh,BeforeGenerated=S.Energy.GeneratedKWh,BeforeConsumed=S.Energy.ConsumedKWh,BeforeSpilled=S.Energy.SpilledKWh;
    S.Energy.Tick(S,60);
    TestTrue(TEXT("Local generation powers a remote connected consumer"),S.Energy.Fraction(Consumer)>0);
    TestTrue(TEXT("Generation equals passive consumption plus stored and spilled energy"),FMath::IsNearlyEqual(BeforeStored+S.Energy.GeneratedKWh-BeforeGenerated,S.Energy.Info(S).StoredKWh+S.Energy.ConsumedKWh-BeforeConsumed+S.Energy.SpilledKWh-BeforeSpilled,1.e-8));
    S.FindBuilding(Solar)->Enabled=false;S.FindBuilding(Solar)->BatteryEnergyKWh=0;S.Energy.Tick(S,1);
    TestEqual(TEXT("Distant command surplus cannot teleport to an empty remote grid"),S.Energy.Fraction(Consumer),0.);
    TestFalse(TEXT("Unpowered road loses its powered transport advantage"),S.Energy.RoadPowered(S.Roads[0].Id));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeBatchEnergyTest,"Seige.Simulation.Economy.BatchEscrowAndPersistence",EconomyFlags)
bool FSeigeBatchEnergyTest::RunTest(const FString&)
{
    FSeigeSimulation S,Loaded;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const int Factory=FixtureBuilding(S,TEXT("alloy_refinery"),FVector2D(1300,0),Error);
    if(!Factory||!FixtureRoad(S,S.Buildings[0].Id,Factory,Error)){AddError(Error);return false;}
    const int Support=FixtureBuilding(S,TEXT("robot_service_bay"),FVector2D(0,-1400),Error);
    if(!Support||!FixtureRoad(S,S.Buildings[0].Id,Support,Error)){AddError(Error);return false;}
    auto& B=*S.FindBuilding(Factory);const auto& Recipe=S.Recipes[S.Definition(B)->Recipe];
    for(const auto& Input:Recipe.Inputs){if(S.Buildings[0].Inventory.FindRef(Input.Key)<Input.Value){AddError(TEXT("Fixture lacks real recipe inputs"));return false;}S.Buildings[0].Inventory.FindOrAdd(Input.Key)-=Input.Value;B.Inventory.FindOrAdd(Input.Key)+=Input.Value;}
    const double Iron=S.TotalStock(TEXT("iron_ore"));S.Tick(S.FixedStepSeconds());
    TestTrue(TEXT("Production commits a local batch once energy is available"),B.ProductionCommitted);
    TestEqual(TEXT("Physical input is held in the in-process batch rather than erased"),S.TotalStock(TEXT("iron_ore")),Iron);
    TestTrue(TEXT("Batch reserves enough litres for inputs and all outputs"),B.ProductionReservedLitres>=S.InventoryLitres(Recipe.Outputs));
    const double Reserved=S.Energy.ConsumedKWh;
    if(!S.Save(File(TEXT("batch")),Error)||!Loaded.Initialize(Rules(),Error)||!Loaded.Load(File(TEXT("batch")),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Loaded unfinished batch retains its consumed energy and escrow"),Loaded.FindBuilding(Factory)->ProductionCommitted&&Loaded.Energy.ConsumedKWh==Reserved);
    S.Tick(Recipe.Seconds+1);Loaded.Tick(Recipe.Seconds+1);
    for(const auto& P:Recipe.Outputs)TestEqual(TEXT("Every configured coproduct emerges from the same completed batch"),S.ProducedUnits.FindRef(P.Key),P.Value);
    if(!S.Save(File(TEXT("batch-a")),Error)||!Loaded.Save(File(TEXT("batch-b")),Error)){AddError(Error);return false;}
    FString A,C;FFileHelper::LoadFileToString(A,*File(TEXT("batch-a")));FFileHelper::LoadFileToString(C,*File(TEXT("batch-b")));TestEqual(TEXT("Batch and energy continuation is identical after loading"),A,C);
    TSharedPtr<FJsonObject> Invalid;if(!Json(File(TEXT("batch")),Invalid))return false;Invalid->GetArrayField(TEXT("buildings"))[1]->AsObject()->SetNumberField(TEXT("production_reserved_litres"),99999);Write(File(TEXT("bad-batch")),Invalid);
    const double Time=Loaded.Time;TestFalse(TEXT("Corrupt batch reservation is rejected"),Loaded.Load(File(TEXT("bad-batch")),Error));TestEqual(TEXT("Corrupt batch loading is atomic"),Loaded.Time,Time);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeExternalTradeTest,"Seige.Simulation.Economy.ExportImportAndUpgrade",EconomyFlags)
bool FSeigeExternalTradeTest::RunTest(const FString&)
{
    FSeigeSimulation S,Loaded;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const int Port=FixtureBuilding(S,TEXT("trading_port"),FVector2D(1300,0),Error);
    if(!Port||!FixtureRoad(S,S.Buildings[0].Id,Port,Error)){AddError(Error);return false;}
    const int Support=FixtureBuilding(S,TEXT("robot_service_bay"),FVector2D(0,-1400),Error);
    if(!Support||!FixtureRoad(S,S.Buildings[0].Id,Support,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Landing begins with no free Galactic credits"),S.Credits,0.);
    TestFalse(TEXT("Imports cannot create debt or free cargo"),S.TryTrade(Port,TEXT("water"),1,true,Error));
    TestFalse(TEXT("Even a fractional import cannot overdraft the zero-credit account"),S.TryTrade(Port,TEXT("water"),.000001,true,Error));
    TestEqual(TEXT("Rejected fractional import leaves the account unchanged"),S.Credits,0.);
    // Sell finite real carried alloy through local delivery; no credit fixture grant.
    const double Quantity=10,Expected=S.TradeQuote(TEXT("alloy"),Quantity,false);
    if(!S.TryTrade(Port,TEXT("alloy"),Quantity,false,Error)){AddError(Error);return false;}
    TestFalse(TEXT("Same port cannot queue a second outstanding shipment"),S.TryTrade(Port,TEXT("alloy"),1,false,Error));
    TestEqual(TEXT("Ordering an export does not grant money immediately"),S.Credits,0.);
    for(int I=0;I<1000&&!S.FindBuilding(Port)->Shipment.Departed;++I)S.Tick(1);
    if(!TestTrue(TEXT("Export waits for physically delivered cargo before departure"),S.FindBuilding(Port)->Shipment.Departed))return false;
    if(!S.Save(File(TEXT("export")),Error)||!Loaded.Initialize(Rules(),Error)||!Loaded.Load(File(TEXT("export")),Error)){AddError(Error);return false;}
    S.Tick(200);Loaded.Tick(200);TestTrue(TEXT("Completed export pays exactly the locked external price"),FMath::IsNearlyEqual(S.Credits,Expected,1.e-9));TestEqual(TEXT("Loaded export pays once"),Loaded.Credits,S.Credits);
    const double Water=5,Before=S.TotalStock(TEXT("water")),Price=S.TradeQuote(TEXT("water"),Water,true);
    if(!S.TryTrade(Port,TEXT("water"),Water,true,Error)){AddError(Error);return false;}
    TestTrue(TEXT("Import reserves real earned credits"),FMath::IsNearlyEqual(S.Credits,Expected-Price,1.e-9));S.Tick(200);
    TestEqual(TEXT("Imported cargo arrives at the actual port stock"),S.TotalStock(TEXT("water"))-Before,Water);
    const double Installed=S.FindBuilding(Port)->InstalledMaterials.FindRef(TEXT("alloy"));
    if(!S.UpgradeBuilding(Port,Error)){AddError(Error);return false;}
    TestTrue(TEXT("Port upgrade is a physical construction site"),S.FindBuilding(Port)->IsConstructing);
    TestEqual(TEXT("Existing installed structure is preserved while upgrading"),S.FindBuilding(Port)->PreviousLevelMaterials.FindRef(TEXT("alloy")),Installed);
    for(int I=0;I<600&&S.FindBuilding(Port)->IsConstructing;++I)S.Tick(10);
    TestEqual(TEXT("Paid delivered upgrade changes to level two"),S.FindBuilding(Port)->DefId,FString(TEXT("trading_port_2")));
    if(!S.Save(File(TEXT("port-upgraded")),Error)||!Loaded.Load(File(TEXT("port-upgraded")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Upgrade level and material ledger survive saving"),Loaded.FindBuilding(Port)->DefId,S.FindBuilding(Port)->DefId);
    TestTrue(TEXT("An earned-credit import remains available before evacuation"),S.CanTrade(Port,TEXT("water"),1,true,Error));
    const double EvacuatedCredits=S.Credits;
    S.LaunchShuttle();
    TestFalse(TEXT("Evacuated colonies cannot reserve new import credits"),S.TryTrade(Port,TEXT("water"),1,true,Error));
    TestFalse(TEXT("Evacuated colonies cannot queue exports"),S.TryTrade(Port,TEXT("alloy"),1,false,Error));
    TestEqual(TEXT("Rejected post-evacuation orders preserve earned credits"),S.Credits,EvacuatedCredits);
    TestTrue(TEXT("Rejected post-evacuation orders leave no shipment"),S.FindBuilding(Port)->Shipment.Resource.IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeExportPickupReservationTest,"Seige.Simulation.Economy.ExportPreservesClaimedPickups",EconomyFlags)
bool FSeigeExportPickupReservationTest::RunTest(const FString&)
{
    FSeigeSimulation Base;FString Error;if(!Deploy(Base,Error)){AddError(Error);return false;}
    const int32 Core=Base.Buildings[0].Id,Port=FixtureBuilding(Base,TEXT("trading_port"),FVector2D(1300,0),Error);
    if(!Port||!FixtureRoad(Base,Core,Port,Error)){AddError(Error);return false;}
    // Place the finite carried silica lot at the installed port to isolate the
    // order/pickup race. Its worker, grid, goods and sale price remain real.
    const FString Resource=TEXT("silica");double Lot=0;
    for(auto& B:Base.Buildings){Lot+=B.Inventory.FindRef(Resource);B.Inventory.Remove(Resource);}
    Base.FindBuilding(Port)->Inventory.Add(Resource,Lot);
    const double Claim=FMath::Min(10.,Base.Workers.HaulUnits(Base,Resource));
    if(!TestTrue(TEXT("Fixture has an operating port and a legal finite export lot"),Base.FindBuilding(Port)->Workers>=Base.Definition(*Base.FindBuilding(Port))->Jobs&&Base.Energy.Fraction(Port)>0&&Base.OperatingEfficiency()>0&&Lot>Claim&&Base.CanTrade(Port,Resource,Lot,false,Error)))return false;
    auto ClaimHaul=[&](FSeigeSimulation& S)
    {return S.Workers.Dispatch(S,Port,Core,0,Resource,Claim,false);};

    FSeigeSimulation Claimed=Base;
    if(!TestTrue(TEXT("An existing worker accepts a port pickup before the order"),ClaimHaul(Claimed)))return false;
    TestEqual(TEXT("The claimed goods remain physically at the port before pickup"),Claimed.FindBuilding(Port)->Inventory.FindRef(Resource),Lot);
    TestFalse(TEXT("Export admission cannot double-promise a claimed port pickup"),Claimed.TryTrade(Port,Resource,Lot,false,Error));
    TestTrue(TEXT("Rejected export leaves the physical delivery intact"),Claimed.Couriers.Num()==1&&Claimed.Couriers[0].ReservedAmount==Claim&&Claimed.Couriers[0].Amount==0);

    FSeigeSimulation Ordered=Base;
    if(!Ordered.TryTrade(Port,Resource,Lot,false,Error)||!ClaimHaul(Ordered)){AddError(Error);return false;}
    const double EnergyBefore=Ordered.Energy.ConsumedKWh,CreditsBefore=Ordered.Credits;
    Ordered.Trade.Tick(Ordered,0);
    TestFalse(TEXT("A later pickup claim is rechecked before shipment departure"),Ordered.FindBuilding(Port)->Shipment.Departed);
    TestEqual(TEXT("Waiting export consumes no claimed goods"),Ordered.FindBuilding(Port)->Inventory.FindRef(Resource),Lot);
    TestEqual(TEXT("Waiting export consumes no shipment energy"),Ordered.Energy.ConsumedKWh,EnergyBefore);
    TestEqual(TEXT("Waiting export grants no credits"),Ordered.Credits,CreditsBefore);

    const double Surplus=Lot-Claim;
    if(!Claimed.TryTrade(Port,Resource,Surplus,false,Error)){AddError(Error);return false;}
    const FString Carrier=Claimed.Couriers[0].WorkerId;const int32 Bodies=Claimed.Workers.Bodies.Num();
    Claimed.Trade.Tick(Claimed,0);
    TestTrue(TEXT("Genuine surplus departs without being blocked by its own export demand"),Claimed.FindBuilding(Port)->Shipment.Departed);
    TestEqual(TEXT("Only unclaimed cargo moves into shipment escrow"),Claimed.FindBuilding(Port)->Shipment.GoodsEscrow,Surplus);
    TestEqual(TEXT("The waiting worker's complete pickup remains local"),Claimed.FindBuilding(Port)->Inventory.FindRef(Resource),Claim);
    TestTrue(TEXT("Export neither deletes nor substitutes the assigned carrier"),Claimed.Couriers[0].WorkerId==Carrier&&Claimed.Workers.Bodies.Num()==Bodies&&Claimed.Couriers[0].ReservedAmount==Claim);
    TestEqual(TEXT("Inventory and escrow conserve the original finite lot"),Claimed.TotalStock(Resource),Lot);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeImportInboundStorageTest,"Seige.Simulation.Economy.ImportPreservesInboundStorage",EconomyFlags)
bool FSeigeImportInboundStorageTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!Deploy(S,Error)){AddError(Error);return false;}
    const int32 Core=S.Buildings[0].Id,Port=FixtureBuilding(S,TEXT("trading_port"),FVector2D(1300,0),Error);
    if(!Port||!FixtureRoad(S,Core,Port,Error)){AddError(Error);return false;}
    const FString Carried=TEXT("silica"),Imported=TEXT("water");constexpr double Quantity=10;
    const double IncomingLitres=Quantity*S.Resources[Carried].LitresPerUnit,ImportLitres=Quantity*S.Resources[Imported].LitresPerUnit;
    auto& PortDefinition=S.BuildingDefs[S.FindBuilding(Port)->DefId];const double OccupiedBefore=S.StorageUsed(*S.FindBuilding(Port));
    // A small-port boundary fixture makes each order fit alone but not together.
    // The arriving cargo still belongs to an existing physical starter worker.
    PortDefinition.StorageCapacity=OccupiedBefore+FMath::Max(IncomingLitres,ImportLitres)+FMath::Min(IncomingLitres,ImportLitres)*.5;
    S.Credits=S.TradeQuote(Imported,Quantity,true);const double CreditsBefore=S.Credits;
    if(!TestTrue(TEXT("The paid import fits before a physical delivery claims space"),S.CanTrade(Port,Imported,Quantity,true,Error)))return false;
    if(!TestTrue(TEXT("An existing body reserves a real inbound cargo load"),S.Workers.Dispatch(S,Core,Port,0,Carried,Quantity,false)))return false;
    const int32 CourierId=S.Couriers.Last().Id;const FString WorkerId=S.Couriers.Last().WorkerId;
    TestFalse(TEXT("Import cannot sell storage already promised to an arriving worker"),S.TryTrade(Port,Imported,Quantity,true,Error));
    TestEqual(TEXT("Rejected import reserves no credits"),S.Credits,CreditsBefore);
    TestTrue(TEXT("Rejected import leaves no shipment or lost delivery"),S.FindBuilding(Port)->Shipment.Resource.IsEmpty()&&S.Couriers.Last().Id==CourierId&&S.Couriers.Last().WorkerId==WorkerId);
    PortDefinition.StorageCapacity=OccupiedBefore+IncomingLitres+ImportLitres;
    if(!TestTrue(TEXT("Import succeeds when both cargo commitments genuinely fit"),S.TryTrade(Port,Imported,Quantity,true,Error)))return false;
    TestEqual(TEXT("Accepted import pays its actual quoted price"),S.Credits,0.);
    TestTrue(TEXT("Import reserves the remaining space without erasing the inbound claim"),FMath::IsNearlyZero(S.StorageRoom(*S.FindBuilding(Port)),1.e-8)&&S.Couriers.Last().ReservedAmount==Quantity);
    return true;
}
#endif
