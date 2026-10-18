#pragma once

#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldNpcCharacter.h"

struct FNpPresenceRecord
{
    PerfectWorld::NpcSchedule Schedule;
    PerfectWorld::NpcIntent Intent;
    bool bIntentValid = false;
    PerfectWorld::GameInstant UaIntentAt = 0;
    FText DisplayName;
    EPWNpcOutfit Outfit = EPWNpcOutfit::HouseholdAdult;
    FNpResolvedBinding Binding;
    TWeakObjectPtr<APWNpcCharacter> Actor;
    FNpYieldState Yield;
    FString LastDiagnostic;
    /** A local departure route survives an off-lane schedule transition until the physical exit. */
    TArray<FVector> DeparturePoints;
    FName DepartureAnchor;
    TArray<FVector> AppliedRoutePoints;
    /** Authored off-lane ingress retained while its threshold is visible or occupied. */
    TArray<FVector> UaArrivalPoints;
    FTransform UaArrivalFeet;
};
