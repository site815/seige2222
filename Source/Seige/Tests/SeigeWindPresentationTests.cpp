#include "Misc/AutomationTest.h"
#include "SeigeWindPresentation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWindPresentationTest,"Seige.Presentation.WindClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeWindPresentationTest::RunTest(const FString&)
{
    constexpr double Cycle=10,Step=.1;
    const auto Phase=[=](double Seconds){const double Fixed=FMath::FloorToDouble(Seconds/Step)*Step;return SeigeWindPresentation::Phase(FMath::RoundToInt64(Fixed*1000000),Seconds-Fixed,Step,Cycle);};
    const double Start=.137;
    for(const double Speed:{1.,5.,10.})
    {
        const double Observed=double(Phase(Start+.05*Speed))-Phase(Start);
        TestTrue(*FString::Printf(TEXT("%.0fx game time gives %.0fx wind slope"),Speed,Speed),FMath::IsNearlyEqual(Observed,.05*Speed/Cycle*UE_DOUBLE_TWO_PI,1.e-6));
    }
    const float Paused=Phase(Start);
    for(int32 Frame=0;Frame<60;++Frame)TestEqual(TEXT("Pause preserves the substep phase without snapping"),Phase(Start),Paused);
    // Changing the control value does not enter the phase calculation. Only the
    // next elapsed interval uses the new rate; the current vertex pose is retained.
    double Time=Start;
    for(const double Speed:{1.,5.,10.,1.})
    {
        const float Before=Phase(Time);
        TestEqual(TEXT("Speed switch has no instantaneous phase offset"),Phase(Time),Before);
        Time+=.02*Speed;
        TestTrue(TEXT("First frame after speed switch advances by the new interval only"),FMath::IsNearlyEqual(double(Phase(Time))-Before,.02*Speed/Cycle*UE_DOUBLE_TWO_PI,1.e-6));
    }
    const float BeforeWrap=Phase(Cycle-.0001),AfterWrap=Phase(Cycle+.0001);
    for(const double Harmonic:{1.,3.,7.})TestTrue(TEXT("Integer harmonic is continuous across cycle wrap"),FMath::Abs(FMath::Sin(BeforeWrap*Harmonic)-FMath::Sin(AfterWrap*Harmonic))<.001);
    const float LongSession=SeigeWindPresentation::Phase(1000000000137000LL,0,Step,Cycle);
    TestTrue(TEXT("Persistent-session time retains sub-second phase precision"),FMath::IsNearlyEqual(LongSession,Phase(Start),1.e-6f));
    return true;
}
