#include "SeigeHardpointLayout.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCoreHardpointLayoutTest,"Seige.Presentation.CoreHardpointLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCoreHardpointLayoutTest::RunTest(const FString& Parameters)
{
    auto Check=[&](const TArray<int32>& Sizes,const FString& Label)
    {
        const TArray<int32> Before=Sizes;TArray<FIntVector> Cells,Again;
        if(!TestTrue(Label+TEXT(" packs"),SeigePackHardpointBanks(Sizes,Cells)))return false;
        if(!TestEqual(Label+TEXT(" retains all weapon slots"),Cells.Num(),Sizes.Num()))return false;
        bool Used[2][4][4]={};
        for(int32 Index=0;Index<Sizes.Num();++Index)
        {
            const auto& P=Cells[Index];const int32 Side=Sizes[Index];
            if(!TestTrue(Label+TEXT(" keeps original slot within its bank"),P.X>=0&&P.X<2&&P.Y>=0&&P.Z>=0&&P.Y+Side<=4&&P.Z+Side<=4))return false;
            for(int32 Y=P.Z;Y<P.Z+Side;++Y)for(int32 X=P.Y;X<P.Y+Side;++X)
            {if(!TestFalse(Label+TEXT(" sockets never overlap"),Used[P.X][Y][X]))return false;Used[P.X][Y][X]=true;}
        }
        TestTrue(Label+TEXT(" does not reorder caller weapons"),Sizes==Before);
        TestTrue(Label+TEXT(" repacks deterministically"),SeigePackHardpointBanks(Sizes,Again)&&Cells==Again);
        return true;
    };
    const TArray<int32> Fragmented={1,1,1,1,2,2,2,4};
    if(!Check(Fragmented,TEXT("Four small then three medium then one large")))return false;
    TArray<FIntVector> Cells;SeigePackHardpointBanks(Fragmented,Cells);
    TestTrue(TEXT("The final large weapon retains slot seven in the first full bank"),Cells[7]==FIntVector(0,0,0));
    if(!Check({4,2,2,1,1,1,1,1,1,1,1},TEXT("Confirmed mixed starting loadout")))return false;
    // Exhaust the authored size counts within two-bank area, including the
    // most fragmented small-first order and the opposite order. Assertions
    // inspect geometric occupancy rather than duplicating the sorting rule.
    for(int32 Large=0;Large<=2;++Large)for(int32 Medium=0;Medium<=8;++Medium)for(int32 Small=0;Small<=32;++Small)
    {
        if(Large*16+Medium*4+Small>32)continue;
        TArray<int32> Sizes;for(int32 I=0;I<Small;++I)Sizes.Add(1);for(int32 I=0;I<Medium;++I)Sizes.Add(2);for(int32 I=0;I<Large;++I)Sizes.Add(4);
        const FString Label=FString::Printf(TEXT("%d large/%d medium/%d small"),Large,Medium,Small);
        if(!Check(Sizes,Label))return false;
        TArray<int32> Reversed;for(int32 I=Sizes.Num()-1;I>=0;--I)Reversed.Add(Sizes[I]);
        if(!Check(Reversed,Label+TEXT(" reversed")))return false;
    }
    for(int32 Invalid:{-1,0,3,5})
    {Cells={FIntVector(1,1,1)};TestFalse(TEXT("Invalid socket width rejected"),SeigePackHardpointBanks({Invalid},Cells));TestTrue(TEXT("Failure clears stale locations"),Cells.IsEmpty());}
    TestFalse(TEXT("Three large weapons exceed two banks"),SeigePackHardpointBanks({4,4,4},Cells));
    TestTrue(TEXT("Overcapacity leaves no partial layout"),Cells.IsEmpty());
    return true;
}
#endif
