#include "SeigeEnvironment.h"
#include "SeigeCanonicalJson.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"

namespace
{
double PointSegment(FVector2D P,FVector2D A,FVector2D B,double* Along=nullptr)
{const FVector2D D=B-A;const double T=D.SizeSquared()>1.e-12?FMath::Clamp(FVector2D::DotProduct(P-A,D)/D.SizeSquared(),0.,1.):0;if(Along)*Along=T;return FVector2D::Distance(P,A+D*T);}
double Cross(FVector2D A,FVector2D B){return A.X*B.Y-A.Y*B.X;}
bool BoundsNear(FVector2D A,FVector2D B,FVector2D C,FVector2D D,double Margin)
{return FMath::Max(A.X,B.X)+Margin>=FMath::Min(C.X,D.X)&&FMath::Min(A.X,B.X)-Margin<=FMath::Max(C.X,D.X)&&FMath::Max(A.Y,B.Y)+Margin>=FMath::Min(C.Y,D.Y)&&FMath::Min(A.Y,B.Y)-Margin<=FMath::Max(C.Y,D.Y);}
void SmoothDrainage(TArray<FSeigeRiverPoint>& River,const TArray<FSeigeFord>& Fords,double HalfWidth)
{
    // Keep authored lake levels and crossing centres as interpolation anchors.
    // A bounded Hermite polyline is prepared once, then shared by rendering and
    // physics. Generated points never multiply the route graph's waypoint count.
    TArray<FSeigeRiverPoint> Anchors;
    for(int32 I=1;I<River.Num();++I)
    {
        const auto& A=River[I-1];const auto& B=River[I];Anchors.Add(A);
        TArray<TPair<double,FSeigeRiverPoint>> Crossings;
        for(const auto& F:Fords)
        {
            double Along=0;if(PointSegment(F.Center,A.Position,B.Position,&Along)<HalfWidth*.2&&Along>.001&&Along<.999)
            {FSeigeRiverPoint Point;Point.Position=F.Center;Point.Height=FMath::Lerp(A.Height,B.Height,Along);Crossings.Emplace(Along,Point);}
        }
        Crossings.Sort([](const auto& A,const auto& B){return A.Key<B.Key;});for(const auto& F:Crossings)Anchors.Add(F.Value);
    }
    Anchors.Add(River.Last());TArray<FSeigeRiverPoint> Curve;
    for(int32 I=1;I<Anchors.Num();++I)
    {
        const auto& A=Anchors[I-1];const auto& B=Anchors[I];const auto& Before=Anchors[FMath::Max(0,I-2)];const auto& After=Anchors[FMath::Min(Anchors.Num()-1,I+1)];
        const double Length=(B.Position-A.Position).Size();
        const FVector2D M0=(B.Position-Before.Position)*(Length/FMath::Max(Length+(A.Position-Before.Position).Size(),1.));
        const FVector2D M1=(After.Position-A.Position)*(Length/FMath::Max(Length+(After.Position-B.Position).Size(),1.));
        const int32 Steps=FMath::Clamp(FMath::CeilToInt(Length/3500.),2,8);Curve.Add(A);
        for(int32 J=1;J<Steps;++J)
        {
            const double T=double(J)/Steps,T2=T*T,T3=T2*T;
            Curve.Add({A.Position*(2*T3-3*T2+1)+M0*(T3-2*T2+T)+B.Position*(-2*T3+3*T2)+M1*(T3-T2),FMath::Lerp(A.Height,B.Height,T),false});
        }
    }
    Curve.Add(Anchors.Last());River=MoveTemp(Curve);
}
double SegmentDistance(FVector2D A,FVector2D B,FVector2D C,FVector2D D)
{
    const double Den=Cross(B-A,D-C);
    if(FMath::Abs(Den)>1.e-12){const double T=Cross(C-A,D-C)/Den,U=Cross(C-A,B-A)/Den;if(T>=0&&T<=1&&U>=0&&U<=1)return 0;}
    return FMath::Min(FMath::Min(PointSegment(A,C,D),PointSegment(B,C,D)),FMath::Min(PointSegment(C,A,B),PointSegment(D,A,B)));
}
bool LakeDry(const FSeigeLake& L,FVector2D A,FVector2D B,double Radius)
{
    if(PointSegment(L.Center,A,B)>FMath::Max(L.Radii.X,L.Radii.Y)*(1+L.ShoreVariation)+Radius)return true;
    if(L.RadiusAt(A)<=1||L.RadiusAt(B)<=1)return false;
    if(L.Shoreline.Num()>=3)
    {for(int32 I=0;I<L.Shoreline.Num();++I)if(SegmentDistance(A,B,L.Center+L.Shoreline[I]*L.Radii,L.Center+L.Shoreline[(I+1)%L.Shoreline.Num()]*L.Radii)<=Radius)return false;}
    else {const FVector2D R=L.Radii+FVector2D(Radius,Radius);if(PointSegment(FVector2D::ZeroVector,(A-L.Center)/R,(B-L.Center)/R)<=1)return false;}
    return true;
}
using FWaterInterval=TPair<double,double>;
bool CircleInterval(FVector2D A,FVector2D B,FVector2D Center,double Radius,FWaterInterval& Out)
{
    if(Radius<0)return false;const FVector2D D=B-A,P=A-Center;const double Length2=D.SizeSquared(),C=P.SizeSquared()-Radius*Radius;
    if(Length2<1.e-12){if(C>0)return false;Out={0,1};return true;}
    const double HalfB=FVector2D::DotProduct(P,D),Discriminant=HalfB*HalfB-Length2*C;if(Discriminant<0)return false;
    const double Root=FMath::Sqrt(Discriminant);Out={FMath::Max(0.,(-HalfB-Root)/Length2),FMath::Min(1.,(-HalfB+Root)/Length2)};return Out.Key<=Out.Value;
}
bool Slab(double Start,double Delta,double Low,double High,FWaterInterval& Interval)
{
    if(FMath::Abs(Delta)<1.e-12)return Start>=Low&&Start<=High;
    double A=(Low-Start)/Delta,B=(High-Start)/Delta;if(A>B)Swap(A,B);
    Interval.Key=FMath::Max(Interval.Key,A);Interval.Value=FMath::Min(Interval.Value,B);return Interval.Key<=Interval.Value;
}
TArray<FWaterInterval,TInlineAllocator<3>> CapsuleIntervals(FVector2D A,FVector2D B,FVector2D C,FVector2D D,double Radius)
{
    TArray<FWaterInterval,TInlineAllocator<3>> Result;FWaterInterval Interval;
    if(CircleInterval(A,B,C,Radius,Interval))Result.Add(Interval);
    if(CircleInterval(A,B,D,Radius,Interval))Result.Add(Interval);
    const FVector2D Axis=(D-C).GetSafeNormal(),Side(-Axis.Y,Axis.X),P=A-C,Delta=B-A;Interval={0,1};
    if(Slab(FVector2D::DotProduct(P,Axis),FVector2D::DotProduct(Delta,Axis),0,(D-C).Size(),Interval)&&
        Slab(FVector2D::DotProduct(P,Side),FVector2D::DotProduct(Delta,Side),-Radius,Radius,Interval))Result.Add(Interval);
    return Result;
}
double ChannelHeight(double Distance,double HalfWidth,double Surface,double Depth,double Bank,double Crest,double Valley,double Base)
{
    if(Distance>=HalfWidth+Valley)return Base;
    if(Distance<=HalfWidth)return Surface-Depth*(1-FMath::SmoothStep(0.,HalfWidth,Distance));
    const double DryDistance=Distance-HalfWidth;
    // A shallow shore rises into a broad drainage valley. The former narrow
    // fixed-height rim raised ribbons above low terrain and made artificial levees.
    const double Shore=Surface+Crest*FMath::SmoothStep(0.,Bank,DryDistance);
    const double ValleyBlend=FMath::SmoothStep(Bank,Valley,DryDistance);
    return FMath::Lerp(Shore,Base,ValleyBlend);
}
}
void FSeigeLake::BuildShoreline()
{
    Shoreline.Reset();constexpr int32 Count=128;
    for(int32 I=0;I<Count;++I)
    {
        const double A=2*PI*I/Count;
        const double R=1+ShoreVariation*(FMath::Sin(A*3+ShorePhase)+.5*FMath::Sin(A*5-ShorePhase*.7)+.25*FMath::Sin(A*7+ShorePhase*.4))/1.75;
        Shoreline.Add(FVector2D(FMath::Cos(A),FMath::Sin(A))*R);
    }
}
double FSeigeLake::RadialScale(FVector2D Direction) const
{
    if(Shoreline.Num()<3)return 1;
    Direction=Direction.GetSafeNormal();if(Direction.IsNearlyZero())return 1;
    double Angle=FMath::Atan2(Direction.Y,Direction.X);if(Angle<0)Angle+=2*PI;
    const int32 I=FMath::Min(Shoreline.Num()-1,FMath::FloorToInt(Angle/(2*PI)*Shoreline.Num()));
    const FVector2D A=Shoreline[I],B=Shoreline[(I+1)%Shoreline.Num()];
    return Cross(A,B)/FMath::Max(1.e-12,Cross(Direction,B-A));
}
double FSeigeLake::RadiusAt(FVector2D World) const
{const FVector2D P=(World-Center)/Radii;return P.Size()/RadialScale(P);}
double FSeigeCliff::Elevation(FVector2D World) const
{
    const double Angle=FMath::DegreesToRadians(AngleDegrees),C=FMath::Cos(Angle),S=FMath::Sin(Angle);
    const FVector2D P=World-Center;
    const double Along=(P.X*C+P.Y*S)/Radii.X,Across=(-P.X*S+P.Y*C)/Radii.Y;
    if(FMath::Abs(Along)>=1.15||Across<-.8||Across>1.3)return 0;
    // One fractured escarpment follows the ridge, with an eroded gentle back
    // slope. It does not wrap a circular plateau around an otherwise round hill.
    const double Fault=-.25+Irregularity*(FMath::Sin(Along*7.1)+.4*FMath::Sin(Along*17.3));
    const double Face=FMath::SmoothStep(Fault-EdgeRatio,Fault+EdgeRatio*.25,Across);
    const double Back=1-FMath::SmoothStep(.05,1.25,Across);
    const double Ends=1-FMath::SmoothStep(.62,1.15,FMath::Abs(Along));
    return Height*Face*Back*Ends*(.94+.06*FMath::Cos(Along*13.));
}
bool FSeigeEnvironment::Load(const FString& Filename,FString& Error)
{
    FString Raw;TSharedPtr<FJsonObject> Root;FSeigeEnvironment Next;Next.WorldOffset=WorldOffset;
    if(!FFileHelper::LoadFileToString(Raw,*Filename)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Root)||!Root){Error=TEXT("Cannot read physical environment profile: ")+Filename;return false;}
    auto Number=[&](const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double Min,double Max,double& Value)
    {if(!O||!O->TryGetNumberField(Key,Value)||!FMath::IsFinite(Value)||Value<Min||Value>Max){Error=TEXT("Invalid environment value: ")+FString(Key);return false;}return true;};
    double Version=0;if(!Number(Root,TEXT("schema_version"),1,2,Version)||Version!=FMath::FloorToDouble(Version)||!Root->TryGetBoolField(TEXT("enabled"),Next.Enabled))return false;
    if(!Number(Root,TEXT("river_half_width"),20,2000,Next.RiverHalfWidth)||!Number(Root,TEXT("river_depth"),1,200,Next.RiverDepth)||!Number(Root,TEXT("bank_width"),50,3000,Next.BankWidth)||!Number(Root,TEXT("bank_height"),0,200,Next.BankHeight))return false;
    if(Version>=2)
    {
        if(!Number(Root,TEXT("valley_width"),1000,18000,Next.ValleyWidth)||Next.ValleyWidth<=Next.BankWidth||
            !Number(Root,TEXT("base_elevation"),-1500,1500,Next.BaseElevation)||!Number(Root,TEXT("north_slope"),0,.02,Next.NorthSlope)||
            !Number(Root,TEXT("macro_amplitude"),0,1000,Next.MacroAmplitude)){if(Error.IsEmpty())Error=TEXT("Valley must extend beyond its bank");return false;}
    }
    auto ReadRiver=[&](const TArray<TSharedPtr<FJsonValue>>& Points,TArray<FSeigeRiverPoint>& Target)
    {
        if(Points.Num()<2||Points.Num()>64){Error=TEXT("Environment river needs 2..64 downhill points");return false;}
        for(const auto& Value:Points)
        {
            const auto O=Value->AsObject();FSeigeRiverPoint P;
            if(!Number(O,TEXT("x"),-100000,100000,P.Position.X)||!Number(O,TEXT("y"),-100000,100000,P.Position.Y)||!Number(O,TEXT("height"),-2000,2000,P.Height))return false;
            if(!Target.IsEmpty()&&(P.Height>Target.Last().Height||FVector2D::Distance(P.Position,Target.Last().Position)<100)){Error=TEXT("River must descend or stay level through distinct points");return false;}Target.Add(P);
        }
        return true;
    };
    const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
    if(!Root->TryGetArrayField(TEXT("river"),Points)||!ReadRiver(*Points,Next.River))return false;
    if(Version>=2)
    {
        if(!Root->TryGetArrayField(TEXT("additional_rivers"),Points)||Points->Num()>4){Error=TEXT("Invalid additional drainage rivers");return false;}
        for(const auto& Value:*Points){const TArray<TSharedPtr<FJsonValue>>* Path=nullptr;FSeigeRiver R;if(!Value->TryGetArray(Path)||!ReadRiver(*Path,R.Points))return false;Next.AdditionalRivers.Add(MoveTemp(R));}
    }
    const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;
    if(!Root->TryGetArrayField(TEXT("lakes"),Items)||Items->Num()>8){Error=TEXT("Invalid environment lakes");return false;}
    for(const auto& Value:*Items)
    {
        const auto O=Value->AsObject();FSeigeLake L;
        if(!Number(O,TEXT("x"),-90000,90000,L.Center.X)||!Number(O,TEXT("y"),-90000,90000,L.Center.Y)||!Number(O,TEXT("radius_x"),500,12000,L.Radii.X)||!Number(O,TEXT("radius_y"),500,12000,L.Radii.Y)||!Number(O,TEXT("height"),-2000,2000,L.Height)||!Number(O,TEXT("depth"),1,500,L.Depth))return false;
        if(Version>=2&&(!Number(O,TEXT("shore_variation"),0,.3,L.ShoreVariation)||!Number(O,TEXT("shore_phase"),-10,10,L.ShorePhase)))return false;
        bool Connected=false;Next.ForEachRiver([&](const auto& R){for(int32 I=1;I<R.Num();++I)if(FMath::IsNearlyEqual(R[I-1].Height,L.Height,.001)&&FMath::IsNearlyEqual(R[I].Height,L.Height,.001)&&PointSegment(L.Center/L.Radii,R[I-1].Position/L.Radii,R[I].Position/L.Radii)<1-L.ShoreVariation)Connected=true;});
        if(!Connected){Error=TEXT("Each lake must intersect a level river reach at its water elevation");return false;}L.BuildShoreline();Next.Lakes.Add(MoveTemp(L));
    }
    if(Version>=2)
    {
        if(!Root->TryGetArrayField(TEXT("fords"),Items)||Items->Num()>32){Error=TEXT("Invalid natural ford array");return false;}
        for(const auto& Value:*Items)
        {
            const auto O=Value->AsObject();FSeigeFord F;
            if(!Number(O,TEXT("x"),-90000,90000,F.Center.X)||!Number(O,TEXT("y"),-90000,90000,F.Center.Y)||!Number(O,TEXT("radius"),500,2500,F.Radius)||
                !Number(O,TEXT("blend"),200,1500,F.Blend)||!Number(O,TEXT("depth"),.5,4,F.Depth))return false;
            bool OnRiver=false;Next.ForEachRiver([&](const auto& R){for(int32 I=1;I<R.Num();++I)if(PointSegment(F.Center,R[I-1].Position,R[I].Position)<Next.RiverHalfWidth*.2)OnRiver=true;});
            bool InLake=false;for(const auto& L:Next.Lakes)if(((F.Center-L.Center)/L.Radii).Size()<1+L.ShoreVariation+(F.Radius+F.Blend)/FMath::Min(L.Radii.X,L.Radii.Y))InLake=true;
            if(!OnRiver||InLake||F.Radius<Next.RiverHalfWidth*2){Error=TEXT("Ford must span a river away from lakes");return false;}Next.Fords.Add(F);
        }
    }
    if(!Root->TryGetArrayField(TEXT("cliffs"),Items)||Items->Num()>16){Error=TEXT("Invalid environment cliffs");return false;}
    for(const auto& Value:*Items)
    {
        const auto O=Value->AsObject();FSeigeCliff C;
        if(!Number(O,TEXT("x"),-90000,90000,C.Center.X)||!Number(O,TEXT("y"),-90000,90000,C.Center.Y)||!Number(O,TEXT("radius_x"),500,15000,C.Radii.X)||!Number(O,TEXT("radius_y"),500,15000,C.Radii.Y)||!Number(O,TEXT("height"),0,1200,C.Height)||!Number(O,TEXT("edge_ratio"),.03,.6,C.EdgeRatio))return false;
        if(Version>=2&&(!Number(O,TEXT("angle_degrees"),-360,360,C.AngleDegrees)||!Number(O,TEXT("irregularity"),0,.3,C.Irregularity)))return false;Next.Cliffs.Add(C);
    }
    if(Version>=2){SmoothDrainage(Next.River,Next.Fords,Next.RiverHalfWidth);for(auto& R:Next.AdditionalRivers)SmoothDrainage(R.Points,Next.Fords,Next.RiverHalfWidth);}
    Next.Fingerprint=FMD5::HashAnsiString(*SeigeCanonicalJson(Root));*this=MoveTemp(Next);return true;
}
FSeigeWaterSample FSeigeEnvironment::WaterAt(FVector2D Local) const
{
    FSeigeWaterSample Sample;if(!Enabled)return Sample;const FVector2D P=Local+WorldOffset;
    for(const auto& L:Lakes)
    {
        if(FMath::Abs(P.X-L.Center.X)>L.Radii.X*(1+L.ShoreVariation)||FMath::Abs(P.Y-L.Center.Y)>L.Radii.Y*(1+L.ShoreVariation))continue;
        const double Radius=L.RadiusAt(P);if(Radius<=1){Sample.Present=true;Sample.Surface=L.Height;Sample.Depth=L.Depth*(1-FMath::SmoothStep(0.,1.,Radius));Sample.Shore=(1-Radius)*FMath::Min(L.Radii.X,L.Radii.Y);return Sample;}
    }
    double Distance=0,Surface=0;
    if(RiverProfileAt(P,RiverHalfWidth,Distance,Surface))
    {Sample.Present=true;Sample.Surface=Surface;Sample.Depth=RiverDepthAt(P)*(1-FMath::SmoothStep(0.,RiverHalfWidth,Distance));Sample.Shore=RiverHalfWidth-Distance;}
    return Sample;
}
bool FSeigeEnvironment::RiverProfileAt(FVector2D P,double MaximumDistance,double& Distance,double& Surface) const
{
    // Distance to a polyline is continuous, but the projected elevation of its
    // winning segment is not: adjacent reach bisectors previously produced tiny
    // vertical bank steps which stopped real centimetre-scale movement. Blend
    // nearby projections with a compact smooth kernel, including zero-weight
    // candidates beyond the query radius. Water, bed and rendered faces share it.
    const double Blend=RiverHalfWidth*2,Limit=MaximumDistance+Blend;
    TArray<FVector2D,TInlineAllocator<32>> Samples;Distance=TNumericLimits<double>::Max();Surface=0;
    ForEachRiver([&](const auto& R){for(int32 I=1;I<R.Num();++I)
    {
        if(!BoundsNear(P,P,R[I-1].Position,R[I].Position,Limit))continue;
        double Along=0;const double D=PointSegment(P,R[I-1].Position,R[I].Position,&Along);if(D>Limit)continue;
        Distance=FMath::Min(Distance,D);Samples.Add(FVector2D(D,FMath::Lerp(R[I-1].Height,R[I].Height,Along)));
    }});
    if(Distance>MaximumDistance)return false;
    double Total=0;
    for(const auto& S:Samples){const double W=FMath::Square(FMath::Max(0.,1-(S.X-Distance)/Blend));Surface+=S.Y*W;Total+=W;}
    Surface/=Total;return true;
}
bool FSeigeEnvironment::CanStand(FVector2D P,double Radius) const{return SegmentDry(P,P,Radius);}
bool FSeigeEnvironment::SegmentDry(FVector2D A,FVector2D B,double Radius) const
{
    if(!Enabled)return true;A+=WorldOffset;B+=WorldOffset;Radius=FMath::Max(0.,Radius);
    for(const auto& L:Lakes)if(!LakeDry(L,A,B,Radius))return false;
    bool Dry=true;ForEachRiver([&](const auto& R){if(!Dry)return;for(int32 I=1;I<R.Num();++I)if(BoundsNear(A,B,R[I-1].Position,R[I].Position,RiverHalfWidth+Radius)&&SegmentDistance(A,B,R[I-1].Position,R[I].Position)<=RiverHalfWidth+Radius){Dry=false;break;}});return Dry;
}
double FSeigeEnvironment::RiverDepthAt(FVector2D P) const
{
    double Depth=RiverDepth;
    for(const auto& F:Fords)
    {
        const double Distance=FVector2D::Distance(P,F.Center);if(Distance>=F.Radius+F.Blend)continue;
        Depth=FMath::Min(Depth,FMath::Lerp(F.Depth,RiverDepth,FMath::SmoothStep(F.Radius,F.Radius+F.Blend,Distance)));
    }
    return Depth;
}
bool FSeigeEnvironment::CanTraverse(FVector2D P,double Radius) const{return SegmentTraversable(P,P,Radius);}
bool FSeigeEnvironment::SegmentTraversable(FVector2D A,FVector2D B,double Radius) const
{
    if(!Enabled)return true;A+=WorldOffset;B+=WorldOffset;Radius=FMath::Max(0.,Radius);
    for(const auto& L:Lakes)if(!LakeDry(L,A,B,Radius))return false;
    TArray<FWaterInterval,TInlineAllocator<32>> Crossings;
    for(const auto& F:Fords){FWaterInterval I;if(CircleInterval(A,B,F.Center,F.Radius-Radius,I))Crossings.Add(I);}
    Crossings.Sort([](const auto& X,const auto& Y){return X.Key<Y.Key;});
    bool Allowed=true;
    ForEachRiver([&](const auto& R)
    {
        if(!Allowed)return;
        for(int32 I=1;I<R.Num();++I)
        {
            if(!BoundsNear(A,B,R[I-1].Position,R[I].Position,RiverHalfWidth+Radius))continue;
            if(SegmentDistance(A,B,R[I-1].Position,R[I].Position)>RiverHalfWidth+Radius)continue;
            for(const auto& Wet:CapsuleIntervals(A,B,R[I-1].Position,R[I].Position,RiverHalfWidth+Radius))
            {
                double Covered=Wet.Key;
                for(const auto& Ford:Crossings)
                {
                    if(Ford.Value<Covered)continue;if(Ford.Key>Covered+1.e-9)break;
                    Covered=FMath::Max(Covered,Ford.Value);if(Covered>=Wet.Value-1.e-9)break;
                }
                if(Covered<Wet.Value-1.e-9||(Wet.Key==Wet.Value&&!Crossings.ContainsByPredicate([&](const auto& F){return F.Key<=Wet.Key&&F.Value>=Wet.Value;}))){Allowed=false;return;}
            }
        }
    });
    return Allowed;
}
TArray<FVector2D> FSeigeEnvironment::RoutingWaypoints(double Radius) const
{
    TArray<FVector2D> Result;if(!Enabled)return Result;const double Margin=Radius+1;
    for(const auto& L:Lakes)for(int32 I=0;I<32;++I)
    {const double A=I*2*PI/32;const FVector2D Direction(FMath::Cos(A),FMath::Sin(A));const FVector2D P=L.Center+Direction*(L.Radii*L.RadialScale(Direction)+FVector2D(Margin,Margin))*1.04;if(CanStand(P-WorldOffset,Radius))Result.Add(P-WorldOffset);}
    ForEachRiver([&](const auto& R){for(int32 I=0;I<R.Num();++I)if(R[I].RoutingAnchor)for(int32 J=0;J<8;++J){const double A=J*PI/4;const FVector2D P=R[I].Position+FVector2D(FMath::Cos(A),FMath::Sin(A))*(RiverHalfWidth+Margin)*1.5;if(CanStand(P-WorldOffset,Radius))Result.Add(P-WorldOffset);}});
    for(const auto& F:Fords)
    {
        double Best=TNumericLimits<double>::Max();FVector2D Direction(0,1);
        ForEachRiver([&](const auto& R){for(int32 I=1;I<R.Num();++I){const double Distance=PointSegment(F.Center,R[I-1].Position,R[I].Position);if(Distance<Best){Best=Distance;Direction=(R[I].Position-R[I-1].Position).GetSafeNormal();}}});
        const FVector2D Side(-Direction.Y,Direction.X);
        for(double Sign:{-1.,1.})for(double Distance:{RiverHalfWidth+Margin+40,RiverHalfWidth+BankWidth+Margin+40})
        {const FVector2D P=F.Center+Side*Sign*Distance;if(CanTraverse(P-WorldOffset,Radius))Result.Add(P-WorldOffset);}
        if(CanTraverse(F.Center-WorldOffset,Radius))Result.Add(F.Center-WorldOffset);
    }
    return Result;
}
double FSeigeEnvironment::ShapeHeight(FVector2D Local,double Base) const
{
    if(!Enabled)return Base;const FVector2D P=Local+WorldOffset;
    for(const auto& C:Cliffs)Base+=C.Elevation(P);
    double Nearest=RiverHalfWidth+ValleyWidth,Surface=0;
    const bool NearRiver=RiverProfileAt(P,Nearest,Nearest,Surface);
    const double NaturalBase=Base;
    if(NearRiver)
    {
        Base=ChannelHeight(Nearest,RiverHalfWidth,Surface,RiverDepthAt(P),BankWidth,BankHeight,ValleyWidth,Base);
    }
    for(const auto& L:Lakes)
    {
        const double Scale=FMath::Min(L.Radii.X,L.Radii.Y);
        if(FMath::Abs(P.X-L.Center.X)>L.Radii.X*(1+L.ShoreVariation)+ValleyWidth*L.Radii.X/Scale||FMath::Abs(P.Y-L.Center.Y)>L.Radii.Y*(1+L.ShoreVariation)+ValleyWidth*L.Radii.Y/Scale)continue;
        const double D=L.RadiusAt(P)*Scale;
        if(D<Scale+ValleyWidth)
        {
            const double LakeHeight=ChannelHeight(D,Scale,L.Height,L.Depth,BankWidth,BankHeight,ValleyWidth,NaturalBase);
            // A distant pond valley must not overwrite the river shore then be
            // abruptly clamped at the wet edge. Fade its dry shoulder influence
            // away across the river bank, leaving the ford's real bed continuous.
            const double Influence=D<=Scale?1.:FMath::SmoothStep(RiverHalfWidth,RiverHalfWidth+BankWidth,Nearest);
            Base=FMath::Lerp(Base,FMath::Min(Base,LakeHeight),Influence);
        }
    }
    return FMath::Clamp(Base,-5500.,5500.);
}
