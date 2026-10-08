#include "SeigeGameMode.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"

void ASeigeGameMode::CreateEnvironmentWater()
{
    if(!Landscape||!Sim.Environment.Enabled)return;
    const auto& Profile=Sim.Environment;
    auto* Surface=NewObject<UProceduralMeshComponent>(Landscape);
    Surface->SetupAttachment(Landscape->GetRootComponent());Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Surface->SetCastShadow(false);Surface->ComponentTags={FName(TEXT("seige_water"))};Surface->RegisterComponent();Landscape->AddInstanceComponent(Surface);
    TArray<FVector> V,N;TArray<FVector2D> UV;TArray<int32> T;TArray<FLinearColor> Colors;TArray<FProcMeshTangent> Tangents;
    auto Vertex=[&](FVector2D P,double H,float Shore)
    {const int32 Index=V.Add(FVector(P.X*RenderScale,P.Y*RenderScale,(H+.5)*RenderScale));N.Add(FVector::UpVector);UV.Add(P*RenderScale/1000);Colors.Add(FLinearColor(Shore,Shore,Shore,1));Tangents.Add(FProcMeshTangent(1,0,0));return Index;};
    // Water elevations come from the exact same downstream profile as the bed.
    // Mitered joins avoid gaps or crossing ribbons at bends.
    TArray<FVector2D> Sides;
    for(int32 I=0;I<Profile.River.Num();++I)
    {
        const FVector2D Before=(Profile.River[I].Position-Profile.River[FMath::Max(0,I-1)].Position).GetSafeNormal();
        const FVector2D After=(Profile.River[FMath::Min(I+1,Profile.River.Num()-1)].Position-Profile.River[I].Position).GetSafeNormal();
        const FVector2D Direction=(Before+After).GetSafeNormal();Sides.Add(FVector2D(-Direction.Y,Direction.X)*Profile.RiverHalfWidth*.97);
    }
    for(int32 I=1;I<Profile.River.Num();++I)
    {
        const auto& A=Profile.River[I-1];const auto& B=Profile.River[I];
        const int32 Steps=FMath::Max(1,FMath::CeilToInt((B.Position-A.Position).Size()/160));
        for(int32 J=0;J<Steps;++J)
        {
            const double U=double(J)/Steps,W=double(J+1)/Steps;const FVector2D P=FMath::Lerp(A.Position,B.Position,U),Q=FMath::Lerp(A.Position,B.Position,W);
            const FVector2D S=FMath::Lerp(Sides[I-1],Sides[I],U),R=FMath::Lerp(Sides[I-1],Sides[I],W);
            const double H=FMath::Lerp(A.Height,B.Height,U),K=FMath::Lerp(A.Height,B.Height,W);
            // The lake owns its flat water face; omit covered river strips to
            // avoid coplanar flicker without introducing separate pond levels.
            bool Lake=false;for(const auto& L:Profile.Lakes)if(((((P+Q)*.5-L.Center)/L.Radii).Size()<.96)){Lake=true;break;}if(Lake)continue;
            const int32 A0=Vertex(P-S,H,0),A1=Vertex(P+S,H,0),B0=Vertex(Q-R,K,0),B1=Vertex(Q+R,K,0);T.Append({A0,B0,A1,A1,B0,B1});
        }
    }
    // Match the round endpoint caps used by the physical water-distance query.
    // Without these a dry-looking sliver at the source was still unroutable.
    if(Profile.River.Num()>1)for(int32 End:{0,Profile.River.Num()-1})
    {
        const auto& Point=Profile.River[End];const int32 Neighbor=End==0?1:End-1;
        const FVector2D Out=(Point.Position-Profile.River[Neighbor].Position).GetSafeNormal();
        const double Angle=FMath::Atan2(Out.Y,Out.X);const int32 Center=Vertex(Point.Position,Point.Height,1);constexpr int32 Segments=24;
        for(int32 I=0;I<=Segments;++I){const double A=Angle-PI*.5+PI*I/Segments;Vertex(Point.Position+FVector2D(FMath::Cos(A),FMath::Sin(A))*Profile.RiverHalfWidth*.97,Point.Height,0);}
        for(int32 I=0;I<Segments;++I)T.Append({Center,Center+I+2,Center+I+1});
    }
    for(const auto& L:Profile.Lakes)
    {
        const int32 Center=Vertex(L.Center,L.Height,1);constexpr int32 Segments=192;
        for(int32 I=0;I<=Segments;++I){const double A=2*PI*I/Segments;Vertex(L.Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*L.Radii*.995,L.Height,0);}
        for(int32 I=0;I<Segments;++I)T.Append({Center,Center+I+2,Center+I+1});
    }
    Surface->CreateMeshSection_LinearColor(0,V,T,N,UV,Colors,Tangents,false);
    auto* Water=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/EnvironmentV09/M_WaterV09.M_WaterV09"),nullptr,LOAD_NoWarn);
    Surface->SetMaterial(0,Water?Water:Material(FLinearColor(.025f,.10f,.12f)));
}
