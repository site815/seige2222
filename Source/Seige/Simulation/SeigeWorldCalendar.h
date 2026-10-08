#pragma once
#include <cstdint>

// Simulation-only arithmetic. Colony age and renderer time are deliberately separate.
struct FSeigeCalendarRules
{
    std::int64_t DaylightMicroseconds=1800000000LL;
    std::int64_t NightMicroseconds=1800000000LL;
    std::int32_t DaysPerSeason=30;
    std::int64_t InitialElapsedMicroseconds=0;
};
struct FSeigeCalendarSample
{
    std::int64_t DayIndex=0;
    std::int32_t SeasonIndex=0, DayOfSeason=1;
    double CycleFraction=0, SeasonFraction=0, SolarFactor=0;
    bool IsDay=true;
};
class FSeigeWorldCalendar
{
public:
    static constexpr std::int64_t MaximumElapsedMicroseconds=9007199254740991LL;
    bool Configure(const FSeigeCalendarRules& InRules);
    bool SetElapsedMicroseconds(std::int64_t Value);
    bool Advance(double Seconds);
    std::int64_t ElapsedMicroseconds() const { return Elapsed; }
    const FSeigeCalendarRules& GetRules() const { return Rules; }
    FSeigeCalendarSample Sample() const;
    double MeanSolarFactor(double Seconds) const;
private:
    FSeigeCalendarRules Rules;
    std::int64_t Elapsed=0;
    static bool Duration(double Seconds,std::int64_t& Microseconds);
    double SolarIntegral(std::int64_t Start,std::int64_t End) const;
};
