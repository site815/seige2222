#pragma once
#include "CoreMinimal.h"

struct FSeigeSceneryView
{
    FVector Position=FVector::ZeroVector,Forward=FVector::ForwardVector,Right=FVector::RightVector,Up=FVector::UpVector;
    double TanHalfHorizontal=.52,Aspect=16./9.;
    bool Intersects(const FBox& Box) const
    {
        const FVector Center=Box.GetCenter()-Position,Extent=Box.GetExtent();
        const auto Outside=[&](FVector N){return FVector::DotProduct(N,Center)+FVector::DotProduct(N.GetAbs(),Extent)<0;};
        return !Outside(Forward)&&!Outside(Forward*TanHalfHorizontal+Right)&&!Outside(Forward*TanHalfHorizontal-Right)&&
            !Outside(Forward*(TanHalfHorizontal/Aspect)+Up)&&!Outside(Forward*(TanHalfHorizontal/Aspect)-Up);
    }
    bool WithinDistance(const FBox& Box,double Radius) const
    {return Box.ComputeSquaredDistanceToPoint(Position)<=Radius*Radius;}
};

inline int32 SeigeSceneryWorkPriority(bool BaseMissing,bool DetailMissing,bool Visible,bool Near,bool Prefetch)
{
    if(Near&&(BaseMissing||DetailMissing))return Visible?0:1;
    if(BaseMissing&&Visible)return 2;
    if(DetailMissing&&Prefetch)return 3;
    if(BaseMissing)return 4;
    return 5;
}
