#pragma once

#include "Doors.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld::LaneMap
{
/**
 * The lane map (order F2): a deterministic, presentation-free projection of the authoritative lane layout into map features,
 * for the minimap and the full-screen map. Nothing here is drawn, coloured or translated; the presenter does that.
 *
 * Sources, all of them portable and already authoritative:
 *   - the lane generator's first rows (the same hash loop as PerfectWorldLaneBuildings.cpp BuildFirstRows; the frontage
 *     catalogue's pins are the cross-check, so a generator that drifts leaves a frontage sealed here exactly as it does in the world);
 *   - LaneFrontageCatalogue (kit-built frontages: id, variant, furniture set, entrance, front door id);
 *   - the four ground floors built by their own code (children's home, opening home, stationery shop, tea shop), whose entrance
 *     centres are the published constants of their Unreal files;
 *   - Doors (the authored access rule, opening hours and start state).
 *
 * Frame: world centimetres exactly as the Unreal lane uses them. X runs along the lane, Y across it, the road centre line is Y = 0.
 * A world yaw of 0 faces +X and 90 faces +Y. Polygons are counter-clockwise in this (X, Y) plane; every polygon the lane produces is
 * an axis-aligned rectangle, and presenters may rely on IsRectangle.
 *
 * Coverage is the lane the player can walk: the road, the two side alleys, the footpaths in front of the near buildings, the nineteen
 * near frontages and the two masses that close the lane (R6-05: the community hall and the market hall, entered through doorless archways;
 * LaneEndLayouts.h). The second row, the third row and the skyline are backdrop and are not mapped.
 * Only exterior front doors have an authored position; interior doors (home_living, care_dormitory ...) are not on the map.
 *
 * Integer-valued geometry in doubles; no exceptions.
 */
struct Point
{
    double X = 0.0;
    double Y = 0.0;
    friend bool operator==(const Point&, const Point&) = default;
};

using Polygon = std::vector<Point>;

/** The lane's fixed numbers (PerfectWorldLaneBlocks.h), centimetres. */
inline constexpr int LaneX0Cm = -2400;
inline constexpr int LaneX1Cm = 6400;
inline constexpr int RoadHalfWidthCm = 250;
inline constexpr int FootpathOuterCm = 400;
inline constexpr int AlleyMinusAXCm = 4300;
inline constexpr int AlleyMinusBXCm = 4900;
inline constexpr int AlleyPlusAXCm = 2800;
inline constexpr int AlleyPlusBXCm = 3400;
inline constexpr int AlleyReachCm = 1500; // |Y| the side alleys reach

enum class SurfaceKind : std::uint8_t { Road, Alley, Footpath };

struct Surface
{
    SurfaceKind Kind = SurfaceKind::Road;
    Polygon Outline;
};

/**
 * What a building is, for colouring and the legend. Home covers every dwelling and the children's home; Workshop is a shop with the
 * workshop furniture set (repair, tailoring); Shop is every other trading or browsing room; School is reserved for the school
 * the lane will gain (no building uses it today, but ExtraBuilding can add one); Sealed is a ground floor nobody can enter yet (none remain
 * since R6-05); Civic is a public hall that is not a shop (the community hall at the west end of the lane).
 */
enum class BuildingKind : std::uint8_t { Home, Shop, School, Workshop, Sealed, Civic };
inline constexpr std::size_t BuildingKindCount = 6;

struct Building
{
    /** "building.nabapara.<name>"; the stable identity. */
    std::string Id;
    /** English display name, empty for an unnamed sealed building. The presenter may translate through LabelKey. */
    std::string Label;
    /** "map.building.<name>"; empty when there is no label. */
    std::string LabelKey;
    BuildingKind Kind = BuildingKind::Sealed;
    /** Ground footprint, counter-clockwise rectangle. */
    Polygon Footprint;
    Point Centre;
    /** Where the front door is; meaningful only when bHasEntrance. */
    Point Entrance;
    bool bHasEntrance = false;
    /** -1 or +1: the side of the lane; 0 for the two masses that close its ends (their entrance is on the lane-facing X edge, R6-05). */
    int Side = 0;
    int Storeys = 0;
    /** True when the ground floor can be entered (it has a front door or doorway). */
    bool bEnterable = false;
};

struct MapDoor
{
    std::string Id;
    std::string BuildingId;
    /** The doorway's centre on the building's front wall. */
    Point Position;
    /** Unit vector pointing from the doorway out to the road. */
    Point Outward;
    DoorAccess Access = DoorAccess::Public;
    bool bInitiallyOpen = false;
};

enum class PoiKind : std::uint8_t { Home, Shop, Workshop, School, Alley, LaneEnd, Civic };

struct PointOfInterest
{
    std::string Id;
    std::string Label;
    std::string LabelKey;
    PoiKind Kind = PoiKind::Shop;
    Point Position;
    /** The building this stands at, empty for an alley or a lane end. */
    std::string BuildingId;
};

struct Map
{
    std::vector<Surface> Surfaces;
    std::vector<Building> Buildings;
    std::vector<MapDoor> Doors;
    std::vector<PointOfInterest> Pois;
    /** Tight bounds of every surface and building. */
    Point BoundsMin;
    Point BoundsMax;
};

/** A building the lane does not generate yet (the school): the integrator appends it through MapOptions, never by editing the generator. */
struct ExtraBuilding
{
    std::string_view Id;
    std::string_view Label;
    std::string_view LabelKey;
    BuildingKind Kind = BuildingKind::School;
    double X0Cm = 0.0;
    double Y0Cm = 0.0;
    double X1Cm = 0.0;
    double Y1Cm = 0.0;
    int Storeys = 1;
    /** An entrance is added when HasEntrance; DoorId (when nonempty) must be an authored door. */
    bool HasEntrance = false;
    double EntranceXCm = 0.0;
    double EntranceYCm = 0.0;
    std::string_view DoorId;
};

struct MapOptions
{
    std::span<const ExtraBuilding> Extra{};
};

/** One generated near frontage (the lane generator's output). The catalogue pins rows to these numbers. */
struct Frontage
{
    int Side = 0;
    int Index = 0;
    int X0Cm = 0;
    int X1Cm = 0;
    int WidthCm = 0;
    int DepthCm = 0;
    int SetbackCm = 0;
    int Storeys = 0;
    std::uint32_t Seed = 0;
    [[nodiscard]] int FrontAbsYCm() const noexcept { return FootpathOuterCm + SetbackCm; }
};

/** The lane generator's hash (PerfectWorldLaneBlocks.h LaneHash, uint32 arithmetic). */
[[nodiscard]] PERFECTWORLDCORE_API std::uint32_t LaneMapHash(std::int32_t a, std::int32_t b = 0, std::int32_t c = 0) noexcept;

/** All near frontages in the generator's order: the -Y row, then the +Y row. Nineteen today (ten and nine). */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<Frontage> EnumerateFrontages();

/** The whole map. Deterministic: two calls give equal results. */
[[nodiscard]] PERFECTWORLDCORE_API Map BuildLaneMap(const MapOptions& options = {});

/** Structural check the tests call: unique ids, rectangular outlines, no overlapping buildings, doors on their building's front wall, authored doors only. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsMapValid(const Map& map) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API bool IsRectangle(const Polygon& polygon) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API const Building* FindBuilding(const Map& map, std::string_view buildingId) noexcept;
/** The building whose footprint contains the point (edges inclusive), or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const Building* FindBuildingAt(const Map& map, Point world) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const MapDoor* FindMapDoor(const Map& map, std::string_view doorId) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API std::string_view BuildingKindId(BuildingKind kind) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view PoiKindId(PoiKind kind) noexcept;

/** What a visitor can expect of a door right now. The leaf position is separate: a door can be Closed to visitors with its leaf ajar. */
enum class DoorAvailability : std::uint8_t
{
    /** Anyone may walk in now (a public door, or a business inside its opening hours). */
    OpenToAll,
    /** A business outside its hours. */
    ClosedForNow,
    /** A dwelling: residents and invited visitors; anyone else may knock. */
    PrivateKnock,
    /** Nobody. */
    Locked
};

struct DoorStatus
{
    /** The physical leaf: the live state when it holds this door, otherwise the authored start state. */
    bool bLeafOpen = false;
    DoorAvailability Availability = DoorAvailability::OpenToAll;
    /** ClosedForNow only: minutes until the next opening; otherwise -1. */
    int MinutesUntilOpening = -1;
    /** BusinessHours only: the daily window containing, or next after, the minute (OpenMinute..CloseMinute); otherwise both -1. */
    int WindowOpenMinute = -1;
    int WindowCloseMinute = -1;
};

/**
 * The door's current status. `live` may be null (the authored start state is used). `minuteOfDay` is 0..1439 (DoorMinuteOfDay of the
 * world clock); anything else is treated as midnight. An id the authored table does not know reads as Locked and closed.
 */
[[nodiscard]] PERFECTWORLDCORE_API DoorStatus QueryDoorStatus(const MapDoor& door, const DoorState* live, int minuteOfDay) noexcept;
} // namespace PerfectWorld::LaneMap
