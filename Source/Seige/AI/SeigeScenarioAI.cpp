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
bool DevelopedSeed(const FObject& Object, const FSeigeSimulation& Colony, int32& Population, TMap<FString, double>& Inventory, FString& Error)
{
    const FSeigeBuildingDef* Core = Colony.BuildingDefs.Find(Colony.CoreDefinition);
    if (!Core || !Integer(Object, TEXT("population"), Population, Colony.Population, MAX_int32, Error)) return false;
    const FObject* Items = nullptr;
    if (!Object->TryGetObjectField(TEXT("inventory"), Items) || !Items || !Items->IsValid())
    { Error = TEXT("Developed AI preset requires inventory"); return false; }
    double Total = 0;
    for (const auto& Pair : (*Items)->Values)
    {
        const FString Resource(Pair.Key);
        double Amount = 0;
        if (!Colony.Resources.Contains(Resource) || !Pair.Value->TryGetNumber(Amount) || !FMath::IsFinite(Amount) || Amount < 0)
        { Error = TEXT("Invalid developed AI inventory item: ") + Resource; return false; }
        Inventory.Add(Resource, Amount); Total += Amount*Colony.Resources[Resource].LitresPerUnit;
    }
    double Kit=0;for(const auto& Pair:Core->Cost)Kit+=Pair.Value*Colony.Resources[Pair.Key].LitresPerUnit;
    if (Total+Kit > Core->StorageCapacity) { Error = TEXT("Developed AI inventory and deployment kit exceed command-core storage"); return false; }
    return true;
}
bool NodeOccupied(const FSeigeSimulation& Colony, const FSeigeNode& Node)
{
    for (const FSeigeBuilding& B : Colony.Buildings)
    {
        const FSeigeBuildingDef* Def = Colony.Definition(B);
        if (B.Health <= 0 || !Def || Def->ExtractResource != Node.Resource) continue;
        const FSeigeNode* Closest = nullptr;
        double Distance = TNumericLimits<double>::Max();
        for (const FSeigeNode& Candidate : Colony.Nodes)
        {
            const double D = FVector2D::DistSquared(B.Position, Candidate.Position);
            if (Candidate.Resource == Node.Resource && D < Distance) { Closest = &Candidate; Distance = D; }
        }
        if (Closest && Closest->Id == Node.Id) return true;
    }
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
    const TArray<TSharedPtr<FJsonValue>>* Plan = nullptr;
    if (!Config->TryGetArrayField(TEXT("build_targets"), Plan) || !Plan || Plan->IsEmpty() || Plan->Num() > 128)
    { Error = TEXT("AI needs a bounded, nonempty build_targets list"); return false; }
    Targets.Empty(); TMap<FString, int32> PreviousCounts;
    for (const auto& Value : *Plan)
    {
        const FObject* Entry = nullptr; FSeigeAIBuildTarget Target;
        if (!Value->TryGetObject(Entry) || !Entry || !Entry->IsValid() || !(*Entry)->TryGetStringField(TEXT("definition"), Target.Definition) || !Colony.BuildMenu.Contains(Target.Definition))
        { Error = TEXT("AI target references an unknown or unbuildable definition"); return false; }
        if (!Integer(*Entry, TEXT("count"), Target.Count, 1, 128, Error)) return false;
        if (PreviousCounts.FindRef(Target.Definition) >= Target.Count)
        { Error = TEXT("Repeated AI target counts must increase: ") + Target.Definition; return false; }
        PreviousCounts.Add(Target.Definition, Target.Count); Targets.Add(Target);
    }
    int32 SeedPopulation = 0; TMap<FString, double> SeedInventory;
    if (!DevelopedSeed(Preset, Colony, SeedPopulation, SeedInventory, Error)) return false;
    ConfigFingerprint = FMD5::HashAnsiString(*(Raw + PresetRaw));
    return true;
}

bool FSeigeScenarioAI::Initialize(FSeigeSimulation& Colony,const FString& RulesDirectory,const FString& AIDirectory,bool bDeveloped,FString& Error,bool bBackgroundBugs,bool bPeriodicAttacks,int32 SeedOverride)
{
    FSeigeScenarioAI Builder;FSeigeSimulation Candidate;
    if(!Builder.BeginInitialize(Candidate,RulesDirectory,AIDirectory,bDeveloped,Error,bBackgroundBugs,bPeriodicAttacks,SeedOverride)){*this=MoveTemp(Builder);return false;}
    bool Complete=!Builder.IsPreparing();
    while(!Complete)if(!Builder.AdvancePreparation(Candidate,50,Complete,Error)){*this=MoveTemp(Builder);return false;}
    Colony=MoveTemp(Candidate);*this=MoveTemp(Builder);Error.Empty();return true;
}

bool FSeigeScenarioAI::BeginInitialize(FSeigeSimulation& Colony,const FString& RulesDirectory,const FString& AIDirectory,bool bDeveloped,FString& Error,bool bBackgroundBugs,bool bPeriodicAttacks,int32 SeedOverride)
{
    *this=FSeigeScenarioAI();FSeigeSimulation Candidate;
    if(!Candidate.Initialize(RulesDirectory,Error,bBackgroundBugs,bPeriodicAttacks,SeedOverride)||!LoadConfig(Candidate,AIDirectory,Error))return false;
    const FSeigeNode* Local=ExportNode(Candidate);bool Landed=false;
    if(Local)for(int32 I=0;I<Angles;++I)
    {const double Angle=I*UE_TWO_PI/Angles;const FVector2D At=Local->Position+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*RingStart;if(Candidate.SetInitialCorePosition(At,Error)){Landed=true;break;}}
    if(!Landed){Error=TEXT("AI could not find a legal landing beside a generated standard deposit");return false;}
    if(bDeveloped)
    {
        FObject Preset;FString Raw;int32 Population=0;TMap<FString,double> Inventory;
        if(!ReadAIJson(FPaths::Combine(AIDirectory,TEXT("developed_start.json")),Preset,Raw,Error)||!DevelopedSeed(Preset,Candidate,Population,Inventory,Error))return false;
        Candidate.Population=Population;Candidate.Buildings[0].Inventory=MoveTemp(Inventory);
    }
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
    UE_LOG(LogTemp,Log,TEXT("Seige AI preparation: seed=%d time=%.2f ready=%d/%d best=%d destroyed=%d unfinished=%d status=%s missing=[%s]"),Colony.GenerationSeed,Colony.Time,Completed,Required,PreparationBestCompleted,Destroyed,Unfinished,*Status,*FString::Join(Missing,TEXT(", ")));
}

int32 FSeigeScenarioAI::CountLive(const FSeigeSimulation& Colony, const FString& Definition) const
{
    int32 Count = 0;
    for (const FSeigeBuilding& B : Colony.Buildings) if (B.Health > 0 && B.DefId == Definition) ++Count;
    return Count;
}

bool FSeigeScenarioAI::BuildNear(FSeigeSimulation& Colony, const FString& Definition, FVector2D Anchor, double StartingAngle, double FirstRadius)
{
    const FSeigeBuildingDef& Def = Colony.BuildingDefs[Definition];
    auto Attempt = [&](FVector2D Position)
    {
        for (const FSeigeNode& Node : Colony.Nodes)
            if (FVector2D::Distance(Position, Node.Position) < NodeClearance + Def.ReservedFootprint) return false;
        FString Error;
        if (!Colony.PlaceBuilding(Definition, Position, Error)) { Status = Error; return false; }
        Status = TEXT("Built ") + Def.Name; return true;
    };
    if (Attempt(Anchor)) return true;
    for (double Radius = FirstRadius>=0?FirstRadius:RingStart; Radius <= RingLimit; Radius += RingStep)
        for (int32 I = 0; I < Angles; ++I)
        {
            const double Angle = StartingAngle + I * UE_TWO_PI / Angles;
            if (Attempt(Anchor + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius)) return true;
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
    if(D->ExtractResource.IsEmpty())return true;
    const auto* Node=ExportNode(Colony);return Node&&D->ExtractResource==Node->Resource;
}

bool FSeigeScenarioAI::ConnectPowerRoad(FSeigeSimulation& Colony,bool& Waiting)
{
    Waiting=false;const auto* Core=Command(Colony);if(!Core)return false;
    for(const auto& R:Colony.Roads)if(R.Health>0&&R.IsConstructing){Waiting=true;Status=TEXT("Constructing road-grid connection");return false;}
    auto OnSegment=[](FVector2D P,FVector2D A,FVector2D B)
    {const FVector2D V=B-A;const double T=FVector2D::DotProduct(P-A,V)/FMath::Max(V.SizeSquared(),UE_DOUBLE_SMALL_NUMBER);return T>=-.00001&&T<=1.00001&&FVector2D::Distance(P,A+V*T)<.01;};
    for(const auto& Building:Colony.Buildings)
    {
        if(Building.Id==Core->Id||Building.Health<=0||Building.IsConstructing||Colony.IsRoadGridConnected(Core->Id,Building.Id))continue;
        TArray<FVector2D> Starts{Colony.BuildingAccessPoint(*Core)};
        for(const auto& R:Colony.Roads)if(R.Health>0&&!R.Tier.IsEmpty()){Starts.AddUnique(R.A);Starts.AddUnique(R.B);}
        const FVector2D End=Colony.BuildingAccessPoint(Building);
        Starts.Sort([&](const FVector2D&A,const FVector2D&B){return FVector2D::DistSquared(A,End)<FVector2D::DistSquared(B,End);});
        FString Error;Waiting=true;
        for(const auto& Start:Starts)
        {
            TArray<FVector2D> Route;if(!Colony.FindRoadRoute(Start,End,Route))continue;
            FVector2D Previous=Start;
            for(const auto& Point:Route)
            {
                const bool Exists=Colony.Roads.ContainsByPredicate([&](const auto& R){return R.Health>0&&!R.Tier.IsEmpty()&&OnSegment(Previous,R.A,R.B)&&OnSegment(Point,R.A,R.B);});
                if(!Exists)
                {
                    bool Placed=Colony.PlaceRoad(Previous,Point,Error);
                    // A short terminal spur may be below the transport minimum. Extend
                    // it beyond the access point: the port lies ON a real paid road,
                    // without changing minimum length or inventing wireless connectivity.
                    if(!Placed&&Point.Equals(End,.01)&&!Previous.Equals(Point,.01))
                        Placed=Colony.PlaceRoad(Previous,Point+(Point-Previous).GetSafeNormal()*RingStep,Error);
                    if(Placed){Status=TEXT("Connecting ")+Colony.Definition(Building)->Name+TEXT(" to the road power grid");return true;}break;
                }
                Previous=Point;
            }
        }
        Status=TEXT("Waiting for a valid road-grid connection: ")+Colony.Definition(Building)->Name+TEXT(". ")+Error;return false;
    }
    return false;
}

bool FSeigeScenarioAI::ManageTrade(FSeigeSimulation& Colony)
{
    const FSeigeBuilding* Port=Colony.Buildings.FindByPredicate([&](const auto&B){return B.Health>0&&!B.IsConstructing&&B.Enabled&&Colony.Definition(B)->Role==TEXT("trade");});
    const auto* Node=ExportNode(Colony);if(!Port||!Node||!Port->Shipment.Resource.IsEmpty())return false;
    TMap<FString,double> Desired=ReserveTargets;
    for(const auto& B:Colony.Buildings)if(B.Health>0&&B.Enabled)
    {const auto* D=Colony.Definition(B);if(!D->Recipe.IsEmpty())for(const auto& P:Colony.Recipes[D->Recipe].Inputs)Desired.FindOrAdd(P.Key)+=P.Value*RecipeInputBuffer;}
    const auto* Construction=NextConstructionTarget(Colony);
    if(Construction)for(const auto& P:Colony.BuildingDefs[Construction->Definition].Cost)Desired.FindOrAdd(P.Key)+=P.Value;
    TArray<FString> Ids;Desired.GetKeys(Ids);Ids.Sort();FString Missing;double Worst=0,CreditBuffer=0;
    for(const auto& Id:Ids)
    {
        CreditBuffer=FMath::Max(CreditBuffer,Colony.TradeQuote(Id,ImportBatch,true)*CreditBufferBatches);
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
    if(Construction)
    {
        const auto& Cost=Colony.BuildingDefs[Construction->Definition].Cost;TArray<FString> Materials;Cost.GetKeys(Materials);Materials.Sort();
        double CriticalRatio=0;FString Critical;double Deficit=0;
        for(const auto& Id:Materials)
        {
            const double Need=FMath::Max(0.,Cost[Id]-Colony.ConstructionAvailable(Id)),Ratio=Need/FMath::Max(Cost[Id],1.);
            if(Ratio>CriticalRatio){CriticalRatio=Ratio;Critical=Id;Deficit=Need;}
        }
        if(!Critical.IsEmpty()){Missing=Critical;Desired.FindOrAdd(Critical)=Colony.TotalStock(Critical)+Deficit;}
    }
    FString Error;
    if(!Missing.IsEmpty())
    {
        const double Amount=FMath::Min(ImportBatch,Desired[Missing]-Colony.TotalStock(Missing));
        if(Amount>UE_DOUBLE_SMALL_NUMBER&&Colony.TryTrade(Port->Id,Missing,Amount,true,Error)){Status=TEXT("Importing ")+Colony.Resources[Missing].Name+TEXT(" using export earnings");return true;}
    }
    if(!Missing.IsEmpty()||Colony.Credits<CreditBuffer)
    {
        const double Surplus=FMath::Max(0.,Colony.TotalStock(Node->Resource)-Desired.FindRef(Node->Resource));
        const double Amount=FMath::Min(ExportBatch,Surplus);
        if(Amount>UE_DOUBLE_SMALL_NUMBER&&Colony.TryTrade(Port->Id,Node->Resource,Amount,false,Error)){Status=TEXT("Exporting local ")+Colony.Resources[Node->Resource].Name;return true;}
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
        // A passive move to the core is a persisted ordinary service order. Its
        // mission and destination retain hysteresis across save/load, without
        // a hidden AI-only state machine or free battery replenishment.
        const bool Servicing=Fleet.Mission==TEXT("move")&&Fleet.Aggression==TEXT("passive")&&Fleet.Destination.Equals(ServiceStation,.01);
        if(Fleet.DestinationSector!=4||(Fleet.Mission!=TEXT("defense")&&Fleet.Mission!=TEXT("escort")&&!Servicing))continue;
        bool Living=false,Embarked=false;double LowestBattery=1;
        for(const auto& V:Colony.Combat.Vehicles)if(V.FleetId==Fleet.Id&&V.Health>0&&!V.Evacuated)
        {Living=true;Embarked|=V.Embarked;LowestBattery=FMath::Min(LowestBattery,V.BatteryKWh/Colony.Combat.Chassis[V.ChassisId].BatteryKWh);}
        if(!Living)continue;
        if(LowestBattery<GuardRechargeBelow||(Servicing&&LowestBattery<GuardResumeAbove))
        {
            if(!Servicing)
            {
                FString Error;
                if(Colony.Combat.OrderFleet(Colony,Fleet.Id,TEXT("move"),ServiceStation,0,Error)&&Colony.Combat.SetAggression(Fleet.Id,TEXT("passive"),Error))
                {Status=TEXT("Guard fleet returning for grid-powered charging");return true;}
            }
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
        if(BuildNear(Colony,Target.Definition,Core->Position,Index*UE_TWO_PI/Angles))return true;
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
    bool WaitingForRoad=false;if(ConnectPowerRoad(Colony,WaitingForRoad))return true;
    if(SharedBudget&&ManageTrade(Colony))return true;
    if(WaitingForRoad)return false;
    bool WaitingForSupport=false;if(RecoverWorkerSupport(Colony,WaitingForSupport))return true;
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
        if (!Def.ExtractResource.IsEmpty())
        {
            TArray<const FSeigeNode*> Nodes;
            for (const FSeigeNode& N : Colony.Nodes) if (N.Resource == Def.ExtractResource && !NodeOccupied(Colony, N)) Nodes.Add(&N);
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
                if (Colony.PlaceBuilding(Target.Definition, Node->Position, Error)) { Status = TEXT("Built ") + Def.Name; return true; }
                Status = Error;
            }
        }
        else
        {
            double Angle = Index * UE_TWO_PI / Angles;
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
