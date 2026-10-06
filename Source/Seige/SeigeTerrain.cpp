#include "SeigeGameMode.h"
#include "SeigeSceneryContact.h"
#include "SeigeSceneryPlacement.h"
#include "SeigeSceneryStreaming.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "HAL/PlatformTime.h"

struct FSeigeStreamedCell
{
    bool BaseReady=false,DetailReady=false;
    TMap<FName,TArray<FPrimitiveInstanceId>> Instances;
};
struct FSeigeSceneryStreamState
{
    TMap<FName,TWeakObjectPtr<UInstancedStaticMeshComponent>> Sets;
    TMap<FIntPoint,FSeigeStreamedCell> Cells;
    TMap<FString,TWeakObjectPtr<UStaticMesh>> Meshes;
    TArray<FIntPoint> Wanted;
    TArray<FIntPoint> Retiring;
    TMap<FIntPoint,FBox> CellBounds;
    TSet<FIntPoint> VisibleCells,NearCells,DetailCells;
    FVector PriorityPosition=FVector::ZeroVector,PriorityForward=FVector::ZeroVector;
    double PriorityAspect=0,PriorityTanHalf=0;
    bool PriorityDirty=true,Committing=false;
    FString PendingTerrainSignature;
    TArray<FName> CommitKeys;
    int32 CommitKey=0;
    FIntPoint FocusCell=FIntPoint(MAX_int32,MAX_int32),CameraCell=FIntPoint(MAX_int32,MAX_int32);
    FIntPoint PendingCell;
    int32 PendingIndex=MIN_int32;
    bool PendingBase=false,PendingDetail=false;
    FRandomStream Random;
    TMap<FName,TArray<FTransform>> PendingTransforms;
    int32 Remaining=0;
    int32 RemainingVisible=0,RemainingNear=0;
    double Started=0;
};

namespace
{
FTransform AlignProxyBounds(const FTransform& SourceTransform,const UStaticMesh* Source,const UStaticMesh* Proxy)
{
    FTransform Result=SourceTransform;
    Result.SetScale3D(SourceTransform.GetScale3D()*Source->GetBounds().BoxExtent/Proxy->GetBounds().BoxExtent);
    const FVector Center=SourceTransform.TransformPosition(Source->GetBounds().Origin);
    Result.SetLocation(Center-Result.TransformVector(Proxy->GetBounds().Origin));
    return Result;
}
FVector ProxyPivotOffset(const FTransform& Transform,const UStaticMesh* Source,const UStaticMesh* Proxy)
{
    const FVector SourceScale=Transform.GetScale3D()*Proxy->GetBounds().BoxExtent/Source->GetBounds().BoxExtent;
    return Transform.GetRotation().RotateVector(Source->GetBounds().Origin*SourceScale-Proxy->GetBounds().Origin*Transform.GetScale3D());
}
bool HidePendingHomeFoundation(const ASeigeGameMode& G)
{
    if(G.Observer)return false;
    if(G.Screen==TEXT("landing")||(G.MenuOpen&&G.MenuReturnScreen==TEXT("landing")))return true;
    // The initial scenario reserves a core before a human chooses its site.
    // Main/settings backdrops must not turn that reservation into a graded pad.
    return G.Sim.Time<=UE_DOUBLE_SMALL_NUMBER&&G.Screen!=TEXT("playing")&&!(G.MenuOpen&&G.MenuReturnScreen==TEXT("playing"));
}
double RoadHalfWidth(const FSeigeSimulation& Colony,const FSeigeTransportSegment& Road)
{
    double Width=0;
    if(const auto* Tier=Colony.TransportTiers.Find(Road.Tier))Width=Tier->WidthMeters;
    if(Road.IsConstructing)if(const auto* Tier=Colony.TransportTiers.Find(Road.TargetTier))Width=FMath::Max(Width,Tier->WidthMeters);
    return Width/(2*FMath::Max(.000001,Colony.MetersPerWorldUnit()));
}
FVector2D ClosestRoadPoint(FVector2D P,FVector2D A,FVector2D B)
{
    const FVector2D Delta=B-A;const double LengthSquared=Delta.SquaredLength();
    return A+Delta*(LengthSquared>UE_DOUBLE_SMALL_NUMBER?FMath::Clamp(FVector2D::DotProduct(P-A,Delta)/LengthSquared,0.,1.):0.);
}
FString RoadSightToken(const ASeigeGameMode& G,FVector2D A,FVector2D B,bool& Revealed)
{
    // Match the rendered route's 8 m visibility sampling. Two visible endpoints
    // must not expose a graded corridor through an unseen gap between sensors.
    Revealed=true;const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector2D::Distance(A,B)*G.RenderScale/800.));
    for(int32 I=0;I<=Steps;++I)if(!G.IsWorldVisible(FMath::Lerp(A,B,double(I)/Steps))){Revealed=false;break;}
    FString Result;
    for(const auto& Sensor:G.Sim.Buildings)if(const auto* D=G.Sim.Definition(Sensor))if(D->SensorRange>0)
    {
        Result+=FString::Printf(TEXT("%d:%.3f:%.3f:%d:%d:%d:%d;"),Sensor.Id,Sensor.Position.X,Sensor.Position.Y,
            Sensor.Enabled?1:0,Sensor.Health>0?1:0,Sensor.IsConstructing?1:0,Sensor.Workers);
    }
    return Result;
}
struct FHeightPad
{
    FVector2D Position;
    double Height=0,Inner=0,Outer=0;
};
struct FHeightRoad
{
    FVector2D A,B;
    double Inner=0,Outer=0;
    bool RequiresSight=false;
};
struct FPreparedTerrain
{
    const ASeigeGameMode& Game;
    TArray<FHeightPad> Pads[9];
    TArray<FHeightRoad> Roads[9];
    TMap<FString,FVector4> Bounds;
    FString Signature;
    double RidgeCos=0,RidgeSin=0;
    explicit FPreparedTerrain(const ASeigeGameMode& G):Game(G)
    {
        const double Angle=FMath::DegreesToRadians(double(G.RidgeAngleDegrees));
        RidgeCos=FMath::Cos(Angle);RidgeSin=FMath::Sin(Angle);
        auto AddColony=[&](const FSeigeSimulation& Colony,FVector2D Offset,int32 Index)
        {
            if(Index<0||Index>8||(Index==4&&HidePendingHomeFoundation(G)))return;
            for(const auto& B:Colony.Buildings)
            {
                const FVector2D P=B.Position+Offset;
                if(B.Health<=0||(!G.Observer&&Index!=4&&!G.IsWorldVisible(P)))continue;
                if(const auto* D=Colony.Definition(B))
                {
                    const double GridStep=G.Sim.WorldHalfSize*2/G.DetailedTerrainResolution;
                    // Grid vertices bordering an off-grid foundation must also be
                    // flat, otherwise a triangle cuts through its outer corners.
                    // Reserve future upgrades for placement, but grade only the built footprint.
                    const auto* Planned=B.UpgradeTarget.IsEmpty()?D:Colony.BuildingDefs.Find(B.UpgradeTarget);
                    const double Reserved=Planned?Planned->Footprint:D->Footprint;
                    const double Inner=FMath::Max(double(Reserved*G.CorePadInnerRatio),Reserved+GridStep);
                    const double Outer=FMath::Max(double(Reserved*G.CorePadOuterRatio),Inner+GridStep);
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
        auto AddRoads=[&](const FSeigeSimulation& Colony,FVector2D Offset,int32 Index)
        {
            if(Index<0||Index>8)return;
            const double Step=G.Sim.WorldHalfSize*2/G.DetailedTerrainResolution,Span=G.Sim.WorldHalfSize*2;
            for(const auto& Road:Colony.Roads)
            {
                if(Road.Health<=0)continue;
                const double Half=RoadHalfWidth(Colony,Road);if(Half<=0)continue;
                const FVector2D A=Road.A+Offset,B=Road.B+Offset,Center=(A+B)*.5;
                const double Inner=Half+Step,Outer=Inner+Step;
                const bool RequiresSight=!G.Observer&&Index!=4;
                bool AnyVisible=true;const FString Sight=RequiresSight?RoadSightToken(G,A,B,AnyVisible):FString();
                if(!AnyVisible)continue;
                const FHeightRoad Grade{A,B,Inner,Outer,RequiresSight};
                const double Radius=FMath::Max(FMath::Abs(A.X-B.X),FMath::Abs(A.Y-B.Y))*.5+Outer;
                const FString Key=FString::Printf(TEXT("road:%d:%d:%s"),Index,Road.Id,*Sight);
                Bounds.Add(Key,FVector4(Center.X,Center.Y,Inner,Radius));
                Signature+=FString::Printf(TEXT("%s:%.4f:%.4f:%.4f:%.4f:%.4f;"),*Key,A.X,A.Y,B.X,B.Y,Half);
                const int32 MinX=FMath::Clamp(FMath::FloorToInt((FMath::Min(A.X,B.X)-Outer+Span*.5)/Span),-1,1),MaxX=FMath::Clamp(FMath::FloorToInt((FMath::Max(A.X,B.X)+Outer+Span*.5)/Span),-1,1);
                const int32 MinY=FMath::Clamp(FMath::FloorToInt((FMath::Min(A.Y,B.Y)-Outer+Span*.5)/Span),-1,1),MaxY=FMath::Clamp(FMath::FloorToInt((FMath::Max(A.Y,B.Y)+Outer+Span*.5)/Span),-1,1);
                for(int32 Y=MinY;Y<=MaxY;++Y)for(int32 X=MinX;X<=MaxX;++X)Roads[(Y+1)*3+X+1].Add(Grade);
            }
        };
        AddRoads(G.Sim,FVector2D::ZeroVector,4);
        for(const auto& N:G.Neighbors)AddRoads(N.Sim,N.Offset,N.Index);
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
    double PadHeight(FVector2D P,double Base) const
    {
        const double Span=Game.Sim.WorldHalfSize*2;
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
    double Height(FVector2D P) const
    {
        double Base=Natural(P);const double Span=Game.Sim.WorldHalfSize*2;
        const int32 X=FMath::FloorToInt((P.X+Span*.5)/Span),Y=FMath::FloorToInt((P.Y+Span*.5)/Span);
        if(X<-1||X>1||Y<-1||Y>1)return Base;
        double WeightedHeight=0,TotalWeight=0,Influence=0;
        for(const auto& Road:Roads[(Y+1)*3+X+1])
        {
            const FVector2D Q=ClosestRoadPoint(P,Road.A,Road.B);
            const double Distance=(P-Q).Length();
            if(Distance>=Road.Outer||(Road.RequiresSight&&(!Game.IsWorldVisible(Q)||!Game.IsWorldVisible(P))))continue;
            const double Weight=1-FMath::SmoothStep(Road.Inner,Road.Outer,Distance);
            const double BlendWeight=Weight/FMath::Max(.000001,1-Weight);
            const FVector2D Along=(Road.B-Road.A).GetSafeNormal()*Span/Game.DetailedTerrainResolution;
            // Smooth a few meters along the existing hill, not a straight ramp
            // between distant endpoints. Pads take precedence at plot entrances.
            const FVector2D Before=ClosestRoadPoint(Q-Along,Road.A,Road.B),After=ClosestRoadPoint(Q+Along,Road.A,Road.B);
            const double Target=(PadHeight(Before,Natural(Before))+2*PadHeight(Q,Natural(Q))+PadHeight(After,Natural(After)))*.25;
            WeightedHeight+=Target*BlendWeight;TotalWeight+=BlendWeight;Influence=FMath::Max(Influence,Weight);
        }
        if(TotalWeight>0)Base=FMath::Lerp(Base,WeightedHeight/TotalWeight,Influence);
        return PadHeight(P,Base);
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
FTransform GroundCoverContact(const ASeigeGameMode& G,const FTransform& Transform,const UStaticMesh* Mesh)
{
    if(!Mesh)return Transform;
    return FitSeigeSceneryRootPlane(Transform,Mesh->GetBoundingBox(),[&](FVector2D World)
    {return G.GroundHeight(World/G.RenderScale)*G.RenderScale;});
}
TArray<FDirtPatch> PrepareDirt(const ASeigeGameMode& G)
{
    TArray<FDirtPatch> DirtPatches;
    const FVector2D Offset=G.DetailedSectorOffset();
    if(const auto* Colony=G.ViewedSimulation())
    {
        if(G.DetailedSectorIndex()!=4||!HidePendingHomeFoundation(G))for(const auto& B:Colony->Buildings)
            if(B.Health>0&&(G.Observer||G.DetailedSectorIndex()==4||G.IsWorldVisible(B.Position+Offset)))if(const auto* D=Colony->Definition(B))
            {
                const double Step=G.Sim.WorldHalfSize*2/G.DetailedTerrainResolution;
                // Reserve future upgrades for placement, but grade only the built footprint.
                    const double Reserved=D->Footprint;
                const double PadInner=FMath::Max(double(Reserved*G.CorePadInnerRatio),Reserved+Step);
                const double PadOuter=FMath::Max(double(Reserved*G.CorePadOuterRatio),PadInner+Step);
                // Exposed soil follows the foundation edge, while the wider
                // graded bank remains a meadow rather than a square dirt lot.
                DirtPatches.Add({B.Position+Offset,D->Footprint*.72,D->Footprint+90,.65,true,PadOuter+Step});
            }
        if(G.Observer||G.DetailedSectorIndex()==4)
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
            const bool Boundary=X==0||X==Resolution||Y==0||Y==Resolution;
            const double NormalStep=Boundary?Half*2/128:Step;
            const double DX=!Boundary?(Tile.Heights[I+1]-Tile.Heights[I-1])/(Step*2):
                (G.GroundHeight(P+FVector2D(NormalStep,0))-G.GroundHeight(P-FVector2D(NormalStep,0)))/(NormalStep*2);
            const double DY=!Boundary?(Tile.Heights[I+Resolution+1]-Tile.Heights[I-Resolution-1])/(Step*2):
                (G.GroundHeight(P+FVector2D(0,NormalStep))-G.GroundHeight(P-FVector2D(0,NormalStep)))/(NormalStep*2);
            const FVector Normal=FVector(-DX,-DY,1).GetSafeNormal();
            Vertices.Add(FVector(P.X*G.RenderScale,P.Y*G.RenderScale,Tile.Heights[I]*G.RenderScale));Normals.Add(Normal);UVs.Add(P*G.RenderScale/700);
            const double Woodland=G.WoodlandDensity(P);
            double Dirt=FMath::Clamp((FMath::PerlinNoise2D(P/1250+FVector2D(14,3))-.28)*.7,0.,.22);
            // Sward occupancy varies over 7.8 m and is thresholded for placement.
            // Sampling that field on the 3.5 m terrain lattice aliases into
            // diagonal soil bands. Keep soil's broader field and local patches;
            // grass placement still uses the independent fine occupancy field.
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
            // A broad world-anchored meadow variation, separate from the much
            // smaller sward-density patches. Shared positions give both tile
            // detail levels the same material input without per-pixel noise.
            const double Vigor=FMath::Clamp(.5+FMath::PerlinNoise2D(P/1300+FVector2D(7.4,-21.8))*.6+
                FMath::PerlinNoise2D(P/310+FVector2D(-8.2,5.7))*.18,.15,.85);
            Colors.Add(FLinearColor(float(Dirt*(1-Rock)),float(Rock),float(Woodland),float(Vigor)));
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
            // The neighboring 128 grids retain the same continuous world-space
            // surface material. Their lower mesh density is the detail boundary,
            // rather than an unrelated flat-color surface around the home tile.
            Terrain->SetMaterial(0,TerrainMaterial?TerrainMaterial:Material(FLinearColor(.29f,.32f,.22f)));
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
    if(SceneryStream){SceneryStream->CellBounds.Reset();SceneryStream->PriorityDirty=true;}
    TArray<FChangedRect> Rects;int32 UpdatedVertices=0,UpdatedChunks=0;
    const double Half=Sim.WorldHalfSize;
    for(int32 TileIndex=0;TileIndex<TerrainTiles.Num();++TileIndex)
    {
        auto& Tile=TerrainTiles[TileIndex];const double Step=Half*2/Tile.Resolution;
        for(const auto& Area:Changed)
        {
            const FVector2D P(Area.X-Tile.Offset.X,Area.Y-Tile.Offset.Y);
            // Interior normals read one cell either side; shared-edge normals
            // use the common coarse step, including on the detailed tile.
            const double CoarseStep=Half*2/128;
            const double EdgeDistance=Half-FMath::Max(FMath::Abs(P.X),FMath::Abs(P.Y));
            const double NormalInfluence=EdgeDistance<=Area.W+CoarseStep?CoarseStep:0;
            const double Radius=Area.W+Step*2+FMath::Max(SharedEdgeInfluence(*this,Area),NormalInfluence);
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
        TArray<UInstancedStaticMeshComponent*> Components;Actor->GetComponents(Components);
        for(auto* Component:Components)
        {
            UStaticMesh* ProxySource=nullptr;
            for(const FName Tag:Component->ComponentTags)
            {
                const FString Text=Tag.ToString();
                if(Text.StartsWith(TEXT("seige_source:")))
                {const FString Path=NatureAssets.FindRef(Text.RightChop(13));if(!Path.IsEmpty())ProxySource=LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn);break;}
            }
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
            for(int32 Index:Touched)
            {
                FTransform Transform;
                if(Component->GetInstanceTransform(Index,Transform,true))
                {
                    const FVector Pivot=ProxySource?ProxyPivotOffset(Transform,ProxySource,Component->GetStaticMesh()):FVector::ZeroVector;
                    FVector P=Transform.GetLocation()-Pivot;P.Z=GroundHeight(FVector2D(P)/RenderScale)*RenderScale;
                    if(Actor==GroundCover.Get())Transform.SetRotation(GroundCoverRotation(*this,FVector2D(P)/RenderScale,Transform.Rotator().Yaw));
                    Transform.SetLocation(P+(ProxySource?ProxyPivotOffset(Transform,ProxySource,Component->GetStaticMesh()):FVector::ZeroVector));
                    if(Actor==GroundCover.Get()&&Component->ComponentHasTag(FName(TEXT("seige_sward"))))
                    {
                        if(ProxySource)
                        {
                            FTransform Near=Transform;Near.SetLocation(P);
                            Near.SetScale3D(Transform.GetScale3D()*Component->GetStaticMesh()->GetBounds().BoxExtent/ProxySource->GetBounds().BoxExtent);
                            Transform=AlignProxyBounds(GroundCoverContact(*this,Near,ProxySource),ProxySource,Component->GetStaticMesh());
                        }
                        else Transform=GroundCoverContact(*this,Transform,Component->GetStaticMesh());
                    }
                    Component->UpdateInstanceTransform(Index,Transform,true,false,true);
                }
            }
            if(!Touched.IsEmpty())Component->MarkRenderStateDirty();
        }
    }
    UE_LOG(LogTemp,Display,TEXT("Terrain foundation update: %d vertices, %d chunks, %.3f seconds; forest retained"),UpdatedVertices,UpdatedChunks,FPlatformTime::Seconds()-Started);
}
namespace
{
constexpr double GroundTileSize=900;
constexpr int32 GroundReferenceCells=81;
constexpr int32 SceneryLodBands=8;
struct FSceneryClearance
{
    FVector2D Position;
    double Radius;
    bool Square=false;
    FVector2D End=FVector2D::ZeroVector;
    bool Segment=false,RequiresSight=false;
};
void AddRoadClearances(const ASeigeGameMode& G,const FSeigeSimulation& Colony,FVector2D Offset,int32 Sector,TArray<FSceneryClearance>& Areas)
{
    for(const auto& Road:Colony.Roads)
    {
        if(Road.Health<=0)continue;
        const double Half=RoadHalfWidth(Colony,Road);if(Half<=0)continue;
        const FVector2D A=Road.A+Offset,B=Road.B+Offset;const bool RequiresSight=!G.Observer&&Sector!=4;
        bool AnyVisible=true;if(RequiresSight)RoadSightToken(G,A,B,AnyVisible);if(!AnyVisible)continue;
        Areas.Add({A,Half,false,B,true,RequiresSight});
    }
}
TArray<FSceneryClearance> VisibleClearances(const ASeigeGameMode& G)
{
    TArray<FSceneryClearance> Areas;const auto* Colony=G.ViewedSimulation();if(!Colony)return Areas;
    const FVector2D Offset=G.DetailedSectorOffset();
    if(G.DetailedSectorIndex()!=4||!HidePendingHomeFoundation(G))for(const auto& B:Colony->Buildings)
        if(B.Health>0&&(G.Observer||G.DetailedSectorIndex()==4||G.IsWorldVisible(B.Position+Offset)))if(const auto* D=Colony->Definition(B))Areas.Add({B.Position+Offset,D->ReservedFootprint,true});
    if(G.Observer||G.DetailedSectorIndex()==4)
        for(const auto& N:Colony->Nodes)Areas.Add({N.Position+Offset,90});
    AddRoadClearances(G,*Colony,Offset,G.DetailedSectorIndex(),Areas);
    return Areas;
}
bool IsSceneryClear(const ASeigeGameMode& G,FVector2D P,double Radius,FVector2D Offset,double Half,const TArray<FSceneryClearance>& Areas)
{
    const FVector2D Local=P-Offset;if(FMath::Abs(Local.X)>Half||FMath::Abs(Local.Y)>Half)return true;
    for(const auto& A:Areas)
    {
        const FVector2D Closest=A.Segment?ClosestRoadPoint(P,A.Position,A.End):A.Position;
        if(A.RequiresSight&&(!G.IsWorldVisible(P)||!G.IsWorldVisible(Closest)))continue;
        const FVector2D D=P-Closest;
        if(A.Square?FMath::Max(FMath::Abs(D.X),FMath::Abs(D.Y))<A.Radius+Radius:D.SquaredLength()<FMath::Square(A.Radius+Radius))return true;
    }
    return false;
}
uint32 GroundCellSeed(int32 X,int32 Y)
{
    uint32 Seed=uint32(X)*0x9e3779b9u^uint32(Y)*0x85ebca6bu^2222u;
    Seed^=Seed>>16;Seed*=0x7feb352du;Seed^=Seed>>15;Seed*=0x846ca68bu;return Seed^(Seed>>16);
}
UInstancedStaticMeshComponent* VegetationSet(AActor* Actor,USceneComponent* Root,UStaticMesh* Mesh,const FString& Kind,bool GrassIndirectLighting=false,float GrassProgrammableDistanceMeters=0,float MinDistance=0,float MaxDistance=0)
{
    if(!Mesh)return nullptr;
    // Nanite handles per-instance culling/LOD itself. CPU HISM trees were built
    // synchronously for each streamed strip despite never supplying these LODs.
    auto* Set=NewObject<UInstancedStaticMeshComponent>(Actor);Set->SetStaticMesh(Mesh);Set->SetupAttachment(Root);
    Set->SetCollisionEnabled(ECollisionEnabled::NoCollision);Set->SetCanEverAffectNavigation(false);Set->SetRemoveSwap();
    const bool Sward=Kind.StartsWith(TEXT("Grass"))||Kind==TEXT("Wildflowers");
    Set->InstanceMinDrawDistance=FMath::RoundToInt(MinDistance);
    Set->SetCullDistances(FMath::RoundToInt(MaxDistance),FMath::RoundToInt(MaxDistance));
    // These assets have no wind/deformation. Rigid still invalidates when a
    // foundation moves instances; Static would incorrectly suppress that update.
    Set->SetEvaluateWorldPositionOffset(false);
    Set->ShadowCacheInvalidationBehavior=EShadowCacheInvalidationBehavior::Rigid;
    // Zero preserves masked rasterization at all distances. Set before scene
    // registration so the Nanite proxy receives the optional grass-only cutoff.
    // Wildflower silhouettes remain masked regardless of this experiment.
    if(Kind==TEXT("Grass")||Kind==TEXT("GrassB"))
        Set->NanitePixelProgrammableDistance=GrassProgrammableDistanceMeters*100.f;
    if(Sward)
    {
        Set->SetCastShadow(true);
        // Dense overlapping swards are optional contributors to the Lumen/DF
        // scene. They still receive scene lighting and retain near VSM shadows.
        Set->SetAffectDistanceFieldLighting(GrassIndirectLighting);
        Set->SetAffectDynamicIndirectLighting(GrassIndirectLighting);
    }
    Set->RegisterComponent();Actor->AddInstanceComponent(Set);return Set;
}
const FName SwardTag(TEXT("seige_sward"));
const FName ProxyTag(TEXT("seige_proxy"));
void UpdateGroundCoverShadows(const ASeigeGameMode& G,AActor* Actor)
{
    if(!Actor)return;
    const FVector CameraPosition=G.CameraTransform(G.CameraViewZoom()).GetLocation();
    const double Distance=G.GrassShadowDistanceMeters*100.;
    TArray<UInstancedStaticMeshComponent*> Components;Actor->GetComponents(Components);
    for(auto* Component:Components)
    {
        if(!Component->ComponentHasTag(SwardTag)||Component->GetInstanceCount()==0)continue;
        if(Component->ComponentHasTag(ProxyTag)){if(Component->CastShadow)Component->SetCastShadow(false);continue;}
        // Full grass coverage is retained. Only tiny distant grass shadows are
        // omitted; tree/building shadows and nearby sward shadows stay enabled.
        // A small hysteresis avoids rebuilding shadow state at a hovering edge.
        const double Limit=Distance*(Component->CastShadow?1.1:1.);
        const bool Cast=Distance>0&&Component->Bounds.GetBox().ComputeSquaredDistanceToPoint(CameraPosition)<=Limit*Limit;
        if(bool(Component->CastShadow)!=Cast)Component->SetCastShadow(Cast);
    }
}
}
void ASeigeGameMode::CreateFoliage()
{
    if(!FApp::CanEverRender())return;
    const double Started=FPlatformTime::Seconds();
    if(Foliage)Foliage->Destroy();if(GroundCover){GroundCover->Destroy();GroundCover=nullptr;}SceneryStream.Reset();SceneryMeshReferences.Reset();
    auto* Ground=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(Ground);Ground->SetRootComponent(Root);Root->RegisterComponent();Foliage=Ground;
    const double Half=Sim.WorldHalfSize,FarDistance=Half*RenderScale*8;
    auto* BroadProxy=LoadObject<UStaticMesh>(nullptr,*BroadleafProxyAsset,nullptr,LOAD_NoWarn);
    auto* PineProxy=LoadObject<UStaticMesh>(nullptr,*ConiferProxyAsset,nullptr,LOAD_NoWarn);
    TMap<FString,UStaticMesh*> Meshes;
    TMap<FName,UInstancedStaticMeshComponent*> Sets;
    TMap<FName,TArray<FTransform>> Batches;
    for(const FString Kind:{TEXT("OakA"),TEXT("OakB"),TEXT("PineA"),TEXT("PineB"),TEXT("RockA"),TEXT("RockB")})
    {
        const FString Name=TEXT("SM_")+Kind,Path=NatureAssets.Contains(Kind)?NatureAssets[Kind]:FString::Printf(TEXT("/Game/Art/%s.%s"),*Name,*Name);
        if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn))Meshes.Add(Kind,Mesh);
    }
    auto EnsureSet=[&](const FString& Kind,int32 Band,bool Proxy)->UInstancedStaticMeshComponent*
    {
        const FName Key(*FString::Printf(TEXT("%s_%d_%s"),*Kind,Band,Proxy?TEXT("proxy"):TEXT("detail")));
        if(auto** Existing=Sets.Find(Key))return *Existing;
        const bool Tree=Kind.StartsWith(TEXT("Oak"))||Kind.StartsWith(TEXT("Pine"));
        auto* Source=Meshes.FindRef(Kind);auto* ProxyMesh=Kind.StartsWith(TEXT("Pine"))?PineProxy:BroadProxy;
        // Missing optional cooked proxy falls back to continuous source foliage,
        // never an invisible band. Configuration validation normally catches it.
        const bool HasProxy=Tree&&ProxyMesh;
        const double Cut=(ForestDetailDistanceMeters+ForestLodTransitionMeters*(Band+.5)/SceneryLodBands)*100.;
        auto* Set=VegetationSet(Ground,Root,Proxy?ProxyMesh:Source,Kind,false,0,Proxy?Cut:0,Tree?(Proxy?FarDistance:HasProxy?Cut:FarDistance):FarDistance);
        if(!Set)return nullptr;
        if(Proxy)
        {
            Set->ComponentTags.Add(ProxyTag);Set->SetCastShadow(NeighborForestShadows);
            Set->ComponentTags.Add(FName(*(TEXT("seige_source:")+Kind)));
            Set->SetAffectDistanceFieldLighting(false);Set->SetAffectDynamicIndirectLighting(false);
        }
        Sets.Add(Key,Set);return Set;
    };
    auto Add=[&](const FString& Kind,FVector2D P,double Size,double Yaw,int32 Band)
    {
        const bool Tree=Kind.StartsWith(TEXT("Oak"))||Kind.StartsWith(TEXT("Pine"));
        auto* Source=Meshes.FindRef(Kind);if(!Source)return;
        const FTransform Transform(FRotator(0,Yaw,0),RenderPosition(P),FVector(Size));
        const FName DetailKey(*FString::Printf(TEXT("%s_%d_detail"),*Kind,Band));
        if(EnsureSet(Kind,Band,false))Batches.FindOrAdd(DetailKey).Add(Transform);
        if(Tree)if(auto* Proxy=Kind.StartsWith(TEXT("Pine"))?PineProxy:BroadProxy)
        {
            if(EnsureSet(Kind,Band,true))
            {
                const FTransform Far=AlignProxyBounds(Transform,Source,Proxy);
                const FName ProxyKey(*FString::Printf(TEXT("%s_%d_proxy"),*Kind,Band));
                Batches.FindOrAdd(ProxyKey).Add(Far);
            }
        }
    };
    int32 Trees=0;
    // Every sector keeps the same full placement sequence and density at every
    // focus level. Only the representation changes with camera distance.
    for(int32 Sector=0;Sector<9;++Sector)
    {
        const FVector2D Offset=FVector2D(Sector%3-1,Sector/3-1)*Half*2;
        const FSeigeSimulation* Colony=Sector==4?&Sim:nullptr;
        if(!Colony)for(const auto& N:Neighbors)if(N.Index==Sector){Colony=&N.Sim;break;}
        TArray<FSceneryClearance> Areas;
        if(Colony&&(Sector!=4||!HidePendingHomeFoundation(*this)))
        {
            for(const auto& B:Colony->Buildings)if(B.Health>0&&(Observer||Sector==4||IsWorldVisible(B.Position+Offset)))
                if(const auto* D=Colony->Definition(B))Areas.Add({B.Position+Offset,D->ReservedFootprint,true});
            if(Observer||Sector==4)for(const auto& N:Colony->Nodes)Areas.Add({N.Position+Offset,90});
            AddRoadClearances(*this,*Colony,Offset,Sector,Areas);
        }
        FRandomStream R(2222+Sector*100003);
        auto Tree=[&](FVector2D P,int32 I)
        {
            const double Chance=R.FRand(),Size=R.FRandRange(.72,1.16),Yaw=R.FRandRange(0,360);
            if(IsSceneryClear(*this,P,210,Offset,Half,Areas)||Chance>WoodlandDensity(P)*.9)return;
            const FString Kind=I%9==0?(I%2?TEXT("PineA"):TEXT("PineB")):(I%2?TEXT("OakA"):TEXT("OakB"));
            const int32 Band=int32(GroundCellSeed(I,Sector)%SceneryLodBands);
            Add(Kind,P,Size,Yaw,Band);++Trees;
        };
        for(int32 I=0;I<ForestCandidates;++I)Tree(Offset+FVector2D(R.FRandRange(-Half,Half),R.FRandRange(-Half,Half)),I);
        for(int32 I=0;I<NearForestCandidates;++I)Tree(Offset+FVector2D(R.FRandRange(-8000,8000),R.FRandRange(-8000,8000)),I);
        // Resource-specific clusters must not reveal a hidden neighbor's nodes.
        if(Sector==DetailedSectorIndex()&&Colony&&(Observer||Sector==4))for(const auto& N:Colony->Nodes)for(int32 I=0;I<13;++I)
        {
            const FVector2D P=Offset+N.Position+FVector2D(R.FRandRange(-125,125),R.FRandRange(-125,125));
            const double Size=R.FRandRange(.35,.8),Yaw=R.FRandRange(0,360);
            if(IsSceneryClear(*this,P,30,Offset,Half,Areas))continue;
            Add(I%2?TEXT("RockA"):TEXT("RockB"),P,Size,Yaw,0);
        }
    }
    for(auto& Pair:Batches)if(auto** Set=Sets.Find(Pair.Key))(*Set)->AddInstances(Pair.Value,false,false,false);
    UE_LOG(LogTemp,Display,TEXT("SCENERY_FOREST_READY: %d stable trees across nine sectors, %d ISM batches, %.3f seconds; opaque distance proxies %s"),Trees,Sets.Num(),FPlatformTime::Seconds()-Started,BroadProxy&&PineProxy?TEXT("enabled"):TEXT("fallback"));
    CreateGroundCover();
}
void ASeigeGameMode::CreateGroundCover()
{
    if(!FApp::CanEverRender())return;
    if(!GroundCover)
    {
        GroundCover=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(GroundCover);GroundCover->SetRootComponent(Root);Root->RegisterComponent();
        SceneryStream=MakeShared<FSeigeSceneryStreamState>();SceneryStream->Started=FPlatformTime::Seconds();
    }
    if(!SceneryStream)SceneryStream=MakeShared<FSeigeSceneryStreamState>();
    auto& State=*SceneryStream;
    const double Started=FPlatformTime::Seconds(),Budget=GrassStreamBudgetMs*.001;
    // ISM bounds changes invalidate every instance in that primitive in the
    // renderer's scene-culling hierarchy. A world-wide group made a small cell
    // upload resubmit over a million instances. Bound each group to a 2x2-cell
    // (108m) page while retaining the existing per-species/per-LOD batching.
    auto InstancePageKey=[](FIntPoint Cell,const FString& Kind,int32 Band)
    {
        constexpr int32 CellsPerInstancePage=2;
        const int32 X=FMath::FloorToInt(double(Cell.X)/CellsPerInstancePage);
        const int32 Y=FMath::FloorToInt(double(Cell.Y)/CellsPerInstancePage);
        return FName(*FString::Printf(TEXT("%d:%d:%s_%d"),X,Y,*Kind,Band));
    };
    const FTransform CameraPose=CameraTransform(CameraViewZoom());
    const FVector CameraPosition=CameraPose.GetLocation();
    const FVector2D CameraXY=FVector2D(CameraPosition)/RenderScale,Focus=FVector2D(CameraCenter),Offset=DetailedSectorOffset();
    const double Half=Sim.WorldHalfSize,Radius=GrassStreamRadiusMeters*100/RenderScale;
    const double DetailDistance=(GrassDetailDistanceMeters+GrassLodTransitionMeters)*100;
    const double DetailPreload=DetailDistance+GroundTileSize*RenderScale*.5;
    const int32 RadiusCells=FMath::CeilToInt(Radius/GroundTileSize);
    const FIntPoint FocusCell(FMath::FloorToInt(Focus.X/GroundTileSize),FMath::FloorToInt(Focus.Y/GroundTileSize));
    const FIntPoint CameraCell(FMath::FloorToInt(CameraXY.X/GroundTileSize),FMath::FloorToInt(CameraXY.Y/GroundTileSize));
    auto CellDistanceSq=[](FIntPoint Cell,FVector2D P)
    {
        const FVector2D Min(Cell.X*GroundTileSize,Cell.Y*GroundTileSize),Max=Min+FVector2D(GroundTileSize);
        return FMath::Square(FMath::Max(FMath::Max(Min.X-P.X,P.X-Max.X),0.))+FMath::Square(FMath::Max(FMath::Max(Min.Y-P.Y,P.Y-Max.Y),0.));
    };
    auto WantsDetail=[&](FIntPoint Cell){return State.DetailCells.Contains(Cell);};
    if(State.FocusCell!=FocusCell||State.CameraCell!=CameraCell)
    {
        State.FocusCell=FocusCell;State.CameraCell=CameraCell;FoliageCenter=Focus;
        State.Wanted.Reset();TSet<FIntPoint> Wanted;
        for(const FIntPoint Center:{FocusCell,CameraCell})
            for(int32 Y=Center.Y-RadiusCells;Y<=Center.Y+RadiusCells;++Y)for(int32 X=Center.X-RadiusCells;X<=Center.X+RadiusCells;++X)
            {
                const FIntPoint Cell(X,Y);const FVector2D Origin(X*GroundTileSize,Y*GroundTileSize);
                if(Origin.X+GroundTileSize<Offset.X-Half||Origin.X>Offset.X+Half||Origin.Y+GroundTileSize<Offset.Y-Half||Origin.Y>Offset.Y+Half)continue;
                if(FMath::Min(CellDistanceSq(Cell,Focus),CellDistanceSq(Cell,CameraXY))<=Radius*Radius)Wanted.Add(Cell);
            }
        State.Wanted=Wanted.Array();State.PriorityDirty=true;
        // One-cell retention hysteresis prevents churn when a camera hovers on
        // a grid edge. Existing proxy cover remains while incoming cells fill.
        State.Retiring.Reset();
        const double RetainRadius=Radius+GroundTileSize;
        for(auto& Pair:State.Cells)
            if(!Wanted.Contains(Pair.Key)&&FMath::Min(CellDistanceSq(Pair.Key,Focus),CellDistanceSq(Pair.Key,CameraXY))>RetainRadius*RetainRadius)State.Retiring.Add(Pair.Key);
        if(State.PendingIndex!=MIN_int32&&!State.Committing&&!Wanted.Contains(State.PendingCell))
        {State.PendingIndex=MIN_int32;State.PendingTransforms.Reset();}
    }
    FSeigeSceneryView View;View.Position=CameraPosition;View.Forward=CameraPose.GetUnitAxis(EAxis::X);
    View.Right=CameraPose.GetUnitAxis(EAxis::Y);View.Up=CameraPose.GetUnitAxis(EAxis::Z);
    View.TanHalfHorizontal=FMath::Tan(FMath::DegreesToRadians(double(CompanionView?Sim.Companions.ViewFov:CameraFov)*.5));
    if(auto* PC=GetWorld()->GetFirstPlayerController()){int32 W=0,H=0;PC->GetViewportSize(W,H);if(W>0&&H>0)View.Aspect=double(W)/H;}
    auto BoundsForCell=[&](FIntPoint Cell)->FBox
    {
        if(const FBox* Existing=State.CellBounds.Find(Cell))return *Existing;
        const FVector2D Lo(Cell.X*GroundTileSize,Cell.Y*GroundTileSize),Hi=Lo+FVector2D(GroundTileSize);
        double MinHeight=DBL_MAX,MaxHeight=-DBL_MAX;
        // Bound the actual cached triangles, including their bordering vertices.
        // Sampling only cell centers can miss a nearby hillside or pad edge.
        if(TerrainTiles.IsValidIndex(DetailedSectorIndex())&&TerrainTiles[DetailedSectorIndex()].Resolution>0&&
            !TerrainTiles[DetailedSectorIndex()].Heights.IsEmpty())
        {
            const auto& Tile=TerrainTiles[DetailedSectorIndex()];const double Step=Half*2/Tile.Resolution;
            const FVector2D A=Lo-Tile.Offset,B=Hi-Tile.Offset;
            const int32 X0=FMath::Clamp(FMath::FloorToInt((A.X+Half)/Step),0,Tile.Resolution),X1=FMath::Clamp(FMath::CeilToInt((B.X+Half)/Step),0,Tile.Resolution);
            const int32 Y0=FMath::Clamp(FMath::FloorToInt((A.Y+Half)/Step),0,Tile.Resolution),Y1=FMath::Clamp(FMath::CeilToInt((B.Y+Half)/Step),0,Tile.Resolution);
            for(int32 Y=Y0;Y<=Y1;++Y)for(int32 X=X0;X<=X1;++X)
            {const double H=Tile.Heights[Y*(Tile.Resolution+1)+X];MinHeight=FMath::Min(MinHeight,H);MaxHeight=FMath::Max(MaxHeight,H);}
        }
        if(MinHeight==DBL_MAX)MinHeight=MaxHeight=GroundHeight((Lo+Hi)*.5);
        // The shipped sward meshes are below one meter before configured scale.
        // This pad is conservative for foliage tips, not a terrain-height change.
        const FBox Bounds(FVector(Lo.X*RenderScale,Lo.Y*RenderScale,MinHeight*RenderScale),
            FVector(Hi.X*RenderScale,Hi.Y*RenderScale,MaxHeight*RenderScale+GrassScaleMax*100));
        State.CellBounds.Add(Cell,Bounds);return Bounds;
    };
    if(State.PriorityDirty||FVector::DistSquared(State.PriorityPosition,CameraPosition)>FMath::Square(GroundTileSize*RenderScale*.125)||
        FVector::DotProduct(State.PriorityForward,View.Forward)<.995||
        State.PriorityAspect!=View.Aspect||State.PriorityTanHalf!=View.TanHalfHorizontal)
    {
        State.PriorityDirty=false;State.PriorityPosition=CameraPosition;State.PriorityForward=View.Forward;
        State.PriorityAspect=View.Aspect;State.PriorityTanHalf=View.TanHalfHorizontal;
        State.VisibleCells.Reset();State.NearCells.Reset();State.DetailCells.Reset();
        for(const FIntPoint Cell:State.Wanted)
        {
            const FBox Bounds=BoundsForCell(Cell);
            if(View.Intersects(Bounds))State.VisibleCells.Add(Cell);
            if(View.WithinDistance(Bounds,DetailDistance))State.NearCells.Add(Cell);
            if(View.WithinDistance(Bounds,DetailPreload))State.DetailCells.Add(Cell);
        }
        State.Wanted.Sort([&](const FIntPoint& A,const FIntPoint& B)
        {
            const bool AV=State.VisibleCells.Contains(A),BV=State.VisibleCells.Contains(B);if(AV!=BV)return AV;
            // The focus is the visible ground ahead, while camera-XY can be well
            // behind it in an overview. Prefetch in viewing order, not behind us.
            const double DA=AV?CellDistanceSq(A,Focus):CellDistanceSq(A,CameraXY);
            const double DB=BV?CellDistanceSq(B,Focus):CellDistanceSq(B,CameraXY);
            if(DA!=DB)return DA<DB;return A.Y==B.Y?A.X<B.X:A.Y<B.Y;
        });
    }
    // Retiring an entire strip in one frame caused the same hitch as building
    // it. Retire at most one cell here; removal is also inside the frame budget.
    if(!State.Retiring.IsEmpty())
    {
        const FIntPoint Cell=State.Retiring.Pop(EAllowShrinking::No);
        if(State.Committing&&Cell==State.PendingCell)State.Retiring.Insert(Cell,0);
        else if(auto* Record=State.Cells.Find(Cell))
        {
            for(auto& Pair:Record->Instances)if(auto* Set=State.Sets.FindRef(Pair.Key).Get())
            {
                Pair.Value.RemoveAll([&](FPrimitiveInstanceId Id){return !Set->IsValidId(Id);});
                Set->RemoveInstancesById(Pair.Value,false);
                if(Set->GetInstanceCount()==0)
                {
                    // Never keep empty page primitives or growing stale bounds
                    // behind a camera that travels across the sector.
                    State.Sets.Remove(Pair.Key);
                    GroundCover->RemoveInstanceComponent(Set);Set->DestroyComponent();
                }
            }
            State.Cells.Remove(Cell);
            State.CellBounds.Remove(Cell);
        }
    }
    const auto Areas=VisibleClearances(*this);
    if(State.Meshes.IsEmpty())
    {
        for(const FString Kind:{TEXT("Grass"),TEXT("GrassB"),TEXT("Wildflowers"),TEXT("Shrub"),TEXT("RockA"),TEXT("RockB")})
        {
            const FString Name=TEXT("SM_")+Kind,Path=NatureAssets.Contains(Kind)?NatureAssets[Kind]:FString::Printf(TEXT("/Game/Art/%s.%s"),*Name,*Name);
            auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn);State.Meshes.Add(Kind,Mesh);SceneryMeshReferences.Add(Kind,Mesh);
        }
        State.Meshes.Add(TEXT("GrassProxy"),LoadObject<UStaticMesh>(nullptr,*GrassProxyAsset,nullptr,LOAD_NoWarn));
        SceneryMeshReferences.Add(TEXT("GrassProxy"),State.Meshes.FindRef(TEXT("GrassProxy")).Get());
    }
    const bool HasProxy=State.Meshes.FindRef(TEXT("GrassProxy")).IsValid();
    auto EnsureSet=[&](const FString& Kind,int32 Band)->UInstancedStaticMeshComponent*
    {
        const FName Key=InstancePageKey(State.PendingCell,Kind,Band);
        if(auto* Existing=State.Sets.FindRef(Key).Get())return Existing;
        const bool Proxy=Kind.StartsWith(TEXT("GrassProxy")),Detail=Kind==TEXT("Grass")||Kind==TEXT("GrassB");
        const double Cut=(GrassDetailDistanceMeters+GrassLodTransitionMeters*(Band+.5)/SceneryLodBands)*100.;
        const double End=(Detail&&HasProxy)?Cut:Sim.WorldHalfSize*RenderScale*8;
        auto* Set=VegetationSet(GroundCover,GroundCover->GetRootComponent(),State.Meshes.FindRef(Proxy?TEXT("GrassProxy"):Kind).Get(),Kind,GrassDistanceFieldLighting,GrassProgrammableDistanceMeters,Proxy?Cut:0,End);
        if(!Set)return nullptr;
        // Cheap incremental bounds remain confined to this page. They cannot
        // grow across the whole sector as cells are inserted and retired.
        Set->SetUseConservativeBounds(true);
        Set->ComponentTags.Add(Key);
        if(Detail||Proxy||Kind==TEXT("Wildflowers"))Set->ComponentTags.Add(SwardTag);
        if(Proxy){Set->ComponentTags.Add(ProxyTag);Set->ComponentTags.Add(FName(Kind==TEXT("GrassProxyA")?TEXT("seige_source:Grass"):TEXT("seige_source:GrassB")));Set->SetCastShadow(false);Set->SetAffectDistanceFieldLighting(false);Set->SetAffectDynamicIndirectLighting(false);}
        State.Sets.Add(Key,Set);return Set;
    };
    constexpr int32 PropsPerCell=6500/GroundReferenceCells;
    const int32 GrassPerCell=GroundCoverCandidates/GroundReferenceCells;
    int32 Completed=0;
    while(Completed<GrassStreamCellsPerFrame&&FPlatformTime::Seconds()-Started<Budget)
    {
        if(State.PendingIndex==MIN_int32)
        {
            bool Found=false;int32 BestPriority=5;FIntPoint SelectedCell;
            // Detail replacements are preloaded before the camera reaches the
            // handoff band; proxy-only cells extend much farther into the view.
            for(const FIntPoint Cell:State.Wanted)
            {
                const auto* Record=State.Cells.Find(Cell);
                const int32 Priority=SeigeSceneryWorkPriority(!Record||!Record->BaseReady,!Record||!Record->DetailReady,
                    State.VisibleCells.Contains(Cell),State.NearCells.Contains(Cell),!HasProxy||WantsDetail(Cell));
                if(Priority<BestPriority){BestPriority=Priority;SelectedCell=Cell;Found=true;if(Priority==0)break;}
            }
            if(!Found)break;
            const auto* Record=State.Cells.Find(SelectedCell);State.PendingCell=SelectedCell;State.PendingBase=!Record||!Record->BaseReady;
            State.PendingDetail=!HasProxy||State.NearCells.Contains(SelectedCell)||(!State.PendingBase&&WantsDetail(SelectedCell));
            State.PendingIndex=-PropsPerCell;State.PendingTerrainSignature=TerrainPadSignature;
            State.Random.Initialize(static_cast<int32>(GroundCellSeed(SelectedCell.X,SelectedCell.Y)));State.PendingTransforms.Reset();
        }
        const FVector2D Origin(State.PendingCell.X*GroundTileSize,State.PendingCell.Y*GroundTileSize);
        auto Add=[&](const FString& Kind,int32 Band,const FTransform& Transform)
        {
            State.PendingTransforms.FindOrAdd(FName(*FString::Printf(TEXT("%s_%d"),*Kind,Band))).Add(Transform);
        };
        for(int32 Batch=0;Batch<128&&State.PendingIndex<GrassPerCell;++Batch,++State.PendingIndex)
        {
            const int32 I=State.PendingIndex;auto& R=State.Random;
            if(I<0)
            {
                const FVector2D P=Origin+FVector2D(R.FRandRange(0,GroundTileSize),R.FRandRange(0,GroundTileSize));const double Size=R.FRandRange(.4,1.1),Yaw=R.FRandRange(0,360);
                if(!State.PendingBase||IsSceneryClear(*this,P,30,Offset,Half,Areas))continue;
                const bool Outcrop=FMath::PerlinNoise2D(P/950+FVector2D(13,-8))>.24;const int32 Index=I+PropsPerCell;
                const FString Kind=Index%5==0&&Outcrop?(Index%2?TEXT("RockA"):TEXT("RockB")):TEXT("Shrub");
                Add(Kind,0,FTransform(GroundCoverRotation(*this,P,Yaw),RenderPosition(P),FVector(Size)));continue;
            }
            // Keep the same candidate count, random consumption, coherent density
            // and plot exclusions. Near/proxy regeneration uses identical strata.
            const FVector2D Jitter(R.FRand(),R.FRand());
            const FVector2D P=Origin+SeigeSwardCandidate(I,GrassPerCell,Jitter)*GroundTileSize;
            const double Chance=R.FRand(),Size=R.FRandRange(GrassScaleMin,GrassScaleMax),Yaw=R.FRandRange(0,360);
            const double Woodland=WoodlandDensity(P);const bool Flowers=I%35==0&&Woodland<.15&&FMath::PerlinNoise2D(P/700+FVector2D(4.2,18.6))>.02;
            const FString Kind=Flowers?TEXT("Wildflowers"):I%2?TEXT("Grass"):TEXT("GrassB");
            const UStaticMesh* Mesh=State.Meshes.FindRef(Kind).Get();const double Margin=Mesh?Mesh->GetBounds().SphereRadius*Size/RenderScale:15.;
            if(!Mesh||IsSceneryClear(*this,P,Margin,Offset,Half,Areas)||Chance<Woodland*.7||Chance>MeadowSwardDensity(P))continue;
            const FTransform Transform=GroundCoverContact(*this,FTransform(GroundCoverRotation(*this,P,Yaw),RenderPosition(P),FVector(Size)),Mesh);
            const int32 Band=int32(GroundCellSeed(I,State.PendingCell.X*31+State.PendingCell.Y)%SceneryLodBands);
            if(Flowers){if(State.PendingBase)Add(Kind,0,Transform);continue;}
            if(State.PendingDetail)Add(Kind,Band,Transform);
            if(State.PendingBase&&HasProxy)
            {
                auto* Proxy=State.Meshes.FindRef(TEXT("GrassProxy")).Get();
                // Matching full XYZ bounds keeps the Nanite center-point near
                // and far distance tests coincident even between grass variants.
                const FTransform Far=AlignProxyBounds(Transform,Mesh,Proxy);
                Add(Kind==TEXT("Grass")?TEXT("GrassProxyA"):TEXT("GrassProxyB"),Band,Far);
            }
        }
        if(State.PendingIndex>=GrassPerCell)
        {
            auto& Record=State.Cells.FindOrAdd(State.PendingCell);
            if(!State.Committing){State.PendingTransforms.GetKeys(State.CommitKeys);State.CommitKey=0;State.Committing=true;}
            const bool TerrainChanged=State.PendingTerrainSignature!=TerrainPadSignature;
            while(State.CommitKey<State.CommitKeys.Num()&&FPlatformTime::Seconds()-Started<Budget)
            {
                const FName Key=State.CommitKeys[State.CommitKey++];auto& Transforms=State.PendingTransforms[Key];
                FString Kind,BandText;Key.ToString().Split(TEXT("_"),&Kind,&BandText,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
                // A construction pad may change during multi-frame preparation.
                // Recheck the full source clump against current clearances,
                // including when its pivot is outside a newly built foundation.
                const bool Proxy=Kind.StartsWith(TEXT("GrassProxy"));
                const auto* Source=State.Meshes.FindRef(Proxy?(Kind==TEXT("GrassProxyA")?TEXT("Grass"):TEXT("GrassB")):Kind).Get();
                const auto* ProxyMesh=State.Meshes.FindRef(TEXT("GrassProxy")).Get();
                if(TerrainChanged)Transforms.RemoveAll([&](const FTransform& Transform)
                {
                    const FVector Pivot=Proxy?ProxyPivotOffset(Transform,Source,ProxyMesh):FVector::ZeroVector;
                    const FVector2D Logical=FVector2D(Transform.GetLocation()-Pivot)/RenderScale;
                    const FVector SourceScale=Proxy&&Source&&ProxyMesh?Transform.GetScale3D()*ProxyMesh->GetBounds().BoxExtent/Source->GetBounds().BoxExtent:Transform.GetScale3D();
                    const double Margin=Source?Source->GetBounds().SphereRadius*SourceScale.GetAbsMax()/RenderScale:30.;
                    return IsSceneryClear(*this,Logical,Margin,Offset,Half,Areas);
                });
                // Commit retained instances against the authoritative surface.
                if(TerrainChanged)for(auto& Transform:Transforms)
                {
                    const FVector Pivot=Proxy?ProxyPivotOffset(Transform,Source,ProxyMesh):FVector::ZeroVector;
                    FVector P=Transform.GetLocation()-Pivot;const FVector2D Logical=FVector2D(P)/RenderScale;
                    P.Z=GroundHeight(Logical)*RenderScale;
                    Transform.SetRotation(GroundCoverRotation(*this,Logical,Transform.Rotator().Yaw));
                    Transform.SetLocation(P+(Proxy?ProxyPivotOffset(Transform,Source,ProxyMesh):FVector::ZeroVector));
                    if(Proxy)
                    {
                        FTransform Near=Transform;Near.SetLocation(P);
                        Near.SetScale3D(Transform.GetScale3D()*ProxyMesh->GetBounds().BoxExtent/Source->GetBounds().BoxExtent);
                        Transform=AlignProxyBounds(GroundCoverContact(*this,Near,Source),Source,ProxyMesh);
                    }
                    else if(Kind==TEXT("Grass")||Kind==TEXT("GrassB")||Kind==TEXT("Wildflowers"))
                        Transform=GroundCoverContact(*this,Transform,Source);
                }
                if(!Transforms.IsEmpty())
                {
                    const int32 Band=FCString::Atoi(*BandText);
                    if(auto* Set=EnsureSet(Kind,Band))
                        Record.Instances.FindOrAdd(InstancePageKey(State.PendingCell,Kind,Band)).Append(Set->AddInstancesById(Transforms,false,false));
                }
            }
            if(State.CommitKey<State.CommitKeys.Num())break;
            Record.BaseReady|=State.PendingBase;Record.DetailReady|=State.PendingDetail;
            State.PendingIndex=MIN_int32;State.Committing=false;State.CommitKeys.Reset();State.PendingTransforms.Reset();++Completed;
        }
        if(FPlatformTime::Seconds()-Started>=Budget)break;
    }
    State.Remaining=0;State.RemainingVisible=0;State.RemainingNear=0;
    for(const FIntPoint Cell:State.Wanted)
    {
        const auto* Record=State.Cells.Find(Cell);
        const bool MissingBase=!Record||!Record->BaseReady,MissingDetail=!Record||!Record->DetailReady;
        if(MissingBase||(MissingDetail&&(!HasProxy||WantsDetail(Cell))))++State.Remaining;
        if(State.VisibleCells.Contains(Cell)&&(MissingBase||(MissingDetail&&State.NearCells.Contains(Cell))))++State.RemainingVisible;
        if(State.NearCells.Contains(Cell)&&(MissingBase||MissingDetail))++State.RemainingNear;
    }
    if(State.Remaining==0&&State.Started>0)
    {
        int32 Instances=0;for(const auto& Pair:State.Sets)if(auto* Set=Pair.Value.Get())Instances+=Set->GetInstanceCount();
        UE_LOG(LogTemp,Display,TEXT("SCENERY_STREAM_READY: %d cells, %d ISM groups, %d representations, %.3f seconds elapsed; no density reduction"),State.Cells.Num(),State.Sets.Num(),Instances,FPlatformTime::Seconds()-State.Started);State.Started=0;
    }
    UpdateGroundCoverShadows(*this,GroundCover);
}
bool ASeigeGameMode::IsSceneryStreamingReady() const
{
    return !FApp::CanEverRender()||RegionMapAlpha()>=1||(SceneryStream&&SceneryStream->Remaining==0&&SceneryStream->PendingIndex==MIN_int32&&SceneryStream->Retiring.IsEmpty());
}
int32 ASeigeGameMode::PendingSceneryCells() const
{
    return SceneryStream?SceneryStream->Remaining+SceneryStream->Retiring.Num():0;
}
int32 ASeigeGameMode::PendingVisibleSceneryCells() const
{return SceneryStream?SceneryStream->RemainingVisible:0;}
int32 ASeigeGameMode::PendingNearSceneryCells() const
{return SceneryStream?SceneryStream->RemainingNear:0;}
void ASeigeGameMode::RefreshEnvironment()
{
    if(!FApp::CanEverRender())return;const bool Map=RegionMapAlpha()>=1;
    if(!Map)
    {
        if(RenderedSector!=DetailedSectorIndex()){SelectedId=0;SelectedBuild.Empty();CreateLandscape();}
        else
        {
            const FString Before=TerrainPadSignature;
            RefreshBuildingPads();
            if(Before!=TerrainPadSignature)RefreshTransportScenery();
            CreateGroundCover();
        }
    }
    if(Landscape)Landscape->SetActorHiddenInGame(Map);if(Foliage)Foliage->SetActorHiddenInGame(Map);if(GroundCover)GroundCover->SetActorHiddenInGame(Map);
}
void ASeigeGameMode::RefreshTransportScenery()
{
    RefreshBuildingPads();
    TArray<FSceneryClearance> Roads;
    AddRoadClearances(*this,Sim,FVector2D::ZeroVector,4,Roads);
    for(const auto& N:Neighbors)AddRoadClearances(*this,N.Sim,N.Offset,N.Index,Roads);
    if(Roads.IsEmpty())return;
    auto OverlapsRoad=[&](const FTransform& Transform,const UStaticMesh* Mesh,const FSceneryClearance& Road)
    {
        if(!Mesh)return false;
        const FBox Box=Mesh->GetBoundingBox().TransformBy(Transform);
        const FVector2D P=FVector2D(Box.GetCenter())/RenderScale;
        const FVector2D Closest=ClosestRoadPoint(P,Road.Position,Road.End);
        if(Road.RequiresSight&&(!IsWorldVisible(P)||!IsWorldVisible(Closest)))return false;
        const double Margin=FVector2D(Box.GetExtent()).Length()/RenderScale;
        return (P-Closest).SquaredLength()<FMath::Square(Road.Radius+Margin);
    };
    for(AActor* Actor:{Foliage.Get(),GroundCover.Get()})if(Actor)
    {
        TArray<UInstancedStaticMeshComponent*> Components;Actor->GetComponents(Components);
        for(auto* Component:Components)
        {
            TSet<int32> Removed;
            for(const auto& Road:Roads)
            {
                const FVector2D Lo(FMath::Min(Road.Position.X,Road.End.X)-Road.Radius,FMath::Min(Road.Position.Y,Road.End.Y)-Road.Radius);
                const FVector2D Hi(FMath::Max(Road.Position.X,Road.End.X)+Road.Radius,FMath::Max(Road.Position.Y,Road.End.Y)+Road.Radius);
                const FBox Bounds(FVector(Lo.X*RenderScale,Lo.Y*RenderScale,-6000*RenderScale),FVector(Hi.X*RenderScale,Hi.Y*RenderScale,6000*RenderScale));
                for(int32 Index:Component->GetInstancesOverlappingBox(Bounds,true))
                {
                    FTransform Transform;
                    if(Component->GetInstanceTransform(Index,Transform,true)&&OverlapsRoad(Transform,Component->GetStaticMesh(),Road))Removed.Add(Index);
                }
            }
            if(Removed.IsEmpty())continue;
            if(Actor==GroundCover.Get()&&SceneryStream)
                for(auto& Cell:SceneryStream->Cells)for(auto& Pair:Cell.Value.Instances)
                    if(SceneryStream->Sets.FindRef(Pair.Key).Get()==Component)
                        Pair.Value.RemoveAll([&](FPrimitiveInstanceId Id){return !Component->IsValidId(Id)||Removed.Contains(Component->GetInstanceIndexForId(Id));});
            Component->RemoveInstances(Removed.Array());
        }
    }
    if(SceneryStream)for(auto& Pair:SceneryStream->PendingTransforms)
    {
        FString Kind,Band;Pair.Key.ToString().Split(TEXT("_"),&Kind,&Band,ESearchCase::CaseSensitive,ESearchDir::FromEnd);
        const UStaticMesh* Mesh=SceneryStream->Meshes.FindRef(Kind.StartsWith(TEXT("GrassProxy"))?TEXT("GrassProxy"):Kind).Get();
        Pair.Value.RemoveAll([&](const FTransform& Transform)
        {
            for(const auto& Road:Roads)if(OverlapsRoad(Transform,Mesh,Road))return true;
            return false;
        });
    }
}
void ASeigeGameMode::ClearSceneryAt(FVector2D Position,float Radius)
{
    const bool Home=FMath::Abs(Position.X)<=Sim.WorldHalfSize&&FMath::Abs(Position.Y)<=Sim.WorldHalfSize;
    if(!Observer&&!Home&&!IsWorldVisible(Position))return;
    for(AActor* Actor:{Foliage.Get(),GroundCover.Get()})if(Actor)
    {
        TArray<UInstancedStaticMeshComponent*> Components;Actor->GetComponents(Components);
        for(auto* Component:Components)
        {
            const FBox Bounds(FVector((Position.X-Radius)*RenderScale,(Position.Y-Radius)*RenderScale,-6000*RenderScale),FVector((Position.X+Radius)*RenderScale,(Position.Y+Radius)*RenderScale,6000*RenderScale));
            const auto Indices=Component->GetInstancesOverlappingBox(Bounds,true);
            if(!Indices.IsEmpty())
            {
                // Forget handles before removing indexed instances: the ISM ID
                // allocator may reuse freed IDs for a different streamed cell.
                if(Actor==GroundCover.Get()&&SceneryStream)
                {
                    TSet<int32> Removed;for(int32 Index:Indices)Removed.Add(Index);
                    for(auto& Cell:SceneryStream->Cells)for(auto& Pair:Cell.Value.Instances)
                        if(SceneryStream->Sets.FindRef(Pair.Key).Get()==Component)
                            Pair.Value.RemoveAll([&](FPrimitiveInstanceId Id){return !Component->IsValidId(Id)||Removed.Contains(Component->GetInstanceIndexForId(Id));});
                }
                Component->RemoveInstances(Indices);
            }
        }
    }
    if(SceneryStream)for(auto& Pair:SceneryStream->PendingTransforms)
        Pair.Value.RemoveAll([&](const FTransform& Transform)
        {
            const FVector2D Delta=FVector2D(Transform.GetLocation())/RenderScale-Position;
            return FMath::Abs(Delta.X)<=Radius&&FMath::Abs(Delta.Y)<=Radius;
        });
}
