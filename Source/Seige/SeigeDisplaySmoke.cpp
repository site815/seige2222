#include "SeigeGameMode.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "HighResScreenshot.h"

// Opt-in actual viewport test. Launch with -DisplaySmoke -NoSaveDisplay, without
// ForceRes/UiSmoke: exercise engine mode changes, rather than capture overrides.
void ASeigeGameMode::RunDisplaySmoke()
{
    static TWeakObjectPtr<ASeigeGameMode> TestOwner;
    static int32 Stage=0,Failures=0;
    static TArray<TSharedPtr<FJsonValue>> Results;
    if(TestOwner.Get()!=this)
    {
        TestOwner=this;Stage=0;Failures=0;Results.Reset();
        IFileManager::Get().MakeDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Review-v08")),true);
    }
    // A requested screenshot is serviced after Tick. Never resize its source
    // backbuffer in the same frame (D3D12 readback would use the old extent).
    if(Stage>8||RenderClock<2+Stage*2||FScreenshotRequest::IsScreenshotRequested())return;
    auto* Settings=UGameUserSettings::GetGameUserSettings();
    if(!Settings){FPlatformMisc::RequestExitWithStatus(false,1);return;}
    auto Record=[&](const TCHAR* Name,EWindowMode::Type Mode,FIntPoint Expected,float Resolution,int32 ExpectedAntialiasing)
    {
        const FIntPoint Actual=DisplayResolution();
        const auto* ScreenPercentage=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
        const auto* ShadowQuality=IConsoleManager::Get().FindConsoleVariable(TEXT("sg.ShadowQuality"));
        const auto* Antialiasing=IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod"));
        const bool Valid=Settings->GetFullscreenMode()==Mode&&Actual==Expected&&ScreenPercentage&&FMath::IsNearlyEqual(ScreenPercentage->GetFloat(),Resolution,.01f)&&ShadowQuality&&ShadowQuality->GetInt()==MediumQualityGroups.FindRef(TEXT("Shadow"))&&Antialiasing&&Antialiasing->GetInt()==ExpectedAntialiasing;
        if(!Valid)++Failures;
        auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("state"),Name);Row->SetBoolField(TEXT("valid"),Valid);
        Row->SetNumberField(TEXT("mode"),static_cast<int32>(Settings->GetFullscreenMode()));
        Row->SetNumberField(TEXT("width"),Actual.X);Row->SetNumberField(TEXT("height"),Actual.Y);
        Row->SetNumberField(TEXT("expected_width"),Expected.X);Row->SetNumberField(TEXT("expected_height"),Expected.Y);
        Row->SetNumberField(TEXT("render_percent"),ScreenPercentage?ScreenPercentage->GetFloat():-1);
        Row->SetNumberField(TEXT("shadow_quality"),ShadowQuality?ShadowQuality->GetInt():-1);
        Row->SetNumberField(TEXT("antialiasing_method"),Antialiasing?Antialiasing->GetInt():-1);
        Row->SetNumberField(TEXT("expected_antialiasing_method"),ExpectedAntialiasing);
        Results.Add(MakeShared<FJsonValueObject>(Row));
        FScreenshotRequest::RequestScreenshot(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Screenshots/Review-v08/display_")+FString(Name)+TEXT(".png")),true,false);
        UE_LOG(LogTemp,Display,TEXT("DISPLAY_SMOKE %s %dx%d mode=%d antialiasing=%d expected=%d valid=%d"),Name,Actual.X,Actual.Y,static_cast<int32>(Settings->GetFullscreenMode()),Antialiasing?Antialiasing->GetInt():-1,ExpectedAntialiasing,Valid?1:0);
    };
    switch(Stage++)
    {
    case 0: SetRenderResolutionPercent(100);SetFullscreen(true);break;
    case 1: Record(TEXT("borderless"),EWindowMode::WindowedFullscreen,Settings->GetDesktopResolution(),100,2);break;
    case 2: WindowResolution=FIntPoint(1280,720);SetFullscreen(false);break;
    case 3: Record(TEXT("windowed"),EWindowMode::Windowed,WindowResolution,100,2);break;
    case 4: SetRenderResolutionPercent(75);break;
    case 5: Record(TEXT("render75"),EWindowMode::Windowed,WindowResolution,75,4);break;
    case 6: SetFullscreen(true);SetRenderResolutionPercent(100);break;
    case 7: Record(TEXT("restored"),EWindowMode::WindowedFullscreen,Settings->GetDesktopResolution(),100,2);break;
    case 8:
    {
        auto Report=MakeShared<FJsonObject>();Report->SetNumberField(TEXT("failures"),Failures);Report->SetArrayField(TEXT("states"),Results);
        FString Json;FJsonSerializer::Serialize(Report,TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json));
        if(!FFileHelper::SaveStringToFile(Json,*FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("DisplaySmoke.json"))))++Failures;
        FPlatformMisc::RequestExitWithStatus(false,Failures?1:0);break;
    }
    }
}
