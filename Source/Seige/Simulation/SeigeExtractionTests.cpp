#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString MineRules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
FString MineSave(const TCHAR* Name){return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/Extraction"),FString(Name)+TEXT(".json"));}
bool ReadMineSave(const FString& Path,TSharedPtr<FJsonObject>& Object)
{FString Text;return FFileHelper::LoadFileToString(Text,*Path)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Object);}
bool WriteMineSave(const FString& Path,const TSharedPtr<FJsonObject>& Object)
{FString Text;return FJsonSerializer::Serialize(Object.ToSharedRef(),TJsonWriterFactory<>::Create(&Text))&&FFileHelper::SaveStringToFile(Text,*Path);}
// Isolate mine selection and production from construction pacing. The landing
// kit and site bill are installed from carried stock, as in workforce fixtures.
bool LandBeside(FSeigeSimulation& S,const FSeigeNode& Node,FString& Error)
{
    if(!S.SetInitialCorePosition(Node.Position-FVector2D(1200,0),Error))return false;
    auto& C=S.Buildings[0];C.InstalledMaterials=C.ConstructionMaterials;C.ConstructionMaterials.Empty();
    C.IsConstructing=false;C.ConstructionProgress=1;C.Builders=C.BuildersOnSite=C.TravellingBuilders=0;
    S.Tick(S.FixedStepSeconds());return true;
}
void InstallMine(FSeigeSimulation& S,int32 Id)
{
    auto& B=*S.FindBuilding(Id);B.InstalledMaterials=S.Definition(B)->Cost;
    for(const auto& P:B.InstalledMaterials)S.Buildings[0].Inventory.FindOrAdd(P.Key)-=P.Value;
    B.ConstructionMaterials.Empty();B.IsConstructing=false;B.ConstructionProgress=1;
    B.Builders=B.BuildersOnSite=B.TravellingBuilders=0;B.BuilderRoute.Empty();B.BuilderNextWaypoint=0;
    S.Population=S.Definition(S.Buildings[0])->Jobs+S.Definition(B)->Jobs;B.Workers=S.Definition(B)->Jobs;
}
bool ConnectMineFixture(FSeigeSimulation& S,int32 Id,FString& Error)
{
    FVector2D Last=S.BuildingAccessPoint(S.Buildings[0]);TArray<FVector2D> Route;
    if(!S.FindRoadRoute(Last,S.BuildingAccessPoint(*S.FindBuilding(Id)),Route))return false;
    for(const auto& End:Route)
    {
        if(!S.PlaceRoad(Last,End,Error))return false;
        auto& R=S.Roads.Last();R.InstalledMaterials=S.RoadCost(R.A,R.B,R.TargetTier);
        for(const auto& P:R.InstalledMaterials)S.Buildings[0].Inventory.FindOrAdd(P.Key)-=P.Value;
        R.Tier=R.TargetTier;R.IsConstructing=false;R.ConstructionProgress=1;
        R.Builders=R.BuildersOnSite=R.TravellingBuilders=0;R.BuilderRoute.Empty();R.BuilderNextWaypoint=0;Last=End;
    }
    S.Energy.Invalidate();S.Energy.Tick(S,0);return S.IsRoadGridConnected(S.Buildings[0].Id,Id);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeExtractionMineTest,"Seige.Simulation.ExtractionMine.AllDepositOutputsAndGates",Flags)
bool FSeigeExtractionMineTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation Catalog;if(!Catalog.Initialize(MineRules(),Error,false,false)){AddError(Error);return false;}
    TArray<FString> Mines;for(const auto& Id:Catalog.BuildMenu)if(Catalog.BuildingDefs[Id].Role==TEXT("extractor"))Mines.Add(Id);
    if(!TestEqual(TEXT("Exactly one buildable mine replaces all resource-specific blueprints"),Mines.Num(),1))return false;
    const FString Mine=Mines[0];TSet<FString> Tested;
    for(int32 Seed=0;Seed<128&&Tested.Num()<Catalog.BuildingDefs[Mine].ExtractionRates.Num();++Seed)
    {
        FSeigeSimulation Probe;if(!Probe.Initialize(MineRules(),Error,false,false,Seed)){AddError(Error);return false;}
        for(const auto& Node:Probe.Nodes)
        {
            if(Tested.Contains(Node.Resource))continue;
            FSeigeSimulation S=Probe;if(!LandBeside(S,Node,Error)){AddError(Error);return false;}
            TestFalse(TEXT("Mine placement needs a real deposit"),S.CanPlaceBuilding(Mine,Node.Position+FVector2D(600,0),Error));
            if(!S.PlaceBuilding(Mine,Node.Position,Error)){AddError(Error);return false;}
            const int32 Id=S.Buildings.Last().Id;auto& B=*S.FindBuilding(Id);
            TestEqual(TEXT("Placement binds the actual deposit identity"),B.DepositId,Node.Id);
            TestEqual(TEXT("Generic mine resolves this deposit's output"),S.ExtractionResource(B),Node.Resource);
            TestFalse(TEXT("A living construction site reserves the deposit"),S.CanPlaceBuilding(Mine,Node.Position+FVector2D(40,40),Error));
            S.StepProduction(1);TestEqual(TEXT("Unfinished mine produces nothing"),B.Inventory.FindRef(Node.Resource),0.);
            InstallMine(S,Id);S.AllocateWorkers();S.Energy.Invalidate();S.Energy.Tick(S,0);S.StepProduction(1);
            TestEqual(TEXT("A completed mine without a powered road cannot extract"),B.Inventory.FindRef(Node.Resource),0.);
            if(!ConnectMineFixture(S,Id,Error)){AddError(Error);return false;}S.AllocateWorkers();S.Energy.Tick(S,0);
            const double Rate=S.ExtractionRate(B)*S.WorkFraction(B);S.StepProduction(1);
            TestTrue(TEXT("The mine produces its configured deposit rate"),FMath::IsNearlyEqual(B.Inventory.FindRef(Node.Resource),Rate,1.e-8));
            for(const auto& Raw:Catalog.Resources)if(Raw.Value.Class!=TEXT("manufactured")&&Raw.Key!=Node.Resource)TestEqual(TEXT("No unrelated raw material is created"),B.Inventory.FindRef(Raw.Key),0.);
            B.Enabled=false;S.StepProduction(1);TestTrue(TEXT("Disabled mine stops extraction"),FMath::IsNearlyEqual(B.Inventory.FindRef(Node.Resource),Rate,1.e-8));B.Enabled=true;
            const double Capacity=S.Definition(B)->StorageCapacity/S.Resources[Node.Resource].LitresPerUnit;
            B.Inventory.Empty();B.Inventory.Add(Node.Resource,Capacity-.25);S.StepProduction(1);
            TestTrue(TEXT("Storage cap respects each material's litres per unit"),FMath::IsNearlyEqual(B.Inventory.FindRef(Node.Resource),Capacity,1.e-8));
            TestFalse(TEXT("Full mine has no active extraction work"),S.HasActiveWork(B));
            Tested.Add(Node.Resource);
        }
    }
    TestEqual(TEXT("All configured standard and rare deposit types were exercised"),Tested.Num(),Catalog.BuildingDefs[Mine].ExtractionRates.Num());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeMinePersistenceTest,"Seige.Simulation.ExtractionMine.BindingPersistenceAndAtomicRejection",Flags)
bool FSeigeMinePersistenceTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;
    if(!S.Initialize(MineRules(),Error,false,false)||!Loaded.Initialize(MineRules(),Error,false,false)){AddError(Error);return false;}
    const auto Node=S.Nodes[0];FString Mine;
    for(const auto& Id:S.BuildMenu)if(S.BuildingDefs[Id].ExtractionRates.Contains(Node.Resource)){Mine=Id;break;}
    if(!LandBeside(S,Node,Error)||!S.PlaceBuilding(Mine,Node.Position,Error)){AddError(Error);return false;}
    const int32 Id=S.Buildings.Last().Id;const FString Path=MineSave(TEXT("binding"));
    if(!S.Save(Path,Error)||!Loaded.Load(Path,Error)){AddError(Error);return false;}
    TestTrue(TEXT("An unfinished mine retains its physical site and deposit after load"),Loaded.FindBuilding(Id)->IsConstructing&&Loaded.FindBuilding(Id)->DepositId==Node.Id);
    InstallMine(S,Id);S.FindBuilding(Id)->Inventory.Add(Node.Resource,7.25);
    if(!S.Save(Path,Error)||!Loaded.Load(Path,Error)){AddError(Error);return false;}
    TestEqual(TEXT("Completed mine retains output selection and fractional inventory"),Loaded.ExtractionResource(*Loaded.FindBuilding(Id)),Node.Resource);
    TestEqual(TEXT("Mine inventory survives the round trip"),Loaded.FindBuilding(Id)->Inventory.FindRef(Node.Resource),7.25);
    const double Before=Loaded.Time;const FString Bad=MineSave(TEXT("invalid-binding"));
    for(int32 Case=0;Case<5;++Case)
    {
        TSharedPtr<FJsonObject> Document;if(!ReadMineSave(Path,Document))return false;
        auto Site=Document->GetArrayField(TEXT("buildings"))[1]->AsObject();
        if(Case==0)Site->SetNumberField(TEXT("deposit_id"),S.Nodes[1].Id);
        if(Case==1)Site->RemoveField(TEXT("deposit_id"));
        if(Case==2)Document->SetNumberField(TEXT("save_format"),5);
        if(Case==3)Document->GetArrayField(TEXT("buildings"))[0]->AsObject()->SetNumberField(TEXT("deposit_id"),Node.Id);
        if(Case==4)
        {
            TSharedPtr<FJsonObject> Copy;if(!ReadMineSave(Path,Copy))return false;
            auto Duplicate=Copy->GetArrayField(TEXT("buildings"))[1]->AsObject();
            const double IdValue=Document->GetNumberField(TEXT("next_id"));Duplicate->SetNumberField(TEXT("id"),IdValue);
            auto Buildings=Document->GetArrayField(TEXT("buildings"));Buildings.Add(MakeShared<FJsonValueObject>(Duplicate));
            Document->SetArrayField(TEXT("buildings"),Buildings);Document->SetNumberField(TEXT("next_id"),IdValue+1);
        }
        if(!WriteMineSave(Bad,Document))return false;
        TestFalse(TEXT("Invalid deposit binding or previous save format is rejected"),Loaded.Load(Bad,Error));
        TestTrue(TEXT("Rejection identifies the binding or explicitly incompatible format"),Error.Contains(Case==2?TEXT("format 6"):TEXT("deposit binding")));
        TestTrue(TEXT("Rejected load leaves the current mine and inventory untouched"),Loaded.Time==Before&&Loaded.FindBuilding(Id)->DepositId==Node.Id&&Loaded.FindBuilding(Id)->Inventory.FindRef(Node.Resource)==7.25);
    }
    FString After;if(!FFileHelper::LoadFileToString(After,*Bad))return false;
    TestTrue(TEXT("Rejected old or malformed saves remain on disk"),!After.IsEmpty());
    return true;
}
#endif
