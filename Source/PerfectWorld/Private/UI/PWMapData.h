#pragma once

#include "CoreMinimal.h"
#include "LaneMap.h"
#include "LaneMapView.h"

class UWorld;

/**
 * The data behind the minimap and the full-screen map (order F2). The portable LaneMap is the only source of geometry, names, kinds and
 * door rules; this file adds what only Unreal knows: translated text, who is standing where right now, and the live door leaves.
 * Nothing here decides anything: it reads the authoritative layout (LaneMap.h) and the live world and hands both to the painters.
 */

/** Player-facing words for a map key, with the English default the portable map carries when no translation exists. */
namespace PWMap
{
FText LabelText(const std::string& Key, const std::string& Fallback);
FText KindText(PerfectWorld::LaneMap::BuildingKind Kind);
/** "08:00", from minutes since midnight. */
FText ClockText(int32 MinuteOfDay);
/** "40 min" or "1 h 40 min". */
FText DurationText(int32 Minutes);
} // namespace PWMap

/** The static half: the portable map and its translated labels (parallel to Map.Buildings and Map.Pois). */
struct FPWMapScene
{
    PerfectWorld::LaneMap::Map Map;
    TArray<FText> BuildingLabels;
    TArray<FText> PoiLabels;

    static TSharedRef<FPWMapScene> Build(const PerfectWorld::LaneMap::MapOptions& Options = {});
};

/** One person standing in the lane right now (a presented actor, never a schedule row). */
struct FPWMapMarker
{
    FName PersonId;
    FText Name;
    PerfectWorld::LaneMap::Point WorldCm;
};

/** The live half, rebuilt once per rendered frame. */
struct FPWMapSnapshot
{
    bool bHasPlayer = false;
    PerfectWorld::LaneMap::Point PlayerCm;
    /** The camera's world yaw in degrees (0 faces +X, 90 faces +Y). */
    double PlayerYawDegrees = 0.0;
    TArray<FPWMapMarker> People;
    /** Parallel to FPWMapScene::Map.Doors. */
    TArray<PerfectWorld::LaneMap::DoorStatus> Doors;
    /** Index into Map.Buildings of the home the player's origin lives in, or INDEX_NONE. */
    int32 PlayerHomeBuilding = INDEX_NONE;
    /** Minutes since midnight on the world clock. */
    int32 MinuteOfDay = 0;
};

/** The settings-ready switches. Stored in GameUserSettings next to the other comfort preferences; defaults are on and rotating. */
struct FPWMapPreferences
{
    bool bMinimapEnabled = true;
    /** True turns the minimap with the camera; false keeps the lane still and turns the arrow. Reduced motion forces the second. */
    bool bMinimapRotates = true;

    static FPWMapPreferences Load();
    void Save() const;
    [[nodiscard]] PerfectWorld::LaneMap::MinimapOrientation Orientation(bool bReducedMotion) const;
};

/**
 * The shared, per-world source for both maps. Refresh gathers the snapshot at most once per rendered frame, so the minimap, the full map
 * and the subsystem can all call it. With no world (tests) it keeps whatever snapshot it was given.
 */
class FPWMapLive : public TSharedFromThis<FPWMapLive>
{
public:
    explicit FPWMapLive(const TWeakObjectPtr<UWorld>& InWorld);
    /** A world-free source with a fixed snapshot, for the automation tests. */
    static TSharedRef<FPWMapLive> MakeFixed(const TSharedRef<FPWMapScene>& Scene, const FPWMapSnapshot& Snapshot);

    void Refresh();

    const FPWMapScene& GetScene() const { return *Scene; }
    const FPWMapSnapshot& GetSnapshot() const { return Snapshot; }
    const FPWMapPreferences& GetPreferences() const { return Preferences; }
    void SetPreferences(const FPWMapPreferences& NewPreferences);

    /** What is in front of the world right now, for the visibility rules in LaneMapView.h. */
    PerfectWorld::LaneMap::HudState ReadHudState(bool bWorldMapOpen) const;

    UWorld* GetWorld() const { return World.Get(); }

private:
    void GatherPlayer(UWorld& InWorld);
    void GatherPeople(UWorld& InWorld);
    void GatherDoors(UWorld& InWorld);
    void GatherHome(UWorld& InWorld);

    TWeakObjectPtr<UWorld> World;
    TSharedRef<FPWMapScene> Scene;
    FPWMapSnapshot Snapshot;
    FPWMapPreferences Preferences;
    uint64 LastFrame = MAX_uint64;
    double NextHomeCheckSeconds = 0.0;
    bool bFixed = false;
};

/** The text a hover or an accessibility reader gives for one building: its name, kind and, when it has a door, how that door stands now. */
namespace PWMap
{
FText DescribeBuilding(const FPWMapScene& Scene, const FPWMapSnapshot& Snapshot, int32 BuildingIndex);
FText DescribeDoor(const PerfectWorld::LaneMap::DoorStatus& Status);
} // namespace PWMap
