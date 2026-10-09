#include "SeigeScenarioAI.h"
#include "Dom/JsonObject.h"

namespace
{
bool PresetNumber(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& N,double Min,double Max,FString& Error)
{if(!O||!O->TryGetNumberField(Key,N)||!FMath::IsFinite(N)||N<Min||N>Max){Error=TEXT("Invalid established manifest field: ")+FString(Key);return false;}return true;}
bool PresetInteger(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,int32& N,int32 Min,int32 Max,FString& Error)
{double V=0;if(!PresetNumber(O,Key,V,Min,Max,Error)||V!=FMath::FloorToDouble(V)){Error=TEXT("Invalid established manifest integer: ")+FString(Key);return false;}N=int32(V);return true;}
bool OnRoad(FVector2D P,FVector2D A,FVector2D B)
{const auto D=B-A;const double T=FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),1.e-10);return T>=-1.e-8&&T<=1+1.e-8&&FVector2D::Distance(P,A+D*FMath::Clamp(T,0.,1.))<.01;}
}

bool FSeigeScenarioAI::LoadEstablishedPreset(const FSeigeSimulation& S,const TSharedPtr<FJsonObject>& O,FString& Error)
{
    FString Kind,Equipment,Fleet;
    if(!O->TryGetStringField(TEXT("kind"),Kind)||Kind!=TEXT("established_colony")||
       !O->TryGetStringField(TEXT("equipment"),Equipment)||Equipment!=TEXT("definition_defaults")||
       !O->TryGetStringField(TEXT("fleet"),Fleet)||Fleet!=TEXT("scenario_guard_manifest")||
       !O->TryGetStringField(TEXT("road_tier"),EstablishedRoadTier)||EstablishedRoadTier!=S.InitialRoadTier())
    {Error=TEXT("Unknown established scenario kind, equipment, fleet or road tier");return false;}
    if(!PresetNumber(O,TEXT("age_seconds"),EstablishedAge,60,31536000,Error)||
       !PresetNumber(O,TEXT("credits"),EstablishedCredits,0,1000000,Error)||
       !PresetInteger(O,TEXT("idle_workers"),EstablishedIdle,0,1000,Error)||
       !PresetInteger(O,TEXT("layout_rotations"),EstablishedRotations,1,4,Error))return false;
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    if(!O->TryGetArrayField(TEXT("buildings"),Rows)||!Rows||Rows->IsEmpty()||Rows->Num()>128){Error=TEXT("Established manifest needs 1..128 explicit buildings");return false;}
    EstablishedBuildings.Empty();int32 MineCount=0,Operators=EstablishedIdle,Support=0;
    for(int32 I=0;I<Rows->Num();++I)
    {
        const TSharedPtr<FJsonObject>* Row=nullptr;FSeigeEstablishedBuilding B;
        if(!(*Rows)[I]->TryGetObject(Row)||!Row||!(*Row)->TryGetStringField(TEXT("definition"),B.Definition)||!S.BuildingDefs.Contains(B.Definition)||
           !(*Row)->TryGetStringField(TEXT("anchor"),B.Anchor)||(B.Anchor!=TEXT("core")&&B.Anchor!=TEXT("deposit")))
        {Error=TEXT("Invalid established building definition or anchor");return false;}
        const auto& D=S.BuildingDefs[B.Definition];
        // Rows after the core are buildable blueprints or upgraded levels of one.
        if((I==0&&(B.Definition!=S.CoreDefinition||B.Anchor!=TEXT("core")))||(I>0&&(!S.BuildMenu.Contains(S.BaseBlueprint(B.Definition))||D.Role==TEXT("core"))))
        {Error=TEXT("Established manifest must start with exactly one scenario command core");return false;}
        if((D.Role==TEXT("extractor"))!=(B.Anchor==TEXT("deposit"))){Error=TEXT("Established Extraction Mine must bind the actual deposit");return false;}
        if(B.Anchor==TEXT("deposit"))++MineCount;
        const TArray<TSharedPtr<FJsonValue>>* Offset=nullptr;
        if(!(*Row)->TryGetArrayField(TEXT("offset_meters"),Offset)||Offset->Num()!=2||!(*Offset)[0]->TryGetNumber(B.OffsetMeters.X)||!(*Offset)[1]->TryGetNumber(B.OffsetMeters.Y)||
           !FMath::IsFinite(B.OffsetMeters.X)||!FMath::IsFinite(B.OffsetMeters.Y)||B.OffsetMeters.GetAbsMax()>S.WorldHalfSize*S.MetersPerWorldUnit()||
           ((I==0||B.Anchor==TEXT("deposit"))&&!B.OffsetMeters.IsNearlyZero()))
        {Error=TEXT("Invalid established offset_meters");return false;}
        if(!PresetInteger(*Row,TEXT("operators"),B.Operators,D.Jobs,D.Jobs,Error)||!PresetNumber(*Row,TEXT("battery_kwh"),B.BatteryKWh,0,S.Energy.Definition(B.Definition)->BatteryCapacityKWh,Error))return false;
        if(!(*Row)->TryGetStringField(TEXT("recipe"),B.Recipe)||(!B.Recipe.IsEmpty()&&B.Recipe!=D.Recipe&&!D.AllowedRecipes.Contains(B.Recipe)))
        {Error=TEXT("Established recipe is not available in this building");return false;}
        const TSharedPtr<FJsonObject>* Items=nullptr;
        if(!(*Row)->TryGetObjectField(TEXT("inventory"),Items)||!Items){Error=TEXT("Established building requires explicit inventory");return false;}
        for(const auto& Pair:(*Items)->Values)
        {
            const FString Id(Pair.Key);double Amount=0;const auto* R=S.Resources.Find(Id);
            if(!R||!Pair.Value->TryGetNumber(Amount)||!FMath::IsFinite(Amount)||Amount<0||Amount>1.e9||(R->Discrete&&Amount!=FMath::FloorToDouble(Amount))||(Id==TEXT("stored_workers")&&!D.StoresInactiveWorkers))
            {Error=TEXT("Invalid established inventory item: ")+Id;return false;}B.Inventory.Add(Id,Amount);
        }
        if(S.InventoryLitres(B.Inventory)>D.StorageCapacity+1.e-8){Error=TEXT("Established inventory exceeds building storage capacity: ")+B.Definition;return false;}
        if(B.Anchor==TEXT("core"))for(const auto& Other:EstablishedBuildings)if(Other.Anchor==TEXT("core"))
        {
            const double Separation=(D.ReservedFootprint+S.BuildingDefs[Other.Definition].ReservedFootprint+S.Number(TEXT("minimum_build_spacing")))*S.MetersPerWorldUnit();
            if(FMath::Abs(B.OffsetMeters.X-Other.OffsetMeters.X)<Separation&&FMath::Abs(B.OffsetMeters.Y-Other.OffsetMeters.Y)<Separation)
            {Error=TEXT("Established reserved plots overlap");return false;}
        }
        Operators+=B.Operators;Support+=D.RobotSupportCapacity;EstablishedBuildings.Add(MoveTemp(B));
    }
    if(MineCount!=1||Operators>Support){Error=TEXT("Established manifest needs one bound mine and sufficient worker support");return false;}
    FSeigeSimulation JobsProbe=S;JobsProbe.Buildings.Empty();
    for(const auto& Entry:EstablishedBuildings){FSeigeBuilding B;B.DefId=Entry.Definition;B.Health=1;JobsProbe.Buildings.Add(B);}
    if(EstablishedIdle<S.Workers.LogisticsJobs(JobsProbe)){Error=TEXT("Established idle workers do not cover actual logistics jobs");return false;}
    Error.Empty();return true;
}

bool FSeigeScenarioAI::TryEstablishedLayout(FSeigeSimulation& S,int32 DepositId,FVector2D CorePosition,double Rotation,FString& Error)
{
    // This function is only called on an uncommitted, freshly initialized scenario.
    // Authored starting structures are not live construction orders or inventory grants.
    if(S.Time!=0||S.Buildings.Num()!=1||!S.Roads.IsEmpty()||!S.Couriers.IsEmpty()){Error=TEXT("Established initialization requires a fresh candidate");return false;}
    if(!S.SetInitialCorePosition(CorePosition,Error))return false;
    const auto* Node=S.Nodes.FindByPredicate([&](const auto& N){return N.Id==DepositId;});if(!Node)return false;
    const FVector2D DepositPosition=Node->Position;const double Cos=FMath::Cos(Rotation),Sin=FMath::Sin(Rotation);
    TMap<int32,int32> Operators;
    for(int32 I=0;I<EstablishedBuildings.Num();++I)
    {
        const auto& Entry=EstablishedBuildings[I];const auto& D=S.BuildingDefs[Entry.Definition];
        const FVector2D Offset(Entry.OffsetMeters.X*Cos-Entry.OffsetMeters.Y*Sin,Entry.OffsetMeters.X*Sin+Entry.OffsetMeters.Y*Cos);
        const FVector2D Position=Entry.Anchor==TEXT("deposit")?DepositPosition:CorePosition+Offset/S.MetersPerWorldUnit();
        if(I>0&&!S.CanPlaceBuildingGeometry(Entry.Definition,Position,Error))return false;
        if(I>0)for(const auto& N:S.Nodes)if(N.Id!=DepositId||Entry.Anchor!=TEXT("deposit"))
            if(FVector2D::Distance(N.Position,Position)<D.ReservedFootprint+NodeClearance){Error=TEXT("Established plot obstructs a deposit");return false;}
        FSeigeBuilding B;B.Id=I==0?S.Buildings[0].Id:S.NextId++;B.DefId=Entry.Definition;B.Position=Position;B.Health=D.Health;
        B.Status=TEXT("Established scenario installation");B.Inventory=Entry.Inventory;B.InstalledMaterials=S.ConstructionCost(B);B.PreviousLevelMaterials=S.PreviousLevelBill(B.DefId);B.SelectedRecipe=Entry.Recipe;
        B.BatteryEnergyKWh=Entry.BatteryKWh;B.Workers=Entry.Operators;B.BuilderPosition=Position;
        if(Entry.Anchor==TEXT("deposit")){const auto* Bound=S.ExtractionNode(B.DefId,Position);if(!Bound||Bound->Id!=DepositId){Error=TEXT("Established mine cannot bind deposit");return false;}B.DepositId=DepositId;}
        Operators.Add(B.Id,Entry.Operators);if(I==0)S.Buildings[0]=MoveTemp(B);else S.Buildings.Add(MoveTemp(B));
    }
    ++S.TransportRevision;S.Energy.Invalidate();S.Energy.Tick(S,0);
    // All plots exist before routing, so a later building cannot obstruct an earlier road.
    // Add only route sections that are not already present in the connected network.
    const FVector2D Origin=S.BuildingAccessPoint(S.Buildings[0]);
    for(int32 I=1;I<S.Buildings.Num();++I)
    {
        const FVector2D Destination=S.BuildingAccessPoint(S.Buildings[I]);TArray<FVector2D> Starts{Origin};
        for(const auto& R:S.Roads){Starts.AddUnique(R.A);Starts.AddUnique(R.B);}
        Starts.StableSort([&](const auto& A,const auto& B){return FVector2D::DistSquared(A,Destination)<FVector2D::DistSquared(B,Destination);});
        bool Connected=false;
        for(const auto& Start:Starts)
        {
            FSeigeSimulation Trial=S;TArray<FVector2D> Route;if(!Trial.FindRoadRoute(Start,Destination,Route))continue;
            FVector2D Previous=Start;bool Valid=true;
            for(const auto& End:Route)
            {
                const bool Existing=Trial.Roads.ContainsByPredicate([&](const auto& R){return OnRoad(Previous,R.A,R.B)&&OnRoad(End,R.A,R.B);});
                if(!Previous.Equals(End,.01)&&!Existing)
                {
                    if(!Trial.CanPlaceRoadGeometry(Previous,End,Error)){Valid=false;break;}
                    FSeigeTransportSegment R;R.Id=Trial.NextId++;R.A=Previous;R.B=End;R.Tier=R.TargetTier=EstablishedRoadTier;R.IsConstructing=false;R.ConstructionProgress=1;
                    R.InstalledMaterials=Trial.RoadCost(R.A,R.B,R.Tier);R.Health=R.MaxHealth=FVector2D::Distance(R.A,R.B)*Trial.MetersPerWorldUnit()*Trial.TransportTiers[R.Tier].HealthPerMeter;
                    Trial.Roads.Add(MoveTemp(R));++Trial.TransportRevision;
                }
                Previous=End;
            }
            if(Valid){S=MoveTemp(Trial);Connected=true;break;}
        }
        if(!Connected){Error=TEXT("Established layout has no legal dry road connection: ")+S.Buildings[I].DefId+TEXT("; ")+Error;return false;}
    }

    S.Time=EstablishedAge;S.Credits=EstablishedCredits;
    S.NextWaveTime=S.Time+S.Number(TEXT("wave_first_time"));S.NextRoamTime=S.Time+S.Number(TEXT("roam_first_time"));
    if(!S.Workers.SeedEstablished(S,Operators,EstablishedIdle,Error))return false;
    S.Energy.Invalidate();S.Energy.Tick(S,0);S.Workers.RefreshMetrics(S);
    for(const auto& B:S.Buildings)
    {
        if(!S.IsRoadGridConnected(S.Buildings[0].Id,B.Id)||S.Energy.Fraction(B.Id)<=0){Error=TEXT("Established building lacks its live connected power grid: ")+B.DefId;return false;}
        if(const auto* P=S.Combat.BuildingPlatforms.Find(B.DefId))
        {FSeigeBuildingCombatState State;State.Id=B.Id;State.Definition=B.DefId;State.Shield=P->Shield;State.Armor=P->Armor;State.Weapons=P->Weapons;State.Cooldowns.Init(0,State.Weapons.Num());S.Combat.BuildingState.Add(B.Id,MoveTemp(State));}
    }
    if(S.SupportedPopulation<S.Population||S.Population<S.TotalJobs){Error=TEXT("Established workers do not meet actual supported operating and logistics jobs");return false;}
    S.AddEvent(TEXT("Established scenario loaded from its explicit starting-state manifest; no simulated growth history."));
    Error.Empty();return true;
}

bool FSeigeScenarioAI::InitializeEstablished(FSeigeSimulation& Colony,FString& Error)
{
    TArray<FSeigeNode> Deposits;for(const auto& N:Colony.Nodes)if(Colony.Resources[N.Resource].Class==TEXT("standard"))Deposits.Add(N);
    Deposits.Sort([](const auto& A,const auto& B){const double DA=A.Position.SizeSquared(),DB=B.Position.SizeSquared();return DA==DB?A.Id<B.Id:DA<DB;});
    FString LastError;
    for(const auto& N:Deposits)for(int32 Direction=0;Direction<Angles;++Direction)for(int32 Turn=0;Turn<EstablishedRotations;++Turn)
    {
        const double Angle=Direction*UE_TWO_PI/Angles;FSeigeSimulation Candidate=Colony;
        const FVector2D CorePosition=N.Position+FVector2D(FMath::Cos(Angle),FMath::Sin(Angle))*RingStart;
        if(TryEstablishedLayout(Candidate,N.Id,CorePosition,Turn*UE_HALF_PI,LastError)){Colony=MoveTemp(Candidate);Error.Empty();return true;}
    }
    Error=TEXT("No legal established layout fits this region: ")+LastError;return false;
}
