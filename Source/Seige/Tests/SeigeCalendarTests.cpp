#include "Misc/AutomationTest.h"
#include "Simulation/SeigeWorldCalendar.h"
#include <cmath>
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCalendarBoundaries,"Seige.Calendar.BoundariesAndSeasons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCalendarBoundaries::RunTest(const FString&)
{
    FSeigeWorldCalendar C;
    TestTrue(TEXT("Dawn starts in spring"),C.Sample().IsDay&&C.Sample().SeasonIndex==0&&C.Sample().DayOfSeason==1);
    TestTrue(TEXT("Reach midday"),C.Advance(900));TestTrue(TEXT("Midday full solar"),std::abs(C.Sample().SolarFactor-1)<1.e-12);
    C.Advance(900);TestFalse(TEXT("Sunset belongs to night"),C.Sample().IsDay);TestEqual(TEXT("Night solar zero"),C.MeanSolarFactor(1800),0.0);
    C.Advance(1800);TestEqual(TEXT("Following dawn"),int64(C.Sample().DayIndex),int64(1));
    for(int32 Season=0;Season<4;++Season){C.SetElapsedMicroseconds(int64(Season)*30*3600*1000000LL);TestEqual(TEXT("Exact season boundary"),C.Sample().SeasonIndex,Season);TestEqual(TEXT("Season starts day one"),C.Sample().DayOfSeason,1);}
    C.SetElapsedMicroseconds(120LL*3600*1000000);TestEqual(TEXT("Year wraps spring"),C.Sample().SeasonIndex,0);
    const auto Before=C.ElapsedMicroseconds();TestFalse(TEXT("Reject negative"),C.Advance(-1));TestFalse(TEXT("Reject nonfinite"),C.Advance(std::numeric_limits<double>::infinity()));TestFalse(TEXT("Reject positive duration below clock precision"),C.Advance(.0000001));TestEqual(TEXT("Invalid step is atomic"),int64(C.ElapsedMicroseconds()),int64(Before));
    C.SetElapsedMicroseconds(FSeigeWorldCalendar::MaximumElapsedMicroseconds);TestFalse(TEXT("Reject clock overflow"),C.Advance(.000001));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCalendarSolar,"Seige.Calendar.SolarIntegrationAndClockPartition",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCalendarSolar::RunTest(const FString&)
{
    FSeigeWorldCalendar Whole,Stepped;
    constexpr double Pi=3.14159265358979323846;
    TestTrue(TEXT("Half-sine daily average"),std::abs(Whole.MeanSolarFactor(3600)-1/Pi)<1.e-12);
    TestTrue(TEXT("25 kW array daily energy"),std::abs(25*Whole.MeanSolarFactor(3600)-25/Pi)<1.e-12);
    Whole.SetElapsedMicroseconds(1799900000);Stepped=Whole;
    const double Expected=Whole.MeanSolarFactor(3600.2)*3600.2;
    double Integrated=0;
    for(int32 I=0;I<72004;++I){Integrated+=Stepped.MeanSolarFactor(.05)*.05;Stepped.Advance(.05);}
    Whole.Advance(3600.2);
    TestTrue(TEXT("Boundary-crossing energy is step independent"),std::abs(Integrated-Expected)<1.e-7);
    TestEqual(TEXT("1x/5x/10x batches preserve calendar microseconds"),int64(Whole.ElapsedMicroseconds()),int64(Stepped.ElapsedMicroseconds()));
    FSeigeWorldCalendar Restored;Restored.Configure(Stepped.GetRules());Restored.SetElapsedMicroseconds(Stepped.ElapsedMicroseconds());
    TestEqual(TEXT("Restored phase exact"),Restored.Sample().SolarFactor,Stepped.Sample().SolarFactor);
    return true;
}
