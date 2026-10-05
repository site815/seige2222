#include "Simulation/SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeCombatEditableBudgetTest,"Seige.Combat.EditableHardpointAndFleetBudgets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeCombatEditableBudgetTest::RunTest(const FString& Parameters)
{
    const FString Rules=FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));FString Error;FSeigeSimulation Default;
    if(!Default.Initialize(Rules,Error,false,false)){AddError(Error);return false;}
    // Confirm the user's authored defaults without embedding them as runtime
    // rejection conditions that prevent a consistent modded definition set.
    TestEqual(TEXT("Default fleet capacity is fifty"),Default.Combat.FleetCapacity,50);
    int32 Index=0;for(const FString Size:{FString(TEXT("small")),FString(TEXT("medium")),FString(TEXT("large"))})
    {
        const auto& H=Default.Combat.HardpointSizes[Size];
        TestEqual(TEXT("Default hardpoint width"),H.WidthMeters,.5*(1<<Index));
        TestEqual(TEXT("Default hardpoint height"),H.HeightMeters,H.WidthMeters);
        TestEqual(TEXT("Default hardpoint length"),H.LengthMeters,2.*(1<<Index));
        TestEqual(TEXT("Default hardpoint mass ceiling"),H.MassLimitKg,500.*FMath::Pow(8.,Index));
        TestEqual(TEXT("Default frontal-area points"),H.MountPoints,1<<(2*Index));++Index;
    }
    for(const auto& C:Default.Combat.Chassis)TestEqual(TEXT("Authored chassis points remain one/two/four/eight"),C.Value.CapacityPoints,1<<(C.Value.Tier-1));
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/CombatEditableBudgets"));IFileManager::Get().MakeDirectory(*Directory,true);
    TArray<FString> Files;IFileManager::Get().FindFiles(Files,*FPaths::Combine(Rules,TEXT("*.json")),true,false);
    for(const auto& File:Files)if(IFileManager::Get().Copy(*FPaths::Combine(Directory,File),*FPaths::Combine(Rules,File))!=COPY_OK){AddError(TEXT("Cannot copy isolated combat definitions"));return false;}
    auto Edit=[&](const FString& Name,const TFunction<void(TSharedPtr<FJsonObject>)>& Change)
    {
        const FString Path=FPaths::Combine(Directory,Name);FString Raw;TSharedPtr<FJsonObject> O;
        if(!FFileHelper::LoadFileToString(Raw,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw),O))return false;
        Change(O);Raw.Empty();return FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&Raw))&&FFileHelper::SaveStringToFile(Raw,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    };
    if(!Edit(TEXT("weapons.json"),[](auto O)
    {
        for(const auto& P:O->GetObjectField(TEXT("hardpoint_sizes"))->Values)
        {auto H=P.Value->AsObject();for(const TCHAR* K:{TEXT("width_meters"),TEXT("height_meters"),TEXT("mass_limit_kg")})H->SetNumberField(K,H->GetNumberField(K)*1.2);H->SetNumberField(TEXT("length_meters"),H->GetNumberField(TEXT("length_meters"))*1.25);H->SetNumberField(TEXT("mount_points"),H->GetNumberField(TEXT("mount_points"))*2);}
        for(const auto& V:O->GetArrayField(TEXT("weapons"))){auto W=V->AsObject();W->SetNumberField(TEXT("mount_points"),W->GetNumberField(TEXT("mount_points"))*2);if(W->GetStringField(TEXT("id"))==TEXT("laser_small")){W->SetNumberField(TEXT("mass_kg"),550);W->GetObjectField(TEXT("cost"))->SetNumberField(TEXT("alloy"),545);}}
    })||!Edit(TEXT("combat.json"),[](auto O)
    {O->SetNumberField(TEXT("fleet_capacity"),60);for(const auto& P:O->GetObjectField(TEXT("building_platforms"))->Values){auto B=P.Value->AsObject();B->SetNumberField(TEXT("mount_points"),B->GetNumberField(TEXT("mount_points"))*2);B->SetNumberField(TEXT("max_weapon_mass_kg"),B->GetNumberField(TEXT("max_weapon_mass_kg"))*2);}})
    ||!Edit(TEXT("chassis.json"),[](auto O)
    {for(const auto& V:O->GetArrayField(TEXT("chassis"))){auto C=V->AsObject();C->SetNumberField(TEXT("mount_points"),C->GetNumberField(TEXT("mount_points"))*2);C->SetNumberField(TEXT("max_weapon_mass_kg"),C->GetNumberField(TEXT("max_weapon_mass_kg"))*2);C->SetNumberField(TEXT("capacity_points"),C->GetNumberField(TEXT("capacity_points"))*20);}}))return false;
    FSeigeSimulation Edited;if(!Edited.Initialize(Directory,Error,false,false)){AddError(Error);return false;}
    TestEqual(TEXT("Edited fleet capacity is authoritative"),Edited.Combat.FleetCapacity,60);
    TestEqual(TEXT("Edited hardpoint envelope is loaded"),Edited.Combat.HardpointSizes[TEXT("small")].WidthMeters,.6);
    TestEqual(TEXT("Edited mass ceiling admits a module above the original 500 kg"),Edited.Combat.Weapons[TEXT("laser_small")].MassKg,550.);
    TestTrue(TEXT("Edited module uses the configured point unit"),Edited.Combat.ValidateLoadout(TEXT("wheeled_small"),{TEXT("laser_small")},Error));
    TestFalse(TEXT("Edited area budget still rejects a second module"),Edited.Combat.ValidateLoadout(TEXT("wheeled_small"),{TEXT("laser_small"),TEXT("laser_small")},Error));
    const int32 Fleet=Edited.Combat.Fleets[0].Id;TestEqual(TEXT("Actual fleet usage comes from edited chassis costs"),Edited.Combat.FleetUsed(Fleet),40);
    auto Vehicle=Edited.Combat.Vehicles[0];Vehicle.Id=999001;Vehicle.Embarked=false;Vehicle.FleetId=0;Edited.Combat.Vehicles.Add(Vehicle);
    TestTrue(TEXT("A third twenty-point chassis fits the edited sixty-point fleet"),Edited.Combat.AssignVehicle(Edited,Vehicle.Id,Fleet,Error));
    Vehicle.Id=999002;Edited.Combat.Vehicles.Add(Vehicle);
    TestFalse(TEXT("A fourth chassis exceeds the edited fleet budget"),Edited.Combat.AssignVehicle(Edited,Vehicle.Id,Fleet,Error));
    if(!Edit(TEXT("weapons.json"),[](auto O){for(const auto& V:O->GetArrayField(TEXT("weapons")))if(V->AsObject()->GetStringField(TEXT("id"))==TEXT("laser_small"))V->AsObject()->SetNumberField(TEXT("mass_kg"),601);}))return false;
    FSeigeSimulation Invalid;TestFalse(TEXT("Configured mass ceiling remains enforced"),Invalid.Initialize(Directory,Error,false,false));
    if(!Edit(TEXT("weapons.json"),[](auto O){for(const auto& V:O->GetArrayField(TEXT("weapons")))if(V->AsObject()->GetStringField(TEXT("id"))==TEXT("laser_small"))V->AsObject()->SetNumberField(TEXT("mass_kg"),550);O->GetObjectField(TEXT("hardpoint_sizes"))->GetObjectField(TEXT("medium"))->SetNumberField(TEXT("mount_points"),9);}))return false;
    TestFalse(TEXT("Points inconsistent with physical frontal area are rejected"),Invalid.Initialize(Directory,Error,false,false));
    return true;
}
#endif
