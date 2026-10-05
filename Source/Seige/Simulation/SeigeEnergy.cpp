#include "SeigeEnergy.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"

namespace {
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& V,double Min=0){return O&&O->TryGetNumberField(Key,V)&&FMath::IsFinite(V)&&V>=Min;}
double DistanceToSegment(FVector2D P,FVector2D A,FVector2D B){const auto D=B-A;return FVector2D::Distance(P,A+D*FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),1.e-12),0.,1.));}
double Cross(FVector2D A,FVector2D B){return A.X*B.Y-A.Y*B.X;}
bool Touch(const FSeigeTransportSegment& A,const FSeigeTransportSegment& B,double Tol){if(DistanceToSegment(A.A,B.A,B.B)<=Tol||DistanceToSegment(A.B,B.A,B.B)<=Tol||DistanceToSegment(B.A,A.A,A.B)<=Tol||DistanceToSegment(B.B,A.A,A.B)<=Tol)return true;const auto U=A.B-A.A,V=B.B-B.A;const double Den=Cross(U,V);if(FMath::Abs(Den)<1.e-9)return false;const double T=Cross(B.A-A.A,V)/Den,S=Cross(B.A-A.A,U)/Den;return T>=0&&T<=1&&S>=0&&S<=1;}
}
bool FSeigeEnergySystem::Initialize(const TSharedPtr<FJsonObject>& Doc,FSeigeSimulation& Sim,FString& Error)
{
    *this=FSeigeEnergySystem();const TSharedPtr<FJsonObject>* O=nullptr;const TSharedPtr<FJsonObject>* Ds=nullptr;
    if(!Doc->TryGetObjectField(TEXT("energy"),O)||!Number(*O,TEXT("worker_kw"),WorkerKW)||!Number(*O,TEXT("connection_tolerance_meters"),ConnectionToleranceMeters)||!Number(*O,TEXT("minimum_operating_fraction"),MinimumOperatingFraction,1.e-9)||MinimumOperatingFraction>1||!(*O)->TryGetObjectField(TEXT("buildings"),Ds)){Error=TEXT("Invalid energy policy");return false;}
    for(const auto& P:(*Ds)->Values){const FString Id(P.Key);const auto D=P.Value->AsObject();FSeigeEnergyDefinition E;double Priority=0;
        if(!Sim.BuildingDefs.Contains(Id)||!Number(D,TEXT("generation_kw"),E.GenerationKW)||!Number(D,TEXT("battery_capacity_kwh"),E.BatteryCapacityKWh)||!Number(D,TEXT("initial_battery_kwh"),E.InitialBatteryKWh)||E.InitialBatteryKWh>E.BatteryCapacityKWh||!Number(D,TEXT("idle_kw"),E.IdleKW)||!Number(D,TEXT("fuel_units_per_kwh"),E.FuelUnitsPerKWh)||!Number(D,TEXT("fuel_buffer_seconds"),E.FuelBufferSeconds)||!Number(D,TEXT("priority"),Priority)||Priority>1000||Priority!=FMath::FloorToDouble(Priority)||!D->TryGetStringField(TEXT("fuel_resource"),E.FuelResource)||!D->TryGetBoolField(TEXT("self_start"),E.SelfStart)||!D->TryGetBoolField(TEXT("requires_road_grid"),E.RequiresRoadGrid)){Error=TEXT("Invalid energy definition: ")+Id;return false;}
        if((!E.FuelResource.IsEmpty()&&(!Sim.Resources.Contains(E.FuelResource)||E.GenerationKW<=0||E.FuelUnitsPerKWh<=0))||(E.FuelResource.IsEmpty()&&E.FuelUnitsPerKWh!=0)||(E.InitialBatteryKWh>0&&Sim.BuildingDefs[Id].Role!=TEXT("core"))){Error=TEXT("Invalid generator fuel or initial charge: ")+Id;return false;}
        E.Priority=int32(Priority);Definitions.Add(Id,E);Sim.BuildingDefs[Id].PowerUsageKW=E.IdleKW;Sim.BuildingDefs[Id].PowerGenerationKW=E.GenerationKW;
    }
    if(Definitions.Num()!=Sim.BuildingDefs.Num()){Error=TEXT("Every building needs an energy definition");return false;}
    for(auto& B:Sim.Buildings)B.BatteryEnergyKWh=Definitions[B.DefId].InitialBatteryKWh;
    Ready=true;Tick(Sim,0);return true;
}
void FSeigeEnergySystem::Rebuild(FSeigeSimulation& Sim)
{
    Grids.Empty();BuildingGrid.Empty();RoadGrid.Empty();Fractions.Empty();RoadFractions.Empty();
    TArray<int32> Active;for(int I=0;I<Sim.Roads.Num();++I)if(Sim.Roads[I].Health>0&&!Sim.Roads[I].Tier.IsEmpty())Active.Add(I);
    TArray<int32> Parent;for(int I=0;I<Active.Num();++I)Parent.Add(I);
    auto Root=[&](int I){while(Parent[I]!=I)I=Parent[I];return I;};const double Tol=ConnectionToleranceMeters/Sim.MetersPerWorldUnit();
    for(int I=0;I<Active.Num();++I)for(int J=I+1;J<Active.Num();++J)if(Touch(Sim.Roads[Active[I]],Sim.Roads[Active[J]],Tol))Parent[Root(J)]=Root(I);
    TMap<int32,int32> Groups;
    for(int I=0;I<Active.Num();++I){const int R=Root(I);int32* Found=Groups.Find(R);int32 G=Found?*Found:Grids.Add(FGrid());Groups.Add(R,G);const int Id=Sim.Roads[Active[I]].Id;Grids[G].Roads.Add(Id);RoadGrid.Add(Id,G);}
    for(const auto& B:Sim.Buildings)if(B.Health>0){int G=INDEX_NONE;const auto Port=Sim.BuildingAccessPoint(B);for(int I:Active)if(DistanceToSegment(Port,Sim.Roads[I].A,Sim.Roads[I].B)<=Tol){G=RoadGrid[Sim.Roads[I].Id];break;}if(G==INDEX_NONE)G=Grids.Add(FGrid());Grids[G].Buildings.Add(B.Id);BuildingGrid.Add(B.Id,G);}
    for(auto& G:Grids){G.State.ComponentId=MAX_int32;for(int Id:G.Roads)G.State.ComponentId=FMath::Min(G.State.ComponentId,Id);for(int Id:G.Buildings)G.State.ComponentId=FMath::Min(G.State.ComponentId,Id);G.State.Connected=!G.Roads.IsEmpty();G.Buildings.Sort([&](int A,int B){const auto* X=Sim.FindBuilding(A);const auto* Y=Sim.FindBuilding(B);const int XP=Definitions[X->DefId].Priority,YP=Definitions[Y->DefId].Priority;return XP==YP?A<B:XP<YP;});}
    TopologyRevision=Sim.TransportRevision;
}
double FSeigeEnergySystem::Stored(const FSeigeSimulation& Sim,const FGrid& G)const{double N=0;for(int Id:G.Buildings)if(const auto* B=Sim.FindBuilding(Id))if(B->Health>0&&(!B->IsConstructing||Sim.Definition(*B)->Role==TEXT("core")))N+=B->BatteryEnergyKWh;return N;}
double FSeigeEnergySystem::Capacity(const FSeigeSimulation& Sim,const FGrid& G)const{double N=0;for(int Id:G.Buildings)if(const auto* B=Sim.FindBuilding(Id))if(B->Health>0&&(!B->IsConstructing||Sim.Definition(*B)->Role==TEXT("core")))N+=Definitions[B->DefId].BatteryCapacityKWh;return N;}
void FSeigeEnergySystem::Distribute(FSeigeSimulation& Sim,const FGrid& G,double N)const{const double Cap=Capacity(Sim,G);for(int Id:G.Buildings)if(auto* B=Sim.FindBuilding(Id))if(B->Health>0&&(!B->IsConstructing||Sim.Definition(*B)->Role==TEXT("core")))B->BatteryEnergyKWh=Cap>0?FMath::Clamp(N,0.,Cap)*Definitions[B->DefId].BatteryCapacityKWh/Cap:0;}
void FSeigeEnergySystem::Tick(FSeigeSimulation& Sim,double Seconds)
{
    if(!Ready)return;if(TopologyRevision!=Sim.TransportRevision)Rebuild(Sim);
    for(auto& G:Grids){double Supply=0,Demand=0;TMap<int32,double> Loads;
        for(int Id:G.Buildings){auto* B=Sim.FindBuilding(Id);const auto& E=Definitions[B->DefId];const auto* D=Sim.Definition(*B);const bool Active=B->Enabled&&(!B->IsConstructing||D->Role==TEXT("core"));const bool Connected=G.State.Connected||!E.RequiresRoadGrid;
            double KW=0;if(Active&&Connected){KW=E.IdleKW+B->Workers*WorkerKW;if(D->Role==TEXT("core")){int Operating=0;for(const auto& Other:Sim.Buildings)Operating+=Other.Workers;KW+=FMath::Max(0,Sim.Population-Operating)*WorkerKW;}}
            Loads.Add(Id,KW);Demand+=KW;Fractions.Add(Id,Active&&Connected?1.:0.);
            if(Active&&(E.SelfStart||Connected)&&E.GenerationKW>0){double Generation=E.GenerationKW*(B->IsConstructing&&D->Role==TEXT("core")?1.:D->Jobs==0?1.:D->WorkforceMode==TEXT("proportional")?Sim.WorkforceEfficiency*FMath::Min(1.,double(B->Workers)/D->Jobs):B->Workers>=D->Jobs?Sim.WorkforceEfficiency:0.);if(!E.FuelResource.IsEmpty()){const double Need=Generation*Seconds/3600.*E.FuelUnitsPerKWh;const double Available=B->Inventory.FindRef(E.FuelResource);if(Seconds>0){const double Used=FMath::Min(Need,Available);Generation=Used*3600./Seconds/E.FuelUnitsPerKWh;B->Inventory.FindOrAdd(E.FuelResource)=FMath::Max(0.,Available-Used);}else if(Available<=0)Generation=0;}Supply+=Generation;}
        }
        double RoadKW=0;for(int Id:G.Roads){const auto* R=Sim.FindRoad(Id);RoadKW+=Sim.TransportTiers[R->Tier].IdleKWPer100Meters*FVector2D::Distance(R->A,R->B)*Sim.MetersPerWorldUnit()/100.;}Demand+=RoadKW;
        const double Cap=Capacity(Sim,G),Prior=Stored(Sim,G),Generated=Supply*Seconds/3600.;double Available=Prior+Generated,Consumed=0;
        // Stable priority allocation, with no partial recipe commits. Passive power may brown out.
        auto Allocate=[&](double KW){if(KW<=0)return 1.;if(Seconds<=0)return Supply+Prior*3600.>=KW?1.:0.;const double Need=KW*Seconds/3600.,Fraction=FMath::Clamp(Available/Need,0.,1.);const double Used=Need*Fraction;Available-=Used;Consumed+=Used;return Fraction>=MinimumOperatingFraction?Fraction:0.;};
        for(int Id:G.Buildings){const double F=Allocate(Loads[Id]);Fractions[Id]*=F;}
        const double RoadFraction=Allocate(RoadKW);for(int Id:G.Roads)RoadFractions.Add(Id,RoadFraction);
        if(Seconds>0){GeneratedKWh+=Generated;ConsumedKWh+=Consumed;SpilledKWh+=FMath::Max(0.,Available-Cap);Distribute(Sim,G,FMath::Min(Available,Cap));}
        G.State.GenerationKW=Supply;G.State.DemandKW=Demand;G.State.SuppliedKW=Seconds>0?Consumed*3600./Seconds:FMath::Min(Demand,Supply+Prior*3600.);G.State.StoredKWh=Stored(Sim,G);G.State.CapacityKWh=Cap;G.State.PowerFraction=Demand>0?FMath::Clamp(G.State.SuppliedKW/Demand,0.,1.):1.;
    }
}
double FSeigeEnergySystem::Fraction(int32 Id)const{return Ready?Fractions.FindRef(Id):1.;}
bool FSeigeEnergySystem::RoadPowered(int32 Id)const{return !Ready||RoadFractions.FindRef(Id)>=MinimumOperatingFraction;}
double FSeigeEnergySystem::FuelDemand(const FString& Id,const FString& Resource)const{const auto* D=Definitions.Find(Id);return D&&D->FuelResource==Resource?D->GenerationKW*D->FuelBufferSeconds/3600.*D->FuelUnitsPerKWh:0;}
bool FSeigeEnergySystem::CanConsume(const FSeigeSimulation& Sim,int32 Id,double N)const{if(N<0||!FMath::IsFinite(N)||Fraction(Id)<=0)return false;const auto* G=BuildingGrid.Find(Id);return G&&(N<=0||Stored(Sim,Grids[*G])+1.e-9>=N);}
bool FSeigeEnergySystem::Consume(FSeigeSimulation& Sim,int32 Id,double N){if(!CanConsume(Sim,Id,N))return false;auto& G=Grids[BuildingGrid[Id]];Distribute(Sim,G,FMath::Max(0.,Stored(Sim,G)-N));ConsumedKWh+=N;G.State.StoredKWh=Stored(Sim,G);return true;}
FSeigeEnergyInfo FSeigeEnergySystem::Info(const FSeigeSimulation& Sim,int32 Id)const{if(Id){const auto* G=BuildingGrid.Find(Id);if(!G)return {};auto S=Grids[*G].State;S.StoredKWh=Stored(Sim,Grids[*G]);S.PowerFraction=Fraction(Id);return S;}FSeigeEnergyInfo S;for(const auto& G:Grids){S.GenerationKW+=G.State.GenerationKW;S.DemandKW+=G.State.DemandKW;S.SuppliedKW+=G.State.SuppliedKW;S.StoredKWh+=Stored(Sim,G);S.CapacityKWh+=Capacity(Sim,G);}S.PowerFraction=S.DemandKW>0?S.SuppliedKW/S.DemandKW:1;return S;}
void FSeigeEnergySystem::Save(const TSharedPtr<FJsonObject>& O)const{O->SetNumberField(TEXT("generated_kwh"),GeneratedKWh);O->SetNumberField(TEXT("consumed_kwh"),ConsumedKWh);O->SetNumberField(TEXT("spilled_kwh"),SpilledKWh);}
bool FSeigeEnergySystem::Load(const TSharedPtr<FJsonObject>& O,FSeigeSimulation& Sim,FString& Error){if(!Number(O,TEXT("generated_kwh"),GeneratedKWh)||!Number(O,TEXT("consumed_kwh"),ConsumedKWh)||!Number(O,TEXT("spilled_kwh"),SpilledKWh)){Error=TEXT("Invalid saved energy ledger");return false;}Invalidate();Tick(Sim,0);return true;}

bool FSeigeEnergySystem::RoadConnectedToBuilding(int32 RoadId,int32 BuildingId) const
{const auto* R=RoadGrid.Find(RoadId);const auto* B=BuildingGrid.Find(BuildingId);return R&&B&&*R==*B;}
