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
double ASeigeGameMode::GroundHeight(FVector2D P) const
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
void ASeigeGameMode::CreateLandscape()
{
    if(!FApp::CanEverRender()) return;
    if(Landscape) Landscape->Destroy();
    auto* Ground=GetWorld()->SpawnActor<AActor>();
    auto* Root=NewObject<USceneComponent>(Ground); Ground->SetRootComponent(Root); Root->RegisterComponent(); Landscape=Ground;
    const double Half=Sim.WorldHalfSize,Extent=Half*3;
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
            Vertices.Add(FVector(P,GroundHeight(P))); Normals.Add(Normal); UVs.Add(P/700);
            double Dirt=FMath::Clamp((FMath::PerlinNoise2D(P/1450+FVector2D(14,3))-.08)*1.8,0.,.65);
            if(Offset.IsNearlyZero())
            {
                if(Screen!=TEXT("landing")) Dirt=FMath::Max(Dirt,1-FMath::SmoothStep(190.,470.,FVector2D::Distance(P,HomePosition())));
                for(const auto& N:Sim.Nodes) Dirt=FMath::Max(Dirt,1-FMath::SmoothStep(130.,270.,FVector2D::Distance(P,N.Position)));
            }
            const double Noise=FMath::PerlinNoise2D(P/1700+FVector2D(32,15));
            const double Rock=FMath::Clamp((1-Normal.Z)*8+(Noise-.25)*1.4,0.,.9);
            Colors.Add(FLinearColor(float(Dirt*(1-Rock)),float(Rock),0,1));
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
    for(int32 Y=-1;Y<=1;Y++) for(int32 X=-1;X<=1;X++) MakeTerrain(FVector2D(X,Y)*Half*2,(X==0&&Y==0)?256:80);
    TMap<FString,UHierarchicalInstancedStaticMeshComponent*> Sets;
    for(const FString Kind:{TEXT("OakA"),TEXT("OakB"),TEXT("PineA"),TEXT("PineB"),TEXT("Shrub"),TEXT("Grass"),TEXT("RockA"),TEXT("RockB")})
    {
        const FString Name=TEXT("SM_")+Kind;
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Art/%s.%s"),*Name,*Name),nullptr,LOAD_NoWarn);
        if(!Mesh) continue;
        auto* Instances=NewObject<UHierarchicalInstancedStaticMeshComponent>(Ground);
        Instances->bAutoRebuildTreeOnInstanceChanges=false; Instances->SetStaticMesh(Mesh); Instances->SetupAttachment(Root);
        // HISM's LOD-distance calculation assumes perspective; normalize it to our fixed
        // orthographic camera distance so zoom still selects useful foliage detail.
        Instances->SetLODDistanceScale(CameraOffset.Size());
        Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        // Forest silhouettes must survive sector/region zoom; small ground detail may cull.
        const bool IsTree=Kind.StartsWith(TEXT("Oak"))||Kind.StartsWith(TEXT("Pine"));
        Instances->SetCullDistances(IsTree?0:18000,IsTree?0:(Kind==TEXT("Grass")?23000:35000));
        if(Kind==TEXT("Grass")) Instances->SetCastShadow(false);
        Instances->RegisterComponent(); Ground->AddInstanceComponent(Instances); Sets.Add(Kind,Instances);
    }
    FRandomStream R(2222);
    auto Add=[&](const FString& Kind,FVector2D P,double Scale,double Rotation)
    {
        if(auto** Set=Sets.Find(Kind)) (*Set)->AddInstance(FTransform(FRotator(0,Rotation,0),FVector(P,GroundHeight(P)),FVector(Scale)));
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
        if(Clear(P,2200)) return;
        const double Patch=FMath::PerlinNoise2D(P/3500+FVector2D(4,19));
        if(Patch<-.08&&R.FRand()>.08) return;
        const FString Kind=NaturalHeight(P)>100?(I%2?TEXT("PineA"):TEXT("PineB")):(I%2?TEXT("OakA"):TEXT("OakB"));
        Add(Kind,P,R.FRandRange(.65,1.35),R.FRandRange(0,360));
    };
    for(int32 I=0;I<26000;I++) Tree(FVector2D(R.FRandRange(-Extent,Extent),R.FRandRange(-Extent,Extent)),I);
    for(int32 I=0;I<2400;I++) Tree(HomePosition()+FVector2D(R.FRandRange(-8000,8000),R.FRandRange(-8000,8000)),I);
    for(int32 I=0;I<6500;I++)
    {
        const FVector2D P=HomePosition()+FVector2D(R.FRandRange(-11000,11000),R.FRandRange(-11000,11000));
        if(Clear(P,650)) continue;
        Add(I%5==0?(I%2?TEXT("RockA"):TEXT("RockB")):TEXT("Shrub"),P,R.FRandRange(.4,1.1),R.FRandRange(0,360));
    }
    for(int32 I=0;I<26000;I++)
    {
        const FVector2D P=HomePosition()+FVector2D(R.FRandRange(-7500,7500),R.FRandRange(-7500,7500));
        if(Clear(P,400)) continue;
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
        const auto Indices=Component->GetInstancesOverlappingSphere(FVector(Position,GroundHeight(Position)+40),Radius,true);
        if(!Indices.IsEmpty()) Component->RemoveInstances(Indices);
    }
}
