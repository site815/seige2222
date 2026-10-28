#include "Simulation/SeigeSimulation.h"
#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS
// Opt-in economic diagnosis only. Fresh FirstPlayableSolvable retains its
// ordinary starting cargo, threats and completion criteria. This replay never
// replaces the fresh bounded FirstPlayableSolvable acceptance.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeHostileFundingReplay,"Diagnostics.HostileFundingReplay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeHostileFundingReplay::RunTest(const FString&)
{
    FString Path,Error;double Hours=6;
    if(!FParse::Value(FCommandLine::Get(),TEXT("HostileFundingSnapshot="),Path)||Path.IsEmpty())
    {AddError(TEXT("Requires -HostileFundingSnapshot=<preserved hostile save>"));return false;}
    FParse::Value(FCommandLine::Get(),TEXT("HostileFundingHours="),Hours);
    if(!FMath::IsFinite(Hours)||Hours<=0||Hours>12){AddError(TEXT("Funding replay requires a positive bound up to12 hours"));return false;}
    FSeigeSimulation S;FSeigeScenarioAI AI;
    if(!AI.Initialize(S,FPaths::ProjectDir()/TEXT("Rules"),FPaths::ProjectDir()/TEXT("AIFILES"),false,Error)||!S.Load(Path,Error)){AddError(Error);return false;}
    if(!S.BackgroundBugsEnabled||!S.PeriodicAttacksEnabled){AddError(TEXT("Funding replay requires both original threat systems"));return false;}
    const FString Root=FPaths::ProjectSavedDir()/TEXT("Diagnostics/HostileFunding");IFileManager::Get().MakeDirectory(*Root,true);
    const double Start=S.Time;double NextReport=Start;
    auto Report=[&]()
    {
        AddInfo(FString::Printf(TEXT("Funding t=%.2fh elapsed=%.2fh credits=%.6f population=%d jobs=%d inactive=%d reserve=%d produced_components=%.1f won=%d failed=%d status=%s"),S.Time/3600,(S.Time-Start)/3600,S.Credits,S.Population,S.TotalJobs,S.InactiveWorkerCount(),S.WorkerReserveTarget(),S.ProducedUnits.FindRef(TEXT("components")),int32(S.Won),int32(S.Failed),*AI.GetStatus()));
        for(const TCHAR* Id:{TEXT("robot_service_bay"),TEXT("alloy_refinery"),TEXT("component_works")})
        {
            const auto* Def=S.BuildingDefs.Find(Id);if(!Def)continue;
            const auto* B=S.Buildings.FindByPredicate([&](const auto& Candidate){return Candidate.Health>0&&S.Definition(Candidate)->Family==Def->Family;});
            AddInfo(FString::Printf(TEXT("Funding target=%s live=%d progress=%.3f onsite=%d"),Id,int32(B!=nullptr),B?B->ConstructionProgress:0,B?B->Workers:0));
            if(!B)for(const auto& Cost:Def->Cost)AddInfo(FString::Printf(TEXT("Funding target=%s material=%s cost=%.3f available=%.3f total=%.3f deficit=%.3f"),Id,*Cost.Key,Cost.Value,S.ConstructionAvailable(Cost.Key),S.TotalStock(Cost.Key),FMath::Max(0.,Cost.Value-S.ConstructionAvailable(Cost.Key))));
        }
        for(const auto& B:S.Buildings)if(B.Health>0&&S.Definition(B)->Role==TEXT("trade"))
            AddInfo(FString::Printf(TEXT("Funding shipment resource=%s amount=%.3f buy=%d departed=%d progress=%.3f"),*B.Shipment.Resource,B.Shipment.Quantity,int32(B.Shipment.Buy),int32(B.Shipment.Departed),B.Shipment.Progress));
        const FString Checkpoint=Root/FString::Printf(TEXT("state-%d.json"),FMath::RoundToInt(S.Time));
        if(!S.Save(Checkpoint,Error))AddError(TEXT("Funding checkpoint: ")+Error);
    };
    Report();NextReport+=1800;
    while(S.Time<Start+Hours*3600&&!S.Won&&!S.Escaped&&!S.Failed)
    {
        AI.Tick(S,FMath::Min(10.,Start+Hours*3600-S.Time));
        if(S.Time+UE_DOUBLE_SMALL_NUMBER>=NextReport){Report();NextReport+=1800;}
    }
    Report();TestTrue(TEXT("The unchanged hostile colony reaches its production objective within the diagnostic bound"),S.Won);
    return true;
}
#endif
