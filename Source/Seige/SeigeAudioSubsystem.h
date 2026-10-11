#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SeigeAudioSubsystem.generated.h"

class USoundBase;
class USoundAttenuation;
class APointLight;
class ASeigeGameMode;
namespace SeigeAudio { SEIGE_API void PlayUi(UWorld* World,FName Cue); }

// Cosmetic feedback only. No audio or light component can change simulation state.
UCLASS()
class SEIGE_API USeigeAudioSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
    {return WorldType==EWorldType::Game||WorldType==EWorldType::PIE;}
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual TStatId GetStatId() const override {RETURN_QUICK_DECLARE_CYCLE_STAT(SeigePresentationFeedback,STATGROUP_Tickables);}
    void PlayUi(FName Cue);
private:
    void Play(FName Cue,FVector Position,bool Spatial);
    void UpdateLights(ASeigeGameMode& Game,float DeltaSeconds);
    UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> Sounds;
    UPROPERTY() TObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY() TArray<TObjectPtr<APointLight>> Lights;
    TArray<TWeakObjectPtr<AActor>> LightOwners;
    TArray<bool> BuildingLights;
    TSet<FString> RecentShots;
    TSet<int32> KnownProjectiles;
    TMap<int32,bool> BuildingConstruction;
    TMap<int32,double> BuildingDamage;
    double LastTime=-1,Clock=0,LastUi=-1,LastAlarm=-10,LightRefresh=-1;
    bool PreviousRetreat=false;
    int32 VoicesThisFrame=0,MaximumVoices=8,MaximumLights=20;
    float MasterVolume=.65f,NightFactor=0,BuildingLumens=1800,WorkerLumens=85,VehicleLumens=650;
};
