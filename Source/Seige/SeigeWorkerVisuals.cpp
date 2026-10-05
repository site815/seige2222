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

void ASeigeGameMode::SyncWorkerVisuals(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D WorldPosition,const FString& Key,TSet<FString>& Live)
{
    if(Building.IsConstructing||!Building.Enabled||Building.Workers<=0)return;
    const int32 Count=FMath::Min(Building.Workers,3);const double Time=RenderSimulationTime(Colony);
    // Read the same inputs, space, staffing and support gates as the simulation;
    // diagnostic status strings are not a second production-state machine.
    const bool Active=Colony.HasActiveWork(Building);
    const bool Extraction=Definition.WorkerActivity==TEXT("extraction");
    TArray<FSeigeWorksiteBounds> Occupied=KnownBuildingBounds(*this,Colony,Building.Id);
    const double Clearance=110./FMath::Max(RenderScale,1.f);
    for(const auto& Resource:Colony.Resources)
    {
        const FString PileKey=Key+TEXT("_inventory_")+Resource.Key;
        if(Live.Contains(PileKey))if(const auto* Pile=Visuals.FindRef(PileKey).Get())Occupied.Add({FVector2D(Pile->GetActorLocation())/RenderScale,Clearance});
    }
    for(int32 I=0;I<Count;++I)
    {
        FVector2D Station;
        if(!SeigeFindExteriorPosition(WorldPosition,Definition.Footprint,5+I,Clearance,Occupied,Station))continue;
        FVector2D Tools=Station+(Station-WorldPosition).GetSafeNormal()*30.;
        for(const auto& Area:Occupied)
        {
            const FVector2D Low(FMath::Min(Station.X,Tools.X)-Clearance,FMath::Min(Station.Y,Tools.Y)-Clearance);
            const FVector2D High(FMath::Max(Station.X,Tools.X)+Clearance,FMath::Max(Station.Y,Tools.Y)+Clearance);
            if(Low.X<Area.Position.X+Area.HalfWidth&&High.X>Area.Position.X-Area.HalfWidth&&Low.Y<Area.Position.Y+Area.HalfWidth&&High.Y>Area.Position.Y-Area.HalfWidth){Tools=Station;break;}
        }
        Occupied.Add({Station,Clearance});
        const double Trip=FVector2D::Distance(Station,Tools)/FMath::Max(Colony.WalkingSpeed(),.001);
        const double Cycle=Trip*2+8.;
        const double Phase=FMath::Fmod(Time+I*2.7+Building.Id*.31,Cycle);
        double Travel=0;
        if(Active&&Trip>0&&Phase<Trip)Travel=Phase/Trip;
        else if(Active&&Phase<Trip+2)Travel=1;
        else if(Active&&Trip>0&&Phase<2*Trip+2)Travel=1-(Phase-Trip-2)/Trip;
        const FVector2D Position=FMath::Lerp(Station,Tools,Travel);
        if(!Observer&&DetailedSectorIndex()!=4&&!IsWorldVisible(Position))continue;
        const FString WorkerKey=Key+FString::Printf(TEXT("_operator_%d"),I);Live.Add(WorkerKey);
        auto* Worker=Visual(WorkerKey,TEXT("Robot"),RenderPosition(Position,3),WorkAmber,95);
        const FVector2D Facing=Travel>.01?(Phase<Trip+2?Tools-Station:Station-Tools):WorldPosition-Position;
        Worker->SetActorRotation(FVector(Facing,0).Rotation());
        if(!Worker->ActorHasTag(TEXT("OperatorTool")))
        {
            Part(Worker,TEXT("Cube"),FVector(40,0,42),Extraction?FVector(.42,.1,.1):FVector(.29,.22,.24),ToolSteel);
            TArray<UStaticMeshComponent*> Parts;Worker->GetComponents(Parts);Parts.Last()->ComponentTags.Add(TEXT("OperatorTool"));
            Worker->Tags.Add(TEXT("OperatorTool"));
        }
        TArray<UStaticMeshComponent*> Parts;Worker->GetComponents(Parts);
        for(auto* Part:Parts)if(Part->ComponentHasTag(TEXT("OperatorTool")))
            Part->SetRelativeRotation(FRotator(Active&&Phase>=2*Trip+2?FMath::Sin(Time*(Extraction?7:3)+I)*18:0,0,0));
        // Tool stations represent the existing assigned workers and their tools;
        // material transfer remains exclusively the simulation's cargo couriers.
        const FString BenchKey=Key+FString::Printf(TEXT("_workbench_%d"),I);Live.Add(BenchKey);
        auto* Bench=Visuals.FindRef(BenchKey).Get();
        if(!Bench)
        {
            Bench=WorkActor(GetWorld(),RenderPosition(Station));Visuals.Add(BenchKey,Bench);
            Part(Bench,TEXT("Cube"),FVector(55,0,45),FVector(.55,.75,.08),ToolSteel);
            Part(Bench,TEXT("Cube"),FVector(55,0,23),FVector(.08,.45,.46),ToolSteel);
        }
        Bench->SetActorLocation(RenderPosition(Station));Bench->SetActorHiddenInGame(false);
    }
}
