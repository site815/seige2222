#include "SeigeGameMode.h"
#include "SeigeHardpointLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Math/RotationMatrix.h"

namespace
{
const FLinearColor Hull(.57f,.64f,.64f),Panel(.29f,.37f,.38f),Dark(.055f,.07f,.075f),Trim(.72f,.79f,.75f),Glass(.025f,.09f,.12f);
FLinearColor WeaponColor(const FString& Family){return Family==TEXT("energy")?FLinearColor(.25f,.8f,.95f):Family==TEXT("plasma")?FLinearColor(.55f,.27f,.95f):Family==TEXT("missile")?FLinearColor(.95f,.55f,.15f):FLinearColor(.9f,.82f,.56f);}
FName BarrelTag(int32 Slot,const FString& Weapon){return FName(*FString::Printf(TEXT("seige_barrel_%d_%s"),Slot,*Weapon));}
void TagLastBarrel(AActor* Actor,int32 Slot,const FString& Weapon)
{
    TInlineComponentArray<UStaticMeshComponent*> Parts(Actor);
    if(!Parts.IsEmpty())Parts.Last()->ComponentTags.Add(BarrelTag(Slot,Weapon));
}
UStaticMeshComponent* FindBarrel(AActor* Actor,int32 Slot,const FString& Weapon)
{
    if(!Actor||Actor->IsHidden())return nullptr;
    TInlineComponentArray<UStaticMeshComponent*> Parts(Actor);
    const FName Tag=BarrelTag(Slot,Weapon);
    for(auto* Part:Parts)if(Part->ComponentHasTag(Tag))return Part;
    return nullptr;
}
FVector AimBarrel(UStaticMeshComponent* Barrel,FVector Pivot,FVector End,double CenterDistance)
{
    const FVector Direction=(End-Pivot).GetSafeNormal();
    if(!Direction.IsNearlyZero())
    {
        Barrel->SetWorldLocation(Pivot+Direction*CenterDistance);
        // Engine cylinders extend along local Z. Only the barrel pitches;
        // its support and the vehicle/building stay planted on the ground.
        Barrel->SetWorldRotation(FRotationMatrix::MakeFromZ(Direction).Rotator());
    }
    return Barrel->GetComponentTransform().TransformPosition(FVector(0,0,50));
}
}
void ASeigeGameMode::ConfigureCombatTerrain()
{
    Sim.Combat.SetTerrainSampler([this](FVector2D P){return TerrainHeight(P);});
    Sim.Combat.SetSectorResolver([this](int32 Index)->const FSeigeSimulation*{if(Index==4)return &Sim;const auto* Sector=Neighbors.FindByPredicate([&](const auto& N){return N.Index==Index;});return Sector?&Sector->Sim:nullptr;});
    for(auto& N:Neighbors){const FVector2D Offset=N.Offset;N.Sim.Combat.SetTerrainSampler([this,Offset](FVector2D P){return TerrainHeight(P+Offset);});}
}
void ASeigeGameMode::SyncCombatVisuals(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live)
{
    const auto* Snapshot=PresentationSnapshot(Colony);const auto& C=Colony.Combat;
    const int32 BaseSector=Offset.IsNearlyZero()?4:(FMath::RoundToInt(Offset.Y/(Sim.WorldHalfSize*2))+1)*3+FMath::RoundToInt(Offset.X/(Sim.WorldHalfSize*2))+1;
    auto Sector=[&](int32 Local){return Local==4?BaseSector:Local;};
    auto SectorOffset=[&](int32 Index){return FVector2D(Index%3-1,Index/3-1)*Sim.WorldHalfSize*2;};
    auto Actor=[&](const FString& Key){Live.Add(Key);auto* A=Visuals.FindRef(Key).Get();if(!A){A=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(A);A->SetRootComponent(Root);Root->RegisterComponent();Visuals.Add(Key,A);}A->SetActorHiddenInGame(false);return A;};
    for(const auto& V:C.Vehicles)
    {
        if(V.Health<=0||V.Embarked||V.Evacuated)continue;const auto* D=C.Chassis.Find(V.ChassisId);if(!D)continue;
        const int32 Index=Sector(V.SectorIndex);if(Index!=DetailedSectorIndex())continue;
        // Never interpolate a boundary transfer across two local coordinate frames.
        const FVector2D Position=(Snapshot?Snapshot->Vehicle(V,PresentationAlpha()):V.Position)+SectorOffset(Index);
        if(!Observer&&!Offset.IsNearlyZero()&&!IsWorldVisible(Position))continue;
        const FString Key=Prefix+FString::Printf(TEXT("vehicle_%d"),V.Id),Signature=V.ChassisId+TEXT("_")+FString::Join(V.Weapons,TEXT("_"));
        if(auto* Previous=Visuals.FindRef(Key).Get())if(!Previous->ActorHasTag(FName(*Signature))){Previous->Destroy();Visuals.Remove(Key);}
        const bool New=!Visuals.Contains(Key);auto* A=Actor(Key);
        const double Width=D->RadiusMeters*130.,Length=Width*(D->Family==TEXT("tracked")?1.85:1.65),Z=D->Family==TEXT("mech")?Width*.85:Width*.33;
        if(New)
        {
            A->Tags.Add(FName(*Signature));
            Part(A,TEXT("Cube"),FVector(0,0,Z),FVector(Length*.009,Width*.008,.004*Width),Hull);
            Part(A,TEXT("Cube"),FVector(Length*.20,0,Z+Width*.26),FVector(Length*.003,Width*.006,Width*.002),Panel);
            Part(A,TEXT("Cube"),FVector(Length*.34,0,Z+Width*.25),FVector(.06,Width*.005,Width*.0012),Glass);
            Part(A,TEXT("Cube"),FVector(-Length*.32,0,Z+Width*.24),FVector(Length*.0015,Width*.006,.06),Trim);
            for(double Side:{-1.,1.})
            {
                if(D->Family==TEXT("tracked"))
                {
                    Part(A,TEXT("Cube"),FVector(0,Side*Width*.48,Width*.23),FVector(Length*.011,Width*.0025,Width*.004),Dark);
                    for(int32 K=0;K<5;++K)Part(A,TEXT("Cylinder"),FVector((K/4.-.5)*Length*.86,Side*Width*.63,Width*.23),FVector(Width*.003,Width*.003,.08),Panel,FRotator(0,0,90));
                    Part(A,TEXT("Cube"),FVector(0,Side*Width*.5,Width*.46),FVector(Length*.011,Width*.003,.09),Panel);
                }
                else if(D->Family==TEXT("wheeled"))
                {
                    const int32 Axles=FMath::Max(2,D->Wheels/2);
                    for(int32 K=0;K<Axles;++K)
                    {const FVector P((double(K)/(Axles-1)-.5)*Length*.78,Side*Width*.48,Width*.22);Part(A,TEXT("Cylinder"),P,FVector(Width*.004,Width*.004,Width*.0019),Dark,FRotator(0,0,90));Part(A,TEXT("Cylinder"),P+FVector(0,Side*Width*.10,0),FVector(Width*.002,Width*.002,.06),Trim,FRotator(0,0,90));}
                }
                else
                {
                    const int32 Pairs=FMath::Max(1,D->Legs/2);
                    for(int32 K=0;K<Pairs;++K)
                    {
                        const double X=Pairs>1?(double(K)/(Pairs-1)-.5)*Length*.72:0;
                        Part(A,TEXT("Sphere"),FVector(X,Side*Width*.43,Z),FVector(Width*.0028),Panel);
                        Part(A,TEXT("Cube"),FVector(X,Side*Width*.54,Z*.56),FVector(Width*.0017,Width*.0017,Z*.008),Trim,FRotator(0,0,Side*12));
                        Part(A,TEXT("Cube"),FVector(X+Width*.07,Side*Width*.64,Width*.07),FVector(Width*.004,Width*.0027,Width*.0014),Dark);
                    }
                }
                Part(A,TEXT("Sphere"),FVector(Length*.45,Side*Width*.31,Z+Width*.10),FVector(.10,.10,.10),FLinearColor(.8f,.95f,1));
            }
            const int32 Columns=FMath::Max(1,FMath::CeilToInt(FMath::Sqrt(double(V.Weapons.Num()))));
            for(int32 I=0;I<V.Weapons.Num();++I)if(const auto* Weapon=C.Weapons.Find(V.Weapons[I]))
            {
                const FVector P((I/Columns-(Columns-1)*.5)*Length*.6/Columns,(I%Columns-(Columns-1)*.5)*Width*.7/Columns,Z+Width*.42);
                const double Scale=Weapon->Size==TEXT("large")?4.:Weapon->Size==TEXT("medium")?2.:1.;
                Part(A,TEXT("Cube"),P,FVector(Scale*.50,Scale*.5,Scale*.5),Panel);
                Part(A,Weapon->Family==TEXT("missile")?TEXT("Cube"):TEXT("Cylinder"),P+FVector(Scale*65,0,0),Weapon->Family==TEXT("missile")?FVector(Scale*1.5,Scale*.43,Scale*.43):FVector(Scale*.10,Scale*.10,Scale*1.6),WeaponColor(Weapon->Family),Weapon->Family==TEXT("missile")?FRotator::ZeroRotator:FRotator(90,0,0));
                if(Weapon->Family!=TEXT("missile"))TagLastBarrel(A,I,V.Weapons[I]);
            }
        }
        A->SetActorLocation(RenderPosition(Position,3));A->SetActorRotation(FRotator(0,V.Heading,0));
    }
    for(const auto& B:Colony.Buildings)if(B.Health>0)
    {
        if(BaseSector!=DetailedSectorIndex())continue;
        const auto* Platform=C.BuildingPlatforms.Find(B.DefId);if(!Platform)continue;const auto* D=Colony.Definition(B);if(!D)continue;
        const FVector2D P=B.Position+Offset;if(!Observer&&!Offset.IsNearlyZero()&&!IsWorldVisible(P))continue;
        const auto* State=C.BuildingState.Find(B.Id);const auto& Weapons=State?State->Weapons:Platform->Weapons;
        TArray<FIntVector> CoreMountCells;
        if(D->Role==TEXT("core"))
        {
            TArray<int32> Sizes;Sizes.Reserve(Weapons.Num());
            for(const auto& Id:Weapons){const auto* Mounted=C.Weapons.Find(Id);Sizes.Add(!Mounted?0:Mounted->Size==TEXT("large")?4:Mounted->Size==TEXT("medium")?2:1);}
            if(!SeigePackHardpointBanks(Sizes,CoreMountCells))continue;
        }
        for(int32 I=0;I<Weapons.Num();++I)if(const auto* Weapon=C.Weapons.Find(Weapons[I]))
        {
            const FString Key=Prefix+FString::Printf(TEXT("mount_%d_%d_%s"),B.Id,I,*Weapons[I]);const bool New=!Visuals.Contains(Key);auto* A=Actor(Key);
            const double S=Weapon->Size==TEXT("large")?4.:Weapon->Size==TEXT("medium")?2.:1.;
            if(New){Part(A,TEXT("Cylinder"),FVector(0,0,12*S),FVector(S*.48,S*.48,S*.22),Panel);Part(A,TEXT("Cube"),FVector(0,0,34*S),FVector(S*.50,S*.5,S*.35),Trim);Part(A,Weapon->Family==TEXT("missile")?TEXT("Cube"):TEXT("Cylinder"),FVector(70*S,0,34*S),Weapon->Family==TEXT("missile")?FVector(S*1.4,S*.45,S*.45):FVector(S*.12,S*.12,S*1.6),WeaponColor(Weapon->Family),Weapon->Family==TEXT("missile")?FRotator::ZeroRotator:FRotator(90,0,0));if(Weapon->Family!=TEXT("missile"))TagLastBarrel(A,I,Weapons[I]);}
            const int32 Cols=FMath::Max(1,FMath::CeilToInt(FMath::Sqrt(double(Weapons.Num()))));
            FVector2D Mount((I/Cols-(Cols-1)*.5)*D->Footprint*1.35/Cols,(I%Cols-(Cols-1)*.5)*D->Footprint*1.35/Cols);
            double MountZ=250;
            if(D->Role==TEXT("core"))
            {
                double Footprint=D->Footprint;for(const auto& Pair:Colony.BuildingDefs)if(Pair.Value.Role==TEXT("core")&&Pair.Value.Level==1){Footprint=Pair.Value.Footprint;break;}
                const double ShipSize=Footprint*2*RenderScale,ShipScale=ShipSize/848.586975;
                // Locations are computed largest-first, then mapped back to
                // each unchanged simulation weapon/shot-event slot.
                const FIntVector& Cell=CoreMountCells[I];
                const int32 Bank=Cell.X,CellX=Cell.Y,CellY=Cell.Z;
                const double CellWidth=50.; // Authored 50 cm small socket; module dimensions are physical, not map-scaled.
                Mount=FVector2D(309*ShipScale,((Bank==0?267.:-267.)*ShipScale)+(CellX+S*.5-2)*CellWidth)/RenderScale;
                MountZ=1120*ShipScale+(CellY+S*.5-2)*CellWidth;
                if(!Colony.DeploymentGrounded)MountZ+=(1.-FMath::SmoothStep(0.,1.,Colony.DeploymentElapsed/Colony.Workers.DeploymentDescentSeconds()))*ShipSize*1.7;
            }
            A->SetActorLocation(RenderPosition(P+Mount,MountZ));
            A->SetActorRotation(FVector(B.LastShotPosition-B.Position,0).Rotation());A->SetActorHiddenInGame(B.IsConstructing&&!D->DeploymentDefense);
            if(B.LastShotTime>=0)if(auto* Barrel=FindBarrel(A,I,Weapons[I]))AimBarrel(Barrel,A->GetActorTransform().TransformPosition(FVector(0,0,34*S)),RenderPosition(B.LastShotPosition+Offset,150),70*S);
        }
    }
    for(const auto& P:C.Projectiles)
    {
        const int32 Index=Sector(P.SectorIndex);if(Index!=DetailedSectorIndex())continue;
        const auto* W=C.Weapons.Find(P.WeaponId);if(!W)continue;const FVector2D Point=FMath::Lerp(P.PreviousPosition,P.Position,PresentationAlpha())+SectorOffset(Index);
        if(!Observer&&!IsWorldVisible(Point))continue;const FString Key=Prefix+FString::Printf(TEXT("projectile_%d"),P.Id);const bool New=!Visuals.Contains(Key);auto* A=Actor(Key);
        if(New)Part(A,TEXT("Sphere"),FVector::ZeroVector,FVector(W->Family==TEXT("plasma")?.5:.18),WeaponColor(W->Family));A->SetActorLocation(RenderPosition(Point,130));
    }
    int32 EventIndex=0;for(const auto& Shot:C.ShotEvents)
    {
        const FVector2D Center=(Shot.Start+Shot.End)*.5+Offset;const double Span=Sim.WorldHalfSize*2;
        const int32 Index=(FMath::FloorToInt((Center.Y+Span*.5)/Span)+1)*3+FMath::FloorToInt((Center.X+Span*.5)/Span)+1;
        if(Index!=DetailedSectorIndex())continue;
        if(Colony.Time-Shot.Time>.15)continue;if(!Observer&&!IsWorldVisible(Shot.Start+Offset)&&!IsWorldVisible(Shot.End+Offset))continue;
        const FString Key=Prefix+FString::Printf(TEXT("shot_%d_%s"),EventIndex++,*Shot.Family);const bool New=!Visuals.Contains(Key);auto* A=Actor(Key);
        if(New)Part(A,TEXT("Cube"),FVector::ZeroVector,FVector::OneVector,WeaponColor(Shot.Family));
        FVector Start=RenderPosition(Shot.Start+Offset,150);const FVector End=RenderPosition(Shot.End+Offset,150);
        const FString SourcePrefix=Shot.OwnerSector==4?Prefix:FString::Printf(TEXT("zone_%d_"),Shot.OwnerSector);
        const auto* Weapon=C.Weapons.Find(Shot.WeaponId);
        if(Weapon&&Shot.WeaponSlot!=INDEX_NONE)
        {
            const double WeaponScale=Weapon->Size==TEXT("large")?4.:Weapon->Size==TEXT("medium")?2.:1.;
            if(Shot.OwnerKind==TEXT("building"))
            {
                auto* Mount=Visuals.FindRef(SourcePrefix+FString::Printf(TEXT("mount_%d_%d_%s"),Shot.OwnerId,Shot.WeaponSlot,*Shot.WeaponId)).Get();
                if(Mount&&Live.Contains(SourcePrefix+FString::Printf(TEXT("mount_%d_%d_%s"),Shot.OwnerId,Shot.WeaponSlot,*Shot.WeaponId)))
                    if(auto* Barrel=FindBarrel(Mount,Shot.WeaponSlot,Shot.WeaponId))Start=AimBarrel(Barrel,Mount->GetActorTransform().TransformPosition(FVector(0,0,34*WeaponScale)),End,70*WeaponScale);
            }
            else if(Shot.OwnerKind==TEXT("vehicle"))
            {
                const FSeigeCombatSystem* SourceCombat=&C;
                if(Shot.OwnerSector!=4)if(const auto* Neighbor=Neighbors.FindByPredicate([&](const auto& N){return N.Index==Shot.OwnerSector;}))SourceCombat=&Neighbor->Sim.Combat;
                const auto* Vehicle=SourceCombat->FindVehicle(Shot.OwnerId);
                const auto* Chassis=Vehicle?SourceCombat->Chassis.Find(Vehicle->ChassisId):nullptr;
                const FString VehicleKey=SourcePrefix+FString::Printf(TEXT("vehicle_%d"),Shot.OwnerId);
                auto* VehicleActor=Visuals.FindRef(VehicleKey).Get();
                if(Vehicle&&Chassis&&VehicleActor&&Live.Contains(VehicleKey))if(auto* Barrel=FindBarrel(VehicleActor,Shot.WeaponSlot,Shot.WeaponId))
                {
                    const double Width=Chassis->RadiusMeters*130.,Length=Width*(Chassis->Family==TEXT("tracked")?1.85:1.65),Height=Chassis->Family==TEXT("mech")?Width*.85:Width*.33;
                    const int32 Columns=FMath::Max(1,FMath::CeilToInt(FMath::Sqrt(double(Vehicle->Weapons.Num()))));
                    const FVector Pivot((Shot.WeaponSlot/Columns-(Columns-1)*.5)*Length*.6/Columns,(Shot.WeaponSlot%Columns-(Columns-1)*.5)*Width*.7/Columns,Height+Width*.42);
                    Start=AimBarrel(Barrel,VehicleActor->GetActorTransform().TransformPosition(Pivot),End,65*WeaponScale);
                }
            }
        }
        A->SetActorLocation((Start+End)*.5);A->SetActorRotation((End-Start).Rotation());A->SetActorScale3D(FVector((End-Start).Size()/100.,.045,.045));
    }
}
