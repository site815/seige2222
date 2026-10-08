#include "SeigeGameMode.h"
namespace
{
const FLinearColor Text(.91f,.95f,.96f),Muted(.58f,.71f,.76f),Gold(.91f,.72f,.39f),Mint(.35f,.85f,.78f);
bool Ready(const ASeigeGameMode& G){return G.Ready&&!G.Observer&&!G.CompanionView&&G.Screen==TEXT("playing")&&!G.MenuOpen&&!G.Sim.Escaped&&!G.Sim.Failed&&(!G.Sim.Won||G.WinAcknowledged)&&G.SelectedBuild.IsEmpty()&&!G.IsRoadToolActive()&&!G.WallPlacementActive;}
bool HomeBuildingView(const ASeigeGameMode& G){return G.DetailedSectorIndex()==4&&!G.IsRegionMap();}
TArray<FString> Hulls(const FSeigeSimulation& S,int32 FactoryId)
{TArray<FString> Ids;const auto* B=S.FindBuilding(FactoryId);const auto* F=B?S.Combat.Factories.Find(B->DefId):nullptr;for(const auto& P:S.Combat.Chassis)if(F&&(F->Family==TEXT("all")||F->Family==P.Value.Family)&&P.Value.Tier<=F->MaximumTier)Ids.Add(P.Key);Ids.Sort([&](const auto& A,const auto& B){const auto& X=S.Combat.Chassis[A];const auto& Y=S.Combat.Chassis[B];if(X.Family!=Y.Family)return X.Family<Y.Family;return X.CapacityPoints<Y.CapacityPoints;});return Ids;}
TArray<FString> WeaponIds(const FSeigeSimulation& S){TArray<FString> Ids;for(const auto& P:S.Combat.Weapons)if(!P.Key.StartsWith(TEXT("bug")))Ids.Add(P.Key);Ids.Sort();return Ids;}
void Cycle(FString& Value,const TArray<FString>& Values,int32 Direction)
{if(Values.IsEmpty()){Value.Empty();return;}int32 I=Values.IndexOfByKey(Value);if(I==INDEX_NONE)I=0;Value=Values[(I+Direction+Values.Num())%Values.Num()];}
}
bool ASeigeHUD::HandleCombatAction(const FString& A,ASeigeGameMode& G)
{
    if(!A.StartsWith(TEXT("combat:")))return false;if(!Ready(G)||Ui.BuildOpen)return true;auto& C=G.Sim.Combat;FString E;
    const bool BuildingAction=A==TEXT("combat:request")||A==TEXT("combat:queue")||A==TEXT("combat:refit-request")||A==TEXT("combat:hull-prev")||A==TEXT("combat:hull-next")||(A==TEXT("combat:refit")&&OutfitBuilding);
    if(BuildingAction&&!HomeBuildingView(G)){G.Notice=TEXT("View and select your own factory or building to change its equipment or production.");return true;}
    if(A!=TEXT("combat:open")&&A!=TEXT("combat:close")&&!CombatPanelOpen)return true;
    if(A==TEXT("combat:open")){CombatPanelOpen=true;CombatTab=TEXT("fleet");Ui.CloseMenus();}
    else if(A==TEXT("combat:close")){CombatPanelOpen=false;G.FleetOrderActive=false;}
    else if(A.StartsWith(TEXT("combat:tab:"))){CombatTab=A.RightChop(11);LoadoutContext.Empty();}
    else if(A==TEXT("combat:fleet-next")||A==TEXT("combat:fleet-prev"))
    {if(C.Fleets.Num()){int32 I=C.Fleets.IndexOfByPredicate([&](const auto& F){return F.Id==G.SelectedFleetId;});I=(FMath::Max(0,I)+(A.EndsWith(TEXT("next"))?1:C.Fleets.Num()-1))%C.Fleets.Num();G.SelectedFleetId=C.Fleets[I].Id;}}
    else if(A==TEXT("combat:create"))G.Notice=C.CreateFleet(G.Sim,FString::Printf(TEXT("Fleet %d"),C.Fleets.Num()+1),G.SelectedFleetId,E)?TEXT("Fleet created. Assign completed vehicles to it."):E;
    else if(A==TEXT("combat:board"))G.Notice=C.BoardFleet(G.Sim,G.SelectedFleetId,E)?TEXT("Fleet aboard the parked command shuttle, ready for evacuation."):E;
    else if(A==TEXT("combat:move")){G.FleetOrderActive=true;G.Notice=TEXT("Click terrain to move the selected fleet. Esc cancels. Individual units fight autonomously.");}
    else if(A==TEXT("combat:privateer"))
    {
        auto* Target=G.Neighbors.FindByPredicate([&](const auto& N){return N.Index==G.DetailedSectorIndex();});
        const FVector2D Destination=G.CursorOnWorld?G.CursorWorld-G.DetailedSectorOffset():FVector2D::ZeroVector;
        G.Notice=Target?C.OrderPrivateer(G.Sim,Target->Sim,G.SelectedFleetId,Target->Index,Destination,E)?TEXT("Privateer mission dispatched. Transit, ammunition, combat and stolen cargo are physical."):E:TEXT("View a populated neighboring sector to dispatch a privateer mission.");
    }
    else if(A==TEXT("combat:defense")||A==TEXT("combat:escort"))
    {const auto* B=HomeBuildingView(G)?G.Sim.FindBuilding(G.SelectedId):nullptr;const auto* Core=G.Sim.Buildings.FindByPredicate([&](const auto& X){const auto* D=G.Sim.Definition(X);return X.Health>0&&D&&D->Role==TEXT("core");});const FVector2D Destination=B?G.Sim.BuildingAccessPoint(*B):Core?G.Sim.BuildingAccessPoint(*Core):G.HomePosition();G.Notice=C.OrderFleet(G.Sim,G.SelectedFleetId,A.EndsWith(TEXT("escort"))?TEXT("escort"):TEXT("defense"),Destination,B?B->Id:0,E)?TEXT("Fleet mission updated."):E;}
    else if(A==TEXT("combat:aggression"))
    {if(auto* F=C.Fleets.FindByPredicate([&](const auto& F){return F.Id==G.SelectedFleetId;})){const FString Next=F->Aggression==TEXT("passive")?TEXT("defensive"):F->Aggression==TEXT("defensive")?TEXT("aggressive"):TEXT("passive");G.Notice=C.SetAggression(F->Id,Next,E)?TEXT("Fleet aggression: ")+Next:E;}}
    else if(A==TEXT("combat:vehicle-next")||A==TEXT("combat:vehicle-prev"))
    {TArray<int32> Ids;for(const auto& V:C.Vehicles)if(V.Health>0&&!V.Evacuated)Ids.Add(V.Id);if(Ids.Num()){int32 I=Ids.IndexOfByKey(OutfitVehicleId);I=(FMath::Max(I,0)+(A.EndsWith(TEXT("next"))?1:Ids.Num()-1))%Ids.Num();OutfitVehicleId=Ids[I];LoadoutContext.Empty();}}
    else if(A==TEXT("combat:assign"))G.Notice=C.AssignVehicle(G.Sim,OutfitVehicleId,G.SelectedFleetId,E)?TEXT("Vehicle assigned within fleet capacity."):E;
    else if(A==TEXT("combat:hull-prev")||A==TEXT("combat:hull-next"))
    {Cycle(ChosenChassis,Hulls(G.Sim,G.SelectedId),A.EndsWith(TEXT("next"))?1:-1);DraftWeapons.Empty();LoadoutContext.Empty();}
    else if(A==TEXT("combat:weapon-prev")||A==TEXT("combat:weapon-next"))Cycle(ChosenWeapon,WeaponIds(G.Sim),A.EndsWith(TEXT("next"))?1:-1);
    else if(A==TEXT("combat:add"))
    {if(C.Weapons.Contains(ChosenWeapon)&&DraftWeapons.Num()<64)DraftWeapons.Add(ChosenWeapon);}
    else if(A==TEXT("combat:remove"))
    {const int32 I=DraftWeapons.FindLast(ChosenWeapon);if(I!=INDEX_NONE)DraftWeapons.RemoveAt(I);else if(DraftWeapons.Num())DraftWeapons.Pop();}
    else if(A==TEXT("combat:clear"))DraftWeapons.Empty();
    else if(A==TEXT("combat:outfit-platform")){OutfitBuilding=!OutfitBuilding;LoadoutContext.Empty();}
    else if(A==TEXT("combat:request"))G.Notice=C.SetFabricationPlan(G.Sim,G.SelectedId,ChosenChassis,DraftWeapons,E)?TEXT("Factory supply plan set. Couriers will deliver the selected chassis and weapon bill."):E;
    else if(A==TEXT("combat:queue"))G.Notice=C.QueueVehicle(G.Sim,G.SelectedId,ChosenChassis,DraftWeapons,E)?TEXT("Vehicle assembly started with paid local materials and energy."):E;
    else if(A==TEXT("combat:refit-request"))G.Notice=C.SetRefitPlan(G.Sim,G.SelectedId,DraftWeapons,E)?TEXT("Refit supply plan set at the selected building. Couriers must deliver the parts before installation."):E;
    else if(A==TEXT("combat:refit"))G.Notice=(OutfitBuilding?C.RefitBuilding(G.Sim,G.SelectedId,DraftWeapons,E):C.Refit(G.Sim,OutfitVehicleId,DraftWeapons,E))?TEXT("Equipment installed using local parts and energy."):E;
    return true;
}

void ASeigeHUD::DrawCombatInfo(ASeigeGameMode& G,float W,float H)
{
    if(!Ready(G)||!CombatPanelOpen||Ui.BuildOpen||!Ui.HoverPanel.IsEmpty()||G.IsRegionMap())return;
    auto& C=G.Sim.Combat;const auto* Building=HomeBuildingView(G)?G.Sim.FindBuilding(G.SelectedId):nullptr;
    if(!C.Fleets.ContainsByPredicate([&](const auto& F){return F.Id==G.SelectedFleetId;})&&C.Fleets.Num())G.SelectedFleetId=C.Fleets[0].Id;
    const auto* Fleet=C.Fleets.FindByPredicate([&](const auto& F){return F.Id==G.SelectedFleetId;});
    if(!C.FindVehicle(OutfitVehicleId)||C.FindVehicle(OutfitVehicleId)->Health<=0||C.FindVehicle(OutfitVehicleId)->Evacuated){OutfitVehicleId=0;for(const auto& V:C.Vehicles)if(V.Health>0&&!V.Evacuated){OutfitVehicleId=V.Id;break;}}
    const auto* Vehicle=C.FindVehicle(OutfitVehicleId);
    const float X=24,Y=Ui.ContentTop,PW=450,PH=FMath::Min(640.f,H-Y-96.f);Frame(X,Y,PW,PH);
    Label(TEXT("FLEETS & EQUIPMENT"),X+18,Y+18,18,Gold);Button(TEXT("x"),TEXT("combat:close"),X+PW-44,Y+9,32,30);
    Button(TEXT("Fleets"),TEXT("combat:tab:fleet"),X+18,Y+52,132,34,CombatTab==TEXT("fleet"));
    Button(TEXT("Chassis"),TEXT("combat:tab:factory"),X+158,Y+52,132,34,CombatTab==TEXT("factory"));
    Button(TEXT("Hardpoints"),TEXT("combat:tab:outfit"),X+298,Y+52,134,34,CombatTab==TEXT("outfit"));
    float TY=Y+105;
    if(CombatTab==TEXT("fleet"))
    {
        Label(FString::Printf(TEXT("Command limit: %d / %d fleets"),C.Fleets.Num(),C.FleetLimit(G.Sim)),X+18,TY,14,Text);TY+=29;
        Button(TEXT("<"),TEXT("combat:fleet-prev"),X+18,TY,34,34);Button(TEXT(">"),TEXT("combat:fleet-next"),X+PW-52,TY,34,34);
        Label(Fleet?Fleet->Name:TEXT("No fleet"),X+66,TY+7,16,Gold);TY+=47;
        if(Fleet)
        {
            Label(FString::Printf(TEXT("%d / %d points  ·  %s"),C.FleetUsed(Fleet->Id),C.FleetCapacity,*Fleet->Mission),X+18,TY,14,Text);TY+=30;
            Button(TEXT("Defend"),TEXT("combat:defense"),X+18,TY,128,35);Button(TEXT("Move fleet"),TEXT("combat:move"),X+158,TY,132,35);Button(TEXT("Escort"),TEXT("combat:escort"),X+302,TY,130,35);TY+=44;
            Button(TEXT("Aggression: ")+Fleet->Aggression,TEXT("combat:aggression"),X+18,TY,PW-36,35,false,TEXT("Passive avoids combat, defensive fights nearby threats, aggressive pursues contacts."));TY+=44;
            Button(TEXT("Privateer neighbor"),TEXT("combat:privateer"),X+18,TY,201,34);Button(TEXT("Board shuttle"),TEXT("combat:board"),X+231,TY,201,34);TY+=43;
        }
        Button(TEXT("Create fleet"),TEXT("combat:create"),X+18,TY,PW-36,34);TY+=49;
        if(Vehicle)
        {
            Button(TEXT("<"),TEXT("combat:vehicle-prev"),X+18,TY,34,34);Button(TEXT(">"),TEXT("combat:vehicle-next"),X+PW-52,TY,34,34);Label(C.Chassis[Vehicle->ChassisId].Name,X+66,TY+8,13,Text);TY+=43;
            Wrapped(FString::Printf(TEXT("Unit %d / fleet %d / hull %.0f / shield %.0f / armor %.0f / %s"),Vehicle->Id,Vehicle->FleetId,Vehicle->Health,Vehicle->Shield,Vehicle->Armor,Vehicle->Evacuated?TEXT("Evacuated"):Vehicle->Embarked?TEXT("Aboard shuttle"):*Vehicle->Status),X+18,TY,PW-36,13,Muted);
            Button(TEXT("Assign vehicle to selected fleet"),TEXT("combat:assign"),X+18,TY+8,PW-36,35);
        }
        Label(TEXT("Small 1  /  Medium 2  /  Large 4  /  Behemoth 8 points"),X+18,Y+PH-31,12,Muted);return;
    }
    const bool Factory=CombatTab==TEXT("factory");const auto Available=Building?Hulls(G.Sim,Building->Id):TArray<FString>();
    if(Factory)
    {
        if(Available.IsEmpty()){Wrapped(TEXT("Select a command center or a completed vehicle, tank, or mech factory. Its level controls available chassis."),X+18,TY,PW-36,15,Muted);return;}
        if(!Available.Contains(ChosenChassis)){ChosenChassis=Available[0];DraftWeapons.Empty();}
        Button(TEXT("<"),TEXT("combat:hull-prev"),X+18,TY,34,34);Button(TEXT(">"),TEXT("combat:hull-next"),X+PW-52,TY,34,34);Label(C.Chassis[ChosenChassis].Name,X+66,TY+7,14,Text);TY+=45;
    }
    else
    {
        Button(OutfitBuilding?TEXT("Platform: selected building"):TEXT("Platform: selected vehicle"),TEXT("combat:outfit-platform"),X+18,TY,PW-36,34);TY+=44;
        if(!OutfitBuilding)
        {Button(TEXT("<"),TEXT("combat:vehicle-prev"),X+18,TY,34,34);Button(TEXT(">"),TEXT("combat:vehicle-next"),X+PW-52,TY,34,34);Label(Vehicle?C.Chassis[Vehicle->ChassisId].Name:TEXT("No vehicle"),X+66,TY+7,14,Text);TY+=42;}
        const FString Context=OutfitBuilding?FString::Printf(TEXT("building_%d"),Building?Building->Id:0):FString::Printf(TEXT("vehicle_%d"),OutfitVehicleId);
        if(LoadoutContext!=Context){LoadoutContext=Context;DraftWeapons.Empty();if(OutfitBuilding&&Building){if(const auto* State=C.BuildingState.Find(Building->Id))DraftWeapons=State->Weapons;else if(const auto* P=C.BuildingPlatforms.Find(Building->DefId))DraftWeapons=P->Weapons;}else if(Vehicle)DraftWeapons=Vehicle->Weapons;}
    }
    const auto* Hull=Factory?C.Chassis.Find(ChosenChassis):!OutfitBuilding&&Vehicle?C.Chassis.Find(Vehicle->ChassisId):nullptr;
    const auto* Platform=!Factory&&OutfitBuilding&&Building?C.BuildingPlatforms.Find(Building->DefId):nullptr;
    if(!Hull&&!Platform){Wrapped(TEXT("Select a platform with equipment hardpoints."),X+18,TY,PW-36,14,Muted);return;}
    int32 Used=0;double Mass=0,Dps=0;for(const auto& Id:DraftWeapons)if(const auto* Weapon=C.Weapons.Find(Id)){Used+=Weapon->MountPoints;Mass+=Weapon->MassKg;Dps+=Weapon->Damage/Weapon->ReloadSeconds;}
    const int32 Capacity=Hull?Hull->MountPoints:Platform->MountPoints;const double MassCap=Hull?Hull->MaxWeaponMassKg:Platform->MaxWeaponMassKg;
    Label(FString::Printf(TEXT("Mounts %d / %d small-equivalent  |  %.0f / %.0f kg"),Used,Capacity,Mass,MassCap),X+18,TY,12,Used<=Capacity&&Mass<=MassCap?Mint:Gold);TY+=23;
    Label(TEXT("1 large = 4 medium = 16 small mounting points"),X+18,TY,12,Muted);TY+=27;
    const auto Ids=WeaponIds(G.Sim);if(!Ids.Contains(ChosenWeapon)&&Ids.Num())ChosenWeapon=Ids[0];
    Button(TEXT("<"),TEXT("combat:weapon-prev"),X+18,TY,34,34);Button(TEXT(">"),TEXT("combat:weapon-next"),X+PW-52,TY,34,34);
    const auto* Weapon=C.Weapons.Find(ChosenWeapon);if(Weapon)Label(Weapon->Name,X+66,TY+7,14,Gold);TY+=42;
    if(Weapon){Wrapped(FString::Printf(TEXT("%.0f damage / %.1fs reload / %.1f DPS / %.2f kWh + %.0f %s per shot"),Weapon->Damage,Weapon->ReloadSeconds,Weapon->Damage/Weapon->ReloadSeconds,Weapon->EnergyKWh,Weapon->AmmoPerShot,Weapon->Ammo.IsEmpty()?TEXT("ammo"):*Weapon->Ammo),X+18,TY,PW-36,12,Muted);TY+=6;}
    Button(TEXT("Add module"),TEXT("combat:add"),X+18,TY,132,34);Button(TEXT("Remove"),TEXT("combat:remove"),X+158,TY,132,34);Button(TEXT("Clear"),TEXT("combat:clear"),X+298,TY,134,34);TY+=44;
    TMap<FString,int32> Counts;for(const auto& Id:DraftWeapons)++Counts.FindOrAdd(Id);TArray<FString> Rows;for(const auto& Pair:Counts)Rows.Add(FString::Printf(TEXT("%d x %s"),Pair.Value,*C.Weapons[Pair.Key].Name));Rows.Sort();
    auto Lines=WrapLines(Rows.IsEmpty()?TEXT("No modules fitted"):FString::Join(Rows,TEXT(" / ")),PW-36,12);const int32 MaxLines=FMath::Max(1,FMath::FloorToInt((Y+PH-110-TY)/19));if(Lines.Num()>MaxLines){Lines.SetNum(MaxLines);Lines.Last()+=TEXT(" ...");}for(const auto& Line:Lines){Label(Line,X+18,TY,12,Text);TY+=19;}
    if(Factory&&Hull)
    {
        const auto* Capability=Building?C.Factories.Find(Building->DefId):nullptr;const double Rate=Capability?Capability->SpeedMultiplier:1.;
        Label(FString::Printf(TEXT("%.0f min / %.1f kWh / %.0f L cargo / %d fleet points"),Hull->BuildSeconds/FMath::Max(Rate,.0001)/60,Hull->BuildKWh,Hull->StorageLitres,Hull->CapacityPoints),X+18,Y+PH-89,12,Muted);
        Button(TEXT("Request materials"),TEXT("combat:request"),X+18,Y+PH-57,201,38);Button(TEXT("Assemble vehicle"),TEXT("combat:queue"),X+231,Y+PH-57,201,38);
    }
    else
    {
        Label(FString::Printf(TEXT("%.1f nominal DPS / select a home service building for parts"),Dps),X+18,Y+PH-89,12,Muted);
        Button(TEXT("Request parts"),TEXT("combat:refit-request"),X+18,Y+PH-57,201,38,false,TEXT("Requests the draft weapon bill at the selected own building; a vehicle must reach a completed factory to install it."));
        Button(TEXT("Install outfit"),TEXT("combat:refit"),X+231,Y+PH-57,201,38,false,TEXT("Requires local delivered materials and energy. Removed modules are not refunded."));
    }
}
