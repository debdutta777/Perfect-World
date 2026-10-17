#pragma once

#include "LaneInteriorLayout.h"

#include <functional>
#include <string>
#include <vector>

namespace PerfectWorld::LaneInterior::Detail
{
inline constexpr std::string_view OutsideId = "outside";

/** One piece of furniture to place: size in U and V, heights above the floor, and its stable asset. */
struct Item
{
    std::string Name;
    std::string Asset;
    std::string Palette;
    FurnitureKind Kind = FurnitureKind::Table;
    int SU = 0;
    int SV = 0;
    int H0 = 0;
    int H1 = 0;
    bool Solid = true;
};

/** The open side of a piece: where someone stands or sits, and the free space that needs. */
struct Use
{
    int SideU = 0; // -1, 0 or +1: the side the user approaches from (one of SideU, SideV is non-zero)
    int SideV = 0;
    int Depth = 0; // 0 = no use space
    ZoneKind Zone = ZoneKind::Standing;
    bool Anchor = false;
    AnchorKind Kind = AnchorKind::Stand;
    std::string AnchorName;
    int Lateral = 0;      // width of the use space; 0 = the piece's width clamped to 80..120
    bool HasCentre = false;
    int Centre = 0;       // lateral centre override (U for a V side, V for a U side)
};

struct Mark
{
    size_t Pieces = 0;
    size_t Anchors = 0;
    size_t Zones = 0;
};

/** A straight run of wall (A along the run, P across its thickness), cut by openings. */
struct Run
{
    std::string Name;
    bool AlongU = true;
    int P0 = 0;
    int P1 = 0;
    int A0 = 0;
    int A1 = 0;
    std::vector<Opening> Open;
};

class Builder
{
public:
    explicit Builder(const InteriorInput& in);

    InteriorInput In;
    InteriorLayout L;
    int W = 0;
    int D = 0;
    Rect Inner;

    [[nodiscard]] std::string Key(std::string_view kind, std::string_view name) const;

    Room AddRoom(std::string_view name, RoomKind kind, const Rect& area, std::string_view floor, std::string_view wall);
    void AddZone(ZoneKind kind, std::string_view roomId, const Rect& area, std::string_view ownerId, std::string_view name);
    void AddConnection(std::string_view name, std::string_view roomA, std::string_view roomB, const std::string& openingId, int clearWidth);

    /** Adds an opening to the run. A door with a leaf also gets its sweep zone (leaf swings toward +V or +U inward). */
    void AddOpening(Run& run, std::string_view name, OpeningKind kind, int a0, int a1, int w0, int w1, std::string_view roomA,
                    std::string_view roomB, const std::string& doorId = std::string(), bool hingeAtLow = true);
    /** Emits the wall segments between and around the run's openings and records the openings. */
    void EndRun(Run& run);

    [[nodiscard]] bool HitsSolid(const Rect& r) const;
    [[nodiscard]] bool HitsZone(const Rect& r) const;
    [[nodiscard]] static Rect UseRect(const Rect& fp, const Use& use);
    [[nodiscard]] Mark MarkNow() const;
    void Rollback(const Mark& mark);

    /** Places one piece if it and its use space fit in the room and clash with no solid or zone. */
    bool TryPlace(const Room& room, const Item& item, const Rect& fp, const Use& use);
    /** Tries origins on a 5 cm grid, nearest the reference point first; the callback places a whole group or returns false. */
    bool ScanPlace(const Rect& area, int w, int h, int refU, int refV, const std::function<bool(int, int)>& place);

    void AddLights(const Room& room);
    /** Window in the wall run, centred in the span [spanA0, spanA1]; skipped when too short or a tall solid stands against it. */
    void MaybeWindow(Run& run, std::string_view name, int spanA0, int spanA1, int maxWidth, std::string_view room);
    /** Records a staff route and reserves a corridor for it; false when a leg would touch a solid. */
    [[nodiscard]] bool AddRoute(std::string_view name, std::string_view fromAnchor, std::string_view toAnchor, std::vector<RoutePoint> points);
};

[[nodiscard]] Refusal BuildTwoRoom(Builder& b, bool shop);
[[nodiscard]] Refusal BuildSharedEntry(Builder& b);
[[nodiscard]] int ClampInt(int v, int lo, int hi);

// Furniture groups shared by the variants (LaneInteriorFurnish.cpp). Each tries its placements in a stable order and either
// commits everything or nothing. `suffix` keeps ids unique when a variant has several dwellings ("", "_a", "_b").
/** A table with two seats; the seats get anchors seat<suffix>_1 and seat<suffix>_2. */
[[nodiscard]] bool PlaceDiningSet(Builder& b, const Room& room, const std::string& suffix, int refU, int refV);
/** A bed with free space along one long side and a stand anchor bed_side<suffix>. */
[[nodiscard]] bool PlaceBed(Builder& b, const Room& room, const std::string& suffix, int refU, int refV);
/** A wall unit flush to one wall of a strip (U or V range given by `strip`); optional, returns false when nothing fits. */
bool PlaceWallUnit(Builder& b, const Room& room, const Rect& strip, bool alongV, const Item& item, int standDepth, int refU, int refV);
} // namespace PerfectWorld::LaneInterior::Detail
