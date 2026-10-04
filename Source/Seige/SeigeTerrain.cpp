#include "SeigeGameMode.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"

namespace
{
double NaturalHeight(FVector2D P)
{
    return FMath::PerlinNoise2D(P/13000+FVector2D(17.8,-8.1))*1500+
        FMath::PerlinNoise2D(P/2600+FVector2D(-4.6,25.4))*180+
        FMath::PerlinNoise2D(P/410+FVector2D(41.2,12.5))*12;
}
}
double ASeigeGameMode::TerrainHeight(FVector2D P) const
{
    // The terrain is presentation; all nine colonies share planar simulation rules.
    double Height=NaturalHeight(P);
    const FSeigeSimulation* Sector=&Sim;
    FVector2D Offset=FVector2D::ZeroVector;
    const int32 X=FMath::RoundToInt(P.X/(Sim.WorldHalfSize*2)),Y=FMath::RoundToInt(P.Y/(Sim.WorldHalfSize*2));
    if(X!=0||Y!=0)
    {
        Offset=FVector2D(X,Y)*Sim.WorldHalfSize*2;
        const int32 Index=(Y+1)*3+(X+1);
        for(const auto& N:Neighbors) if(N.Index==Index) { Sector=&N.Sim; break; }
    }
    const FVector2D Local=P-Offset;
    if(!(Screen==TEXT("landing")&&X==0&&Y==0))
    {
        for(const auto& B:Sector->Buildings) if(const auto* D=Sector->Definition(B)) if(D->Role==TEXT("core"))
        {
            const double Distance=FVector2D::Distance(Local,B.Position);
            if(Distance<1400) Height=FMath::Lerp(NaturalHeight(B.Position+Offset),Height,FMath::SmoothStep(400.,1400.,Distance));
            break;
        }
    }
    for(const auto& N:Sector->Nodes)
    {
        const double Distance=FVector2D::Distance(Local,N.Position);
        if(Distance<470) Height=FMath::Lerp(NaturalHeight(N.Position+Offset),Height,FMath::SmoothStep(140.,470.,Distance));
    }
    return Height;
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
    const double Half=Sim.WorldHalfSize,CoarseStep=Half*2/128;
    TerrainTiles.Reset();
    for(int32 Y=-1;Y<=1;++Y)for(int32 X=-1;X<=1;++X)
    {
        FSeigeTerrainTile Tile;Tile.Offset=FVector2D(X,Y)*Half*2;Tile.Resolution=(X==0&&Y==0)?256:128;
        const double Step=Half*2/Tile.Resolution;
        for(int32 V=0;V<=Tile.Resolution;++V)for(int32 U=0;U<=Tile.Resolution;++U)
        {
            const FVector2D P=Tile.Offset+FVector2D(-Half+U*Step,-Half+V*Step);
            double Height=TerrainHeight(P);
            // All shared edges follow the same coarse vertices, including the finer
            // center tile. Cache and rendered triangles therefore share one surface.
            if(U==0||U==Tile.Resolution)
            {const double At=FMath::FloorToDouble(P.Y/CoarseStep)*CoarseStep;Height=FMath::Lerp(TerrainHeight(FVector2D(P.X,At)),TerrainHeight(FVector2D(P.X,At+CoarseStep)),(P.Y-At)/CoarseStep);}
            else if(V==0||V==Tile.Resolution)
            {const double At=FMath::FloorToDouble(P.X/CoarseStep)*CoarseStep;Height=FMath::Lerp(TerrainHeight(FVector2D(At,P.Y)),TerrainHeight(FVector2D(At+CoarseStep,P.Y)),(P.X-At)/CoarseStep);}
            Tile.Heights.Add(Height);
        }
        TerrainTiles.Add(MoveTemp(Tile));
    }
}
void ASeigeGameMode::CreateLandscape()
{
    if(!FApp::CanEverRender()) return;
    if(Landscape) Landscape->Destroy();
    auto* Ground=GetWorld()->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(Ground); Ground->SetRootComponent(Root); Root->RegisterComponent(); Landscape=Ground;
    const double Half=Sim.WorldHalfSize,Extent=Half*3;
    RebuildTerrainHeights();
    auto* TerrainMaterial=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/M_Terrain.M_Terrain"),nullptr,LOAD_NoWarn);
    auto MakeTerrain=[&](FVector2D Offset,int32 Resolution)
    {
        auto* Terrain=NewObject<UProceduralMeshComponent>(Ground);
        Terrain->SetupAttachment(Root); Terrain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Terrain->RegisterComponent(); Ground->AddInstanceComponent(Terrain);
        const double Step=Half*2/Resolution;
        TArray<FVector> Vertices,Normals; TArray<int32> Triangles; TArray<FVector2D> UVs;
        TArray<FLinearColor> Colors; TArray<FProcMeshTangent> Tangents;
        Vertices.Reserve((Resolution+1)*(Resolution+1)); Triangles.Reserve(Resolution*Resolution*6);
        for(int32 Y=0;Y<=Resolution;Y++) for(int32 X=0;X<=Resolution;X++)
        {
            const FVector2D P=Offset+FVector2D(-Half+X*Step,-Half+Y*Step);
            const double DX=GroundHeight(P+FVector2D(15,0))-GroundHeight(P-FVector2D(15,0)),DY=GroundHeight(P+FVector2D(0,15))-GroundHeight(P-FVector2D(0,15));
            const FVector Normal=FVector(-DX,-DY,30).GetSafeNormal();
            Vertices.Add(RenderPosition(P)); Normals.Add(Normal); UVs.Add(P*RenderScale/700);
            double Dirt=FMath::Clamp((FMath::PerlinNoise2D(P/1450+FVector2D(14,3))-.08)*1.8,0.,.65);
            if(Offset.IsNearlyZero())
            {
                if(Screen!=TEXT("landing")) Dirt=FMath::Max(Dirt,1-FMath::SmoothStep(190.,470.,FVector2D::Distance(P,HomePosition())));
                for(const auto& N:Sim.Nodes) Dirt=FMath::Max(Dirt,1-FMath::SmoothStep(130.,270.,FVector2D::Distance(P,N.Position)));
            }
            const double Noise=FMath::PerlinNoise2D(P/1700+FVector2D(32,15));
            const double Rock=FMath::Clamp((1-Normal.Z)*8+(Noise-.25)*1.4,0.,.9);
            const double Woodland=FMath::SmoothStep(-.1f,.25f,FMath::PerlinNoise2D(P/3500+FVector2D(4,19)));
            Colors.Add(FLinearColor(float(Dirt*(1-Rock)),float(Rock),float(Woodland),1));
            Tangents.Add(FProcMeshTangent(FVector(30,0,DX).GetSafeNormal(),false));
            if(X<Resolution&&Y<Resolution)
            {
                const int32 I=Y*(Resolution+1)+X;
                Triangles.Append({I,I+Resolution+1,I+1,I+1,I+Resolution+1,I+Resolution+2});
            }
        }
        Terrain->CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UVs,Colors,Tangents,false);
        Terrain->SetMaterial(0,TerrainMaterial?TerrainMaterial:Material(FLinearColor(.18f,.24f,.11f)));
    };
    for(int32 Y=-1;Y<=1;Y++) for(int32 X=-1;X<=1;X++) MakeTerrain(FVector2D(X,Y)*Half*2,(X==0&&Y==0)?256:128);
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    for(const FString Kind:{TEXT("OakA"),TEXT("OakB"),TEXT("PineA"),TEXT("PineB"),TEXT("Shrub"),TEXT("Grass"),TEXT("RockA"),TEXT("RockB")})
    {
        const FString Name=TEXT("SM_")+Kind;
        const FString Path=NatureAssets.Contains(Kind)?NatureAssets[Kind]:FString::Printf(TEXT("/Game/Art/%s.%s"),*Name,*Name);
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path,nullptr,LOAD_NoWarn);
        if(!Mesh) continue;
        auto* Instances=NewObject<UHierarchicalInstancedStaticMeshComponent>(Ground);
        Instances->bAutoRebuildTreeOnInstanceChanges=false; Instances->SetStaticMesh(Mesh); Instances->SetupAttachment(Root);
        Instances->SetLODDistanceScale(1);
        Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // Forest silhouettes must survive sector/region zoom; small ground detail may cull.
        const bool IsTree=Kind.StartsWith(TEXT("Oak"))||Kind.StartsWith(TEXT("Pine"));
        Instances->SetCullDistances(IsTree?0:18000,IsTree?0:(Kind==TEXT("Grass")?35000:60000));
        if(Kind==TEXT("Grass")) Instances->SetCastShadow(false);
        Instances->RegisterComponent(); Ground->AddInstanceComponent(Instances); Sets.Add(Kind,Instances);
    }
    FRandomStream R(2222);
    auto Add=[&](const FString& Kind,FVector2D P,double Scale,double Rotation)
    {
        if(auto** Set=Sets.Find(Kind)) (*Set)->AddInstance(FTransform(FRotator(0,Rotation,0),RenderPosition(P),FVector(Scale)));
    };
    auto Clear=[&](FVector2D P,double Radius)
    {
        if(FVector2D::Distance(P,HomePosition())<Radius) return true;
        for(const auto& N:Sim.Nodes) if(FVector2D::Distance(P,N.Position)<360) return true;
        for(const auto& N:Neighbors) for(const auto& B:N.Sim.Buildings) if(FVector2D::Distance(P,B.Position+N.Offset)<Radius) return true;
        return false;
    };
    auto Tree=[&](FVector2D P,int32 I)
    {
        if(Clear(P,650)) return;
        const double Patch=FMath::PerlinNoise2D(P/3500+FVector2D(4,19));
        if(Patch<-.08&&R.FRand()>.08) return;
        const FString Kind=NaturalHeight(P)>100?(I%2?TEXT("PineA"):TEXT("PineB")):(I%2?TEXT("OakA"):TEXT("OakB"));
        Add(Kind,P,R.FRandRange(.65,1.35),R.FRandRange(0,360));
    };
    for(int32 I=0;I<ForestCandidates;I++) Tree(FVector2D(R.FRandRange(-Extent,Extent),R.FRandRange(-Extent,Extent)),I);
    for(int32 I=0;I<NearForestCandidates;I++) Tree(HomePosition()+FVector2D(R.FRandRange(-5000,5000),R.FRandRange(-5000,5000)),I);
    for(int32 I=0;I<6500;I++)
    {
        const FVector2D P=HomePosition()+FVector2D(R.FRandRange(-3500,3500),R.FRandRange(-3500,3500));
        if(Clear(P,260)) continue;
        // Boulders form outcrops; a uniformly scattered rock field looks planted.
        const bool Outcrop=FMath::PerlinNoise2D(P/950+FVector2D(13,-8))>.24;
        Add(I%5==0&&Outcrop?(I%2?TEXT("RockA"):TEXT("RockB")):TEXT("Shrub"),P,R.FRandRange(.4,1.1),R.FRandRange(0,360));
    }
    for(int32 I=0;I<85000;I++)
    {
        const FVector2D P=HomePosition()+FVector2D(R.FRandRange(-2200,2200),R.FRandRange(-2200,2200));
        if(Clear(P,230)) continue;
        Add(TEXT("Grass"),P,R.FRandRange(.5,1.1),R.FRandRange(0,360));
    }
    for(const auto& N:Sim.Nodes) for(int32 I=0;I<13;I++)
    {
        const FVector2D P=N.Position+FVector2D(R.FRandRange(-125,125),R.FRandRange(-125,125));
        Add(I%2?TEXT("RockA"):TEXT("RockB"),P,R.FRandRange(.35,.8),R.FRandRange(0,360));
    }
    for(auto& Pair:Sets) { Pair.Value->BuildTreeIfOutdated(false,true); Pair.Value->bAutoRebuildTreeOnInstanceChanges=true; }
    for(const auto& B:Sim.Buildings) if(B.Health>0) if(const auto* D=Sim.Definition(B)) ClearSceneryAt(B.Position,D->Footprint);
    for(const auto& N:Neighbors) for(const auto& B:N.Sim.Buildings) if(B.Health>0) if(const auto* D=N.Sim.Definition(B)) ClearSceneryAt(B.Position+N.Offset,D->Footprint);
}
void ASeigeGameMode::ClearSceneryAt(FVector2D Position,float Radius)
{
    if(!Landscape) return;
    TArray<UHierarchicalInstancedStaticMeshComponent*> Components; Landscape->GetComponents(Components);
    for(auto* Component:Components)
    {
        const auto Indices=Component->GetInstancesOverlappingSphere(RenderPosition(Position,40),Radius*RenderScale,true);
        if(!Indices.IsEmpty()) Component->RemoveInstances(Indices);
    }
}
