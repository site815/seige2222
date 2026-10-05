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
#include "HAL/PlatformTime.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "SceneView.h"

namespace {
const FLinearColor Ink(.023f,.029f,.033f,.93f),Panel(.043f,.052f,.057f,.95f),Raised(.105f,.124f,.133f,.99f);
const FLinearColor Gold(.86f,.80f,.63f),Green(.55f,.79f,.68f),Text(.94f,.92f,.85f),Muted(.65f,.70f,.70f),Red(.96f,.46f,.38f);
constexpr float TopHeight=80;
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
        if(G.Observer||G.IsRegionMap()||G.DetailedSectorIndex()!=4)return true;
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
            if(Action.StartsWith(TEXT("focus-sector:"))||Action==TEXT("region-map")||Action.StartsWith(TEXT("screen:"))||Action.StartsWith(TEXT("slot:"))||Action.StartsWith(TEXT("quality:"))||Action==TEXT("start-scenario")||Action==TEXT("main-menu")||Action==TEXT("back-screen")||Action==TEXT("load")||Action==TEXT("fullscreen")||Action==TEXT("exit"))ExecuteAction(Action,G);
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
    if(A==TEXT("region-map")){Ui.CloseMenus();G.SelectedBuild.Empty();G.SelectedId=0;G.Zoom=G.Sim.WorldHalfSize*12;G.CameraCenter=FVector::ZeroVector;G.UpdateCamera();return true;}
    if(A.StartsWith(TEXT("focus-sector:"))){const int32 Index=FCString::Atoi(*A.RightChop(13));if(G.Screen!=TEXT("landing")||Index==4){Ui.CloseMenus();G.FocusSector(Index);}return true;}
    if(A==TEXT("build-menu"))return ProcessShortcut(EKeys::B,G);
    if(A==TEXT("colony-menu")){const bool Open=!Ui.ColonyOpen;Ui.CloseMenus();Ui.ColonyOpen=Open;return true;}
    if(A.StartsWith(TEXT("group:"))){Ui.Category=A.RightChop(6);Ui.GroupFocused=true;return true;}
    if(A.StartsWith(TEXT("summary:"))){Ui.HoverPanel=A.RightChop(8);return true;}
    if(A==TEXT("back")){Ui.GroupFocused=false;return true;}if(A==TEXT("close")){Ui.CloseMenus();return true;}
    if(A==TEXT("deselect")){G.SelectedId=0;G.SelectedBuild.Empty();BuildingInfoSection.Empty();return true;}
    if(A.StartsWith(TEXT("info-section:"))){BuildingInfoSection=A.RightChop(13);BuildingInfoPage=0;return true;}
    if(A==TEXT("info-next")){++BuildingInfoPage;return true;}
    if(A==TEXT("info-prev")){BuildingInfoPage=FMath::Max(0,BuildingInfoPage-1);return true;}
    if(A.StartsWith(TEXT("build:")))
    {
        if(G.Observer||G.IsRegionMap()||G.DetailedSectorIndex()!=4)return true;
        const FString Id=A.RightChop(6);
        if(G.Sim.BuildMenu.Contains(Id)){G.SelectedBuild=Id;G.SelectedId=0;Ui.CloseMenus();G.Notice=TEXT("Place ")+G.Sim.BuildingDefs[Id].Name+TEXT(". Right-click or Esc cancels.");}
        return true;
    }
    if(A==TEXT("pause"))G.Paused=!G.Paused;else if(A==TEXT("speed"))G.Speed=G.Speed==1?3:1;
    else if(A==TEXT("save")){LastNotice.Empty();G.SaveGame();Ui.CloseMenus();}else if(A==TEXT("load")){LastNotice.Empty();G.LoadGame();Ui.CloseMenus();}
    else if(A==TEXT("reset")){G.ResetColony();Ui.CloseMenus();}else if(A==TEXT("continue"))G.WinAcknowledged=true;
    else if(A==TEXT("toggle")&&!G.Observer&&!G.IsRegionMap()&&G.DetailedSectorIndex()==4)G.Sim.ToggleBuilding(G.SelectedId);
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
void ASeigeHUD::Frame(float X,float Y,float W,float H){Box(X+3,Y+5,W,H,FLinearColor(0,0,0,.28f));Box(X,Y,W,H,Panel);Box(X,Y,W,1,FLinearColor(.43f,.47f,.46f,.7f));Box(X,Y+H-1,W,1,FLinearColor(.24f,.29f,.29f,.7f));Region(TEXT("panel"),X,Y,W,H);}
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
    const float NW=FMath::Min(700.f,W-420),X=(W-NW)*.5f,Y=TopHeight+10;
    auto Lines=WrapLines(LastNotice,NW-36,15);
    if(Lines.Num()>3){Lines.SetNum(3);Lines.Last()+=TEXT(" ...");}
    const float NH=22+Lines.Num()*22;
    if(Ui.HitRegions.ContainsByPredicate([&](const FSeigeButton& R){return R.Action!=TEXT("region-surface")&&!R.Action.StartsWith(TEXT("focus-sector:"))&&X<R.Position.X+R.Size.X&&X+NW>R.Position.X&&Y<R.Position.Y+R.Size.Y&&Y+NH>R.Position.Y;}))return 0;
    Frame(X,Y,NW,NH);Box(X,Y,3,NH,!G.Error.IsEmpty()&&G.Notice==G.Error?Red:Gold);
    float TY=Y+11;for(const auto& Line:Lines){Label(Line,X+18,TY,15,Text);TY+=22;}
    NoticeVisibleSeconds+=FMath::Max(0.f,GetWorld()->GetDeltaSeconds());return NH+12;
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
    TArray<FString> Keys;D.Cost.GetKeys(Keys);Keys.Sort();
    for(const FString& Id:Keys){const double Held=G.Sim.ConstructionAvailable(Id);Label(FString::Printf(TEXT("%.0f  %s"),D.Cost[Id],*ResourceName(G.Sim,Id)),X+18,TY,15,Held>=D.Cost[Id]?Text:Red);TY+=24;}
    TY+=7;Label(FString::Printf(TEXT("%.0fs assembly / %d builders / %d jobs"),D.ConstructionSeconds,D.ConstructionWorkers,D.Jobs),X+18,TY,13,Muted);TY+=27;
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
void ASeigeHUD::DrawBuildingInfo(ASeigeGameMode& G,float W,float H)
{
    const auto* S=G.ViewedSimulation();if(!S||(!G.Observer&&G.DetailedSectorIndex()!=4))return;const auto* B=S->FindBuilding(G.SelectedId);const auto* D=B?S->Definition(*B):S->BuildingDefs.Find(G.SelectedBuild);if(!D)return;
    const auto Rows=S->BuildingInfo(D->Id,B?B->Id:0,G.RenderScale);TArray<FString> Sections;
    for(const auto& R:Rows)Sections.AddUnique(R.Section);
    if(Sections.IsEmpty())return;
    if(!Sections.Contains(BuildingInfoSection)){BuildingInfoSection=Sections[0];BuildingInfoPage=0;}
    const float PW=416,X=W-PW-24,Y=102,MaxH=H-Y-110,ContentY=Y+173,ContentH=MaxH-227;
    TArray<TArray<int32>> Pages;Pages.Emplace();float Used=0;
    auto RowHeight=[&](const FSeigeBuildingInfoRow& R){return FMath::Max(WrapLines(R.Label,155,13).Num(),WrapLines(R.Value,205,14).Num())*21.f+12;};
    for(int32 I=0;I<Rows.Num();++I)if(Rows[I].Section==BuildingInfoSection)
    {
        const float RH=RowHeight(Rows[I]);if(Used+RH>ContentH&&!Pages.Last().IsEmpty()){Pages.Emplace();Used=0;}
        Pages.Last().Add(I);Used+=RH;
    }
    BuildingInfoPage=FMath::Clamp(BuildingInfoPage,0,Pages.Num()-1);
    float PageHeight=0;for(int32 I:Pages[BuildingInfoPage])PageHeight+=RowHeight(Rows[I]);
    const float PH=FMath::Max(350.f,FMath::Min(MaxH,PageHeight+230));
    Frame(X,Y,PW,PH);Icon(D->Visual,X+17,Y+18,37,D->Color);Label(D->Name,X+68,Y+21,19,Gold);
    Button(TEXT("x"),TEXT("deselect"),X+PW-39,Y+9,29,29);
    Label(!B?TEXT("BLUEPRINT / BUILDING SPECIFICATION"):G.Observer||G.DetailedSectorIndex()!=4?TEXT("BUILDING DOSSIER / READ ONLY"):TEXT("BUILDING DOSSIER"),X+68,Y+49,10,Muted);
    const float TabW=(PW-34)/3;for(int32 I=0;I<Sections.Num();++I)
        Button(Sections[I],TEXT("info-section:")+Sections[I],X+17+(I%3)*TabW,Y+81+(I/3)*36,TabW-5,31,Sections[I]==BuildingInfoSection);
    Box(X+17,ContentY-12,PW-34,1,FLinearColor(.22f,.27f,.27f));
    float TY=ContentY;
    for(int32 I:Pages[BuildingInfoPage])
    {
        float LY=TY,VY=TY;Wrapped(Rows[I].Label,X+18,LY,155,13,Muted);Wrapped(Rows[I].Value,X+184,VY,205,14,Text);
        TY+=RowHeight(Rows[I]);Box(X+18,TY-5,PW-36,1,FLinearColor(.12f,.16f,.17f,.7f));
    }
    const float FY=Y+PH-43;
    if(Pages.Num()>1)
    {
        Button(TEXT("<"),TEXT("info-prev"),X+18,FY,36,29);Label(FString::Printf(TEXT("%d / %d"),BuildingInfoPage+1,Pages.Num()),X+64,FY+7,12,Muted);Button(TEXT(">"),TEXT("info-next"),X+119,FY,36,29);
    }
    if(B&&D->Role!=TEXT("core")&&!G.Observer&&G.DetailedSectorIndex()==4)Button(B->Enabled?TEXT("Disable building"):TEXT("Enable building"),TEXT("toggle"),X+PW-217,FY,199,29,!B->Enabled);
}
void ASeigeHUD::DrawRegionMap(ASeigeGameMode& G,float W,float H)
{
    // A survey representation uses the same world coordinates, heights and woodland
    // distribution as detailed terrain. No enemy positions are exposed here.
    Box(0,0,W,H,FLinearColor(.065f,.080f,.082f,1));Region(TEXT("region-surface"),0,0,W,H);
    const float Size=FMath::Min(H-230,W-500),X=(W-Size)*.5f,Y=118,Cell=Size/3;
    Frame(X-9,Y-9,Size+18,Size+18);Ui.HitRegions.Last().Action=TEXT("region-surface");Box(X,Y,Size,Size,FLinearColor(.63f,.62f,.54f,1));
    const double Edge=G.Sim.WorldHalfSize*3;
    auto Map=[&](FVector2D P){return FVector2D(X+(P.X+Edge)/(Edge*2)*Size,Y+(P.Y+Edge)/(Edge*2)*Size);};
    auto Line=[&](FVector2D A,FVector2D B,FLinearColor C,float Thick=1.f){DrawLine(A.X*Scale,A.Y*Scale,B.X*Scale,B.Y*Scale,C,Thick*Scale);};
    for(int32 J=0;J<34;++J)for(int32 I=0;I<34;++I)
    {
        // Stable jitter breaks the symbol lattice without inventing different woods.
        FRandomStream Jitter(1947+J*104729+I*65537);
        const FVector2D P(-Edge+(I+.5+Jitter.FRandRange(-.38,.38))*(Edge*2/34),-Edge+(J+.5+Jitter.FRandRange(-.38,.38))*(Edge*2/34));
        const float Density=G.WoodlandDensity(P);
        if(Density<.48f)continue;const FVector2D Q=Map(P);const float R=(1.6f+Density*2.5f)*Jitter.FRandRange(.8f,1.15f);const FLinearColor C(.20f,.32f,.27f,.20f+Density*.28f);
        Line(Q+FVector2D(-R,R*.6f),Q+FVector2D(0,-R),C);Line(Q+FVector2D(0,-R),Q+FVector2D(R,R*.6f),C);Line(Q+FVector2D(-R,R*.6f),Q+FVector2D(R,R*.6f),C);
    }
    // Sparse contours use a finer sampled grid and remain cached between redraws.
    struct FContour {FVector2D A,B;};static TArray<FContour> Contours;static double LastSample=-10,LastEdge=0;
    const double Now=FPlatformTime::Seconds();
    if(Now-LastSample>2||LastEdge!=Edge)
    {
        LastSample=Now;LastEdge=Edge;Contours.Reset();constexpr int32 N=60;TArray<double> Heights;Heights.SetNum((N+1)*(N+1));const double Step=Edge*2/N;
        for(int32 J=0;J<=N;++J)for(int32 I=0;I<=N;++I)Heights[J*(N+1)+I]=G.GroundHeight(FVector2D(-Edge+I*Step,-Edge+J*Step));
        for(int32 J=0;J<N;++J)for(int32 I=0;I<N;++I)
        {
            const FVector2D P(-Edge+I*Step,-Edge+J*Step);const FVector2D Corners[]={P,P+FVector2D(Step,0),P+FVector2D(Step,Step),P+FVector2D(0,Step)};
            const double Z[]={Heights[J*(N+1)+I],Heights[J*(N+1)+I+1],Heights[(J+1)*(N+1)+I+1],Heights[(J+1)*(N+1)+I]};
            const double Min=FMath::Min(FMath::Min(Z[0],Z[1]),FMath::Min(Z[2],Z[3])),Max=FMath::Max(FMath::Max(Z[0],Z[1]),FMath::Max(Z[2],Z[3]));
            for(double Level=FMath::CeilToDouble(Min/500)*500;Level<Max;Level+=500)
            {
                TArray<FVector2D> Crossings;for(int32 E=0;E<4;++E){const int32 K=(E+1)%4;if((Z[E]<Level)!=(Z[K]<Level))Crossings.Add(FMath::Lerp(Corners[E],Corners[K],(Level-Z[E])/(Z[K]-Z[E])));}
                for(int32 C=0;C+1<Crossings.Num();C+=2)Contours.Add({Crossings[C],Crossings[C+1]});
            }
        }
    }
    for(const auto& C:Contours)Line(Map(C.A),Map(C.B),FLinearColor(.29f,.29f,.24f,.13f));
    float MX=-1,MY=-1;if(auto* PC=GetOwningPlayerController())PC->GetMousePosition(MX,MY);MX/=Scale;MY/=Scale;
    for(int32 I=0;I<9;++I)
    {
        const float CX=X+(I%3)*Cell,CY=Y+(I/3)*Cell;const bool CanFocus=G.Screen!=TEXT("landing")||I==4;
        const bool Hover=MX>=CX&&MX<CX+Cell&&MY>=CY&&MY<CY+Cell;
        if(I==4)Box(CX+1,CY+1,Cell-2,Cell-2,FLinearColor(.88f,.77f,.48f,.12f));
        if(Hover&&CanFocus)Box(CX+1,CY+1,Cell-2,Cell-2,FLinearColor(.87f,.83f,.60f,.16f));
        Region(CanFocus?TEXT("focus-sector:")+FString::FromInt(I):TEXT("region-surface"),CX,CY,Cell,Cell);
        const FString Type=G.ScenarioSlots.IsValidIndex(I)?G.ScenarioSlots[I]:TEXT("empty");
        const FSeigeSimulation* Sector=I==4?&G.Sim:nullptr;for(const auto& N:G.Neighbors)if(N.Index==I){Sector=&N.Sim;break;}
        const FVector2D Offset((I%3-1)*G.Sim.WorldHalfSize*2,(I/3-1)*G.Sim.WorldHalfSize*2);
        if(Sector&&(G.Observer||I==4))for(const auto& Node:Sector->Nodes)
        {
            if(!G.Observer&&G.Screen!=TEXT("landing")&&!Sector->IsVisible(Node.Position))continue;
            const auto* Resource=Sector->Resources.Find(Node.Resource);if(!Resource)continue;const FVector2D At=Map(Node.Position+Offset);
            Box(At.X-3,At.Y-3,6,6,Resource->Color);Box(At.X-1,At.Y-1,2,2,Ink);
        }
        const FString Name=I==4?TEXT("HOME SECTOR"):FString::Printf(TEXT("SECTOR %02d"),I<4?I+1:I);
        const FString State=Type==TEXT("player")?TEXT("Your colony"):Type==TEXT("developed")?TEXT("Developed AI"):Type==TEXT("starting")?TEXT("Starting AI"):TEXT("Unsettled wilderness");
        const float TagW=FMath::Min(Cell-24,190.f),TagX=CX+(Cell-TagW)*.5f,TagY=CY+Cell-68;
        Box(TagX,TagY,TagW,52,Panel);Label(Name,TagX+13,TagY+10,12,I==4?Gold:Text);Label(State,TagX+13,TagY+30,12,Muted);
        if(Hover&&CanFocus){Label(TEXT("CLICK TO INSPECT"),CX+14,CY+14,11,FLinearColor(.15f,.21f,.21f));}
    }
    for(int32 I=0;I<4;++I){Line(FVector2D(X+I*Cell,Y),FVector2D(X+I*Cell,Y+Size),FLinearColor(.16f,.21f,.21f,.93f),2.4f);Line(FVector2D(X,Y+I*Cell),FVector2D(X+Size,Y+I*Cell),FLinearColor(.16f,.21f,.21f,.93f),2.4f);}
    const FVector2D HomeCorners[]={FVector2D(X+Cell,Y+Cell),FVector2D(X+Cell*2,Y+Cell),FVector2D(X+Cell*2,Y+Cell*2),FVector2D(X+Cell,Y+Cell*2)};
    for(int32 I=0;I<4;++I){Line(HomeCorners[I],HomeCorners[(I+1)%4],FLinearColor(.23f,.24f,.19f,.95f),5.f);Line(HomeCorners[I],HomeCorners[(I+1)%4],FLinearColor(.82f,.70f,.40f,.97f),2.6f);}
    const float LX=FMath::Max(24.f,X-310);Label(TEXT("REGIONAL SURVEY"),LX,Y+24,23,Gold);float TY=Y+68;
    Wrapped(TEXT("Select a sector to inspect its terrain and settlement. Only the focused sector uses the detailed world view."),LX,TY,270,15,Text);TY+=26;
    Wrapped(G.Observer?TEXT("Observation reveals AI industry. Colonies still run independent simulations."):TEXT("Survey symbols respect your known deposits. This map does not reveal hostile units beyond your sensors."),LX,TY,270,14,Muted);TY+=34;
    Label(FString::Printf(TEXT("%.1f km per sector"),G.Sim.WorldHalfSize*2*G.RenderScale/100000),LX,TY,15,Gold);
    Label(TEXT("Woodland / terrain contours"),LX,TY+32,12,Muted);
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
    const bool RegionMap=G->IsRegionMap();const auto* Viewed=G->ViewedSimulation();const bool Readable=Viewed&&(G->Observer||G->DetailedSectorIndex()==4);const FSeigeSimulation& Local=Readable?*Viewed:G->Sim;const FVector2D SectorOffset=G->DetailedSectorOffset();
    if(RegionMap){Ui.BuildOpen=false;Ui.GroupFocused=false;G->SelectedBuild.Empty();}
    if(RegionMap)DrawRegionMap(*G,W,H);
    auto WorldLine=[&](FVector A,FVector B,FLinearColor C,float Thickness){FVector2D P,Q;if(RegionMap||!PC||!PC->ProjectWorldLocationToScreen(A,P)||!PC->ProjectWorldLocationToScreen(B,Q))return;if(P.Y/Scale>TopHeight&&Q.Y/Scale>TopHeight)DrawLine(P.X,P.Y,Q.X,Q.Y,C,Thickness*Scale);};
    auto Ground=[&](FVector2D P,float Lift){return G->RenderPosition(P,Lift);};
    auto Circle=[&](FVector2D Center,double Radius,FLinearColor C){for(int32 I=0;I<64;++I){const double A=I*UE_TWO_PI/64,B=(I+1)*UE_TWO_PI/64;WorldLine(Ground(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius,12),Ground(Center+FVector2D(FMath::Cos(B),FMath::Sin(B))*Radius,12),C,1.4f);}};
    const bool NeighborhoodOverview=RegionMap;
    if(!RegionMap)
    {
        // Continuous survey boundaries are anchored to the terrain, independent
        // of the active tile's detail level or hidden settlement information.
        const double Half=G->Sim.WorldHalfSize,Extent=Half*3;
        const double Reach=G->Zoom*2.5+1200;
        for(int32 Axis=0;Axis<2;++Axis)for(double Coordinate:{-Extent,-Half,Half,Extent})
        {
            const double Cross=Axis==0?G->CameraCenter.X:G->CameraCenter.Y;
            if(FMath::Abs(Cross-Coordinate)>Reach)continue;
            const double Along=Axis==0?G->CameraCenter.Y:G->CameraCenter.X;
            const double Low=FMath::Max(-Extent,Along-Reach),High=FMath::Min(Extent,Along+Reach);
            const int32 Segments=FMath::Clamp(FMath::CeilToInt((High-Low)/500),1,192);
            for(int32 I=0;I<Segments;++I)
            {
                const double A=FMath::Lerp(Low,High,double(I)/Segments),B=FMath::Lerp(Low,High,double(I+1)/Segments);
                const bool Home=FMath::Abs(Coordinate)==Half&&FMath::Abs((A+B)*.5)<=Half;
                const FVector2D P=Axis==0?FVector2D(Coordinate,A):FVector2D(A,Coordinate);
                const FVector2D Q=Axis==0?FVector2D(Coordinate,B):FVector2D(B,Coordinate);
                WorldLine(Ground(P,14),Ground(Q,14),FLinearColor(.015f,.025f,.025f,.85f),4);
                WorldLine(Ground(P,14),Ground(Q,14),Home?Gold:FLinearColor(.58f,.72f,.7f,.8f),1.8f);
            }
        }
    }
    const FTransform LabelView=G->CameraTransform();
    const bool LabelCameraMoved=!LabelCameraPosition.Equals(LabelView.GetLocation(),.1)||!LabelCameraRotation.Equals(LabelView.Rotator(),.01)||!LabelViewport.Equals(FVector2D(W,H),.1);
    LabelStillSeconds=LabelCameraMoved?0:FMath::Min(1.f,LabelStillSeconds+Dt);
    LabelCameraPosition=LabelView.GetLocation();LabelCameraRotation=LabelView.Rotator();LabelViewport=FVector2D(W,H);
    auto DrawDeposits=[&](bool Survey,const TArray<FBox2D>& Reserved)
    {
        // Retain per-deposit anchors during camera movement; resolve overlap only
        // after it settles. Never hide a badge because greedy placement failed.
        struct FMarker {FVector2D Position;FString Resource,Key;};
        TArray<FMarker> Markers;TArray<FBox2D> Occupied=Reserved;
        if(RegionMap||!Readable)return;
        for(const auto& N:Local.Nodes)
        {
            FVector2D P;
            if((!Survey&&!G->Observer&&!Local.IsVisible(N.Position))||!PC||!PC->ProjectWorldLocationToScreen(Ground(N.Position+SectorOffset,150),P))continue;
            P/=Scale;if(P.X<8||P.X>W-8||P.Y<TopHeight+12||P.Y>H-18)continue;
            P=FVector2D(FMath::RoundToDouble(P.X),FMath::RoundToDouble(P.Y));
            Markers.Add({P,N.Resource,FString::Printf(TEXT("%d:%d"),G->DetailedSectorIndex(),N.Id)});
        }
        for(const auto& M:Markers)Occupied.Add(FBox2D(M.Position-FVector2D(10,10),M.Position+FVector2D(10,10)));
        for(const auto& M:Markers)if(const auto* R=Local.Resources.Find(M.Resource))
        {
            const float SX=M.Position.X,SY=M.Position.Y;
            Box(SX-7,SY-7,14,14,R->Color);Box(SX-4,SY-4,8,8,Panel);
            float TW=0,TH=0,BW=0,BH=0;Canvas->StrLen(GEngine->GetLargeFont(),R->Name,TW,TH);Canvas->StrLen(GEngine->GetLargeFont(),TEXT("Ag"),BW,BH);
            const float LW=FMath::Max(94.f,TW*15/FMath::Max(BH,1.f)+16),LH=29;
            auto& State=DepositLabels.FindOrAdd(M.Key);
            FVector2D At=M.Position+State.Offset;
            auto Fits=[&](FVector2D P)
            {
                const FBox2D Bounds(P,P+FVector2D(LW,LH));
                if(Bounds.Min.X<12||Bounds.Max.X>W-12||Bounds.Min.Y<TopHeight+12||Bounds.Max.Y>H-92)return false;
                return !Occupied.ContainsByPredicate([&](const FBox2D& B){return Bounds.Min.X<B.Max.X+4&&Bounds.Max.X>B.Min.X-4&&Bounds.Min.Y<B.Max.Y+4&&Bounds.Max.Y>B.Min.Y-4;});
            };
            if(!State.Initialized||(LabelStillSeconds>.25f&&!Fits(At)))
            {
                bool Found=false;
                for(int32 Ring=0;Ring<5&&!Found;++Ring)
                {
                    const float Gap=14+Ring*33;
                    const FVector2D Candidates[]={FVector2D(SX+14,SY-14+Ring*33),FVector2D(SX-LW-14,SY-14-Ring*33),FVector2D(SX-LW/2,SY-LH-Gap),FVector2D(SX-LW/2,SY+Gap)};
                    for(const auto& Candidate:Candidates)if(Fits(Candidate)){At=Candidate;Found=true;break;}
                }
                State.Offset=At-M.Position;State.Initialized=true;
            }
            At.X=FMath::Clamp(At.X,12.,FMath::Max(12.,double(W-LW-12)));
            At.Y=FMath::Clamp(At.Y,double(TopHeight+12),FMath::Max(double(TopHeight+12),double(H-LH-92)));
            At=FVector2D(FMath::RoundToDouble(At.X),FMath::RoundToDouble(At.Y));
            Occupied.Add(FBox2D(At,At+FVector2D(LW,LH)));
            const FVector2D End(FMath::Clamp(double(SX),At.X,At.X+LW),FMath::Clamp(double(SY),At.Y,At.Y+LH));
            DrawLine(SX*Scale,SY*Scale,End.X*Scale,End.Y*Scale,R->Color,Scale);
            Box(At.X,At.Y,LW,LH,Panel);Label(R->Name,At.X+8,At.Y+6,15,R->Color);
        }
    };
    if(G->Screen==TEXT("landing"))
    {
        // Survey information is available before the command core starts its live sensors.
        const FVector2D Suggested=G->HomePosition();FVector2D Guide;
        TArray<FBox2D> Reserved;
        if(!RegionMap&&PC&&PC->ProjectWorldLocationToScreen(Ground(Suggested,25),Guide)&&Guide.Y/Scale>TopHeight+20)
        {Circle(Suggested,350,Gold);const FVector2D P(Guide.X/Scale-95,Guide.Y/Scale+18);Reserved.Add(FBox2D(P,P+FVector2D(190,29)));Box(P.X,P.Y,190,29,Panel);Label(TEXT("Suggested landing"),P.X+10,P.Y+6,15,Gold);}
        DrawDeposits(true,Reserved);
        if(!RegionMap&&G->CursorOnWorld&&!WasOverUi)if(const auto* D=G->Sim.BuildingDefs.Find(G->Sim.CoreDefinition)){FString Why;Circle(G->CursorWorld,D->Footprint,G->CanLand(G->CursorWorld,Why)?Green:Red);}
        Label(TEXT("seige2222"),24,21,21,Text);Label(FString::Printf(TEXT("v%s / %.0f FPS"),*Version,SmoothedFps),25,51,12,Muted);
        const float LandingW=900,LandingX=(W-LandingW)*.5f;Frame(LandingX,14,LandingW,62);
        Label(RegionMap?TEXT("CHOOSE YOUR HOME SECTOR"):TEXT("CHOOSE YOUR COMMAND CORE LOCATION"),LandingX+20,25,18,Gold);
        Label(RegionMap?TEXT("Click the center sector to survey it in detail. Time is paused."):TEXT("The world is paused. Click valid ground to land and begin."),LandingX+20,50,14,Text);
        Button(TEXT("Main menu"),TEXT("main-menu"),W-176,18,152,49);
        Button(RegionMap?TEXT("Survey home sector"):TEXT("Regional map"),RegionMap?TEXT("focus-sector:4"):TEXT("region-map"),W/2-120,H-76,240,50);
        const float NoticeHeight=DrawNotice(*G,W,H);
        FString Why;if(!RegionMap&&G->CursorOnWorld&&!G->CanLand(G->CursorWorld,Why)){float TY=102+NoticeHeight;Wrapped(Why,W/2-300,TY,600,15,Gold);}return;
    }
    if(G->Ready)
    {
        if(!NeighborhoodOverview&&!G->SelectedBuild.IsEmpty()&&G->CursorOnWorld&&!WasOverUi)if(const auto* D=G->Sim.BuildingDefs.Find(G->SelectedBuild)){FString Why;Circle(G->CursorWorld,D->Footprint,G->Sim.CanPlaceBuilding(G->SelectedBuild,G->CursorWorld,Why)?Green:Red);}
        if(!RegionMap&&Readable)if(const auto* B=Local.FindBuilding(G->SelectedId))if(const auto* D=Local.Definition(*B)){Circle(B->Position+SectorOffset,D->Footprint+20,Green);if(D->AttackRange>0)Circle(B->Position+SectorOffset,D->AttackRange,Gold);if(D->SensorRange>0)Circle(B->Position+SectorOffset,D->SensorRange,FLinearColor(.35f,.68f,.85f));}
        if(!RegionMap&&Readable)for(const auto& B:Local.Buildings)
        {
            const auto* D=Local.Definition(B);if(!D||B.Health<=0||B.LastShotTime<0||Local.Time-B.LastShotTime>.12)continue;
            if(!G->Observer&&!Local.IsVisible(B.LastShotPosition))continue;
            WorldLine(Ground(B.Position+SectorOffset,D->Footprint*.7f),Ground(B.LastShotPosition+SectorOffset,50),Green,2);
        }
        if(!NeighborhoodOverview)DrawDeposits(false,{});
    }
    // Floating information strip leaves the landscape visible around every edge.
    Label(TEXT("seige2222"),24,21,21,Text);Label(FString::Printf(TEXT("v%s  /  %.0f FPS"),*Version,SmoothedFps),25,51,12,Muted);
    const float StripW=900,StripX=(W-StripW)*.5f,StripY=14,StripH=52;
    Frame(StripX,StripY,StripW,StripH);
    auto Summary=[&](const FString& Id,const FString& Heading,const FString& Value,float Offset,float Width,FLinearColor C)
    {
        const float X=StripX+Offset*(900.f/1040.f);Width*=900.f/1040.f;
        Label(Heading,X+12,StripY+8,9,Muted);Label(Value,X+12,StripY+27,14,C);
        Region(TEXT("summary:")+Id,X,StripY,Width,StripH);
        if(Offset>0)Box(X,StripY+12,1,28,FLinearColor(.23f,.28f,.28f,.7f));
    };
    FString MajorStock;for(const auto& R:Ui.SummaryResources){if(!MajorStock.IsEmpty())MajorStock+=TEXT("   ");MajorStock+=FString::Printf(TEXT("%s %.0f"),*R.Label,Local.TotalStock(R.Resource));}
    Summary(TEXT("resources"),TEXT("MATERIAL STOCKPILES"),Readable?MajorStock:TEXT("No colony data"),0,300,Text);
    Summary(TEXT("workforce"),TEXT("ROBOT WORKFORCE"),Readable?FString::Printf(TEXT("%d / %d jobs"),Local.Population,Local.TotalJobs):TEXT("Unavailable"),300,190,Local.TotalJobs>Local.Employed?Gold:Text);
    Summary(TEXT("logistics"),TEXT("IN TRANSIT"),Readable?FString::Printf(TEXT("%d couriers"),Local.Couriers.Num()):TEXT("Unavailable"),490,150,Text);
    Summary(TEXT("threats"),TEXT("NEXT ALIEN PULSE"),Readable?FString::Printf(TEXT("%.0f seconds"),FMath::Max(0.,Local.NextWaveTime-Local.Time)):TEXT("Unavailable"),640,210,Gold);
    Summary(TEXT("objective"),G->Observer||G->DetailedSectorIndex()!=4?TEXT("OBSERVATION"):TEXT("FIRST LANDING"),G->Observer||G->DetailedSectorIndex()!=4?TEXT("Read only"):G->Sim.Won?TEXT("Complete"):TEXT("Objectives"),850,190,Green);
    const float DockW=570,DockX=(W-DockW)*.5f,DockY=H-75;
    Frame(DockX,DockY,DockW,60);
    auto DockButton=[&](const FString& Name,const FString& A,const FString& Visual,float X,float Width,bool Active)
    {
        Button(TEXT(""),A,X,DockY+6,Width,48,Active);Icon(Visual,X+8,DockY+17,23,Active?Gold:Text);Label(Name,X+38,DockY+23,13,Active?Gold:Text);
    };
    DockButton(G->Observer||G->DetailedSectorIndex()!=4?TEXT("Observe"):RegionMap?TEXT("Map"):TEXT("Build [B]"),G->Observer||G->DetailedSectorIndex()!=4||RegionMap?TEXT("observer"):TEXT("build-menu"),TEXT("factory"),DockX+6,108,Ui.BuildOpen);
    DockButton(TEXT("Colony"),TEXT("colony-menu"),TEXT("depot"),DockX+118,98,Ui.ColonyOpen);
    DockButton(TEXT("Regions"),TEXT("region-map"),TEXT("sensor"),DockX+220,105,RegionMap);
    Box(DockX+333,DockY+13,1,34,FLinearColor(.23f,.28f,.28f,.7f));
    Label(FString::Printf(TEXT("%02d:%02d"),int32(G->Sim.Time)/60,int32(G->Sim.Time)%60),DockX+347,DockY+13,19,Text);Label(TEXT("LOCAL TIME"),DockX+348,DockY+39,8,Muted);
    Button(G->Paused?TEXT("Resume"):TEXT("Pause"),TEXT("pause"),DockX+430,DockY+6,76,48,G->Paused);
    Button(FString::Printf(TEXT("%.0fx"),G->Speed),TEXT("speed"),DockX+511,DockY+6,53,48,G->Speed>1);
    if(!Readable)Ui.HoverPanel.Empty();
    if(Readable&&PreviousHover.StartsWith(TEXT("summary:"))&&!Ui.BuildOpen&&!Ui.ColonyOpen)Ui.HoverPanel=PreviousHover.RightChop(8);else if(!PreviousHover.StartsWith(TEXT("hover-panel:")))Ui.HoverPanel.Empty();
    if(!G->Ready)
    {
        Ui.HitRegions.Reset();Frame(W/2-340,H/2-160,680,320);Label(TEXT("RULE FILE ERROR"),W/2-310,H/2-130,26,Red);float Y=H/2-78;Wrapped(G->Error,W/2-310,Y,610,16,Text);Button(TEXT("Reload corrected rules"),TEXT("reset"),W/2-310,H/2+88,610,42);return;
    }
    if(Ui.BuildOpen)
    {
        Ui.HoverPanel.Empty();const float BW=980,BH=294,X=(W-BW)*.5f,Y=DockY-BH-12;
        Frame(X,Y,BW,BH);Label(TEXT("CONSTRUCTION"),X+20,Y+18,17,Gold);Button(TEXT("Close"),TEXT("close"),X+BW-87,Y+9,69,31);
        if(!InterfaceLoaded){float EY=Y+78;Wrapped(InterfaceError,X+20,EY,BW-40,16,Red);}
        else
        {
            if(PreviousHover.StartsWith(TEXT("group:")))Ui.Category=PreviousHover.RightChop(6);
            const float TabW=(BW-40)/FMath::Max(1,Ui.Categories.Num());int32 GI=0;
            for(const auto& Group:Ui.Categories){Button(Group.Shortcut+TEXT("  ")+Group.Name,TEXT("group:")+Group.Id,X+20+GI++*TabW,Y+51,TabW-6,39,Ui.Category==Group.Id,Group.Description);}
            if(const auto* Group=Ui.Categories.FindByPredicate([this](const FSeigeMenuGroup& I){return I.Id==Ui.Category;}))
            {
                int32 EI=0;const float CardW=178,Gap=10;
                for(const auto& Entry:Group->Entries)if(const auto* D=G->Sim.BuildingDefs.Find(Entry.Definition))
                {
                    const float EX=X+20+EI++*(CardW+Gap),EY=Y+104;
                    Button(TEXT(""),TEXT("build:")+D->Id,EX,EY,CardW,133,false,D->Description);
                    Icon(D->Visual,EX+14,EY+13,44,D->Color);Label(Entry.Shortcut,EX+CardW-28,EY+17,15,Gold);
                    float NY=EY+72;Wrapped(D->Name,EX+13,NY,CardW-26,14,Text);Label(FString::Printf(TEXT("%d jobs"),D->Jobs),EX+13,EY+113,11,Muted);
                }
                if(PreviousHover.StartsWith(TEXT("build:")))if(const auto* D=G->Sim.BuildingDefs.Find(PreviousHover.RightChop(6)))Description(*D,*G,FMath::Min(X+BW-410,W-430),FMath::Max(90.f,Y-372),410);
            }
            Label(Ui.GroupFocused?TEXT("Choose a blueprint key. Esc returns to categories."):TEXT("R / I / L / D selects a category. Hover a blueprint for its requirements."),X+20,Y+259,13,Muted);
        }
    }
    else if(Ui.ColonyOpen)
    {
        Ui.HoverPanel.Empty();const float X=DockX+145,Y=DockY-515;Frame(X,Y,330,503);Label(TEXT("COLONY COMMAND"),X+20,Y+19,17,Gold);
        Button(TEXT("Save colony                  F5"),TEXT("save"),X+12,Y+58,306,43);Button(TEXT("Load colony                  F9"),TEXT("load"),X+12,Y+107,306,43);
        if(!G->Observer)Button(TEXT("Launch escape shuttle"),TEXT("escape"),X+12,Y+166,306,43);
        Button(TEXT("Settings"),TEXT("screen:settings"),X+12,Y+215,306,43);Button(TEXT("Credits"),TEXT("screen:credits"),X+12,Y+264,306,43);
        Button(TEXT("Return to main menu"),TEXT("main-menu"),X+12,Y+313,306,43);Button(TEXT("Exit game"),TEXT("exit"),X+12,Y+362,306,43);
        float TY=Y+430;Wrapped(TEXT("WASD: pan / Wheel: zoom / Q,E: orbit / Middle drag: orbit + tilt / Home: colony"),X+18,TY,294,13,Muted);
    }
    else if(!Ui.HoverPanel.IsEmpty())
    {
        float Offset=0;if(Ui.HoverPanel==TEXT("workforce"))Offset=300;else if(Ui.HoverPanel==TEXT("logistics"))Offset=490;else if(Ui.HoverPanel==TEXT("threats"))Offset=640;else if(Ui.HoverPanel==TEXT("objective"))Offset=850;const float X=FMath::Min(StripX+Offset*(900.f/1040.f),W-445);
        const float Y=90,PW=425,PH=Ui.HoverPanel==TEXT("resources")?356:242;Frame(X,Y,PW,PH);Region(TEXT("hover-panel:")+Ui.HoverPanel,X,Y-13,PW,PH+13);float TY=Y+21;
        if(Ui.HoverPanel==TEXT("resources"))
        {
            Label(TEXT("COLONY MATERIALS"),X+18,TY,18,Gold);TY+=39;Label(TEXT("Includes physical cargo in transit"),X+18,TY,12,Muted);TY+=28;TArray<FString> Keys;Local.Resources.GetKeys(Keys);Keys.Sort();
            for(const FString& Id:Keys){const auto& R=Local.Resources[Id];Box(X+19,TY+4,7,9,R.Color);Label(R.Name,X+37,TY,15,Text);Label(FString::Printf(TEXT("%.1f"),Local.TotalStock(Id)),X+340,TY,15,Text);TY+=26;}
        }
        else if(Ui.HoverPanel==TEXT("workforce")){Label(TEXT("ROBOT WORKFORCE"),X+18,TY,18,Gold);TY+=43;Wrapped(Local.WorkforceStatus(),X+18,TY,PW-36,16,Text);TY+=12;Wrapped(TEXT("Robots fill jobs automatically. The core supports your first workers; charging and maintenance hubs [B L C] support expansion. Keep components supplied for efficient operation."),X+18,TY,PW-36,14,Muted);}
        else if(Ui.HoverPanel==TEXT("logistics")){Label(TEXT("PHYSICAL LOGISTICS"),X+18,TY,18,Gold);TY+=43;Wrapped(FString::Printf(TEXT("%d couriers moving / %.0f units delivered / %d couriers lost"),Local.Couriers.Num(),Local.DeliveredUnits,Local.LostCouriers),X+18,TY,PW-36,16,Text);TY+=12;Wrapped(TEXT("Factories consume locally delivered stock. Construction reserves core materials, then couriers carry them to the site. Robots assemble buildings once supplies arrive."),X+18,TY,PW-36,14,Muted);}
        else if(Ui.HoverPanel==TEXT("threats")){Label(TEXT("SECTOR PRESSURE"),X+18,TY,18,Gold);TY+=43;Wrapped(FString::Printf(TEXT("Pulse %d / Next pulse in %.0f seconds"),Local.Wave,FMath::Max(0.,Local.NextWaveTime-Local.Time)),X+18,TY,PW-36,16,Text);TY+=12;Wrapped(TEXT("Roaming bugs can arrive at any time. Sensors reveal live contacts; defenses require staffing. Repairs consume local materials."),X+18,TY,PW-36,14,Muted);}
        else{Label(TEXT("FIRST LANDING OBJECTIVES"),X+18,TY,18,Gold);TY+=43;TArray<FString> Goals;Local.ObjectiveText().ParseIntoArray(Goals,TEXT(" | "),true);for(const FString& Goal:Goals){Wrapped(Goal,X+18,TY,PW-36,16,Text);TY+=7;}}
    }
    if(!Ui.BuildOpen&&!Ui.ColonyOpen&&Ui.HoverPanel.IsEmpty())
    {
        if(!RegionMap&&Readable&&(Local.FindBuilding(G->SelectedId)||!G->SelectedBuild.IsEmpty()))DrawBuildingInfo(*G,W,H);
        if(!NeighborhoodOverview&&!G->SelectedBuild.IsEmpty())if(const auto* D=G->Sim.BuildingDefs.Find(G->SelectedBuild))
        {
            const float PW=410,PH=136,X=(W-PW)*.5f,Y=DockY-PH-12;Frame(X,Y,PW,PH);Icon(D->Visual,X+16,Y+17,38,D->Color);Label(D->Name,X+69,Y+20,19,Gold);
            float TY=Y+72;FString Why;const bool Valid=G->CursorOnWorld&&G->Sim.CanPlaceBuilding(D->Id,G->CursorWorld,Why);Wrapped(Valid?TEXT("Click terrain to place. Esc cancels."):Why,X+18,TY,PW-36,14,Valid?Green:Muted);
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
