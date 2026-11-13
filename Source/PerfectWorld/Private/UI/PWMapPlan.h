#pragma once

#include "CoreMinimal.h"
#include "LaneMapView.h"
#include "UI/PWMapData.h"

/**
 * What to draw, decided without Slate (order F2). BuildPlan takes the map's scene, the live snapshot and a view (LaneMapView.h) and returns
 * the visible rectangles, door and person marks and the player arrow in widget pixels, already culled. The minimap and the full map both
 * paint a plan, so what is drawn can be tested without a window; the painter only turns primitives into Slate elements.
 */
namespace PWMap
{
enum class EQuadRole : uint8
{
    Road,
    Alley,
    Footpath,
    Home,
    Shop,
    School,
    Workshop,
    Sealed
};

/** A map rectangle on the widget: centre and size in pixels, turned by AngleRadians (clockwise on screen) about its centre. */
struct FPlanQuad
{
    EQuadRole Role = EQuadRole::Road;
    FVector2f Centre = FVector2f::ZeroVector;
    FVector2f Size = FVector2f::ZeroVector;
    float AngleRadians = 0.0f;
    /** Index into Map.Surfaces (when !bBuilding) or Map.Buildings. */
    int32 SourceIndex = INDEX_NONE;
    bool bBuilding = false;
};

enum class EDotRole : uint8
{
    Person,
    DoorOpen,
    DoorClosed,
    DoorPrivate,
    DoorLocked
};

struct FPlanDot
{
    EDotRole Role = EDotRole::Person;
    FVector2f Position = FVector2f::ZeroVector;
    /** Index into Snapshot.People (Person) or Map.Doors. */
    int32 SourceIndex = INDEX_NONE;
};

struct FPlanArrow
{
    bool bVisible = false;
    FVector2f Position = FVector2f::ZeroVector;
    /** Clockwise from straight up, in degrees. */
    float AngleDegrees = 0.0f;
};

struct FPlanOptions
{
    bool bDoors = true;
    bool bPeople = true;
    /** Marks nearer the clip edge than this are dropped so a dot is never half cut off. */
    float DotInsetPx = 3.0f;
};

struct FMapPlan
{
    TArray<FPlanQuad> Quads;
    TArray<FPlanDot> Dots;
    FPlanArrow Player;
    /** How many map rectangles were rejected for being outside the clip. */
    int32 CulledQuads = 0;
};

EQuadRole QuadRoleOf(PerfectWorld::LaneMap::SurfaceKind Kind);
EQuadRole QuadRoleOf(PerfectWorld::LaneMap::BuildingKind Kind);
EDotRole DotRoleOf(PerfectWorld::LaneMap::DoorAvailability Availability);

FMapPlan BuildPlan(const FPWMapScene& Scene, const FPWMapSnapshot& Snapshot, const PerfectWorld::LaneMap::View& View, const FVector2f& WidgetSize,
                   const PerfectWorld::LaneMap::Rect& Clip, const FPlanOptions& Options = FPlanOptions());

/** The minimap's view for a snapshot, preferences and side length; an unplaced player is shown at the lane's start. */
PerfectWorld::LaneMap::View MakeMinimapView(const FPWMapSnapshot& Snapshot, const FPWMapPreferences& Preferences, float SidePx, bool bReducedMotion);
} // namespace PWMap
