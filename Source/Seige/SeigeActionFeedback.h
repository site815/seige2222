#pragma once
#include "SeigeAudioSubsystem.h"

// A command's actual return value chooses both its message and audible result.
// Navigation ticks are separate; a rejected order never emits a success cue.
inline FString SeigeActionNotice(UWorld* World,bool Success,const FString& Accepted,const FString& Rejected,FName SuccessCue=TEXT("confirm"))
{
    SeigeAudio::PlayResult(World,Success,SuccessCue);
    return Success?Accepted:Rejected;
}

// Save/load contain several validation exits. Report exactly one result after
// completion without inferring success from localized notice text.
struct FSeigeActionFeedback
{
    explicit FSeigeActionFeedback(UWorld* InWorld):World(InWorld){}
    ~FSeigeActionFeedback(){SeigeAudio::PlayResult(World,Success);}
    UWorld* World=nullptr;
    bool Success=false;
};
