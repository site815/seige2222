#pragma once

#include "CoreMinimal.h"
#include "Simulation/SeigeSimulation.h"

// Pure deterministic layout: catalogue iteration order does not affect the result.
// The centered square occupies 75% of the sector AREA, not 75% of each side.
struct FSeigeResourceGenerationSettings
{
    int32 StandardCount=3, RareCount=2, AttemptBudget=512;
    double InnerAreaFraction=.75, MinimumSeparationHalfSizeFraction=.12;
    TFunction<bool(FVector2D)> CanPlace;
};
SEIGE_API bool GenerateSeigeResourceNodes(const TMap<FString, FSeigeResourceDef>& Resources,
    int32 Seed, double WorldHalfSize, TArray<FSeigeNode>& OutNodes, FString& Error,
    const FSeigeResourceGenerationSettings& Settings=FSeigeResourceGenerationSettings());

// Stable per-region salt; region 4 is the player's center region.
SEIGE_API int32 SeigeSectorResourceSeed(int32 BaseSeed, int32 SectorIndex);
