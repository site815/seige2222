#include "SeigeGameMode.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"

namespace
{
struct FHeightPad
{
    FVector2D Position;
    double Height=0,Inner=0,Outer=0;
};
struct FPreparedTerrain
{
    const ASeigeGameMode& Game;
    TArray<FHeightPad> Pads[9];
    TMap<FString,FVector4> Bounds;
    FString Signature;
    double RidgeCos=0,RidgeSin=0;
    explicit FPreparedTerrain(const ASeigeGameMode& G):Game(G)
    {
        const double Angle=FMath::DegreesToRadians(double(G.RidgeAngleDegrees));
        RidgeCos=FMath::Cos(Angle);RidgeSin=FMath::Sin(Angle);
        auto AddColony=[&](const FSeigeSimulation& Colony,FVector2D Offset,int32 Index)
        {
            if(Index<0||Index>8||(Index==4&&G.Screen==TEXT("landing")))return;
            for(const auto& B:Colony.Buildings)
            {
                const FVector2D P=B.Position+Offset;
                if(B.Health<=0||(!G.Observer&&Index!=4&&!G.Sim.IsVisible(P)))continue;
                if(const auto* D=Colony.Definition(B))
                {
                    const double GridStep=G.Sim.WorldHalfSize*2/G.DetailedTerrainResolution;
                    // Grid vertices bordering an off-grid foundation must also be
                    // flat, otherwise a triangle cuts through its outer corners.
                    const double Inner=FMath::Max(double(D->Footprint*G.CorePadInnerRatio),D->Footprint+GridStep);
                    const double Outer=FMath::Max(double(D->Footprint*G.CorePadOuterRatio),Inner+GridStep);
                    const FHeightPad Pad{P,Natural(P),Inner,Outer};
                    const FString Key=FString::Printf(TEXT("%d:%d"),Index,B.Id);
                    Bounds.Add(Key,FVector4(P.X,P.Y,Inner,Outer));
                    Signature+=FString::Printf(TEXT("%s:%.4f:%.4f:%.4f:%.4f;"),*Key,P.X,P.Y,Inner,Outer);
                    const double Span=G.Sim.WorldHalfSize*2;
                    const int32 MinX=FMath::Clamp(FMath::FloorToInt((P.X-Pad.Outer+Span*.5)/Span),-1,1),MaxX=FMath::Clamp(FMath::FloorToInt((P.X+Pad.Outer+Span*.5)/Span),-1,1);
                    const int32 MinY=FMath::Clamp(FMath::FloorToInt((P.Y-Pad.Outer+Span*.5)/Span),-1,1),MaxY=FMath::Clamp(FMath::FloorToInt((P.Y+Pad.Outer+Span*.5)/Span),-1,1);
                    // A border-side foundation has the same influence from either
                    // adjacent tile, so changing detail level cannot open a seam.
                    for(int32 Y=MinY;Y<=MaxY;++Y)for(int32 X=MinX;X<=MaxX;++X)Pads[(Y+1)*3+X+1].Add(Pad);
                }
            }
        };
        AddColony(G.Sim,FVector2D::ZeroVector,4);
        for(const auto& N:G.Neighbors)AddColony(N.Sim,N.Offset,N.Index);
    }
    double Natural(FVector2D P) const
    {
        const FVector2D Delta=P-Game.RidgeCenter;
        const double Along=(Delta.X*RidgeCos+Delta.Y*RidgeSin)/Game.RidgeLength;
        const double Across=(-Delta.X*RidgeSin+Delta.Y*RidgeCos)/Game.RidgeWidth;
        const double Ridge=Game.RidgeHeight*FMath::Exp(-Across*Across-Along*Along*Along*Along);
        return FMath::PerlinNoise2D(P/13000+FVector2D(17.8,-8.1))*1500+
            FMath::PerlinNoise2D(P/Game.RollingTerrainWavelength+FVector2D(-4.6,25.4))*Game.RollingTerrainAmplitude+
            FMath::PerlinNoise2D(P/Game.MicroTerrainWavelength+FVector2D(41.2,12.5))*Game.MicroTerrainAmplitude+Ridge;
    }
    double Height(FVector2D P) const
    {
        const double Base=Natural(P),Span=Game.Sim.WorldHalfSize*2;
        const int32 X=FMath::FloorToInt((P.X+Span*.5)/Span),Y=FMath::FloorToInt((P.Y+Span*.5)/Span);
        if(X<-1||X>1||Y<-1||Y>1)return Base;
        double WeightedHeight=0,TotalWeight=0,Influence=0;
        for(const auto& Pad:Pads[(Y+1)*3+X+1])
        {
            // A square foundation needs level corners as well as its center.
            // Smooth compact falloffs keep the surrounding meadow continuous.
            const FVector2D D=P-Pad.Position;
            const double Distance=FMath::Max(FMath::Abs(D.X),FMath::Abs(D.Y));
            if(Distance>=Pad.Outer)continue;
            const double Weight=1-FMath::SmoothStep(Pad.Inner,Pad.Outer,Distance);
            const double BlendWeight=Weight/FMath::Max(.000001,1-Weight);
            WeightedHeight+=Pad.Height*BlendWeight;TotalWeight+=BlendWeight;Influence=FMath::Max(Influence,Weight);
        }
        return TotalWeight>0?FMath::Lerp(Base,WeightedHeight/TotalWeight,Influence):Base;
    }
};
double VertexHeight(const FPreparedTerrain& Prepared,const FSeigeTerrainTile& Tile,int32 X,int32 Y)
{
    const double Half=Prepared.Game.Sim.WorldHalfSize,Step=Half*2/Tile.Resolution,CoarseStep=Half*2/128;
    const FVector2D P=Tile.Offset+FVector2D(-Half+X*Step,-Half+Y*Step);
    // Shared edge vertices are anchored to the common coarse surface regardless
    // of which neighboring sector currently owns the detailed mesh.
    if(X==0||X==Tile.Resolution)
    {
        const double At=FMath::FloorToDouble(P.Y/CoarseStep)*CoarseStep;
        return FMath::Lerp(Prepared.Height(FVector2D(P.X,At)),Prepared.Height(FVector2D(P.X,At+CoarseStep)),(P.Y-At)/CoarseStep);
    }
    if(Y==0||Y==Tile.Resolution)
    {
        const double At=FMath::FloorToDouble(P.X/CoarseStep)*CoarseStep;
        return FMath::Lerp(Prepared.Height(FVector2D(At,P.Y)),Prepared.Height(FVector2D(At+CoarseStep,P.Y)),(P.X-At)/CoarseStep);
    }
    return Prepared.Height(P);
}
double SharedEdgeInfluence(const ASeigeGameMode& G,const FVector4& Area)
{
    const double Half=G.Sim.WorldHalfSize,Span=Half*2;
    const double LocalX=Area.X-FMath::FloorToDouble((Area.X+Half)/Span)*Span;
    const double LocalY=Area.Y-FMath::FloorToDouble((Area.Y+Half)/Span)*Span;
    // A changed coarse boundary vertex influences the whole adjacent edge
    // segment, even when most of that segment lies outside the analytical pad.
    return Half-FMath::Max(FMath::Abs(LocalX),FMath::Abs(LocalY))<=Area.W?Span/128:0;
}
}
double ASeigeGameMode::TerrainHeight(FVector2D P) const
{
    // Used only before the triangle cache exists or outside it. Mesh generation
    // prepares known building pads once for all vertices, with no hidden-colony data.
    return FPreparedTerrain(*this).Height(P);
}
double ASeigeGameMode::GroundHeight(FVector2D P) const
{
    const double Half=Sim.WorldHalfSize;
    for(const auto& Tile:TerrainTiles)
    {
        const FVector2D Local=P-Tile.Offset;
        if(FMath::Abs(Local.X)>Half||FMath::Abs(Local.Y)>Half||Tile.Heights.IsEmpty())continue;
        const double X=FMath::Clamp((Local.X+Half)/(Half*2)*Tile.Resolution,0.,double(Tile.Resolution));
        const double Y=FMath::Clamp((Local.Y+Half)/(Half*2)*Tile.Resolution,0.,double(Tile.Resolution));
        const int32 IX=FMath::Min(FMath::FloorToInt(X),Tile.Resolution-1),IY=FMath::Min(FMath::FloorToInt(Y),Tile.Resolution-1);
        const double FX=X-IX,FY=Y-IY;
        const int32 I=IY*(Tile.Resolution+1)+IX;
        const double A=Tile.Heights[I],B=Tile.Heights[I+1],C=Tile.Heights[I+Tile.Resolution+1],D=Tile.Heights[I+Tile.Resolution+2];
        return FX+FY<=1?A+(B-A)*FX+(C-A)*FY:D+(C-D)*(1-FX)+(B-D)*(1-FY);
    }
    return TerrainHeight(P);
}
void ASeigeGameMode::RebuildTerrainHeights()
{
    const double Started=FPlatformTime::Seconds();
    const double Half=Sim.WorldHalfSize;
    const FPreparedTerrain Prepared(*this);
    TerrainPadSignature=Prepared.Signature;TerrainPadBounds=Prepared.Bounds;
    TerrainTiles.Reset();
    for(int32 Y=-1;Y<=1;++Y)for(int32 X=-1;X<=1;++X)
    {
        FSeigeTerrainTile Tile;Tile.Offset=FVector2D(X,Y)*Half*2;Tile.Resolution=((Y+1)*3+X+1==DetailedSectorIndex())?DetailedTerrainResolution:128;
        Tile.Heights.Reserve((Tile.Resolution+1)*(Tile.Resolution+1));
        for(int32 V=0;V<=Tile.Resolution;++V)for(int32 U=0;U<=Tile.Resolution;++U)
            Tile.Heights.Add(VertexHeight(Prepared,Tile,U,V));
        TerrainTiles.Add(MoveTemp(Tile));
    }
    UE_LOG(LogTemp,Display,TEXT("Terrain height cache: detailed sector %d, %d subdivisions (%.2f m), %.3f seconds"),DetailedSectorIndex(),DetailedTerrainResolution,Half*2/DetailedTerrainResolution*RenderScale/100,FPlatformTime::Seconds()-Started);
}
namespace
{
constexpr int32 TerrainChunkCells=128;
struct FDirtPatch {FVector2D Position;double Inner,Outer,Strength;bool Square=false;double GradeOuter=0;};
double MeadowSwardDensity(FVector2D P)
{
    // Coherent sparse patches, shared by the soil mask and instanced sward.
    // Most meadow remains covered instead of receiving uniform bare speckles.
    const double Patch=FMath::PerlinNoise2D(P/130+FVector2D(31.7,-17.2));
    return FMath::Lerp(.25,1.,FMath::SmoothStep(-.6,-.05,Patch));
}
FQuat GroundCoverRotation(const ASeigeGameMode& G,FVector2D P,double Yaw)
{
    const double DX=G.GroundHeight(P+FVector2D(12,0))-G.GroundHeight(P-FVector2D(12,0));
    const double DY=G.GroundHeight(P+FVector2D(0,12))-G.GroundHeight(P-FVector2D(0,12));
    const FVector Normal=FVector(-DX,-DY,24).GetSafeNormal();
    return FRotationMatrix::MakeFromZX(Normal,FRotator(0,Yaw,0).Vector()).ToQuat();
}
TArray<FDirtPatch> PrepareDirt(const ASeigeGameMode& G)
{
    TArray<FDirtPatch> DirtPatches;
    const FVector2D Offset=G.DetailedSectorOffset();
    if(const auto* Colony=G.ViewedSimulation())
    {
        if(G.Screen!=TEXT("landing"))for(const auto& B:Colony->Buildings)
            if(B.Health>0&&(G.Observer||G.DetailedSectorIndex()==4||G.Sim.IsVisible(B.Position+Offset)))if(const auto* D=Colony->Definition(B))
            {
                const double Step=G.Sim.WorldHalfSize*2/G.DetailedTerrainResolution;
                const double PadInner=FMath::Max(double(D->Footprint*G.CorePadInnerRatio),D->Footprint+Step);
                const double PadOuter=FMath::Max(double(D->Footprint*G.CorePadOuterRatio),PadInner+Step);
                // Exposed soil follows the foundation edge, while the wider
                // graded bank remains a meadow rather than a square dirt lot.
                DirtPatches.Add({B.Position+Offset,D->Footprint*.72,D->Footprint+90,.65,true,PadOuter+Step});
            }
        for(const auto& N:Colony->Nodes)DirtPatches.Add({N.Position+Offset,110,210,.6});
    }
    return DirtPatches;
}
void BuildTerrainChunk(const ASeigeGameMode& G,const FSeigeTerrainTile& Tile,int32 StartX,int32 StartY,UProceduralMeshComponent* Terrain,const TArray<FDirtPatch>& DirtPatches)
{
        const int32 Resolution=Tile.Resolution,Cells=FMath::Min(TerrainChunkCells,Resolution);
        const FVector2D Offset=Tile.Offset;const bool Detailed=Offset.Equals(G.DetailedSectorOffset(),1.);
        const double Half=G.Sim.WorldHalfSize,Step=Half*2/Resolution;
        TArray<FVector> Vertices,Normals; TArray<int32> Triangles; TArray<FVector2D> UVs;
        TArray<FLinearColor> Colors; TArray<FProcMeshTangent> Tangents;
        const int32 Count=(Cells+1)*(Cells+1);
        Vertices.Reserve(Count);Normals.Reserve(Count);UVs.Reserve(Count);Colors.Reserve(Count);Tangents.Reserve(Count);Triangles.Reserve(Cells*Cells*6);
        for(int32 V=0;V<=Cells;V++)for(int32 U=0;U<=Cells;U++)
        {
            const int32 X=StartX+U,Y=StartY+V;
            const FVector2D P=Offset+FVector2D(-Half+X*Step,-Half+Y*Step);
            const int32 I=Y*(Resolution+1)+X;
            // Interior normals use the already sampled vertex grid. Shared-edge
            // normals sample the common triangle cache on either side of the seam.
            const double DX=X>0&&X<Resolution?(Tile.Heights[I+1]-Tile.Heights[I-1])/(Step*2):
                (G.GroundHeight(P+FVector2D(Step,0))-G.GroundHeight(P-FVector2D(Step,0)))/(Step*2);
            const double DY=Y>0&&Y<Resolution?(Tile.Heights[I+Resolution+1]-Tile.Heights[I-Resolution-1])/(Step*2):
                (G.GroundHeight(P+FVector2D(0,Step))-G.GroundHeight(P-FVector2D(0,Step)))/(Step*2);
            const FVector Normal=FVector(-DX,-DY,1).GetSafeNormal();
            Vertices.Add(FVector(P.X*G.RenderScale,P.Y*G.RenderScale,Tile.Heights[I]*G.RenderScale));Normals.Add(Normal);UVs.Add(P*G.RenderScale/700);
            const double Woodland=G.WoodlandDensity(P);
            double Dirt=FMath::Clamp((FMath::PerlinNoise2D(P/1250+FVector2D(14,3))-.28)*.7,0.,.22);
            Dirt=FMath::Max(Dirt,(1-MeadowSwardDensity(P))*.3*(1-Woodland));
            double FoundationGrade=0;
            if(Detailed)for(const auto& Patch:DirtPatches)
            {
                const FVector2D Delta=P-Patch.Position;
                const double Distance=Patch.Square?FMath::Max(FMath::Abs(Delta.X),FMath::Abs(Delta.Y)):Delta.Length();
                if(Patch.Square)FoundationGrade=FMath::Max(FoundationGrade,1-FMath::SmoothStep(Patch.GradeOuter-Step,Patch.GradeOuter,Distance));
                if(Distance<Patch.Outer)Dirt=FMath::Max(Dirt,Patch.Strength*(1-FMath::SmoothStep(Patch.Inner,Patch.Outer,Distance)));
            }
            const double Noise=FMath::PerlinNoise2D(P/1700+FVector2D(32,15));
            // Meadow slopes retain soil. Exposed rock belongs on steep faces or
            // localized outcrops, not every gently rolling hill.
            const double Rock=FMath::Clamp((.82-Normal.Z)*5+FMath::Max(0.,Noise-.42)*.45,0.,.85)*(1-FMath::Max(Dirt,FoundationGrade));
            Colors.Add(FLinearColor(float(Dirt*(1-Rock)),float(Rock),float(Woodland),1));
            Tangents.Add(FProcMeshTangent(FVector(1,0,DX).GetSafeNormal(),false));
            if(U<Cells&&V<Cells)
            {
                const int32 J=V*(Cells+1)+U;
                Triangles.Append({J,J+Cells+1,J+1,J+1,J+Cells+1,J+Cells+2});
            }
        }
        Terrain->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UVs,Colors,Tangents,false);
}
}
void ASeigeGameMode::CreateLandscape()
{
    if(!FApp::CanEverRender())return;
    const double Started=FPlatformTime::Seconds();
    if(Landscape)Landscape->Destroy();
    auto* Ground=GetWorld()->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(Ground);Ground->SetRootComponent(Root);Root->RegisterComponent();Landscape=Ground;
    RenderedSector=DetailedSectorIndex();RebuildTerrainHeights();
    const auto DirtPatches=PrepareDirt(*this);
    auto* TerrainMaterial=LoadObject<UMaterialInterface>(nullptr,*TerrainMaterialPath,nullptr,LOAD_NoWarn);
    for(int32 TileIndex=0;TileIndex<TerrainTiles.Num();++TileIndex)
    {
        const auto& Tile=TerrainTiles[TileIndex];
        for(int32 Y=0;Y<Tile.Resolution;Y+=TerrainChunkCells)for(int32 X=0;X<Tile.Resolution;X+=TerrainChunkCells)
        {
            auto* Terrain=NewObject<UProceduralMeshComponent>(Ground);
            Terrain->SetupAttachment(Root);Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Terrain->ComponentTags={FName(*FString::FromInt(TileIndex)),FName(*FString::FromInt(X)),FName(*FString::FromInt(Y))};
            Terrain->RegisterComponent();Ground->AddInstanceComponent(Terrain);
            BuildTerrainChunk(*this,Tile,X,Y,Terrain,DirtPatches);
            Terrain->SetMaterial(0,TileIndex==RenderedSector&&TerrainMaterial?TerrainMaterial:Material(FLinearColor(.29f,.32f,.22f)));
        }
    }
    UE_LOG(LogTemp,Display,TEXT("Terrain surface ready: focused %d grid, eight 128 grids, %.3f seconds before foliage"),DetailedTerrainResolution,FPlatformTime::Seconds()-Started);
    CreateFoliage();
}
void ASeigeGameMode::RefreshBuildingPads()
{
    if(TerrainTiles.IsEmpty())return;
    const FPreparedTerrain Prepared(*this);
    if(TerrainPadSignature==Prepared.Signature)return;
    const double Started=FPlatformTime::Seconds();
    TArray<FVector4> Changed;
    for(const auto& Pair:TerrainPadBounds)
    {
        const FVector4* Next=Prepared.Bounds.Find(Pair.Key);
        if(!Next||!Next->Equals(Pair.Value,.0001))Changed.Add(Pair.Value);
    }
    for(const auto& Pair:Prepared.Bounds)
    {
        const FVector4* Previous=TerrainPadBounds.Find(Pair.Key);
        if(!Previous||!Previous->Equals(Pair.Value,.0001))Changed.Add(Pair.Value);
    }
    TerrainPadSignature=Prepared.Signature;TerrainPadBounds=Prepared.Bounds;
    struct FChangedRect {int32 Tile,MinX,MinY,MaxX,MaxY;};
    TArray<FChangedRect> Rects;int32 UpdatedVertices=0,UpdatedChunks=0;
    const double Half=Sim.WorldHalfSize;
    for(int32 TileIndex=0;TileIndex<TerrainTiles.Num();++TileIndex)
    {
        auto& Tile=TerrainTiles[TileIndex];const double Step=Half*2/Tile.Resolution;
        for(const auto& Area:Changed)
        {
            const FVector2D P(Area.X-Tile.Offset.X,Area.Y-Tile.Offset.Y);
            // Two cells also refresh the vertex normals bordering the edited pad.
            const double Radius=Area.W+Step*2+SharedEdgeInfluence(*this,Area);
            if(P.X+Radius<-Half||P.X-Radius>Half||P.Y+Radius<-Half||P.Y-Radius>Half)continue;
            const int32 MinX=FMath::Clamp(FMath::FloorToInt((P.X-Radius+Half)/Step),0,Tile.Resolution),MaxX=FMath::Clamp(FMath::CeilToInt((P.X+Radius+Half)/Step),0,Tile.Resolution);
            const int32 MinY=FMath::Clamp(FMath::FloorToInt((P.Y-Radius+Half)/Step),0,Tile.Resolution),MaxY=FMath::Clamp(FMath::CeilToInt((P.Y+Radius+Half)/Step),0,Tile.Resolution);
            Rects.Add({TileIndex,MinX,MinY,MaxX,MaxY});
            for(int32 Y=MinY;Y<=MaxY;++Y)for(int32 X=MinX;X<=MaxX;++X)
            {Tile.Heights[Y*(Tile.Resolution+1)+X]=VertexHeight(Prepared,Tile,X,Y);++UpdatedVertices;}
        }
    }
    if(Landscape)
    {
        const auto DirtPatches=PrepareDirt(*this);
        TArray<UProceduralMeshComponent*> Components;Landscape->GetComponents(Components);
        for(auto* Component:Components)
        {
            if(Component->ComponentTags.Num()!=3)continue;
            const int32 Index=FCString::Atoi(*Component->ComponentTags[0].ToString()),X=FCString::Atoi(*Component->ComponentTags[1].ToString()),Y=FCString::Atoi(*Component->ComponentTags[2].ToString());
            if(!TerrainTiles.IsValidIndex(Index))continue;
            for(const auto& Rect:Rects)if(Rect.Tile==Index&&Rect.MinX<=X+TerrainChunkCells&&Rect.MaxX>=X&&Rect.MinY<=Y+TerrainChunkCells&&Rect.MaxY>=Y)
            {BuildTerrainChunk(*this,TerrainTiles[Index],X,Y,Component,DirtPatches);++UpdatedChunks;break;}
        }
    }
    // Keep retained nearby foliage on the updated triangle surface. Full forest
    // regeneration is unnecessary, so a new foundation cannot reshuffle trees.
    for(AActor* Actor:{Foliage.Get(),GroundCover.Get()})if(Actor)
    {
        TArray<UHierarchicalInstancedStaticMeshComponent*> Components;Actor->GetComponents(Components);
        for(auto* Component:Components)
        {
            TSet<int32> Touched;
            for(const auto& Area:Changed)
            {
                const FVector2D P(Area.X,Area.Y);
                const double Radius=Area.W+Sim.WorldHalfSize*4/DetailedTerrainResolution+SharedEdgeInfluence(*this,Area);
                // Query the entire changed XY footprint through the terrain's
                // height bounds; the old instance can be above or below its new pad.
                const FBox Bounds(FVector((P.X-Radius)*RenderScale,(P.Y-Radius)*RenderScale,-6000*RenderScale),FVector((P.X+Radius)*RenderScale,(P.Y+Radius)*RenderScale,6000*RenderScale));
                for(int32 Index:Component->GetInstancesOverlappingBox(Bounds,true))Touched.Add(Index);
            }
            const bool AutoRebuild=Component->bAutoRebuildTreeOnInstanceChanges;
            if(!Touched.IsEmpty())Component->bAutoRebuildTreeOnInstanceChanges=false;
            for(int32 Index:Touched)
            {
                FTransform Transform;
                if(Component->GetInstanceTransform(Index,Transform,true))
                {
                    FVector P=Transform.GetLocation();P.Z=GroundHeight(FVector2D(P)/RenderScale)*RenderScale;Transform.SetLocation(P);
                    if(Actor==GroundCover.Get())Transform.SetRotation(GroundCoverRotation(*this,FVector2D(P)/RenderScale,Transform.Rotator().Yaw));
                    Component->UpdateInstanceTransform(Index,Transform,true,false,true);
                }
            }
            if(!Touched.IsEmpty()){Component->BuildTreeIfOutdated(false,true);Component->MarkRenderStateDirty();Component->bAutoRebuildTreeOnInstanceChanges=AutoRebuild;}
        }
    }
    UE_LOG(LogTemp,Display,TEXT("Terrain foundation update: %d vertices, %d chunks, %.3f seconds; forest retained"),UpdatedVertices,UpdatedChunks,FPlatformTime::Seconds()-Started);
}
namespace
{
constexpr double GroundTileSize=900;
constexpr int32 GroundTileRadius=4;
struct FSceneryClearance {FVector2D Position;double Radius;bool Square=false;};
TArray<FSceneryClearance> VisibleClearances(const ASeigeGameMode& G)
{
    TArray<FSceneryClearance> Areas;const auto* Colony=G.ViewedSimulation();if(!Colony)return Areas;
    const FVector2D Offset=G.DetailedSectorOffset();
    if(G.Screen!=TEXT("landing"))for(const auto& B:Colony->Buildings)
        if(B.Health>0&&(G.Observer||G.DetailedSectorIndex()==4||G.Sim.IsVisible(B.Position+Offset)))if(const auto* D=Colony->Definition(B))Areas.Add({B.Position+Offset,D->Footprint,true});
    for(const auto& N:Colony->Nodes)Areas.Add({N.Position+Offset,90});
    return Areas;
}
bool IsSceneryClear(FVector2D P,double Radius,FVector2D Offset,double Half,const TArray<FSceneryClearance>& Areas)
{
    const FVector2D Local=P-Offset;if(FMath::Abs(Local.X)>Half||FMath::Abs(Local.Y)>Half)return true;
    for(const auto& A:Areas)
    {
        const FVector2D D=P-A.Position;
        if(A.Square?FMath::Max(FMath::Abs(D.X),FMath::Abs(D.Y))<A.Radius+Radius:D.SquaredLength()<FMath::Square(A.Radius+Radius))return true;
    }
    return false;
}
uint32 GroundCellSeed(int32 X,int32 Y)
{
    uint32 Seed=uint32(X)*0x9e3779b9u^uint32(Y)*0x85ebca6bu^2222u;
    Seed^=Seed>>16;Seed*=0x7feb352du;Seed^=Seed>>15;Seed*=0x846ca68bu;return Seed^(Seed>>16);
}
UHierarchicalInstancedStaticMeshComponent* VegetationSet(AActor* Actor,USceneComponent* Root,UStaticMesh* Mesh,const FString& Kind)
{
    if(!Mesh)return nullptr;
    auto* Set=NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor);Set->bAutoRebuildTreeOnInstanceChanges=false;Set->SetStaticMesh(Mesh);Set->SetupAttachment(Root);
    Set->SetLODDistanceScale(1);Set->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const bool Tree=Kind.StartsWith(TEXT("Oak"))||Kind.StartsWith(TEXT("Pine"));
    const bool Sward=Kind.StartsWith(TEXT("Grass"))||Kind==TEXT("Wildflowers");
    Set->SetCullDistances(Tree?0:Sward?50000:15000,Tree?0:Sward?90000:42000);
    if(Sward)Set->SetCastShadow(true);
    Set->RegisterComponent();Actor->AddInstanceComponent(Set);return Set;
}
}
void ASeigeGameMode::CreateFoliage()
{
    if(!FApp::CanEverRender())return;
    if(Foliage)Foliage->Destroy();if(GroundCover){GroundCover->Destroy();GroundCover=nullptr;}
    auto* Ground=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(Ground);Ground->SetRootComponent(Root);Root->RegisterComponent();Foliage=Ground;
    const FVector2D Offset=DetailedSectorOffset();const double Half=Sim.WorldHalfSize;const auto Areas=VisibleClearances(*this);
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    for(const FString Kind:{TEXT("OakA"),TEXT("OakB"),TEXT("PineA"),TEXT("PineB"),TEXT("RockA"),TEXT("RockB")})
    {
        const FString Name=TEXT("SM_")+Kind,Path=NatureAssets.Contains(Kind)?NatureAssets[Kind]:FString::Printf(TEXT("/Game/Art/%s.%s"),*Name,*Name);
        if(auto* Set=VegetationSet(Ground,Root,LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn),Kind))Sets.Add(Kind,Set);
    }
    auto Add=[&](const FString& Kind,FVector2D P,double Size,double Rotation){if(auto** Set=Sets.Find(Kind))(*Set)->AddInstance(FTransform(FRotator(0,Rotation,0),RenderPosition(P),FVector(Size)));};
    FRandomStream R(2222+DetailedSectorIndex()*100003);
    auto Tree=[&](FVector2D P,int32 I)
    {
        // Consume a fixed random sequence before visibility/clearance filtering.
        // Returning to a changed colony therefore cannot relocate unrelated trees.
        const double Chance=R.FRand(),Size=R.FRandRange(.72,1.16),Yaw=R.FRandRange(0,360);
        if(IsSceneryClear(P,210,Offset,Half,Areas)||Chance>WoodlandDensity(P)*.9)return;
        const FString Kind=I%9==0?(I%2?TEXT("PineA"):TEXT("PineB")):(I%2?TEXT("OakA"):TEXT("OakB"));Add(Kind,P,Size,Yaw);
    };
    for(int32 I=0;I<ForestCandidates;++I)Tree(Offset+FVector2D(R.FRandRange(-Half,Half),R.FRandRange(-Half,Half)),I);
    // Supplemental woodland is fixed to the sector, never to the moving camera.
    for(int32 I=0;I<NearForestCandidates;++I)Tree(Offset+FVector2D(R.FRandRange(-8000,8000),R.FRandRange(-8000,8000)),I);
    if(const auto* Colony=ViewedSimulation())for(const auto& N:Colony->Nodes)for(int32 I=0;I<13;++I)
    {
        const FVector2D P=Offset+N.Position+FVector2D(R.FRandRange(-125,125),R.FRandRange(-125,125));Add(I%2?TEXT("RockA"):TEXT("RockB"),P,R.FRandRange(.35,.8),R.FRandRange(0,360));
    }
    for(auto& Pair:Sets){Pair.Value->BuildTreeIfOutdated(false,true);Pair.Value->bAutoRebuildTreeOnInstanceChanges=true;}
    CreateGroundCover();
}
void ASeigeGameMode::CreateGroundCover()
{
    if(!FApp::CanEverRender())return;
    if(!GroundCover)
    {
        GroundCover=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(GroundCover);GroundCover->SetRootComponent(Root);Root->RegisterComponent();
    }
    FoliageCenter=FVector2D(CameraCenter);const FVector2D Offset=DetailedSectorOffset();const double Half=Sim.WorldHalfSize;const auto Areas=VisibleClearances(*this);
    const int32 CenterX=FMath::FloorToInt(FoliageCenter.X/GroundTileSize),CenterY=FMath::FloorToInt(FoliageCenter.Y/GroundTileSize);
    TSet<FName> Wanted,Present;
    for(int32 Y=CenterY-GroundTileRadius;Y<=CenterY+GroundTileRadius;++Y)for(int32 X=CenterX-GroundTileRadius;X<=CenterX+GroundTileRadius;++X)Wanted.Add(FName(*FString::Printf(TEXT("ground_%d_%d"),X,Y)));
    TArray<UHierarchicalInstancedStaticMeshComponent*> Existing;GroundCover->GetComponents(Existing);
    for(auto* Component:Existing)
    {
        if(Component->ComponentTags.IsEmpty()||!Wanted.Contains(Component->ComponentTags[0])){GroundCover->RemoveInstanceComponent(Component);Component->DestroyComponent();}
        else Present.Add(Component->ComponentTags[0]);
    }
    TMap<FString,UStaticMesh*> Meshes;
    for(const FString Kind:{TEXT("Grass"),TEXT("GrassB"),TEXT("Wildflowers"),TEXT("Shrub"),TEXT("RockA"),TEXT("RockB")})
    {
        const FString Name=TEXT("SM_")+Kind,Path=NatureAssets.Contains(Kind)?NatureAssets[Kind]:FString::Printf(TEXT("/Game/Art/%s.%s"),*Name,*Name);
        if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn))Meshes.Add(Kind,Mesh);
    }
    constexpr int32 Cells=(GroundTileRadius*2+1)*(GroundTileRadius*2+1),PropsPerCell=6500/Cells;
    const int32 GrassPerCell=GroundCoverCandidates/Cells;
    for(int32 Y=CenterY-GroundTileRadius;Y<=CenterY+GroundTileRadius;++Y)for(int32 X=CenterX-GroundTileRadius;X<=CenterX+GroundTileRadius;++X)
    {
        const FName Tag(*FString::Printf(TEXT("ground_%d_%d"),X,Y));if(Present.Contains(Tag))continue;
        const FVector2D Origin(X*GroundTileSize,Y*GroundTileSize);
        if(Origin.X+GroundTileSize<Offset.X-Half||Origin.X>Offset.X+Half||Origin.Y+GroundTileSize<Offset.Y-Half||Origin.Y>Offset.Y+Half)continue;
        TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
        for(const auto& Pair:Meshes)if(auto* Set=VegetationSet(GroundCover,GroundCover->GetRootComponent(),Pair.Value,Pair.Key)){Set->ComponentTags.Add(Tag);Sets.Add(Pair.Key,Set);}
        auto Add=[&](const FString& Kind,FVector2D P,double Size,double Yaw){if(auto** Set=Sets.Find(Kind))(*Set)->AddInstance(FTransform(GroundCoverRotation(*this,P,Yaw),RenderPosition(P),FVector(Size)));};
        FRandomStream R{static_cast<int32>(GroundCellSeed(X,Y))};
        for(int32 I=0;I<PropsPerCell;++I)
        {
            const FVector2D P=Origin+FVector2D(R.FRandRange(0,GroundTileSize),R.FRandRange(0,GroundTileSize));const double Size=R.FRandRange(.4,1.1),Yaw=R.FRandRange(0,360);
            if(IsSceneryClear(P,30,Offset,Half,Areas))continue;const bool Outcrop=FMath::PerlinNoise2D(P/950+FVector2D(13,-8))>.24;
            Add(I%5==0&&Outcrop?(I%2?TEXT("RockA"):TEXT("RockB")):TEXT("Shrub"),P,Size,Yaw);
        }
        for(int32 I=0;I<GrassPerCell;++I)
        {
            const FVector2D P=Origin+FVector2D(R.FRandRange(0,GroundTileSize),R.FRandRange(0,GroundTileSize));const double Chance=R.FRand(),Size=R.FRandRange(GrassScaleMin,GrassScaleMax),Yaw=R.FRandRange(0,360);
            const double Woodland=WoodlandDensity(P);
            const bool Flowers=I%35==0&&Woodland<.15&&FMath::PerlinNoise2D(P/700+FVector2D(4.2,18.6))>.02;
            const FString Kind=Flowers?TEXT("Wildflowers"):I%2?TEXT("Grass"):TEXT("GrassB");
            const UStaticMesh* Mesh=Meshes.FindRef(Kind);
            // Include the rotated, scaled clump footprint so blades stay outside foundations.
            const double Margin=Mesh?Mesh->GetBounds().SphereRadius*Size/RenderScale:15.;
            if(IsSceneryClear(P,Margin,Offset,Half,Areas)||Chance<Woodland*.7||Chance>MeadowSwardDensity(P))continue;
            Add(Kind,P,Size,Yaw);
        }
        for(auto& Pair:Sets){Pair.Value->BuildTreeIfOutdated(false,true);Pair.Value->bAutoRebuildTreeOnInstanceChanges=true;}
    }
}
void ASeigeGameMode::RefreshEnvironment()
{
    if(!FApp::CanEverRender())return;const bool Map=IsRegionMap();
    if(!Map)
    {
        if(RenderedSector!=DetailedSectorIndex()){SelectedId=0;SelectedBuild.Empty();CreateLandscape();}
        else
        {
            RefreshBuildingPads();
            if(FMath::FloorToInt(FoliageCenter.X/GroundTileSize)!=FMath::FloorToInt(CameraCenter.X/GroundTileSize)||FMath::FloorToInt(FoliageCenter.Y/GroundTileSize)!=FMath::FloorToInt(CameraCenter.Y/GroundTileSize))CreateGroundCover();
        }
    }
    if(Landscape)Landscape->SetActorHiddenInGame(Map);if(Foliage)Foliage->SetActorHiddenInGame(Map);if(GroundCover)GroundCover->SetActorHiddenInGame(Map);
}
void ASeigeGameMode::ClearSceneryAt(FVector2D Position,float Radius)
{
    const bool Home=FMath::Abs(Position.X)<=Sim.WorldHalfSize&&FMath::Abs(Position.Y)<=Sim.WorldHalfSize;
    if(!Observer&&!Home&&!Sim.IsVisible(Position))return;
    for(AActor* Actor:{Foliage.Get(),GroundCover.Get()})if(Actor)
    {
        TArray<UHierarchicalInstancedStaticMeshComponent*> Components;Actor->GetComponents(Components);
        for(auto* Component:Components)
        {
            const FBox Bounds(FVector((Position.X-Radius)*RenderScale,(Position.Y-Radius)*RenderScale,-6000*RenderScale),FVector((Position.X+Radius)*RenderScale,(Position.Y+Radius)*RenderScale,6000*RenderScale));
            const auto Indices=Component->GetInstancesOverlappingBox(Bounds,true);
            if(!Indices.IsEmpty())Component->RemoveInstances(Indices);
        }
    }
}
