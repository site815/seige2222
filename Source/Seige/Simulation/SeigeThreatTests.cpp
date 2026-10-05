#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags ThreatFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString ThreatRules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
FString ThreatSave(const FString& Name){return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/ThreatSettings"),Name+TEXT(".json"));}
bool ReadThreatJson(const FString& Filename,TSharedPtr<FJsonObject>& Object)
{FString Raw;return FFileHelper::LoadFileToString(Raw,*Filename)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Object)&&Object.IsValid();}
bool WriteThreatJson(const FString& Filename,const TSharedPtr<FJsonObject>& Object)
{FString Raw;return FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw))&&FFileHelper::SaveStringToFile(Raw,*Filename);}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeIndependentThreatTest,"Seige.Simulation.IndependentScenarioThreats",ThreatFlags)
bool FSeigeIndependentThreatTest::RunTest(const FString& Parameters)
{
    TSharedPtr<FJsonObject> PolicyDocument;
    if(!ReadThreatJson(FPaths::Combine(ThreatRules(),TEXT("policies.json")),PolicyDocument)){AddError(TEXT("Cannot read threat timing fixture"));return false;}
    const auto Policy=PolicyDocument->GetObjectField(TEXT("policies"));
    const double FirstRoam=Policy->GetNumberField(TEXT("roam_first_time")),FirstWave=Policy->GetNumberField(TEXT("wave_first_time"));
    if(!TestTrue(TEXT("Authored opening exposes background pressure before the first pulse"),FirstRoam<FirstWave))return false;
    FString Error;FSeigeSimulation Default;
    if(!Default.Initialize(ThreatRules(),Error)){AddError(Error);return false;}
    TestTrue(TEXT("Existing callers retain both threats by default"),Default.BackgroundBugsEnabled&&Default.PeriodicAttacksEnabled);
    for(int32 Combination=0;Combination<4;++Combination)
    {
        const bool Background=(Combination&1)!=0,Periodic=(Combination&2)!=0;
        FSeigeSimulation A,B;
        if(!A.Initialize(ThreatRules(),Error,Background,Periodic)){AddError(Error);return false;}
        A.Tick(FirstRoam+A.FixedStepSeconds());
        TestEqual(FString::Printf(TEXT("Combination %d: only the background switch controls roaming spawns"),Combination),A.Enemies.Num(),Background?Policy->GetIntegerField(TEXT("roam_count")):0);
        TestEqual(TEXT("No early periodic pulse is introduced"),A.Wave,0);
        A.Tick(FirstWave+A.FixedStepSeconds()-A.Time);
        TestEqual(FString::Printf(TEXT("Combination %d: only the periodic switch controls scheduled pulses"),Combination),A.Wave,Periodic?1:0);
        if(Periodic)TestTrue(TEXT("An enabled scheduled pulse creates real enemies"),A.Enemies.Num()>0);
        if(!Background&&!Periodic)TestTrue(TEXT("Both disabled leaves the colony free of spawned bugs"),A.Enemies.IsEmpty());
        const FString Save=ThreatSave(FString::Printf(TEXT("combination-%d"),Combination));
        if(!A.Save(Save,Error)||!B.Initialize(ThreatRules(),Error,!Background,!Periodic)||!B.Load(Save,Error)){AddError(Error);return false;}
        TestTrue(TEXT("Loading overrides current initialization choices with the saved pair"),B.BackgroundBugsEnabled==Background&&B.PeriodicAttacksEnabled==Periodic);
        const double Continuation=A.NextWaveTime+A.FixedStepSeconds()-A.Time;
        A.Tick(Continuation);B.Tick(Continuation);
        if(!A.Save(ThreatSave(TEXT("uninterrupted")),Error)||!B.Save(ThreatSave(TEXT("restored")),Error)){AddError(Error);return false;}
        FString Left,Right;FFileHelper::LoadFileToString(Left,*ThreatSave(TEXT("uninterrupted")));FFileHelper::LoadFileToString(Right,*ThreatSave(TEXT("restored")));
        TestEqual(TEXT("Threat choices, schedules, RNG and combat continue identically after save/load"),Left,Right);
        if(!Periodic)
        {
            const int32 Count=A.Enemies.Num();A.TriggerWave();
            TestEqual(TEXT("Explicit pulse dispatch also honors a disabled scenario rule"),A.Enemies.Num(),Count);
            TestEqual(TEXT("Disabled pulses never advance the wave counter"),A.Wave,0);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeThreatSaveValidationTest,"Seige.Simulation.ThreatSaveValidation",ThreatFlags)
bool FSeigeThreatSaveValidationTest::RunTest(const FString& Parameters)
{
    FString Error;FSeigeSimulation S;
    if(!S.Initialize(ThreatRules(),Error,false,false)){AddError(Error);return false;}
    S.Tick(50);
    const FString Original=ThreatSave(TEXT("strict-original")),Invalid=ThreatSave(TEXT("strict-invalid"));
    if(!S.Save(Original,Error)){AddError(Error);return false;}
    FString Before;FFileHelper::LoadFileToString(Before,*Original);
    for(int32 Variant=0;Variant<6;++Variant)
    {
        TSharedPtr<FJsonObject> Object;if(!ReadThreatJson(Original,Object))return false;
        if(Variant==0)Object->RemoveField(TEXT("background_bugs"));
        if(Variant==1)Object->RemoveField(TEXT("periodic_attacks"));
        if(Variant==2)Object->SetStringField(TEXT("background_bugs"),TEXT("false"));
        if(Variant==3)Object->SetNumberField(TEXT("periodic_attacks"),0);
        if(Variant==4)Object->SetField(TEXT("background_bugs"),MakeShared<FJsonValueNull>());
        if(Variant==5)Object->SetArrayField(TEXT("periodic_attacks"),TArray<TSharedPtr<FJsonValue>>());
        if(!WriteThreatJson(Invalid,Object))return false;
        TestFalse(TEXT("Missing or non-boolean threat flag is rejected"),S.Load(Invalid,Error));
        TestTrue(TEXT("Rejection explains the invalid threat settings"),Error.Contains(TEXT("threat settings")));
        if(!S.Save(ThreatSave(TEXT("strict-after")),Error))return false;
        FString After;FFileHelper::LoadFileToString(After,*ThreatSave(TEXT("strict-after")));
        TestEqual(TEXT("Failed loading preserves the whole running simulation"),After,Before);
    }
    TSharedPtr<FJsonObject> Legacy;if(!ReadThreatJson(Original,Legacy))return false;
    Legacy->RemoveField(TEXT("background_bugs"));Legacy->RemoveField(TEXT("periodic_attacks"));
    if(!WriteThreatJson(Invalid,Legacy)||!S.Load(Invalid,Error)){AddError(Error);return false;}
    TestTrue(TEXT("Compatible legacy snapshots with neither field use the original enabled behavior"),S.BackgroundBugsEnabled&&S.PeriodicAttacksEnabled);
    return true;
}
#endif
