#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
bool DevelopmentFixture(FSeigeSimulation& S,FSeigeScenarioAI& AI,FString& Error)
{
    if(!AI.Initialize(S,FPaths::ProjectDir()/TEXT("Rules"),FPaths::ProjectDir()/TEXT("AIFILES"),false,Error,false,false,815))return false;
    S.Environment.Enabled=false;S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);
    if(S.Buildings[0].IsConstructing){Error=TEXT("Finite workers did not finish deployment");return false;}
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIDevelopmentFundingTest,"Seige.AI.CompletedPlanFundsUpgrades",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIDevelopmentFundingTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(S,AI,Error)){AddError(Error);return false;}
    const FVector2D Origin=S.Buildings[0].Position;
    const int32 Array=S.AddReviewBuilding(TEXT("solar_array"),Origin+FVector2D(1500,0));
    const int32 Port=S.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(0,1500));
    // Installed-state decision fixture. The paid-growth acceptance separately
    // builds its facilities from the finite landing kit with physical delivery.
    AI.Targets.Empty();AI.ReserveTargets.Empty();AI.UpgradeFamilies={TEXT("solar_array")};AI.DesiredTransports=0;
    S.Buildings[0].Enabled=false; // Exclude an unrelated worker-input request.
    for(auto& B:S.Buildings){B.Inventory.Remove(TEXT("circuits"));B.ProductionInputs.Empty();B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.ProductionReservedLitres=0;}
    const double Need=S.BuildingDefs[TEXT("solar_array")].UpgradeCost[TEXT("circuits")];
    const auto Bill=AI.NextDevelopmentBill(S),Goals=AI.ProductionGoals(S);
    TestEqual(TEXT("A finished plan exposes the exact next upgrade bill"),Bill.FindRef(TEXT("circuits")),Need);
    TestTrue(TEXT("Specialist production sees the missing upgrade parts"),Goals.FindRef(TEXT("circuits"))>=Need);
    const double Protected=AI.ManufacturedExportSurplus(S,TEXT("alloy"));
    AI.MaxConcurrentUpgrades=0;
    TestTrue(TEXT("Upgrade construction stock stays protected from export"),AI.ManufacturedExportSurplus(S,TEXT("alloy"))>=Protected+S.BuildingDefs[TEXT("solar_array")].UpgradeCost[TEXT("alloy")]-1.e-8);
    AI.MaxConcurrentUpgrades=1;
    S.Credits=S.TradeQuote(TEXT("circuits"),Need,true);const int32 Bodies=S.Workers.Bodies.Num();
    if(!TestTrue(TEXT("The AI imports an unfunded upgrade prerequisite through its real port"),AI.ManageTrade(S)))return false;
    const auto& Order=S.FindBuilding(Port)->Shipment;
    TestTrue(TEXT("The order is the actual missing upgrade material"),Order.Buy&&Order.Resource==TEXT("circuits")&&FMath::IsNearlyEqual(Order.Quantity,Need,1.e-8));
    TestTrue(TEXT("Ordering pays the quote without inventing usable stock or workers"),FMath::IsNearlyZero(S.Credits,1.e-8)&&S.TotalStock(TEXT("circuits"))==0&&S.Workers.Bodies.Num()==Bodies);
    TestFalse(TEXT("Funding is not an instant upgrade"),S.FindBuilding(Array)->IsConstructing);
    FSeigeAIBuildTarget Awaiting;Awaiting.Definition=TEXT("solar_array");Awaiting.Count=1;AI.Targets.Add(Awaiting);
    TestTrue(TEXT("An unstaffed prerequisite does not become a completed plan"),AI.NextDevelopmentBill(S).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAITransportGrowthTest,"Seige.AI.PaidTransportGrowthAndReplacement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAITransportGrowthTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(S,AI,Error)){AddError(Error);return false;}
    AI.Targets.Empty();AI.UpgradeFamilies.Empty();AI.DesiredTransports=2;
    FSeigeAIBuildTarget Later;Later.Definition=TEXT("depot");Later.Count=1;AI.Targets.Add(Later);
    const int32 Factory=S.AddReviewBuilding(TEXT("vehicle_factory"),S.Buildings[0].Position+FVector2D(1800,0));
    if(!TestTrue(TEXT("The finite manifest has two real operators available for this installed fixture"),S.Workers.Bodies.Num()>=2))return false;
    S.Workers.Bodies[0].CrewBuildingId=Factory;S.Workers.Bodies[1].CrewBuildingId=Factory;
    const int32 OriginalVehicles=S.TransportVehicles.LiveCount(),Bodies=S.Workers.Bodies.Num();const double Alloy=S.TotalStock(TEXT("alloy"));
    if(!TestTrue(TEXT("Staffed factory queues a missing transport before the remaining city plan is complete"),AI.ManageTransport(S)))return false;
    TestEqual(TEXT("A queued transport is not instantly created"),S.TransportVehicles.LiveCount(),OriginalVehicles);
    TestTrue(TEXT("Queueing grants no mass or worker bodies"),S.TotalStock(TEXT("alloy"))==Alloy&&S.Workers.Bodies.Num()==Bodies);
    TestEqual(TEXT("Only the unmet target enters the ordinary fabrication queue"),S.TransportVehicles.Jobs.Num(),1);
    TestFalse(TEXT("A second decision does not duplicate an already accepted transport"),AI.ManageTransport(S));
    const auto& Cost=S.TransportVehicles.Definition().BuildCost;
    TestEqual(TEXT("Trade and production plan the actual vehicle alloy bill"),AI.NextDevelopmentBill(S).FindRef(TEXT("alloy")),Cost[TEXT("alloy")]);
    S.TransportVehicles.Bodies[0].Health=0;
    TestTrue(TEXT("Losing the carried truck queues a paid replacement"),AI.ManageTransport(S));
    TestEqual(TEXT("Growth plus replacement stays at the configured target"),S.TransportVehicles.Jobs.Num(),2);
    TestFalse(TEXT("Repeated decisions never exceed the replacement target"),AI.ManageTransport(S));
    TestEqual(TEXT("Both uncommitted bodies reserve their real material mass"),AI.TransportMaterialGoals(S).FindRef(TEXT("alloy")),2*Cost[TEXT("alloy")]);
    S.FindBuilding(Factory)->Enabled=false;
    TestTrue(TEXT("A disabled factory cannot promise new usable production"),AI.TransportMaterialGoals(S).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIProductionBudgetTest,"Seige.AI.SurplusProductionPreservesDevelopmentInputs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIProductionBudgetTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(S,AI,Error)){AddError(Error);return false;}
    const int32 Core=S.Buildings[0].Id;
    const int32 Processor=S.AddReviewBuilding(TEXT("component_works"),S.Buildings[0].Position+FVector2D(1800,0));
    // Installed-state scheduling fixture: move existing finite inputs and
    // operators to the processor; allow the normal recipe to pay its own batch.
    auto* Factory=S.FindBuilding(Processor);const auto Inputs=S.ProductionInputs(*Factory,TEXT("make_components"));
    for(const auto& P:Inputs){S.FindBuilding(Core)->Inventory.FindOrAdd(P.Key)-=P.Value*2;Factory->Inventory.Add(P.Key,P.Value*2);}
    S.FindBuilding(Core)->Inventory[TEXT("alloy")]=100;
    S.FindBuilding(Core)->Inventory[TEXT("components")]-=500;Factory->Inventory.Add(TEXT("components"),500);
    for(int32 I=0;I<2;++I)
    {
        auto& W=S.Workers.Bodies[I];W.CrewBuildingId=W.BuildingId=Processor;W.Activity=TEXT("operate");W.Outdoor=true;
        W.ContainerKind.Empty();W.ContainerId=W.RoadId=W.DeliveryId=W.ChargingBuildingId=W.TransportVehicleId=0;W.Route.Empty();W.NextWaypoint=0;
        W.Position=S.BuildingAccessPoint(*Factory)+FVector2D(0,(I+1)*20);
    }
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);S.Tick(S.FixedStepSeconds());
    Factory=S.FindBuilding(Processor);
    if(!TestTrue(TEXT("The normal factory actually commits a paid batch before throttling"),Factory->ProductionCommitted))return false;
    AI.Targets.Empty();AI.UpgradeFamilies.Empty();AI.DesiredTransports=0;
    FSeigeAIBuildTarget Existing;Existing.Definition=TEXT("component_works");Existing.Count=1;AI.Targets.Add(Existing);
    FSeigeAIBuildTarget Next;Next.Definition=TEXT("robot_service_bay");Next.Count=1;AI.Targets.Add(Next);
    const double Alloy=S.TotalStock(TEXT("alloy")),Components=S.TotalStock(TEXT("components")),Progress=Factory->Progress;
    const auto PaidInputs=Factory->ProductionInputs;const int32 Bodies=S.Workers.Bodies.Num();
    FSeigeSimulation Continuous=S;FSeigeScenarioAI Legacy=AI;Legacy.ProductionPriorityPolicy=TEXT("continuous");
    TestFalse(TEXT("The comparison policy does not override ordinary player-style continuous production"),Legacy.ManageIndustryProduction(Continuous));
    TestTrue(TEXT("Continuous comparison leaves its factory running"),Continuous.FindBuilding(Processor)->Enabled);
    if(!TestTrue(TEXT("Excess component production yields scarce alloy to the next station"),AI.ManageIndustryProduction(S)))return false;
    TestFalse(TEXT("Only the surplus specialist is paused"),S.FindBuilding(Processor)->Enabled);
    TestTrue(TEXT("The worker command module stays enabled"),S.FindBuilding(Core)->Enabled);
    const auto* Target=AI.NextConstructionTarget(S);
    TestTrue(TEXT("A deliberately idle but staffed specialist still satisfies the ordered plan"),Target&&Target->Definition==Next.Definition);
    TestTrue(TEXT("Throttling preserves physical stock, worker identity and paid batch progress"),S.TotalStock(TEXT("alloy"))==Alloy&&S.TotalStock(TEXT("components"))==Components&&S.Workers.Bodies.Num()==Bodies&&S.FindBuilding(Processor)->Progress==Progress&&S.FindBuilding(Processor)->ProductionCommitted);
    for(const auto& P:PaidInputs)TestEqual(TEXT("Every committed ingredient remains in the original batch escrow"),S.FindBuilding(Processor)->ProductionInputs.FindRef(P.Key),P.Value);
    AI.MakeDecision(S);TestFalse(TEXT("Construction scheduling does not immediately re-enable the conflicting factory"),S.FindBuilding(Processor)->Enabled);
    const FString Save=FPaths::ProjectSavedDir()/TEXT("Automation/ScenarioAI/production-budget.json");FSeigeSimulation Restored;FSeigeScenarioAI RestoredAI;
    if(!S.Save(Save,Error)||!RestoredAI.Initialize(Restored,FPaths::ProjectDir()/TEXT("Rules"),FPaths::ProjectDir()/TEXT("AIFILES"),false,Error,false,false,815)||!Restored.Load(Save,Error)){AddError(Error);return false;}
    TestTrue(TEXT("A paused paid batch and permanent crew survive strict persistence"),!Restored.FindBuilding(Processor)->Enabled&&Restored.FindBuilding(Processor)->ProductionCommitted&&Restored.Workers.AssignedCount(Processor)==2);
    // A later output demand must release the throttle, with the same already
    // paid recipe continuing instead of being cancelled or charged twice.
    for(auto& B:S.Buildings)B.Inventory.Remove(TEXT("components"));
    TestTrue(TEXT("A real shortage of the specialist output resumes production"),AI.ManageIndustryProduction(S));
    TestTrue(TEXT("Resumption retains its original paid batch"),S.FindBuilding(Processor)->Enabled&&S.FindBuilding(Processor)->ProductionCommitted&&S.FindBuilding(Processor)->Progress==Progress);
    TestEqual(TEXT("No extra worker is invented by production recovery"),S.Workers.Bodies.Num(),Bodies);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIGeneratorFundingTest,"Seige.AI.GeneratorFundingFollowsActualPowerShortage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIGeneratorFundingTest::RunTest(const FString&)
{
    FSeigeSimulation Base;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(Base,AI,Error)){AddError(Error);return false;}
    const auto Origin=Base.Buildings[0].Position;
    const int32 Port=Base.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(0,1800));
    Base.AddReviewBuilding(TEXT("alloy_refinery"),Origin+FVector2D(1800,0));
    AI.Targets.Empty();AI.UpgradeFamilies.Empty();AI.DesiredTransports=0;AI.ReserveTargets.Empty();
    FSeigeAIBuildTarget Next;Next.Definition=TEXT("fuel_generator");Next.Count=1;AI.Targets.Add(Next);
    for(auto& B:Base.Buildings){B.Inventory.Remove(TEXT("alloy"));B.Inventory.Remove(TEXT("iron_ore"));}
    Base.Credits=1;Base.Workers.RefreshMetrics(Base);Base.Energy.Invalidate();Base.Energy.Tick(Base,0);
    auto Grid=Base.Energy.Info(Base);
    if(!TestTrue(TEXT("The existing finite core supplies the installed ordinary load"),Grid.DemandKW>0&&Grid.SuppliedKW+1.e-8>=Grid.DemandKW))return false;
    FSeigeSimulation Healthy=Base;
    if(!TestTrue(TEXT("Spare generation can be built using the installed refinery"),AI.ManageTrade(Healthy)))return false;
    TestTrue(TEXT("A healthy grid buys real ore feedstock instead of finished generator alloys"),Healthy.FindBuilding(Port)->Shipment.Buy&&Healthy.FindBuilding(Port)->Shipment.Resource==TEXT("iron_ore"));
    TestTrue(TEXT("Feedstock funding spends credits without granting immediately usable ore"),Healthy.Credits<Base.Credits&&Healthy.TotalStock(TEXT("iron_ore"))==0);
    FSeigeSimulation Brownout=Base;
    // Actual outage fixture: disable the sole existing fusion source and
    // deplete its battery, retaining the same physical load and workers.
    Brownout.Buildings[0].Enabled=false;for(auto& B:Brownout.Buildings)B.BatteryEnergyKWh=0;Brownout.Energy.Tick(Brownout,1);
    Grid=Brownout.Energy.Info(Brownout);
    if(!TestTrue(TEXT("The comparison grid has a measured unmet passive load"),Grid.SuppliedKW+1.e-8<Grid.DemandKW))return false;
    if(!TestTrue(TEXT("A real outage directly funds the recovery generator"),AI.ManageTrade(Brownout)))return false;
    TestTrue(TEXT("Power recovery buys the missing finished construction alloy"),Brownout.FindBuilding(Port)->Shipment.Buy&&Brownout.FindBuilding(Port)->Shipment.Resource==TEXT("alloy"));
    TestTrue(TEXT("Emergency funding also waits for ordinary delivery"),Brownout.Credits<Base.Credits&&Brownout.TotalStock(TEXT("alloy"))==Base.TotalStock(TEXT("alloy")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAICivilianLossDefenseTest,"Seige.AI.CivilianLossFundsBoundedDefense",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAICivilianLossDefenseTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(S,AI,Error)){AddError(Error);return false;}
    AI.Targets.Empty();AI.UpgradeFamilies.Empty();AI.DesiredTransports=0;
    for(const auto& Entry:TArray<TPair<FString,int32>>{{TEXT("trading_port"),1},{TEXT("extraction_mine"),1},{TEXT("turret"),1},{TEXT("turret"),4}})
    {FSeigeAIBuildTarget T;T.Definition=Entry.Key;T.Count=Entry.Value;AI.Targets.Add(T);}
    FSeigeSimulation Peaceful=S;Peaceful.BackgroundBugsEnabled=Peaceful.PeriodicAttacksEnabled=false;
    TestNull(TEXT("Peaceful startup preserves the ordinary economic ordering"),AI.DefenseRecoveryTarget(Peaceful));
    FSeigeSimulation Hostile=Peaceful;Hostile.BackgroundBugsEnabled=true;
    const auto* Early=AI.DefenseRecoveryTarget(Hostile);
    TestTrue(TEXT("Background threats require the first paid tower before civilian exposure"),Early&&Early->Definition==TEXT("turret")&&Early->Count==1);
    Hostile.BackgroundBugsEnabled=false;Hostile.PeriodicAttacksEnabled=true;
    TestTrue(TEXT("Periodic attacks independently select the same finite first milestone"),AI.DefenseRecoveryTarget(Hostile)!=nullptr);
    TestEqual(TEXT("The early order protects exactly the ordinary construction bill"),AI.NextDevelopmentBill(Hostile).FindRef(TEXT("alloy")),Hostile.BuildingDefs[TEXT("turret")].Cost.FindRef(TEXT("alloy")));
    const FVector2D Origin=S.Buildings[0].Position;
    const int32 Port=S.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(0,1600));
    const int32 Lost=S.AddReviewBuilding(TEXT("extraction_mine"),Origin+FVector2D(1600,0));
    S.FindBuilding(Lost)->Health=0;
    S.Workers.Bodies[0].CrewBuildingId=Port;S.Workers.RefreshMetrics(S);
    FSeigeScenarioAI Ordered=AI;Ordered.DefenseRecoveryPolicy=TEXT("ordered_plan_only");
    const auto* Ordinary=Ordered.NextConstructionTarget(S);
    TestTrue(TEXT("The comparison policy retries the lost economic prerequisite first"),Ordinary&&Ordinary->Definition==TEXT("extraction_mine"));
    const auto* Recovery=AI.DefenseRecoveryTarget(S);
    if(!TestTrue(TEXT("Owned civilian loss selects only the first planned defensive milestone"),Recovery&&Recovery->Definition==TEXT("turret")&&Recovery->Count==1))return false;
    TestEqual(TEXT("Production and trade fund the exact paid tower bill"),AI.NextDevelopmentBill(S).FindRef(TEXT("alloy")),S.BuildingDefs[TEXT("turret")].Cost[TEXT("alloy")]);
    FSeigeSimulation Unfunded=S;for(auto& B:Unfunded.Buildings)B.Inventory.Remove(TEXT("alloy"));Unfunded.Credits=1;
    const double Before=Unfunded.TotalStock(TEXT("alloy"));
    TestTrue(TEXT("An intact real port funds the missing defensive material"),AI.ManageTrade(Unfunded));
    TestTrue(TEXT("Emergency defense imports its construction alloy without instant delivery"),Unfunded.FindBuilding(Port)->Shipment.Buy&&Unfunded.FindBuilding(Port)->Shipment.Resource==TEXT("alloy")&&Unfunded.TotalStock(TEXT("alloy"))==Before);
    FSeigeSimulation NoPort=S;NoPort.FindBuilding(Port)->Health=0;for(auto& B:NoPort.Buildings)B.Inventory.Remove(TEXT("alloy"));
    TestNull(TEXT("An unaffordable gun cannot block restoring a destroyed trading port"),AI.DefenseRecoveryTarget(NoPort));
    const auto* Economic=AI.NextConstructionTarget(NoPort);
    TestTrue(TEXT("Without a funding source the ordinary port prerequisite remains available"),Economic&&Economic->Definition==TEXT("trading_port"));
    const int32 Bodies=S.Workers.Bodies.Num(),Vehicles=S.Combat.Vehicles.Num(),Buildings=S.Buildings.Num();const double Alloy=S.TotalStock(TEXT("alloy"));
    if(!TestTrue(TEXT("The recovery order places an ordinary paid tower worksite"),AI.MakeDecision(S)))return false;
    TestTrue(TEXT("The new site is unfinished and reserves actual materials"),S.Buildings.Num()==Buildings+1&&S.Buildings.Last().DefId==TEXT("turret")&&S.Buildings.Last().IsConstructing&&S.TotalStock(TEXT("alloy"))==Alloy);
    TestTrue(TEXT("No defenders or worker identities are invented"),S.Workers.Bodies.Num()==Bodies&&S.Combat.Vehicles.Num()==Vehicles);
    AI.MakeDecision(S);TestEqual(TEXT("Waiting for physical construction does not queue duplicate defenses"),S.Buildings.Num(),Buildings+1);
    // Installed endpoint fixture: after the same tower completes and gains its
    // real crew, the recovery milestone releases the ordinary economic plan.
    auto& Tower=S.Buildings.Last();Tower.IsConstructing=false;Tower.ConstructionProgress=1;
    S.Workers.Bodies[1].CrewBuildingId=Tower.Id;S.Workers.Bodies[2].CrewBuildingId=Tower.Id;
    TestNull(TEXT("One staffed tower ends recovery instead of rushing the final four"),AI.DefenseRecoveryTarget(S));
    const auto* Resumed=AI.NextConstructionTarget(S);
    TestTrue(TEXT("Economic reconstruction resumes after the bounded defense milestone"),Resumed&&Resumed->Definition==TEXT("extraction_mine"));
    // Repeated losses are new evidence that the first paid tower did not cover
    // the whole settlement. Escalate one slot at a time within the authored cap.
    FSeigeSimulation Repeated=S;
    const int32 SecondLoss=Repeated.AddReviewBuilding(TEXT("extraction_mine"),Origin+FVector2D(-1600,0));Repeated.FindBuilding(SecondLoss)->Health=0;
    const auto* Second=AI.DefenseRecoveryTarget(Repeated);
    TestTrue(TEXT("A second civilian loss funds two towers, not an immediate final four"),Second&&Second->Definition==TEXT("turret")&&Second->Count==2);
    const int32 ThirdLoss=Repeated.AddReviewBuilding(TEXT("solar_array"),Origin+FVector2D(-1600,-1600));Repeated.FindBuilding(ThirdLoss)->Health=0;
    const auto* Third=AI.DefenseRecoveryTarget(Repeated);
    TestTrue(TEXT("A third loss advances the same finite planned family by one"),Third&&Third->Count==3);
    for(int32 I=0;I<8;++I){const int32 Id=Repeated.AddReviewBuilding(TEXT("solar_array"),Origin+FVector2D(-2000-I*300,-1600));Repeated.FindBuilding(Id)->Health=0;}
    const auto* Capped=AI.DefenseRecoveryTarget(Repeated);
    TestTrue(TEXT("Arbitrarily many losses cannot exceed the authored four-tower milestone"),Capped&&Capped->Count==4);
    FSeigeScenarioAI ShortPlan=AI;ShortPlan.Targets.Last().Count=2;
    const auto* Short=ShortPlan.DefenseRecoveryTarget(Repeated);
    TestTrue(TEXT("The bounded response derives from the actual plan rather than a hardcoded cap"),Short&&Short->Count==2);
    FSeigeSimulation UnfundedRepeat=Repeated;UnfundedRepeat.FindBuilding(Port)->Health=0;for(auto& B:UnfundedRepeat.Buildings)B.Inventory.Remove(TEXT("alloy"));
    TestNull(TEXT("Repeated losses still cannot block restoring the means of funding defense"),AI.DefenseRecoveryTarget(UnfundedRepeat));
    TestTrue(TEXT("Inspecting losses neither grants stock nor creates worker or vehicle identities"),Repeated.TotalStock(TEXT("alloy"))==Alloy&&Repeated.Workers.Bodies.Num()==Bodies&&Repeated.Combat.Vehicles.Num()==Vehicles);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAITransportEnergyTest,"Seige.AI.FundedTransportAccumulatesRealAssemblyEnergy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAITransportEnergyTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(S,AI,Error)){AddError(Error);return false;}
    AI.Targets.Empty();AI.UpgradeFamilies.Empty();AI.DesiredTransports=2;AI.ReserveTargets.Empty();
    const int32 Core=S.Buildings[0].Id;const auto Origin=S.Buildings[0].Position;
    const int32 Factory=S.AddReviewBuilding(TEXT("vehicle_factory"),Origin+FVector2D(1800,0));
    const int32 Processor=S.AddReviewBuilding(TEXT("component_works"),Origin+FVector2D(0,1800));
    const int32 Bank=S.AddReviewBuilding(TEXT("battery_bank"),Origin+FVector2D(-1800,0));
    const int32 Port=S.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(0,-1800));
    // Installed infrastructure isolates scheduling. The entire queued vehicle
    // bill is physically transferred from the unchanged finite starting kit.
    const auto Cost=S.TransportVehicles.Definition().BuildCost;
    for(const auto& P:Cost)
    {
        if(!TestTrue(TEXT("The finite kit owns the fixture's actual vehicle ingredient"),S.FindBuilding(Core)->Inventory.FindRef(P.Key)>=P.Value))return false;
        S.FindBuilding(Core)->Inventory.FindOrAdd(P.Key)-=P.Value;S.FindBuilding(Factory)->Inventory.Add(P.Key,P.Value);
    }
    for(int32 I=0;I<4;++I)
    {
        auto& W=S.Workers.Bodies[I];const int32 Workplace=I<2?Factory:Processor;
        W.CrewBuildingId=W.BuildingId=Workplace;W.Activity=TEXT("operate");W.Outdoor=true;W.ContainerKind.Empty();
        W.ContainerId=W.RoadId=W.DeliveryId=W.ChargingBuildingId=W.TransportVehicleId=0;W.Route.Empty();W.NextWaypoint=0;
        W.Position=S.BuildingAccessPoint(*S.FindBuilding(Workplace))+FVector2D(0,(I%2+1)*20);
    }
    for(auto& B:S.Buildings)B.BatteryEnergyKWh=0;
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);
    if(!TestTrue(TEXT("A real permanently staffed factory accepts the paid job"),AI.ManageTransport(S)))return false;
    const double Alloy=S.TotalStock(TEXT("alloy")),Components=S.TotalStock(TEXT("components"));
    const int32 Bodies=S.Workers.Bodies.Num(),Vehicles=S.TransportVehicles.LiveCount();
    TestEqual(TEXT("The locally funded job exposes its energy priority"),AI.TransportEnergyPriority(S),Factory);
    TestTrue(TEXT("Surplus industry pauses to accumulate the large assembly payment"),AI.ManageIndustryProduction(S));
    TestFalse(TEXT("Optional component batches are suspended"),S.FindBuilding(Processor)->Enabled);
    TestTrue(TEXT("Core generation, actual factory and battery stay enabled"),S.FindBuilding(Core)->Enabled&&S.FindBuilding(Factory)->Enabled&&S.FindBuilding(Bank)->Enabled);
    TestFalse(TEXT("New export transactions cannot spend the accumulating energy"),AI.ManageTrade(S));
    TestTrue(TEXT("Deferral leaves the port available without issuing a phantom order"),S.FindBuilding(Port)->Enabled&&S.FindBuilding(Port)->Shipment.Resource.IsEmpty());
    AI.MakeDecision(S);TestEqual(TEXT("The displayed wait identifies assembly energy"),AI.GetStatus(),FString(TEXT("Waiting for paid transport assembly energy")));
    TestTrue(TEXT("Scheduling creates no charge, cargo or worker identities"),S.Energy.Info(S).StoredKWh==0&&S.TotalStock(TEXT("alloy"))==Alloy&&S.TotalStock(TEXT("components"))==Components&&S.Workers.Bodies.Num()==Bodies&&S.TransportVehicles.LiveCount()==Vehicles);
    const FString Checkpoint=FPaths::ProjectSavedDir()/TEXT("Automation/ScenarioAI/transport-energy.json");FSeigeSimulation Restored;FSeigeScenarioAI FreshBrain;
    if(!S.Save(Checkpoint,Error)||!FreshBrain.Initialize(Restored,FPaths::ProjectDir()/TEXT("Rules"),FPaths::ProjectDir()/TEXT("AIFILES"),false,Error,false,false,815)||!Restored.Load(Checkpoint,Error)){AddError(TEXT("Energy wait checkpoint: ")+Error);return false;}
    TestTrue(TEXT("A fresh brain recovers the energy wait and paused specialist from saved real state"),FreshBrain.TransportEnergyPriority(Restored)==Factory&&!Restored.FindBuilding(Processor)->Enabled&&Restored.TransportVehicles.Jobs.Num()==1&&!Restored.TransportVehicles.Jobs[0].Committed);
    S=MoveTemp(Restored);
    FSeigeSimulation Insufficient=S;Insufficient.FindBuilding(Bank)->Health=0;Insufficient.Energy.Invalidate();Insufficient.Energy.Tick(Insufficient,0);
    TestEqual(TEXT("Missing storage cannot lock optional operations into an impossible energy wait"),AI.TransportEnergyPriority(Insufficient),0);
    FSeigeSimulation Outage=S;Outage.FindBuilding(Core)->Enabled=false;Outage.Energy.Tick(Outage,1);
    TestEqual(TEXT("An actual generation outage releases the temporary energy priority"),AI.TransportEnergyPriority(Outage),0);
    // Advance the real generation/fabrication subsystems: no added charge or
    // instant completion, and no rendered/pathfinding fixture dependency.
    const double Generated=S.Energy.GeneratedKWh,Consumed=S.Energy.ConsumedKWh;
    for(int32 I=0;I<1440&&!S.TransportVehicles.Jobs[0].Committed;++I)
    {S.Energy.Tick(S,10);S.TransportVehicles.Tick(S,10);}
    if(!TestTrue(TEXT("Ordinary generated power eventually commits the paid vehicle"),S.TransportVehicles.Jobs.Num()==1&&S.TransportVehicles.Jobs[0].Committed))return false;
    TestTrue(TEXT("At least the complete assembly energy came from generation and was spent"),S.Energy.GeneratedKWh-Generated>=S.TransportVehicles.Definition().BuildEnergyKWh&&S.Energy.ConsumedKWh-Consumed>=S.TransportVehicles.Definition().BuildEnergyKWh);
    TestEqual(TEXT("Energy commitment is still not instant vehicle completion"),S.TransportVehicles.LiveCount(),Vehicles);
    TestEqual(TEXT("The priority ends at a real paid commitment"),AI.TransportEnergyPriority(S),0);
    TestTrue(TEXT("A fresh brain resumes normal industry after the payment"),FreshBrain.ManageIndustryProduction(S));
    TestTrue(TEXT("The same specialist and operators resume"),S.FindBuilding(Processor)->Enabled&&S.Workers.AssignedCount(Processor)==2);
    TestEqual(TEXT("Paid ingredients remain accounted for in fabrication escrow"),S.TotalStock(TEXT("alloy")),Alloy);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAITransportFundingPriorityTest,"Seige.AI.TransportIngredientsCountOnceAndFundMissingImports",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAITransportFundingPriorityTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(S,AI,Error)){AddError(Error);return false;}
    AI.Targets.Empty();AI.UpgradeFamilies.Empty();AI.ReserveTargets.Empty();AI.DesiredTransports=2;
    const int32 Core=S.Buildings[0].Id;const auto Origin=S.Buildings[0].Position;
    const int32 Factory=S.AddReviewBuilding(TEXT("vehicle_factory"),Origin+FVector2D(1800,0));
    const int32 Port=S.AddReviewBuilding(TEXT("trading_port"),Origin+FVector2D(0,1800));
    const int32 Refinery=S.AddReviewBuilding(TEXT("alloy_refinery"),Origin+FVector2D(-1800,0));
    for(int32 I=0;I<4;++I)S.Workers.Bodies[I].CrewBuildingId=I<2?Factory:Refinery;
    // Installed late-colony inventory fixture: preserve the already delivered
    // bill in its real factory, with only four of eighty battery modules ready.
    for(auto& B:S.Buildings){B.Inventory.Remove(TEXT("alloy"));B.Inventory.Remove(TEXT("batteries"));B.Inventory.Remove(TEXT("iron_ore"));}
    S.FindBuilding(Factory)->Inventory.Add(TEXT("alloy"),806);
    S.FindBuilding(Factory)->Inventory.Add(TEXT("components"),100);
    S.FindBuilding(Factory)->Inventory.Add(TEXT("circuits"),20);
    S.FindBuilding(Factory)->Inventory.Add(TEXT("batteries"),4);
    S.Workers.RefreshMetrics(S);S.Energy.Invalidate();S.Energy.Tick(S,0);
    if(!TestTrue(TEXT("The actual installed factory queues its ordinary unpaid job"),AI.ManageTransport(S)))return false;
    const double Free=S.ConstructionAvailable(TEXT("alloy"));
    TestTrue(TEXT("Unrelated construction cannot take the reserved factory alloy"),Free<1.e-8);
    TestEqual(TEXT("The vehicle can count exactly its own delivered alloy once"),AI.DevelopmentAvailable(S,TEXT("alloy")),800.);
    TestEqual(TEXT("Its partial battery delivery also counts exactly once"),AI.DevelopmentAvailable(S,TEXT("batteries")),4.);
    TestEqual(TEXT("The planner does not demand a second copy of the locally funded alloy bill"),AI.ProductionGoals(S).FindRef(TEXT("alloy")),S.TotalStock(TEXT("alloy")));
    FSeigeSimulation Claimed=S;FSeigeCourier Reservation;Reservation.Id=999990;Reservation.SourceId=Factory;Reservation.TargetId=Core;Reservation.Resource=TEXT("alloy");Reservation.ReservedAmount=20;Claimed.Couriers.Add(Reservation);
    TestEqual(TEXT("An existing physical pickup claim is not released by AI accounting"),AI.DevelopmentAvailable(Claimed,TEXT("alloy")),780.);
    FSeigeSimulation TwoJobs=S;auto Extra=TwoJobs.TransportVehicles.Jobs[0];Extra.Id+=1;TwoJobs.TransportVehicles.Jobs.Add(Extra);
    TestEqual(TEXT("Two queued bodies cannot count the same locally delivered material twice"),AI.DevelopmentAvailable(TwoJobs,TEXT("alloy")),800.);
    // A legitimate routine alloy buffer remains short of ore. It must not
    // preempt the unmanufacturable final ingredient for the ready vehicle job.
    AI.ReserveTargets.Add(TEXT("alloy"),1000);
    S.Credits=S.TradeQuote(TEXT("batteries"),76,true);const double Before=S.TotalStock(TEXT("batteries"));
    if(!TestTrue(TEXT("The ordinary port orders the missing transport ingredient"),AI.ManageTrade(S)))return false;
    const auto& Order=S.FindBuilding(Port)->Shipment;
    TestTrue(TEXT("The order purchases only the seventy-six missing modules before unrelated ore"),Order.Buy&&Order.Resource==TEXT("batteries")&&FMath::IsNearlyEqual(Order.Quantity,76.,1.e-8));
    TestTrue(TEXT("Import order spends the actual quote and grants no instant material"),FMath::IsNearlyZero(S.Credits,1.e-8)&&S.TotalStock(TEXT("batteries"))==Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAIExtractorAccessTest,"Seige.AI.ExtractorSearchKeepsDepositAndPhysicalAccess",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAIExtractorAccessTest::RunTest(const FString&)
{
    FSeigeSimulation S;FSeigeScenarioAI AI;FString Error;
    if(!DevelopmentFixture(S,AI,Error)){AddError(Error);return false;}
    const FString Mine=TEXT("extraction_mine");
    const auto* Export=AI.ExportNode(S);if(!TestNotNull(TEXT("A real starter deposit exists"),Export))return false;
    FSeigeNode Node=*Export;Node.Position=S.Buildings[0].Position+FVector2D(1100,0);S.Nodes={Node};
    FSeigeBuilding Preview;Preview.DefId=Mine;Preview.Position=Node.Position;
    const FVector2D BlockedAccess=S.BuildingAccessPoint(Preview);
    // An isolated steep bank at the exact deposit-center entrance reproduces
    // the route failure without moving workers or weakening slope limits.
    S.Mobility.SetTerrainSampler([BlockedAccess](FVector2D P){return 100.*FMath::Max(0.,1.-FVector2D::Distance(P,BlockedAccess)/45.);});
    const int32 BeforeBuildings=S.Buildings.Num(),BeforeWorkers=S.Workers.Bodies.Num();
    const double BeforeStock=S.TotalStock(TEXT("alloy")),BeforeAvailable=S.ConstructionAvailable(TEXT("alloy"));
    if(!TestFalse(TEXT("The center entrance is physically inaccessible"),AI.PlaceConnectedBuilding(S,Mine,Node.Position,Error)))return false;
    TestEqual(TEXT("A rejected center reserves no material"),S.ConstructionAvailable(TEXT("alloy")),BeforeAvailable);
    FSeigeSimulation Inaccessible=S;Inaccessible.Mobility.SetTerrainSampler([](FVector2D P){return P.X*2.;});
    TestFalse(TEXT("Searching cannot bypass entirely impassable terrain"),AI.PlaceExtractorNear(Inaccessible,Mine,Node,Error));
    TestEqual(TEXT("Failed bounded search leaves the colony unchanged"),Inaccessible.Buildings.Num(),BeforeBuildings);
    FSeigeSimulation BlockedStation=S;
    const FVector2D Station=BlockedStation.Workers.WorkstationPosition(BlockedStation,Preview,1);
    BlockedStation.Mobility.SetTerrainSampler([Station](FVector2D P){return 100.*FMath::Max(0.,1.-FVector2D::Distance(P,Station)/10.);});
    TArray<FVector2D> EntranceRoute;
    TestTrue(TEXT("A reachable entrance alone is insufficient evidence of usable workstations"),BlockedStation.FindRoute(BlockedStation.BuildingAccessPoint(BlockedStation.Buildings[0]),BlockedAccess,EntranceRoute,-1,false,&Preview));
    TestFalse(TEXT("An isolated cliff at a real operator slot rejects the plot"),AI.PlaceConnectedBuilding(BlockedStation,Mine,Node.Position,Error));
    TestEqual(TEXT("Station rejection reserves no construction material"),BlockedStation.ConstructionAvailable(TEXT("alloy")),BeforeAvailable);
    if(!TestTrue(TEXT("A nearby entrance reaches the same physical deposit"),AI.PlaceExtractorNear(S,Mine,Node,Error))){AddError(Error);return false;}
    const auto& Built=S.Buildings.Last();const auto* Bound=S.ExtractionNode(Mine,Built.Position);
    TestTrue(TEXT("Alternative plot remains bound to exactly the original deposit"),Bound&&Bound->Id==Node.Id&&Built.DepositId==Node.Id);
    TestFalse(TEXT("Placement moves off the blocked center"),Built.Position.Equals(Node.Position,.01));
    TestEqual(TEXT("Only one ordinary construction order is accepted"),S.Buildings.Num(),BeforeBuildings+1);
    TestTrue(TEXT("Construction still needs paid materials and physical workers"),Built.IsConstructing&&Built.ConstructionProgress==0&&S.Workers.Bodies.Num()==BeforeWorkers);
    TestEqual(TEXT("The normal full alloy cost is reserved"),BeforeAvailable-S.ConstructionAvailable(TEXT("alloy")),S.BuildingDefs[Mine].Cost.FindRef(TEXT("alloy")));
    TestEqual(TEXT("Placement creates no material"),S.TotalStock(TEXT("alloy")),BeforeStock);
    TArray<FVector2D> Route;TestTrue(TEXT("Selected entrance has a real worker route"),S.FindRoute(S.BuildingAccessPoint(S.Buildings[0]),S.BuildingAccessPoint(Built),Route));

    FSeigeSimulation ReturnSite;FSeigeScenarioAI ReturnAI;
    if(!DevelopmentFixture(ReturnSite,ReturnAI,Error)){AddError(Error);return false;}
    // Installed geometry fixture: a long ridge permits a real route around
    // its far end, but not a sustainable return on the minimum charge reserve.
    const FVector2D Move=FVector2D(-1000,0)-ReturnSite.Buildings[0].Position;
    ReturnSite.Buildings[0].Position+=Move;for(auto& W:ReturnSite.Workers.Bodies)W.Position+=Move;
    ReturnSite.Nodes.Empty();ReturnSite.Environment.Enabled=true;ReturnSite.Environment.WorldOffset=FVector2D::ZeroVector;
    ReturnSite.Environment.River.Empty();ReturnSite.Environment.AdditionalRivers.Empty();ReturnSite.Environment.Fords.Empty();ReturnSite.Environment.Cliffs.Empty();ReturnSite.Environment.Lakes.Empty();
    FSeigeLake Lake;Lake.Center=FVector2D(0,14500);Lake.Radii=FVector2D(300,300);Lake.BuildShoreline();ReturnSite.Environment.Lakes.Add(Lake);
    ReturnSite.Mobility.SetTerrainSampler([](FVector2D P){return FMath::Abs(P.Y)<12000?500.*FMath::Max(0.,1.-FMath::Abs(P.X)/100.):0.;});
    FSeigeBuilding Remote;Remote.Id=INDEX_NONE;Remote.DefId=TEXT("trading_port");Remote.Position=FVector2D(1000,0);
    const auto RemoteStation=ReturnSite.Workers.WorkstationPosition(ReturnSite,Remote,0);
    const auto ChargerQueue=ReturnSite.Workers.ChargingQueuePosition(ReturnSite,ReturnSite.Buildings[0],1);
    if(!TestTrue(TEXT("The long charging return route is physically valid"),ReturnSite.FindRoute(RemoteStation,ChargerQueue,Route,-1,false,&Remote)))return false;
    TestFalse(TEXT("A physically valid detour can still exceed the real charge reserve"),ReturnSite.Workers.WorkstationHasChargingAccess(ReturnSite,RemoteStation,&Remote));
    const double AvailableBeforeReturn=ReturnSite.ConstructionAvailable(TEXT("alloy"));const int32 ReturnBodies=ReturnSite.Workers.Bodies.Num();
    TestFalse(TEXT("AI refuses the unserviceable plot before material reservation"),ReturnAI.PlaceConnectedBuilding(ReturnSite,Remote.DefId,Remote.Position,Error));
    TestTrue(TEXT("Failure identifies the charging return rather than claiming unreachable terrain"),Error.Contains(TEXT("return trip")));
    TestEqual(TEXT("Rejected long-return plot grants or consumes no material"),ReturnSite.ConstructionAvailable(TEXT("alloy")),AvailableBeforeReturn);
    TestEqual(TEXT("Rejected long-return plot creates no replacement workers"),ReturnSite.Workers.Bodies.Num(),ReturnBodies);
    ReturnSite.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    if(!TestTrue(TEXT("An ordinary paid plot remains viable using only the starting core charger"),ReturnAI.PlaceConnectedBuilding(ReturnSite,Remote.DefId,Remote.Position,Error)))AddError(Error);
    return true;
}
#endif
