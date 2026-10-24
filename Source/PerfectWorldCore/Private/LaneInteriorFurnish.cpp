#include "LaneInteriorDetail.h"

namespace PerfectWorld::LaneInterior::Detail
{
namespace
{
constexpr int LfTableLong = 90;
constexpr int LfTableShort = 60;
constexpr int LfSeat = 40;
constexpr int LfBedLong = 190;
constexpr int LfBedShort = 90;

Item LfTable()
{
    return Item{"table", "asset.lane.furniture.table_wood", "wood_light", FurnitureKind::Table, LfTableLong, LfTableShort, 0, 75, true};
}

Item LfSeatItem(const std::string& name)
{
    return Item{name, "asset.lane.furniture.stool_wood", "door_wood", FurnitureKind::Chair, LfSeat, LfSeat, 0, 48, true};
}

Use LfSeatUse(int sideU, int sideV, const std::string& anchor)
{
    Use u;
    u.SideU = sideU;
    u.SideV = sideV;
    u.Depth = SeatApproachCm;
    u.Zone = ZoneKind::SeatApproach;
    u.Anchor = true;
    u.Kind = AnchorKind::Seat;
    u.AnchorName = anchor;
    u.Lateral = LfSeat;
    return u;
}
} // namespace

bool PlaceDiningSet(Builder& b, const Room& room, const std::string& suffix, int refU, int refV)
{
    const std::string seat1 = "seat" + suffix + "_1";
    const std::string seat2 = "seat" + suffix + "_2";
    Item table = LfTable();
    table.Name = "table" + suffix;
    // Orientation 0: the table runs along U with a seat on each of its V sides. Orientation 1: along V, seats on the U sides.
    for (int orientation = 0; orientation < 2; ++orientation)
    {
        const bool alongU = orientation == 0;
        const int w = alongU ? LfTableLong : LfTableShort + 2 * LfSeat;
        const int h = alongU ? LfTableShort + 2 * LfSeat : LfTableLong;
        const bool placed = b.ScanPlace(room.Area, w, h, refU, refV, [&](int u0, int v0) {
            if (alongU)
            {
                const Rect t{u0, u0 + LfTableLong, v0 + LfSeat, v0 + LfSeat + LfTableShort};
                const int cu = u0 + (LfTableLong - LfSeat) / 2;
                return b.TryPlace(room, table, t, Use{}) &&
                       b.TryPlace(room, LfSeatItem("seat_piece" + suffix + "_1"), Rect{cu, cu + LfSeat, v0, v0 + LfSeat}, LfSeatUse(0, -1, seat1)) &&
                       b.TryPlace(room, LfSeatItem("seat_piece" + suffix + "_2"), Rect{cu, cu + LfSeat, t.V1, t.V1 + LfSeat}, LfSeatUse(0, 1, seat2));
            }
            const Rect t{u0 + LfSeat, u0 + LfSeat + LfTableShort, v0, v0 + LfTableLong};
            const int cv = v0 + (LfTableLong - LfSeat) / 2;
            Item rotated = table;
            rotated.SU = LfTableShort;
            rotated.SV = LfTableLong;
            return b.TryPlace(room, rotated, t, Use{}) &&
                   b.TryPlace(room, LfSeatItem("seat_piece" + suffix + "_1"), Rect{u0, u0 + LfSeat, cv, cv + LfSeat}, LfSeatUse(-1, 0, seat1)) &&
                   b.TryPlace(room, LfSeatItem("seat_piece" + suffix + "_2"), Rect{t.U1, t.U1 + LfSeat, cv, cv + LfSeat}, LfSeatUse(1, 0, seat2));
        });
        if (placed)
        {
            return true;
        }
    }
    return false;
}

bool PlaceBed(Builder& b, const Room& room, const std::string& suffix, int refU, int refV)
{
    Item bed{"bed" + suffix, "asset.lane.furniture.bed_frame", "cloth_magenta", FurnitureKind::Bed, LfBedLong, LfBedShort, 0, 50, true};
    for (int orientation = 0; orientation < 2; ++orientation)
    {
        const bool alongU = orientation == 0;
        const int w = alongU ? LfBedLong : LfBedShort;
        const int h = alongU ? LfBedShort : LfBedLong;
        const bool placed = b.ScanPlace(room.Area, w, h, refU, refV, [&](int u0, int v0) {
            const Rect fp{u0, u0 + w, v0, v0 + h};
            for (int side = -1; side <= 1; side += 2)
            {
                Use use;
                use.SideU = alongU ? 0 : side;
                use.SideV = alongU ? side : 0;
                use.Depth = StandZoneCm;
                use.Anchor = true;
                use.Kind = AnchorKind::Stand;
                use.AnchorName = "bed_side" + suffix;
                use.Lateral = StandZoneCm;
                const Mark mark = b.MarkNow();
                Item placedBed = bed;
                placedBed.SU = w;
                placedBed.SV = h;
                if (b.TryPlace(room, placedBed, fp, use))
                {
                    return true;
                }
                b.Rollback(mark);
            }
            return false;
        });
        if (placed)
        {
            return true;
        }
    }
    return false;
}

bool PlaceWallUnit(Builder& b, const Room& room, const Rect& strip, bool alongV, const Item& item, int standDepth, int refU, int refV)
{
    const int fw = item.SU;
    const int fh = item.SV;
    return b.ScanPlace(strip, fw, fh, refU, refV, [&](int u0, int v0) {
        const Rect fp{u0, u0 + fw, v0, v0 + fh};
        for (int side = -1; side <= 1; side += 2)
        {
            Use use;
            use.SideU = alongV ? side : 0;
            use.SideV = alongV ? 0 : side;
            use.Depth = standDepth;
            use.Zone = ZoneKind::Standing;
            const Mark mark = b.MarkNow();
            if (b.TryPlace(room, item, fp, use))
            {
                return true;
            }
            b.Rollback(mark);
        }
        return false;
    });
}
} // namespace PerfectWorld::LaneInterior::Detail
