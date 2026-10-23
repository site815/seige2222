#include "SeigeAudioPolicy.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAudioRateTest,"Seige.Audio.RealTimeRateAndFunctionalFeedback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAudioRateTest::RunTest(const FString&)
{
    FSeigeAudioRateGate Gate;Gate.MaximumPerFrame=1;Gate.MaximumPerSecond=2;
    for(int32 I=0;I<50;++I)TestFalse(TEXT("An explicitly muted user mix never consumes an audible budget"),Gate.TryPlay(TEXT("select"),TEXT("ui"),0,0,0,0,0));
    TestTrue(TEXT("A real cue still plays after muted feedback"),Gate.TryPlay(TEXT("laser"),TEXT("combat"),.2f,0,0,0,0));
    TestFalse(TEXT("Frame limit spans UI calls and world tick"),Gate.TryPlay(TEXT("complete"),TEXT("construction"),.2f,.01,0,0,0));
    TestTrue(TEXT("Next frame admits a second voice"),Gate.TryPlay(TEXT("laser"),TEXT("combat"),.2f,.1,1,0,0));
    TestFalse(TEXT("Real-time rolling window rejects a third voice"),Gate.TryPlay(TEXT("alert"),TEXT("alert"),.2f,.2,2,0,0));
    TestTrue(TEXT("Oldest voice expires at exactly one real second"),Gate.TryPlay(TEXT("alert"),TEXT("alert"),.2f,1.,3,0,0));

    int32 Baseline=INDEX_NONE;
    for(int32 Speed:{1,5,10,20})
    {
        FSeigeAudioRateGate Burst;int32 Audible=0;
        for(uint64 Frame=0;Frame<180;++Frame)
            for(int32 Event=0;Event<Speed;++Event)
                Audible+=Burst.TryPlay(Event%2?TEXT("laser"):TEXT("kinetic"),TEXT("combat"),.2f,double(Frame)/60.,Frame,.2,.25)?1:0;
        if(Baseline==INDEX_NONE)Baseline=Audible;
        TestEqual(FString::Printf(TEXT("%dx catch-up event bursts retain the same audible real-time cadence"),Speed),Audible,Baseline);
    }
    TestTrue(TEXT("Combat cadence stays at most four cues per real second"),Baseline<=12);

    FSeigeAudioRateGate Mixed;Mixed.MaximumPerFrame=1;Mixed.MaximumPerSecond=1;
    TestTrue(TEXT("World shot consumes the entire configured world budget"),Mixed.TryPlay(TEXT("laser"),TEXT("combat"),.3f,0,0,0,0));
    TestFalse(TEXT("Excess world shots are dropped"),Mixed.TryPlay(TEXT("kinetic"),TEXT("combat"),.3f,0,0,0,0));
    TestTrue(TEXT("An audible UI selection survives saturated combat"),Mixed.TryPlay(TEXT("select"),TEXT("ui"),.3f,0,0,.09,.075,ESeigeAudioLane::Interface));
    TestTrue(TEXT("Important damage alert survives saturated combat and UI"),Mixed.TryPlay(TEXT("alert"),TEXT("alert"),.5f,0,0,30,30,ESeigeAudioLane::Notification));
    TestFalse(TEXT("Repeated selection does not chatter"),Mixed.TryPlay(TEXT("select"),TEXT("ui"),.3f,.02,1,.09,.075,ESeigeAudioLane::Interface));
    TestTrue(TEXT("A distinct accepted command remains responsive"),Mixed.TryPlay(TEXT("confirm"),TEXT("ui"),.3f,.12,2,.12,.075,ESeigeAudioLane::Interface));
    TestFalse(TEXT("The alert itself still has a cooldown"),Mixed.TryPlay(TEXT("alert"),TEXT("alert"),.5f,.5,3,30,30,ESeigeAudioLane::Notification));
    TestEqual(TEXT("Successful placement chooses the actual placement cue"),SeigeAudio::ResultCue(true,TEXT("build")),FName(TEXT("build")));
    TestEqual(TEXT("Rejected placement chooses error instead of an acceptance sound"),SeigeAudio::ResultCue(false,TEXT("build")),FName(TEXT("error")));
    TestEqual(TEXT("Other successful transactions use confirm"),SeigeAudio::ResultCue(true),FName(TEXT("confirm")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeAudioIncidentTest,"Seige.Audio.DamageIncidentsAndSectorPositions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeAudioIncidentTest::RunTest(const FString&)
{
    FSeigeAudioDamageIncident Damage;
    TestFalse(TEXT("Quiet frames do not create alerts"),Damage.Observe(false,0,30));
    TestTrue(TEXT("First damage announces the incident"),Damage.Observe(true,0,30));
    for(int32 Second=1;Second<=120;++Second)TestFalse(TEXT("Continuous fighting never restarts the alarm tone"),Damage.Observe(true,Second,30));
    TestFalse(TEXT("Silence does not itself play an alert"),Damage.Observe(false,151,30));
    TestTrue(TEXT("New damage after thirty quiet seconds is a new incident"),Damage.Observe(true,151,30));
    Damage.Reset();TestTrue(TEXT("New scenario resets the old incident"),Damage.Observe(true,152,30));
    const FVector2D Local(12,34);constexpr double Half=1000;
    TestTrue(TEXT("Home projectiles keep their local position"),SeigeAudio::ProjectileWorldPosition(Local,4,Half).Equals(Local));
    TestTrue(TEXT("East-sector projectile audio stays in the east sector"),SeigeAudio::ProjectileWorldPosition(Local,5,Half).Equals(Local+FVector2D(2000,0)));
    TestTrue(TEXT("Northwest projectile audio stays in its actual sector"),SeigeAudio::ProjectileWorldPosition(Local,0,Half).Equals(Local-FVector2D(2000,2000)));
    return true;
}
#endif
