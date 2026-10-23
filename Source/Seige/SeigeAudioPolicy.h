#pragma once
#include "CoreMinimal.h"

// Presentation time only: simulation speed and fixed-step catch-up never alter
// the audible rate. Rejected events are discarded rather than deferred.
enum class ESeigeAudioLane : uint8 { World, Interface, Notification };

struct FSeigeAudioRateGate
{
    int32 MaximumPerFrame=3,MaximumPerSecond=5;
    int32 InterfacePerFrame=2,InterfacePerSecond=6,NotificationPerFrame=2,NotificationPerSecond=3;
    bool TryPlay(FName Cue,FName Category,float Gain,double Now,uint64 Frame,double CueCooldown,double CategoryCooldown,ESeigeAudioLane Lane=ESeigeAudioLane::World)
    {
        if(!FMath::IsFinite(Gain)||Gain<=KINDA_SMALL_NUMBER||!FMath::IsFinite(Now))return false;
        // Interface and important notifications have reserved bounded lanes.
        // A busy battlefield cannot consume an error or damage alert's budget.
        auto& Budget=Lanes[int32(Lane)];
        const int32 FrameLimit=Lane==ESeigeAudioLane::Interface?InterfacePerFrame:Lane==ESeigeAudioLane::Notification?NotificationPerFrame:MaximumPerFrame;
        const int32 SecondLimit=Lane==ESeigeAudioLane::Interface?InterfacePerSecond:Lane==ESeigeAudioLane::Notification?NotificationPerSecond:MaximumPerSecond;
        if(Frame!=Budget.LastFrame){Budget.LastFrame=Frame;Budget.FrameVoices=0;}
        if(Budget.FrameVoices>=FrameLimit)return false;
        if(const double* Previous=LastCue.Find(Cue))if(Now-*Previous<CueCooldown)return false;
        if(const double* Previous=LastCategory.Find(Category))if(Now-*Previous<CategoryCooldown)return false;
        Budget.VoiceTimes.RemoveAll([Now](double Time){return Now-Time>=1.;});
        if(Budget.VoiceTimes.Num()>=SecondLimit)return false;
        Budget.VoiceTimes.Add(Now);LastCue.Add(Cue,Now);LastCategory.Add(Category,Now);++Budget.FrameVoices;return true;
    }
private:
    struct FLaneBudget { uint64 LastFrame=MAX_uint64; int32 FrameVoices=0; TArray<double> VoiceTimes; };
    FLaneBudget Lanes[3];
    TMap<FName,double> LastCue,LastCategory;
};

struct FSeigeAudioDamageIncident
{
    bool Observe(bool NewDamage,double Now,double QuietSeconds)
    {
        if(!NewDamage)return false;
        const bool NewIncident=LastDamage<0||Now-LastDamage>=QuietSeconds;
        LastDamage=Now;return NewIncident;
    }
    void Reset(){LastDamage=-1;}
private:
    double LastDamage=-1;
};

namespace SeigeAudio
{
inline FName ResultCue(bool Success,FName SuccessCue=TEXT("confirm"))
{return Success?SuccessCue:FName(TEXT("error"));}
inline FVector2D ProjectileWorldPosition(FVector2D Local,int32 Sector,double SectorHalfSize)
{return Local+FVector2D((Sector%3)-1,(Sector/3)-1)*(SectorHalfSize*2);}
}
