#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
#include "Simulation/SeigeResourceGeneration.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString SaveRoot() { return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames")); }
bool WriteObject(const TSharedPtr<FJsonObject>& Object,const FString& Filename)
{
    FString Text;
    return FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*Filename,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
}
void ASeigeGameMode::SaveGame()
{
    if(!Ready||(Screen!=TEXT("playing")&&!(MenuOpen&&MenuReturnScreen==TEXT("playing")))) { Notice=TEXT("Begin a scenario before saving."); return; }
    const FString Generation=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Directory=FPaths::Combine(SaveRoot(),TEXT("Scenarios"),Generation);
    if(!IFileManager::Get().MakeDirectory(*Directory,true)) { Notice=TEXT("Could not create the save directory."); return; }
    if(!Sim.Save(FPaths::Combine(Directory,TEXT("center.json")),Error)) { Notice=Error; return; }
    auto Metadata=MakeShared<FJsonObject>(); Metadata->SetNumberField(TEXT("format"),5); Metadata->SetStringField(TEXT("generation"),Generation);
    const FVector SavedFocus=CompanionView?SavedColonyCamera:CameraCenter;
    Metadata->SetNumberField(TEXT("camera_x"),SavedFocus.X); Metadata->SetNumberField(TEXT("camera_y"),SavedFocus.Y);
    Metadata->SetNumberField(TEXT("camera_yaw"),CompanionView?SavedColonyYaw:CameraYaw); Metadata->SetNumberField(TEXT("camera_pitch"),CompanionView?SavedColonyPitch:CameraPitch);
    Metadata->SetNumberField(TEXT("zoom"),CompanionView?SavedColonyZoom:Zoom); Metadata->SetBoolField(TEXT("paused"),MenuOpen?PauseBeforeMenu:Paused); Metadata->SetNumberField(TEXT("speed"),Speed);
    Metadata->SetBoolField(TEXT("objective_acknowledged"),WinAcknowledged);
    Metadata->SetBoolField(TEXT("background_bugs"),Sim.BackgroundBugsEnabled);
    Metadata->SetBoolField(TEXT("periodic_attacks"),Sim.PeriodicAttacksEnabled);
    Metadata->SetStringField(TEXT("center_ai"),CenterBrain?CenterBrain->GetConfigFingerprint():TEXT(""));
    TArray<TSharedPtr<FJsonValue>> Slots;
    for(const auto& Slot:ScenarioSlots) Slots.Add(MakeShared<FJsonValueString>(Slot));
    Metadata->SetArrayField(TEXT("slots"),Slots);
    TArray<TSharedPtr<FJsonValue>> Fingerprints;
    for(const auto& N:Neighbors)
    {
        if(!N.Sim.Save(FPaths::Combine(Directory,FString::Printf(TEXT("sector_%d.json"),N.Index)),Error)) { Notice=Error; return; }
        auto Entry=MakeShared<FJsonObject>(); Entry->SetNumberField(TEXT("index"),N.Index); Entry->SetStringField(TEXT("ai"),N.Brain->GetConfigFingerprint());
        Fingerprints.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Metadata->SetArrayField(TEXT("neighbor_ai"),Fingerprints);
    const FString Pending=FPaths::Combine(SaveRoot(),TEXT("Scenario.pending.json")), Final=FPaths::Combine(SaveRoot(),TEXT("Scenario.json"));
    if(!WriteObject(Metadata,Pending)||!IFileManager::Get().Move(*Final,*Pending,true,true,false,true)) { Notice=TEXT("Could not finalize the scenario save. The previous snapshot is retained."); return; }
    Notice=TEXT("Entire scenario saved: center, neighbors, AI and camera. F9 restores it.");
}
void ASeigeGameMode::LoadGame()
{
    if(!GraphicsSettingsValid){Notice=Error.IsEmpty()?TEXT("Correct Graphics/scene.json and restart the game."):Error;return;}
    FString Raw;
    TSharedPtr<FJsonObject> Metadata;
    const FString Filename=FPaths::Combine(SaveRoot(),TEXT("Scenario.json"));
    if(!FFileHelper::LoadFileToString(Raw,*Filename)) { Notice=TEXT("No saved scenario yet. Start Single player, then save from Menu [Esc / F10]."); return; }
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Metadata)||!Metadata.IsValid()) { Notice=TEXT("The scenario save is unreadable."); return; }
    double Format=0,CameraX=0,CameraY=0,StoredZoom=0,StoredSpeed=1;
    double StoredYaw=0,StoredPitch=0;
    FString Generation,CenterFingerprint;
    bool StoredPaused=false,StoredAcknowledged=false,StoredBackgroundBugs=true,StoredPeriodicAttacks=true;
    const TArray<TSharedPtr<FJsonValue>>* Slots=nullptr; const TArray<TSharedPtr<FJsonValue>>* Fingerprints=nullptr;
    FGuid GenerationId;
    if(!Metadata->HasTypedField<EJson::Number>(TEXT("format"))||!Metadata->TryGetNumberField(TEXT("format"),Format)||Format!=5)
    {Notice=TEXT("This scenario save is incompatible with version 0.8. Start a new scenario.");return;}
    for(const TCHAR* Field:{TEXT("camera_x"),TEXT("camera_y"),TEXT("zoom"),TEXT("speed")})
        if(!Metadata->HasTypedField<EJson::Number>(Field)){Notice=TEXT("Scenario save metadata is invalid.");return;}
    if(!Metadata->TryGetStringField(TEXT("generation"),Generation)||!FGuid::ParseExact(Generation,EGuidFormats::Digits,GenerationId)||
       !Metadata->TryGetArrayField(TEXT("slots"),Slots)||Slots->Num()!=9||!Metadata->TryGetArrayField(TEXT("neighbor_ai"),Fingerprints)||
       !Metadata->TryGetStringField(TEXT("center_ai"),CenterFingerprint)||!Metadata->TryGetNumberField(TEXT("camera_x"),CameraX)||!FMath::IsFinite(CameraX)||
       !Metadata->TryGetNumberField(TEXT("camera_y"),CameraY)||!FMath::IsFinite(CameraY)||!Metadata->TryGetNumberField(TEXT("zoom"),StoredZoom)||!FMath::IsFinite(StoredZoom)||
       StoredZoom<MinimumZoom||StoredZoom>MaximumZoom||
       !Metadata->TryGetNumberField(TEXT("speed"),StoredSpeed)||!IsSupportedGameSpeed(StoredSpeed)||!Metadata->HasTypedField<EJson::Boolean>(TEXT("paused"))||!Metadata->TryGetBoolField(TEXT("paused"),StoredPaused)||
       !Metadata->HasTypedField<EJson::Boolean>(TEXT("objective_acknowledged"))||!Metadata->TryGetBoolField(TEXT("objective_acknowledged"),StoredAcknowledged)) { Notice=TEXT("Scenario save metadata is invalid."); return; }
    if(!Metadata->HasTypedField<EJson::Boolean>(TEXT("background_bugs"))||!Metadata->HasTypedField<EJson::Boolean>(TEXT("periodic_attacks"))||
       !Metadata->TryGetBoolField(TEXT("background_bugs"),StoredBackgroundBugs)||!Metadata->TryGetBoolField(TEXT("periodic_attacks"),StoredPeriodicAttacks))
    {Notice=TEXT("Saved scenario threat settings are invalid.");return;}
    if(!Metadata->HasTypedField<EJson::Number>(TEXT("camera_yaw"))||!Metadata->TryGetNumberField(TEXT("camera_yaw"),StoredYaw)||!FMath::IsFinite(StoredYaw)||StoredYaw < -360||StoredYaw > 360||
       !Metadata->HasTypedField<EJson::Number>(TEXT("camera_pitch"))||!Metadata->TryGetNumberField(TEXT("camera_pitch"),StoredPitch)||!FMath::IsFinite(StoredPitch)||StoredPitch < MinimumCameraPitch||StoredPitch > MaximumCameraPitch)
    { Notice=TEXT("Saved camera orientation is invalid."); return; }
    TArray<FString> NewSlots;
    for(int32 I=0;I<9;I++)
    {
        FString Type;
        if(!(*Slots)[I]->TryGetString(Type)||(Type!=TEXT("starting")&&Type!=TEXT("developed")&&Type!=(I==4?TEXT("player"):TEXT("empty")))) { Notice=TEXT("Saved scenario has an invalid sector type."); return; }
        NewSlots.Add(Type);
    }
    TMap<int32,FString> SavedFingerprints;
    for(const auto& Value:*Fingerprints)
    {
        const auto Entry=Value->AsObject(); double Index=0; FString Fingerprint;
        if(!Entry||!Entry->TryGetNumberField(TEXT("index"),Index)||Index<0||Index>8||FMath::FloorToDouble(Index)!=Index||Index==4||
           SavedFingerprints.Contains(int32(Index))||!Entry->TryGetStringField(TEXT("ai"),Fingerprint)) { Notice=TEXT("Saved AI metadata is invalid."); return; }
        SavedFingerprints.Add(int32(Index),Fingerprint);
    }
    const FString Directory=FPaths::Combine(SaveRoot(),TEXT("Scenarios"),Generation);
    FSeigeSimulation NewCenter; TSharedPtr<FSeigeScenarioAI> NewBrain; TArray<FSeigeNeighbor> NewNeighbors;
    const bool NewObserver=NewSlots[4]!=TEXT("player");
    if(NewObserver)
    {
        NewBrain=MakeShared<FSeigeScenarioAI>();
        if(!NewBrain->Initialize(NewCenter,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),false,Error,StoredBackgroundBugs,StoredPeriodicAttacks)) { Notice=Error; return; }
        if(NewBrain->GetConfigFingerprint()!=CenterFingerprint) { Notice=TEXT("AI files have changed since this save. Restore those files or start a new scenario."); return; }
    }
    else if(!NewCenter.Initialize(DataDirectory(TEXT("Rules")),Error,StoredBackgroundBugs,StoredPeriodicAttacks)) { Notice=Error; return; }
    if(!NewCenter.Load(FPaths::Combine(Directory,TEXT("center.json")),Error)) { Notice=Error; return; }
    if(FMath::Abs(CameraX)>NewCenter.WorldHalfSize*2.8||FMath::Abs(CameraY)>NewCenter.WorldHalfSize*2.8)
    {Notice=TEXT("Saved camera position is outside the scenario.");return;}
    if(NewCenter.BackgroundBugsEnabled!=StoredBackgroundBugs||NewCenter.PeriodicAttacksEnabled!=StoredPeriodicAttacks)
    {Notice=TEXT("Scenario threat settings disagree with the saved center.");return;}
    for(int32 Index=0;Index<9;Index++)
    {
        if(Index==4||NewSlots[Index]==TEXT("empty")) continue;
        FSeigeNeighbor N; N.Index=Index; N.Type=NewSlots[Index]; N.Offset=FVector2D(Index%3-1,Index/3-1)*NewCenter.WorldHalfSize*2;
        N.Brain=MakeShared<FSeigeScenarioAI>();
        if(!N.Brain->Initialize(N.Sim,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),false,Error,StoredBackgroundBugs,StoredPeriodicAttacks,SeigeSectorResourceSeed(NewCenter.GenerationSeed,Index))) { Notice=Error; return; }
        if(SavedFingerprints.FindRef(Index)!=N.Brain->GetConfigFingerprint()) { Notice=TEXT("AI files have changed since this save. Restore those files or start a new scenario."); return; }
        if(!N.Sim.Load(FPaths::Combine(Directory,FString::Printf(TEXT("sector_%d.json"),Index)),Error)) { Notice=Error; return; }
        if(N.Sim.BackgroundBugsEnabled!=StoredBackgroundBugs||N.Sim.PeriodicAttacksEnabled!=StoredPeriodicAttacks)
        {Notice=TEXT("Scenario threat settings disagree with a saved neighbor.");return;}
        NewNeighbors.Add(MoveTemp(N));
    }
    if(SavedFingerprints.Num()!=NewNeighbors.Num()) { Notice=TEXT("Scenario save has inconsistent neighbor records."); return; }
    TArray<FSeigeRegionResources> NewEmptyRegions;
    if(!GenerateEmptyRegionResources(NewCenter,NewSlots,NewEmptyRegions,Error)){Notice=Error;return;}
    for(auto& Pair:Visuals) if(Pair.Value) Pair.Value->Destroy();
    Visuals.Empty();
    for(auto It=Materials.CreateIterator();It;++It)if(It.Key().StartsWith(TEXT("construction_original_")))It.RemoveCurrent();
    ExitCompanionView();
    Sim=MoveTemp(NewCenter); CenterBrain=MoveTemp(NewBrain); Neighbors=MoveTemp(NewNeighbors); ScenarioSlots=MoveTemp(NewSlots);
    EmptyRegionResources=MoveTemp(NewEmptyRegions);ConfigureCombatTerrain();SelectedFleetId=0;FleetOrderActive=false;
    ScenarioBackgroundBugs=StoredBackgroundBugs;ScenarioPeriodicAttacks=StoredPeriodicAttacks;
    Observer=NewObserver; Ready=true; Accumulator=0; SelectedId=SelectedRoadId=0; CancelRoadTool();CancelWallTool(); SelectedBuild.Empty(); Paused=StoredPaused; Speed=StoredSpeed; WinAcknowledged=StoredAcknowledged;
    ResetSimulationPresentation();
    CameraCenter=FVector(CameraX,CameraY,0);
    Zoom=StoredZoom; CameraYaw=StoredYaw; CameraPitch=StoredPitch; Screen=TEXT("playing");
    MenuOpen=false;
    CreateLandscape(); SyncVisuals(); UpdateCamera(); Notice=TEXT("Scenario restored, including all AI neighbors.");
}
