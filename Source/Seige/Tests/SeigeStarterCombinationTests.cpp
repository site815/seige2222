#include "AI/SeigeScenarioAI.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
const TArray<FString> StandardTypes={TEXT("carbon"),TEXT("iron_ore"),TEXT("silica"),TEXT("water")};
const TArray<FString> RareTypes={TEXT("copper_ore"),TEXT("crystalline"),TEXT("hydrocarbons"),TEXT("radioactive_ore")};
FString Combination(const TArray<FSeigeNode>& Nodes)
{
    FString Missing;TArray<FString> Rare;
    for(const auto& Id:StandardTypes)if(!Nodes.ContainsByPredicate([&](const auto& N){return N.Resource==Id;}))Missing=Id;
    for(const auto& N:Nodes)if(RareTypes.Contains(N.Resource))Rare.Add(N.Resource);
    Rare.Sort();return Missing+TEXT("__")+FString::Join(Rare,TEXT("_"));
}
bool BootstrapPlan(const FString& Directory,FString& Error)
{
    IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Source=FPaths::Combine(FPaths::ProjectDir(),TEXT("AIFILES"));
    for(const TCHAR* File:{TEXT("colony_ai.json"),TEXT("developed_start.json")})
    {
        FString Raw;if(!FFileHelper::LoadFileToString(Raw,*FPaths::Combine(Source,File)))return false;
        if(FString(File)==TEXT("colony_ai.json"))
        {
            TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),O))return false;
            TArray<TSharedPtr<FJsonValue>> Targets;int32 Index=0;
            // A real human-sized economic bootstrap, with no combat/endurance claim.
            for(const TCHAR* Id:{TEXT("solar_array"),TEXT("robot_service_bay"),TEXT("extraction_mine"),TEXT("trading_port"),TEXT("sensor")})
            {auto T=MakeShared<FJsonObject>();T->SetStringField(TEXT("definition"),Id);T->SetNumberField(TEXT("count"),1);T->SetNumberField(TEXT("placement_index"),Index++);Targets.Add(MakeShared<FJsonValueObject>(T));}
            O->SetArrayField(TEXT("build_targets"),Targets);
            Raw.Empty();if(!FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw)))return false;
        }
        if(!FFileHelper::SaveStringToFile(Raw,*FPaths::Combine(Directory,File)))return false;
    }
    return true;
}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FSeigeStarterCombinations,"Seige.Economy.StarterCombinations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FSeigeStarterCombinations::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const
{
    for(const auto& Missing:StandardTypes)for(int32 A=0;A<RareTypes.Num();++A)for(int32 B=A+1;B<RareTypes.Num();++B)
    {const FString Key=Missing+TEXT("__")+RareTypes[A]+TEXT("_")+RareTypes[B];Names.Add(Key);Commands.Add(Key);}
}
bool FSeigeStarterCombinations::RunTest(const FString& Parameters)
{
    const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));
    FSeigeSimulation Catalog;FString Error;if(!Catalog.Initialize(Rules,Error,false,false)){AddError(Error);return false;}
    int32 Seed=INDEX_NONE;TArray<FSeigeNode> Nodes;
    for(int32 Candidate=0;Candidate<4096;++Candidate)
        if(Catalog.GenerateResourceNodesForSeed(Candidate,Nodes,Error)&&Combination(Nodes)==Parameters){Seed=Candidate;break;}
    if(!TestTrue(TEXT("A deterministic legal seed exists for this exact 3+2 composition"),Seed!=INDEX_NONE))return false;
    const FString Plan=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/StarterCombinations"),Parameters);
    if(!BootstrapPlan(Plan,Error)){AddError(TEXT("Cannot create isolated bootstrap plan"));return false;}
    FSeigeScenarioAI Brain;FSeigeSimulation S;
    if(!Brain.Initialize(S,Rules,Plan,false,Error,false,false,Seed)){AddError(Error);return false;}
    TestEqual(TEXT("Exact loose manifest mass"),S.InventoryMassKg(S.Buildings[0].Inventory),9570.0);
    TestEqual(TEXT("No granted credits"),S.Credits,0.0);
    FString Missing;for(const auto& Id:StandardTypes)if(!S.Nodes.ContainsByPredicate([&](const auto& N){return N.Resource==Id;}))Missing=Id;
    const double ImportPrice=S.TradeQuote(Missing,1,true);
    const TArray<FString> Required={TEXT("solar_array"),TEXT("robot_service_bay"),TEXT("extraction_mine"),TEXT("trading_port"),TEXT("sensor")};
    auto Complete=[&](){for(const auto& Id:Required)if(!S.Buildings.ContainsByPredicate([&](const auto& B){return B.Health>0&&!B.IsConstructing&&B.Enabled&&B.DefId==Id&&B.Workers>=S.Definition(B)->Jobs;}))return false;return true;};
    // Ordinary fixed-step simulation, physical workers, actual kit, roads, daylight
    // and paid trade. Threats are explicitly disabled to isolate economic viability.
    while(S.Time<86400&&!S.Failed&&(!Complete()||S.Credits<ImportPrice))Brain.Tick(S,10);
    AddInfo(FString::Printf(TEXT("seed=%d time=%.0fs workers=%d credits=%.6f status=%s"),Seed,S.Time,S.Population,S.Credits,*Brain.GetStatus()));
    if(!TestTrue(TEXT("Finite-kit paid bootstrap completes and staffs all five targets"),Complete()))return false;
    TestTrue(TEXT("Materials reached actual local inventories"),S.DeliveredUnits>0);
    TestTrue(TEXT("Local goods earned positive credits"),S.Credits>0);
    const auto* Mine=S.Buildings.FindByPredicate([](const auto& B){return B.DefId==TEXT("extraction_mine");});
    TestTrue(TEXT("Local extraction is powered and staffed"),Mine&&Mine->Workers>=S.Definition(*Mine)->Jobs&&S.Energy.Fraction(Mine->Id)>0);
    auto* Port=S.Buildings.FindByPredicate([](const auto& B){return B.DefId==TEXT("trading_port");});
    if(!Port)return false;const int32 PortId=Port->Id;
    for(int32 I=0;I<1000&&!S.FindBuilding(PortId)->Shipment.Resource.IsEmpty();++I)S.Tick(1);
    const double Before=S.TotalStock(Missing),Credits=S.Credits,Price=S.TradeQuote(Missing,1,true);
    if(!S.TryTrade(PortId,Missing,1,true,Error)){AddError(TEXT("Earned-credit missing-raw import: ")+Error);return false;}
    TestTrue(TEXT("Import spends earned credits exactly"),FMath::IsNearlyEqual(S.Credits,Credits-Price,1.e-9));
    for(int32 I=0;I<1000&&!S.FindBuilding(PortId)->Shipment.Resource.IsEmpty();++I)S.Tick(1);
    TestTrue(TEXT("Absent standard resource physically arrives"),S.TotalStock(Missing)>=Before+1-1.e-8);
    TestFalse(TEXT("Economic bootstrap does not destroy the core"),S.Failed);
    return true;
}
#endif
