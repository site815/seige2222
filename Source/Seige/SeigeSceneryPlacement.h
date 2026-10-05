#pragma once

#include "CoreMinimal.h"

// One stable candidate per lightly jittered stratum. Independent uniform draws
// clustered dense clumps together, leaving long exposed gaps at grazing views.
// Unequal row lengths retain the exact requested count, including non-squares.
inline FVector2D SeigeSwardCandidate(int32 Index,int32 Count,FVector2D RandomFraction)
{
    if(Count<=0||Index<0||Index>=Count)return FVector2D::ZeroVector;
    const int32 Rows=FMath::Max(1,FMath::RoundToInt(FMath::Sqrt(double(Count))));
    const int32 Row=int32((int64(Index+1)*Rows-1)/Count);
    const int32 Start=int32(int64(Row)*Count/Rows),End=int32(int64(Row+1)*Count/Rows),Columns=End-Start;
    const double JitterX=(FMath::Clamp(RandomFraction.X,0.,1.)-.5)*.5;
    const double JitterY=(FMath::Clamp(RandomFraction.Y,0.,1.)-.5)*.5;
    const double Stagger=(Row&1)? .5:0.;
    return FVector2D(FMath::Frac((Index-Start+.5+Stagger+JitterX)/Columns),(Row+.5+JitterY)/Rows);
}
