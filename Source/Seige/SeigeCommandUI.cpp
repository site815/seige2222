#include "SeigeGameMode.h"

namespace
{
const FLinearColor Text(.91f,.95f,.96f),Muted(.58f,.71f,.76f),Gold(.91f,.72f,.39f),Mint(.35f,.85f,.78f),Red(.91f,.48f,.39f);
bool CanManage(const ASeigeGameMode& G)
{return G.Ready&&!G.Observer&&!G.CompanionView&&G.Screen==TEXT("playing")&&!G.MenuOpen&&!G.IsRegionMap()&&G.DetailedSectorIndex()==4&&!G.Sim.Failed&&!G.Sim.Escaped&&(!G.Sim.Won||G.WinAcknowledged)&&G.SelectedBuild.IsEmpty()&&!G.WallPlacementActive&&!G.IsRoadToolActive()&&!G.FleetOrderActive;}
FString RecipeLabel(const FSeigeSimulation& Sim,const FString& Id)
{
    const auto* R=Sim.Recipes.Find(Id);if(!R)return TEXT("Idle");
    if(R->WorkerOutput)return TEXT("Workers / command-center assembly");
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
    else if(B&&B->Health>0)
    {
        if(A==TEXT("command:transport-build"))
            G.Notice=G.Sim.TransportVehicles.QueueVehicle(G.Sim,B->Id,Error)?TEXT("Transport fabrication ordered. Materials and energy are required before assembly."):Error;
        else if(A==TEXT("command:transport-prev")||A==TEXT("command:transport-next"))
        {
            TArray<int32> Ids;for(const auto& V:G.Sim.TransportVehicles.Bodies)if(V.Health>0&&!V.Evacuated)Ids.Add(V.Id);
            if(!Ids.IsEmpty()){int32 I=Ids.IndexOfByKey(TransportFocusId);if(I<0)I=0;TransportFocusId=Ids[(I+(A.EndsWith(TEXT("next"))?1:Ids.Num()-1))%Ids.Num()];}
        }
        else if(A==TEXT("command:transport-board")&&G.Sim.Definition(*B)->Role==TEXT("core"))
            G.Notice=G.Sim.TransportVehicles.BoardShuttle(G.Sim,TransportFocusId,Error)?TEXT("Transport shuttle order accepted."):Error;
        else if(A==TEXT("command:upgrade"))G.Notice=G.Sim.UpgradeBuilding(B->Id,Error)?TEXT("Upgrade committed. Materials and workers will build the next level."):Error;
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
    Label(FString::Printf(TEXT("Stored: %d  /  spare target: %d"),G.Sim.Workers.StoredCount(),G.Sim.WorkerSurplusTarget),X,Y,14,Text);
    Label(FString::Printf(TEXT("Total target including trading ports: %d"),G.Sim.WorkerReserveTarget()),X,Y+24,12,Muted);
    if(!CanManage(G))return;
    Button(TEXT("-"),TEXT("command:reserve-less"),X,Y+48,38,34);
    Button(TEXT("+"),TEXT("command:reserve-more"),X+44,Y+48,38,34);
    Label(TEXT("Spare production target"),X+94,Y+57,13,Mint);
}

void ASeigeHUD::DrawCommandInfo(ASeigeGameMode& G,float W,float H)
{
    if(!CanManage(G)||CombatPanelOpen||Ui.BuildOpen||!Ui.HoverPanel.IsEmpty()||G.WallPlacementActive||G.IsRoadToolActive()||!G.SelectedBuild.IsEmpty())return;
    const auto* B=G.Sim.FindBuilding(G.SelectedId);const auto* D=B?G.Sim.Definition(*B):nullptr;
    if(!B||B->Health<=0||!D||D->Role==TEXT("trade"))return;
    const auto Options=G.Sim.ProductionOptions(B->Id);const bool Core=D->Role==TEXT("core");
    const bool CombatCapable=G.Sim.Combat.Factories.Contains(B->DefId)||G.Sim.Combat.BuildingPlatforms.Contains(B->DefId);
    if(Options.IsEmpty()&&D->NextUpgrade.IsEmpty()&&!Core&&!CombatCapable)return;
    const float X=24,Y=Ui.ContentTop,PW=410,PH=FMath::Min(Core?700.f:480.f,H-Y-96.f);
    if(PH<360)return;
    const float FooterTop=Y+PH-68,WorkforceTop=FooterTop-105,TransportTop=WorkforceTop-145,TextBottom=Core?TransportTop-8:FooterTop-8;
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
    const auto& Hauler=G.Sim.TransportVehicles.Definition();
    if(Hauler.Factories.Contains(B->DefId))
    {
        FString Reason;const bool Can=G.Sim.TransportVehicles.CanQueueVehicle(G.Sim,B->Id,Reason);
        Button(TEXT("Build transport hauler"),TEXT("command:transport-build"),X+18,TY,PW-36,36,false,Can?TEXT("Manufacture one physical transport vehicle using the listed material bill."):Reason);TY+=47;
        BoundedText(FString::Printf(TEXT("%.2f kg / %.2f L cargo / %.2f kWh battery"),Hauler.CargoMassKg,Hauler.CargoLitres,Hauler.BatteryKWh),X+18,PW-36,12,Mint,2);
        TArray<FString> Costs;for(const auto& P:Hauler.BuildCost)Costs.Add(FString::Printf(TEXT("%.2f %s %s"),P.Value,*G.Sim.Resources[P.Key].Unit,*G.Sim.Resources[P.Key].Name));Costs.Sort();
        // Each component gets its own row, so the complete rules-defined bill
        // remains visible instead of being elided into a short tooltip.
        for(const auto& Cost:Costs)BoundedText(Cost,X+18,PW-36,12,Muted,1);
        const double FactoryRate=FMath::Max(.0001,Hauler.Factories.FindRef(B->DefId));
        BoundedText(FString::Printf(TEXT("%.2f kWh / %.2f min assembly"),Hauler.BuildEnergyKWh,Hauler.BuildSeconds/FactoryRate/60),X+18,PW-36,12,Muted,1);
        int32 Pending=0;double Progress=0;for(const auto& Job:G.Sim.TransportVehicles.Jobs)if(Job.FactoryId==B->Id){++Pending;Progress=FMath::Max(Progress,Job.ProgressSeconds);}
        if(Pending)BoundedText(FString::Printf(TEXT("%d ordered / current %.2f%%"),Pending,Hauler.BuildSeconds>0?Progress*100/Hauler.BuildSeconds:0),X+18,PW-36,12,Gold,1);
    }
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
        if(Core)BoundedText(TEXT("Only the command center manufactures new workers."),X+18,PW-36,11,Muted,1);
        TY+=12;
    }
    if(Core)
    {
        Label(TEXT("TRANSPORT / SHUTTLE BAY"),X+18,TransportTop,11,Muted);
        auto* V=G.Sim.TransportVehicles.Find(TransportFocusId);
        if(!V||V->Health<=0||V->Evacuated)
        {V=G.Sim.TransportVehicles.Bodies.FindByPredicate([](const auto& Unit){return Unit.Health>0&&!Unit.Evacuated;});TransportFocusId=V?V->Id:0;}
        if(V)
        {
            Button(TEXT("<"),TEXT("command:transport-prev"),X+18,TransportTop+23,32,31);
            Button(TEXT(">"),TEXT("command:transport-next"),X+PW-50,TransportTop+23,32,31);
            Label(FString::Printf(TEXT("Hauler #%d / %.2f kWh"),V->Id,V->BatteryKWh),X+62,TransportTop+32,12,Text);
            FString Status=V->Status;if(Status.Len()>48)Status=Status.Left(45)+TEXT("...");
            Label(Status,X+18,TransportTop+64,11,Muted);
            Button(V->Embarked?TEXT("Deploy from shuttle"):TEXT("Return to shuttle bay"),TEXT("command:transport-board"),X+18,TransportTop+88,PW-36,34,false,TEXT("Only an empty, available hauler can board. Combat and transport vehicles share shuttle capacity."));
        }
        else Label(TEXT("No owned transport vehicles"),X+18,TransportTop+39,13,Muted);
        DrawWorkforceControls(G,X+18,WorkforceTop,PW-36);
    }
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
    // Clear the two-row command dock (108 high, 14 bottom margin) by 12 px.
    const float PW=520,X=(W-PW)*.5f,Y=H-329;Frame(X,Y,PW,195);
    FSeigeWallPlan Plan;FString Reason;const bool Valid=G.Sim.Walls.Plan(G.Sim,G.WallJoints,G.WallInsideLeft,Plan,Reason);
    Label(TEXT("WALL PLAN"),X+18,Y+15,17,Gold);
    Label(FString::Printf(TEXT("%d joints / %.1f m / %d sections / inside %s"),G.WallJoints.Num(),Plan.LengthMeters,Plan.Segments.Num(),G.WallInsideLeft?TEXT("left"):TEXT("right")),X+18,Y+45,13,Text);
    float TY=Y+70;TArray<FString> Costs;for(const auto& C:Plan.Cost)Costs.Add(FString::Printf(TEXT("%.0f %s"),C.Value,*G.Sim.Resources[C.Key].Name));Costs.Sort();
    Wrapped(Valid?FString::Join(Costs,TEXT(" / ")):Reason,X+18,TY,PW-36,12,Valid?Mint:Red);
    Label(TEXT("Click: add / select / move joints. Click an edge: insert."),X+18,Y+117,12,Muted);
    Label(TEXT("E: flip inside  /  Delete: remove joint  /  Esc: cancel"),X+18,Y+139,12,Muted);
    Label(TEXT("Enter: commit material delivery and construction"),X+18,Y+165,13,Gold);
}
