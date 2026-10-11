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
double FSeigeMobilityRules::Grade(const FSeigeSimulation& S,FVector2D A,FVector2D B,double SampleMeters) const
{
    const double Length=FVector2D::Distance(A,B);if(Length<1.e-8)return 0;
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(Length*S.MetersPerWorldUnit()/SampleMeters));
    auto Height=[&](FVector2D P){return TerrainHeight?TerrainHeight(P):S.Environment.ShapeHeight(P,0);};
    double Previous=Height(A),Maximum=0;for(int32 I=1;I<=Steps;++I){const double H=Height(FMath::Lerp(A,B,double(I)/Steps));if(!FMath::IsFinite(H)||!FMath::IsFinite(Previous))return TNumericLimits<double>::Max();Maximum=FMath::Max(Maximum,FMath::Abs(H-Previous)/(Length/Steps));Previous=H;}return Maximum;
}
bool FSeigeMobilityRules::MovementAllowed(const FSeigeSimulation& S,const FString& Id,FVector2D A,FVector2D B,double AdditionalRadiusMeters) const
{
    const auto* P=Profiles.Find(Id);if(!P)return false;
    const double Radius=FMath::Max(P->BodyRadiusMeters,AdditionalRadiusMeters)+P->ClearanceMeters;
    if(!P->AllowWater&&!S.Environment.SegmentDry(A,B,Radius/S.MetersPerWorldUnit()))return false;
    const double G=Grade(S,A,B,P->SampleMeters);if(G>P->MaximumSlope)return false;
    const bool Road=S.RouteSpeedMultiplier(A,B)>1;
    if(Road)return P->AllowRoad;
    return G>P->RoughSlope?P->AllowRough:P->AllowEasy;
}
double FSeigeMobilityRules::Speed(const FSeigeSimulation& S,const FString& Id,FVector2D A,FVector2D B,double BaseKmhOverride) const
{
    const auto* P=Profiles.Find(Id);if(!P)return 0;const double Base=(BaseKmhOverride>0?BaseKmhOverride:P->SpeedKmh)/3.6;
    const double Road=S.RouteSpeedMultiplier(A,B);if(Road>1&&P->AllowRoad)return Base*Road;
    // Movement ticks query the local surface rather than resampling kilometres
    // of an already validated route each 50 ms. Path clearance validates all edges.
    const FVector2D Sample=A+(B-A).GetSafeNormal()*FMath::Min(FVector2D::Distance(A,B),P->SampleMeters/S.MetersPerWorldUnit());
    return Base*(Grade(S,A,Sample,P->SampleMeters)>P->RoughSlope?P->RoughMultiplier:1.);
}
