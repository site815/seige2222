#pragma once
#include "CoreMinimal.h"

namespace SeigeWindPresentation
{
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
