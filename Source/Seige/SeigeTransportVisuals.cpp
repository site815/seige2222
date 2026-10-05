#include "SeigeGameMode.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace
{
const FLinearColor Asphalt(.075f,.085f,.085f),Rail(.30f,.34f,.36f),Tube(.60f,.69f,.70f),Survey(.58f,.72f,.65f);
AActor* RoadActor(UWorld* World)
{
    auto* A=World->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(A);
    A->SetRootComponent(Root);Root->RegisterComponent();return A;
}
struct FSurface
{
    TArray<FVector> V,N;TArray<int32> I;TArray<FVector2D> UV;
    void Quad(FVector A,FVector B,FVector C,FVector D)
    {
        const int32 First=V.Num();const FVector Normal=FVector::CrossProduct(B-A,C-A).GetSafeNormal();
        V.Append({A,B,C,D});N.Append({Normal,Normal,Normal,Normal});
        UV.Append({FVector2D(0,0),FVector2D(0,1),FVector2D(1,1),FVector2D(1,0)});
        I.Append({First,First+1,First+2,First,First+2,First+3});
    }
    void Beam(FVector A,FVector B,double Width,double Height)
    {
        FVector Direction=FVector::CrossProduct((B-A).GetSafeNormal(),FVector::UpVector).GetSafeNormal();
        if(Direction.IsNearlyZero())Direction=FVector::RightVector;
        const FVector Side=Direction*Width*.5;
        const FVector Up=(FMath::Abs((B-A).GetSafeNormal().Z)>.95?FVector::ForwardVector:FVector::UpVector)*Height;
        Quad(A-Side,B-Side,B+Side,A+Side);Quad(A-Side+Up,A+Side+Up,B+Side+Up,B-Side+Up);
        Quad(A-Side,A-Side+Up,B-Side+Up,B-Side);Quad(B+Side,B+Side+Up,A+Side+Up,A+Side);
        Quad(A+Side,A+Side+Up,A-Side+Up,A-Side);Quad(B-Side,B-Side+Up,B+Side+Up,B+Side);
    }
    void Pipe(FVector A,FVector B,double Radius)
    {
        const FVector Along=(B-A).GetSafeNormal(),Side=FVector::CrossProduct(Along,FVector::UpVector).GetSafeNormal(),Up=FVector::CrossProduct(Side,Along);
        for(int32 K=0;K<12;++K)
        {
            const double P=K*2*PI/12,Q=(K+1)*2*PI/12;
            const FVector X=(Side*FMath::Cos(P)+Up*FMath::Sin(P))*Radius,Y=(Side*FMath::Cos(Q)+Up*FMath::Sin(Q))*Radius;
            Quad(A+X,B+X,B+Y,A+Y);
        }
    }
    void Update(UProceduralMeshComponent* Mesh,UMaterialInterface* Material) const
    {
        const auto* Section=Mesh->GetProcMeshSection(0);
        // Every surface uses the same sequential quad topology. Moving a plot
        // or a same-length road preview only changes vertices and normals.
        if(Section&&Section->ProcVertexBuffer.Num()==V.Num()&&Section->ProcIndexBuffer.Num()==I.Num())
            Mesh->UpdateMeshSection_LinearColor(0,V,N,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>());
        else Mesh->CreateMeshSection_LinearColor(0,V,I,N,UV,TArray<FLinearColor>(),TArray<FProcMeshTangent>(),false);
        if(Mesh->GetMaterial(0)!=Material)Mesh->SetMaterial(0,Material);
    }
    UProceduralMeshComponent* Install(AActor* A,UMaterialInterface* Material) const
    {
        if(V.IsEmpty())return nullptr;
        auto* Mesh=NewObject<UProceduralMeshComponent>(A);Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetupAttachment(A->GetRootComponent());Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->RegisterComponent();A->AddInstanceComponent(Mesh);
        Update(Mesh,Material);return Mesh;
    }
};
}

FVector2D ASeigeGameMode::SnapRoadCursor(FVector2D Position) const{return Sim.SnapRoadPoint(Position);}
void ASeigeGameMode::CancelRoadTool(){RoadPlacementActive=RoadUpgradeActive=RoadHasStart=false;RoadStart=FVector2D::ZeroVector;}
void ASeigeGameMode::BeginRoadPlacement()
{
    if(Observer||Screen!=TEXT("playing")||DetailedSectorIndex()!=4||IsRegionMap()||Sim.Escaped||Sim.Failed)return;
    CancelWallTool();CancelRoadTool();SelectedBuild.Empty();SelectedId=SelectedRoadId=0;RoadPlacementActive=true;
    Notice=TEXT("Road: select a building edge port or road endpoint, then the destination. Esc cancels.");
}
void ASeigeGameMode::BeginRoadUpgrade()
{
    if(Observer||Screen!=TEXT("playing")||DetailedSectorIndex()!=4||IsRegionMap()||Sim.Escaped||Sim.Failed)return;
    CancelWallTool();CancelRoadTool();SelectedBuild.Empty();SelectedId=0;
    if(SelectedRoadId>0)
    {
        if(Sim.UpgradeRoad(SelectedRoadId,Error)){RefreshTransportScenery();Notice=TEXT("Transport upgrade queued. The existing route remains operational during construction.");}
        else Notice=Error;
        return;
    }
    RoadUpgradeActive=true;Notice=TEXT("Select a completed road to add rails, or a rail corridor to add vacuum tubes.");
}

void ASeigeGameMode::SyncRoadVisuals(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live)
{
    for(const auto& Road:Colony.Roads)
    {
        if(Road.Health<=0)continue;
        // Never disclose a hidden neighbor's route through its visible endpoint.
        if(!Observer&&!Offset.IsNearlyZero()&&(!IsWorldVisible(Road.A+Offset)||!IsWorldVisible(Road.B+Offset)))continue;
        const auto* Current=Colony.TransportTiers.Find(Road.Tier);
        const auto* Target=Colony.TransportTiers.Find(Road.TargetTier);
        const auto* Definition=Target?Target:Current;if(!Definition)continue;
        const double Length=FVector2D::Distance(Road.A,Road.B)*RenderScale;
        const double Width=Definition->WidthMeters*100.;
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(Length/800.));
        bool Revealed=true;
        if(!Observer&&!Offset.IsNearlyZero())for(int32 I=0;I<=Steps;++I)if(!IsWorldVisible(FMath::Lerp(Road.A,Road.B,double(I)/Steps)+Offset)){Revealed=false;break;}
        if(!Revealed)continue;
        const FString Key=Prefix+FString::Printf(TEXT("road_%d"),Road.Id);Live.Add(Key);
        const FName Signature(*FString::Printf(TEXT("%s_%s_%d_%d_%u"),*Road.Tier,*Road.TargetTier,FMath::FloorToInt(Road.ConstructionProgress*50),SelectedRoadId==Road.Id&&DetailedSectorIndex()==4,GetTypeHash(TerrainPadSignature)));
        auto* Actor=Visuals.FindRef(Key).Get();
        if(Actor&&!Actor->ActorHasTag(Signature)){Actor->Destroy();Visuals.Remove(Key);Actor=nullptr;}
        if(!Actor)
        {
            Actor=RoadActor(GetWorld());Actor->Tags.Add(Signature);Visuals.Add(Key,Actor);
            FSurface Earth,Paving,Rails,Tubes,Markings;
            const FVector2D Along=(Road.B-Road.A).GetSafeNormal(),Side(-Along.Y,Along.X);
            auto Position=[&](double T,double Across,double Z)
            {
                const FVector2D P=FMath::Lerp(Road.A,Road.B,T)+Offset+Side*(Across/RenderScale);
                return RenderPosition(P,Z);
            };
            const double TargetProgress=Road.IsConstructing?Road.ConstructionProgress:1.;
            for(int32 I=0;I<Steps;++I)
            {
                const double T=double(I)/Steps,U=double(I+1)/Steps;
                Earth.Quad(Position(T,-Width*.55,3),Position(U,-Width*.55,3),Position(U,Width*.55,3),Position(T,Width*.55,3));
                const double PavedWidth=Target&&U<=TargetProgress?Width:Current?Current->WidthMeters*100.:0.;
                if(PavedWidth>0)
                    Paving.Quad(Position(T,-PavedWidth*.5,5),Position(U,-PavedWidth*.5,5),Position(U,PavedWidth*.5,5),Position(T,PavedWidth*.5,5));
                const auto* VisibleTier=Target&&U<=TargetProgress?Target:Current;
                if(VisibleTier&&(VisibleTier->Visual==TEXT("rail")||VisibleTier->Visual==TEXT("vacuum")))
                {
                    for(double S:{-.19,.19})Rails.Beam(Position(T,Width*S,11),Position(U,Width*S,11),6,7);
                    for(int32 J=0;J<4;++J){const double Tie=FMath::Lerp(T,U,(J+.5)/4.);Rails.Beam(Position(Tie,-Width*.27,6),Position(Tie,Width*.27,6),18,5);}
                }
                if(VisibleTier&&VisibleTier->Visual==TEXT("vacuum"))
                {
                    Tubes.Pipe(Position(T,Width*.36,130),Position(U,Width*.36,130),43);
                    Tubes.Beam(Position(T,Width*.36,6),Position(T,Width*.36,125),18,8);
                }
                if(SelectedRoadId==Road.Id&&DetailedSectorIndex()==4)
                    for(double S:{-.52,.52})Markings.Beam(Position(T,Width*S,7),Position(U,Width*S,7),5,2);
            }
            Earth.Install(Actor,Material(FLinearColor(.19f,.16f,.11f)));Paving.Install(Actor,Material(Asphalt));
            Rails.Install(Actor,Material(Rail));Tubes.Install(Actor,Material(Tube));Markings.Install(Actor,Material(Survey));
        }
        Actor->SetActorHiddenInGame(false);
        if(!Road.IsConstructing)continue;
        const FVector2D Access=Colony.RoadAccessPoint(Road)+Offset;
        const FVector2D Direction=(Road.B-Road.A).GetSafeNormal(),Side(-Direction.Y,Direction.X);
        int32 Slot=0;
        for(const auto& Resource:Colony.Resources)
        {
            const double Amount=Road.ConstructionMaterials.FindRef(Resource.Key);if(Amount<=0)continue;
            const FVector2D P=Access+Side*(Definition->WidthMeters*.5/Colony.MetersPerWorldUnit()+35)+Direction*(Slot++*40);
            if(!Observer&&!Offset.IsNearlyZero()&&!IsWorldVisible(P))continue;
            SyncStockpile(Resource.Value,Amount,P,Key+TEXT("_stock_")+Resource.Key,Live);
        }
        const auto* Snapshot=PresentationSnapshot(Colony);
        const FVector2D Crew=Snapshot?Snapshot->RoadBuilder(Road,PresentationAlpha()):Road.BuilderPosition;
        const auto* Command=Colony.Buildings.FindByPredicate([&](const FSeigeBuilding& B){return B.DefId==Colony.CoreDefinition;});
        const FVector2D Awaiting=Command?Colony.BuildingAccessPoint(*Command):Crew;
        for(int32 I=0;I<FMath::Min(Road.Builders,3);++I)
        {
            // The crew's route and arrival are simulated. Its site tools animate
            // at render frequency without moving workers faster than walking.
            const bool OnSite=I<Road.BuildersOnSite;
            const bool Travelling=!OnSite&&I<Road.BuildersOnSite+Road.TravellingBuilders;
            const FVector2D P=(OnSite?Colony.RoadAccessPoint(Road):Travelling?Crew:Awaiting)+Offset+Side*(I*20.);
            if(!Observer&&!Offset.IsNearlyZero()&&!IsWorldVisible(P))continue;
            const FString WorkerKey=Key+FString::Printf(TEXT("_builder_%d"),I);Live.Add(WorkerKey);
            auto* Worker=Visual(WorkerKey,TEXT("Robot"),RenderPosition(P,4),Survey,95);
            Worker->SetActorRotation(FVector(Road.B-Road.A,0).Rotation());
            if(!Worker->ActorHasTag(TEXT("PavingTool")))
            {Part(Worker,TEXT("Cube"),FVector(40,0,30),FVector(.5,.22,.12),Rail);Worker->Tags.Add(TEXT("PavingTool"));}
        }
    }
}

void ASeigeGameMode::SyncRoadPlacementGhost(TSet<FString>& Live)
{
    if(!RoadPlacementActive||!CursorOnWorld||Screen!=TEXT("playing")||Observer||DetailedSectorIndex()!=4||IsRegionMap())return;
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(auto* HUD=Cast<ASeigeHUD>(PC->GetHUD()))if(HUD->IsPointerOverUI())return;
    const FVector2D End=SnapRoadCursor(CursorWorld),Start=RoadHasStart?RoadStart:End-FVector2D(15,0);
    FString Reason;const bool Valid=!RoadHasStart||Sim.CanPlaceRoad(Start,End,Reason);
    const auto* Tier=Sim.TransportTiers.Find(TEXT("road"));if(!Tier)return;
    const FString Key=TEXT("road_placement");Live.Add(Key);
    const FString Signature=FString::Printf(TEXT("%.3f_%.3f_%.3f_%.3f_%.3f_%d_%u"),Start.X,Start.Y,End.X,End.Y,Tier->WidthMeters,Valid,GetTypeHash(TerrainPadSignature));
    auto* A=Visuals.FindRef(Key).Get();const bool Update=!A||RoadGhostSignature!=Signature;
    if(!A){A=RoadActor(GetWorld());Visuals.Add(Key,A);}
    if(Update)
    {
        FSurface Surface;
        const FVector2D Delta=End-Start,Side=FVector2D(-Delta.Y,Delta.X).GetSafeNormal()*Tier->WidthMeters*.5/Sim.MetersPerWorldUnit();
        const int32 Steps=FMath::Max(1,FMath::CeilToInt(Delta.Size()*RenderScale/800));
        for(int32 I=0;I<Steps;++I)
        {
            const FVector2D P=FMath::Lerp(Start,End,double(I)/Steps),Q=FMath::Lerp(Start,End,double(I+1)/Steps);
            Surface.Quad(RenderPosition(P-Side,12),RenderPosition(Q-Side,12),RenderPosition(Q+Side,12),RenderPosition(P+Side,12));
        }
        auto* Material=ConstructionMaterial(Valid?FLinearColor(.2f,.9f,.6f):FLinearColor(.9f,.12f,.06f));
        if(auto* ExistingMesh=A->FindComponentByClass<UProceduralMeshComponent>())Surface.Update(ExistingMesh,Material);
        else if(auto* CreatedMesh=Surface.Install(A,Material))CreatedMesh->SetCastShadow(false);
        RoadGhostSignature=Signature;
    }
    A->SetActorHiddenInGame(false);
}

void ASeigeGameMode::SyncBuildingPlot(const FSeigeSimulation& Colony,const FSeigeBuilding& Building,const FSeigeBuildingDef& Definition,FVector2D Position,const FString& Key,TSet<FString>& Live)
{
    const bool Selected=SelectedId==Building.Id&&ViewedSimulation()==&Colony;
    if(!Building.IsConstructing&&!Selected&&!RoadPlacementActive)return;
    const FString PlotKey=Key+TEXT("_plot");Live.Add(PlotKey);
    auto* Plot=Visuals.FindRef(PlotKey).Get();
    const FVector Origin=RenderPosition(Position);
    const bool Preview=PlotKey.StartsWith(TEXT("placement_"));
    const FString Signature=Preview?FString::Printf(TEXT("%s_%.3f_%.3f_%.3f_%.3f_%.3f_%u"),*Key,Position.X,Position.Y,Definition.ReservedFootprint,Definition.AccessPort.X,Definition.AccessPort.Y,GetTypeHash(TerrainPadSignature)):FString();
    const bool Update=!Plot||(Preview&&BuildingPlotGhostSignature!=Signature)||!Plot->GetActorLocation().Equals(Origin,.01);
    if(!Plot){Plot=RoadActor(GetWorld());Visuals.Add(PlotKey,Plot);}
    if(Update)
    {
        FSurface Marks;
        const double R=Definition.ReservedFootprint,Arm=FMath::Min(45.,R*.12);
        for(double X:{-1.,1.})for(double Y:{-1.,1.})
        {
            const FVector2D Corner=Position+FVector2D(X*R,Y*R);
            Marks.Beam(RenderPosition(Corner,8),RenderPosition(Corner-FVector2D(X*Arm,0),8),8,2);
            Marks.Beam(RenderPosition(Corner,8),RenderPosition(Corner-FVector2D(0,Y*Arm),8),8,2);
        }
        const FVector2D Port=Colony.BuildingAccessPoint(Building)+Position-Building.Position;
        const FVector2D Across(-Definition.AccessPort.Y,Definition.AccessPort.X);
        Marks.Beam(RenderPosition(Port-Across*25.,10),RenderPosition(Port+Across*25.,10),14,3);
        for(auto& Vertex:Marks.V)Vertex-=Origin;
        Plot->SetActorLocation(Origin);
        if(auto* ExistingMesh=Plot->FindComponentByClass<UProceduralMeshComponent>())Marks.Update(ExistingMesh,Material(Survey));
        else if(auto* CreatedMesh=Marks.Install(Plot,Material(Survey)))if(Preview)CreatedMesh->SetCastShadow(false);
        if(Preview)BuildingPlotGhostSignature=Signature;
    }
    Plot->SetActorHiddenInGame(false);
}
