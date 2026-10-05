#include "SeigeCombat.h"
#include "SeigeSimulation.h"
#include "SeigeCombatJson.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
using namespace SeigeCombatJson;
namespace
{
bool Amounts(const O& V,const TCHAR* K,TMap<FString,double>& M,const FSeigeSimulation& S){const O* A=nullptr;if(!V||!V->TryGetObjectField(K,A))return false;M.Reset();for(const auto& X:(*A)->Values){double N=0;FString Key(X.Key);if(!S.Resources.Contains(Key)||!X.Value->TryGetNumber(N)||!FMath::IsFinite(N)||N<0||N>1.e12)return false;M.Add(Key,N);}return true;}
bool Read(const FString& P,O& V,FString& Raw){int32 Version=0;return FFileHelper::LoadFileToString(Raw,*P)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),V)&&Int(V,TEXT("schema_version"),Version,1,1);}
}
bool FSeigeCombatSystem::Fits(int32 Points,double Mass,const TArray<FString>& Loadout,FString& Error) const
{
    int32 Used=0;double Weight=0;for(const auto& Id:Loadout){const auto* W=Weapons.Find(Id);if(!W||Id==BugWeapon){Error=TEXT("Unknown or non-player weapon: ")+Id;return false;}Used+=W->MountPoints;Weight+=W->MassKg;}
    if(Used>Points||Weight>Mass+1.e-8){Error=TEXT("Weapon frontal area or hardpoint mass budget exceeded");return false;}Error.Empty();return true;
}
bool FSeigeCombatSystem::ValidateLoadout(const FString& Id,const TArray<FString>& Loadout,FString& Error) const
{const auto* C=Chassis.Find(Id);if(!C){Error=TEXT("Unknown chassis");return false;}return Fits(C->MountPoints,C->MaxWeaponMassKg,Loadout,Error);}
bool FSeigeCombatSystem::Initialize(const FString& Directory,FSeigeSimulation& Sim,FString& Error)
{
    *this=FSeigeCombatSystem();O Main,WeaponDoc,ChassisDoc;FString A,B,C;
    if(!Read(FPaths::Combine(Directory,TEXT("combat.json")),Main,A)||!Read(FPaths::Combine(Directory,TEXT("weapons.json")),WeaponDoc,B)||!Read(FPaths::Combine(Directory,TEXT("chassis.json")),ChassisDoc,C)){Error=TEXT("Cannot read combat rules/schema_version 1");return false;}
    Fingerprint=FMD5::HashAnsiString(*(A+B+C));MetresPerUnit=Sim.MetersPerWorldUnit();SectorHalfSize=Sim.WorldHalfSize;const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    auto Fail=[&](const FString& Reason){Error=TEXT("Combat rules: ")+Reason;return false;};
    const O* Sizes=nullptr;const O* Profiles=nullptr;if(!WeaponDoc->TryGetObjectField(TEXT("hardpoint_sizes"),Sizes)||(*Sizes)->Values.Num()!=3||!Main->TryGetObjectField(TEXT("damage_profiles"),Profiles))return Fail(TEXT("hardpoint sizes/damage profiles missing"));
    for(const FString& Size:{FString(TEXT("small")),FString(TEXT("medium")),FString(TEXT("large"))})
    {
        const O* Entry=nullptr;FSeigeHardpointSize H;
        if(!(*Sizes)->TryGetObjectField(Size,Entry)||!Num(*Entry,TEXT("width_meters"),H.WidthMeters,.001,1000)||!Num(*Entry,TEXT("height_meters"),H.HeightMeters,.001,1000)||!Num(*Entry,TEXT("length_meters"),H.LengthMeters,.001,1000)||!Num(*Entry,TEXT("mass_limit_kg"),H.MassLimitKg,.001,1.e9)||!Int(*Entry,TEXT("mount_points"),H.MountPoints,1,256))return Fail(TEXT("invalid hardpoint dimensions or budget"));
        if(const auto* Base=HardpointSizes.Find(TEXT("small")))
        {
            const double Area=H.WidthMeters*H.HeightMeters*Base->MountPoints,PointArea=Base->WidthMeters*Base->HeightMeters*H.MountPoints;
            if(FMath::Abs(Area-PointArea)>1.e-6*FMath::Max(Area,PointArea))return Fail(TEXT("hardpoint points must be proportional to frontal area"));
        }
        HardpointSizes.Add(Size,H);
    }
    for(const FString& Family:{FString(TEXT("energy")),FString(TEXT("kinetic")),FString(TEXT("missile")),FString(TEXT("plasma"))}){const O* Entry=nullptr;double S=0,Ar=0;if(!(*Profiles)->TryGetObjectField(Family,Entry)||!Num(*Entry,TEXT("shield_multiplier"),S,.05,10)||!Num(*Entry,TEXT("armor_multiplier"),Ar,.05,10))return Fail(TEXT("damage family profile"));DamageProfiles.Add(Family,FVector2D(S,Ar));}
    if(!WeaponDoc->TryGetArrayField(TEXT("weapons"),Values)||Values->Num()>64)return Fail(TEXT("weapons missing or excessive"));
    for(const auto& V:*Values)
    {
        O J=V->AsObject();FSeigeWeaponDef W;
        if(!Str(J,TEXT("id"),W.Id)||W.Id.IsEmpty()||Weapons.Contains(W.Id)||!Str(J,TEXT("name"),W.Name)||!Str(J,TEXT("family"),W.Family)||!OneOf(W.Family,{TEXT("energy"),TEXT("kinetic"),TEXT("missile"),TEXT("plasma")})||!Str(J,TEXT("size"),W.Size)||!OneOf(W.Size,{TEXT("small"),TEXT("medium"),TEXT("large")})||!Str(J,TEXT("ammo"),W.Ammo)||!Int(J,TEXT("mount_points"),W.MountPoints,1,256)||!Num(J,TEXT("mass_kg"),W.MassKg,.001,1.e9)||!Num(J,TEXT("damage"),W.Damage,.001,1.e6)||!Num(J,TEXT("reload_seconds"),W.ReloadSeconds,Sim.FixedStepSeconds(),600)||!Num(J,TEXT("range_meters"),W.RangeMeters,1,3000)||!Num(J,TEXT("speed_m_s"),W.SpeedMetersSecond,0,10000)||!Num(J,TEXT("accuracy_degrees"),W.AccuracyDegrees,0,45)||!Num(J,TEXT("splash_meters"),W.SplashMeters,0,100)||!Num(J,TEXT("energy_kwh"),W.EnergyKWh,0,1000)||!Num(J,TEXT("ammo_per_shot"),W.AmmoPerShot,0,1000)||!Num(J,TEXT("shield_multiplier"),W.ShieldMultiplier,.05,10)||!Num(J,TEXT("armor_multiplier"),W.ArmorMultiplier,.05,10)||!Num(J,TEXT("homing_degrees_s"),W.HomingDegreesSecond,0,720)||!Amounts(J,TEXT("cost"),W.Cost,Sim))return Fail(TEXT("invalid weapon definition"));
        const auto& Size=HardpointSizes[W.Size];
        if(W.MountPoints!=Size.MountPoints||W.MassKg>Size.MassLimitKg||(!W.Ammo.IsEmpty()&&(!Sim.Resources.Contains(W.Ammo)||W.AmmoPerShot<=0))||(W.Ammo.IsEmpty()&&W.AmmoPerShot!=0)||(W.Family!=TEXT("energy")&&W.SpeedMetersSecond<=0)||(W.Family==TEXT("energy")&&(!W.Ammo.IsEmpty()||W.SpeedMetersSecond!=0)))return Fail(TEXT("weapon size/ammunition/projectile contract"));
        Weapons.Add(W.Id,W);
    }
    if(!ChassisDoc->TryGetArrayField(TEXT("chassis"),Values)||Values->Num()!=12)return Fail(TEXT("exactly twelve chassis required"));TSet<FString> Combinations;
    for(const auto& V:*Values)
    {
        O J=V->AsObject();FSeigeChassisDef D;
        if(!Str(J,TEXT("id"),D.Id)||D.Id.IsEmpty()||Chassis.Contains(D.Id)||!Str(J,TEXT("name"),D.Name)||!Str(J,TEXT("family"),D.Family)||!OneOf(D.Family,{TEXT("wheeled"),TEXT("tracked"),TEXT("mech")})||!Str(J,TEXT("size"),D.Size)||!OneOf(D.Size,{TEXT("small"),TEXT("medium"),TEXT("large"),TEXT("behemoth")})||!Int(J,TEXT("tier"),D.Tier,1,4)||!Int(J,TEXT("capacity_points"),D.CapacityPoints,1,65536)||!Int(J,TEXT("mount_points"),D.MountPoints,1,128)||!Int(J,TEXT("wheels"),D.Wheels,0,12)||!Int(J,TEXT("legs"),D.Legs,0,8)||!Num(J,TEXT("mass_kg"),D.MassKg,1,1.e7)||!Num(J,TEXT("max_weapon_mass_kg"),D.MaxWeaponMassKg,1,1.e6)||!Num(J,TEXT("cargo_mass_kg"),D.CargoMassKg,1,1.e6)||!Num(J,TEXT("storage_litres"),D.StorageLitres,1,1.e6)||!Num(J,TEXT("health"),D.Health,1,1.e7)||!Num(J,TEXT("armor"),D.Armor,0,1.e7)||!Num(J,TEXT("shield"),D.Shield,0,1.e7)||!Num(J,TEXT("battery_kwh"),D.BatteryKWh,.001,1.e6)||!Num(J,TEXT("speed_kmh"),D.SpeedKmh,.1,150)||!Num(J,TEXT("radius_meters"),D.RadiusMeters,.2,30)||!Num(J,TEXT("sensor_meters"),D.SensorMeters,1,1000)||!Num(J,TEXT("max_grade"),D.MaxGrade,.01,4)||!Num(J,TEXT("rough_speed_multiplier"),D.RoughSpeedMultiplier,.01,1)||!Num(J,TEXT("travel_kwh_km"),D.TravelKWhPerKm,.001,10000)||!Num(J,TEXT("road_damage_hp_m"),D.RoadDamagePerMeter,0,1000)||!Num(J,TEXT("build_seconds"),D.BuildSeconds,.1,86400)||!Num(J,TEXT("build_kwh"),D.BuildKWh,.001,100000)||!Amounts(J,TEXT("cost"),D.Cost,Sim))return Fail(TEXT("invalid chassis definition"));
        const int32 Tier=D.Size==TEXT("small")?1:D.Size==TEXT("medium")?2:D.Size==TEXT("large")?3:4;
        if(D.Tier!=Tier||Combinations.Contains(D.Family+D.Size)||Sim.InventoryMassKg(D.Cost)+1.e-8<D.MassKg)return Fail(TEXT("chassis capacity/coverage/material mass"));
        if((D.Family==TEXT("wheeled")&&((Tier>1&&D.Wheels!=(Tier==2?6:Tier==3?8:12))||D.Legs!=0))||(D.Family==TEXT("mech")&&((Tier>=3&&D.Legs!=(Tier==3?4:8))||D.Wheels!=0)))return Fail(TEXT("wheel/leg arrangement"));
        Combinations.Add(D.Family+D.Size);Chassis.Add(D.Id,D);
    }
    if(!Int(Main,TEXT("fleet_capacity"),FleetCapacity,1,65536)||!Int(Main,TEXT("maximum_vehicles"),MaximumVehicles,1,256)||!Int(Main,TEXT("maximum_projectiles"),MaximumProjectiles,1,10000)||!Int(Main,TEXT("maximum_queue"),MaximumQueue,1,128)||!Num(Main,TEXT("service_meters"),ServiceMeters,1,100)||!Num(Main,TEXT("charge_kw"),ChargeKW,.01,10000)||!Num(Main,TEXT("ammo_buffer_shots"),AmmoBufferShots,1,1000)||!Num(Main,TEXT("enemy_radius_meters"),EnemyRadiusMeters,.1,20)||!Num(Main,TEXT("shield_regen_hp_s"),ShieldRegen,0,10000)||!Num(Main,TEXT("shield_regen_kwh_hp"),ShieldRegenKWh,.000001,100)||!Num(Main,TEXT("shield_delay_seconds"),ShieldDelay,0,3600)||!Num(Main,TEXT("formation_spacing_meters"),FormationSpacingMeters,.1,100)||!Num(Main,TEXT("rough_grade"),RoughGrade,.001,1)||!Num(Main,TEXT("road_speed_multiplier"),RoadSpeedMultiplier,1,4)||!Num(Main,TEXT("bug_range_meters"),BugRangeMeters,1,1000)||!Num(Main,TEXT("bug_fire_interval"),BugFireInterval,.1,600)||!Num(Main,TEXT("bug_ranged_fraction"),BugRangedFraction,0,1)||!Num(Main,TEXT("loot_range_meters"),LootRangeMeters,1,100)||!Num(Main,TEXT("loot_kg_s"),LootKgPerSecond,.01,10000)||!Num(Main,TEXT("refit_energy_fraction"),RefitEnergyFraction,.001,1)||!Str(Main,TEXT("bug_weapon"),BugWeapon)||!Weapons.Contains(BugWeapon))return Fail(TEXT("invalid combat policies"));
    if(!List(Main,TEXT("loot_priority"),LootPriority)||LootPriority.Num()!=3)return Fail(TEXT("loot_priority must contain three ordered criteria"));
    TSet<FString> LootCriteria;for(const auto& Criterion:LootPriority)
    {if(!OneOf(Criterion,{TEXT("tier_desc"),TEXT("owned_quantity_asc"),TEXT("resource_id_asc")})||LootCriteria.Contains(Criterion))return Fail(TEXT("loot_priority criteria must be known and unique"));LootCriteria.Add(Criterion);}
    const O* Platforms=nullptr;const O* Limits=nullptr;const O* FactoryDefs=nullptr;
    if(!Main->TryGetObjectField(TEXT("building_platforms"),Platforms)||!Main->TryGetObjectField(TEXT("core_fleet_limits"),Limits)||!Main->TryGetObjectField(TEXT("factories"),FactoryDefs))return Fail(TEXT("platforms/factories/limits missing"));
    for(const auto& Pair:(*Platforms)->Values){FString Id(Pair.Key);O J=Pair.Value->AsObject();FSeigeCombatPlatform P;if(!Sim.BuildingDefs.Contains(Id)||!Int(J,TEXT("mount_points"),P.MountPoints,0,256)||!Num(J,TEXT("max_weapon_mass_kg"),P.MaxWeaponMassKg,0,1.e7)||!Num(J,TEXT("shield_hp"),P.Shield,0,1.e7)||!Num(J,TEXT("armor_hp"),P.Armor,0,1.e7)||!List(J,TEXT("loadout"),P.Weapons)||!Fits(P.MountPoints,P.MaxWeaponMassKg,P.Weapons,Error))return Fail(TEXT("invalid platform: ")+Id);BuildingPlatforms.Add(Id,P);}
    for(const auto& Pair:(*Limits)->Values){double N=0;FString Id(Pair.Key);if(!Sim.BuildingDefs.Contains(Id)||!Pair.Value->TryGetNumber(N)||N<1||N>3||N!=FMath::FloorToDouble(N))return Fail(TEXT("core fleet limit"));CoreFleetLimits.Add(Id,int32(N));}
    if(CoreFleetLimits.FindRef(Sim.CoreDefinition)!=1)return Fail(TEXT("starting core must permit one fleet"));
    for(const auto& Pair:(*FactoryDefs)->Values){O J=Pair.Value->AsObject();FSeigeFactoryCapability F;FString Id(Pair.Key);if(!Sim.BuildingDefs.Contains(Id)||!Str(J,TEXT("family"),F.Family)||!OneOf(F.Family,{TEXT("all"),TEXT("wheeled"),TEXT("tracked"),TEXT("mech")})||!Int(J,TEXT("maximum_tier"),F.MaximumTier,1,4)||!Num(J,TEXT("speed_multiplier"),F.SpeedMultiplier,.01,100))return Fail(TEXT("factory capability"));Factories.Add(Id,F);}
    Random.Initialize(Sim.GenerationSeed^0x3151A);EnsureBuildings(Sim);const auto* Core=Sim.Core();
    if(!Main->TryGetArrayField(TEXT("initial_vehicles"),Values)||Values->Num()>MaximumVehicles)return Fail(TEXT("initial vehicles"));
    if(Values->Num()>0&&Core){FSeigeFleet F;F.Id=NextId++;F.Name=TEXT("Shuttle guard");F.Destination=Sim.BuildingAccessPoint(*Core);Fleets.Add(F);}
    for(const auto& V:*Values){O J=V->AsObject();FString Id;TArray<FString> Loadout;double Battery=0;if(!Str(J,TEXT("chassis"),Id)||!List(J,TEXT("loadout"),Loadout)||!ValidateLoadout(Id,Loadout,Error)||!Num(J,TEXT("battery_kwh"),Battery,0,Chassis[Id].BatteryKWh)||!Core)return Fail(TEXT("initial vehicle loadout"));auto Vehicle=Spawn(Sim,Id,Loadout,Sim.BuildingAccessPoint(*Core)+FVector2D(0,(Vehicles.Num()+1)*FormationSpacingMeters/MetresPerUnit),Battery);Vehicle.FleetId=Fleets[0].Id;Vehicle.Embarked=true;Vehicle.Status=TEXT("Aboard parked shuttle");Vehicles.Add(Vehicle);}
    if(Fleets.Num()&&FleetUsed(Fleets[0].Id)>FleetCapacity)return Fail(TEXT("starting fleet capacity"));Error.Empty();return true;
}
