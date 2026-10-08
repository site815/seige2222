#include "SeigeCalendarRules.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

bool LoadSeigeCalendarRules(const TSharedPtr<FJsonObject>& Document,FSeigeWorldCalendar& Calendar,FString& Error)
{
    const TSharedPtr<FJsonObject>* Object=nullptr;
    if(!Document || !Document->TryGetObjectField(TEXT("calendar"),Object) || !Object || !Object->IsValid()) {Error=TEXT("calendar.json requires calendar object");return false;}
    const auto& Data=*Object;
    FSeigeCalendarRules Rules;
    auto Seconds=[&](const TCHAR* Key,double Minimum,double Maximum,std::int64_t& Value)
    {
        double Number=0;
        if(!Data->TryGetNumberField(Key,Number)||!FMath::IsFinite(Number)||Number<Minimum||Number>Maximum||FMath::Abs(Number*1000000.0-FMath::RoundToDouble(Number*1000000.0))>.001)
        {Error=FString::Printf(TEXT("calendar.%s must be finite and microsecond-representable"),Key);return false;}
        Value=static_cast<std::int64_t>(FMath::RoundToDouble(Number*1000000.0)); return true;
    };
    if(!Seconds(TEXT("daylight_seconds"),1,86400,Rules.DaylightMicroseconds)||!Seconds(TEXT("night_seconds"),1,86400,Rules.NightMicroseconds)||!Seconds(TEXT("initial_elapsed_seconds"),0,9007199254.0,Rules.InitialElapsedMicroseconds))return false;
    double Days=0;FString Curve;const TArray<TSharedPtr<FJsonValue>>* Seasons=nullptr;
    if(!Data->TryGetNumberField(TEXT("days_per_season"),Days)||Days<1||Days>366||Days!=FMath::FloorToDouble(Days)||!Data->TryGetStringField(TEXT("solar_curve"),Curve)||Curve!=TEXT("daylight_half_sine")||!Data->TryGetArrayField(TEXT("seasons"),Seasons)||Seasons->Num()!=4)
    {Error=TEXT("Invalid calendar days, solar curve or seasons");return false;}
    const TCHAR* Names[]={TEXT("spring"),TEXT("summer"),TEXT("autumn"),TEXT("winter")};
    for(int32 I=0;I<4;++I)if((*Seasons)[I]->AsString()!=Names[I]){Error=TEXT("Calendar seasons must be spring, summer, autumn, winter in that order");return false;}
    Rules.DaysPerSeason=static_cast<std::int32_t>(Days);
    if(!Calendar.Configure(Rules)){Error=TEXT("Calendar rule range is invalid");return false;}
    return true;
}
