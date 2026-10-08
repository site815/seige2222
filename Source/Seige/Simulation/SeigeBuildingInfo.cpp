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
        for(const FString& Key:Keys)if(Values[Key]>0)Parts.Add(N(Values[Key])+TEXT(" ")+Resources[Key].Unit+TEXT(" ")+Name(Key));
        return Parts.IsEmpty()?FString(TEXT("None (0)")):FString::Join(Parts,TEXT(", "));
    };
    auto Metres=[&](double Units){return N(Units*CentimetersPerUnit/100)+TEXT(" m");};
    const double Fraction=B?WorkFraction(*B):1;
    const double WeaponFraction=B&&B->IsConstructing&&D.DeploymentDefense&&B->Enabled&&B->Health>0?Energy.Fraction(B->Id):Fraction;
    Add(TEXT("Overview"),TEXT("Building"),D.Name);
    Add(TEXT("Overview"),TEXT("Role"),D.Role);
    Add(TEXT("Overview"),TEXT("Level"),FString::FromInt(D.Level)+TEXT(" / 3"));
    if(!D.NextUpgrade.IsEmpty())Add(TEXT("Overview"),TEXT("Next-level upgrade"),BuildingDefs[D.NextUpgrade].Name+TEXT(": ")+Amounts(D.UpgradeCost));
    Add(TEXT("Overview"),TEXT("Status"),B?B->Status:TEXT("Blueprint"));
    Add(TEXT("Overview"),TEXT("Health"),B?N(B->Health)+TEXT(" / ")+N(D.Health):N(D.Health)+TEXT(" maximum"));
    Add(TEXT("Overview"),TEXT("Jobs"),B?FString::Printf(TEXT("%d / %d staffed"),B->Workers,D.Jobs):FString::FromInt(D.Jobs));
    Add(TEXT("Overview"),TEXT("Automatic staffing priority"),FString::FromInt(D.StaffingPriority)+TEXT(" (lower first; includes construction)"));
    Add(TEXT("Overview"),TEXT("Operating efficiency"),N(Fraction*100)+TEXT("%")+(B?TEXT(""):TEXT(" at full staffing and upkeep")));
    Add(TEXT("Overview"),TEXT("Current building width"),Metres(D.Footprint*2));
    Add(TEXT("Overview"),TEXT("Reserved plot width"),Metres(D.ReservedFootprint*2));
    Add(TEXT("Overview"),TEXT("Sensor range"),Metres(D.SensorRange));
    FString Activity=D.WorkerActivity;if(!Activity.IsEmpty())Activity[0]=FChar::ToUpper(Activity[0]);
    Add(TEXT("Overview"),TEXT("Worker activity"),Activity);
    Add(TEXT("Overview"),TEXT("Construction cost"),Amounts(B&&B->IsConstructing?ConstructionCost(*B):D.Cost));
    Add(TEXT("Overview"),TEXT("Construction payment"),D.Role==TEXT("core")?TEXT("Deployment kit physically carried by the landing shuttle"):TEXT("Reserved stock; physically delivered to the exterior site"));
    Add(TEXT("Overview"),TEXT("Construction duration"),N(B?ConstructionSeconds(*B):D.ConstructionSeconds)+TEXT(" s with ")+FString::FromInt(B?RequiredBuilders(*B):D.ConstructionWorkers)+TEXT(" builders at full efficiency"));
    Add(TEXT("Resources"),TEXT("Stock location"),D.InventoryPresentation==TEXT("outdoor")?TEXT("Outdoor stockyard"):TEXT("Inside the building"));
    if(B&&B->IsConstructing)Add(TEXT("Resources"),TEXT("Delivered materials"),TEXT("Uninstalled material at the site; workers incorporate it progressively."));
    if(B&&B->IsConstructing){Add(TEXT("Overview"),TEXT("Construction phase"),ConstructionStage(*B));Add(TEXT("Overview"),TEXT("Builders on site"),FString::FromInt(B->BuildersOnSite));Add(TEXT("Overview"),TEXT("Travelling builders"),FString::FromInt(B->TravellingBuilders));}
    if(B){Add(TEXT("Overview"),TEXT("Construction progress"),N(B->ConstructionProgress*100)+TEXT("%"));Add(TEXT("Overview"),TEXT("Assigned builders"),FString::FromInt(B->Builders));}

    const auto* Platform=Combat.BuildingPlatforms.Find(D.Id);
    const auto* WeaponState=B?Combat.BuildingState.Find(B->Id):nullptr;
    const TArray<FString> Loadout=WeaponState?WeaponState->Weapons:Platform?Platform->Weapons:TArray<FString>();
    double DPS=0,ShotEnergy=0;int32 Area=0;double Mass=0;TMap<FString,double> Ammunition;
    Add(TEXT("Weapons"),TEXT("Weapon"),Loadout.IsEmpty()?TEXT("Unarmed (0)"):FString::FromInt(Loadout.Num())+TEXT(" equipped modules"));
    for(int32 I=0;I<Loadout.Num();++I)if(const auto* W=Combat.Weapons.Find(Loadout[I]))
    {
        DPS+=W->Damage/W->ReloadSeconds;ShotEnergy+=W->EnergyKWh;Area+=W->MountPoints;Mass+=W->MassKg;
        if(!W->Ammo.IsEmpty())Ammunition.FindOrAdd(W->Ammo)+=W->AmmoPerShot;
        Rows.Add({TEXT("Weapons"),FString::Printf(TEXT("Mount %d - %s"),I+1,*W->Name),N(W->Damage)+TEXT(" damage / ")+N(W->ReloadSeconds)+TEXT(" s = ")+N(W->Damage/W->ReloadSeconds)+TEXT(" DPS; ")+N(W->RangeMeters)+TEXT(" m; ")+N(W->EnergyKWh)+TEXT(" kWh/shot")});
    }
    Add(TEXT("Weapons"),TEXT("Nominal DPS"),N(DPS)+TEXT(" health/s before misses and protection"));
    Add(TEXT("Weapons"),TEXT("Operating DPS"),N(DPS*WeaponFraction)+TEXT(" theoretical; requires energy/ammunition"));
    if(B)Add(TEXT("Weapons"),TEXT("Fire control"),Combat.BuildingFireStatus(*this,B->Id));
    Add(TEXT("Weapons"),TEXT("Mount area used / capacity"),FString::FromInt(Area)+TEXT(" / ")+FString::FromInt(Platform?Platform->MountPoints:0)+TEXT(" small units (1 large = 4 medium = 16 small)"));
    Add(TEXT("Weapons"),TEXT("Weapon mass used / capacity"),N(Mass)+TEXT(" / ")+N(Platform?Platform->MaxWeaponMassKg:0)+TEXT(" kg"));
    Add(TEXT("Weapons"),TEXT("Energy per full volley"),N(ShotEnergy)+TEXT(" kWh"));
    Add(TEXT("Weapons"),TEXT("Ammunition per full volley"),Amounts(Ammunition));
    Add(TEXT("Weapons"),TEXT("Shield / armor"),N(WeaponState?WeaponState->Shield:Platform?Platform->Shield:0)+TEXT(" / ")+N(WeaponState?WeaponState->Armor:Platform?Platform->Armor:0)+TEXT(" HP"));
    if(Loadout.IsEmpty()){Add(TEXT("Weapons"),TEXT("Damage per shot"),TEXT("0"));Add(TEXT("Weapons"),TEXT("Reload time"),TEXT("0 s"));Add(TEXT("Weapons"),TEXT("Attack range"),TEXT("0 m"));}

    const auto* E=Energy.Definition(D.Id);const auto Grid=B?Energy.Info(*this,B->Id):FSeigeEnergyInfo();
    Add(TEXT("Power"),TEXT("Power consumption"),N(E?E->IdleKW:0)+TEXT(" kW"));
    Add(TEXT("Power"),E&&E->GenerationSource==TEXT("solar")?TEXT("Rated generation (daylight peak)"):TEXT("Rated generation"),N(E?E->GenerationKW:0)+TEXT(" kW"));
    if(E&&E->GenerationSource==TEXT("solar"))Add(TEXT("Power"),TEXT("Solar availability now"),N(Calendar.Sample().SolarFactor*100)+TEXT("% of peak before staffing; 0 at night"));
    Add(TEXT("Power"),TEXT("Battery charge / capacity"),N(B?B->BatteryEnergyKWh:0)+TEXT(" / ")+N(E?E->BatteryCapacityKWh:0)+TEXT(" kWh"));
    Add(TEXT("Power"),TEXT("Power system"),B?(Grid.Connected?TEXT("Connected road grid"):E&&!E->RequiresRoadGrid?TEXT("Self-contained bootstrap generation"):TEXT("Disconnected; connect a powered road")):TEXT("Shares power and batteries within its connected road network"));
    Add(TEXT("Power"),TEXT("Current grid generation / demand"),N(Grid.GenerationKW)+TEXT(" / ")+N(Grid.DemandKW)+TEXT(" kW"));
    Add(TEXT("Power"),TEXT("Grid battery charge / capacity"),N(Grid.StoredKWh)+TEXT(" / ")+N(Grid.CapacityKWh)+TEXT(" kWh"));
    Add(TEXT("Power"),TEXT("Worker electricity"),N(Energy.WorkerKW)+TEXT(" kW per worker"));
    if(E&&!E->FuelResource.IsEmpty())Add(TEXT("Power"),TEXT("Generator fuel"),Name(E->FuelResource)+TEXT("; ")+N(E->FuelUnitsPerKWh)+TEXT(" units / kWh"));
    if(const auto* Port=Trade.Definition(D.Id)){Add(TEXT("Production"),TEXT("Trade level"),FString::FromInt(Port->Level));Add(TEXT("Production"),TEXT("Shipment capacity"),N(Port->CapacityKg)+TEXT(" kg"));Add(TEXT("Production"),TEXT("Shipment duration"),N(Port->ShipmentSeconds)+TEXT(" s"));Add(TEXT("Production"),TEXT("Shipment energy"),N(Port->EnergyKWh)+TEXT(" kWh"));Add(TEXT("Production"),TEXT("Credit account"),FString::Printf(TEXT("%.6f credits"),Credits));if(B&&!B->Shipment.Resource.IsEmpty())Add(TEXT("Production"),TEXT("Current shipment"),(B->Shipment.Buy?TEXT("Import "):TEXT("Export "))+Name(B->Shipment.Resource)+TEXT(" ")+N(B->Shipment.Quantity)+TEXT("; ")+N(B->Shipment.Progress*100)+TEXT("%"));if(B)Add(TEXT("Production"),TEXT("Worker export reserve target"),FString::FromInt(B->WorkerExportTarget));}


    const bool CoreBuilding=D.Role==TEXT("core");
    FString RecipeId=B?(B->ProductionCommitted?B->CommittedRecipe:B->SelectedRecipe):D.Recipe;
    if(RecipeId.IsEmpty()&&!D.AllowedRecipes.IsEmpty())RecipeId=D.AllowedRecipes[0];
    const FSeigeRecipeDef* Recipe=Recipes.Find(RecipeId);
    if(Recipe)
    {
        TMap<FString,double> Inputs=Recipe->Inputs;for(auto& P:Inputs)P.Value*=D.RecipeInputMultiplier;
        const double Duration=Recipe->Seconds*D.RecipeTimeMultiplier;
        Add(TEXT("Production"),TEXT("Selected recipe"),RecipeId);
        Add(TEXT("Production"),TEXT("Inputs per cycle"),Amounts(Inputs));
        Add(TEXT("Production"),TEXT("Batch electricity"),N(Recipe->EnergyKWh*D.RecipeEnergyMultiplier)+TEXT(" kWh reserved before production"));
        Add(TEXT("Production"),TEXT("Outputs per cycle"),Recipe->WorkerOutput>0?FString::FromInt(Recipe->WorkerOutput)+TEXT(" stored worker(s)"):Amounts(Recipe->Outputs));
        Add(TEXT("Production"),TEXT("Base cycle time"),N(Duration)+TEXT(" simulation s"));
        Add(TEXT("Production"),TEXT("Operating cycle time"),Fraction>0?N(Duration/Fraction)+TEXT(" s; requires local inputs and output space"):TEXT("Paused until operational"));
        if(B)Add(TEXT("Production"),TEXT("Cycle progress"),N(B->Progress*100)+TEXT("%"));
        if(CoreBuilding)Add(TEXT("Production"),TEXT("Assembly policy"),TEXT("Automatic vacancy cover when no worker factory operates; selected paid batch finishes first"));
        TMap<FString,double> Rates;for(const auto& Pair:Inputs)Rates.Add(Pair.Key,Pair.Value/Duration);
        Add(TEXT("Production"),TEXT("Nominal input rates"),Amounts(Rates)+TEXT(" / s"));
        Rates.Empty();for(const auto& Pair:Recipe->Outputs)Rates.Add(Pair.Key,Pair.Value/Duration);
        Add(TEXT("Production"),TEXT("Nominal output rates"),Amounts(Rates)+TEXT(" / s"));
    }
    else
    {
        Add(TEXT("Production"),TEXT("Resource inputs"),TEXT("None (0)"));
        const FString Resource=B?ExtractionResource(*B):FString();
        const double Rate=B?ExtractionRate(*B):0;
        const FString Unit=Resources.Contains(Resource)?Resources[Resource].Unit:TEXT("units");
        Add(TEXT("Production"),TEXT("Resource output"),!Resource.IsEmpty()?Name(Resource):D.ExtractionRates.IsEmpty()?TEXT("None (0)"):TEXT("Selected automatically from the local deposit"));
        if(!B&&!D.ExtractionRates.IsEmpty())Add(TEXT("Production"),TEXT("Deposit rates"),Amounts(D.ExtractionRates)+TEXT(" / simulation s"));
        else Add(TEXT("Production"),TEXT("Nominal extraction"),N(Rate)+TEXT(" ")+Unit+TEXT(" / simulation s"));
        const bool Full=B&&Occupied(*B)>=D.StorageCapacity-UE_DOUBLE_SMALL_NUMBER;
        Add(TEXT("Production"),TEXT("Operating extraction"),N(Full?0:Rate*Fraction)+TEXT(" ")+Unit+TEXT(" / s"));
        Add(TEXT("Production"),TEXT("Extraction limit"),D.ExtractionRates.IsEmpty()?TEXT("Not a mine"):TEXT("One mine per deposit; output stops when local storage is full"));
    }

    if(D.StoresInactiveWorkers)
    {
        Add(TEXT("Production"),TEXT("Inactive workers here"),FString::FromInt(B?FMath::RoundToInt(B->Inventory.FindRef(TextRule(TEXT("inactive_worker_resource")))):0));
        Add(TEXT("Production"),TEXT("Colony worker stock target"),FString::FromInt(WorkerSurplusTarget));
        Add(TEXT("Production"),TEXT("Disassembly"),TEXT("Stored surplus only; ")+N(DisassemblyEnergyKWh())+TEXT(" kWh per worker; ")+Amounts(DisassemblyOutputs()));
        if(B)Add(TEXT("Production"),TEXT("Disassembly queue / progress"),FString::FromInt(B->DisassemblyQueued)+TEXT(" / ")+N(B->DisassemblyProgress*100)+TEXT("%"));
    }
    Add(TEXT("Resources"),TEXT("Local storage"),B?N(Occupied(*B))+TEXT(" / ")+N(D.StorageCapacity)+TEXT(" L"):N(D.StorageCapacity)+TEXT(" L capacity"));
    TSet<FString> StockIds;
    if(B&&B->IsConstructing)for(const auto& Pair:ConstructionCost(*B))StockIds.Add(Pair.Key);
    if(CoreBuilding||D.Role==TEXT("storage"))for(const auto& Pair:Resources)StockIds.Add(Pair.Key);
    StockIds.Add(TextRule(TEXT("repair_resource")));
    if(B){const FString Resource=ExtractionResource(*B);if(!Resource.IsEmpty())StockIds.Add(Resource);}
    if(Recipe){for(const auto& Pair:Recipe->Inputs)StockIds.Add(Pair.Key);for(const auto& Pair:Recipe->Outputs)StockIds.Add(Pair.Key);}
    if(B){for(const auto& Pair:B->Inventory)StockIds.Add(Pair.Key);for(const auto& Pair:B->ConstructionMaterials)StockIds.Add(Pair.Key);for(const auto& C:Couriers)if(C.TargetId==B->Id)StockIds.Add(C.Resource);}
    TArray<FString> StockKeys=StockIds.Array();StockKeys.Sort();
    for(const FString& Key:StockKeys)
    {
        FString Value=B?N(B->Inventory.FindRef(Key))+TEXT(" ")+Resources[Key].Unit:TEXT("0 ")+Resources[Key].Unit+TEXT(" (no instance)");
        if(B&&B->IsConstructing)Value+=TEXT("; construction ")+N(B->ConstructionMaterials.FindRef(Key))+TEXT(" uninstalled; ")+N(B->InstalledMaterials.FindRef(Key))+TEXT(" / ")+N(ConstructionCost(*B).FindRef(Key))+TEXT(" installed");
        const double Inbound=B?Incoming(B->Id,Key):0;if(Inbound>0)Value+=TEXT(" (+")+N(Inbound)+TEXT(" inbound)");
        Rows.Add({TEXT("Resources"),Name(Key),Value});
    }
    Add(TEXT("Resources"),TEXT("Transport"),TEXT("Physical couriers; ")+N(Number(TEXT("courier_capacity")))+TEXT(" kg per cargo batch"));

    Add(TEXT("Maintenance"),TEXT("Repair material"),Name(TextRule(TEXT("repair_resource"))));
    Add(TEXT("Maintenance"),TEXT("Maximum repair rate"),N(Number(TEXT("repair_health_per_second")))+TEXT(" health / s from local stock"));
    Add(TEXT("Maintenance"),TEXT("Repair conversion"),N(Number(TEXT("repair_health_per_unit")))+TEXT(" health / material unit"));
    Add(TEXT("Maintenance"),TEXT("Local repair buffer"),N(Number(TEXT("repair_buffer_units")))+TEXT(" units"));
    Add(TEXT("Maintenance"),TEXT("Repair operation"),TEXT("Automatic after construction, including while disabled; destroyed buildings do not repair"));
    Add(TEXT("Maintenance"),TEXT("Worker upkeep"),N(Number(TEXT("upkeep_per_robot")))+TEXT(" ")+Name(TextRule(TEXT("upkeep_resource")))+TEXT(" / worker every ")+N(Number(TEXT("upkeep_interval")))+TEXT(" s"));
    Add(TEXT("Maintenance"),TEXT("Worker support capacity"),FString::FromInt(D.RobotSupportCapacity)+TEXT(" workers when completed and operating"));
    Add(TEXT("Maintenance"),TEXT("Upkeep payment"),TEXT("Local core/service-bay stock, delivered by physical couriers"));
    if(B&&D.RobotSupportCapacity>0){Add(TEXT("Maintenance"),TEXT("Workers supported here"),FString::FromInt(B->SupportedRobots));Add(TEXT("Maintenance"),TEXT("Service supply"),B->IsConstructing?TEXT("Under construction"):B->MaintenanceSupplied?TEXT("Supplied at last maintenance interval"):TEXT("Maintenance shortage"));}
    Add(TEXT("Maintenance"),TEXT("Shortage efficiency"),N(Number(TEXT("upkeep_shortage_efficiency"))*100)+TEXT("% until a supplied upkeep interval"));
    if(CoreBuilding)
    {
        Add(TEXT("Maintenance"),TEXT("Shuttle capacity"),N(Number(TEXT("shuttle_capacity")))+TEXT(" units; preloaded cargo only"));
        Add(TEXT("Maintenance"),TEXT("Shuttle cargo"),B?Amounts(ShuttleCargo):TEXT("Scenario-defined; no cargo-loading command yet"));
    }
    return Rows;
}
