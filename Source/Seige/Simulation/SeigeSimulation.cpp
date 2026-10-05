#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"

namespace
{
using FObject = TSharedPtr<FJsonObject>;
bool ReadJson(const FString& Path, FObject& Out, FString& Raw, FString& Error)
{
    if (!FFileHelper::LoadFileToString(Raw, *Path)) { Error = TEXT("Cannot read ") + Path; return false; }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid()) { Error = TEXT("Invalid JSON in ") + Path + TEXT(": ") + Reader->GetErrorMessage(); return false; }
    return true;
}
bool StringField(const FObject& O, const FString& Key, FString& Out, FString& Error)
{
    if (!O.IsValid() || !O->TryGetStringField(Key, Out)) { Error = TEXT("Missing string field: ") + Key; return false; }
    return true;
}
bool Numeric(const FObject& O, const FString& Key, double& Out, double Minimum, FString& Error, bool Integer = false)
{
    if (!O.IsValid() || !O->TryGetNumberField(Key, Out) || !FMath::IsFinite(Out) || Out < Minimum || (Integer && (Out > MAX_int32 || FMath::FloorToDouble(Out) != Out)))
    { Error = TEXT("Invalid numeric field: ") + Key; return false; }
    return true;
}
bool IntegerField(const FObject& O, const FString& Key, int32& Out, int32 Minimum, FString& Error)
{
    double Value = 0;
    if (!Numeric(O, Key, Value, Minimum, Error, true)) return false;
    Out = static_cast<int32>(Value); return true;
}
bool ArrayField(const FObject& O, const FString& Key, const TArray<TSharedPtr<FJsonValue>>*& Out, FString& Error)
{
    if (!O.IsValid() || !O->TryGetArrayField(Key, Out)) { Error = TEXT("Missing array field: ") + Key; return false; }
    return true;
}
bool ObjectField(const FObject& O, const FString& Key, FObject& Out, FString& Error)
{
    const FObject* Value = nullptr;
    if (!O.IsValid() || !O->TryGetObjectField(Key, Value) || !Value || !Value->IsValid()) { Error = TEXT("Missing object field: ") + Key; return false; }
    Out = *Value; return true;
}
bool PositionField(const FObject& O, const FString& Key, FVector2D& Out, FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!ArrayField(O, Key, Values, Error) || Values->Num() != 2) { Error = TEXT("Position requires two coordinates: ") + Key; return false; }
    double X = 0, Y = 0;
    if (!(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y) || !FMath::IsFinite(X) || !FMath::IsFinite(Y)) { Error = TEXT("Nonfinite position: ") + Key; return false; }
    Out = FVector2D(X, Y); return true;
}
bool ColorField(const FObject& O, FLinearColor& Out, FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!ArrayField(O, TEXT("color"), Values, Error) || Values->Num() != 3) { Error = TEXT("Color requires three channels"); return false; }
    double C[3];
    for (int32 I = 0; I < 3; ++I)
        if (!(*Values)[I]->TryGetNumber(C[I]) || !FMath::IsFinite(C[I]) || C[I] < 0 || C[I] > 1) { Error = TEXT("Color channels must be between zero and one"); return false; }
    Out = FLinearColor(C[0], C[1], C[2]); return true;
}
bool Amounts(const FObject& O, const FString& Key, TMap<FString, double>& Out, const TMap<FString, FSeigeResourceDef>& Resources, FString& Error)
{
    FObject Map;
    if (!ObjectField(O, Key, Map, Error)) return false;
    Out.Empty();
    for (const auto& Pair : Map->Values)
    {
        const FString ResourceId(Pair.Key);
        double Value = 0;
        if (!Resources.Contains(ResourceId) || !Pair.Value->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0)
        { Error = TEXT("Unknown resource or invalid quantity in ") + Key + TEXT(": ") + ResourceId; return false; }
        Out.Add(ResourceId, Value);
    }
    return true;
}
bool HasAmounts(const TMap<FString, double>& Stock, const TMap<FString, double>& Need)
{
    for (const auto& Pair : Need) if (Stock.FindRef(Pair.Key) + UE_DOUBLE_SMALL_NUMBER < Pair.Value) return false;
    return true;
}
void Consume(TMap<FString, double>& Stock, const TMap<FString, double>& Need)
{
    for (const auto& Pair : Need) Stock.FindOrAdd(Pair.Key) = FMath::Max(0.0, Stock.FindRef(Pair.Key) - Pair.Value);
}
double Sum(const TMap<FString, double>& Stock)
{
    double Total = 0; for (const auto& Pair : Stock) Total += Pair.Value; return Total;
}
FObject JsonAmounts(const TMap<FString, double>& Map)
{
    FObject Out = MakeShared<FJsonObject>();
    TArray<FString> Keys; Map.GetKeys(Keys); Keys.Sort();
    for (const FString& Key : Keys) Out->SetNumberField(Key, Map.FindRef(Key));
    return Out;
}
void WritePosition(const FObject& O, FVector2D P)
{
    O->SetArrayField(TEXT("position"), {MakeShared<FJsonValueNumber>(P.X), MakeShared<FJsonValueNumber>(P.Y)});
}
bool WriteJson(const FObject& O, const FString& Filename, FString& Error)
{
    FString Text;
    if (!FJsonSerializer::Serialize(O.ToSharedRef(), TJsonWriterFactory<>::Create(&Text))) { Error = TEXT("Could not serialize state"); return false; }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    const FString Temporary = Filename + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(Text, *Temporary, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { Error = TEXT("Cannot write save file: ") + Temporary; return false; }
    if (!IFileManager::Get().Move(*Filename, *Temporary, true, true)) { Error = TEXT("Cannot replace save file: ") + Filename; return false; }
    return true;
}
}

bool FSeigeSimulation::Initialize(const FString& RulesDirectory, FString& Error)
{
    *this = FSeigeSimulation();
    RulesPath = FPaths::ConvertRelativePathToFull(RulesDirectory);
    FString Fingerprint;
    TMap<FString, FObject> Documents;
    const TArray<FString> Names = {TEXT("resources"), TEXT("recipes"), TEXT("buildings"), TEXT("policies"), TEXT("scenario")};
    for (const FString& Name : Names)
    {
        FString Raw, Version; FObject O;
        if (!ReadJson(FPaths::Combine(RulesPath, Name + TEXT(".json")), O, Raw, Error) || !StringField(O, TEXT("version"), Version, Error)) return false;
        if (Version.IsEmpty() || (!RulesVersion.IsEmpty() && RulesVersion != Version)) { Error = TEXT("Rule file versions must match and be nonempty"); return false; }
        RulesVersion = Version; Fingerprint += Raw; Documents.Add(Name, O);
    }
    RulesFingerprint = FMD5::HashAnsiString(*Fingerprint);
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!ArrayField(Documents[TEXT("resources")], TEXT("resources"), Values, Error)) return false;
    for (const auto& V : *Values)
    {
        const FObject O = V->AsObject(); FSeigeResourceDef R;
        if (!StringField(O, TEXT("id"), R.Id, Error) || !StringField(O, TEXT("name"), R.Name, Error) || !IntegerField(O, TEXT("tier"), R.Tier, 0, Error) || !ColorField(O, R.Color, Error)) return false;
        if (R.Id.IsEmpty() || Resources.Contains(R.Id)) { Error = TEXT("Duplicate or empty resource ID: ") + R.Id; return false; }
        Resources.Add(R.Id, R);
    }
    if (Resources.IsEmpty()) { Error = TEXT("Resource catalog is empty"); return false; }
    if (!ArrayField(Documents[TEXT("recipes")], TEXT("recipes"), Values, Error)) return false;
    for (const auto& V : *Values)
    {
        const FObject O = V->AsObject(); FSeigeRecipeDef R;
        if (!StringField(O, TEXT("id"), R.Id, Error) || !Numeric(O, TEXT("seconds"), R.Seconds, UE_DOUBLE_SMALL_NUMBER, Error) || !Amounts(O, TEXT("inputs"), R.Inputs, Resources, Error) || !Amounts(O, TEXT("outputs"), R.Outputs, Resources, Error)) return false;
        if (R.Id.IsEmpty() || Recipes.Contains(R.Id) || Sum(R.Inputs) <= 0) { Error = TEXT("Recipe needs a unique ID and positive inputs: ") + R.Id; return false; }
        Recipes.Add(R.Id, R);
    }
    if (!ArrayField(Documents[TEXT("buildings")], TEXT("buildings"), Values, Error)) return false;
    int32 CoreDefinitions = 0;
    for (const auto& V : *Values)
    {
        const FObject O = V->AsObject(); FSeigeBuildingDef B;
        if (!StringField(O, TEXT("id"), B.Id, Error) || !StringField(O, TEXT("name"), B.Name, Error) || !StringField(O, TEXT("category"), B.Category, Error) || !StringField(O, TEXT("role"), B.Role, Error) || !StringField(O, TEXT("description"), B.Description, Error) || !StringField(O, TEXT("visual"), B.Visual, Error) || !StringField(O, TEXT("recipe"), B.Recipe, Error) || !StringField(O, TEXT("extract_resource"), B.ExtractResource, Error) || !ColorField(O, B.Color, Error) || !Amounts(O, TEXT("cost"), B.Cost, Resources, Error)) return false;
        if (!IntegerField(O, TEXT("jobs"), B.Jobs, 0, Error) || !Numeric(O, TEXT("health"), B.Health, UE_DOUBLE_SMALL_NUMBER, Error) || !Numeric(O, TEXT("footprint"), B.Footprint, UE_DOUBLE_SMALL_NUMBER, Error) || !Numeric(O, TEXT("storage_capacity"), B.StorageCapacity, UE_DOUBLE_SMALL_NUMBER, Error) || !Numeric(O, TEXT("sensor_range"), B.SensorRange, 0, Error) || !Numeric(O, TEXT("attack_range"), B.AttackRange, 0, Error) || !Numeric(O, TEXT("extract_rate"), B.ExtractRate, 0, Error) || !StringField(O,TEXT("weapon_name"),B.WeaponName,Error) || !Numeric(O,TEXT("damage_per_shot"),B.DamagePerShot,0,Error) || !Numeric(O,TEXT("reload_seconds"),B.ReloadSeconds,0,Error) || !Numeric(O,TEXT("power_usage_kw"),B.PowerUsageKW,0,Error) || !Numeric(O,TEXT("power_generation_kw"),B.PowerGenerationKW,0,Error)) return false;
        const bool Armed=B.DamagePerShot>0;
        if (O->HasField(TEXT("damage_per_second")) || (Armed && (B.WeaponName.IsEmpty() || B.ReloadSeconds<=0 || B.AttackRange<=0)) || (!Armed && (!B.WeaponName.IsEmpty() || B.ReloadSeconds!=0 || B.AttackRange!=0)))
        { Error=TEXT("Weapon requires consistent name, shot damage, reload and range; DPS is derived: ")+B.Id;return false; }
        if (B.PowerUsageKW!=0 || B.PowerGenerationKW!=0) {Error=TEXT("Power grid is not implemented; power values must remain zero: ")+B.Id;return false;}
        B.DamagePerSecond=Armed?B.DamagePerShot/B.ReloadSeconds:0;
        if (!FMath::IsFinite(B.DamagePerSecond)) {Error=TEXT("Weapon DPS is nonfinite: ")+B.Id;return false;}
        const TArray<FString> Roles = {TEXT("core"), TEXT("extractor"), TEXT("processor"), TEXT("storage"), TEXT("sensor"), TEXT("defense")};
        if (B.Id.IsEmpty() || BuildingDefs.Contains(B.Id) || !Roles.Contains(B.Role) || (!B.Recipe.IsEmpty() && !Recipes.Contains(B.Recipe)) || (!B.ExtractResource.IsEmpty() && !Resources.Contains(B.ExtractResource))) { Error = TEXT("Invalid building definition: ") + B.Id; return false; }
        if (B.Role == TEXT("core")) ++CoreDefinitions;
        if ((B.Role == TEXT("extractor")) != (!B.ExtractResource.IsEmpty() && B.ExtractRate > 0) || (B.Role == TEXT("processor")) != !B.Recipe.IsEmpty()) { Error = TEXT("Building role/capability mismatch: ") + B.Id; return false; }
        if (!B.Recipe.IsEmpty() && (Sum(Recipes[B.Recipe].Outputs) <= 0 || Sum(Recipes[B.Recipe].Inputs) > B.StorageCapacity || Sum(Recipes[B.Recipe].Outputs) > B.StorageCapacity)) { Error = TEXT("Recipe does not fit building storage: ") + B.Id; return false; }
        BuildingDefs.Add(B.Id, B);
    }
    if (CoreDefinitions != 1) { Error = TEXT("Exactly one command-core definition is required"); return false; }
    if (!ArrayField(Documents[TEXT("buildings")], TEXT("build_menu"), Values, Error)) return false;
    for (const auto& V : *Values)
    {
        FString Id;
        if (!V->TryGetString(Id) || !BuildingDefs.Contains(Id) || BuildingDefs[Id].Role == TEXT("core") || BuildMenu.Contains(Id)) { Error = TEXT("Invalid or duplicated build-menu reference"); return false; }
        BuildMenu.Add(Id);
    }
    if (!ObjectField(Documents[TEXT("policies")], TEXT("policies"), Policy, Error) || !ObjectField(Documents[TEXT("scenario")], TEXT("scenario"), Scenario, Error)) return false;
    const TArray<FString> Positive = {TEXT("fixed_step_seconds"),TEXT("dispatch_interval"),TEXT("courier_speed"),TEXT("courier_capacity"),TEXT("courier_min_batch"),TEXT("delivery_buffer_cycles"),TEXT("repair_health_per_unit"),TEXT("robot_retire_seconds"),TEXT("upkeep_interval"),TEXT("wave_interval"),TEXT("spawn_radius"),TEXT("roam_interval"),TEXT("enemy_health"),TEXT("enemy_speed"),TEXT("extractor_snap_distance")};
    const TArray<FString> Nonnegative = {TEXT("repair_buffer_units"),TEXT("repair_health_per_second"),TEXT("minimum_build_spacing"),TEXT("population_buffer_robots"),TEXT("upkeep_per_robot"),TEXT("upkeep_buffer_intervals"),TEXT("upkeep_shortage_efficiency"),TEXT("objective_produced_amount"),TEXT("objective_survival_seconds"),TEXT("wave_first_time"),TEXT("wave_per_building"),TEXT("wave_per_population"),TEXT("wave_escalation_per_wave"),TEXT("roam_first_time"),TEXT("enemy_attack_range"),TEXT("enemy_damage_per_second"),TEXT("enemy_courier_attack_range"),TEXT("shuttle_capacity")};
    const TArray<FString> Integers = {TEXT("max_couriers"),TEXT("minimum_population"),TEXT("objective_building_count"),TEXT("wave_base_count"),TEXT("wave_max_count"),TEXT("roam_count"),TEXT("event_history_limit"),TEXT("placement_requires_visibility")};
    double Scratch = 0;
    for (const FString& Key : Positive) if (!Numeric(Policy, Key, Scratch, UE_DOUBLE_SMALL_NUMBER, Error)) return false;
    for (const FString& Key : Nonnegative) if (!Numeric(Policy, Key, Scratch, 0, Error)) return false;
    for (const FString& Key : Integers) if (!Numeric(Policy, Key, Scratch, 0, Error, true)) return false;
    if (Number(TEXT("max_couriers")) < 1 || Number(TEXT("event_history_limit")) < 1 || Number(TEXT("wave_max_count")) < Number(TEXT("wave_base_count")) || Number(TEXT("placement_requires_visibility")) > 1 || Number(TEXT("upkeep_shortage_efficiency")) > 1 || Number(TEXT("courier_min_batch")) > Number(TEXT("courier_capacity")) || Number(TEXT("fixed_step_seconds")) > Number(TEXT("dispatch_interval"))) { Error = TEXT("Policy ranges are inconsistent"); return false; }
    for (const auto& Pair:BuildingDefs) if (Pair.Value.DamagePerShot>0 && Pair.Value.ReloadSeconds<Number(TEXT("fixed_step_seconds")))
    {Error=TEXT("Weapon reload cannot be shorter than fixed_step_seconds: ")+Pair.Key;return false;}
    const TMap<FString,FString> Selectors = {
        {TEXT("population_policy"),TEXT("fill_open_jobs")},{TEXT("surplus_policy"),TEXT("retire_without_refund")},
        {TEXT("logistics_policy"),TEXT("local_delivery")},{TEXT("enemy_target_policy"),TEXT("nearest_building")},
        {TEXT("extraction_limit_policy"),TEXT("one_extractor_per_node")},{TEXT("repair_policy"),TEXT("local_materials")},
        {TEXT("objective_policy"),TEXT("survive_and_manufacture")},{TEXT("shuttle_policy"),TEXT("preloaded_cargo_only")},
        {TEXT("storage_policy"),TEXT("overflow_only")},{TEXT("rule_time_basis"),TEXT("simulation_seconds")}};
    for (const auto& Pair : Selectors)
    { FString Value; if (!StringField(Policy, Pair.Key, Value, Error)) return false; if (Value != Pair.Value) { Error = TEXT("Unsupported policy ") + Pair.Key + TEXT(": ") + Value; return false; } }
    FString Workforce;
    if (!StringField(Policy, TEXT("workforce_mode"), Workforce, Error) || (Workforce != TEXT("full_staff") && Workforce != TEXT("proportional"))) { Error = TEXT("Unsupported workforce_mode"); return false; }
    for (const FString& Key : {FString(TEXT("repair_resource")),FString(TEXT("upkeep_resource")),FString(TEXT("objective_resource"))})
    { FString Id; if (!StringField(Policy, Key, Id, Error) || !Resources.Contains(Id)) { Error = TEXT("Unknown resource policy: ") + Key; return false; } }
    FString PopulationRecipe, ObjectiveBuilding;
    if (!StringField(Policy, TEXT("population_recipe"), PopulationRecipe, Error) || !Recipes.Contains(PopulationRecipe) || !Recipes[PopulationRecipe].Outputs.IsEmpty()) { Error = TEXT("Population recipe must exist and have no physical outputs"); return false; }
    if (!StringField(Policy, TEXT("objective_building"), ObjectiveBuilding, Error) || !BuildingDefs.Contains(ObjectiveBuilding)) { Error = TEXT("Unknown objective building"); return false; }
    if (!Amounts(Policy, TEXT("core_reserves"), CoreReserves, Resources, Error) || !StringField(Scenario, TEXT("title"), Title, Error) || !StringField(Scenario, TEXT("core_definition"), CoreDefinition, Error) || !Numeric(Scenario, TEXT("world_half_size"), WorldHalfSize, UE_DOUBLE_SMALL_NUMBER, Error) || !IntegerField(Scenario, TEXT("starting_population"), Population, 0, Error)) return false;
    if (!BuildingDefs.Contains(CoreDefinition) || BuildingDefs[CoreDefinition].Role != TEXT("core") || Number(TEXT("spawn_radius")) > WorldHalfSize || Population < Number(TEXT("minimum_population"))) { Error = TEXT("Scenario core, population or spawn bounds are invalid"); return false; }
    int32 Seed = 0; if (!IntegerField(Scenario, TEXT("random_seed"), Seed, 0, Error)) return false; Random.Initialize(Seed);
    FSeigeBuilding B; B.Id = NextId++; B.DefId = CoreDefinition; B.Health = BuildingDefs[CoreDefinition].Health;
    if (!PositionField(Scenario, TEXT("core_position"), B.Position, Error) || !Amounts(Scenario, TEXT("starting_inventory"), B.Inventory, Resources, Error)) return false;
    if (FMath::Abs(B.Position.X) + BuildingDefs[CoreDefinition].Footprint > WorldHalfSize || FMath::Abs(B.Position.Y) + BuildingDefs[CoreDefinition].Footprint > WorldHalfSize || Sum(B.Inventory) > BuildingDefs[CoreDefinition].StorageCapacity) { Error = TEXT("Starting core stock or position exceeds capacity"); return false; }
    Buildings.Add(B);
    if (!Amounts(Scenario,TEXT("starting_shuttle_cargo"),ShuttleCargo,Resources,Error) || Sum(ShuttleCargo) > Number(TEXT("shuttle_capacity"))) { Error = TEXT("Invalid preloaded shuttle inventory"); return false; }
    if (!ArrayField(Scenario, TEXT("deposits"), Values, Error)) return false;
    for (const auto& V : *Values)
    {
        FSeigeNode N; N.Id = NextId++;
        if (!StringField(V->AsObject(), TEXT("resource"), N.Resource, Error) || !Resources.Contains(N.Resource) || !PositionField(V->AsObject(), TEXT("position"), N.Position, Error)) { if (Error.IsEmpty()) Error = TEXT("Unknown deposit resource"); return false; }
        if (FMath::Abs(N.Position.X) > WorldHalfSize || FMath::Abs(N.Position.Y) > WorldHalfSize) { Error = TEXT("Deposit outside world"); return false; }
        Nodes.Add(N);
    }
    TSet<FString> Renewable;
    for (const FSeigeNode& N : Nodes) for (const FString& Id : BuildMenu) if (BuildingDefs[Id].ExtractResource == N.Resource) Renewable.Add(N.Resource);
    int32 Previous = -1;
    while (Previous != Renewable.Num())
    {
        Previous = Renewable.Num();
        for (const FString& Id : BuildMenu)
        {
            const FSeigeBuildingDef& D = BuildingDefs[Id]; if (D.Recipe.IsEmpty()) continue;
            const FSeigeRecipeDef& R = Recipes[D.Recipe]; bool Reachable = true;
            for (const auto& Pair : R.Inputs) if (Pair.Value > 0 && !Renewable.Contains(Pair.Key)) Reachable = false;
            if (Reachable) for (const auto& Pair : R.Outputs) if (Pair.Value > 0) Renewable.Add(Pair.Key);
        }
    }
    TArray<FString> Required; Recipes[PopulationRecipe].Inputs.GetKeys(Required);
    Required.Append({TextRule(TEXT("repair_resource")),TextRule(TEXT("upkeep_resource")),TextRule(TEXT("objective_resource"))});
    for (const FString& Id : Required) if (!Renewable.Contains(Id)) { Error = TEXT("No renewable resource path for recurring requirement: ") + Id; return false; }
    for (const auto& Pair : BuildingDefs)
    {
        const FSeigeBuildingDef& D = Pair.Value;
        double Buffer = Number(TEXT("repair_buffer_units"));
        if (!D.Recipe.IsEmpty()) Buffer += Sum(Recipes[D.Recipe].Inputs) * Number(TEXT("delivery_buffer_cycles"));
        if (D.Role == TEXT("core")) Buffer += Sum(CoreReserves) + Sum(Recipes[PopulationRecipe].Inputs) * Number(TEXT("population_buffer_robots")) + Population * Number(TEXT("upkeep_per_robot")) * Number(TEXT("upkeep_buffer_intervals"));
        if (Buffer > D.StorageCapacity) { Error = TEXT("Demand buffers exceed building capacity: ") + D.Id; return false; }
    }
    NextWaveTime = Number(TEXT("wave_first_time")); NextRoamTime = Number(TEXT("roam_first_time"));
    AllocateWorkers(); AddEvent(TEXT("First landing. Establish extraction, industry and local defenses.")); Error.Empty(); return true;
}

double FSeigeSimulation::Number(const FString& Key) const { return Policy->GetNumberField(Key); }
bool FSeigeSimulation::CanSetInitialCorePosition(FVector2D Position, FString& Error) const
{
    const FSeigeBuilding* Command = Core();
    if (!Policy || !Command || Time != 0 || Buildings.Num() != 1)
    { Error = TEXT("Core placement is only available before a new colony begins"); return false; }
    const double Radius = BuildingDefs[CoreDefinition].Footprint;
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || FMath::Abs(Position.X) + Radius > WorldHalfSize || FMath::Abs(Position.Y) + Radius > WorldHalfSize)
    { Error = TEXT("Command core footprint must fit inside the sector"); return false; }
    for (const FSeigeNode& Node : Nodes)
    {
        double ExtractorRadius = 0;
        for (const FString& Id : BuildMenu)
            if (BuildingDefs[Id].ExtractResource == Node.Resource) ExtractorRadius = FMath::Max(ExtractorRadius, BuildingDefs[Id].Footprint);
        if (FVector2D::Distance(Position, Node.Position) < Radius + ExtractorRadius + Number(TEXT("minimum_build_spacing")))
        { Error = TEXT("The command core would obstruct a resource deposit"); return false; }
    }
    Error.Empty(); return true;
}
bool FSeigeSimulation::SetInitialCorePosition(FVector2D Position, FString& Error)
{
    if (!CanSetInitialCorePosition(Position, Error)) return false;
    Core()->Position = Position; return true;
}
FString FSeigeSimulation::TextRule(const FString& Key) const { return Policy->GetStringField(Key); }
const FSeigeBuildingDef* FSeigeSimulation::Definition(const FSeigeBuilding& B) const { return BuildingDefs.Find(B.DefId); }
FSeigeBuilding* FSeigeSimulation::FindBuilding(int32 Id) { return Buildings.FindByPredicate([Id](const FSeigeBuilding& B){return B.Id == Id;}); }
const FSeigeBuilding* FSeigeSimulation::FindBuilding(int32 Id) const { return Buildings.FindByPredicate([Id](const FSeigeBuilding& B){return B.Id == Id;}); }
FSeigeBuilding* FSeigeSimulation::Core() { return Buildings.FindByPredicate([this](const FSeigeBuilding& B){return B.DefId == CoreDefinition;}); }
const FSeigeBuilding* FSeigeSimulation::Core() const { return Buildings.FindByPredicate([this](const FSeigeBuilding& B){return B.DefId == CoreDefinition;}); }
double FSeigeSimulation::Occupied(const FSeigeBuilding& B) const { return Sum(B.Inventory); }
double FSeigeSimulation::Incoming(int32 Target, const FString& Resource) const
{ double Amount = 0; for (const FSeigeCourier& C : Couriers) if (C.TargetId == Target && (Resource.IsEmpty() || Resource == C.Resource)) Amount += C.Amount; return Amount; }
double FSeigeSimulation::WorkFraction(const FSeigeBuilding& B) const
{
    const FSeigeBuildingDef* D = Definition(B);
    if (!D || !B.Enabled || B.Health <= 0) return 0;
    if (D->Jobs == 0) return WorkforceEfficiency;
    if (TextRule(TEXT("workforce_mode")) == TEXT("full_staff")) return B.Workers >= D->Jobs ? WorkforceEfficiency : 0;
    return WorkforceEfficiency * static_cast<double>(B.Workers) / D->Jobs;
}
double FSeigeSimulation::TotalStock(const FString& Resource) const
{
    double Amount = 0;
    for (const FSeigeBuilding& B : Buildings) if (B.Health > 0) Amount += B.Inventory.FindRef(Resource);
    for (const FSeigeCourier& C : Couriers) if (C.Resource == Resource) Amount += C.Amount;
    return Amount;
}
bool FSeigeSimulation::IsVisible(FVector2D P) const
{
    for (const FSeigeBuilding& B : Buildings)
    { const FSeigeBuildingDef* D = Definition(B); if (D && D->SensorRange > 0 && WorkFraction(B) > 0 && FVector2D::Distance(P, B.Position) <= D->SensorRange) return true; }
    return false;
}
bool FSeigeSimulation::CanPlaceBuilding(const FString& Id, FVector2D P, FString& Error) const
{
    const FSeigeBuildingDef* D = BuildingDefs.Find(Id);
    const FSeigeBuilding* C = Core();
    if (!Policy || Escaped || Failed || !C || C->Health <= 0) { Error = TEXT("Colony is no longer under your command"); return false; }
    if (!D || !BuildMenu.Contains(Id)) { Error = TEXT("Definition is not available in the build menu"); return false; }
    if (D->Role == TEXT("core")) { Error = TEXT("Only one command core is allowed"); return false; }
    if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || FMath::Abs(P.X) + D->Footprint > WorldHalfSize || FMath::Abs(P.Y) + D->Footprint > WorldHalfSize) { Error = TEXT("Outside the sector boundary"); return false; }
    if (Number(TEXT("placement_requires_visibility")) > 0 && !IsVisible(P)) { Error = TEXT("Outside live sensor coverage; extend your sensors first"); return false; }
    for (const FSeigeBuilding& B : Buildings)
        if (B.Health > 0 && FVector2D::Distance(P, B.Position) < D->Footprint + Definition(B)->Footprint + Number(TEXT("minimum_build_spacing"))) { Error = TEXT("Too close to another building"); return false; }
    if (!D->ExtractResource.IsEmpty())
    {
        const FSeigeNode* Selected = nullptr; double Best = Number(TEXT("extractor_snap_distance"));
        for (const FSeigeNode& N : Nodes)
        { const double Distance = FVector2D::Distance(P, N.Position); if (N.Resource == D->ExtractResource && Distance <= Best) { Selected = &N; Best = Distance; } }
        if (!Selected) { Error = TEXT("Place this extractor on its matching resource deposit"); return false; }
        for (const FSeigeBuilding& B : Buildings)
            if (B.Health > 0 && Definition(B)->ExtractResource == D->ExtractResource && FVector2D::Distance(B.Position, Selected->Position) <= Number(TEXT("extractor_snap_distance"))) { Error = TEXT("This deposit already has an extractor"); return false; }
    }
    if (!HasAmounts(C->Inventory, D->Cost)) { Error = TEXT("Insufficient construction materials at the command core"); return false; }
    Error.Empty(); return true;
}
bool FSeigeSimulation::PlaceBuilding(const FString& Id, FVector2D P, FString& Error)
{
    if (!CanPlaceBuilding(Id, P, Error)) return false;
    const FSeigeBuildingDef& D = BuildingDefs[Id]; Consume(Core()->Inventory, D.Cost);
    FSeigeBuilding B; B.Id = NextId++; B.DefId = Id; B.Position = P; B.Health = D.Health; B.Status = TEXT("Awaiting automatic staffing");
    Buildings.Add(B); AllocateWorkers(); AddEvent(D.Name + TEXT(" established")); return true;
}
void FSeigeSimulation::ToggleBuilding(int32 Id)
{
    if (Escaped || Failed) return;
    if (FSeigeBuilding* B = FindBuilding(Id))
        if (B->Health > 0 && B->DefId != CoreDefinition) { B->Enabled = !B->Enabled; B->Status = B->Enabled ? TEXT("Enabled") : TEXT("Disabled"); AllocateWorkers(); }
}
void FSeigeSimulation::AllocateWorkers()
{
    TotalJobs = 0; Employed = 0; int32 Available = Population;
    for (FSeigeBuilding& B : Buildings)
    {
        B.Workers = 0;
        if (!B.Enabled || B.Health <= 0) continue;
        const int32 Jobs = Definition(B)->Jobs; TotalJobs += Jobs;
        B.Workers = FMath::Min(Jobs, Available); Available -= B.Workers; Employed += B.Workers;
    }
}
void FSeigeSimulation::Tick(double Seconds)
{
    if (!Policy || !FMath::IsFinite(Seconds) || Seconds <= 0 || Escaped || Failed) return;
    // Bounded substeps preserve collision/transport behavior at accelerated simulation speed.
    while (Seconds > UE_DOUBLE_SMALL_NUMBER && !Escaped && !Failed)
    {
        const double Step = FMath::Min(Seconds, Number(TEXT("fixed_step_seconds"))); Seconds -= Step; Time += Step;
        AllocateWorkers(); StepPopulation(Step); AllocateWorkers(); StepProduction(Step); StepLogistics(Step);
        while (Time >= NextWaveTime) { TriggerWave(); NextWaveTime += Number(TEXT("wave_interval")); }
        while (Time >= NextRoamTime) { SpawnEnemies(static_cast<int32>(Number(TEXT("roam_count")))); NextRoamTime += Number(TEXT("roam_interval")); }
        StepCombat(Step); CheckObjectives();
    }
}
void FSeigeSimulation::StepPopulation(double Seconds)
{
    FSeigeBuilding* C = Core(); if (!C || C->Health <= 0) return;
    const int32 Target = FMath::Max(TotalJobs, static_cast<int32>(Number(TEXT("minimum_population"))));
    const FSeigeRecipeDef& Recipe = Recipes[TextRule(TEXT("population_recipe"))];
    if (Population == Target) PopulationClock = 0;
    else
    {
        PopulationClock += Seconds;
        const double Interval = Population < Target ? Recipe.Seconds : Number(TEXT("robot_retire_seconds"));
        while (PopulationClock >= Interval && Population != Target)
        {
            if (Population > Target) { --Population; PopulationClock -= Interval; }
            else if (HasAmounts(C->Inventory, Recipe.Inputs)) { Consume(C->Inventory, Recipe.Inputs); ++Population; PopulationClock -= Interval; }
            else { PopulationClock = Interval; break; }
        }
        if (Population == Target) PopulationClock = 0;
    }
    UpkeepClock += Seconds;
    while (UpkeepClock >= Number(TEXT("upkeep_interval")))
    {
        UpkeepClock -= Number(TEXT("upkeep_interval"));
        const FString Resource = TextRule(TEXT("upkeep_resource")); const double Need = Population * Number(TEXT("upkeep_per_robot"));
        double& Stock = C->Inventory.FindOrAdd(Resource);
        const bool Supplied = Stock + UE_DOUBLE_SMALL_NUMBER >= Need;
        const double NewEfficiency = Supplied ? 1 : Number(TEXT("upkeep_shortage_efficiency"));
        if (WorkforceEfficiency != NewEfficiency) AddEvent(Supplied ? TEXT("Robot maintenance restored; normal operating efficiency") : TEXT("Core maintenance supply exhausted; workforce efficiency reduced"));
        if (Supplied) Stock = FMath::Max(0.0, Stock - Need);
        WorkforceEfficiency = NewEfficiency;
    }
}
void FSeigeSimulation::StepProduction(double Seconds)
{
    for (FSeigeBuilding& B : Buildings)
    {
        const FSeigeBuildingDef& D = *Definition(B);
        if (B.Health <= 0) { B.Status = TEXT("Destroyed"); continue; }
        // Repair is automatic even on disabled buildings, but always consumes local material.
        if (B.Health < D.Health)
        {
            const FString Resource = TextRule(TEXT("repair_resource")); double& Stock = B.Inventory.FindOrAdd(Resource);
            const double Restored = FMath::Min3(D.Health - B.Health, Number(TEXT("repair_health_per_second")) * Seconds, Stock * Number(TEXT("repair_health_per_unit")));
            Stock = FMath::Max(0.0, Stock - Restored / Number(TEXT("repair_health_per_unit"))); B.Health += Restored;
        }
        if (!B.Enabled) { B.Status = TEXT("Disabled (repairs remain automatic)"); continue; }
        const double Fraction = WorkFraction(B);
        if (Fraction <= 0) { B.Status = TEXT("Waiting for robots"); continue; }
        if (!D.ExtractResource.IsEmpty())
        {
            const double Room = FMath::Max(0.0, D.StorageCapacity - Occupied(B));
            const double Produced = FMath::Min(Room, D.ExtractRate * Seconds * Fraction);
            B.Inventory.FindOrAdd(D.ExtractResource) += Produced;
            B.Status = Room <= UE_DOUBLE_SMALL_NUMBER ? TEXT("Storage full; waiting for courier") : TEXT("Extracting");
        }
        else if (!D.Recipe.IsEmpty())
        {
            const FSeigeRecipeDef& R = Recipes[D.Recipe];
            if (!HasAmounts(B.Inventory, R.Inputs)) { B.Status = TEXT("Waiting for delivered inputs"); continue; }
            if (Occupied(B) - Sum(R.Inputs) + Sum(R.Outputs) > D.StorageCapacity + UE_DOUBLE_SMALL_NUMBER) { B.Status = TEXT("Storage full; waiting for courier"); continue; }
            B.Progress += Seconds * Fraction / R.Seconds; B.Status = TEXT("Producing");
            while (B.Progress + UE_DOUBLE_SMALL_NUMBER >= 1)
            {
                if (!HasAmounts(B.Inventory, R.Inputs) || Occupied(B) - Sum(R.Inputs) + Sum(R.Outputs) > D.StorageCapacity + UE_DOUBLE_SMALL_NUMBER) { B.Progress = 0; break; }
                Consume(B.Inventory, R.Inputs); B.Progress = FMath::Max(0.0, B.Progress - 1);
                for (const auto& Pair : R.Outputs) { B.Inventory.FindOrAdd(Pair.Key) += Pair.Value; ProducedUnits.FindOrAdd(Pair.Key) += Pair.Value; }
            }
            if (!HasAmounts(B.Inventory,R.Inputs)) B.Progress = 0;
        }
        else if (B.DefId == CoreDefinition) B.Status = Population < TotalJobs ? TEXT("Assembling robots for open jobs") : TEXT("Command and maintenance online");
        else if (D.Role == TEXT("storage")) B.Status = TEXT("Receiving and redistributing overflow");
        else if (D.Role == TEXT("defense")) B.Status = TEXT("Automatic defense online");
        else B.Status = TEXT("Sensor coverage online");
    }
}
double FSeigeSimulation::Demand(const FSeigeBuilding& B, const FString& Resource, bool IncludeCoreReserve) const
{
    if (B.Health <= 0) return 0;
    const FSeigeBuildingDef& D = *Definition(B); double Need = 0;
    if (Resource == TextRule(TEXT("repair_resource"))) Need += Number(TEXT("repair_buffer_units"));
    if (B.Enabled && !D.Recipe.IsEmpty()) Need += Recipes[D.Recipe].Inputs.FindRef(Resource) * Number(TEXT("delivery_buffer_cycles"));
    if (B.DefId == CoreDefinition)
    {
        Need += Recipes[TextRule(TEXT("population_recipe"))].Inputs.FindRef(Resource) * Number(TEXT("population_buffer_robots"));
        if (Resource == TextRule(TEXT("upkeep_resource"))) Need += Population * Number(TEXT("upkeep_per_robot")) * Number(TEXT("upkeep_buffer_intervals"));
        if (IncludeCoreReserve) Need += CoreReserves.FindRef(Resource);
    }
    return Need;
}
void FSeigeSimulation::StepLogistics(double Seconds)
{
    for (int32 I = Couriers.Num() - 1; I >= 0; --I)
    {
        FSeigeCourier& C = Couriers[I]; FSeigeBuilding* Target = FindBuilding(C.TargetId);
        if (!Target || Target->Health <= 0)
        {
            Target = FindBuilding(C.SourceId);
            if (!Target || Target->Health <= 0) Target = Core();
            if (!Target || Target->Health <= 0) { ++LostCouriers; Couriers.RemoveAt(I); continue; }
            C.TargetId = Target->Id;
        }
        const FVector2D Delta = Target->Position - C.Position; const double Distance = Delta.Size();
        const double Move = Number(TEXT("courier_speed")) * Seconds;
        if (Distance <= Move + UE_DOUBLE_SMALL_NUMBER)
        {
            C.Position = Target->Position;
            const double Amount = FMath::Min(C.Amount, FMath::Max(0.0, Definition(*Target)->StorageCapacity - Occupied(*Target)));
            Target->Inventory.FindOrAdd(C.Resource) += Amount; C.Amount -= Amount; DeliveredUnits += Amount;
            if (C.Amount <= UE_DOUBLE_SMALL_NUMBER) Couriers.RemoveAt(I);
        }
        else C.Position += Delta / Distance * Move;
    }
    DispatchClock += Seconds;
    if (DispatchClock < Number(TEXT("dispatch_interval"))) return;
    DispatchClock = FMath::Fmod(DispatchClock, Number(TEXT("dispatch_interval")));
    TArray<FString> ResourceIds; Resources.GetKeys(ResourceIds); ResourceIds.Sort();
    // Supply consuming workplaces before building up the construction reserve at the core.
    TArray<int32> Targets;
    for (int32 I = 0; I < Buildings.Num(); ++I) if (Buildings[I].DefId != CoreDefinition && Definition(Buildings[I])->Role != TEXT("storage")) Targets.Add(I);
    for (int32 I = 0; I < Buildings.Num(); ++I) if (Buildings[I].DefId == CoreDefinition) Targets.Add(I);
    for (int32 I = 0; I < Buildings.Num(); ++I) if (Definition(Buildings[I])->Role == TEXT("storage")) Targets.Add(I);
    for (int32 TargetIndex : Targets)
    {
        FSeigeBuilding& Target = Buildings[TargetIndex]; if (Target.Health <= 0) continue;
        const FSeigeBuildingDef& TD = *Definition(Target);
        if (TD.Role == TEXT("storage") && WorkFraction(Target) <= 0) continue;
        for (const FString& Resource : ResourceIds)
        {
            if (Couriers.Num() >= Number(TEXT("max_couriers"))) return;
            double Need = Demand(Target, Resource, true) - Target.Inventory.FindRef(Resource) - Incoming(Target.Id, Resource);
            if (TD.Role == TEXT("storage")) Need = TD.StorageCapacity - Occupied(Target) - Incoming(Target.Id, TEXT(""));
            Need = FMath::Min(Need, TD.StorageCapacity - Occupied(Target) - Incoming(Target.Id, TEXT("")));
            if (Need <= UE_DOUBLE_SMALL_NUMBER) continue;
            FSeigeBuilding* Source = nullptr; double Closest = TNumericLimits<double>::Max(); double Supply = 0;
            for (FSeigeBuilding& Candidate : Buildings)
            {
                if (Candidate.Id == Target.Id || Candidate.Health <= 0) continue;
                const FSeigeBuildingDef& SD = *Definition(Candidate);
                if (TD.Role == TEXT("storage") && (SD.Role == TEXT("storage") || SD.Role == TEXT("core"))) continue;
                double Available = Candidate.Inventory.FindRef(Resource) - Demand(Candidate, Resource, false);
                if (TD.Role == TEXT("storage"))
                {
                    // Only overflow when all non-depot demand is fulfilled; no depot-to-depot loops.
                    bool Outstanding = false;
                    for (const FSeigeBuilding& Other : Buildings)
                        if (Other.Id != Candidate.Id && Other.Health > 0 && Definition(Other)->Role != TEXT("storage") && Demand(Other, Resource, true) > Other.Inventory.FindRef(Resource) + Incoming(Other.Id, Resource) + UE_DOUBLE_SMALL_NUMBER) { Outstanding = true; break; }
                    if (Outstanding) continue;
                }
                const double Distance = FVector2D::Distance(Candidate.Position, Target.Position);
                if (Available + UE_DOUBLE_SMALL_NUMBER >= FMath::Min(Need, Number(TEXT("courier_min_batch"))) && Available > UE_DOUBLE_SMALL_NUMBER && Distance < Closest) { Source = &Candidate; Closest = Distance; Supply = Available; }
            }
            if (!Source) continue;
            const double Amount = FMath::Min3(Need, Supply, Number(TEXT("courier_capacity")));
            FSeigeCourier C; C.Id = NextId++; C.SourceId = Source->Id; C.TargetId = Target.Id; C.Resource = Resource; C.Amount = Amount; C.Position = Source->Position;
            Source->Inventory.FindOrAdd(Resource) -= Amount; Couriers.Add(C);
        }
    }
}
void FSeigeSimulation::SpawnEnemies(int32 Count)
{
    const FSeigeBuilding* Command = Core();
    const FVector2D Center = Command ? Command->Position : FVector2D::ZeroVector;
    for (int32 I = 0; I < Count; ++I)
    {
        const double Angle = Random.FRand() * UE_TWO_PI; FSeigeEnemy E; E.Id = NextId++;
        E.Position = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Number(TEXT("spawn_radius"));
        E.Position.X = FMath::Clamp(E.Position.X, -WorldHalfSize, WorldHalfSize);
        E.Position.Y = FMath::Clamp(E.Position.Y, -WorldHalfSize, WorldHalfSize);
        E.Health = Number(TEXT("enemy_health")); Enemies.Add(E);
    }
}
void FSeigeSimulation::TriggerWave()
{
    if (!Policy || Escaped || Failed) return;
    int32 Live = 0; for (const FSeigeBuilding& B : Buildings) if (B.Health > 0) ++Live;
    const int32 Count = FMath::Clamp(FMath::CeilToInt(Number(TEXT("wave_base_count")) + Live * Number(TEXT("wave_per_building")) + Population * Number(TEXT("wave_per_population")) + Wave * Number(TEXT("wave_escalation_per_wave"))), 0, static_cast<int32>(Number(TEXT("wave_max_count"))));
    ++Wave; SpawnEnemies(Count); AddEvent(TEXT("Alien pulse activity detected. Live positions require sensor coverage."));
}
void FSeigeSimulation::StepCombat(double Seconds)
{
    for (FSeigeBuilding& B : Buildings)
    {
        const FSeigeBuildingDef& D = *Definition(B); const double Fraction = WorkFraction(B);
        if (D.DamagePerShot <= 0 || Fraction <= 0) continue;
        double WorkSeconds=Seconds*Fraction;
        while (true)
        {
            if (B.WeaponCooldown>WorkSeconds+UE_DOUBLE_SMALL_NUMBER) {B.WeaponCooldown-=WorkSeconds;break;}
            WorkSeconds=FMath::Max(0.,WorkSeconds-B.WeaponCooldown);B.WeaponCooldown=0;
            FSeigeEnemy* Target = nullptr; double Closest = D.AttackRange;
            for (FSeigeEnemy& E : Enemies)
            { const double Distance = FVector2D::Distance(E.Position, B.Position); if (E.Health > 0 && Distance <= Closest && IsVisible(E.Position)) { Target = &E; Closest = Distance; } }
            // No target stores one ready shot, never a backlog of idle-time damage.
            if (!Target) break;
            Target->Health-=D.DamagePerShot;B.WeaponCooldown=D.ReloadSeconds;
            B.LastShotTime=Time;B.LastShotPosition=Target->Position;
            if (WorkSeconds<=UE_DOUBLE_SMALL_NUMBER) break;
        }
    }
    Enemies.RemoveAll([](const FSeigeEnemy& E){return E.Health <= 0;});
    for (FSeigeEnemy& E : Enemies)
    {
        FSeigeBuilding* Target = nullptr; double Closest = TNumericLimits<double>::Max();
        for (FSeigeBuilding& B : Buildings)
        { const double Distance = FVector2D::Distance(E.Position, B.Position); if (B.Health > 0 && Distance < Closest) { Target = &B; Closest = Distance; } }
        if (!Target) continue;
        const double AttackDistance = Number(TEXT("enemy_attack_range")) + Definition(*Target)->Footprint;
        if (Closest > AttackDistance)
            E.Position += (Target->Position - E.Position).GetSafeNormal() * FMath::Min(Number(TEXT("enemy_speed")) * Seconds, Closest - AttackDistance);
        else
        {
            Target->Health = FMath::Max(0.0, Target->Health - Number(TEXT("enemy_damage_per_second")) * Seconds);
            if (Target->Health <= 0)
            {
                AddEvent(Definition(*Target)->Name + TEXT(" destroyed")); Target->Workers = 0;
                if (Target->DefId == CoreDefinition) { Failed = true; LaunchShuttle(); return; }
                Target->Inventory.Empty();
            }
        }
        for (int32 I = Couriers.Num() - 1; I >= 0; --I)
            if (FVector2D::Distance(E.Position, Couriers[I].Position) <= Number(TEXT("enemy_courier_attack_range"))) { Couriers.RemoveAt(I); ++LostCouriers; }
    }
}
void FSeigeSimulation::LaunchShuttle()
{
    if (Escaped || !Policy) return;
    Escaped = true;
    AddEvent(Failed ? TEXT("Core destroyed. Emergency shuttle launched with only cargo already aboard; colony command lost.") : TEXT("Shuttle launched with only cargo already aboard. Local assets remain behind; this prototype ends here."));
}
void FSeigeSimulation::CheckObjectives()
{
    if (Won || Failed || Escaped) return;
    int32 Count = 0; for (const FSeigeBuilding& B : Buildings) if (B.Health > 0 && B.DefId == TextRule(TEXT("objective_building"))) ++Count;
    if (Time >= Number(TEXT("objective_survival_seconds")) && Count >= Number(TEXT("objective_building_count")) && ProducedUnits.FindRef(TextRule(TEXT("objective_resource"))) >= Number(TEXT("objective_produced_amount")))
    { Won = true; AddEvent(TEXT("First landing objective complete. Your industry survived; you may keep building.")); }
}
void FSeigeSimulation::AddEvent(const FString& Message)
{
    FSeigeEvent Event; Event.Time = Time; Event.Text = Message; Events.Add(Event);
    if (Policy) while (Events.Num() > Number(TEXT("event_history_limit"))) Events.RemoveAt(0);
}
FString FSeigeSimulation::ObjectiveText() const
{
    if (!Policy) return TEXT("Rules not loaded");
    int32 Count = 0; for (const FSeigeBuilding& B : Buildings) if (B.Health > 0 && B.DefId == TextRule(TEXT("objective_building"))) ++Count;
    return FString::Printf(TEXT("Survive %.0f / %.0f s | Manufacture %.0f / %.0f %s | %s %d / %.0f"), Time, Number(TEXT("objective_survival_seconds")), ProducedUnits.FindRef(TextRule(TEXT("objective_resource"))), Number(TEXT("objective_produced_amount")), *Resources[TextRule(TEXT("objective_resource"))].Name, *BuildingDefs[TextRule(TEXT("objective_building"))].Name, Count, Number(TEXT("objective_building_count")));
}
FString FSeigeSimulation::WorkforceStatus() const
{ return FString::Printf(TEXT("Robots %d | Jobs %d | Open %d | Efficiency %.0f%%"), Population, TotalJobs, FMath::Max(0, TotalJobs - Employed), WorkforceEfficiency * 100); }
double FSeigeSimulation::FixedStepSeconds() const { return Policy ? Number(TEXT("fixed_step_seconds")) : 0; }

bool FSeigeSimulation::Save(const FString& Filename, FString& Error) const
{
    if (!Policy) { Error = TEXT("Cannot save before rules are initialized"); return false; }
    FObject O = MakeShared<FJsonObject>(); O->SetNumberField(TEXT("save_format"), 1);
    O->SetStringField(TEXT("rules_version"), RulesVersion); O->SetStringField(TEXT("rules_fingerprint"), RulesFingerprint);
    O->SetNumberField(TEXT("time"),Time); O->SetNumberField(TEXT("next_wave_time"),NextWaveTime); O->SetNumberField(TEXT("next_roam_time"),NextRoamTime);
    O->SetNumberField(TEXT("population_clock"),PopulationClock); O->SetNumberField(TEXT("dispatch_clock"),DispatchClock); O->SetNumberField(TEXT("upkeep_clock"),UpkeepClock); O->SetNumberField(TEXT("workforce_efficiency"),WorkforceEfficiency);
    O->SetNumberField(TEXT("population"),Population); O->SetNumberField(TEXT("wave"),Wave); O->SetNumberField(TEXT("lost_couriers"),LostCouriers); O->SetNumberField(TEXT("delivered_units"),DeliveredUnits); O->SetNumberField(TEXT("next_id"),NextId); O->SetNumberField(TEXT("random_seed"),Random.GetCurrentSeed());
    O->SetBoolField(TEXT("escaped"),Escaped); O->SetBoolField(TEXT("failed"),Failed); O->SetBoolField(TEXT("won"),Won);
    O->SetObjectField(TEXT("shuttle_cargo"),JsonAmounts(ShuttleCargo)); O->SetObjectField(TEXT("produced_units"),JsonAmounts(ProducedUnits));
    TArray<TSharedPtr<FJsonValue>> A;
    for (const FSeigeBuilding& B : Buildings)
    {
        FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("id"),B.Id); V->SetStringField(TEXT("definition"),B.DefId); V->SetStringField(TEXT("status"),B.Status); WritePosition(V,B.Position);
        V->SetNumberField(TEXT("health"),B.Health); V->SetNumberField(TEXT("progress"),B.Progress); V->SetBoolField(TEXT("enabled"),B.Enabled); V->SetObjectField(TEXT("inventory"),JsonAmounts(B.Inventory));
        V->SetNumberField(TEXT("weapon_cooldown"),B.WeaponCooldown);V->SetNumberField(TEXT("last_shot_time"),B.LastShotTime);
        V->SetArrayField(TEXT("last_shot_position"),{MakeShared<FJsonValueNumber>(B.LastShotPosition.X),MakeShared<FJsonValueNumber>(B.LastShotPosition.Y)});A.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("buildings"),A); A.Empty();
    for (const FSeigeNode& N : Nodes) { FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("id"),N.Id); V->SetStringField(TEXT("resource"),N.Resource); WritePosition(V,N.Position); A.Add(MakeShared<FJsonValueObject>(V)); }
    O->SetArrayField(TEXT("nodes"),A); A.Empty();
    for (const FSeigeCourier& C : Couriers) { FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("id"),C.Id); V->SetNumberField(TEXT("source"),C.SourceId); V->SetNumberField(TEXT("target"),C.TargetId); V->SetStringField(TEXT("resource"),C.Resource); V->SetNumberField(TEXT("amount"),C.Amount); WritePosition(V,C.Position); A.Add(MakeShared<FJsonValueObject>(V)); }
    O->SetArrayField(TEXT("couriers"),A); A.Empty();
    for (const FSeigeEnemy& E : Enemies) { FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("id"),E.Id); V->SetNumberField(TEXT("health"),E.Health); WritePosition(V,E.Position); A.Add(MakeShared<FJsonValueObject>(V)); }
    O->SetArrayField(TEXT("enemies"),A); A.Empty();
    for (const FSeigeEvent& E : Events) { FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("time"),E.Time); V->SetStringField(TEXT("text"),E.Text); A.Add(MakeShared<FJsonValueObject>(V)); }
    O->SetArrayField(TEXT("events"),A);
    return WriteJson(O,Filename,Error);
}
bool FSeigeSimulation::Load(const FString& Filename, FString& Error)
{
    if (!Policy) { Error = TEXT("Initialize rules before loading a colony"); return false; }
    FObject O; FString Raw, Version, Fingerprint; double Format = 0;
    if (!ReadJson(Filename,O,Raw,Error) || !Numeric(O,TEXT("save_format"),Format,1,Error,true) || !StringField(O,TEXT("rules_version"),Version,Error) || !StringField(O,TEXT("rules_fingerprint"),Fingerprint,Error)) return false;
    if (Format != 1 || Version != RulesVersion || Fingerprint != RulesFingerprint) { Error = TEXT("Save is incompatible with this rule version or edited rule files. Start a new colony."); return false; }
    // Parse into a temporary simulation: a corrupt save must never damage the running colony.
    FSeigeSimulation Candidate = *this;
    if (!Numeric(O,TEXT("time"),Candidate.Time,0,Error) || !Numeric(O,TEXT("next_wave_time"),Candidate.NextWaveTime,0,Error) || !Numeric(O,TEXT("next_roam_time"),Candidate.NextRoamTime,0,Error) || !Numeric(O,TEXT("population_clock"),Candidate.PopulationClock,0,Error) || !Numeric(O,TEXT("dispatch_clock"),Candidate.DispatchClock,0,Error) || !Numeric(O,TEXT("upkeep_clock"),Candidate.UpkeepClock,0,Error) || !Numeric(O,TEXT("workforce_efficiency"),Candidate.WorkforceEfficiency,0,Error) || !Numeric(O,TEXT("delivered_units"),Candidate.DeliveredUnits,0,Error) || !IntegerField(O,TEXT("population"),Candidate.Population,0,Error) || !IntegerField(O,TEXT("wave"),Candidate.Wave,0,Error) || !IntegerField(O,TEXT("lost_couriers"),Candidate.LostCouriers,0,Error) || !IntegerField(O,TEXT("next_id"),Candidate.NextId,1,Error)) return false;
    double Seed = 0;
    if (!Numeric(O,TEXT("random_seed"),Seed,MIN_int32,Error) || Seed > MAX_int32 || Seed != FMath::FloorToDouble(Seed)) { Error = TEXT("Invalid saved random state"); return false; }
    Candidate.Random.Initialize(static_cast<int32>(Seed));
    if (!O->TryGetBoolField(TEXT("escaped"),Candidate.Escaped) || !O->TryGetBoolField(TEXT("failed"),Candidate.Failed) || !O->TryGetBoolField(TEXT("won"),Candidate.Won)) { Error = TEXT("Missing saved outcome flags"); return false; }
    if (!Amounts(O,TEXT("shuttle_cargo"),Candidate.ShuttleCargo,Resources,Error) || !Amounts(O,TEXT("produced_units"),Candidate.ProducedUnits,Resources,Error)) return false;
    if (Candidate.WorkforceEfficiency > 1 || Candidate.DispatchClock >= Number(TEXT("dispatch_interval")) + UE_DOUBLE_SMALL_NUMBER || Candidate.UpkeepClock >= Number(TEXT("upkeep_interval")) + UE_DOUBLE_SMALL_NUMBER || Candidate.NextWaveTime < Candidate.Time || Candidate.NextRoamTime < Candidate.Time || (Candidate.Failed && !Candidate.Escaped) || Sum(Candidate.ShuttleCargo) > Number(TEXT("shuttle_capacity")) + UE_DOUBLE_SMALL_NUMBER) { Error = TEXT("Inconsistent saved timers, flags or cargo"); return false; }
    Candidate.Buildings.Empty(); Candidate.Nodes.Empty(); Candidate.Couriers.Empty(); Candidate.Enemies.Empty(); Candidate.Events.Empty();
    TSet<int32> Ids; int32 MaximumId = 0, CoreCount = 0;
    auto ReadId = [&](const FObject& V,int32& Id)->bool { if (!IntegerField(V,TEXT("id"),Id,1,Error)) return false; if (Ids.Contains(Id)) { Error = TEXT("Duplicate saved entity ID"); return false; } Ids.Add(Id); MaximumId = FMath::Max(MaximumId,Id); return true; };
    auto Inside = [&](FVector2D P)->bool { if (FMath::Abs(P.X) > WorldHalfSize || FMath::Abs(P.Y) > WorldHalfSize) { Error = TEXT("Saved entity outside sector"); return false; } return true; };
    const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
    if (!ArrayField(O,TEXT("buildings"),A,Error)) return false;
    for (const auto& Value : *A)
    {
        const FObject V = Value->AsObject(); FSeigeBuilding B;
        if (!ReadId(V,B.Id) || !StringField(V,TEXT("definition"),B.DefId,Error) || !BuildingDefs.Contains(B.DefId) || !StringField(V,TEXT("status"),B.Status,Error) || !PositionField(V,TEXT("position"),B.Position,Error) || !Inside(B.Position) || !Numeric(V,TEXT("health"),B.Health,0,Error) || !Numeric(V,TEXT("progress"),B.Progress,0,Error) || !Amounts(V,TEXT("inventory"),B.Inventory,Resources,Error)) { if (Error.IsEmpty()) Error = TEXT("Invalid saved building"); return false; }
        if (!V->TryGetBoolField(TEXT("enabled"),B.Enabled) || B.Health > BuildingDefs[B.DefId].Health || B.Progress > 1 + UE_DOUBLE_SMALL_NUMBER || Occupied(B) > BuildingDefs[B.DefId].StorageCapacity + UE_DOUBLE_SMALL_NUMBER) { Error = TEXT("Invalid building health, progress or storage"); return false; }
        if (!Numeric(V,TEXT("weapon_cooldown"),B.WeaponCooldown,0,Error) || B.WeaponCooldown>BuildingDefs[B.DefId].ReloadSeconds+UE_DOUBLE_SMALL_NUMBER || !Numeric(V,TEXT("last_shot_time"),B.LastShotTime,-1,Error) || B.LastShotTime>Candidate.Time || (B.LastShotTime<0 && B.LastShotTime!=-1) || !PositionField(V,TEXT("last_shot_position"),B.LastShotPosition,Error) || !Inside(B.LastShotPosition))
        {Error=TEXT("Invalid saved weapon timing or target position");return false;}
        if (B.DefId == CoreDefinition) ++CoreCount;
        Candidate.Buildings.Add(B);
    }
    if (CoreCount != 1 || (!Candidate.Escaped && Candidate.Core()->Health <= 0)) { Error = TEXT("Save requires exactly one command core"); return false; }
    if (!ArrayField(O,TEXT("nodes"),A,Error)) return false;
    for (const auto& Value : *A) { const FObject V = Value->AsObject(); FSeigeNode N; if (!ReadId(V,N.Id) || !StringField(V,TEXT("resource"),N.Resource,Error) || !Resources.Contains(N.Resource) || !PositionField(V,TEXT("position"),N.Position,Error) || !Inside(N.Position)) { if(Error.IsEmpty()) Error = TEXT("Invalid saved deposit"); return false; } Candidate.Nodes.Add(N); }
    if (!ArrayField(O,TEXT("couriers"),A,Error)) return false;
    for (const auto& Value : *A)
    {
        const FObject V = Value->AsObject(); FSeigeCourier C;
        if (!ReadId(V,C.Id) || !IntegerField(V,TEXT("source"),C.SourceId,1,Error) || !IntegerField(V,TEXT("target"),C.TargetId,1,Error) || !StringField(V,TEXT("resource"),C.Resource,Error) || !Resources.Contains(C.Resource) || !Numeric(V,TEXT("amount"),C.Amount,UE_DOUBLE_SMALL_NUMBER,Error) || !PositionField(V,TEXT("position"),C.Position,Error) || !Inside(C.Position)) { if(Error.IsEmpty()) Error = TEXT("Invalid saved courier"); return false; }
        if (C.Amount > Number(TEXT("courier_capacity")) + UE_DOUBLE_SMALL_NUMBER || !Candidate.FindBuilding(C.SourceId) || !Candidate.FindBuilding(C.TargetId)) { Error = TEXT("Invalid courier capacity or endpoint"); return false; } Candidate.Couriers.Add(C);
    }
    if (Candidate.Couriers.Num() > Number(TEXT("max_couriers"))) { Error = TEXT("Saved courier count exceeds rule limit"); return false; }
    if (!ArrayField(O,TEXT("enemies"),A,Error)) return false;
    for (const auto& Value : *A) { const FObject V = Value->AsObject(); FSeigeEnemy E; if (!ReadId(V,E.Id) || !Numeric(V,TEXT("health"),E.Health,UE_DOUBLE_SMALL_NUMBER,Error) || E.Health > Number(TEXT("enemy_health")) || !PositionField(V,TEXT("position"),E.Position,Error) || !Inside(E.Position)) { if(Error.IsEmpty()) Error = TEXT("Invalid saved enemy"); return false; } Candidate.Enemies.Add(E); }
    if (!ArrayField(O,TEXT("events"),A,Error)) return false;
    for (const auto& Value : *A) { FSeigeEvent E; if (!Numeric(Value->AsObject(),TEXT("time"),E.Time,0,Error) || E.Time > Candidate.Time || !StringField(Value->AsObject(),TEXT("text"),E.Text,Error)) return false; Candidate.Events.Add(E); }
    if (Candidate.NextId <= MaximumId || Candidate.Events.Num() > Number(TEXT("event_history_limit"))) { Error = TEXT("Invalid next entity ID or event history"); return false; }
    Candidate.AllocateWorkers(); *this = MoveTemp(Candidate); Error.Empty(); return true;
}
