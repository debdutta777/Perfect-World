#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld::LaneInterior
{
/**
 * Parametric interior kit for the lane's ground floors (LANE_LIFE_UPGRADE_PLAN LL-03). Portable and deterministic: from a
 * footprint, an entrance, a variant and a furniture set it produces wall segments around real openings, rooms and their
 * connections, door sweeps, furniture with physical contracts, usable stand/seat anchors, circulation and use clearances,
 * room-light specifications and staff routes. Unreal only emits block visuals, collision and lights from the result.
 *
 * Frame. Everything is in whole centimetres in the building frame: U runs along the frontage, V runs from the front face
 * (V 0) into the building, so the front wall occupies V 0 to 25. Heights (H, W) are measured from the FINISHED FLOOR
 * (0 = floor, ClearHeight = ceiling underside); the caller adds FinishedFloorCm and its own world frame, which carries
 * the orientation. MirrorU reflects the whole layout across the frontage midline; identifiers do not change.
 */
inline constexpr int WallThicknessCm = 25;
inline constexpr int PartitionThicknessCm = 12;
inline constexpr int DoorOpeningWidthCm = 94;   // leaf 90 plus jamb strips
inline constexpr int DoorOpeningHeightCm = 212; // leaf 210 plus head strip
inline constexpr int DoorClearWidthCm = 90;     // the contract: every doorway is at least 90 by 210 clear
inline constexpr int DoorClearHeightCm = 210;
inline constexpr int CirculationCm = 120;       // every circulation corridor is at least this wide
inline constexpr int StandZoneCm = 80;          // square free of solids around a stand point (capsule radius 34, margin 6)
inline constexpr int SeatApproachCm = 60;       // free space on the open side of a seat
inline constexpr int StaffGapCm = 120;          // the side access through a shop counter (a full circulation width)
inline constexpr int SlabAndPlasterCm = 26;     // storey height minus ceiling clear height (274 clear for a 300 storey)
inline constexpr int WindowSillCm = 75;         // 120 x 135 clear windows
inline constexpr int WindowHeadCm = 210;

struct Rect
{
    int U0 = 0;
    int U1 = 0;
    int V0 = 0;
    int V1 = 0;
    [[nodiscard]] constexpr int Width() const noexcept { return U1 - U0; }
    [[nodiscard]] constexpr int Depth() const noexcept { return V1 - V0; }
    /** True when the interiors overlap; rectangles that merely touch do not overlap. */
    [[nodiscard]] constexpr bool Overlaps(const Rect& o) const noexcept
    {
        return U0 < o.U1 && o.U0 < U1 && V0 < o.V1 && o.V0 < V1;
    }
    [[nodiscard]] constexpr bool Contains(const Rect& o) const noexcept
    {
        return o.U0 >= U0 && o.U1 <= U1 && o.V0 >= V0 && o.V1 <= V1;
    }
    [[nodiscard]] constexpr bool ContainsPoint(int u, int v) const noexcept
    {
        return u >= U0 && u <= U1 && v >= V0 && v <= V1;
    }
    friend constexpr bool operator==(const Rect&, const Rect&) = default;
};

enum class Variant : std::uint8_t { CompactResidence, ShopWithStockroom, SharedEntry };
/**
 * What goes in the rooms. Residence suits CompactResidence and SharedEntry; Stationery, GeneralGoods and Workshop suit
 * ShopWithStockroom. The numeric values are frozen (NL-03): new sets only ever append.
 *  - GeneralGoods: a provision / hardware-style counter shop. Sales room "sales" (customer browse points, a counter) and stock
 *    room "stock" (rack, sacks and cartons).
 *  - Workshop: a repair / tailoring / carpentry shop. Sales room "sales" (a service counter and a short display) and a work
 *    floor "workshop" (RoomKind::Workshop) with benches, a tool rack and a finishing table.
 */
enum class FurnitureSet : std::uint8_t { None, Residence, Stationery, GeneralGoods, Workshop };

enum class Refusal : std::uint8_t
{
    None,
    InvalidBuildingId, WidthTooNarrow, DepthTooShallow, FootprintTooLarge, StoreyHeightInvalid, FloorDatumInvalid,
    EntranceOffFacade, InternalDoorOffWall, FurnitureSetMismatch, NoRoomForUnits, FurnitureDoesNotFit,
    RoomsDisconnected, DoorwayTooSmall, CirculationBlocked, SweepBlocked, ZoneBlocked, FurnitureOverlap,
    FurnitureOutsideRoom, DuplicateId, AnchorInvalid, RouteBlocked, WallInvalid,
    /** Appended by NL-03: a unit door id on a variant without dwelling units, or one repeating the other unit's or the entrance's id. */
    UnitDoorIdInvalid
};

/** Stable machine-readable code ("width_too_narrow", ...). "ok" for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view RefusalCode(Refusal refusal) noexcept;

struct InteriorInput
{
    /** Stable building identity, independent of generated row indices, e.g. "building.nabapara.stationery". */
    std::string BuildingId;
    Variant Kind = Variant::CompactResidence;
    FurnitureSet Furniture = FurnitureSet::None;
    int WidthCm = 0;  // outer footprint along the frontage
    int DepthCm = 0;  // outer footprint into the block
    bool MirrorU = false;
    int FinishedFloorCm = 15;  // finished floor above the ground datum (the lane footpath is 15)
    int StoreyHeightCm = 300;  // floor to floor
    int EntranceCentreU = 0;   // centre of the front doorway along the frontage
    /** Two-room variants only: centre of the doorway between the rooms; 0 selects it automatically. */
    int InternalDoorCentreU = 0;
    /** Lane door id for the front leaf (for example "door.nabapara.stationery_front"); empty builds an open doorway. */
    std::string EntranceDoorId;
    /**
     * SharedEntry only (NL-03 interface, consumed by NL-04): lane door ids for the leaves of the two dwelling doorways. A is the
     * unit on the low-U side of the passage ("unit_a" before any mirroring), B the high-U side ("unit_b"). Empty keeps the
     * doorway open (the behaviour before NL-04). A non-empty id on another variant, an id equal to the other unit's or to
     * EntranceDoorId, or an id for a unit the footprint does not have, is refused with UnitDoorIdInvalid (the last check applies
     * once NL-04 wires the leaves into LaneInteriorShared.cpp; until then the ids are validated for shape and otherwise ignored).
     */
    std::string UnitDoorIdA;
    std::string UnitDoorIdB;
};

enum class RoomKind : std::uint8_t { Living, Sleeping, Sales, Stock, Passage, Dwelling, Workshop };
struct Room
{
    std::string Id;
    RoomKind Kind = RoomKind::Living;
    Rect Area;                 // clear floor area inside the walls
    std::string FloorPalette;  // "floor_oxide", "floor_tile" ...
    std::string WallPalette;   // "plaster_warm", "plaster_pale" ...
};

enum class OpeningKind : std::uint8_t { Door, Doorway, Window };
struct Opening
{
    std::string Id;
    std::string WallId;
    OpeningKind Kind = OpeningKind::Doorway;
    Rect Plan;                 // footprint in the wall, plan view
    int W0 = 0;                // floor-relative sill (0 for doors)
    int W1 = 0;                // floor-relative head
    int ClearWidthCm = 0;      // along the wall, jamb strips excluded
    std::string RoomA;         // room on the low-V side ("outside" for the front entrance)
    std::string RoomB;
    std::string DoorId;        // lane door id when a leaf is hung, else empty
    int HingeU = 0;            // meaningful when DoorId is set
    int HingeV = 0;
    int SweepSign = 1;         // +1 the leaf opens toward +V (into the building)
};

struct WallSegment
{
    std::string Id;
    std::string WallId;
    Rect Plan;
    int W0 = 0;
    int W1 = 0;
};

enum class FurnitureKind : std::uint8_t { Counter, Shelf, Table, Chair, Bed, Desk, Rack, Carton, Trunk, Display };
struct Furniture
{
    std::string Id;
    std::string AssetId;       // stable asset id, e.g. "asset.lane.furniture.counter"
    std::string Palette;       // colour token the presenter maps to a block colour
    std::string RoomId;
    FurnitureKind Kind = FurnitureKind::Table;
    Rect Footprint;
    int H0 = 0;
    int H1 = 0;
    bool Solid = true;         // physical contract: blocks Pawn, Camera and Visibility with a box of the same size
};

enum class AnchorKind : std::uint8_t { Stand, Seat, Threshold };
struct Anchor
{
    std::string Id;
    AnchorKind Kind = AnchorKind::Stand;
    std::string RoomId;        // "outside" for a threshold
    int U = 0;
    int V = 0;
    int FaceU = 0;             // unit facing, one of -1, 0, 1 each
    int FaceV = 0;
    std::string FurnitureId;   // the piece it serves, when any
};

enum class ZoneKind : std::uint8_t { Circulation, DoorSweep, Standing, SeatApproach, Route };
struct Zone
{
    std::string Id;
    ZoneKind Kind = ZoneKind::Circulation;
    std::string RoomId;
    Rect Area;                 // must stay free of solid furniture
    std::string OwnerId;       // opening, anchor or furniture it belongs to
};

struct Connection
{
    std::string Id;
    std::string RoomA;         // "outside" for the entrance
    std::string RoomB;
    std::string OpeningId;
    int ClearWidthCm = 0;
};

struct LightSpec
{
    std::string Id;
    std::string RoomId;
    int U = 0;
    int V = 0;
    int HeightCm = 0;          // above the finished floor
    int RadiusCm = 0;          // attenuation radius, within the room's shorter side
    int Lumens = 0;
};

struct RoutePoint
{
    int U = 0;
    int V = 0;
};
/** A staff walking line between two anchors: axis-aligned legs, every leg at least 34 cm clear of solid furniture. */
struct Route
{
    std::string Id;
    std::string FromAnchorId;
    std::string ToAnchorId;
    std::vector<RoutePoint> Points;
    [[nodiscard]] int LengthCm() const noexcept
    {
        int total = 0;
        for (size_t i = 0; i + 1 < Points.size(); ++i)
        {
            total += (Points[i + 1].U > Points[i].U ? Points[i + 1].U - Points[i].U : Points[i].U - Points[i + 1].U) +
                     (Points[i + 1].V > Points[i].V ? Points[i + 1].V - Points[i].V : Points[i].V - Points[i + 1].V);
        }
        return total;
    }
};

struct InteriorLayout
{
    Refusal Status = Refusal::None;
    std::string BuildingId;
    Variant Kind = Variant::CompactResidence;
    bool Mirrored = false;
    int WidthCm = 0;
    int DepthCm = 0;
    int FinishedFloorCm = 15;
    int StoreyHeightCm = 300;
    int ClearHeightCm = 274;   // ceiling underside above the finished floor
    Rect Interior;             // inside the perimeter walls
    std::vector<Room> Rooms;
    std::vector<Opening> Openings;
    std::vector<WallSegment> Walls;
    std::vector<Connection> Connections;
    std::vector<Furniture> Pieces;
    std::vector<Anchor> Anchors;
    std::vector<Zone> Zones;
    std::vector<LightSpec> Lights;
    std::vector<Route> Routes;
    [[nodiscard]] bool Ok() const noexcept { return Status == Refusal::None; }
};

/** Refuses invalid inputs with a code and an otherwise empty layout; never repairs. Result is a pure function of the input. */
[[nodiscard]] PERFECTWORLDCORE_API InteriorLayout GenerateInteriorLayout(const InteriorInput& input);

/** Re-checks every contract on any layout: doorways 90x210, corridors 120, free sweeps/stand/seat spaces, connectivity. */
[[nodiscard]] PERFECTWORLDCORE_API Refusal ValidateInteriorLayout(const InteriorLayout& layout);

[[nodiscard]] PERFECTWORLDCORE_API const Anchor* FindAnchor(const InteriorLayout& layout, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const Room* FindRoom(const InteriorLayout& layout, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const Furniture* FindFurniture(const InteriorLayout& layout, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const Route* FindRoute(const InteriorLayout& layout, std::string_view id);

/** Reflects every U coordinate across the frontage (U -> WidthCm - U); ids, ordering and heights are unchanged. */
PERFECTWORLDCORE_API void ReflectLayoutU(InteriorLayout& layout);

/** A canonical text dump of the whole layout, for determinism and stable-id checks. */
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribeLayout(const InteriorLayout& layout);
} // namespace PerfectWorld::LaneInterior
