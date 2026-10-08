#include "SeigeGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
const FLinearColor BuildMint(.18f,.9f,.68f),BuildRed(1.f,.12f,.08f),Steel(.18f,.23f,.25f),Safety(.8f,.47f,.08f);
double LandingFootprint(const FSeigeSimulation& Colony,double Fallback)
{for(const auto& Pair:Colony.BuildingDefs)if(Pair.Value.Role==TEXT("core")&&Pair.Value.Level==1)return Pair.Value.Footprint;return Fallback;}
AActor* PresentationActor(UWorld* World,FVector Location)
{
    auto* Actor=World->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(Actor);Actor->SetRootComponent(Root);Root->RegisterComponent();
    Actor->SetActorLocation(Location);return Actor;
}
void GhostMaterials(AActor* Actor,UMaterialInterface* Material)
{
    TArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
    for(auto* Mesh:Meshes)
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);
        for(int32 I=0;I<Mesh->GetNumMaterials();++I)Mesh->SetMaterial(I,Material);
    }
}
double Smooth(double T){T=FMath::Clamp(T,0.,1.);return T*T*(3.-2.*T);}
UMaterialInterface* ConstantMaterialParent(UMaterialInterface* Material)
{
    // Unreal forbids a dynamic instance as another instance's parent. Preserve
    // a constant instance's static settings, then copy dynamic uniforms below.
    while(auto* Dynamic=Cast<UMaterialInstanceDynamic>(Material))Material=Dynamic->Parent.Get();
    return Material?Material:UMaterial::GetDefaultMaterial(MD_Surface);
}
}

UMaterialInterface* ASeigeGameMode::ConstructionMaterial(FLinearColor Color,bool Reveal)
{
    const FString Key=(Reveal?TEXT("construction_reveal_"):TEXT("construction_ghost_"))+Color.ToString();
    if(auto* Found=Materials.Find(Key))return *Found;
    const TCHAR* Path=Reveal?TEXT("/Game/Art/Construction/M_ConstructionReveal.M_ConstructionReveal"):TEXT("/Game/Art/Construction/M_ConstructionHologram.M_ConstructionHologram");
    auto* Parent=LoadObject<UMaterialInterface>(nullptr,Path,nullptr,LOAD_NoWarn);
    if(!Parent)return Material(Color); // Missing optional art still has a usable, colored primitive fallback.
    auto* Dynamic=UMaterialInstanceDynamic::Create(Parent,this);
    Dynamic->SetVectorParameterValue(TEXT("Tint"),Color);Dynamic->SetScalarParameterValue(TEXT("Opacity"),.25f);
    Materials.Add(Key,Dynamic);return Dynamic;
}

void ASeigeGameMode::SyncPlacementGhost(TSet<FString>& Live)
{
    if(Observer||DetailedSectorIndex()!=4||!CursorOnWorld||Sim.Escaped||Sim.Failed||(Sim.Won&&!WinAcknowledged)||(Screen!=TEXT("landing")&&Screen!=TEXT("playing")))return;
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(auto* HUD=Cast<ASeigeHUD>(PC->GetHUD()))if(HUD->IsPointerOverUI())return;
    const bool Landing=Screen==TEXT("landing");
    const FString DefinitionId=Landing?Sim.CoreDefinition:SelectedBuild;
    const auto* Definition=Sim.BuildingDefs.Find(DefinitionId);if(!Definition)return;
    FString Reason;const bool Valid=Landing?CanLand(CursorWorld,Reason):Sim.CanPlaceBuilding(DefinitionId,CursorWorld,Reason);
    FString Kind=BuildingVisualKind(*Definition);if(Kind.IsEmpty())Kind=TEXT("Factory");Kind[0]=FChar::ToUpper(Kind[0]);
    const FString Key=TEXT("placement_")+DefinitionId;Live.Add(Key);
    auto* Ghost=Visual(Key,Kind,RenderPosition(CursorWorld,4),Valid?BuildMint:BuildRed,Definition->Footprint*2*RenderScale);
    GhostMaterials(Ghost,ConstructionMaterial(Valid?BuildMint:BuildRed));
    FSeigeBuilding Plot;Plot.DefId=DefinitionId;Plot.Position=CursorWorld;Plot.IsConstructing=true;
    SyncBuildingPlot(Sim,Plot,*Definition,CursorWorld,Key,Live);
}

void ASeigeGameMode::SetConstructionReveal(AActor* Actor,double Progress)
{
    const bool Complete=Progress>=1.;Progress=FMath::Clamp(Progress,0.,1.);
    auto* Template=ConstructionMaterial(FLinearColor::White,true);
    const bool CanClip=!Actor->ActorHasTag(TEXT("PrimitiveFallback"))&&Template&&Template->GetMaterial()&&Template->GetMaterial()->GetName()==TEXT("M_ConstructionReveal");
    TArray<UStaticMeshComponent*> Meshes;Actor->GetComponents(Meshes);
    for(auto* Mesh:Meshes)
    {
        if(!Mesh->ComponentHasTag(TEXT("BuildingBody")))continue;
        // Invisible upper geometry must not intercept picking rays. Sites remain
        // selectable through the existing ground/footprint selection path.
        Mesh->SetCollisionEnabled(Complete?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
        const FString OriginalPrefix=TEXT("construction_original_")+Mesh->GetPathName()+TEXT("_");
        if(Complete)
        {
            if(Mesh->ComponentHasTag(TEXT("ConstructionReveal")))
            {
                for(int32 I=0;I<Mesh->GetNumMaterials();++I)
                {
                    const FString OriginalKey=OriginalPrefix+FString::FromInt(I);
                    if(auto* Original=Materials.Find(OriginalKey))Mesh->SetMaterial(I,*Original);
                    else Mesh->SetMaterial(I,Mesh->GetStaticMesh()->GetMaterial(I));
                    Materials.Remove(OriginalKey);
                }
                Mesh->ComponentTags.Remove(TEXT("ConstructionReveal"));
            }
            continue;
        }
        if(!CanClip)continue;
        if(!Mesh->ComponentHasTag(TEXT("ConstructionReveal")))
        {
            for(int32 I=0;I<Mesh->GetNumMaterials();++I)
            {
                auto* Original=Mesh->GetMaterial(I);
                auto* Dynamic=UMaterialInstanceDynamic::Create(ConstantMaterialParent(Template),Mesh);
                auto* Preserved=UMaterialInstanceDynamic::Create(ConstantMaterialParent(Original),Mesh);
                if(Original)Preserved->CopyMaterialUniformParameters(Original);
                Materials.Add(OriginalPrefix+FString::FromInt(I),Preserved);
                if(Original)Dynamic->CopyMaterialUniformParameters(Original);
                Mesh->SetMaterial(I,Dynamic);
            }
            Mesh->ComponentTags.Add(TEXT("ConstructionReveal"));
        }
        const FBox Bounds=Mesh->GetStaticMesh()->GetBoundingBox();
        const double Height=Mesh->GetComponentLocation().Z+FMath::Lerp(Bounds.Min.Z-4,Bounds.Max.Z+4,Progress)*Mesh->GetComponentScale().Z;
        for(int32 I=0;I<Mesh->GetNumMaterials();++I)if(auto* Dynamic=Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(I)))Dynamic->SetScalarParameterValue(TEXT("RevealHeight"),Height);
    }
    // The no-art path has no clipping shader. Growing its small placeholder is
    // preferable to hiding the complete object until the construction timer ends.
    Actor->SetActorScale3D(FVector(1,1,Complete||CanClip?1:FMath::Max(.025,Progress)));
}

void ASeigeGameMode::SyncConstructionVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live)
{
    const bool Core=Building.DefId==Colony.CoreDefinition;
    const double Progress=Building.IsConstructing?FMath::Clamp(RenderConstructionProgress(Colony,Building),0.,1.):1.;
    FSeigeBuilding RenderBuilding=Building;RenderBuilding.ConstructionProgress=Progress;
    const double Foundation=Colony.ConstructionPhaseProgress(RenderBuilding,0);
    const double Framing=Colony.ConstructionPhaseProgress(RenderBuilding,1);
    const double Enclosure=Colony.ConstructionPhaseProgress(RenderBuilding,2);
    const double Reveal=Building.IsConstructing?Enclosure*.9999:1.;
    const double Size=Definition.Footprint*2*RenderScale;
    AActor* Body=Visuals.FindRef(Key).Get();if(!Body)return;
    const bool IntactCommandHull=Core&&Definition.Visual==TEXT("shuttle")&&!Colony.Escaped;
    // The shuttle arrives intact. Do not clip then restore it every frame while
    // deployment crews work; that would also churn materials and query state.
    if(!IntactCommandHull)SetConstructionReveal(Body,Reveal);
    if(IntactCommandHull)
    {
        // Visual() also creates decorative docked shuttles, which must remain
        // noncolliding. Only this actual command building owns a pickable hull.
        TArray<UStaticMeshComponent*> Hulls;Body->GetComponents(Hulls);
        for(auto* Hull:Hulls)
        {
            if(!Hull->ComponentHasTag(TEXT("BuildingBody")))
            {
                Hull->ComponentTags.Add(TEXT("BuildingBody"));
                Hull->SetCollisionResponseToAllChannels(ECR_Ignore);
                Hull->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
            }
        }
        const double Descent=Colony.DeploymentGrounded?1.:Smooth(Colony.DeploymentElapsed/Colony.Workers.DeploymentDescentSeconds());
        Body->SetActorLocation(RenderPosition(WorldPosition)+FVector(0,0,(1.-Descent)*Size*1.7));
        Body->SetActorRotation(FRotator::ZeroRotator);
        // The level-one command center is the actual parked spacecraft. Its
        // hull lands intact while workers deploy its ground service equipment.
        SetConstructionReveal(Body,1.);
    }
    if(Core&&Definition.Visual!=TEXT("shuttle")&&!Colony.Escaped)
    {
        // Expansion wings surround the original spacecraft. Neither the landing
        // point nor the ship scale changes when the command campus upgrades.
        const FString ShuttleKey=Key+TEXT("_shuttle");Live.Add(ShuttleKey);
        const double ShipSize=LandingFootprint(Colony,Definition.Footprint)*2*RenderScale;
        auto* Shuttle=Visual(ShuttleKey,TEXT("Shuttle"),RenderPosition(WorldPosition),FLinearColor(.65f,.7f,.72f),ShipSize);
        Shuttle->SetActorRotation(FRotator::ZeroRotator);
        TArray<UStaticMeshComponent*> Meshes;Shuttle->GetComponents(Meshes);for(auto* Mesh:Meshes)Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if(Core&&!Colony.Escaped)
    {
        const double ShipSize=LandingFootprint(Colony,Definition.Footprint)*2*RenderScale,ShipScale=ShipSize/848.586975;
        const double Airborne=Colony.DeploymentGrounded?0.:(1.-Smooth(Colony.DeploymentElapsed/Colony.Workers.DeploymentDescentSeconds()))*ShipSize*1.7;
        const FString HatchKey=Key+TEXT("_cargo_hatch");Live.Add(HatchKey);auto* Hatch=Visuals.FindRef(HatchKey).Get();
        if(!Hatch){Hatch=PresentationActor(GetWorld(),RenderPosition(WorldPosition));Visuals.Add(HatchKey,Hatch);Part(Hatch,TEXT("Cube"),FVector::ZeroVector,FVector(.08,1.57,2.82),FLinearColor(.57f,.65f,.68f));}
        const double Open=Colony.DeploymentHatchOpen?1.:Colony.DeploymentGrounded?Smooth((Colony.DeploymentElapsed-Colony.Workers.DeploymentDescentSeconds())/Colony.Workers.DeploymentHatchSeconds()):0.;
        Hatch->SetActorScale3D(FVector(ShipScale));Hatch->SetActorLocation(RenderPosition(WorldPosition)+FVector(308*ShipScale,0,(147+Open*290)*ShipScale+Airborne));
    }
    if(!Building.IsConstructing)return;

    const FString SiteKey=Key+TEXT("_site");Live.Add(SiteKey);
    const FVector SiteLocation=RenderPosition(WorldPosition);
    AActor* Site=Visuals.FindRef(SiteKey).Get();
    double Height=FMath::Max(180.,Body->GetComponentsBoundingBox(true).GetSize().Z);
    if(!Site)
    {
        Site=PresentationActor(GetWorld(),SiteLocation);Visuals.Add(SiteKey,Site);
        const double Radius=Size*.45;
        auto TagLast=[&](const TCHAR* Tag){TArray<UStaticMeshComponent*> Parts;Site->GetComponents(Parts);Parts.Last()->ComponentTags.Add(Tag);};
        for(int32 X=-1;X<=1;++X)for(int32 Y=-1;Y<=1;++Y)
        {
            Part(Site,TEXT("Cube"),FVector(X*Radius*.75,Y*Radius*.75,14),FVector(Size*.0025,Size*.0025,.28),FLinearColor(.34f,.35f,.33f));TagLast(TEXT("Foundation"));
        }
        for(int32 Corner=0;Corner<4;++Corner)
        {
            const double X=(Corner&1)?Radius:-Radius,Y=(Corner&2)?Radius:-Radius;
            Part(Site,TEXT("Cube"),FVector(X,Y,Height*.5),FVector(.16,.16,Height/100),Steel);TagLast(TEXT("FrameColumn"));
            Part(Site,TEXT("Cube"),FVector(X,Y,7),FVector(.5,.5,.14),Safety);
        }
        for(int32 Level=1;Level<=3;++Level)
        {
            const double Z=Height*Level/3;
            for(double Side:{-1.,1.})
            {
                Part(Site,TEXT("Cube"),FVector(0,Side*Radius,Z),FVector(Size*.009,.10,.10),Steel);TagLast(TEXT("FrameBeam"));
                Part(Site,TEXT("Cube"),FVector(Side*Radius,0,Z),FVector(.10,Size*.009,.10),Steel);TagLast(TEXT("FrameBeam"));
            }
        }
        // Scaffolding and modular foundations remain while equipment is fitted.
        // The opaque finished shell appears only during the enclosure phase.
    }
    Site->SetActorLocation(SiteLocation);Site->SetActorHiddenInGame(false);
    TArray<UStaticMeshComponent*> SiteParts;Site->GetComponents(SiteParts);int32 FoundationIndex=0;
    for(auto* Piece:SiteParts)
    {
        if(Piece->ComponentHasTag(TEXT("Foundation")))Piece->SetVisibility(Foundation*9>FoundationIndex++);
        if(Piece->ComponentHasTag(TEXT("FrameColumn")))
        {
            Piece->SetVisibility(Framing>0);
            Piece->SetRelativeLocation(FVector(Piece->GetRelativeLocation().X,Piece->GetRelativeLocation().Y,Height*Framing*.5));
            Piece->SetRelativeScale3D(FVector(.16,.16,Height*FMath::Max(.001,Framing)/100));
        }
        if(Piece->ComponentHasTag(TEXT("FrameBeam")))Piece->SetVisibility(Framing>=Piece->GetRelativeLocation().Z/Height);
    }

    TArray<FString> Resources;Colony.ConstructionCost(Building).GetKeys(Resources);Resources.Sort();
    for(int32 I=0;I<Resources.Num();++I)
    {
        if(Core&&!Colony.DeploymentGrounded)continue; // The deployment kit is still aboard the descending shuttle.
        // Only delivered, uninstalled materials remain physically at the site.
        const double Amount=Building.ConstructionMaterials.FindRef(Resources[I]);
        const FVector2D StackPosition=WorldPosition+FVector2D((I%3-1)*FMath::Max(48.,Definition.Footprint*.65),-Definition.Footprint-65.-(I/3)*65.);
        if(!Observer&&DetailedSectorIndex()!=4&&!IsWorldVisible(StackPosition))continue;
        if(const auto* Resource=Colony.Resources.Find(Resources[I]))SyncStockpile(*Resource,Amount,StackPosition,Key+TEXT("_stock_")+Resources[I],Live);
    }
}

void ASeigeGameMode::SyncServiceVisuals(const FSeigeSimulation&,const FSeigeBuilding&,FVector2D,const FString&,TSet<FString>&) {}
