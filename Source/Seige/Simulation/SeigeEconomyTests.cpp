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
bool Deploy(FSeigeSimulation& S,FString& Error){if(!S.Initialize(Rules(),Error,false,false))return false;S.Tick(S.BuildingDefs[S.CoreDefinition].ConstructionSeconds+S.FixedStepSeconds());return !S.Buildings[0].IsConstructing;}
// Focused subsystem fixture: legal paid placement, then install its bill immediately.
// Construction timing and no-grant scenario viability are tested separately.
int32 FixtureBuilding(FSeigeSimulation& S,const FString& Id,FVector2D P,FString& Error){if(!S.PlaceBuilding(Id,P,Error))return 0;auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);for(const auto& V:Cost)S.Buildings[0].Inventory.FindOrAdd(V.Key)-=V.Value;B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=Cost;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;B.Workers=S.Definition(B)->Jobs;S.Energy.Invalidate();return B.Id;}
bool FixtureRoad(FSeigeSimulation& S,int A,int B,FString& Error){TArray<FVector2D> Route;FVector2D Last=S.BuildingAccessPoint(*S.FindBuilding(A));if(!S.FindRoadRoute(Last,S.BuildingAccessPoint(*S.FindBuilding(B)),Route))return false;for(const auto& P:Route){if((P-Last).IsNearlyZero())continue;if(!S.PlaceRoad(Last,P,Error))return false;auto& R=S.Roads.Last();const auto Cost=S.RoadCost(R.A,R.B,R.TargetTier);for(const auto& V:Cost)S.Buildings[0].Inventory.FindOrAdd(V.Key)-=V.Value;R.InstalledMaterials=Cost;R.IsConstructing=false;R.ConstructionProgress=1;R.Tier=R.TargetTier;R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;Last=P;}S.Energy.Invalidate();S.Energy.Tick(S,0);return true;}
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
    S.Population=8;auto& B=*S.FindBuilding(Factory);const auto& Recipe=S.Recipes[S.Definition(B)->Recipe];B.Inventory=Recipe.Inputs;
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
    if(!Port||!FixtureRoad(S,S.Buildings[0].Id,Port,Error)){AddError(Error);return false;}S.Population=7;
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
#endif
