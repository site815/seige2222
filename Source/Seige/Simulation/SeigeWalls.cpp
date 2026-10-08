#include "SeigeWalls.h"
#include "SeigeSimulation.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
double PointDistance(FVector2D P,FVector2D A,FVector2D B)
{const auto D=B-A;return FVector2D::Distance(P,A+D*FMath::Clamp(FVector2D::DotProduct(P-A,D)/FMath::Max(D.SizeSquared(),1.e-12),0.,1.));}
double SegmentDistance(FVector2D A,FVector2D B,FVector2D C,FVector2D D)
{
    const auto U=B-A,V=D-C,W=C-A;const double Den=FVector2D::CrossProduct(U,V);
    if(FMath::Abs(Den)>1.e-8){const double T=FVector2D::CrossProduct(W,V)/Den,S=FVector2D::CrossProduct(W,U)/Den;if(T>=0&&T<=1&&S>=0&&S<=1)return 0;}
    return FMath::Min(FMath::Min(PointDistance(A,C,D),PointDistance(B,C,D)),FMath::Min(PointDistance(C,A,B),PointDistance(D,A,B)));
}
bool HitsBox(FVector2D A,FVector2D B,FVector2D Center,double Half)
{
    double Low=0,High=1;const auto D=B-A;
    for(int Axis=0;Axis<2;++Axis){const double P=Axis?A.Y:A.X,Q=Axis?D.Y:D.X,C=Axis?Center.Y:Center.X;
        if(FMath::Abs(Q)<1.e-8){if(P<C-Half||P>C+Half)return false;}
        else{double T=(C-Half-P)/Q,U=(C+Half-P)/Q;if(T>U)Swap(T,U);Low=FMath::Max(Low,T);High=FMath::Min(High,U);if(Low>High)return false;}}
    return true;
}
bool SharedEnd(const FSeigeWallSegment& A,const FSeigeWallSegment& B)
{return A.A.Equals(B.A,.01)||A.A.Equals(B.B,.01)||A.B.Equals(B.A,.01)||A.B.Equals(B.B,.01);}
}
bool FSeigeWallSystem::Initialize(const FString& Directory,const FSeigeSimulation& Sim,FString& Error)
{
    FString Text;TSharedPtr<FJsonObject> Root;Segments.Reset();LevelDefinitions.Reset();HeightMeters.Reset();
    if(!FFileHelper::LoadFileToString(Text,*FPaths::Combine(Directory,TEXT("walls.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root){Error=TEXT("Cannot read wall rules");return false;}
    const TSharedPtr<FJsonObject>* Object=nullptr;if(!Root->TryGetObjectField(TEXT("walls"),Object)||!Object){Error=TEXT("Missing wall rules");return false;}const auto W=*Object;
    auto Number=[&](const TCHAR* Key,double& Value,double Min,double Max){return W->HasTypedField<EJson::Number>(Key)&&W->TryGetNumberField(Key,Value)&&FMath::IsFinite(Value)&&Value>=Min&&Value<=Max;};
    double J=0,S=0;
    if(!Number(TEXT("segment_length_meters"),SegmentLengthMeters,1,30)||!Number(TEXT("minimum_edge_meters"),MinimumEdgeMeters,.1,SegmentLengthMeters)||!Number(TEXT("width_meters"),WidthMeters,.1,SegmentLengthMeters)||!Number(TEXT("access_clearance_meters"),AccessClearanceMeters,.1,10)||!Number(TEXT("joint_pick_radius_meters"),JointPickRadiusMeters,.1,20)||!Number(TEXT("maximum_joints"),J,2,128)||J!=FMath::FloorToDouble(J)||!Number(TEXT("maximum_segments"),S,1,1024)||S!=FMath::FloorToDouble(S)){Error=TEXT("Invalid wall dimensions or limits");return false;}
    MaximumJoints=int32(J);MaximumSegments=int32(S);const TArray<TSharedPtr<FJsonValue>> *Defs=nullptr,*Heights=nullptr;
    if(!W->TryGetArrayField(TEXT("level_definitions"),Defs)||Defs->Num()!=3||!W->TryGetArrayField(TEXT("height_meters"),Heights)||Heights->Num()!=3){Error=TEXT("Walls require three levels");return false;}
    for(int32 I=0;I<3;++I){FString Id;double H=0;if(!(*Defs)[I]->TryGetString(Id)||!Sim.BuildingDefs.Contains(Id)||Sim.BuildingDefs[Id].Role!=TEXT("wall")||!(*Heights)[I]->TryGetNumber(H)||!FMath::IsFinite(H)||H<1||H>20){Error=TEXT("Invalid wall level");return false;}LevelDefinitions.Add(Id);HeightMeters.Add(H);}
    Error.Empty();return true;
}
const FSeigeWallSegment* FSeigeWallSystem::Find(int32 Id) const{return Segments.FindByPredicate([Id](const auto& S){return S.BuildingId==Id;});}
bool FSeigeWallSystem::AccessPoint(const FSeigeSimulation& Sim,int32 Id,FVector2D& Out) const
{
    const auto* S=Find(Id);if(!S)return false;const auto D=(S->B-S->A).GetSafeNormal();const FVector2D N(-D.Y,D.X);
    const auto* B=Sim.FindBuilding(Id);const auto* Def=B?Sim.Definition(*B):nullptr;
    const double Half=Def?Def->ReservedFootprint:WidthMeters*.5/Sim.MetersPerWorldUnit();
    const double Offset=Half/FMath::Max(FMath::Abs(N.X),FMath::Abs(N.Y))+AccessClearanceMeters/Sim.MetersPerWorldUnit();
    Out=(S->A+S->B)*.5+N*(S->InsideLeft?1.:-1.)*Offset;return true;
}
bool FSeigeWallSystem::Plan(const FSeigeSimulation& Sim,const TArray<FVector2D>& Joints,bool InsideLeft,FSeigeWallPlan& Out,FString& Error) const
{
    Out={};const auto* Core=Sim.Core();
    if(Sim.Escaped||Sim.Failed||!Core||Core->Health<=0||Core->IsConstructing){Error=TEXT("Deploy a command center before building walls");return false;}
    if(Joints.Num()<2||Joints.Num()>MaximumJoints||LevelDefinitions.Num()!=3){Error=TEXT("Wall plan needs 2 to 64 joints");return false;}
    const double Metres=Sim.MetersPerWorldUnit(),Radius=WidthMeters*.5/Metres;
    for(const auto& P:Joints)if(P.ContainsNaN()||FMath::Abs(P.X)+Radius>Sim.WorldHalfSize||FMath::Abs(P.Y)+Radius>Sim.WorldHalfSize){Error=TEXT("Wall joints must be inside the sector");return false;}
    // Test original edges before subdivision: a crossing can coincidentally
    // become a shared endpoint of two subdivided sections.
    for(int32 I=1;I<Joints.Num();++I)
    {
        if(I>1){const auto A=(Joints[I-1]-Joints[I-2]).GetSafeNormal(),B=(Joints[I]-Joints[I-1]).GetSafeNormal();if(FMath::Abs(FVector2D::CrossProduct(A,B))<1.e-8&&FVector2D::DotProduct(A,B)<0){Error=TEXT("Wall plan doubles back over itself");return false;}}
        for(int32 K=1;K<I-1;++K)
        {
            const bool Closure=K==1&&I==Joints.Num()-1&&Joints[I].Equals(Joints[0],.01);
            if(!Closure&&SegmentDistance(Joints[I-1],Joints[I],Joints[K-1],Joints[K])<Radius*2){Error=TEXT("Wall plan crosses itself");return false;}
        }
    }
    for(int32 I=1;I<Joints.Num();++I)
    {
        const auto A=Joints[I-1],B=Joints[I];const double Length=FVector2D::Distance(A,B)*Metres;
        if(A.ContainsNaN()||B.ContainsNaN()||!FMath::IsFinite(Length)||Length<MinimumEdgeMeters){Error=TEXT("Wall joints are too close or invalid");return false;}
        const int32 Count=FMath::CeilToInt(Length/SegmentLengthMeters);
        if(Out.Segments.Num()+Count>MaximumSegments){Error=TEXT("Wall plan exceeds its segment limit");return false;}
        for(int32 K=0;K<Count;++K){FSeigeWallSegment S;S.A=FMath::Lerp(A,B,double(K)/Count);S.B=FMath::Lerp(A,B,double(K+1)/Count);S.InsideLeft=InsideLeft;Out.Segments.Add(S);}
        Out.LengthMeters+=Length;
    }
    for(int32 I=0;I<Out.Segments.Num();++I)
    {
        const auto& S=Out.Segments[I];const FVector2D Mid=(S.A+S.B)*.5;
        // Routing reserves each section's square plot immediately, including
        // its movement clearance. Never close that plot around a live unit.
        const double Plot=Sim.BuildingDefs[LevelDefinitions[0]].ReservedFootprint;
        const double WorkerClearance=Sim.Transport->GetNumberField(TEXT("path_clearance"));
        auto Occupies=[&](FVector2D P,double Clearance){return FMath::Abs(P.X-Mid.X)<Plot+Clearance&&FMath::Abs(P.Y-Mid.Y)<Plot+Clearance;};
        for(const auto& W:Sim.Workers.Bodies)if(W.State==TEXT("active")&&W.Outdoor&&Occupies(W.Position,Sim.Workers.BodyRadiusMeters()/Metres)){Error=TEXT("Wait for the worker to leave the wall corridor");return false;}
        for(const auto& C:Sim.Couriers)if(Occupies(C.Position,WorkerClearance)){Error=TEXT("Wait for the delivery worker to leave the wall corridor");return false;}
        for(const auto& B:Sim.Buildings)if(B.Health>0&&((B.TravellingBuilders>0&&Occupies(B.BuilderPosition,WorkerClearance))||(B.BuildersOnSite>0&&Occupies(Sim.BuildingAccessPoint(B),WorkerClearance)))){Error=TEXT("Wait for the construction crew to leave the wall corridor");return false;}
        for(const auto& R:Sim.Roads)if(R.Health>0&&((R.TravellingBuilders>0&&Occupies(R.BuilderPosition,WorkerClearance))||(R.BuildersOnSite>0&&Occupies(Sim.RoadAccessPoint(R),WorkerClearance)))){Error=TEXT("Wait for the road crew to leave the wall corridor");return false;}
        for(const auto& V:Sim.Combat.Vehicles)if(V.Health>0&&!V.Embarked&&!V.Evacuated&&V.SectorIndex==4&&Occupies(V.Position,Sim.Combat.Chassis[V.ChassisId].RadiusMeters/Metres)){Error=TEXT("Move the vehicle outside the wall corridor first");return false;}
        for(auto P:{S.A,Mid,S.B})if(FMath::Abs(P.X)+Radius>Sim.WorldHalfSize||FMath::Abs(P.Y)+Radius>Sim.WorldHalfSize||!Sim.IsVisible(P)){Error=TEXT("Wall plan must stay inside the sector and live sensor coverage");return false;}
        for(const auto& B:Sim.Buildings)if(B.Health>0)if(const auto* D=Sim.Definition(B))
        {
            if(const auto* Existing=Find(B.Id)){if(SegmentDistance(S.A,S.B,Existing->A,Existing->B)<Radius*2&&!SharedEnd(S,*Existing)){Error=TEXT("Wall plan crosses an existing wall");return false;}if(Mid.Equals(B.Position,.1)){Error=TEXT("Wall segment already exists");return false;}}
            else if(HitsBox(S.A,S.B,B.Position,D->ReservedFootprint+Radius)){Error=TEXT("Wall plan intersects a reserved building plot");return false;}
        }
        for(const auto& R:Sim.Roads)if(R.Health>0&&SegmentDistance(S.A,S.B,R.A,R.B)<Radius+Sim.TransportTiers.FindRef(R.IsConstructing?R.TargetTier:R.Tier).WidthMeters*.5/Metres){Error=TEXT("Leave a gate gap at the road crossing");return false;}
        for(int32 K=0;K<I;++K)if(!SharedEnd(S,Out.Segments[K])&&SegmentDistance(S.A,S.B,Out.Segments[K].A,Out.Segments[K].B)<Radius*2){Error=TEXT("Wall plan crosses itself");return false;}
    }
    for(const auto& Cost:Sim.BuildingDefs[LevelDefinitions[0]].Cost){Out.Cost.Add(Cost.Key,Cost.Value*Out.Segments.Num());if(Sim.ConstructionAvailable(Cost.Key)+UE_DOUBLE_SMALL_NUMBER<Cost.Value*Out.Segments.Num()){Error=TEXT("Insufficient unreserved materials for the whole wall plan");return false;}}
    Error.Empty();return true;
}
bool FSeigeWallSystem::Commit(FSeigeSimulation& Sim,const TArray<FVector2D>& Joints,bool InsideLeft,FString& Error)
{
    FSeigeWallPlan PlanResult;if(!Plan(Sim,Joints,InsideLeft,PlanResult,Error))return false;
    FSeigeSimulation Candidate=Sim;const auto& Def=Candidate.BuildingDefs[LevelDefinitions[0]];const auto Start=Candidate.BuildingAccessPoint(*Candidate.Core());
    for(auto S:PlanResult.Segments)
    {
        FSeigeBuilding B;B.Id=Candidate.NextId++;B.DefId=Def.Id;B.Position=(S.A+S.B)*.5;B.Health=Def.Health;B.IsConstructing=true;B.ConstructionProgress=0;B.BuilderPosition=Start;B.Status=TEXT("Wall plan committed; awaiting materials and builders");S.BuildingId=B.Id;Candidate.Walls.Segments.Add(S);Candidate.Buildings.Add(B);
    }
    ++Candidate.TransportRevision;
    for(const auto& S:PlanResult.Segments){auto* B=Candidate.Buildings.FindByPredicate([&](const auto& B){return B.Position.Equals((S.A+S.B)*.5,.01)&&Candidate.Definition(B)->Role==TEXT("wall");});if(!B||!Candidate.FindRoute(Start,Candidate.BuildingAccessPoint(*B),B->BuilderRoute)){Error=TEXT("Builders cannot reach the inside of the wall; flip its side or leave an opening");return false;}}
    Candidate.AllocateWorkers();Candidate.AddEvent(FString::Printf(TEXT("Committed %d wall sections; materials reserved"),PlanResult.Segments.Num()));Sim=MoveTemp(Candidate);Error.Empty();return true;
}
bool FSeigeWallSystem::Save(const TSharedPtr<FJsonObject>& Root) const
{
    TArray<TSharedPtr<FJsonValue>> Rows;for(const auto& S:Segments){auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("building"),S.BuildingId);O->SetNumberField(TEXT("ax"),S.A.X);O->SetNumberField(TEXT("ay"),S.A.Y);O->SetNumberField(TEXT("bx"),S.B.X);O->SetNumberField(TEXT("by"),S.B.Y);O->SetBoolField(TEXT("inside_left"),S.InsideLeft);Rows.Add(MakeShared<FJsonValueObject>(O));}Root->SetArrayField(TEXT("wall_segments"),Rows);return true;
}
bool FSeigeWallSystem::Load(const TSharedPtr<FJsonObject>& Root,const FSeigeSimulation& Sim,FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!Root->TryGetArrayField(TEXT("wall_segments"),Rows)||Rows->Num()>100000){Error=TEXT("Missing or invalid saved walls");return false;}TArray<FSeigeWallSegment> Loaded;TSet<int32> Seen;
    for(const auto& Row:*Rows){if(Row->Type!=EJson::Object){Error=TEXT("Invalid saved wall");return false;}auto O=Row->AsObject();FSeigeWallSegment S;double Id=0;
        if(!O->TryGetNumberField(TEXT("building"),Id)||!FMath::IsFinite(Id)||Id<1||Id>MAX_int32||Id!=FMath::FloorToDouble(Id)||!O->TryGetNumberField(TEXT("ax"),S.A.X)||!O->TryGetNumberField(TEXT("ay"),S.A.Y)||!O->TryGetNumberField(TEXT("bx"),S.B.X)||!O->TryGetNumberField(TEXT("by"),S.B.Y)||!O->TryGetBoolField(TEXT("inside_left"),S.InsideLeft)||S.A.ContainsNaN()||S.B.ContainsNaN()){Error=TEXT("Invalid wall coordinates");return false;}
        S.BuildingId=int32(Id);const auto* B=Sim.FindBuilding(S.BuildingId);const double Length=FVector2D::Distance(S.A,S.B)*Sim.MetersPerWorldUnit();
        if(!B||!Sim.Definition(*B)||Sim.Definition(*B)->Role!=TEXT("wall")||Seen.Contains(S.BuildingId)||!B->Position.Equals((S.A+S.B)*.5,.01)||Length<.01||Length>SegmentLengthMeters+.001||FMath::Abs(S.A.X)>Sim.WorldHalfSize||FMath::Abs(S.A.Y)>Sim.WorldHalfSize||FMath::Abs(S.B.X)>Sim.WorldHalfSize||FMath::Abs(S.B.Y)>Sim.WorldHalfSize){Error=TEXT("Saved wall does not match its physical building");return false;}Seen.Add(S.BuildingId);Loaded.Add(S);}
    for(const auto& B:Sim.Buildings)if(Sim.Definition(B)&&Sim.Definition(B)->Role==TEXT("wall")&&!Seen.Contains(B.Id)){Error=TEXT("Wall building lacks saved geometry");return false;}
    Segments=MoveTemp(Loaded);Error.Empty();return true;
}
