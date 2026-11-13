#pragma once

#include "CoreMinimal.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldNpcYield.h"

namespace PWLane { struct FLaneFrame; }

enum class ENpBindingKind : uint8 { Missing, Local, OffLane };

struct FNpAnchorBinding
{
    ENpBindingKind Kind = ENpBindingKind::Local;
    FTransform Feet = FTransform::Identity;
    /** Local portal used when this off-lane location departs/arrives. */
    FName ExitAnchor;
    bool bPreferSit = false;
};

struct FNpRouteBinding
{
    /** Points run in the schedule route definition's From -> To direction. */
    TArray<FVector> Points;
    bool bOffLane = false;
};

/** A building owns its registration. Re-registering its stable ID atomically replaces its bindings. */
struct FNpBindingProvider
{
    TMap<FName, FNpAnchorBinding> Anchors;
    TMap<FName, FNpRouteBinding> Routes;
    TArray<FNpDoorwayZone> Doorways;
};

struct FNpResolvedBinding
{
    ENpBindingKind Kind = ENpBindingKind::Missing;
    FTransform Feet = FTransform::Identity;
    TArray<FVector> RoutePoints;
    int32 LocalProgress = 0;
    FName ExitAnchor;
    FString Diagnostic;
    bool bPreferSit = false;
};

/** No building switch: modules register providers; the presenter consumes their anchors and route segments. */
class FNpLaneNpcBindings
{
public:
    bool RegisterProvider(FName ProviderId, FNpBindingProvider Provider);
    void RemoveProvider(FName ProviderId) { Providers.Remove(ProviderId); }
    const FNpAnchorBinding* FindAnchor(FName Id) const;
    const FNpRouteBinding* FindRoute(FName Id) const;
    TArray<FNpDoorwayZone> Doorways() const;
    FNpResolvedBinding Resolve(const PerfectWorld::NpcSchedule& Schedule, const PerfectWorld::NpcIntent& Intent) const;

private:
    TMap<FName, FNpBindingProvider> Providers;
};

/** Care module adapter; call only after the care building was constructed. Shop modules supply their own providers. */
FNpBindingProvider NpCareHomeBindings(const PWLane::FLaneFrame& Frame);
