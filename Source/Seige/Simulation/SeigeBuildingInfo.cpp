#include "SeigeSimulation.h"

TArray<FSeigeBuildingInfoRow> FSeigeSimulation::BuildingInfo(const FString& DefinitionId,int32 BuildingId,double CentimetersPerUnit) const
{
    TArray<FSeigeBuildingInfoRow> Rows;
    const FSeigeBuildingDef* Def=BuildingDefs.Find(DefinitionId);
    const FSeigeBuilding* B=BuildingId?FindBuilding(BuildingId):nullptr;
    if(!Policy||!Def||(BuildingId&&(!B||B->DefId!=DefinitionId)))return Rows;
    const auto& D=*Def;
    if(!FMath::IsFinite(CentimetersPerUnit)||CentimetersPerUnit<=0)CentimetersPerUnit=1;
    auto N=[](double Value){return FString::SanitizeFloat(Value,0);};
    auto Name=[&](const FString& Id){const auto* R=Resources.Find(Id);return R?R->Name:Id;};
    auto Add=[&](const TCHAR* Section,const TCHAR* Label,const FString& Value){Rows.Add({Section,Label,Value});};
    auto Amounts=[&](const TMap<FString,double>& Values)
    {
        TArray<FString> Keys,Parts;Values.GetKeys(Keys);Keys.Sort();
        for(const FString& Key:Keys)if(Values[Key]>0)Parts.Add(N(Values[Key])+TEXT(" ")+Name(Key));
        return Parts.IsEmpty()?FString(TEXT("None (0)")):FString::Join(Parts,TEXT(", "));
    };
    auto Metres=[&](double Units){return N(Units*CentimetersPerUnit/100)+TEXT(" m");};
    const double Fraction=B?WorkFraction(*B):1;
    Add(TEXT("Overview"),TEXT("Building"),D.Name);
    Add(TEXT("Overview"),TEXT("Role"),D.Role);
    Add(TEXT("Overview"),TEXT("Status"),B?B->Status:TEXT("Blueprint"));
    Add(TEXT("Overview"),TEXT("Health"),B?N(B->Health)+TEXT(" / ")+N(D.Health):N(D.Health)+TEXT(" maximum"));
    Add(TEXT("Overview"),TEXT("Jobs"),B?FString::Printf(TEXT("%d / %d staffed"),B->Workers,D.Jobs):FString::FromInt(D.Jobs));
    Add(TEXT("Overview"),TEXT("Automatic staffing priority"),FString::FromInt(D.StaffingPriority)+TEXT(" (lower first; includes construction)"));
    Add(TEXT("Overview"),TEXT("Operating efficiency"),N(Fraction*100)+TEXT("%")+(B?TEXT(""):TEXT(" at full staffing and upkeep")));
    Add(TEXT("Overview"),TEXT("Footprint radius"),Metres(D.Footprint));
    Add(TEXT("Overview"),TEXT("Sensor range"),Metres(D.SensorRange));
    Add(TEXT("Overview"),TEXT("Construction cost"),Amounts(D.Cost));
    Add(TEXT("Overview"),TEXT("Construction payment"),D.Role==TEXT("core")?TEXT("Deployment kit physically carried by the landing shuttle"):TEXT("Reserved at core; couriers deliver before worker construction"));
    Add(TEXT("Overview"),TEXT("Construction duration"),N(D.ConstructionSeconds)+TEXT(" s with ")+FString::FromInt(D.ConstructionWorkers)+TEXT(" builders at full efficiency"));
    if(B){Add(TEXT("Overview"),TEXT("Construction progress"),N(B->ConstructionProgress*100)+TEXT("%"));Add(TEXT("Overview"),TEXT("Assigned builders"),FString::FromInt(B->Builders));}

    const bool Armed=D.DamagePerShot>0;
    Add(TEXT("Weapons"),TEXT("Weapon"),Armed?D.WeaponName:TEXT("Unarmed"));
    Add(TEXT("Weapons"),TEXT("Damage per shot"),N(D.DamagePerShot)+TEXT(" health"));
    Add(TEXT("Weapons"),TEXT("Reload time"),N(D.ReloadSeconds)+TEXT(" s at full efficiency"));
    Add(TEXT("Weapons"),TEXT("Nominal DPS"),N(D.DamagePerSecond)+TEXT(" health/s"));
    Add(TEXT("Weapons"),TEXT("Operating DPS"),N(D.DamagePerSecond*Fraction)+TEXT(" health/s with continuous targets"));
    Add(TEXT("Weapons"),TEXT("Attack range"),Metres(D.AttackRange));
    Add(TEXT("Weapons"),TEXT("Targets"),Armed?TEXT("Nearest living visible bug in range"):TEXT("None (0)"));
    Add(TEXT("Weapons"),TEXT("Ammunition use"),TEXT("None (0); no ammunition resource in this prototype"));
    if(B)Add(TEXT("Weapons"),TEXT("Reload remaining"),!Armed?TEXT("0 s (unarmed)"):B->WeaponCooldown<=UE_DOUBLE_SMALL_NUMBER?TEXT("0 s (ready)"):Fraction>0?N(B->WeaponCooldown/Fraction)+TEXT(" s"):TEXT("Paused until operational"));

    Add(TEXT("Power"),TEXT("Power consumption"),N(D.PowerUsageKW)+TEXT(" kW"));
    Add(TEXT("Power"),TEXT("Power generation"),N(D.PowerGenerationKW)+TEXT(" kW"));
    Add(TEXT("Power"),TEXT("Power system"),TEXT("No separate power grid is simulated in this prototype"));

    const bool CoreBuilding=D.Role==TEXT("core");
    const FSeigeRecipeDef* Recipe=Recipes.Find(CoreBuilding?TextRule(TEXT("population_recipe")):D.Recipe);
    if(Recipe)
    {
        Add(TEXT("Production"),TEXT("Inputs per cycle"),Amounts(Recipe->Inputs));
        Add(TEXT("Production"),TEXT("Outputs per cycle"),CoreBuilding?TEXT("1 robot when population is below job demand/minimum"):Amounts(Recipe->Outputs));
        Add(TEXT("Production"),TEXT("Base cycle time"),N(Recipe->Seconds)+TEXT(" simulation s"));
        if(!CoreBuilding)Add(TEXT("Production"),TEXT("Operating cycle time"),Fraction>0?N(Recipe->Seconds/Fraction)+TEXT(" s; requires local inputs and output space"):TEXT("Paused until operational"));
        else Add(TEXT("Production"),TEXT("Assembly policy"),TEXT("Open jobs, available service capacity and local inputs; after deployment"));
        if(B)
        {
            const int32 Target=FMath::Max(TotalJobs,int32(Number(TEXT("minimum_population"))));
            const bool Retiring=CoreBuilding&&Population>Target,Idle=CoreBuilding&&Population==Target;
            const double Progress=CoreBuilding?(Idle?0:PopulationClock/(Retiring?Number(TEXT("robot_retire_seconds")):Recipe->Seconds)):B->Progress;
            Add(TEXT("Production"),TEXT("Cycle progress"),N(FMath::Clamp(Progress,0.,1.)*100)+TEXT("%")+(Retiring?TEXT(" (retirement)"):Idle?TEXT(" (idle)"):TEXT("")));
        }
        if(!CoreBuilding)
        {
            TMap<FString,double> Rates;for(const auto& Pair:Recipe->Inputs)Rates.Add(Pair.Key,Pair.Value/Recipe->Seconds);
            Add(TEXT("Production"),TEXT("Nominal input rates"),Amounts(Rates)+TEXT(" / s"));
            Rates.Empty();for(const auto& Pair:Recipe->Outputs)Rates.Add(Pair.Key,Pair.Value/Recipe->Seconds);
            Add(TEXT("Production"),TEXT("Nominal output rates"),Amounts(Rates)+TEXT(" / s"));
        }
        else
        {
            Add(TEXT("Production"),TEXT("Population target"),FString::FromInt(FMath::Max(TotalJobs,int32(Number(TEXT("minimum_population"))))));
            Add(TEXT("Production"),TEXT("Surplus retirement"),TEXT("1 robot / ")+N(Number(TEXT("robot_retire_seconds")))+TEXT(" s; no material refund"));
        }
    }
    else
    {
        Add(TEXT("Production"),TEXT("Resource inputs"),TEXT("None (0)"));
        Add(TEXT("Production"),TEXT("Resource output"),D.ExtractResource.IsEmpty()?TEXT("None (0)"):Name(D.ExtractResource));
        Add(TEXT("Production"),TEXT("Nominal extraction"),N(D.ExtractRate)+TEXT(" units / simulation s"));
        const bool Full=B&&Occupied(*B)>=D.StorageCapacity-UE_DOUBLE_SMALL_NUMBER;
        Add(TEXT("Production"),TEXT("Operating extraction"),N(Full?0:D.ExtractRate*Fraction)+TEXT(" units / s"));
        Add(TEXT("Production"),TEXT("Extraction limit"),D.ExtractResource.IsEmpty()?TEXT("Not an extractor"):TEXT("One matching deposit; output stops when local storage is full"));
    }

    Add(TEXT("Resources"),TEXT("Local storage"),B?N(Occupied(*B))+TEXT(" / ")+N(D.StorageCapacity)+TEXT(" units"):N(D.StorageCapacity)+TEXT(" units capacity"));
    TSet<FString> StockIds;
    if(B&&B->IsConstructing)for(const auto& Pair:D.Cost)StockIds.Add(Pair.Key);
    if(CoreBuilding||D.Role==TEXT("storage"))for(const auto& Pair:Resources)StockIds.Add(Pair.Key);
    StockIds.Add(TextRule(TEXT("repair_resource")));
    if(!D.ExtractResource.IsEmpty())StockIds.Add(D.ExtractResource);
    if(Recipe){for(const auto& Pair:Recipe->Inputs)StockIds.Add(Pair.Key);for(const auto& Pair:Recipe->Outputs)StockIds.Add(Pair.Key);}
    if(B){for(const auto& Pair:B->Inventory)StockIds.Add(Pair.Key);for(const auto& Pair:B->ConstructionMaterials)StockIds.Add(Pair.Key);for(const auto& C:Couriers)if(C.TargetId==B->Id)StockIds.Add(C.Resource);}
    TArray<FString> StockKeys=StockIds.Array();StockKeys.Sort();
    for(const FString& Key:StockKeys)
    {
        FString Value=B?N(B->Inventory.FindRef(Key))+TEXT(" units"):TEXT("0 units (no instance)");
        if(B&&B->IsConstructing)Value+=TEXT("; construction ")+N(B->ConstructionMaterials.FindRef(Key))+TEXT(" / ")+N(D.Cost.FindRef(Key));
        const double Inbound=B?Incoming(B->Id,Key):0;if(Inbound>0)Value+=TEXT(" (+")+N(Inbound)+TEXT(" inbound)");
        Rows.Add({TEXT("Resources"),Name(Key),Value});
    }
    Add(TEXT("Resources"),TEXT("Transport"),TEXT("Physical couriers; ")+N(Number(TEXT("courier_capacity")))+TEXT(" units per cargo batch"));

    Add(TEXT("Maintenance"),TEXT("Repair material"),Name(TextRule(TEXT("repair_resource"))));
    Add(TEXT("Maintenance"),TEXT("Maximum repair rate"),N(Number(TEXT("repair_health_per_second")))+TEXT(" health / s from local stock"));
    Add(TEXT("Maintenance"),TEXT("Repair conversion"),N(Number(TEXT("repair_health_per_unit")))+TEXT(" health / material unit"));
    Add(TEXT("Maintenance"),TEXT("Local repair buffer"),N(Number(TEXT("repair_buffer_units")))+TEXT(" units"));
    Add(TEXT("Maintenance"),TEXT("Repair operation"),TEXT("Automatic after construction, including while disabled; destroyed buildings do not repair"));
    Add(TEXT("Maintenance"),TEXT("Robot upkeep"),N(Number(TEXT("upkeep_per_robot")))+TEXT(" ")+Name(TextRule(TEXT("upkeep_resource")))+TEXT(" / robot every ")+N(Number(TEXT("upkeep_interval")))+TEXT(" s"));
    Add(TEXT("Maintenance"),TEXT("Robot support capacity"),FString::FromInt(D.RobotSupportCapacity)+TEXT(" robots when completed and operating"));
    Add(TEXT("Maintenance"),TEXT("Upkeep payment"),TEXT("Local core/service-bay stock, delivered by physical couriers"));
    if(B&&D.RobotSupportCapacity>0){Add(TEXT("Maintenance"),TEXT("Robots supported here"),FString::FromInt(B->SupportedRobots));Add(TEXT("Maintenance"),TEXT("Service supply"),B->IsConstructing?TEXT("Under construction"):B->MaintenanceSupplied?TEXT("Supplied at last maintenance interval"):TEXT("Maintenance shortage"));}
    Add(TEXT("Maintenance"),TEXT("Shortage efficiency"),N(Number(TEXT("upkeep_shortage_efficiency"))*100)+TEXT("% until a supplied upkeep interval"));
    if(CoreBuilding)
    {
        Add(TEXT("Maintenance"),TEXT("Shuttle capacity"),N(Number(TEXT("shuttle_capacity")))+TEXT(" units; preloaded cargo only"));
        Add(TEXT("Maintenance"),TEXT("Shuttle cargo"),B?Amounts(ShuttleCargo):TEXT("Scenario-defined; no cargo-loading command yet"));
    }
    return Rows;
}
