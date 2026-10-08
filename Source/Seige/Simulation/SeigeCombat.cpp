#include "SeigeCombat.h"
#include "SeigeSimulation.h"
namespace
{
double HitCircle(FVector2D A,FVector2D B,FVector2D C,double R){const auto D=B-A,F=A-C;const double AA=D.SizeSquared();if(F.SizeSquared()<=R*R)return 0;if(AA<1.e-16)return 2;const double BB=2*FVector2D::DotProduct(F,D),Disc=BB*BB-4*AA*(F.SizeSquared()-R*R);if(Disc<0)return 2;const double T=(-BB-FMath::Sqrt(Disc))/(2*AA);return T>=0&&T<=1?T:2;}
double HitBox(FVector2D A,FVector2D B,FVector2D C,double R){double Lo=0,Hi=1;for(int I=0;I<2;++I){const double S=A[I]-C[I],D=B[I]-A[I];if(FMath::Abs(D)<1.e-12){if(FMath::Abs(S)>R)return 2;continue;}double T1=(-R-S)/D,T2=(R-S)/D;if(T1>T2)Swap(T1,T2);Lo=FMath::Max(Lo,T1);Hi=FMath::Min(Hi,T2);if(Lo>Hi)return 2;}return Lo;}
void Layers(double Damage,double SM,double AM,double& Shield,double& Armor,double& Health){if(Shield>0){const double Used=FMath::Min(Shield,Damage*SM);Shield-=Used;Damage-=Used/SM;}if(Damage>0&&Armor>0){const double Used=FMath::Min(Armor,Damage*AM);Armor-=Used;Damage-=Used/AM;}Health=FMath::Max(0.,Health-FMath::Max(0.,Damage));}
}
bool FSeigeCombatSystem::ClearFriendlyFire(const FSeigeSimulation& Sim,const FString& Kind,int32 Owner,FVector2D From,FVector2D Aim,FString* Blocker) const
{
    // Check the intended line only. Spread, splash and later unit movement
    // still use the physical projectile collision path and can hit allies.
    for(const auto& B:Sim.Buildings)if(B.Health>0&&!(Kind==TEXT("building")&&B.Id==Owner)&&HitBox(From,Aim,B.Position,Sim.Definition(B)->Footprint)<=1){if(Blocker)*Blocker=Sim.Definition(B)->Name;return false;}
    for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4&&!(Kind==TEXT("vehicle")&&V.Id==Owner)&&HitCircle(From,Aim,V.Position,Chassis[V.ChassisId].RadiusMeters/MetresPerUnit)<=1){if(Blocker)*Blocker=Chassis[V.ChassisId].Name;return false;}
    return true;
}
double FSeigeCombatSystem::BuildingWeaponWork(const FSeigeSimulation& Sim,int32 Id) const
{
    const auto* B=Sim.FindBuilding(Id);if(!B||B->Health<=0)return 0;
    return B->IsConstructing&&Sim.Definition(*B)->DeploymentDefense&&B->Enabled?Sim.Energy.Fraction(Id):Sim.WorkFraction(*B);
}
FSeigeCombatSystem::FFireControl FSeigeCombatSystem::QueryFire(const FSeigeSimulation& Sim,const FString& Kind,int32 Owner,FVector2D Position,const FSeigeWeaponDef& W,double Cooldown,bool RequireStoredEnergy) const
{
    FFireControl Result;
    auto Hold=[&](const FString& Reason){Result.Status=Reason;return Result;};
    if(Sim.Escaped||Sim.Failed)return Hold(TEXT("Command ended"));
    const auto* B=Kind==TEXT("building")?Sim.FindBuilding(Owner):nullptr;
    const auto* V=Kind==TEXT("vehicle")?FindVehicle(Owner):nullptr;
    if(B)
    {
        if(B->Health<=0)return Hold(TEXT("Destroyed"));
        if(!B->Enabled)return Hold(TEXT("Disabled"));
        if(B->IsConstructing&&!Sim.Definition(*B)->DeploymentDefense)return Hold(TEXT("Under construction"));
        if(BuildingWeaponWork(Sim,Owner)<=0)return Hold(Sim.Energy.Fraction(Owner)<=0?TEXT("Waiting for road-grid power"):TEXT("Waiting for workers or upkeep"));
    }
    else if(V)
    {
        if(V->Health<=0||V->Embarked||V->Evacuated||V->SectorIndex!=4)return Hold(TEXT("Not deployed locally"));
        const auto* Fleet=Fleets.FindByPredicate([&](const auto& F){return F.Id==V->FleetId;});
        if(Fleet&&Fleet->Aggression==TEXT("passive"))return Hold(TEXT("Passive: holding fire"));
    }
    else return Hold(TEXT("Weapon owner unavailable"));
    double Closest=W.RangeMeters/MetresPerUnit;FString Blocker;
    for(const auto& Enemy:Sim.Enemies)if(Enemy.Health>0&&Sim.IsVisible(Enemy.Position))
    {
        const double Distance=FVector2D::Distance(Position,Enemy.Position);
        if(Distance<Closest)
        {
            FString Obstruction;
            if(ClearFriendlyFire(Sim,Kind,Owner,Position,Enemy.Position,&Obstruction)){Closest=Distance;Result.TargetId=Enemy.Id;}
            else if(Blocker.IsEmpty())Blocker=Obstruction;
        }
    }
    if(!Result.TargetId)return Hold(Blocker.IsEmpty()?TEXT("No detected enemy in weapon range"):TEXT("Holding fire: friendly ")+Blocker+TEXT(" blocks the shot"));
    if(Cooldown>1.e-8)return Hold(TEXT("Reloading"));
    if(Projectiles.Num()>=MaximumProjectiles)return Hold(TEXT("Waiting for projectile capacity"));
    if(!W.Ammo.IsEmpty()&&(B?Sim.Spendable(*B,W.Ammo):V->Inventory.FindRef(W.Ammo))+1.e-8<W.AmmoPerShot)return Hold(TEXT("Waiting for local ammunition"));
    if(RequireStoredEnergy&&(B?!Sim.Energy.CanConsume(Sim,Owner,W.EnergyKWh,ESeigeEnergyPurpose::DefensiveShot):V->BatteryKWh+1.e-8<W.EnergyKWh))return Hold(TEXT("Waiting for stored shot energy"));
    Result.Ready=true;Result.Status=TEXT("Ready to fire");return Result;
}
FString FSeigeCombatSystem::BuildingFireStatus(const FSeigeSimulation& Sim,int32 Id) const
{
    const auto* B=Sim.FindBuilding(Id);if(!B)return TEXT("Building unavailable");
    const auto* State=BuildingState.Find(Id);if(!State||State->Weapons.IsEmpty())return TEXT("Unarmed");
    TArray<FString> Reasons;
    for(int32 I=0;I<State->Weapons.Num();++I)
    {
        const auto* W=Weapons.Find(State->Weapons[I]);if(!W)continue;
        const auto Result=QueryFire(Sim,TEXT("building"),Id,B->Position,*W,State->Cooldowns.IsValidIndex(I)?State->Cooldowns[I]:0);
        if(Result.Ready)return Result.Status;Reasons.AddUnique(Result.Status);
    }
    return FString::Join(Reasons,TEXT("; "));
}
bool FSeigeCombatSystem::DefensivePosition(const FSeigeSimulation& Sim,const FSeigeVehicle& V,FVector2D Anchor,FVector2D& Position) const
{
    const auto& C=Chassis[V.ChassisId];double Range=0,SafeDistance=C.RadiusMeters*3;
    for(const auto& Id:V.Weapons){Range=FMath::Max(Range,Weapons[Id].RangeMeters);SafeDistance=FMath::Max(SafeDistance,Weapons[Id].SplashMeters+C.RadiusMeters);}
    if(Range<=0)return false;TArray<const FSeigeEnemy*> Threats;
    for(const auto& E:Sim.Enemies)if(E.Health>0&&Sim.IsVisible(E.Position)&&FVector2D::Distance(V.Position,E.Position)*MetresPerUnit<=C.SensorMeters&&FVector2D::Distance(Anchor,E.Position)*MetresPerUnit<=C.SensorMeters)Threats.Add(&E);
    if(Threats.IsEmpty())return false;
    auto CanFire=[&](FVector2D P){for(const auto* E:Threats)if(FVector2D::Distance(P,E->Position)*MetresPerUnit<Range&&ClearFriendlyFire(Sim,TEXT("vehicle"),V.Id,P,E->Position))return true;return false;};
    if(CanFire(V.Position)){Position=V.Position;return true;}
    // Keep a valid route while flanking instead of recomputing a visibility
    // graph each fixed step. No enemy outside the unit's sensor is pursued.
    if(V.Status==TEXT("Repositioning for defense")&&V.Route.IsValidIndex(V.NextWaypoint)&&FVector2D::Distance(Anchor,V.Destination)*MetresPerUnit<=C.SensorMeters&&CanFire(V.Destination)){Position=V.Destination;return true;}
    double Best=TNumericLimits<double>::Max();const double Clearance=C.RadiusMeters/MetresPerUnit;
    auto Consider=[&](FVector2D Candidate,const FSeigeEnemy& Enemy)
    {
        const double Distance=FVector2D::Distance(Candidate,Enemy.Position)*MetresPerUnit;
        if(Distance>=Range||Distance<SafeDistance||FVector2D::Distance(Candidate,Anchor)*MetresPerUnit>C.SensorMeters||FMath::Abs(Candidate.X)>Sim.WorldHalfSize-Clearance||FMath::Abs(Candidate.Y)>Sim.WorldHalfSize-Clearance||!Sim.ClearWalkingLine(Candidate,Candidate,Clearance)||!ClearFriendlyFire(Sim,TEXT("vehicle"),V.Id,Candidate,Enemy.Position))return;
        TArray<FVector2D> Route;if(!Sim.FindRoute(V.Position,Candidate,Route,Clearance))return;double Length=0;FVector2D Previous=V.Position;
        for(const auto& P:Route)
        {
            // The building route graph does not contain mobile circles. A
            // defensive detour must not solve LOS by walking through an ally.
            for(const auto& Other:Vehicles)if(Other.Id!=V.Id&&Other.Health>0&&!Other.Embarked&&!Other.Evacuated&&Other.SectorIndex==4&&HitCircle(Previous,P,Other.Position,Chassis[Other.ChassisId].RadiusMeters/MetresPerUnit+Clearance)<=1)return;
            Length+=(P-Previous).Size();Previous=P;
        }
        if(Length<Best){Best=Length;Position=Candidate;}
    };
    for(const auto* E:Threats)
    {
        for(const auto& B:Sim.Buildings)if(B.Health>0&&HitBox(V.Position,E->Position,B.Position,Sim.Definition(B)->Footprint)<=1)
        {
            const double R=Sim.Definition(B)->ReservedFootprint+Clearance+.001;
            for(int X:{-1,1})for(int Y:{-1,1})Consider(B.Position+FVector2D(X*R,Y*R),*E);
        }
        for(const auto& Other:Vehicles)if(Other.Id!=V.Id&&Other.Health>0&&!Other.Embarked&&!Other.Evacuated&&Other.SectorIndex==4&&HitCircle(V.Position,E->Position,Other.Position,Chassis[Other.ChassisId].RadiusMeters/MetresPerUnit)<=1)
        {
            const double R=Chassis[Other.ChassisId].RadiusMeters/MetresPerUnit+Clearance+.001;
            for(int X:{-1,1})for(int Y:{-1,1})Consider(Other.Position+FVector2D(X*R,Y*R),*E);
        }
    }
    return Best<TNumericLimits<double>::Max();
}
void FSeigeCombatSystem::DamageBuilding(FSeigeSimulation& Sim,int32 Id,double Damage,const FString& Family,double ShieldMultiplier,double ArmorMultiplier)
{
    auto* B=Sim.FindBuilding(Id);if(!B||B->Health<=0||!FMath::IsFinite(Damage)||Damage<=0)return;EnsureBuildings(Sim);const auto Profile=DamageProfiles.FindRef(Family);const double SM=ShieldMultiplier>0?ShieldMultiplier:FMath::Max(.05,Profile.X),AM=ArmorMultiplier>0?ArmorMultiplier:FMath::Max(.05,Profile.Y);
    if(auto* State=BuildingState.Find(Id)){Layers(Damage,SM,AM,State->Shield,State->Armor,B->Health);State->LastDamageTime=Sim.Time;}else B->Health=FMath::Max(0.,B->Health-Damage);
    if(B->Health<=0){UE_LOG(LogTemp,Log,TEXT("Seige combat destruction: seed=%d time=%.2f building=%d definition=%s final_damage_family=%s"),Sim.GenerationSeed,Sim.Time,Id,*B->DefId,*Family);FabricationPlans.Remove(Id);RefitPlans.Remove(Id);Sim.OnBuildingDestroyed(Id);}
}
void FSeigeCombatSystem::DamageVehicle(FSeigeSimulation& Sim,int32 Id,double Damage,const FString& Family,double ShieldMultiplier,double ArmorMultiplier)
{
    auto* V=FindVehicle(Id);if(!V||V->Health<=0||!FMath::IsFinite(Damage)||Damage<=0)return;const auto Profile=DamageProfiles.FindRef(Family);const double SM=ShieldMultiplier>0?ShieldMultiplier:FMath::Max(.05,Profile.X),AM=ArmorMultiplier>0?ArmorMultiplier:FMath::Max(.05,Profile.Y);Layers(Damage,SM,AM,V->Shield,V->Armor,V->Health);V->LastDamageTime=Sim.Time;if(V->Health<=0){for(auto& Body:Sim.Workers.Bodies)if(Body.State==TEXT("vehicle")&&Body.ContainerId==V->Id){Body.State=TEXT("destroyed");Body.Activity=TEXT("terminal");}V->Inventory.Empty();V->Route.Empty();V->NextWaypoint=0;V->Status=TEXT("Destroyed");}
}
bool FSeigeCombatSystem::Fire(FSeigeSimulation& Sim,const FString& Kind,int32 Owner,FVector2D Position,int32 TargetId,const FSeigeWeaponDef& W,int32 WeaponSlot)
{
    const auto* Target=Sim.Enemies.FindByPredicate([&](const auto& E){return E.Id==TargetId&&E.Health>0;});if(!Target||Projectiles.Num()>=MaximumProjectiles)return false;
    if(Kind==TEXT("building")){auto* B=Sim.FindBuilding(Owner);if(!B||(!W.Ammo.IsEmpty()&&Sim.Spendable(*B,W.Ammo)+1.e-8<W.AmmoPerShot)||!Sim.Energy.CanConsume(Sim,Owner,W.EnergyKWh,ESeigeEnergyPurpose::DefensiveShot))return false;if(!Sim.Energy.Consume(Sim,Owner,W.EnergyKWh,ESeigeEnergyPurpose::DefensiveShot))return false;if(!W.Ammo.IsEmpty())B->Inventory.FindOrAdd(W.Ammo)=FMath::Max(0.,B->Inventory.FindRef(W.Ammo)-W.AmmoPerShot);B->LastShotTime=Sim.Time;B->LastShotPosition=Target->Position;}
    else{auto* V=FindVehicle(Owner);if(!V||V->BatteryKWh+1.e-8<W.EnergyKWh||(!W.Ammo.IsEmpty()&&V->Inventory.FindRef(W.Ammo)+1.e-8<W.AmmoPerShot))return false;V->BatteryKWh=FMath::Max(0.,V->BatteryKWh-W.EnergyKWh);if(!W.Ammo.IsEmpty())V->Inventory.FindOrAdd(W.Ammo)=FMath::Max(0.,V->Inventory.FindRef(W.Ammo)-W.AmmoPerShot);}
    ++ShotsFired;EnergySpentKWh+=W.EnergyKWh;AmmoSpent+=W.AmmoPerShot;
    const double Angle=FMath::Atan2(Target->Position.Y-Position.Y,Target->Position.X-Position.X)+FMath::DegreesToRadians((Random.FRand()*2-1)*W.AccuracyDegrees);
    FSeigeProjectile P;P.Id=NextId++;P.OwnerId=Owner;P.OwnerKind=Kind;P.TargetEnemyId=TargetId;P.WeaponId=W.Id;P.WeaponSlot=WeaponSlot;P.Position=P.PreviousPosition=Position;P.Velocity=FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*W.SpeedMetersSecond/MetresPerUnit;P.RemainingMeters=W.RangeMeters;
    if(W.SpeedMetersSecond==0){P.Velocity=FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*W.RangeMeters/MetresPerUnit;bool Remove=false;const double Before=Hits;AdvanceProjectile(Sim,P,1,Remove);ShotEvents.Add({Sim.Time,Owner,Kind,W.Family,Position,P.Position,Hits>Before,W.Id,WeaponSlot,4});if(auto* B=Kind==TEXT("building")?Sim.FindBuilding(Owner):nullptr)B->LastShotPosition=FVector2D(FMath::Clamp(P.Position.X,-Sim.WorldHalfSize,Sim.WorldHalfSize),FMath::Clamp(P.Position.Y,-Sim.WorldHalfSize,Sim.WorldHalfSize));}else Projectiles.Add(P);return true;
}
void FSeigeCombatSystem::Impact(FSeigeSimulation& Sim,const FSeigeProjectile& P,const FString& Kind,int32 Id,FVector2D Position)
{
    const auto& W=Weapons[P.WeaponId];++Hits;
    auto Damage=[&](const FString& K,int32 I,double Amount){if(K==TEXT("building"))DamageBuilding(Sim,I,Amount,W.Family,W.ShieldMultiplier,W.ArmorMultiplier);else if(K==TEXT("vehicle"))DamageVehicle(Sim,I,Amount,W.Family,W.ShieldMultiplier,W.ArmorMultiplier);else if(auto* E=Sim.Enemies.FindByPredicate([&](const auto& V){return V.Id==I;}))E->Health=FMath::Max(0.,E->Health-Amount);};
    if(W.SplashMeters<=0){Damage(Kind,Id,W.Damage);return;}
    auto Splash=[&](const FString& K,int32 I,FVector2D Q){double Scale=1-FVector2D::Distance(Position,Q)*MetresPerUnit/W.SplashMeters;if(K==Kind&&I==Id)Scale=1;if(Scale>0)Damage(K,I,W.Damage*Scale);};
    for(auto& B:Sim.Buildings)if(B.Health>0)Splash(TEXT("building"),B.Id,B.Position);for(auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4)Splash(TEXT("vehicle"),V.Id,V.Position);for(auto& E:Sim.Enemies)if(E.Health>0)Splash(TEXT("enemy"),E.Id,E.Position);
}
void FSeigeCombatSystem::AdvanceProjectile(FSeigeSimulation& Sim,FSeigeProjectile& P,double Seconds,bool& Remove)
{
    const auto& W=Weapons[P.WeaponId];P.PreviousPosition=P.Position;
    if(W.HomingDegreesSecond>0&&P.TargetEnemyId>0)if(const auto* E=Sim.Enemies.FindByPredicate([&](const auto& V){return V.Id==P.TargetEnemyId&&V.Health>0;})){const double A=FMath::Atan2(P.Velocity.Y,P.Velocity.X),B=FMath::Atan2(E->Position.Y-P.Position.Y,E->Position.X-P.Position.X),Delta=FMath::UnwindRadians(B-A),Turn=FMath::Clamp(Delta,-FMath::DegreesToRadians(W.HomingDegreesSecond)*Seconds,FMath::DegreesToRadians(W.HomingDegreesSecond)*Seconds);P.Velocity=FVector2D(FMath::Cos(A+Turn),FMath::Sin(A+Turn))*W.SpeedMetersSecond/MetresPerUnit;}
    const double Travel=FMath::Min(P.RemainingMeters,P.Velocity.Size()*Seconds*MetresPerUnit);const FVector2D End=P.Position+P.Velocity.GetSafeNormal()*Travel/MetresPerUnit;double Best=2;int32 HitId=0;FString Kind;
    auto Accept=[&](double T,const FString& K,int32 I){if(K==P.OwnerKind&&I==P.OwnerId)return;if(T<Best){Best=T;Kind=K;HitId=I;}};
    for(const auto& B:Sim.Buildings)if(B.Health>0)Accept(HitBox(P.Position,End,B.Position,Sim.Definition(B)->Footprint),TEXT("building"),B.Id);
    for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4)Accept(HitCircle(P.Position,End,V.Position,Chassis[V.ChassisId].RadiusMeters/MetresPerUnit),TEXT("vehicle"),V.Id);
    for(const auto& E:Sim.Enemies)if(E.Health>0)Accept(HitCircle(P.Position,End,E.Position,EnemyRadiusMeters/MetresPerUnit),TEXT("enemy"),E.Id);
    P.Position=Best<=1?FMath::Lerp(P.Position,End,Best):End;P.RemainingMeters=FMath::Max(0.,P.RemainingMeters-Travel);P.AgeSeconds+=Seconds;
    if(Best<=1){Impact(Sim,P,Kind,HitId,P.Position);Remove=true;}else if(P.RemainingMeters<=1.e-8){if(W.SplashMeters>0)Impact(Sim,P,TEXT(""),0,P.Position);Remove=true;}
}
void FSeigeCombatSystem::CollectPendingShotEnergy(const FSeigeSimulation& Sim,double Seconds,TMap<int32,double>& ByGrid) const
{
    ByGrid.Empty();if(!Sim.Enemies.IsEmpty())for(const auto& B:Sim.Buildings)
    {
        const auto* State=BuildingState.Find(B.Id);const double Work=BuildingWeaponWork(Sim,B.Id);
        if(!State||Work<=0)continue;
        const int32 Grid=Sim.Energy.Info(Sim,B.Id).ComponentId;if(Grid==INDEX_NONE)continue;
        for(int32 I=0;I<State->Weapons.Num();++I)
        {
            const auto* W=Weapons.Find(State->Weapons[I]);
            if(!W||W->EnergyKWh<=0||!State->Cooldowns.IsValidIndex(I)||State->Cooldowns[I]>Seconds*Work+1.e-8)continue;
            if(QueryFire(Sim,TEXT("building"),B.Id,B.Position,*W,0,false).Ready)ByGrid.FindOrAdd(Grid)+=W->EnergyKWh;
        }
    }
}
void FSeigeCombatSystem::ServiceVehicles(FSeigeSimulation& Sim,double Seconds)
{
    for(auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4){const auto& C=Chassis[V.ChassisId];for(auto& B:Sim.Buildings)if(B.Health>0&&!B.IsConstructing&&B.Enabled&&(Factories.Contains(B.DefId)||CoreFleetLimits.Contains(B.DefId))&&FVector2D::Distance(V.Position,Sim.BuildingAccessPoint(B))*MetresPerUnit<=ServiceMeters)
    {if(V.ReturningCargo){TArray<FString> Keys;V.Inventory.GetKeys(Keys);Keys.Sort();for(const auto& Id:Keys){const auto& R=Sim.Resources[Id];double Keep=0;for(const auto& Weapon:V.Weapons)if(Weapons[Weapon].Ammo==Id)Keep+=AmmoBufferShots*Weapons[Weapon].AmmoPerShot;double Amount=FMath::Max(0.,FMath::Min(V.Inventory[Id]-Keep,Sim.StorageRoom(B)/R.LitresPerUnit));if(R.Discrete)Amount=FMath::FloorToDouble(Amount);if(Amount>0){if(Id==Sim.TextRule(TEXT("inactive_worker_resource"))&&!Sim.Workers.ReceiveStored(Sim,TEXT("vehicle"),V.Id,B.Id,int32(Amount)))continue;V.Inventory[Id]-=Amount;B.Inventory.FindOrAdd(Id)+=Amount;}}bool Pending=false;for(const auto& Stock:V.Inventory){double Keep=0;for(const auto& Weapon:V.Weapons)if(Weapons[Weapon].Ammo==Stock.Key)Keep+=AmmoBufferShots*Weapons[Weapon].AmmoPerShot;if(Stock.Value>Keep+1.e-8)Pending=true;}V.ReturningCargo=Pending;}
     const double Charge=FMath::Min(C.BatteryKWh-V.BatteryKWh,ChargeKW*Seconds/3600);if(Charge>0&&Sim.Energy.Consume(Sim,B.Id,Charge))V.BatteryKWh+=Charge;
     for(const auto& Id:V.Weapons){const auto& W=Weapons[Id];if(W.Ammo.IsEmpty())continue;const auto& R=Sim.Resources[W.Ammo];const double Free=FMath::Min((C.StorageLitres-Sim.InventoryLitres(V.Inventory))/R.LitresPerUnit,(C.CargoMassKg-Sim.InventoryMassKg(V.Inventory))/R.UnitMassKg);const double Take=FMath::Max(0.,FMath::Min3(AmmoBufferShots*W.AmmoPerShot-V.Inventory.FindRef(W.Ammo),Sim.Spendable(B,W.Ammo),Free));if(Take>0){B.Inventory.FindOrAdd(W.Ammo)-=Take;V.Inventory.FindOrAdd(W.Ammo)+=Take;}}break;}
     if(Sim.Time-V.LastDamageTime>=ShieldDelay&&V.Shield<C.Shield){const double Restore=FMath::Min3(C.Shield-V.Shield,ShieldRegen*Seconds,V.BatteryKWh/ShieldRegenKWh);V.Shield+=Restore;V.BatteryKWh-=Restore*ShieldRegenKWh;EnergySpentKWh+=Restore*ShieldRegenKWh;}}
}
void FSeigeCombatSystem::MoveVehicles(FSeigeSimulation& Sim,double Seconds)
{
    for(auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4)
    {
        const auto& C=Chassis[V.ChassisId];const auto* F=Fleets.FindByPredicate([&](const auto& X){return X.Id==V.FleetId;});if(!F){V.Status=TEXT("Unassigned");continue;}if(F->DestinationSector!=4)continue;FVector2D Target=F->Destination;bool Repositioning=false;
        if(F->Mission==TEXT("escort"))if(const auto* B=Sim.FindBuilding(F->EscortBuildingId)){Target=Sim.BuildingAccessPoint(*B);const auto* Courier=Sim.Couriers.FindByPredicate([&](const auto& X){return X.SourceId==B->Id||X.TargetId==B->Id;});if(Courier)Target=Courier->Position;}
        int32 Index=0;for(const auto& Other:Vehicles)if(Other.Id<V.Id&&Other.FleetId==V.FleetId&&Other.Health>0)++Index;
        const double FormationStep=FMath::Max(FormationSpacingMeters,C.RadiusMeters*2);const double Lateral=Index==0?0:((Index+1)/2)*(Index%2?1:-1)*FormationStep;Target+=FVector2D(0,Lateral/MetresPerUnit);const double Limit=Sim.WorldHalfSize-C.RadiusMeters/MetresPerUnit;Target.X=FMath::Clamp(Target.X,-Limit,Limit);Target.Y=FMath::Clamp(Target.Y,-Limit,Limit);
        if(F->Aggression==TEXT("aggressive")){const auto* E=Sim.Enemies.FindByPredicate([&](const auto& X){return X.Health>0&&Sim.IsVisible(X.Position)&&FVector2D::Distance(X.Position,V.Position)*MetresPerUnit<C.SensorMeters;});if(E&&FVector2D::Distance(E->Position,V.Position)*MetresPerUnit>C.RadiusMeters*3)Target=E->Position+(V.Position-E->Position).GetSafeNormal()*C.RadiusMeters*3/MetresPerUnit;}
        if(F->Aggression==TEXT("defensive")&&(F->Mission==TEXT("defense")||F->Mission==TEXT("escort"))){FVector2D Cover;if(DefensivePosition(Sim,V,Target,Cover)){Target=Cover;Repositioning=true;}}
        // DefensivePosition returns the present position only when its firing
        // line is clear. An obstacle corner can be less than a metre away while
        // the present muzzle is still screened; station tolerance must not
        // discard that final paid movement and strand the guard indefinitely.
        const bool Arrived=Repositioning?Target.Equals(V.Position,UE_DOUBLE_SMALL_NUMBER):FVector2D::Distance(Target,V.Position)*MetresPerUnit<1;
        if(Arrived){V.Status=Repositioning?TEXT("Defensive firing position"):TEXT("On station");if(Repositioning){V.Route.Empty();V.NextWaypoint=0;}continue;}
        if(V.Route.IsEmpty()||FVector2D::Distance(Target,V.Destination)*MetresPerUnit>FormationSpacingMeters){V.Destination=Target;V.NextWaypoint=0;if(!Sim.FindRoute(V.Position,Target,V.Route,C.RadiusMeters/MetresPerUnit)){V.Status=TEXT("Route blocked");continue;}}
        if(!V.Route.IsValidIndex(V.NextWaypoint)){V.Route.Empty();V.NextWaypoint=0;continue;}const FVector2D Delta=V.Route[V.NextWaypoint]-V.Position;const double Distance=Delta.Size();if(Distance<(Repositioning?UE_DOUBLE_SMALL_NUMBER:.001)){++V.NextWaypoint;continue;}
        double Grade=0;const FVector2D Sample=V.Position+Delta.GetSafeNormal()*FMath::Min(Distance,5/MetresPerUnit);if(TerrainHeight){const double H0=TerrainHeight(V.Position),H1=TerrainHeight(Sample);if(!FMath::IsFinite(H0)||!FMath::IsFinite(H1)){V.Status=TEXT("Terrain unavailable");continue;}Grade=FMath::Abs(H1-H0)/FMath::Max(.001,FVector2D::Distance(V.Position,Sample));}if(Grade>C.MaxGrade){V.Status=TEXT("Slope blocked");continue;}
        bool Road=false;int32 RoadId=0;for(const auto& R:Sim.Roads)if(!R.IsConstructing&&!R.Tier.IsEmpty()&&R.Health>0){const auto AB=R.B-R.A;const double T=FMath::Clamp(FVector2D::DotProduct(V.Position-R.A,AB)/FMath::Max(1.,AB.SizeSquared()),0.,1.);if(FVector2D::Distance(V.Position,R.A+AB*T)*MetresPerUnit<Sim.TransportTiers[R.Tier].WidthMeters*.5){Road=true;RoadId=R.Id;break;}}
        const double Speed=C.SpeedKmh/3.6*(Road&&C.Family==TEXT("wheeled")?RoadSpeedMultiplier:Grade>RoughGrade?C.RoughSpeedMultiplier:1.);const double Travel=FMath::Min3(Distance*MetresPerUnit,Speed*Seconds,V.BatteryKWh/C.TravelKWhPerKm*1000);if(Travel<=1.e-8){V.Status=TEXT("Battery empty");continue;}
        const FVector2D Next=V.Position+Delta/Distance*Travel/MetresPerUnit;bool Blocked=false;for(const auto& B:Sim.Buildings)if(B.Health>0&&HitBox(V.Position,Next,B.Position,Sim.Definition(B)->Footprint+C.RadiusMeters/MetresPerUnit-.001)<=1){Blocked=true;break;}if(Blocked){V.Route.Empty();V.NextWaypoint=0;V.Status=TEXT("Route obstructed");continue;}
        V.Heading=FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X));V.Position=Next;V.DistanceMeters+=Travel;V.BatteryKWh=FMath::Max(0.,V.BatteryKWh-Travel*C.TravelKWhPerKm/1000);EnergySpentKWh+=Travel*C.TravelKWhPerKm/1000;V.Status=Repositioning?TEXT("Repositioning for defense"):TEXT("Moving");if(Travel>=Distance*MetresPerUnit-1.e-8)++V.NextWaypoint;if(RoadId&&C.RoadDamagePerMeter>0)Sim.DamageRoad(RoadId,Travel*C.RoadDamagePerMeter);
    }
}
void FSeigeCombatSystem::Tick(FSeigeSimulation& Sim,double Seconds,bool DefensiveReservePrepared)
{
    if(!FMath::IsFinite(Seconds)||Seconds<=0||Sim.Escaped||Sim.Failed)return;EnsureBuildings(Sim);for(auto It=FabricationPlans.CreateIterator();It;++It)if(!Sim.FindBuilding(It.Key())||Sim.FindBuilding(It.Key())->Health<=0)It.RemoveCurrent();for(auto It=RefitPlans.CreateIterator();It;++It)if(!Sim.FindBuilding(It.Key())||Sim.FindBuilding(It.Key())->Health<=0)It.RemoveCurrent();ShotEvents.RemoveAll([&](const auto& E){return Sim.Time-E.Time>.3;});
    for(int32 I=Fabrication.Num()-1;I>=0;--I){auto& J=Fabrication[I];auto* B=Sim.FindBuilding(J.FactoryId);if(!B||B->Health<=0){Fabrication.RemoveAt(I);continue;}bool First=true;for(int32 Earlier=0;Earlier<I;++Earlier)if(Fabrication[Earlier].FactoryId==J.FactoryId){First=false;break;}if(!First)continue;J.ProgressSeconds+=Seconds*Sim.WorkFraction(*B);if(J.ProgressSeconds+1.e-8>=J.RequiredSeconds){auto V=Spawn(Sim,J.ChassisId,J.Weapons,Sim.BuildingAccessPoint(*B)+FVector2D(0,FormationSpacingMeters/MetresPerUnit),0);V.Status=TEXT("Awaiting charge and fleet assignment");Vehicles.Add(V);Fabrication.RemoveAt(I);}}
    if(!DefensiveReservePrepared)Sim.Energy.RefreshDefensiveReserve(Sim,Seconds);ServiceVehicles(Sim,Seconds);AdvanceTransit(Sim,Seconds);MoveVehicles(Sim,Seconds);
    for(auto& B:Sim.Buildings)if(B.Health>0)if(const auto* P=BuildingPlatforms.Find(B.DefId))
    {auto& State=BuildingState[B.Id];const double Work=BuildingWeaponWork(Sim,B.Id);
     if(Work>0&&Sim.Time-State.LastDamageTime>=ShieldDelay){const double Restore=FMath::Min(P->Shield-State.Shield,ShieldRegen*Seconds*Work);if(Restore>0&&Sim.Energy.Consume(Sim,B.Id,Restore*ShieldRegenKWh)){State.Shield+=Restore;EnergySpentKWh+=Restore*ShieldRegenKWh;}}
     for(int32 I=0;I<State.Weapons.Num();++I){const auto& W=Weapons[State.Weapons[I]];double Available=Seconds*Work;while(Work>0&&Available+1.e-8>=State.Cooldowns[I]){Available=FMath::Max(0.,Available-State.Cooldowns[I]);State.Cooldowns[I]=0;const auto Control=QueryFire(Sim,TEXT("building"),B.Id,B.Position,W,0);if(!Control.Ready||!Fire(Sim,TEXT("building"),B.Id,B.Position,Control.TargetId,W,I)){Available=0;break;}State.Cooldowns[I]=W.ReloadSeconds;}State.Cooldowns[I]=FMath::Max(0.,State.Cooldowns[I]-Available);}}
    for(auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4){for(int32 I=0;I<V.Weapons.Num();++I){const auto& W=Weapons[V.Weapons[I]];double Available=Seconds;while(Available+1.e-8>=V.Cooldowns[I]){Available=FMath::Max(0.,Available-V.Cooldowns[I]);V.Cooldowns[I]=0;const auto Control=QueryFire(Sim,TEXT("vehicle"),V.Id,V.Position,W,0);if(!Control.Ready||!Fire(Sim,TEXT("vehicle"),V.Id,V.Position,Control.TargetId,W,I)){Available=0;break;}V.Cooldowns[I]=W.ReloadSeconds;}V.Cooldowns[I]=FMath::Max(0.,V.Cooldowns[I]-Available);}}
    for(const auto& E:Sim.Enemies)if(E.Health>0&&E.Id%100<int32(BugRangedFraction*100))
    {double& Cooldown=BugCooldowns.FindOrAdd(E.Id);Cooldown=FMath::Max(0.,Cooldown-Seconds);if(Cooldown>0||Projectiles.Num()>=MaximumProjectiles)continue;FVector2D Aim=FVector2D::ZeroVector;double Best=BugRangeMeters/MetresPerUnit;bool Found=false;for(const auto& B:Sim.Buildings)if(B.Health>0&&FVector2D::Distance(E.Position,B.Position)<Best){Aim=B.Position;Best=FVector2D::Distance(E.Position,B.Position);Found=true;}for(const auto& V:Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4&&FVector2D::Distance(E.Position,V.Position)<Best){Aim=V.Position;Best=FVector2D::Distance(E.Position,V.Position);Found=true;}if(!Found)continue;const auto& W=Weapons[BugWeapon];const double Angle=FMath::Atan2(Aim.Y-E.Position.Y,Aim.X-E.Position.X)+FMath::DegreesToRadians((Random.FRand()*2-1)*W.AccuracyDegrees);FSeigeProjectile P;P.Id=NextId++;P.OwnerId=E.Id;P.OwnerKind=TEXT("enemy");P.WeaponId=BugWeapon;P.Position=P.PreviousPosition=E.Position;P.Velocity=FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*W.SpeedMetersSecond/MetresPerUnit;P.RemainingMeters=W.RangeMeters;Projectiles.Add(P);Cooldown=BugFireInterval;}
    for(int32 I=Projectiles.Num()-1;I>=0;--I){if(Projectiles[I].SectorIndex!=4)continue;bool Remove=false;AdvanceProjectile(Sim,Projectiles[I],Seconds,Remove);if(Remove)Projectiles.RemoveAt(I);}
    for(auto It=BugCooldowns.CreateIterator();It;++It)if(!Sim.Enemies.ContainsByPredicate([&](const auto& E){return E.Id==It.Key()&&E.Health>0;}))It.RemoveCurrent();
}
