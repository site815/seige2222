#include "SeigeWorldCalendar.h"
#include <algorithm>
#include <cmath>

namespace { constexpr double CalendarPi=3.14159265358979323846; }
bool FSeigeWorldCalendar::Configure(const FSeigeCalendarRules& InRules)
{
    if(InRules.DaylightMicroseconds<1000000 || InRules.NightMicroseconds<1000000 ||
       InRules.DaylightMicroseconds>86400000000LL || InRules.NightMicroseconds>86400000000LL ||
       InRules.DaysPerSeason<1 || InRules.DaysPerSeason>366 ||
       InRules.InitialElapsedMicroseconds<0 || InRules.InitialElapsedMicroseconds>MaximumElapsedMicroseconds) return false;
    Rules=InRules; Elapsed=Rules.InitialElapsedMicroseconds; return true;
}
bool FSeigeWorldCalendar::SetElapsedMicroseconds(std::int64_t Value)
{
    if(Value<0 || Value>MaximumElapsedMicroseconds) return false;
    Elapsed=Value; return true;
}
bool FSeigeWorldCalendar::Duration(double Seconds,std::int64_t& Microseconds)
{
    if(!std::isfinite(Seconds) || Seconds<0 || Seconds>static_cast<double>(MaximumElapsedMicroseconds)/1000000.0) return false;
    const double Exact=Seconds*1000000.0;
    Microseconds=static_cast<std::int64_t>(std::llround(Exact));
    // Round runtime residuals to clock precision; long double-based Tick loops
    // can leave a final substep a few nanoseconds off the authored cadence.
    return Seconds==0 || Microseconds>0;
}
bool FSeigeWorldCalendar::Advance(double Seconds)
{
    std::int64_t Delta=0;
    if(!Duration(Seconds,Delta) || Delta>MaximumElapsedMicroseconds-Elapsed) return false;
    Elapsed+=Delta; return true;
}
FSeigeCalendarSample FSeigeWorldCalendar::Sample() const
{
    FSeigeCalendarSample Out;
    const auto Cycle=Rules.DaylightMicroseconds+Rules.NightMicroseconds;
    const auto Phase=Elapsed%Cycle;
    Out.DayIndex=Elapsed/Cycle;
    Out.SeasonIndex=static_cast<std::int32_t>((Out.DayIndex/Rules.DaysPerSeason)%4);
    Out.DayOfSeason=static_cast<std::int32_t>(Out.DayIndex%Rules.DaysPerSeason)+1;
    Out.CycleFraction=static_cast<double>(Phase)/static_cast<double>(Cycle);
    Out.SeasonFraction=(Out.DayOfSeason-1+Out.CycleFraction)/Rules.DaysPerSeason;
    Out.IsDay=Phase<Rules.DaylightMicroseconds;
    Out.SolarFactor=Out.IsDay?std::sin(CalendarPi*static_cast<double>(Phase)/Rules.DaylightMicroseconds):0.0;
    return Out;
}
double FSeigeWorldCalendar::SolarIntegral(std::int64_t Start,std::int64_t End) const
{
    const auto A=std::min(Start,Rules.DaylightMicroseconds);
    const auto B=std::min(End,Rules.DaylightMicroseconds);
    if(B<=A) return 0;
    const double Day=static_cast<double>(Rules.DaylightMicroseconds);
    return 2*Day/CalendarPi*std::sin(CalendarPi*(A+B)/(2*Day))*std::sin(CalendarPi*(B-A)/(2*Day));
}
double FSeigeWorldCalendar::MeanSolarFactor(double Seconds) const
{
    std::int64_t Delta=0;
    if(!Duration(Seconds,Delta) || Delta>MaximumElapsedMicroseconds-Elapsed) return 0;
    if(Delta==0) return Sample().SolarFactor;
    const auto Cycle=Rules.DaylightMicroseconds+Rules.NightMicroseconds;
    const auto Phase=Elapsed%Cycle;
    const auto First=std::min(Delta,Cycle-Phase);
    double Integral=SolarIntegral(Phase,Phase+First);
    const auto Remaining=Delta-First;
    Integral+=(Remaining/Cycle)*(2.0*Rules.DaylightMicroseconds/CalendarPi);
    Integral+=SolarIntegral(0,Remaining%Cycle);
    return std::clamp(Integral/static_cast<double>(Delta),0.0,1.0);
}
