#include "Simulation/SeigeSimulation.h"
#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if WITH_DEV_AUTOMATION_TESTS
// Diagnostic only (not part of the Seige.* gate): replays the shipped
// first-playable AI run and writes a JSON-lines timeline of the colony, its
// stock, workforce, threats and every serious building damage event, so AI and
// balance changes can be judged from evidence instead of the final state alone.
// Saved/Diagnostics/first-playable-timeline.jsonl; -SeigeTimelineSeconds=N
// shortens or lengthens the run (default 64800, the gate's horizon).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeFirstPlayableTimelineDiagnostic,"Diagnostics.FirstPlayableTimeline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeFirstPlayableTimelineDiagnostic::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;FSeigeScenarioAI Controller;
    if(!Controller.Initialize(S,FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES")),false,Error)){AddError(Error);return false;}
    double Horizon=64800;FParse::Value(FCommandLine::Get(),TEXT("SeigeTimelineSeconds="),Horizon);
    TArray<FString> Lines;
    auto Q=[](const FString& Text){return TEXT("\"")+Text.Replace(TEXT("\\"),TEXT("/")).Replace(TEXT("\""),TEXT("'"))+TEXT("\"");};
    const TArray<FString> Stocks={TEXT("alloy"),TEXT("conductors"),TEXT("substrates"),TEXT("circuits"),TEXT("components"),TEXT("batteries"),TEXT("plastic"),TEXT("fuel"),TEXT("iron_ore"),TEXT("silica"),TEXT("carbon"),TEXT("water"),TEXT("biomass")};
    auto Snapshot=[&](const TCHAR* Kind,const FString& Extra)
    {
        const FSeigeBuilding* Core=S.Core();
        FString Line=FString::Printf(TEXT("{\"kind\":\"%s\",\"t\":%.0f,\"wave\":%d,\"won\":%d,\"population\":%d,\"employed\":%d,\"jobs\":%d,\"support\":%d,\"efficiency\":%.3f,\"credits\":%.2f,\"delivered\":%.0f,\"ai\":%s"),Kind,S.Time,S.Wave,S.Won?1:0,S.Population,S.Employed,S.TotalJobs,S.RobotSupportCapacity,S.OperatingEfficiency(),S.Credits,S.DeliveredUnits,*Q(Controller.GetStatus()));
        Line+=TEXT(",\"stock\":{");bool First=true;
        for(const auto& Id:Stocks)if(S.Resources.Contains(Id)){Line+=FString::Printf(TEXT("%s\"%s\":[%.1f,%.1f]"),First?TEXT(""):TEXT(","),*Id,S.TotalStock(Id),S.ConstructionAvailable(Id));First=false;}
        Line+=TEXT("},\"enemies\":[");First=true;
        for(const auto& E:S.Enemies)if(E.Health>0&&Core)
        {const double D=FVector2D::Distance(E.Position,Core->Position);if(D>6000)continue;Line+=FString::Printf(TEXT("%s[%.0f,%.0f,%.0f,%d,%d]"),First?TEXT(""):TEXT(","),E.Position.X-Core->Position.X,E.Position.Y-Core->Position.Y,E.Health,S.IsVisible(E.Position)?1:0,E.TargetBuildingId);First=false;}
        Line+=TEXT("],\"buildings\":[");First=true;
        for(const auto& B:S.Buildings)
        {
            const auto* D=S.Definition(B);if(!D||!Core)continue;
            Line+=FString::Printf(TEXT("%s{\"id\":%d,\"def\":\"%s\",\"x\":%.0f,\"y\":%.0f,\"hp\":%.0f,\"c\":%d,\"p\":%.2f,\"w\":%d,\"j\":%d,\"pw\":%.2f,\"rep\":%.1f,\"repok\":%.1f,\"st\":%s}"),First?TEXT(""):TEXT(","),B.Id,*B.DefId,B.Position.X-Core->Position.X,B.Position.Y-Core->Position.Y,B.Health,B.IsConstructing?1:0,B.ConstructionProgress,B.Workers,D->Jobs,S.Energy.Fraction(B.Id),B.Inventory.FindRef(S.TextRule(TEXT("repair_resource"))),S.Spendable(B,S.TextRule(TEXT("repair_resource"))),*Q(B.Status));
            First=false;
        }
        Line+=TEXT("],\"fleets\":[");First=true;
        for(const auto& F:S.Combat.Fleets)
        {
            double Battery=1;int32 Alive=0;for(const auto& V:S.Combat.Vehicles)if(V.FleetId==F.Id&&V.Health>0&&!V.Evacuated){++Alive;Battery=FMath::Min(Battery,V.BatteryKWh/S.Combat.Chassis[V.ChassisId].BatteryKWh);}
            Line+=FString::Printf(TEXT("%s{\"mission\":\"%s\",\"alive\":%d,\"battery\":%.2f}"),First?TEXT(""):TEXT(","),*F.Mission,Alive,Battery);First=false;
        }
        Line+=TEXT("]")+Extra+TEXT("}");Lines.Add(Line);
    };
    TMap<int32,double> Warned;TSet<int32> Lost;double NextSample=0;
    while(S.Time<Horizon&&!S.Won&&!S.Escaped&&!S.Failed)
    {
        Controller.Tick(S,10);
        if(S.Time>=NextSample){Snapshot(TEXT("sample"),FString());NextSample=S.Time+600;}
        for(const auto& B:S.Buildings)
        {
            const auto* D=S.Definition(B);if(!D)continue;
            const bool Dead=B.Health<=0,Low=B.Health<D->Health*.5;
            if((Dead&&!Lost.Contains(B.Id))||(Low&&!Dead&&S.Time-Warned.FindRef(B.Id)>1800&&!Warned.Contains(-B.Id)))
            {
                FString Extra=FString::Printf(TEXT(",\"event\":{\"building\":%d,\"def\":\"%s\",\"dead\":%d,\"guns\":["),B.Id,*B.DefId,Dead?1:0);bool First=true;
                for(const auto& G:S.Buildings)if(G.Health>0&&S.Combat.BuildingState.Contains(G.Id)&&!S.Combat.BuildingState[G.Id].Weapons.IsEmpty())
                {Extra+=FString::Printf(TEXT("%s[%d,\"%s\",%.0f,%s]"),First?TEXT(""):TEXT(","),G.Id,*G.DefId,FVector2D::Distance(G.Position,B.Position),*Q(S.Combat.BuildingFireStatus(S,G.Id)));First=false;}
                Extra+=TEXT("]}");Snapshot(Dead?TEXT("lost"):TEXT("damaged"),Extra);
                if(Dead)Lost.Add(B.Id);else Warned.Add(B.Id,S.Time);
            }
        }
    }
    Snapshot(TEXT("final"),FString());
    const FString Path=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Diagnostics/first-playable-timeline.jsonl"));
    FFileHelper::SaveStringToFile(FString::Join(Lines,TEXT("\n"))+TEXT("\n"),*Path);
    AddInfo(FString::Printf(TEXT("Timeline %d lines -> %s; won=%d t=%.0f; %s"),Lines.Num(),*Path,S.Won?1:0,S.Time,*S.ObjectiveText()));
    return true;
}
#endif
