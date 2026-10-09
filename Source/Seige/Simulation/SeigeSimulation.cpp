#include "SeigeSimulation.h"
#include "SeigeResourceGeneration.h"
#include "SeigeCalendarRules.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SeigeCanonicalJson.h"
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
        if (!Resources.Contains(ResourceId) || !Pair.Value->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0 || (Resources[ResourceId].Discrete && Value != FMath::FloorToDouble(Value)))
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
void WritePoint(const FObject& O,const FString& Key,FVector2D P)
{O->SetArrayField(Key,{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)});}
void WriteRoute(const FObject& O,const FString& Key,const TArray<FVector2D>& Route)
{TArray<TSharedPtr<FJsonValue>> A;for(const auto& P:Route){TArray<TSharedPtr<FJsonValue>> XY={MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)};A.Add(MakeShared<FJsonValueArray>(XY));}O->SetArrayField(Key,A);}
bool ReadRoute(const FObject& O,const FString& Key,TArray<FVector2D>& Route,double Bounds,FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;if(!ArrayField(O,Key,A,Error)||A->Num()>4096){Error=TEXT("Invalid saved route");return false;}Route.Empty();
    for(const auto& V:*A){const TArray<TSharedPtr<FJsonValue>>* XY=nullptr;double X=0,Y=0;if(!V->TryGetArray(XY)||XY->Num()!=2||!(*XY)[0]->TryGetNumber(X)||!(*XY)[1]->TryGetNumber(Y)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||FMath::Abs(X)>Bounds||FMath::Abs(Y)>Bounds){Error=TEXT("Invalid saved route coordinate");return false;}Route.Add(FVector2D(X,Y));}return true;
}
template<typename T> void WriteCrew(const FObject& O,const T& B)
{WritePoint(O,TEXT("builder_position"),B.BuilderPosition);WriteRoute(O,TEXT("builder_route"),B.BuilderRoute);O->SetNumberField(TEXT("builder_next_waypoint"),B.BuilderNextWaypoint);O->SetNumberField(TEXT("builders"),B.Builders);O->SetNumberField(TEXT("builders_on_site"),B.BuildersOnSite);O->SetNumberField(TEXT("travelling_builders"),B.TravellingBuilders);O->SetObjectField(TEXT("installed_materials"),JsonAmounts(B.InstalledMaterials));}
template<typename T> bool ReadCrew(const FObject& O,T& B,int32 MaxBuilders,double Bounds,const TMap<FString,FSeigeResourceDef>& Resources,FString& Error)
{
    if(!PositionField(O,TEXT("builder_position"),B.BuilderPosition,Error)||FMath::Abs(B.BuilderPosition.X)>Bounds||FMath::Abs(B.BuilderPosition.Y)>Bounds||!ReadRoute(O,TEXT("builder_route"),B.BuilderRoute,Bounds,Error)||!IntegerField(O,TEXT("builder_next_waypoint"),B.BuilderNextWaypoint,0,Error)||B.BuilderNextWaypoint>B.BuilderRoute.Num()||!IntegerField(O,TEXT("builders"),B.Builders,0,Error)||!IntegerField(O,TEXT("builders_on_site"),B.BuildersOnSite,0,Error)||B.Builders>MaxBuilders||!IntegerField(O,TEXT("travelling_builders"),B.TravellingBuilders,0,Error)||B.BuildersOnSite+B.TravellingBuilders>B.Builders||!Amounts(O,TEXT("installed_materials"),B.InstalledMaterials,Resources,Error)){Error=TEXT("Invalid saved construction crew");return false;}return true;
}
bool ValidInstallation(const TMap<FString,double>& Cost,const TMap<FString,double>& Stock,const TMap<FString,double>& Installed,double Progress,FString& Error)
{
    for(const auto& P:Stock)if(P.Value+Installed.FindRef(P.Key)>Cost.FindRef(P.Key)+1.e-6){Error=TEXT("Construction stock exceeds uninstalled bill");return false;}
    for(const auto& P:Installed)if(!Cost.Contains(P.Key)){Error=TEXT("Installed material is not in construction bill");return false;}
    for(const auto& P:Cost)if(FMath::Abs(Installed.FindRef(P.Key)-P.Value*Progress)>1.e-6){Error=TEXT("Installed materials disagree with construction progress");return false;}
    return true;
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

bool FSeigeSimulation::Initialize(const FString& RulesDirectory, FString& Error, bool bBackgroundBugs, bool bPeriodicAttacks, int32 SeedOverride,FVector2D WorldOffset)
{
    *this = FSeigeSimulation();
    BackgroundBugsEnabled = bBackgroundBugs; PeriodicAttacksEnabled = bPeriodicAttacks;
    RulesPath = FPaths::ConvertRelativePathToFull(RulesDirectory);
    if(!FMath::IsFinite(WorldOffset.X)||!FMath::IsFinite(WorldOffset.Y)||FMath::Abs(WorldOffset.X)>90000||FMath::Abs(WorldOffset.Y)>90000){Error=TEXT("Invalid world region offset");return false;}
    Environment.WorldOffset=WorldOffset;if(!Environment.Load(FPaths::Combine(RulesPath,TEXT("environment.json")),Error))return false;
    FString Fingerprint;
    TMap<FString, FObject> Documents;
    const TArray<FString> Names = {TEXT("resources"), TEXT("recipes"), TEXT("buildings"), TEXT("policies"), TEXT("scenario"), TEXT("transport"), TEXT("energy"), TEXT("trade"), TEXT("companions"), TEXT("walls"), TEXT("workers"), TEXT("calendar")};
    for (const FString& Name : Names)
    {
        FString Raw, Version; FObject O;
        if (!ReadJson(FPaths::Combine(RulesPath, Name + TEXT(".json")), O, Raw, Error) || !StringField(O, TEXT("version"), Version, Error)) return false;
        if (Version.IsEmpty() || (!RulesVersion.IsEmpty() && RulesVersion != Version)) { Error = TEXT("Rule file versions must match and be nonempty"); return false; }
        RulesVersion = Version; Fingerprint += SeigeCanonicalJson(O); Documents.Add(Name, O);
    }
    RulesFingerprint = FMD5::HashAnsiString(*(Fingerprint+Environment.Fingerprint));
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!ArrayField(Documents[TEXT("resources")], TEXT("resources"), Values, Error)) return false;
    for (const auto& V : *Values)
    {
        const FObject O = V->AsObject(); FSeigeResourceDef R;
        if (!StringField(O, TEXT("id"), R.Id, Error) || !StringField(O, TEXT("name"), R.Name, Error) || !IntegerField(O, TEXT("tier"), R.Tier, 0, Error) || !ColorField(O, R.Color, Error)) return false;
        if (R.Id.IsEmpty() || Resources.Contains(R.Id)) { Error = TEXT("Duplicate or empty resource ID: ") + R.Id; return false; }
        if(!StringField(O,TEXT("stockpile_visual"),R.StockpileVisual,Error) || (R.StockpileVisual!=TEXT("bulk")&&R.StockpileVisual!=TEXT("ingots")&&R.StockpileVisual!=TEXT("crates"))) {Error=TEXT("Invalid resource stockpile_visual: ")+R.Id;return false;}
        if(!StringField(O,TEXT("class"),R.Class,Error)||!StringField(O,TEXT("unit"),R.Unit,Error)||!Numeric(O,TEXT("unit_mass_kg"),R.UnitMassKg,1.e-9,Error)||!Numeric(O,TEXT("litres_per_unit"),R.LitresPerUnit,1.e-9,Error)||(R.Unit!=TEXT("kg")&&R.Unit!=TEXT("L")&&R.Unit!=TEXT("workers"))||(R.Unit==TEXT("kg")&&R.UnitMassKg!=1)||(R.Class!=TEXT("standard")&&R.Class!=TEXT("rare")&&R.Class!=TEXT("manufactured"))){Error=TEXT("Invalid resource units or class");return false;}
        if(!O->TryGetBoolField(TEXT("discrete"),R.Discrete)||(R.Unit==TEXT("workers")&&!R.Discrete)){Error=TEXT("Invalid discrete cargo selector");return false;}
        Resources.Add(R.Id, R);
    }
    if (Resources.IsEmpty()) { Error = TEXT("Resource catalog is empty"); return false; }
    if(!LoadTransport(Documents[TEXT("transport")],Error))return false;
    if (!ArrayField(Documents[TEXT("recipes")], TEXT("recipes"), Values, Error)) return false;
    for (const auto& V : *Values)
    {
        const FObject O = V->AsObject(); FSeigeRecipeDef R;
        if (!StringField(O, TEXT("id"), R.Id, Error) || !Numeric(O, TEXT("seconds"), R.Seconds, UE_DOUBLE_SMALL_NUMBER, Error) || !Amounts(O, TEXT("inputs"), R.Inputs, Resources, Error) || !Amounts(O, TEXT("outputs"), R.Outputs, Resources, Error)) return false;
        if (R.Id.IsEmpty() || Recipes.Contains(R.Id) || Sum(R.Inputs) <= 0) { Error = TEXT("Recipe needs a unique ID and positive inputs: ") + R.Id; return false; }
        if(!Numeric(O,TEXT("energy_kwh"),R.EnergyKWh,0,Error)||InventoryMassKg(R.Outputs)>InventoryMassKg(R.Inputs)+1.e-8){Error=TEXT("Recipe energy or mass conservation invalid: ")+R.Id;return false;}
        if(!IntegerField(O,TEXT("worker_output"),R.WorkerOutput,0,Error)||R.WorkerOutput>128||(R.WorkerOutput>0&&!R.Outputs.IsEmpty())){Error=TEXT("Invalid worker recipe output");return false;}
        Recipes.Add(R.Id, R);
    }
    if (!ArrayField(Documents[TEXT("buildings")], TEXT("buildings"), Values, Error)) return false;
    int32 CoreDefinitions = 0;
    for (const auto& V : *Values)
    {
        const FObject O = V->AsObject(); FSeigeBuildingDef B;
        if (!StringField(O, TEXT("id"), B.Id, Error) || !StringField(O, TEXT("name"), B.Name, Error) || !StringField(O, TEXT("role"), B.Role, Error) || !StringField(O, TEXT("description"), B.Description, Error) || !StringField(O, TEXT("visual"), B.Visual, Error) || !StringField(O, TEXT("recipe"), B.Recipe, Error) || !Amounts(O,TEXT("extraction_rates"),B.ExtractionRates,Resources,Error) || !ColorField(O, B.Color, Error) || !Amounts(O, TEXT("cost"), B.Cost, Resources, Error)) return false;
        if(!Numeric(O,TEXT("reserved_footprint"),B.ReservedFootprint,UE_DOUBLE_SMALL_NUMBER,Error)||!PositionField(O,TEXT("access_port"),B.AccessPort,Error)||!O->TryGetBoolField(TEXT("deployment_defense"),B.DeploymentDefense))return false;
        if (!IntegerField(O, TEXT("jobs"), B.Jobs, 0, Error) || !Numeric(O, TEXT("health"), B.Health, UE_DOUBLE_SMALL_NUMBER, Error) || !Numeric(O, TEXT("footprint"), B.Footprint, UE_DOUBLE_SMALL_NUMBER, Error) || !Numeric(O, TEXT("storage_capacity"), B.StorageCapacity, UE_DOUBLE_SMALL_NUMBER, Error) || !Numeric(O, TEXT("sensor_range"), B.SensorRange, 0, Error) || !Numeric(O, TEXT("attack_range"), B.AttackRange, 0, Error) || !StringField(O,TEXT("weapon_name"),B.WeaponName,Error) || !Numeric(O,TEXT("damage_per_shot"),B.DamagePerShot,0,Error) || !Numeric(O,TEXT("reload_seconds"),B.ReloadSeconds,0,Error) || !Numeric(O,TEXT("power_usage_kw"),B.PowerUsageKW,0,Error) || !Numeric(O,TEXT("power_generation_kw"),B.PowerGenerationKW,0,Error)) return false;
        if (!Numeric(O,TEXT("construction_seconds"),B.ConstructionSeconds,UE_DOUBLE_SMALL_NUMBER,Error) || !IntegerField(O,TEXT("construction_workers"),B.ConstructionWorkers,1,Error) || !IntegerField(O,TEXT("robot_support_capacity"),B.RobotSupportCapacity,0,Error) || !IntegerField(O,TEXT("staffing_priority"),B.StaffingPriority,0,Error)) return false;
        if(!StringField(O,TEXT("inventory_presentation"),B.InventoryPresentation,Error)||!StringField(O,TEXT("worker_activity"),B.WorkerActivity,Error))return false;
        const TArray<FString> Activities={TEXT("extraction"),TEXT("assembly"),TEXT("handling"),TEXT("inspection"),TEXT("service")};
        if((B.InventoryPresentation!=TEXT("outdoor")&&B.InventoryPresentation!=TEXT("indoor"))||!Activities.Contains(B.WorkerActivity)){Error=TEXT("Invalid building presentation metadata: ")+B.Id;return false;}
        if (InventoryLitres(B.Cost)>B.StorageCapacity || (B.Role!=TEXT("core") && B.Role!=TEXT("service") && B.RobotSupportCapacity>0)) {Error=TEXT("Invalid construction storage or support role: ")+B.Id;return false;}
        if(B.ReservedFootprint<B.Footprint||FMath::Max(FMath::Abs(B.AccessPort.X),FMath::Abs(B.AccessPort.Y))!=1|| (B.DeploymentDefense&&B.Role!=TEXT("core"))){Error=TEXT("Invalid reserved plot, access port or deployment defense");return false;}
        const bool Armed=B.DamagePerShot>0;
        if (O->HasField(TEXT("damage_per_second")) || (Armed && (B.WeaponName.IsEmpty() || B.ReloadSeconds<=0 || B.AttackRange<=0)) || (!Armed && (!B.WeaponName.IsEmpty() || B.ReloadSeconds!=0 || B.AttackRange!=0)))
        { Error=TEXT("Weapon requires consistent name, shot damage, reload and range; DPS is derived: ")+B.Id;return false; }
        if(B.PowerUsageKW!=0||B.PowerGenerationKW!=0){Error=TEXT("Power definitions belong in energy.json; legacy fields must be zero");return false;}
        if(!StringField(O,TEXT("next_upgrade"),B.NextUpgrade,Error)||!Amounts(O,TEXT("upgrade_cost"),B.UpgradeCost,Resources,Error))return false;
        if(!StringField(O,TEXT("workforce_mode"),B.WorkforceMode,Error)||(B.WorkforceMode!=TEXT("full_staff")&&B.WorkforceMode!=TEXT("proportional"))){Error=TEXT("Invalid building workforce mode");return false;}
        if(!StringField(O,TEXT("family"),B.Family,Error)||B.Family.IsEmpty()||!IntegerField(O,TEXT("level"),B.Level,1,Error)||B.Level>3||!Numeric(O,TEXT("recipe_time_multiplier"),B.RecipeTimeMultiplier,1.e-9,Error)||!Numeric(O,TEXT("recipe_energy_multiplier"),B.RecipeEnergyMultiplier,1.e-9,Error)||!Numeric(O,TEXT("recipe_input_multiplier"),B.RecipeInputMultiplier,1.e-9,Error)||!O->TryGetBoolField(TEXT("stores_inactive_workers"),B.StoresInactiveWorkers))return false;
        const TArray<TSharedPtr<FJsonValue>>* Allowed=nullptr;if(!ArrayField(O,TEXT("allowed_recipes"),Allowed,Error))return false;
        for(const auto& Item:*Allowed){FString Id;if(!Item->TryGetString(Id)||!Recipes.Contains(Id)||B.AllowedRecipes.Contains(Id)){Error=TEXT("Invalid selectable recipe");return false;}B.AllowedRecipes.Add(Id);}
        B.DamagePerSecond=Armed?B.DamagePerShot/B.ReloadSeconds:0;
        if (!FMath::IsFinite(B.DamagePerSecond)) {Error=TEXT("Weapon DPS is nonfinite: ")+B.Id;return false;}
        const TArray<FString> Roles = {TEXT("core"), TEXT("extractor"), TEXT("processor"), TEXT("storage"), TEXT("sensor"), TEXT("defense"),TEXT("service"),TEXT("generator"),TEXT("battery"),TEXT("trade"),TEXT("worker_factory"),TEXT("vehicle_factory"),TEXT("wall")};
        if (B.Id.IsEmpty() || BuildingDefs.Contains(B.Id) || !Roles.Contains(B.Role) || (!B.Recipe.IsEmpty() && !Recipes.Contains(B.Recipe))) { Error = TEXT("Invalid building definition: ") + B.Id; return false; }
        if(O->HasField(TEXT("extract_resource"))||O->HasField(TEXT("extract_rate"))){Error=TEXT("Use extraction_rates for deposit-selected mining");return false;}
        for(const auto& Rate:B.ExtractionRates)if(Rate.Value<=0||Resources[Rate.Key].Class==TEXT("manufactured")||Resources[Rate.Key].Discrete){Error=TEXT("Extraction requires positive rates for raw deposits: ")+B.Id;return false;}
        if (B.Role == TEXT("core")) ++CoreDefinitions;
        if ((B.Role == TEXT("extractor")) != (!B.ExtractionRates.IsEmpty()) || (B.Role == TEXT("processor")) != (!B.Recipe.IsEmpty()&&B.Role!=TEXT("worker_factory")&&B.Role!=TEXT("core"))) { Error = TEXT("Building role/capability mismatch: ") + B.Id; return false; }
        if (!B.Recipe.IsEmpty() && ((Sum(Recipes[B.Recipe].Outputs) <= 0 && Recipes[B.Recipe].WorkerOutput == 0) || InventoryLitres(Recipes[B.Recipe].Inputs) > B.StorageCapacity || InventoryLitres(Recipes[B.Recipe].Outputs) > B.StorageCapacity)) { Error = TEXT("Recipe does not fit building storage: ") + B.Id; return false; }
        BuildingDefs.Add(B.Id, B);
    }
    for(const auto& P:BuildingDefs){TSet<FString> Seen;FString Current=P.Key;while(!Current.IsEmpty()){if(Seen.Contains(Current)||!BuildingDefs.Contains(Current)){Error=TEXT("Invalid or cyclic building upgrade chain");return false;}Seen.Add(Current);Current=BuildingDefs[Current].NextUpgrade;}int Parents=0;for(const auto& Other:BuildingDefs)if(Other.Value.NextUpgrade==P.Key)++Parents;if(Parents>1){Error=TEXT("Building upgrade target must have one predecessor");return false;}}
    TSet<int32> CoreLevels;FString CoreFamily;for(const auto& P:BuildingDefs)if(P.Value.Role==TEXT("core")){if((!CoreFamily.IsEmpty()&&CoreFamily!=P.Value.Family)||CoreLevels.Contains(P.Value.Level)){Error=TEXT("Command core levels must belong to one unique family");return false;}CoreFamily=P.Value.Family;CoreLevels.Add(P.Value.Level);}
    if (CoreDefinitions < 1 || CoreDefinitions > 3) { Error = TEXT("One command-core family with up to three levels is required"); return false; }
    if (!ArrayField(Documents[TEXT("buildings")], TEXT("build_menu"), Values, Error)) return false;
    for (const auto& V : *Values)
    {
        FString Id;
        if (!V->TryGetString(Id) || !BuildingDefs.Contains(Id) || BuildingDefs[Id].Role == TEXT("core") || BuildMenu.Contains(Id)) { Error = TEXT("Invalid or duplicated build-menu reference"); return false; }
        BuildMenu.Add(Id);
    }
    if (!ObjectField(Documents[TEXT("policies")], TEXT("policies"), Policy, Error) || !ObjectField(Documents[TEXT("scenario")], TEXT("scenario"), Scenario, Error)) return false;
    const TArray<FString> Positive = {TEXT("fixed_step_seconds"),TEXT("dispatch_interval"),TEXT("courier_capacity"),TEXT("courier_min_batch"),TEXT("delivery_buffer_cycles"),TEXT("repair_health_per_unit"),TEXT("robot_retire_seconds"),TEXT("upkeep_interval"),TEXT("wave_interval"),TEXT("spawn_radius"),TEXT("roam_interval"),TEXT("enemy_health"),TEXT("enemy_speed"),TEXT("extractor_snap_distance")};
    const TArray<FString> Nonnegative = {TEXT("repair_buffer_units"),TEXT("repair_health_per_second"),TEXT("minimum_build_spacing"),TEXT("population_buffer_robots"),TEXT("upkeep_per_robot"),TEXT("upkeep_buffer_intervals"),TEXT("upkeep_shortage_efficiency"),TEXT("objective_produced_amount"),TEXT("objective_survival_seconds"),TEXT("wave_first_time"),TEXT("wave_per_building"),TEXT("wave_per_population"),TEXT("wave_escalation_per_wave"),TEXT("roam_first_time"),TEXT("enemy_attack_range"),TEXT("enemy_damage_per_second"),TEXT("enemy_courier_attack_range"),TEXT("shuttle_capacity")};
    const TArray<FString> Integers = {TEXT("max_couriers"),TEXT("minimum_population"),TEXT("objective_building_count"),TEXT("wave_base_count"),TEXT("wave_max_count"),TEXT("roam_count"),TEXT("event_history_limit"),TEXT("placement_requires_visibility")};
    double Scratch = 0;
    for (const FString& Key : Positive) if (!Numeric(Policy, Key, Scratch, UE_DOUBLE_SMALL_NUMBER, Error)) return false;
    for (const FString& Key : Nonnegative) if (!Numeric(Policy, Key, Scratch, 0, Error)) return false;
    for (const FString& Key : Integers) if (!Numeric(Policy, Key, Scratch, 0, Error, true)) return false;
    if (Number(TEXT("max_couriers")) < 1 || Number(TEXT("event_history_limit")) < 1 || Number(TEXT("wave_max_count")) < Number(TEXT("wave_base_count")) || Number(TEXT("placement_requires_visibility")) > 1 || Number(TEXT("upkeep_shortage_efficiency")) > 1 || Number(TEXT("courier_min_batch")) > Number(TEXT("courier_capacity")) || Number(TEXT("fixed_step_seconds")) > Number(TEXT("dispatch_interval"))) { Error = TEXT("Policy ranges are inconsistent"); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Priorities=nullptr;
    if(!Numeric(Policy,TEXT("delivery_raw_input_buffer_loads"),Scratch,0,Error))return false;
    const TSet<FString> Categories={TEXT("fuel"),TEXT("maintenance"),TEXT("repair"),TEXT("defense"),TEXT("construction"),TEXT("production"),TEXT("trade"),TEXT("reserve"),TEXT("storage")};
    if(!Numeric(Policy,TEXT("delivery_refill_trigger_fraction"),Scratch,UE_DOUBLE_SMALL_NUMBER,Error)||Scratch>1||!ArrayField(Policy,TEXT("delivery_priority_order"),Priorities,Error)||Priorities->Num()!=Categories.Num()){Error=TEXT("Invalid delivery priority or refill policy");return false;}
    DeliveryPriorities.Empty();for(const auto& V:*Priorities){FString Category;if(!V->TryGetString(Category)||!Categories.Contains(Category)||DeliveryPriorities.Contains(Category)){Error=TEXT("Delivery priorities must contain every known category once");return false;}DeliveryPriorities.Add(Category);}
    for (const auto& Pair:BuildingDefs) if (Pair.Value.DamagePerShot>0 && Pair.Value.ReloadSeconds<Number(TEXT("fixed_step_seconds")))
    {Error=TEXT("Weapon reload cannot be shorter than fixed_step_seconds: ")+Pair.Key;return false;}
    const TMap<FString,FString> Selectors = {
        {TEXT("population_policy"),TEXT("fill_open_jobs")},{TEXT("surplus_policy"),TEXT("store_inactive")},
        {TEXT("logistics_policy"),TEXT("local_delivery")},{TEXT("enemy_target_policy"),TEXT("nearest_building")},
        {TEXT("extraction_limit_policy"),TEXT("one_extractor_per_node")},{TEXT("repair_policy"),TEXT("local_materials")},
        {TEXT("objective_policy"),TEXT("survive_and_manufacture")},{TEXT("shuttle_policy"),TEXT("preloaded_cargo_only")},
        {TEXT("storage_policy"),TEXT("overflow_only")},{TEXT("rule_time_basis"),TEXT("simulation_seconds")},
        {TEXT("construction_policy"),TEXT("phased_physical_delivery")},{TEXT("robot_support_policy"),TEXT("local_capacity_and_maintenance")}};
    for (const auto& Pair : Selectors)
    { FString Value; if (!StringField(Policy, Pair.Key, Value, Error)) return false; if (Value != Pair.Value) { Error = TEXT("Unsupported policy ") + Pair.Key + TEXT(": ") + Value; return false; } }
    FString ConstructionSourcePolicy;if(!StringField(Policy,TEXT("construction_source_policy"),ConstructionSourcePolicy,Error)||ConstructionSourcePolicy!=TEXT("surplus_then_largest_load")){Error=TEXT("Unsupported construction source policy");return false;}
    const TArray<TSharedPtr<FJsonValue>>* Stages=nullptr;
    if(!ArrayField(Policy,TEXT("construction_stages"),Stages,Error)||Stages->IsEmpty())return false;
    double StageEnd=0;for(const auto& Stage:*Stages){FString Name;double End=0;if(!StringField(Stage->AsObject(),TEXT("name"),Name,Error)||Name.IsEmpty()||!Numeric(Stage->AsObject(),TEXT("end"),End,StageEnd,Error)||End<=StageEnd||End>1){Error=TEXT("Construction stage fractions must strictly increase to one");return false;}StageEnd=End;}
    if(StageEnd!=1){Error=TEXT("Construction stages must end at one");return false;}
    FString Workforce;
    if (!StringField(Policy, TEXT("workforce_mode"), Workforce, Error) || (Workforce != TEXT("full_staff") && Workforce != TEXT("proportional"))) { Error = TEXT("Unsupported workforce_mode"); return false; }
    for (const FString& Key : {FString(TEXT("repair_resource")),FString(TEXT("upkeep_resource")),FString(TEXT("objective_resource"))})
    { FString Id; if (!StringField(Policy, Key, Id, Error) || !Resources.Contains(Id)) { Error = TEXT("Unknown resource policy: ") + Key; return false; } }
    if(!LoadWorkforce(Error))return false;
    FString PopulationRecipe, ObjectiveBuilding;
    if (!StringField(Policy, TEXT("population_recipe"), PopulationRecipe, Error) || !Recipes.Contains(PopulationRecipe) || !Recipes[PopulationRecipe].Outputs.IsEmpty() || Recipes[PopulationRecipe].WorkerOutput<=0) { Error = TEXT("Population recipe must exist and have no physical outputs"); return false; }
    if (!StringField(Policy, TEXT("objective_building"), ObjectiveBuilding, Error) || !BuildingDefs.Contains(ObjectiveBuilding)) { Error = TEXT("Unknown objective building"); return false; }
    if (!Amounts(Policy, TEXT("core_reserves"), CoreReserves, Resources, Error) || !StringField(Scenario, TEXT("title"), Title, Error) || !StringField(Scenario, TEXT("core_definition"), CoreDefinition, Error) || !Numeric(Scenario, TEXT("world_half_size"), WorldHalfSize, UE_DOUBLE_SMALL_NUMBER, Error) || !IntegerField(Scenario, TEXT("starting_population"), Population, 0, Error)) return false;
    if (!BuildingDefs.Contains(CoreDefinition) || BuildingDefs[CoreDefinition].Role != TEXT("core") || Number(TEXT("spawn_radius")) > WorldHalfSize || Population < Number(TEXT("minimum_population"))) { Error = TEXT("Scenario core, population or spawn bounds are invalid"); return false; }
    bool SupportCanExpand=false;
    const auto& CoreDef=BuildingDefs[CoreDefinition];
    for(const FString& Id:BuildMenu){const auto& D=BuildingDefs[Id];if(D.RobotSupportCapacity>D.Jobs && CoreDef.RobotSupportCapacity>=CoreDef.Jobs+FMath::Max(D.ConstructionWorkers,D.Jobs))SupportCanExpand=true;}
    if(!SupportCanExpand){Error=TEXT("Starter support must leave enough builders and staff to establish a support expansion");return false;}
    int32 Seed = 0; if (!IntegerField(Scenario, TEXT("random_seed"), Seed, 0, Error)) return false; GenerationSeed=SeedOverride==INDEX_NONE?Seed:SeedOverride; Random.Initialize(GenerationSeed);
    FSeigeBuilding B; B.Id = NextId++; B.DefId = CoreDefinition; B.Health = BuildingDefs[CoreDefinition].Health;
    if (!PositionField(Scenario, TEXT("core_position"), B.Position, Error) || !Amounts(Scenario, TEXT("starting_inventory"), B.Inventory, Resources, Error) || !Amounts(Scenario,TEXT("starting_deployment_materials"),B.ConstructionMaterials,Resources,Error)) return false;
    if (!HasAmounts(B.ConstructionMaterials,BuildingDefs[CoreDefinition].Cost) || Sum(B.ConstructionMaterials)!=Sum(BuildingDefs[CoreDefinition].Cost) || Population<BuildingDefs[CoreDefinition].ConstructionWorkers || BuildingDefs[CoreDefinition].RobotSupportCapacity<Population)
    {Error=TEXT("Landing shuttle must carry the complete deployment kit and enough supported builders");return false;}
    B.IsConstructing=true;B.ConstructionProgress=0;B.BuilderPosition=B.Position;B.Status=TEXT("Shuttle deploying command core");
    if (FMath::Abs(B.Position.X) + BuildingDefs[CoreDefinition].ReservedFootprint > WorldHalfSize || FMath::Abs(B.Position.Y) + BuildingDefs[CoreDefinition].ReservedFootprint > WorldHalfSize || Occupied(B) > BuildingDefs[CoreDefinition].StorageCapacity) { Error = TEXT("Starting core stock or position exceeds capacity"); return false; }
    B.SelectedRecipe=CoreDef.Recipe.IsEmpty()?(CoreDef.AllowedRecipes.IsEmpty()?FString():CoreDef.AllowedRecipes[0]):CoreDef.Recipe;
    Buildings.Add(B);
    if (!Amounts(Scenario,TEXT("starting_shuttle_cargo"),ShuttleCargo,Resources,Error) || InventoryMassKg(ShuttleCargo) > Number(TEXT("shuttle_capacity"))) { Error = TEXT("Invalid preloaded shuttle inventory"); return false; }
    FObject Generation;FSeigeResourceGenerationSettings Settings;
    if(!ObjectField(Scenario,TEXT("resource_generation"),Generation,Error)||!IntegerField(Generation,TEXT("standard_count"),Settings.StandardCount,1,Error)||!IntegerField(Generation,TEXT("rare_count"),Settings.RareCount,1,Error)||!Numeric(Generation,TEXT("inner_area_fraction"),Settings.InnerAreaFraction,1.e-9,Error)||!Numeric(Generation,TEXT("minimum_separation_half_size_fraction"),Settings.MinimumSeparationHalfSizeFraction,1.e-9,Error))return false;
    double MineMargin=0;for(const auto& Def:BuildingDefs)if(Def.Value.Role==TEXT("extractor"))MineMargin=FMath::Max(MineMargin,Def.Value.ReservedFootprint+Transport->GetNumberField(TEXT("access_clearance")));
    Settings.CanPlace=[&](FVector2D P){return Environment.CanStand(P,MineMargin*UE_SQRT_2);};
    if(!GenerateSeigeResourceNodes(Resources,GenerationSeed,WorldHalfSize,Nodes,Error,Settings))return false;
    for(auto& N:Nodes)N.Id=NextId++;
    if(!Numeric(Scenario,TEXT("starting_credits"),Credits,0,Error)||Credits!=0){Error=TEXT("Starting credits must be zero; outside trade earns currency");return false;}
    if(!LoadSeigeCalendarRules(Documents[TEXT("calendar")],Calendar,Error)||!Workers.Initialize(Documents[TEXT("workers")],*this,Error)||!Energy.Initialize(Documents[TEXT("energy")],*this,Error)||!Trade.Initialize(Documents[TEXT("trade")],*this,Error)||!Companions.Initialize(RulesPath,*this,Error)||!Walls.Initialize(RulesPath,*this,Error)||!Combat.Initialize(RulesPath,*this,Error))return false;
    RulesFingerprint=FMD5::HashAnsiString(*(RulesFingerprint+Combat.GetFingerprint()));
    for(const auto& P:BuildingDefs){const auto& D=P.Value;if(!D.NextUpgrade.IsEmpty()&&(!BuildingDefs.Contains(D.NextUpgrade)||BuildingDefs[D.NextUpgrade].Role!=D.Role||BuildingDefs[D.NextUpgrade].ReservedFootprint!=D.ReservedFootprint||BuildingDefs[D.NextUpgrade].Family!=D.Family||BuildingDefs[D.NextUpgrade].Level!=D.Level+1||(D.Role!=TEXT("core")&&BuildingDefs[D.NextUpgrade].Footprint!=D.Footprint)||D.UpgradeCost.IsEmpty())){Error=TEXT("Invalid in-place building upgrade");return false;}}
    // Upgraded levels document the cumulative installed bill (previous level
    // cost plus its upgrade_cost); reject drift so placement review, losses and
    // the HUD never disagree with what an upgrade actually charged.
    for(const auto& P:BuildingDefs){const auto& D=P.Value;if(D.NextUpgrade.IsEmpty())continue;const auto& Next=BuildingDefs[D.NextUpgrade];TSet<FString> Ids;for(const auto& C:D.Cost)Ids.Add(C.Key);for(const auto& C:D.UpgradeCost)Ids.Add(C.Key);for(const auto& C:Next.Cost)Ids.Add(C.Key);
        for(const FString& Id:Ids)if(FMath::Abs(Next.Cost.FindRef(Id)-D.Cost.FindRef(Id)-D.UpgradeCost.FindRef(Id))>1.e-6){Error=FString::Printf(TEXT("Cumulative cost of %s in %s must equal %s cost plus upgrade_cost"),*Id,*Next.Id,*D.Id);return false;}}
    TSet<FString> Renewable;
    for (const FSeigeNode& N : Nodes) for (const FString& Id : BuildMenu) if (BuildingDefs[Id].ExtractionRates.Contains(N.Resource)) Renewable.Add(N.Resource);
    // A working external trade port exchanges physically exported local goods for missing types.
    if(!Trade.Prices.IsEmpty()&&!Renewable.IsEmpty())for(const auto& P:Trade.Prices)Renewable.Add(P.Key);
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
    // Catalogue reachability without the paid-import shortcut: every raw type has a
    // buildable extractor and the recurring requirements follow from buildable and
    // core recipes alone. This mirrors Tools/validate_rules.mjs so the native check
    // cannot be satisfied by the trade port on its own.
    TSet<FString> Catalogue;
    for (const auto& Pair : Resources) if (Pair.Value.Tier == 0)
    {
        bool Extractable = false;
        for (const FString& Id : BuildMenu) if (BuildingDefs[Id].ExtractionRates.FindRef(Pair.Key) > 0) Extractable = true;
        if (!Extractable) { Error = TEXT("No buildable extractor for raw resource: ") + Pair.Key; return false; }
        Catalogue.Add(Pair.Key);
    }
    TArray<FString> Usable;
    for (const auto& Pair : BuildingDefs) if (BuildMenu.Contains(Pair.Key) || Pair.Value.Role == TEXT("core"))
    { if (!Pair.Value.Recipe.IsEmpty()) Usable.AddUnique(Pair.Value.Recipe); for (const FString& R : Pair.Value.AllowedRecipes) Usable.AddUnique(R); }
    for (int32 Before = -1; Before != Catalogue.Num();)
    {
        Before = Catalogue.Num();
        for (const FString& RecipeId : Usable)
        {
            const FSeigeRecipeDef* R = Recipes.Find(RecipeId); if (!R) continue; bool Reachable = true;
            for (const auto& Pair : R->Inputs) if (!Catalogue.Contains(Pair.Key)) Reachable = false;
            if (!Reachable) continue;
            for (const auto& Pair : R->Outputs) Catalogue.Add(Pair.Key);
            if (R->WorkerOutput > 0 && Resources.Contains(TEXT("stored_workers"))) Catalogue.Add(TEXT("stored_workers"));
        }
    }
    for (const FString& Id : Required) if (!Catalogue.Contains(Id)) { Error = TEXT("No recipe path from raw deposits to recurring requirement: ") + Id; return false; }
    for (const auto& Pair : BuildingDefs)
    {
        const FSeigeBuildingDef& D = Pair.Value;
        double Buffer = Number(TEXT("repair_buffer_units"))*Resources[TextRule(TEXT("repair_resource"))].LitresPerUnit;
        if (!D.Recipe.IsEmpty()){FSeigeBuilding Probe;Probe.DefId=D.Id;for(const auto& Input:Recipes[D.Recipe].Inputs)Buffer+=ProductionInputBuffer(Probe,D.Recipe,Input.Key)*Resources[Input.Key].LitresPerUnit;}
        if (D.Role == TEXT("core")) Buffer += InventoryLitres(CoreReserves) + InventoryLitres(Recipes[PopulationRecipe].Inputs) * Number(TEXT("population_buffer_robots"));
        Buffer+=D.RobotSupportCapacity*Number(TEXT("upkeep_per_robot"))*Number(TEXT("upkeep_buffer_intervals"))*Resources[TextRule(TEXT("upkeep_resource"))].LitresPerUnit;
        if (Buffer > D.StorageCapacity) { Error = TEXT("Demand buffers exceed building capacity: ") + D.Id; return false; }
    }
    NextWaveTime = Number(TEXT("wave_first_time")); NextRoamTime = Number(TEXT("roam_first_time"));
    AllocateWorkers(); AddEvent(TEXT("First landing. Establish extraction, industry and local defenses.")); Error.Empty(); return true;
}

bool FSeigeSimulation::GenerateResourceNodesForSeed(int32 Seed,TArray<FSeigeNode>& OutNodes,FString& Error,FVector2D WorldOffset) const
{
    if(!Scenario){Error=TEXT("Initialize scenario rules before generating regional deposits");return false;}
    const auto G=Scenario->GetObjectField(TEXT("resource_generation"));FSeigeResourceGenerationSettings Settings;
    Settings.StandardCount=int32(G->GetNumberField(TEXT("standard_count")));Settings.RareCount=int32(G->GetNumberField(TEXT("rare_count")));Settings.InnerAreaFraction=G->GetNumberField(TEXT("inner_area_fraction"));Settings.MinimumSeparationHalfSizeFraction=G->GetNumberField(TEXT("minimum_separation_half_size_fraction"));
    FSeigeEnvironment RegionEnvironment=Environment;RegionEnvironment.WorldOffset=WorldOffset;
    double Margin=0;for(const auto& Def:BuildingDefs)if(Def.Value.Role==TEXT("extractor"))Margin=FMath::Max(Margin,Def.Value.ReservedFootprint+Transport->GetNumberField(TEXT("access_clearance")));
    Settings.CanPlace=[&](FVector2D P){return RegionEnvironment.CanStand(P,Margin*UE_SQRT_2);};
    return GenerateSeigeResourceNodes(Resources,Seed,WorldHalfSize,OutNodes,Error,Settings);
}
double FSeigeSimulation::Number(const FString& Key) const { return Policy->GetNumberField(Key); }
bool FSeigeSimulation::CanSetInitialCorePosition(FVector2D Position, FString& Error) const
{
    const FSeigeBuilding* Command = Core();
    if (!Policy || !Command || Time != 0 || Buildings.Num() != 1)
    { Error = TEXT("Core placement is only available before a new colony begins"); return false; }
    const double Radius = BuildingDefs[CoreDefinition].ReservedFootprint+Transport->GetNumberField(TEXT("access_clearance"));
    if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || FMath::Abs(Position.X) + Radius > WorldHalfSize || FMath::Abs(Position.Y) + Radius > WorldHalfSize)
    { Error = TEXT("Command core footprint must fit inside the sector"); return false; }
    if(!Environment.CanStand(Position,Radius*UE_SQRT_2)){Error=TEXT("Command core and its reserved plot require dry land");return false;}
    for (const FSeigeNode& Node : Nodes)
    {
        double ExtractorRadius = 0;
        for (const FString& Id : BuildMenu)
            if (BuildingDefs[Id].ExtractionRates.Contains(Node.Resource)) ExtractorRadius = FMath::Max(ExtractorRadius, BuildingDefs[Id].ReservedFootprint);
        if (FMath::Abs(Position.X-Node.Position.X)<Radius+ExtractorRadius+Number(TEXT("minimum_build_spacing"))&&FMath::Abs(Position.Y-Node.Position.Y)<Radius+ExtractorRadius+Number(TEXT("minimum_build_spacing")))
        { Error = TEXT("The command core would obstruct a resource deposit"); return false; }
    }
    Error.Empty(); return true;
}
bool FSeigeSimulation::SetInitialCorePosition(FVector2D Position, FString& Error)
{
    if (!CanSetInitialCorePosition(Position, Error)) return false;
    const auto Delta=Position-Core()->Position;Core()->Position = Position; Core()->BuilderPosition=Position;Companions.ShiftHome(Delta);Combat.ShiftHome(Delta);Workers.ShiftHome(Delta); ++TransportRevision;Energy.Invalidate();Energy.Tick(*this,0);return true;
}
FString FSeigeSimulation::TextRule(const FString& Key) const { return Policy->GetStringField(Key); }
const FSeigeBuildingDef* FSeigeSimulation::Definition(const FSeigeBuilding& B) const { return BuildingDefs.Find(B.DefId); }
const FSeigeNode* FSeigeSimulation::ExtractionNode(const FString& Id,FVector2D Position) const
{
    const auto* D=BuildingDefs.Find(Id);if(!Policy||!D||D->Role!=TEXT("extractor"))return nullptr;
    const FSeigeNode* Best=nullptr;double Distance=FMath::Square(Number(TEXT("extractor_snap_distance")));
    for(const auto& Node:Nodes)if(D->ExtractionRates.Contains(Node.Resource))
    {
        const double Candidate=FVector2D::DistSquared(Position,Node.Position);
        if(Candidate<Distance||(Candidate==Distance&&(!Best||Node.Id<Best->Id))){Best=&Node;Distance=Candidate;}
    }
    return Best;
}
FString FSeigeSimulation::ExtractionResource(const FSeigeBuilding& B) const
{
    const auto* Node=ExtractionNode(B.DefId,B.Position);
    return Node&&Node->Id==B.DepositId?Node->Resource:FString();
}
double FSeigeSimulation::ExtractionRate(const FSeigeBuilding& B) const
{const auto* D=Definition(B);return D?D->ExtractionRates.FindRef(ExtractionResource(B)):0;}
FSeigeBuilding* FSeigeSimulation::FindBuilding(int32 Id) { return Buildings.FindByPredicate([Id](const FSeigeBuilding& B){return B.Id == Id;}); }
const FSeigeBuilding* FSeigeSimulation::FindBuilding(int32 Id) const { return Buildings.FindByPredicate([Id](const FSeigeBuilding& B){return B.Id == Id;}); }
FSeigeBuilding* FSeigeSimulation::Core() { return Buildings.FindByPredicate([this](const FSeigeBuilding& B){return B.DefId == CoreDefinition;}); }
const FSeigeBuilding* FSeigeSimulation::Core() const { return Buildings.FindByPredicate([this](const FSeigeBuilding& B){return B.DefId == CoreDefinition;}); }
double FSeigeSimulation::InventoryLitres(const TMap<FString,double>& Stock) const {double N=0;for(const auto& P:Stock)if(const auto* R=Resources.Find(P.Key))N+=P.Value*R->LitresPerUnit;return N;}
double FSeigeSimulation::InventoryMassKg(const TMap<FString,double>& Stock) const {double N=0;for(const auto& P:Stock)if(const auto* R=Resources.Find(P.Key))N+=P.Value*R->UnitMassKg;return N;}
double FSeigeSimulation::Occupied(const FSeigeBuilding& B) const { double N=InventoryLitres(B.Inventory)+InventoryLitres(B.ConstructionMaterials)+B.ProductionReservedLitres+B.DisassemblyReservedLitres;if(!B.Shipment.Resource.IsEmpty()){const auto* R=Resources.Find(B.Shipment.Resource);if(R)N+=(B.Shipment.Buy?B.Shipment.Quantity:B.Shipment.GoodsEscrow)*R->LitresPerUnit;}return N; }
double FSeigeSimulation::StorageRoom(const FSeigeBuilding& B,int32 ArrivingCourierId,const FString& ArrivingWorkerId) const
{
    double Committed=Occupied(B);
    for(const auto& C:Couriers)if(C.Id!=ArrivingCourierId&&C.TargetId==B.Id&&C.RoadTargetId==0&&C.Phase!=TEXT("done"))
        Committed+=(C.Amount+C.ReservedAmount+(C.SelfTransfer?1:0))*Resources[C.Resource].LitresPerUnit;
    const double Berth=Resources[TextRule(TEXT("inactive_worker_resource"))].LitresPerUnit;
    for(const auto& W:Workers.Bodies)if(W.Id!=ArrivingWorkerId&&W.State==TEXT("active")&&W.ContainerId==B.Id)
    {if(W.Activity==TEXT("to_store")||W.Activity==TEXT("store"))Committed+=Berth;else if(W.Activity==TEXT("to_recycle"))Committed+=InventoryLitres(DisassemblyOutputs());}
    const auto* D=Definition(B);return D?FMath::Max(0.,D->StorageCapacity-Committed):0.;
}
const TMap<FString,double>& FSeigeSimulation::ConstructionCost(const FSeigeBuilding& B) const {if(!B.UpgradeTarget.IsEmpty())return Definition(B)->UpgradeCost;for(const auto& P:BuildingDefs)if(P.Value.NextUpgrade==B.DefId)return P.Value.UpgradeCost;return Definition(B)->Cost;}
double FSeigeSimulation::ConstructionSeconds(const FSeigeBuilding& B) const {return BuildingDefs[B.UpgradeTarget.IsEmpty()?B.DefId:B.UpgradeTarget].ConstructionSeconds;}
int32 FSeigeSimulation::RequiredBuilders(const FSeigeBuilding& B) const {return BuildingDefs[B.UpgradeTarget.IsEmpty()?B.DefId:B.UpgradeTarget].ConstructionWorkers;}
FString FSeigeSimulation::BaseBlueprint(const FString& Id) const
{
    FString Current=Id;
    for(int32 Guard=0;Guard<16;++Guard){const FSeigeBuildingDef* Parent=nullptr;for(const auto& P:BuildingDefs)if(P.Value.NextUpgrade==Current){Parent=&P.Value;break;}if(!Parent)break;Current=Parent->Id;}
    return Current;
}
TMap<FString,double> FSeigeSimulation::PreviousLevelBill(const FString& Id) const
{
    TMap<FString,double> Bill;FString Current=Id;
    while(true){const FSeigeBuildingDef* Parent=nullptr;for(const auto& P:BuildingDefs)if(P.Value.NextUpgrade==Current){Parent=&P.Value;break;}if(!Parent)break;const FSeigeBuildingDef* Earlier=nullptr;for(const auto& P:BuildingDefs)if(P.Value.NextUpgrade==Parent->Id){Earlier=&P.Value;break;}const auto& Cost=Earlier?Earlier->UpgradeCost:Parent->Cost;for(const auto& P:Cost)Bill.FindOrAdd(P.Key)+=P.Value;Current=Parent->Id;}
    return Bill;
}
bool FSeigeSimulation::CanUpgradeBuilding(int32 Id,FString& Error) const
{
    const auto* B=FindBuilding(Id);const auto* D=B?Definition(*B):nullptr;
    if(!B||!D||B->Health<=0||B->IsConstructing||D->NextUpgrade.IsEmpty()||!BuildingDefs.Contains(D->NextUpgrade)||Escaped||Failed){Error=TEXT("Building has no available upgrade");return false;}
    if(!B->Shipment.Resource.IsEmpty()||B->ProductionCommitted||B->DisassemblyQueued>0){Error=TEXT("Finish the current shipment or production batch before upgrading");return false;}
    if(Couriers.ContainsByPredicate([&](const auto& C){return C.TargetId==Id&&C.RoadTargetId==0&&C.Phase!=TEXT("done");})){Error=TEXT("Finish incoming deliveries before upgrading");return false;}
    double AdditionalLitres=0;
    for(const auto& P:D->UpgradeCost)
    {
        if(ConstructionAvailable(P.Key)+1.e-8<P.Value){Error=TEXT("Insufficient upgrade materials");return false;}
        const double Local=FMath::Max(0.,B->Inventory.FindRef(P.Key)-Workers.PickupReserved(*this,Id,P.Key));
        AdditionalLitres+=FMath::Max(0.,P.Value-Local)*Resources[P.Key].LitresPerUnit;
    }
    // Local stock is already counted. Only the missing part of the bill needs
    // additional room; packed-worker arrivals keep their own promised berths.
    if(AdditionalLitres>StorageRoom(*B)+1.e-8){Error=TEXT("Clear storage space for upgrade materials");return false;}
    Error.Empty();return true;
}
bool FSeigeSimulation::UpgradeBuilding(int32 Id,FString& Error)
{if(!CanUpgradeBuilding(Id,Error))return false;auto* B=FindBuilding(Id);B->UpgradeTarget=Definition(*B)->NextUpgrade;for(const auto& P:B->InstalledMaterials)B->PreviousLevelMaterials.FindOrAdd(P.Key)+=P.Value;B->InstalledMaterials.Empty();for(const auto& P:Definition(*B)->UpgradeCost){const double Local=FMath::Min(P.Value,FMath::Max(0.,B->Inventory.FindRef(P.Key)-Workers.PickupReserved(*this,Id,P.Key)));B->Inventory.FindOrAdd(P.Key)-=Local;B->ConstructionMaterials.FindOrAdd(P.Key)+=Local;}B->IsConstructing=true;B->ConstructionProgress=0;B->BuildersOnSite=B->TravellingBuilders=0;B->BuilderPosition=BuildingAccessPoint(*Core());++TransportRevision;AllocateWorkers();AddEvent(TEXT("Building upgrade queued"));return true;}
double FSeigeSimulation::ConstructionReserved(const FString& Resource) const
{
    double Total=0;
    for(const FSeigeBuilding& B:Buildings) if(B.Health>0 && B.IsConstructing)
    {
        double InTransit=0;for(const FSeigeCourier& C:Couriers)if(C.ForConstruction&&C.TargetId==B.Id&&C.Resource==Resource)InTransit+=C.Amount+C.ReservedAmount;
        Total+=FMath::Max(0.,ConstructionCost(B).FindRef(Resource)-B.InstalledMaterials.FindRef(Resource)-B.ConstructionMaterials.FindRef(Resource)-InTransit);
    }
    for(const auto& R:Roads)if(R.Health>0&&R.IsConstructing)Total+=FMath::Max(0.,RoadCost(R.A,R.B,R.TargetTier).FindRef(Resource)-R.InstalledMaterials.FindRef(Resource)-R.ConstructionMaterials.FindRef(Resource)-IncomingRoad(R.Id,Resource));
    return Total;
}
double FSeigeSimulation::Spendable(const FSeigeBuilding& B,const FString& Resource) const
{
    double Reserved=ConstructionReserved(Resource);
    for(const auto& Source:Buildings)if(Source.Health>0&&!Source.IsConstructing)
    {
        // Placement already excluded operating buffers when accepting the
        // bill. Once promised, its remaining stock must stay reserved across
        // successive production batches; replenishable demand is not another
        // source of uncommitted material.
        const double Available=FMath::Max(0.,Source.Inventory.FindRef(Resource)-Workers.PickupReserved(*this,Source.Id,Resource));
        if(Source.Id==B.Id)return FMath::Max(0.,B.Inventory.FindRef(Resource)-FMath::Min(Available,Reserved)-Workers.PickupReserved(*this,B.Id,Resource));
        Reserved=FMath::Max(0.,Reserved-Available);
    }
    return FMath::Max(0.,B.Inventory.FindRef(Resource)-Workers.PickupReserved(*this,B.Id,Resource));
}
bool FSeigeSimulation::HasSpendable(const FSeigeBuilding& B,const TMap<FString,double>& Amounts) const
{for(const auto& P:Amounts)if(Spendable(B,P.Key)+UE_DOUBLE_SMALL_NUMBER<P.Value)return false;return true;}
double FSeigeSimulation::ConstructionAvailable(const FString& Resource) const
{const auto* C=Core();if(!C||C->IsConstructing||C->Health<=0)return 0;double Available=0;for(const auto& B:Buildings)if(B.Health>0&&!B.IsConstructing)Available+=FMath::Max(0.,B.Inventory.FindRef(Resource)-Workers.PickupReserved(*this,B.Id,Resource)-Demand(B,Resource,false));return FMath::Max(0.,Available-ConstructionReserved(Resource));}
double FSeigeSimulation::OperatingBuffer(const FString& Resource) const
{double Total=0;for(const auto& B:Buildings)if(B.Health>0&&!B.IsConstructing)Total+=Demand(B,Resource,true);return Total;}
double FSeigeSimulation::Incoming(int32 Target, const FString& Resource) const
{ double Amount = 0; for (const FSeigeCourier& C : Couriers) if (C.RoadTargetId==0 && C.TargetId == Target && (Resource.IsEmpty() || Resource == C.Resource)) Amount += C.Amount+C.ReservedAmount+(C.SelfTransfer?1:0); return Amount; }
double FSeigeSimulation::WorkFraction(const FSeigeBuilding& B) const
{
    const FSeigeBuildingDef* D = Definition(B);
    if (!D || !B.Enabled || B.Health <= 0 || B.IsConstructing) return 0;
    const double Efficiency=WorkforceEfficiency*Energy.Fraction(B.Id)*Companions.EfficiencyAt(B.Position);
    if (D->Jobs == 0) return Efficiency;
    if (D->WorkforceMode == TEXT("full_staff")) return B.Workers >= D->Jobs ? Efficiency : 0;
    return Efficiency * static_cast<double>(B.Workers) / D->Jobs;
}
double FSeigeSimulation::TotalStock(const FString& Resource) const
{
    double Amount = 0;
    for (const FSeigeBuilding& B : Buildings) if (B.Health > 0) Amount += B.Inventory.FindRef(Resource)+B.ConstructionMaterials.FindRef(Resource)+B.ProductionInputs.FindRef(Resource)+(B.DisassemblyCommitted&&Resource==TextRule(TEXT("inactive_worker_resource"))?1:0)+(B.Shipment.Resource==Resource?B.Shipment.GoodsEscrow:0);
    for(const auto& R:Roads)if(R.Health>0)Amount+=R.ConstructionMaterials.FindRef(Resource);
    for (const FSeigeCourier& C : Couriers) if (C.Resource == Resource) Amount += C.Amount;
    return Amount+Combat.CargoStock(Resource)+Workers.DeploymentStock.FindRef(Resource);
}
bool FSeigeSimulation::IsVisible(FVector2D P) const
{
    // Sensor coverage needs power and, for staffed sensors, an assigned operator.
    // The command core keeps its coverage while powered even when its crew is
    // out building or hauling (persistent workers leave the hull physically), and
    // a deploying core covers its surroundings as soon as it has power.
    for (const FSeigeBuilding& B : Buildings)
    {
        const FSeigeBuildingDef* D = Definition(B); if (!D || D->SensorRange <= 0 || B.Health <= 0 || !B.Enabled) continue;
        const bool Powered = Energy.Fraction(B.Id) > 0;
        const bool Staffed = D->Jobs == 0 || B.Workers > 0 || D->Role == TEXT("core");
        const bool Active = B.IsConstructing ? (D->DeploymentDefense && Powered) : (Powered && Staffed && WorkforceEfficiency > 0);
        if (Active && FVector2D::Distance(P, B.Position) <= D->SensorRange) return true;
    }
    return Combat.IsVisible(P);
}
bool FSeigeSimulation::CanPlaceBuilding(const FString& Id, FVector2D P, FString& Error) const
{
    const FSeigeBuildingDef* D = BuildingDefs.Find(Id);
    const FSeigeBuilding* C = Core();
    if (!Policy || Escaped || Failed || !C || C->Health <= 0) { Error = TEXT("Colony is no longer under your command"); return false; }
    if(C->IsConstructing){Error=TEXT("Wait for the landing shuttle to finish deploying the command core");return false;}
    if (!D || !BuildMenu.Contains(Id)) { Error = TEXT("Definition is not available in the build menu"); return false; }
    if (D->Role == TEXT("core")) { Error = TEXT("Only one command core is allowed"); return false; }
    if (Number(TEXT("placement_requires_visibility")) > 0 && !IsVisible(P)) { Error = TEXT("Outside live sensor coverage; extend your sensors first"); return false; }
    if(!CanPlaceBuildingGeometry(Id,P,Error))return false;
    for(const auto& Cost:D->Cost)if(ConstructionAvailable(Cost.Key)+UE_DOUBLE_SMALL_NUMBER<Cost.Value)
    { Error = TEXT("Insufficient unreserved construction materials"); return false; }
    Error.Empty();return true;
}
bool FSeigeSimulation::CanPlaceBuildingGeometry(const FString& Id,FVector2D P,FString& Error) const
{
    const auto* D=BuildingDefs.Find(Id);if(!D||!BuildMenu.Contains(Id)||D->Role==TEXT("core")){Error=TEXT("Invalid building definition");return false;}
    const double PlotMargin=D->ReservedFootprint+Transport->GetNumberField(TEXT("access_clearance"));
    if (!FMath::IsFinite(P.X) || !FMath::IsFinite(P.Y) || FMath::Abs(P.X) + PlotMargin > WorldHalfSize || FMath::Abs(P.Y) + PlotMargin > WorldHalfSize) { Error = TEXT("Reserved plot and access port must fit inside the sector boundary"); return false; }
    if(!Environment.CanStand(P,PlotMargin*UE_SQRT_2)){Error=TEXT("Reserved building plot and access port require dry land");return false;}
    for (const FSeigeBuilding& B : Buildings)
        if (B.Health > 0 && FMath::Abs(P.X-B.Position.X) < D->ReservedFootprint + Definition(B)->ReservedFootprint + Number(TEXT("minimum_build_spacing")) && FMath::Abs(P.Y-B.Position.Y) < D->ReservedFootprint + Definition(B)->ReservedFootprint + Number(TEXT("minimum_build_spacing"))) { Error = TEXT("Too close to another building"); return false; }
    // Plot reservation is immediate. Do not enclose a moving worker or vehicle
    // before its next routing step can avoid the new obstacle.
    const double WorkerClearance=Transport->GetNumberField(TEXT("path_clearance"));
    auto OccupiesPlot=[&](FVector2D At,double Clearance){return FMath::Abs(P.X-At.X)<D->ReservedFootprint+Clearance&&FMath::Abs(P.Y-At.Y)<D->ReservedFootprint+Clearance;};
    for(const auto& Worker:Workers.Bodies)if(Worker.State==TEXT("active")&&Worker.Outdoor&&OccupiesPlot(Worker.Position,Workers.BodyRadiusMeters()/MetersPerWorldUnit()))
    {Error=TEXT("Wait for the worker to leave this reserved plot");return false;}
    for(const auto& Courier:Couriers)if(OccupiesPlot(Courier.Position,WorkerClearance))
    {Error=TEXT("Wait for the delivery worker to leave this reserved plot");return false;}
    for(const auto& B:Buildings)if(B.Health>0&&((B.TravellingBuilders>0&&OccupiesPlot(B.BuilderPosition,WorkerClearance))||(B.BuildersOnSite>0&&OccupiesPlot(BuildingAccessPoint(B),WorkerClearance))))
    {Error=TEXT("Wait for the construction crew to leave this reserved plot");return false;}
    for(const auto& R:Roads)if(R.Health>0&&((R.TravellingBuilders>0&&OccupiesPlot(R.BuilderPosition,WorkerClearance))||(R.BuildersOnSite>0&&OccupiesPlot(RoadAccessPoint(R),WorkerClearance))))
    {Error=TEXT("Wait for the road crew to leave this reserved plot");return false;}
    for(const auto& V:Combat.Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4&&OccupiesPlot(V.Position,Combat.Chassis[V.ChassisId].RadiusMeters/MetersPerWorldUnit()))
    {Error=TEXT("Move the vehicle outside this reserved plot first");return false;}
    for(const auto& R:Roads)if(R.Health>0)
    {const FString Tier=R.IsConstructing?R.TargetTier:R.Tier;const double Width=TransportTiers[Tier].WidthMeters*.5/MetersPerWorldUnit();const FVector2D Delta=R.B-R.A;const double T=FMath::Clamp(FVector2D::DotProduct(P-R.A,Delta)/Delta.SizeSquared(),0.,1.);const FVector2D Nearest=R.A+Delta*T;if(FMath::Abs(P.X-Nearest.X)<D->ReservedFootprint+Width && FMath::Abs(P.Y-Nearest.Y)<D->ReservedFootprint+Width){Error=TEXT("Reserved plot would obstruct a road corridor");return false;}}
    if (D->Role==TEXT("extractor"))
    {
        const FSeigeNode* Selected=ExtractionNode(Id,P);
        if(!Selected){Error=TEXT("Place the Extraction Mine on a supported resource deposit");return false;}
        for(const auto& B:Buildings)if(B.Health>0&&B.DepositId==Selected->Id){Error=TEXT("This deposit already has an Extraction Mine");return false;}
    }
    Error.Empty(); return true;
}
bool FSeigeSimulation::PlaceBuilding(const FString& Id, FVector2D P, FString& Error)
{
    if (!CanPlaceBuilding(Id, P, Error)) return false;
    const FSeigeBuildingDef& D = BuildingDefs[Id];
    FSeigeBuilding B; B.Id = NextId++; B.DefId = Id; B.Position = P; B.Health = D.Health; B.Status = TEXT("Construction materials reserved; awaiting couriers");
    if(const auto* Node=ExtractionNode(Id,P))B.DepositId=Node->Id;
    B.SelectedRecipe=D.Recipe.IsEmpty()?(D.AllowedRecipes.IsEmpty()?FString():D.AllowedRecipes[0]):D.Recipe;
    B.WorkerExportTarget=D.Role==TEXT("trade")?int32(Number(TEXT("default_port_worker_reserve_target"))):0;
    B.IsConstructing=true;B.ConstructionProgress=0;B.BuilderPosition=BuildingAccessPoint(*Core());
    Buildings.Add(B);++TransportRevision;
    if(!FindRoute(Buildings.Last().BuilderPosition,BuildingAccessPoint(Buildings.Last()),Buildings.Last().BuilderRoute)){Buildings.Pop();Error=TEXT("Workers cannot reach the building access port");return false;} AllocateWorkers(); AddEvent(D.Name + TEXT(" construction queued")); return true;
}
int32 FSeigeSimulation::AddReviewBuilding(const FString& Id, FVector2D P)
{
    const FSeigeBuildingDef* D=BuildingDefs.Find(Id);if(!D)return 0;
    FSeigeBuilding B;B.Id=NextId++;B.DefId=Id;B.Position=P;B.Health=D->Health;B.Status=TEXT("Review placement");
    B.SelectedRecipe=D->Recipe.IsEmpty()?(D->AllowedRecipes.IsEmpty()?FString():D->AllowedRecipes[0]):D->Recipe;
    B.IsConstructing=false;B.ConstructionProgress=1;B.InstalledMaterials=D->Cost;
    Buildings.Add(B);++TransportRevision;return B.Id;
}
bool FSeigeSimulation::AddReviewStoredWorkers(int32 Id,int32 Count)
{
    FSeigeBuilding* B=FindBuilding(Id);const FSeigeBuildingDef* D=B?Definition(*B):nullptr;
    if(!D||!D->StoresInactiveWorkers||B->Health<=0||B->IsConstructing||Count<=0||Count>16)return false;
    B->Inventory.FindOrAdd(TextRule(TEXT("inactive_worker_resource")))+=Count;B->WorkerExportTarget+=Count;
    Workers.NewStored(*this,Id,Count);return true;
}
void FSeigeSimulation::ToggleBuilding(int32 Id)
{
    if (Escaped || Failed) return;
    if (FSeigeBuilding* B = FindBuilding(Id))
        if (B->Health > 0 && B->DefId != CoreDefinition) { B->Enabled = !B->Enabled; B->Status = B->Enabled ? TEXT("Enabled") : TEXT("Disabled"); AllocateWorkers(); }
}
void FSeigeSimulation::AllocateWorkers()
{ Workers.RefreshMetrics(*this); }
void FSeigeSimulation::UpdateSupport()
{
    RobotSupportCapacity=0;SupportedPopulation=0;int32 Remaining=Population,Maintained=0;
    for(FSeigeBuilding& B:Buildings)
    {
        B.SupportedRobots=0;const auto& D=*Definition(B);
        if(B.Health<=0||!B.Enabled||B.IsConstructing||(D.WorkforceMode==TEXT("full_staff")?B.Workers<D.Jobs:D.Jobs>0&&B.Workers==0)||Energy.Fraction(B.Id)<=0||D.RobotSupportCapacity<=0)continue;
        RobotSupportCapacity+=D.RobotSupportCapacity;B.SupportedRobots=FMath::Min(Remaining,D.RobotSupportCapacity);
        Remaining-=B.SupportedRobots;SupportedPopulation+=B.SupportedRobots;if(B.MaintenanceSupplied)Maintained+=B.SupportedRobots;
    }
    const auto* C=Core();
    if(C&&C->Health>0&&C->IsConstructing){RobotSupportCapacity=Definition(*C)->RobotSupportCapacity;SupportedPopulation=FMath::Min(Population,RobotSupportCapacity);Maintained=SupportedPopulation;}
    WorkforceEfficiency=Population>0?(Maintained+(Population-Maintained)*Number(TEXT("upkeep_shortage_efficiency")))/Population:1.;
}
FString FSeigeSimulation::ConstructionStage(const FSeigeBuilding& B) const
{
    if(!B.IsConstructing)return TEXT("Complete");
    for(const auto& V:Policy->GetArrayField(TEXT("construction_stages")))if(B.ConstructionProgress<V->AsObject()->GetNumberField(TEXT("end")))return V->AsObject()->GetStringField(TEXT("name"));
    return TEXT("Complete");
}
double FSeigeSimulation::ConstructionPhaseProgress(const FSeigeBuilding& B,int32 Index) const
{
    const auto& Stages=Policy->GetArrayField(TEXT("construction_stages"));if(!Stages.IsValidIndex(Index))return 0;
    const double Start=Index?Stages[Index-1]->AsObject()->GetNumberField(TEXT("end")):0,End=Stages[Index]->AsObject()->GetNumberField(TEXT("end"));
    return FMath::Clamp((B.ConstructionProgress-Start)/(End-Start),0.,1.);
}
void FSeigeSimulation::UpdateConstructionCrew(FVector2D Destination,int32 Assigned,int32& OnSite,int32& Travelling,FVector2D& Position,TArray<FVector2D>& Route,int32& Next,int32& RouteRevision,double Seconds,bool Deployment)
{
    OnSite=FMath::Min(OnSite,Assigned);Travelling=FMath::Min(Travelling,Assigned-OnSite);
    if(Deployment){OnSite=Assigned;Travelling=0;Position=Destination;return;}
    if(Assigned<=0){OnSite=Travelling=0;Route.Empty();Next=0;return;}
    if(Travelling==0&&Assigned>OnSite)
    {
        const auto* Command=Core();if(!Command||Command->Health<=0)return;
        Position=BuildingAccessPoint(*Command);Next=0;RouteRevision=TransportRevision;
        if(FindRoute(Position,Destination,Route))Travelling=Assigned-OnSite;
    }
    if(Travelling<=0)return;
    if(RouteRevision!=TransportRevision){Next=0;FindRoute(Position,Destination,Route);RouteRevision=TransportRevision;}
    if(WalkRoute(Position,Route,Next,Seconds)){OnSite+=Travelling;Travelling=0;}
}
void FSeigeSimulation::StepConstruction(double Seconds)
{
    for(FSeigeBuilding& B:Buildings)
    {
        if(!B.IsConstructing||B.Health<=0){B.BuildersOnSite=B.TravellingBuilders=0;continue;}const auto& D=*Definition(B);
        if(!B.Enabled){B.Status=TEXT("Construction paused; materials remain reserved");continue;}
        if(B.Builders<=0){B.Status=TEXT("Waiting for construction workers");continue;}
        if(B.BuildersOnSite<=0){B.Status=TEXT("Workers travelling to the construction site");continue;}
        double Delta=FMath::Min(1-B.ConstructionProgress,Seconds*WorkforceEfficiency*B.BuildersOnSite/(ConstructionSeconds(B)*RequiredBuilders(B)));
        for(const auto& P:ConstructionCost(B))if(P.Value>0)Delta=FMath::Min(Delta,B.ConstructionMaterials.FindRef(P.Key)/P.Value);
        if(Delta<=UE_DOUBLE_SMALL_NUMBER){B.Status=TEXT("Workers awaiting delivered construction materials");continue;}
        for(const auto& P:ConstructionCost(B)){const double Amount=P.Value*Delta;B.ConstructionMaterials.FindOrAdd(P.Key)=FMath::Max(0.,B.ConstructionMaterials.FindRef(P.Key)-Amount);B.InstalledMaterials.FindOrAdd(P.Key)+=Amount;}
        B.ConstructionProgress+=Delta;B.Status=ConstructionStage(B);
        if(B.ConstructionProgress+1.e-7>=1)
        {B.ConstructionProgress=1;B.IsConstructing=false;B.Builders=0;B.BuildersOnSite=0;B.TravellingBuilders=0;B.ConstructionMaterials.Empty();B.InstalledMaterials=ConstructionCost(B);if(!B.UpgradeTarget.IsEmpty()){B.DefId=B.UpgradeTarget;B.UpgradeTarget.Empty();B.Health=Definition(B)->Health;if(Definition(B)->Role==TEXT("core"))CoreDefinition=B.DefId;B.BatteryEnergyKWh=FMath::Min(B.BatteryEnergyKWh,Energy.Definition(B.DefId)->BatteryCapacityKWh);}++TransportRevision;AddEvent(Definition(B)->Name+TEXT(" construction completed"));}
    }
    StepRoadConstruction(Seconds);
}
void FSeigeSimulation::StepRoadConstruction(double Seconds)
{
    for(auto& R:Roads)
    {
        if(!R.IsConstructing||R.Health<=0){R.BuildersOnSite=R.TravellingBuilders=0;continue;}
        if(R.BuildersOnSite<=0)continue;
        const auto& D=TransportTiers[R.TargetTier];const auto Cost=RoadCost(R.A,R.B,R.TargetTier);
        const double Duration=RoadConstructionSeconds(R);
        double Delta=FMath::Min(1-R.ConstructionProgress,Seconds*WorkforceEfficiency*R.BuildersOnSite/(Duration*D.ConstructionWorkers));
        for(const auto& P:Cost)if(P.Value>0)Delta=FMath::Min(Delta,R.ConstructionMaterials.FindRef(P.Key)/P.Value);
        for(const auto& P:Cost){R.ConstructionMaterials.FindOrAdd(P.Key)=FMath::Max(0.,R.ConstructionMaterials.FindRef(P.Key)-P.Value*Delta);R.InstalledMaterials.FindOrAdd(P.Key)+=P.Value*Delta;}
        R.ConstructionProgress+=Delta;
        if(R.ConstructionProgress+1.e-7>=1){R.ConstructionProgress=1;R.IsConstructing=false;R.Tier=R.TargetTier;R.MaxHealth=FVector2D::Distance(R.A,R.B)*MetersPerWorldUnit()*D.HealthPerMeter;R.Health=R.MaxHealth;R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;R.ConstructionMaterials.Empty();R.InstalledMaterials=Cost;++TransportRevision;AddEvent(D.Name+TEXT(" completed"));}
    }
}
void FSeigeSimulation::Tick(double Seconds)
{
    if (!Policy || !FMath::IsFinite(Seconds) || Seconds <= 0 || Escaped || Failed) return;
    // Bounded substeps preserve collision/transport behavior at accelerated simulation speed.
    while (Seconds > UE_DOUBLE_SMALL_NUMBER && !Escaped && !Failed)
    {
        const double Step = FMath::Min(Seconds, Number(TEXT("fixed_step_seconds"))); Seconds -= Step; Time += Step;
        Workers.Tick(*this,Step); Energy.Tick(*this,Step);StepPopulation(Step); StepConstruction(Step); AllocateWorkers();
        Energy.RefreshDefensiveReserve(*this,Step);const int32 ReservedThreatCount=Enemies.Num();
        StepProduction(Step); StepLogistics(Step);StepRoadRepairs(Step);StepWorkerDisassembly(Step);Trade.Tick(*this,Step);Companions.Tick(*this,Step);
        while (Time >= NextWaveTime) { TriggerWave(); NextWaveTime += Number(TEXT("wave_interval")); }
        // Advance disabled schedules too: saves always retain a future deadline,
        // while the switches suppress spawning rather than accumulating a backlog.
        while (Time >= NextRoamTime) { if (BackgroundBugsEnabled) SpawnEnemies(static_cast<int32>(Number(TEXT("roam_count")))); NextRoamTime += Number(TEXT("roam_interval")); }
        // Spawn schedules run after optional transactions. Only new threats
        // require another reserve query before servicing/defensive fire.
        if(Enemies.Num()!=ReservedThreatCount)Energy.RefreshDefensiveReserve(*this,Step);
        StepCombat(Step); AllocateWorkers();CheckObjectives(); Calendar.Advance(Step);
    }
}
bool FSeigeSimulation::HasActiveWork(const FSeigeBuilding& B) const
{
    const auto* D=Definition(B);
    if(!Policy||!D||B.Health<=0||B.IsConstructing||!B.Enabled||WorkFraction(B)<=0)return false;
    if(D->Role==TEXT("extractor"))return !ExtractionResource(B).IsEmpty()&&StorageRoom(B)>UE_DOUBLE_SMALL_NUMBER;
    if(B.DisassemblyCommitted)return true;
    const FString RecipeId=ActiveProductionRecipe(B);
    if(!RecipeId.IsEmpty())
    {
        if(B.ProductionCommitted)return true;
        const auto Inputs=ProductionInputs(B,RecipeId);const auto& Recipe=Recipes[RecipeId];
        const double Reserved=FMath::Max(InventoryLitres(Inputs),ProductionOutputLitres(Recipe));
        return (Recipe.WorkerOutput==0||WorkersNeeded()>0)&&Reserved<=StorageRoom(B)+InventoryLitres(Inputs)+1.e-8&&HasSpendable(B,Inputs)&&Energy.CanConsume(*this,B.Id,ProductionEnergy(B,RecipeId));
    }
    if(D->Role==TEXT("core")||D->Role==TEXT("worker_factory"))return false;
    if(D->Role==TEXT("service"))return B.SupportedRobots>0&&B.MaintenanceSupplied;
    if(D->Role==TEXT("storage"))return Sum(B.Inventory)>0;
    return true; // Online sensors/defenses may still be inspected between actions.
}
void FSeigeSimulation::StepProduction(double Seconds)
{
    for (FSeigeBuilding& B : Buildings)
    {
        const FSeigeBuildingDef& D = *Definition(B);
        if (B.Health <= 0) { B.Status = TEXT("Destroyed"); continue; }
        if (B.IsConstructing) continue;
        // Repair is automatic even on disabled buildings, but always consumes local material.
        if (B.Health < D.Health)
        {
            const FString Resource = TextRule(TEXT("repair_resource")); double& Stock = B.Inventory.FindOrAdd(Resource);
            const double Restored = FMath::Min3(D.Health - B.Health, Number(TEXT("repair_health_per_second")) * Seconds, Spendable(B,Resource) * Number(TEXT("repair_health_per_unit")));
            Stock = FMath::Max(0.0, Stock - Restored / Number(TEXT("repair_health_per_unit"))); B.Health += Restored;
        }
        if (!B.Enabled) { B.Status = TEXT("Disabled (repairs remain automatic)"); continue; }
        const double Fraction = WorkFraction(B);
        if (Fraction <= 0) { B.Status = Energy.Fraction(B.Id)<=0?TEXT("Waiting for road-grid power"):TEXT("Waiting for workers"); continue; }
        if (D.Role==TEXT("extractor"))
        {
            const FString Resource=ExtractionResource(B);
            if(Resource.IsEmpty()){B.Status=TEXT("No valid bound deposit");continue;}
            const double Room=StorageRoom(B);
            const double Produced=FMath::Min(Room/Resources[Resource].LitresPerUnit,ExtractionRate(B)*Seconds*Fraction);
            B.Inventory.FindOrAdd(Resource)+=Produced;
            B.Status=Room<=UE_DOUBLE_SMALL_NUMBER?TEXT("Storage full; waiting for courier"):TEXT("Extracting ")+Resources[Resource].Name;
        }
        else if(!ActiveProductionRecipe(B).IsEmpty())StepRecipe(B,Seconds);
        else if (D.Role==TEXT("core")||D.Role==TEXT("worker_factory")) B.Status=TEXT("Worker target satisfied; production idle");
        else if(D.Role==TEXT("service"))B.Status=B.MaintenanceSupplied?TEXT("Automatic worker charging and maintenance online"):TEXT("Waiting for local maintenance supplies");
        else if (D.Role == TEXT("storage")) B.Status = TEXT("Receiving and redistributing overflow");
        else if(D.Role==TEXT("generator"))B.Status=TEXT("Generating electricity for connected road grid");
        else if(D.Role==TEXT("battery"))B.Status=TEXT("Shared grid battery storage online");
        else if(D.Role==TEXT("trade"))B.Status=TEXT("External trading port ready");
        else if (D.Role == TEXT("defense")) B.Status = TEXT("Automatic defense online");
        else B.Status = TEXT("Sensor coverage online");
    }
}
double FSeigeSimulation::Demand(const FSeigeBuilding& B, const FString& Resource, bool IncludeCoreReserve) const
{
    double Need=0;for(const auto& Category:DeliveryPriorities)if(IncludeCoreReserve||Category!=TEXT("reserve"))Need+=DeliveryDemand(B,Resource,Category);return Need;
}
double FSeigeSimulation::ProductionInputBuffer(const FSeigeBuilding& B,const FString& RecipeId,const FString& Resource) const
{
    const double Input=ProductionInputs(B,RecipeId).FindRef(Resource);if(Input<=0)return 0;
    const double Cycles=Input*Number(TEXT("delivery_buffer_cycles"));const auto* D=Resources.Find(Resource);
    if(D&&!D->Discrete&&(D->Class==TEXT("standard")||D->Class==TEXT("rare")))return FMath::Max(Cycles,Number(TEXT("delivery_raw_input_buffer_loads"))*Workers.HaulUnits(*this,Resource));
    return Cycles;
}
double FSeigeSimulation::DeliveryDemand(const FSeigeBuilding& B,const FString& Resource,const FString& Category) const
{
    if(B.Health<=0||B.IsConstructing)return 0;const auto& D=*Definition(B);
    if(Category==TEXT("fuel"))return Energy.FuelDemand(B.DefId,Resource);
    if(Category==TEXT("maintenance"))return B.Enabled&&Resource==TextRule(TEXT("upkeep_resource"))?D.RobotSupportCapacity*Number(TEXT("upkeep_per_robot"))*Number(TEXT("upkeep_buffer_intervals")):0;
    if(Category==TEXT("repair"))return (Resource==TextRule(TEXT("repair_resource"))?Number(TEXT("repair_buffer_units")):0)+RoadRepairDemand(B,Resource);
    if(Category==TEXT("defense"))return Combat.AmmoDemand(*this,B.Id,Resource);
    if(Category==TEXT("production"))
    {
        double Need=FMath::Max(0.,Combat.Demand(*this,B.Id,Resource)-Combat.AmmoDemand(*this,B.Id,Resource));
        if(B.Enabled){const FString R=ActiveProductionRecipe(B);if(!R.IsEmpty())Need+=ProductionInputBuffer(B,R,Resource);if(B.DefId==CoreDefinition&&WorkersNeeded()>0&&R!=TextRule(TEXT("population_recipe")))Need+=ProductionInputBuffer(B,TextRule(TEXT("population_recipe")),Resource);}return Need;
    }
    if(Category==TEXT("trade"))return Trade.Demand(*this,B.Id,Resource)+(Resource==TextRule(TEXT("inactive_worker_resource"))?B.WorkerExportTarget+(B.DefId==CoreDefinition?WorkerSurplusTarget:0):0);
    if(Category==TEXT("reserve"))return B.DefId==CoreDefinition?CoreReserves.FindRef(Resource):0;
    return 0;
}
void FSeigeSimulation::StepLogistics(double Seconds)
{
    DispatchClock+=Seconds;if(DispatchClock<Number(TEXT("dispatch_interval")))return;
    DispatchClock=FMath::Fmod(DispatchClock,Number(TEXT("dispatch_interval")));
    TArray<FString> ResourceIds;Resources.GetKeys(ResourceIds);ResourceIds.Sort();
    auto Dispatch=[&](FSeigeBuilding& Source,int32 Target,int32 RoadId,const FString& Resource,double Amount,bool Construction,FVector2D Destination)
    {return Couriers.Num()<Number(TEXT("max_couriers"))&&Workers.Dispatch(*this,Source.Id,Target,RoadId,Resource,Amount,Construction);};
    auto ConstructionDelivery=[&](int32 BuildingId,int32 RoadId,const TMap<FString,double>& Cost,const TMap<FString,double>& Stock,const TMap<FString,double>& Installed,FVector2D Destination)
    {
        for(const FString& Resource:ResourceIds)
        {
            double Need=Cost.FindRef(Resource)-Stock.FindRef(Resource)-Installed.FindRef(Resource)-(RoadId?IncomingRoad(RoadId,Resource):Incoming(BuildingId,Resource));
            TSet<int32> Unavailable;
            while(Need>UE_DOUBLE_SMALL_NUMBER)
            {
                FSeigeBuilding* Best=nullptr;double BestAmount=0,BestDistance=TNumericLimits<double>::Max();bool BestSurplus=false;
                const double Haul=Resource==TextRule(TEXT("inactive_worker_resource"))?1.:Workers.HaulUnits(*this,Resource);
                for(auto& Source:Buildings)if(Source.Health>0&&!Source.IsConstructing&&Source.Id!=BuildingId&&!Unavailable.Contains(Source.Id))
                {
                    const double Supply=FMath::Max(0.,Source.Inventory.FindRef(Resource)-Workers.PickupReserved(*this,Source.Id,Resource));
                    const double Surplus=FMath::Max(0.,Supply-Demand(Source,Resource,false));
                    const bool UsefulSurplus=Surplus+UE_DOUBLE_SMALL_NUMBER>=FMath::Min3(Need,Haul,Number(TEXT("courier_min_batch")));
                    double Amount=FMath::Min3(Need,UsefulSurplus?Surplus:Supply,Haul);if(Resources[Resource].Discrete)Amount=FMath::FloorToDouble(Amount+1.e-9);if(Amount<=UE_DOUBLE_SMALL_NUMBER)continue;
                    const double Distance=FVector2D::DistSquared(BuildingAccessPoint(Source),Destination);
                    if(!Best||(UsefulSurplus&&!BestSurplus)||(UsefulSurplus==BestSurplus&&(Amount>BestAmount+UE_DOUBLE_SMALL_NUMBER||(FMath::IsNearlyEqual(Amount,BestAmount,UE_DOUBLE_SMALL_NUMBER)&&(Distance<BestDistance||(Distance==BestDistance&&Source.Id<Best->Id))))))
                    {Best=&Source;BestAmount=Amount;BestDistance=Distance;BestSurplus=UsefulSurplus;}
                }
                if(!Best)break;
                // Preserve operating buffers while bulk stock can fulfil the
                // accepted bill. When all surplus is exhausted, its protected
                // material remains eligible even if distributed inside buffers.
                if(Dispatch(*Best,BuildingId,RoadId,Resource,BestAmount,true,Destination))Need-=Couriers.Last().ReservedAmount+(Couriers.Last().SelfTransfer?1.:0.);
                else Unavailable.Add(Best->Id);
            }
        }
    };
    // The landed deployment kit is a separate onboard compartment. A real worker
    // carries each batch through the hatch before the core can install it.
    if(auto* C=Core();C&&C->IsConstructing&&C->UpgradeTarget.IsEmpty())for(const FString& Resource:ResourceIds){const double Available=Workers.DeploymentStock.FindRef(Resource)-Workers.PickupReserved(*this,C->Id,Resource,true);const double Need=ConstructionCost(*C).FindRef(Resource)-C->InstalledMaterials.FindRef(Resource)-C->ConstructionMaterials.FindRef(Resource)-Incoming(C->Id,Resource);if(Available>0&&Need>0)Workers.Dispatch(*this,C->Id,C->Id,0,Resource,FMath::Min(Available,Need),true,true);}
    TArray<int32> Targets;
    for(int I=0;I<Buildings.Num();++I)if(Buildings[I].DefId!=CoreDefinition&&Definition(Buildings[I])->Role!=TEXT("storage"))Targets.Add(I);
    for(int I=0;I<Buildings.Num();++I)if(Buildings[I].DefId==CoreDefinition)Targets.Add(I);
    for(int I=0;I<Buildings.Num();++I)if(Definition(Buildings[I])->Role==TEXT("storage"))Targets.Add(I);
    for(const FString& Category:DeliveryPriorities)
    {
        // Existing carrying/loading tasks are never preempted. The authored
        // order only chooses what the next genuinely available body collects.
        if(Category==TEXT("construction"))
        {
            for(auto& B:Buildings)if(B.Health>0&&B.IsConstructing&&B.Enabled)ConstructionDelivery(B.Id,0,ConstructionCost(B),B.ConstructionMaterials,B.InstalledMaterials,BuildingAccessPoint(B));
            for(auto& R:Roads)if(R.IsConstructing&&R.Health>0)ConstructionDelivery(0,R.Id,RoadCost(R.A,R.B,R.TargetTier),R.ConstructionMaterials,R.InstalledMaterials,RoadAccessPoint(R));
            continue;
        }
        for(int TargetIndex:Targets)
        {
        auto& Target=Buildings[TargetIndex];if(Target.Health<=0||Target.IsConstructing)continue;const auto& TD=*Definition(Target);
        if(TD.Role==TEXT("storage")&&WorkFraction(Target)<=0)continue;
        if(Category==TEXT("storage")&&TD.Role!=TEXT("storage"))continue;
        for(const FString& Resource:ResourceIds)
        {
            if(Couriers.Num()>=Number(TEXT("max_couriers")))return;
            const double Desired=DeliveryDemand(Target,Resource,Category);if(Category!=TEXT("storage")&&Desired<=UE_DOUBLE_SMALL_NUMBER)continue;
            double Prior=0;for(const auto& Earlier:DeliveryPriorities){if(Earlier==Category)break;Prior+=DeliveryDemand(Target,Resource,Earlier);}
            const double ClassStock=Target.Inventory.FindRef(Resource)+Incoming(Target.Id,Resource)-Prior;
            // Finite orders must receive their exact final fraction. Standing
            // buffers wait for their low-water mark instead of sending a body
            // across the colony after every tiny consumption tick.
            const bool Finite=Category==TEXT("trade")||Category==TEXT("storage")||(Category==TEXT("production")&&Combat.Demand(*this,Target.Id,Resource)>Combat.AmmoDemand(*this,Target.Id,Resource)+UE_DOUBLE_SMALL_NUMBER);
            if(!Finite&&ClassStock>Desired*Number(TEXT("delivery_refill_trigger_fraction"))+UE_DOUBLE_SMALL_NUMBER)continue;
            double Need=Desired-ClassStock;
            const double Room=StorageRoom(Target)/Resources[Resource].LitresPerUnit;
            if(Category==TEXT("storage"))Need=Room;
            Need=FMath::Min(Need,Room);if(Need<=UE_DOUBLE_SMALL_NUMBER)continue;
            FSeigeBuilding* Source=nullptr;double Closest=TNumericLimits<double>::Max(),Supply=0;
            for(auto& Candidate:Buildings)
            {
                if(Candidate.Id==Target.Id||Candidate.Health<=0||Candidate.IsConstructing)continue;const auto& SD=*Definition(Candidate);
                if(TD.Role==TEXT("storage")&&(SD.Role==TEXT("storage")||SD.Role==TEXT("core")))continue;
                const double Available=Spendable(Candidate,Resource)-Demand(Candidate,Resource,false);
                if(TD.Role==TEXT("storage"))
                {
                    bool Outstanding=false;for(const auto& Other:Buildings)if(Other.Id!=Candidate.Id&&Other.Health>0&&Definition(Other)->Role!=TEXT("storage")&&Demand(Other,Resource,true)>Other.Inventory.FindRef(Resource)+Incoming(Other.Id,Resource)+UE_DOUBLE_SMALL_NUMBER){Outstanding=true;break;}
                    if(Outstanding)continue;
                }
                const double Distance=FVector2D::Distance(Candidate.Position,Target.Position);
                if(Available+UE_DOUBLE_SMALL_NUMBER>=FMath::Min(Need,Number(TEXT("courier_min_batch")))&&Available>UE_DOUBLE_SMALL_NUMBER&&Distance<Closest){Source=&Candidate;Closest=Distance;Supply=Available;}
            }
            if(Source)Dispatch(*Source,Target.Id,0,Resource,FMath::Min3(Need,Supply,Resource==TextRule(TEXT("inactive_worker_resource"))?1.:Workers.HaulUnits(*this,Resource)),false,BuildingAccessPoint(Target));
        }
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
    if (!Policy || Escaped || Failed || !PeriodicAttacksEnabled) return;
    int32 Live = 0; for (const FSeigeBuilding& B : Buildings) if (B.Health > 0) ++Live;
    const int32 Count = FMath::Clamp(FMath::CeilToInt(Number(TEXT("wave_base_count")) + Live * Number(TEXT("wave_per_building")) + Population * Number(TEXT("wave_per_population")) + Wave * Number(TEXT("wave_escalation_per_wave"))), 0, static_cast<int32>(Number(TEXT("wave_max_count"))));
    ++Wave; SpawnEnemies(Count); AddEvent(TEXT("Alien pulse activity detected. Live positions require sensor coverage."));
}
void FSeigeSimulation::StepCombat(double Seconds)
{
    Combat.Tick(*this,Seconds,true);
    if(Escaped||Failed)return;
    Enemies.RemoveAll([](const FSeigeEnemy& E){return E.Health <= 0;});
    for (FSeigeEnemy& E : Enemies)
    {
        FSeigeBuilding* Target = nullptr; double Closest = TNumericLimits<double>::Max();
        for (FSeigeBuilding& B : Buildings)
        { const double Distance = FVector2D::Distance(E.Position, B.Position); if (B.Health > 0 && Distance < Closest) { Target = &B; Closest = Distance; } }
        E.TargetBuildingId=Target?Target->Id:0;
        if (!Target) continue;
        const double AttackDistance = Number(TEXT("enemy_attack_range")) + Definition(*Target)->Footprint;
        // A clamped approach can finish a few floating-point ULPs outside the
        // radius, where the next move is too small to change the position.
        // Numerical contact tolerance is not an additional gameplay range.
        if (Closest > AttackDistance + UE_DOUBLE_KINDA_SMALL_NUMBER)
            E.Position += (Target->Position - E.Position).GetSafeNormal() * FMath::Min(Number(TEXT("enemy_speed")) * Seconds, Closest - AttackDistance);
        else
        {
            const double Damage=Number(TEXT("enemy_damage_per_second"))*Seconds;
            Combat.DamageBuilding(*this,Target->Id,Damage);
            if(Damage>0)E.LastAttackTime=Time;
            if(Escaped||Failed)return;
        }
        for (int32 I = Couriers.Num() - 1; I >= 0; --I)
            if (FVector2D::Distance(E.Position, Couriers[I].Position) <= Number(TEXT("enemy_courier_attack_range"))) { Workers.KillCourier(*this,Couriers[I].Id); }
    }
}
void FSeigeSimulation::LaunchShuttle()
{
    if (Escaped || !Policy) return;
    for(auto& Dog:Companions.Dogs){Dog.Evacuated=true;Dog.Moving=false;}Companions.ControlledId=0;
    Combat.EvacuateShuttle();Workers.Evacuate();for(auto& Body:Workers.Bodies)if(Body.State==TEXT("vehicle")){const auto* V=Combat.FindVehicle(Body.ContainerId);if(V&&V->Evacuated){Body.State=TEXT("evacuated");Body.Activity=TEXT("terminal");}}
    Workers.RefreshMetrics(*this);
    Escaped = true;
    AddEvent(Failed ? TEXT("Core destroyed. Emergency shuttle launched with only cargo already aboard; colony command lost.") : TEXT("Shuttle launched with only cargo already aboard. Local assets remain behind; this prototype ends here."));
}
void FSeigeSimulation::OnBuildingDestroyed(int32 Id)
{
    auto* B=FindBuilding(Id);if(!B)return;
    Workers.OnBuildingDestroyed(*this,Id);B->Health=0;B->Enabled=false;B->Workers=B->Builders=B->BuildersOnSite=B->TravellingBuilders=0;
    B->Inventory.Empty();B->ConstructionMaterials.Empty();B->ProductionInputs.Empty();B->ProductionCommitted=false;B->CommittedRecipe.Empty();B->ProductionReservedLitres=B->Progress=B->BatteryEnergyKWh=0;
    B->DisassemblyQueued=0;B->DisassemblyCommitted=false;B->DisassemblyProgress=B->DisassemblyReservedLitres=0;B->Shipment={};B->Status=TEXT("Destroyed");
    ++TransportRevision;Energy.Invalidate();AllocateWorkers();
    if(Definition(*B)->Role==TEXT("core")){Failed=true;LaunchShuttle();}
}
void FSeigeSimulation::DamageRoad(int32 Id,double Damage)
{
    auto* R=FindRoad(Id);if(!R||R->Health<=0||!FMath::IsFinite(Damage)||Damage<=0)return;
    R->Health=FMath::Max(0.,R->Health-Damage);
    if(R->Health<=0){R->Builders=R->BuildersOnSite=R->TravellingBuilders=0;R->ConstructionMaterials.Empty();++TransportRevision;Energy.Invalidate();AllocateWorkers();AddEvent(TEXT("Transport corridor destroyed"));}
}
void FSeigeSimulation::CheckObjectives()
{
    if (Won || Failed || Escaped) return;
    int32 Count = 0; for (const FSeigeBuilding& B : Buildings) if (B.Health > 0 && !B.IsConstructing && B.DefId == TextRule(TEXT("objective_building"))) ++Count;
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
    int32 Count = 0; for (const FSeigeBuilding& B : Buildings) if (B.Health > 0 && !B.IsConstructing && B.DefId == TextRule(TEXT("objective_building"))) ++Count;
    return FString::Printf(TEXT("Survive %.0f / %.0f s | Manufacture %.0f / %.0f %s | %s %d / %.0f"), Time, Number(TEXT("objective_survival_seconds")), ProducedUnits.FindRef(TextRule(TEXT("objective_resource"))), Number(TEXT("objective_produced_amount")), *Resources[TextRule(TEXT("objective_resource"))].Name, *BuildingDefs[TextRule(TEXT("objective_building"))].Name, Count, Number(TEXT("objective_building_count")));
}
FString FSeigeSimulation::WorkforceStatus() const
{ return FString::Printf(TEXT("Workers %d | Jobs %d | Open %d | Service capacity %d | Supported %d | Efficiency %.0f%%"), Population, TotalJobs, FMath::Max(0, TotalJobs - Employed),RobotSupportCapacity,SupportedPopulation, WorkforceEfficiency * 100); }
double FSeigeSimulation::FixedStepSeconds() const { return Policy ? Number(TEXT("fixed_step_seconds")) : 0; }

bool FSeigeSimulation::Save(const FString& Filename, FString& Error) const
{
    if (!Policy) { Error = TEXT("Cannot save before rules are initialized"); return false; }
    FObject O = MakeShared<FJsonObject>(); O->SetNumberField(TEXT("save_format"), 7);
    WritePoint(O,TEXT("environment_world_offset"),Environment.WorldOffset);
    O->SetNumberField(TEXT("generation_seed"),GenerationSeed);O->SetNumberField(TEXT("credits"),Credits);FObject Grid=MakeShared<FJsonObject>();Energy.Save(Grid);O->SetObjectField(TEXT("energy"),Grid);Companions.Save(O);Combat.Save(O);Walls.Save(O);Workers.Save(O);O->SetNumberField(TEXT("calendar_elapsed_us"),double(Calendar.ElapsedMicroseconds()));
    O->SetNumberField(TEXT("worker_surplus_target"),WorkerSurplusTarget);O->SetNumberField(TEXT("workers_disassembled"),WorkersDisassembled);O->SetNumberField(TEXT("worker_store_clock"),WorkerStoreClock);O->SetNumberField(TEXT("worker_reactivate_clock"),WorkerReactivateClock);
    O->SetStringField(TEXT("rules_version"), RulesVersion); O->SetStringField(TEXT("rules_fingerprint"), RulesFingerprint);
    O->SetNumberField(TEXT("time"),Time); O->SetNumberField(TEXT("next_wave_time"),NextWaveTime); O->SetNumberField(TEXT("next_roam_time"),NextRoamTime);
    O->SetBoolField(TEXT("background_bugs"),BackgroundBugsEnabled); O->SetBoolField(TEXT("periodic_attacks"),PeriodicAttacksEnabled);
    O->SetNumberField(TEXT("population_clock"),PopulationClock); O->SetNumberField(TEXT("dispatch_clock"),DispatchClock); O->SetNumberField(TEXT("upkeep_clock"),UpkeepClock); O->SetNumberField(TEXT("workforce_efficiency"),WorkforceEfficiency);
    O->SetNumberField(TEXT("population"),Population); O->SetNumberField(TEXT("wave"),Wave); O->SetNumberField(TEXT("lost_couriers"),LostCouriers); O->SetNumberField(TEXT("delivered_units"),DeliveredUnits); O->SetNumberField(TEXT("next_id"),NextId); O->SetNumberField(TEXT("random_seed"),Random.GetCurrentSeed());
    O->SetBoolField(TEXT("escaped"),Escaped); O->SetBoolField(TEXT("failed"),Failed); O->SetBoolField(TEXT("won"),Won);
    O->SetObjectField(TEXT("shuttle_cargo"),JsonAmounts(ShuttleCargo)); O->SetObjectField(TEXT("produced_units"),JsonAmounts(ProducedUnits));
    TArray<TSharedPtr<FJsonValue>> A;
    for (const FSeigeBuilding& B : Buildings)
    {
        FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("id"),B.Id); V->SetStringField(TEXT("definition"),B.DefId);V->SetNumberField(TEXT("deposit_id"),B.DepositId); V->SetStringField(TEXT("status"),B.Status); WritePosition(V,B.Position);
        V->SetNumberField(TEXT("health"),B.Health); V->SetNumberField(TEXT("progress"),B.Progress); V->SetBoolField(TEXT("enabled"),B.Enabled); V->SetObjectField(TEXT("inventory"),JsonAmounts(B.Inventory));
        V->SetBoolField(TEXT("is_constructing"),B.IsConstructing);V->SetNumberField(TEXT("construction_progress"),B.ConstructionProgress);
        WriteCrew(V,B);V->SetObjectField(TEXT("construction_materials"),JsonAmounts(B.ConstructionMaterials));V->SetBoolField(TEXT("maintenance_supplied"),B.MaintenanceSupplied);
        V->SetStringField(TEXT("upgrade_target"),B.UpgradeTarget);V->SetObjectField(TEXT("previous_level_materials"),JsonAmounts(B.PreviousLevelMaterials));
        V->SetBoolField(TEXT("production_committed"),B.ProductionCommitted);V->SetObjectField(TEXT("production_inputs"),JsonAmounts(B.ProductionInputs));V->SetNumberField(TEXT("production_reserved_litres"),B.ProductionReservedLitres);V->SetNumberField(TEXT("battery_kwh"),B.BatteryEnergyKWh);
        V->SetStringField(TEXT("selected_recipe"),B.SelectedRecipe);V->SetStringField(TEXT("committed_recipe"),B.CommittedRecipe);V->SetNumberField(TEXT("worker_export_target"),B.WorkerExportTarget);V->SetNumberField(TEXT("disassembly_queued"),B.DisassemblyQueued);V->SetBoolField(TEXT("disassembly_committed"),B.DisassemblyCommitted);V->SetNumberField(TEXT("disassembly_progress"),B.DisassemblyProgress);V->SetNumberField(TEXT("disassembly_reserved_litres"),B.DisassemblyReservedLitres);
        FObject Shipment=MakeShared<FJsonObject>();Shipment->SetStringField(TEXT("resource"),B.Shipment.Resource);Shipment->SetBoolField(TEXT("buy"),B.Shipment.Buy);Shipment->SetBoolField(TEXT("departed"),B.Shipment.Departed);Shipment->SetNumberField(TEXT("quantity"),B.Shipment.Quantity);Shipment->SetNumberField(TEXT("price_credits"),B.Shipment.PriceCredits);Shipment->SetNumberField(TEXT("progress"),B.Shipment.Progress);Shipment->SetNumberField(TEXT("goods_escrow"),B.Shipment.GoodsEscrow);V->SetObjectField(TEXT("shipment"),Shipment);
        V->SetNumberField(TEXT("weapon_cooldown"),B.WeaponCooldown);V->SetNumberField(TEXT("last_shot_time"),B.LastShotTime);
        V->SetArrayField(TEXT("last_shot_position"),{MakeShared<FJsonValueNumber>(B.LastShotPosition.X),MakeShared<FJsonValueNumber>(B.LastShotPosition.Y)});A.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("buildings"),A); A.Empty();
    for(const auto& R:Roads)if(R.Health>0)
    {
        FObject V=MakeShared<FJsonObject>();V->SetNumberField(TEXT("id"),R.Id);WritePoint(V,TEXT("a"),R.A);WritePoint(V,TEXT("b"),R.B);V->SetStringField(TEXT("tier"),R.Tier);V->SetStringField(TEXT("target_tier"),R.TargetTier);V->SetBoolField(TEXT("is_constructing"),R.IsConstructing);V->SetNumberField(TEXT("construction_progress"),R.ConstructionProgress);V->SetObjectField(TEXT("construction_materials"),JsonAmounts(R.ConstructionMaterials));V->SetObjectField(TEXT("previous_tier_materials"),JsonAmounts(R.PreviousTierMaterials));WriteCrew(V,R);V->SetNumberField(TEXT("health"),R.Health);V->SetNumberField(TEXT("max_health"),R.MaxHealth);A.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("roads"),A);A.Empty();
    for (const FSeigeNode& N : Nodes) { FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("id"),N.Id); V->SetStringField(TEXT("resource"),N.Resource); WritePosition(V,N.Position); A.Add(MakeShared<FJsonValueObject>(V)); }
    O->SetArrayField(TEXT("nodes"),A); A.Empty();
    for (const FSeigeCourier& C : Couriers) { FObject V = MakeShared<FJsonObject>(); V->SetNumberField(TEXT("id"),C.Id); V->SetNumberField(TEXT("source"),C.SourceId); V->SetNumberField(TEXT("target"),C.TargetId); V->SetStringField(TEXT("resource"),C.Resource); V->SetNumberField(TEXT("amount"),C.Amount);V->SetStringField(TEXT("worker_id"),C.WorkerId);V->SetStringField(TEXT("phase"),C.Phase);V->SetNumberField(TEXT("reserved_amount"),C.ReservedAmount);V->SetNumberField(TEXT("phase_seconds"),C.PhaseSeconds);V->SetBoolField(TEXT("source_deployment"),C.SourceDeployment);V->SetBoolField(TEXT("self_transfer"),C.SelfTransfer); V->SetBoolField(TEXT("for_construction"),C.ForConstruction); V->SetNumberField(TEXT("road_target"),C.RoadTargetId);V->SetNumberField(TEXT("next_waypoint"),C.NextWaypoint);WriteRoute(V,TEXT("route"),C.Route); WritePosition(V,C.Position); A.Add(MakeShared<FJsonValueObject>(V)); }
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
    if (Format != 7 || Version != RulesVersion || Fingerprint != RulesFingerprint) { Error = TEXT("Save is incompatible with individual-worker rules (save format 7) or edited rule files. The existing save was not changed; start a new colony."); return false; }
    // Parse into a temporary simulation: a corrupt save must never damage the running colony.
    FSeigeSimulation Candidate = *this;
    FVector2D SavedOffset;
    if(!PositionField(O,TEXT("environment_world_offset"),SavedOffset,Error)||!SavedOffset.Equals(Environment.WorldOffset,.000001)){Error=TEXT("Saved geography belongs to a different world region");return false;}
    // Version 8 construction and route state intentionally require a new-format save.
    Candidate.BackgroundBugsEnabled = true; Candidate.PeriodicAttacksEnabled = true;
    if (true)
    {
        if (!O->HasTypedField<EJson::Boolean>(TEXT("background_bugs")) || !O->HasTypedField<EJson::Boolean>(TEXT("periodic_attacks")) ||
            !O->TryGetBoolField(TEXT("background_bugs"),Candidate.BackgroundBugsEnabled) || !O->TryGetBoolField(TEXT("periodic_attacks"),Candidate.PeriodicAttacksEnabled))
        { Error = TEXT("Invalid saved scenario threat settings"); return false; }
    }
    if (!Numeric(O,TEXT("time"),Candidate.Time,0,Error) || !Numeric(O,TEXT("next_wave_time"),Candidate.NextWaveTime,0,Error) || !Numeric(O,TEXT("next_roam_time"),Candidate.NextRoamTime,0,Error) || !Numeric(O,TEXT("population_clock"),Candidate.PopulationClock,0,Error) || !Numeric(O,TEXT("dispatch_clock"),Candidate.DispatchClock,0,Error) || !Numeric(O,TEXT("upkeep_clock"),Candidate.UpkeepClock,0,Error) || !Numeric(O,TEXT("workforce_efficiency"),Candidate.WorkforceEfficiency,0,Error) || !Numeric(O,TEXT("delivered_units"),Candidate.DeliveredUnits,0,Error) || !IntegerField(O,TEXT("population"),Candidate.Population,0,Error) || !IntegerField(O,TEXT("wave"),Candidate.Wave,0,Error) || !IntegerField(O,TEXT("lost_couriers"),Candidate.LostCouriers,0,Error) || !IntegerField(O,TEXT("next_id"),Candidate.NextId,1,Error)) return false;
    if(!IntegerField(O,TEXT("worker_surplus_target"),Candidate.WorkerSurplusTarget,0,Error)||Candidate.WorkerSurplusTarget>100000||!IntegerField(O,TEXT("workers_disassembled"),Candidate.WorkersDisassembled,0,Error)||!Numeric(O,TEXT("worker_store_clock"),Candidate.WorkerStoreClock,0,Error)||!Numeric(O,TEXT("worker_reactivate_clock"),Candidate.WorkerReactivateClock,0,Error)||Candidate.WorkerStoreClock>Number(TEXT("worker_store_seconds"))+1.e-8||Candidate.WorkerReactivateClock>Number(TEXT("worker_reactivate_seconds"))+1.e-8){Error=TEXT("Invalid saved workforce targets or clocks");return false;}
    double Seed = 0;
    if (!Numeric(O,TEXT("random_seed"),Seed,MIN_int32,Error) || Seed > MAX_int32 || Seed != FMath::FloorToDouble(Seed)) { Error = TEXT("Invalid saved random state"); return false; }
    Candidate.Random.Initialize(static_cast<int32>(Seed));
    if(!Numeric(O,TEXT("generation_seed"),Seed,MIN_int32,Error)||Seed>MAX_int32||Seed!=FMath::FloorToDouble(Seed)||!Numeric(O,TEXT("credits"),Candidate.Credits,0,Error)){Error=TEXT("Invalid saved generation seed or credit balance");return false;}Candidate.GenerationSeed=int32(Seed);
    if (!O->TryGetBoolField(TEXT("escaped"),Candidate.Escaped) || !O->TryGetBoolField(TEXT("failed"),Candidate.Failed) || !O->TryGetBoolField(TEXT("won"),Candidate.Won)) { Error = TEXT("Missing saved outcome flags"); return false; }
    if (!Amounts(O,TEXT("shuttle_cargo"),Candidate.ShuttleCargo,Resources,Error) || !Amounts(O,TEXT("produced_units"),Candidate.ProducedUnits,Resources,Error)) return false;
    if (Candidate.WorkforceEfficiency > 1 || Candidate.DispatchClock >= Number(TEXT("dispatch_interval")) + UE_DOUBLE_SMALL_NUMBER || Candidate.UpkeepClock >= Number(TEXT("upkeep_interval")) + UE_DOUBLE_SMALL_NUMBER || Candidate.NextWaveTime < Candidate.Time || Candidate.NextRoamTime < Candidate.Time || (Candidate.Failed && !Candidate.Escaped) || InventoryMassKg(Candidate.ShuttleCargo) > Number(TEXT("shuttle_capacity")) + UE_DOUBLE_SMALL_NUMBER) { Error = TEXT("Inconsistent saved timers, flags or cargo"); return false; }
    Candidate.Roads.Empty();Candidate.Buildings.Empty(); Candidate.Nodes.Empty(); Candidate.Couriers.Empty(); Candidate.Enemies.Empty(); Candidate.Events.Empty();
    TSet<int32> Ids; int32 MaximumId = 0, CoreCount = 0;
    auto ReadId = [&](const FObject& V,int32& Id)->bool { if (!IntegerField(V,TEXT("id"),Id,1,Error)) return false; if (Ids.Contains(Id)) { Error = TEXT("Duplicate saved entity ID"); return false; } Ids.Add(Id); MaximumId = FMath::Max(MaximumId,Id); return true; };
    auto Inside = [&](FVector2D P)->bool { if (FMath::Abs(P.X) > WorldHalfSize || FMath::Abs(P.Y) > WorldHalfSize) { Error = TEXT("Saved entity outside sector"); return false; } return true; };
    const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
    if (!ArrayField(O,TEXT("buildings"),A,Error)) return false;
    for (const auto& Value : *A)
    {
        const FObject V = Value->AsObject(); FSeigeBuilding B;
        if (!ReadId(V,B.Id) || !StringField(V,TEXT("definition"),B.DefId,Error) || !BuildingDefs.Contains(B.DefId) || !StringField(V,TEXT("status"),B.Status,Error) || !PositionField(V,TEXT("position"),B.Position,Error) || !Inside(B.Position) || !Numeric(V,TEXT("health"),B.Health,0,Error) || !Numeric(V,TEXT("progress"),B.Progress,0,Error) || !Amounts(V,TEXT("inventory"),B.Inventory,Resources,Error)) { if (Error.IsEmpty()) Error = TEXT("Invalid saved building"); return false; }
        if(!V->TryGetBoolField(TEXT("is_constructing"),B.IsConstructing)||!V->TryGetBoolField(TEXT("maintenance_supplied"),B.MaintenanceSupplied)||!Numeric(V,TEXT("construction_progress"),B.ConstructionProgress,0,Error)||!Amounts(V,TEXT("construction_materials"),B.ConstructionMaterials,Resources,Error)) {Error=TEXT("Missing construction or maintenance state");return false;}
        if(!StringField(V,TEXT("upgrade_target"),B.UpgradeTarget,Error)||!Amounts(V,TEXT("previous_level_materials"),B.PreviousLevelMaterials,Resources,Error)||!V->TryGetBoolField(TEXT("production_committed"),B.ProductionCommitted)||!Amounts(V,TEXT("production_inputs"),B.ProductionInputs,Resources,Error)||!Numeric(V,TEXT("production_reserved_litres"),B.ProductionReservedLitres,0,Error)||!Numeric(V,TEXT("battery_kwh"),B.BatteryEnergyKWh,0,Error))return false;
        if((!B.UpgradeTarget.IsEmpty()&&(!B.IsConstructing||B.UpgradeTarget!=BuildingDefs[B.DefId].NextUpgrade||!BuildingDefs.Contains(B.UpgradeTarget)))||B.BatteryEnergyKWh>Energy.Definition(B.DefId)->BatteryCapacityKWh+1.e-8){Error=TEXT("Invalid upgrade or battery state");return false;}
        const auto& Def=BuildingDefs[B.DefId];
        if(!IntegerField(V,TEXT("deposit_id"),B.DepositId,0,Error)||(Def.Role==TEXT("extractor"))!=(B.DepositId>0)){Error=TEXT("Invalid saved Extraction Mine deposit binding");return false;}
        if(!StringField(V,TEXT("selected_recipe"),B.SelectedRecipe,Error)||!StringField(V,TEXT("committed_recipe"),B.CommittedRecipe,Error)||!IntegerField(V,TEXT("worker_export_target"),B.WorkerExportTarget,0,Error)||B.WorkerExportTarget>100000||!IntegerField(V,TEXT("disassembly_queued"),B.DisassemblyQueued,0,Error)||!V->TryGetBoolField(TEXT("disassembly_committed"),B.DisassemblyCommitted)||!Numeric(V,TEXT("disassembly_progress"),B.DisassemblyProgress,0,Error)||!Numeric(V,TEXT("disassembly_reserved_litres"),B.DisassemblyReservedLitres,0,Error)){Error=TEXT("Invalid saved production/workforce state");return false;}
        TArray<FString> Allowed=Def.AllowedRecipes;if(!Def.Recipe.IsEmpty())Allowed.AddUnique(Def.Recipe);
        if((!B.SelectedRecipe.IsEmpty()&&!Allowed.Contains(B.SelectedRecipe))||(B.WorkerExportTarget>0&&!Trade.Definition(B.DefId))){Error=TEXT("Invalid selected recipe or trade worker reserve");return false;}
        if(B.ProductionCommitted){const auto* R=Recipes.Find(B.CommittedRecipe);if(B.Health<=0||B.IsConstructing||!R||!Allowed.Contains(B.CommittedRecipe)||!ValidInstallation(ProductionInputs(B,B.CommittedRecipe),{},B.ProductionInputs,1,Error)||FMath::Abs(B.ProductionReservedLitres-FMath::Max(InventoryLitres(ProductionInputs(B,B.CommittedRecipe)),ProductionOutputLitres(*R)))>1.e-8){Error=TEXT("Invalid production batch escrow");return false;}}
        else if(!B.CommittedRecipe.IsEmpty()||!B.ProductionInputs.IsEmpty()||B.ProductionReservedLitres!=0||B.Progress!=0){Error=TEXT("Uncommitted batch has inventory or progress");return false;}
        if(B.DisassemblyQueued>0&&(!Def.StoresInactiveWorkers||B.Health<=0||B.IsConstructing)){Error=TEXT("Invalid disassembly workstation");return false;}
        if(B.DisassemblyCommitted){if(B.DisassemblyQueued<1||B.DisassemblyProgress>=1||FMath::Abs(B.DisassemblyReservedLitres-InventoryLitres(DisassemblyOutputs()))>1.e-8){Error=TEXT("Invalid paid disassembly escrow");return false;}}
        else if(B.DisassemblyProgress!=0||B.DisassemblyReservedLitres!=0){Error=TEXT("Unpaid disassembly has progress or reserved output");return false;}
        if(B.DisassemblyQueued-(B.DisassemblyCommitted?1:0)>B.Inventory.FindRef(TextRule(TEXT("inactive_worker_resource")))){Error=TEXT("Disassembly queue exceeds stored workers");return false;}
        FObject S;if(!ObjectField(V,TEXT("shipment"),S,Error)||!StringField(S,TEXT("resource"),B.Shipment.Resource,Error)||!S->TryGetBoolField(TEXT("buy"),B.Shipment.Buy)||!S->TryGetBoolField(TEXT("departed"),B.Shipment.Departed)||!Numeric(S,TEXT("quantity"),B.Shipment.Quantity,0,Error)||!Numeric(S,TEXT("price_credits"),B.Shipment.PriceCredits,0,Error)||!Numeric(S,TEXT("progress"),B.Shipment.Progress,0,Error)||!Numeric(S,TEXT("goods_escrow"),B.Shipment.GoodsEscrow,0,Error))return false;
        if(B.Shipment.Resource.IsEmpty()){if(B.Shipment.Buy||B.Shipment.Departed||B.Shipment.Quantity!=0||B.Shipment.PriceCredits!=0||B.Shipment.Progress!=0||B.Shipment.GoodsEscrow!=0){Error=TEXT("Empty shipment has outstanding state");return false;}}
        else{const auto* Port=Trade.Definition(B.DefId);const auto* R=Resources.Find(B.Shipment.Resource);if(!Port||!R||B.IsConstructing||B.Shipment.Quantity<=0||(R->Discrete&&B.Shipment.Quantity!=FMath::FloorToDouble(B.Shipment.Quantity))||B.Shipment.Quantity*R->UnitMassKg>Port->CapacityKg+1.e-8||B.Shipment.Progress>=1||(!B.Shipment.Departed&&B.Shipment.Progress!=0)||FMath::Abs(B.Shipment.PriceCredits-Trade.Quote(B.Shipment.Resource,B.Shipment.Quantity,B.Shipment.Buy))>1.e-8||B.Shipment.GoodsEscrow!=(!B.Shipment.Buy&&B.Shipment.Departed?B.Shipment.Quantity:0)){Error=TEXT("Invalid saved trade escrow");return false;}}
        if(!ReadCrew(V,B,Candidate.RequiredBuilders(B),WorldHalfSize,Resources,Error))return false;
        TMap<FString,double> PreviousBill=PreviousLevelBill(B.DefId);
        if(!B.UpgradeTarget.IsEmpty()){const TMap<FString,double>* Last=&Def.Cost;for(const auto& P:BuildingDefs)if(P.Value.NextUpgrade==B.DefId){Last=&P.Value.UpgradeCost;break;}for(const auto& P:*Last)PreviousBill.FindOrAdd(P.Key)+=P.Value;}
        if(!ValidInstallation(PreviousBill,{},B.PreviousLevelMaterials,1,Error))return false;
        if(B.ConstructionProgress>1 || (!B.IsConstructing&&(B.ConstructionProgress!=1||Sum(B.ConstructionMaterials)>1.e-6)) || (B.IsConstructing&&(B.ConstructionProgress>=1||B.Progress>0))) {Error=TEXT("Inconsistent construction state");return false;}
        if(B.Health>0&&!ValidInstallation(Candidate.ConstructionCost(B),B.ConstructionMaterials,B.InstalledMaterials,B.ConstructionProgress,Error))return false;
        if (!V->TryGetBoolField(TEXT("enabled"),B.Enabled) || B.Health > BuildingDefs[B.DefId].Health || B.Progress > 1 + UE_DOUBLE_SMALL_NUMBER || Occupied(B) > BuildingDefs[B.DefId].StorageCapacity + UE_DOUBLE_SMALL_NUMBER) { Error = TEXT("Invalid building health, progress or storage"); return false; }
        if (!Numeric(V,TEXT("weapon_cooldown"),B.WeaponCooldown,0,Error) || !Numeric(V,TEXT("last_shot_time"),B.LastShotTime,-1,Error) || B.LastShotTime>Candidate.Time || (B.LastShotTime<0 && B.LastShotTime!=-1) || !PositionField(V,TEXT("last_shot_position"),B.LastShotPosition,Error) || !Inside(B.LastShotPosition))
        {Error=TEXT("Invalid saved weapon timing or target position");return false;}
        if(B.IsConstructing&&B.UpgradeTarget.IsEmpty()&&!BuildingDefs[B.DefId].DeploymentDefense&&(B.WeaponCooldown>0||B.LastShotTime>=0)){Error=TEXT("Unfinished building cannot have fired a weapon");return false;}
        if (Def.Role == TEXT("core")){++CoreCount;Candidate.CoreDefinition=B.DefId;}
        Candidate.Buildings.Add(B);
    }
    if (CoreCount != 1 || (!Candidate.Escaped && Candidate.Core()->Health <= 0)) { Error = TEXT("Save requires exactly one command core"); return false; }
    if(!Candidate.Walls.Load(O,Candidate,Error))return false;
    if(!ArrayField(O,TEXT("roads"),A,Error)||A->Num()>Transport->GetNumberField(TEXT("max_segments")))return false;
    for(const auto& Value:*A)
    {
        const FObject V=Value->AsObject();FSeigeTransportSegment R;
        if(!ReadId(V,R.Id)||!PositionField(V,TEXT("a"),R.A,Error)||!PositionField(V,TEXT("b"),R.B,Error)||!Inside(R.A)||!Inside(R.B)||!StringField(V,TEXT("tier"),R.Tier,Error)||!StringField(V,TEXT("target_tier"),R.TargetTier,Error)||!TransportTiers.Contains(R.TargetTier)||(!R.Tier.IsEmpty()&&!TransportTiers.Contains(R.Tier))||!V->TryGetBoolField(TEXT("is_constructing"),R.IsConstructing)||!Numeric(V,TEXT("construction_progress"),R.ConstructionProgress,0,Error)||!Amounts(V,TEXT("construction_materials"),R.ConstructionMaterials,Resources,Error)||!Amounts(V,TEXT("previous_tier_materials"),R.PreviousTierMaterials,Resources,Error)){Error=TEXT("Invalid saved transport segment");return false;}
        const auto& D=TransportTiers[R.TargetTier];const double Length=FVector2D::Distance(R.A,R.B)*MetersPerWorldUnit();
        if(Length<Transport->GetNumberField(TEXT("minimum_segment_meters"))||Length>Transport->GetNumberField(TEXT("maximum_segment_meters"))||R.ConstructionProgress>1||(!R.IsConstructing&&(R.ConstructionProgress!=1||R.Tier!=R.TargetTier||Sum(R.ConstructionMaterials)>1.e-6))||(R.IsConstructing&&(R.ConstructionProgress>=1||(R.Tier.IsEmpty()?R.TargetTier!=Transport->GetStringField(TEXT("initial_tier")):R.TargetTier!=TransportTiers[R.Tier].NextTier)))){Error=TEXT("Invalid saved transport tier or construction progress");return false;}
        if(!Numeric(V,TEXT("health"),R.Health,0,Error)||!Numeric(V,TEXT("max_health"),R.MaxHealth,1.e-9,Error)||R.Health>R.MaxHealth||FMath::Abs(R.MaxHealth-Length*TransportTiers[R.Tier.IsEmpty()?R.TargetTier:R.Tier].HealthPerMeter)>1.e-6){Error=TEXT("Invalid road durability");return false;}
        if(!ReadCrew(V,R,D.ConstructionWorkers,WorldHalfSize,Resources,Error)||(R.Health>0&&!ValidInstallation(RoadCost(R.A,R.B,R.TargetTier),R.ConstructionMaterials,R.InstalledMaterials,R.ConstructionProgress,Error))||(R.Health>0&&!Candidate.ClearWalkingLine(R.A,R.B,D.WidthMeters*.5/MetersPerWorldUnit())))return false;
        TMap<FString,double> PreviousCost;FString Tier=Transport->GetStringField(TEXT("initial_tier"));while(Tier!=R.TargetTier&&!Tier.IsEmpty()){for(const auto& P:RoadCost(R.A,R.B,Tier))PreviousCost.FindOrAdd(P.Key)+=P.Value;Tier=TransportTiers[Tier].NextTier;}
        if(!ValidInstallation(PreviousCost,{},R.PreviousTierMaterials,1,Error))return false;
        Candidate.Roads.Add(R);
    }
    if (!ArrayField(O,TEXT("nodes"),A,Error)) return false;
    for (const auto& Value : *A) { const FObject V = Value->AsObject(); FSeigeNode N; if (!ReadId(V,N.Id) || !StringField(V,TEXT("resource"),N.Resource,Error) || !Resources.Contains(N.Resource) || !PositionField(V,TEXT("position"),N.Position,Error) || !Inside(N.Position)) { if(Error.IsEmpty()) Error = TEXT("Invalid saved deposit"); return false; } Candidate.Nodes.Add(N); }
    TArray<FSeigeNode> ExpectedNodes;
    if(!Candidate.GenerateResourceNodesForSeed(Candidate.GenerationSeed,ExpectedNodes,Error,Candidate.Environment.WorldOffset)||ExpectedNodes.Num()!=Candidate.Nodes.Num()){Error=TEXT("Saved deposits disagree with generation seed");return false;}
    for(int I=0;I<ExpectedNodes.Num();++I)if(ExpectedNodes[I].Resource!=Candidate.Nodes[I].Resource||!ExpectedNodes[I].Position.Equals(Candidate.Nodes[I].Position,1.e-8)){Error=TEXT("Saved deposit identity or position altered");return false;}
    TSet<int32> OccupiedDeposits;
    for(const auto& B:Candidate.Buildings)if(B.DepositId>0)
    {
        const auto* Node=Candidate.ExtractionNode(B.DefId,B.Position);
        if(!Node||Node->Id!=B.DepositId||(B.Health>0&&OccupiedDeposits.Contains(B.DepositId))){Error=TEXT("Saved mine has an invalid or duplicate deposit binding");return false;}
        if(B.Health>0)OccupiedDeposits.Add(B.DepositId);
    }
    if (!ArrayField(O,TEXT("couriers"),A,Error)) return false;
    for (const auto& Value : *A)
    {
        const FObject V = Value->AsObject(); FSeigeCourier C;
        if (!ReadId(V,C.Id) || !IntegerField(V,TEXT("source"),C.SourceId,1,Error) || !IntegerField(V,TEXT("target"),C.TargetId,0,Error) || !StringField(V,TEXT("resource"),C.Resource,Error) || !Resources.Contains(C.Resource) || !Numeric(V,TEXT("amount"),C.Amount,0,Error) || !PositionField(V,TEXT("position"),C.Position,Error) || !Inside(C.Position)) { if(Error.IsEmpty()) Error = TEXT("Invalid saved courier"); return false; }
        if(!StringField(V,TEXT("worker_id"),C.WorkerId,Error)||!StringField(V,TEXT("phase"),C.Phase,Error)||!Numeric(V,TEXT("reserved_amount"),C.ReservedAmount,0,Error)||!Numeric(V,TEXT("phase_seconds"),C.PhaseSeconds,0,Error)||!V->TryGetBoolField(TEXT("source_deployment"),C.SourceDeployment)||!V->TryGetBoolField(TEXT("self_transfer"),C.SelfTransfer)){Error=TEXT("Missing physical delivery task state");return false;}
        // A topology change can temporarily leave a courier without a route.
        // Empty + waypoint zero is a real waiting state: retain its physical
        // cargo and let StepLogistics retry, rather than making saves unloadable.
        if ((Resources[C.Resource].Discrete&&C.Amount!=FMath::FloorToDouble(C.Amount))||!V->TryGetBoolField(TEXT("for_construction"),C.ForConstruction)||!IntegerField(V,TEXT("road_target"),C.RoadTargetId,0,Error)||!IntegerField(V,TEXT("next_waypoint"),C.NextWaypoint,0,Error)||!ReadRoute(V,TEXT("route"),C.Route,WorldHalfSize,Error)||C.NextWaypoint>C.Route.Num()||C.Amount+C.ReservedAmount>Candidate.Workers.HaulUnits(Candidate,C.Resource)+UE_DOUBLE_SMALL_NUMBER||!Candidate.FindBuilding(C.SourceId)||(C.RoadTargetId?C.TargetId!=0||!Candidate.FindRoad(C.RoadTargetId):!Candidate.FindBuilding(C.TargetId))) { Error=TEXT("Invalid courier route, capacity or endpoint");return false; }
        if(C.RoadTargetId)
        {const auto& R=*Candidate.FindRoad(C.RoadTargetId);if(!C.ForConstruction||(R.Health>0&&(!R.IsConstructing||R.ConstructionMaterials.FindRef(C.Resource)+R.InstalledMaterials.FindRef(C.Resource)+Candidate.IncomingRoad(R.Id,C.Resource)+C.Amount+C.ReservedAmount>RoadCost(R.A,R.B,R.TargetTier).FindRef(C.Resource)+1.e-6))){Error=TEXT("Invalid road construction delivery");return false;}}
        else
        {const auto* Target=Candidate.FindBuilding(C.TargetId);if(C.ForConstruction&&Target->Health>0&&(!Target->IsConstructing||Target->ConstructionMaterials.FindRef(C.Resource)+Target->InstalledMaterials.FindRef(C.Resource)+Candidate.Incoming(Target->Id,C.Resource)+C.Amount+C.ReservedAmount>Candidate.ConstructionCost(*Target).FindRef(C.Resource)+1.e-6)){Error=TEXT("Invalid construction courier destination or excess payload");return false;}}
        Candidate.Couriers.Add(C);
    }
    if (Candidate.Couriers.Num() > Number(TEXT("max_couriers"))) { Error = TEXT("Saved courier count exceeds rule limit"); return false; }
    if (!ArrayField(O,TEXT("enemies"),A,Error)) return false;
    for (const auto& Value : *A) { const FObject V = Value->AsObject(); FSeigeEnemy E; if (!ReadId(V,E.Id) || !Numeric(V,TEXT("health"),E.Health,UE_DOUBLE_SMALL_NUMBER,Error) || E.Health > Number(TEXT("enemy_health")) || !PositionField(V,TEXT("position"),E.Position,Error) || !Inside(E.Position)) { if(Error.IsEmpty()) Error = TEXT("Invalid saved enemy"); return false; } Candidate.Enemies.Add(E); }
    if (!ArrayField(O,TEXT("events"),A,Error)) return false;
    for (const auto& Value : *A) { FSeigeEvent E; if (!Numeric(Value->AsObject(),TEXT("time"),E.Time,0,Error) || E.Time > Candidate.Time || !StringField(Value->AsObject(),TEXT("text"),E.Text,Error)) return false; Candidate.Events.Add(E); }
    if (Candidate.NextId <= MaximumId || Candidate.Events.Num() > Number(TEXT("event_history_limit"))) { Error = TEXT("Invalid next entity ID or event history"); return false; }
    ++Candidate.TransportRevision;for(auto& C:Candidate.Couriers)C.RouteRevision=Candidate.TransportRevision;for(auto& B:Candidate.Buildings)B.BuilderRouteRevision=Candidate.TransportRevision;for(auto& R:Candidate.Roads)R.BuilderRouteRevision=Candidate.TransportRevision;
    double CalendarUs=0;if(!Numeric(O,TEXT("calendar_elapsed_us"),CalendarUs,0,Error)||CalendarUs!=FMath::FloorToDouble(CalendarUs)||CalendarUs>9007199254740991.||!Candidate.Calendar.SetElapsedMicroseconds(int64(CalendarUs))){Error=TEXT("Invalid saved calendar clock");return false;}
    FObject Grid;if(!ObjectField(O,TEXT("energy"),Grid,Error)||!Candidate.Energy.Load(Grid,Candidate,Error)||!Candidate.Companions.Load(O,Candidate,Error)||!Candidate.Combat.Load(O,Candidate,Error)||!Candidate.Workers.Load(O,Candidate,Error))return false;
    Candidate.AllocateWorkers();Candidate.Energy.Tick(Candidate,0);
    Candidate.Energy.RefreshDefensiveReserve(Candidate,Candidate.FixedStepSeconds());
    *this = MoveTemp(Candidate); Error.Empty(); return true;
}
