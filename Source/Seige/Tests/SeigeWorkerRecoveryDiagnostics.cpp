#include "Simulation/SeigeSimulation.h"
#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
// Explicit saved-state diagnosis, not a substitute for the complete hostile
// bootstrap acceptance. No inventory, worker or threat overrides are applied.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerStarvationReplay,"Diagnostics.WorkerStarvationReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeWorkerStarvationReplay::RunTest(const FString&)
{
    FString Path,Error;double Limit=7200;
    if(!FParse::Value(FCommandLine::Get(),TEXT("SeigeWorkerRecoverySnapshot="),Path)||Path.IsEmpty())
    {AddError(TEXT("Requires -SeigeWorkerRecoverySnapshot=<preserved failed save>"));return false;}
    FParse::Value(FCommandLine::Get(),TEXT("SeigeWorkerRecoverySeconds="),Limit);
    if(!FMath::IsFinite(Limit)||Limit<=0||Limit>86400){AddError(TEXT("Recovery replay requires a positive bound up to86400 seconds"));return false;}
    FSeigeSimulation S;FSeigeScenarioAI AI;
    if(!AI.Initialize(S,FPaths::ProjectDir()/TEXT("Rules"),FPaths::ProjectDir()/TEXT("AIFILES"),false,Error)||!S.Load(Path,Error)){AddError(Error);return false;}
    const auto* Pending=S.Couriers.FindByPredicate([&](const auto& C){return C.ForConstruction&&C.TransportVehicleId>0&&C.ReservedAmount>0&&C.Amount==0;});
    if(!TestNotNull(TEXT("The preserved state contains a real blocked construction pickup"),Pending))return false;
    const int32 Delivery=Pending->Id,Site=Pending->TargetId,Bodies=S.Workers.Bodies.Num();const double Start=S.Time;
    const auto* Building=S.FindBuilding(Site);if(!TestNotNull(TEXT("The original paid construction still exists"),Building))return false;
    const double Progress=Building->ConstructionProgress;
    bool Recovery=false;
    while(S.Time<Start+Limit&&!S.Escaped&&!S.Failed)
    {
        AI.Tick(S,FMath::Min(10.,Start+Limit-S.Time));
        const auto* Current=S.FindBuilding(Site);
        Recovery=S.Workers.Bodies.Num()>Bodies&&Current&&Current->Health>0&&Current->ConstructionProgress>Progress+.001&&
            !S.Couriers.ContainsByPredicate([&](const auto& C){return C.Id==Delivery;});
        if(Recovery)break;
    }
    const FString Output=FPaths::ProjectSavedDir()/TEXT("Automation/worker-starvation-replay-final.json");
    if(!S.Save(Output,Error))AddError(Error);
    AddInfo(FString::Printf(TEXT("Recovery after %.0fs: worker identities %d -> %d; status=%s; saved=%s"),S.Time-Start,Bodies,S.Workers.Bodies.Num(),*AI.GetStatus(),*Output));
    TestTrue(TEXT("Paid worker manufacture and physical loading resume the same construction within the bound"),Recovery);
    return true;
}
#endif
