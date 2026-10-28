// Asset-authoring support only: Python sample assignment bypasses the blend
// space editor's native ResampleData call. Nothing in this file is shipped.
#if WITH_EDITOR
#include "Animation/BlendSpace.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
void RebuildRexBlendSpace()
{
    UBlendSpace* Blend=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Art/CompanionDog/BS_DogLocomotion.BS_DogLocomotion"));
    if(!Blend)
    {
        UE_LOG(LogTemp,Error,TEXT("Rex locomotion blend space could not be loaded."));
        return;
    }
    Blend->ValidateSampleData();
    Blend->ResampleData();
    // Check the actual runtime interpolation, including the run sample and
    // its padded upper bound, before allowing the importer to save the cache.
    for(float Speed:{0.f,.6f,5.f/3.6f,2.6f,14.f/3.6f,4.f})
    {
        TArray<FBlendSampleData> Samples;
        int32 CachedIndex=INDEX_NONE;
        if(!Blend->GetSamplesFromBlendInput(FVector(Speed,0,0),Samples,CachedIndex,true)||Samples.IsEmpty())
        {
            UE_LOG(LogTemp,Error,TEXT("Rex locomotion blend interpolation failed at %.3f m/s."),Speed);
            return;
        }
    }
    Blend->MarkPackageDirty();
    UE_LOG(LogTemp,Display,TEXT("SEIGE_REX_BLENDSPACE_REBUILT samples=%d segments=%d"),
        Blend->GetNumberOfBlendSamples(),Blend->GetBlendSpaceData().Segments.Num());
}
FAutoConsoleCommand RexBlendSpaceCommand(TEXT("Seige.RebuildRexBlendSpace"),
    TEXT("Rebuild the authored Rex locomotion blend space after animation-only Python import."),
    FConsoleCommandDelegate::CreateStatic(&RebuildRexBlendSpace));
}
#endif
