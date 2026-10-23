#pragma once
#include "CoreMinimal.h"

enum class ESeigeInspectKind : uint8 { None, Worker, Transport, Vehicle, Enemy, Companion, Building, Stockpile };

// Owner sector, not just the numeric entity id, is part of a unit's identity.
// A home fleet may physically visit a different sector without changing owner.
struct FSeigeInspectionRef
{
    ESeigeInspectKind Kind=ESeigeInspectKind::None;
    int32 OwnerSector=4,Id=0;
    FString WorkerId;
    FString ResourceId;
    bool ConstructionStock=false;
    bool IsValid() const{return Kind!=ESeigeInspectKind::None&&(Kind==ESeigeInspectKind::Worker?!WorkerId.IsEmpty():Id>0);}
    bool operator==(const FSeigeInspectionRef& Other) const{return Kind==Other.Kind&&OwnerSector==Other.OwnerSector&&Id==Other.Id&&WorkerId==Other.WorkerId&&ResourceId==Other.ResourceId&&ConstructionStock==Other.ConstructionStock;}
};

struct FSeigeInspectionCandidate
{
    FSeigeInspectionRef Ref;
    FString Name;
    FBox Bounds=FBox(ForceInit);
    double Health=0,MaximumHealth=0,ConstructionProgress=-1;
    double StockAmount=0;
    int32 ViewSector=4;
};

struct FSeigeInspectionProjection
{
    int32 Candidate=INDEX_NONE;
    FBox2D Screen=FBox2D(ForceInit);
    double RayDepth=0;
};

struct FSeigeHoverBars
{
    FBox2D Health,Progress;
    double HealthFraction=0,ProgressFraction=0;
    bool HasProgress=false;
};

namespace SeigeInspection
{
    SEIGE_API bool Visible(double Health,bool Outdoor,bool Embarked,bool Evacuated,bool Rendered,bool Revealed);
    SEIGE_API bool RayBox(const FVector& Origin,const FVector& Direction,const FBox& Bounds,double& Depth);
    SEIGE_API int32 Pick(const TArray<FSeigeInspectionProjection>& Candidates,FVector2D Mouse,double TerrainDepth=TNumericLimits<double>::Max());
    SEIGE_API FSeigeHoverBars Bars(const FBox2D& ScreenBounds,double Health,double MaximumHealth,double Progress,double Scale=1);
}
