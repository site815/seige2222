#pragma once
#include "CoreMinimal.h"

// Two physical large banks, each four small sockets across. Results retain
// input weapon indices: X is bank, Y is column, Z is row. Invalid inputs clear
// the output rather than leaving a partial or overlapping visual layout.
bool SeigePackHardpointBanks(const TArray<int32>& SocketWidths,TArray<FIntVector>& OutBankCells);
