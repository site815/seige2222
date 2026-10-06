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
bool Clear(const FSeigeSimulation& Sim,FVector2D P,double Radius)
{
    for(const auto& B:Sim.Buildings)if(B.Health>0)if(const auto* D=Sim.Definition(B))
        if(FMath::Abs(P.X-B.Position.X)<D->Footprint+Radius&&FMath::Abs(P.Y-B.Position.Y)<D->Footprint+Radius)return false;
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
       !D->TryGetStringField(TEXT("mesh"),MeshPath)||!D->TryGetStringField(TEXT("walk_animation"),WalkAnimation)||!D->TryGetStringField(TEXT("idle_animation"),IdleAnimation)||
       !Number(D,TEXT("initial_count"),Count,0,1)||Count!=FMath::FloorToDouble(Count)||!Number(D,TEXT("seed"),Seed,0,MAX_int32)||Seed!=FMath::FloorToDouble(Seed)||
       !Number(D,TEXT("walk_kmh"),WalkKmh,.1,20)||!Number(D,TEXT("walk_cycle_meters"),WalkCycleMeters,.05,5)||!Number(D,TEXT("roam_radius_meters"),RoamRadiusMeters,1,1000)||
       !Number(D,TEXT("food_per_meal_kg"),FoodPerMealKg,.001,100)||!Number(D,TEXT("meal_interval_seconds"),MealIntervalSeconds,1,86400)||
       !Number(D,TEXT("fed_duration_seconds"),FedDurationSeconds,1,172800)||!Number(D,TEXT("feed_radius_meters"),FeedRadiusMeters,1,1000)||
       !Number(D,TEXT("morale_bonus"),MoraleBonus,0,.5)||!Number(D,TEXT("morale_radius_meters"),MoraleRadiusMeters,1,1000)||
       !Number(D,TEXT("rest_seconds"),RestSeconds,0,300)||!Number(D,TEXT("eye_height_cm"),EyeHeightCm,20,150)||
       !Number(D,TEXT("view_fov"),ViewFov,45,110)||!Number(D,TEXT("look_sensitivity"),LookSensitivity,.01,1)||
       !Number(D,TEXT("body_radius_meters"),BodyRadiusMeters,.05,1)||!Number(D,TEXT("maximum_slope_grade"),MaximumSlopeGrade,.1,2))
    {Error=TEXT("Invalid companion rule or missing organic food resource");return false;}
    MetresPerUnit=Sim.MetersPerWorldUnit();Dogs.Reset();ControlledId=0;ControlDirection=FVector2D::ZeroVector;Clock=0;Random.Initialize(int32(Seed));
    const auto* Command=Sim.Buildings.FindByPredicate([&](const auto& B){return B.DefId==Sim.CoreDefinition;});
    if(Count>0&&Command)
    {
        FSeigeCompanion Dog;Dog.Home=Sim.BuildingAccessPoint(*Command)+FVector2D(0,80);Dog.Position=Dog.Target=Dog.Home;Dogs.Add(Dog);
    }
    Error.Empty();return true;
}
void FSeigeCompanionSystem::ShiftHome(FVector2D Delta)
{for(auto& D:Dogs){D.Home+=Delta;D.Position+=Delta;D.Target+=Delta;for(auto& P:D.Route)P+=Delta;}}
bool FSeigeCompanionSystem::SetControlled(int32 Id)
{
    if(Id>0&&(!Find(Id)||Find(Id)->Evacuated))return false;
    ControlledId=Id;ControlDirection=FVector2D::ZeroVector;
    if(auto* D=Find(Id)){D->Route.Empty();D->NextWaypoint=0;D->Moving=false;}
    return true;
}
double FSeigeCompanionSystem::EfficiencyAt(FVector2D P) const
{
    // Morale is capped, so adding companions cannot stack an unlimited bonus.
    for(const auto& D:Dogs)if(!D.Evacuated&&D.FedUntil>Clock&&FVector2D::Distance(P,D.Position)<=MoraleRadiusMeters/FMath::Max(MetresPerUnit,.000001))return 1+MoraleBonus;
    return 1;
}
void FSeigeCompanionSystem::Tick(FSeigeSimulation& Sim,double Seconds)
{
    Clock=Sim.Time;const double Metres=Sim.MetersPerWorldUnit();if(Metres<=0)return;
    const double Step=WalkKmh/3.6/Metres*Seconds;
    for(auto& D:Dogs)
    {
        D.Moving=false;
        if(Sim.Escaped||Sim.Failed){D.Evacuated=true;ControlledId=0;continue;}
        if(D.Evacuated)continue;
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
            const FVector2D Desired=D.Position+ControlDirection*Step;const double Radius=BodyRadiusMeters/Metres,Limit=Sim.WorldHalfSize*3-Radius;
            auto Allowed=[&](FVector2D P){return FMath::Abs(P.X)<Limit&&FMath::Abs(P.Y)<Limit&&Clear(Sim,P,Radius);};
            if(Allowed(Desired))D.Position=Desired;
            else
            {
                const FVector2D AlongX(Desired.X,D.Position.Y),AlongY(D.Position.X,Desired.Y);
                if(Allowed(AlongX))D.Position=AlongX;else if(Allowed(AlongY))D.Position=AlongY;
            }
        }
        else
        {
            if(D.Route.IsEmpty()&&Clock>=D.RestUntil)
            {
                for(int32 Attempt=0;Attempt<12;++Attempt)
                {
                    const double Angle=Random.FRand()*2*PI,Radius=FMath::Sqrt(Random.FRand())*RoamRadiusMeters/Metres;
                    const FVector2D Target=D.Home+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*Radius;
                    if(FMath::Abs(Target.X)>Sim.WorldHalfSize||FMath::Abs(Target.Y)>Sim.WorldHalfSize||!Clear(Sim,Target,BodyRadiusMeters/Metres))continue;
                    if(Sim.FindRoute(D.Position,Target,D.Route)){D.Target=Target;D.NextWaypoint=0;break;}
                }
                if(D.Route.IsEmpty())D.RestUntil=Clock+RestSeconds;
            }
            double Remaining=Step;
            while(D.Route.IsValidIndex(D.NextWaypoint)&&Remaining>0)
            {
                const FVector2D Delta=D.Route[D.NextWaypoint]-D.Position;const double Distance=Delta.Size();
                const FVector2D Next=Distance<=Remaining?D.Route[D.NextWaypoint]:D.Position+Delta/Distance*Remaining;
                if(!Clear(Sim,Next,BodyRadiusMeters/Metres)){D.Route.Empty();D.NextWaypoint=0;D.RestUntil=Clock+RestSeconds;break;}
                if(Distance<=Remaining){D.Position=Next;++D.NextWaypoint;Remaining-=Distance;}
                else{D.Position=Next;Remaining=0;}
            }
            if(!D.Route.IsEmpty()&&!D.Route.IsValidIndex(D.NextWaypoint)){D.Route.Empty();D.NextWaypoint=0;D.RestUntil=Clock+RestSeconds;}
        }
        const FVector2D Delta=D.Position-Before;D.Moving=Delta.SizeSquared()>1.e-9;
        if(D.Moving){D.Heading=FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X));D.DistanceWalked+=Delta.Size()*Metres;}
    }
}
bool FSeigeCompanionSystem::Save(const TSharedPtr<FJsonObject>& Object) const
{
    auto State=MakeShared<FJsonObject>();State->SetNumberField(TEXT("random_seed"),Random.GetCurrentSeed());State->SetNumberField(TEXT("clock"),Clock);
    TArray<TSharedPtr<FJsonValue>> List;
    for(const auto& D:Dogs)
    {
        auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("id"),D.Id);CompanionJsonPoint(O,TEXT("position"),D.Position);CompanionJsonPoint(O,TEXT("home"),D.Home);CompanionJsonPoint(O,TEXT("target"),D.Target);
        O->SetNumberField(TEXT("fed_until"),D.FedUntil);O->SetNumberField(TEXT("next_meal"),D.NextMeal);O->SetNumberField(TEXT("rest_until"),D.RestUntil);O->SetNumberField(TEXT("heading"),D.Heading);
        O->SetNumberField(TEXT("distance_walked_meters"),D.DistanceWalked);O->SetNumberField(TEXT("food_consumed_kg"),D.FoodConsumedKg);O->SetBoolField(TEXT("evacuated"),D.Evacuated);
        O->SetNumberField(TEXT("next_waypoint"),D.NextWaypoint);TArray<TSharedPtr<FJsonValue>> Route;
        for(const auto& P:D.Route){auto V=MakeShared<FJsonObject>();CompanionJsonPoint(V,TEXT("p"),P);Route.Add(MakeShared<FJsonValueObject>(V));}O->SetArrayField(TEXT("route"),Route);
        List.Add(MakeShared<FJsonValueObject>(O));
    }
    State->SetArrayField(TEXT("dogs"),List);Object->SetObjectField(TEXT("companions"),State);return true;
}
bool FSeigeCompanionSystem::Load(const TSharedPtr<FJsonObject>& Object,const FSeigeSimulation& Sim,FString& Error)
{
    const TSharedPtr<FJsonObject>* State=nullptr;const TArray<TSharedPtr<FJsonValue>>* List=nullptr;double Seed=0,Time=0;
    if(!Object->TryGetObjectField(TEXT("companions"),State)||!State||!State->IsValid()||!Number(*State,TEXT("random_seed"),Seed,MIN_int32,MAX_int32)||Seed!=FMath::FloorToDouble(Seed)||!Number(*State,TEXT("clock"),Time,0,Sim.Time+.001)||!(*State)->TryGetArrayField(TEXT("dogs"),List)||List->Num()!=Dogs.Num())
    {Error=TEXT("Invalid saved companion state");return false;}
    TArray<FSeigeCompanion> New;
    for(const auto& V:*List)
    {
        const auto O=V->AsObject();FSeigeCompanion D;double Id=0,Next=0;const TArray<TSharedPtr<FJsonValue>>* Route=nullptr;
        if(!O||!Number(O,TEXT("id"),Id,1,1)||!CompanionJsonPoint(O,TEXT("position"),D.Position,Sim.WorldHalfSize*3)||!CompanionJsonPoint(O,TEXT("home"),D.Home,Sim.WorldHalfSize*3)||!CompanionJsonPoint(O,TEXT("target"),D.Target,Sim.WorldHalfSize*3)||
           !Number(O,TEXT("fed_until"),D.FedUntil,0,Sim.Time+FedDurationSeconds+1)||!Number(O,TEXT("next_meal"),D.NextMeal,0,Sim.Time+MealIntervalSeconds+1)||!Number(O,TEXT("rest_until"),D.RestUntil,0,Sim.Time+RestSeconds+1)||
           !Number(O,TEXT("heading"),D.Heading,-180,180)||!Number(O,TEXT("distance_walked_meters"),D.DistanceWalked,0,1.e12)||!Number(O,TEXT("food_consumed_kg"),D.FoodConsumedKg,0,1.e12)||
           !O->HasTypedField<EJson::Boolean>(TEXT("evacuated"))||!O->TryGetBoolField(TEXT("evacuated"),D.Evacuated)||!O->TryGetArrayField(TEXT("route"),Route)||Route->Num()>4096||!Number(O,TEXT("next_waypoint"),Next,0,Route->Num())||Next!=FMath::FloorToDouble(Next))
        {Error=TEXT("Invalid saved dog companion");return false;}
        D.Id=int32(Id);D.NextWaypoint=int32(Next);
        for(const auto& Waypoint:*Route){FVector2D P;auto W=Waypoint->AsObject();if(!W||!CompanionJsonPoint(W,TEXT("p"),P,Sim.WorldHalfSize*3)){Error=TEXT("Invalid companion route");return false;}D.Route.Add(P);}
        New.Add(D);
    }
    Dogs=MoveTemp(New);Clock=Time;Random.Initialize(int32(Seed));ControlledId=0;ControlDirection=FVector2D::ZeroVector;Error.Empty();return true;
}
