#include "Misc/AutomationTest.h"
#include "SeigeWindPresentation.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWindPresentationTest,"Seige.Presentation.WindClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeWindPresentationTest::RunTest(const FString&)
{
    constexpr double Cycle=10,Step=.1;
    const auto Phase=[=](double Seconds){const double Fixed=FMath::FloorToDouble(Seconds/Step)*Step;return SeigeWindPresentation::Phase(FMath::RoundToInt64(Fixed*1000000),Seconds-Fixed,Step,Cycle);};
    const double Start=.137;
    for(const double Speed:{1.,10.,20.})
    {
        const double Observed=double(Phase(Start+.05*Speed))-Phase(Start);
        TestTrue(*FString::Printf(TEXT("%.0fx game time gives %.0fx wind slope"),Speed,Speed),FMath::IsNearlyEqual(Observed,.05*Speed/Cycle*UE_DOUBLE_TWO_PI,1.e-6));
    }
    const float Paused=Phase(Start);
    for(int32 Frame=0;Frame<60;++Frame)TestEqual(TEXT("Pause preserves the substep phase without snapping"),Phase(Start),Paused);
    // Changing the control value does not enter the phase calculation. Only the
    // next elapsed interval uses the new rate; the current vertex pose is retained.
    double Time=Start;
    for(const double Speed:{1.,10.,20.,1.})
    {
        const float Before=Phase(Time);
        TestEqual(TEXT("Speed switch has no instantaneous phase offset"),Phase(Time),Before);
        Time+=.02*Speed;
        TestTrue(TEXT("First frame after speed switch advances by the new interval only"),FMath::IsNearlyEqual(double(Phase(Time))-Before,.02*Speed/Cycle*UE_DOUBLE_TWO_PI,1.e-6));
    }
    const float BeforeWrap=Phase(Cycle-.0001),AfterWrap=Phase(Cycle+.0001);
    for(const double Harmonic:{1.,2.,3.})TestTrue(TEXT("Integer harmonic is continuous across cycle wrap"),FMath::Abs(FMath::Sin(BeforeWrap*Harmonic)-FMath::Sin(AfterWrap*Harmonic))<.001);
    const float LongSession=SeigeWindPresentation::Phase(1000000000137000LL,0,Step,Cycle);
    TestTrue(TEXT("Persistent-session time retains sub-second phase precision"),FMath::IsNearlyEqual(LongSession,Phase(Start),1.e-6f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWindTimelapseTest,"Seige.Presentation.WindTimelapse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeWindTimelapseTest::RunTest(const FString&)
{
    constexpr double Limit=4;
    for(const double Speed:{1.,10.,20.})
    {
        // The same one-second interval must have the same final pose at 30 and
        // 120 FPS, with real sub-frame calendar time rather than fixed-step hops.
        double End30=17.137,End120=End30;
        for(int32 Frame=0;Frame<30;++Frame)End30=SeigeWindPresentation::AdvanceSeconds(End30,Speed/30.,1./30.,Limit);
        for(int32 Frame=0;Frame<120;++Frame)End120=SeigeWindPresentation::AdvanceSeconds(End120,Speed/120.,1./120.,Limit);
        TestTrue(TEXT("Wind rate is independent of render frame rate"),FMath::IsNearlyEqual(End30,End120,1.e-10));
        const double Expected=FMath::Min(FMath::Sqrt(Speed),Limit);
        TestTrue(*FString::Printf(TEXT("%.0fx time-lapse advances at %.3fx cosmetic wind speed"),Speed,Expected),FMath::IsNearlyEqual(End30-17.137,Expected,1.e-10));
    }
    double Time=13.137;
    for(const double Speed:{1.,10.,0.,20.,1.})
    {
        const double Before=Time;
        Time=SeigeWindPresentation::AdvanceSeconds(Time,Speed*.02,.02,Limit);
        TestTrue(TEXT("Speed changes never reset or reverse the visual clock"),Time>=Before&&Time-Before<=.08000001);
        if(Speed==0)TestEqual(TEXT("Paused calendar freezes exact crown pose"),Time,Before);
    }
    TestEqual(TEXT("A calendar reset cannot create reverse motion"),SeigeWindPresentation::AdvanceSeconds(Time,-10,.02,Limit),Time);
    TestEqual(TEXT("A zero-duration frame cannot jump the crown pose"),SeigeWindPresentation::AdvanceSeconds(Time,10,0,Limit),Time);
    return true;
}
