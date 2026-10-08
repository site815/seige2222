#include "SeigeGameMode.h"

namespace
{
const FLinearColor Text(.91f,.95f,.96f),Muted(.58f,.71f,.76f),Gold(.91f,.72f,.39f),Mint(.35f,.85f,.78f),Red(.91f,.48f,.39f);
bool CanManage(const ASeigeGameMode& G)
{return G.Ready&&!G.Observer&&!G.CompanionView&&G.Screen==TEXT("playing")&&!G.MenuOpen&&!G.IsRegionMap()&&G.DetailedSectorIndex()==4&&!G.Sim.Failed&&!G.Sim.Escaped&&(!G.Sim.Won||G.WinAcknowledged)&&G.SelectedBuild.IsEmpty()&&!G.WallPlacementActive&&!G.IsRoadToolActive()&&!G.FleetOrderActive;}
FString RecipeLabel(const FSeigeSimulation& Sim,const FString& Id)
{
    const auto* R=Sim.Recipes.Find(Id);if(!R)return TEXT("Idle");
    if(R->WorkerOutput)return TEXT("Workers / automatic demand");
    TArray<FString> Names;for(const auto& P:R->Outputs)if(const auto* D=Sim.Resources.Find(P.Key))Names.Add(D->Name);Names.Sort();return FString::Join(Names,TEXT(" + "));
}
}

bool ASeigeHUD::HandleCommandAction(const FString& A,ASeigeGameMode& G)
{
    if(!A.StartsWith(TEXT("command:")))return false;
    if(!CanManage(G)||CombatPanelOpen||Ui.BuildOpen)return true;
    FString Error;auto* B=G.Sim.FindBuilding(G.SelectedId);
    if(A==TEXT("command:reserve-less")||A==TEXT("command:reserve-more"))
    {G.Sim.SetWorkerSurplusTarget(FMath::Clamp(G.Sim.WorkerSurplusTarget+(A.EndsWith(TEXT("more"))?1:-1),0,100000),Error);G.Notice=Error.IsEmpty()?TEXT("Colony spare-worker target updated."):Error;}
    else if(A==TEXT("command:disassemble"))
    {
        int32 Queued=0;
        // Queue only whole, unreserved bodies already inside real storage.
        for(const auto& Store:G.Sim.Buildings)
            while(G.Sim.CanDisassembleWorkers(Store.Id,1,Error))
            {if(!G.Sim.DisassembleWorkers(Store.Id,1,Error))break;++Queued;}
        G.Notice=Queued?FString::Printf(TEXT("Recycling %d surplus workers for parts; %.2f kWh each. Reserve targets are protected."),Queued,G.Sim.DisassemblyEnergyKWh()):TEXT("No stored workers above job demand and reserve targets are available.");
    }
    else if(B&&B->Health>0)
    {
        if(A==TEXT("command:upgrade"))G.Notice=G.Sim.UpgradeBuilding(B->Id,Error)?TEXT("Upgrade committed. Materials and workers will build the next level."):Error;
        else if(A==TEXT("command:recipe-prev")||A==TEXT("command:recipe-next"))
        {
            const auto Options=G.Sim.ProductionOptions(B->Id);if(Options.Num())
            {const auto* D=G.Sim.Definition(*B);const auto Current=B->SelectedRecipe.IsEmpty()?D->Recipe:B->SelectedRecipe;int32 I=Options.IndexOfByKey(Current);if(I==INDEX_NONE)I=0;I=(I+(A.EndsWith(TEXT("next"))?1:Options.Num()-1))%Options.Num();G.Notice=G.Sim.SetProductionRecipe(B->Id,Options[I],Error)?TEXT("Production selection updated; a paid batch finishes before switching."):Error;}
        }
    }
    return true;
}

void ASeigeHUD::DrawWorkforceControls(ASeigeGameMode& G,float X,float Y,float W)
{
    Label(FString::Printf(TEXT("Stored: %d  /  colony spare target: %d"),G.Sim.InactiveWorkerCount(),G.Sim.WorkerSurplusTarget),X,Y,14,Text);
    Label(FString::Printf(TEXT("Total target including trading ports: %d"),G.Sim.WorkerReserveTarget()),X,Y+24,12,Muted);
    if(!CanManage(G))return;
    Button(TEXT("-"),TEXT("command:reserve-less"),X,Y+48,38,34);
    Button(TEXT("+"),TEXT("command:reserve-more"),X+44,Y+48,38,34);
    Label(TEXT("Spare production target"),X+94,Y+57,13,Mint);
    Button(TEXT("Disassemble surplus for parts"),TEXT("command:disassemble"),X,Y+93,W,37,false,FString::Printf(TEXT("Recycles stored bodies above all reserve targets. Each worker requires %.2f kWh; occupied jobs are protected."),G.Sim.DisassemblyEnergyKWh()));
}

void ASeigeHUD::DrawCommandInfo(ASeigeGameMode& G,float W,float H)
{
    if(!CanManage(G)||CombatPanelOpen||Ui.BuildOpen||!Ui.HoverPanel.IsEmpty()||G.WallPlacementActive||G.IsRoadToolActive()||!G.SelectedBuild.IsEmpty())return;
    const auto* B=G.Sim.FindBuilding(G.SelectedId);const auto* D=B?G.Sim.Definition(*B):nullptr;
    if(!B||B->Health<=0||!D||D->Role==TEXT("trade"))return;
    const auto Options=G.Sim.ProductionOptions(B->Id);const bool Core=D->Role==TEXT("core");
    const bool CombatCapable=G.Sim.Combat.Factories.Contains(B->DefId)||G.Sim.Combat.BuildingPlatforms.Contains(B->DefId);
    if(Options.IsEmpty()&&D->NextUpgrade.IsEmpty()&&!Core&&!CombatCapable)return;
    const float X=24,Y=Ui.ContentTop,PW=410,PH=FMath::Min(Core?640.f:440.f,H-Y-96.f);
    if(PH<360)return;
    const float FooterTop=Y+PH-68,WorkforceTop=FooterTop-145,TextBottom=Core?WorkforceTop-8:FooterTop-8;
    Frame(X,Y,PW,PH);Label(Core?TEXT("COMMAND & PRODUCTION"):TEXT("BUILDING OPERATIONS"),X+18,Y+18,17,Gold);float TY=Y+56;
    auto BoundedText=[&](const FString& Value,float Left,float Width,float Size,FLinearColor Color,int32 MaximumLines)
    {
        auto Lines=WrapLines(Value,Width,Size);const float LineHeight=Size+7;
        const int32 Limit=FMath::Max(0,FMath::Min(MaximumLines,FMath::FloorToInt((TextBottom-TY)/LineHeight)));
        if(Lines.Num()>Limit){Lines.SetNum(Limit);if(Limit)Lines.Last()=Lines.Last().LeftChop(FMath::Min(3,Lines.Last().Len()))+TEXT("...");}
        for(const auto& Line:Lines){Label(Line,Left,TY,Size,Color);TY+=LineHeight;}
    };
    if(CombatCapable)
    {Button(TEXT("Fleets / chassis / hardpoints"),TEXT("combat:open"),X+18,TY,PW-36,34);TY+=45;}
    if(!Options.IsEmpty())
    {
        const FString Selected=B->SelectedRecipe.IsEmpty()?D->Recipe:B->SelectedRecipe;
        Label(TEXT("SELECTED PRODUCT"),X+18,TY,11,Muted);TY+=24;
        Button(TEXT("<"),TEXT("command:recipe-prev"),X+18,TY,34,35);Button(TEXT(">"),TEXT("command:recipe-next"),X+PW-52,TY,34,35);
        const float RowStart=TY;TY+=2;BoundedText(RecipeLabel(G.Sim,Selected),X+64,PW-128,14,Text,2);TY=FMath::Max(RowStart+44,TY+8);
        if(const auto* R=G.Sim.Recipes.Find(Selected))
        {
            TArray<FString> Inputs;for(const auto& P:G.Sim.ProductionInputs(*B,Selected)){const auto& Res=G.Sim.Resources[P.Key];Inputs.Add(FString::Printf(TEXT("%.1f %s %s"),P.Value,*Res.Unit,*Res.Name));}Inputs.Sort();
            BoundedText(FString::Join(Inputs,TEXT(" + ")),X+18,PW-36,12,Muted,2);
            BoundedText(FString::Printf(TEXT("%.2f kWh / batch  ·  %.1f min assembly"),G.Sim.ProductionEnergy(*B,Selected),G.Sim.ProductionSeconds(*B,Selected)/60.),X+18,PW-36,13,Mint,1);
        }
        const FString Active=G.Sim.ActiveProductionRecipe(*B);
        BoundedText(FString::Printf(TEXT("Now: %s  /  %.0f%%"),*RecipeLabel(G.Sim,Active),B->Progress*100),X+18,PW-36,13,Text,2);
        if(Core)BoundedText(TEXT("Vacancies take priority until worker-factory supply."),X+18,PW-36,11,Muted,1);
        TY+=12;
    }
    if(Core)DrawWorkforceControls(G,X+18,WorkforceTop,PW-36);
    if(!D->NextUpgrade.IsEmpty())
    {
        FString Reason;const bool Can=G.Sim.CanUpgradeBuilding(B->Id,Reason);const auto* Next=G.Sim.BuildingDefs.Find(D->NextUpgrade);
        Button(Next?TEXT("Upgrade: ")+Next->Name:TEXT("Upgrade building"),TEXT("command:upgrade"),X+18,Y+PH-54,PW-36,38,false,Can?TEXT("The reserved plot is retained. Couriers deliver materials before assembly."):Reason);
    }
    else Label(TEXT("Maximum level reached"),X+18,Y+PH-43,13,Muted);
}

void ASeigeHUD::DrawWallPlan(ASeigeGameMode& G,float W,float H)
{
    if(!G.WallPlacementActive||G.Screen!=TEXT("playing")||G.IsRegionMap())return;
    const float PW=520,X=(W-PW)*.5f,Y=H-290;Frame(X,Y,PW,195);
    FSeigeWallPlan Plan;FString Reason;const bool Valid=G.Sim.Walls.Plan(G.Sim,G.WallJoints,G.WallInsideLeft,Plan,Reason);
    Label(TEXT("WALL PLAN"),X+18,Y+15,17,Gold);
    Label(FString::Printf(TEXT("%d joints / %.1f m / %d sections / inside %s"),G.WallJoints.Num(),Plan.LengthMeters,Plan.Segments.Num(),G.WallInsideLeft?TEXT("left"):TEXT("right")),X+18,Y+45,13,Text);
    float TY=Y+70;TArray<FString> Costs;for(const auto& C:Plan.Cost)Costs.Add(FString::Printf(TEXT("%.0f %s"),C.Value,*G.Sim.Resources[C.Key].Name));Costs.Sort();
    Wrapped(Valid?FString::Join(Costs,TEXT(" / ")):Reason,X+18,TY,PW-36,12,Valid?Mint:Red);
    Label(TEXT("Click: add / select / move joints. Click an edge: insert."),X+18,Y+117,12,Muted);
    Label(TEXT("E: flip inside  /  Delete: remove joint  /  Esc: cancel"),X+18,Y+139,12,Muted);
    Label(TEXT("Enter: commit material delivery and construction"),X+18,Y+165,13,Gold);
}
