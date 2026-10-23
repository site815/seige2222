#pragma once
#include "CoreMinimal.h"

namespace SeigeWindPresentation
{
// Wind is cosmetic: time-lapse speeds make it visibly faster, but should not
// turn branch motion into a high-frequency mechanical vibration. Integrate the
// elapsed calendar time under a wall-clock rate ceiling rather than multiplying
// an absolute clock by the selected speed (which would jump on every change).
inline double AdvanceSeconds(double Previous,double CalendarDelta,double FrameSeconds,double MaximumRate)
{
    if(!FMath::IsFinite(Previous)||!FMath::IsFinite(CalendarDelta)||!FMath::IsFinite(FrameSeconds)||!FMath::IsFinite(MaximumRate))return Previous;
    if(CalendarDelta<=0||FrameSeconds<=0)return Previous;
    const double PlaybackRate=CalendarDelta/FrameSeconds;
    const double CosmeticRate=FMath::Min(PlaybackRate,FMath::Min(FMath::Sqrt(PlaybackRate),FMath::Max(1.,MaximumRate)));
    return Previous+FrameSeconds*CosmeticRate;
}

// A zero cutoff means all visible instances keep their low-frequency motion.
// Fine leaf detail fades in the shader; grass retains its bounded nearby range.
inline int32 DisableDistance(bool Tree,float DistanceMeters,float BoundsRadius)
{
    return Tree?0:FMath::RoundToInt(DistanceMeters*100+BoundsRadius);
}

// The fixed-step remainder is retained while paused. Multiplying an engine Time
// node by the selected speed would jump every time that speed changed instead.
inline float Phase(int64 CalendarMicroseconds,double RemainderSeconds,double FixedStepSeconds,double CycleSeconds)
{
    if(!FMath::IsFinite(RemainderSeconds)||!FMath::IsFinite(CycleSeconds)||CycleSeconds<=0)return 0;
    const double Seconds=double(CalendarMicroseconds)/1000000.+FMath::Clamp(RemainderSeconds,0.,FixedStepSeconds);
    // Only integer harmonics are used by the shader, so every harmonic remains
    // continuous across this wrap and precision survives long persistent games.
    return float(FMath::Fmod(Seconds,CycleSeconds)/CycleSeconds*UE_DOUBLE_TWO_PI);
}
}
