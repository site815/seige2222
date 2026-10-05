#include "Simulation/SeigeResourceGeneration.h"

int32 SeigeSectorResourceSeed(int32 BaseSeed, int32 SectorIndex)
{
    return static_cast<int32>(static_cast<uint32>(BaseSeed) +
        static_cast<uint32>(SectorIndex - 4) * 0x9e3779b9u);
}

bool GenerateSeigeResourceNodes(const TMap<FString, FSeigeResourceDef>& Resources,
    int32 Seed, double WorldHalfSize, TArray<FSeigeNode>& OutNodes, FString& Error,
    const FSeigeResourceGenerationSettings& Settings)
{
    if (!FMath::IsFinite(WorldHalfSize) || WorldHalfSize <= 0)
    { Error = TEXT("Resource generation requires a positive finite sector half-size"); return false; }
    TArray<FString> Standard, Rare;
    for (const auto& Pair : Resources)
    {
        if (Pair.Value.Class == TEXT("standard")) Standard.Add(Pair.Key);
        else if (Pair.Value.Class == TEXT("rare")) Rare.Add(Pair.Key);
    }
    if (Standard.Num() != 4 || Rare.Num() != 4)
    { Error = TEXT("Resource generation requires exactly four standard and four rare types"); return false; }
    if(Settings.StandardCount<1||Settings.StandardCount>Standard.Num()||Settings.RareCount<1||Settings.RareCount>Rare.Num()||Settings.AttemptBudget<1||Settings.AttemptBudget>4096||!FMath::IsFinite(Settings.InnerAreaFraction)||Settings.InnerAreaFraction<=0||Settings.InnerAreaFraction>1||!FMath::IsFinite(Settings.MinimumSeparationHalfSizeFraction)||Settings.MinimumSeparationHalfSizeFraction<0||Settings.MinimumSeparationHalfSizeFraction>1)
    {Error=TEXT("Resource generation settings are outside supported bounds");return false;}
    Standard.Sort(); Rare.Sort();
    FRandomStream Random(Seed);
    auto Shuffle = [&](TArray<FString>& Types)
    {
        for (int32 I = Types.Num() - 1; I > 0; --I) Types.Swap(I, Random.RandRange(0, I));
    };
    Shuffle(Standard); Shuffle(Rare);
    TArray<FString> Chosen;for(int32 I=0;I<Settings.StandardCount;++I)Chosen.Add(Standard[I]);for(int32 I=0;I<Settings.RareCount;++I)Chosen.Add(Rare[I]);
    const double InnerHalfSize = WorldHalfSize * FMath::Sqrt(Settings.InnerAreaFraction);
    const double MinimumSeparation = WorldHalfSize * Settings.MinimumSeparationHalfSizeFraction;
    TArray<FSeigeNode> Generated;
    for (const FString& Resource : Chosen)
    {
        bool Placed = false;
        for (int32 Attempt = 0; Attempt < Settings.AttemptBudget; ++Attempt)
        {
            const FVector2D Position((Random.GetFraction() * 2. - 1.) * InnerHalfSize,
                (Random.GetFraction() * 2. - 1.) * InnerHalfSize);
            if (Generated.ContainsByPredicate([&](const FSeigeNode& Node)
                { return FVector2D::DistSquared(Node.Position, Position) < FMath::Square(MinimumSeparation); })) continue;
            FSeigeNode Node; Node.Id = Generated.Num() + 1; Node.Resource = Resource; Node.Position = Position;
            Generated.Add(MoveTemp(Node)); Placed = true; break;
        }
        if (!Placed)
        { Error = TEXT("Could not generate separated resource locations within the inner 75% area"); return false; }
    }
    OutNodes = MoveTemp(Generated); Error.Empty(); return true;
}
