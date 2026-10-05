#pragma once

#include "CoreMinimal.h"

// A rigid clump can straddle several terrain triangles even when its pivot is
// exactly on the ground. Fit its authored root plane, not its sphere bounds or
// leaf tips. All coordinates and the height callback use rendered centimeters.
inline FTransform FitSeigeSceneryRootPlane(const FTransform& Transform,const FBox& SourceBounds,
    TFunctionRef<double(FVector2D)> SurfaceHeight)
{
    FTransform Result=Transform;
    if(!SourceBounds.IsValid)return Result;
    double Penetration=0;
    for(int32 Y=0;Y<3;++Y)for(int32 X=0;X<3;++X)
    {
        const FVector Root(FMath::Lerp(SourceBounds.Min.X,SourceBounds.Max.X,double(X)*.5),
            FMath::Lerp(SourceBounds.Min.Y,SourceBounds.Max.Y,double(Y)*.5),SourceBounds.Min.Z);
        const FVector World=Transform.TransformPosition(Root);
        Penetration=FMath::Max(Penetration,SurfaceHeight(FVector2D(World))-World.Z);
    }
    // No blanket offset: planar contact remains unchanged. XY, orientation,
    // scale and the underlying terrain are never modified by this correction.
    Result.AddToTranslation(FVector(0,0,Penetration));
    return Result;
}
