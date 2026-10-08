#include "SeigeGameMode.h"
#include "Simulation/SeigeWorksiteLayout.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
const FLinearColor ToolSteel(.17f,.20f,.22f),WorkAmber(.86f,.52f,.15f);
AActor* WorkActor(UWorld* World,FVector Position)
{
    auto* A=World->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(A);
    A->SetRootComponent(Root);Root->RegisterComponent();A->SetActorLocation(Position);return A;
}
double Ease(double T){T=FMath::Clamp(T,0.,1.);return T*T*(3.-2.*T);}
TArray<FSeigeWorksiteBounds> KnownBuildingBounds(const ASeigeGameMode& G,const FSeigeSimulation& OwnerColony,int32 OwnerId)
{
    TArray<FSeigeWorksiteBounds> Result;
    auto Add=[&](const FSeigeSimulation& Colony,FVector2D Offset,bool Home)
    {
        for(const auto& B:Colony.Buildings)
        {
            if(B.Health<=0||(&Colony==&OwnerColony&&B.Id==OwnerId))continue;
            const FVector2D P=B.Position+Offset;
            if(!G.Observer&&!Home&&!G.IsWorldVisible(P))continue;
            if(const auto* D=Colony.Definition(B))Result.Add({P,D->ReservedFootprint});
        }
    };
    Add(G.Sim,FVector2D::ZeroVector,true);for(const auto& N:G.Neighbors)Add(N.Sim,N.Offset,false);
    return Result;
}
}

void ASeigeGameMode::CaptureSimulationPresentation()
{
    PresentationSnapshots.FindOrAdd(4).Capture(Sim);
    for(const auto& N:Neighbors)PresentationSnapshots.FindOrAdd(N.Index).Capture(N.Sim);
}
void ASeigeGameMode::ResetSimulationPresentation(){PresentationSnapshots.Reset();CaptureSimulationPresentation();}
const FSeigeRenderSnapshot* ASeigeGameMode::PresentationSnapshot(const FSeigeSimulation& Colony) const
{
    if(&Colony==&Sim)return PresentationSnapshots.Find(4);
    for(const auto& N:Neighbors)if(&N.Sim==&Colony)return PresentationSnapshots.Find(N.Index);
    return nullptr;
}
double ASeigeGameMode::PresentationAlpha() const{return Ready?FMath::Clamp(Accumulator/FMath::Max(Sim.FixedStepSeconds(),UE_DOUBLE_SMALL_NUMBER),0.,1.):0.;}
double ASeigeGameMode::RenderSimulationTime(const FSeigeSimulation& Colony) const
{const auto* Snapshot=PresentationSnapshot(Colony);return Snapshot?Snapshot->RenderTime(Colony,PresentationAlpha()):Colony.Time;}
double ASeigeGameMode::RenderConstructionProgress(const FSeigeSimulation& Colony,const FSeigeBuilding& Building) const
{const auto* Snapshot=PresentationSnapshot(Colony);return Snapshot?Snapshot->Progress(Building,PresentationAlpha()):Building.ConstructionProgress;}

void ASeigeGameMode::SyncStockpile(const FSeigeResourceDef& Resource,double Amount,FVector2D Position,const FString& Key,TSet<FString>& Live)
{
    if(Amount<=UE_DOUBLE_SMALL_NUMBER)return;
    Live.Add(Key);auto* A=Visuals.FindRef(Key).Get();
    const bool Bulk=Resource.StockpileVisual==TEXT("bulk"),Ingots=Resource.StockpileVisual==TEXT("ingots");
    if(!A)
    {
        A=WorkActor(GetWorld(),RenderPosition(Position));Visuals.Add(Key,A);
        Part(A,TEXT("Cube"),FVector(0,0,5),FVector(1.9,1.5,.10),ToolSteel);
        if(Bulk)
        {
            FLinearColor RockTint=Resource.Color*.58f+FLinearColor(.065f,.065f,.065f,0);RockTint.A=1;
            auto* RoughMaterial=Cast<UMaterialInstanceDynamic>(Material(RockTint));
            if(RoughMaterial){RoughMaterial->SetScalarParameterValue(TEXT("Roughness"),.96f);RoughMaterial->SetScalarParameterValue(TEXT("Metallic"),0);}
            for(int32 I=0;I<9;++I)
            {
                const double X=(I%3-1)*45+FMath::Sin(I*5.1)*9,Y=(I/3-1)*33+FMath::Cos(I*3.4)*8;
                const FString Path=NatureAssets.FindRef(I%2?TEXT("RockA"):TEXT("RockB"));
                auto* Rock=Path.IsEmpty()?nullptr:LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn);
                if(!Rock){Part(A,TEXT("Cone"),FVector(X,Y,28),FVector(.74,.59,.49),RockTint,FRotator(I*13,I*67,I*9));continue;}
                auto* Piece=NewObject<UStaticMeshComponent>(A);Piece->SetMobility(EComponentMobility::Movable);Piece->SetStaticMesh(Rock);
                Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);Piece->SetupAttachment(A->GetRootComponent());
                const double Unit=72./FMath::Max(Rock->GetBoundingBox().GetSize().GetMax(),1.);
                const FVector Scale=FVector(.8+.12*(I%3),.73+.1*((I+1)%3),.65+.12*((I+2)%3))*Unit;
                const FRotator Rotation(I*13,I*67,I*9);
                const FBox Bounds=Rock->GetBoundingBox().TransformBy(FTransform(Rotation,FVector::ZeroVector,Scale));
                Piece->SetRelativeTransform(FTransform(Rotation,FVector(X-Bounds.GetCenter().X,Y-Bounds.GetCenter().Y,10-Bounds.Min.Z+(I==4?22:0)),Scale));
                for(int32 Slot=0;Slot<Piece->GetNumMaterials();++Slot)Piece->SetMaterial(Slot,RoughMaterial);
                Piece->RegisterComponent();A->AddInstanceComponent(Piece);
            }
        }
        else for(int32 I=0;I<8;++I)
        {
            const double X=(I%2?1:-1)*42,Y=((I/2)%2?1:-1)*31,Z=Ingots?20+(I/4)*22:29+(I/4)*48;
            Part(A,TEXT("Cube"),FVector(X,Y,Z),Ingots?FVector(.75,.49,.18):FVector(.65,.51,.43),Resource.Color);
            if(!Ingots)Part(A,TEXT("Cube"),FVector(X,Y-26,Z),FVector(.12,.015,.43),ToolSteel);
        }
    }
    A->SetActorLocation(RenderPosition(Position));A->SetActorHiddenInGame(false);
    // A continuous proportional illustration of this one real inventory amount,
    // not additional simulation stock or a fixed decorative pile at zero stock.
    const double Fill=FMath::Clamp(Amount/24.,.025,1.);
    A->SetActorScale3D(FVector(1,1,Bulk?FMath::Sqrt(Fill):Fill));
}

void ASeigeGameMode::SyncInventoryVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live)
{
    if(Building.IsConstructing||Definition.InventoryPresentation!=TEXT("outdoor"))return;
    TArray<FString> ResourceIds;Colony.Resources.GetKeys(ResourceIds);ResourceIds.Sort();
    TArray<FSeigeWorksiteBounds> Occupied=KnownBuildingBounds(*this,Colony,Building.Id);
    const double Clearance=110./FMath::Max(RenderScale,1.f);int32 Slot=0;
    for(const FString& Id:ResourceIds)
    {
        const double Amount=Building.Inventory.FindRef(Id);if(Amount<=UE_DOUBLE_SMALL_NUMBER)continue;
        FVector2D P;
        if(!SeigeFindExteriorPosition(WorldPosition,Definition.Footprint,Slot++,Clearance,Occupied,P))continue;
        if(!Observer&&DetailedSectorIndex()!=4&&!IsWorldVisible(P))continue;
        Occupied.Add({P,Clearance});
        SyncStockpile(Colony.Resources[Id],Amount,P,Key+TEXT("_inventory_")+Id,Live);
    }
}

void ASeigeGameMode::SyncWorkerVisuals(const FSeigeSimulation&,const FSeigeBuilding&,const FSeigeBuildingDef&,FVector2D,const FString&,TSet<FString>&) {}

void ASeigeGameMode::SyncWorkerAgents(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live)
{
    const auto* Snapshot=PresentationSnapshot(Colony);const double Alpha=PresentationAlpha(),Time=RenderSimulationTime(Colony);
    for(const auto& W:Colony.Workers.Bodies)
    {
        if(W.State!=TEXT("active")||!W.Outdoor)continue;
        const FVector2D P=(Snapshot?Snapshot->Worker(W,Alpha):W.Position)+Offset;
        if(!Observer&&!Offset.IsNearlyZero()&&(!IsWorldVisible(P)||!IsWorldVisible(W.Position+Offset)))continue;
        const FString Key=Prefix+TEXT("worker_")+W.Id;Live.Add(Key);
        auto* Actor=Visual(Key,TEXT("Robot"),RenderPosition(P,30),WorkAmber,95);
        Actor->SetActorRotation(FVector(W.Heading,0).Rotation());
        const auto* C=Colony.Couriers.FindByPredicate([&](const auto& Delivery){return Delivery.Id==W.DeliveryId;});
        if(C&&C->Amount>0)
        {
            const FString CargoKey=Key+TEXT("_cargo");Live.Add(CargoKey);auto* Cargo=Visuals.FindRef(CargoKey).Get();
            if(!Cargo){Cargo=WorkActor(GetWorld(),FVector::ZeroVector);Visuals.Add(CargoKey,Cargo);Part(Cargo,TEXT("Cube"),FVector::ZeroVector,FVector(.54,.47,.40),Colony.Resources[C->Resource].Color);}
            Cargo->SetActorHiddenInGame(false);Cargo->SetActorRotation(Actor->GetActorRotation());Cargo->SetActorLocation(Actor->GetActorLocation()+Actor->GetActorForwardVector()*48+FVector(0,0,24));
            const double Fraction=C->Amount/FMath::Max(Colony.Workers.HaulUnits(Colony,C->Resource),1.e-8);Cargo->SetActorScale3D(FVector(1,1,FMath::Clamp(Fraction,.08,1.)));
        }
        const auto* Road=Colony.FindRoad(Colony.Workers.RoadId(Colony,W));const auto* Tier=Road?Colony.TransportTiers.Find(Road->Tier):nullptr;
        if(Tier&&Tier->SpeedMultiplier>1&&Colony.Energy.RoadPowered(Road->Id))
        {
            const FString CarrierKey=Key+TEXT("_carrier");Live.Add(CarrierKey);auto* Carrier=Visuals.FindRef(CarrierKey).Get();
            if(!Carrier){Carrier=WorkActor(GetWorld(),FVector::ZeroVector);Visuals.Add(CarrierKey,Carrier);Part(Carrier,TEXT("Cube"),FVector(0,0,8),FVector(1.25,.92,.16),ToolSteel);}
            Carrier->SetActorHiddenInGame(false);Carrier->SetActorLocation(RenderPosition(P,5));Carrier->SetActorRotation(Actor->GetActorRotation());
        }
        const auto* B=Colony.FindBuilding(W.BuildingId);const bool Working=W.Activity==TEXT("build")||W.Activity==TEXT("road_build")||(W.Activity==TEXT("operate")&&B&&Colony.HasActiveWork(*B));
        if(Working)
        {
            const FString ToolKey=Key+TEXT("_tool");Live.Add(ToolKey);auto* Tool=Visuals.FindRef(ToolKey).Get();
            if(!Tool){Tool=WorkActor(GetWorld(),FVector::ZeroVector);Visuals.Add(ToolKey,Tool);Part(Tool,TEXT("Cube"),FVector::ZeroVector,FVector(.36,.10,.10),ToolSteel);}
            Tool->SetActorHiddenInGame(false);Tool->SetActorLocation(Actor->GetActorLocation()+Actor->GetActorForwardVector()*52+FVector(0,0,38));Tool->SetActorRotation(Actor->GetActorRotation()+FRotator(FMath::Sin(Time*6)*12,0,0));
        }
    }
}
