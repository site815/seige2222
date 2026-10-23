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
    auto Vertex=[&](FVector2D P,double H,float Shore,bool RiverVertex=false)
    {
        if(RiverVertex){double Distance=0,MappedSurface=0;if(Profile.RiverProfileAt(P,Profile.RiverHalfWidth*1.1,Distance,MappedSurface))H=MappedSurface;}
        const int32 Index=V.Add(FVector(P.X*RenderScale,P.Y*RenderScale,(H+.5)*RenderScale));N.Add(FVector::UpVector);UV.Add(P*RenderScale/1000);
        const float Shallow=FMath::Clamp(float(1-Profile.RiverDepthAt(P)/Profile.RiverDepth),0.f,1.f);
        Colors.Add(FLinearColor(Shore,Shallow,0,1));Tangents.Add(FProcMeshTangent(1,0,0));return Index;
    };
    const double WorldLimit=Sim.WorldHalfSize*3;
    auto Face=[&](TArray<FVector,TInlineAllocator<8>> Polygon)
    {
        // Physical river profiles continue beyond this neighborhood. Clip the
        // visible surface to its terrain rectangle, rather than drawing floating
        // ribbons and round source caps beyond the edge of the world mesh.
        for(int32 Axis:{0,1})for(double Sign:{-1.,1.})
        {
            TArray<FVector,TInlineAllocator<8>> Clipped;if(Polygon.IsEmpty())return;
            FVector Previous=Polygon.Last();double PreviousDistance=WorldLimit-Sign*Previous[Axis];
            for(const FVector& Current:Polygon)
            {
                const double Distance=WorldLimit-Sign*Current[Axis];
                if((Distance>=0)!=(PreviousDistance>=0))Clipped.Add(FMath::Lerp(Previous,Current,PreviousDistance/(PreviousDistance-Distance)));
                if(Distance>=0)Clipped.Add(Current);Previous=Current;PreviousDistance=Distance;
            }
            Polygon=MoveTemp(Clipped);
        }
        if(Polygon.Num()<3)return;const int32 First=V.Num();
        for(const auto& P:Polygon)Vertex(FVector2D(P.X,P.Y),P.Z,0,true);
        for(int32 I=1;I+1<Polygon.Num();++I)T.Append({First,First+I,First+I+1});
    };
    // Water elevations come from the exact same downstream profile as the bed.
    // Mitered joins avoid gaps or crossing ribbons at bends.
    Profile.ForEachRiver([&](const TArray<FSeigeRiverPoint>& River)
    {
    TArray<FVector2D> Sides;
    for(int32 I=0;I<River.Num();++I)
    {
        const FVector2D Before=(River[I].Position-River[FMath::Max(0,I-1)].Position).GetSafeNormal();
        const FVector2D After=(River[FMath::Min(I+1,River.Num()-1)].Position-River[I].Position).GetSafeNormal();
        const FVector2D Direction=(Before+After).GetSafeNormal();Sides.Add(FVector2D(-Direction.Y,Direction.X)*Profile.RiverHalfWidth*.97);
    }
    for(int32 I=1;I<River.Num();++I)
    {
        const auto& A=River[I-1];const auto& B=River[I];
        const int32 Steps=FMath::Max(1,FMath::CeilToInt((B.Position-A.Position).Size()/160));
        for(int32 J=0;J<Steps;++J)
        {
            const double U=double(J)/Steps,W=double(J+1)/Steps;const FVector2D P=FMath::Lerp(A.Position,B.Position,U),Q=FMath::Lerp(A.Position,B.Position,W);
            const FVector2D S=FMath::Lerp(Sides[I-1],Sides[I],U),R=FMath::Lerp(Sides[I-1],Sides[I],W);
            const double H=FMath::Lerp(A.Height,B.Height,U),K=FMath::Lerp(A.Height,B.Height,W);
            // The lake owns its flat water face; omit covered river strips to
            // avoid coplanar flicker without introducing separate pond levels.
            bool Lake=false;for(const auto& L:Profile.Lakes)if(L.RadiusAt((P+Q)*.5)<.96){Lake=true;break;}if(Lake)continue;
            Face({FVector(P-S,H),FVector(Q-R,K),FVector(Q+R,K),FVector(P+S,H)});
        }
    }
    // Match the round endpoint caps used by the physical water-distance query.
    // Without these a dry-looking sliver at the source was still unroutable.
    if(River.Num()>1)for(int32 End:{0,River.Num()-1})
    {
        const auto& Point=River[End];const int32 Neighbor=End==0?1:End-1;
        if(FMath::Max(FMath::Abs(Point.Position.X),FMath::Abs(Point.Position.Y))>WorldLimit+Profile.RiverHalfWidth)continue;
        const FVector2D Out=(Point.Position-River[Neighbor].Position).GetSafeNormal();
        const double Angle=FMath::Atan2(Out.Y,Out.X);const int32 Center=Vertex(Point.Position,Point.Height,1,true);constexpr int32 Segments=24;
        for(int32 I=0;I<=Segments;++I){const double A=Angle-PI*.5+PI*I/Segments;Vertex(Point.Position+FVector2D(FMath::Cos(A),FMath::Sin(A))*Profile.RiverHalfWidth*.97,Point.Height,0,true);}
        for(int32 I=0;I<Segments;++I)T.Append({Center,Center+I+2,Center+I+1});
    }
    });
    for(const auto& L:Profile.Lakes)
    {
        const int32 Center=Vertex(L.Center,L.Height,1);const int32 Segments=L.Shoreline.Num();
        for(int32 I=0;I<=Segments;++I)Vertex(L.Center+L.Shoreline[I%Segments]*L.Radii*.998,L.Height,0);
        for(int32 I=0;I<Segments;++I)T.Append({Center,Center+I+2,Center+I+1});
    }
    Surface->CreateMeshSection_LinearColor(0,V,T,N,UV,Colors,Tangents,false);
    auto* Water=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Art/EnvironmentV09/M_WaterV09.M_WaterV09"),nullptr,LOAD_NoWarn);
    Surface->SetMaterial(0,Water?Water:Material(FLinearColor(.025f,.10f,.12f)));
}
