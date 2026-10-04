#include "SeigeGameMode.h"
#include "AI/SeigeScenarioAI.h"
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
    if(!Ready||Screen!=TEXT("playing")) { Notice=TEXT("Begin a scenario before saving."); return; }
    const FString Generation=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Directory=FPaths::Combine(SaveRoot(),TEXT("Scenarios"),Generation);
    if(!IFileManager::Get().MakeDirectory(*Directory,true)) { Notice=TEXT("Could not create the save directory."); return; }
    if(!Sim.Save(FPaths::Combine(Directory,TEXT("center.json")),Error)) { Notice=Error; return; }
    auto Metadata=MakeShared<FJsonObject>(); Metadata->SetNumberField(TEXT("format"),2); Metadata->SetStringField(TEXT("generation"),Generation);
    Metadata->SetNumberField(TEXT("camera_x"),CameraCenter.X); Metadata->SetNumberField(TEXT("camera_y"),CameraCenter.Y);
    Metadata->SetNumberField(TEXT("zoom"),Zoom); Metadata->SetBoolField(TEXT("paused"),Paused); Metadata->SetNumberField(TEXT("speed"),Speed);
    Metadata->SetBoolField(TEXT("objective_acknowledged"),WinAcknowledged);
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
    FString Raw;
    TSharedPtr<FJsonObject> Metadata;
    const FString Filename=FPaths::Combine(SaveRoot(),TEXT("Scenario.json"));
    if(!FFileHelper::LoadFileToString(Raw,*Filename)) { Notice=TEXT("No saved scenario yet. Start Single player, then save from the Colony menu."); return; }
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Metadata)||!Metadata.IsValid()) { Notice=TEXT("The scenario save is unreadable."); return; }
    double Format=0,CameraX=0,CameraY=0,StoredZoom=0,StoredSpeed=1;
    FString Generation,CenterFingerprint;
    bool StoredPaused=false,StoredAcknowledged=false;
    const TArray<TSharedPtr<FJsonValue>>* Slots=nullptr; const TArray<TSharedPtr<FJsonValue>>* Fingerprints=nullptr;
    FGuid GenerationId;
    if(!Metadata->TryGetNumberField(TEXT("format"),Format)||Format!=2||!Metadata->TryGetStringField(TEXT("generation"),Generation)||!FGuid::ParseExact(Generation,EGuidFormats::Digits,GenerationId)||
       !Metadata->TryGetArrayField(TEXT("slots"),Slots)||Slots->Num()!=9||!Metadata->TryGetArrayField(TEXT("neighbor_ai"),Fingerprints)||
       !Metadata->TryGetStringField(TEXT("center_ai"),CenterFingerprint)||!Metadata->TryGetNumberField(TEXT("camera_x"),CameraX)||!FMath::IsFinite(CameraX)||
       !Metadata->TryGetNumberField(TEXT("camera_y"),CameraY)||!FMath::IsFinite(CameraY)||!Metadata->TryGetNumberField(TEXT("zoom"),StoredZoom)||!FMath::IsFinite(StoredZoom)||
       !Metadata->TryGetNumberField(TEXT("speed"),StoredSpeed)||(StoredSpeed!=1&&StoredSpeed!=3)||!Metadata->TryGetBoolField(TEXT("paused"),StoredPaused)||
       !Metadata->TryGetBoolField(TEXT("objective_acknowledged"),StoredAcknowledged)) { Notice=TEXT("Scenario save metadata is invalid."); return; }
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
        if(!NewBrain->Initialize(NewCenter,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),NewSlots[4]==TEXT("developed"),Error)) { Notice=Error; return; }
        if(NewBrain->GetConfigFingerprint()!=CenterFingerprint) { Notice=TEXT("AI files have changed since this save. Restore those files or start a new scenario."); return; }
    }
    else if(!NewCenter.Initialize(DataDirectory(TEXT("Rules")),Error)) { Notice=Error; return; }
    if(!NewCenter.Load(FPaths::Combine(Directory,TEXT("center.json")),Error)) { Notice=Error; return; }
    for(int32 Index=0;Index<9;Index++)
    {
        if(Index==4||NewSlots[Index]==TEXT("empty")) continue;
        FSeigeNeighbor N; N.Index=Index; N.Type=NewSlots[Index]; N.Offset=FVector2D(Index%3-1,Index/3-1)*NewCenter.WorldHalfSize*2;
        N.Brain=MakeShared<FSeigeScenarioAI>();
        if(!N.Brain->Initialize(N.Sim,DataDirectory(TEXT("Rules")),DataDirectory(TEXT("AIFILES")),N.Type==TEXT("developed"),Error)) { Notice=Error; return; }
        if(SavedFingerprints.FindRef(Index)!=N.Brain->GetConfigFingerprint()) { Notice=TEXT("AI files have changed since this save. Restore those files or start a new scenario."); return; }
        if(!N.Sim.Load(FPaths::Combine(Directory,FString::Printf(TEXT("sector_%d.json"),Index)),Error)) { Notice=Error; return; }
        NewNeighbors.Add(MoveTemp(N));
    }
    if(SavedFingerprints.Num()!=NewNeighbors.Num()) { Notice=TEXT("Scenario save has inconsistent neighbor records."); return; }
    for(auto& Pair:Visuals) if(Pair.Value) Pair.Value->Destroy();
    Visuals.Empty(); Sim=MoveTemp(NewCenter); CenterBrain=MoveTemp(NewBrain); Neighbors=MoveTemp(NewNeighbors); ScenarioSlots=MoveTemp(NewSlots);
    Observer=NewObserver; Ready=true; Accumulator=0; SelectedId=0; SelectedBuild.Empty(); Paused=StoredPaused; Speed=StoredSpeed; WinAcknowledged=StoredAcknowledged;
    CameraCenter=FVector(FMath::Clamp(CameraX,-Sim.WorldHalfSize*2.8,Sim.WorldHalfSize*2.8),FMath::Clamp(CameraY,-Sim.WorldHalfSize*2.8,Sim.WorldHalfSize*2.8),0);
    Zoom=FMath::Clamp(StoredZoom,2500.,Sim.WorldHalfSize*12); Screen=TEXT("playing");
    CreateLandscape(); SyncVisuals(); UpdateCamera(); Notice=TEXT("Scenario restored, including all AI neighbors.");
}
