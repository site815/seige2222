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
    {W.Activity=TEXT("operate");W.BuildingId=Needed-->0?Workplace:S.Buildings[0].Id;W.CrewBuildingId=W.BuildingId;W.RoadId=W.DeliveryId=W.ContainerId=W.ChargingBuildingId=W.TransportVehicleId=0;W.ContainerKind.Empty();W.Outdoor=true;W.Route.Empty();W.NextWaypoint=0;W.StationSlot=Slot++;W.Position=S.BuildingAccessPoint(*S.FindBuilding(W.BuildingId))+FVector2D(0,20+Slot*12);}
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);
}
void CancelFixtureBatch(FSeigeBuilding& B)
{for(const auto& P:B.ProductionInputs)B.Inventory.FindOrAdd(P.Key)+=P.Value;B.ProductionInputs.Empty();B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.ProductionReservedLitres=B.Progress=0;}
bool CoreFixture(FSeigeSimulation& S,FString& Error)
{if(!S.Initialize(Rules(),Error,false,false))return false;for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);if(S.Buildings[0].IsConstructing){Error=TEXT("Real starter crew failed deployment");return false;}S.Tick(20);for(auto& Dog:S.Companions.Dogs){Dog.Position=Dog.Target=S.Buildings[0].Position+FVector2D(-1500,-1500);Dog.Route.Empty();Dog.NextWaypoint=0;Dog.Moving=false;Dog.RestUntil=S.Time;Dog.MotionSpeedMetersPerSecond=Dog.SpeedMetersPerSecond=0;S.Companions.SetControlled(Dog.Id);}CancelFixtureBatch(S.Buildings[0]);FixtureWorkers(S);return true;}
int FactoryFixture(FSeigeSimulation& S,FString& Error,const FString& Definition=TEXT("worker_factory"))
{
    if(!S.PlaceBuilding(Definition,FVector2D(1300,0),Error))return 0;auto& B=S.Buildings.Last();const auto Cost=S.ConstructionCost(B);
    for(const auto& P:Cost)S.Buildings[0].Inventory.FindOrAdd(P.Key)-=P.Value;B.InstalledMaterials=Cost;B.ConstructionProgress=1;B.IsConstructing=false;B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;const int Id=B.Id;
    FixtureWorkers(S,Id);return Id;
}
bool Read(const FString& P,TSharedPtr<FJsonObject>& O){FString Text;return FFileHelper::LoadFileToString(Text,*P)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O);}
bool Write(const FString& P,const TSharedPtr<FJsonObject>& O){FString Text;return FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*P);}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeReplicatorWorkforceTest,"Seige.Simulation.Workforce.ReplicatorAndFactory",Flags)
bool FSeigeReplicatorWorkforceTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!CoreFixture(S,Error)){AddError(Error);return false;}
    const FString Worker=S.TextRule(TEXT("population_recipe")),Body=S.TextRule(TEXT("inactive_worker_resource"));
    TestEqual(TEXT("Command center offers exactly its authored worker recipe"),S.ProductionOptions(S.Buildings[0].Id).Num(),1);
    TestTrue(TEXT("The offered command recipe manufactures physical workers"),S.ProductionOptions(S.Buildings[0].Id).Contains(Worker));
    TestFalse(TEXT("Command cannot manufacture specialist goods"),S.SetProductionRecipe(S.Buildings[0].Id,TEXT("make_shells"),Error));
    auto& Core=S.Buildings[0];const auto Inputs=S.ProductionInputs(Core,Worker);Core.Inventory=Inputs;
    const double Energy=S.Energy.ConsumedKWh;const int32 InitialBodies=S.Workers.Bodies.Num();
    S.StepRecipe(Core,.01);
    TestTrue(TEXT("Paid core worker batch commits its real recipe"),Core.ProductionCommitted&&Core.CommittedRecipe==Worker);
    TestTrue(TEXT("Core debits exact configured batch energy once"),FMath::IsNearlyEqual(S.Energy.ConsumedKWh-Energy,S.ProductionEnergy(Core,Worker),1.e-8));
    for(const auto& P:Inputs){TestEqual(TEXT("Input is physically escrowed"),Core.ProductionInputs.FindRef(P.Key),P.Value);TestEqual(TEXT("Escrow removes local available input"),Core.Inventory.FindRef(P.Key),0.);}
    TestEqual(TEXT("Beginning a batch creates no worker identity"),S.Workers.Bodies.Num(),InitialBodies);
    if(!S.Save(SavePath(TEXT("replicator-batch")),Error)||!Loaded.Initialize(Rules(),Error,false,false)||!Loaded.Load(SavePath(TEXT("replicator-batch")),Error)){AddError(Error);return false;}
    const double Remaining=(1-Core.Progress)*S.ProductionSeconds(Core,Worker)/S.WorkFraction(Core);
    S.StepRecipe(Core,Remaining-.1);Loaded.StepRecipe(Loaded.Buildings[0],Remaining-.1);
    TestEqual(TEXT("Worker cannot emerge before the whole physical cycle"),Core.Inventory.FindRef(Body),0.);
    S.StepRecipe(Core,.1);Loaded.StepRecipe(Loaded.Buildings[0],.1);
    TestEqual(TEXT("Completed paid core batch creates one stored worker"),Core.Inventory.FindRef(Body),1.);
    TestEqual(TEXT("One completed batch creates exactly one identity"),S.Workers.Bodies.Num(),InitialBodies+1);
    TestEqual(TEXT("Reloaded batch produces the same worker once"),Loaded.Workers.Bodies.Last().Id,S.Workers.Bodies.Last().Id);
    TestFalse(TEXT("Completed batch releases escrow"),Core.ProductionCommitted);
    TestTrue(TEXT("Partial stepping charges the worker batch once"),FMath::IsNearlyEqual(S.Energy.ConsumedKWh-Energy,S.ProductionEnergy(Core,Worker),1.e-8));

    FSeigeSimulation Factory;if(!CoreFixture(Factory,Error)){AddError(Error);return false;}
    const int HubId=FactoryFixture(Factory,Error);if(!HubId){AddError(Error);return false;}
    const auto& Hub=*Factory.FindBuilding(HubId);
    TestEqual(TEXT("Legacy worker-factory ID now describes a construction hub"),Factory.Definition(Hub)->Role,FString(TEXT("construction")));
    TestEqual(TEXT("Construction hub retains four permanent specialist posts"),Factory.Definition(Hub)->Jobs,4);
    TestTrue(TEXT("Construction hub has no manufacturing recipes"),Factory.ProductionOptions(HubId).IsEmpty());
    TestFalse(TEXT("Construction hub cannot bypass core-only worker manufacture"),Factory.SetProductionRecipe(HubId,Worker,Error));

    FSeigeSimulation GoodsSim;if(!CoreFixture(GoodsSim,Error)){AddError(Error);return false;}
    const int GoodsId=FactoryFixture(GoodsSim,Error,TEXT("ammunition_works"));if(!GoodsId){AddError(Error);return false;}
    auto& Works=*GoodsSim.FindBuilding(GoodsId);const FString Goods=TEXT("make_shells");const auto Recipe=GoodsSim.Recipes[Goods];Works.Inventory=GoodsSim.ProductionInputs(Works,Goods);
    if(!GoodsSim.SetProductionRecipe(GoodsId,Goods,Error)){AddError(Error);return false;}
    const double GoodsEnergy=GoodsSim.Energy.ConsumedKWh;GoodsSim.StepRecipe(Works,.01);
    if(!GoodsSim.SetProductionRecipe(GoodsId,TEXT("make_missiles"),Error)||!GoodsSim.Save(SavePath(TEXT("specialist-batch")),Error)||!Loaded.Load(SavePath(TEXT("specialist-batch")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Selection change cannot replace a paid specialist batch"),Loaded.FindBuilding(GoodsId)->CommittedRecipe,Goods);
    const double GoodsRemaining=(1-Works.Progress)*GoodsSim.ProductionSeconds(Works,Goods)/GoodsSim.WorkFraction(Works);
    GoodsSim.StepRecipe(Works,GoodsRemaining);Loaded.StepRecipe(*Loaded.FindBuilding(GoodsId),GoodsRemaining);
    for(const auto& P:Recipe.Outputs){TestEqual(TEXT("Specialist produces its configured physical output"),Works.Inventory.FindRef(P.Key),P.Value);TestEqual(TEXT("Restored specialist outputs once"),Loaded.FindBuilding(GoodsId)->Inventory.FindRef(P.Key),P.Value);}
    TestTrue(TEXT("Specialist charges batch energy exactly once"),FMath::IsNearlyEqual(GoodsSim.Energy.ConsumedKWh-GoodsEnergy,GoodsSim.ProductionEnergy(Works,Goods),1.e-8));

    FSeigeSimulation CapacityTest;if(!CoreFixture(CapacityTest,Error)){AddError(Error);return false;}
    const int32 CapacityDonor=FactoryFixture(CapacityTest,Error);if(!CapacityDonor){AddError(Error);return false;}
    auto& Limited=CapacityTest.Buildings[0];const FString Repair=CapacityTest.TextRule(TEXT("repair_resource")),Filler=TEXT("water");
    CapacityTest.FindBuilding(CapacityDonor)->Inventory.Add(Repair,1);Limited.Inventory=CapacityTest.ProductionInputs(Limited,Worker);
    const double InputLitres=CapacityTest.InventoryLitres(Limited.Inventory),OutputLitres=CapacityTest.ProductionOutputLitres(CapacityTest.Recipes[Worker]);
    Limited.Inventory.FindOrAdd(Filler)+=(CapacityTest.Definition(Limited)->StorageCapacity-OutputLitres)/CapacityTest.Resources[Filler].LitresPerUnit;
    if(!TestTrue(TEXT("Stored-worker production expands physical occupied volume"),OutputLitres>InputLitres)||!CapacityTest.Workers.Dispatch(CapacityTest,CapacityDonor,Limited.Id,0,Repair,1,false)){AddError(TEXT("Core capacity probe requires an actual incoming load"));return false;}
    const double Uncharged=CapacityTest.Energy.ConsumedKWh,InputsBefore=CapacityTest.InventoryLitres(Limited.Inventory);CapacityTest.StepRecipe(Limited,.01);
    TestFalse(TEXT("A batch cannot occupy a promised incoming delivery berth"),Limited.ProductionCommitted);
    TestEqual(TEXT("Capacity-blocked assembly preserves its local input bill"),CapacityTest.InventoryLitres(Limited.Inventory),InputsBefore);
    TestEqual(TEXT("Capacity-blocked assembly consumes no batch energy"),CapacityTest.Energy.ConsumedKWh,Uncharged);
    FSeigeSimulation Reserved;if(!CoreFixture(Reserved,Error)){AddError(Error);return false;}auto& ReservedCore=Reserved.Buildings[0];
    const auto WorkerInputs=Reserved.ProductionInputs(ReservedCore,Worker);const FString Parts=TEXT("components");const double Bill=Reserved.BuildingDefs[TEXT("worker_factory")].Cost.FindRef(Parts);
    const double Buffer=ReservedCore.Inventory.FindRef(Parts)-Reserved.ConstructionAvailable(Parts);ReservedCore.Inventory[Parts]=Bill+Buffer;
    if(!Reserved.PlaceBuilding(TEXT("worker_factory"),FVector2D(1300,0),Error)){AddError(Error);return false;}
    auto& ProtectedCore=Reserved.Buildings[0];const int32 StoredBefore=Reserved.Workers.StoredAt(ProtectedCore.Id);for(int32 Attempt=0;Attempt<6;++Attempt)Reserved.StepRecipe(ProtectedCore,Reserved.ProductionSeconds(ProtectedCore,Worker)/Reserved.WorkFraction(ProtectedCore)+.01);
    TestTrue(TEXT("Successive paid worker batches never consume an accepted construction bill"),ProtectedCore.Inventory.FindRef(Parts)+1.e-8>=Bill);
    TestEqual(TEXT("Only genuinely unreserved whole worker batches may finish"),Reserved.Workers.StoredAt(ProtectedCore.Id)-StoredBefore,int32(FMath::FloorToInt((Buffer+1.e-8)/WorkerInputs.FindRef(Parts))));
    TestTrue(TEXT("The remaining protected parts cannot fund another assembly batch"),Reserved.Spendable(ProtectedCore,Parts)+1.e-8<WorkerInputs.FindRef(Parts));
    const int32 PromisedSite=Reserved.Buildings.Last().Id;
    for(int32 I=0;I<1200&&Reserved.FindBuilding(PromisedSite)->IsConstructing;++I)Reserved.Tick(10);
    TestFalse(TEXT("Protected stock is physically delivered and installed by the finite crew"),Reserved.FindBuilding(PromisedSite)->IsConstructing);
    TestTrue(TEXT("The completed factory embodies its whole promised parts bill"),FMath::IsNearlyEqual(Reserved.FindBuilding(PromisedSite)->InstalledMaterials.FindRef(Parts),Bill,1.e-8));
    FSeigeSimulation Distributed;if(!CoreFixture(Distributed,Error)){AddError(Error);return false;}const int32 DonorId=FactoryFixture(Distributed,Error,TEXT("ammunition_works"));if(!DonorId){AddError(Error);return false;}
    auto* Donor=Distributed.FindBuilding(DonorId);const double SmallBill=Distributed.BuildingDefs[TEXT("sensor")].Cost.FindRef(Parts);
    const int32 BufferBatches=FMath::CeilToInt(SmallBill/Distributed.ProductionInputs(*Donor,TEXT("make_shells")).FindRef(Parts));
    for(const auto& P:Distributed.ProductionInputs(*Donor,TEXT("make_shells"))){const double Amount=P.Value*BufferBatches;Distributed.Buildings[0].Inventory.FindOrAdd(P.Key)-=Amount;Donor->Inventory.FindOrAdd(P.Key)+=Amount;}
    Distributed.Buildings[0].Inventory[Parts]=Distributed.Demand(Distributed.Buildings[0],Parts,false);Donor->Inventory[Parts]=Distributed.Demand(*Donor,Parts,false)+SmallBill;
    if(!Distributed.PlaceBuilding(TEXT("sensor"),FVector2D(0,1300),Error)){AddError(Error);return false;}const int32 BufferedSite=Distributed.Buildings.Last().Id;Donor=Distributed.FindBuilding(DonorId);
    // Finite bodies: the rest of the bill is already on site so the first
    // construction claim of the dispatch round is the buffered parts, not alloy.
    {auto* Site=Distributed.FindBuilding(BufferedSite);for(const auto& P:Distributed.ConstructionCost(*Site))if(P.Key!=Parts){Distributed.Buildings[0].Inventory.FindOrAdd(P.Key)-=P.Value;Site->ConstructionMaterials.Add(P.Key,P.Value);}}
    for(int32 I=0;I<BufferBatches;++I)Distributed.StepRecipe(*Donor,Distributed.ProductionSeconds(*Donor,TEXT("make_shells"))/Distributed.WorkFraction(*Donor)+.01);
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
    // The parked transport occupies the eastern hatch approach. Preserve that
    // real friendly body and exercise shot history on the unobstructed west side.
    Upgrade.PeriodicAttacksEnabled=true;Upgrade.TriggerWave();Upgrade.Enemies.SetNum(1);Upgrade.Enemies[0].Position=FVector2D(-1000,0);Upgrade.StepCombat(.05);TestTrue(TEXT("Core has fired before upgrading"),Upgrade.Buildings[0].LastShotTime>=0);
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
    FString Error;FSeigeSimulation S,Loaded;if(!CoreFixture(S,Error)){AddError(Error);return false;}
    const int ReservePort=FactoryFixture(S,Error,TEXT("trading_port"));if(!ReservePort){AddError(Error);return false;}
    auto& C=S.Buildings[0];const FString Body=S.TextRule(TEXT("inactive_worker_resource"));const int Jobs=S.TotalJobs;
    auto ManufactureStored=[](FSeigeSimulation& Sim,int32 Count,FString& OutError)
    {
        auto& B=Sim.Buildings[0];const FString R=Sim.TextRule(TEXT("population_recipe"));
        for(int32 I=0;I<Count;++I)
        {
            const int Before=Sim.Workers.StoredAt(B.Id);
            // Isolate production from reserve-hauling, but advance its actual
            // elapsed time and grid ledger. Consecutive paid cycles cannot draw
            // indefinitely from the starting battery without fusion generation.
            const double Limit=Sim.ProductionSeconds(B,R)/FMath::Max(.001,Sim.WorkFraction(B))*2+3600.;
            for(double Elapsed=0;Elapsed<Limit&&Sim.Workers.StoredAt(B.Id)==Before;Elapsed+=10)
            {
                Sim.Time+=10;Sim.Calendar.Advance(10);Sim.Energy.Tick(Sim,10);Sim.StepRecipe(B,10);
                // These fixtures disable both threat sources; their schedules
                // still advance, as in Tick, so the saved world has no backlog.
                while(Sim.NextWaveTime<=Sim.Time)Sim.NextWaveTime+=Sim.Number(TEXT("wave_interval"));
                while(Sim.NextRoamTime<=Sim.Time)Sim.NextRoamTime+=Sim.Number(TEXT("roam_interval"));
            }
            if(Sim.Workers.StoredAt(B.Id)!=Before+1)
            {
                OutError=FString::Printf(TEXT("Paid worker fixture batch %d/%d failed: %s; work %.3f; stored energy %.3f; needed %d; committed %d"),I+1,Count,*B.Status,Sim.WorkFraction(B),Sim.Energy.Info(Sim,B.Id).StoredKWh,Sim.WorkersNeeded(),B.ProductionCommitted?1:0);
                for(const auto& P:Sim.ProductionInputs(B,R))OutError+=FString::Printf(TEXT("; %s %.3f/%.3f"),*P.Key,Sim.Spendable(B,P.Key),P.Value);
                return false;
            }
        }
        return true;
    };
    // Complete the actual facility crew vacancies using paid bodies, then retain
    // two packed workers. No phantom logistics posts or population grants.
    for(int32 I=0;I<1500&&(S.Population<Jobs||S.InactiveWorkerCount()<2);++I)S.Tick(10);
    CancelFixtureBatch(C);FixtureWorkers(S,ReservePort);
    TestEqual(TEXT("Existing manufactured bodies fill permanent facility crews"),S.Population,Jobs);
    if(!TestEqual(TEXT("Two paid bodies remain in their physical berths"),C.Inventory.FindRef(Body),2.))return false;
    TestTrue(TEXT("Stored bodies add their configured litres"),S.InventoryLitres({{Body,2}})==S.Resources[Body].LitresPerUnit*2);
    auto* Lost=S.Workers.Bodies.FindByPredicate([](const auto& W){return W.State==TEXT("active")&&W.Activity==TEXT("operate");});if(!Lost)return false;
    Lost->Health=0;Lost->State=TEXT("destroyed");Lost->Activity=TEXT("terminal");Lost->Outdoor=false;Lost->BuildingId=Lost->CrewBuildingId=0;Lost->Route.Empty();Lost->NextWaypoint=0;S.Workers.RefreshMetrics(S);
    S.Time+=S.Number(TEXT("worker_reactivate_seconds"));S.Calendar.Advance(S.Number(TEXT("worker_reactivate_seconds")));S.Workers.Tick(S,S.Number(TEXT("worker_reactivate_seconds")));S.Workers.RefreshMetrics(S);
    TestEqual(TEXT("Existing stored body fills a vacancy before manufacturing"),S.Population,Jobs);TestEqual(TEXT("Reactivation consumes exactly one stored body"),C.Inventory.FindRef(Body),1.);
    TestFalse(TEXT("Command reserve prevents recycling protected bodies"),S.DisassembleWorkers(C.Id,1,Error));
    if(!S.SetPortWorkerTarget(ReservePort,1,Error)){AddError(Error);return false;}
    FixtureWorkers(S,ReservePort);
    if(!ManufactureStored(S,S.WorkerReserveTarget()-S.InactiveWorkerCount(),Error)){AddError(Error);return false;}
    TestFalse(TEXT("Trade reserve also protects paid stored identities"),S.DisassembleWorkers(C.Id,1,Error));
    if(!S.SetPortWorkerTarget(ReservePort,0,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Lowering trade reserve exposes exactly one genuinely excess body"),S.DisassemblyAvailable(C),1.);
    const auto Returns=S.DisassemblyOutputs();TMap<FString,double> Before=C.Inventory;const double BeforeEnergy=S.Energy.ConsumedKWh;
    if(!S.DisassembleWorkers(C.Id,1,Error)){AddError(Error);return false;}S.StepWorkerDisassembly(.05);
    TestEqual(TEXT("Recycling never deletes an active worker"),S.Population,Jobs);TestEqual(TEXT("One body enters paid disassembly escrow"),C.Inventory.FindRef(Body),double(S.WorkerSurplusTarget()));
    TestTrue(TEXT("Disassembly charges the authored kWh exactly once"),FMath::IsNearlyEqual(S.Energy.ConsumedKWh-BeforeEnergy,S.DisassemblyEnergyKWh(),1.e-8));
    if(!S.Save(SavePath(TEXT("disassembly")),Error)||!Loaded.Initialize(Rules(),Error)||!Loaded.Load(SavePath(TEXT("disassembly")),Error)){AddError(Error);return false;}
    const double Remaining=(1-C.DisassemblyProgress)*S.Number(TEXT("worker_disassemble_seconds"))/S.WorkFraction(C);S.StepWorkerDisassembly(Remaining);Loaded.StepWorkerDisassembly(Remaining);
    for(const auto& P:Returns){TestEqual(TEXT("Finished disassembly returns configured physical parts"),C.Inventory.FindRef(P.Key),Before.FindRef(P.Key)+P.Value);TestEqual(TEXT("Reloaded disassembly produces the same parts once"),Loaded.Buildings[0].Inventory.FindRef(P.Key),C.Inventory.FindRef(P.Key));}
    TestEqual(TEXT("Completed body count is persisted"),Loaded.WorkersDisassembled,1);
    TestFalse(TEXT("Active workforce cannot be recycled through the surplus API"),S.DisassembleWorkers(C.Id,1,Error));
    TSharedPtr<FJsonObject> Bad;if(!Read(SavePath(TEXT("disassembly")),Bad))return false;Bad->GetArrayField(TEXT("buildings"))[0]->AsObject()->GetObjectField(TEXT("inventory"))->SetNumberField(Body,.5);Write(SavePath(TEXT("fractional-body")),Bad);
    const double Time=Loaded.Time;TestFalse(TEXT("Fractional stored body save is rejected"),Loaded.Load(SavePath(TEXT("fractional-body")),Error));TestEqual(TEXT("Rejected worker save is atomic"),Loaded.Time,Time);
    FSeigeSimulation Full=S;const int32 BeforeBodies=Full.Workers.Bodies.Num();
    auto* Excess=Full.Workers.Bodies.FindByPredicate([](const auto& W){return W.State==TEXT("stored");});if(!Excess)return false;
    const FString ExcessId=Excess->Id;Full.FindBuilding(Excess->ContainerId)->Inventory.FindOrAdd(Body)-=1;
    Excess->State=TEXT("active");Excess->Activity=TEXT("idle");Excess->Outdoor=true;Excess->CrewBuildingId=Excess->BuildingId=Excess->ContainerId=0;Excess->ContainerKind.Empty();Excess->Position=Full.BuildingAccessPoint(Full.Buildings[0]);
    for(auto& B:Full.Buildings)if(Full.Definition(B)->StoresInactiveWorkers)B.Inventory.FindOrAdd(TEXT("water"))+=Full.StorageRoom(B)/Full.Resources[TEXT("water")].LitresPerUnit;
    Full.Workers.RefreshMetrics(Full);const double UnspentEnergy=Full.Energy.ConsumedKWh;
    Full.Time+=1;Full.Calendar.Advance(1);Full.Workers.Tick(Full,1);
    TestFalse(TEXT("Storage overflow cannot silently recycle a physical worker"),Full.Buildings[0].DisassemblyCommitted);
    TestEqual(TEXT("Full storage retains the same intact surplus identity"),Full.Workers.Find(ExcessId)->State,FString(TEXT("active")));
    TestEqual(TEXT("Full storage neither creates nor deletes bodies"),Full.Workers.Bodies.Num(),BeforeBodies);
    TestEqual(TEXT("No implicit recycling energy is charged"),Full.Energy.ConsumedKWh,UnspentEnergy);
    FSeigeSimulation Trade=S,TradeLoaded;const int Port=ReservePort;
    if(!Trade.SetPortWorkerTarget(Port,3,Error)){AddError(Error);return false;}
    // Build and physically deliver real export stock above the five-body
    // command reserve, with the port crew vacancy already filled by a paid body.
    for(int32 I=0;I<3000&&(Trade.Population<Trade.TotalJobs||Trade.Workers.StoredAt(Port)<3||Trade.InactiveWorkerCount()<Trade.WorkerReserveTarget());++I)Trade.Tick(10);
    if(!TestEqual(TEXT("Paid export stock is physically delivered to the port"),Trade.Workers.StoredAt(Port),3)||
       !TestEqual(TEXT("Export stock leaves the whole command reserve intact"),Trade.Workers.StoredAt(Trade.Buildings[0].Id),Trade.WorkerSurplusTarget()))return false;
    const int BeforeExport=Trade.InactiveWorkerCount();
    TestFalse(TEXT("Worker imports must be whole bodies"),Trade.CanTrade(Port,Body,.5,true,Error));TestFalse(TEXT("Worker exports must be whole bodies"),Trade.CanTrade(Port,Body,.5,false,Error));
    const int32 OverCapacity=FMath::FloorToInt(Trade.Trade.Definition(Trade.FindBuilding(Port)->DefId)->CapacityKg/Trade.Resources[Body].UnitMassKg)+1;
    TestFalse(TEXT("A shipment cannot carry more whole bodies than its authored mass capacity"),Trade.CanTrade(Port,Body,OverCapacity,false,Error));
    if(!Trade.TryTrade(Port,Body,1,false,Error)){AddError(Error);return false;}const double Earned=Trade.TradeQuote(Body,1,false);Trade.Trade.Tick(Trade,.05);
    TestTrue(TEXT("Port target workers enter real shipment escrow"),Trade.FindBuilding(Port)->Shipment.Departed&&Trade.FindBuilding(Port)->Shipment.GoodsEscrow==1);TestEqual(TEXT("In-flight workers remain in the material ledger"),Trade.InactiveWorkerCount(),BeforeExport);
    FSeigeSimulation LostPort=Trade,LostPortLoaded;LostPort.OnBuildingDestroyed(Port);
    TestFalse(TEXT("Destroyed port cannot retain a shipment body after its escrow is lost"),LostPort.Workers.Bodies.ContainsByPredicate([&](const auto& W){return W.ContainerId==Port&&W.State==TEXT("shipment");}));
    if(!LostPort.Save(SavePath(TEXT("lost-worker-export")),Error)||!LostPortLoaded.Initialize(Rules(),Error,false,false)||!LostPortLoaded.Load(SavePath(TEXT("lost-worker-export")),Error)){AddError(Error);return false;}
    if(!Trade.Save(SavePath(TEXT("worker-export")),Error)||!TradeLoaded.Initialize(Rules(),Error)||!TradeLoaded.Load(SavePath(TEXT("worker-export")),Error)){AddError(Error);return false;}Trade.Trade.Tick(Trade,1000);TradeLoaded.Trade.Tick(TradeLoaded,1000);
    TestEqual(TEXT("Completed body export pays its exact external price"),Trade.Credits,Earned);TestEqual(TEXT("Loading an in-flight body export does not duplicate credits"),TradeLoaded.Credits,Trade.Credits);TestEqual(TEXT("One export leaves the colony reserve and two unsold port bodies"),Trade.InactiveWorkerCount(),BeforeExport-1);
    return true;
}
#endif
