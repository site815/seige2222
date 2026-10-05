#include "SeigeGameMode.h"

namespace
{
const FSeigeBuilding* SelectedTradePort(const ASeigeGameMode& G)
{
    if(!G.Ready||G.Observer||G.CompanionView||G.Screen!=TEXT("playing")||G.MenuOpen||G.IsRegionMap()||G.DetailedSectorIndex()!=4||!G.SelectedBuild.IsEmpty()||G.IsRoadToolActive()||G.WallPlacementActive||G.FleetOrderActive||G.Sim.Failed||G.Sim.Escaped||(G.Sim.Won&&!G.WinAcknowledged))return nullptr;
    const auto* B=G.Sim.FindBuilding(G.SelectedId);const auto* D=B?G.Sim.Definition(*B):nullptr;
    return B&&B->Health>0&&D&&D->Role==TEXT("trade")?B:nullptr;
}
TArray<FString> TradeResources(const FSeigeSimulation& Sim)
{
    TArray<FString> Ids;Sim.Trade.Prices.GetKeys(Ids);
    Ids.Sort([&](const FString&A,const FString&B){const auto& X=Sim.Resources[A];const auto& Y=Sim.Resources[B];if(X.Tier!=Y.Tier)return X.Tier<Y.Tier;return X.Name<Y.Name;});return Ids;
}
}

bool ASeigeHUD::HandleTradeAction(const FString& Action,ASeigeGameMode& G)
{
    if(!Action.StartsWith(TEXT("trade:")))return false;
    if(CombatPanelOpen||Ui.BuildOpen)return true;
    const auto* Port=SelectedTradePort(G);if(!Port){G.Notice=TEXT("Select your own trading port to manage external trade.");return true;}
    const auto Ids=TradeResources(G.Sim);if(Ids.IsEmpty())return true;
    int32 Index=Ids.IndexOfByKey(TradeResourceSelection);if(Index==INDEX_NONE)Index=0;
    if(Action==TEXT("trade:previous"))Index=(Index+Ids.Num()-1)%Ids.Num();
    else if(Action==TEXT("trade:next"))Index=(Index+1)%Ids.Num();
    TradeResourceSelection=Ids[Index];
    const double Step=G.Sim.Resources[TradeResourceSelection].Discrete?1.:10.;
    if(Action==TEXT("trade:less"))TradeQuantity=FMath::Max(1.,TradeQuantity-Step);
    else if(Action==TEXT("trade:more"))TradeQuantity=FMath::Min(10000.,TradeQuantity+Step);
    else if(Action==TEXT("trade:reserve-less")||Action==TEXT("trade:reserve-more"))
    {FString Error;G.Notice=G.Sim.SetPortWorkerTarget(Port->Id,FMath::Clamp(Port->WorkerExportTarget+(Action.EndsWith(TEXT("more"))?1:-1),0,100000),Error)?TEXT("Trading-port worker stock target updated."):Error;}
    else if(Action==TEXT("trade:import")||Action==TEXT("trade:export"))
    {
        FString Error;const bool Buy=Action==TEXT("trade:import");
        G.Notice=G.Sim.TryTrade(Port->Id,TradeResourceSelection,TradeQuantity,Buy,Error)?
            (Buy?TEXT("Import ordered. Credits are reserved; cargo arrives through this port."):TEXT("Export ordered. Goods must reach this port before departure; credits arrive after shipment.")):Error;
    }
    else if(Action==TEXT("trade:upgrade"))
    {FString Error;G.Notice=G.Sim.UpgradeBuilding(Port->Id,Error)?TEXT("Trading-port upgrade queued. Workers and materials will arrive at the site."):Error;}
    return true;
}

void ASeigeHUD::DrawTradeInfo(ASeigeGameMode& G,float W,float H)
{
    const auto* Port=SelectedTradePort(G);if(!Port||CombatPanelOpen||Ui.BuildOpen||!Ui.HoverPanel.IsEmpty())return;
    const auto Ids=TradeResources(G.Sim);if(Ids.IsEmpty())return;
    if(!Ids.Contains(TradeResourceSelection))TradeResourceSelection=Ids[0];
    const auto& Resource=G.Sim.Resources[TradeResourceSelection];const auto* Definition=G.Sim.Definition(*Port);
    const auto* PortDefinition=G.Sim.Trade.Definition(Port->DefId);
    const FLinearColor Text(.89f,.92f,.9f),Muted(.56f,.65f,.64f),Gold(.89f,.75f,.43f),Mint(.46f,.84f,.68f),Red(.91f,.48f,.39f);
    const float X=24,Y=104,PW=410,PH=FMath::Min(590.f,H-Y-96.f);if(PH<500)return;
    Frame(X,Y,PW,PH);Region(TEXT("trade:panel"),X,Y,PW,PH);
    Label(TEXT("EXTERNAL TRADE"),X+18,Y+16,18,Gold);
    Label(FString::Printf(TEXT("Level %d  /  %.0f kg shipment capacity"),PortDefinition?PortDefinition->Level:1,PortDefinition?PortDefinition->CapacityKg:0),X+18,Y+43,13,Muted);
    Label(FString::Printf(TEXT("%.6f Galactic credits"),G.Sim.Credits),X+18,Y+69,17,Text);
    Label(TEXT("Price anchor: 1 credit = value of 1 kg gold"),X+18,Y+94,12,Muted);
    Button(TEXT("<"),TEXT("trade:previous"),X+18,Y+124,36,36);
    Button(TEXT(">"),TEXT("trade:next"),X+PW-54,Y+124,36,36);
    float NameY=Y+125;Wrapped(Resource.Name,X+66,NameY,PW-132,16,Resource.Color);
    Button(Resource.Discrete?TEXT("-1"):TEXT("-10"),TEXT("trade:less"),X+18,Y+170,60,34);
    Button(Resource.Discrete?TEXT("+1"):TEXT("+10"),TEXT("trade:more"),X+PW-78,Y+170,60,34);
    Label(FString::Printf(TEXT("%.0f %s"),TradeQuantity,*Resource.Unit),X+PW*.38f,Y+177,16,Text);
    Label(FString::Printf(TEXT("%.1f kg  /  %.1f L storage"),TradeQuantity*Resource.UnitMassKg,TradeQuantity*Resource.LitresPerUnit),X+18,Y+216,12,Muted);
    FString BuyReason,SellReason;const bool Buy=G.Sim.CanTrade(Port->Id,TradeResourceSelection,TradeQuantity,true,BuyReason),Sell=G.Sim.CanTrade(Port->Id,TradeResourceSelection,TradeQuantity,false,SellReason);
    Button(FString::Printf(TEXT("Import  %.6f"),G.Sim.TradeQuote(TradeResourceSelection,TradeQuantity,true)),TEXT("trade:import"),X+18,Y+240,181,38,false,Buy?TEXT("Reserve credits and request a physical import shipment."):BuyReason);
    Button(FString::Printf(TEXT("Export  %.6f"),G.Sim.TradeQuote(TradeResourceSelection,TradeQuantity,false)),TEXT("trade:export"),X+211,Y+240,181,38,false,Sell?TEXT("Deliver local goods to this port, then receive credits after shipment."):SellReason);
    float StatusY=Y+291;const float StatusBottom=Y+PH-164;
    auto StatusText=[&](const FString& Value,float Size,FLinearColor Color)
    {
        auto Lines=WrapLines(Value,PW-36,Size);const float LineHeight=Size+7;
        const int32 Limit=FMath::Max(0,FMath::FloorToInt((StatusBottom-StatusY)/LineHeight));
        if(Lines.Num()>Limit){Lines.SetNum(Limit);if(Limit)Lines.Last()=Lines.Last().LeftChop(FMath::Min(3,Lines.Last().Len()))+TEXT("...");}
        for(const auto& Line:Lines){Label(Line,X+18,StatusY,Size,Color);StatusY+=LineHeight;}
    };
    if(!Port->Shipment.Resource.IsEmpty())
    {
        const auto& Shipment=Port->Shipment;const auto& R=G.Sim.Resources[Shipment.Resource];
        StatusText(FString::Printf(TEXT("%s %.1f %s %s · %.0f%%"),Shipment.Buy?TEXT("Import"):TEXT("Export"),Shipment.Quantity,*R.Unit,*R.Name,Shipment.Progress*100),14,Mint);
        StatusText(Port->Status,12,Muted);
    }
    else
    {
        StatusText(!Buy?BuyReason:!Sell?SellReason:TEXT("Choose import or export. Credits are used only for external trade."),13,!Buy&&!Sell?Red:Muted);
    }
    Label(FString::Printf(TEXT("Worker export stock target: %d"),Port->WorkerExportTarget),X+18,Y+PH-151,14,Text);
    Button(TEXT("-"),TEXT("trade:reserve-less"),X+18,Y+PH-124,38,34);
    Button(TEXT("+"),TEXT("trade:reserve-more"),X+62,Y+PH-124,38,34);
    Label(TEXT("Manufacture and deliver spare workers"),X+112,Y+PH-115,11,Muted);
    if(!Definition->NextUpgrade.IsEmpty())
    {
        FString Reason;const bool Upgrade=G.Sim.CanUpgradeBuilding(Port->Id,Reason);
        Button(TEXT("Upgrade trading port"),TEXT("trade:upgrade"),X+18,Y+PH-55,PW-36,37,false,Upgrade?TEXT("Build the next port level using its additional material cost."):Reason);
    }
    else Label(TEXT("Level 3 · Fully upgraded"),X+18,Y+PH-43,14,Mint);
}
