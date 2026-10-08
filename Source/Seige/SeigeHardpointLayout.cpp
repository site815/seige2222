#include "SeigeHardpointLayout.h"

bool SeigePackHardpointBanks(const TArray<int32>& SocketWidths,TArray<FIntVector>& OutBankCells)
{
    OutBankCells.Reset();
    constexpr int32 Banks=2,Width=4;
    int32 Area=0;TArray<int32> Order;Order.Reserve(SocketWidths.Num());
    for(int32 Index=0;Index<SocketWidths.Num();++Index)
    {
        const int32 Side=SocketWidths[Index];
        if(Side!=1&&Side!=2&&Side!=4)return false;
        Area+=Side*Side;if(Area>Banks*Width*Width)return false;
        Order.Add(Index);
    }
    // Pack the largest squares before smaller modules can fragment a bank.
    // Sorting only indices preserves weapon/cooldown/shot-event slot identity.
    Order.Sort([&](int32 A,int32 B){return SocketWidths[A]!=SocketWidths[B]?SocketWidths[A]>SocketWidths[B]:A<B;});
    bool Used[Banks][Width][Width]={};
    TArray<FIntVector> Cells;Cells.Init(FIntVector(INDEX_NONE,INDEX_NONE,INDEX_NONE),SocketWidths.Num());
    for(int32 Index:Order)
    {
        const int32 Side=SocketWidths[Index];bool Placed=false;
        for(int32 Bank=0;Bank<Banks&&!Placed;++Bank)
            for(int32 Row=0;Row<=Width-Side&&!Placed;++Row)
                for(int32 Column=0;Column<=Width-Side&&!Placed;++Column)
                {
                    bool Free=true;
                    for(int32 Y=Row;Y<Row+Side;++Y)for(int32 X=Column;X<Column+Side;++X)Free&=!Used[Bank][Y][X];
                    if(!Free)continue;
                    for(int32 Y=Row;Y<Row+Side;++Y)for(int32 X=Column;X<Column+Side;++X)Used[Bank][Y][X]=true;
                    Cells[Index]=FIntVector(Bank,Column,Row);Placed=true;
                }
        if(!Placed)return false;
    }
    OutBankCells=MoveTemp(Cells);return true;
}
