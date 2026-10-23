#include "SeigeUnitInspection.h"
#include "SeigeGameMode.h"
#include "SeigeAudioSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"

namespace
{
const FLinearColor Text(.91f,.95f,.96f),Muted(.58f,.71f,.76f),Gold(.91f,.72f,.39f),Mint(.35f,.85f,.78f),Red(.96f,.35f,.28f);
bool CanInspect(const ASeigeGameMode& G)
{
    return G.Ready&&G.Screen==TEXT("playing")&&!G.MenuOpen&&!G.IsRegionMap()&&!G.CompanionView&&!G.Sim.Escaped&&!G.Sim.Failed&&
        (G.Observer||!G.Sim.Won||G.WinAcknowledged)&&G.SelectedBuild.IsEmpty()&&!G.IsRoadToolActive()&&!G.WallPlacementActive&&!G.FleetOrderActive;
}
const FSeigeSimulation* InspectionOwner(const ASeigeGameMode& G,int32 Sector)
{
    if(Sector==4)return &G.Sim;
    const auto* N=G.Neighbors.FindByPredicate([&](const auto& N){return N.Index==Sector;});return N?&N->Sim:nullptr;
}
bool ProjectBounds(const APlayerController& PC,const FBox& Bounds,FBox2D& Screen)
{
    if(!Bounds.IsValid)return false;
    Screen=FBox2D(ForceInit);
    for(int32 I=0;I<8;++I)
    {
        FVector2D P;
        const FVector Corner(I&1?Bounds.Max.X:Bounds.Min.X,I&2?Bounds.Max.Y:Bounds.Min.Y,I&4?Bounds.Max.Z:Bounds.Min.Z);
        if(!PC.ProjectWorldLocationToScreen(Corner,P)||P.ContainsNaN())return false;
        Screen+=P;
    }
    return Screen.bIsValid;
}
FBox VisibleBounds(const AActor* Actor)
{
    FBox Result(ForceInit);if(!IsValid(Actor)||Actor->IsHidden())return Result;
    TInlineComponentArray<UPrimitiveComponent*> Parts(Actor);
    for(const auto* Part:Parts)
    {
        if(!Part->IsRegistered()||!Part->IsVisible()||Part->bHiddenInGame)continue;
        FBox Bounds=Part->Bounds.GetBox();
        // Construction reveal is a material clip, so the engine's ordinary
        // component bounds still contain the absent upper floors. Read the
        // existing visual uniform to keep those floors out of picking and bars.
        if(Part->ComponentHasTag(TEXT("ConstructionReveal")))if(const auto* Mesh=Cast<UStaticMeshComponent>(Part))if(const auto* Material=Mesh->GetMaterial(0))
        {
            float RevealHeight=0;
            if(Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("RevealHeight")),RevealHeight))
            {if(RevealHeight<=Bounds.Min.Z)continue;Bounds.Max.Z=FMath::Min(Bounds.Max.Z,double(RevealHeight));}
        }
        Result+=Bounds;
    }
    return Result;
}
}

bool SeigeInspection::Visible(double Health,bool Outdoor,bool Embarked,bool Evacuated,bool Rendered,bool Revealed)
{return FMath::IsFinite(Health)&&Health>0&&Outdoor&&!Embarked&&!Evacuated&&Rendered&&Revealed;}

bool SeigeInspection::RayBox(const FVector& Origin,const FVector& Direction,const FBox& B,double& Depth)
{
    if(!B.IsValid||Origin.ContainsNaN()||Direction.ContainsNaN()||Direction.IsNearlyZero())return false;
    double Near=0,Far=TNumericLimits<double>::Max();
    for(int32 Axis=0;Axis<3;++Axis)
    {
        if(FMath::Abs(Direction[Axis])<1.e-12){if(Origin[Axis]<B.Min[Axis]||Origin[Axis]>B.Max[Axis])return false;continue;}
        double A=(B.Min[Axis]-Origin[Axis])/Direction[Axis],Z=(B.Max[Axis]-Origin[Axis])/Direction[Axis];
        if(A>Z)Swap(A,Z);Near=FMath::Max(Near,A);Far=FMath::Min(Far,Z);if(Near>Far)return false;
    }
    if(Far<0)return false;Depth=Near;return true;
}

int32 SeigeInspection::Pick(const TArray<FSeigeInspectionProjection>& Candidates,FVector2D Mouse,double TerrainDepth)
{
    if(Mouse.ContainsNaN())return INDEX_NONE;
    int32 Picked=INDEX_NONE;double Nearest=TNumericLimits<double>::Max();
    for(const auto& P:Candidates)
        if(P.Candidate>=0&&P.Screen.bIsValid&&P.Screen.IsInsideOrOn(Mouse)&&FMath::IsFinite(P.RayDepth)&&P.RayDepth>=0&&P.RayDepth<=TerrainDepth+5&&P.RayDepth<Nearest)
        {Picked=P.Candidate;Nearest=P.RayDepth;}
    return Picked;
}

FSeigeHoverBars SeigeInspection::Bars(const FBox2D& B,double HP,double MaxHP,double Progress,double Scale)
{
    FSeigeHoverBars R;if(!B.bIsValid||!FMath::IsFinite(Scale)||Scale<=0)return R;
    const double Width=112*Scale,Height=7*Scale,X=B.GetCenter().X-Width*.5;
    R.Health=FBox2D(FVector2D(X,B.Min.Y-14*Scale),FVector2D(X+Width,B.Min.Y-14*Scale+Height));
    R.HealthFraction=FMath::IsFinite(HP)&&FMath::IsFinite(MaxHP)&&MaxHP>0?FMath::Clamp(HP/MaxHP,0.,1.):0;
    R.HasProgress=FMath::IsFinite(Progress)&&Progress>=0;
    R.ProgressFraction=R.HasProgress?FMath::Clamp(Progress,0.,1.):0;
    if(R.HasProgress)R.Progress=FBox2D(FVector2D(X,B.Max.Y+7*Scale),FVector2D(X+Width,B.Max.Y+7*Scale+Height));
    return R;
}

void ASeigeGameMode::GatherInspectionCandidates(TArray<FSeigeInspectionCandidate>& Out) const
{
    Out.Reset();if(!CanInspect(*this))return;
    const int32 View=DetailedSectorIndex();
    auto Gather=[&](const FSeigeSimulation& S,int32 Sector)
    {
        const FString Prefix=Sector==4?TEXT("home_"):FString::Printf(TEXT("zone_%d_"),Sector);
        auto Add=[&](ESeigeInspectKind Kind,int32 Id,const FString& WorkerId,const FString& Key,const FString& Name,double Health,double Maximum,FVector2D Logical,int32 PhysicalSector,double Progress=-1)
        {
            if(PhysicalSector!=View)return;
            const auto* A=Visuals.FindRef(Key).Get();
            const bool Revealed=Observer||(Sector==4&&Kind!=ESeigeInspectKind::Enemy)||IsWorldVisible(Logical);
            if(!SeigeInspection::Visible(Health,true,false,false,IsValid(A)&&!A->IsHidden(),Revealed))return;
            // Rendered actor bounds already contain interpolation, facing and the
            // current construction pose. Never pick an invisible simulation proxy.
            FBox Bounds=VisibleBounds(A);
            if(Kind==ESeigeInspectKind::Building)
            {
                // Foundations/scaffolding and the permanent command shuttle
                // are separate actors, but belong to the same real building.
                for(const auto* Suffix:{TEXT("_site"),TEXT("_shuttle")})
                {const FBox Extra=VisibleBounds(Visuals.FindRef(Key+Suffix).Get());if(Extra.IsValid)Bounds+=Extra;}
            }
            if(!Bounds.IsValid)return;
            FSeigeInspectionCandidate C;C.Ref={Kind,Sector,Id,WorkerId};C.Name=Name;C.Bounds=Bounds;C.Health=Health;C.MaximumHealth=Maximum;C.ConstructionProgress=Progress;C.ViewSector=View;Out.Add(MoveTemp(C));
        };
        const FVector2D Offset=FVector2D(Sector%3-1,Sector/3-1)*Sim.WorldHalfSize*2;
        if(Sector==View)
        {
            for(const auto& W:S.Workers.Bodies)if(W.State==TEXT("active")&&W.Outdoor)
                Add(ESeigeInspectKind::Worker,0,W.Id,Prefix+TEXT("worker_")+W.Id,TEXT("Worker ")+W.Id,W.Health,S.Workers.MaximumHealth(),W.Position+Offset,Sector);
            for(const auto& V:S.TransportVehicles.Bodies)if(!V.Embarked&&!V.Evacuated)
                Add(ESeigeInspectKind::Transport,V.Id,{},Prefix+FString::Printf(TEXT("transport_%d"),V.Id),FString::Printf(TEXT("%s #%d"),*S.TransportVehicles.Definition().Name,V.Id),V.Health,S.TransportVehicles.Definition().Health,V.Position+Offset,Sector);
            for(const auto& E:S.Enemies)
                Add(ESeigeInspectKind::Enemy,E.Id,{},Prefix+FString::Printf(TEXT("enemy_%d"),E.Id),FString::Printf(TEXT("Arachnid #%d"),E.Id),E.Health,S.EnemyMaximumHealth(),E.Position+Offset,Sector);
            for(const auto& B:S.Buildings)if(const auto* D=S.Definition(B))
            {
                Add(ESeigeInspectKind::Building,B.Id,{},Prefix+(D->Role==TEXT("wall")?TEXT("wall_"):TEXT("building_"))+FString::FromInt(B.Id),D->Name,B.Health,D->Health,B.Position+Offset,Sector,B.IsConstructing?RenderConstructionProgress(S,B):1.);
                if(B.Health<=0||(!B.IsConstructing&&D->InventoryPresentation!=TEXT("outdoor")))continue;
                const auto& Stock=B.IsConstructing?B.ConstructionMaterials:B.Inventory;
                const FString Key=Prefix+TEXT("building_")+FString::FromInt(B.Id)+(B.IsConstructing?TEXT("_stock_"):TEXT("_inventory_"));
                for(const auto& Item:Stock)if(Item.Value>UE_DOUBLE_SMALL_NUMBER)if(const auto* R=S.Resources.Find(Item.Key))
                {
                    const auto* A=Visuals.FindRef(Key+Item.Key).Get();const FBox Bounds=VisibleBounds(A);
                    if(!Bounds.IsValid||(!Observer&&Sector!=4&&!IsWorldVisible(FVector2D(A->GetActorLocation())/RenderScale)))continue;
                    FSeigeInspectionCandidate C;C.Ref={ESeigeInspectKind::Stockpile,Sector,B.Id,{},Item.Key,B.IsConstructing};
                    C.Name=R->Name;C.Bounds=Bounds;C.StockAmount=Item.Value;C.ViewSector=View;Out.Add(MoveTemp(C));
                }
            }
        }
        for(const auto& V:S.Combat.Vehicles)if(!V.Embarked&&!V.Evacuated)if(const auto* D=S.Combat.Chassis.Find(V.ChassisId))
        {
            const int32 Physical=V.SectorIndex==4?Sector:V.SectorIndex;
            const FVector2D P=V.Position+FVector2D(Physical%3-1,Physical/3-1)*Sim.WorldHalfSize*2;
            Add(ESeigeInspectKind::Vehicle,V.Id,{},Prefix+FString::Printf(TEXT("vehicle_%d"),V.Id),FString::Printf(TEXT("%s #%d"),*D->Name,V.Id),V.Health,D->Health,P,Physical);
        }
    };
    Gather(Sim,4);for(const auto& N:Neighbors)Gather(N.Sim,N.Index);
    if(View==4)for(const auto& Dog:Sim.Companions.Dogs)if(FSeigeCompanionSystem::IsOutside(Dog))
    {
        const auto* A=Visuals.FindRef(FString::Printf(TEXT("companion_%d"),Dog.Id)).Get();if(!IsValid(A)||A->IsHidden())continue;
        FSeigeInspectionCandidate C;C.Ref={ESeigeInspectKind::Companion,4,Dog.Id,{}};C.Name=Sim.Companions.Name;C.Health=Dog.Health;C.MaximumHealth=Sim.Companions.MaximumHealth;C.Bounds=VisibleBounds(A);if(C.Bounds.IsValid)Out.Add(MoveTemp(C));
    }
}

bool ASeigeHUD::InspectionAt(float X,float Y,const ASeigeGameMode& G,FSeigeInspectionCandidate& Out,const TArray<FSeigeInspectionCandidate>* KnownCandidates) const
{
    if(!CanInspect(G)||Ui.BuildOpen||ProgressionOpen||CombatPanelOpen||!Ui.HoverPanel.IsEmpty()||!Ui.HitTest(X,Y).IsEmpty())return false;
    if(HasUnitInspection()&&UnitInspectorBounds.bIsValid&&UnitInspectorBounds.IsInsideOrOn(FVector2D(X,Y)/Scale))return false;
    const auto* PC=Cast<ASeigeController>(GetOwningPlayerController());if(!PC)return false;
    FVector Origin,Direction;if(!PC->ScreenRay(FVector2D(X,Y),Origin,Direction))return false;
    FVector Terrain;const double Depth=G.TraceGroundRay(Origin,Direction,Terrain)?FVector::Distance(Origin,Terrain):TNumericLimits<double>::Max();
    TArray<FSeigeInspectionCandidate> Gathered;
    if(!KnownCandidates)G.GatherInspectionCandidates(Gathered);
    const TArray<FSeigeInspectionCandidate>& Candidates=KnownCandidates?*KnownCandidates:Gathered;
    TArray<FSeigeInspectionProjection> Projected;
    for(int32 I=0;I<Candidates.Num();++I)
    {
        FSeigeInspectionProjection P;P.Candidate=I;
        if(!ProjectBounds(*PC,Candidates[I].Bounds,P.Screen))continue;
        if(!SeigeInspection::RayBox(Origin,Direction,Candidates[I].Bounds,P.RayDepth))
        {
            // Only tiny units get a minimum pixel target, never an enormous
            // building footprint or a simulation-position-only click target.
            if(Candidates[I].Ref.Kind==ESeigeInspectKind::Building||P.Screen.GetSize().GetMax()>24*Scale)continue;
            const FVector2D Center=P.Screen.GetCenter(),Half(FMath::Max(7.*Scale,P.Screen.GetSize().X*.5),FMath::Max(7.*Scale,P.Screen.GetSize().Y*.5));
            P.Screen=FBox2D(Center-Half,Center+Half);P.RayDepth=FVector::DotProduct(Candidates[I].Bounds.GetCenter()-Origin,Direction);
        }
        Projected.Add(P);
    }
    const int32 Hit=SeigeInspection::Pick(Projected,FVector2D(X,Y),Depth);
    if(!Candidates.IsValidIndex(Hit))return false;Out=Candidates[Hit];return true;
}

void ASeigeHUD::ClearUnitInspection(){SelectedInspection={};SelectedInspectionView=INDEX_NONE;UnitInspectorBounds=FBox2D(ForceInit);SelectedWorkerId.Empty();HoveredWorkerId.Empty();}
bool ASeigeHUD::HasUnitInspection() const{return SelectedInspection.IsValid();}

bool ASeigeHUD::SelectUnitAt(float X,float Y,ASeigeGameMode& G)
{
    FSeigeInspectionCandidate C;
    if(!InspectionAt(X,Y,G,C)||C.Ref.Kind==ESeigeInspectKind::Building){ClearUnitInspection();return false;}
    SelectedInspection=C.Ref;SelectedInspectionView=G.DetailedSectorIndex();SelectedWorkerId.Empty();
    G.SelectedId=G.SelectedRoadId=G.SelectedCompanionId=0;Ui.CloseMenus();
    if(C.Ref.Kind==ESeigeInspectKind::Transport&&C.Ref.OwnerSector==4)TransportFocusId=C.Ref.Id;
    if(C.Ref.Kind==ESeigeInspectKind::Vehicle&&C.Ref.OwnerSector==4)if(const auto* V=G.Sim.Combat.FindVehicle(C.Ref.Id))
    {G.SelectedFleetId=V->FleetId;OutfitVehicleId=V->Id;G.Notice=TEXT("Vehicle selected. Fleet missions remain in the command-center fleet panel.");}
    if(C.Ref.Kind==ESeigeInspectKind::Companion)G.SelectedCompanionId=C.Ref.Id;
    SeigeAudio::PlayUi(GetWorld(),TEXT("select"));return true;
}

void ASeigeHUD::DrawWorldHoverStatus(ASeigeGameMode& G,const FSeigeInspectionCandidate* KnownHover)
{
    float X=0,Y=0;const auto* PC=GetOwningPlayerController();FSeigeInspectionCandidate Picked;
    if(!PC)return;
    if(!KnownHover&&(!PC->GetMousePosition(X,Y)||!InspectionAt(X,Y,G,Picked)))return;
    const FSeigeInspectionCandidate& C=KnownHover?*KnownHover:Picked;
    FBox2D Bounds;if(!ProjectBounds(*PC,C.Bounds,Bounds))return;
    if(C.Ref.Kind==ESeigeInspectKind::Stockpile)
    {
        const auto* S=InspectionOwner(G,C.Ref.OwnerSector);const auto* R=S?S->Resources.Find(C.Ref.ResourceId):nullptr;if(!R)return;
        const FString Caption=FString::Printf(TEXT("%s / %.2f %s"),*R->Name,C.StockAmount,*R->Unit);
        const FVector2D Size=MeasureLabel(Caption,12);const double CaptionX=Bounds.GetCenter().X/Scale-Size.X*.5,CaptionY=Bounds.Min.Y/Scale-24;
        Box(CaptionX-6,CaptionY-3,Size.X+12,21,FLinearColor(.015,.025,.03,.94));Label(Caption,CaptionX,CaptionY,12,Mint);return;
    }
    const auto Bars=SeigeInspection::Bars(Bounds,C.Health,C.MaximumHealth,C.ConstructionProgress,Scale);
    auto DrawBar=[&](const FBox2D& B,double Fraction,FLinearColor Color)
    {const FVector2D P=B.Min/Scale,S=B.GetSize()/Scale;Box(P.X-1,P.Y-1,S.X+2,S.Y+2,FLinearColor(.015,.025,.03,.94));Box(P.X,P.Y,S.X,S.Y,FLinearColor(.14,.20,.22,.96));Box(P.X,P.Y,S.X*Fraction,S.Y,Color);};
    DrawBar(Bars.Health,Bars.HealthFraction,Bars.HealthFraction<.3?Red:Mint);
    const FString HP=FString::Printf(TEXT("HP %.0f / %.0f"),C.Health,C.MaximumHealth);
    Label(HP,(Bars.Health.GetCenter().X/Scale)-MeasureLabel(HP,12).X*.5,Bars.Health.Min.Y/Scale-17,12,Text);
    if(Bars.HasProgress)
    {
        DrawBar(Bars.Progress,Bars.ProgressFraction,Gold);
        const FString Progress=Bars.ProgressFraction>=1?TEXT("Built 100%"):FString::Printf(TEXT("Construction %.0f%%"),Bars.ProgressFraction*100);
        Label(Progress,Bars.Progress.GetCenter().X/Scale-MeasureLabel(Progress,12).X*.5,Bars.Progress.Max.Y/Scale+5,12,Text);
    }
}

void ASeigeHUD::DrawUnitInspector(ASeigeGameMode& G,float W,float H)
{
    if(!CanInspect(G)||CombatPanelOpen||Ui.BuildOpen||ProgressionOpen||!Ui.HoverPanel.IsEmpty())return;
    TArray<FSeigeInspectionCandidate> Candidates;G.GatherInspectionCandidates(Candidates);
    if(SelectedInspection.IsValid()&&(SelectedInspectionView!=G.DetailedSectorIndex()||!Candidates.ContainsByPredicate([&](const auto& C){return C.Ref==SelectedInspection;})))ClearUnitInspection();
    FSeigeInspectionCandidate C;float MX=0,MY=0;const auto* PC=GetOwningPlayerController();
    // One physical-bounds scan serves hover bars, picking and pinned details.
    // The array is scoped to this draw, so same-frame fog/health/world changes
    // or independent automation fixtures cannot inherit stale candidates.
    const bool Hovered=PC&&PC->GetMousePosition(MX,MY)&&InspectionAt(MX,MY,G,C,&Candidates);
    if(Hovered)DrawWorldHoverStatus(G,&C);
    const bool Hover=Hovered&&C.Ref.Kind!=ESeigeInspectKind::Building;
    if(!Hover)
    {
        const auto* Pinned=Candidates.FindByPredicate([&](const auto& Item){return Item.Ref==SelectedInspection;});if(!Pinned)return;C=*Pinned;
    }
    // Rex's existing panel owns Roam/Return controls when selected. A second
    // inspector must not cover those buttons; his world health bar still draws.
    if(C.Ref.Kind==ESeigeInspectKind::Companion&&G.SelectedCompanionId==C.Ref.Id)return;
    const bool Pinned=C.Ref==SelectedInspection;const auto* S=InspectionOwner(G,C.Ref.OwnerSector);if(!S)return;
    const float PW=FMath::Min(440.f,W-48),PH=FMath::Min(550.f,H-Ui.ContentTop-98),X=W-PW-24,Y=Ui.ContentTop;
    if(PH<160)return;
    UnitInspectorBounds=FBox2D(FVector2D(X,Y),FVector2D(X+PW,Y+PH));
    Frame(X,Y,PW,PH);Label(C.Ref.Kind==ESeigeInspectKind::Worker?TEXT("WORKER / ")+C.Ref.WorkerId:C.Name.ToUpper(),X+18,Y+18,16,Gold);
    if(Pinned)Button(TEXT("x"),TEXT("deselect"),X+PW-40,Y+8,30,30);
    float TY=Y+57;
    auto Line=[&](const FString& Value,FLinearColor Color=Text)
    {for(const auto& L:WrapLines(Value,PW-36,13)){if(TY+20>Y+PH-36)return;Label(L,X+18,TY,13,Color);TY+=21;}TY+=7;};
    auto Building=[&](int32 Id)->FString{if(const auto* B=S->FindBuilding(Id))if(const auto* D=S->Definition(*B))return FString::Printf(TEXT("%s #%d"),*D->Name,Id);return Id?TEXT("Unavailable assignment"):TEXT("Unassigned");};
    auto Delivery=[&](int32 Id)
    {
        const auto* D=S->Couriers.FindByPredicate([&](const auto& D){return D.Id==Id;});
        if(!D){Line(TEXT("Cargo: empty / no active delivery"),Muted);return;}
        const auto* R=S->Resources.Find(D->Resource);
        Line(FString::Printf(TEXT("Delivery #%d / %s"),D->Id,*D->Phase.Replace(TEXT("_"),TEXT(" "))));
        Line(FString::Printf(TEXT("Cargo %.2f %s %s / %.2f reserved for pickup"),D->Amount,R?*R->Unit:TEXT(""),R?*R->Name:*D->Resource,D->ReservedAmount));
        Line(TEXT("From: ")+Building(D->SourceId),Muted);Line(D->RoadTargetId?FString::Printf(TEXT("To: road #%d"),D->RoadTargetId):TEXT("To: ")+Building(D->TargetId),Muted);
    };
    if(C.Ref.Kind!=ESeigeInspectKind::Stockpile)Line(FString::Printf(TEXT("Health %.0f / %.0f HP"),C.Health,C.MaximumHealth),C.Health<C.MaximumHealth*.3?Red:Mint);
    if(C.Ref.Kind==ESeigeInspectKind::Stockpile)if(const auto* R=S->Resources.Find(C.Ref.ResourceId))
    {
        Line(FString::Printf(TEXT("Quantity %.2f %s"),C.StockAmount,*R->Unit),Mint);
        Line(FString::Printf(TEXT("Mass %.2f kg / volume %.2f L"),C.StockAmount*R->UnitMassKg,C.StockAmount*R->LitresPerUnit));
        Line(TEXT("At: ")+Building(C.Ref.Id));
        Line(C.Ref.ConstructionStock?TEXT("Delivered construction materials, waiting to be installed."):TEXT("Outdoor inventory, available for physical pickup."),Muted);
        double Claimed=0;if(!C.Ref.ConstructionStock)for(const auto& Shipment:S->Couriers)if(Shipment.SourceId==C.Ref.Id&&Shipment.Resource==C.Ref.ResourceId)Claimed+=Shipment.ReservedAmount;
        Claimed=FMath::Clamp(Claimed,0.,C.StockAmount);
        Line(FString::Printf(TEXT("Reserved %.2f %s / unclaimed %.2f %s"),C.Ref.ConstructionStock?C.StockAmount:Claimed,*R->Unit,C.Ref.ConstructionStock?0.:C.StockAmount-Claimed,*R->Unit));
        Line(C.Ref.ConstructionStock?TEXT("Builders install these materials as work progresses."):TEXT("Workers and transport vehicles collect goods from here."),Muted);
    }
    if(C.Ref.Kind==ESeigeInspectKind::Worker)if(const auto* Worker=S->Workers.Find(C.Ref.WorkerId))
    {
        Line(Worker->State.ToUpper()+TEXT(" / ")+Worker->Activity.Replace(TEXT("_"),TEXT(" ")),Mint);
        Line(TEXT("Permanent crew: ")+Building(Worker->CrewBuildingId));
        if(Worker->BuildingId)Line(TEXT("Current workplace: ")+Building(Worker->BuildingId));
        if(Worker->RoadId)Line(FString::Printf(TEXT("Road worksite #%d"),Worker->RoadId));
        Line(FString::Printf(TEXT("Battery %.2f / %.2f kWh"),Worker->BatteryKWh,Worker->BatteryCapacityKWh));
        if(Worker->ChargingBuildingId)Line(TEXT("Charging at: ")+Building(Worker->ChargingBuildingId),Muted);
        Delivery(Worker->DeliveryId);if(Worker->TransportVehicleId)Line(FString::Printf(TEXT("Handling transport #%d"),Worker->TransportVehicleId));
    }
    if(C.Ref.Kind==ESeigeInspectKind::Transport)if(const auto* V=S->TransportVehicles.Find(C.Ref.Id))
    {
        const auto& D=S->TransportVehicles.Definition();Line(V->Status,Mint);
        Line(FString::Printf(TEXT("Battery %.2f / %.2f kWh"),V->BatteryKWh,D.BatteryKWh));
        Line(FString::Printf(TEXT("Cargo capacity %.0f kg / %.0f L"),D.CargoMassKg,D.CargoLitres));
        Line(V->HandlerWorkerId.IsEmpty()?TEXT("Handler: no worker currently handling cargo"):TEXT("Cargo handler: ")+V->HandlerWorkerId,Muted);
        Delivery(V->DeliveryId);Line(FString::Printf(TEXT("Distance travelled %.1f m"),V->DistanceMeters),Muted);
    }
    if(C.Ref.Kind==ESeigeInspectKind::Vehicle)if(const auto* V=S->Combat.FindVehicle(C.Ref.Id))if(const auto* D=S->Combat.Chassis.Find(V->ChassisId))
    {
        Line(V->Status,Mint);Line(FString::Printf(TEXT("Fleet %d / %d capacity points"),V->FleetId,D->CapacityPoints));
        if(const auto* F=S->Combat.Fleets.FindByPredicate([&](const auto& F){return F.Id==V->FleetId;}))Line(F->Name+TEXT(" / ")+F->Mission+TEXT(" / ")+F->Aggression,Muted);
        Line(FString::Printf(TEXT("Shield %.0f / %.0f / armor %.0f / %.0f"),V->Shield,D->Shield,V->Armor,D->Armor));
        Line(FString::Printf(TEXT("Battery %.2f / %.2f kWh"),V->BatteryKWh,D->BatteryKWh));
        double Mass=0,Litres=0;TArray<FString> Cargo;for(const auto& Entry:V->Inventory)if(Entry.Value>0)if(const auto* R=S->Resources.Find(Entry.Key))
        {Cargo.Add(FString::Printf(TEXT("%.1f %s %s"),Entry.Value,*R->Unit,*R->Name));Mass+=Entry.Value*R->UnitMassKg;Litres+=Entry.Value*R->LitresPerUnit;}
        Line(FString::Printf(TEXT("Cargo %.1f / %.0f kg; %.1f / %.0f L"),Mass,D->CargoMassKg,Litres,D->StorageLitres));
        Cargo.Sort();Line(Cargo.IsEmpty()?TEXT("Cargo: empty"):FString::Join(Cargo,TEXT(" / ")),Muted);
        TArray<FString> Weapons;for(const auto& Id:V->Weapons)if(const auto* Weapon=S->Combat.Weapons.Find(Id))Weapons.Add(Weapon->Name);
        Line(Weapons.IsEmpty()?TEXT("Weapons: none"):TEXT("Weapons: ")+FString::Join(Weapons,TEXT(", ")),Muted);
    }
    if(C.Ref.Kind==ESeigeInspectKind::Companion)if(const auto* Dog=S->Companions.Find(C.Ref.Id))
    {Line(Dog->Retreating?TEXT("Running to shuttle shelter"):Dog->ExitingShuttle?TEXT("Leaving shuttle"):Dog->Moving?TEXT("Roaming"):TEXT("Resting"),Mint);Line(FString::Printf(TEXT("Home territory: %.0f m around the orbital command module"),S->Companions.TerritoryRadiusMeters));Line(TEXT("Returns to shelter when hurt or threatened."),Muted);}
    if(C.Ref.Kind==ESeigeInspectKind::Enemy)
    {Line(TEXT("Hostile arachnid"),Red);const auto* E=S->Enemies.FindByPredicate([&](const auto& E){return E.Id==C.Ref.Id;});if(E&&E->TargetBuildingId)Line(TEXT("Attacking: ")+Building(E->TargetBuildingId));}
    Label(Pinned?TEXT("Selected / Esc or x clears selection"):TEXT("Click to keep these details open"),X+18,Y+PH-24,11,Muted);
}
