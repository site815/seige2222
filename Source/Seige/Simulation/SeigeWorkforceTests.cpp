#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#if WITH_DEV_AUTOMATION_TESTS
namespace {
constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString Rules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
FString SavePath(const FString& Name){return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Workforce"),Name+TEXT(".json"));}
// Isolate production/workforce from independently tested construction time: the
// real carried deployment kit is installed, without adding materials or credits.
bool CoreFixture(FSeigeSimulation& S,FString& Error){if(!S.Initialize(Rules(),Error,false,false))return false;auto& B=S.Buildings[0];B.InstalledMaterials=B.ConstructionMaterials;B.ConstructionMaterials.Empty();B.ConstructionProgress=1;B.IsConstructing=false;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;B.Workers=S.Definition(B)->Jobs;S.Population=B.Workers;S.Energy.Invalidate();S.Tick(S.FixedStepSeconds());return true;}
int FactoryFixture(FSeigeSimulation& S,FString& Error,const FString& Definition=TEXT("worker_factory")){if(!S.PlaceBuilding(Definition,FVector2D(1300,0),Error))return 0;auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);for(const auto& P:Cost)S.Buildings[0].Inventory.FindOrAdd(P.Key)-=P.Value;B.InstalledMaterials=Cost;B.ConstructionProgress=1;B.IsConstructing=false;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;const int Id=B.Id;TArray<FVector2D> Route;FVector2D Last=S.BuildingAccessPoint(S.Buildings[0]);if(!S.FindRoadRoute(Last,S.BuildingAccessPoint(B),Route))return 0;for(const auto& P:Route){if(!S.PlaceRoad(Last,P,Error))return 0;auto& R=S.Roads.Last();R.InstalledMaterials=S.RoadCost(R.A,R.B,R.TargetTier);for(const auto& V:R.InstalledMaterials)S.Buildings[0].Inventory.FindOrAdd(V.Key)-=V.Value;R.Tier=R.TargetTier;R.IsConstructing=false;R.ConstructionProgress=1;R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;Last=P;}S.Population=S.BuildingDefs[S.CoreDefinition].Jobs+S.Definition(*S.FindBuilding(Id))->Jobs;S.FindBuilding(Id)->Workers=S.Definition(*S.FindBuilding(Id))->Jobs;S.Energy.Invalidate();S.Tick(S.FixedStepSeconds());return Id;}
bool Read(const FString& P,TSharedPtr<FJsonObject>& O){FString Text;return FFileHelper::LoadFileToString(Text,*P)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);}
bool Write(const FString& P,const TSharedPtr<FJsonObject>& O){FString Text;return FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*P);}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeReplicatorWorkforceTest,"Seige.Simulation.Workforce.ReplicatorAndFactory",Flags)
bool FSeigeReplicatorWorkforceTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!CoreFixture(S,Error)){AddError(Error);return false;}auto& Core=S.Buildings[0];
    FString Goods;for(const auto& Id:S.ProductionOptions(Core.Id))if(S.Recipes[Id].WorkerOutput==0){Goods=Id;break;}
    if(!TestFalse(TEXT("Universal core offers a physical-goods recipe"),Goods.IsEmpty()))return false;
    const auto Recipe=S.Recipes[Goods];const auto Inputs=S.ProductionInputs(Core,Goods);Core.Inventory=Inputs;S.SetWorkerSurplusTarget(1,Error);
    if(!S.SetProductionRecipe(Core.Id,Goods,Error)){AddError(Error);return false;}const double Energy=S.Energy.ConsumedKWh;
    S.StepRecipe(Core,.01);
    TestTrue(TEXT("Missing worker parts do not block the selected goods recipe"),Core.ProductionCommitted&&Core.CommittedRecipe==Goods);
    TestTrue(TEXT("Replicator debits exact configured batch energy once"),FMath::IsNearlyEqual(S.Energy.ConsumedKWh-Energy,S.ProductionEnergy(Core,Goods),1.e-8));
    for(const auto& P:Inputs){TestEqual(TEXT("Input is physically escrowed"),Core.ProductionInputs.FindRef(P.Key),P.Value);TestEqual(TEXT("Escrow removes the local available input"),Core.Inventory.FindRef(P.Key),0.);}
    const FString Worker=S.TextRule(TEXT("population_recipe"));if(!S.SetProductionRecipe(Core.Id,Worker,Error)||!S.Save(SavePath(TEXT("replicator-batch")),Error)||!Loaded.Initialize(Rules(),Error)||!Loaded.Load(SavePath(TEXT("replicator-batch")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Selection change does not replace a paid batch"),Loaded.Buildings[0].CommittedRecipe,Goods);
    const double Remaining=(1-Core.Progress)*S.ProductionSeconds(Core,Goods)/S.WorkFraction(Core);
    S.StepRecipe(Core,Remaining);Loaded.StepRecipe(Loaded.Buildings[0],Remaining);
    for(const auto& P:Recipe.Outputs){TestEqual(TEXT("Universal replicator yields every configured coproduct"),Core.Inventory.FindRef(P.Key),P.Value);TestEqual(TEXT("Reloaded paid batch yields identical output"),Loaded.Buildings[0].Inventory.FindRef(P.Key),P.Value);}
    TestFalse(TEXT("Completed batch releases escrow"),Core.ProductionCommitted);
    FSeigeSimulation Factory;if(!CoreFixture(Factory,Error)){AddError(Error);return false;}const int Id=FactoryFixture(Factory,Error);if(!Id){AddError(Error);return false;}
    auto& Fast=*Factory.FindBuilding(Id);Fast.Inventory=Factory.ProductionInputs(Fast,Worker);Factory.SetWorkerSurplusTarget(1,Error);Factory.AllocateWorkers();Factory.Energy.Tick(Factory,0);
    Core.Inventory=S.ProductionInputs(Core,Worker);S.SetWorkerSurplusTarget(1,Error);S.AllocateWorkers();S.Energy.Tick(S,0);
    TestTrue(TEXT("Worker factory has a shorter physical cycle than the core"),Factory.ProductionSeconds(Fast,Worker)<S.ProductionSeconds(Core,Worker));
    TestTrue(TEXT("Worker factory consumes less batch energy"),Factory.ProductionEnergy(Fast,Worker)<S.ProductionEnergy(Core,Worker));
    const double FastDuration=Factory.ProductionSeconds(Fast,Worker)/Factory.WorkFraction(Fast),BeforeEnergy=Factory.Energy.ConsumedKWh;
    Factory.StepRecipe(Fast,FastDuration-.1);S.StepRecipe(Core,FastDuration-.1);
    TestEqual(TEXT("Workers cannot emerge before the full factory cycle"),Fast.Inventory.FindRef(S.TextRule(TEXT("inactive_worker_resource"))),0.);
    Factory.StepRecipe(Fast,.1);
    TestEqual(TEXT("Dedicated factory creates one real stored worker"),Fast.Inventory.FindRef(S.TextRule(TEXT("inactive_worker_resource"))),1.);
    TestTrue(TEXT("Core remains part-way through its slower worker cycle"),Core.ProductionCommitted&&Core.Progress<1);
    TestTrue(TEXT("Factory charges batch energy exactly once across partial steps"),FMath::IsNearlyEqual(Factory.Energy.ConsumedKWh-BeforeEnergy,Factory.ProductionEnergy(Fast,Worker),1.e-8));
    FSeigeSimulation Upgrade;if(!CoreFixture(Upgrade,Error)){AddError(Error);return false;}const int CoreId=Upgrade.Buildings[0].Id;const double ReservedPlot=Upgrade.Definition(Upgrade.Buildings[0])->ReservedFootprint;
    // Supply only the missing higher-tier upgrade inputs in this isolated level-transition fixture.
    // Full scenario viability uses the separate no-grant economy test.
    Upgrade.PeriodicAttacksEnabled=true;Upgrade.TriggerWave();Upgrade.Enemies.SetNum(1);Upgrade.Enemies[0].Position=FVector2D(1000,0);Upgrade.StepCombat(.05);TestTrue(TEXT("Core has fired before upgrading"),Upgrade.Buildings[0].LastShotTime>=0);
    for(int Level=2;Level<=3;++Level){for(const auto& P:Upgrade.Definition(Upgrade.Buildings[0])->UpgradeCost)Upgrade.Buildings[0].Inventory.FindOrAdd(P.Key)=FMath::Max(Upgrade.Buildings[0].Inventory.FindRef(P.Key),P.Value);if(!Upgrade.UpgradeBuilding(CoreId,Error)){AddError(Error);return false;}Upgrade.StepConstruction(Upgrade.ConstructionSeconds(Upgrade.Buildings[0])*2);Upgrade.AllocateWorkers();Upgrade.Energy.Tick(Upgrade,0);Upgrade.Combat.Tick(Upgrade,Upgrade.FixedStepSeconds());TestFalse(TEXT("Core upgrade finishes from real local paid material"),Upgrade.Buildings[0].IsConstructing);TestEqual(TEXT("Current core identity follows its new level"),Upgrade.Definition(Upgrade.Buildings[0])->Level,Level);TestEqual(TEXT("Every core level preserves the originally reserved plot"),Upgrade.Definition(Upgrade.Buildings[0])->ReservedFootprint,ReservedPlot);TestTrue(TEXT("Existing crew can operate upgraded core without a staffing deadlock"),Upgrade.WorkFraction(Upgrade.Buildings[0])>0);}
    if(!Upgrade.Save(SavePath(TEXT("core-level-three")),Error)||!Loaded.Load(SavePath(TEXT("core-level-three")),Error)){AddError(Error);return false;}TestEqual(TEXT("Level-three core identity is restored before validation"),Loaded.CoreDefinition,Upgrade.CoreDefinition);TestEqual(TEXT("Upgrading never replaces the core entity"),Loaded.Buildings[0].Id,CoreId);
    FSeigeSimulation Armed;if(!CoreFixture(Armed,Error)){AddError(Error);return false;}const int Turret=FactoryFixture(Armed,Error,TEXT("turret"));if(!Turret){AddError(Error);return false;}Armed.Combat.BuildingState[Armed.Buildings[0].Id].Weapons.Empty();Armed.Combat.BuildingState[Armed.Buildings[0].Id].Cooldowns.Empty();Armed.PeriodicAttacksEnabled=true;Armed.TriggerWave();Armed.Enemies.SetNum(1);Armed.Enemies[0].Position=Armed.FindBuilding(Turret)->Position+FVector2D(300,0);Armed.StepCombat(.05);
    if(!TestTrue(TEXT("An ordinary defense building has fired before upgrade"),Armed.FindBuilding(Turret)->LastShotTime>=0))return false;
    for(const auto& P:Armed.Definition(*Armed.FindBuilding(Turret))->UpgradeCost)Armed.Buildings[0].Inventory.FindOrAdd(P.Key)=FMath::Max(Armed.Buildings[0].Inventory.FindRef(P.Key),P.Value);
    if(!Armed.UpgradeBuilding(Turret,Error)||!Armed.Save(SavePath(TEXT("armed-upgrade")),Error)||!Loaded.Load(SavePath(TEXT("armed-upgrade")),Error)){AddError(Error);return false;}TestEqual(TEXT("An upgrading defense preserves legitimate prior shot history"),Loaded.FindBuilding(Turret)->LastShotTime,Armed.FindBuilding(Turret)->LastShotTime);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeStoredWorkerLifecycleTest,"Seige.Simulation.Workforce.StoredBodiesAndDisassembly",Flags)
bool FSeigeStoredWorkerLifecycleTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!CoreFixture(S,Error)){AddError(Error);return false;}auto& C=S.Buildings[0];const FString Body=S.TextRule(TEXT("inactive_worker_resource"));const int Jobs=S.TotalJobs;
    S.Population=Jobs+2;S.AllocateWorkers();S.StepPopulation(S.Number(TEXT("worker_store_seconds"))*2);
    TestEqual(TEXT("Only surplus active workers enter storage"),S.Population,Jobs);TestEqual(TEXT("Every deactivated body consumes real cargo storage"),C.Inventory.FindRef(Body),2.);
    TestTrue(TEXT("Stored bodies add their configured litres"),S.InventoryLitres({{Body,2}})==S.Resources[Body].LitresPerUnit*2);
    S.Population-=1;S.StepPopulation(S.Number(TEXT("worker_reactivate_seconds")));S.AllocateWorkers();
    TestEqual(TEXT("Existing stored body fills a vacancy before manufacturing"),S.Population,Jobs);TestEqual(TEXT("Reactivation consumes exactly one stored body"),C.Inventory.FindRef(Body),1.);
    S.SetWorkerSurplusTarget(1,Error);TestFalse(TEXT("Reserve target prevents recycling the last protected body"),S.DisassembleWorkers(C.Id,1,Error));
    S.SetWorkerSurplusTarget(0,Error);const auto Returns=S.DisassemblyOutputs();TMap<FString,double> Before=C.Inventory;const double BeforeEnergy=S.Energy.ConsumedKWh;
    if(!S.DisassembleWorkers(C.Id,1,Error)){AddError(Error);return false;}S.StepWorkerDisassembly(.05);
    TestEqual(TEXT("Recycling never deletes an active worker"),S.Population,Jobs);TestEqual(TEXT("One body enters paid disassembly escrow"),C.Inventory.FindRef(Body),0.);
    TestTrue(TEXT("Disassembly charges exactly one kWh"),FMath::IsNearlyEqual(S.Energy.ConsumedKWh-BeforeEnergy,1.,1.e-8));
    if(!S.Save(SavePath(TEXT("disassembly")),Error)||!Loaded.Initialize(Rules(),Error)||!Loaded.Load(SavePath(TEXT("disassembly")),Error)){AddError(Error);return false;}
    const double Remaining=(1-C.DisassemblyProgress)*S.Number(TEXT("worker_disassemble_seconds"))/S.WorkFraction(C);S.StepWorkerDisassembly(Remaining);Loaded.StepWorkerDisassembly(Remaining);
    for(const auto& P:Returns){TestEqual(TEXT("Finished disassembly returns configured physical parts"),C.Inventory.FindRef(P.Key),Before.FindRef(P.Key)+P.Value);TestEqual(TEXT("Reloaded disassembly produces the same parts once"),Loaded.Buildings[0].Inventory.FindRef(P.Key),C.Inventory.FindRef(P.Key));}
    TestEqual(TEXT("Completed body count is persisted"),Loaded.WorkersDisassembled,1);
    TestFalse(TEXT("Active workforce cannot be recycled through the surplus API"),S.DisassembleWorkers(C.Id,1,Error));
    TSharedPtr<FJsonObject> Bad;if(!Read(SavePath(TEXT("disassembly")),Bad))return false;Bad->GetArrayField(TEXT("buildings"))[0]->AsObject()->GetObjectField(TEXT("inventory"))->SetNumberField(Body,.5);Write(SavePath(TEXT("fractional-body")),Bad);
    const double Time=Loaded.Time;TestFalse(TEXT("Fractional stored body save is rejected"),Loaded.Load(SavePath(TEXT("fractional-body")),Error));TestEqual(TEXT("Rejected worker save is atomic"),Loaded.Time,Time);
    FSeigeSimulation Full;if(!CoreFixture(Full,Error)){AddError(Error);return false;}auto& F=Full.Buildings[0];F.Inventory.Empty();const FString Part=Returns.CreateConstIterator().Key();const double RefundL=Full.InventoryLitres(Returns);F.Inventory.Add(Part,(Full.Definition(F)->StorageCapacity-RefundL)/Full.Resources[Part].LitresPerUnit);Full.Population=Full.TotalJobs+1;Full.AllocateWorkers();
    Full.StepPopulation(Full.Number(TEXT("worker_store_seconds")));
    TestTrue(TEXT("Storage overflow enters paid recycling only for an unassigned body"),F.DisassemblyCommitted&&Full.Population==Full.TotalJobs);TestTrue(TEXT("Overflow recycling reserves exactly the remaining storage for recovered parts"),FMath::IsNearlyEqual(Full.Occupied(F),Full.Definition(F)->StorageCapacity,1.e-8));
    FSeigeSimulation Blocked;if(!CoreFixture(Blocked,Error)){AddError(Error);return false;}auto& NoRoom=Blocked.Buildings[0];NoRoom.Inventory.Empty();
    NoRoom.Inventory.Add(Part,(Blocked.Definition(NoRoom)->StorageCapacity-RefundL+.001)/Blocked.Resources[Part].LitresPerUnit);Blocked.Population=Blocked.TotalJobs+1;Blocked.AllocateWorkers();const double UnspentEnergy=Blocked.Energy.ConsumedKWh;
    Blocked.StepPopulation(Blocked.Number(TEXT("worker_store_seconds")));
    TestFalse(TEXT("A real recovered-parts capacity shortage blocks recycling"),NoRoom.DisassemblyCommitted);TestEqual(TEXT("Capacity shortage preserves the unassigned worker"),Blocked.Population,Blocked.TotalJobs+1);TestEqual(TEXT("Blocked recycling charges no energy"),Blocked.Energy.ConsumedKWh,UnspentEnergy);
    FSeigeSimulation Trade,TradeLoaded;if(!CoreFixture(Trade,Error)){AddError(Error);return false;}const int Port=FactoryFixture(Trade,Error,TEXT("trading_port"));if(!Port){AddError(Error);return false;}
    Trade.Buildings[0].Inventory.Add(Body,1);Trade.FindBuilding(Port)->Inventory.Add(Body,3);Trade.SetWorkerSurplusTarget(1,Error);Trade.SetPortWorkerTarget(Port,3,Error);Trade.AllocateWorkers();Trade.Energy.Tick(Trade,0);
    TestFalse(TEXT("Worker imports must be whole bodies"),Trade.CanTrade(Port,Body,.5,true,Error));TestFalse(TEXT("Worker exports must be whole bodies"),Trade.CanTrade(Port,Body,.5,false,Error));
    if(!Trade.TryTrade(Port,Body,3,false,Error)){AddError(Error);return false;}const double Earned=Trade.TradeQuote(Body,3,false);Trade.Trade.Tick(Trade,.05);
    TestTrue(TEXT("Port target workers enter real shipment escrow"),Trade.FindBuilding(Port)->Shipment.Departed&&Trade.FindBuilding(Port)->Shipment.GoodsEscrow==3);TestEqual(TEXT("In-flight workers remain in the material ledger"),Trade.InactiveWorkerCount(),4);
    if(!Trade.Save(SavePath(TEXT("worker-export")),Error)||!TradeLoaded.Initialize(Rules(),Error)||!TradeLoaded.Load(SavePath(TEXT("worker-export")),Error)){AddError(Error);return false;}Trade.Trade.Tick(Trade,1000);TradeLoaded.Trade.Tick(TradeLoaded,1000);
    TestEqual(TEXT("Completed body export pays its exact external price"),Trade.Credits,Earned);TestEqual(TEXT("Loading an in-flight body export does not duplicate credits"),TradeLoaded.Credits,Trade.Credits);TestEqual(TEXT("Colony reserve remains after exporting port stock"),Trade.InactiveWorkerCount(),1);
    return true;
}
#endif
