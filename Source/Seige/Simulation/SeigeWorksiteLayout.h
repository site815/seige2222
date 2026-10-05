#pragma once
#include "CoreMinimal.h"

struct FSeigeWorksiteBounds
{
    FVector2D Position;
    double HalfWidth=0;
};

// Pure visual placement. Callers supply only already-known occupied bounds.
// Failure hides that representation rather than drawing it through a building.
inline bool SeigeFindExteriorPosition(FVector2D Owner,double OwnerHalfWidth,int32 Slot,double Clearance,const TArray<FSeigeWorksiteBounds>& Occupied,FVector2D& Out)
{
    const double Pitch=Clearance*2+12;
    const int32 Lanes[]={0,-1,1,-2,2};
    for(int32 Ring=0;Ring<6;++Ring)for(int32 SideOffset=0;SideOffset<4;++SideOffset)for(int32 LaneOffset=0;LaneOffset<5;++LaneOffset)
    {
        const int32 Side=(FMath::Max(0,Slot)/5+SideOffset)%4,Lane=(FMath::Max(0,Slot)%5+LaneOffset)%5;
        const double Across=Lanes[Lane]*Pitch,Radius=OwnerHalfWidth+Clearance+20+Ring*Pitch;
        const FVector2D Local=Side==0?FVector2D(Across,-Radius):Side==1?FVector2D(Radius,Across):Side==2?FVector2D(Across,Radius):FVector2D(-Radius,Across);
        const FVector2D Candidate=Owner+Local;bool Clear=true;
        for(const auto& Area:Occupied)
        {
            const FVector2D Delta=Candidate-Area.Position;
            if(FMath::Abs(Delta.X)<Area.HalfWidth+Clearance&&FMath::Abs(Delta.Y)<Area.HalfWidth+Clearance){Clear=false;break;}
        }
        if(Clear){Out=Candidate;return true;}
    }
    return false;
}
