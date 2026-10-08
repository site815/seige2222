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
// Physical bootstrap is exercised before focused production fixtures. Installed
// fixtures below pay their actual bill and reassign existing bodies explicitly.
void FixtureWorkers(FSeigeSimulation& S,int32 Workplace=0)
{
    int32 Needed=Workplace?S.Definition(*S.FindBuilding(Workplace))->Jobs:0,Slot=0;
    for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active"))
    {W.Activity=TEXT("operate");W.BuildingId=Needed-->0?Workplace:S.Buildings[0].Id;W.RoadId=W.DeliveryId=W.ContainerId=0;W.ContainerKind.Empty();W.Outdoor=true;W.Route.Empty();W.NextWaypoint=0;W.StationSlot=Slot++;W.Position=S.BuildingAccessPoint(*S.FindBuilding(W.BuildingId))+FVector2D(0,20+Slot*12);}
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);
}
void CancelFixtureBatch(FSeigeBuilding& B)
{for(const auto& P:B.ProductionInputs)B.Inventory.FindOrAdd(P.Key)+=P.Value;B.ProductionInputs.Empty();B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.ProductionReservedLitres=B.Progress=0;}
bool CoreFixture(FSeigeSimulation& S,FString& Error)
{if(!S.Initialize(Rules(),Error,false,false))return false;for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);if(S.Buildings[0].IsConstructing){Error=TEXT("Real starter crew failed deployment");return false;}S.Tick(20);CancelFixtureBatch(S.Buildings[0]);FixtureWorkers(S);return true;}
int FactoryFixture(FSeigeSimulation& S,FString& Error,const FString& Definition=TEXT("worker_factory"))
{
    if(!S.PlaceBuilding(Definition,FVector2D(1300,0),Error))return 0;auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);
    for(const auto& P:Cost)S.Buildings[0].Inventory.FindOrAdd(P.Key)-=P.Value;B.InstalledMaterials=Cost;B.ConstructionProgress=1;B.IsConstructing=false;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;const int Id=B.Id;
    TArray<FVector2D> Route;FVector2D Last=S.BuildingAccessPoint(S.Buildings[0]);if(!S.FindRoadRoute(Last,S.BuildingAccessPoint(B),Route))return 0;
    for(const auto& P:Route){if(FVector2D::Distance(Last,P)<.001)continue;if(!S.PlaceRoad(Last,P,Error))return 0;auto& R=S.Roads.Last();R.InstalledMaterials=S.RoadCost(R.A,R.B,R.TargetTier);for(const auto& V:R.InstalledMaterials)S.Buildings[0].Inventory.FindOrAdd(V.Key)-=V.Value;R.Tier=R.TargetTier;R.IsConstructing=false;R.ConstructionProgress=1;R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;Last=P;}
    FixtureWorkers(S,Id);return Id;
}
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
    FSeigeSimulation CapacityTest;if(!CoreFixture(CapacityTest,Error)){AddError(Error);return false;}
    const int32 CapacityFactory=FactoryFixture(CapacityTest,Error);if(!CapacityFactory){AddError(Error);return false;}
    auto& Limited=*CapacityTest.FindBuilding(CapacityFactory);CapacityTest.SetWorkerSurplusTarget(10,Error);
    Limited.Inventory=CapacityTest.ProductionInputs(Limited,Worker);
    const double InputLitres=CapacityTest.InventoryLitres(Limited.Inventory),OutputLitres=CapacityTest.ProductionOutputLitres(CapacityTest.Recipes[Worker]);
    const FString Filler=TEXT("water"),Repair=CapacityTest.TextRule(TEXT("repair_resource"));
    Limited.Inventory.FindOrAdd(Filler)+=(CapacityTest.Definition(Limited)->StorageCapacity-OutputLitres)/CapacityTest.Resources[Filler].LitresPerUnit;
    if(!TestTrue(TEXT("Stored-worker production expands physical occupied volume"),OutputLitres>InputLitres)||!CapacityTest.Workers.Dispatch(CapacityTest,CapacityTest.Buildings[0].Id,CapacityFactory,0,Repair,1,false)){AddError(TEXT("Factory capacity probe requires an actual incoming load"));return false;}
    const double Uncharged=CapacityTest.Energy.ConsumedKWh,InputsBefore=CapacityTest.InventoryLitres(Limited.Inventory);
    CapacityTest.StepRecipe(Limited,.01);
    TestFalse(TEXT("A batch cannot occupy a promised incoming delivery berth"),Limited.ProductionCommitted);
    TestEqual(TEXT("Capacity-blocked assembly preserves its real local input bill"),CapacityTest.InventoryLitres(Limited.Inventory),InputsBefore);
    TestEqual(TEXT("Capacity-blocked assembly consumes no batch energy"),CapacityTest.Energy.ConsumedKWh,Uncharged);
    FSeigeSimulation Reserved;if(!CoreFixture(Reserved,Error)){AddError(Error);return false;}auto& ReservedCore=Reserved.Buildings[0];Reserved.SetWorkerSurplusTarget(10,Error);
    const auto WorkerInputs=Reserved.ProductionInputs(ReservedCore,Worker);const FString Parts=TEXT("components");const double Bill=Reserved.BuildingDefs[TEXT("worker_factory")].Cost.FindRef(Parts);
    const double Buffer=ReservedCore.Inventory.FindRef(Parts)-Reserved.ConstructionAvailable(Parts);ReservedCore.Inventory[Parts]=Bill+Buffer;
    if(!Reserved.PlaceBuilding(TEXT("worker_factory"),FVector2D(1300,0),Error)){AddError(Error);return false;}
    auto& ProtectedCore=Reserved.Buildings[0];const int32 StoredBefore=Reserved.Workers.StoredAt(ProtectedCore.Id);for(int32 Attempt=0;Attempt<6;++Attempt)Reserved.StepRecipe(ProtectedCore,Reserved.ProductionSeconds(ProtectedCore,Worker)/Reserved.WorkFraction(ProtectedCore)+.01);
    TestTrue(TEXT("Successive paid worker batches never consume an accepted construction bill"),ProtectedCore.Inventory.FindRef(Parts)+1.e-8>=Bill);
    TestEqual(TEXT("Only genuinely unreserved whole worker batches may finish"),Reserved.Workers.StoredAt(ProtectedCore.Id)-StoredBefore,int32(FMath::FloorToInt((Buffer+1.e-8)/WorkerInputs.FindRef(Parts))));
    TestTrue(TEXT("The remaining protected parts cannot fund another assembly batch"),Reserved.Spendable(ProtectedCore,Parts)+1.e-8<WorkerInputs.FindRef(Parts));
    const int32 PromisedSite=Reserved.Buildings.Last().Id;Reserved.SetWorkerSurplusTarget(0,Error);
    for(int32 I=0;I<1200&&Reserved.FindBuilding(PromisedSite)->IsConstructing;++I)Reserved.Tick(10);
    TestFalse(TEXT("Protected stock is physically delivered and installed by the finite crew"),Reserved.FindBuilding(PromisedSite)->IsConstructing);
    TestTrue(TEXT("The completed factory embodies its whole promised parts bill"),FMath::IsNearlyEqual(Reserved.FindBuilding(PromisedSite)->InstalledMaterials.FindRef(Parts),Bill,1.e-8));
    FSeigeSimulation Distributed;if(!CoreFixture(Distributed,Error)){AddError(Error);return false;}const int32 DonorId=FactoryFixture(Distributed,Error);if(!DonorId){AddError(Error);return false;}Distributed.SetWorkerSurplusTarget(10,Error);
    auto* Donor=Distributed.FindBuilding(DonorId);for(const auto& P:Distributed.ProductionInputs(*Donor,Worker)){const double Amount=P.Value*2;Distributed.Buildings[0].Inventory.FindOrAdd(P.Key)-=Amount;Donor->Inventory.FindOrAdd(P.Key)+=Amount;}
    const double SmallBill=Distributed.BuildingDefs[TEXT("sensor")].Cost.FindRef(Parts);
    Distributed.Buildings[0].Inventory[Parts]=Distributed.Demand(Distributed.Buildings[0],Parts,false);Donor->Inventory[Parts]=Distributed.Demand(*Donor,Parts,false)+SmallBill;
    if(!Distributed.PlaceBuilding(TEXT("sensor"),FVector2D(0,1300),Error)){AddError(Error);return false;}const int32 BufferedSite=Distributed.Buildings.Last().Id;Donor=Distributed.FindBuilding(DonorId);
    for(int32 I=0;I<2;++I)Distributed.StepRecipe(*Donor,Distributed.ProductionSeconds(*Donor,Worker)/Distributed.WorkFraction(*Donor)+.01);
    TestTrue(TEXT("A promised bill can remain physically present entirely inside operating refill buffers"),Distributed.Buildings[0].Inventory.FindRef(Parts)<=Distributed.Demand(Distributed.Buildings[0],Parts,false)+1.e-8&&Donor->Inventory.FindRef(Parts)<=Distributed.Demand(*Donor,Parts,false)+1.e-8);
    const double BufferedStock=Distributed.TotalStock(Parts);Distributed.StepLogistics(Distributed.Number(TEXT("dispatch_interval")));
    if(!Distributed.Couriers.ContainsByPredicate([&](const auto& C){return C.TargetId==BufferedSite&&C.Resource==Parts&&C.ForConstruction&&C.ReservedAmount>0;}))
    {
        FString Diag=FString::Printf(TEXT("Distributed probe: hatch=%d clock=%.2f reserved=%.2f available=%.2f core=%.2f/%.2f donor=%.2f/%.2f haul=%.2f couriers=%d"),Distributed.DeploymentHatchOpen?1:0,Distributed.DispatchClock,Distributed.ConstructionReserved(Parts),Distributed.ConstructionAvailable(Parts),Distributed.Buildings[0].Inventory.FindRef(Parts),Distributed.Demand(Distributed.Buildings[0],Parts,false),Donor->Inventory.FindRef(Parts),Distributed.Demand(*Donor,Parts,false),Distributed.Workers.HaulUnits(Distributed,Parts),Distributed.Couriers.Num());
        for(const auto& C:Distributed.Couriers)Diag+=FString::Printf(TEXT(" | courier src=%d dst=%d road=%d %s reserved=%.2f amount=%.2f construction=%d phase=%s"),C.SourceId,C.TargetId,C.RoadTargetId,*C.Resource,C.ReservedAmount,C.Amount,C.ForConstruction?1:0,*C.Phase);
        for(const auto& W:Distributed.Workers.Bodies)Diag+=FString::Printf(TEXT(" | %s %s/%s b=%d d=%d"),*W.Id,*W.State,*W.Activity,W.BuildingId,W.DeliveryId);
        AddInfo(Diag);
    }
    TestTrue(TEXT("Construction still claims its protected bill from distributed buffer stock"),Distributed.Couriers.ContainsByPredicate([&](const auto& C){return C.TargetId==BufferedSite&&C.Resource==Parts&&C.ForConstruction&&C.ReservedAmount>0;}));
    TestEqual(TEXT("Claiming distributed stock does not create or debit goods before pickup"),Distributed.TotalStock(Parts),BufferedStock);
    FSeigeSimulation Upgrade;if(!CoreFixture(Upgrade,Error)){AddError(Error);return false;}const int CoreId=Upgrade.Buildings[0].Id;const double ReservedPlot=Upgrade.Definition(Upgrade.Buildings[0])->ReservedFootprint;
    // Supply only the missing higher-tier upgrade inputs in this isolated level-transition fixture.
    // Full scenario viability uses the separate no-grant economy test.
    Upgrade.PeriodicAttacksEnabled=true;Upgrade.TriggerWave();Upgrade.Enemies.SetNum(1);Upgrade.Enemies[0].Position=FVector2D(1000,0);Upgrade.StepCombat(.05);TestTrue(TEXT("Core has fired before upgrading"),Upgrade.Buildings[0].LastShotTime>=0);
    for(int Level=2;Level<=3;++Level){for(const auto& P:Upgrade.Definition(Upgrade.Buildings[0])->UpgradeCost)Upgrade.Buildings[0].Inventory.FindOrAdd(P.Key)+=FMath::Max(0.,P.Value-Upgrade.ConstructionAvailable(P.Key));if(!Upgrade.UpgradeBuilding(CoreId,Error)){AddError(Error);return false;}{auto& U=Upgrade.Buildings[0];for(auto& W:Upgrade.Workers.Bodies)if(W.State==TEXT("active")){W.Activity=TEXT("build");W.BuildingId=CoreId;W.Position=Upgrade.BuildingAccessPoint(U);W.Route.Empty();W.NextWaypoint=0;}Upgrade.Workers.RefreshMetrics(Upgrade);Upgrade.StepConstruction(Upgrade.ConstructionSeconds(U)*2);FixtureWorkers(Upgrade);}Upgrade.Energy.Tick(Upgrade,0);Upgrade.Combat.Tick(Upgrade,Upgrade.FixedStepSeconds());TestFalse(TEXT("Core upgrade finishes from real local paid material"),Upgrade.Buildings[0].IsConstructing);TestEqual(TEXT("Current core identity follows its new level"),Upgrade.Definition(Upgrade.Buildings[0])->Level,Level);TestEqual(TEXT("Every core level preserves the originally reserved plot"),Upgrade.Definition(Upgrade.Buildings[0])->ReservedFootprint,ReservedPlot);TestTrue(TEXT("Existing crew can operate upgraded core without a staffing deadlock"),Upgrade.WorkFraction(Upgrade.Buildings[0])>0);}
    if(!Upgrade.Save(SavePath(TEXT("core-level-three")),Error)||!Loaded.Load(SavePath(TEXT("core-level-three")),Error)){AddError(Error);return false;}TestEqual(TEXT("Level-three core identity is restored before validation"),Loaded.CoreDefinition,Upgrade.CoreDefinition);TestEqual(TEXT("Upgrading never replaces the core entity"),Loaded.Buildings[0].Id,CoreId);
    FSeigeSimulation Armed;if(!CoreFixture(Armed,Error)){AddError(Error);return false;}const int Turret=FactoryFixture(Armed,Error,TEXT("turret"));if(!Turret){AddError(Error);return false;}Armed.Combat.BuildingState[Armed.Buildings[0].Id].Weapons.Empty();Armed.Combat.BuildingState[Armed.Buildings[0].Id].Cooldowns.Empty();Armed.PeriodicAttacksEnabled=true;Armed.TriggerWave();Armed.Enemies.SetNum(1);Armed.Enemies[0].Position=Armed.FindBuilding(Turret)->Position+FVector2D(300,0);Armed.StepCombat(.05);
    if(!TestTrue(TEXT("An ordinary defense building has fired before upgrade"),Armed.FindBuilding(Turret)->LastShotTime>=0))return false;
    for(const auto& P:Armed.Definition(*Armed.FindBuilding(Turret))->UpgradeCost)Armed.Buildings[0].Inventory.FindOrAdd(P.Key)+=FMath::Max(0.,P.Value-Armed.ConstructionAvailable(P.Key));
    if(!Armed.UpgradeBuilding(Turret,Error)||!Armed.Save(SavePath(TEXT("armed-upgrade")),Error)||!Loaded.Load(SavePath(TEXT("armed-upgrade")),Error)){AddError(Error);return false;}TestEqual(TEXT("An upgrading defense preserves legitimate prior shot history"),Loaded.FindBuilding(Turret)->LastShotTime,Armed.FindBuilding(Turret)->LastShotTime);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeStoredWorkerLifecycleTest,"Seige.Simulation.Workforce.StoredBodiesAndDisassembly",Flags)
bool FSeigeStoredWorkerLifecycleTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!CoreFixture(S,Error)){AddError(Error);return false;}auto& C=S.Buildings[0];const FString Body=S.TextRule(TEXT("inactive_worker_resource"));const int Jobs=S.TotalJobs;
    auto ManufactureStored=[](FSeigeSimulation& S,int32 Count,FString& Error)
{auto& B=S.Buildings[0];S.SetWorkerSurplusTarget(Count,Error);const FString R=S.TextRule(TEXT("population_recipe"));for(int32 I=0;I<Count;++I){const int Before=S.Workers.StoredAt(B.Id);S.StepRecipe(B,S.ProductionSeconds(B,R)/S.WorkFraction(B)+.01);if(S.Workers.StoredAt(B.Id)!=Before+1){Error=TEXT("Finite paid worker fixture could not assemble a body");return false;}}return true;};
    if(!ManufactureStored(S,3,Error)){AddError(Error);return false;}
    S.SetWorkerSurplusTarget(2,Error);S.Time+=S.Number(TEXT("worker_reactivate_seconds"))*2;S.Calendar.Advance(S.Number(TEXT("worker_reactivate_seconds"))*2);S.Workers.Tick(S,S.Number(TEXT("worker_reactivate_seconds"))*2);S.Workers.RefreshMetrics(S);
    TestEqual(TEXT("Existing stored body fills the real logistics vacancy"),S.Population,Jobs);TestEqual(TEXT("Two paid bodies remain in their physical berths"),C.Inventory.FindRef(Body),2.);
    TestTrue(TEXT("Stored bodies add their configured litres"),S.InventoryLitres({{Body,2}})==S.Resources[Body].LitresPerUnit*2);
    auto* Lost=S.Workers.Bodies.FindByPredicate([](const auto& W){return W.State==TEXT("active")&&W.Activity==TEXT("operate");});if(!Lost)return false;Lost->State=TEXT("destroyed");Lost->Activity=TEXT("terminal");Lost->Outdoor=false;Lost->BuildingId=0;Lost->Route.Empty();Lost->NextWaypoint=0;S.Workers.RefreshMetrics(S);
    S.SetWorkerSurplusTarget(1,Error);S.Time+=S.Number(TEXT("worker_reactivate_seconds"));S.Calendar.Advance(S.Number(TEXT("worker_reactivate_seconds")));S.Workers.Tick(S,S.Number(TEXT("worker_reactivate_seconds")));S.Workers.RefreshMetrics(S);
    TestEqual(TEXT("Existing stored body fills a vacancy before manufacturing"),S.Population,Jobs);TestEqual(TEXT("Reactivation consumes exactly one stored body"),C.Inventory.FindRef(Body),1.);
    S.SetWorkerSurplusTarget(1,Error);TestFalse(TEXT("Reserve target prevents recycling the last protected body"),S.DisassembleWorkers(C.Id,1,Error));
    S.SetWorkerSurplusTarget(0,Error);const auto Returns=S.DisassemblyOutputs();TMap<FString,double> Before=C.Inventory;const double BeforeEnergy=S.Energy.ConsumedKWh;
    if(!S.DisassembleWorkers(C.Id,1,Error)){AddError(Error);return false;}S.StepWorkerDisassembly(.05);
    TestEqual(TEXT("Recycling never deletes an active worker"),S.Population,Jobs);TestEqual(TEXT("One body enters paid disassembly escrow"),C.Inventory.FindRef(Body),0.);
    TestTrue(TEXT("Disassembly charges the authored kWh exactly once"),FMath::IsNearlyEqual(S.Energy.ConsumedKWh-BeforeEnergy,S.DisassemblyEnergyKWh(),1.e-8));
    if(!S.Save(SavePath(TEXT("disassembly")),Error)||!Loaded.Initialize(Rules(),Error)||!Loaded.Load(SavePath(TEXT("disassembly")),Error)){AddError(Error);return false;}
    const double Remaining=(1-C.DisassemblyProgress)*S.Number(TEXT("worker_disassemble_seconds"))/S.WorkFraction(C);S.StepWorkerDisassembly(Remaining);Loaded.StepWorkerDisassembly(Remaining);
    for(const auto& P:Returns){TestEqual(TEXT("Finished disassembly returns configured physical parts"),C.Inventory.FindRef(P.Key),Before.FindRef(P.Key)+P.Value);TestEqual(TEXT("Reloaded disassembly produces the same parts once"),Loaded.Buildings[0].Inventory.FindRef(P.Key),C.Inventory.FindRef(P.Key));}
    TestEqual(TEXT("Completed body count is persisted"),Loaded.WorkersDisassembled,1);
    TestFalse(TEXT("Active workforce cannot be recycled through the surplus API"),S.DisassembleWorkers(C.Id,1,Error));
    TSharedPtr<FJsonObject> Bad;if(!Read(SavePath(TEXT("disassembly")),Bad))return false;Bad->GetArrayField(TEXT("buildings"))[0]->AsObject()->GetObjectField(TEXT("inventory"))->SetNumberField(Body,.5);Write(SavePath(TEXT("fractional-body")),Bad);
    const double Time=Loaded.Time;TestFalse(TEXT("Fractional stored body save is rejected"),Loaded.Load(SavePath(TEXT("fractional-body")),Error));TestEqual(TEXT("Rejected worker save is atomic"),Loaded.Time,Time);
    FSeigeSimulation Full;if(!CoreFixture(Full,Error)){AddError(Error);return false;}auto& F=Full.Buildings[0];F.Inventory.Empty();const FString Part=Returns.CreateConstIterator().Key();const double RefundL=Full.InventoryLitres(Returns);F.Inventory.Add(Part,(Full.Definition(F)->StorageCapacity-RefundL)/Full.Resources[Part].LitresPerUnit);for(int I=0;I<2;++I){Full.Workers.NewStored(Full,F.Id,1);auto& W=Full.Workers.Bodies.Last();W.State=TEXT("active");W.Activity=TEXT("idle");W.Outdoor=true;W.ContainerKind.Empty();W.ContainerId=0;W.Position=Full.BuildingAccessPoint(F);}Full.Workers.RefreshMetrics(Full);
    Full.Time+=1;Full.Calendar.Advance(1);Full.Workers.Tick(Full,1);Full.Time+=1;Full.Calendar.Advance(1);Full.Workers.Tick(Full,1);
    TestTrue(TEXT("Storage overflow enters paid recycling only for an unassigned body"),F.DisassemblyCommitted&&Full.Population==Full.TotalJobs);TestTrue(TEXT("Overflow recycling reserves exactly the remaining storage for recovered parts"),FMath::IsNearlyEqual(Full.Occupied(F),Full.Definition(F)->StorageCapacity,1.e-8));
    FSeigeSimulation Blocked;if(!CoreFixture(Blocked,Error)){AddError(Error);return false;}auto& NoRoom=Blocked.Buildings[0];NoRoom.Inventory.Empty();
    NoRoom.Inventory.Add(Part,(Blocked.Definition(NoRoom)->StorageCapacity-RefundL+.001)/Blocked.Resources[Part].LitresPerUnit);for(int I=0;I<2;++I){Blocked.Workers.NewStored(Blocked,NoRoom.Id,1);auto& W=Blocked.Workers.Bodies.Last();W.State=TEXT("active");W.Activity=TEXT("idle");W.Outdoor=true;W.ContainerKind.Empty();W.ContainerId=0;W.Position=Blocked.BuildingAccessPoint(NoRoom);}Blocked.Workers.RefreshMetrics(Blocked);const double UnspentEnergy=Blocked.Energy.ConsumedKWh;
    Blocked.Time+=1;Blocked.Calendar.Advance(1);Blocked.Workers.Tick(Blocked,1);Blocked.Time+=1;Blocked.Calendar.Advance(1);Blocked.Workers.Tick(Blocked,1);
    TestFalse(TEXT("A real recovered-parts capacity shortage blocks recycling"),NoRoom.DisassemblyCommitted);TestEqual(TEXT("Capacity shortage preserves the unassigned worker"),Blocked.Population,Blocked.TotalJobs+1);TestEqual(TEXT("Blocked recycling charges no energy"),Blocked.Energy.ConsumedKWh,UnspentEnergy);
    FSeigeSimulation Trade,TradeLoaded;if(!CoreFixture(Trade,Error)){AddError(Error);return false;}const int Port=FactoryFixture(Trade,Error,TEXT("trading_port"));if(!Port){AddError(Error);return false;}
    Trade.Buildings[0].Inventory.Add(Body,1);Trade.Workers.NewStored(Trade,Trade.Buildings[0].Id,1);Trade.FindBuilding(Port)->Inventory.Add(Body,3);Trade.Workers.NewStored(Trade,Port,3);Trade.SetWorkerSurplusTarget(1,Error);Trade.SetPortWorkerTarget(Port,3,Error);Trade.AllocateWorkers();Trade.Energy.Tick(Trade,0);
    TestFalse(TEXT("Worker imports must be whole bodies"),Trade.CanTrade(Port,Body,.5,true,Error));TestFalse(TEXT("Worker exports must be whole bodies"),Trade.CanTrade(Port,Body,.5,false,Error));
    const int32 OverCapacity=FMath::FloorToInt(Trade.Trade.Definition(Trade.FindBuilding(Port)->DefId)->CapacityKg/Trade.Resources[Body].UnitMassKg)+1;
    TestFalse(TEXT("A shipment cannot carry more whole bodies than its authored mass capacity"),Trade.CanTrade(Port,Body,OverCapacity,false,Error));
    if(!Trade.TryTrade(Port,Body,1,false,Error)){AddError(Error);return false;}const double Earned=Trade.TradeQuote(Body,1,false);Trade.Trade.Tick(Trade,.05);
    TestTrue(TEXT("Port target workers enter real shipment escrow"),Trade.FindBuilding(Port)->Shipment.Departed&&Trade.FindBuilding(Port)->Shipment.GoodsEscrow==1);TestEqual(TEXT("In-flight workers remain in the material ledger"),Trade.InactiveWorkerCount(),4);
    FSeigeSimulation LostPort=Trade,LostPortLoaded;LostPort.OnBuildingDestroyed(Port);
    TestFalse(TEXT("Destroyed port cannot retain a shipment body after its escrow is lost"),LostPort.Workers.Bodies.ContainsByPredicate([&](const auto& W){return W.ContainerId==Port&&W.State==TEXT("shipment");}));
    if(!LostPort.Save(SavePath(TEXT("lost-worker-export")),Error)||!LostPortLoaded.Initialize(Rules(),Error,false,false)||!LostPortLoaded.Load(SavePath(TEXT("lost-worker-export")),Error)){AddError(Error);return false;}
    if(!Trade.Save(SavePath(TEXT("worker-export")),Error)||!TradeLoaded.Initialize(Rules(),Error)||!TradeLoaded.Load(SavePath(TEXT("worker-export")),Error)){AddError(Error);return false;}Trade.Trade.Tick(Trade,1000);TradeLoaded.Trade.Tick(TradeLoaded,1000);
    TestEqual(TEXT("Completed body export pays its exact external price"),Trade.Credits,Earned);TestEqual(TEXT("Loading an in-flight body export does not duplicate credits"),TradeLoaded.Credits,Trade.Credits);TestEqual(TEXT("One export leaves the colony reserve and two unsold port bodies"),Trade.InactiveWorkerCount(),3);
    return true;
}
#endif
