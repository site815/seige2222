#pragma once
#include "CoreMinimal.h"
#include "SeigeWorldCalendar.h"
class FJsonObject;
bool LoadSeigeCalendarRules(const TSharedPtr<FJsonObject>& Document,FSeigeWorldCalendar& Calendar,FString& Error);
