#include "Simulation/SeigeResourceGeneration.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeResourceGenerationTest,"Seige.Simulation.SeededResourceRegions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeResourceGenerationTest::RunTest(const FString& Parameters)
{
    FSeigeSimulation Sim; FString Error;
    if(!Sim.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    TSet<FString> Signatures,TypeSets;
    for(int32 Seed=0;Seed<128;++Seed)
    {
        TArray<FSeigeNode> A,B;
        if(!GenerateSeigeResourceNodes(Sim.Resources,Seed,Sim.WorldHalfSize,A,Error)||!GenerateSeigeResourceNodes(Sim.Resources,Seed,Sim.WorldHalfSize,B,Error)){AddError(Error);return false;}
        TestEqual(TEXT("Exactly five deposits"),A.Num(),5);int32 Standard=0,Rare=0;TSet<FString> Types;FString Signature;
        for(int32 I=0;I<A.Num();++I)
        {
            const auto& N=A[I];const FString Class=Sim.Resources[N.Resource].Class;
            Standard+=Class==TEXT("standard");Rare+=Class==TEXT("rare");Types.Add(N.Resource);
            TestEqual(TEXT("Same seed repeats resource"),N.Resource,B[I].Resource);
            TestEqual(TEXT("Same seed repeats position"),N.Position,B[I].Position);
            TestTrue(TEXT("Deposit lies in centered square occupying 75% area"),FMath::Abs(N.Position.X)<=Sim.WorldHalfSize*FMath::Sqrt(.75)&&FMath::Abs(N.Position.Y)<=Sim.WorldHalfSize*FMath::Sqrt(.75));
            for(int32 J=0;J<I;++J)TestTrue(TEXT("Locations have useful separation"),FVector2D::Distance(N.Position,A[J].Position)>=Sim.WorldHalfSize*.12);
            Signature+=N.Resource+N.Position.ToString();
        }
        TestEqual(TEXT("Three standard deposits"),Standard,3);TestEqual(TEXT("Two rare deposits"),Rare,2);TestEqual(TEXT("No duplicate resource type"),Types.Num(),5);
        TArray<FString> Sorted=Types.Array();Sorted.Sort();TypeSets.Add(FString::Join(Sorted,TEXT(",")));Signatures.Add(Signature);
    }
    TestEqual(TEXT("Distinct seeds create distinct layouts"),Signatures.Num(),128);
    TestTrue(TEXT("Generator samples multiple resource subsets"),TypeSets.Num()>10);
    TMap<FString,FSeigeResourceDef> Reverse;TArray<FString> Keys;Sim.Resources.GetKeys(Keys);Keys.Sort([](const FString&A,const FString&B){return A>B;});for(const auto& K:Keys)Reverse.Add(K,Sim.Resources[K]);
    TArray<FSeigeNode> A,B;GenerateSeigeResourceNodes(Sim.Resources,815,Sim.WorldHalfSize,A,Error);GenerateSeigeResourceNodes(Reverse,815,Sim.WorldHalfSize,B,Error);
    for(int32 I=0;I<A.Num();++I){TestEqual(TEXT("Catalog insertion order does not change resource"),A[I].Resource,B[I].Resource);TestEqual(TEXT("Catalog insertion order does not change position"),A[I].Position,B[I].Position);}
    Reverse.Remove(TEXT("water"));TestFalse(TEXT("Incomplete raw pool is rejected"),GenerateSeigeResourceNodes(Reverse,815,Sim.WorldHalfSize,B,Error));TestEqual(TEXT("Invalid generation preserves prior output"),B.Num(),5);
    TSet<int32> RegionSeeds;for(int32 I=0;I<9;++I)RegionSeeds.Add(SeigeSectorResourceSeed(815,I));TestEqual(TEXT("Nine distinct region seeds"),RegionSeeds.Num(),9);TestEqual(TEXT("Center preserves scenario seed"),SeigeSectorResourceSeed(815,4),815);
    return true;
}
#endif
