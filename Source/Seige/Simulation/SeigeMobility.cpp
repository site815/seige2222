#include "SeigeMobility.h"
#include "SeigeSimulation.h"
#include "SeigeCombatJson.h"
#include "SeigeCanonicalJson.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
using namespace SeigeCombatJson;

bool FSeigeMobilityRules::Load(const FString& Directory,FString& Error)
{
    O Root;FString Raw;const O* Rows=nullptr;int32 Schema=0;
    if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(Directory,TEXT("mobility.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Root)||!Int(Root,TEXT("schema_version"),Schema,1,1)||!Root->TryGetObjectField(TEXT("profiles"),Rows)){Error=TEXT("Invalid mobility rules");return false;}
    TMap<FString,FSeigeMobilityProfile> Pending;
    for(const TCHAR* Id:{TEXT("drone"),TEXT("transport"),TEXT("wheeled"),TEXT("tracked"),TEXT("mech")})
    {
        const O* Row=nullptr;FSeigeMobilityProfile P;
        if(!(*Rows)->TryGetObjectField(Id,Row)||!Num(*Row,TEXT("body_radius_meters"),P.BodyRadiusMeters,.01,30)||!Num(*Row,TEXT("clearance_meters"),P.ClearanceMeters,0,10)||!Num(*Row,TEXT("access_meters"),P.AccessMeters,.01,100)||!Num(*Row,TEXT("speed_kmh"),P.SpeedKmh,.01,300)||!Num(*Row,TEXT("maximum_slope"),P.MaximumSlope,.001,5)||!Num(*Row,TEXT("rough_slope"),P.RoughSlope,.001,P.MaximumSlope)||!Num(*Row,TEXT("rough_speed_multiplier"),P.RoughMultiplier,.001,1)||!Num(*Row,TEXT("sample_meters"),P.SampleMeters,.25,20)||!(*Row)->TryGetBoolField(TEXT("allow_easy"),P.AllowEasy)||!(*Row)->TryGetBoolField(TEXT("allow_rough"),P.AllowRough)||!(*Row)->TryGetBoolField(TEXT("allow_water"),P.AllowWater)||!(*Row)->TryGetBoolField(TEXT("allow_road"),P.AllowRoad))
        {Error=FString(TEXT("Invalid mobility profile: "))+Id;return false;}Pending.Add(Id,P);
    }
    if((*Rows)->Values.Num()!=Pending.Num()){Error=TEXT("Mobility rules require exactly the supported unit families");return false;}
    Profiles=MoveTemp(Pending);Fingerprint=FMD5::HashAnsiString(*SeigeCanonicalJson(Root));Error.Empty();return true;
}
double FSeigeMobilityRules::ClearanceMeters(const FString& Id) const{const auto* P=Profiles.Find(Id);return P?P->BodyRadiusMeters+P->ClearanceMeters:0;}
double FSeigeMobilityRules::AccessMeters(const FString& Id) const{const auto* P=Profiles.Find(Id);return P?P->AccessMeters:0;}
double FSeigeMobilityRules::Grade(const FSeigeSimulation& S,FVector2D A,FVector2D B,double SampleMeters,double GradeLimit) const
{
    const double Length=FVector2D::Distance(A,B);if(Length<1.e-8)return 0;
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(Length*S.MetersPerWorldUnit()/SampleMeters));
    auto Height=[&](FVector2D P){return TerrainHeight?TerrainHeight(P):S.Environment.ShapeHeight(P,0);};
    // A compact shoulder at an endpoint can disappear completely between the
    // long edge's midpoint samples. Check the real entry/exit slopes at short
    // movement scales as well. This is a fixed eight probes, not a centimetre
    // scan over every kilometre of a route.
    const FVector2D Direction=(B-A)/Length;const double StartHeight=Height(A),EndHeight=Height(B);
    if(!FMath::IsFinite(StartHeight)||!FMath::IsFinite(EndHeight))return TNumericLimits<double>::Max();
    double EndpointGrade=0;const double Probe=SampleMeters*.01/S.MetersPerWorldUnit();
    for(double Scale:{.25,.5,1.,2.})
    {
        const double Span=FMath::Min(Length*.5,Probe*Scale);if(Span<1.e-9)continue;
        const double Entry=Height(A+Direction*Span),Exit=Height(B-Direction*Span);
        if(!FMath::IsFinite(Entry)||!FMath::IsFinite(Exit))return TNumericLimits<double>::Max();
        EndpointGrade=FMath::Max3(EndpointGrade,FMath::Abs(Entry-StartHeight)/Span,FMath::Abs(EndHeight-Exit)/Span);
        if(EndpointGrade>GradeLimit)return EndpointGrade;
    }
    // End-to-end averages can conceal a short steep bank or a foundation seam.
    // Refine changing slopes inside each authored sample interval; flat/linear
    // spans remain cheap and impassable edges stop sampling immediately.
    auto Inspect=[&](auto&& Self,FVector2D Start,FVector2D End,double H0,double H1,int Depth)->double
    {
        if(!FMath::IsFinite(H0)||!FMath::IsFinite(H1))return TNumericLimits<double>::Max();
        const double Span=FVector2D::Distance(Start,End);if(Span<1.e-9)return 0;
        const FVector2D Mid=(Start+End)*.5;const double HM=Height(Mid);
        if(!FMath::IsFinite(HM))return TNumericLimits<double>::Max();
        const double Left=(HM-H0)/(Span*.5),Right=(H1-HM)/(Span*.5);
        double Result=FMath::Max(FMath::Abs(Left),FMath::Abs(Right));
        if(Result>GradeLimit)return Result;
        // Small curvature is still important near the capability limit: two
        // acceptable half-span averages can hide a local peak which stops the
        // real vehicle on its next short step. Refine these marginal spans too
        // instead of certifying a route the physical mover cannot follow.
        if(Depth<4&&(Result>GradeLimit*.9||FMath::Abs(Left-Right)>GradeLimit*.05))
        {
            Result=FMath::Max(Result,Self(Self,Start,Mid,H0,HM,Depth+1));if(Result>GradeLimit)return Result;
            Result=FMath::Max(Result,Self(Self,Mid,End,HM,H1,Depth+1));
        }
        return Result;
    };
    FVector2D PreviousPoint=A;double Previous=StartHeight,Maximum=EndpointGrade;
    for(int32 I=1;I<=Steps;++I)
    {const FVector2D Point=FMath::Lerp(A,B,double(I)/Steps);const double H=Height(Point);Maximum=FMath::Max(Maximum,Inspect(Inspect,PreviousPoint,Point,Previous,H,0));if(Maximum>GradeLimit)return Maximum;Previous=H;PreviousPoint=Point;}
    return Maximum;
}
bool FSeigeMobilityRules::MovementAllowed(const FSeigeSimulation& S,const FString& Id,FVector2D A,FVector2D B,double AdditionalRadiusMeters) const
{
    const auto* P=Profiles.Find(Id);if(!P)return false;
    const double Radius=FMath::Max(P->BodyRadiusMeters,AdditionalRadiusMeters)+P->ClearanceMeters;
    if(!P->AllowWater&&!S.Environment.SegmentTraversable(A,B,Radius/S.MetersPerWorldUnit()))return false;
    const double G=Grade(S,A,B,P->SampleMeters,P->MaximumSlope);if(G>P->MaximumSlope)return false;
    const bool Road=S.HasRoadLink(A,B);
    if(Road)return P->AllowRoad;
    return G>P->RoughSlope?P->AllowRough:P->AllowEasy;
}
double FSeigeMobilityRules::Speed(const FSeigeSimulation& S,const FString& Id,FVector2D A,FVector2D B,double BaseKmhOverride) const
{
    const auto* P=Profiles.Find(Id);if(!P)return 0;const double Base=(BaseKmhOverride>0?BaseKmhOverride:P->SpeedKmh)/3.6;
    const double Road=S.RouteSpeedMultiplier(A,B);if(P->AllowRoad&&S.HasRoadLink(A,B))return Base*Road;
    // Movement ticks query the local surface rather than resampling kilometres
    // of an already validated route each 50 ms. Path clearance validates all edges.
    const FVector2D Sample=A+(B-A).GetSafeNormal()*FMath::Min(FVector2D::Distance(A,B),P->SampleMeters/S.MetersPerWorldUnit());
    return Base*(Grade(S,A,Sample,P->SampleMeters,P->RoughSlope)>P->RoughSlope?P->RoughMultiplier:1.);
}
