#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"

bool FSeigeSimulation::LoadWorkforce(FString& Error)
{
    FString Resource;
    if(!Policy->TryGetStringField(TEXT("inactive_worker_resource"),Resource)||!Resources.Contains(Resource)||!Resources[Resource].Discrete){Error=TEXT("Inactive workers require a discrete physical cargo definition");return false;}
    for(const TCHAR* Key:{TEXT("inactive_worker_litres"),TEXT("worker_store_seconds"),TEXT("worker_reactivate_seconds"),TEXT("worker_disassemble_seconds"),TEXT("worker_disassembly_kwh")})
    {double V=0;if(!Policy->TryGetNumberField(Key,V)||!FMath::IsFinite(V)||V<=0){Error=FString(TEXT("Invalid workforce policy: "))+Key;return false;}}
    if(Number(TEXT("inactive_worker_litres"))!=Resources[Resource].LitresPerUnit){Error=TEXT("Worker storage must match physical cargo volume");return false;}
    for(const TCHAR* Key:{TEXT("auto_disassemble_parts_shortage"),TEXT("auto_disassemble_storage_full")}){bool Flag=false;if(!Policy->TryGetBoolField(Key,Flag)){Error=TEXT("Missing automatic disassembly selector");return false;}}
    for(const TCHAR* Key:{TEXT("default_inactive_worker_target"),TEXT("default_port_worker_reserve_target")}){double V=0;if(!Policy->TryGetNumberField(Key,V)||!FMath::IsFinite(V)||V<0||V>100000||V!=FMath::FloorToDouble(V)){Error=TEXT("Invalid worker reserve target");return false;}}
    WorkerSurplusTarget=int32(Number(TEXT("default_inactive_worker_target")));
    const TSharedPtr<FJsonObject>* Returns=nullptr;if(!Policy->TryGetObjectField(TEXT("worker_disassembly_outputs"),Returns)||!Returns||!Returns->IsValid()){Error=TEXT("Missing disassembly output bill");return false;}
    double Mass=0;for(const auto& P:(*Returns)->Values){const FString Id(P.Key);double V=0;if(!Resources.Contains(Id)||Id==Resource||!P.Value->TryGetNumber(V)||!FMath::IsFinite(V)||V<0||(Resources[Id].Discrete&&V!=FMath::FloorToDouble(V))){Error=TEXT("Invalid worker disassembly output");return false;}Mass+=V*Resources[Id].UnitMassKg;}
    if(Mass<=0||Mass>Resources[Resource].UnitMassKg+1.e-8){Error=TEXT("Disassembly cannot create physical mass");return false;}
    for(const auto& P:Recipes)if(P.Value.WorkerOutput>0&&InventoryMassKg(P.Value.Inputs)+1.e-8<P.Value.WorkerOutput*Resources[Resource].UnitMassKg){Error=TEXT("Worker assembly requires its full physical mass");return false;}
    for(const auto& P:BuildingDefs){const auto& D=P.Value;TArray<FString> Options=D.AllowedRecipes;if(!D.Recipe.IsEmpty())Options.AddUnique(D.Recipe);for(const auto& Id:Options){const auto& R=Recipes[Id];TMap<FString,double> Inputs=R.Inputs;for(auto& I:Inputs){I.Value*=D.RecipeInputMultiplier;if(Resources[I.Key].Discrete&&I.Value!=FMath::FloorToDouble(I.Value)){Error=TEXT("Production cannot consume fractional discrete cargo");return false;}}if(InventoryLitres(Inputs)>D.StorageCapacity||ProductionOutputLitres(R)>D.StorageCapacity||InventoryMassKg(Inputs)+1.e-8<InventoryMassKg(R.Outputs)+R.WorkerOutput*Resources[Resource].UnitMassKg){Error=TEXT("Building recipe cannot fit storage or conserve physical mass");return false;}}}
    return true;
}

TArray<FString> FSeigeSimulation::ProductionOptions(int32 Id) const
{const auto* B=FindBuilding(Id);if(!B)return {};auto Options=Definition(*B)->AllowedRecipes;if(Options.IsEmpty()&&!Definition(*B)->Recipe.IsEmpty())Options.Add(Definition(*B)->Recipe);return Options;}
TMap<FString,double> FSeigeSimulation::ProductionInputs(const FSeigeBuilding& B,const FString& Id) const
{TMap<FString,double> Inputs;if(const auto* R=Recipes.Find(Id)){Inputs=R->Inputs;for(auto& P:Inputs)P.Value*=Definition(B)->RecipeInputMultiplier;}return Inputs;}
double FSeigeSimulation::ProductionSeconds(const FSeigeBuilding& B,const FString& Id) const{return Recipes[Id].Seconds*Definition(B)->RecipeTimeMultiplier;}
double FSeigeSimulation::ProductionEnergy(const FSeigeBuilding& B,const FString& Id) const{return Recipes[Id].EnergyKWh*Definition(B)->RecipeEnergyMultiplier;}
bool FSeigeSimulation::CanCommitProduction(const FSeigeBuilding& B,const FString& Id) const
{
    const auto* Recipe=Recipes.Find(Id);
    if(!Recipe||B.Health<=0||!B.Enabled||B.IsConstructing||B.ProductionCommitted||!ProductionOptions(B.Id).Contains(Id))return false;
    const auto Inputs=ProductionInputs(B,Id);
    const double Reserved=FMath::Max(InventoryLitres(Inputs),ProductionOutputLitres(*Recipe));
    return HasSpendable(B,Inputs)&&Reserved<=StorageRoom(B)+InventoryLitres(Inputs)+1.e-8&&Energy.CanConsume(*this,B.Id,ProductionEnergy(B,Id));
}
double FSeigeSimulation::ProductionOutputLitres(const FSeigeRecipeDef& R) const{return InventoryLitres(R.Outputs)+R.WorkerOutput*Number(TEXT("inactive_worker_litres"));}
int32 FSeigeSimulation::InactiveWorkerCount() const{if(!Policy)return 0;int32 Count=FMath::RoundToInt(TotalStock(TextRule(TEXT("inactive_worker_resource"))));for(const auto& B:Buildings)if(B.Health>0&&B.DisassemblyCommitted)--Count;return Count;}
double FSeigeSimulation::DisassemblyEnergyKWh() const{return Policy?Number(TEXT("worker_disassembly_kwh")):0;}
int32 FSeigeSimulation::WorkerReserveTarget() const{int32 Target=WorkerSurplusTarget;for(const auto& B:Buildings)if(B.Health>0)Target+=B.WorkerExportTarget;return Target;}
int32 FSeigeSimulation::PendingWorkers() const{int32 Count=0;const FString Resource=TextRule(TEXT("inactive_worker_resource"));for(const auto& B:Buildings)if(B.Health>0){if(B.ProductionCommitted)if(const auto* R=Recipes.Find(B.CommittedRecipe))Count+=R->WorkerOutput;if(B.Shipment.Buy&&B.Shipment.Resource==Resource)Count+=FMath::RoundToInt(B.Shipment.Quantity);}return Count;}
int32 FSeigeSimulation::WorkersNeeded() const
{const int32 Active=FMath::Min(RobotSupportCapacity,FMath::Max(TotalJobs,int32(Number(TEXT("minimum_population")))));return FMath::Max(0,Active+WorkerReserveTarget()-Population-InactiveWorkerCount()-PendingWorkers());}
FString FSeigeSimulation::ActiveProductionRecipe(const FSeigeBuilding& B) const
{
    if(B.ProductionCommitted)return B.CommittedRecipe;
    const auto* D=Definition(B);if(!D)return {};
    FString Id=B.SelectedRecipe.IsEmpty()?D->Recipe:B.SelectedRecipe;
    if(Id.IsEmpty()&&!D->AllowedRecipes.IsEmpty())Id=D->AllowedRecipes[0];
    if(D->Role==TEXT("core"))
    {
        bool Factory=false;for(const auto& Other:Buildings)if(Other.Id!=B.Id&&Definition(Other)->Role==TEXT("worker_factory")&&WorkFraction(Other)>0){Factory=true;break;}
        if(WorkersNeeded()>0&&!Factory)
        {
            const FString WorkerRecipe=TextRule(TEXT("population_recipe"));const auto Inputs=ProductionInputs(B,WorkerRecipe);bool Ready=true;
            for(const auto& P:Inputs)if(B.Inventory.FindRef(P.Key)+1.e-8<P.Value){Ready=false;break;}
            const double Reserved=FMath::Max(InventoryLitres(Inputs),ProductionOutputLitres(Recipes[WorkerRecipe]));
            if(Reserved>StorageRoom(B)+InventoryLitres(Inputs)+1.e-8||!Energy.CanConsume(*this,B.Id,ProductionEnergy(B,WorkerRecipe)))Ready=false;
            // A blocked automatic worker batch must not prevent the selected
            // replicator recipe from making its missing parts or freeing storage.
            if(Ready)Id=WorkerRecipe;
        }
        else if(Recipes.Contains(Id)&&Recipes[Id].WorkerOutput>0&&Factory)return {};
    }
    if(const auto* R=Recipes.Find(Id)){if(R->WorkerOutput>0&&WorkersNeeded()<=0)return {};return Id;}return {};
}
bool FSeigeSimulation::SetProductionRecipe(int32 Id,const FString& Recipe,FString& Error)
{
    auto* B=FindBuilding(Id);if(Escaped||Failed||!B||B->Health<=0||B->IsConstructing||!ProductionOptions(Id).Contains(Recipe)){Error=TEXT("Select an available recipe on a completed production building");return false;}
    B->SelectedRecipe=Recipe;Error.Empty();AddEvent(B->ProductionCommitted?TEXT("Production selection saved; current paid batch finishes first"):TEXT("Production selection changed"));return true;
}
bool FSeigeSimulation::SetWorkerSurplusTarget(int32 Target,FString& Error)
{if(Escaped||Failed||Target<0||Target>100000){Error=TEXT("Invalid inactive-worker target");return false;}WorkerSurplusTarget=Target;Error.Empty();return true;}
bool FSeigeSimulation::SetPortWorkerTarget(int32 Id,int32 Target,FString& Error)
{auto* B=FindBuilding(Id);if(Escaped||Failed||!B||B->Health<=0||!Trade.Definition(B->DefId)||Target<0||Target>100000){Error=TEXT("Select a trading port and a valid whole-worker reserve target");return false;}B->WorkerExportTarget=Target;Error.Empty();return true;}

bool FSeigeSimulation::StepRecipe(FSeigeBuilding& B,double Seconds)
{
    const FString Id=ActiveProductionRecipe(B);const auto* Recipe=Recipes.Find(Id);if(!Recipe)return false;
    const auto& R=*Recipe;const auto Inputs=ProductionInputs(B,Id);
    if(!B.ProductionCommitted)
    {
        if(!HasSpendable(B,Inputs)){B.Status=TEXT("Waiting for delivered inputs");return true;}
        const double Reserved=FMath::Max(InventoryLitres(Inputs),ProductionOutputLitres(R));
        if(Reserved>StorageRoom(B)+InventoryLitres(Inputs)+1.e-8){B.Status=TEXT("Storage full; production waiting");return true;}
        if(!Energy.Consume(*this,B.Id,ProductionEnergy(B,Id))){B.Status=TEXT("Waiting for batch energy in connected batteries");return true;}
        for(const auto& P:Inputs)B.Inventory.FindOrAdd(P.Key)=FMath::Max(0.,B.Inventory.FindRef(P.Key)-P.Value);
        B.ProductionInputs=Inputs;B.ProductionReservedLitres=Reserved;B.ProductionCommitted=true;B.CommittedRecipe=Id;B.Progress=0;
    }
    B.Progress=FMath::Min(1.,B.Progress+Seconds*WorkFraction(B)/ProductionSeconds(B,Id));B.Status=R.WorkerOutput>0?TEXT("Assembling workers"):TEXT("Producing selected goods");
    if(B.Progress+UE_DOUBLE_SMALL_NUMBER>=1)
    {
        for(const auto& P:R.Outputs){B.Inventory.FindOrAdd(P.Key)+=P.Value;ProducedUnits.FindOrAdd(P.Key)+=P.Value;}
        if(R.WorkerOutput>0){B.Inventory.FindOrAdd(TextRule(TEXT("inactive_worker_resource")))+=R.WorkerOutput;Workers.NewStored(*this,B.Id,R.WorkerOutput);}
        B.ProductionInputs.Empty();B.ProductionReservedLitres=0;B.ProductionCommitted=false;B.CommittedRecipe.Empty();B.Progress=0;
    }
    return true;
}

TMap<FString,double> FSeigeSimulation::DisassemblyOutputs() const
{TMap<FString,double> Out;for(const auto& P:Policy->GetObjectField(TEXT("worker_disassembly_outputs"))->Values)Out.Add(FString(P.Key),P.Value->AsNumber());return Out;}
double FSeigeSimulation::DisassemblyAvailable(const FSeigeBuilding& B) const
{
    const FString Resource=TextRule(TEXT("inactive_worker_resource"));int32 Queued=0;for(const auto& Other:Buildings)if(Other.Health>0)Queued+=Other.DisassemblyQueued-(Other.DisassemblyCommitted?1:0);
    const int32 Jobs=FMath::Max(0,FMath::Min(RobotSupportCapacity,FMath::Max(TotalJobs,int32(Number(TEXT("minimum_population")))))-Population);
    const double Global=InactiveWorkerCount()-WorkerReserveTarget()-Jobs-Queued;
    const double Local=B.Inventory.FindRef(Resource)-(B.DisassemblyQueued-(B.DisassemblyCommitted?1:0))-B.WorkerExportTarget-Trade.Demand(*this,B.Id,Resource);
    return FMath::Max(0.,FMath::Min(Global,Local));
}
bool FSeigeSimulation::CanDisassembleWorkers(int32 Id,int32 Count,FString& Error) const
{const auto* B=FindBuilding(Id);if(Escaped||Failed||!B||B->Health<=0||B->IsConstructing||!Definition(*B)->StoresInactiveWorkers||Count<=0||DisassemblyAvailable(*B)<Count){Error=TEXT("Only stored surplus workers above colony and trade reserves can be disassembled");return false;}Error.Empty();return true;}
bool FSeigeSimulation::DisassembleWorkers(int32 Id,int32 Count,FString& Error)
{if(!CanDisassembleWorkers(Id,Count,Error))return false;FindBuilding(Id)->DisassemblyQueued+=Count;AddEvent(FString::Printf(TEXT("Surplus worker disassembly queued; each worker requires %g kWh"),DisassemblyEnergyKWh()));return true;}
void FSeigeSimulation::StepWorkerDisassembly(double Seconds)
{
    const FString Resource=TextRule(TEXT("inactive_worker_resource"));const auto Returns=DisassemblyOutputs();const double Volume=InventoryLitres(Returns);
    for(auto& B:Buildings)
    {
        if(B.Health<=0||B.IsConstructing||!Definition(B)->StoresInactiveWorkers||WorkFraction(B)<=0)continue;
        // No local body or existing job means no disassembly is possible. Avoid
        // scanning colony construction demand every fixed step in that case.
        // Active overflow has already committed a workstation in StepPopulation.
        if(!B.DisassemblyCommitted&&B.DisassemblyQueued==0&&B.Inventory.FindRef(Resource)<1)continue;
        bool Shortage=false;if(Policy->GetBoolField(TEXT("auto_disassemble_parts_shortage")))for(const auto& P:Returns)if(ConstructionAvailable(P.Key)+1.e-8<P.Value){Shortage=true;break;}
        const bool Full=Policy->GetBoolField(TEXT("auto_disassemble_storage_full"))&&StorageRoom(B)+1.e-8<Resources[Resource].LitresPerUnit;
        if(B.DisassemblyQueued==0&&(Shortage||Full)&&DisassemblyAvailable(B)>=1)B.DisassemblyQueued=1;
        if(B.DisassemblyQueued<=0)continue;
        if(!B.DisassemblyCommitted)
        {
            // Newly raised reserve targets and vacancies supersede unstarted recycling.
            const int32 Queued=B.DisassemblyQueued;B.DisassemblyQueued=0;const double Available=DisassemblyAvailable(B);B.DisassemblyQueued=FMath::Min(Queued,FMath::FloorToInt(Available));
            if(B.DisassemblyQueued<=0)continue;
            if(Workers.StoredAt(B.Id)<1||Volume>StorageRoom(B)+Resources[Resource].LitresPerUnit+1.e-8||!Energy.Consume(*this,B.Id,Number(TEXT("worker_disassembly_kwh"))))continue;
            Workers.BeginDisassembly(*this,B.Id);B.Inventory.FindOrAdd(Resource)-=1;B.DisassemblyCommitted=true;B.DisassemblyReservedLitres=Volume;B.DisassemblyProgress=0;
        }
        B.DisassemblyProgress=FMath::Min(1.,B.DisassemblyProgress+Seconds*WorkFraction(B)/Number(TEXT("worker_disassemble_seconds")));
        if(B.DisassemblyProgress+UE_DOUBLE_SMALL_NUMBER>=1){for(const auto& P:Returns)B.Inventory.FindOrAdd(P.Key)+=P.Value;Workers.FinishDisassembly(*this,B.Id);++WorkersDisassembled;--B.DisassemblyQueued;B.DisassemblyCommitted=false;B.DisassemblyProgress=B.DisassemblyReservedLitres=0;}
    }
}

void FSeigeSimulation::StepPopulation(double Seconds)
{
    auto* C=Core();if(!C||C->Health<=0||C->IsConstructing)return;
    const int32 Target=FMath::Max(TotalJobs,int32(Number(TEXT("minimum_population"))));const FString Worker=TextRule(TEXT("inactive_worker_resource"));
    // Bodies reactivate/store only through the physical worker lifecycle.
    WorkerReactivateClock=WorkerStoreClock=0;
    PopulationClock=0; // Kept in the serialized base clock set; batches own assembly time.
    UpkeepClock+=Seconds;
    while(UpkeepClock>=Number(TEXT("upkeep_interval")))
    {
        UpkeepClock-=Number(TEXT("upkeep_interval"));const FString Resource=TextRule(TEXT("upkeep_resource"));
        for(auto& B:Buildings)if(B.SupportedRobots>0){const double Need=B.SupportedRobots*Number(TEXT("upkeep_per_robot"));const bool Supplied=Spendable(B,Resource)+UE_DOUBLE_SMALL_NUMBER>=Need;if(Supplied)B.Inventory.FindOrAdd(Resource)=FMath::Max(0.,B.Inventory.FindRef(Resource)-Need);B.MaintenanceSupplied=Supplied;}UpdateSupport();
    }
}
