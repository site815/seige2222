#include "SeigeAudioSubsystem.h"
#include "SeigeGameMode.h"
#include "Camera/CameraActor.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "CoreGlobals.h"

namespace SeigeAudio
{
void PlayUi(UWorld* World,FName Cue){if(World)if(auto* Feedback=World->GetSubsystem<USeigeAudioSubsystem>())Feedback->PlayUi(Cue);}
void PlayResult(UWorld* World,bool Success,FName SuccessCue){PlayUi(World,ResultCue(Success,SuccessCue));}
}
void USeigeAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FString Raw;TSharedPtr<FJsonObject> Doc;
    const FString ProjectGraphics=FPaths::Combine(FPaths::ProjectDir(),TEXT("Graphics"));
    const FString Directory=IFileManager::Get().DirectoryExists(*ProjectGraphics)?ProjectGraphics:FPaths::Combine(FPlatformProcess::BaseDir(),TEXT("Graphics"));
    const FString Path=FPaths::Combine(Directory,TEXT("audio.json"));
    if(FFileHelper::LoadFileToString(Raw,*Path)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Doc)&&Doc)
    {
        double Value=0;if(Doc->TryGetNumberField(TEXT("master_volume"),Value))MasterVolume=FMath::Clamp(float(Value),0.f,1.f);
        if(Doc->TryGetNumberField(TEXT("maximum_voices_per_frame"),Value))RateGate.MaximumPerFrame=FMath::Clamp(int32(Value),1,16);
        if(Doc->TryGetNumberField(TEXT("maximum_voices_per_second"),Value))RateGate.MaximumPerSecond=FMath::Clamp(int32(Value),1,32);
        if(Doc->TryGetNumberField(TEXT("interface_voices_per_frame"),Value))RateGate.InterfacePerFrame=FMath::Clamp(int32(Value),1,4);
        if(Doc->TryGetNumberField(TEXT("interface_voices_per_second"),Value))RateGate.InterfacePerSecond=FMath::Clamp(int32(Value),1,12);
        if(Doc->TryGetNumberField(TEXT("notification_voices_per_frame"),Value))RateGate.NotificationPerFrame=FMath::Clamp(int32(Value),1,4);
        if(Doc->TryGetNumberField(TEXT("notification_voices_per_second"),Value))RateGate.NotificationPerSecond=FMath::Clamp(int32(Value),1,8);
        if(Doc->TryGetNumberField(TEXT("damage_alert_quiet_seconds"),Value))DamageAlertQuietSeconds=FMath::Clamp(Value,1.,300.);
        const TSharedPtr<FJsonObject>* Groups=nullptr;
        if(Doc->TryGetObjectField(TEXT("categories"),Groups))for(const auto& Pair:(*Groups)->Values)
        {
            const TSharedPtr<FJsonObject>* Group=nullptr;if(!Pair.Value->TryGetObject(Group))continue;
            FAudioCategory Mix;
            if((*Group)->TryGetNumberField(TEXT("volume"),Value))Mix.Volume=FMath::Clamp(float(Value),0.f,1.f);
            if((*Group)->TryGetNumberField(TEXT("cooldown_seconds"),Value))Mix.Cooldown=FMath::Clamp(Value,0.,60.);
            FString Lane;if((*Group)->TryGetStringField(TEXT("lane"),Lane))Mix.Lane=Lane==TEXT("interface")?ESeigeAudioLane::Interface:Lane==TEXT("notification")?ESeigeAudioLane::Notification:ESeigeAudioLane::World;
            Categories.Add(FName(*Pair.Key),Mix);
        }
        const TSharedPtr<FJsonObject>* Mixes=nullptr;
        if(Doc->TryGetObjectField(TEXT("cue_mix"),Mixes))for(const auto& Pair:(*Mixes)->Values)
        {
            const TSharedPtr<FJsonObject>* Entry=nullptr;if(!Pair.Value->TryGetObject(Entry))continue;
            FCueMix Mix;FString Category;if((*Entry)->TryGetStringField(TEXT("category"),Category))Mix.Category=FName(*Category);
            if((*Entry)->TryGetNumberField(TEXT("volume"),Value))Mix.Volume=FMath::Clamp(float(Value),0.f,1.f);
            if((*Entry)->TryGetNumberField(TEXT("cooldown_seconds"),Value))Mix.Cooldown=FMath::Clamp(Value,0.,60.);
            CueMix.Add(FName(*Pair.Key),Mix);
        }
        const TSharedPtr<FJsonObject>* Cues=nullptr;
        if(Doc->TryGetObjectField(TEXT("cues"),Cues))for(const auto& Pair:(*Cues)->Values)
        {FString Asset;if(Pair.Value->TryGetString(Asset)&&Asset.StartsWith(TEXT("/Game/Art/Audio/")))if(auto* Sound=LoadObject<USoundBase>(nullptr,*Asset))Sounds.Add(FName(*Pair.Key),Sound);}
    }
    const FString Weather=FPaths::Combine(Directory,TEXT("weather.json"));
    if(FFileHelper::LoadFileToString(Raw,*Weather)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),Doc)&&Doc)
    {
        double V=0;if(Doc->TryGetNumberField(TEXT("maximum_local_lights"),V))MaximumLights=FMath::Clamp(int32(V),0,32);
        if(Doc->TryGetNumberField(TEXT("building_lamp_lumens"),V))BuildingLumens=FMath::Clamp(float(V),0.f,10000.f);
        if(Doc->TryGetNumberField(TEXT("worker_lamp_lumens"),V))WorkerLumens=FMath::Clamp(float(V),0.f,1000.f);
        if(Doc->TryGetNumberField(TEXT("vehicle_lamp_lumens"),V))VehicleLumens=FMath::Clamp(float(V),0.f,5000.f);
    }
    Attenuation=NewObject<USoundAttenuation>(this);
    Attenuation->Attenuation.bAttenuate=true;Attenuation->Attenuation.bSpatialize=true;
    Attenuation->Attenuation.AttenuationShapeExtents=FVector(12000,0,0);Attenuation->Attenuation.FalloffDistance=80000;
}
void USeigeAudioSubsystem::Deinitialize()
{
    for(auto& L:Lights)if(L)L->Destroy();Lights.Empty();LightOwners.Empty();Sounds.Empty();Super::Deinitialize();
}
void USeigeAudioSubsystem::Play(FName Cue,FVector Position,bool Spatial)
{
    const auto* Mix=CueMix.Find(Cue);if(!Mix)return;
    const auto* Category=Categories.Find(Mix->Category);if(!Category)return;
    const float Volume=MasterVolume*Category->Volume*Mix->Volume;if(Volume<=KINDA_SMALL_NUMBER)return;
    const auto* Found=Sounds.Find(Cue);if(!Found||!Found->Get())return;
    if(Spatial)if(const auto* Game=Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()))if(Game->Camera&&Attenuation)
    {
        const double AudibleRadius=Attenuation->Attenuation.AttenuationShapeExtents.X+Attenuation->Attenuation.FalloffDistance;
        if(FVector::DistSquared(Position,Game->Camera->GetActorLocation())>FMath::Square(AudibleRadius))return;
    }
    // Wall-clock limits remain the same at pause, 1x, 5x, 10x and 20x. Dropped sounds are
    // never queued up to burst later, and muted clicks consume no audible budget.
    const double Now=FPlatformTime::Seconds();
    if(!RateGate.TryPlay(Cue,Mix->Category,Volume,Now,GFrameCounter,Mix->Cooldown,Category->Cooldown,Category->Lane))return;
    if(Spatial)UGameplayStatics::PlaySoundAtLocation(GetWorld(),Found->Get(),Position,Volume,1,0,Attenuation);
    else UGameplayStatics::PlaySound2D(GetWorld(),Found->Get(),Volume);
}
void USeigeAudioSubsystem::PlayUi(FName Cue)
{
    Play(Cue,FVector::ZeroVector,false);
}
void USeigeAudioSubsystem::Tick(float DeltaSeconds)
{
    Clock=FPlatformTime::Seconds();
    auto* Game=Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode());if(!Game||!Game->Ready)return;
    UpdateLights(*Game,DeltaSeconds);
    auto& Sim=Game->Sim;
    if(Game->Screen!=TEXT("playing")){LastTime=-1;return;}
    const bool Reset=LastTime<0||Sim.Time<LastTime;
    if(Reset){RecentShots.Empty();KnownProjectiles.Empty();BuildingConstruction.Empty();BuildingDamage.Empty();PreviousRetreat=false;DamageIncident.Reset();}
    TSet<FString> Current;
    for(const auto& Shot:Sim.Combat.ShotEvents)
    {
        const FString Key=FString::Printf(TEXT("%.6f:%d:%d:%d:%s"),Shot.Time,Shot.OwnerSector,Shot.OwnerId,Shot.WeaponSlot,*Shot.OwnerKind);Current.Add(Key);
        if(!Reset&&!Game->Paused&&!RecentShots.Contains(Key))Play(FName(*Shot.Family),Game->RenderPosition(Shot.Start),true);
    }
    RecentShots=MoveTemp(Current);TSet<int32> Projectiles;
    for(const auto& P:Sim.Combat.Projectiles)
    {
        Projectiles.Add(P.Id);if(!Reset&&!Game->Paused&&!KnownProjectiles.Contains(P.Id))
        if(const auto* Weapon=Sim.Combat.Weapons.Find(P.WeaponId))Play(FName(*Weapon->Family),Game->RenderPosition(SeigeAudio::ProjectileWorldPosition(P.Position,P.SectorIndex,Sim.WorldHalfSize)),true);
    }
    KnownProjectiles=MoveTemp(Projectiles);
    bool DamageSeen=false,ConstructionCompleted=false;
    for(const auto& B:Sim.Buildings)
    {
        if(!Reset)
        {
            if(const bool* Was=BuildingConstruction.Find(B.Id))
            {if(*Was&&!B.IsConstructing&&B.Health>0)ConstructionCompleted=true;}
            // Placement feedback is emitted from the actual successful player
            // action, not from polling (which also sees autonomous AI builds).
        }
        BuildingConstruction.Add(B.Id,B.IsConstructing);
        if(const auto* Defense=Sim.Combat.BuildingState.Find(B.Id))
        {
            const double* Previous=BuildingDamage.Find(B.Id);
            if(!Reset&&Previous&&Defense->LastDamageTime>*Previous)
            DamageSeen=true;
            BuildingDamage.Add(B.Id,Defense->LastDamageTime);
        }
    }
    // One alert at the start of an incident, rearmed only after a real quiet
    // interval. A long battle no longer emits a fresh warning tone every cooldown.
    if(DamageIncident.Observe(DamageSeen,Clock,DamageAlertQuietSeconds))Play(TEXT("alert"),FVector::ZeroVector,false);
    if(ConstructionCompleted)Play(TEXT("complete"),FVector::ZeroVector,false);
    const bool Retreat=Sim.Companions.Dogs.Num()>0&&Sim.Companions.Dogs[0].Retreating;
    if(Retreat&&!PreviousRetreat&&!Reset)Play(TEXT("rex"),FVector::ZeroVector,false);
    PreviousRetreat=Retreat;LastTime=Sim.Time;
}
void USeigeAudioSubsystem::UpdateLights(ASeigeGameMode& Game,float DeltaSeconds)
{
    const auto Date=Game.ScenarioCalendar.Sample();
    const float Target=1-float(FMath::SmoothStep(0.,double(Game.SunriseSoftness),Date.SolarFactor));
    NightFactor=FMath::FInterpTo(NightFactor,Target,DeltaSeconds,3.f);
    if(Clock-LightRefresh>.25)
    {
        LightRefresh=Clock;
        struct FCandidate{AActor* Actor;bool Building;double Distance;};TArray<FCandidate> Candidates;
        const FVector Focus=Game.RenderPosition(FVector2D(Game.CameraCenter));
        if(NightFactor>.01&&Game.RegionMapAlpha()<.8&&(Game.Screen==TEXT("playing")||Game.MenuOpen))
        for(const auto& Pair:Game.Visuals)if(auto* Actor=Pair.Value.Get())if(!Actor->IsHidden())
        {
            const bool Building=Pair.Key.Contains(TEXT("building_"));
            const bool Mobile=(Pair.Key.Contains(TEXT("worker_"))&&!Pair.Key.Contains(TEXT("_tool"))&&!Pair.Key.Contains(TEXT("_cargo"))&&!Pair.Key.Contains(TEXT("_carrier")))||Pair.Key.Contains(TEXT("vehicle_"));
            if(!Building&&!Mobile)continue;
            if(Building)
            {
                // Unpowered civilian buildings do not acquire free electric lighting.
                bool Powered=false;
                auto Match=[&](const FSeigeSimulation& S,const FString& Prefix){for(const auto& B:S.Buildings)if(Pair.Key==Prefix+FString::Printf(TEXT("building_%d"),B.Id))Powered=B.Health>0&&!B.IsConstructing&&B.Enabled&&S.Energy.Fraction(B.Id)>0;};
                Match(Game.Sim,TEXT("home_"));for(const auto& N:Game.Neighbors)Match(N.Sim,FString::Printf(TEXT("zone_%d_"),N.Index));
                if(!Powered)continue;
            }
            const double Distance=FVector::DistSquared2D(Actor->GetActorLocation(),Focus);if(Distance<1600000000.)Candidates.Add({Actor,Building,Distance});
        }
        Candidates.Sort([](const auto& A,const auto& B){return A.Distance<B.Distance;});
        const int32 Count=FMath::Min(MaximumLights,Candidates.Num());
        while(Lights.Num()<Count)
        {
            auto* Light=GetWorld()->SpawnActor<APointLight>();auto* C=Light->PointLightComponent.Get();
            C->SetMobility(EComponentMobility::Movable);C->SetCastShadows(false);C->SetUseInverseSquaredFalloff(true);C->SetIntensityUnits(ELightUnits::Lumens);
            Lights.Add(Light);LightOwners.Add(nullptr);BuildingLights.Add(false);
        }
        for(int32 I=0;I<Lights.Num();++I)
        {Lights[I]->SetActorHiddenInGame(I>=Count);LightOwners[I]=I<Count?Candidates[I].Actor:nullptr;BuildingLights[I]=I<Count&&Candidates[I].Building;}
    }
    for(int32 I=0;I<Lights.Num();++I)if(auto* Actor=LightOwners[I].Get())
    {
        auto* C=Lights[I]->PointLightComponent.Get();const bool Building=BuildingLights[I];
        const FBox Bounds=Actor->GetComponentsBoundingBox(true);const bool Vehicle=!Building&&Bounds.GetSize().GetMax()>250;
        FVector Position=Actor->GetActorLocation();
        if(Building)Position=FVector(Bounds.Max.X+100,Bounds.GetCenter().Y,Bounds.Min.Z+340);
        else Position+=Actor->GetActorForwardVector()*(Vehicle?Bounds.GetSize().X*.4:55)+FVector(0,0,Vehicle?150:85);
        Lights[I]->SetActorLocation(Position);C->SetIntensity(NightFactor*(Building?BuildingLumens:Vehicle?VehicleLumens:WorkerLumens));
        C->SetAttenuationRadius(Building?2400:Vehicle?1400:500);C->SetLightColor(Building?FLinearColor(1,.76,.47):FLinearColor(.68,.87,1));
    }
}
