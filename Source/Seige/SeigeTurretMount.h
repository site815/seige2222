#pragma once
#include "CoreMinimal.h"

// Modular weapon banks ride the exposed azimuth ring outside the shuttle's
// pressure hull. Rotating the carrier as well as the gun removes the hull's
// blind hemisphere without drawing barrels through the spacecraft.
inline FVector SeigeOrbitingBankMount(FIntVector Cell,double Side,double ShipScale,double AimYaw)
{
    const FVector Local(350.*ShipScale,(Cell.X==0?-150.:150.)+(Cell.Y+Side*.5-2.)*50.,1120.*ShipScale+(Cell.Z+Side*.5-2.)*50.);
    return FRotator(0,AimYaw,0).RotateVector(Local);
}

// All tower tiers keep the same footprint. Their raised bearing clears the
// authored level-three surveillance mast (803 cm) and lower deck furniture.
inline constexpr double SeigeTowerBearingHeightCm=840.;
