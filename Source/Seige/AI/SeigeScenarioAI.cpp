#include "SeigeScenarioAI.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

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
    return Colony.Buildings.FindByPredicate([&](const FSeigeBuilding& B) { return B.DefId == Colony.CoreDefinition && B.Health > 0; });
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
        Inventory.Add(Resource, Amount); Total += Amount;
    }
    if (Total > Core->StorageCapacity) { Error = TEXT("Developed AI inventory exceeds command-core storage"); return false; }
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
        !Number(*Placement, TEXT("ring_start"), RingStart, UE_DOUBLE_SMALL_NUMBER, Colony.WorldHalfSize * 2, Error) ||
        !Number(*Placement, TEXT("ring_step"), RingStep, UE_DOUBLE_SMALL_NUMBER, Colony.WorldHalfSize * 2, Error) ||
        !Number(*Placement, TEXT("ring_limit"), RingLimit, RingStart, Colony.WorldHalfSize * 2, Error) ||
        !Integer(*Placement, TEXT("angles"), Angles, 4, 128, Error) ||
        !Number(*Placement, TEXT("node_clearance"), NodeClearance, 0, Colony.WorldHalfSize, Error) ||
        !Number(*Placement, TEXT("sensor_overlap"), SensorOverlap, UE_DOUBLE_SMALL_NUMBER, 1, Error) ||
        !Number(*Placement, TEXT("defense_distance"), DefenseDistance, 0, Colony.WorldHalfSize, Error)) return false;
    if (DecisionInterval < Colony.FixedStepSeconds() || ((RingLimit - RingStart) / RingStep + 1) * Angles > 4096)
    { Error = TEXT("AI cadence is below the simulation step or placement search is too large"); return false; }
    if (!Config->TryGetStringField(TEXT("sensor_definition"), SensorDefinition) || !Colony.BuildMenu.Contains(SensorDefinition) || Colony.BuildingDefs[SensorDefinition].SensorRange <= 0)
    { Error = TEXT("AI sensor_definition must name a buildable sensor"); return false; }
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

bool FSeigeScenarioAI::Initialize(FSeigeSimulation& Colony, const FString& RulesDirectory, const FString& AIDirectory, bool bDeveloped, FString& Error)
{
    *this = FSeigeScenarioAI();
    FSeigeSimulation Candidate;
    if (!Candidate.Initialize(RulesDirectory, Error) || !LoadConfig(Candidate, AIDirectory, Error)) return false;
    if (bDeveloped)
    {
        FObject Preset; FString Raw; int32 Population = 0; TMap<FString, double> Inventory;
        if (!ReadAIJson(FPaths::Combine(AIDirectory, TEXT("developed_start.json")), Preset, Raw, Error) || !DevelopedSeed(Preset, Candidate, Population, Inventory, Error)) return false;
        // An explicit scenario seed, applied exactly once before simulation starts.
        Candidate.Population = Population;
        Candidate.Buildings[0].Inventory = MoveTemp(Inventory);
        for (int32 Action = 0; Action < DevelopedSetupLimit; ++Action) if (!MakeDecision(Candidate)) break;
        for (const FSeigeAIBuildTarget& Target : Targets)
            if (CountLive(Candidate, Target.Definition) < Target.Count)
            { Error = TEXT("Developed AI preset cannot establish target: ") + Target.Definition + TEXT(". ") + Status; return false; }
        Candidate.AddEvent(TEXT("Developed AI colony loaded from a finite external scenario preset."));
    }
    Colony = MoveTemp(Candidate); Ready = true;
    Status = bDeveloped ? TEXT("Developed colony ready") : TEXT("Starting colony ready");
    Error.Empty(); return true;
}

int32 FSeigeScenarioAI::CountLive(const FSeigeSimulation& Colony, const FString& Definition) const
{
    int32 Count = 0;
    for (const FSeigeBuilding& B : Colony.Buildings) if (B.Health > 0 && B.DefId == Definition) ++Count;
    return Count;
}

bool FSeigeScenarioAI::BuildNear(FSeigeSimulation& Colony, const FString& Definition, FVector2D Anchor, double StartingAngle)
{
    const FSeigeBuildingDef& Def = Colony.BuildingDefs[Definition];
    auto Attempt = [&](FVector2D Position)
    {
        for (const FSeigeNode& Node : Colony.Nodes)
            if (FVector2D::Distance(Position, Node.Position) < NodeClearance + Def.Footprint) return false;
        FString Error;
        if (!Colony.PlaceBuilding(Definition, Position, Error)) { Status = Error; return false; }
        Status = TEXT("Built ") + Def.Name; return true;
    };
    if (Attempt(Anchor)) return true;
    for (double Radius = RingStart; Radius <= RingLimit; Radius += RingStep)
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
        if (B.DefId == SensorDefinition && B.Health > 0 && B.Enabled && B.Workers < Colony.Definition(B)->Jobs)
        { Status = TEXT("Waiting for existing sensor staffing"); return false; }
    const FSeigeBuilding* Source = nullptr; double BestRemaining = TNumericLimits<double>::Max();
    for (const FSeigeBuilding& B : Colony.Buildings)
    {
        const FSeigeBuildingDef* Def = Colony.Definition(B);
        if (B.Health <= 0 || !B.Enabled || !Def || Def->SensorRange <= 0 || B.Workers < Def->Jobs) continue;
        const double Remaining = FVector2D::Distance(B.Position, Destination) - Def->SensorRange;
        if (Remaining < BestRemaining) { Source = &B; BestRemaining = Remaining; }
    }
    if (!Source) { Status = TEXT("Waiting for an operating sensor"); return false; }
    const FVector2D Direction = (Destination - Source->Position).GetSafeNormal();
    const FVector2D Position = Source->Position + Direction * Colony.Definition(*Source)->SensorRange * SensorOverlap;
    return BuildNear(Colony, SensorDefinition, Position, FMath::Atan2(Direction.Y, Direction.X));
}

bool FSeigeScenarioAI::MakeDecision(FSeigeSimulation& Colony)
{
    const FSeigeBuilding* Core = Command(Colony);
    if (!Core || Colony.Failed || Colony.Escaped) { Status = TEXT("Colony command ended"); return false; }
    const FVector2D Origin = Core->Position;
    for (int32 Index = 0; Index < Targets.Num(); ++Index)
    {
        const FSeigeAIBuildTarget& Target = Targets[Index];
        for (const FSeigeBuilding& B : Colony.Buildings)
            if (B.DefId == Target.Definition && B.Health > 0 && !B.Enabled)
            { Colony.ToggleBuilding(B.Id); Status = TEXT("Re-enabled ") + Target.Definition; return true; }
        const int32 Existing = CountLive(Colony, Target.Definition);
        if (Existing >= Target.Count) continue;
        const FSeigeBuildingDef& Def = Colony.BuildingDefs[Target.Definition];
        bool Affordable = true;
        Core = Command(Colony);
        for (const auto& Pair : Def.Cost) if (Core->Inventory.FindRef(Pair.Key) + UE_DOUBLE_SMALL_NUMBER < Pair.Value) Affordable = false;
        if (!Affordable) { Status = TEXT("Waiting for construction materials"); continue; }
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
            FVector2D Anchor = Origin;
            double Angle = Index * UE_TWO_PI / Angles;
            if (Def.DamagePerSecond > 0)
            {
                TArray<const FSeigeNode*> Nodes;
                for (const auto& N : Colony.Nodes) Nodes.Add(&N);
                Nodes.Sort([&](const FSeigeNode& A, const FSeigeNode& B)
                {
                    const double DA = FVector2D::DistSquared(Origin, A.Position), DB = FVector2D::DistSquared(Origin, B.Position);
                    return DA == DB ? A.Id < B.Id : DA < DB;
                });
                if (!Nodes.IsEmpty())
                {
                    const FVector2D Direction = (Nodes[Existing % Nodes.Num()]->Position - Origin).GetSafeNormal();
                    Anchor += Direction * DefenseDistance; Angle = FMath::Atan2(Direction.Y, Direction.X);
                }
            }
            if (BuildNear(Colony, Target.Definition, Anchor, Angle)) return true;
        }
    }
    return false;
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
        if (After > Before) for (int32 Action = 0; Action < MaxActions; ++Action) if (!MakeDecision(Colony)) break;
    }
}
