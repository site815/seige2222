#include "SeigeTransportVehicles.h"
#include "SeigeSimulation.h"
#include "SeigeCombatJson.h"
#include "SeigeCanonicalJson.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
using namespace SeigeCombatJson;

bool FSeigeTransportVehicleSystem::Initialize(const FString& Directory,FSeigeSimulation& S,FString& Error)
{
    O Root;FString Raw;const O *Costs=nullptr,*Factories=nullptr;int32 Schema=0,Initial=0;const TArray<TSharedPtr<FJsonValue>>* Size=nullptr;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(Directory,TEXT("transport_vehicles.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Root)||!Int(Root,TEXT("schema_version"),Schema,1,1)||!Str(Root,TEXT("name"),Def.Name)||Def.Name.IsEmpty()||!Int(Root,TEXT("initial_count"),Initial,1,1)||!Int(Root,TEXT("shuttle_vehicle_capacity"),Def.ShuttleVehicleCapacity,1,1)||!Int(Root,TEXT("maximum_queue"),Def.MaximumQueue,1,128)||!Root->TryGetArrayField(TEXT("body_size_meters"),Size)||Size->Num()!=3||!Root->TryGetObjectField(TEXT("build_cost"),Costs)||!Root->TryGetObjectField(TEXT("factories"),Factories))
    {Error=TEXT("Invalid autonomous transport rules");return false;}
    for(auto Pair:{TPair<const TCHAR*,double*>(TEXT("cargo_mass_kg"),&Def.CargoMassKg),{TEXT("cargo_litres"),&Def.CargoLitres},{TEXT("body_mass_kg"),&Def.BodyMassKg},{TEXT("health"),&Def.Health},{TEXT("battery_kwh"),&Def.BatteryKWh},{TEXT("initial_battery_kwh"),&Def.InitialBatteryKWh},{TEXT("travel_kwh_per_km"),&Def.TravelKWhPerKm},{TEXT("charging_kw"),&Def.ChargingKW},{TEXT("handling_seconds_per_drone_load"),&Def.HandlingSecondsPerDroneLoad},{TEXT("build_seconds"),&Def.BuildSeconds},{TEXT("build_energy_kwh"),&Def.BuildEnergyKWh},{TEXT("service_meters"),&Def.ServiceMeters}})
        if(!Num(Root,Pair.Key,*Pair.Value,.000001,1.e9)){Error=FString(TEXT("Invalid transport field: "))+Pair.Key;return false;}
    if(Def.InitialBatteryKWh>Def.BatteryKWh||!Num(Root,TEXT("minimum_departure_charge_fraction"),Def.MinimumDepartureChargeFraction,.01,1)){Error=TEXT("Transport starting battery or departure reserve is invalid");return false;}
    for(int I=0;I<3;++I){double V=0;if(!(*Size)[I]->TryGetNumber(V)||!FMath::IsFinite(V)||V<=0||V>100){Error=TEXT("Invalid transport dimensions");return false;}Def.BodySizeMeters[I]=V;}
    Def.BuildCost.Empty();Def.Factories.Empty();double Mass=0;
    for(const auto& P:(*Costs)->Values){double V=0;const FString R(P.Key);const auto* D=S.Resources.Find(R);if(!D||D->Discrete||!P.Value->TryGetNumber(V)||!FMath::IsFinite(V)||V<=0){Error=TEXT("Transport bill requires positive physical material quantities");return false;}Def.BuildCost.Add(R,V);Mass+=V*D->UnitMassKg;}
    if(!FMath::IsNearlyEqual(Mass,Def.BodyMassKg,.001)){Error=TEXT("Transport material bill must conserve body mass");return false;}
    for(const auto& P:(*Factories)->Values){double V=0;const FString Id(P.Key);if(!S.BuildingDefs.Contains(Id)||S.BuildingDefs[Id].Role!=TEXT("vehicle_factory")||!P.Value->TryGetNumber(V)||!FMath::IsFinite(V)||V<=0||V>10){Error=TEXT("Invalid autonomous transport factory");return false;}Def.Factories.Add(Id,V);}
    if(Def.Factories.IsEmpty()){Error=TEXT("Transport needs a vehicle factory");return false;}
    Fingerprint=FMD5::HashAnsiString(*SeigeCanonicalJson(Root));Bodies.Empty();Jobs.Empty();NextId=NextJobId=1;
    if(const auto* Core=S.Core()){FSeigeTransportVehicle V;V.Id=NextId++;V.Health=Def.Health;V.BatteryKWh=Def.InitialBatteryKWh;V.Position=S.Workers.HatchPoint(S);V.Embarked=true;V.Status=TEXT("Aboard shuttle");Bodies.Add(V);}
    Error.Empty();return true;
}
FSeigeTransportVehicle* FSeigeTransportVehicleSystem::Find(int32 Id){return Bodies.FindByPredicate([&](const auto& V){return V.Id==Id;});}
const FSeigeTransportVehicle* FSeigeTransportVehicleSystem::Find(int32 Id) const{return Bodies.FindByPredicate([&](const auto& V){return V.Id==Id;});}
int32 FSeigeTransportVehicleSystem::LiveCount() const{int32 N=0;for(const auto& V:Bodies)if(V.Health>0&&!V.Evacuated)++N;return N;}
int32 FSeigeTransportVehicleSystem::EmbarkedCount() const{int32 N=0;for(const auto& V:Bodies)if(V.Health>0&&V.Embarked&&!V.Evacuated)++N;return N;}
double FSeigeTransportVehicleSystem::HaulUnits(const FSeigeSimulation& S,const FString& R) const
{const auto* D=S.Resources.Find(R);if(!D||R==S.TextRule(TEXT("inactive_worker_resource")))return 0;const double N=FMath::Min(Def.CargoMassKg/D->UnitMassKg,Def.CargoLitres/D->LitresPerUnit);return D->Discrete?FMath::FloorToDouble(N):N;}
bool FSeigeTransportVehicleSystem::Available(const FSeigeSimulation& S) const
{if(!S.DeploymentHatchOpen)return false;for(const auto& V:Bodies)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&!V.ReturningToShuttle&&!V.DeliveryId&&V.BatteryKWh>=Def.BatteryKWh*Def.MinimumDepartureChargeFraction&&V.Status!=TEXT("Leaving shuttle"))return true;return false;}
void FSeigeTransportVehicleSystem::ShiftHome(FVector2D Delta){for(auto& V:Bodies)V.Position+=Delta;}
void FSeigeTransportVehicleSystem::ReleaseHandler(FSeigeSimulation& S,FSeigeTransportVehicle& V)
{if(!V.HandlerWorkerId.IsEmpty())S.Workers.ReleaseTransportHandler(V.HandlerWorkerId,V.Id);V.HandlerWorkerId.Empty();}
bool FSeigeTransportVehicleSystem::Dispatch(FSeigeSimulation& S,int32 Source,int32 Target,int32 Road,const FString& R,double Amount,bool Construction,bool Deployment)
{
    if(!Available(S)||Deployment||HaulUnits(S,R)<=0)return false;const auto* A=S.FindBuilding(Source);const auto* B=S.FindBuilding(Target);const auto* Lane=S.FindRoad(Road);
    if(!A||A->Health<=0||A->IsConstructing||(!B&&!Lane)||(B&&B->Health<=0)||(Lane&&Lane->Health<=0))return false;
    // Both ends must have a physical workforce; never reserve cargo to an
    // unmanned permanent building that cannot supply an unloading handler.
    if(S.Definition(*A)->Jobs>0&&S.Workers.AssignedCount(A->Id)<=0)return false;
    if(B&&!Construction&&S.Definition(*B)->Jobs>0&&S.Workers.AssignedCount(B->Id)<=0)return false;
    Amount=FMath::Min3(Amount,HaulUnits(S,R),FMath::Max(0.,A->Inventory.FindRef(R)-S.Workers.PickupReserved(S,Source,R)));
    if(B&&!Construction)Amount=FMath::Min(Amount,S.StorageRoom(*B)/S.Resources[R].LitresPerUnit);
    if(S.Resources[R].Discrete)Amount=FMath::FloorToDouble(Amount+1.e-9);if(Amount<=1.e-9)return false;
    const FVector2D Pickup=S.BuildingAccessPoint(*A),Destination=Lane?S.RoadAccessPoint(*Lane):S.BuildingAccessPoint(*B);
    TArray<FVector2D> LoadedRoute;if(!S.FindMobilityRoute(TEXT("transport"),Pickup,Destination,LoadedRoute))return false;
    for(auto& V:Bodies)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&!V.ReturningToShuttle&&!V.DeliveryId&&V.BatteryKWh>=Def.BatteryKWh*Def.MinimumDepartureChargeFraction&&V.Status!=TEXT("Leaving shuttle"))
    {
        TArray<FVector2D> Route;if(!S.FindMobilityRoute(TEXT("transport"),V.Position,Pickup,Route))continue;
        double Distance=0;FVector2D Previous=V.Position;for(const auto& P:Route){Distance+=FVector2D::Distance(Previous,P);Previous=P;}Previous=Pickup;for(const auto& P:LoadedRoute){Distance+=FVector2D::Distance(Previous,P);Previous=P;}if(V.BatteryKWh<Distance*S.MetersPerWorldUnit()/1000*Def.TravelKWhPerKm+Def.BatteryKWh*Def.MinimumDepartureChargeFraction)continue;
        FSeigeCourier C;C.Id=S.NextId++;C.TransportVehicleId=V.Id;C.SourceId=Source;C.TargetId=Target;C.RoadTargetId=Road;C.Resource=R;C.ReservedAmount=Amount;C.ForConstruction=Construction;C.Position=V.Position;
        S.Couriers.Add(C);V.DeliveryId=C.Id;V.Route=MoveTemp(Route);V.NextWaypoint=0;V.RouteRevision=S.TransportRevision;V.Status=TEXT("Collecting cargo");return true;
    }
    return false;
}
bool FSeigeTransportVehicleSystem::Move(FSeigeSimulation& S,FSeigeTransportVehicle& V,FVector2D Destination,double Seconds)
{
    if(FVector2D::Distance(V.Position,Destination)*S.MetersPerWorldUnit()<=S.Mobility.AccessMeters(TEXT("transport")))return true;
    if(V.Route.IsEmpty()||V.RouteRevision!=S.TransportRevision||FVector2D::Distance(V.Route.Last(),Destination)>.001)
    {
        if(S.Time<V.RetryAt)return false;V.Route.Reset();V.NextWaypoint=0;V.RouteRevision=S.TransportRevision;
        if(!S.FindMobilityRoute(TEXT("transport"),V.Position,Destination,V.Route)){V.RetryAt=S.Time+2;V.Status=TEXT("Waiting for accessible route");return false;}
    }
    while(V.Route.IsValidIndex(V.NextWaypoint)&&Seconds>1.e-9)
    {
        const FVector2D End=V.Route[V.NextWaypoint];const double Distance=FVector2D::Distance(V.Position,End);if(Distance<.000001){++V.NextWaypoint;continue;}
        const double Speed=S.Mobility.Speed(S,TEXT("transport"),V.Position,End)/S.MetersPerWorldUnit();
        if(Speed<=0||V.BatteryKWh<=1.e-9){V.Status=TEXT("Battery depleted");return false;}
        const double FuelDistance=V.BatteryKWh/Def.TravelKWhPerKm*1000/S.MetersPerWorldUnit(),Step=FMath::Min3(Distance,Speed*Seconds,FuelDistance);
        const FVector2D Next=V.Position+(End-V.Position)/Distance*Step;
        if(!S.Mobility.MovementAllowed(S,TEXT("transport"),V.Position,Next)){V.Route.Empty();V.RetryAt=S.Time+2;V.Status=TEXT("Terrain blocked");return false;}
        V.Heading=(End-V.Position).GetSafeNormal();V.Position=Next;V.DistanceMeters+=Step*S.MetersPerWorldUnit();V.BatteryKWh=FMath::Max(0.,V.BatteryKWh-Step*S.MetersPerWorldUnit()/1000*Def.TravelKWhPerKm);Seconds-=Step/Speed;
        if(Distance-Step<.000001)++V.NextWaypoint;else break;
    }
    return V.NextWaypoint>=V.Route.Num()&&!V.Route.IsEmpty();
}
void FSeigeTransportVehicleSystem::FinishDelivery(FSeigeSimulation& S,FSeigeTransportVehicle& V,int32 Id)
{ReleaseHandler(S,V);V.DeliveryId=0;V.Route.Empty();V.NextWaypoint=0;V.Status=TEXT("Available");S.Couriers.RemoveAll([&](const auto& C){return C.Id==Id;});}
void FSeigeTransportVehicleSystem::StepDelivery(FSeigeSimulation& S,FSeigeTransportVehicle& V,FSeigeCourier& C,double Seconds)
{
    if(C.Phase==TEXT("done")&&C.Amount<=1.e-9&&C.ReservedAmount<=1.e-9){FinishDelivery(S,V,C.Id);return;}
    auto* Source=S.FindBuilding(C.SourceId);auto* Target=S.FindBuilding(C.TargetId);auto* Road=S.FindRoad(C.RoadTargetId);
    if(C.Amount<=1.e-9&&(!Source||Source->Health<=0)){FinishDelivery(S,V,C.Id);return;}
    bool DestinationValid=Road?Road->Health>0:(Target&&Target->Health>0);
    if(!DestinationValid)
    {
        ReleaseHandler(S,V);if(C.Amount<=1.e-9){FinishDelivery(S,V,C.Id);return;}
        // Loaded goods return physically, retaining the sole cargo ledger.
        Target=Source&&Source->Health>0?Source:S.Core();if(!Target||Target->Health<=0){V.Status=TEXT("Cargo held: no surviving depot");return;}
        C.TargetId=Target->Id;C.RoadTargetId=0;C.ForConstruction=false;C.Phase=TEXT("carrying");C.PhaseSeconds=0;Road=nullptr;V.Route.Empty();
    }
    const bool Pickup=C.Amount<=1.e-9;
    const FVector2D Endpoint=Pickup?S.BuildingAccessPoint(*Source):(Road?S.RoadAccessPoint(*Road):S.BuildingAccessPoint(*Target));
    if(!Move(S,V,Endpoint,Seconds)){C.Position=V.Position;return;}C.Position=V.Position;
    const auto* Handler=S.Workers.Find(V.HandlerWorkerId);if(Handler&&Handler->TransportVehicleId!=V.Id)Handler=nullptr;
    if(!Handler)
    {
        V.HandlerWorkerId.Empty();
        // The crew stands beside the cargo door, not inside the vehicle mesh.
        const FVector2D Door=V.Position+FVector2D(-V.Heading.Y,V.Heading.X)*(Def.BodySizeMeters.Y*.5+S.Mobility.ClearanceMeters(TEXT("drone")))/S.MetersPerWorldUnit();
        V.HandlerWorkerId=S.Workers.AcquireTransportHandler(S,V.Id,Pickup?C.SourceId:C.TargetId,Pickup?0:C.RoadTargetId,Door,!Pickup&&C.ForConstruction);
    }
    if(!S.Workers.TransportHandlingReady(V.HandlerWorkerId,V.Id)){V.Status=Pickup?TEXT("Waiting for loading worker"):TEXT("Waiting for unloading worker");return;}
    V.Status=Pickup?TEXT("Worker loading cargo"):TEXT("Worker unloading cargo");C.Phase=Pickup?TEXT("loading"):TEXT("unloading");
    const double Amount=Pickup?C.ReservedAmount:C.Amount,Loads=Amount/FMath::Max(1.e-9,S.Workers.HaulUnits(S,C.Resource));
    C.PhaseSeconds+=Seconds;if(C.PhaseSeconds+1.e-9<Def.HandlingSecondsPerDroneLoad*Loads)return;
    if(Pickup)
    {
        const double Picked=FMath::Min(C.ReservedAmount,FMath::Max(0.,Source->Inventory.FindRef(C.Resource)));
        Source->Inventory.FindOrAdd(C.Resource)-=Picked;C.Amount=Picked;C.ReservedAmount=0;C.PhaseSeconds=0;C.Phase=TEXT("carrying");ReleaseHandler(S,V);V.Route.Empty();
        if(Picked<=1.e-9)FinishDelivery(S,V,C.Id);return;
    }
    double Delivered=0;
    if(C.ForConstruction)
    {
        if(Road&&Road->IsConstructing){const auto Bill=S.RoadCost(Road->A,Road->B,Road->TargetTier);Delivered=FMath::Min(C.Amount,FMath::Max(0.,Bill.FindRef(C.Resource)-Road->InstalledMaterials.FindRef(C.Resource)-Road->ConstructionMaterials.FindRef(C.Resource)));Road->ConstructionMaterials.FindOrAdd(C.Resource)+=Delivered;}
        else if(Target&&Target->IsConstructing){Delivered=FMath::Min(C.Amount,FMath::Max(0.,S.ConstructionCost(*Target).FindRef(C.Resource)-Target->InstalledMaterials.FindRef(C.Resource)-Target->ConstructionMaterials.FindRef(C.Resource)));Target->ConstructionMaterials.FindOrAdd(C.Resource)+=Delivered;}
        else {const auto* Return=Source&&Source->Health>0?Source:S.Core();if(!Return){V.Status=TEXT("Cargo held: no surviving depot");return;}C.TargetId=Return->Id;C.RoadTargetId=0;C.ForConstruction=false;C.PhaseSeconds=0;ReleaseHandler(S,V);V.Route.Empty();return;}
    }
    else if(Target){Delivered=FMath::Min(C.Amount,S.StorageRoom(*Target,C.Id)/S.Resources[C.Resource].LitresPerUnit);if(S.Resources[C.Resource].Discrete)Delivered=FMath::FloorToDouble(Delivered+1.e-9);Target->Inventory.FindOrAdd(C.Resource)+=Delivered;}
    C.Amount=FMath::Max(0.,C.Amount-Delivered);S.DeliveredUnits+=Delivered;
    if(C.Amount<=1.e-9){FinishDelivery(S,V,C.Id);return;}
    // No silent destruction when capacity or a changed bill leaves leftovers.
    // Return construction excess; full normal storage waits with its actual cargo.
    if(C.ForConstruction&&Source&&Source->Health>0){C.ForConstruction=false;C.TargetId=Source->Id;C.RoadTargetId=0;C.PhaseSeconds=0;C.Phase=TEXT("carrying");ReleaseHandler(S,V);V.Route.Empty();}
    else V.Status=TEXT("Cargo held: storage full");
}
void FSeigeTransportVehicleSystem::Tick(FSeigeSimulation& S,double Seconds)
{
    if(S.Escaped||S.Failed)return;StepFabrication(S,Seconds);
    for(auto& V:Bodies)
    {
        if(V.Health<=0||V.Evacuated)continue;
        if(V.Embarked){if(!S.DeploymentHatchOpen||V.ReturningToShuttle)continue;V.Embarked=false;V.Status=TEXT("Leaving shuttle");}
        if(V.Status==TEXT("Leaving shuttle"))
        {
            const auto* Core=S.Core();if(!Core)continue;const FVector2D Goal=S.BuildingAccessPoint(*Core),Delta=Goal-V.Position;const double Step=S.Mobility.Speed(S,TEXT("transport"),V.Position,Goal)/S.MetersPerWorldUnit()*Seconds;
            if(Delta.Size()<=Step){V.Position=Goal;V.Status=TEXT("Available");}else{V.Heading=Delta.GetSafeNormal();V.Position+=V.Heading*Step;}continue;
        }
        // Grid-connected service pads refill a real vehicle battery from the
        // building energy ledger. This is not a driver or a worker assignment.
        if(V.BatteryKWh<Def.BatteryKWh)for(auto& B:S.Buildings)if(B.Health>0&&!B.IsConstructing&&B.Enabled&&FVector2D::Distance(V.Position,S.BuildingAccessPoint(B))*S.MetersPerWorldUnit()<=Def.ServiceMeters)
        {const double KWh=FMath::Min3(Def.BatteryKWh-V.BatteryKWh,Def.ChargingKW*Seconds/3600,S.Energy.Info(S,B.Id).StoredKWh);if(KWh>0&&S.Energy.Consume(S,B.Id,KWh)){V.BatteryKWh+=KWh;break;}}
        if(V.ReturningToShuttle)
        {
            const auto* Core=S.Core();if(!Core||Core->Health<=0){V.ReturningToShuttle=false;continue;}
            if(Move(S,V,S.BuildingAccessPoint(*Core),Seconds))
            {
                int32 Occupied=EmbarkedCount();for(const auto& Other:S.Combat.Vehicles)if(Other.Health>0&&Other.Embarked&&!Other.Evacuated)++Occupied;
                if(Occupied<Def.ShuttleVehicleCapacity){V.Embarked=true;V.Status=TEXT("Aboard shuttle");}else{V.ReturningToShuttle=false;V.Status=TEXT("Shuttle vehicle bay occupied");}
            }
            continue;
        }
        if(V.DeliveryId){auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==V.DeliveryId;});if(C)StepDelivery(S,V,*C,Seconds);else{ReleaseHandler(S,V);V.DeliveryId=0;V.Status=TEXT("Available");}}
    }
}
bool FSeigeTransportVehicleSystem::CanQueueVehicle(const FSeigeSimulation& S,int32 FactoryId,FString& Error) const
{
    const auto* B=S.FindBuilding(FactoryId);if(!B||B->Health<=0||B->IsConstructing||!B->Enabled||!Def.Factories.Contains(B->DefId)||S.Escaped||S.Failed){Error=TEXT("Select an operational vehicle factory");return false;}
    if(Jobs.Num()>=Def.MaximumQueue){Error=TEXT("Transport assembly queue is full");return false;}Error.Empty();return true;
}
bool FSeigeTransportVehicleSystem::QueueVehicle(FSeigeSimulation& S,int32 FactoryId,FString& Error)
{if(!CanQueueVehicle(S,FactoryId,Error))return false;FSeigeTransportFabrication J;J.Id=NextJobId++;J.FactoryId=FactoryId;Jobs.Add(J);S.AddEvent(TEXT("Autonomous transport queued: awaiting local materials, energy and factory crew"));return true;}
double FSeigeTransportVehicleSystem::Demand(const FSeigeSimulation& S,int32 FactoryId,const FString& R) const
{for(const auto& J:Jobs)if(J.FactoryId==FactoryId)return J.Committed?0:Def.BuildCost.FindRef(R);return 0;}
double FSeigeTransportVehicleSystem::CommittedStock(const FString& R) const
{double N=0;for(const auto& J:Jobs)if(J.Committed)N+=Def.BuildCost.FindRef(R);return N;}
void FSeigeTransportVehicleSystem::StepFabrication(FSeigeSimulation& S,double Seconds)
{
    TSet<int32> Used;for(int32 I=0;I<Jobs.Num();)
    {
        auto& J=Jobs[I];auto* B=S.FindBuilding(J.FactoryId);if(!B||B->Health<=0){Jobs.RemoveAt(I);continue;}
        if(Used.Contains(J.FactoryId)){++I;continue;}Used.Add(J.FactoryId);
        if(B->IsConstructing||!B->Enabled||S.WorkFraction(*B)<=0||S.Energy.Fraction(B->Id)<=0){++I;continue;}
        if(!J.Committed)
        {
            bool Stock=true;for(const auto& P:Def.BuildCost)if(S.Spendable(*B,P.Key)+1.e-9<P.Value)Stock=false;
            if(!Stock||!S.Energy.Consume(S,B->Id,Def.BuildEnergyKWh)){++I;continue;}
            for(const auto& P:Def.BuildCost)B->Inventory.FindOrAdd(P.Key)-=P.Value;J.Committed=true;
        }
        J.ProgressSeconds+=Seconds*Def.Factories.FindRef(B->DefId)*S.WorkFraction(*B);
        if(J.ProgressSeconds+1.e-9<Def.BuildSeconds){++I;continue;}
        FSeigeTransportVehicle V;V.Id=NextId++;V.Health=Def.Health;V.Position=S.BuildingAccessPoint(*B);V.Status=TEXT("Available");Bodies.Add(V);Jobs.RemoveAt(I);S.AddEvent(TEXT("Autonomous transport assembled; charging at factory"));
    }
}
void FSeigeTransportVehicleSystem::Damage(FSeigeSimulation& S,int32 Id,double Amount)
{if(auto* V=Find(Id);V&&V->Health>0&&!V->Evacuated){V->Health=FMath::Max(0.,V->Health-Amount);if(V->Health<=0){ReleaseHandler(S,*V);if(V->DeliveryId){S.Couriers.RemoveAll([&](const auto& C){return C.Id==V->DeliveryId;});++S.LostCouriers;}V->DeliveryId=0;V->Embarked=false;V->Status=TEXT("Destroyed");}}}
void FSeigeTransportVehicleSystem::CompleteEmptyDelivery(FSeigeSimulation& S,int32 Id)
{if(auto* V=Find(Id)){const auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==V->DeliveryId;});if(C&&C->Amount<=1.e-9&&C->ReservedAmount<=1.e-9)FinishDelivery(S,*V,C->Id);}}
void FSeigeTransportVehicleSystem::OnBuildingDestroyed(FSeigeSimulation& S,int32 BuildingId)
{Jobs.RemoveAll([&](const auto& J){return J.FactoryId==BuildingId;});/* Parked shuttle remains the emergency escape vehicle. */}
bool FSeigeTransportVehicleSystem::BoardShuttle(FSeigeSimulation& S,int32 Id,FString& Error)
{
    auto* V=Find(Id);if(!V||V->Health<=0||V->Evacuated||V->DeliveryId||!S.Core()||S.Core()->Health<=0||S.Escaped||S.Failed){Error=TEXT("An available transport and surviving shuttle are required");return false;}
    if(V->Embarked){V->ReturningToShuttle=false;V->Embarked=false;V->Status=TEXT("Leaving shuttle");Error.Empty();return true;}
    int32 Occupied=EmbarkedCount();for(const auto& Other:S.Combat.Vehicles)if(Other.Health>0&&Other.Embarked&&!Other.Evacuated)++Occupied;
    if(Occupied>=Def.ShuttleVehicleCapacity){Error=TEXT("The shuttle carries only one vehicle");return false;}
    TArray<FVector2D> Route;if(!S.FindMobilityRoute(TEXT("transport"),V->Position,S.BuildingAccessPoint(*S.Core()),Route)){Error=TEXT("Transport cannot reach the shuttle");return false;}
    V->ReturningToShuttle=true;V->Route=MoveTemp(Route);V->NextWaypoint=0;V->RouteRevision=S.TransportRevision;V->Status=TEXT("Returning to shuttle");Error.Empty();return true;
}
int32 FSeigeTransportVehicleSystem::Evacuate(FSeigeSimulation& S,int32 Capacity)
{int32 Taken=0;for(auto& V:Bodies)if(V.Health>0&&V.Embarked&&!V.Evacuated&&Taken<Capacity){ReleaseHandler(S,V);V.Evacuated=true;V.Status=TEXT("Evacuated with shuttle");++Taken;}return Taken;}
