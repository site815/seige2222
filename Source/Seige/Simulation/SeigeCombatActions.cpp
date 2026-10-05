#include "SeigeCombat.h"
#include "SeigeSimulation.h"
#include "SeigeCombatJson.h"
using namespace SeigeCombatJson;
namespace
{
void AddCost(TMap<FString,double>& A,const TMap<FString,double>& B){for(const auto& X:B)A.FindOrAdd(X.Key)+=X.Value;}
void Pay(TMap<FString,double>& A,const TMap<FString,double>& B){for(const auto& X:B)A.FindOrAdd(X.Key)=FMath::Max(0.,A.FindRef(X.Key)-X.Value);}
}
FSeigeVehicle FSeigeCombatSystem::Spawn(const FSeigeSimulation& Sim,const FString& Id,const TArray<FString>& Loadout,FVector2D Position,double Battery)
{const auto& C=Chassis[Id];FSeigeVehicle V;V.Id=NextId++;V.ChassisId=Id;V.Position=V.Destination=Position;V.Health=C.Health;V.Armor=C.Armor;V.Shield=C.Shield;V.BatteryKWh=Battery;V.Weapons=Loadout;V.Cooldowns.Init(0,Loadout.Num());V.Status=TEXT("Ready");return V;}
void FSeigeCombatSystem::EnsureBuildings(const FSeigeSimulation& Sim)
{for(const auto& B:Sim.Buildings)if(const auto* D=BuildingPlatforms.Find(B.DefId)){auto* State=BuildingState.Find(B.Id);if(!State||State->Definition!=B.DefId){FSeigeBuildingCombatState S;S.Id=B.Id;S.Definition=B.DefId;S.Shield=State?FMath::Min(State->Shield,D->Shield):D->Shield;S.Armor=State?FMath::Min(State->Armor,D->Armor):D->Armor;S.Weapons=D->Weapons;S.Cooldowns.Init(0,S.Weapons.Num());BuildingState.Add(B.Id,S);}}}
FSeigeVehicle* FSeigeCombatSystem::FindVehicle(int32 Id){return Vehicles.FindByPredicate([&](const auto& V){return V.Id==Id;});}
const FSeigeVehicle* FSeigeCombatSystem::FindVehicle(int32 Id) const{return Vehicles.FindByPredicate([&](const auto& V){return V.Id==Id;});}
int32 FSeigeCombatSystem::FleetLimit(const FSeigeSimulation& Sim) const{const auto* Core=Sim.Core();return Core&&Core->Health>0?CoreFleetLimits.FindRef(Core->DefId):0;}
int32 FSeigeCombatSystem::FleetUsed(int32 Id) const{int32 Used=0;for(const auto& V:Vehicles)if(V.Health>0&&V.FleetId==Id)if(const auto* D=Chassis.Find(V.ChassisId))Used+=D->CapacityPoints;return Used;}
bool FSeigeCombatSystem::CreateFleet(const FSeigeSimulation& Sim,const FString& Name,int32& OutId,FString& Error)
{if(Name.TrimStartAndEnd().IsEmpty()||Name.Len()>64||Fleets.Num()>=FleetLimit(Sim)){Error=TEXT("Fleet name or command-core fleet limit");return false;}FSeigeFleet F;F.Id=NextId++;F.Name=Name;F.Destination=Sim.Core()?Sim.BuildingAccessPoint(*Sim.Core()):FVector2D::ZeroVector;Fleets.Add(F);OutId=F.Id;Error.Empty();return true;}
bool FSeigeCombatSystem::AssignVehicle(const FSeigeSimulation& Sim,int32 VehicleId,int32 FleetId,FString& Error)
{auto* V=FindVehicle(VehicleId);if(!V||V->Health<=0||V->Evacuated||V->Embarked||!Fleets.ContainsByPredicate([&](const auto& F){return F.Id==FleetId;})||(V->FleetId!=FleetId&&FleetUsed(FleetId)+Chassis[V->ChassisId].CapacityPoints>FleetCapacity)){Error=TEXT("Invalid vehicle/fleet or fleet capacity exceeded");return false;}V->FleetId=FleetId;V->Route.Empty();V->NextWaypoint=0;Error.Empty();return true;}
bool FSeigeCombatSystem::OrderFleet(const FSeigeSimulation& Sim,int32 Id,const FString& Mission,FVector2D Destination,int32 Escort,FString& Error)
{auto* F=Fleets.FindByPredicate([&](const auto& V){return V.Id==Id;});const auto* B=Sim.FindBuilding(Escort);if(!F||!OneOf(Mission,{TEXT("defense"),TEXT("escort"),TEXT("move")})||!FMath::IsFinite(Destination.X)||!FMath::IsFinite(Destination.Y)||FMath::Abs(Destination.X)>Sim.WorldHalfSize||FMath::Abs(Destination.Y)>Sim.WorldHalfSize||(Mission==TEXT("escort")&&(!B||B->Health<=0))){Error=TEXT("Invalid local mission/target; privateering requires the world mission bridge");return false;}F->Mission=Mission;F->DestinationSector=4;F->Destination=Destination;F->EscortBuildingId=Escort;for(auto& V:Vehicles)if(V.FleetId==Id&&!V.Evacuated){V.ReturningCargo|=V.SectorIndex!=4;V.Embarked=false;V.Route.Empty();V.NextWaypoint=0;}Error.Empty();return true;}
bool FSeigeCombatSystem::SetAggression(int32 Id,const FString& A,FString& Error)
{auto* F=Fleets.FindByPredicate([&](const auto& V){return V.Id==Id;});if(!F||!OneOf(A,{TEXT("passive"),TEXT("defensive"),TEXT("aggressive")})){Error=TEXT("Invalid fleet aggression");return false;}F->Aggression=A;Error.Empty();return true;}
bool FSeigeCombatSystem::QueueVehicle(FSeigeSimulation& Sim,int32 FactoryId,const FString& Id,const TArray<FString>& Loadout,FString& Error)
{
    auto* B=Sim.FindBuilding(FactoryId);const auto* C=Chassis.Find(Id);const auto* F=B?Factories.Find(B->DefId):nullptr;
    if(!B||B->Health<=0||B->IsConstructing||!B->Enabled||!C||!F||C->Tier>F->MaximumTier||(F->Family!=TEXT("all")&&F->Family!=C->Family)||Vehicles.Num()+Fabrication.Num()>=MaximumVehicles||Fabrication.Num()>=MaximumQueue||!ValidateLoadout(Id,Loadout,Error)){Error=TEXT("Unavailable factory, chassis tier, queue or loadout");return false;}
    if(!SetFabricationPlan(Sim,FactoryId,Id,Loadout,Error))return false;
    TMap<FString,double> Cost=C->Cost;for(const auto& W:Loadout)AddCost(Cost,Weapons[W].Cost);
    if(!Sim.HasSpendable(*B,Cost)||!Sim.Energy.CanConsume(Sim,B->Id,C->BuildKWh)){Error=TEXT("Factory needs local materials and stored grid energy");return false;}
    if(!Sim.Energy.Consume(Sim,B->Id,C->BuildKWh)){Error=TEXT("Factory energy unavailable");return false;}Pay(B->Inventory,Cost);EnergySpentKWh+=C->BuildKWh;
    FSeigeFabrication J;J.Id=NextId++;J.FactoryId=FactoryId;J.FactoryDefinition=B->DefId;J.ChassisId=Id;J.Weapons=Loadout;J.RequiredSeconds=C->BuildSeconds/F->SpeedMultiplier;J.PaidMaterials=Cost;Fabrication.Add(J);FabricationPlans.Remove(FactoryId);Error.Empty();return true;
}
bool FSeigeCombatSystem::SetFabricationPlan(const FSeigeSimulation& Sim,int32 FactoryId,const FString& Id,const TArray<FString>& Loadout,FString& Error)
{
    const auto* B=Sim.FindBuilding(FactoryId);const auto* C=Chassis.Find(Id);const auto* F=B?Factories.Find(B->DefId):nullptr;
    if(!B||B->Health<=0||!C||!F||C->Tier>F->MaximumTier||(F->Family!=TEXT("all")&&F->Family!=C->Family)||!ValidateLoadout(Id,Loadout,Error)){Error=TEXT("Invalid factory fabrication plan");return false;}
    TMap<FString,double> Bill=C->Cost;for(const auto& W:Loadout)AddCost(Bill,Weapons[W].Cost);
    if(Sim.InventoryLitres(Bill)>Sim.Definition(*B)->StorageCapacity){Error=TEXT("Fabrication bill exceeds this factory's local storage; upgrade it");return false;}
    FSeigeFabrication Plan;Plan.FactoryId=FactoryId;Plan.ChassisId=Id;Plan.Weapons=Loadout;FabricationPlans.Add(FactoryId,Plan);Error.Empty();return true;
}
bool FSeigeCombatSystem::Refit(FSeigeSimulation& Sim,int32 Id,const TArray<FString>& Loadout,FString& Error)
{
    auto* V=FindVehicle(Id);if(!V||V->Health<=0||V->Evacuated||V->Embarked||V->SectorIndex!=4){Error=TEXT("Deploy a living vehicle in the home sector before refitting");return false;}if(!ValidateLoadout(V->ChassisId,Loadout,Error))return false;
    for(auto& B:Sim.Buildings)if(B.Health>0&&!B.IsConstructing&&B.Enabled&&Factories.Contains(B.DefId)&&FVector2D::Distance(V->Position,Sim.BuildingAccessPoint(B))*MetresPerUnit<=ServiceMeters)
    {TMap<FString,double> Cost;for(const auto& W:Loadout)AddCost(Cost,Weapons[W].Cost);const double Energy=Chassis[V->ChassisId].BuildKWh*RefitEnergyFraction;if(!Sim.HasSpendable(B,Cost)||!Sim.Energy.CanConsume(Sim,B.Id,Energy))continue;if(!Sim.Energy.Consume(Sim,B.Id,Energy))continue;Pay(B.Inventory,Cost);EnergySpentKWh+=Energy;V->Weapons=Loadout;V->Cooldowns.Init(0,Loadout.Num());V->Route.Empty();V->NextWaypoint=0;RefitPlans.Remove(B.Id);Error.Empty();return true;}
    Error=TEXT("Refit needs a nearby factory with local parts/energy; removed modules are not refunded");return false;
}
bool FSeigeCombatSystem::TransferCargo(FSeigeSimulation& Sim,int32 VehicleId,int32 BuildingId,const FString& Resource,double Amount,bool ToVehicle,FString& Error)
{
    auto* V=FindVehicle(VehicleId);auto* B=Sim.FindBuilding(BuildingId);if(!V||V->Health<=0||V->Evacuated||V->Embarked||V->SectorIndex!=4||!B||B->Health<=0||B->IsConstructing||!Sim.Resources.Contains(Resource)||!FMath::IsFinite(Amount)||Amount<=0||FVector2D::Distance(V->Position,Sim.BuildingAccessPoint(*B))*MetresPerUnit>ServiceMeters){Error=TEXT("Cargo requires positive quantity and physical service proximity");return false;}
    const auto& R=Sim.Resources[Resource];if(R.Discrete&&Amount!=FMath::FloorToDouble(Amount)){Error=TEXT("Discrete cargo requires whole units");return false;}if(ToVehicle&&Resource==Sim.TextRule(TEXT("inactive_worker_resource"))&&B->Inventory.FindRef(Resource)-Amount<B->DisassemblyQueued-(B->DisassemblyCommitted?1:0)){Error=TEXT("Those stored workers are reserved for disassembly");return false;}const auto& C=Chassis[V->ChassisId];auto& From=ToVehicle?B->Inventory:V->Inventory;auto& To=ToVehicle?V->Inventory:B->Inventory;
    if((ToVehicle?Sim.Spendable(*B,Resource):From.FindRef(Resource))+1.e-8<Amount||(ToVehicle&&(Sim.InventoryLitres(To)+Amount*R.LitresPerUnit>C.StorageLitres+1.e-8||Sim.InventoryMassKg(To)+Amount*R.UnitMassKg>C.CargoMassKg+1.e-8))||(!ToVehicle&&Sim.StorageUsed(*B)+Amount*R.LitresPerUnit>Sim.Definition(*B)->StorageCapacity+1.e-8)){Error=TEXT("Insufficient local cargo or mass/volume capacity");return false;}
    From.FindOrAdd(Resource)=FMath::Max(0.,From.FindRef(Resource)-Amount);To.FindOrAdd(Resource)+=Amount;Error.Empty();return true;
}
double FSeigeCombatSystem::CargoStock(const FString& Resource) const{double Total=0;for(const auto& V:Vehicles)if(V.Health>0&&!V.Evacuated)Total+=V.Inventory.FindRef(Resource);for(const auto& J:Fabrication)Total+=J.PaidMaterials.FindRef(Resource);return Total;}
bool FSeigeCombatSystem::IsVisible(FVector2D P) const{for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.BatteryKWh>0)if(const auto* C=Chassis.Find(V.ChassisId))if(FVector2D::Distance(P,V.Position+SectorOffset(V.SectorIndex))*MetresPerUnit<=C->SensorMeters)return true;return false;}
double FSeigeCombatSystem::Demand(const FSeigeSimulation& Sim,int32 Id,const FString& Resource) const
{
    const auto* B=Sim.FindBuilding(Id);if(!B)return 0;double Need=0;
    if(const auto* P=BuildingPlatforms.Find(B->DefId))for(const auto& W:BuildingState.Contains(B->Id)?BuildingState[B->Id].Weapons:P->Weapons)if(Weapons[W].Ammo==Resource)Need+=AmmoBufferShots*Weapons[W].AmmoPerShot;
    if(const auto* Plan=FabricationPlans.Find(Id)){Need+=Chassis[Plan->ChassisId].Cost.FindRef(Resource);for(const auto& W:Plan->Weapons)Need+=Weapons[W].Cost.FindRef(Resource);}
    if(const auto* Refit=RefitPlans.Find(Id))for(const auto& W:*Refit)Need+=Weapons[W].Cost.FindRef(Resource);
    if(Factories.Contains(B->DefId))for(const auto& W:Weapons)if(W.Value.Ammo==Resource)Need=FMath::Max(Need,AmmoBufferShots*W.Value.AmmoPerShot);
    return Need;
}
void FSeigeCombatSystem::ShiftHome(FVector2D D){for(auto& V:Vehicles)if(V.SectorIndex==4){V.Position+=D;V.Destination+=D;for(auto& P:V.Route)P+=D;}for(auto& F:Fleets)if(F.DestinationSector==4)F.Destination+=D;for(auto& P:Projectiles)if(P.SectorIndex==4){P.Position+=D;P.PreviousPosition+=D;}}

bool FSeigeCombatSystem::RefitBuilding(FSeigeSimulation& Sim,int32 Id,const TArray<FString>& Loadout,FString& Error)
{
    auto* B=Sim.FindBuilding(Id);const auto* P=B?BuildingPlatforms.Find(B->DefId):nullptr;
    if(!B||B->Health<=0||B->IsConstructing||!B->Enabled||!P||!Fits(P->MountPoints,P->MaxWeaponMassKg,Loadout,Error)){Error=TEXT("Building is unavailable or loadout exceeds its hardpoints");return false;}
    TMap<FString,double> Cost;double Energy=0;for(const auto& W:Loadout){AddCost(Cost,Weapons[W].Cost);Energy+=Weapons[W].EnergyKWh;}
    if(!Sim.HasSpendable(*B,Cost)||!Sim.Energy.CanConsume(Sim,Id,Energy)){Error=TEXT("Building refit needs local weapon materials and energy");return false;}
    if(!Sim.Energy.Consume(Sim,Id,Energy))return false;Pay(B->Inventory,Cost);EnergySpentKWh+=Energy;EnsureBuildings(Sim);auto& S=BuildingState[Id];S.Weapons=Loadout;S.Cooldowns.Init(0,Loadout.Num());RefitPlans.Remove(Id);Error.Empty();return true;
}

bool FSeigeCombatSystem::SetRefitPlan(const FSeigeSimulation& Sim,int32 Id,const TArray<FString>& Loadout,FString& Error)
{
    const auto* B=Sim.FindBuilding(Id);if(!B||B->Health<=0||Loadout.Num()>128||(!Factories.Contains(B->DefId)&&!BuildingPlatforms.Contains(B->DefId))){Error=TEXT("Invalid equipment service building");return false;}
    TMap<FString,double> Bill;for(const auto& W:Loadout){if(!Weapons.Contains(W)||W==BugWeapon){Error=TEXT("Unknown player weapon");return false;}AddCost(Bill,Weapons[W].Cost);}
    if(const auto* P=BuildingPlatforms.Find(B->DefId))if(!Fits(P->MountPoints,P->MaxWeaponMassKg,Loadout,Error))return false;
    if(Sim.InventoryLitres(Bill)>Sim.Definition(*B)->StorageCapacity){Error=TEXT("Equipment bill exceeds local storage; upgrade storage capacity");return false;}
    RefitPlans.Add(Id,Loadout);Error.Empty();return true;
}

bool FSeigeCombatSystem::BoardFleet(const FSeigeSimulation& Sim,int32 FleetId,FString& Error)
{
    const auto* Core=Sim.Core();const auto* Fleet=Fleets.FindByPredicate([&](const auto& F){return F.Id==FleetId;});
    if(!Core||Core->Health<=0||Core->IsConstructing||Sim.Escaped||!Fleet){Error=TEXT("Shuttle docking needs a deployed, operational command core");return false;}
    bool Found=false;for(const auto& V:Vehicles)if(V.Health>0&&!V.Evacuated){if(V.Embarked&&V.FleetId!=FleetId){Error=TEXT("The shuttle carries one fleet; deploy its current fleet first");return false;}if(V.FleetId==FleetId){Found=true;if(V.SectorIndex!=4||FVector2D::Distance(V.Position,Sim.BuildingAccessPoint(*Core))*MetresPerUnit>ServiceMeters){Error=TEXT("Bring every surviving fleet vehicle to the command-core service port before boarding");return false;}}}
    if(!Found){Error=TEXT("Fleet has no surviving vehicles to board");return false;}
    for(auto& V:Vehicles)if(V.FleetId==FleetId&&V.Health>0&&!V.Evacuated){V.Embarked=true;V.ReturningCargo=false;V.Route.Empty();V.NextWaypoint=0;V.Status=TEXT("Aboard parked shuttle");}Error.Empty();return true;
}
void FSeigeCombatSystem::EvacuateShuttle()
{for(auto& V:Vehicles)if(V.Embarked&&V.Health>0){V.Evacuated=true;V.Route.Empty();V.NextWaypoint=0;V.Status=TEXT("Evacuated aboard shuttle");}}
double FSeigeCombatSystem::ShuttleFleetCargoStock(const FString& Resource) const
{double Total=0;for(const auto& V:Vehicles)if(V.Health>0&&V.Evacuated)Total+=V.Inventory.FindRef(Resource);return Total;}
