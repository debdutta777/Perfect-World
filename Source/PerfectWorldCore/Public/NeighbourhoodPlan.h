#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
inline constexpr std::string_view NeighbourhoodId = "district.kolkata.nabapara";
inline constexpr std::string_view NeighbourhoodSchoolInstitutionId = "institution.nabapara_vidyalaya";
inline constexpr std::string_view NeighbourhoodSchoolContentLocationId = "loc_nhss";
inline constexpr std::string_view NeighbourhoodSchoolScheduleLocationId = "location.nabapara.school";
inline constexpr std::string_view NeighbourhoodSchoolGateId = "entrance.nabapara.school_south_gate";
inline constexpr std::string_view NeighbourhoodSchoolGateNodeId = "walk.nabapara.school_gate";
inline constexpr std::string_view NeighbourhoodMainRoadNodeId = "walk.nabapara.main_west";
inline constexpr int NeighbourhoodRoadZCm = -200;
inline constexpr int NeighbourhoodFootpathZCm = -185;

struct NeighbourhoodPoint
{
    int X = 0, Y = 0, Z = NeighbourhoodFootpathZCm;
    friend bool operator==(const NeighbourhoodPoint&, const NeighbourhoodPoint&) = default;
};
/** Ordered corners; intersection includes touching edges, reservations use positive-area overlap. */
struct NeighbourhoodRect
{
    int X0 = 0, Y0 = 0, X1 = 0, Y1 = 0;
    friend bool operator==(const NeighbourhoodRect&, const NeighbourhoodRect&) = default;
};
enum class NeighbourhoodStreetClass : std::uint8_t { None, Lane, CrossStreet, ParaRoad, MainRoad };
enum class NeighbourhoodFeatureKind : std::uint8_t
{
    CentreLine, Carriageway, Footpath, Site, Footprint, Entrance, Destination,
    PopulationEntrance, Crossing, VehicleLane, StopLine, Reservation, BusStop
};
enum class NeighbourhoodFootprintKind : std::uint8_t
{
    None, ExistingBuilding, TeachingBlock, Forecourt, Playground, Club,
    PandalSpace, SweetShop, Pharmacy, TeaPitch, VendorPitch, TransportStand
};
/**
 * Geometry is in the existing lane's world centimetres, with absolute Z. Polygons have four
 * vertices (implicit closing edge); lines have two; point features have one. Bounds are XY.
 * ParentId groups street surfaces, school components, entrance/anchor owners or a stop line's
 * vehicle lane. RelatedId on a stop line is its crossing. On an entrance it is an EXISTING
 * door id where applicable; new planned entrances deliberately do not register save doors.
 * LinkNodeId is the pedestrian access point, also for destinations and population entrances.
 * Existing buildings are references: presenters must not render them a second time.
 */
struct NeighbourhoodFeature
{
    std::string Id;
    NeighbourhoodFeatureKind Kind = NeighbourhoodFeatureKind::Destination;
    std::vector<NeighbourhoodPoint> Geometry;
    NeighbourhoodRect Bounds;
    std::string ParentId, RelatedId, LinkNodeId;
    NeighbourhoodStreetClass StreetClass = NeighbourhoodStreetClass::None;
    NeighbourhoodFootprintKind FootprintKind = NeighbourhoodFootprintKind::None;
    int WidthCm = 0;
    int Storeys = 0;
    bool Existing = false;
    friend bool operator==(const NeighbourhoodFeature&, const NeighbourhoodFeature&) = default;
};
struct NeighbourhoodLinkNode
{
    std::string Id;
    NeighbourhoodPoint Position;
    friend bool operator==(const NeighbourhoodLinkNode&, const NeighbourhoodLinkNode&) = default;
};
enum class NeighbourhoodLinkKind : std::uint8_t { Walk, Crossing, Interior, Stair };
struct NeighbourhoodLink
{
    std::string Id, From, To;
    /** Axis-aligned XY legs; stairs may rise. Length sums absolute XYZ legs in cm. */
    std::vector<NeighbourhoodPoint> Points;
    std::int64_t LengthCm = 0;
    NeighbourhoodLinkKind Kind = NeighbourhoodLinkKind::Walk;
    /** Crossing feature for Crossing links; existing door id for an admission boundary. */
    std::string ConstraintId;
    friend bool operator==(const NeighbourhoodLink&, const NeighbourhoodLink&) = default;
};
struct NeighbourhoodPlan
{
    std::uint64_t Seed = 0;
    std::vector<NeighbourhoodFeature> Features;
    std::vector<NeighbourhoodLinkNode> Nodes;
    /** Bidirectional pedestrian edges, stable authored order. Vehicle lanes are directed features. */
    std::vector<NeighbourhoodLink> Links;
    /** Entrance feature ids for every currently modelled lane dwelling's exterior access. */
    std::vector<std::string> HomeEntrances;
    friend bool operator==(const NeighbourhoodPlan&, const NeighbourhoodPlan&) = default;
};
struct NeighbourhoodRoute
{
    bool Found = false;
    std::vector<std::string> NodeIds, LinkIds;
    std::int64_t LengthCm = 0;
};

/** Frozen authored geometry: seed is metadata, never changes coordinates or consumes gameplay RNG. */
[[nodiscard]] PERFECTWORLDCORE_API NeighbourhoodPlan BuildNeighbourhoodPlan(std::uint64_t seed);
/** Returned pointers belong to plan; invalidated by plan mutation. Unknown ids return nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const NeighbourhoodFeature* FindNeighbourhoodFeature(
    const NeighbourhoodPlan& plan, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const NeighbourhoodLinkNode* FindNeighbourhoodNode(
    const NeighbourhoodPlan& plan, std::string_view id);
/** Includes point and line features; stable feature order, empty for inverted rectangles. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<const NeighbourhoodFeature*> NeighbourhoodFeaturesInRect(
    const NeighbourhoodPlan& plan, NeighbourhoodRect rectangle);
/** Shortest length, ties resolved by node/link authored order. Unknown/unreachable endpoints fail. */
[[nodiscard]] PERFECTWORLDCORE_API NeighbourhoodRoute RouteNeighbourhood(
    const NeighbourhoodPlan& plan, std::string_view from, std::string_view to);
/** Use BEFORE accepting generated OldDenseCore lots or decorative buildings (whole lot rejection).
 * Cells are in cm after conversion from DistrictGenerator's 8 m grid with an explicit world origin.
 * Reservations include the original lane/end masses, new streets/sites and alley mouths.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool NeighbourhoodReservesRect(
    const NeighbourhoodPlan& plan, NeighbourhoodRect rectangle);
/** Stable diagnostics, empty on success. Sites contain components and are not solid footprints.
 * Validates footprints, geometry, ids, edge lengths, connected boundary links and every home route.
 * A route is spatial possibility; admission and dynamic traffic remain their systems' responsibility.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateNeighbourhoodPlan(const NeighbourhoodPlan& plan);
} // namespace PerfectWorld
