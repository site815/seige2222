#include "SeigeCombat.h"
#include "SeigeSimulation.h"
namespace
{
double Circle(FVector2D A,FVector2D B,FVector2D P,double R){auto D=B-A,F=A-P;double AA=D.SizeSquared();if(F.SizeSquared()<=R*R)return 0;if(AA<1.e-12)return 2;double BB=2*FVector2D::DotProduct(F,D),Disc=BB*BB-4*AA*(F.SizeSquared()-R*R);if(Disc<0)return 2;double T=(-BB-FMath::Sqrt(Disc))/(2*AA);return T>=0&&T<=1?T:2;}
double Box(FVector2D A,FVector2D B,FVector2D C,double R){double Lo=0,Hi=1;for(int I=0;I<2;++I){double D=B[I]-A[I],S=A[I]-C[I];if(FMath::Abs(D)<1.e-12){if(FMath::Abs(S)>R)return 2;continue;}double P=(-R-S)/D,Q=(R-S)/D;if(P>Q)Swap(P,Q);Lo=FMath::Max(Lo,P);Hi=FMath::Min(Hi,Q);if(Lo>Hi)return 2;}return Lo;}
}
FVector2D FSeigeCombatSystem::SectorOffset(int32 I) const{return FVector2D((I%3)-1,(I/3)-1)*SectorHalfSize*2;}
bool FSeigeCombatSystem::IsVisibleInSector(int32 Sector,FVector2D P) const
{for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==Sector&&V.BatteryKWh>0)if(const auto* C=Chassis.Find(V.ChassisId))if(FVector2D::Distance(P,V.Position)*MetresPerUnit<=C->SensorMeters)return true;return false;}
bool FSeigeCombatSystem::OrderPrivateer(const FSeigeSimulation& Home,const FSeigeSimulation& Target,int32 Id,int32 Sector,FVector2D Destination,FString& Error)
{
    auto* F=Fleets.FindByPredicate([&](const auto& X){return X.Id==Id;});
    if(&Home==&Target||Sector<0||Sector>8||Sector==4||!F||Target.Combat.GetFingerprint()!=Fingerprint||Target.Escaped||Target.Failed||!Target.Core()||!FMath::IsFinite(Destination.X)||!FMath::IsFinite(Destination.Y)||FMath::Abs(Destination.X)>SectorHalfSize||FMath::Abs(Destination.Y)>SectorHalfSize){Error=TEXT("Privateering requires an existing neighboring colony and valid destination");return false;}
    // Navigation may avoid an occupied plot without granting sensor knowledge
    // of that colony. A destination inside a footprint otherwise has no route.
    double Clearance=0;for(const auto& V:Vehicles)if(V.FleetId==Id&&V.Health>0&&!V.Evacuated)Clearance=FMath::Max(Clearance,Chassis[V.ChassisId].RadiusMeters/MetresPerUnit);
    if(!Target.ClearWalkingLine(Destination,Destination,Clearance))
    {
        TArray<FVector2D> Candidates;for(const auto& B:Target.Buildings)if(B.Health>0){const double R=Target.Definition(B)->ReservedFootprint+Clearance+.001;const FVector2D P=Destination-B.Position;if(FMath::Abs(P.X)<=R&&FMath::Abs(P.Y)<=R){Candidates.Add(B.Position+FVector2D(R,FMath::Clamp(P.Y,-R,R)));Candidates.Add(B.Position+FVector2D(-R,FMath::Clamp(P.Y,-R,R)));Candidates.Add(B.Position+FVector2D(FMath::Clamp(P.X,-R,R),R));Candidates.Add(B.Position+FVector2D(FMath::Clamp(P.X,-R,R),-R));}}
        double Best=TNumericLimits<double>::Max();FVector2D Safe=Destination;for(const auto& P:Candidates)if(FMath::Abs(P.X)<=SectorHalfSize-Clearance&&FMath::Abs(P.Y)<=SectorHalfSize-Clearance&&Target.ClearWalkingLine(P,P,Clearance)){const double Distance=(P-Destination).SizeSquared();if(Distance<Best){Best=Distance;Safe=P;}}
        if(Best==TNumericLimits<double>::Max()){Error=TEXT("No accessible rally point beside that occupied plot");return false;}Destination=Safe;
    }
    F->Mission=TEXT("privateer");F->DestinationSector=Sector;F->Destination=Destination;F->EscortBuildingId=0;for(auto& V:Vehicles)if(V.FleetId==Id&&!V.Evacuated){V.Embarked=false;V.Route.Empty();V.NextWaypoint=0;}Error.Empty();return true;
}
void FSeigeCombatSystem::AdvanceTransit(FSeigeSimulation& Sim,double Seconds)
{
    for(auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated)
    {
        const auto* F=Fleets.FindByPredicate([&](const auto& X){return X.Id==V.FleetId;});if(!F||F->DestinationSector==V.SectorIndex)continue;const auto& C=Chassis[V.ChassisId];
        const FVector2D Global=V.Position+SectorOffset(V.SectorIndex),Final=F->Destination+SectorOffset(F->DestinationSector);FVector2D Aim=Final;
        // Resolve the current sector at call time: no saved raw simulation pointer
        // and no sensor/intelligence changes are introduced by navigation.
        const FSeigeSimulation* Local=V.SectorIndex==4?&Sim:SectorResolver?SectorResolver(V.SectorIndex):nullptr;
        if(Local&&V.Route.IsEmpty()&&FMath::Max(FMath::Abs(V.Position.X),FMath::Abs(V.Position.Y))<SectorHalfSize-2)
        {const auto LocalFinal=Final-SectorOffset(V.SectorIndex);const FVector2D Exit(FMath::Clamp(LocalFinal.X,-SectorHalfSize+1,SectorHalfSize-1),FMath::Clamp(LocalFinal.Y,-SectorHalfSize+1,SectorHalfSize-1));V.NextWaypoint=0;if(!Local->FindRoute(V.Position,Exit,V.Route,C.RadiusMeters/MetresPerUnit)){V.Status=TEXT("Sector exit route blocked");continue;}}
        if(V.Route.IsValidIndex(V.NextWaypoint))Aim=V.Route[V.NextWaypoint]+SectorOffset(V.SectorIndex);
        const auto Delta=Aim-Global;const double Distance=Delta.Size();if(Distance<.001){++V.NextWaypoint;continue;}
        double Grade=0;const auto Sample=Global+Delta.GetSafeNormal()*FMath::Min(Distance,5/MetresPerUnit);if(TerrainHeight){const double H0=TerrainHeight(Global),H1=TerrainHeight(Sample);if(!FMath::IsFinite(H0)||!FMath::IsFinite(H1)){V.Status=TEXT("Transit terrain unavailable");continue;}Grade=FMath::Abs(H1-H0)/FMath::Max(.001,(Sample-Global).Size());}if(Grade>C.MaxGrade){V.Status=TEXT("Transit slope blocked");continue;}
        const double Speed=C.SpeedKmh/3.6*(Grade>RoughGrade?C.RoughSpeedMultiplier:1);const double Travel=FMath::Min3(Distance*MetresPerUnit,Speed*Seconds,V.BatteryKWh/C.TravelKWhPerKm*1000);if(Travel<=1.e-8){V.Status=TEXT("Transit battery empty");continue;}
        const FVector2D Next=Global+Delta/Distance*Travel/MetresPerUnit;bool Blocked=false;if(Local)for(const auto& B:Local->Buildings)if(B.Health>0&&Box(V.Position,Next-SectorOffset(V.SectorIndex),B.Position,Local->Definition(B)->ReservedFootprint+C.RadiusMeters/MetresPerUnit-.001)<=1){Blocked=true;break;}if(Blocked){V.Route.Empty();V.NextWaypoint=0;V.Status=TEXT("Sector transit obstructed");continue;}const int32 Col=FMath::Clamp(FMath::FloorToInt((Next.X+SectorHalfSize*3)/(SectorHalfSize*2)),0,2),Row=FMath::Clamp(FMath::FloorToInt((Next.Y+SectorHalfSize*3)/(SectorHalfSize*2)),0,2),NewSector=Row*3+Col;
        V.Heading=FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X));V.Position=Next-SectorOffset(NewSector);V.DistanceMeters+=Travel;const double Cost=Travel*C.TravelKWhPerKm/1000;V.BatteryKWh=FMath::Max(0.,V.BatteryKWh-Cost);EnergySpentKWh+=Cost;V.Status=NewSector==4?TEXT("Returning home"):TEXT("Cross-sector transit");
        if(NewSector!=V.SectorIndex){V.SectorIndex=NewSector;V.Route.Empty();V.NextWaypoint=0;V.Destination=V.Position;}
        else if(Travel>=Distance*MetresPerUnit-1.e-8)++V.NextWaypoint;
    }
}
void FSeigeCombatSystem::TickExternalSector(FSeigeSimulation& Home,FSeigeSimulation& Target,int32 Sector,double Seconds)
{
    if(Sector<0||Sector>8||Sector==4||&Home==&Target||!FMath::IsFinite(Seconds)||Seconds<=0||Home.Escaped||Home.Failed||Target.Combat.GetFingerprint()!=Fingerprint)return;
    Target.Combat.EnsureBuildings(Target);
    auto Fleet=[&](const FSeigeVehicle& V){return Fleets.FindByPredicate([&](const auto& F){return F.Id==V.FleetId;});};
    auto Active=[&](const FSeigeVehicle& V){const auto* F=Fleet(V);return V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==Sector&&F&&F->Mission==TEXT("privateer")&&F->DestinationSector==Sector;};
    auto Loot=[&](FSeigeVehicle& V,TMap<FString,double>& Source)
    {
        const auto& C=Chassis[V.ChassisId];double& Budget=V.LootCreditKg;
        TArray<FString> Keys;Source.GetKeys(Keys);
        Keys.RemoveAll([&](const FString& Id){return !Home.Resources.Contains(Id)||Source[Id]<=0;});
        // Snapshot owned quantities before sorting. This includes physical
        // deliveries and fleet cargo, and cannot change during comparisons.
        TMap<FString,double> Owned;for(const auto& Id:Keys)Owned.Add(Id,Home.TotalStock(Id));
        Keys.Sort([&](const FString& A,const FString& B)
        {
            for(const auto& Criterion:LootPriority)
            {
                if(Criterion==TEXT("tier_desc")&&Home.Resources[A].Tier!=Home.Resources[B].Tier)return Home.Resources[A].Tier>Home.Resources[B].Tier;
                if(Criterion==TEXT("owned_quantity_asc")&&Owned[A]!=Owned[B])return Owned[A]<Owned[B];
                if(Criterion==TEXT("resource_id_asc")&&A!=B)return A<B;
            }
            return false;
        });
        for(const auto& Id:Keys){const auto* R=Home.Resources.Find(Id);if(!R||Budget<=0)continue;double Amount=FMath::Max(0.,FMath::Min(Source[Id],FMath::Min3(Budget/R->UnitMassKg,(C.CargoMassKg-Home.InventoryMassKg(V.Inventory))/R->UnitMassKg,(C.StorageLitres-Home.InventoryLitres(V.Inventory))/R->LitresPerUnit)));if(R->Discrete)Amount=FMath::FloorToDouble(Amount);if(Amount>0){Source[Id]-=Amount;V.Inventory.FindOrAdd(Id)+=Amount;Budget-=Amount*R->UnitMassKg;V.Status=TEXT("Capturing cargo");}}
    };
    // One origin owns every visiting unit, its battery, cooldowns and cargo.
    for(auto& V:Vehicles)if(Active(V))
    {
        V.LootCreditKg=FMath::Min(LootKgPerSecond,V.LootCreditKg+LootKgPerSecond*Seconds);const auto& C=Chassis[V.ChassisId];const auto* F=Fleet(V);FVector2D Goal=F->Destination;double Nearest=C.SensorMeters/MetresPerUnit;
        for(const auto& Courier:Target.Couriers){const double D=FVector2D::Distance(V.Position,Courier.Position);if(D<Nearest){Nearest=D;Goal=Courier.Position;}}
        if(V.Route.IsEmpty()||FVector2D::Distance(Goal,V.Destination)*MetresPerUnit>FormationSpacingMeters){V.NextWaypoint=0;V.Destination=Goal;Target.FindRoute(V.Position,Goal,V.Route,C.RadiusMeters/MetresPerUnit);}
        if(V.Route.IsValidIndex(V.NextWaypoint)){const auto Delta=V.Route[V.NextWaypoint]-V.Position;const double Distance=Delta.Size();double Grade=0;const auto World=V.Position+SectorOffset(Sector);if(TerrainHeight&&Distance>.001){const auto Sample=World+Delta.GetSafeNormal()*FMath::Min(Distance,5/MetresPerUnit);Grade=FMath::Abs(TerrainHeight(Sample)-TerrainHeight(World))/FMath::Max(.001,(Sample-World).Size());}const double Travel=FMath::IsFinite(Grade)&&Grade<=C.MaxGrade?FMath::Min3(Distance*MetresPerUnit,C.SpeedKmh/3.6*Seconds*(Grade>RoughGrade?C.RoughSpeedMultiplier:1),V.BatteryKWh/C.TravelKWhPerKm*1000):0;if(Distance>.001&&Travel>0){const FVector2D Next=V.Position+Delta/Distance*Travel/MetresPerUnit;bool Blocked=false;for(const auto& B:Target.Buildings)if(B.Health>0&&Box(V.Position,Next,B.Position,Target.Definition(B)->Footprint+C.RadiusMeters/MetresPerUnit-.001)<=1){Blocked=true;break;}if(Blocked){V.Route.Empty();V.NextWaypoint=0;V.Status=TEXT("Raid route obstructed");continue;}V.Position=Next;V.DistanceMeters+=Travel;V.Heading=FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X));const double Cost=Travel*C.TravelKWhPerKm/1000;V.BatteryKWh=FMath::Max(0.,V.BatteryKWh-Cost);EnergySpentKWh+=Cost;}if(Distance*MetresPerUnit<=Travel+1.e-8)++V.NextWaypoint;}
        for(int32 I=Target.Couriers.Num()-1;I>=0;--I){auto& Courier=Target.Couriers[I];if(FVector2D::Distance(V.Position,Courier.Position)*MetresPerUnit<=LootRangeMeters){TMap<FString,double> Stock;Stock.Add(Courier.Resource,Courier.Amount);Loot(V,Stock);Courier.Amount=Stock[Courier.Resource];if(Courier.Amount<=1.e-8&&Courier.Phase!=TEXT("pickup")&&Courier.Phase!=TEXT("loading")&&!Courier.SelfTransfer){Courier.Phase=TEXT("done");}}}
        for(auto& B:Target.Buildings)if(B.Health>0&&FVector2D::Distance(V.Position,Target.BuildingAccessPoint(B))*MetresPerUnit<=LootRangeMeters){const auto* Protection=Target.Combat.BuildingState.Find(B.Id);if(!Protection||Protection->Shield<=0){TMap<FString,double> Available;for(const auto& P:B.Inventory)Available.Add(P.Key,FMath::Max(0.,Target.Spendable(B,P.Key)-(P.Key==Target.TextRule(TEXT("inactive_worker_resource"))?B.DisassemblyQueued-(B.DisassemblyCommitted?1:0):0)));const auto Before=Available;Loot(V,Available);for(const auto& P:Before){const double Taken=P.Value-Available.FindRef(P.Key);if(P.Key==Target.TextRule(TEXT("inactive_worker_resource"))&&Taken>0&&!Target.Workers.TransferStoredTo(Home.Workers,TEXT("building"),B.Id,TEXT("vehicle"),V.Id,int32(Taken))){V.Inventory.FindOrAdd(P.Key)-=Taken;continue;}B.Inventory.FindOrAdd(P.Key)-=Taken;}}}
        for(double& Cooldown:V.Cooldowns)Cooldown=FMath::Max(0.,Cooldown-Seconds);
    }
    auto Launch=[&](int32 OwnerSector,const FString& Kind,int32 Owner,FVector2D From,const FString& TargetKind,int32 TargetId,FVector2D Aim,const FSeigeWeaponDef& W,int32 WeaponSlot)->bool
    {
        if(Projectiles.Num()>=MaximumProjectiles)return false;
        if(OwnerSector==4){auto* V=FindVehicle(Owner);if(!V||V->BatteryKWh<W.EnergyKWh||(!W.Ammo.IsEmpty()&&V->Inventory.FindRef(W.Ammo)<W.AmmoPerShot))return false;V->BatteryKWh-=W.EnergyKWh;if(!W.Ammo.IsEmpty())V->Inventory.FindOrAdd(W.Ammo)-=W.AmmoPerShot;}
        else if(Kind==TEXT("building")){auto* B=Target.FindBuilding(Owner);if(!B||(!W.Ammo.IsEmpty()&&Target.Spendable(*B,W.Ammo)<W.AmmoPerShot)||!Target.Energy.Consume(Target,Owner,W.EnergyKWh,ESeigeEnergyPurpose::DefensiveShot))return false;if(!W.Ammo.IsEmpty())B->Inventory.FindOrAdd(W.Ammo)-=W.AmmoPerShot;B->LastShotTime=Target.Time;B->LastShotPosition=Aim;}
        else{auto* V=Target.Combat.FindVehicle(Owner);if(!V||V->BatteryKWh<W.EnergyKWh||(!W.Ammo.IsEmpty()&&V->Inventory.FindRef(W.Ammo)<W.AmmoPerShot))return false;V->BatteryKWh-=W.EnergyKWh;if(!W.Ammo.IsEmpty())V->Inventory.FindOrAdd(W.Ammo)-=W.AmmoPerShot;}
        const double Angle=FMath::Atan2(Aim.Y-From.Y,Aim.X-From.X)+FMath::DegreesToRadians((Random.FRand()*2-1)*W.AccuracyDegrees);FSeigeProjectile P;P.Id=NextId++;P.OwnerId=Owner;P.OwnerKind=Kind;P.OwnerSector=OwnerSector;P.SectorIndex=Sector;P.TargetKind=TargetKind;P.TargetEnemyId=TargetId;P.WeaponId=W.Id;P.WeaponSlot=WeaponSlot;P.Position=P.PreviousPosition=From;P.Velocity=FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*(W.SpeedMetersSecond>0?W.SpeedMetersSecond:W.RangeMeters/Seconds)/MetresPerUnit;P.RemainingMeters=W.RangeMeters;Projectiles.Add(P);++ShotsFired;EnergySpentKWh+=W.EnergyKWh;AmmoSpent+=W.AmmoPerShot;return true;
    };
    auto RaiderClear=[&](int32 Owner,FVector2D From,FVector2D Aim){for(const auto& Ally:Vehicles)if(Ally.Id!=Owner&&Ally.Health>0&&!Ally.Embarked&&!Ally.Evacuated&&Ally.SectorIndex==Sector&&Circle(From,Aim,Ally.Position,Chassis[Ally.ChassisId].RadiusMeters/MetresPerUnit)<=1)return false;return true;};
    for(auto& V:Vehicles)if(Active(V)){const auto* F=Fleet(V);if(F->Aggression==TEXT("passive"))continue;for(int I=0;I<V.Weapons.Num();++I)if(V.Cooldowns[I]<=0){const auto& W=Weapons[V.Weapons[I]];double Best=FMath::Min(W.RangeMeters,Chassis[V.ChassisId].SensorMeters)/MetresPerUnit;FString Kind;int32 Id=0;FVector2D Aim=FVector2D::ZeroVector;
        for(const auto& B:Target.Buildings)if(B.Health>0&&FVector2D::Distance(V.Position,B.Position)<Best&&RaiderClear(V.Id,V.Position,B.Position)){Best=FVector2D::Distance(V.Position,B.Position);Kind=TEXT("building");Id=B.Id;Aim=B.Position;}
        for(const auto& D:Target.Combat.Vehicles)if(D.Health>0&&!D.Embarked&&!D.Evacuated&&D.SectorIndex==4&&FVector2D::Distance(V.Position,D.Position)<Best&&RaiderClear(V.Id,V.Position,D.Position)){Best=FVector2D::Distance(V.Position,D.Position);Kind=TEXT("defender");Id=D.Id;Aim=D.Position;}
        if(Id&&Launch(4,TEXT("vehicle"),V.Id,V.Position,Kind,Id,Aim,W,I))V.Cooldowns[I]=W.ReloadSeconds;}}
    auto Intruder=[&](const FString& Kind,int32 Owner,FVector2D P,double Range)->const FSeigeVehicle*{const FSeigeVehicle* Best=nullptr;double D=Range/MetresPerUnit;for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==Sector&&Target.IsVisible(V.Position)){const double X=FVector2D::Distance(V.Position,P);if(X<D&&Target.Combat.ClearFriendlyFire(Target,Kind,Owner,P,V.Position)){D=X;Best=&V;}}return Best;};
    for(auto& B:Target.Buildings)if(B.Health>0&&B.Enabled)if(auto* State=Target.Combat.BuildingState.Find(B.Id)){const auto* D=Target.Definition(B);const double Fraction=B.IsConstructing&&D->DeploymentDefense?Target.Energy.Fraction(B.Id):Target.WorkFraction(B);if(Fraction<=0)continue;for(int I=0;I<State->Weapons.Num();++I)if(State->Cooldowns[I]<=0){const auto& W=Weapons[State->Weapons[I]];if(const auto* V=Intruder(TEXT("building"),B.Id,B.Position,W.RangeMeters))if(Launch(Sector,TEXT("building"),B.Id,B.Position,TEXT("raider"),V->Id,V->Position,W,I))State->Cooldowns[I]=W.ReloadSeconds;}}
    for(auto& D:Target.Combat.Vehicles)if(D.Health>0&&!D.Embarked&&!D.Evacuated&&D.SectorIndex==4)for(int I=0;I<D.Weapons.Num();++I)if(D.Cooldowns[I]<=0){const auto& W=Weapons[D.Weapons[I]];if(const auto* V=Intruder(TEXT("vehicle"),D.Id,D.Position,W.RangeMeters))if(Launch(Sector,TEXT("vehicle"),D.Id,D.Position,TEXT("raider"),V->Id,V->Position,W,I))D.Cooldowns[I]=W.ReloadSeconds;}
    for(int I=Projectiles.Num()-1;I>=0;--I)
    {
        auto& P=Projectiles[I];if(P.SectorIndex!=Sector)continue;const auto& W=Weapons[P.WeaponId];P.PreviousPosition=P.Position;
        if(W.HomingDegreesSecond>0){FVector2D Aim=FVector2D::ZeroVector;bool Found=false;if(P.TargetKind==TEXT("raider")){if(const auto* V=FindVehicle(P.TargetEnemyId)){Aim=V->Position;Found=V->Health>0&&!V->Embarked&&!V->Evacuated&&V->SectorIndex==Sector;}}else if(P.TargetKind==TEXT("building")){if(const auto* B=Target.FindBuilding(P.TargetEnemyId)){Aim=B->Position;Found=B->Health>0;}}else if(const auto* V=Target.Combat.FindVehicle(P.TargetEnemyId)){Aim=V->Position;Found=V->Health>0&&!V->Embarked&&!V->Evacuated;}if(Found){double A=FMath::Atan2(P.Velocity.Y,P.Velocity.X),B=FMath::Atan2(Aim.Y-P.Position.Y,Aim.X-P.Position.X),Turn=FMath::Clamp(FMath::UnwindRadians(B-A),-FMath::DegreesToRadians(W.HomingDegreesSecond)*Seconds,FMath::DegreesToRadians(W.HomingDegreesSecond)*Seconds);P.Velocity=FVector2D(FMath::Cos(A+Turn),FMath::Sin(A+Turn))*W.SpeedMetersSecond/MetresPerUnit;}}
        const double Travel=FMath::Min(P.RemainingMeters,P.Velocity.Size()*Seconds*MetresPerUnit);const auto End=P.Position+P.Velocity.GetSafeNormal()*Travel/MetresPerUnit;double Best=2;FString Kind;int32 Hit=0;
        auto Accept=[&](double T,const FString& K,int32 Id,int32 Owner){const FString Base=K==TEXT("raider")||K==TEXT("defender")?TEXT("vehicle"):K;if(P.OwnerSector==Owner&&P.OwnerKind==Base&&P.OwnerId==Id)return;if(T<Best){Best=T;Kind=K;Hit=Id;}};
        for(const auto& B:Target.Buildings)if(B.Health>0)Accept(Box(P.Position,End,B.Position,Target.Definition(B)->Footprint),TEXT("building"),B.Id,Sector);
        for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==Sector)Accept(Circle(P.Position,End,V.Position,Chassis[V.ChassisId].RadiusMeters/MetresPerUnit),TEXT("raider"),V.Id,4);
        for(const auto& V:Target.Combat.Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4)Accept(Circle(P.Position,End,V.Position,Chassis[V.ChassisId].RadiusMeters/MetresPerUnit),TEXT("defender"),V.Id,Sector);
        for(const auto& E:Target.Enemies)if(E.Health>0)Accept(Circle(P.Position,End,E.Position,EnemyRadiusMeters/MetresPerUnit),TEXT("enemy"),E.Id,Sector);
        for(const auto& C:Target.Couriers)Accept(Circle(P.Position,End,C.Position,EnemyRadiusMeters/MetresPerUnit),TEXT("courier"),C.Id,Sector);
        P.Position=Best<=1?FMath::Lerp(P.Position,End,Best):End;P.RemainingMeters=FMath::Max(0.,P.RemainingMeters-Travel);P.AgeSeconds+=Seconds;
        const bool Impacted=Best<=1,Expired=P.RemainingMeters<=1.e-8;
        if(W.SpeedMetersSecond==0)ShotEvents.Add({Home.Time,P.OwnerId,P.OwnerKind,W.Family,P.PreviousPosition+SectorOffset(Sector),P.Position+SectorOffset(Sector),Impacted,W.Id,P.WeaponSlot,P.OwnerSector});
        if(Impacted||(Expired&&W.SplashMeters>0))
        {
            ++Hits;auto Damage=[&](const FString& K,int32 Id,double Amount){if(K==TEXT("building"))Target.Combat.DamageBuilding(Target,Id,Amount,W.Family,W.ShieldMultiplier,W.ArmorMultiplier);else if(K==TEXT("raider"))DamageVehicle(Home,Id,Amount,W.Family,W.ShieldMultiplier,W.ArmorMultiplier);else if(K==TEXT("defender"))Target.Combat.DamageVehicle(Target,Id,Amount,W.Family,W.ShieldMultiplier,W.ArmorMultiplier);else if(K==TEXT("courier")){Target.Workers.KillCourier(Target,Id);}else if(auto* E=Target.Enemies.FindByPredicate([&](const auto& X){return X.Id==Id;}))E->Health=FMath::Max(0.,E->Health-Amount);};
            if(W.SplashMeters<=0)Damage(Kind,Hit,W.Damage);else{auto Splash=[&](const FString& K,int32 Id,FVector2D Q){double Scale=1-FVector2D::Distance(P.Position,Q)*MetresPerUnit/W.SplashMeters;if(K==Kind&&Id==Hit)Scale=1;if(Scale>0)Damage(K,Id,W.Damage*Scale);};for(const auto& B:Target.Buildings)if(B.Health>0)Splash(TEXT("building"),B.Id,B.Position);for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==Sector)Splash(TEXT("raider"),V.Id,V.Position);for(const auto& V:Target.Combat.Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4)Splash(TEXT("defender"),V.Id,V.Position);for(const auto& E:Target.Enemies)if(E.Health>0)Splash(TEXT("enemy"),E.Id,E.Position);TArray<TPair<int32,FVector2D>> CourierTargets;for(const auto& C:Target.Couriers)CourierTargets.Emplace(C.Id,C.Position);for(const auto& C:CourierTargets)Splash(TEXT("courier"),C.Key,C.Value);}
        }
        if(Impacted||Expired||W.SpeedMetersSecond==0)Projectiles.RemoveAt(I);
    }
    Target.Enemies.RemoveAll([](const auto& E){return E.Health<=0;});for(auto It=Target.Combat.BugCooldowns.CreateIterator();It;++It)if(!Target.Enemies.ContainsByPredicate([&](const auto& E){return E.Id==It.Key();}))It.RemoveCurrent();
}
