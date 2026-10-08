#include "SeigeScenarioAI.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"

namespace
{
using FObject = TSharedPtr<FJsonObject>;
bool ReadAIJson(const FString& Path, FObject& Object, FString& Raw, FString& Error)
{
    if (!FFileHelper::LoadFileToString(Raw, *Path)) { Error = TEXT("Cannot read AI definition: ") + Path; return false; }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
    { Error = TEXT("Invalid AI JSON: ") + Path + TEXT(": ") + Reader->GetErrorMessage(); return false; }
    double Version = 0;
    if (!Object->TryGetNumberField(TEXT("version"), Version) || Version != 1)
    { Error = TEXT("Unsupported AI definition version: ") + Path; return false; }
    return true;
}
bool Number(const FObject& Object, const TCHAR* Key, double& Value, double Min, double Max, FString& Error)
{
    if (!Object || !Object->TryGetNumberField(Key, Value) || !FMath::IsFinite(Value) || Value < Min || Value > Max)
    { Error = TEXT("Invalid AI field: ") + FString(Key); return false; }
    return true;
}
bool Integer(const FObject& Object, const TCHAR* Key, int32& Value, int32 Min, int32 Max, FString& Error)
{
    double N = 0;
    if (!Number(Object, Key, N, Min, Max, Error)) return false;
    if (N != FMath::FloorToDouble(N)) { Error = TEXT("AI field must be an integer: ") + FString(Key); return false; }
    Value = static_cast<int32>(N); return true;
}
const FSeigeBuilding* Command(const FSeigeSimulation& Colony)
{
    return Colony.Buildings.FindByPredicate([&](const FSeigeBuilding& B) { return B.Health > 0 && Colony.Definition(B)->Role == TEXT("core"); });
}
bool NodeOccupied(const FSeigeSimulation& Colony, const FSeigeNode& Node)
{
    for(const auto& B:Colony.Buildings)if(B.Health>0&&B.DepositId==Node.Id)return true;
    return false;
}
}

bool FSeigeScenarioAI::LoadConfig(const FSeigeSimulation& Colony, const FString& Directory, FString& Error)
{
    FObject Config, Preset; FString Raw, PresetRaw;
    if (!ReadAIJson(FPaths::Combine(Directory, TEXT("colony_ai.json")), Config, Raw, Error) ||
        !ReadAIJson(FPaths::Combine(Directory, TEXT("developed_start.json")), Preset, PresetRaw, Error)) return false;
    const FObject* Placement = nullptr;
    if (!Config->TryGetObjectField(TEXT("placement"), Placement) || !Placement || !Placement->IsValid())
    { Error = TEXT("AI placement definition is missing"); return false; }
    if (!Number(Config, TEXT("decision_interval_seconds"), DecisionInterval, UE_DOUBLE_SMALL_NUMBER, 3600, Error) ||
        !Integer(Config, TEXT("max_actions_per_decision"), MaxActions, 1, 16, Error) ||
        !Integer(Config, TEXT("max_sensors"), MaxSensors, 1, 128, Error) ||
        !Integer(Config, TEXT("developed_setup_action_limit"), DevelopedSetupLimit, 1, 512, Error) ||
        !Number(Config,TEXT("developed_setup_seconds"),DevelopedSetupSeconds,DecisionInterval,172800,Error) ||
        !Number(*Placement, TEXT("ring_start"), RingStart, UE_DOUBLE_SMALL_NUMBER, Colony.WorldHalfSize * 2, Error) ||
        !Number(*Placement, TEXT("ring_step"), RingStep, UE_DOUBLE_SMALL_NUMBER, Colony.WorldHalfSize * 2, Error) ||
        !Number(*Placement, TEXT("ring_limit"), RingLimit, RingStart, Colony.WorldHalfSize * 2, Error) ||
        !Integer(*Placement, TEXT("angles"), Angles, 4, 128, Error) ||
        !Number(*Placement, TEXT("node_clearance"), NodeClearance, 0, Colony.WorldHalfSize, Error) ||
        !Number(*Placement, TEXT("sensor_overlap"), SensorOverlap, UE_DOUBLE_SMALL_NUMBER, 1, Error) ||
        !Number(*Placement, TEXT("defense_distance"), DefenseDistance, 0, Colony.WorldHalfSize, Error)) return false;
    if (DecisionInterval < Colony.FixedStepSeconds() || ((RingLimit - RingStart) / RingStep + 1) * Angles > 4096)
    { Error = TEXT("AI cadence is below the simulation step or placement search is too large"); return false; }
    if(!(*Placement)->TryGetStringField(TEXT("defense_coverage_policy"),DefenseCoveragePolicy)||
       (DefenseCoveragePolicy!=TEXT("prefer_covered_approaches")&&DefenseCoveragePolicy!=TEXT("first_legal"))||
       !Integer(*Placement,TEXT("coverage_samples"),CoverageSamples,8,64,Error)||
       !Number(*Placement,TEXT("coverage_probe_distance_meters"),CoverageProbeMeters,UE_DOUBLE_SMALL_NUMBER,100,Error))
    {if(Error.IsEmpty())Error=TEXT("Invalid AI defense_coverage_policy");return false;}
    const TArray<TSharedPtr<FJsonValue>>* ExcludedRoles=nullptr;
    if(!(*Placement)->TryGetArrayField(TEXT("coverage_excluded_roles"),ExcludedRoles)||!ExcludedRoles||ExcludedRoles->IsEmpty())
    {Error=TEXT("AI coverage_excluded_roles must name known building roles");return false;}
    CoverageExcludedRoles.Empty();
    for(const auto& Value:*ExcludedRoles)
    {
        FString Role;bool Known=false;if(Value->TryGetString(Role))for(const auto& Pair:Colony.BuildingDefs)if(Pair.Value.Role==Role){Known=true;break;}
        if(!Known||CoverageExcludedRoles.Contains(Role)){Error=TEXT("Invalid or duplicate AI coverage_excluded_roles entry");return false;}
        CoverageExcludedRoles.Add(Role);
    }
    if(!Config->TryGetStringField(TEXT("decision_scheduling_policy"),DecisionSchedulingPolicy)||
        (DecisionSchedulingPolicy!=TEXT("independent_tactics_trade_construction")&&DecisionSchedulingPolicy!=TEXT("shared_action_budget")))
    {Error=TEXT("Unsupported AI decision_scheduling_policy");return false;}
    const FObject* Guard=nullptr;
    if(!Config->TryGetObjectField(TEXT("guard_service"),Guard)||!Guard||
        !Number(*Guard,TEXT("recharge_below_fraction"),GuardRechargeBelow,UE_DOUBLE_SMALL_NUMBER,1,Error)||
        !Number(*Guard,TEXT("resume_above_fraction"),GuardResumeAbove,GuardRechargeBelow,1,Error)||GuardResumeAbove<=GuardRechargeBelow)
    {Error=TEXT("AI guard_service requires increasing recharge/resume battery fractions");return false;}
    if (!Config->TryGetStringField(TEXT("sensor_definition"), SensorDefinition) || !Colony.BuildMenu.Contains(SensorDefinition) || Colony.BuildingDefs[SensorDefinition].SensorRange <= 0)
    { Error = TEXT("AI sensor_definition must name a buildable sensor"); return false; }
    FString TargetPolicy;
    if(!Config->TryGetStringField(TEXT("target_policy"),TargetPolicy)||TargetPolicy!=TEXT("complete_and_staff_in_order"))
    {Error=TEXT("Unsupported AI target_policy");return false;}
    FString RecoveryPolicy;
    if(!Config->TryGetStringField(TEXT("support_recovery_policy"),RecoveryPolicy)||RecoveryPolicy!=TEXT("restore_capacity_before_expansion"))
    {Error=TEXT("Unsupported AI support_recovery_policy");return false;}
    const FObject* Economy=nullptr;
    if(!Config->TryGetObjectField(TEXT("economy"),Economy)||!Economy||!(*Economy)->TryGetStringField(TEXT("solar_definition"),SolarDefinition)||!(*Economy)->TryGetStringField(TEXT("trade_definition"),TradeDefinition)||!Colony.BuildMenu.Contains(SolarDefinition)||!Colony.BuildMenu.Contains(TradeDefinition)||Colony.BuildingDefs[SolarDefinition].Role!=TEXT("generator")||Colony.BuildingDefs[TradeDefinition].Role!=TEXT("trade")||!Number(*Economy,TEXT("export_batch"),ExportBatch,UE_DOUBLE_SMALL_NUMBER,10000,Error)||!Number(*Economy,TEXT("import_batch"),ImportBatch,UE_DOUBLE_SMALL_NUMBER,10000,Error))
    {Error=TEXT("Invalid AI economy bootstrap/trade settings");return false;}
    const FObject* Reserves=nullptr;
    if(!Number(*Economy,TEXT("recipe_input_buffer_cycles"),RecipeInputBuffer,1,100,Error)||!Number(*Economy,TEXT("credit_buffer_batches"),CreditBufferBatches,0,100,Error)||!(*Economy)->TryGetObjectField(TEXT("reserve_targets"),Reserves)||!Reserves)
    {Error=TEXT("Invalid AI reserve targets or trade buffers");return false;}
    ReserveTargets.Empty();for(const auto& P:(*Reserves)->Values){const FString Id(P.Key);double Amount=0;if(!Colony.Resources.Contains(Id)||!P.Value->TryGetNumber(Amount)||!FMath::IsFinite(Amount)||Amount<0){Error=TEXT("Invalid AI reserve target: ")+Id;return false;}ReserveTargets.Add(Id,Amount);}
    if(!Number(*Economy,TEXT("fuel_import_buffer_cycles"),FuelImportBufferCycles,1,100,Error)||!Number(*Economy,TEXT("fuel_import_refill_fraction"),FuelImportRefillFraction,UE_DOUBLE_SMALL_NUMBER,1,Error))return false;
    if(!(*Economy)->TryGetStringField(TEXT("bulk_input_policy"),BulkInputPolicy)||(BulkInputPolicy!=TEXT("remaining_output_bill")&&BulkInputPolicy!=TEXT("recipe_buffers")))
    {Error=TEXT("Invalid AI bulk_input_policy");return false;}
    if(!(*Economy)->TryGetStringField(TEXT("export_policy"),SurplusExportPolicy)||(SurplusExportPolicy!=TEXT("surplus_shipment_value")&&SurplusExportPolicy!=TEXT("local_raw_only")))
    {Error=TEXT("Invalid AI export_policy");return false;}
    FString ReplicationPolicy;const TArray<TSharedPtr<FJsonValue>>* ReplicationRecipes=nullptr;
    if(!(*Economy)->TryGetStringField(TEXT("core_replication_policy"),ReplicationPolicy)||ReplicationPolicy!=TEXT("funded_shortage_first")||!(*Economy)->TryGetArrayField(TEXT("core_replication_recipes"),ReplicationRecipes)||!ReplicationRecipes||ReplicationRecipes->IsEmpty())
    {Error=TEXT("Invalid AI core_replication_policy or core_replication_recipes");return false;}
    CoreReplicationRecipes.Empty();for(const auto& Value:*ReplicationRecipes)
    {
        FString Id;if(!Value->TryGetString(Id)||!Colony.Recipes.Contains(Id)||Colony.Recipes[Id].WorkerOutput>0||Colony.Recipes[Id].Outputs.IsEmpty()||!Colony.BuildingDefs[Colony.CoreDefinition].AllowedRecipes.Contains(Id)||CoreReplicationRecipes.Contains(Id))
        {Error=TEXT("Invalid AI core_replication_recipes entry");return false;}
        CoreReplicationRecipes.Add(Id);
    }
    const TArray<TSharedPtr<FJsonValue>>* Plan = nullptr;
    if (!Config->TryGetArrayField(TEXT("build_targets"), Plan) || !Plan || Plan->IsEmpty() || Plan->Num() > 128)
    { Error = TEXT("AI needs a bounded, nonempty build_targets list"); return false; }
    Targets.Empty(); TMap<FString, int32> PreviousCounts;
    for (const auto& Value : *Plan)
    {
        const FObject* Entry = nullptr; FSeigeAIBuildTarget Target;
        if (!Value->TryGetObject(Entry) || !Entry || !Entry->IsValid() || !(*Entry)->TryGetStringField(TEXT("definition"), Target.Definition) || !Colony.BuildMenu.Contains(Target.Definition))
        { Error = TEXT("AI target references an unknown or unbuildable definition"); return false; }
        if (!Integer(*Entry, TEXT("count"), Target.Count, 1, 128, Error) || !Integer(*Entry,TEXT("placement_index"),Target.PlacementIndex,0,4096,Error)) return false;
        if (PreviousCounts.FindRef(Target.Definition) >= Target.Count)
        { Error = TEXT("Repeated AI target counts must increase: ") + Target.Definition; return false; }
        PreviousCounts.Add(Target.Definition, Target.Count); Targets.Add(Target);
    }
    if(!Config->TryGetStringField(TEXT("developed_initialization"),DevelopedInitialization)||(DevelopedInitialization!=TEXT("established_manifest")&&DevelopedInitialization!=TEXT("simulated_history")))
    {Error=TEXT("Unsupported AI developed_initialization");return false;}
    if(!LoadEstablishedPreset(Colony,Preset,Error))return false;
    ConfigFingerprint = FMD5::HashAnsiString(*(Raw + PresetRaw));
    return true;
}

bool FSeigeScenarioAI::Initialize(FSeigeSimulation& Colony,const FString& RulesDirectory,const FString& AIDirectory,bool bDeveloped,FString& Error,bool bBackgroundBugs,bool bPeriodicAttacks,int32 SeedOverride,FVector2D WorldOffset)
{
    FSeigeScenarioAI Builder;FSeigeSimulation Candidate;
    if(!Builder.BeginInitialize(Candidate,RulesDirectory,AIDirectory,bDeveloped,Error,bBackgroundBugs,bPeriodicAttacks,SeedOverride,WorldOffset)){*this=MoveTemp(Builder);return false;}
    bool Complete=!Builder.IsPreparing();
    while(!Complete)if(!Builder.AdvancePreparation(Candidate,50,Complete,Error)){*this=MoveTemp(Builder);return false;}
    Colony=MoveTemp(Candidate);*this=MoveTemp(Builder);Error.Empty();return true;
}

bool FSeigeScenarioAI::BeginInitialize(FSeigeSimulation& Colony,const FString& RulesDirectory,const FString& AIDirectory,bool bDeveloped,FString& Error,bool bBackgroundBugs,bool bPeriodicAttacks,int32 SeedOverride,FVector2D WorldOffset)
{
    *this=FSeigeScenarioAI();FSeigeSimulation Candidate;
    if(!Candidate.Initialize(RulesDirectory,Error,bBackgroundBugs,bPeriodicAttacks,SeedOverride,WorldOffset)||!LoadConfig(Candidate,AIDirectory,Error))return false;
    if(bDeveloped&&DevelopedInitialization==TEXT("established_manifest"))
    {
        if(!InitializeEstablished(Candidate,Error))return false;
        Colony=MoveTemp(Candidate);Ready=true;Preparing=false;Status=TEXT("Established colony ready");Error.Empty();return true;
    }
    const FSeigeNode* Local=ExportNode(Candidate);bool Landed=false;
    if(Local)for(int32 I=0;I<Angles;++I)
    {const double Angle=I*UE_TWO_PI/Angles;const FVector2D At=Local->Position+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*RingStart;if(Candidate.SetInitialCorePosition(At,Error)){Landed=true;break;}}
    if(!Landed){Error=TEXT("AI could not find a legal landing beside a generated standard deposit");return false;}

    Colony=MoveTemp(Candidate);Ready=true;Preparing=bDeveloped;PreparationChunkRemaining=0;
    Status=bDeveloped?TEXT("Preparing developed colony through ordinary construction"):TEXT("Starting colony ready");Error.Empty();return true;
}

bool FSeigeScenarioAI::DevelopmentComplete(const FSeigeSimulation& Colony) const
{
    if(Colony.Failed||Colony.Escaped)return false;
    const auto* Core=Command(Colony);if(!Core)return false;
    for(const auto& Target:Targets)
    {
        if(!IncludesTarget(Colony,Target.Definition))continue;int32 Count=0;
        for(const auto& B:Colony.Buildings)if(B.Health>0&&!B.IsConstructing&&B.DefId==Target.Definition&&B.Workers>=Colony.Definition(B)->Jobs&&Colony.IsRoadGridConnected(Core->Id,B.Id))++Count;
        if(Count<Target.Count)return false;
    }
    return true;
}

bool FSeigeScenarioAI::FinishPreparation(FSeigeSimulation& Colony,FString& Error)
{
    Preparing=false;
    if(DevelopmentComplete(Colony))
    {Colony.AddEvent(TEXT("Developed AI colony constructed through normal delivery and worker rules from a finite scenario seed."));Status=TEXT("Developed colony ready");Error.Empty();return true;}
    for(const auto& Target:Targets)
    {
        if(!IncludesTarget(Colony,Target.Definition))continue;int32 Count=0;const auto* Core=Command(Colony);
        for(const auto& B:Colony.Buildings)if(B.Health>0&&!B.IsConstructing&&B.DefId==Target.Definition&&B.Workers>=Colony.Definition(B)->Jobs&&Core&&Colony.IsRoadGridConnected(Core->Id,B.Id))++Count;
        if(Colony.Failed||Colony.Escaped||Count<Target.Count)
        {
            int32 Live=0,Destroyed=0,Unfinished=0,TargetDestroyed=0;for(const auto& B:Colony.Buildings){if(B.Health<=0){++Destroyed;if(B.DefId==Target.Definition)++TargetDestroyed;}else{++Live;if(B.IsConstructing)++Unfinished;}}
            const FString Diagnostic=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Diagnostics"),FString::Printf(TEXT("developed-ai-failed-%d.json"),Colony.GenerationSeed));FString SaveError;const bool Saved=Colony.Save(Diagnostic,SaveError);
            Ready=false;Error=FString::Printf(TEXT("Developed AI preset cannot finish %s (%d/%d) by %.2f seconds. %s. %s; buildings %d live, %d destroyed, %d unfinished; target destroyed %d; failed %d. %s"),*Target.Definition,Count,Target.Count,Colony.Time,*Status,*Colony.WorkforceStatus(),Live,Destroyed,Unfinished,TargetDestroyed,Colony.Failed?1:0,*(Saved?TEXT("Diagnostic: ")+Diagnostic:TEXT("Diagnostic save failed: ")+SaveError));return false;
        }
    }
    Ready=false;Error=TEXT("Developed scenario preparation did not complete");return false;
}

bool FSeigeScenarioAI::AdvancePreparation(FSeigeSimulation& Colony,double MaxWallMilliseconds,bool& Complete,FString& Error)
{
    Complete=!Preparing;if(!Preparing)return Ready;
    if(!FMath::IsFinite(MaxWallMilliseconds)||MaxWallMilliseconds<=0){Error=TEXT("Preparation frame budget must be positive");return false;}
    const double Deadline=FPlatformTime::Seconds()+MaxWallMilliseconds/1000.;
    do
    {
        if(PreparationChunkRemaining<=UE_DOUBLE_SMALL_NUMBER)
        {
            if(Colony.Time+UE_DOUBLE_SMALL_NUMBER>=DevelopedSetupSeconds||Colony.Failed||Colony.Escaped){Complete=true;return FinishPreparation(Colony,Error);}
            PreparationChunkRemaining=FMath::Min(DecisionInterval,DevelopedSetupSeconds-Colony.Time);
        }
        // Preserve the original Tick(DecisionInterval) subtraction schedule,
        // including its final fractional substep. Recreating exact fixed steps
        // on every displayed frame changes floating-point decision boundaries
        // over a many-hour preparation and can change the resulting colony.
        const double Before=Colony.Time;
        const double Step=FMath::Min(PreparationChunkRemaining,Colony.FixedStepSeconds());
        PreparationChunkRemaining-=Step;
        Tick(Colony,Step);
        if(Colony.Time<=Before){Ready=false;Preparing=false;Complete=true;Error=TEXT("Developed AI preparation made no simulation progress");return false;}
        if(PreparationChunkRemaining<=UE_DOUBLE_SMALL_NUMBER||Colony.Failed||Colony.Escaped)
        {
            ReportPreparationMilestone(Colony);
            if(Colony.Buildings.Num()>DevelopedSetupLimit+1){Ready=false;Preparing=false;Complete=true;Error=TEXT("Developed AI exceeded its construction action budget");return false;}
            if(DevelopmentComplete(Colony)||Colony.Failed||Colony.Escaped){Complete=true;return FinishPreparation(Colony,Error);}
        }
    }while(FPlatformTime::Seconds()<Deadline);
    Complete=false;Error.Empty();return true;
}

void FSeigeScenarioAI::ReportPreparationMilestone(const FSeigeSimulation& Colony)
{
    // Diagnostics only: report new readiness highs and occasional summaries,
    // without changing colony events, saved state, decisions or completion.
    int32 Completed=0,Required=0,Destroyed=0,Unfinished=0;
    TMap<FString,int32> FinalCounts;
    for(const auto& Target:Targets)if(IncludesTarget(Colony,Target.Definition))FinalCounts.FindOrAdd(Target.Definition)=Target.Count;
    const auto* Core=Command(Colony);TArray<FString> Missing;TArray<FString> Ids;FinalCounts.GetKeys(Ids);Ids.Sort();
    for(const auto& Id:Ids)
    {
        int32 Count=0;
        for(const auto& B:Colony.Buildings)if(B.Health>0&&!B.IsConstructing&&B.Enabled&&B.DefId==Id&&B.Workers>=Colony.Definition(B)->Jobs&&Core&&Colony.IsRoadGridConnected(Core->Id,B.Id))++Count;
        Completed+=FMath::Min(Count,FinalCounts[Id]);Required+=FinalCounts[Id];
        if(Count<FinalCounts[Id])Missing.Add(FString::Printf(TEXT("%s:%d/%d"),*Id,Count,FinalCounts[Id]));
    }
    if(Completed<=PreparationBestCompleted&&Colony.Time-PreparationLastReportTime<6000)return;
    for(const auto& B:Colony.Buildings){if(B.Health<=0)++Destroyed;else if(B.IsConstructing)++Unfinished;}
    PreparationBestCompleted=FMath::Max(PreparationBestCompleted,Completed);PreparationLastReportTime=Colony.Time;
    const auto Grid=Colony.Energy.Info(Colony,Core?Core->Id:0);double GuardBattery=1;FString GuardOrder;
    for(const auto& Fleet:Colony.Combat.Fleets)
    {if(!GuardOrder.IsEmpty())GuardOrder+=TEXT(",");GuardOrder+=Fleet.Mission;for(const auto& V:Colony.Combat.Vehicles)if(V.FleetId==Fleet.Id&&V.Health>0&&!V.Evacuated)GuardBattery=FMath::Min(GuardBattery,V.BatteryKWh/Colony.Combat.Chassis[V.ChassisId].BatteryKWh);}
    UE_LOG(LogTemp,Log,TEXT("Seige AI preparation: seed=%d time=%.2f ready=%d/%d best=%d destroyed=%d unfinished=%d grid=%.2f/%.2fkW stored=%.3fkWh guard=%.1f%%/%s credits=%.5f status=%s missing=[%s]"),Colony.GenerationSeed,Colony.Time,Completed,Required,PreparationBestCompleted,Destroyed,Unfinished,Grid.GenerationKW,Grid.DemandKW,Grid.StoredKWh,GuardBattery*100,*GuardOrder,Colony.Credits,*Status,*FString::Join(Missing,TEXT(", ")));
}

int32 FSeigeScenarioAI::CountLive(const FSeigeSimulation& Colony, const FString& Definition) const
{
    int32 Count = 0;
    for (const FSeigeBuilding& B : Colony.Buildings) if (B.Health > 0 && B.DefId == Definition) ++Count;
    return Count;
}

int32 FSeigeScenarioAI::PlotDefenseCoverage(const FSeigeSimulation& Colony,const FSeigeBuildingDef& Definition,FVector2D Position) const
{
    // Structural firing geometry, not promised staffing, power or accuracy.
    // Only the owner's known buildings and actual equipped fixed weapons enter
    // this score. No enemies, mobile defenders or neighboring state are read.
    auto HitsBox=[](FVector2D From,FVector2D To,FVector2D Center,double HalfWidth)
    {
        double Lo=0,Hi=1;
        for(int32 Axis=0;Axis<2;++Axis)
        {
            const double Start=From[Axis]-Center[Axis],Delta=To[Axis]-From[Axis];
            if(FMath::Abs(Delta)<1.e-12){if(FMath::Abs(Start)>HalfWidth)return false;continue;}
            double A=(-HalfWidth-Start)/Delta,B=(HalfWidth-Start)/Delta;if(A>B)Swap(A,B);
            Lo=FMath::Max(Lo,A);Hi=FMath::Min(Hi,B);if(Lo>Hi)return false;
        }
        return true;
    };
    struct FFixedGun{int32 Id;FVector2D Position;double Range;};TArray<FFixedGun> Guns;
    for(const auto& Building:Colony.Buildings)if(Building.Health>0&&Building.Enabled&&!Building.IsConstructing)
        if(const auto* State=Colony.Combat.BuildingState.Find(Building.Id))
        {
            double Range=0;for(const auto& Id:State->Weapons)if(const auto* Weapon=Colony.Combat.Weapons.Find(Id))if(Weapon->Damage>0)Range=FMath::Max(Range,Weapon->RangeMeters/Colony.MetersPerWorldUnit());
            if(Range>0)Guns.Add({Building.Id,Building.Position,Range});
        }
    if(Guns.IsEmpty()||CoverageSamples<=0)return 0;
    int32 Covered=0;
    for(int32 Sample=0;Sample<CoverageSamples;++Sample)
    {
        const double Angle=Sample*UE_TWO_PI/CoverageSamples;const FVector2D Direction(FMath::Cos(Angle),FMath::Sin(Angle));
        // Probe outside the actual square body, including its corners. Reserved
        // upgrade plots are placement obstacles, not opaque combat hitboxes.
        const double Radius=Definition.Footprint/FMath::Max(FMath::Abs(Direction.X),FMath::Abs(Direction.Y))+CoverageProbeMeters/Colony.MetersPerWorldUnit();
        const FVector2D Approach=Position+Direction*Radius;
        for(const auto& Gun:Guns)
        {
            if(FVector2D::Distance(Gun.Position,Approach)>=Gun.Range||HitsBox(Gun.Position,Approach,Position,Definition.Footprint))continue;
            bool Blocked=false;
            for(const auto& Building:Colony.Buildings)if(Building.Health>0&&Building.Id!=Gun.Id&&HitsBox(Gun.Position,Approach,Building.Position,Colony.Definition(Building)->Footprint)){Blocked=true;break;}
            if(!Blocked){++Covered;break;}
        }
    }
    return Covered;
}

bool FSeigeScenarioAI::BuildNear(FSeigeSimulation& Colony, const FString& Definition, FVector2D Anchor, double StartingAngle, double FirstRadius)
{
    const FSeigeBuildingDef& Def = Colony.BuildingDefs[Definition];
    const bool PreferCoverage=DefenseCoveragePolicy==TEXT("prefer_covered_approaches")&&!CoverageExcludedRoles.Contains(Def.Role);
    struct FPlot{FVector2D Position;int32 Coverage;};TArray<FPlot> Candidates;
    auto Consider = [&](FVector2D Position)
    {
        for (const FSeigeNode& Node : Colony.Nodes)
            if (FVector2D::Distance(Position, Node.Position) < NodeClearance + Def.ReservedFootprint) return false;
        FString Error;
        if(PreferCoverage)
        {
            if(!Colony.CanPlaceBuilding(Definition,Position,Error)){Status=Error;return false;}
            Candidates.Add({Position,PlotDefenseCoverage(Colony,Def,Position)});return false;
        }
        if (!PlaceConnectedBuilding(Colony, Definition, Position, Error)) { Status = Error; return false; }
        Status = TEXT("Built ") + Def.Name; return true;
    };
    if (Consider(Anchor)) return true;
    for (double Radius = FirstRadius>=0?FirstRadius:RingStart; Radius <= RingLimit; Radius += RingStep)
        for (int32 I = 0; I < Angles; ++I)
        {
            const double Angle = StartingAngle + I * UE_TWO_PI / Angles;
            if (Consider(Anchor + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius)) return true;
        }
    // Road preflight remains mandatory, but only ranked candidates need it.
    // Stable ties retain authored spread; zero coverage is a valid fallback,
    // so bootstrap or an impossible defensive layout never becomes a gate.
    Candidates.StableSort([](const FPlot& A,const FPlot& B){return A.Coverage>B.Coverage;});
    for(const auto& Candidate:Candidates)
    {
        FString Error;if(!PlaceConnectedBuilding(Colony,Definition,Candidate.Position,Error)){Status=Error;continue;}
        Status=TEXT("Built ")+Def.Name;return true;
    }
    return false;
}

bool FSeigeScenarioAI::ExtendSensors(FSeigeSimulation& Colony, FVector2D Destination)
{
    if (CountLive(Colony, SensorDefinition) >= MaxSensors) { Status = TEXT("Sensor extension limit reached"); return false; }
    for (const FSeigeBuilding& B : Colony.Buildings)
        if (B.DefId == SensorDefinition && B.Health > 0 && B.Enabled && (B.IsConstructing||B.Workers < Colony.Definition(B)->Jobs))
        { Status = TEXT("Waiting for existing sensor staffing"); return false; }
    const FSeigeBuilding* Source = nullptr; double BestRemaining = TNumericLimits<double>::Max();
    for (const FSeigeBuilding& B : Colony.Buildings)
    {
        const FSeigeBuildingDef* Def = Colony.Definition(B);
        if (B.Health <= 0 || B.IsConstructing || !B.Enabled || !Def || Def->SensorRange <= 0 || B.Workers < Def->Jobs) continue;
        const double Remaining = FVector2D::Distance(B.Position, Destination) - Def->SensorRange;
        if (Remaining < BestRemaining) { Source = &B; BestRemaining = Remaining; }
    }
    if (!Source) { Status = TEXT("Waiting for an operating sensor"); return false; }
    const FVector2D Direction = (Destination - Source->Position).GetSafeNormal();
    const FVector2D Position = Source->Position + Direction * Colony.Definition(*Source)->SensorRange * SensorOverlap;
    return BuildNear(Colony, SensorDefinition, Position, FMath::Atan2(Direction.Y, Direction.X));
}

const FSeigeNode* FSeigeScenarioAI::ExportNode(const FSeigeSimulation& Colony) const
{
    const auto* Core=Command(Colony);const FVector2D Origin=Core?Core->Position:FVector2D::ZeroVector;
    const FSeigeNode* Best=nullptr;double Distance=TNumericLimits<double>::Max();
    for(const auto& Node:Colony.Nodes)if(const auto* R=Colony.Resources.Find(Node.Resource))if(R->Class==TEXT("standard"))
    {const double D=FVector2D::DistSquared(Origin,Node.Position);if(D<Distance||(D==Distance&&Best&&Node.Id<Best->Id)){Best=&Node;Distance=D;}}
    return Best;
}

bool FSeigeScenarioAI::IncludesTarget(const FSeigeSimulation& Colony,const FString& Definition) const
{
    const auto* D=Colony.BuildingDefs.Find(Definition);if(!D)return false;
    if(D->ExtractionRates.IsEmpty())return true;
    const auto* Node=ExportNode(Colony);return Node&&D->ExtractionRates.Contains(Node->Resource);
}

bool FSeigeScenarioAI::PlaceConnectedBuilding(FSeigeSimulation& Colony,const FString& Definition,FVector2D Position,FString& Error)
{
    if(!Colony.CanPlaceBuilding(Definition,Position,Error))return false;
    FSeigeBuilding Preview;Preview.Id=INDEX_NONE;Preview.DefId=Definition;Preview.Position=Position;Preview.Health=Colony.BuildingDefs[Definition].Health;
    FVector2D A,B;bool NeedsSegment=false;
    if(!FindPowerConnection(Colony,Preview,&Preview,A,B,NeedsSegment,Error))return false;
    return Colony.PlaceBuilding(Definition,Position,Error);
}

bool FSeigeScenarioAI::FindPowerConnection(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuilding* ProspectivePlot,FVector2D& OutA,FVector2D& OutB,bool& NeedsSegment,FString& Error) const
{
    NeedsSegment=false;const auto* Core=Command(Colony);if(!Core){Error=TEXT("No operating command core");return false;}
    auto OnSegment=[](FVector2D P,FVector2D A,FVector2D B)
    {const FVector2D V=B-A;const double T=FVector2D::DotProduct(P-A,V)/FMath::Max(V.SizeSquared(),UE_DOUBLE_SMALL_NUMBER);return T>=-.00001&&T<=1.00001&&FVector2D::Distance(P,A+V*T)<.01;};
    TArray<FVector2D> Starts{Colony.BuildingAccessPoint(*Core)};
    // Disconnected fragments must not make a new plot appear grid-connected.
    for(const auto& R:Colony.Roads)if(R.Health>0&&!R.Tier.IsEmpty()&&Colony.Energy.RoadConnectedToBuilding(R.Id,Core->Id)){Starts.AddUnique(R.A);Starts.AddUnique(R.B);}
    const FVector2D End=Colony.BuildingAccessPoint(Building);
    Starts.Sort([&](const FVector2D&A,const FVector2D&B){return FVector2D::DistSquared(A,End)<FVector2D::DistSquared(B,End);});
    Error=TEXT("No dry, covered route reaches this access point");
    for(const auto& Start:Starts)
    {
        TArray<FVector2D> Route;if(!Colony.FindRoadRoute(Start,End,Route,ProspectivePlot))continue;
        FVector2D Previous=Start,FirstA=Start,FirstB=Start;bool Valid=true,HasNewSegment=false;
        for(const auto& Point:Route)
        {
            if(Previous.Equals(Point,.01)){Previous=Point;continue;}
            const bool Exists=Colony.Roads.ContainsByPredicate([&](const auto& R){return R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(Previous,R.A,R.B)&&OnSegment(Point,R.A,R.B);});
            if(!Exists)
            {
                FVector2D Candidate=Point;
                if(Point.Equals(End,.01)&&FVector2D::Distance(Previous,Point)<Colony.MinimumRoadLength())
                    Candidate=Point+(Point-Previous).GetSafeNormal()*FMath::Max(RingStep,Colony.MinimumRoadLength());
                // An extended endpoint did not belong to the route search. Recheck
                // its whole corridor, including the not-yet-committed plot.
                if(!Candidate.Equals(Point,.000001)&&!Colony.ClearRoadLine(Previous,Candidate,ProspectivePlot))
                {Error=TEXT("Extended road corridor intersects water or a reserved plot");Valid=false;break;}
                // Check the entire future connection before accepting the plot,
                // while ordinary PlaceRoad still charges each segment in order.
                if(!Colony.CanPlaceRoad(Previous,Candidate,Error,false)){Valid=false;break;}
                if(!HasNewSegment){FirstA=Previous;FirstB=Candidate;HasNewSegment=true;}
            }
            Previous=Point;
        }
        if(Valid){NeedsSegment=HasNewSegment;OutA=FirstA;OutB=FirstB;Error.Empty();return true;}
    }
    return false;
}

bool FSeigeScenarioAI::NextPowerRoad(const FSeigeSimulation& Colony,FVector2D& OutA,FVector2D& OutB,FString& TargetName,FString& Error) const
{
    TargetName.Empty();Error.Empty();const auto* Core=Command(Colony);if(!Core)return false;
    for(const auto& R:Colony.Roads)if(R.Health>0&&R.IsConstructing){Error=TEXT("Constructing road-grid connection");return false;}
    for(const auto& Building:Colony.Buildings)
    {
        if(Building.Id==Core->Id||Building.Health<=0||Building.IsConstructing||Colony.IsRoadGridConnected(Core->Id,Building.Id))continue;
        TargetName=Colony.Definition(Building)->Name;
        bool NeedsSegment=false;
        return FindPowerConnection(Colony,Building,nullptr,OutA,OutB,NeedsSegment,Error)&&NeedsSegment;
    }
    return false;
}

bool FSeigeScenarioAI::ConnectPowerRoad(FSeigeSimulation& Colony,bool& Waiting)
{
    FVector2D A,B;FString TargetName,Error;
    const bool Planned=NextPowerRoad(Colony,A,B,TargetName,Error);
    Waiting=Planned||!TargetName.IsEmpty()||!Error.IsEmpty();
    if(Planned&&Colony.PlaceRoad(A,B,Error)){Status=TEXT("Connecting ")+TargetName+TEXT(" to the road power grid");return true;}
    if(Waiting)Status=TargetName.IsEmpty()?Error:TEXT("Waiting for a valid road-grid connection: ")+TargetName+TEXT(". ")+Error;
    return false;
}

TMap<FString,double> FSeigeScenarioAI::ProductionGoals(const FSeigeSimulation& Colony) const
{
    TMap<FString,double> Goals=ReserveTargets;
    for(const auto& B:Colony.Buildings)if(B.Health>0&&B.Enabled&&!B.IsConstructing)
    {
        // The core must not manufacture inputs merely because its own last
        // selection requested them. Other real production remains a consumer.
        if(Colony.Definition(B)->Role!=TEXT("core"))
            for(const auto& P:Colony.ProductionInputs(B,Colony.ActiveProductionRecipe(B)))Goals.FindOrAdd(P.Key)+=P.Value*RecipeInputBuffer;
        if(const auto* E=Colony.Energy.Definition(B.DefId))if(!E->FuelResource.IsEmpty())Goals.FindOrAdd(E->FuelResource)+=Colony.Energy.FuelDemand(B.DefId,E->FuelResource)*FuelImportBufferCycles;
    }
    FVector2D A,B;FString Target,Error;TMap<FString,double> Cost;
    if(NextPowerRoad(Colony,A,B,Target,Error))Cost=Colony.RoadCost(A,B,Colony.InitialRoadTier());
    else if(Target.IsEmpty())if(const auto* Next=NextConstructionTarget(Colony))Cost=Colony.BuildingDefs[Next->Definition].Cost;
    for(const auto& P:Cost)Goals.FindOrAdd(P.Key)=FMath::Max(Goals.FindRef(P.Key),Colony.TotalStock(P.Key)+FMath::Max(0.,P.Value-Colony.ConstructionAvailable(P.Key)));
    return Goals;
}

bool FSeigeScenarioAI::ManageCoreProduction(FSeigeSimulation& Colony)
{
    const auto* Core=Command(Colony);if(!Core||Core->IsConstructing||!Core->Enabled||Core->ProductionCommitted)return false;
    const auto Goals=ProductionGoals(Colony);FString Selected;double Best=-1;bool Funded=false;
    for(const auto& Id:CoreReplicationRecipes)
    {
        if(!Colony.ProductionOptions(Core->Id).Contains(Id))continue;
        // Prefer the faster, less energy-intensive installed specialist once
        // it has actual operators. Its paid input demand is funded below.
        if(Colony.Buildings.ContainsByPredicate([&](const auto& Other){const auto* D=Colony.Definition(Other);return Other.Id!=Core->Id&&Other.Health>0&&Other.Enabled&&!Other.IsConstructing&&Other.Workers>=D->Jobs&&Colony.ActiveProductionRecipe(Other)==Id;}))continue;
        double Score=0;for(const auto& P:Colony.Recipes[Id].Outputs)
        {const double Need=Goals.FindRef(P.Key);if(Need>0)Score=FMath::Max(Score,FMath::Max(0.,Need-Colony.TotalStock(P.Key))/Need);}
        if(Score<=UE_DOUBLE_SMALL_NUMBER)continue;
        const bool ReadyToPay=Colony.CanCommitProduction(*Core,Id);
        if(Selected.IsEmpty()||(ReadyToPay&&!Funded)||(ReadyToPay==Funded&&Score>Best+UE_DOUBLE_SMALL_NUMBER)){Selected=Id;Best=Score;Funded=ReadyToPay;}
    }
    // A waiting selection creates ordinary material demand; it does not start
    // a free batch. If no goods shortage remains, return to job-driven assembly.
    if(Selected.IsEmpty())for(const auto& Id:Colony.ProductionOptions(Core->Id))if(Colony.Recipes[Id].WorkerOutput>0){Selected=Id;break;}
    if(Selected.IsEmpty()||Selected==Core->SelectedRecipe)return false;
    FString Error;if(!Colony.SetProductionRecipe(Core->Id,Selected,Error))return false;
    Status=TEXT("Command replicator selected ")+Selected;return true;
}

TMap<FString,double> FSeigeScenarioAI::UsefulFeedstockTargets(const FSeigeSimulation& Colony,const TMap<FString,double>& Goals) const
{
    TMap<FString,double> TargetsByResource;if(BulkInputPolicy!=TEXT("remaining_output_bill"))return TargetsByResource;
    TMap<FString,double> Remaining;
    for(const auto& Goal:Goals)if(Colony.Resources[Goal.Key].Class==TEXT("manufactured")&&!Colony.Resources[Goal.Key].Discrete)
        Remaining.Add(Goal.Key,FMath::Max(0.,Goal.Value-Colony.TotalStock(Goal.Key)));
    TArray<FString> Outputs;Remaining.GetKeys(Outputs);
    Outputs.Sort([&](const FString& A,const FString& B){const double Left=Remaining[A]*Colony.Resources[A].UnitMassKg,Right=Remaining[B]*Colony.Resources[B].UnitMassKg;return Left==Right?A<B:Left>Right;});
    for(const auto& Output:Outputs)
    {
        if(Remaining[Output]<=UE_DOUBLE_SMALL_NUMBER)continue;
        const FSeigeBuilding* Best=nullptr;const FSeigeRecipeDef* Recipe=nullptr;double BestMass=TNumericLimits<double>::Max(),BestTime=TNumericLimits<double>::Max();
        for(const auto& B:Colony.Buildings)if(B.Health>0&&B.Enabled&&!B.IsConstructing)
        {
            const auto* R=Colony.Recipes.Find(Colony.ActiveProductionRecipe(B));if(!R||R->WorkerOutput>0||R->Outputs.FindRef(Output)<=0)continue;
            const double Mass=Colony.InventoryMassKg(Colony.ProductionInputs(B,R->Id))/(R->Outputs[Output]*Colony.Resources[Output].UnitMassKg),Time=Colony.ProductionSeconds(B,R->Id)/R->Outputs[Output];
            if(!Best||Mass<BestMass-UE_DOUBLE_SMALL_NUMBER||(FMath::IsNearlyEqual(Mass,BestMass,UE_DOUBLE_SMALL_NUMBER)&&Time<BestTime))
            {Best=&B;Recipe=R;BestMass=Mass;BestTime=Time;}
        }
        if(!Best||!Recipe)continue;
        const double Batches=FMath::CeilToDouble(Remaining[Output]/Recipe->Outputs[Output]);
        // Only raw feedstock gets bulk stocking. Worker bodies and expensive
        // manufactured recipe inputs retain their existing finite buffers.
        for(const auto& Input:Colony.ProductionInputs(*Best,Recipe->Id))if(Colony.Resources[Input.Key].Class!=TEXT("manufactured"))TargetsByResource.FindOrAdd(Input.Key)+=Input.Value*Batches;
        // Coproducts satisfy later goals; do not budget their ore a second time.
        for(const auto& Product:Recipe->Outputs)if(Remaining.Contains(Product.Key))Remaining[Product.Key]=FMath::Max(0.,Remaining[Product.Key]-Product.Value*Batches);
    }
    return TargetsByResource;
}

double FSeigeScenarioAI::ManufacturedExportSurplus(const FSeigeSimulation& Colony,const FString& Resource,double NextRoadBill) const
{
    const auto* R=Colony.Resources.Find(Resource);
    if(!R||R->Discrete||R->Class!=TEXT("manufactured"))return 0;
    TMap<FString,int32> FinalCounts;for(const auto& Target:Targets)if(IncludesTarget(Colony,Target.Definition))FinalCounts.FindOrAdd(Target.Definition)=FMath::Max(FinalCounts.FindRef(Target.Definition),Target.Count);
    double FutureBills=NextRoadBill,UninstalledBills=0;
    for(const auto& Target:FinalCounts)FutureBills+=FMath::Max(0,Target.Value-CountLive(Colony,Target.Key))*Colony.BuildingDefs[Target.Key].Cost.FindRef(Resource);
    // In-progress plots count toward the plan, but their remaining paid bill
    // stays protected whether the goods are at source, in transit or on site.
    for(const auto& B:Colony.Buildings)if(B.Health>0&&B.IsConstructing)UninstalledBills+=FMath::Max(0.,Colony.ConstructionCost(B).FindRef(Resource)-B.InstalledMaterials.FindRef(Resource));
    for(const auto& Road:Colony.Roads)if(Road.Health>0&&Road.IsConstructing)UninstalledBills+=FMath::Max(0.,Colony.RoadCost(Road.A,Road.B,Road.TargetTier).FindRef(Resource)-Road.InstalledMaterials.FindRef(Resource));
    double Buffer=Colony.OperatingBuffer(Resource)+ReserveTargets.FindRef(Resource),WorkerInput=0;
    for(const auto& Building:Colony.Buildings)if(Building.Health>0&&Building.Enabled&&!Building.IsConstructing)
    {
        Buffer+=Colony.ProductionInputs(Building,Colony.ActiveProductionRecipe(Building)).FindRef(Resource)*RecipeInputBuffer;
        if(const auto* E=Colony.Energy.Definition(Building.DefId))if(E->FuelResource==Resource)Buffer+=Colony.Energy.FuelDemand(Building.DefId,Resource)*FuelImportBufferCycles;
        for(const auto& Id:Colony.ProductionOptions(Building.Id))if(Colony.Recipes[Id].WorkerOutput>0)
            WorkerInput=FMath::Max(WorkerInput,Colony.ProductionInputs(Building,Id).FindRef(Resource)/Colony.Recipes[Id].WorkerOutput);
    }
    Buffer+=WorkerInput*Colony.WorkerReserveTarget();
    // The local spendable bound also protects pickup claims. The gross-stock
    // bound retains full future bills plus conservative operating/AI buffers.
    return FMath::Max(0.,FMath::Min(Colony.ConstructionAvailable(Resource)-FutureBills,
        Colony.TotalStock(Resource)-FutureBills-UninstalledBills-Buffer));
}

bool FSeigeScenarioAI::ManageTrade(FSeigeSimulation& Colony)
{
    const FSeigeBuilding* Port=Colony.Buildings.FindByPredicate([&](const auto&B){return B.Health>0&&!B.IsConstructing&&B.Enabled&&Colony.Definition(B)->Role==TEXT("trade");});
    const auto* Node=ExportNode(Colony);if(!Port||!Node)return false;
    if(!Port->Shipment.Resource.IsEmpty())
    {
        const auto& Order=Port->Shipment;
        if(!Order.Buy&&!Order.Departed&&Order.GoodsEscrow==0&&Colony.TotalStock(Order.Resource)+UE_DOUBLE_SMALL_NUMBER<Order.Quantity)
        {
            bool CanReplenish=false;
            for(const auto& B:Colony.Buildings)if(B.Health>0&&B.Enabled&&!B.IsConstructing)
            {
                if(Colony.ExtractionResource(B)==Order.Resource&&Colony.ExtractionRate(B)>0){CanReplenish=true;break;}
                const auto* Recipe=Colony.Recipes.Find(Colony.ActiveProductionRecipe(B));
                if(Recipe&&Recipe->Outputs.FindRef(Order.Resource)>0){CanReplenish=true;break;}
            }
            FString Error;
            if(!CanReplenish&&Colony.CancelPendingExport(Port->Id,Error)){Status=TEXT("Cancelled unfillable export after loss of its supply");return true;}
        }
        return false;
    }
    const auto* PortDef=Colony.Trade.Definition(Port->DefId);if(!PortDef)return false;
    auto ShipmentSize=[&](const FString& Id,double DesiredAmount,bool Buy)
    {
        const auto& Resource=Colony.Resources[Id];
        const double Space=Buy?Colony.StorageRoom(*Port):Colony.Definition(*Port)->StorageCapacity;
        double Amount=FMath::Max(0.,FMath::Min3(DesiredAmount,PortDef->CapacityKg/Resource.UnitMassKg,Space/Resource.LitresPerUnit));
        if(Resource.Discrete)Amount=FMath::FloorToDouble(Amount);
        return Amount;
    };
    TMap<FString,double> Desired=ReserveTargets,FuelNeeds;
    for(const auto& B:Colony.Buildings)if(B.Health>0&&B.Enabled)
    {
        // Selectable factories and the command replicator have no fixed D.Recipe.
        // Fund their real next/committed batch, including per-building multipliers.
        const FString Recipe=Colony.ActiveProductionRecipe(B);
        if(!Recipe.IsEmpty())for(const auto& P:Colony.ProductionInputs(B,Recipe))
        {const double Buffer=P.Value*RecipeInputBuffer;Desired.FindOrAdd(P.Key)+=Colony.Resources[P.Key].Class==TEXT("manufactured")?Buffer:FMath::Max(Buffer,Colony.ProductionInputBuffer(B,Recipe,P.Key));}
        if(!B.IsConstructing)if(const auto* E=Colony.Energy.Definition(B.DefId))if(!E->FuelResource.IsEmpty())
        {const double Fuel=Colony.Energy.FuelDemand(B.DefId,E->FuelResource)*FuelImportBufferCycles;FuelNeeds.FindOrAdd(E->FuelResource)+=Fuel;Desired.FindOrAdd(E->FuelResource)+=Fuel;}
    }
    for(const auto& Fuel:FuelNeeds)if(Colony.TotalStock(Fuel.Key)+UE_DOUBLE_SMALL_NUMBER>=Fuel.Value*FuelImportRefillFraction)Desired.FindOrAdd(Fuel.Key)=Colony.TotalStock(Fuel.Key);
    FVector2D RoadA,RoadB;FString RoadTarget,RoadError;
    const bool HasRoadBill=NextPowerRoad(Colony,RoadA,RoadB,RoadTarget,RoadError);
    // Power connection precedes downstream expansion in MakeDecision. Trade
    // funds that same legal segment, not a guessed road reserve or a later factory.
    const auto* Construction=RoadTarget.IsEmpty()?NextConstructionTarget(Colony):nullptr;
    if(Construction)for(const auto& P:Colony.BuildingDefs[Construction->Definition].Cost)Desired.FindOrAdd(P.Key)+=P.Value;
    TArray<FString> Ids;Desired.GetKeys(Ids);Ids.Sort();FString Missing;double Worst=0,CreditBuffer=0;
    for(const auto& Id:Ids)
    {
        CreditBuffer=FMath::Max(CreditBuffer,Colony.TradeQuote(Id,ShipmentSize(Id,ImportBatch,true),true)*CreditBufferBatches);
        if(Id==Node->Resource)continue;
        const double Need=Desired[Id],Stock=Colony.TotalStock(Id);
        const double StockDeficit=FMath::Max(0.,Need-Stock);
        const double Reserve=ReserveTargets.FindRef(Id);
        const double ReserveDeficit=Reserve>0?FMath::Max(0.,Reserve-Colony.ConstructionAvailable(Id)):0;
        // A construction reserve is spare material, not stock already needed
        // by local repairs, factory buffers, or existing paid construction.
        // Recipe-input shortages retain their normal physical-stock measure.
        const double Ratio=FMath::Max(StockDeficit/FMath::Max(Need,1.),ReserveDeficit/FMath::Max(Reserve,1.));
        Desired[Id]=Stock+FMath::Max(StockDeficit,ReserveDeficit);
        if(Ratio>Worst){Missing=Id;Worst=Ratio;}
    }
    // The next required construction takes precedence over routine input
    // buffers, with missing support first. Gross colony stock includes local
    // repair/production reserves that cannot actually pay that building bill.
    if(HasRoadBill||Construction)
    {
        const auto Cost=HasRoadBill?Colony.RoadCost(RoadA,RoadB,Colony.InitialRoadTier()):Colony.BuildingDefs[Construction->Definition].Cost;
        TArray<FString> Materials;Cost.GetKeys(Materials);Materials.Sort();
        double CriticalRatio=0;FString Critical;double Deficit=0;
        for(const auto& Id:Materials)
        {
            const double Need=FMath::Max(0.,Cost[Id]-Colony.ConstructionAvailable(Id)),Ratio=Need/FMath::Max(Cost[Id],1.);
            if(Ratio>CriticalRatio){CriticalRatio=Ratio;Critical=Id;Deficit=Need;}
        }
        if(!Critical.IsEmpty()){Missing=Critical;Desired.FindOrAdd(Critical)=Colony.TotalStock(Critical)+Deficit;}
    }
    // Fund a useful input refill when a real producer cannot make one batch
    // of goods that the current plan needs. Expansion must not buy finished
    // goods forever while its already-paid upstream factories lack feedstock.
    const auto Goals=ProductionGoals(Colony),BulkInputs=UsefulFeedstockTargets(Colony,Goals);TMap<FString,double> BatchInputs,BufferedInputs;
    for(const auto& B:Colony.Buildings)if(B.Health>0&&B.Enabled&&!B.IsConstructing)
    {
        const FString Id=Colony.ActiveProductionRecipe(B);const auto* Recipe=Colony.Recipes.Find(Id);if(!Recipe)continue;
        bool Needed=Recipe->WorkerOutput>0;
        for(const auto& Output:Recipe->Outputs)Needed|=Colony.TotalStock(Output.Key)+UE_DOUBLE_SMALL_NUMBER<Goals.FindRef(Output.Key);
        if(!Needed)continue;
        for(const auto& Input:Colony.ProductionInputs(B,Id))
        {BatchInputs.FindOrAdd(Input.Key)+=Input.Value;const double Buffer=Input.Value*RecipeInputBuffer;BufferedInputs.FindOrAdd(Input.Key)+=Colony.Resources[Input.Key].Class==TEXT("manufactured")?Buffer:FMath::Max(Buffer,Colony.ProductionInputBuffer(B,Id,Input.Key));}
    }
    TArray<FString> BatchResources;BatchInputs.GetKeys(BatchResources);BatchResources.Sort();double BatchRatio=0;
    const bool EssentialConstruction=HasRoadBill||(Construction&&(Colony.BuildingDefs[Construction->Definition].RobotSupportCapacity>0||Colony.BuildingDefs[Construction->Definition].Role==TEXT("generator")||Colony.BuildingDefs[Construction->Definition].Role==TEXT("trade")));
    for(const auto& Id:BatchResources)
    {
        if(Id==Node->Resource)continue;
        const double Stock=Colony.TotalStock(Id),Need=BulkInputs.Contains(Id)?BulkInputs[Id]:BatchInputs[Id],Ratio=FMath::Max(0.,Need-Stock)/FMath::Max(Need,UE_DOUBLE_SMALL_NUMBER);
        if(!EssentialConstruction&&Ratio>BatchRatio){BatchRatio=Ratio;Missing=Id;Desired.FindOrAdd(Id)=BulkInputs.Contains(Id)?BulkInputs[Id]:FMath::Max(Desired.FindRef(Id),BufferedInputs[Id]);}
    }
    // Keep an installed generator's externally configured operating buffer
    // supplied before spending its remaining power on downstream expansion.
    // Local fuel still travels by ordinary courier after the paid import.
    TArray<FString> Fuels;FuelNeeds.GetKeys(Fuels);Fuels.Sort();double FuelRatio=0;
    for(const auto& Fuel:Fuels)
    {
        const double Stock=Colony.TotalStock(Fuel);
        const double Deficit=Stock+UE_DOUBLE_SMALL_NUMBER<FuelNeeds[Fuel]*FuelImportRefillFraction?FMath::Max(0.,FuelNeeds[Fuel]-Stock):0,Ratio=Deficit/FMath::Max(FuelNeeds[Fuel],UE_DOUBLE_SMALL_NUMBER);
        if(Ratio>FuelRatio){FuelRatio=Ratio;Missing=Fuel;Desired.FindOrAdd(Fuel)=Colony.TotalStock(Fuel)+Deficit;}
    }
    FString Error;
    if(!Missing.IsEmpty())
    {
        // Accumulate export earnings for a useful full shipment rather than
        // spending every tiny credit remainder on another partial flight.
        double Amount=ShipmentSize(Missing,FMath::Min(ImportBatch,Desired[Missing]-Colony.TotalStock(Missing)),true);
        if(BulkInputs.Contains(Missing))
        {
            const double Price=Colony.TradeQuote(Missing,1,true);
            if(Price>0)Amount=FMath::Min(Amount,Colony.Credits/Price);
            // Do not turn a tiny credit remainder into repeated useless raw
            // flights. A final small remainder that completes the bill is valid.
            const double UsefulMinimum=FMath::Min(FMath::Max(0.,BulkInputs[Missing]-Colony.TotalStock(Missing)),BatchInputs.FindRef(Missing));
            if(Amount+UE_DOUBLE_SMALL_NUMBER<UsefulMinimum)Amount=0;
        }
        if(Amount>UE_DOUBLE_SMALL_NUMBER&&Colony.TryTrade(Port->Id,Missing,Amount,true,Error)){Status=TEXT("Importing ")+Colony.Resources[Missing].Name+TEXT(" using export earnings");return true;}
    }
    if(!Missing.IsEmpty()||Colony.Credits<CreditBuffer)
    {
        const double Surplus=FMath::Max(0.,Colony.TotalStock(Node->Resource)-Desired.FindRef(Node->Resource));
        FString ExportResource=Node->Resource;double Amount=ShipmentSize(ExportResource,FMath::Min3(ExportBatch,Surplus,Colony.ConstructionAvailable(ExportResource)),false);
        double Value=Amount>UE_DOUBLE_SMALL_NUMBER&&Colony.CanTrade(Port->Id,ExportResource,Amount,false,Error)?Colony.TradeQuote(ExportResource,Amount,false):0;
        if(SurplusExportPolicy==TEXT("surplus_shipment_value"))
        {
            const auto RoadBill=HasRoadBill?Colony.RoadCost(RoadA,RoadB,Colony.InitialRoadTier()):TMap<FString,double>();
            TArray<FString> Candidates;Colony.Resources.GetKeys(Candidates);Candidates.Sort();
            for(const auto& Id:Candidates)
            {
                const double CandidateAmount=ShipmentSize(Id,FMath::Min(ExportBatch,ManufacturedExportSurplus(Colony,Id,RoadBill.FindRef(Id))),false);
                if(CandidateAmount<=UE_DOUBLE_SMALL_NUMBER||!Colony.CanTrade(Port->Id,Id,CandidateAmount,false,Error))continue;
                const double Quote=Colony.TradeQuote(Id,CandidateAmount,false);
                if(Quote>Value+UE_DOUBLE_SMALL_NUMBER){ExportResource=Id;Amount=CandidateAmount;Value=Quote;}
            }
        }
        if(Value>UE_DOUBLE_SMALL_NUMBER&&Colony.TryTrade(Port->Id,ExportResource,Amount,false,Error)){Status=TEXT("Exporting surplus ")+Colony.Resources[ExportResource].Name;return true;}
    }
    return false;
}

bool FSeigeScenarioAI::GuardWorksite(FSeigeSimulation& Colony)
{
    const auto* Protected=Command(Colony);if(!Protected)return false;
    const FVector2D ServiceStation=Colony.BuildingAccessPoint(*Protected);
    // Defend the current paid construction effort; between projects retain the
    // newest living installation as the guard station. This gives orders to the
    // finite carried fleet rather than creating defenders or granting immunity.
    for(const auto& B:Colony.Buildings)if(B.Health>0&&B.Enabled&&B.Id!=Protected->Id)
    {
        if((B.IsConstructing&&!Protected->IsConstructing)||(B.IsConstructing==Protected->IsConstructing&&B.Id>Protected->Id))Protected=&B;
    }
    // Visible enemies select the nearest living installation. Respond to the
    // most imminent observed approach instead of abandoning an attacked port or
    // extractor whenever a newer construction site appears. No hidden contacts
    // or remote-neighbor state enter this decision.
    const FSeigeBuilding* Threatened=nullptr;double Imminence=TNumericLimits<double>::Max();
    for(const auto& Enemy:Colony.Enemies)if(Enemy.Health>0&&Colony.IsVisible(Enemy.Position))
    {
        const FSeigeBuilding* Target=nullptr;double Nearest=TNumericLimits<double>::Max();
        for(const auto& B:Colony.Buildings)if(B.Health>0){const double Distance=FVector2D::DistSquared(Enemy.Position,B.Position);if(Distance<Nearest){Nearest=Distance;Target=&B;}}
        if(Target){const double Distance=FMath::Max(0.,FMath::Sqrt(Nearest)-Colony.Definition(*Target)->Footprint);if(Distance<Imminence){Imminence=Distance;Threatened=Target;}}
    }
    if(Threatened)Protected=Threatened;
    const FVector2D Station=Colony.BuildingAccessPoint(*Protected);
    for(const auto& Fleet:Colony.Combat.Fleets)
    {
        // A fixed move to the core is a persisted ordinary service order. Its
        // mission and destination retain hysteresis across save/load, without
        // a hidden AI-only state machine or free battery replenishment.
        const bool Servicing=Fleet.Mission==TEXT("move")&&Fleet.Destination.Equals(ServiceStation,.01);
        if(Fleet.DestinationSector!=4||(Fleet.Mission!=TEXT("defense")&&Fleet.Mission!=TEXT("escort")&&!Servicing))continue;
        bool Living=false,Embarked=false;double LowestBattery=1;
        for(const auto& V:Colony.Combat.Vehicles)if(V.FleetId==Fleet.Id&&V.Health>0&&!V.Evacuated)
        {Living=true;Embarked|=V.Embarked;LowestBattery=FMath::Min(LowestBattery,V.BatteryKWh/Colony.Combat.Chassis[V.ChassisId].BatteryKWh);}
        if(!Living)continue;
        // A visible attack can interrupt a long refill once every living guard
        // has more than its normal return reserve. Quiet service still waits
        // for the full resume threshold, and low batteries never gain energy.
        const bool RespondDuringService=Threatened&&LowestBattery>GuardRechargeBelow;
        if(LowestBattery<GuardRechargeBelow||(Servicing&&LowestBattery<GuardResumeAbove&&!RespondDuringService))
        {
            if(!Servicing)
            {
                FString Error;
                if(Colony.Combat.OrderFleet(Colony,Fleet.Id,TEXT("move"),ServiceStation,0,Error)&&Colony.Combat.SetAggression(Fleet.Id,TEXT("defensive"),Error))
                {Status=TEXT("Guard fleet returning for grid-powered charging");return true;}
            }
            else if(Fleet.Aggression!=TEXT("defensive"))
            {FString Error;if(Colony.Combat.SetAggression(Fleet.Id,TEXT("defensive"),Error))return true;}
            // Defensive MOVE permits paid in-range fire, but neither pursues
            // enemies nor runs the defense/escort flanking movement.
            // While charging, economic decisions must still progress.
            continue;
        }
        if(!Living||(!Embarked&&Fleet.Mission==TEXT("defense")&&Fleet.Destination.Equals(Station,.01)))continue;
        FString Error;
        if(Colony.Combat.OrderFleet(Colony,Fleet.Id,TEXT("defense"),Station,0,Error)&&Colony.Combat.SetAggression(Fleet.Id,TEXT("defensive"),Error))
        {Status=TEXT("Guard fleet defending ")+Colony.Definition(*Protected)->Name;return true;}
    }
    return false;
}

const FSeigeAIBuildTarget* FSeigeScenarioAI::NextConstructionTarget(const FSeigeSimulation& Colony) const
{
    if(const auto* Recovery=SupportRecoveryTarget(Colony))return Recovery;
    if(Colony.RobotSupportCapacity<FMath::Max(Colony.TotalJobs,Colony.Population))
        for(const auto& B:Colony.Buildings)if(B.Health>0&&Colony.Definition(B)->Role==TEXT("service")&&(!B.Enabled||B.IsConstructing))return nullptr;
    for(const auto& Target:Targets)
    {
        if(!IncludesTarget(Colony,Target.Definition))continue;
        int32 Living=0,Operational=0;
        for(const auto& B:Colony.Buildings)if(B.Health>0&&B.DefId==Target.Definition)
        {if(!B.Enabled)return nullptr;++Living;if(!B.IsConstructing&&B.Workers>=Colony.Definition(B)->Jobs)++Operational;}
        if(Living<Target.Count)return &Target;
        // Match the ordered construction controller: an already queued or
        // understaffed target must finish before funding downstream expansion.
        if(Operational<Target.Count)return nullptr;
    }
    return nullptr;
}

const FSeigeAIBuildTarget* FSeigeScenarioAI::SupportRecoveryTarget(const FSeigeSimulation& Colony) const
{
    if(Colony.RobotSupportCapacity>=FMath::Max(Colony.TotalJobs,Colony.Population))return nullptr;
    int32 PotentialCapacity=0;
    for(const auto& B:Colony.Buildings)if(B.Health>0)
    {
        const auto* D=Colony.Definition(B);
        if(D->Role==TEXT("service")&&(!B.Enabled||B.IsConstructing))return nullptr;
        if(!B.IsConstructing&&B.Enabled)PotentialCapacity+=D->RobotSupportCapacity;
    }
    if(PotentialCapacity>=FMath::Max(Colony.TotalJobs,Colony.Population))return nullptr;
    for(const auto& Target:Targets)
    {const auto& D=Colony.BuildingDefs[Target.Definition];if(D.Role==TEXT("service")&&D.RobotSupportCapacity>D.Jobs&&CountLive(Colony,Target.Definition)<Target.Count)return &Target;}
    return nullptr;
}

bool FSeigeScenarioAI::RecoverWorkerSupport(FSeigeSimulation& Colony,bool& Waiting)
{
    Waiting=false;
    if(Colony.RobotSupportCapacity>=FMath::Max(Colony.TotalJobs,Colony.Population))return false;
    const auto* Core=Command(Colony);if(!Core)return false;
    // A destroyed service bay can prevent worker activation, while a lower
    // priority replacement factory waits for those same workers. Recover the
    // paid support prerequisite before applying the ordinary target ordering.
    for(const auto& B:Colony.Buildings)if(B.Health>0&&Colony.Definition(B)->Role==TEXT("service"))
    {
        if(!B.Enabled){Colony.ToggleBuilding(B.Id);Status=TEXT("Restoring disabled worker support");return true;}
        if(B.IsConstructing){Waiting=true;Status=TEXT("Waiting for worker-support recovery construction");return false;}
    }
    // An intact bay may merely lack power or staffing. The normal generator,
    // road and staffing priorities must restore that capacity, rather than
    // spending the finite reserve on duplicate infrastructure during an outage.
    const auto* Required=SupportRecoveryTarget(Colony);if(!Required)return false;
    for(int32 Index=0;Index<Targets.Num();++Index)
    {
        const auto& Target=Targets[Index];const auto& Def=Colony.BuildingDefs[Target.Definition];
        if(&Target!=Required)continue;
        Waiting=true;bool Affordable=true;
        for(const auto& Cost:Def.Cost)if(Colony.ConstructionAvailable(Cost.Key)+UE_DOUBLE_SMALL_NUMBER<Cost.Value)Affordable=false;
        if(!Affordable){Status=TEXT("Waiting for materials to restore worker support");return false;}
        if(BuildNear(Colony,Target.Definition,Core->Position,Target.PlacementIndex*UE_TWO_PI/Angles))return true;
        return false;
    }
    return false;
}

bool FSeigeScenarioAI::MakeDecision(FSeigeSimulation& Colony)
{
    const FSeigeBuilding* Core = Command(Colony);
    if (!Core || Colony.Failed || Colony.Escaped) { Status = TEXT("Colony command ended"); return false; }
    if(Core->IsConstructing){Status=TEXT("Waiting for shuttle deployment");return false;}
    const bool SharedBudget=DecisionSchedulingPolicy==TEXT("shared_action_budget");
    if(SharedBudget&&GuardWorksite(Colony))return true;
    if(SharedBudget&&ManageCoreProduction(Colony))return true;
    bool WaitingForRoad=false;if(ConnectPowerRoad(Colony,WaitingForRoad))return true;
    if(SharedBudget&&ManageTrade(Colony))return true;
    bool WaitingForSupport=false;if(RecoverWorkerSupport(Colony,WaitingForSupport))return true;
    // A road may itself be waiting for workers at the support limit. Allow the
    // configured paid service prerequisite before waiting on that connection.
    // Recovery already waits for an existing bay and counts intact potential
    // capacity, so an unpowered bay cannot create an expansion loop.
    if(WaitingForRoad)return false;
    if(WaitingForSupport)return false;
    const FVector2D Origin = Core->Position;
    for (int32 Index = 0; Index < Targets.Num(); ++Index)
    {
        const FSeigeAIBuildTarget& Target = Targets[Index];
        if(!IncludesTarget(Colony,Target.Definition))continue;
        for (const FSeigeBuilding& B : Colony.Buildings)
            if (B.DefId == Target.Definition && B.Health > 0 && !B.Enabled)
            { Colony.ToggleBuilding(B.Id); Status = TEXT("Re-enabled ") + Target.Definition; return true; }
        const int32 Existing = CountLive(Colony, Target.Definition);
        if (Existing >= Target.Count)
        {
            int32 Operational=0;for(const auto& B:Colony.Buildings)if(B.DefId==Target.Definition&&B.Health>0&&!B.IsConstructing&&B.Enabled&&B.Workers>=Colony.Definition(B)->Jobs)++Operational;
            if(Operational<Target.Count){Status=TEXT("Waiting for completion and staffing: ")+Target.Definition;return false;}
            continue;
        }
        const FSeigeBuildingDef& Def = Colony.BuildingDefs[Target.Definition];
        bool Affordable = true;
        Core = Command(Colony);
        for (const auto& Pair : Def.Cost) if (Colony.ConstructionAvailable(Pair.Key) + UE_DOUBLE_SMALL_NUMBER < Pair.Value) Affordable = false;
        if (!Affordable) { Status = TEXT("Waiting for construction materials: ")+Target.Definition; return false; }
        if (!Def.ExtractionRates.IsEmpty())
        {
            TArray<const FSeigeNode*> Nodes;
            for (const FSeigeNode& N : Colony.Nodes) if (Def.ExtractionRates.Contains(N.Resource) && ExportNode(Colony) && N.Id==ExportNode(Colony)->Id && !NodeOccupied(Colony, N)) Nodes.Add(&N);
            Nodes.Sort([&](const FSeigeNode& A, const FSeigeNode& B)
            {
                const double DA = FVector2D::DistSquared(Origin, A.Position), DB = FVector2D::DistSquared(Origin, B.Position);
                return DA == DB ? A.Id < B.Id : DA < DB;
            });
            for (const FSeigeNode* Node : Nodes)
            {
                if (!Colony.IsVisible(Node->Position))
                {
                    if (ExtendSensors(Colony, Node->Position)) return true;
                    break;
                }
                FString Error;
                if (PlaceConnectedBuilding(Colony,Target.Definition,Node->Position,Error)) { Status = TEXT("Built ") + Def.Name; return true; }
                Status = Error;
            }
        }
        else
        {
            double Angle = Target.PlacementIndex * UE_TWO_PI / Angles;
            double FirstRadius=RingStart;
            if (Def.Role==TEXT("defense") || Def.SensorRange > 0)
            {
                // A compact perimeter around the actual colony. Previously an
                // unused-deposit anchor plus the search ring doubled distance,
                // leaving towers at the edge of core visibility with no useful
                // outward sight. Distribute final planned defenses around the
                // one exploited deposit; search offsets remain core-relative.
                int32 FinalCount=Target.Count;for(const auto& Planned:Targets)if(Planned.Definition==Target.Definition)FinalCount=FMath::Max(FinalCount,Planned.Count);
                const auto* Node=ExportNode(Colony);const FVector2D Direction=Node?(Node->Position-Origin).GetSafeNormal():FVector2D(1,0);
                Angle=FMath::Atan2(Direction.Y,Direction.X)+Existing*UE_TWO_PI/FinalCount;
                FirstRadius=DefenseDistance;
            }
            if (BuildNear(Colony, Target.Definition, Origin, Angle,FirstRadius)) return true;
        }
        // Do not consume the bootstrap stock on downstream factories while a prerequisite
        // extractor, perimeter sensor or service expansion is still unavailable.
        return false;
    }
    return false;
}

void FSeigeScenarioAI::RunDecisionCycle(FSeigeSimulation& Colony)
{
    const auto* Core=Command(Colony);
    if(!Core||Core->IsConstructing||Colony.Failed||Colony.Escaped)return;
    if(DecisionSchedulingPolicy==TEXT("independent_tactics_trade_construction"))
    {
        // Tactical orders and a normal port transaction have separate channels.
        // Continuous defense changes must not starve paid reconstruction, and a
        // road placement must not leave an otherwise idle trade port unused.
        GuardWorksite(Colony);
        ManageCoreProduction(Colony);
        ManageTrade(Colony);
    }
    for(int32 Action=0;Action<MaxActions;++Action)if(!MakeDecision(Colony))break;
}

void FSeigeScenarioAI::Tick(FSeigeSimulation& Colony, double Seconds)
{
    if (!Ready || !FMath::IsFinite(Seconds) || Seconds <= 0) return;
    const double FixedStep = Colony.FixedStepSeconds();
    if (FixedStep <= 0) return;
    while (Seconds > UE_DOUBLE_SMALL_NUMBER && !Colony.Failed && !Colony.Escaped)
    {
        const double Step = FMath::Min(Seconds, FixedStep); Seconds -= Step;
        const int64 Before = FMath::FloorToInt64((Colony.Time + UE_DOUBLE_SMALL_NUMBER) / DecisionInterval);
        Colony.Tick(Step);
        const int64 After = FMath::FloorToInt64((Colony.Time + UE_DOUBLE_SMALL_NUMBER) / DecisionInterval);
        if (After > Before) RunDecisionCycle(Colony);
    }
}
