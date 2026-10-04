#include "SeigeGameMode.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProcess.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"

namespace {
const FLinearColor Ink(.029f,.035f,.031f,.98f),Panel(.055f,.064f,.054f,.98f),Raised(.087f,.101f,.081f,1);
const FLinearColor Gold(.90f,.76f,.44f),Green(.49f,.80f,.54f),Text(.94f,.92f,.83f),Muted(.64f,.69f,.61f),Red(.97f,.42f,.33f);
constexpr float TopHeight=78;
bool Contains(const FSeigeButton& R,float X,float Y) {return X>=R.Position.X&&X<=R.Position.X+R.Size.X&&Y>=R.Position.Y&&Y<=R.Position.Y+R.Size.Y;}
bool OutcomeModal(const ASeigeGameMode& G) {return !G.Ready||(!G.Observer&&(G.Sim.Escaped||G.Sim.Failed||(G.Sim.Won&&!G.WinAcknowledged)));}
FString ResourceName(const FSeigeSimulation& S,const FString& Id) {const auto* R=S.Resources.Find(Id);return R?R->Name:Id;}
}
FString FSeigeUiState::HitTest(float SX,float SY) const
{
    if(!FMath::IsFinite(SX)||!FMath::IsFinite(SY)||Scale<=0)return TEXT("");
    for(int32 I=HitRegions.Num()-1;I>=0;--I)if(Contains(HitRegions[I],SX/Scale,SY/Scale))return HitRegions[I].Action;
    return TEXT("");
}
void FSeigeUiState::CloseMenus(){BuildOpen=false;ColonyOpen=false;GroupFocused=false;Category.Empty();HoverPanel.Empty();}
ASeigeController::ASeigeController(){bShowMouseCursor=true;PrimaryActorTick.bCanEverTick=true;}
void ASeigeController::BeginPlay(){Super::BeginPlay();FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);SetInputMode(Mode);}
void ASeigeController::HandlePrimaryClick(float X,float Y)
{
    int32 Width=0,Height=0;GetViewportSize(Width,Height);
    if(!FMath::IsFinite(X)||!FMath::IsFinite(Y)||X<0||Y<0||(Width>0&&X>Width)||(Height>0&&Y>Height))return;
    auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;if(!G)return;
    if(GetLocalPlayer())UpdateCursorFromScreen(X,Y);
    auto* UI=Cast<ASeigeHUD>(GetHUD());if(UI&&UI->Click(X,Y))return;
    FVector Origin,Direction;
    if(GetLocalPlayer()&&ScreenRay(FVector2D(X,Y),Origin,Direction)&&G->SelectBuildingRay(Origin,Direction))return;
    G->ClickWorld();
}
bool ASeigeController::ScreenRay(const FVector2D& ScreenPosition,FVector& WorldOrigin,FVector& WorldDirection) const
{
    const ULocalPlayer* Local=GetLocalPlayer();
    if(ScreenPosition.ContainsNaN()||!Local||!Local->ViewportClient||!Local->ViewportClient->Viewport)return false;
    FSceneViewProjectionData Projection;
    if(!Local->GetProjectionData(Local->ViewportClient->Viewport,Projection))return false;
    const FIntRect Rect=Projection.GetConstrainedViewRect();
    if(Rect.Width()<=0||Rect.Height()<=0||ScreenPosition.X<Rect.Min.X||ScreenPosition.Y<Rect.Min.Y||ScreenPosition.X>Rect.Max.X||ScreenPosition.Y>Rect.Max.Y)return false;
    // The engine's screen deprojection snaps to whole pixels. Keep subpixel input
    // so a projected target remains the same terrain point even at region scale.
    const double X=2*(ScreenPosition.X-Rect.Min.X)/Rect.Width()-1;
    const double Y=1-2*(ScreenPosition.Y-Rect.Min.Y)/Rect.Height();
    const FMatrix Inverse=Projection.ComputeViewProjectionMatrix().InverseFast();
    const FVector4 Near=Inverse.TransformFVector4(FVector4(X,Y,1,1));
    const FVector4 Far=Inverse.TransformFVector4(FVector4(X,Y,.01,1));
    if(!FMath::IsFinite(Near.W)||!FMath::IsFinite(Far.W)||FMath::Abs(Near.W)<1.e-12||FMath::Abs(Far.W)<1.e-12)return false;
    const FVector Origin=FVector(Near.X,Near.Y,Near.Z)/Near.W;
    const FVector End=FVector(Far.X,Far.Y,Far.Z)/Far.W;
    if(Origin.ContainsNaN()||End.ContainsNaN())return false;
    const FVector Direction=(End-Origin).GetSafeNormal();
    if(Direction.IsNearlyZero()||Direction.ContainsNaN())return false;
    WorldOrigin=Origin;WorldDirection=Direction;return true;
}
bool ASeigeController::UpdateCursorFromScreen(float X,float Y)
{
    auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;
    if(!G)return false;
    FVector Origin,Direction,Hit;
    G->CursorOnWorld=ScreenRay(FVector2D(X,Y),Origin,Direction)&&G->TraceGroundRay(Origin,Direction,Hit);
    if(G->CursorOnWorld)G->CursorWorld=FVector2D(Hit)/G->RenderScale;
    return G->CursorOnWorld;
}
void ASeigeController::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt);auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;if(!G)return;
    auto* UI=Cast<ASeigeHUD>(GetHUD());
    bool Consumed=false;
    for(auto It=ConsumedKeysUntilRelease.CreateIterator();It;++It)if(!IsInputKeyDown(*It))It.RemoveCurrent();
    TArray<FKey> Keys={EKeys::Escape,EKeys::RightMouseButton,EKeys::SpaceBar,EKeys::F5,EKeys::F9};
    for(TCHAR Letter=TEXT('A');Letter<=TEXT('Z');++Letter)Keys.Add(FKey(FName(*FString::Chr(Letter))));
    for(const FKey& Key:Keys)if(WasInputKeyJustPressed(Key))
    {
        if(UI){const bool Handled=UI->HandleShortcut(Key);Consumed=Handled||Consumed;if(Handled)ConsumedKeysUntilRelease.Add(Key);}
        else if(Key==EKeys::Escape||Key==EKeys::RightMouseButton){G->SelectedBuild.Empty();G->SelectedId=0;Consumed=true;}
    }
    const bool CameraScreen=G->Screen==TEXT("playing")||G->Screen==TEXT("landing");
    const bool Blocked=!CameraScreen||Consumed||!ConsumedKeysUntilRelease.IsEmpty()||(UI&&UI->BlocksCameraKeys());
    if(!Blocked)
    {
        const float Step=G->Zoom*.65f*FMath::Min(Dt,.1f);
        const float Forward=(IsInputKeyDown(EKeys::W)||IsInputKeyDown(EKeys::Up)?1.f:0.f)-(IsInputKeyDown(EKeys::S)||IsInputKeyDown(EKeys::Down)?1.f:0.f);
        const float Right=(IsInputKeyDown(EKeys::D)||IsInputKeyDown(EKeys::Right)?1.f:0.f)-(IsInputKeyDown(EKeys::A)||IsInputKeyDown(EKeys::Left)?1.f:0.f);
        G->CameraCenter+=FVector(G->CameraPanDirection(Forward,Right),0)*Step;
        if(IsInputKeyDown(EKeys::Q))G->CameraYaw-=55*Dt;
        if(IsInputKeyDown(EKeys::E))G->CameraYaw+=55*Dt;
        if(IsInputKeyDown(EKeys::MiddleMouseButton)&&!(UI&&UI->IsPointerOverUI()))
        {float MX=0,MY=0;GetInputMouseDelta(MX,MY);G->CameraYaw+=MX*.25f;G->CameraPitch=FMath::Clamp(G->CameraPitch+MY*.2f,25.f,75.f);}
        G->CameraYaw=FRotator::ClampAxis(G->CameraYaw);
    }
    if(CameraScreen&&!(UI&&UI->IsPointerOverUI()))
    {
        if(WasInputKeyJustPressed(EKeys::MouseScrollUp))G->Zoom=FMath::Max(G->MinimumZoom,G->Zoom*.88f);
        if(WasInputKeyJustPressed(EKeys::MouseScrollDown))G->Zoom=FMath::Min(static_cast<float>(G->Sim.WorldHalfSize*12),G->Zoom*1.12f);
    }
    const double Limit=G->Sim.WorldHalfSize*2.8;
    G->CameraCenter.X=FMath::Clamp(G->CameraCenter.X,-Limit,Limit);G->CameraCenter.Y=FMath::Clamp(G->CameraCenter.Y,-Limit,Limit);
    if(!Blocked&&WasInputKeyJustPressed(EKeys::Home)){G->CameraCenter=FVector(G->HomePosition(),0);G->Zoom=G->DefaultZoom;G->CameraYaw=135;G->CameraPitch=52;}
    G->UpdateCamera();
    if(PlayerCameraManager)PlayerCameraManager->UpdateCamera(Dt);
    float X=0,Y=0;G->CursorOnWorld=false;
    if(GetMousePosition(X,Y)){UpdateCursorFromScreen(X,Y);if(WasInputKeyJustPressed(EKeys::LeftMouseButton))HandlePrimaryClick(X,Y);}
}
bool ASeigeHUD::LoadInterface(const FString& Directory,FString& Error)
{
    FString Json;if(!FFileHelper::LoadFileToString(Json,*FPaths::Combine(Directory,TEXT("ui.json")))){Error=TEXT("Cannot read Interface/ui.json");return false;}
    TSharedPtr<FJsonObject> Root;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Root)||!Root){Error=TEXT("Invalid Interface/ui.json");return false;}
    double V=0;const TArray<TSharedPtr<FJsonValue>>* Groups=nullptr;
    if(!Root->TryGetNumberField(TEXT("version"),V)||V!=1||!Root->TryGetArrayField(TEXT("build_groups"),Groups)||Groups->IsEmpty()){Error=TEXT("Unsupported interface definitions");return false;}
    TArray<FSeigeMenuGroup> Parsed;TSet<FString> GroupIds,GroupKeys,BuildingIds;
    for(const auto& Value:*Groups)
    {
        const auto O=Value->AsObject();FSeigeMenuGroup G;const TArray<TSharedPtr<FJsonValue>>* Entries=nullptr;
        if(!O||!O->TryGetStringField(TEXT("id"),G.Id)||!O->TryGetStringField(TEXT("name"),G.Name)||!O->TryGetStringField(TEXT("shortcut"),G.Shortcut)||!O->TryGetStringField(TEXT("description"),G.Description)||!O->TryGetArrayField(TEXT("entries"),Entries)){Error=TEXT("Incomplete build category");return false;}
        G.Shortcut=G.Shortcut.ToUpper();
        if(G.Id.IsEmpty()||GroupIds.Contains(G.Id)||GroupKeys.Contains(G.Shortcut)||G.Shortcut.Len()!=1||G.Shortcut==TEXT("B")){Error=TEXT("Duplicate or invalid category shortcut");return false;}
        GroupIds.Add(G.Id);GroupKeys.Add(G.Shortcut);TSet<FString> Keys;
        for(const auto& Item:*Entries)
        {
            const auto E=Item->AsObject();FSeigeMenuEntry Entry;
            if(!E||!E->TryGetStringField(TEXT("definition"),Entry.Definition)||!E->TryGetStringField(TEXT("shortcut"),Entry.Shortcut)){Error=TEXT("Incomplete interface entry");return false;}
            Entry.Shortcut=Entry.Shortcut.ToUpper();
            if(Entry.Definition.IsEmpty()||BuildingIds.Contains(Entry.Definition)||Keys.Contains(Entry.Shortcut)||Entry.Shortcut.Len()!=1||Entry.Shortcut==TEXT("B")){Error=TEXT("Duplicate or invalid building shortcut");return false;}
            BuildingIds.Add(Entry.Definition);Keys.Add(Entry.Shortcut);G.Entries.Add(Entry);
        }
        Parsed.Add(G);
    }
    TArray<FSeigeCredit> Credits;const TArray<TSharedPtr<FJsonValue>>* CreditRows=nullptr;
    if(!Root->TryGetArrayField(TEXT("credits"),CreditRows)){Error=TEXT("Interface credits are missing");return false;}
    for(const auto& Row:*CreditRows){const auto O=Row->AsObject();FSeigeCredit C;if(!O||!O->TryGetStringField(TEXT("heading"),C.Heading)||!O->TryGetStringField(TEXT("text"),C.Text)){Error=TEXT("Invalid credits entry");return false;}Credits.Add(C);}
    TArray<FSeigeSummaryResource> Summary;const TArray<TSharedPtr<FJsonValue>>* SummaryRows=nullptr;
    if(!Root->TryGetArrayField(TEXT("summary_resources"),SummaryRows)){Error=TEXT("Interface summary resources are missing");return false;}
    for(const auto& Row:*SummaryRows){const auto O=Row->AsObject();FSeigeSummaryResource R;if(!O||!O->TryGetStringField(TEXT("resource"),R.Resource)||!O->TryGetStringField(TEXT("label"),R.Label)){Error=TEXT("Invalid summary resource entry");return false;}Summary.Add(R);}
    Ui.Categories=MoveTemp(Parsed);Ui.Credits=MoveTemp(Credits);Ui.SummaryResources=MoveTemp(Summary);InterfaceLoaded=true;InterfaceAttempted=true;Error.Empty();return true;
}
bool ASeigeHUD::BlocksCameraKeys() const{return Ui.BuildOpen||Ui.ColonyOpen;}
bool ASeigeHUD::IsPointerOverUI() const{float X=0,Y=0;const auto* PC=GetOwningPlayerController();return PC&&PC->GetMousePosition(X,Y)&&!Ui.HitTest(X,Y).IsEmpty();}
bool ASeigeHUD::HandleShortcut(const FKey& K){auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;return G?ProcessShortcut(K,*G):false;}
bool ASeigeHUD::ProcessShortcut(const FKey& Key,ASeigeGameMode& G)
{
    if(G.Screen!=TEXT("playing"))
    {
        if(Key==EKeys::Escape){Ui.CloseMenus();if(G.Screen==TEXT("settings")||G.Screen==TEXT("credits"))G.ShowScreen(G.ReturnScreen);else if(G.Screen!=TEXT("main"))G.ReturnToMainMenu();return true;}
        if(G.Screen==TEXT("landing"))return false;
        if(G.Screen==TEXT("main")){if(Key==EKeys::S)G.ShowScreen(TEXT("scenario"));else if(Key==EKeys::L||Key==EKeys::F9)G.LoadGame();else if(Key==EKeys::C){G.ReturnScreen=TEXT("main");G.ShowScreen(TEXT("credits"));}}
        return true;
    }
    if(Key==EKeys::Escape||Key==EKeys::RightMouseButton)
    {
        if(Ui.BuildOpen&&Ui.GroupFocused){Ui.GroupFocused=false;return true;}
        if(Ui.BuildOpen||Ui.ColonyOpen||!Ui.HoverPanel.IsEmpty()){Ui.CloseMenus();return true;}
        G.SelectedBuild.Empty();G.SelectedId=0;return true;
    }
    if(OutcomeModal(G))return true;
    if(Key==EKeys::F5){LastNotice.Empty();G.SaveGame();return true;}if(Key==EKeys::F9){LastNotice.Empty();G.LoadGame();Ui.CloseMenus();return true;}if(Key==EKeys::SpaceBar){G.Paused=!G.Paused;return true;}
    if(Key==EKeys::B)
    {
        if(G.Observer)return true;
        const bool Open=!Ui.BuildOpen;Ui.CloseMenus();Ui.BuildOpen=Open;
        if(Open){G.SelectedBuild.Empty();G.SelectedId=0;if(Ui.Categories.Num())Ui.Category=Ui.Categories[0].Id;}
        return true;
    }
    if(Ui.BuildOpen)
    {
        const FString Pressed=Key.GetFName().ToString().ToUpper();
        if(!Ui.GroupFocused){for(const auto& Group:Ui.Categories)if(Group.Shortcut==Pressed){Ui.Category=Group.Id;Ui.GroupFocused=true;return true;}}
        else if(const auto* Group=Ui.Categories.FindByPredicate([this](const FSeigeMenuGroup& I){return I.Id==Ui.Category;}))
            for(const auto& Item:Group->Entries)if(Item.Shortcut==Pressed)return ExecuteAction(TEXT("build:")+Item.Definition,G);
        return true;
    }
    return Ui.ColonyOpen;
}
bool ASeigeHUD::Click(float X,float Y){auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;return G?ProcessClick(X,Y,*G):false;}
bool ASeigeHUD::ProcessClick(float X,float Y,ASeigeGameMode& G)
{
    // Canvas belongs to DrawHUD only; input uses the cached viewport hit regions.
    const FString Action=Ui.HitTest(X,Y);
    if(G.Screen!=TEXT("playing"))
    {
        if(!Action.IsEmpty())
        {
            if(Action.StartsWith(TEXT("screen:"))||Action.StartsWith(TEXT("slot:"))||Action.StartsWith(TEXT("quality:"))||Action==TEXT("start-scenario")||Action==TEXT("main-menu")||Action==TEXT("back-screen")||Action==TEXT("load")||Action==TEXT("fullscreen")||Action==TEXT("exit"))ExecuteAction(Action,G);
            return true;
        }
        return G.Screen!=TEXT("landing");
    }
    if(OutcomeModal(G))
    {
        // Outcome can change between rendering and this input event: discard stale world/menu actions.
        if(Action==TEXT("reset")||Action==TEXT("main-menu")||(Action==TEXT("continue")&&G.Sim.Won&&!G.Sim.Failed&&!G.Sim.Escaped))ExecuteAction(Action,G);
        return true;
    }
    if(!Action.IsEmpty())return ExecuteAction(Action,G);
    if(Ui.BuildOpen||Ui.ColonyOpen){Ui.CloseMenus();return true;}
    if(!Ui.HoverPanel.IsEmpty()){Ui.HoverPanel.Empty();return true;}
    return false;
}
bool ASeigeHUD::ExecuteAction(const FString& A,ASeigeGameMode& G)
{
    if(A.StartsWith(TEXT("screen:"))){G.ReturnScreen=G.Screen;Ui.CloseMenus();G.ShowScreen(A.RightChop(7));return true;}
    if(A.StartsWith(TEXT("slot:"))){G.CycleScenarioSlot(FCString::Atoi(*A.RightChop(5)));return true;}
    if(A.StartsWith(TEXT("quality:"))){G.SetGraphicsQuality(FCString::Atoi(*A.RightChop(8)));return true;}
    if(A==TEXT("fullscreen")){G.SetFullscreen(!G.Fullscreen);return true;}
    if(A==TEXT("start-scenario")){Ui.CloseMenus();G.StartScenario();return true;}
    if(A==TEXT("main-menu")){Ui.CloseMenus();G.ReturnToMainMenu();return true;}
    if(A==TEXT("back-screen")){Ui.CloseMenus();G.ShowScreen(G.ReturnScreen);return true;}
    if(A==TEXT("exit")){if(auto* PC=GetOwningPlayerController())PC->ConsoleCommand(TEXT("quit"));return true;}
    if(A==TEXT("build-menu"))return ProcessShortcut(EKeys::B,G);
    if(A==TEXT("colony-menu")){const bool Open=!Ui.ColonyOpen;Ui.CloseMenus();Ui.ColonyOpen=Open;return true;}
    if(A.StartsWith(TEXT("group:"))){Ui.Category=A.RightChop(6);Ui.GroupFocused=true;return true;}
    if(A.StartsWith(TEXT("summary:"))){Ui.HoverPanel=A.RightChop(8);return true;}
    if(A==TEXT("back")){Ui.GroupFocused=false;return true;}if(A==TEXT("close")){Ui.CloseMenus();return true;}
    if(A==TEXT("deselect")){G.SelectedId=0;G.SelectedBuild.Empty();return true;}
    if(A.StartsWith(TEXT("build:")))
    {
        if(G.Observer)return true;
        const FString Id=A.RightChop(6);
        if(G.Sim.BuildMenu.Contains(Id)){G.SelectedBuild=Id;G.SelectedId=0;Ui.CloseMenus();G.Notice=TEXT("Place ")+G.Sim.BuildingDefs[Id].Name+TEXT(". Right-click or Esc cancels.");}
        return true;
    }
    if(A==TEXT("pause"))G.Paused=!G.Paused;else if(A==TEXT("speed"))G.Speed=G.Speed==1?3:1;
    else if(A==TEXT("save")){LastNotice.Empty();G.SaveGame();Ui.CloseMenus();}else if(A==TEXT("load")){LastNotice.Empty();G.LoadGame();Ui.CloseMenus();}
    else if(A==TEXT("reset")){G.ResetColony();Ui.CloseMenus();}else if(A==TEXT("continue"))G.WinAcknowledged=true;
    else if(A==TEXT("toggle")&&!G.Observer)G.Sim.ToggleBuilding(G.SelectedId);
    else if(A==TEXT("escape")&&!G.Observer){G.Sim.LaunchShuttle();Ui.CloseMenus();G.Notice=TEXT("Shuttle launched with only cargo already aboard.");}
    return true;
}
void ASeigeHUD::Box(float X,float Y,float W,float H,FLinearColor C){if(Canvas)DrawRect(C,X*Scale,Y*Scale,W*Scale,H*Scale);}
void ASeigeHUD::Label(const FString& Value,float X,float Y,float Size,FLinearColor C)
{
    if(!Canvas||!GEngine)return;float TW=0,TH=0;Canvas->StrLen(GEngine->GetLargeFont(),TEXT("Ag"),TW,TH);
    DrawText(Value,C,X*Scale,Y*Scale,GEngine->GetLargeFont(),Size/FMath::Max(TH,1.f)*Scale,false);
}
void ASeigeHUD::Region(const FString& A,float X,float Y,float W,float H,const FString& Tip){Ui.HitRegions.Add({FVector2D(X,Y),FVector2D(W,H),A,Tip});}
void ASeigeHUD::Frame(float X,float Y,float W,float H){Box(X+4,Y+5,W,H,FLinearColor(0,0,0,.3f));Box(X,Y,W,H,Panel);Box(X,Y,W,2,Gold);Region(TEXT("panel"),X,Y,W,H);}
void ASeigeHUD::Button(const FString& Value,const FString& A,float X,float Y,float W,float H,bool Active,const FString& Tip)
{
    float MX=0,MY=0;auto* PC=GetOwningPlayerController();const bool Hover=PC&&PC->GetMousePosition(MX,MY)&&MX/Scale>=X&&MX/Scale<=X+W&&MY/Scale>=Y&&MY/Scale<=Y+H;
    Box(X,Y,W,H,Active||Hover?Raised:Panel);if(Active||Hover)Box(X,Y,2,H,Gold);
    Label(Value,X+12,Y+(H-16)/2,16,Active?Gold:Text);Region(A,X,Y,W,H,Tip);
}
TArray<FString> ASeigeHUD::WrapLines(const FString& Value,float Width,float Size) const
{
    TArray<FString> Lines;if(!Canvas||!GEngine)return Lines;TArray<FString> Words;Value.ParseIntoArray(Words,TEXT(" "),true);FString Line;
    float BW=0,BH=0;Canvas->StrLen(GEngine->GetLargeFont(),TEXT("Ag"),BW,BH);const float FS=Size/FMath::Max(BH,1.f);
    for(const FString& Word:Words){const FString Candidate=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;float TW=0,TH=0;Canvas->StrLen(GEngine->GetLargeFont(),Candidate,TW,TH);if(TW*FS>Width&&!Line.IsEmpty()){Lines.Add(Line);Line=Word;}else Line=Candidate;}
    if(!Line.IsEmpty())Lines.Add(Line);return Lines;
}
void ASeigeHUD::Wrapped(const FString& Value,float X,float& Y,float Width,float Size,FLinearColor C)
{
    for(const FString& Line:WrapLines(Value,Width,Size)){Label(Line,X,Y,Size,C);Y+=Size+7;}
}
float ASeigeHUD::DrawNotice(ASeigeGameMode& G,float W,float H)
{
    if(LastNotice.IsEmpty()||NoticeVisibleSeconds>=8)return 0;
    const float NW=FMath::Min(640.f,W-48),Y=TopHeight+14;
    auto Lines=WrapLines(LastNotice,NW-32,16);
    if(Lines.Num()>4){Lines.SetNum(4);Lines.Last()+=TEXT(" ...");}
    const float NH=26+Lines.Num()*23;if(Y+NH>H-24)return 0;
    const float Candidates[]={24.f,W-NW-24,(W-NW)*.5f};
    for(const float X:Candidates)
    {
        if(Ui.HitRegions.ContainsByPredicate([&](const FSeigeButton& R){return X<R.Position.X+R.Size.X&&X+NW>R.Position.X&&Y<R.Position.Y+R.Size.Y&&Y+NH>R.Position.Y;}))continue;
        Frame(X,Y,NW,NH);const FLinearColor C=!G.Error.IsEmpty()&&G.Notice==G.Error?Red:Gold;
        float TY=Y+13;for(const auto& Line:Lines){Label(Line,X+16,TY,16,C);TY+=23;}
        NoticeVisibleSeconds+=FMath::Max(0.f,GetWorld()->GetDeltaSeconds());return NH+14;
    }
    // Keep the message pending while construction or a hover card occupies the space.
    return 0;
}
void ASeigeHUD::Icon(const FString& Visual,float X,float Y,float S,FLinearColor C)
{
    Box(X,Y,S,S,FLinearColor(.11f,.13f,.10f));
    auto L=[&](float A,float B,float D,float E){DrawLine((X+A*S)*Scale,(Y+B*S)*Scale,(X+D*S)*Scale,(Y+E*S)*Scale,C,1.7f*Scale);};
    if(Visual==TEXT("sensor")){L(.5f,.8f,.5f,.3f);L(.25f,.8f,.75f,.8f);L(.25f,.3f,.5f,.15f);L(.5f,.15f,.75f,.3f);L(.15f,.18f,.5f,.03f);L(.5f,.03f,.85f,.18f);}
    else if(Visual==TEXT("turret")){L(.22f,.78f,.78f,.78f);L(.3f,.75f,.4f,.45f);L(.7f,.75f,.6f,.45f);L(.3f,.4f,.72f,.4f);L(.72f,.4f,.88f,.22f);L(.45f,.3f,.45f,.52f);}
    else if(Visual==TEXT("extractor")){L(.2f,.8f,.8f,.8f);L(.32f,.8f,.32f,.2f);L(.25f,.2f,.65f,.2f);L(.65f,.2f,.65f,.63f);L(.48f,.55f,.65f,.72f);L(.65f,.72f,.82f,.55f);}
    else if(Visual==TEXT("depot")){L(.2f,.38f,.5f,.2f);L(.5f,.2f,.8f,.38f);L(.2f,.38f,.5f,.55f);L(.8f,.38f,.5f,.55f);L(.2f,.38f,.2f,.7f);L(.8f,.38f,.8f,.7f);L(.2f,.7f,.5f,.88f);L(.8f,.7f,.5f,.88f);L(.5f,.55f,.5f,.88f);}
    else{L(.15f,.8f,.85f,.8f);L(.15f,.8f,.15f,.48f);L(.15f,.48f,.4f,.33f);L(.4f,.33f,.4f,.48f);L(.4f,.48f,.65f,.33f);L(.65f,.33f,.85f,.48f);L(.85f,.48f,.85f,.8f);L(.67f,.36f,.67f,.13f);L(.8f,.43f,.8f,.13f);}
}
void ASeigeHUD::Description(const FSeigeBuildingDef& D,ASeigeGameMode& G,float X,float Y,float W)
{
    Frame(X,Y,W,360);Icon(D.Visual,X+18,Y+20,40,D.Color);Label(D.Name,X+72,Y+25,20,Gold);float TY=Y+80;
    Wrapped(D.Description,X+18,TY,W-36,15,Text);TY+=10;Label(TEXT("CONSTRUCTION MATERIALS"),X+18,TY,12,Muted);TY+=24;
    TArray<FString> Keys;D.Cost.GetKeys(Keys);Keys.Sort();const auto* Core=G.Sim.Buildings.FindByPredicate([&](const FSeigeBuilding& B){return B.DefId==G.Sim.CoreDefinition;});
    for(const FString& Id:Keys){const double Held=Core?Core->Inventory.FindRef(Id):0;Label(FString::Printf(TEXT("%.0f  %s"),D.Cost[Id],*ResourceName(G.Sim,Id)),X+18,TY,15,Held>=D.Cost[Id]?Text:Red);TY+=24;}
    TY+=7;Label(FString::Printf(TEXT("%d jobs  /  Automatic staffing and repairs"),D.Jobs),X+18,TY,13,Muted);TY+=27;
    if(const auto* R=G.Sim.Recipes.Find(D.Recipe))
    {
        FString Recipe;TArray<FString> Inputs;R->Inputs.GetKeys(Inputs);Inputs.Sort();
        for(const FString& Id:Inputs){if(!Recipe.IsEmpty())Recipe+=TEXT(" + ");Recipe+=FString::Printf(TEXT("%.0f %s"),R->Inputs[Id],*ResourceName(G.Sim,Id));}
        Recipe+=FString::Printf(TEXT("  /  %.0fs per batch"),R->Seconds);Wrapped(Recipe,X+18,TY,W-36,13,Green);
    }
    else if(!D.ExtractResource.IsEmpty())Wrapped(FString::Printf(TEXT("%.1f %s per second when staffed"),D.ExtractRate,*ResourceName(G.Sim,D.ExtractResource)),X+18,TY,W-36,13,Green);
}
bool ASeigeHUD::DrawFrontend(ASeigeGameMode& G,float W,float H)
{
    if(G.Screen==TEXT("playing")||G.Screen==TEXT("landing"))return false;
    Ui.CloseMenus();Box(0,0,W,H,FLinearColor(.025f,.033f,.027f,.91f));Region(TEXT("frontend"),0,0,W,H);
    Label(TEXT("seige2222"),30,22,29,Gold);Label(FString::Printf(TEXT("v%s / %.0f FPS"),*Version,SmoothedFps),32,63,13,Muted);
    if(G.Screen==TEXT("main"))
    {
        const float X=W/2-230,Y=H/2-222;
        Label(TEXT("FIRST LANDING"),X,Y-60,34,Text);Label(TEXT("Build a colony. Sustain it. Defend it."),X,Y-12,16,Muted);
        Button(TEXT("Single player"),TEXT("screen:scenario"),X,Y+40,460,51);
        Button(TEXT("Load single-player colony"),TEXT("load"),X,Y+98,460,51);
        Button(TEXT("Multiplayer  /  Coming soon"),TEXT("unavailable"),X,Y+156,460,51);
        Button(TEXT("Credits"),TEXT("screen:credits"),X,Y+214,460,51);
        Button(TEXT("Settings"),TEXT("screen:settings"),X,Y+272,460,51);
        Button(TEXT("Exit"),TEXT("exit"),X,Y+330,460,51);
        float TY=Y+408;if(!G.Notice.IsEmpty())Wrapped(G.Notice,X,TY,460,14,Muted);
    }
    else if(G.Screen==TEXT("scenario"))
    {
        const auto NoticeLines=WrapLines(G.Notice,844,16);
        const float Extra=FMath::Min(NoticeLines.Num(),4)*23+(!NoticeLines.IsEmpty()?20:0);
        const float X=W/2-450,Y=H/2-(655+Extra)*.5f;
        Frame(X,Y,900,655+Extra);Label(TEXT("CHOOSE YOUR NEIGHBORHOOD"),X+28,Y+25,27,Gold);
        float TY=Y+72;Wrapped(TEXT("Click a sector to cycle its starting state. Choose Human in the center to command your colony, or an AI to observe the simulation."),X+28,TY,844,16,Text);
        for(int32 I=0;I<9;++I)
        {
            const float CX=X+28+(I%3)*282,CY=Y+154+(I/3)*119;
            const FString Type=G.ScenarioSlots.IsValidIndex(I)?G.ScenarioSlots[I]:(I==4?TEXT("player"):TEXT("empty"));
            const FString Name=Type==TEXT("player")?TEXT("Human player"):Type==TEXT("developed")?TEXT("Developed AI"):Type==TEXT("starting")?TEXT("Starting AI"):TEXT("Empty wilderness");
            Button(TEXT(""),TEXT("slot:")+FString::FromInt(I),CX,CY,270,106,I==4);
            Label(I==4?TEXT("CENTER / HOME SECTOR"):FString::Printf(TEXT("NEIGHBOR %d"),I<4?I+1:I),CX+16,CY+17,12,Muted);
            Label(Name,CX+16,CY+47,22,I==4?Gold:Type==TEXT("empty")?Muted:Green);
            Label(TEXT("Click to change"),CX+16,CY+82,11,Muted);
        }
        const bool Human=G.ScenarioSlots.IsValidIndex(4)&&G.ScenarioSlots[4]==TEXT("player");
        float NY=Y+523;for(int32 I=0;I<FMath::Min(NoticeLines.Num(),4);++I){Label(NoticeLines[I]+(I==3&&NoticeLines.Num()>4?TEXT(" ..."):TEXT("")),X+28,NY,16,!G.Error.IsEmpty()&&G.Notice==G.Error?Red:Gold);NY+=23;}
        Button(TEXT("Back"),TEXT("main-menu"),X+28,Y+560+Extra,190,46);
        Button(Human?TEXT("Choose landing site"):TEXT("Start observer scenario"),TEXT("start-scenario"),X+474,Y+560+Extra,398,46,true);
        Label(TEXT("Independent AI colonies share the same simulation rules."),X+28,Y+623+Extra,13,Muted);
    }
    else if(G.Screen==TEXT("settings"))
    {
        const float X=W/2-370,Y=H/2-200;Frame(X,Y,740,420);Label(TEXT("SETTINGS"),X+28,Y+26,28,Gold);
        Label(TEXT("Graphics quality"),X+28,Y+96,18,Text);
        const TArray<FString> Names={TEXT("Low"),TEXT("Medium"),TEXT("High"),TEXT("Ultra")};
        for(int32 I=0;I<4;++I)Button(Names[I],TEXT("quality:")+FString::FromInt(I),X+28+I*174,Y+135,164,46,G.GraphicsQuality==I);
        Label(TEXT("Display mode"),X+28,Y+225,18,Text);Button(G.Fullscreen?TEXT("Fullscreen"):TEXT("Windowed"),TEXT("fullscreen"),X+300,Y+213,410,46,G.Fullscreen);
        Button(TEXT("Back"),TEXT("back-screen"),X+28,Y+335,684,46);
    }
    else if(G.Screen==TEXT("credits"))
    {
        const float X=W/2-450,Y=H/2-340;Frame(X,Y,900,700);Label(TEXT("CREDITS"),X+28,Y+26,28,Gold);float TY=Y+90;
        if(Ui.Credits.IsEmpty())Wrapped(InterfaceError,X+28,TY,844,16,Red);
        for(const auto& Credit:Ui.Credits){Label(Credit.Heading,X+28,TY,18,Gold);TY+=31;Wrapped(Credit.Text,X+28,TY,844,15,Text);TY+=26;}
        Button(TEXT("Back"),TEXT("back-screen"),X+28,Y+628,844,45);
    }
    else {Label(TEXT("This screen is unavailable"),W/2-200,H/2,24,Gold);Button(TEXT("Main menu"),TEXT("main-menu"),W/2-200,H/2+60,400,46);}
    return true;
}
void ASeigeHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;auto* G=GetWorld()?Cast<ASeigeGameMode>(GetWorld()->GetAuthGameMode()):nullptr;if(!G)return;auto* PC=GetOwningPlayerController();
    Scale=FMath::Max(.1f,FMath::Min(Canvas->SizeX/1600.f,Canvas->SizeY/900.f));Ui.Scale=Scale;Ui.ViewportWidth=Canvas->SizeX;Ui.ViewportHeight=Canvas->SizeY;
    const float W=Ui.ViewportWidth/Scale,H=Ui.ViewportHeight/Scale;float MX=-1,MY=-1;if(PC)PC->GetMousePosition(MX,MY);
    const FString PreviousHover=Ui.HitTest(MX,MY);const bool WasOverUi=!PreviousHover.IsEmpty();Ui.HitRegions.Reset();
    if(!InterfaceAttempted)
    {
        InterfaceAttempted=true;FString Directory=FPaths::Combine(FPaths::ProjectDir(),TEXT("Interface"));
        if(!FPaths::FileExists(FPaths::Combine(Directory,TEXT("ui.json"))))Directory=FPaths::Combine(FPlatformProcess::BaseDir(),TEXT("Interface"));
        if(!LoadInterface(Directory,InterfaceError))G->Notice=InterfaceError;
        if(InterfaceLoaded)for(const auto& Group:Ui.Categories)for(const auto& Item:Group.Entries)if(!G->Sim.BuildMenu.Contains(Item.Definition)){InterfaceError=TEXT("Unavailable interface building: ")+Item.Definition;InterfaceLoaded=false;}
        if(InterfaceLoaded)for(const auto& Item:Ui.SummaryResources)if(!G->Sim.Resources.Contains(Item.Resource)){InterfaceError=TEXT("Unavailable summary resource: ")+Item.Resource;InterfaceLoaded=false;}
    }
    if(Version.IsEmpty()){GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),TEXT("ProjectVersion"),Version,GGameIni);if(Version.IsEmpty())Version=TEXT("unversioned");}
    const float Dt=GetWorld()->GetDeltaSeconds();if(Dt>0)SmoothedFps=SmoothedFps<=0?1.f/Dt:FMath::Lerp(SmoothedFps,1.f/Dt,.08f);
    if(LastNotice!=G->Notice){LastNotice=G->Notice;NoticeVisibleSeconds=0;}
    if(DrawFrontend(*G,W,H))return;
    auto WorldLine=[&](FVector A,FVector B,FLinearColor C,float Thickness){FVector2D P,Q;if(!PC||!PC->ProjectWorldLocationToScreen(A,P)||!PC->ProjectWorldLocationToScreen(B,Q))return;if(P.Y/Scale>TopHeight&&Q.Y/Scale>TopHeight)DrawLine(P.X,P.Y,Q.X,Q.Y,C,Thickness*Scale);};
    auto Ground=[&](FVector2D P,float Lift){return G->RenderPosition(P,Lift);};
    auto Circle=[&](FVector2D Center,double Radius,FLinearColor C){for(int32 I=0;I<64;++I){const double A=I*UE_TWO_PI/64,B=(I+1)*UE_TWO_PI/64;WorldLine(Ground(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius,12),Ground(Center+FVector2D(FMath::Cos(B),FMath::Sin(B))*Radius,12),C,1.4f);}};
    const bool NeighborhoodOverview=G->Screen==TEXT("playing")&&G->Zoom>=G->Sim.WorldHalfSize*2.8;
    if(G->Zoom>=G->Sim.WorldHalfSize*2.8)
    {
        const double Half=G->Sim.WorldHalfSize,Edge=Half*3;
        for(int32 I=0;I<4;++I)
        {
            const double At=-Edge+I*Half*2;
            WorldLine(Ground(FVector2D(At,-Edge),25),Ground(FVector2D(At,Edge),25),FLinearColor(.85f,.76f,.48f,.7f),1.5f);
            WorldLine(Ground(FVector2D(-Edge,At),25),Ground(FVector2D(Edge,At),25),FLinearColor(.85f,.76f,.48f,.7f),1.5f);
        }
        for(int32 I=0;I<9;++I)
        {
            if(G->Screen==TEXT("landing"))continue;
            const FVector2D P((I%3-1)*Half*2,(I/3-1)*Half*2);FVector2D Screen;
            if(PC&&PC->ProjectWorldLocationToScreen(Ground(P,50),Screen)&&Screen.Y/Scale>TopHeight+10)
            {
                const FString Type=G->ScenarioSlots.IsValidIndex(I)?G->ScenarioSlots[I]:TEXT("empty");
                const FString Name=I==4?(G->Observer?TEXT("AI home / observing"):TEXT("Your sector")):Type==TEXT("developed")?TEXT("Developed AI"):Type==TEXT("starting")?TEXT("Starting AI"):TEXT("Empty sector");
                Box(Screen.X/Scale-88,Screen.Y/Scale-15,176,34,Panel);Label(Name,Screen.X/Scale-78,Screen.Y/Scale-7,15,I==4?Gold:Muted);
            }
        }
    }
    auto DrawDeposits=[&](bool Survey,const TArray<FBox2D>& Reserved)
    {
        // Keep dense survey clusters legible without changing their world positions.
        struct FMarker { FVector2D Position; FString Resource; int32 Count=1; };
        TArray<FMarker> Markers;TArray<FBox2D> Occupied=Reserved;
        const bool Overview=G->Zoom>G->Sim.WorldHalfSize*2.5;
        for(const auto& N:G->Sim.Nodes)
        {
            FVector2D P;if((!Survey&&!G->Sim.IsVisible(N.Position))||!PC||!PC->ProjectWorldLocationToScreen(Ground(N.Position,150),P))continue;
            P/=Scale;if(P.X<16||P.X>W-16||P.Y<TopHeight+24||P.Y>H-28)continue;
            FMarker* Cluster=Overview?Markers.FindByPredicate([&](const FMarker& M){return M.Resource==N.Resource&&FVector2D::Distance(M.Position,P)<42;}):nullptr;
            if(Cluster){Cluster->Position=(Cluster->Position*Cluster->Count+P)/(Cluster->Count+1);++Cluster->Count;}
            else Markers.Add({P,N.Resource,1});
        }
        for(const auto& M:Markers)Occupied.Add(FBox2D(M.Position-FVector2D(10,10),M.Position+FVector2D(10,10)));
        for(const auto& M:Markers)if(const auto* R=G->Sim.Resources.Find(M.Resource))
        {
            const float SX=M.Position.X,SY=M.Position.Y;
            Box(SX-7,SY-7,14,14,R->Color);Box(SX-4,SY-4,8,8,Panel);
            FString Name=Overview?R->Name.Left(3).ToUpper():R->Name;if(M.Count>1)Name+=FString::Printf(TEXT(" x%d"),M.Count);
            float TW=0,TH=0,BW=0,BH=0;Canvas->StrLen(GEngine->GetLargeFont(),Name,TW,TH);Canvas->StrLen(GEngine->GetLargeFont(),TEXT("Ag"),BW,BH);
            const float LW=FMath::Max(Overview?54.f:94.f,TW*15/FMath::Max(BH,1.f)+16),LH=29;
            bool Placed=false;FVector2D At=FVector2D::ZeroVector;
            for(int32 Ring=0;Ring<5&&!Placed;++Ring)
            {
                const float Gap=14+Ring*33;
                const FVector2D Candidates[]={FVector2D(SX+14,SY-14+Ring*33),FVector2D(SX-LW-14,SY-14-Ring*33),FVector2D(SX-LW/2,SY-LH-Gap),FVector2D(SX-LW/2,SY+Gap)};
                for(const auto& Candidate:Candidates)
                {
                    const FBox2D Bounds(Candidate,Candidate+FVector2D(LW,LH));
                    if(Bounds.Min.X<12||Bounds.Max.X>W-12||Bounds.Min.Y<TopHeight+12||Bounds.Max.Y>H-12)continue;
                    if(Occupied.ContainsByPredicate([&](const FBox2D& B){return Bounds.Min.X<B.Max.X+4&&Bounds.Max.X>B.Min.X-4&&Bounds.Min.Y<B.Max.Y+4&&Bounds.Max.Y>B.Min.Y-4;}))continue;
                    At=Candidate;Occupied.Add(Bounds);Placed=true;break;
                }
            }
            if(Placed)
            {
                const FVector2D End(FMath::Clamp(double(SX),At.X,At.X+LW),FMath::Clamp(double(SY),At.Y,At.Y+LH));
                DrawLine(SX*Scale,SY*Scale,End.X*Scale,End.Y*Scale,R->Color,Scale);
                Box(At.X,At.Y,LW,LH,Panel);Label(Name,At.X+8,At.Y+6,15,R->Color);
            }
        }
    };
    if(G->Screen==TEXT("landing"))
    {
        // Survey information is available before the command core starts its live sensors.
        const FVector2D Suggested=G->HomePosition();FVector2D Guide;
        TArray<FBox2D> Reserved;
        if(PC&&PC->ProjectWorldLocationToScreen(Ground(Suggested,25),Guide)&&Guide.Y/Scale>TopHeight+20)
        {Circle(Suggested,350,Gold);const FVector2D P(Guide.X/Scale-95,Guide.Y/Scale+18);Reserved.Add(FBox2D(P,P+FVector2D(190,29)));Box(P.X,P.Y,190,29,Panel);Label(TEXT("Suggested landing"),P.X+10,P.Y+6,15,Gold);}
        DrawDeposits(true,Reserved);
        if(G->CursorOnWorld&&!WasOverUi)if(const auto* D=G->Sim.BuildingDefs.Find(G->Sim.CoreDefinition)){FString Why;Circle(G->CursorWorld,D->Footprint,G->CanLand(G->CursorWorld,Why)?Green:Red);}
        Box(0,0,W,TopHeight,Ink);Region(TEXT("top-bar"),0,0,W,TopHeight);Label(TEXT("seige2222"),22,13,25,Text);Label(FString::Printf(TEXT("v%s / %.0f FPS"),*Version,SmoothedFps),24,47,12,Muted);
        Label(TEXT("CHOOSE YOUR COMMAND CORE LOCATION"),255,16,21,Gold);Label(TEXT("The world is paused. Click valid ground in the center sector to land and begin."),255,46,15,Text);
        Button(TEXT("Main menu"),TEXT("main-menu"),W-180,16,155,44);
        const float NoticeHeight=DrawNotice(*G,W,H);
        FString Why;if(G->CursorOnWorld&&!G->CanLand(G->CursorWorld,Why)){float TY=105+NoticeHeight;Wrapped(Why,24,TY,600,16,Gold);}return;
    }
    if(G->Ready)
    {
        if(!NeighborhoodOverview&&!G->SelectedBuild.IsEmpty()&&G->CursorOnWorld&&!WasOverUi)if(const auto* D=G->Sim.BuildingDefs.Find(G->SelectedBuild)){FString Why;Circle(G->CursorWorld,D->Footprint,G->Sim.CanPlaceBuilding(G->SelectedBuild,G->CursorWorld,Why)?Green:Red);}
        if(const auto* B=G->Sim.FindBuilding(G->SelectedId))if(const auto* D=G->Sim.Definition(*B)){Circle(B->Position,D->Footprint+20,Green);if(D->AttackRange>0)Circle(B->Position,D->AttackRange,Gold);if(D->SensorRange>0)Circle(B->Position,D->SensorRange,FLinearColor(.35f,.68f,.85f));}
        for(const auto& B:G->Sim.Buildings)
        {
            const auto* D=G->Sim.Definition(B);if(!D||B.Health<=0||!B.Enabled||B.Workers<D->Jobs||D->DamagePerSecond<=0)continue;const FSeigeEnemy* Target=nullptr;double Closest=D->AttackRange;
            for(const auto& E:G->Sim.Enemies)if(G->Sim.IsVisible(E.Position)){const double Dist=FVector2D::Distance(B.Position,E.Position);if(Dist<Closest){Closest=Dist;Target=&E;}}
            if(Target&&FMath::Fmod(G->Sim.Time,.3)<.12)WorldLine(Ground(B.Position,D->Footprint*.7f),Ground(Target->Position,50),Green,2);
        }
        if(!NeighborhoodOverview)DrawDeposits(false,{});
    }
    Box(0,0,W,TopHeight,Ink);Box(0,TopHeight-1,W,1,FLinearColor(.30f,.32f,.23f));Region(TEXT("top-bar"),0,0,W,TopHeight);
    Label(TEXT("seige2222"),22,13,25,Text);Label(FString::Printf(TEXT("v%s  /  %.0f FPS"),*Version,SmoothedFps),24,47,12,Muted);
    Button(G->Observer?TEXT("Observer"):TEXT("B   Build"),G->Observer?TEXT("observer"):TEXT("build-menu"),220,15,108,46,Ui.BuildOpen);Button(TEXT("Colony"),TEXT("colony-menu"),338,15,100,46,Ui.ColonyOpen);
    auto Summary=[&](const FString& Id,const FString& Heading,const FString& Value,float X,float Width,FLinearColor C){Label(Heading,X+12,14,11,Muted);Label(Value,X+12,38,Id==TEXT("resources")?13:16,C);Region(TEXT("summary:")+Id,X,0,Width,TopHeight);Box(X,19,1,40,FLinearColor(.18f,.22f,.17f));};
    FString MajorStock;for(const auto& R:Ui.SummaryResources){if(!MajorStock.IsEmpty())MajorStock+=TEXT("  ");MajorStock+=FString::Printf(TEXT("%s %.0f"),*R.Label,G->Sim.TotalStock(R.Resource));}
    Summary(TEXT("resources"),TEXT("MATERIALS"),MajorStock,455,230,Text);
    Summary(TEXT("workforce"),TEXT("WORKFORCE"),FString::Printf(TEXT("%d robots / %d jobs"),G->Sim.Population,G->Sim.TotalJobs),685,175,G->Sim.TotalJobs>G->Sim.Employed?Gold:Text);
    Summary(TEXT("logistics"),TEXT("LOGISTICS"),FString::Printf(TEXT("%d couriers"),G->Sim.Couriers.Num()),860,130,Text);
    Summary(TEXT("threats"),TEXT("ALIEN PULSE"),FString::Printf(TEXT("%.0fs until pulse"),FMath::Max(0.,G->Sim.NextWaveTime-G->Sim.Time)),990,165,Gold);
    Summary(TEXT("objective"),TEXT("FIRST LANDING"),G->Sim.Won?TEXT("Complete"):TEXT("Objectives"),1155,135,Green);
    Label(FString::Printf(TEXT("%02d:%02d"),int32(G->Sim.Time)/60,int32(G->Sim.Time)%60),W-270,29,18,Text);
    Button(G->Paused?TEXT("Resume"):TEXT("Pause"),TEXT("pause"),W-185,15,82,46,G->Paused);Button(FString::Printf(TEXT("%.0fx"),G->Speed),TEXT("speed"),W-93,15,69,46,G->Speed>1);
    if(PreviousHover.StartsWith(TEXT("summary:"))&&!Ui.BuildOpen&&!Ui.ColonyOpen)Ui.HoverPanel=PreviousHover.RightChop(8);else if(!PreviousHover.StartsWith(TEXT("hover-panel:")))Ui.HoverPanel.Empty();
    if(!G->Ready)
    {
        Ui.HitRegions.Reset();Frame(W/2-340,H/2-160,680,320);Label(TEXT("RULE FILE ERROR"),W/2-310,H/2-130,26,Red);float Y=H/2-78;Wrapped(G->Error,W/2-310,Y,610,16,Text);Button(TEXT("Reload corrected rules"),TEXT("reset"),W/2-310,H/2+88,610,42);return;
    }
    if(Ui.BuildOpen)
    {
        Ui.HoverPanel.Empty();const float X=220,Y=90,BW=700,BH=448;Frame(X,Y,BW,BH);Label(TEXT("CONSTRUCTION"),X+18,Y+18,19,Gold);Button(TEXT("Close"),TEXT("close"),X+BW-87,Y+10,69,32);
        if(!InterfaceLoaded){float EY=Y+78;Wrapped(InterfaceError,X+20,EY,BW-40,16,Red);}
        else
        {
            if(PreviousHover.StartsWith(TEXT("group:")))Ui.Category=PreviousHover.RightChop(6);float CY=Y+66;
            for(const auto& Group:Ui.Categories){Button(Group.Shortcut+TEXT("   ")+Group.Name,TEXT("group:")+Group.Id,X+14,CY,246,51,Ui.Category==Group.Id,Group.Description);CY+=58;}
            Box(X+274,Y+65,1,BH-123,FLinearColor(.20f,.25f,.18f));
            if(const auto* Group=Ui.Categories.FindByPredicate([this](const FSeigeMenuGroup& I){return I.Id==Ui.Category;}))
            {
                float EY=Y+66;
                for(const auto& Entry:Group->Entries)if(const auto* D=G->Sim.BuildingDefs.Find(Entry.Definition))
                {
                    Button(TEXT(""),TEXT("build:")+D->Id,X+288,EY,394,54,false,D->Description);Icon(D->Visual,X+300,EY+8,38,D->Color);Label(D->Name,X+352,EY+10,16,Text);Label(FString::Printf(TEXT("%d jobs"),D->Jobs),X+352,EY+33,11,Muted);Box(X+640,EY+14,27,25,Raised);Label(Entry.Shortcut,X+648,EY+18,14,Gold);EY+=59;
                }
                if(PreviousHover.StartsWith(TEXT("build:")))if(const auto* D=G->Sim.BuildingDefs.Find(PreviousHover.RightChop(6)))Description(*D,*G,X+BW+14,Y,FMath::Min(420.f,W-X-BW-30));
            }
            Box(X+14,Y+BH-55,BW-28,1,FLinearColor(.19f,.22f,.17f));Label(Ui.GroupFocused?TEXT("Press a building key, then click terrain.  Esc: categories"):TEXT("Choose R / I / L / D.  Hover a building for details."),X+18,Y+BH-37,13,Muted);
        }
    }
    else if(Ui.ColonyOpen)
    {
        Ui.HoverPanel.Empty();const float X=338,Y=90;Frame(X,Y,310,505);Label(TEXT("COLONY COMMAND"),X+18,Y+18,17,Gold);
        Button(TEXT("Save colony                  F5"),TEXT("save"),X+12,Y+58,286,43);Button(TEXT("Load colony                  F9"),TEXT("load"),X+12,Y+107,286,43);
        if(!G->Observer)Button(TEXT("Launch escape shuttle"),TEXT("escape"),X+12,Y+166,286,43);
        Button(TEXT("Settings"),TEXT("screen:settings"),X+12,Y+215,286,43);Button(TEXT("Credits"),TEXT("screen:credits"),X+12,Y+264,286,43);
        Button(TEXT("Return to main menu"),TEXT("main-menu"),X+12,Y+313,286,43);Button(TEXT("Exit game"),TEXT("exit"),X+12,Y+362,286,43);
        float TY=Y+430;Wrapped(TEXT("B: build / WASD: pan / Wheel: zoom / Q,E: orbit / Middle drag: orbit + tilt / Home: core"),X+18,TY,274,13,Muted);
    }
    else if(!Ui.HoverPanel.IsEmpty())
    {
        float X=455;if(Ui.HoverPanel==TEXT("workforce"))X=685;else if(Ui.HoverPanel==TEXT("logistics"))X=860;else if(Ui.HoverPanel==TEXT("threats"))X=990;else if(Ui.HoverPanel==TEXT("objective"))X=FMath::Min(1155.f,W-455);
        const float Y=90,PW=425,PH=Ui.HoverPanel==TEXT("resources")?356:242;Frame(X,Y,PW,PH);Region(TEXT("hover-panel:")+Ui.HoverPanel,X,Y-13,PW,PH+13);float TY=Y+21;
        if(Ui.HoverPanel==TEXT("resources"))
        {
            Label(TEXT("COLONY MATERIALS"),X+18,TY,18,Gold);TY+=39;Label(TEXT("Includes physical cargo in transit"),X+18,TY,12,Muted);TY+=28;TArray<FString> Keys;G->Sim.Resources.GetKeys(Keys);Keys.Sort();
            for(const FString& Id:Keys){const auto& R=G->Sim.Resources[Id];Box(X+19,TY+4,7,9,R.Color);Label(R.Name,X+37,TY,15,Text);Label(FString::Printf(TEXT("%.1f"),G->Sim.TotalStock(Id)),X+340,TY,15,Text);TY+=26;}
        }
        else if(Ui.HoverPanel==TEXT("workforce")){Label(TEXT("ROBOT WORKFORCE"),X+18,TY,18,Gold);TY+=43;Wrapped(G->Sim.WorkforceStatus(),X+18,TY,PW-36,16,Text);TY+=12;Wrapped(TEXT("Robots fill jobs automatically. The core assembles workers for vacancies and retires surplus when buildings are disabled."),X+18,TY,PW-36,14,Muted);}
        else if(Ui.HoverPanel==TEXT("logistics")){Label(TEXT("PHYSICAL LOGISTICS"),X+18,TY,18,Gold);TY+=43;Wrapped(FString::Printf(TEXT("%d couriers moving / %.0f units delivered / %d couriers lost"),G->Sim.Couriers.Num(),G->Sim.DeliveredUnits,G->Sim.LostCouriers),X+18,TY,PW-36,16,Text);TY+=12;Wrapped(TEXT("Factories consume locally delivered stock. Protect exposed routes and use depots for overflow. Construction draws from the core."),X+18,TY,PW-36,14,Muted);}
        else if(Ui.HoverPanel==TEXT("threats")){Label(TEXT("SECTOR PRESSURE"),X+18,TY,18,Gold);TY+=43;Wrapped(FString::Printf(TEXT("Pulse %d / Next pulse in %.0f seconds"),G->Sim.Wave,FMath::Max(0.,G->Sim.NextWaveTime-G->Sim.Time)),X+18,TY,PW-36,16,Text);TY+=12;Wrapped(TEXT("Roaming bugs can arrive at any time. Sensors reveal live contacts; defenses require staffing. Repairs consume local materials."),X+18,TY,PW-36,14,Muted);}
        else{Label(TEXT("FIRST LANDING OBJECTIVES"),X+18,TY,18,Gold);TY+=43;TArray<FString> Goals;G->Sim.ObjectiveText().ParseIntoArray(Goals,TEXT(" | "),true);for(const FString& Goal:Goals){Wrapped(Goal,X+18,TY,PW-36,16,Text);TY+=7;}}
    }
    if(!Ui.BuildOpen&&!Ui.ColonyOpen&&Ui.HoverPanel.IsEmpty())
    {
        const auto* B=G->Sim.FindBuilding(G->SelectedId);
        if(B)
        {
            if(const auto* D=G->Sim.Definition(*B))
            {
                const float X=W-380,Y=90;Frame(X,Y,356,375);Icon(D->Visual,X+18,Y+18,38,D->Color);Label(D->Name,X+69,Y+23,20,Gold);Button(TEXT("x"),TEXT("deselect"),X+313,Y+9,30,30);
                float TY=Y+77;Wrapped(B->Status,X+18,TY,320,15,Green);TY+=10;Label(FString::Printf(TEXT("Hull %.0f / %.0f    Jobs %d / %d"),B->Health,D->Health,B->Workers,D->Jobs),X+18,TY,15,Text);TY+=34;Label(TEXT("LOCAL INVENTORY"),X+18,TY,12,Muted);TY+=25;
                int32 Shown=0;TArray<FString> Keys;B->Inventory.GetKeys(Keys);Keys.Sort();for(const FString& Id:Keys)if(B->Inventory[Id]>.05&&Shown++<5){Label(FString::Printf(TEXT("%.1f  %s"),B->Inventory[Id],*ResourceName(G->Sim,Id)),X+18,TY,14,Text);TY+=23;}
                if(D->Role!=TEXT("core")&&!G->Observer)Button(B->Enabled?TEXT("Disable building"):TEXT("Enable building"),TEXT("toggle"),X+18,Y+321,320,37,!B->Enabled);
            }
        }
        else if(!NeighborhoodOverview&&!G->SelectedBuild.IsEmpty())if(const auto* D=G->Sim.BuildingDefs.Find(G->SelectedBuild))
        {
            const float X=W-390,Y=90;Frame(X,Y,366,130);Icon(D->Visual,X+18,Y+18,38,D->Color);Label(D->Name,X+70,Y+22,19,Gold);float TY=Y+73;FString Why;const bool Valid=G->CursorOnWorld&&G->Sim.CanPlaceBuilding(D->Id,G->CursorWorld,Why);Wrapped(Valid?TEXT("Click terrain to place. Esc cancels."):Why,X+18,TY,330,14,Valid?Green:Muted);
        }
    }
    if(!G->Observer&&(G->Sim.Escaped||G->Sim.Failed||(G->Sim.Won&&!G->WinAcknowledged)))
    {
        Ui.HitRegions.Reset();Ui.CloseMenus();const bool Victory=G->Sim.Won&&!G->Sim.Escaped&&!G->Sim.Failed;Frame(W/2-330,H/2-125,660,225);Label(Victory?TEXT("FIRST LANDING COMPLETE"):TEXT("COLONY EVACUATED"),W/2-304,H/2-94,27,Gold);
        float TY=H/2-43;Wrapped(Victory?TEXT("Your industry survived. Continue building or establish a new colony."):TEXT("The shuttle escaped with only cargo already aboard. Establish a new colony to begin again."),W/2-304,TY,610,16,Text);
        if(Victory){Button(TEXT("Continue"),TEXT("continue"),W/2-304,H/2+41,192,39);Button(TEXT("New colony"),TEXT("reset"),W/2-96,H/2+41,192,39);Button(TEXT("Main menu"),TEXT("main-menu"),W/2+112,H/2+41,192,39);}
        else{Button(TEXT("New colony"),TEXT("reset"),W/2-304,H/2+41,291,39);Button(TEXT("Main menu"),TEXT("main-menu"),W/2+13,H/2+41,291,39);}
    }
    DrawNotice(*G,W,H);
}
