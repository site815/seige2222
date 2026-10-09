#include "SeigeGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"

void ASeigeGameMode::BeginWallPlacement()
{
    if(Observer||Screen!=TEXT("playing")||DetailedSectorIndex()!=4||IsRegionMap()||Sim.Escaped||Sim.Failed)return;
    CancelRoadTool();CancelWallTool();SelectedId=SelectedRoadId=SelectedCompanionId=0;SelectedBuild.Empty();WallPlacementActive=true;
    Notice=TEXT("Wall plan: click to add joints. Click a joint then its new position to move it. Click an edge to insert. E flips inside; Enter commits; Delete removes a selected joint.");
}
void ASeigeGameMode::CancelWallTool(){WallPlacementActive=false;WallJoints.Reset();SelectedWallJoint=INDEX_NONE;WallInsideLeft=true;}
void ASeigeGameMode::ClickWallPlan()
{
    if(!WallPlacementActive||!CursorOnWorld)return;
    const double Radius=Sim.Walls.JointPickRadiusMeters/Sim.MetersPerWorldUnit();
    if(WallJoints.IsValidIndex(SelectedWallJoint)){WallJoints[SelectedWallJoint]=CursorWorld;SelectedWallJoint=INDEX_NONE;return;}
    for(int32 I=0;I<WallJoints.Num();++I)if(FVector2D::Distance(WallJoints[I],CursorWorld)<Radius){SelectedWallJoint=I;return;}
    if(WallJoints.Num()>=Sim.Walls.MaximumJoints){Notice=TEXT("Wall plan has reached the joint limit");return;}
    for(int32 I=1;I<WallJoints.Num();++I){const auto D=WallJoints[I]-WallJoints[I-1];const double T=FVector2D::DotProduct(CursorWorld-WallJoints[I-1],D)/FMath::Max(D.SizeSquared(),1.e-6);
        if(T>.1&&T<.9&&FVector2D::Distance(CursorWorld,WallJoints[I-1]+D*T)<Radius){WallJoints.Insert(CursorWorld,I);SelectedWallJoint=I;return;}}
    WallJoints.Add(CursorWorld);
}
bool ASeigeGameMode::WallShortcut(const FKey& Key)
{
    if(!WallPlacementActive||Screen!=TEXT("playing"))return false;
    if(Key==EKeys::Escape||Key==EKeys::RightMouseButton){CancelWallTool();return true;}
    if(Key==EKeys::E){WallInsideLeft=!WallInsideLeft;return true;}
    if(Key==EKeys::BackSpace){if(WallJoints.Num())WallJoints.Pop();SelectedWallJoint=INDEX_NONE;return true;}
    if(Key==EKeys::Delete){if(WallJoints.IsValidIndex(SelectedWallJoint))WallJoints.RemoveAt(SelectedWallJoint);SelectedWallJoint=INDEX_NONE;return true;}
    if(Key==EKeys::Enter){if(Sim.Walls.Commit(Sim,WallJoints,WallInsideLeft,Error)){CancelWallTool();RefreshBuildingPads();Notice=TEXT("Wall plan committed; couriers and workers will build each section.");}else Notice=Error;return true;}
    return false;
}
void ASeigeGameMode::SyncWallVisuals(const FSeigeSimulation& Colony,FVector2D Offset,const FString& Prefix,TSet<FString>& Live)
{
    for(const auto& S:Colony.Walls.Segments)
    {
        const auto* B=Colony.FindBuilding(S.BuildingId);if(!B||B->Health<=0)continue;const auto* D=Colony.Definition(*B);if(!D)continue;
        if(!Observer&&!Offset.IsNearlyZero()&&(!IsWorldVisible(S.A+Offset)||!IsWorldVisible(S.B+Offset)))continue;
        const int32 Level=FMath::Max(0,Colony.Walls.LevelDefinitions.IndexOfByKey(B->DefId));
        const double Height=Colony.Walls.HeightMeters.IsValidIndex(Level)?Colony.Walls.HeightMeters[Level]*100.:300.;
        const double Progress=B->IsConstructing?FMath::Max(.035,RenderConstructionProgress(Colony,*B)):1.;
        const FString Key=Prefix+FString::Printf(TEXT("wall_%d"),B->Id);Live.Add(Key);
        const FName Signature(*FString::Printf(TEXT("wall_%s"),*B->DefId));auto* A=Visuals.FindRef(Key).Get();
        if(A&&!A->ActorHasTag(Signature)){A->Destroy();Visuals.Remove(Key);A=nullptr;}
        const FVector Start=RenderPosition(S.A+Offset),End=RenderPosition(S.B+Offset);const double Length=(End-Start).Size();
        if(!A)
        {
            A=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(A);A->SetRootComponent(Root);Root->RegisterComponent();A->Tags.Add(Signature);Visuals.Add(Key,A);
            // Kit wall sections (Tools/create_building_kit_v092.py wall_section):
            // a 6 m section authored with its length on X and its inside on +Y,
            // scaled to this segment's length and the level's rules height and
            // turned half round when the inside is on the right. The cube
            // assembly remains the fallback when the mesh is not imported.
            FString Kind=BuildingVisualKind(*D);if(Kind.IsEmpty())Kind=TEXT("wall");Kind[0]=FChar::ToUpper(Kind[0]);
            UStaticMesh* Section=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/Art/SM_%s.SM_%s"),*Kind,*Kind),nullptr,LOAD_NoWarn);
            if(Section)
            {
                const FVector Size=Section->GetBoundingBox().GetSize();
                auto* Mesh=NewObject<UStaticMeshComponent>(A);Mesh->SetMobility(EComponentMobility::Movable);Mesh->SetStaticMesh(Section);
                Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetupAttachment(Root);
                Mesh->SetRelativeScale3D(FVector(Length/FMath::Max(Size.X,1.),1,Height/FMath::Max(Size.Z,1.)));
                Mesh->SetRelativeRotation(FRotator(0,S.InsideLeft?0:180,0));
                Mesh->RegisterComponent();A->AddInstanceComponent(Mesh);
            }
            else
            {
                Part(A,TEXT("Cube"),FVector(0,0,Height*.5),FVector(Length/100.,Colony.Walls.WidthMeters,Height/100.),D->Color);
                Part(A,TEXT("Cube"),FVector(0,0,Height-15),FVector(Length/100.+.12,Colony.Walls.WidthMeters+.2,.3),FLinearColor(.19,.24,.25));
                for(double X:{-.47,.47})Part(A,TEXT("Cube"),FVector(Length*X,0,Height*.5),FVector(.3,Colony.Walls.WidthMeters+.3,Height/100.+.2),FLinearColor(.32,.37,.38));
                Part(A,TEXT("Cube"),FVector(0,(S.InsideLeft?1:-1)*(Colony.Walls.WidthMeters*50+2),Height*.7),FVector(Length/100.*.86,.04,.07),FLinearColor(.25,.6,.55));
            }
            ClearSceneryAt(B->Position+Offset,D->ReservedFootprint);
        }
        A->SetActorLocation((Start+End)*.5);A->SetActorRotation((End-Start).Rotation());A->SetActorScale3D(FVector(1,1,Progress));
        // Existing construction presentation supplies physical stock and crews.
        if(B->IsConstructing){SyncConstructionVisuals(Colony,*B,*D,B->Position+Offset,Key,Live);A->SetActorScale3D(FVector(1,1,Progress));}
    }
}
void ASeigeGameMode::SyncWallGhost(TSet<FString>& Live)
{
    if(!WallPlacementActive||Screen!=TEXT("playing")||IsRegionMap())return;
    FSeigeWallPlan Plan;FString Why;const bool Valid=Sim.Walls.Plan(Sim,WallJoints,WallInsideLeft,Plan,Why);const FLinearColor Color=Valid?FLinearColor(.2,.75,.5,.3):FLinearColor(.8,.25,.2,.3);
    for(int32 I=1;I<WallJoints.Num();++I)
    {
        const FString Key=FString::Printf(TEXT("wall_ghost_%d"),I);Live.Add(Key);auto* A=Visuals.FindRef(Key).Get();
        if(!A){A=GetWorld()->SpawnActor<AActor>();auto* Mesh=NewObject<UStaticMeshComponent>(A);A->SetRootComponent(Mesh);Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->RegisterComponent();A->AddInstanceComponent(Mesh);Visuals.Add(Key,A);}
        const FVector Start=RenderPosition(WallJoints[I-1],150),End=RenderPosition(WallJoints[I],150);A->SetActorLocation((Start+End)*.5);A->SetActorRotation((End-Start).Rotation());A->SetActorScale3D(FVector((End-Start).Size()/100.,Sim.Walls.WidthMeters,3));
        if(auto* Mesh=A->FindComponentByClass<UStaticMeshComponent>())Mesh->SetMaterial(0,ConstructionMaterial(Color));
        const FString SideKey=FString::Printf(TEXT("wall_inside_%d"),I);Live.Add(SideKey);auto* Marker=Visuals.FindRef(SideKey).Get();
        if(!Marker){Marker=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(Marker);Marker->SetRootComponent(Root);Root->RegisterComponent();Visuals.Add(SideKey,Marker);Part(Marker,TEXT("Cone"),FVector::ZeroVector,FVector(.5,.5,1.5),FLinearColor(.25f,.8f,.65f),FRotator(90,0,0));}
        const auto D=(WallJoints[I]-WallJoints[I-1]).GetSafeNormal();const FVector2D N(-D.Y,D.X);const auto Direction=N*(WallInsideLeft?1.:-1.);
        Marker->SetActorLocation(RenderPosition((WallJoints[I]+WallJoints[I-1])*.5+Direction*70,80));Marker->SetActorRotation(FVector(Direction,0).Rotation());Marker->SetActorHiddenInGame(false);
    }
    for(int32 I=0;I<WallJoints.Num();++I)
    {
        const FString Key=FString::Printf(TEXT("wall_joint_%d"),I);Live.Add(Key);auto* A=Visuals.FindRef(Key).Get();
        if(!A){A=GetWorld()->SpawnActor<AActor>();auto* Root=NewObject<USceneComponent>(A);A->SetRootComponent(Root);Root->RegisterComponent();Visuals.Add(Key,A);Part(A,TEXT("Sphere"),FVector::ZeroVector,FVector(.55),FLinearColor(.85f,.75f,.4f));}
        A->SetActorLocation(RenderPosition(WallJoints[I],90));A->SetActorScale3D(FVector(I==SelectedWallJoint?1.8:1.));A->SetActorHiddenInGame(false);
    }
}
