#pragma once
#include "CoreMinimal.h"
class FSeigeSimulation;
class FJsonObject;

struct FSeigeWeaponDef
{
    FString Id,Name,Family,Size,Ammo;
    int32 MountPoints=1;
    double MassKg=0,Damage=0,ReloadSeconds=1,RangeMeters=1,SpeedMetersSecond=0;
    double AccuracyDegrees=0,SplashMeters=0,EnergyKWh=0,AmmoPerShot=0,ShieldMultiplier=1,ArmorMultiplier=1,HomingDegreesSecond=0;
    TMap<FString,double> Cost;
};
struct FSeigeHardpointSize
{
    double WidthMeters=0,HeightMeters=0,LengthMeters=0,MassLimitKg=0;
    int32 MountPoints=0;
};
struct FSeigeChassisDef
{
    FString Id,Name,Family,Size;
    int32 CapacityPoints=1,MountPoints=1,Wheels=0,Legs=0,Tier=1;
    double MassKg=1,MaxWeaponMassKg=1,CargoMassKg=1,StorageLitres=1,Health=1,Armor=0,Shield=0;
    double BatteryKWh=0,SpeedKmh=1,RadiusMeters=1,SensorMeters=1,MaxGrade=1,RoughSpeedMultiplier=1,TravelKWhPerKm=0,RoadDamagePerMeter=0;
    double BuildSeconds=1,BuildKWh=0;
    TMap<FString,double> Cost;
};
struct FSeigeCombatPlatform
{
    int32 MountPoints=0;
    double MaxWeaponMassKg=0,Shield=0,Armor=0;
    TArray<FString> Weapons;
};
struct FSeigeBuildingCombatState
{
    int32 Id=0;
    FString Definition;
    double Shield=0,Armor=0,LastDamageTime=-1;
    TArray<double> Cooldowns;
    TArray<FString> Weapons;
};
struct FSeigeVehicle
{
    int32 Id=0,FleetId=0,NextWaypoint=0,SectorIndex=4;
    bool Embarked=false,Evacuated=false,ReturningCargo=false;
    FString ChassisId,Status;
    FVector2D Position=FVector2D::ZeroVector,Destination=FVector2D::ZeroVector;
    double Heading=0,Health=1,Armor=0,Shield=0,BatteryKWh=0,LastDamageTime=-1,DistanceMeters=0,LootCreditKg=0;
    TArray<FString> Weapons;
    TArray<double> Cooldowns;
    TArray<FVector2D> Route;
    TMap<FString,double> Inventory;
};
struct FSeigeFleet
{
    int32 Id=0,EscortBuildingId=0,DestinationSector=4;
    FString Name,Mission=TEXT("defense"),Aggression=TEXT("defensive");
    FVector2D Destination=FVector2D::ZeroVector;
};
struct FSeigeProjectile
{
    int32 Id=0,OwnerId=0,TargetEnemyId=0,SectorIndex=4,OwnerSector=4;
    // building, vehicle, or enemy. Friendly projectiles can hit friendly bodies.
    FString OwnerKind,WeaponId,TargetKind=TEXT("enemy");
    FVector2D Position=FVector2D::ZeroVector,PreviousPosition=FVector2D::ZeroVector,Velocity=FVector2D::ZeroVector;
    double RemainingMeters=0,AgeSeconds=0;
    // Presentation-only source socket; absent on restored in-flight rounds.
    int32 WeaponSlot=INDEX_NONE;
};
struct FSeigeFabrication
{
    int32 Id=0,FactoryId=0;
    FString ChassisId,FactoryDefinition;
    TArray<FString> Weapons;
    double ProgressSeconds=0,RequiredSeconds=1;
    TMap<FString,double> PaidMaterials;
};
struct FSeigeFactoryCapability
{
    FString Family;
    int32 MaximumTier=4;
    double SpeedMultiplier=1;
};
struct FSeigeCombatShot
{
    double Time=0;
    int32 OwnerId=0;
    FString OwnerKind,Family;
    FVector2D Start=FVector2D::ZeroVector,End=FVector2D::ZeroVector;
    bool Hit=false;
    FString WeaponId;
    int32 WeaponSlot=INDEX_NONE,OwnerSector=4;
};

// Authoritative, deterministic combat and fleet state. Renderers only read it.
class SEIGE_API FSeigeCombatSystem
{
public:
    bool Initialize(const FString& Directory,FSeigeSimulation& Sim,FString& Error);
    void Tick(FSeigeSimulation& Sim,double Seconds,bool DefensiveReservePrepared=false);
    double Demand(const FSeigeSimulation& Sim,int32 BuildingId,const FString& Resource) const;
    double AmmoDemand(const FSeigeSimulation& Sim,int32 BuildingId,const FString& Resource) const;
    double CargoStock(const FString& Resource) const;
    bool IsVisible(FVector2D Position) const;
    FString BuildingFireStatus(const FSeigeSimulation& Sim,int32 BuildingId) const;
    void CollectPendingShotEnergy(const FSeigeSimulation& Sim,double Seconds,TMap<int32,double>& ByGrid) const;
    void Save(const TSharedPtr<FJsonObject>& Root) const;
    bool Load(const TSharedPtr<FJsonObject>& Root,FSeigeSimulation& Sim,FString& Error);
    void ShiftHome(FVector2D Delta);
    const FString& GetFingerprint() const{return Fingerprint;}
    void SetTerrainSampler(TFunction<double(FVector2D)> Sampler){TerrainHeight=MoveTemp(Sampler);}
    void SetSectorResolver(TFunction<const FSeigeSimulation*(int32)> Resolver){SectorResolver=MoveTemp(Resolver);}
    bool ValidateLoadout(const FString& ChassisId,const TArray<FString>& Loadout,FString& Error) const;
    bool QueueVehicle(FSeigeSimulation& Sim,int32 FactoryId,const FString& ChassisId,const TArray<FString>& Loadout,FString& Error);
    bool SetFabricationPlan(const FSeigeSimulation& Sim,int32 FactoryId,const FString& ChassisId,const TArray<FString>& Loadout,FString& Error);
    bool Refit(FSeigeSimulation& Sim,int32 VehicleId,const TArray<FString>& Loadout,FString& Error);
    bool RefitBuilding(FSeigeSimulation& Sim,int32 BuildingId,const TArray<FString>& Loadout,FString& Error);
    bool SetRefitPlan(const FSeigeSimulation& Sim,int32 BuildingId,const TArray<FString>& Loadout,FString& Error);
    bool TransferCargo(FSeigeSimulation& Sim,int32 VehicleId,int32 BuildingId,const FString& Resource,double Amount,bool ToVehicle,FString& Error);
    bool CreateFleet(const FSeigeSimulation& Sim,const FString& Name,int32& OutId,FString& Error);
    bool AssignVehicle(const FSeigeSimulation& Sim,int32 VehicleId,int32 FleetId,FString& Error);
    bool OrderFleet(const FSeigeSimulation& Sim,int32 FleetId,const FString& Mission,FVector2D Destination,int32 EscortBuildingId,FString& Error);
    bool OrderPrivateer(const FSeigeSimulation& Home,const FSeigeSimulation& Target,int32 FleetId,int32 SectorIndex,FVector2D Destination,FString& Error);
    bool BoardFleet(const FSeigeSimulation& Sim,int32 FleetId,FString& Error);
    void EvacuateShuttle();
    double ShuttleFleetCargoStock(const FString& Resource) const;
    void TickExternalSector(FSeigeSimulation& Home,FSeigeSimulation& Target,int32 SectorIndex,double Seconds);
    bool IsVisibleInSector(int32 SectorIndex,FVector2D Position) const;
    bool SetAggression(int32 FleetId,const FString& Aggression,FString& Error);
    int32 FleetLimit(const FSeigeSimulation& Sim) const;
    int32 FleetUsed(int32 Id) const;
    FSeigeVehicle* FindVehicle(int32 Id);
    const FSeigeVehicle* FindVehicle(int32 Id) const;
    void DamageBuilding(FSeigeSimulation& Sim,int32 Id,double Damage,const FString& Family=TEXT("kinetic"),double ShieldMultiplier=-1,double ArmorMultiplier=-1);
    void DamageVehicle(FSeigeSimulation& Sim,int32 Id,double Damage,const FString& Family,double ShieldMultiplier=-1,double ArmorMultiplier=-1);
    TMap<FString,FSeigeHardpointSize> HardpointSizes;
    TMap<FString,FSeigeWeaponDef> Weapons;
    TMap<FString,FSeigeChassisDef> Chassis;
    TMap<FString,FSeigeCombatPlatform> BuildingPlatforms;
    TMap<FString,FSeigeFactoryCapability> Factories;
    TMap<int32,FSeigeBuildingCombatState> BuildingState;
    TArray<FSeigeVehicle> Vehicles;
    TArray<FSeigeFleet> Fleets;
    TArray<FSeigeProjectile> Projectiles;
    TArray<FSeigeFabrication> Fabrication;
    TMap<int32,FSeigeFabrication> FabricationPlans;
    TMap<int32,TArray<FString>> RefitPlans;
    // Short-lived presentation events; excluded from deterministic game saves.
    TArray<FSeigeCombatShot> ShotEvents;
    int32 FleetCapacity=50;
    double ShotsFired=0,Hits=0,EnergySpentKWh=0,AmmoSpent=0;
private:
    friend class FSeigeEnergySystem;
    FString Fingerprint,BugWeapon;
    int32 NextId=1,MaximumVehicles=150,MaximumProjectiles=2048,MaximumQueue=8;
    double MetresPerUnit=.06,SectorHalfSize=30000,ServiceMeters=20,ChargeKW=40,AmmoBufferShots=16,EnemyRadiusMeters=1.2;
    double ShieldRegen=2,ShieldRegenKWh=.002,ShieldDelay=10,FormationSpacingMeters=5;
    double RoughGrade=.08,RoadSpeedMultiplier=1.25,BugRangeMeters=80,BugFireInterval=5,BugRangedFraction=.25;
    double LootRangeMeters=12,LootKgPerSecond=10,RefitEnergyFraction=.1;
    TArray<FString> LootPriority;
    TMap<FString,int32> CoreFleetLimits;
    TMap<FString,FVector2D> DamageProfiles;
    TMap<int32,double> BugCooldowns;
    FRandomStream Random;
    TFunction<double(FVector2D)> TerrainHeight;
    TFunction<const FSeigeSimulation*(int32)> SectorResolver;
    bool Fits(int32 Points,double Mass,const TArray<FString>& Loadout,FString& Error) const;
    struct FFireControl {int32 TargetId=0;FString Status;bool Ready=false;};
    double BuildingWeaponWork(const FSeigeSimulation& Sim,int32 BuildingId) const;
    FFireControl QueryFire(const FSeigeSimulation& Sim,const FString& OwnerKind,int32 OwnerId,FVector2D Position,const FSeigeWeaponDef& Weapon,double Cooldown,bool RequireStoredEnergy=true) const;
    bool ClearFriendlyFire(const FSeigeSimulation& Sim,const FString& OwnerKind,int32 OwnerId,FVector2D From,FVector2D Aim,FString* Blocker=nullptr) const;
    bool DefensivePosition(const FSeigeSimulation& Sim,const FSeigeVehicle& Vehicle,FVector2D Anchor,FVector2D& Position) const;
    void EnsureBuildings(const FSeigeSimulation& Sim);
    void AdvanceProjectile(FSeigeSimulation& Sim,FSeigeProjectile& P,double Seconds,bool& Remove);
    bool Fire(FSeigeSimulation& Sim,const FString& Kind,int32 Owner,FVector2D Position,int32 Target,const FSeigeWeaponDef& Weapon,int32 WeaponSlot);
    void Impact(FSeigeSimulation& Sim,const FSeigeProjectile& P,const FString& HitKind,int32 HitId,FVector2D Position);
    void MoveVehicles(FSeigeSimulation& Sim,double Seconds);
    void ServiceVehicles(FSeigeSimulation& Sim,double Seconds);
    void AdvanceTransit(FSeigeSimulation& Sim,double Seconds);
    FVector2D SectorOffset(int32 Index) const;
    FSeigeVehicle Spawn(const FSeigeSimulation& Sim,const FString& Id,const TArray<FString>& Loadout,FVector2D Position,double Battery);
};
