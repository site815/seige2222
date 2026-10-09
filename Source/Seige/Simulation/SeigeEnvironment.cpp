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
double SegmentDistance(FVector2D A,FVector2D B,FVector2D C,FVector2D D)
{
    const double Den=Cross(B-A,D-C);
    if(FMath::Abs(Den)>1.e-12){const double T=Cross(C-A,D-C)/Den,U=Cross(C-A,B-A)/Den;if(T>=0&&T<=1&&U>=0&&U<=1)return 0;}
    return FMath::Min(FMath::Min(PointSegment(A,C,D),PointSegment(B,C,D)),FMath::Min(PointSegment(C,A,B),PointSegment(D,A,B)));
}
double ChannelHeight(double Distance,double HalfWidth,double Surface,double Depth,double Bank,double Crest,double Base)
{
    if(Distance>=HalfWidth+Bank)return Base;
    const double Bed=Surface-Depth*(1-FMath::SmoothStep(0.,HalfWidth,Distance));
    const double Rim=Surface+Crest*FMath::SmoothStep(HalfWidth,HalfWidth+Bank*.45,Distance);
    return FMath::Lerp(Distance<HalfWidth?Bed:Rim,Base,FMath::SmoothStep(HalfWidth+Bank*.45,HalfWidth+Bank,Distance));
}
}
bool FSeigeEnvironment::Load(const FString& Filename,FString& Error)
{
    FString Raw;TSharedPtr<FJsonObject> Root;FSeigeEnvironment Next;Next.WorldOffset=WorldOffset;
    if(!FFileHelper::LoadFileToString(Raw,*Filename)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Root)||!Root){Error=TEXT("Cannot read physical environment profile: ")+Filename;return false;}
    auto Number=[&](const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double Min,double Max,double& Value)
    {if(!O||!O->TryGetNumberField(Key,Value)||!FMath::IsFinite(Value)||Value<Min||Value>Max){Error=TEXT("Invalid environment value: ")+FString(Key);return false;}return true;};
    double Version=0;if(!Number(Root,TEXT("schema_version"),1,1,Version)||!Root->TryGetBoolField(TEXT("enabled"),Next.Enabled))return false;
    if(!Number(Root,TEXT("river_half_width"),20,2000,Next.RiverHalfWidth)||!Number(Root,TEXT("river_depth"),1,200,Next.RiverDepth)||!Number(Root,TEXT("bank_width"),50,3000,Next.BankWidth)||!Number(Root,TEXT("bank_height"),0,200,Next.BankHeight))return false;
    const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
    if(!Root->TryGetArrayField(TEXT("river"),Points)||Points->Num()<2||Points->Num()>64){Error=TEXT("Environment river needs 2..64 downhill points");return false;}
    for(const auto& Value:*Points)
    {
        const auto O=Value->AsObject();FSeigeRiverPoint P;
        if(!Number(O,TEXT("x"),-100000,100000,P.Position.X)||!Number(O,TEXT("y"),-100000,100000,P.Position.Y)||!Number(O,TEXT("height"),-2000,2000,P.Height))return false;
        if(!Next.River.IsEmpty()&&(P.Height>Next.River.Last().Height||FVector2D::Distance(P.Position,Next.River.Last().Position)<100)){Error=TEXT("River must descend or stay level through distinct points");return false;}
        Next.River.Add(P);
    }
    const TArray<TSharedPtr<FJsonValue>>* Items=nullptr;
    if(!Root->TryGetArrayField(TEXT("lakes"),Items)||Items->Num()>8){Error=TEXT("Invalid environment lakes");return false;}
    for(const auto& Value:*Items)
    {
        const auto O=Value->AsObject();FSeigeLake L;
        if(!Number(O,TEXT("x"),-90000,90000,L.Center.X)||!Number(O,TEXT("y"),-90000,90000,L.Center.Y)||!Number(O,TEXT("radius_x"),500,12000,L.Radii.X)||!Number(O,TEXT("radius_y"),500,12000,L.Radii.Y)||!Number(O,TEXT("height"),-2000,2000,L.Height)||!Number(O,TEXT("depth"),1,500,L.Depth))return false;
        bool Connected=false;for(int32 I=1;I<Next.River.Num();++I)if(FMath::IsNearlyEqual(Next.River[I-1].Height,L.Height,.001)&&FMath::IsNearlyEqual(Next.River[I].Height,L.Height,.001)&&PointSegment(L.Center/L.Radii,Next.River[I-1].Position/L.Radii,Next.River[I].Position/L.Radii)<1)Connected=true;
        if(!Connected){Error=TEXT("Each lake must intersect a level river reach at its water elevation");return false;}Next.Lakes.Add(L);
    }
    if(!Root->TryGetArrayField(TEXT("cliffs"),Items)||Items->Num()>16){Error=TEXT("Invalid environment cliffs");return false;}
    for(const auto& Value:*Items)
    {
        const auto O=Value->AsObject();FSeigeCliff C;
        if(!Number(O,TEXT("x"),-90000,90000,C.Center.X)||!Number(O,TEXT("y"),-90000,90000,C.Center.Y)||!Number(O,TEXT("radius_x"),500,15000,C.Radii.X)||!Number(O,TEXT("radius_y"),500,15000,C.Radii.Y)||!Number(O,TEXT("height"),0,1200,C.Height)||!Number(O,TEXT("edge_ratio"),.03,.6,C.EdgeRatio))return false;Next.Cliffs.Add(C);
    }
    Next.Fingerprint=FMD5::HashAnsiString(*SeigeCanonicalJson(Root));*this=MoveTemp(Next);return true;
}
FSeigeWaterSample FSeigeEnvironment::WaterAt(FVector2D Local) const
{
    FSeigeWaterSample Sample;if(!Enabled)return Sample;const FVector2D P=Local+WorldOffset;
    for(const auto& L:Lakes)
    {const double Radius=((P-L.Center)/L.Radii).Size();if(Radius<=1){Sample.Present=true;Sample.Surface=L.Height;Sample.Depth=L.Depth*(1-FMath::SmoothStep(0.,1.,Radius));Sample.Shore=(1-Radius)*FMath::Min(L.Radii.X,L.Radii.Y);return Sample;}}
    double Best=RiverHalfWidth;
    for(int32 I=1;I<River.Num();++I){double Along=0;const double Distance=PointSegment(P,River[I-1].Position,River[I].Position,&Along);if(Distance<=Best){Best=Distance;Sample.Present=true;Sample.Surface=FMath::Lerp(River[I-1].Height,River[I].Height,Along);Sample.Depth=RiverDepth*(1-FMath::SmoothStep(0.,RiverHalfWidth,Distance));Sample.Shore=RiverHalfWidth-Distance;}}
    return Sample;
}
bool FSeigeEnvironment::CanStand(FVector2D P,double Radius) const{return SegmentDry(P,P,Radius);}
bool FSeigeEnvironment::SegmentDry(FVector2D A,FVector2D B,double Radius) const
{
    if(!Enabled)return true;A+=WorldOffset;B+=WorldOffset;Radius=FMath::Max(0.,Radius);
    for(const auto& L:Lakes){const FVector2D R=L.Radii+FVector2D(Radius,Radius);if(PointSegment(FVector2D::ZeroVector,(A-L.Center)/R,(B-L.Center)/R)<=1)return false;}
    for(int32 I=1;I<River.Num();++I)if(SegmentDistance(A,B,River[I-1].Position,River[I].Position)<=RiverHalfWidth+Radius)return false;
    return true;
}
TArray<FVector2D> FSeigeEnvironment::RoutingWaypoints(double Radius) const
{
    TArray<FVector2D> Result;if(!Enabled)return Result;const double Margin=Radius+1;
    for(const auto& L:Lakes)for(int32 I=0;I<24;++I){const double A=I*2*PI/24;const FVector2D P=L.Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*(L.Radii+FVector2D(Margin,Margin))*1.02;if(CanStand(P-WorldOffset,Radius))Result.Add(P-WorldOffset);}
    for(int32 I=0;I<River.Num();++I)for(int32 J=0;J<8;++J){const double A=J*PI/4;const FVector2D P=River[I].Position+FVector2D(FMath::Cos(A),FMath::Sin(A))*(RiverHalfWidth+Margin)*1.5;if(CanStand(P-WorldOffset,Radius))Result.Add(P-WorldOffset);}
    return Result;
}
double FSeigeEnvironment::ShapeHeight(FVector2D Local,double Base) const
{
    if(!Enabled)return Base;const FVector2D P=Local+WorldOffset;
    for(const auto& C:Cliffs){const double R=((P-C.Center)/C.Radii).Size();Base+=C.Height*(1-FMath::SmoothStep(1-C.EdgeRatio,1.,R));}
    // One nearest reach prevents overlapping segment influences at bends from
    // carving the same bed twice. A connected lake then supplies its flat level.
    double Nearest=RiverHalfWidth+BankWidth,Surface=0;
    for(int32 I=1;I<River.Num();++I){double T=0;const double D=PointSegment(P,River[I-1].Position,River[I].Position,&T);if(D<Nearest){Nearest=D;Surface=FMath::Lerp(River[I-1].Height,River[I].Height,T);}}
    double WetBed=TNumericLimits<double>::Max();
    if(Nearest<RiverHalfWidth+BankWidth)
    {
        Base=ChannelHeight(Nearest,RiverHalfWidth,Surface,RiverDepth,BankWidth,BankHeight,Base);
        if(Nearest<=RiverHalfWidth)WetBed=Base;
    }
    for(const auto& L:Lakes)
    {
        const double Scale=FMath::Min(L.Radii.X,L.Radii.Y),D=((P-L.Center)/L.Radii).Size()*Scale;
        if(D<Scale+BankWidth)
        {
            Base=ChannelHeight(D,Scale,L.Height,L.Depth,BankWidth,BankHeight,Base);
            if(D<=Scale)WetBed=FMath::Min(WetBed,Base);
        }
    }
    // A bank belongs only to dry ground. A lake's shoulder must never dam an
    // already-carved connected river (nor a neighboring lake's wet bed).
    Base=FMath::Min(Base,WetBed);
    return FMath::Clamp(Base,-5500.,5500.);
}
