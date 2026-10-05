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
    FString Kind=Definition->Visual;if(Kind.IsEmpty())Kind=TEXT("Factory");Kind[0]=FChar::ToUpper(Kind[0]);
    const FString Key=TEXT("placement_")+DefinitionId;Live.Add(Key);
    auto* Ghost=Visual(Key,Kind,RenderPosition(CursorWorld,4),Valid?BuildMint:BuildRed,Definition->Footprint*2*RenderScale);
    GhostMaterials(Ghost,ConstructionMaterial(Valid?BuildMint:BuildRed));
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
    const double Progress=Building.IsConstructing?FMath::Clamp(Building.ConstructionProgress,0.,1.):1.;
    const double Reveal=Core?Smooth((Progress-.18)/.82):Progress;
    const double Size=Definition.Footprint*2*RenderScale;
    AActor* Body=Visuals.FindRef(Key).Get();if(!Body)return;
    SetConstructionReveal(Body,Reveal);
    if(Core&&!Colony.Escaped)
    {
        // The authored core has an emergency docking collar on its rear apron.
        // The carried shuttle settles there and stays attached to the colony.
        const FString ShuttleKey=Key+TEXT("_shuttle");Live.Add(ShuttleKey);
        const double Descent=Smooth(Progress/.30);
        // Measured in the centered core source mesh; FBX converts Blender +Y
        // to Unreal -Y. Z is the collar's flat deck, not its raised clamps.
        const FVector DockOffset(0,-1059.*Size/2848.,696.091064*Size/2848.*Reveal+(1.-Descent)*Size*1.7);
        auto* Shuttle=Visual(ShuttleKey,TEXT("Shuttle"),RenderPosition(WorldPosition)+DockOffset,FLinearColor(.65f,.7f,.72f),Size*.20);
        Shuttle->SetActorRotation(FRotator((1-Descent)*-12,0,0));
        TArray<UStaticMeshComponent*> Meshes;Shuttle->GetComponents(Meshes);for(auto* Mesh:Meshes)Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
        for(int32 Corner=0;Corner<4;++Corner)
        {
            const double X=(Corner&1)?Radius:-Radius,Y=(Corner&2)?Radius:-Radius;
            Part(Site,TEXT("Cube"),FVector(X,Y,Height*.5),FVector(.09,.09,Height/100),Steel);
            Part(Site,TEXT("Cube"),FVector(X,Y,7),FVector(.4,.4,.14),Safety);
        }
        for(int32 Level=1;Level<=3;++Level)
        {
            const double Z=Height*Level/3;
            for(double Side:{-1.,1.})
            {
                Part(Site,TEXT("Cube"),FVector(0,Side*Radius,Z),FVector(Size*.009,.06,.06),Steel);
                Part(Site,TEXT("Cube"),FVector(Side*Radius,0,Z),FVector(.06,Size*.009,.06),Steel);
            }
        }
        // Thin translucent final form remains legible above the rising PBR body.
    }
    Site->SetActorLocation(SiteLocation);Site->SetActorHiddenInGame(false);
    const FString PlanKey=Key+TEXT("_plan");Live.Add(PlanKey);
    FString Kind=Definition.Visual;if(Kind.IsEmpty())Kind=TEXT("Factory");Kind[0]=FChar::ToUpper(Kind[0]);
    auto* Plan=Visual(PlanKey,Kind,SiteLocation,BuildMint,Size);
    GhostMaterials(Plan,ConstructionMaterial(FLinearColor(.12f,.42f,.36f)));

    TArray<FString> Resources;Definition.Cost.GetKeys(Resources);Resources.Sort();
    const int32 StackCount=Core?2:FMath::Min(3,Resources.Num());
    for(int32 I=0;I<StackCount;++I)
    {
        const double Ratio=Core?1-Progress:FMath::Clamp(Building.ConstructionMaterials.FindRef(Resources[I])/FMath::Max(Definition.Cost.FindRef(Resources[I]),.001),0.,1.);
        const FString StackKey=Key+FString::Printf(TEXT("_stock_%d"),I);Live.Add(StackKey);
        const FVector2D StackPosition=WorldPosition+FVector2D((I-1)*Definition.Footprint*.38,-Definition.Footprint*.74);
        AActor* Stack=Visuals.FindRef(StackKey).Get();
        if(!Stack)
        {
            Stack=PresentationActor(GetWorld(),RenderPosition(StackPosition));Visuals.Add(StackKey,Stack);
            Part(Stack,TEXT("Cube"),FVector(0,0,6),FVector(.9,.65,.12),Steel);
            const auto* Resource=Core?nullptr:Colony.Resources.Find(Resources[I]);
            const FLinearColor Color=Resource?Resource->Color:FLinearColor(.5f,.6f,.61f);
            for(int32 Level=0;Level<4;++Level)Part(Stack,TEXT("Cube"),FVector(0,0,23+Level*27),FVector(.72,.52,.23),Color);
        }
        Stack->SetActorLocation(RenderPosition(StackPosition));Stack->SetActorHiddenInGame(false);
        TArray<UStaticMeshComponent*> Pieces;Stack->GetComponents(Pieces);
        for(int32 Piece=1;Piece<Pieces.Num();++Piece)Pieces[Piece]->SetVisibility(Piece<=FMath::CeilToInt(Ratio*4));
    }
    const int32 Workers=FMath::Clamp(Building.Builders,0,4);
    for(int32 I=0;I<Workers;++I)
    {
        const double Phase=Colony.Time*.23+I*UE_PI*.5;
        FVector2D Perimeter(FMath::Cos(Phase),FMath::Sin(Phase));
        Perimeter/=FMath::Max(FMath::Abs(Perimeter.X),FMath::Abs(Perimeter.Y));
        const FVector2D Position=WorldPosition+Perimeter*Definition.Footprint*1.1;
        const FString WorkerKey=Key+FString::Printf(TEXT("_builder_%d"),I);Live.Add(WorkerKey);
        auto* Worker=Visual(WorkerKey,TEXT("Robot"),RenderPosition(Position,4),Safety,95);
        Worker->SetActorRotation(FVector(WorldPosition-Position,0).Rotation());
        if(!Worker->ActorHasTag(TEXT("ConstructionTool")))
        {
            Part(Worker,TEXT("Cube"),FVector(38,0,38),FVector(.36,.12,.12),Steel);
            Part(Worker,TEXT("Sphere"),FVector(57,0,38),FVector(.055),BuildMint);
            Worker->Tags.Add(TEXT("ConstructionTool"));
        }
    }
}

void ASeigeGameMode::SyncServiceVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live)
{
    if(Building.IsConstructing||!Building.Enabled||Building.SupportedRobots<=0)return;
    const auto* Definition=Colony.Definition(Building);if(!Definition||Definition->Visual!=TEXT("robotService"))return;
    const double Width=Definition->Footprint*2*RenderScale;
    // Automatic support capacity is independent of staffed production jobs.
    // These are a small representation of supported robots, not extra workers.
    const int32 Occupied=FMath::Clamp(FMath::DivideAndRoundUp(Building.SupportedRobots,4),1,3);
    for(int32 I=0;I<Occupied;++I)
    {
        const FString RobotKey=Key+FString::Printf(TEXT("_service_%d"),I);Live.Add(RobotKey);
        // Authored berth contact centers after Blender-to-Unreal Y conversion.
        const FVector Offset((I-1)*468.*Width/1600.,225.*Width/1600.,87.*Width/1600.);
        auto* Robot=Visual(RobotKey,TEXT("Robot"),RenderPosition(WorldPosition)+Offset,BuildMint,100);
        Robot->SetActorRotation(FRotator(0,180,0));
        if(!Robot->ActorHasTag(TEXT("ServiceStatusLamp")))
        {
            Part(Robot,TEXT("Sphere"),FVector(0,0,112),FVector(.10),BuildMint);
            Robot->Tags.Add(TEXT("ServiceStatusLamp"));
            TArray<UStaticMeshComponent*> Parts;Robot->GetComponents(Parts);if(Parts.Num())Parts.Last()->ComponentTags.Add(TEXT("ServiceStatusLamp"));
        }
        TArray<UStaticMeshComponent*> Parts;Robot->GetComponents(Parts);
        for(auto* Part:Parts)if(Part->ComponentHasTag(TEXT("ServiceStatusLamp")))Part->SetMaterial(0,Material(Building.MaintenanceSupplied?BuildMint:Safety));
    }
}
