#pragma once

#include "PerfectWorldCrowdPresenter.h"

struct FR75PCrowdInstance
{
    uint32 Colour = 0;
    int32 Index = INDEX_NONE;
};

struct FR75PCrowdRecord
{
    PerfectWorld::CrowdPerson Target;
    TWeakObjectPtr<APWCrowdFigure> Actor;
    PWCrowd::FLook Look;
    TArray<FR75PCrowdInstance> Instances;
    FPWNpcPose Pose;
    FVector Feet = FVector::ZeroVector;
    FRotator Facing = FRotator::ZeroRotator;
    double Distance = 0;
    bool bFrozen = false;
    bool bRetiring = false;
};
