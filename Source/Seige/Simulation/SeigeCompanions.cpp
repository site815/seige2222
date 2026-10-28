#include "SeigeCompanions.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

namespace
{
void CompanionJsonPoint(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,FVector2D P)
{O->SetArrayField(Key,{MakeShared<FJsonValueNumber>(P.X),MakeShared<FJsonValueNumber>(P.Y)});}
bool CompanionJsonPoint(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,FVector2D& P,double Bound)
{
    const TArray<TSharedPtr<FJsonValue>>* A=nullptr;double X=0,Y=0;
    if(!O->TryGetArrayField(Key,A)||A->Num()!=2||!(*A)[0]->TryGetNumber(X)||!(*A)[1]->TryGetNumber(Y)||!FMath::IsFinite(X)||!FMath::IsFinite(Y)||FMath::Abs(X)>Bound||FMath::Abs(Y)>Bound)return false;
    P=FVector2D(X,Y);return true;
}
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& V,double Low,double High)
{return O->HasTypedField<EJson::Number>(Key)&&O->TryGetNumberField(Key,V)&&FMath::IsFinite(V)&&V>=Low&&V<=High;}
bool Clear(const FSeigeSimulation& Sim,FVector2D P,double Radius,bool HatchConnector=false)
{
    for(const auto& B:Sim.Buildings)if(B.Health>0)if(const auto* D=Sim.Definition(B))
    {
        if(HatchConnector&&D->Role==TEXT("core"))
        {
            const FVector2D Hatch=Sim.Workers.HatchPoint(Sim),Port=Sim.BuildingAccessPoint(B);
            if(FMath::Abs(P.Y-Hatch.Y)<.001&&P.X>=Hatch.X-.001&&P.X<=Port.X+.001)continue;
        }
        if(FMath::Abs(P.X-B.Position.X)<D->ReservedFootprint+Radius&&FMath::Abs(P.Y-B.Position.Y)<D->ReservedFootprint+Radius)return false;
    }
    return true;
}
}

FSeigeCompanion* FSeigeCompanionSystem::Find(int32 Id){return Dogs.FindByPredicate([Id](const auto& D){return D.Id==Id;});}
const FSeigeCompanion* FSeigeCompanionSystem::Find(int32 Id) const{return Dogs.FindByPredicate([Id](const auto& D){return D.Id==Id;});}
bool FSeigeCompanionSystem::Initialize(const FString& Directory,const FSeigeSimulation& Sim,FString& Error)
{
    FString Raw;TSharedPtr<FJsonObject> Document;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(Directory,TEXT("companions.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Document)||!Document){Error=TEXT("Cannot read companion rules");return false;}
    const TSharedPtr<FJsonObject>* Object=nullptr;
    if(!Document->TryGetObjectField(TEXT("dog"),Object)||!Object||!Object->IsValid()){Error=TEXT("Missing dog companion definition");return false;}
    const auto D=*Object;double Count=0,Seed=0;
    if(!D->TryGetStringField(TEXT("name"),Name)||Name.IsEmpty()||!D->TryGetStringField(TEXT("food_resource"),FoodResource)||!Sim.Resources.Contains(FoodResource)||
       !D->TryGetStringField(TEXT("mesh"),MeshPath)||!D->TryGetStringField(TEXT("walk_animation"),WalkAnimation)||!D->TryGetStringField(TEXT("run_animation"),RunAnimation)||!D->TryGetStringField(TEXT("idle_animation"),IdleAnimation)||
       !Number(D,TEXT("initial_count"),Count,0,1)||Count!=FMath::FloorToDouble(Count)||!Number(D,TEXT("seed"),Seed,0,MAX_int32)||Seed!=FMath::FloorToDouble(Seed)||
       !Number(D,TEXT("maximum_health"),MaximumHealth,1,10000)||!Number(D,TEXT("danger_radius_meters"),DangerRadiusMeters,1,1000)||
       !Number(D,TEXT("heal_per_second"),HealPerSecond,.01,1000)||!Number(D,TEXT("depression_days"),DepressionDays,1,365)||DepressionDays!=FMath::FloorToDouble(DepressionDays)||
       !Number(D,TEXT("depression_speed_multiplier"),DepressionSpeedMultiplier,.01,1)||
       !Number(D,TEXT("walk_kmh"),WalkKmh,.1,20)||!Number(D,TEXT("walk_cycle_meters"),WalkCycleMeters,.05,5)||!Number(D,TEXT("run_cycle_meters"),RunCycleMeters,.05,8)||!Number(D,TEXT("roam_radius_meters"),RoamRadiusMeters,1,1000)||
       !Number(D,TEXT("territory_radius_meters"),TerritoryRadiusMeters,1,1000)||!Number(D,TEXT("damage_alert_radius_meters"),DamageAlertRadiusMeters,1,1000)||
       !Number(D,TEXT("shelter_seconds"),ShelterSeconds,1,3600)||!Number(D,TEXT("run_kmh"),RunKmh,WalkKmh,40)||RoamRadiusMeters>TerritoryRadiusMeters||
       !Number(D,TEXT("acceleration_meters_per_second_squared"),AccelerationMetersPerSecondSquared,.1,30)||
       !Number(D,TEXT("deceleration_meters_per_second_squared"),DecelerationMetersPerSecondSquared,.1,50)||
       !Number(D,TEXT("turn_degrees_per_second"),TurnDegreesPerSecond,30,720)||
       !Number(D,TEXT("food_per_meal_kg"),FoodPerMealKg,.001,100)||!Number(D,TEXT("meal_interval_seconds"),MealIntervalSeconds,1,86400)||
       !Number(D,TEXT("fed_duration_seconds"),FedDurationSeconds,1,172800)||!Number(D,TEXT("feed_radius_meters"),FeedRadiusMeters,1,1000)||
       !Number(D,TEXT("morale_bonus"),MoraleBonus,0,.5)||!Number(D,TEXT("morale_radius_meters"),MoraleRadiusMeters,1,1000)||
       !Number(D,TEXT("rest_seconds"),RestSeconds,0,300)||!Number(D,TEXT("eye_height_cm"),EyeHeightCm,20,150)||
       !Number(D,TEXT("view_fov"),ViewFov,45,110)||!Number(D,TEXT("look_sensitivity"),LookSensitivity,.01,1)||
       !Number(D,TEXT("body_radius_meters"),BodyRadiusMeters,.05,1)||!Number(D,TEXT("maximum_slope_grade"),MaximumSlopeGrade,.1,2))
    {Error=TEXT("Invalid companion rule or missing organic food resource");return false;}
    LocomotionBlendSpace.Empty();D->TryGetStringField(TEXT("locomotion_blend_space"),LocomotionBlendSpace);
    MetresPerUnit=Sim.MetersPerWorldUnit();Dogs.Reset();ResetControl();MovementConstraint={};Clock=0;DepressionUntilCalendarUs=0;Random.Initialize(int32(Seed));
    const auto* Command=Sim.Buildings.FindByPredicate([&](const auto& B){const auto* D=Sim.Definition(B);return D&&D->Role==TEXT("core");});
    if(Count>0&&Command)
    {
        FSeigeCompanion Dog;Dog.Home=Command->Position;Dog.Health=MaximumHealth;Dog.Position=Dog.Target=Sim.Workers.HatchPoint(Sim);Dogs.Add(Dog);
    }
    Error.Empty();return true;
}
void FSeigeCompanionSystem::ShiftHome(FVector2D Delta)
{for(auto& D:Dogs){D.Home+=Delta;D.Position+=Delta;D.Target+=Delta;for(auto& P:D.Route)P+=Delta;}}
void FSeigeCompanionSystem::Alert(const FSeigeSimulation& Sim,FSeigeCompanion& D)
{
    if(D.Dead||D.Evacuated)return;
    D.ShelterUntil=Sim.Time+ShelterSeconds;
    if(!D.Aboard&&!D.Sheltered&&!D.Retreating){D.Retreating=true;D.ExitingShuttle=false;D.Route.Empty();D.NextWaypoint=0;}
    if(ControlledId==D.Id)ResetControl();
}
void FSeigeCompanionSystem::NotifyBuildingDamaged(const FSeigeSimulation& Sim,FVector2D Position)
{
    const auto* Core=Sim.Buildings.FindByPredicate([&](const auto& B){const auto* Def=Sim.Definition(B);return Def&&Def->Role==TEXT("core");});if(!Core||FVector2D::Distance(Position,Core->Position)*Sim.MetersPerWorldUnit()>DamageAlertRadiusMeters)return;
    for(auto& D:Dogs)Alert(Sim,D);
}
void FSeigeCompanionSystem::Damage(FSeigeSimulation& Sim,int32 Id,double Amount)
{
    auto* D=Find(Id);if(!D||!IsOutside(*D)||!FMath::IsFinite(Amount)||Amount<=0)return;
    D->Health=FMath::Max(0.,D->Health-Amount);
    if(D->Health>0){Alert(Sim,*D);return;}
    D->Dead=true;D->Moving=D->Retreating=D->ExitingShuttle=false;D->Route.Empty();D->NextWaypoint=0;
    if(ControlledId==Id)ResetControl();
    D->SpeedMetersPerSecond=D->MotionSpeedMetersPerSecond=0;
    const auto& R=Sim.Calendar.GetRules();
    const int64 Duration=int64(DepressionDays)*(R.DaylightMicroseconds+R.NightMicroseconds);
    DepressionUntilCalendarUs=FMath::Min<int64>(FSeigeWorldCalendar::MaximumElapsedMicroseconds,Sim.Calendar.ElapsedMicroseconds()+Duration);
    Sim.AddEvent(FString::Printf(TEXT("%s has died. Workers move at %.0f%% speed for %.0f calendar days."),*Name,DepressionSpeedMultiplier*100,DepressionDays));
}
double FSeigeCompanionSystem::WorkerSpeedMultiplier(const FSeigeSimulation& Sim) const
{return Sim.Calendar.ElapsedMicroseconds()<DepressionUntilCalendarUs?DepressionSpeedMultiplier:1.;}
bool FSeigeCompanionSystem::SetControlled(int32 Id)
{
    if(Id>0&&(!Find(Id)||!IsOutside(*Find(Id))||Find(Id)->Retreating||Find(Id)->Sheltered||Find(Id)->ExitingShuttle))return false;
    ResetControl();ControlledId=Id;
    if(auto* D=Find(Id)){D->Route.Empty();D->NextWaypoint=0;D->Moving=false;D->SpeedMetersPerSecond=D->MotionSpeedMetersPerSecond=0;ControlFacing=D->Heading;}
    return true;
}
void FSeigeCompanionSystem::ResetControl()
{
    if(auto* D=Find(ControlledId)){D->SpeedMetersPerSecond=D->MotionSpeedMetersPerSecond=0;D->Moving=false;}
    ControlledId=0;ControlDirection=FVector2D::ZeroVector;ControlSprint=false;ControlFacing=0;
}
double FSeigeCompanionSystem::EfficiencyAt(FVector2D P) const
{
    // Morale is capped, so adding companions cannot stack an unlimited bonus.
    for(const auto& D:Dogs)if(IsOutside(D)&&D.FedUntil>Clock&&FVector2D::Distance(P,D.Position)<=MoraleRadiusMeters/FMath::Max(MetresPerUnit,.000001))return 1+MoraleBonus;
    return 1;
}
void FSeigeCompanionSystem::Tick(FSeigeSimulation& Sim,double Seconds)
{
    if(!FMath::IsFinite(Seconds)||Seconds<=0)return;
    Clock=Sim.Time;const double Metres=Sim.MetersPerWorldUnit();if(Metres<=0)return;
    for(auto& D:Dogs)
    {
        D.Moving=false;D.SpeedMetersPerSecond=0;
        if(Sim.Escaped||Sim.Failed){D.Evacuated=true;ResetControl();D.MotionSpeedMetersPerSecond=0;continue;}
        if(D.Evacuated||D.Dead)continue;
        const auto* Core=Sim.Buildings.FindByPredicate([&](const auto& B){const auto* D=Sim.Definition(B);return D&&D->Role==TEXT("core");});if(!Core)continue;
        D.Home=Core->Position;
        // Proximity danger extends the safe interval, including while indoors.
        const FVector2D AlertOrigin=D.Aboard||D.Sheltered?Sim.Workers.HatchPoint(Sim):D.Position;
        bool Danger=false;
        for(const auto& E:Sim.Enemies)if(E.Health>0&&FVector2D::Distance(E.Position,AlertOrigin)*Metres<=DangerRadiusMeters){Danger=true;break;}
        if(!Danger)for(const auto& P:Sim.Combat.Projectiles)if(P.SectorIndex==4&&P.OwnerKind==TEXT("enemy")&&FVector2D::Distance(P.Position,AlertOrigin)*Metres<=DangerRadiusMeters){Danger=true;break;}
        if(Danger)Alert(Sim,D);
        if(D.Aboard)
        {
            if(!Sim.DeploymentHatchOpen||Clock<D.ShelterUntil)continue;
            D.Aboard=false;D.ExitingShuttle=true;D.Route={Sim.BuildingAccessPoint(*Core)};D.NextWaypoint=0;D.Target=Sim.BuildingAccessPoint(*Core);
        }
        const double Step=(D.Retreating?RunKmh:WalkKmh)/3.6/Metres*Seconds;
        if(D.Sheltered)
        {
            D.Health=FMath::Min(MaximumHealth,D.Health+HealPerSecond*Seconds);
            if(Clock<D.ShelterUntil||D.Health<MaximumHealth)continue;
            D.Sheltered=false;D.ExitingShuttle=true;D.Route={Sim.BuildingAccessPoint(*Core)};D.NextWaypoint=0;
        }
        if(D.Retreating&&D.Route.IsEmpty())
        {
            const FVector2D Port=Sim.BuildingAccessPoint(*Core),Hatch=Sim.Workers.HatchPoint(Sim);
            if(Clear(Sim,D.Position,BodyRadiusMeters/Metres)&&Sim.FindRoute(D.Position,Port,D.Route,BodyRadiusMeters/Metres))
            {D.Route.Add(Hatch);D.NextWaypoint=0;D.Target=Hatch;}
            else if(FMath::Abs(D.Position.Y-Hatch.Y)<.001&&D.Position.X>=Hatch.X-.001&&D.Position.X<=Port.X+.001)
            {D.Route={Hatch};D.NextWaypoint=0;D.Target=Hatch;}
        }
        if(Clock>=D.NextMeal)
        {
            // A single actual local stock pays a meal. Remote/global inventory
            // cannot feed a companion through the fog or across a sector.
            for(auto& B:Sim.Buildings)if(B.Health>0&&!B.IsConstructing&&FVector2D::Distance(B.Position,D.Position)*Metres<=FeedRadiusMeters&&B.Inventory.FindRef(FoodResource)+UE_DOUBLE_SMALL_NUMBER>=FoodPerMealKg)
            {
                B.Inventory.FindOrAdd(FoodResource)=FMath::Max(0.,B.Inventory.FindRef(FoodResource)-FoodPerMealKg);D.FoodConsumedKg+=FoodPerMealKg;D.FedUntil=Clock+FedDurationSeconds;D.NextMeal=Clock+MealIntervalSeconds;break;
            }
        }
        const FVector2D Before=D.Position;
        if(ControlledId==D.Id)
        {
            const double Radius=BodyRadiusMeters/Metres,Limit=Sim.WorldHalfSize*3-Radius;
            auto Allowed=[&](FVector2D P){return FVector2D::Distance(P,D.Home)*Metres<=TerritoryRadiusMeters&&FMath::Abs(P.X)<Limit&&FMath::Abs(P.Y)<Limit&&Clear(Sim,P,Radius)&&Sim.ClearWalkingLine(D.Position,P,Radius)&&Sim.Environment.SegmentDry(D.Position,P,Radius)&&(!MovementConstraint||MovementConstraint(D.Position,P,BodyRadiusMeters));};
            const bool HasInput=!ControlDirection.IsNearlyZero();
            const double DesiredHeading=HasInput?FMath::RadiansToDegrees(FMath::Atan2(ControlDirection.Y,ControlDirection.X)):ControlFacing;
            // Integrate short physical strides so turning and collision stay stable
            // at fixed simulation steps. Rex moves along his body, never sideways.
            const int32 Substeps=FMath::Max(1,FMath::CeilToInt(Seconds/.025));
            const double Dt=Seconds/Substeps;
            for(int32 I=0;I<Substeps;++I)
            {
                const double Turn=FMath::FindDeltaAngleDegrees(D.Heading,DesiredHeading);
                D.Heading=FMath::UnwindDegrees(D.Heading+FMath::Clamp(Turn,-TurnDegreesPerSecond*Dt,TurnDegreesPerSecond*Dt));
                const double Alignment=FMath::Max(0.,FMath::Cos(FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(D.Heading,DesiredHeading))));
                const double DesiredSpeed=HasInput?(ControlSprint?RunKmh:WalkKmh)/3.6*ControlDirection.Size()*Alignment*Alignment:0;
                const double Rate=DesiredSpeed>D.MotionSpeedMetersPerSecond?AccelerationMetersPerSecondSquared:DecelerationMetersPerSecondSquared;
                const double PreviousSpeed=D.MotionSpeedMetersPerSecond;
                D.MotionSpeedMetersPerSecond+=FMath::Clamp(DesiredSpeed-D.MotionSpeedMetersPerSecond,-Rate*Dt,Rate*Dt);
                const double Travel=(PreviousSpeed+D.MotionSpeedMetersPerSecond)*.5*Dt/Metres;
                if(Travel<=1.e-9)continue;
                const double Angle=FMath::DegreesToRadians(D.Heading);
                const FVector2D Desired=D.Position+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Travel;
                if(Allowed(Desired))D.Position=Desired;
                else D.MotionSpeedMetersPerSecond=0;
            }
        }
        else
        {
            if(!D.Retreating&&!D.ExitingShuttle&&D.Route.IsEmpty()&&Clock>=D.RestUntil)
            {
                for(int32 Attempt=0;Attempt<12;++Attempt)
                {
                    const double Angle=Random.FRand()*2*PI,Radius=FMath::Sqrt(Random.FRand())*RoamRadiusMeters/Metres;
                    const FVector2D Target=D.Home+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;
                    if(FMath::Abs(Target.X)>Sim.WorldHalfSize||FMath::Abs(Target.Y)>Sim.WorldHalfSize||!Clear(Sim,Target,BodyRadiusMeters/Metres))continue;
                    if(Sim.FindRoute(D.Position,Target,D.Route,BodyRadiusMeters/Metres))
                    {
                        bool Inside=true;for(const auto& P:D.Route)Inside&=FVector2D::Distance(P,D.Home)*Metres<=TerritoryRadiusMeters;
                        if(Inside){D.Target=Target;D.NextWaypoint=0;break;}D.Route.Empty();
                    }
                }
                if(D.Route.IsEmpty())D.RestUntil=Clock+RestSeconds;
            }
            double Remaining=Step;
            while(D.Route.IsValidIndex(D.NextWaypoint)&&Remaining>0)
            {
                const FVector2D Delta=D.Route[D.NextWaypoint]-D.Position;const double Distance=Delta.Size();
                const FVector2D Next=Distance<=Remaining?D.Route[D.NextWaypoint]:D.Position+Delta/Distance*Remaining;
                if(!Sim.Environment.SegmentDry(D.Position,Next,BodyRadiusMeters/Metres)||!Clear(Sim,Next,BodyRadiusMeters/Metres,D.Retreating||D.ExitingShuttle)||FVector2D::Distance(Next,D.Home)*Metres>TerritoryRadiusMeters){D.Route.Empty();D.NextWaypoint=0;D.RestUntil=Clock+RestSeconds;break;}
                if(Distance<=Remaining){D.Position=Next;++D.NextWaypoint;Remaining-=Distance;}
                else{D.Position=Next;Remaining=0;}
            }
            if(!D.Route.IsEmpty()&&!D.Route.IsValidIndex(D.NextWaypoint))
            {D.Route.Empty();D.NextWaypoint=0;D.RestUntil=Clock+RestSeconds;if(D.Retreating){D.Retreating=false;D.Sheltered=true;}D.ExitingShuttle=false;}
        }
        const FVector2D Delta=D.Position-Before;D.Moving=Delta.SizeSquared()>1.e-9;
        if(D.Moving)
        {
            if(ControlledId!=D.Id)D.Heading=FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X));
            D.DistanceWalked+=Delta.Size()*Metres;D.SpeedMetersPerSecond=Delta.Size()*Metres/Seconds;
        }
        if(ControlledId!=D.Id)D.MotionSpeedMetersPerSecond=D.SpeedMetersPerSecond;
    }
}
bool FSeigeCompanionSystem::Save(const TSharedPtr<FJsonObject>& Object) const
{
    auto State=MakeShared<FJsonObject>();State->SetNumberField(TEXT("random_seed"),Random.GetCurrentSeed());State->SetNumberField(TEXT("clock"),Clock);State->SetNumberField(TEXT("depression_until_calendar_us"),double(DepressionUntilCalendarUs));
    TArray<TSharedPtr<FJsonValue>> List;
    for(const auto& D:Dogs)
    {
        auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("id"),D.Id);CompanionJsonPoint(O,TEXT("position"),D.Position);CompanionJsonPoint(O,TEXT("home"),D.Home);CompanionJsonPoint(O,TEXT("target"),D.Target);
        O->SetNumberField(TEXT("fed_until"),D.FedUntil);O->SetNumberField(TEXT("next_meal"),D.NextMeal);O->SetNumberField(TEXT("rest_until"),D.RestUntil);O->SetNumberField(TEXT("heading"),D.Heading);
        O->SetNumberField(TEXT("distance_walked_meters"),D.DistanceWalked);O->SetNumberField(TEXT("food_consumed_kg"),D.FoodConsumedKg);O->SetBoolField(TEXT("evacuated"),D.Evacuated);
        O->SetNumberField(TEXT("health"),D.Health);O->SetBoolField(TEXT("dead"),D.Dead);O->SetBoolField(TEXT("aboard"),D.Aboard);
        O->SetNumberField(TEXT("shelter_until"),D.ShelterUntil);O->SetBoolField(TEXT("retreating"),D.Retreating);O->SetBoolField(TEXT("sheltered"),D.Sheltered);O->SetBoolField(TEXT("exiting_shuttle"),D.ExitingShuttle);
        O->SetNumberField(TEXT("next_waypoint"),D.NextWaypoint);TArray<TSharedPtr<FJsonValue>> Route;
        for(const auto& P:D.Route){auto V=MakeShared<FJsonObject>();CompanionJsonPoint(V,TEXT("p"),P);Route.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("route"),Route);
        List.Add(MakeShared<FJsonValueObject>(O));
    }
    State->SetArrayField(TEXT("dogs"),List);Object->SetObjectField(TEXT("companions"),State);return true;
}
bool FSeigeCompanionSystem::Load(const TSharedPtr<FJsonObject>& Object,const FSeigeSimulation& Sim,FString& Error)
{
    const TSharedPtr<FJsonObject>* State=nullptr;const TArray<TSharedPtr<FJsonValue>>* List=nullptr;double Seed=0,Time=0,Depression=0;
    if(!Object->TryGetObjectField(TEXT("companions"),State)||!State||!State->IsValid()||!Number(*State,TEXT("random_seed"),Seed,MIN_int32,MAX_int32)||Seed!=FMath::FloorToDouble(Seed)||!Number(*State,TEXT("clock"),Time,0,Sim.Time+.001)||!(*State)->TryGetArrayField(TEXT("dogs"),List)||List->Num()!=Dogs.Num())
    {Error=TEXT("Invalid saved companion state");return false;}
    const auto& CalendarRules=Sim.Calendar.GetRules();
    const int64 MaximumDeadline=FMath::Min<int64>(FSeigeWorldCalendar::MaximumElapsedMicroseconds,Sim.Calendar.ElapsedMicroseconds()+int64(DepressionDays)*(CalendarRules.DaylightMicroseconds+CalendarRules.NightMicroseconds));
    if(!Number(*State,TEXT("depression_until_calendar_us"),Depression,0,double(MaximumDeadline))||Depression!=FMath::FloorToDouble(Depression)){Error=TEXT("Invalid saved companion grief deadline");return false;}
    TArray<FSeigeCompanion> New;
    for(const auto& V:*List)
    {
        const auto O=V->AsObject();FSeigeCompanion D;double Id=0,Next=0;const TArray<TSharedPtr<FJsonValue>>* Route=nullptr;
        if(!O||!Number(O,TEXT("id"),Id,1,1)||!CompanionJsonPoint(O,TEXT("position"),D.Position,Sim.WorldHalfSize*3)||!CompanionJsonPoint(O,TEXT("home"),D.Home,Sim.WorldHalfSize*3)||!CompanionJsonPoint(O,TEXT("target"),D.Target,Sim.WorldHalfSize*3)||
           !Number(O,TEXT("fed_until"),D.FedUntil,0,Sim.Time+FedDurationSeconds+1)||!Number(O,TEXT("next_meal"),D.NextMeal,0,Sim.Time+MealIntervalSeconds+1)||!Number(O,TEXT("rest_until"),D.RestUntil,0,Sim.Time+RestSeconds+1)||
           !Number(O,TEXT("heading"),D.Heading,-180,180)||!Number(O,TEXT("distance_walked_meters"),D.DistanceWalked,0,1.e12)||!Number(O,TEXT("food_consumed_kg"),D.FoodConsumedKg,0,1.e12)||
           !O->HasTypedField<EJson::Boolean>(TEXT("evacuated"))||!O->TryGetBoolField(TEXT("evacuated"),D.Evacuated)||!O->TryGetArrayField(TEXT("route"),Route)||Route->Num()>4096||!Number(O,TEXT("next_waypoint"),Next,0,Route->Num())||Next!=FMath::FloorToDouble(Next))
        {Error=TEXT("Invalid saved dog companion");return false;}
        if(!Number(O,TEXT("health"),D.Health,0,MaximumHealth)||!O->TryGetBoolField(TEXT("dead"),D.Dead)||!O->TryGetBoolField(TEXT("aboard"),D.Aboard)||D.Dead!=(D.Health<=0)||
           !Number(O,TEXT("shelter_until"),D.ShelterUntil,0,Sim.Time+ShelterSeconds+1)||!O->TryGetBoolField(TEXT("retreating"),D.Retreating)||!O->TryGetBoolField(TEXT("sheltered"),D.Sheltered)||!O->TryGetBoolField(TEXT("exiting_shuttle"),D.ExitingShuttle)||int32(D.Retreating)+int32(D.Sheltered)+int32(D.ExitingShuttle)+int32(D.Aboard)+int32(D.Dead)>1)
        {Error=TEXT("Invalid saved companion shelter state");return false;}
        if((D.Aboard||D.Sheltered)&&!D.Position.Equals(Sim.Workers.HatchPoint(Sim),.001)||D.Dead&&(Route->Num()>0||Depression<=0)||!D.Dead&&Depression>0)
        {Error=TEXT("Invalid saved companion life state");return false;}
        D.Id=int32(Id);D.NextWaypoint=int32(Next);
        for(const auto& Waypoint:*Route){FVector2D P;auto W=Waypoint->AsObject();if(!W||!CompanionJsonPoint(W,TEXT("p"),P,Sim.WorldHalfSize*3)){Error=TEXT("Invalid companion route");return false;}D.Route.Add(P);}
        New.Add(D);
    }
    Dogs=MoveTemp(New);Clock=Time;DepressionUntilCalendarUs=int64(Depression);Random.Initialize(int32(Seed));ResetControl();MovementConstraint={};Error.Empty();return true;
}
