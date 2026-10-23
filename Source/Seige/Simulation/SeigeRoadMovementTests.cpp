#include "SeigeSimulation.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRoadMovementRegressionTest,"Seige.Transport.PhysicalRoadRoutingAndDelivery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeRoadMovementRegressionTest::RunTest(const FString&)
{
    FSeigeSimulation Base;FString Error;
    if(!Base.Initialize(FPaths::Combine(FPaths::ProjectDir(),TEXT("Rules")),Error,false,false)){AddError(Error);return false;}
    Base.SetRoadsEnabledForTest(true);
    Base.Environment.Enabled=false;Base.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    Base.Buildings[0].IsConstructing=false;Base.Buildings[0].ConstructionProgress=1;
    Base.DeploymentHatchOpen=true;Base.DeploymentGrounded=true;
    const int32 CoreId=Base.Buildings[0].Id;const FVector2D Port=Base.BuildingAccessPoint(Base.Buildings[0]);
    // This is an authored completed road fixture. The delivery below uses only
    // the finite landing cargo and existing body identities, with no production.
    FSeigeTransportSegment Road;Road.Id=900;Road.A=Port;Road.B=Port+FVector2D(5000,0);Road.Tier=Road.TargetTier=Base.InitialRoadTier();Road.Health=Road.MaxHealth=1000;Road.IsConstructing=false;Road.ConstructionProgress=1;
    Base.Roads.Add(Road);++Base.TransportRevision;Base.Energy.Invalidate();Base.Energy.Tick(Base,.1);
    if(!TestTrue(TEXT("Fixture road connects to the live fusion source"),Base.Energy.RoadPowered(Road.Id)))return false;
    const FVector2D Start=Port+FVector2D(400,120),End=Port+FVector2D(3500,120);
    TestTrue(TEXT("New branches snap to the middle of an existing road"),Base.SnapRoadPoint(Port+FVector2D(1600,30)).Equals(Port+FVector2D(1600,0),1.e-8));
    for(bool Vehicle:{false,true})
    {
        FSeigeSimulation S=Base;auto& W=S.Workers.Bodies[0];auto& V=S.TransportVehicles.Bodies[0];
        W.Position=V.Position=Start;W.Route.Empty();V.Route.Empty();W.NextWaypoint=V.NextWaypoint=0;W.RetryAt=V.RetryAt=0;V.Embarked=false;
        const double BodyCount=S.Workers.Bodies.Num(),Stock=S.TotalStock(TEXT("alloy"));
        bool Arrived=false,UsedInterior=false,Continuous=true,Clear=true;
        const double Dt=.1,Maximum=(Vehicle?30.:10.)/3.6/S.MetersPerWorldUnit()*2*Dt;
        for(int I=0;I<1200&&!Arrived;++I)
        {
            const FVector2D Before=Vehicle?V.Position:W.Position;
            Arrived=Vehicle?S.TransportVehicles.Move(S,V,End,Dt):S.Workers.Move(S,W,End,Dt);
            const FVector2D After=Vehicle?V.Position:W.Position;S.Time+=Dt;
            UsedInterior|=After.X>Start.X&&After.X<End.X&&FMath::Abs(After.Y-Road.A.Y)<.001;
            Continuous&=FVector2D::Distance(Before,After)<=Maximum+1.e-6;
            Clear&=S.ClearWalkingLine(Before,After,S.Mobility.ClearanceMeters(Vehicle?TEXT("transport"):TEXT("drone"))/S.MetersPerWorldUnit());
        }
        TestTrue(Vehicle?TEXT("Transport physically joins and leaves a road interior"):TEXT("Worker physically joins and leaves a road interior"),UsedInterior&&Arrived);
        TestTrue(TEXT("Road traversal remains continuous and outside reserved plots"),Continuous&&Clear);
        TestEqual(TEXT("Movement creates no extra worker body"),double(S.Workers.Bodies.Num()),BodyCount);
        TestEqual(TEXT("Movement creates or consumes no cargo"),S.TotalStock(TEXT("alloy")),Stock);
    }
    {
        FSeigeSimulation S=Base;auto& W=S.Workers.Bodies[0];W.Position=Start;W.Route={Start};W.NextWaypoint=1;W.RouteRevision=S.TransportRevision;
        TestFalse(TEXT("An exhausted old route cannot report arrival at a new destination"),S.Workers.Move(S,W,End,.1));
        TestTrue(TEXT("A retargeted worker moves continuously toward a newly solved path"),!W.Position.Equals(Start,1.e-8)&&FVector2D::Distance(Start,W.Position)<=S.WalkingSpeed()*2*.1+1.e-6);
        const FVector2D Other=Start+FVector2D(0,400);const FVector2D Before=W.Position;
        TestFalse(TEXT("A live old route also cannot fulfill a changed task"),S.Workers.Move(S,W,Other,.1));
        TestTrue(TEXT("The replacement path ends at the requested task"),!W.Route.IsEmpty()&&W.Route.Last().Equals(Other,1.e-8));
        TestTrue(TEXT("Changing task does not teleport the body"),FVector2D::Distance(Before,W.Position)<=S.WalkingSpeed()*2*.1+1.e-6);
    }
    {
        FSeigeSimulation S=Base;FSeigeTransportSegment Branch=Road;Branch.Id=901;Branch.A=Port+FVector2D(2000,-1500);Branch.B=Port+FVector2D(2000,1500);S.Roads.Add(Branch);++S.TransportRevision;S.Energy.Invalidate();S.Energy.Tick(S,.1);
        auto& W=S.Workers.Bodies[0];W.Position=Port+FVector2D(2000,-1000);W.Route.Empty();W.NextWaypoint=0;
        bool Arrived=false,Vertical=false,Horizontal=false;
        for(int I=0;I<1200&&!Arrived;++I)
        {
            Arrived=S.Workers.Move(S,W,End,.1);S.Time+=.1;
            Vertical|=FMath::Abs(W.Position.X-Branch.A.X)<.001&&W.Position.Y<Port.Y-100;
            Horizontal|=FMath::Abs(W.Position.Y-Port.Y)<.001&&W.Position.X>Branch.A.X+100;
        }
        TestTrue(TEXT("Physical movement changes corridors at a crossing junction"),Arrived&&Vertical&&Horizontal);
        S.Roads[0].TargetTier=TEXT("road_rail");S.Roads[0].IsConstructing=true;
        TestEqual(TEXT("An upgrade preserves the completed road's current travel service"),S.RouteSpeedMultiplier(Road.A,Road.B),2.);
    }
    {
        FSeigeSimulation S=Base;S.Buildings[0].Enabled=false;S.Buildings[0].BatteryEnergyKWh=0;S.Energy.Invalidate();S.Energy.Tick(S,1);
        TestFalse(TEXT("The disconnected power fixture has no road boost"),S.Energy.RoadPowered(Road.Id));
        TestTrue(TEXT("A completed unpowered road remains a road"),S.HasRoadLink(Road.A,Road.B));
        S.Mobility.SetTerrainSampler([](FVector2D P){return P.X*.1;});
        auto& V=S.TransportVehicles.Bodies[0];V.Position=Road.A;V.Route.Empty();V.NextWaypoint=0;
        const FVector2D Before=V.Position;S.TransportVehicles.Move(S,V,Road.B,1);
        TestTrue(TEXT("An unpowered road uses base travel speed, not off-road rough penalty"),FMath::IsNearlyEqual(FVector2D::Distance(Before,V.Position)*S.MetersPerWorldUnit()*3.6,30.,1.e-6));
        S.Roads[0].Tier.Empty();++S.TransportRevision;
        TestFalse(TEXT("An unfinished corridor is not traversable as a completed road"),S.HasRoadLink(Road.A,Road.B));
        TestEqual(TEXT("An unfinished corridor grants no speed bonus"),S.RouteSpeedMultiplier(Road.A,Road.B),1.);
    }
    {
        FSeigeSimulation S=Base;S.Buildings.Empty();S.Roads.Empty();++S.TransportRevision;
        // A 0.6m-wide 30% bank is hidden by the old five-metre endpoint
        // average, but rejected by the actual vehicle's short movement step.
        // Planning must reject it too, rather than retry that same bad edge.
        S.Mobility.SetTerrainSampler([](FVector2D P){return FMath::Clamp(P.X-30.,0.,10.)*.3;});
        const FVector2D A(0,0),B(1000,0);TArray<FVector2D> Route;
        TestFalse(TEXT("Long-edge slope validation catches the narrow impassable bank"),S.Mobility.MovementAllowed(S,TEXT("transport"),A,B));
        TestFalse(TEXT("The planner does not cache a false successful route through the bank"),S.FindMobilityRoute(TEXT("transport"),A,B,Route));
        auto& V=S.TransportVehicles.Bodies[0];V.Position=A;V.Route.Empty();V.NextWaypoint=0;V.RetryAt=0;
        TestFalse(TEXT("Actual movement remains waiting for a genuinely traversable route"),S.TransportVehicles.Move(S,V,B,1));
        TestTrue(TEXT("A rejected cliff cannot teleport the vehicle onto its far side"),V.Position.Equals(A,1.e-8));
    }
    for(bool Vehicle:{false,true})
    {
        FSeigeSimulation S=Base;S.Buildings.Empty();S.Roads.Empty();++S.TransportRevision;
        // No road or building corners exist to describe the contour: there is
        // a real legal detour around this hill, but the straight line is steep.
        S.Mobility.SetTerrainSampler([](FVector2D P){return 400*FMath::Exp(-FMath::Square(P.X/250.)-FMath::Square(P.Y/400.));});
        const FVector2D A(-1500,0),B(1500,0);const FString Profile=Vehicle?TEXT("transport"):TEXT("drone");
        TestFalse(TEXT("Contour fixture really blocks the straight slope"),S.Mobility.MovementAllowed(S,Profile,A,B));
        auto& V=S.TransportVehicles.Bodies[0];auto& W=S.Workers.Bodies[0];V.Position=W.Position=A;V.Route.Empty();W.Route.Empty();V.NextWaypoint=W.NextWaypoint=0;V.RetryAt=W.RetryAt=0;V.Embarked=false;
        bool Arrived=false,Detoured=false,Legal=true;const double Stock=S.TotalStock(TEXT("alloy"));const int32 Bodies=S.Workers.Bodies.Num();
        for(int32 I=0;I<4000&&!Arrived;++I)
        {
            const FVector2D Before=Vehicle?V.Position:W.Position;
            Arrived=Vehicle?S.TransportVehicles.Move(S,V,B,.1):S.Workers.Move(S,W,B,.1);
            const FVector2D After=Vehicle?V.Position:W.Position;
            Detoured|=FMath::Abs(After.Y)>100;
            Legal&=S.Mobility.MovementAllowed(S,Profile,Before,After)&&FVector2D::Distance(Before,After)<=S.Mobility.Profile(Profile)->SpeedKmh/3.6/S.MetersPerWorldUnit()*.1+1.e-6;
            S.Time+=.1;
        }
        TestTrue(Vehicle?TEXT("Actual truck follows a legal hillside contour to the exact destination"):TEXT("Actual worker follows a legal hillside contour to the exact destination"),Arrived&&Detoured&&Legal&&(Vehicle?V.Position:W.Position).Equals(B,1.e-6));
        TestEqual(TEXT("Contour routing preserves physical body count"),S.Workers.Bodies.Num(),Bodies);
        TestEqual(TEXT("Contour routing preserves all cargo"),S.TotalStock(TEXT("alloy")),Stock);
        // An infinite cliff has no legal contour. A bounded failed search must
        // leave the real body where it is, rather than relaxing slope limits.
        S.Mobility.SetTerrainSampler([](FVector2D P){return P.X>=0?10000.:0.;});
        V.Position=W.Position=A;V.Route.Empty();W.Route.Empty();V.NextWaypoint=W.NextWaypoint=0;V.RetryAt=W.RetryAt=0;
        TestFalse(TEXT("Impassable cliff remains blocked after contour fallback"),Vehicle?S.TransportVehicles.Move(S,V,B,.1):S.Workers.Move(S,W,B,.1));
        TestTrue(TEXT("An impassable contour search never teleports the body"),(Vehicle?V.Position:W.Position).Equals(A,1.e-8));
    }
    {
        FSeigeSimulation S=Base;S.Buildings.Empty();S.Roads.Empty();++S.TransportRevision;
        S.Mobility.SetTerrainSampler([](FVector2D P){return P.X*.9;});
        auto& W=S.Workers.Bodies[0];const FVector2D StartPoint(0,0),Goal(1000,0);W.Position=StartPoint;W.Route={Goal};W.NextWaypoint=0;W.RouteRevision=S.TransportRevision;W.RetryAt=0;
        TestFalse(TEXT("A worker rechecks the actual movement slice of a stale exterior route"),S.Workers.Move(S,W,Goal,.05));
        TestTrue(TEXT("Rejected worker movement retains physical position and schedules a bounded retry"),W.Position.Equals(StartPoint,1.e-8)&&W.Route.IsEmpty()&&W.RetryAt>S.Time);
    }
    {
        FSeigeSimulation S=Base;S.Buildings.Empty();++S.TransportRevision;
        const FVector2D A(2246.825003763547,5000),B(507.46721314877448,4000);TArray<FVector2D> Route;
        if(TestTrue(TEXT("Visibility fixture has an exact direct waypoint"),S.FindMobilityRoute(TEXT("transport"),A,B,Route)))
        {
            TestTrue(TEXT("Route output preserves the original endpoint bits used by edge validation"),Route.Last().X==B.X&&Route.Last().Y==B.Y);
            S.RejectMobilityRouteEdge(TEXT("transport"),A,Route[0]);
            for(int32 I=0;I<131074;++I)S.CachedRouteEdges.Add(FString::Printf(TEXT("eviction-fixture-%d"),I),1.);
            TestTrue(TEXT("Planner memoization eviction retains an observed physical edge failure"),S.FindMobilityRoute(TEXT("transport"),A,B,Route)&&Route.Num()>1);
            S.Mobility.InvalidateTerrain();
            TestTrue(TEXT("Changed terrain permits rechecking the physical rejection"),S.FindMobilityRoute(TEXT("transport"),A,B,Route)&&Route.Num()==1);
        }
    }
    // Real cargo state transitions: reserve, walk to pickup, load, travel on
    // the road, unload. The same assertions cover autonomous vehicle handlers.
    for(int32 Scenario=0;Scenario<3;++Scenario)
    {
        FSeigeSimulation S=Base;const int32 Depot=S.AddReviewBuilding(TEXT("depot"),Port+FVector2D(3230,700));
        const bool Vehicle=Scenario>0,AlternateBerths=Scenario==2;
        if(!TestTrue(TEXT("Authored delivery destination exists"),Depot>0))return false;
        for(auto& W:S.Workers.Bodies){W.Activity=TEXT("operate");W.CrewBuildingId=W.BuildingId=CoreId;W.Outdoor=true;W.ContainerId=0;W.ContainerKind.Empty();W.Position=Port+FVector2D(0,20);W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;}
        S.Workers.Bodies.Last().CrewBuildingId=Depot;S.Workers.Bodies.Last().BuildingId=Depot;S.Workers.Bodies.Last().Position=S.BuildingAccessPoint(*S.FindBuilding(Depot));
        auto& V=S.TransportVehicles.Bodies[0];V.Position=Port;V.Status=TEXT("Available");V.Embarked=false;
        if(AlternateBerths)
        {
            const FVector2D Receiver=S.BuildingAccessPoint(*S.FindBuilding(Depot));
            S.Mobility.SetTerrainSampler([Port,Receiver](FVector2D P){return 18*(FMath::Exp(-(P-Port).SizeSquared()/1600.)+FMath::Exp(-(P-Receiver).SizeSquared()/1600.));});
            V.Position=Port+FVector2D(100,0);TArray<FVector2D> Nominal;
            TestFalse(TEXT("The exact source port has a truck-impassable graded shoulder"),S.FindMobilityRoute(TEXT("transport"),V.Position,Port,Nominal));
            TestTrue(TEXT("A worker can cross that shoulder to handle nearby cargo"),S.FindRoute(Port,V.Position,Nominal));
        }
        const int32 Bodies=S.Workers.Bodies.Num();const double Before=S.TotalStock(TEXT("alloy"));const double Amount=Vehicle?80.:10.;
        const bool Dispatched=Vehicle?S.TransportVehicles.Dispatch(S,CoreId,Depot,0,TEXT("alloy"),Amount,false):S.Workers.Dispatch(S,CoreId,Depot,0,TEXT("alloy"),Amount,false);
        if(!TestTrue(TEXT("Actual finite-stock cargo dispatch succeeds"),Dispatched))return false;
        const int32 CourierId=S.Couriers.Last().Id;const FString WorkerId=S.Couriers.Last().WorkerId;
        bool UsedRoad=false,Continuous=true,Conserved=true,Done=false,HandledNearby=false,HandlerWalked=false;
        for(int I=0;I<5000&&!Done;++I)
        {
            auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==CourierId;});if(!C){Done=true;break;}
            auto* W=Vehicle?nullptr:S.Workers.Find(WorkerId);const FVector2D Previous=Vehicle?V.Position:W->Position;
            if(Vehicle)
            {
                if(auto* Handler=S.Workers.Find(V.HandlerWorkerId);Handler&&Handler->Activity==TEXT("to_handle"))
                {const FVector2D HandlerBefore=Handler->Position;if(S.Workers.Move(S,*Handler,Handler->HandlingPosition,.1))Handler->Activity=TEXT("handling");HandlerWalked|=!Handler->Position.Equals(HandlerBefore,1.e-8);}
                if(S.Workers.TransportHandlingReady(V.HandlerWorkerId,V.Id))
                {const FVector2D Anchor=C->Amount>0?S.BuildingAccessPoint(*S.FindBuilding(Depot)):Port;const double Distance=FVector2D::Distance(V.Position,Anchor)*S.MetersPerWorldUnit();HandledNearby|=Distance>1.e-4&&Distance<=S.TransportVehicles.Definition().ServiceMeters+1.e-6;}
                S.TransportVehicles.StepDelivery(S,V,*C,.1);
            }
            else S.Workers.StepDelivery(S,*W,*C,.1);
            const FVector2D Current=Vehicle?V.Position:W->Position;
            UsedRoad|=Current.X>Port.X+300&&Current.X<Port.X+3300&&FMath::Abs(Current.Y-Port.Y)<.001;
            Continuous&=FVector2D::Distance(Previous,Current)<=(Vehicle?30.:10.)/3.6/S.MetersPerWorldUnit()*2*.1+1.e-6;
            Conserved&=FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy")),Before,1.e-6);
            Done=Vehicle?!V.DeliveryId:C->Phase==TEXT("done");S.Time+=.1;
        }
        TestTrue(AlternateBerths?TEXT("Truck completes pickup and delivery through reachable nearby berths"):Vehicle?TEXT("Vehicle and physical handlers finish the road cargo delivery"):TEXT("Worker finishes the road cargo delivery"),Done&&(UsedRoad||AlternateBerths));
        if(AlternateBerths)TestTrue(TEXT("Nearby handling stays inside eight metres and a real worker walks to the truck"),HandledNearby&&HandlerWalked);
        TestTrue(TEXT("Delivery conserves cargo every step without teleportation"),Conserved&&Continuous);
        TestTrue(TEXT("Only delivered cargo appears at the receiver"),FMath::IsNearlyEqual(S.FindBuilding(Depot)->Inventory.FindRef(TEXT("alloy")),Amount,1.e-6));
        TestEqual(TEXT("The finite crew is unchanged after delivery"),S.Workers.Bodies.Num(),Bodies);
        const FString OldActivity=S.Workers.Bodies[0].Activity;
        TestTrue(TEXT("An unreachable door cannot capture a worker"),S.Workers.AcquireTransportHandler(S,999,CoreId,0,S.Buildings[0].Position+FVector2D(0,100),false).IsEmpty());
        TestEqual(TEXT("Failed cargo-door routing preserves the previous task"),S.Workers.Bodies[0].Activity,OldActivity);
    }
    for(bool PhysicalFailure:{false,true})
    {
        FSeigeSimulation S=Base;const int32 Depot=S.AddReviewBuilding(TEXT("depot"),Port+FVector2D(3230,700));
        if(!TestTrue(TEXT("Recovery receiver exists"),Depot>0))return false;
        for(auto& W:S.Workers.Bodies){W.Activity=TEXT("operate");W.CrewBuildingId=W.BuildingId=CoreId;W.Outdoor=true;W.ContainerId=0;W.ContainerKind.Empty();W.Position=Port+FVector2D(0,20);W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;}
        S.Workers.Bodies.Last().CrewBuildingId=S.Workers.Bodies.Last().BuildingId=Depot;S.Workers.Bodies.Last().Position=S.BuildingAccessPoint(*S.FindBuilding(Depot));
        auto& V=S.TransportVehicles.Bodies[0];V.Position=Port;V.Status=TEXT("Available");V.Embarked=false;
        const double Stock=S.TotalStock(TEXT("alloy")),SourceStock=S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),Amount=20;const int32 BodyCount=S.Workers.Bodies.Num();
        if(!TestTrue(TEXT("Truck initially preflights both valid delivery legs"),S.TransportVehicles.Dispatch(S,CoreId,Depot,0,TEXT("alloy"),Amount,false)))return false;
        bool Changed=false,Returned=false,Conserved=true,Continuous=true,ObservedRecovery=false;double DepartedDistance=0,ReturnTravel=0;
        for(int32 I=0;I<10000&&!Returned;++I)
        {
            auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==V.DeliveryId;});if(!C){Returned=Changed;break;}
            if(!Changed&&C->Amount>0&&V.DistanceMeters>30)
            {
                DepartedDistance=V.DistanceMeters;
                if(PhysicalFailure)
                {
                    // Keep the already-approved route in the vehicle while a
                    // new steep local shoulder blocks its next physical slice.
                    // This must recover even though no berth query has failed.
                    const FVector2D Origin=V.Position,Direction=(V.Route[V.NextWaypoint]-Origin).GetSafeNormal();
                    S.Mobility.SetTerrainSampler([Origin,Direction](FVector2D P){return FMath::Clamp(FVector2D::DotProduct(P-Origin,Direction),0.,400.)*.4;});
                }
                else
                {
                    const double Boundary=S.BuildingAccessPoint(*S.FindBuilding(Depot)).X-600;
                    S.Mobility.SetTerrainSampler([Boundary](FVector2D P){return FMath::Clamp(P.X-Boundary,0.,400.)*.4;});++S.TransportRevision;
                }
                Changed=true;
            }
            if(auto* Handler=S.Workers.Find(V.HandlerWorkerId);Handler&&Handler->Activity==TEXT("to_handle"))
                if(S.Workers.Move(S,*Handler,Handler->HandlingPosition,.1))Handler->Activity=TEXT("handling");
            const FVector2D Before=V.Position;S.TransportVehicles.StepDelivery(S,V,*C,.1);
            if(const auto* Current=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==V.DeliveryId;}))
                if(Changed&&Current->TargetId==CoreId){ObservedRecovery=true;ReturnTravel+=FVector2D::Distance(Before,V.Position)*S.MetersPerWorldUnit();}
            Continuous&=FVector2D::Distance(Before,V.Position)*S.MetersPerWorldUnit()<=30./3.6*2*.1+1.e-6;
            Conserved&=FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy")),Stock,1.e-6);S.Time+=.1;
        }
        TestTrue(PhysicalFailure?TEXT("An actual blocked movement slice returns the loaded truck without a failed berth query"):TEXT("Loaded truck physically returns cargo after destination access changes"),Changed&&Returned&&ObservedRecovery&&Conserved&&Continuous&&DepartedDistance>30&&ReturnTravel>20&&FMath::IsNearlyEqual(S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),SourceStock,1.e-6));
        TestEqual(TEXT("A failed destination receives no unhandled cargo"),S.FindBuilding(Depot)->Inventory.FindRef(TEXT("alloy")),0.);
        TestFalse(TEXT("The failed truck service stays suppressed on unchanged terrain"),S.TransportVehicles.Dispatch(S,CoreId,Depot,0,TEXT("alloy"),Amount,false));
        if(TestTrue(TEXT("A physical worker takes over the original delivery demand"),S.Workers.Dispatch(S,CoreId,Depot,0,TEXT("alloy"),Amount,false)))
        {
            const int32 Courier=S.Couriers.Last().Id;const FString Body=S.Couriers.Last().WorkerId;bool Finished=false;
            for(int32 I=0;I<12000&&!Finished;++I)
            {
                auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==Courier;});if(!C){Finished=true;break;}
                auto* W=S.Workers.Find(Body);S.Workers.StepDelivery(S,*W,*C,.1);Finished=C->Phase==TEXT("done");Conserved&=FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy")),Stock,1.e-6);S.Time+=.1;
            }
            TestTrue(TEXT("Worker completes the original load without duplicated or lost material"),Finished&&Conserved&&FMath::IsNearlyEqual(S.FindBuilding(Depot)->Inventory.FindRef(TEXT("alloy")),Amount,1.e-6));
        }
        TestEqual(TEXT("Truck fallback creates no replacement worker bodies"),S.Workers.Bodies.Num(),BodyCount);
    }
    {
        FSeigeSimulation S=Base;const int32 Depot=S.AddReviewBuilding(TEXT("depot"),Port+FVector2D(3230,700));
        if(!TestTrue(TEXT("Unpicked recovery receiver exists"),Depot>0))return false;
        for(auto& W:S.Workers.Bodies){W.Activity=TEXT("operate");W.CrewBuildingId=W.BuildingId=CoreId;W.Outdoor=true;W.ContainerId=0;W.ContainerKind.Empty();W.Position=Port;}
        S.Workers.Bodies.Last().CrewBuildingId=S.Workers.Bodies.Last().BuildingId=Depot;
        auto& V=S.TransportVehicles.Bodies[0];V.Position=Port+FVector2D(700,0);V.Status=TEXT("Available");V.Embarked=false;
        const double Stock=S.TotalStock(TEXT("alloy")),SourceStock=S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy"));
        if(!TestTrue(TEXT("Unpicked truck reserves a reachable load on the initial terrain"),S.TransportVehicles.Dispatch(S,CoreId,Depot,0,TEXT("alloy"),20,false)))return false;
        const int32 Courier=V.DeliveryId;const FVector2D Before=V.Position,Direction=(V.Route[V.NextWaypoint]-Before).GetSafeNormal();
        S.Mobility.SetTerrainSampler([Before,Direction](FVector2D P){return FMath::Clamp(FVector2D::DotProduct(P-Before,Direction),0.,400.)*.4;});
        auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==Courier;});S.TransportVehicles.StepDelivery(S,V,*C,.1);
        TestTrue(TEXT("Actual pickup movement failure releases the unpicked reservation immediately"),!V.DeliveryId&&!S.Couriers.ContainsByPredicate([&](const auto& Task){return Task.Id==Courier;})&&S.Workers.PickupReserved(S,CoreId,TEXT("alloy"))==0);
        TestTrue(TEXT("Failed pickup preserves stock and vehicle position exactly"),V.Position.Equals(Before,1.e-8)&&FMath::IsNearlyEqual(S.TotalStock(TEXT("alloy")),Stock,1.e-6)&&FMath::IsNearlyEqual(S.FindBuilding(CoreId)->Inventory.FindRef(TEXT("alloy")),SourceStock,1.e-6));
        TestFalse(TEXT("Worker fallback is not monopolized again by the same failed truck service"),S.TransportVehicles.Dispatch(S,CoreId,Depot,0,TEXT("alloy"),20,false));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeNearLimitSlopeTest,"Seige.Transport.NearLimitSlopeConsistency",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeNearLimitSlopeTest::RunTest(const FString&)
{
    FSeigeSimulation S;FString Error;if(!S.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false)){AddError(Error);return false;}
    S.Environment.Enabled=false;S.Buildings.Empty();S.Roads.Empty();
    const double Period=S.Mobility.Profile(TEXT("transport"))->SampleMeters/S.MetersPerWorldUnit();
    // A smooth near-limit incline has a 17.8% average but a real 18.1% peak.
    // The old five-percent curvature trigger certifies the long edge then
    // rejects the first physical movement slice on that very same edge.
    S.Mobility.SetTerrainSampler([Period](FVector2D P){return .178*P.X+.003*Period/(2*PI)*FMath::Sin(2*PI*P.X/Period);});
    const FVector2D Start(0,0),End(Period*4,0),Slice(.01/S.MetersPerWorldUnit(),0);
    TestFalse(TEXT("A near-limit grade peak is rejected during long-edge planning"),S.Mobility.MovementAllowed(S,TEXT("transport"),Start,End));
    TestFalse(TEXT("The same real local peak remains blocked during movement"),S.Mobility.MovementAllowed(S,TEXT("transport"),Start,Slice));
    TArray<FVector2D> Route;
    if(TestTrue(TEXT("A legal contour remains available around the marginal incline"),S.FindMobilityRoute(TEXT("transport"),Start,End,Route)))
    {
        FVector2D Position=Start;bool Physical=true;int32 NextWaypoint=0;
        for(int32 Tick=0;Tick<20000&&NextWaypoint<Route.Num();++Tick)
        {
            const auto& Goal=Route[NextWaypoint];const double Distance=FVector2D::Distance(Position,Goal);
            if(Distance<1.e-6){++NextWaypoint;continue;}
            const double Speed=S.Mobility.Speed(S,TEXT("transport"),Position,Goal)/S.MetersPerWorldUnit();
            const FVector2D Next=Position+(Goal-Position)/Distance*FMath::Min(Distance,Speed*.05);
            if(!S.Mobility.MovementAllowed(S,TEXT("transport"),Position,Next)){Physical=false;break;}
            Position=Next;
        }
        TestTrue(TEXT("Every actual fifty-millisecond contour movement stays within the authored slope limit"),Physical&&Position.Equals(End,1.e-6));
    }
    // Reproduce the stronger live815 failure: a compact initial shoulder is
    // wholly missed by five-metre midpoint intervals, despite its 23.4% entry.
    const FVector2D AliasStart(2276.669573218848,-8033.756366484774),AliasEnd(2110.002906552,-7867.089699818),Direction=(AliasEnd-AliasStart).GetSafeNormal();
    const double Width=.15/S.MetersPerWorldUnit();
    S.Mobility.SetTerrainSampler([AliasStart,Direction,Width](FVector2D P){const double Along=FVector2D::DotProduct(P-AliasStart,Direction);return .234*Along*FMath::Exp(-FMath::Square(Along/Width));});
    TestFalse(TEXT("Short initial shoulder cannot alias into an approved long edge"),S.Mobility.MovementAllowed(S,TEXT("transport"),AliasStart,AliasEnd));
    TestFalse(TEXT("Its real first movement slice remains physically blocked"),S.Mobility.MovementAllowed(S,TEXT("transport"),AliasStart,AliasStart+Direction*(.104/S.MetersPerWorldUnit())));
    S.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    TestTrue(TEXT("Flat cache fixture initially has a direct route"),S.FindMobilityRoute(TEXT("transport"),Start,End,Route)&&Route.Num()==1);
    S.RejectMobilityRouteEdge(TEXT("transport"),Start,End);
    TestTrue(TEXT("A physically rejected edge cannot reuse its cached successful route"),S.FindMobilityRoute(TEXT("transport"),Start,End,Route)&&Route.Num()>1);
    S.Mobility.InvalidateTerrain();
    TestTrue(TEXT("New terrain revision permits reevaluation of previously rejected edges"),S.FindMobilityRoute(TEXT("transport"),Start,End,Route)&&Route.Num()==1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSeigeRoadlessCrewAccessTest,"Seige.Transport.RoadlessCrewAccessAndDelivery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSeigeRoadlessCrewAccessTest::RunTest(const FString&)
{
    FSeigeSimulation Base;FString Error;
    if(!Base.Initialize(FPaths::ProjectDir()/TEXT("Rules"),Error,false,false,815)){AddError(Error);return false;}
    TestFalse(TEXT("Current authored scenario disables roads"),Base.RoadsEnabled());
    Base.Environment.Enabled=false;Base.Mobility.SetTerrainSampler([](FVector2D){return 0.;});
    Base.Buildings[0].IsConstructing=false;Base.Buildings[0].ConstructionProgress=1;Base.DeploymentHatchOpen=Base.DeploymentGrounded=true;
    const int32 Core=Base.Buildings[0].Id;const FVector2D Port=Base.BuildingAccessPoint(Base.Buildings[0]);
    const int32 Depot=Base.AddReviewBuilding(TEXT("depot"),Port+FVector2D(2200,500));
    if(!TestTrue(TEXT("Roadless delivery destination exists"),Depot>0))return false;
    for(auto& W:Base.Workers.Bodies){W.Activity=TEXT("operate");W.CrewBuildingId=W.BuildingId=Core;W.Outdoor=true;W.ContainerId=0;W.ContainerKind.Empty();W.Position=Port+FVector2D(0,20);W.Route.Empty();W.NextWaypoint=0;W.RetryAt=0;}
    Base.Workers.Bodies.Last().CrewBuildingId=Base.Workers.Bodies.Last().BuildingId=Depot;
    Base.Workers.Bodies.Last().Position=Base.BuildingAccessPoint(*Base.FindBuilding(Depot));
    // Reproduce a stranded eligible body from an older save. Its invalid
    // position must not prevent another real operator from handling freight.
    Base.Workers.Bodies[0].Position=Base.Buildings[0].Position+FVector2D(0,100);
    const FString Stranded=Base.Workers.Bodies[0].Id;
    auto& Truck=Base.TransportVehicles.Bodies[0];Truck.Position=Port;Truck.Status=TEXT("Available");Truck.Embarked=false;
    {
        FSeigeSimulation S=Base;FSeigeTransportSegment OldRoad;OldRoad.Id=900;OldRoad.A=Port;OldRoad.B=Port+FVector2D(4000,0);OldRoad.Tier=OldRoad.TargetTier=TEXT("road");OldRoad.Health=OldRoad.MaxHealth=1000;OldRoad.IsConstructing=false;S.Roads.Add(OldRoad);
        TestFalse(TEXT("Disabled road construction rejects direct commands"),S.CanPlaceRoad(Port,Port+FVector2D(1000,0),Error));
        TestFalse(TEXT("Disabled road upgrades reject direct commands"),S.CanUpgradeRoad(OldRoad.Id,Error));
        TestFalse(TEXT("Legacy road data supplies no movement corridor"),S.HasRoadLink(OldRoad.A,OldRoad.B));
        TestEqual(TEXT("Legacy road data supplies no speed bonus"),S.RouteSpeedMultiplier(OldRoad.A,OldRoad.B),1.);
        TArray<FVector2D> Route;TestFalse(TEXT("Disabled roads cannot be planned by AI"),S.FindRoadRoute(Port,OldRoad.B,Route));
        auto& W=S.Workers.Bodies[0];const FString WorkerId=W.Id;const FVector2D BeforePosition=W.Position;
        S.Roads[0].IsConstructing=true;W.RoadId=OldRoad.Id;W.BuildingId=0;W.Activity=TEXT("road_build");
        S.Workers.Tick(S,0);
        TestTrue(TEXT("A disabled legacy road cannot keep a construction worker assigned"),S.Workers.Find(WorkerId)->RoadId==0&&S.Workers.Find(WorkerId)->Activity!=TEXT("road_build")&&S.Workers.Find(WorkerId)->Activity!=TEXT("to_road"));
        TestTrue(TEXT("Cancelling obsolete road work preserves the actual worker position"),S.Workers.Find(WorkerId)->Position.Equals(BeforePosition,1.e-8));
    }
    {
        FSeigeSimulation S=Base;
        if(TestTrue(TEXT("Worker delivery tries reachable crew after an inaccessible first worker"),S.Workers.Dispatch(S,Core,Depot,0,TEXT("alloy"),10,false)))
            TestNotEqual(TEXT("The stranded first body cannot monopolize the reservation"),S.Couriers.Last().WorkerId,Stranded);
        TestEqual(TEXT("Failed candidate keeps its previous activity"),S.Workers.Find(Stranded)->Activity,FString(TEXT("operate")));
        FSeigeSimulation Isolated=Base;
        for(auto& W:Isolated.Workers.Bodies)W.Position=Base.Buildings[0].Position+FVector2D(0,100);
        const double Before=Isolated.TotalStock(TEXT("alloy"));
        TestFalse(TEXT("No worker cargo is reserved if all pickup routes fail"),Isolated.Workers.Dispatch(Isolated,Core,Depot,0,TEXT("alloy"),10,false));
        TestTrue(TEXT("Unreachable pickup leaves inventory and courier ledger untouched"),Isolated.Couriers.IsEmpty()&&FMath::IsNearlyEqual(Before,Isolated.TotalStock(TEXT("alloy")),1.e-6));
    }
    {
        FSeigeSimulation S=Base;auto& V=S.TransportVehicles.Bodies[0];const double Before=S.TotalStock(TEXT("alloy"));
        if(!TestTrue(TEXT("Handler recovery has a real reserved truck task"),S.TransportVehicles.Dispatch(S,Core,Depot,0,TEXT("alloy"),80,false)))return false;
        auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==V.DeliveryId;});S.TransportVehicles.StepDelivery(S,V,*C,.1);
        auto* Handler=S.Workers.Find(V.HandlerWorkerId);
        if(!TestTrue(TEXT("A physical worker is walking to the truck"),Handler&&Handler->Activity==TEXT("to_handle")))return false;
        const FVector2D BeforePosition=Handler->Position;const FString Identity=Handler->Id;
        // The exact state left by Move after observing a blocked physical edge.
        // Clearing the route alone must never leave a permanent handler claim.
        Handler->Route.Empty();Handler->NextWaypoint=0;Handler->RetryAt=S.Time+2;
        S.TransportVehicles.StepDelivery(S,V,*C,.1);Handler=S.Workers.Find(V.HandlerWorkerId);
        TestTrue(TEXT("A failed handler is reacquired on a validated route or replaced by reachable crew"),Handler&&(!Handler->Route.IsEmpty()||Handler->Position.Equals(Handler->HandlingPosition,1.e-6)));
        TestTrue(TEXT("Reassignment does not teleport the original worker or alter unpicked goods"),S.Workers.Find(Identity)->Position.Equals(BeforePosition,1.e-8)&&FMath::IsNearlyEqual(Before,S.TotalStock(TEXT("alloy")),1.e-6)&&C->Amount==0&&C->ReservedAmount==80);
    }
    {
        FSeigeSimulation S=Base;auto& V=S.TransportVehicles.Bodies[0];const double Before=S.TotalStock(TEXT("alloy"));const int32 Count=S.Workers.Bodies.Num();
        if(!TestTrue(TEXT("A roadless truck can preflight both physical service legs"),S.TransportVehicles.Dispatch(S,Core,Depot,0,TEXT("alloy"),80,false)))return false;
        bool Done=false,Continuous=true,Conserved=true,SelectedReachable=false;double Travel=0;
        for(int32 I=0;I<6000&&!Done;++I)
        {
            auto* C=S.Couriers.FindByPredicate([&](const auto& Task){return Task.Id==V.DeliveryId;});if(!C){Done=true;break;}
            if(auto* Handler=S.Workers.Find(V.HandlerWorkerId);Handler&&Handler->Activity==TEXT("to_handle"))
            {SelectedReachable|=Handler->Id!=Stranded;if(S.Workers.Move(S,*Handler,Handler->HandlingPosition,.1))Handler->Activity=TEXT("handling");}
            const FVector2D From=V.Position;S.TransportVehicles.StepDelivery(S,V,*C,.1);const double Moved=FVector2D::Distance(From,V.Position)*S.MetersPerWorldUnit();Travel+=Moved;
            Continuous&=Moved<=30./3.6*.1+1.e-6;Conserved&=FMath::IsNearlyEqual(Before,S.TotalStock(TEXT("alloy")),1.e-6);S.Time+=.1;Done=!V.DeliveryId;
        }
        TestTrue(TEXT("Roadless truck drives to destination and real reachable crew load/unload it"),Done&&Travel>100&&SelectedReachable);
        TestTrue(TEXT("Roadless movement never teleports or duplicates cargo"),Continuous&&Conserved);
        TestEqual(TEXT("Actual delivered cargo reaches the intended receiver"),S.FindBuilding(Depot)->Inventory.FindRef(TEXT("alloy")),80.);
        TestEqual(TEXT("Roadless freight uses only the finite existing worker bodies"),S.Workers.Bodies.Num(),Count);
        TestTrue(TEXT("No delivery creates roads implicitly"),S.Roads.IsEmpty());
    }
    {
        FSeigeSimulation S=Base;
        const int32 Blocked=S.AddReviewBuilding(TEXT("solar_array"),Port+FVector2D(0,1800));
        const int32 Reachable=S.AddReviewBuilding(TEXT("solar_array"),Port+FVector2D(0,-1800));
        if(!TestTrue(TEXT("Two real construction targets exist"),Blocked>0&&Reachable>0))return false;
        for(int32 Id:{Blocked,Reachable}){auto& B=*S.FindBuilding(Id);B.IsConstructing=true;B.ConstructionProgress=0;B.ConstructionMaterials.Add(TEXT("alloy"),20);}
        // A finite ramp can legally be climbed along a shallow diagonal. Use
        // an actual vertical cliff across the map for the unreachable fixture.
        S.Mobility.SetTerrainSampler([Port](FVector2D P){return P.Y>Port.Y+900.?100000.:0.;});
        TArray<FVector2D> Route;
        TestFalse(TEXT("Older-site fixture is physically unreachable"),S.FindRoute(Port,S.BuildingAccessPoint(*S.FindBuilding(Blocked)),Route));
        TestTrue(TEXT("Newer-site fixture has a physical route"),S.FindRoute(Port,S.BuildingAccessPoint(*S.FindBuilding(Reachable)),Route));
        S.Workers.Schedule(S);
        bool ChoseReachable=false,ClaimedBlocked=false;
        for(const auto& W:S.Workers.Bodies){ChoseReachable|=W.BuildingId==Reachable&&W.Activity==TEXT("to_build");ClaimedBlocked|=W.BuildingId==Blocked&&(W.Activity==TEXT("build")||W.Activity==TEXT("to_build"));}
        TestTrue(TEXT("An inaccessible older site cannot starve a physically reachable funded site"),ChoseReachable&&!ClaimedBlocked);
    }
    {
        FSeigeSimulation S=Base;const FVector2D Plot=Port+FVector2D(0,1800);S.TransportVehicles.Bodies[0].Position=Plot;
        TestFalse(TEXT("A new building cannot enclose a cargo vehicle"),S.CanPlaceBuilding(TEXT("solar_array"),Plot,Error));
        S.TransportVehicles.Bodies[0].Position=Port;
        TestTrue(TEXT("The plot becomes available after the vehicle leaves"),S.CanPlaceBuilding(TEXT("solar_array"),Plot,Error));
    }
    for(bool Vehicle:{false,true})
    {
        // Prime a long, certified edge, then reveal a small local obstruction
        // after the real body enters it. This reproduces the live terrain case
        // where finer movement sampling disproves a coarse planner edge.
        FSeigeSimulation S=Base;S.Buildings.Empty();S.Roads.Empty();bool Revealed=false;
        S.Mobility.SetTerrainSampler([&Revealed](FVector2D P){return Revealed&&FMath::Abs(P.X)<10&&FMath::Abs(P.Y)<30?100.:0.;});
        const FVector2D Start(-500,0),Finish(500,0);TArray<FVector2D> Route;
        const auto Plan=[&](TArray<FVector2D>& Out){return Vehicle?S.FindMobilityRoute(TEXT("transport"),Start,Finish,Out):S.FindRoute(Start,Finish,Out);};
        if(!TestTrue(TEXT("Recovery fixture initially certifies one original long edge"),Plan(Route)&&Route.Num()==1))return false;
        auto& W=S.Workers.Bodies[0];auto& V=S.TransportVehicles.Bodies[0];
        W.Position=V.Position=Start;W.Route=V.Route=Route;W.NextWaypoint=V.NextWaypoint=0;W.RouteRevision=V.RouteRevision=S.GeometryRevision();W.RetryAt=V.RetryAt=0;
        bool SawFailure=false,RejectedOriginal=false,Arrived=false,Continuous=true,ValidCursor=true;int32 FailureCount=0;
        for(int32 I=0;I<10000&&!Arrived;++I)
        {
            const FVector2D Before=Vehicle?V.Position:W.Position;if(Before.X>-100)Revealed=true;
            Arrived=Vehicle?S.TransportVehicles.Move(S,V,Finish,.05):S.Workers.Move(S,W,Finish,.05);
            const FVector2D After=Vehicle?V.Position:W.Position;const auto& Active=Vehicle?V.Route:W.Route;
            ValidCursor&=(Vehicle?V.NextWaypoint:W.NextWaypoint)<=Active.Num();
            Continuous&=FVector2D::Distance(Before,After)*S.MetersPerWorldUnit()<=(Vehicle?30.:10.)/3.6*.05+1.e-6;
            if(Revealed&&Active.IsEmpty()&&(Vehicle?V.RetryAt:W.RetryAt)>S.Time)
            {
                if(!SawFailure)
                {
                    SawFailure=true;TArray<FVector2D> Retry;
                    RejectedOriginal=!Plan(Retry)||Retry.Num()!=1;
                    TestTrue(TEXT("Mid-edge failure invalidates its original cached start, preventing a backtrack/retry loop"),RejectedOriginal);
                }
                if((Vehicle?V.RetryAt:W.RetryAt)>S.Time+1.99)++FailureCount;
            }
            S.Time+=.05;
        }
        TestTrue(TEXT("Worker and transport physically detour and arrive after a failed interior step"),SawFailure&&RejectedOriginal&&Arrived&&Continuous&&FailureCount<20);
        TestTrue(TEXT("Cleared retry routes always retain a loadable zero cursor"),ValidCursor);
    }
    return true;
}
#endif
