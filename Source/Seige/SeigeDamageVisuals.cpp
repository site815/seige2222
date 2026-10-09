#include "SeigeGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

// Battle-damage presentation. Health is the only input: nothing here changes
// the simulation. The industry material scorches each building in patches
// that spread as its Damage (custom primitive data 0) rises; below 65% health a
// smoke plume rises from the roof, below 30% it also burns there; a destroyed
// building leaves a burnt-out ruin until something is built on its ground.
namespace
{
    // A stable per-building phase so neighbouring plumes do not pulse together.
    double SeigeDamagePhase(int32 Id){return FMath::Frac(Id*.6180339887);}
    constexpr int32 SmokePuffs=7,FirePuffs=4;
    constexpr double SmokePeriodSeconds=7.,FirePeriodSeconds=1.6;
}

void ASeigeGameMode::ApplyDamageTint(AActor* Actor,float Damage,bool BodiesOnly) const
{
    if(!Actor)return;
    TArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
    for(auto* Mesh:Meshes)
    {
        if(BodiesOnly&&!Mesh->ComponentHasTag(TEXT("BuildingBody")))continue;
        const TArray<float>& Data=Mesh->GetCustomPrimitiveData().Data;
        const float Current=Data.IsValidIndex(0)?Data[0]:0.f;
        if(FMath::IsNearlyEqual(Current,Damage,.004f))continue;   // avoid a render-state update every frame
        Mesh->SetCustomPrimitiveDataFloat(0,Damage);
    }
}

void ASeigeGameMode::SyncDamageVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,const FString& Key,TSet<FString>& Live)
{
    AActor* Body=Visuals.FindRef(Key).Get();if(!Body)return;
    const float Damage=Building.IsConstructing?0.f:float(FMath::Clamp(1.-Building.Health/FMath::Max(1.,Definition.Health),0.,1.));
    ApplyDamageTint(Body,Damage,true);
    if(Damage<.35f)return;
    if(!DamageAssetsLoaded)
    {
        DamageAssetsLoaded=true;
        DamagePuffMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"),nullptr,LOAD_NoWarn);
        DamageSmokeMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Construction/M_DamageSmoke.M_DamageSmoke"),nullptr,LOAD_NoWarn);
        DamageFireMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/Construction/M_DamageFire.M_DamageFire"),nullptr,LOAD_NoWarn);
    }
    if(!DamagePuffMesh||!DamageSmokeMaterial)return;
    // The plume leaves the roof at a fixed spot of this building, drifts with a
    // light wind and grows with height; denser and darker the worse the damage.
    const FBox Box=Body->GetComponentsBoundingBox(true);if(!Box.IsValid)return;
    const double Phase=SeigeDamagePhase(Building.Id),Span=FMath::Max(Box.GetSize().X,Box.GetSize().Y);
    const FVector Vent(Box.GetCenter().X+(Phase-.5)*Span*.35,Box.GetCenter().Y+(FMath::Frac(Phase*7.31)-.5)*Span*.35,Box.Max.Z-40);
    const FVector Wind=FVector(.55,.35,1).GetSafeNormal();
    const double Intensity=FMath::Clamp((Damage-.35)/.45,0.,1.),Rise=900+Span*.45,Size=160+Span*.06;
    auto Puffs=[&](const FString& PuffKey,UMaterialInterface* Material,int32 Count,double Period,auto&& Place)
    {
        Live.Add(PuffKey);AActor* Plume=Visuals.FindRef(PuffKey).Get();
        if(!Plume)
        {
            Plume=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(Plume);Plume->SetRootComponent(Root);Root->RegisterComponent();
            for(int32 I=0;I<Count;++I)
            {
                auto* Puff=NewObject<UStaticMeshComponent>(Plume);Puff->SetMobility(EComponentMobility::Movable);Puff->SetStaticMesh(DamagePuffMesh);Puff->SetMaterial(0,Material);
                Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);Puff->SetCastShadow(false);Puff->SetupAttachment(Root);Puff->RegisterComponent();Plume->AddInstanceComponent(Puff);
            }
            Visuals.Add(PuffKey,Plume);
        }
        Plume->SetActorLocation(Vent);
        TArray<UStaticMeshComponent*> Meshes;Plume->GetComponents(Meshes);
        for(int32 I=0;I<Meshes.Num();++I)
        {
            const double T=FMath::Frac(RenderClock/Period+double(I)/Meshes.Num()+Phase);
            double Fade=0,Scale=1;FVector Offset=FVector::ZeroVector;Place(I,T,Offset,Scale,Fade);
            Meshes[I]->SetRelativeLocation(Offset);Meshes[I]->SetRelativeScale3D(FVector(Scale/100.));
            if(!FMath::IsNearlyEqual(Meshes[I]->GetCustomPrimitiveData().Data.IsValidIndex(0)?Meshes[I]->GetCustomPrimitiveData().Data[0]:-1.f,float(Fade),.01f))
                Meshes[I]->SetCustomPrimitiveDataFloat(0,float(Fade));
        }
    };
    Puffs(Key+TEXT("_smoke"),DamageSmokeMaterial,SmokePuffs,SmokePeriodSeconds,[&](int32 I,double T,FVector& Offset,double& Scale,double& Fade)
    {
        Offset=Wind*Rise*T+FVector(FMath::Sin((T+I)*2.1)*Size*.25,FMath::Cos((T+I)*1.7)*Size*.25,0);
        Scale=Size*(.55+1.9*T)*(.8+.4*Intensity);
        Fade=FMath::Sin(T*PI)*FMath::Min(1.,T*5.)*(.45+.55*Intensity);
    });
    if(Damage>=.7f&&DamageFireMaterial)
        Puffs(Key+TEXT("_fire"),DamageFireMaterial,FirePuffs,FirePeriodSeconds,[&](int32 I,double T,FVector& Offset,double& Scale,double& Fade)
        {
            Offset=FVector((I%2?.5:-.5)*Size*.6,(I/2?.5:-.5)*Size*.6,-30+T*Size*1.4);
            Scale=Size*(.9-.6*T)*(.7+.3*FMath::Sin(RenderClock*11.+I));
            Fade=FMath::Sin(T*PI)*(.6+.4*FMath::Clamp((Damage-.7)/.3,0.,1.));
        });
}

void ASeigeGameMode::SyncRuinVisual(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,FVector2D Offset,const FString& Prefix,TSet<FString>& Live)
{
    const auto* D=Colony.Definition(Building);
    if(!D||Building.IsConstructing||D->Role==TEXT("wall")||D->Role==TEXT("core"))return;
    // A later building on the same ground replaces the ruin.
    for(const auto& Other:Colony.Buildings)
    {
        if(Other.Health<=0||Other.Id==Building.Id)continue;const auto* OtherDefinition=Colony.Definition(Other);if(!OtherDefinition)continue;
        const FVector2D Delta=(Other.Position-Building.Position).GetAbs();const double Reach=OtherDefinition->Footprint+D->Footprint;
        if(Delta.X<Reach&&Delta.Y<Reach)return;
    }
    const FVector2D P=Building.Position+Offset;
    if(!Observer&&!Offset.IsNearlyZero()&&!IsWorldVisible(P))return;
    FString Kind=BuildingVisualKind(*D);if(Kind.IsEmpty())Kind=TEXT("Factory");Kind[0]=FChar::ToUpper(Kind[0]);
    const FString Key=Prefix+FString::Printf(TEXT("ruin_%d"),Building.Id);Live.Add(Key);
    const bool Created=!Visuals.Contains(Key);
    AActor* Ruin=Visual(Key,Kind,RenderPosition(P),D->Color,D->Footprint*2.f*RenderScale);
    if(Created&&Ruin)
    {
        // The building's own mesh, burnt out and collapsed to under a third of
        // its height. It does not block picking, so the plot stays selectable.
        TArray<UStaticMeshComponent*> Meshes;Ruin->GetComponents(Meshes);
        for(auto* Mesh:Meshes){Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->ComponentTags.Remove(TEXT("BuildingBody"));}
        ApplyDamageTint(Ruin,1.f,false);
        Ruin->SetActorScale3D(FVector(1,1,.3));
    }
}
