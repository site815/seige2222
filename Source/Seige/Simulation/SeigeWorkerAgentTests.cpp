#include "SeigeSimulation.h"
#include "SeigeRenderInterpolation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
constexpr EAutomationTestFlags WorkerAgentFlags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
FString WorkerAgentRules(){return FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules"));}
FString WorkerAgentSave(const TCHAR* Name){return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/WorkerAgents"),FString(Name)+TEXT(".json"));}
bool WorkerAgentDeploy(FSeigeSimulation& S,FString& Error)
{if(!S.Initialize(WorkerAgentRules(),Error,false,false))return false;for(int32 I=0;I<400&&S.Buildings[0].IsConstructing;++I)S.Tick(10);if(S.Buildings[0].IsConstructing){Error=TEXT("Finite landed workers could not physically deploy the carried core kit");return false;}return true;}
void WorkerAgentStep(FSeigeSimulation& S,double Step){S.Time+=Step;S.Workers.Tick(S,Step);S.Calendar.Advance(Step);}
bool WorkerAgentSite(FSeigeSimulation& S,int32& Id,FString& Error)
{for(int32 Ring=0;Ring<4;++Ring)for(int32 Side=-1;Side<=1;Side+=2){const FVector2D P=S.Buildings[0].Position+FVector2D(1300+Ring*350,Side*350);if(S.PlaceBuilding(TEXT("sensor"),P,Error)){Id=S.Buildings.Last().Id;return true;}}return false;}
bool WorkerAgentRead(const FString& P,TSharedPtr<FJsonObject>& O){FString T;return FFileHelper::LoadFileToString(T,*P)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T),O);}
bool WorkerAgentWrite(const FString& P,const TSharedPtr<FJsonObject>& O){FString T;return FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&T))&&FFileHelper::SaveStringToFile(T,*P);}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerLandingLedgerTest,"Seige.Workers.LandingFiniteBodiesAndMotion",WorkerAgentFlags)
bool FSeigeWorkerLandingLedgerTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;if(!S.Initialize(WorkerAgentRules(),Error,false,false)){AddError(Error);return false;}
    const int32 Initial=S.Population;TSet<FString> Ids;for(const auto& W:S.Workers.Bodies){Ids.Add(W.Id);TestFalse(TEXT("Starting body is physically concealed aboard the shuttle"),W.Outdoor);TestEqual(TEXT("Starting position is the actual landed shuttle"),W.Position,S.Buildings[0].Position);}
    TestEqual(TEXT("Initial manifest exactly equals population"),Ids.Num(),Initial);TestTrue(TEXT("Deployment materials start aboard rather than magically at the worksite"),S.Workers.DeploymentStock.Num()>0&&S.Buildings[0].ConstructionMaterials.IsEmpty());
    S.Tick(S.Workers.DeploymentDescentSeconds()*.5);for(const auto& W:S.Workers.Bodies)TestFalse(TEXT("No workers exit a descending shuttle"),W.Outdoor);
    double LastTime=S.Time;TMap<FString,FVector2D> Previous;for(const auto& W:S.Workers.Bodies)Previous.Add(W.Id,W.Position);
    bool SawCarry=false,SawConstruction=false,SawExit=false;const double Step=S.FixedStepSeconds();
    while(S.Time<2000&&S.Buildings[0].IsConstructing)
    {
        FSeigeRenderSnapshot Snapshot;Snapshot.Capture(S);S.Tick(Step);
        int32 Active=0;TSet<FString> DeliveryBodies;for(const auto& W:S.Workers.Bodies)
        {
            if(W.State==TEXT("active"))++Active;
            if(const auto* P=Previous.Find(W.Id))
            {const double Distance=FVector2D::Distance(*P,W.Position);if(Distance>S.WalkingSpeed()*(S.Time-LastTime)+1.e-6){AddError(TEXT("A worker teleported or exceeded configured walking speed"));return false;}if(Distance>1.e-6){TestTrue(TEXT("Render interpolation follows the same persistent body"),Snapshot.Worker(W,.5).Equals((*P+W.Position)*.5,1.e-6));}}
            SawExit|=W.Outdoor;SawConstruction|=W.Activity==TEXT("build");Previous.Add(W.Id,W.Position);
        }
        for(const auto& C:S.Couriers){if(DeliveryBodies.Contains(C.WorkerId)){AddError(TEXT("One body was assigned to multiple courier tasks"));return false;}DeliveryBodies.Add(C.WorkerId);SawCarry|=C.Amount>0;}
        if(Active!=S.Population||S.Couriers.Num()>Active){AddError(TEXT("Delivery bodies exceed the authoritative active population"));return false;}LastTime=S.Time;
    }
    TestFalse(TEXT("Finite starter workers and delivered kit complete command deployment"),S.Buildings[0].IsConstructing);TestTrue(TEXT("Observed actual hatch exit, hauling and worksite construction"),SawExit&&SawCarry&&SawConstruction);
    for(const auto& Id:Ids)TestNotNull(TEXT("Role changes retain every original manufactured body ID"),S.Workers.Find(Id));
    TestTrue(TEXT("Core paid installation conserves its carried bill"),FMath::IsNearlyEqual(S.InventoryMassKg(S.Buildings[0].InstalledMaterials),S.InventoryMassKg(S.ConstructionCost(S.Buildings[0])),1.e-6));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerHatchEvacuationTest,"Seige.Workers.ManualLaunchDuringHatchExit",WorkerAgentFlags)
bool FSeigeWorkerHatchEvacuationTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!S.Initialize(WorkerAgentRules(),Error,false,false)){AddError(Error);return false;}
    const int32 BodyCount=S.Workers.Bodies.Num();double LastDeparture=0;for(const auto& W:S.Workers.Bodies)LastDeparture=FMath::Max(LastDeparture,W.DepartureAt);
    const double Deadline=LastDeparture+FVector2D::Distance(S.Buildings[0].Position,S.BuildingAccessPoint(S.Buildings[0]))/S.WalkingSpeed()+2;
    bool StraddlesHatch=false;
    while(S.Time<Deadline&&!StraddlesHatch)
    {
        S.Tick(S.FixedStepSeconds());bool Inside=false,Outside=false;
        for(const auto& W:S.Workers.Bodies)if(W.State==TEXT("active")&&W.Activity==TEXT("exit")){Inside|=!W.Outdoor;Outside|=W.Outdoor;}
        StraddlesHatch=Inside&&Outside;
    }
    if(!TestTrue(TEXT("Ordinary staggered departure puts real exiting bodies on both sides of the hatch"),StraddlesHatch))return false;
    TMap<FString,FVector2D> Positions;TSet<FString> InsideIds,OutsideIds;
    for(const auto& W:S.Workers.Bodies){Positions.Add(W.Id,W.Position);if(W.Outdoor)OutsideIds.Add(W.Id);else InsideIds.Add(W.Id);}
    const double LaunchTime=S.Time;S.LaunchShuttle();
    TestTrue(TEXT("Manual launch succeeds without destroying the core"),S.Escaped&&!S.Failed);
    TestEqual(TEXT("Evacuation creates or deletes no worker identities"),S.Workers.Bodies.Num(),BodyCount);
    TestEqual(TEXT("Only workers physically outside remain in local population"),S.Population,OutsideIds.Num());
    for(const auto& W:S.Workers.Bodies)
    {
        TestEqual(TEXT("Workers still inside the open hatch evacuate; exterior workers remain behind"),W.State,InsideIds.Contains(W.Id)?FString(TEXT("evacuated")):FString(TEXT("active")));
        TestEqual(TEXT("Launch does not teleport any body across the hatch"),W.Position,Positions[W.Id]);
    }
    if(!S.Save(WorkerAgentSave(TEXT("hatch-launch")),Error)||!Loaded.Initialize(WorkerAgentRules(),Error,false,false)||!Loaded.Load(WorkerAgentSave(TEXT("hatch-launch")),Error)){AddError(Error);return false;}
    Loaded.Tick(10);TestEqual(TEXT("A restored evacuated scenario remains stopped"),Loaded.Time,LaunchTime);
    TestEqual(TEXT("Reload keeps the same local population after evacuation"),Loaded.Population,S.Population);
    for(const auto& W:S.Workers.Bodies)
    {
        const auto* Restored=Loaded.Workers.Find(W.Id);if(!TestNotNull(TEXT("Every original body identity survives the launch save"),Restored))return false;
        TestEqual(TEXT("Reload preserves each body's inside/outside evacuation outcome"),Restored->State,W.State);
        TestEqual(TEXT("Reload preserves each body's actual launch position"),Restored->Position,W.Position);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerPhysicalDeliveryTest,"Seige.Workers.PickupCapacityReturnAndAtomicSave",WorkerAgentFlags)
bool FSeigeWorkerPhysicalDeliveryTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!WorkerAgentDeploy(S,Error)){AddError(Error);return false;}int32 Site=0;if(!WorkerAgentSite(S,Site,Error)){AddError(Error);return false;}
    auto& Core=S.Buildings[0];const int CoreId=Core.Id;FString Resource;for(const auto& P:S.ConstructionCost(*S.FindBuilding(Site)))if(P.Value>0&&Core.Inventory.FindRef(P.Key)>0){Resource=P.Key;break;}if(Resource.IsEmpty()){AddError(TEXT("No genuine finite starter material available"));return false;}
    auto& Worker=S.Workers.Bodies[0];Worker.Activity=TEXT("idle");Worker.BuildingId=0;Worker.Route.Empty();Worker.NextWaypoint=0;Worker.Outdoor=true;Worker.Position=S.BuildingAccessPoint(Core)+FVector2D(0,150);const FString Identity=Worker.Id;S.Workers.RefreshMetrics(S);
    const double Before=Core.Inventory.FindRef(Resource),AvailableBefore=S.ConstructionAvailable(Resource);const double Quantity=FMath::Min(S.ConstructionCost(*S.FindBuilding(Site)).FindRef(Resource),S.Workers.HaulUnits(S,Resource));
    if(!S.Workers.Dispatch(S,CoreId,Site,0,Resource,Quantity,true)){AddError(TEXT("A real available worker must accept a payable delivery"));return false;}
    TestEqual(TEXT("Ordering a pickup reserves stock without debiting it"),S.FindBuilding(CoreId)->Inventory.FindRef(Resource),Before);TestEqual(TEXT("The assigned worker retains its existing position"),S.Workers.Find(Identity)->Position,S.BuildingAccessPoint(Core)+FVector2D(0,150));
    TestTrue(TEXT("A pending pickup cannot release its reserved material for a second build order"),FMath::IsNearlyEqual(S.ConstructionAvailable(Resource),AvailableBefore,1.e-8));
    TestEqual(TEXT("A pending pickup is not carried cargo"),S.Couriers.Last().Amount,0.);
    WorkerAgentStep(S,S.FixedStepSeconds());TestEqual(TEXT("Stock stays at source until physical arrival and loading"),S.FindBuilding(CoreId)->Inventory.FindRef(Resource),Before);
    FSeigeSimulation LostSource=S,LostSourceLoaded;const auto WaitingPosition=LostSource.Workers.Find(Identity)->Position;
    LostSource.OnBuildingDestroyed(CoreId);
    TestTrue(TEXT("Core loss releases an uncollected pickup without killing or moving its exterior worker"),LostSource.Workers.Find(Identity)->State==TEXT("active")&&LostSource.Workers.Find(Identity)->Position.Equals(WaitingPosition)&&LostSource.Workers.Find(Identity)->DeliveryId==0);
    TestFalse(TEXT("Destroyed stock cannot retain an outstanding pickup promise"),LostSource.Couriers.ContainsByPredicate([&](const auto& C){return C.SourceId==CoreId&&C.ReservedAmount>0;}));
    if(!LostSource.Save(WorkerAgentSave(TEXT("lost-pickup-source")),Error)||!LostSourceLoaded.Initialize(WorkerAgentRules(),Error,false,false)||!LostSourceLoaded.Load(WorkerAgentSave(TEXT("lost-pickup-source")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Immediate escaped save preserves the surviving worker population"),LostSourceLoaded.Population,LostSource.Population);
    bool LoadedCargo=false;for(int32 I=0;I<10000&&!S.Couriers.IsEmpty();++I)
    {WorkerAgentStep(S,S.FixedStepSeconds());for(const auto& C:S.Couriers)if(C.Amount>0){LoadedCargo=true;TestTrue(TEXT("A haul obeys both mass and volume capacity"),C.Amount<=S.Workers.HaulUnits(S,C.Resource)+1.e-8);break;}if(LoadedCargo)break;}
    if(!TestTrue(TEXT("The existing body reaches source and picks up actual stock"),LoadedCargo))return false;
    TestTrue(TEXT("Pickup transitions preserve a valid route cursor before the next routing step"),S.Couriers[0].NextWaypoint<=S.Couriers[0].Route.Num()&&S.Workers.Find(Identity)->NextWaypoint<=S.Workers.Find(Identity)->Route.Num());
    const double Held=S.Couriers[0].Amount;TestTrue(TEXT("Pickup moves exactly once from source to body"),FMath::IsNearlyEqual(S.FindBuilding(CoreId)->Inventory.FindRef(Resource)+Held,Before,1.e-6));
    FSeigeSimulation LostLoadedSource=S,LostLoadedSourceRestored;const auto CarryPosition=LostLoadedSource.Workers.Find(Identity)->Position;
    LostLoadedSource.OnBuildingDestroyed(CoreId);
    TestTrue(TEXT("Source loss preserves cargo already physically carried by its surviving worker"),LostLoadedSource.Couriers.Num()==1&&LostLoadedSource.Couriers[0].Amount==Held&&LostLoadedSource.Workers.Find(Identity)->Position.Equals(CarryPosition));
    if(!LostLoadedSource.Save(WorkerAgentSave(TEXT("lost-loaded-source")),Error)||!LostLoadedSourceRestored.Initialize(WorkerAgentRules(),Error,false,false)||!LostLoadedSourceRestored.Load(WorkerAgentSave(TEXT("lost-loaded-source")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Immediate core-loss roundtrip does not duplicate or discard carried material"),LostLoadedSourceRestored.Couriers[0].Amount,Held);
    if(!S.Save(WorkerAgentSave(TEXT("carrying")),Error)||!Loaded.Initialize(WorkerAgentRules(),Error,false,false)||!Loaded.Load(WorkerAgentSave(TEXT("carrying")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("Save preserves the same carrier identity"),Loaded.Couriers[0].WorkerId,Identity);
    for(int32 I=0;I<10000&&(!S.Couriers.IsEmpty()||!Loaded.Couriers.IsEmpty());++I){WorkerAgentStep(S,S.FixedStepSeconds());WorkerAgentStep(Loaded,Loaded.FixedStepSeconds());}
    TestTrue(TEXT("Both original and restored carriers physically unload"),S.Couriers.IsEmpty()&&Loaded.Couriers.IsEmpty());TestTrue(TEXT("Real local delivery conserves all material"),FMath::IsNearlyEqual(S.FindBuilding(CoreId)->Inventory.FindRef(Resource)+S.FindBuilding(Site)->ConstructionMaterials.FindRef(Resource),Before,1.e-6));
    TestTrue(TEXT("Reloaded delivery neither duplicates nor loses local cargo"),FMath::IsNearlyEqual(Loaded.FindBuilding(Site)->ConstructionMaterials.FindRef(Resource),Held,1.e-6));TestNotNull(TEXT("Successful delivery does not delete the worker"),S.Workers.Find(Identity));
    TSharedPtr<FJsonObject> Bad;if(!WorkerAgentRead(WorkerAgentSave(TEXT("carrying")),Bad))return false;auto Manifest=Bad->GetObjectField(TEXT("workers"));const auto Bodies=Manifest->GetArrayField(TEXT("bodies"));Bodies[1]->AsObject()->SetStringField(TEXT("id"),Bodies[0]->AsObject()->GetStringField(TEXT("id")));WorkerAgentWrite(WorkerAgentSave(TEXT("duplicate")),Bad);
    const double T=Loaded.Time;const auto Position=Loaded.Workers.Find(Identity)->Position;TestFalse(TEXT("Duplicate body identities are rejected"),Loaded.Load(WorkerAgentSave(TEXT("duplicate")),Error));TestEqual(TEXT("Rejected load preserves the active world's clock"),Loaded.Time,T);TestEqual(TEXT("Rejected load preserves actual body positions"),Loaded.Workers.Find(Identity)->Position,Position);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerCrewReservationTest,"Seige.Workers.SpareOperatorsReachPaidConstruction",WorkerAgentFlags)
bool FSeigeWorkerCrewReservationTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;if(!WorkerAgentDeploy(S,Error)){AddError(Error);return false;}S.Tick(30);
    const int32 CoreId=S.Buildings[0].Id;const int32 Count=S.Workers.Bodies.Num();
    TSet<FString> Operators;for(const auto& W:S.Workers.Bodies)if(W.BuildingId==CoreId&&W.Activity==TEXT("operate"))Operators.Add(W.Id);
    if(!TestTrue(TEXT("Completed core has real spare operators to lend"),Operators.Num()>2))return false;
    const FVector2D Plot=S.Buildings[0].Position+FVector2D(1300,350);auto& Occupant=S.Workers.Bodies[0];const FVector2D BeforePosition=Occupant.Position;
    if(!TestTrue(TEXT("Worker occupancy probe begins with a genuinely legal plot"),S.CanPlaceBuilding(TEXT("sensor"),Plot,Error)))return false;
    Occupant.Position=Plot;TestFalse(TEXT("An outdoor operator cannot be enclosed by a new plot"),S.CanPlaceBuilding(TEXT("sensor"),Plot,Error));
    Occupant.Position=Plot+FVector2D(S.BuildingDefs[TEXT("sensor")].ReservedFootprint+S.Workers.BodyRadiusMeters()/S.MetersPerWorldUnit()+.01,0);
    TestTrue(TEXT("Occupancy uses the actual body radius rather than its future route"),S.CanPlaceBuilding(TEXT("sensor"),Plot,Error));Occupant.Position=BeforePosition;
    int32 Site=0;if(!WorkerAgentSite(S,Site,Error)){AddError(Error);return false;}
    // Supply this scheduling fixture from the actual finite kit, preserving the
    // complete bill. The separate pickup test covers physical hauling.
    auto* B=S.FindBuilding(Site);for(const auto& P:S.ConstructionCost(*B)){if(S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)<P.Value){AddError(TEXT("Finite kit cannot pay scheduling fixture"));return false;}S.FindBuilding(CoreId)->Inventory.FindOrAdd(P.Key)-=P.Value;B->ConstructionMaterials.Add(P.Key,P.Value);}
    WorkerAgentStep(S,S.FixedStepSeconds());int32 Travelling=0,AvailableHaulers=0;bool Borrowed=false;
    for(const auto& W:S.Workers.Bodies){if(W.BuildingId==Site&&W.Activity==TEXT("to_build")){++Travelling;Borrowed|=Operators.Contains(W.Id);}if(W.State==TEXT("active")&&W.Activity==TEXT("idle"))++AvailableHaulers;}
    TestEqual(TEXT("A fully supplied site receives its authored finite crew"),Travelling,S.Definition(*B)->ConstructionWorkers);TestTrue(TEXT("Actual spare core operators walk to the new worksite"),Borrowed);TestTrue(TEXT("Construction preserves an available logistics worker"),AvailableHaulers>0);
    TestEqual(TEXT("Scheduling creates no additional bodies"),S.Workers.Bodies.Num(),Count);
    for(int32 I=0;I<90&&S.FindBuilding(Site)->IsConstructing;++I)S.Tick(10);
    TestFalse(TEXT("A paid site cannot deadlock behind surplus core operation"),S.FindBuilding(Site)->IsConstructing);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerBusyLogisticsReserveTest,"Seige.Workers.CompletedHaulJoinsPaidRoad",WorkerAgentFlags)
bool FSeigeWorkerBusyLogisticsReserveTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;if(!WorkerAgentDeploy(S,Error)){AddError(Error);return false;}
    // Manufacture the seventh real body through the ordinary paid core recipe.
    for(int32 I=0;I<300&&S.Population<7;++I)S.Tick(10);
    if(!TestTrue(TEXT("The finite kit supplies one core operator and six real hauling bodies"),S.Population>=7)||!TestTrue(TEXT("Deployment has no outstanding deliveries"),S.Couriers.IsEmpty()))return false;
    const int32 CoreId=S.Buildings[0].Id,BodyCount=S.Workers.Bodies.Num();const FVector2D Port=S.BuildingAccessPoint(S.Buildings[0]);
    if(!S.PlaceRoad(Port,Port+FVector2D(400,0),Error)){AddError(Error);return false;}
    const int32 RoadId=S.Roads.Last().Id;const FVector2D Worksite=S.RoadAccessPoint(S.Roads.Last());
    const auto Bill=S.RoadCost(S.Roads.Last().A,S.Roads.Last().B,S.Roads.Last().TargetTier);
    FString Resource;for(const auto& P:Bill)if(P.Value>0&&S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)>=P.Value){Resource=P.Key;break;}
    if(Resource.IsEmpty()){AddError(TEXT("The road requires genuine material from the remaining finite kit"));return false;}
    const double Quantity=Bill[Resource]/6,Before=S.FindBuilding(CoreId)->Inventory.FindRef(Resource);
    if(!TestTrue(TEXT("Each physically carried load fits the configured capacity"),Quantity<=S.Workers.HaulUnits(S,Resource)))return false;
    TArray<FString> Haulers;
    for(auto& W:S.Workers.Bodies)if(W.State==TEXT("active"))
    {
        W.BuildingId=W.RoadId=W.DeliveryId=W.ContainerId=0;W.ContainerKind.Empty();W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;W.Outdoor=true;W.Position=Port;W.Activity=TEXT("idle");
        if(Haulers.Num()<6)Haulers.Add(W.Id);else{W.BuildingId=CoreId;W.Activity=TEXT("operate");}
    }
    S.Workers.RefreshMetrics(S);
    TSharedPtr<FJsonObject> Policy;if(!WorkerAgentRead(FPaths::Combine(WorkerAgentRules(),TEXT("workers.json")),Policy))return false;
    // Isolate the real arrival boundary: six existing bodies carry portions of
    // the accepted bill, with one unloading and five still en route. Inventory
    // is debited once, rather than granting cargo or completing the road.
    for(int32 I=0;I<Haulers.Num();++I)
    {
        if(!S.Workers.Dispatch(S,CoreId,0,RoadId,Resource,Quantity,true)){AddError(TEXT("An available existing worker must take the reserved road load"));return false;}
        auto& C=S.Couriers.Last();auto* W=S.Workers.Find(C.WorkerId);
        S.FindBuilding(CoreId)->Inventory.FindOrAdd(Resource)-=Quantity;C.Amount=Quantity;C.ReservedAmount=0;C.Phase=I==0?TEXT("unloading"):TEXT("carrying");
        C.PhaseSeconds=I==0?Policy->GetNumberField(TEXT("unloading_seconds")):0;W->Position=I==0?Worksite:Port;C.Position=W->Position;
    }
    const FString Finished=S.Couriers[0].WorkerId;const FVector2D Arrival=S.Workers.Find(Finished)->Position;
    WorkerAgentStep(S,S.FixedStepSeconds());const auto* Assigned=S.Workers.Find(Finished);
    TestEqual(TEXT("Only the arrived load finishes; five real haulers remain at work"),S.Couriers.Num(),5);
    TestTrue(TEXT("The finished body fills the paid road vacancy while logistics is already staffed"),Assigned->RoadId==RoadId&&Assigned->Activity==TEXT("to_road"));
    TestTrue(TEXT("Changing task retains the actual unloading position"),Assigned->Position.Equals(Arrival,1.e-8));
    TestEqual(TEXT("Assigning construction creates no additional worker bodies"),S.Workers.Bodies.Num(),BodyCount);
    double Carried=0;for(const auto& C:S.Couriers)Carried+=C.Amount;
    TestTrue(TEXT("Task handover preserves all physically delivered and carried road material"),FMath::IsNearlyEqual(S.FindBuilding(CoreId)->Inventory.FindRef(Resource)+S.FindRoad(RoadId)->ConstructionMaterials.FindRef(Resource)+Carried,Before,1.e-8));
    WorkerAgentStep(S,S.FixedStepSeconds());const double Moved=FVector2D::Distance(Arrival,S.Workers.Find(Finished)->Position);
    TestTrue(TEXT("The same body walks to its individual road workstation at physical speed"),Moved>0&&Moved<=S.WalkingSpeed()*S.FixedStepSeconds()+1.e-6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerCoreMinimumTest,"Seige.Workers.OnsiteCoreMinimum",WorkerAgentFlags)
bool FSeigeWorkerCoreMinimumTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S;if(!WorkerAgentDeploy(S,Error)){AddError(Error);return false;}S.Tick(30);
    int32 Site=0;if(!WorkerAgentSite(S,Site,Error)){AddError(Error);return false;}
    if(!TestTrue(TEXT("The finite deployed fixture has two existing workers"),S.Workers.Bodies.Num()>=2)||!TestTrue(TEXT("No delivery is in flight before the isolated staffing probe"),S.Couriers.IsEmpty()))return false;
    const int32 CoreId=S.Buildings[0].Id,BodyCount=S.Workers.Bodies.Num();const FVector2D Port=S.BuildingAccessPoint(S.Buildings[0]);
    // Isolate a real staffing handover: one body operates, one is walking to its
    // station, and the other existing bodies are still returning from work.
    // No bodies or materials are added; movement from this fixture stays physical.
    for(auto& W:S.Workers.Bodies)
    {W.State=TEXT("active");W.Activity=TEXT("return");W.BuildingId=W.RoadId=W.DeliveryId=W.ContainerId=0;W.ContainerKind.Empty();W.Outdoor=true;W.Position=Port+FVector2D(0,500);W.Route={Port};W.NextWaypoint=0;W.RouteRevision=0;W.RetryAt=0;}
    auto& Operator=S.Workers.Bodies[0];Operator.Activity=TEXT("operate");Operator.BuildingId=CoreId;Operator.StationSlot=0;Operator.Position=Port+FVector2D(0,20);Operator.Route.Empty();
    auto& Incoming=S.Workers.Bodies[1];Incoming.Activity=TEXT("to_job");Incoming.BuildingId=CoreId;Incoming.StationSlot=1;Incoming.Position=Port+FVector2D(0,300);Incoming.Route.Empty();
    const FString OperatorId=Operator.Id,IncomingId=Incoming.Id;const FVector2D OperatorPosition=Operator.Position,IncomingPosition=Incoming.Position;S.Workers.RefreshMetrics(S);
    TestEqual(TEXT("Only the physically arrived worker operates the core"),S.FindBuilding(CoreId)->Workers,1);
    const auto Cost=S.ConstructionCost(*S.FindBuilding(Site));FString Resource;for(const auto& P:Cost)if(P.Value>0&&S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)>0){Resource=P.Key;break;}
    if(Resource.IsEmpty()){AddError(TEXT("No genuine construction bill for the hauling probe"));return false;}
    TestFalse(TEXT("A pickup cannot borrow the last operator merely because a replacement is travelling"),S.Workers.Dispatch(S,CoreId,Site,0,Resource,FMath::Min(Cost[Resource],S.Workers.HaulUnits(S,Resource)),true));
    TestTrue(TEXT("Rejected hauling leaves the real operator at its station"),S.Workers.Find(OperatorId)->Activity==TEXT("operate")&&S.Workers.Find(OperatorId)->Position.Equals(OperatorPosition));
    WorkerAgentStep(S,S.FixedStepSeconds());
    TestEqual(TEXT("Construction demand also retains the on-site core operator"),S.FindBuilding(CoreId)->Workers,1);
    TestEqual(TEXT("Builders wait for an actually available body"),S.FindBuilding(Site)->Builders,0);
    const auto* Moving=S.Workers.Find(IncomingId);const double Moved=FVector2D::Distance(IncomingPosition,Moving->Position);
    TestTrue(TEXT("The replacement keeps walking rather than being counted as arrived"),Moving->Activity==TEXT("to_job")&&Moved>0&&Moved<=S.WalkingSpeed()*S.FixedStepSeconds()+1.e-6);
    for(int32 I=0;I<400&&S.FindBuilding(Site)->Builders==0;++I)WorkerAgentStep(S,S.FixedStepSeconds());
    const auto* Assigned=S.Workers.Find(IncomingId);
    TestTrue(TEXT("Only after physical arrival may the spare operator join construction"),Assigned->BuildingId==Site&&Assigned->Activity==TEXT("to_build")&&S.FindBuilding(CoreId)->Workers>=1);
    TestEqual(TEXT("The handover creates no additional worker bodies"),S.Workers.Bodies.Num(),BodyCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerTransferAdmissionTest,"Seige.Workers.SelfRelocationReservesLiveSupport",WorkerAgentFlags)
bool FSeigeWorkerTransferAdmissionTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!WorkerAgentDeploy(S,Error)||!S.SetWorkerSurplusTarget(2,Error)){AddError(Error);return false;}
    for(int32 I=0;I<500&&S.InactiveWorkerCount()<2;++I)S.Tick(10);
    if(!TestTrue(TEXT("Paid assembly supplies two real stored bodies"),S.InactiveWorkerCount()>=2))return false;
    const int32 CoreId=S.Buildings[0].Id,BodyCount=S.Workers.Bodies.Num();FString Resource;
    for(const auto& P:S.Resources)if(P.Value.Unit==TEXT("workers")){Resource=P.Key;break;}
    if(Resource.IsEmpty()){AddError(TEXT("No configured whole-worker cargo"));return false;}
    int32 PortId=0;for(int32 Side:{-1,1})if(S.PlaceBuilding(TEXT("trading_port"),S.Buildings[0].Position+FVector2D(1500,Side*600),Error)){PortId=S.Buildings.Last().Id;break;}
    if(!PortId){AddError(Error);return false;}
    // Install this isolated storage destination from the real remaining kit;
    // the physical construction and delivery pipelines have separate coverage.
    auto* Port=S.FindBuilding(PortId);const auto Bill=S.ConstructionCost(*Port);
    for(const auto& P:Bill){if(S.FindBuilding(CoreId)->Inventory.FindRef(P.Key)<P.Value){AddError(TEXT("Finite kit cannot pay the destination fixture"));return false;}S.FindBuilding(CoreId)->Inventory.FindOrAdd(P.Key)-=P.Value;}
    Port->InstalledMaterials=Bill;Port->ConstructionProgress=1;Port->IsConstructing=false;Port->Builders=Port->BuildersOnSite=Port->TravellingBuilders=0;
    S.Workers.RefreshMetrics(S);const int32 BeforePopulation=S.Population;
    if(!TestEqual(TEXT("The authored core has exactly one free active support slot"),S.RobotSupportCapacity-BeforePopulation,1))return false;
    const double BeforeStock=S.FindBuilding(CoreId)->Inventory.FindRef(Resource);TArray<FString> Stored;
    for(const auto& W:S.Workers.Bodies)if(W.State==TEXT("stored")&&W.ContainerId==CoreId)Stored.Add(W.Id);
    if(!TestTrue(TEXT("Both paid stored identities are available at the source"),Stored.Num()>=2))return false;
    const FVector2D WaitingPosition=S.Workers.Find(Stored[1])->Position;
    TestTrue(TEXT("First intact body may occupy the final support slot"),S.Workers.Dispatch(S,CoreId,PortId,0,Resource,1,false));
    TestEqual(TEXT("Activation updates population before another command or save"),S.Population,BeforePopulation+1);
    TestFalse(TEXT("A second transfer in the same logistics pass cannot reuse that slot"),S.Workers.Dispatch(S,CoreId,PortId,0,Resource,1,false));
    TestEqual(TEXT("Only one self-relocation task exists"),S.Couriers.Num(),1);
    TestEqual(TEXT("Rejected activation leaves the other real body stored"),S.Workers.Find(Stored[1])->State,FString(TEXT("stored")));
    TestEqual(TEXT("Rejected activation does not move the waiting body"),S.Workers.Find(Stored[1])->Position,WaitingPosition);
    TestEqual(TEXT("Only the admitted body leaves packed inventory"),S.FindBuilding(CoreId)->Inventory.FindRef(Resource),BeforeStock-1);
    TestEqual(TEXT("Admission never manufactures an extra body"),S.Workers.Bodies.Num(),BodyCount);
    if(!S.Save(WorkerAgentSave(TEXT("immediate-self-transfer")),Error)||!Loaded.Initialize(WorkerAgentRules(),Error,false,false)||!Loaded.Load(WorkerAgentSave(TEXT("immediate-self-transfer")),Error)){AddError(Error);return false;}
    TestEqual(TEXT("An immediate save preserves the admitted active population"),Loaded.Population,S.Population);
    TestEqual(TEXT("An immediate save retains the same self-relocating body"),Loaded.Couriers[0].WorkerId,S.Couriers[0].WorkerId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeWorkerIdentityLifecycleTest,"Seige.Workers.ManufactureRecycleAndSelfRelocation",WorkerAgentFlags)
bool FSeigeWorkerIdentityLifecycleTest::RunTest(const FString&)
{
    FString Error;FSeigeSimulation S,Loaded;if(!WorkerAgentDeploy(S,Error)){AddError(Error);return false;}const int32 OriginalBodies=S.Workers.Bodies.Num(),CoreId=S.Buildings[0].Id;
    FString BodyResource;for(const auto& P:S.Resources)if(P.Value.Unit==TEXT("workers")){BodyResource=P.Key;break;}if(BodyResource.IsEmpty()){AddError(TEXT("No configured discrete worker cargo"));return false;}
    if(!S.SetWorkerSurplusTarget(2,Error)){AddError(Error);return false;}
    // Real paid core batches fill one operating vacancy and then the requested
    // inactive reserve. No fixture writes inventories or assembly progress.
    for(int32 I=0;I<500&&S.InactiveWorkerCount()<2;++I)S.Tick(10);
    if(!TestTrue(TEXT("Finite starter inputs manufacture the requested real stored bodies"),S.InactiveWorkerCount()>=2))return false;
    TestTrue(TEXT("Manufacturing allocates new persistent IDs only when a paid batch completes"),S.Workers.Bodies.Num()>OriginalBodies);
    const int32 ActiveBefore=S.Population;TestFalse(TEXT("Reserve target protects stored identities from recycling"),S.DisassembleWorkers(CoreId,1,Error));
    S.SetWorkerSurplusTarget(1,Error);FString Recycled;for(const auto& W:S.Workers.Bodies)if(W.State==TEXT("stored")&&W.ContainerId==CoreId){Recycled=W.Id;break;}
    if(!S.DisassembleWorkers(CoreId,1,Error)){AddError(Error);return false;}const double EnergyBefore=S.Energy.ConsumedKWh;S.Tick(S.FixedStepSeconds());
    TestEqual(TEXT("Recycling commits the original stored identity"),S.Workers.Find(Recycled)->State,FString(TEXT("disassembling")));
    TestEqual(TEXT("No active worker is deleted by surplus recycling"),S.Population,ActiveBefore);
    if(!S.Save(WorkerAgentSave(TEXT("recycle")),Error)||!Loaded.Initialize(WorkerAgentRules(),Error,false,false)||!Loaded.Load(WorkerAgentSave(TEXT("recycle")),Error)){AddError(Error);return false;}
    for(int32 I=0;I<100&&S.Workers.Find(Recycled)->State!=TEXT("disassembled");++I){S.Tick(1);Loaded.Tick(1);}
    TestEqual(TEXT("The paid identity becomes terminal once, rather than being reused"),S.Workers.Find(Recycled)->State,FString(TEXT("disassembled")));TestEqual(TEXT("Restored paid recycling reaches the same identity state"),Loaded.Workers.Find(Recycled)->State,S.Workers.Find(Recycled)->State);
    TestTrue(TEXT("Recycling records the unrecovered physical mass"),S.Workers.RecyclingWasteKg>0&&FMath::IsNearlyEqual(S.Workers.RecyclingWasteKg,Loaded.Workers.RecyclingWasteKg,1.e-8));TestTrue(TEXT("Recycling consumes its externally configured energy"),S.Energy.ConsumedKWh-EnergyBefore>=S.DisassemblyEnergyKWh()-1.e-8);
    S.SetWorkerSurplusTarget(0,Error);
    int32 PortId=0;for(int32 Side=-1;Side<=1;Side+=2)if(S.PlaceBuilding(TEXT("trading_port"),S.Buildings[0].Position+FVector2D(1500,Side*600),Error)){PortId=S.Buildings.Last().Id;break;}if(!PortId){AddError(Error);return false;}
    // This isolated transport fixture installs a port from the genuine remaining
    // paid starter bill; construction travel itself is covered by the first test.
    auto* Port=S.FindBuilding(PortId);const auto Cost=S.ConstructionCost(*Port);for(const auto& P:Cost)S.FindBuilding(CoreId)->Inventory.FindOrAdd(P.Key)-=P.Value;Port->InstalledMaterials=Cost;Port->ConstructionProgress=1;Port->IsConstructing=false;Port->Builders=Port->BuildersOnSite=Port->TravellingBuilders=0;
    if(!S.SetPortWorkerTarget(PortId,1,Error)){AddError(Error);return false;}S.Workers.RefreshMetrics(S);FString Relocated;for(const auto& W:S.Workers.Bodies)if(W.State==TEXT("stored")&&W.ContainerId==CoreId){Relocated=W.Id;break;}
    if(!TestFalse(TEXT("A real stored body remains for transfer"),Relocated.IsEmpty()))return false;
    TestEqual(TEXT("An intact worker cannot be hauled by a lower-capacity worker"),S.Workers.HaulUnits(S,BodyResource),0.);
    if(!S.Workers.Dispatch(S,CoreId,PortId,0,BodyResource,1,false)){AddError(TEXT("Intact worker must self-relocate within active support capacity"));return false;}
    const auto* Task=S.Couriers.FindByPredicate([&](const auto& C){return C.WorkerId==Relocated;});TestTrue(TEXT("Self-relocation uses its own body, not an extra hauler or oversized load"),Task&&Task->SelfTransfer&&Task->Amount==0&&S.Workers.Find(Relocated)->State==TEXT("active"));
    FSeigeSimulation Destroyed=S,DestroyedLoaded;Destroyed.OnBuildingDestroyed(CoreId);
    TestTrue(TEXT("Destruction during indoor reactivation removes the actual carrier and its task together"),Destroyed.Workers.Find(Relocated)->State==TEXT("destroyed")&&Destroyed.Workers.Find(Relocated)->DeliveryId==0&&!Destroyed.Couriers.ContainsByPredicate([&](const auto& C){return C.WorkerId==Relocated;}));
    TestEqual(TEXT("Destroyed storage preserves earlier recycling history"),Destroyed.Workers.Find(Recycled)->State,FString(TEXT("disassembled")));
    if(!Destroyed.Save(WorkerAgentSave(TEXT("destroyed-reactivation")),Error)||!DestroyedLoaded.Initialize(WorkerAgentRules(),Error,false,false)||!DestroyedLoaded.Load(WorkerAgentSave(TEXT("destroyed-reactivation")),Error)){AddError(Error);return false;}
    FVector2D Previous=S.Workers.Find(Relocated)->Position;bool Moved=false;
    for(int32 I=0;I<20000&&S.Workers.Find(Relocated)->State!=TEXT("stored");++I)
    {WorkerAgentStep(S,S.FixedStepSeconds());const auto P=S.Workers.Find(Relocated)->Position;const double D=FVector2D::Distance(P,Previous);if(D>S.WalkingSpeed()*S.FixedStepSeconds()+1.e-6){AddError(TEXT("Stored body teleported during self-relocation"));return false;}Moved|=D>1.e-6;Previous=P;}
    const auto* Arrived=S.Workers.Find(Relocated);TestTrue(TEXT("Same ID physically arrives and occupies the destination berth"),Moved&&Arrived->State==TEXT("stored")&&Arrived->ContainerId==PortId);TestEqual(TEXT("Port reserve prevents immediate reactivation despite an open job"),S.FindBuilding(PortId)->Inventory.FindRef(BodyResource),1.);
    return true;
}
#endif
