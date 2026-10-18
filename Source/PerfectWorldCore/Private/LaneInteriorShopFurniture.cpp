#include "LaneInteriorDetail.h"

namespace PerfectWorld::LaneInterior::Detail
{
namespace
{
// NL-03: the general-goods and workshop furniture sets of the two-room shop. Both reuse the stationery shop's skeleton (a counter
// across the sales room with the staff access at the high-U side, a partition door in that staff gap, a back room reached through
// it) and place everything with the kit's existing primitives, so circulation, door sweeps and approach zones stay free by
// construction (TryPlace refuses any solid on a zone) and ValidateInteriorLayout re-proves it. Every required piece returns false
// when it does not fit; the caller turns that into Refusal::FurnitureDoesNotFit. Optional dressing never fails the build.
constexpr int SfCounterDepth = 60;
constexpr int SfShelfDepth = 30;
constexpr int SfStand = StandZoneCm; // 80
constexpr int SfHalfDoor = DoorOpeningWidthCm / 2;
constexpr int SfRackDepth = 40;

Item SfItem(const char* name, const char* asset, const char* palette, FurnitureKind kind, int su, int sv, int h0, int h1)
{
    return Item{name, asset, palette, kind, su, sv, h0, h1, true};
}

/** A stand use on one side of a piece: an 80 deep, 80 wide free space, optionally centred and optionally carrying an anchor. */
Use SfStandUse(int sideU, int sideV, const char* anchorName, bool hasCentre, int centre)
{
    Use u;
    u.SideU = sideU;
    u.SideV = sideV;
    u.Depth = SfStand;
    u.Zone = ZoneKind::Standing;
    u.Anchor = anchorName != nullptr;
    u.AnchorName = anchorName != nullptr ? anchorName : "";
    u.Lateral = SfStand;
    u.HasCentre = hasCentre;
    u.Centre = centre;
    return u;
}

/** Like PlaceWallUnit, but the stand space is 80 square and may carry a named stand anchor (an anchor needs the full 80). */
bool SfUnit(Builder& b, const Room& room, const Rect& strip, bool alongV, const Item& item, int refU, int refV, const char* anchorName,
            bool routeFromDoor)
{
    return b.ScanPlace(strip, item.SU, item.SV, refU, refV, [&](int u0, int v0) {
        const Rect fp{u0, u0 + item.SU, v0, v0 + item.SV};
        for (int side = -1; side <= 1; side += 2)
        {
            const Mark mark = b.MarkNow();
            if (b.TryPlace(room, item, fp, SfStandUse(alongV ? side : 0, alongV ? 0 : side, anchorName, false, 0)))
            {
                if (!routeFromDoor || anchorName == nullptr)
                {
                    return true;
                }
                // A customer point is only useful if someone can walk to it: reserve a walking line from the footpath, so that no
                // later piece can wall it in.
                const Anchor placed = b.L.Anchors.back();
                const int c = b.In.EntranceCentreU;
                if (b.AddRoute(std::string("door_to_") + anchorName, "front_door", anchorName, {{c, -45}, {c, placed.V}, {placed.U, placed.V}}))
                {
                    return true;
                }
            }
            b.Rollback(mark);
        }
        return false;
    });
}

/** A piece with no stand space of its own (dressing). */
bool SfDress(Builder& b, const Room& room, const Rect& strip, bool alongV, const Item& item, int refU, int refV)
{
    return PlaceWallUnit(b, room, strip, alongV, item, 60, refU, refV);
}

/** The counter across the sales room, the staff and customer stand points either side of it. cu is the shared lateral position. */
bool SfCounter(Builder& b, const ShopFrame& f, const char* asset, int& cu)
{
    const Rect in = b.Inner;
    const int counterU1 = in.U1 - StaffGapCm;
    cu = ClampInt(f.S + CirculationCm / 2, in.U0 + SfStand / 2, counterU1 - SfStand / 2);
    const Item counter = SfItem("counter", asset, "wood_light", FurnitureKind::Counter, counterU1 - in.U0, SfCounterDepth, 0, 90);
    if (!b.TryPlace(f.Sales, counter, Rect{in.U0, counterU1, f.Cv1, f.Cv1 + SfCounterDepth}, SfStandUse(0, 1, "staff_counter", true, cu)))
    {
        return false;
    }
    Anchor customer;
    customer.Id = b.Key("anchor", "customer_counter");
    customer.Kind = AnchorKind::Stand;
    customer.RoomId = f.Sales.Id;
    customer.U = cu;
    customer.V = f.Cv1 - SfStand / 2;
    customer.FaceV = 1;
    customer.FurnitureId = b.Key("piece", "counter");
    b.L.Anchors.push_back(customer);
    b.AddZone(ZoneKind::Standing, f.Sales.Id, Rect{cu - SfStand / 2, cu + SfStand / 2, f.Cv1 - SfStand, f.Cv1}, customer.Id, "customer_counter.use");
    return true;
}

/** The shelf on the partition behind the staff, as in the stationery shop; dressing. */
void SfShelfBehindCounter(Builder& b, const ShopFrame& f, const char* name)
{
    const Rect in = b.Inner;
    const int shelfEnd = f.GapC - SfHalfDoor - 20; // 20 cm clear of the doorway jamb
    if (shelfEnd - in.U0 < 100)
    {
        return;
    }
    const Item back = SfItem(name, "asset.lane.furniture.shelf_wall", "wood_light", FurnitureKind::Shelf, shelfEnd - in.U0, SfShelfDepth, 0, 180);
    const Mark mark = b.MarkNow();
    if (!b.TryPlace(f.Sales, back, Rect{in.U0, shelfEnd, f.Pv0 - SfShelfDepth, f.Pv0}, SfStandUse(0, -1, nullptr, false, 0)))
    {
        b.Rollback(mark);
    }
}

/** A full-width rack along the back wall of the back room with a stand point in front of it, centred in the staff column. */
bool SfBackRack(Builder& b, const ShopFrame& f, const char* name, const char* asset, const char* palette, const char* anchorName)
{
    const Rect in = b.Inner;
    return b.TryPlace(f.Back, SfItem(name, asset, palette, FurnitureKind::Rack, in.Width(), SfRackDepth, 0, 180),
                      Rect{in.U0, in.U1, in.V1 - SfRackDepth, in.V1}, SfStandUse(0, -1, anchorName, true, f.GapC));
}

void SfFrontDoorAnchor(Builder& b)
{
    Anchor door;
    door.Id = b.Key("anchor", "front_door");
    door.Kind = AnchorKind::Threshold;
    door.RoomId = std::string(OutsideId);
    door.U = b.In.EntranceCentreU;
    door.V = -45;
    door.FaceV = 1;
    b.L.Anchors.push_back(door);
}

/**
 * The staff routes: from the counter through the staff gap to the back-room anchors and out to the front door. `workU`/`workV` is
 * the work anchor in the back room (it ends the third leg); the rack anchor ends the straight run down the gap.
 */
bool SfRoutes(Builder& b, const ShopFrame& f, int cu, const char* rackAnchor, const char* workAnchor, const char* rackRoute,
              const char* workRoute, int workU, int workV)
{
    const Rect in = b.Inner;
    const int counterV = f.Cv1 + SfCounterDepth + SfStand / 2;
    const int crossV = f.Cv1 - 60;
    const int c = b.In.EntranceCentreU;
    return b.AddRoute(rackRoute, "staff_counter", rackAnchor, {{cu, counterV}, {f.GapC, counterV}, {f.GapC, in.V1 - SfRackDepth - SfStand / 2}}) &&
           b.AddRoute(workRoute, "staff_counter", workAnchor, {{cu, counterV}, {f.GapC, counterV}, {f.GapC, workV}, {workU, workV}}) &&
           b.AddRoute("counter_to_door", "staff_counter", "front_door", {{cu, counterV}, {f.GapC, counterV}, {f.GapC, crossV}, {c, crossV}, {c, -45}});
}
} // namespace

bool FurnishGeneralGoods(Builder& b, const ShopFrame& f)
{
    const Rect in = b.Inner;
    int cu = 0;
    if (!SfCounter(b, f, "asset.lane.furniture.counter_shop", cu))
    {
        return false;
    }
    SfShelfBehindCounter(b, f, "shelf_back");
    if (!SfBackRack(b, f, "rack_stock", "asset.lane.furniture.rack_steel", "shutter_steel", "stock_room"))
    {
        return false;
    }
    // The weighing table: where loose goods are weighed and packed, against the partition beside the staff column.
    const Item weigh = SfItem("table_weigh", "asset.lane.furniture.table_weigh", "wood_light", FurnitureKind::Table, 100, 60, 0, 85);
    if (!b.TryPlace(f.Back, weigh, Rect{f.Sd - 100, f.Sd, f.Pv1, f.Pv1 + 60}, SfStandUse(0, 1, "weigh_station", false, 0)))
    {
        return false;
    }
    // A customer must be able to browse: a wall shelf with a stand point on the left of the entrance stem, else grain bins on the
    // right against the front wall. A shop with neither has nothing to sell off the floor and is refused.
    const Rect leftStrip{in.U0, in.U0 + SfShelfDepth, in.V0, f.Cv1};
    const Item wallShelf = SfItem("shelf_wall_a", "asset.lane.furniture.shelf_wall", "wood_light", FurnitureKind::Shelf, SfShelfDepth, 150, 0, 180);
    const Item bin = SfItem("bin_a", "asset.lane.furniture.bin_grain", "goods_orange", FurnitureKind::Display, 50, 80, 0, 70);
    const Rect rightStrip{in.U1 - 50, in.U1, in.V0, f.Cv1};
    // The route zones (reserved below) must not be crossed, so the browse points are tried against them first: reserve routes now.
    const int weighU = f.Sd - 50;
    const int weighV = f.Pv1 + 60 + SfStand / 2;
    if (!SfRoutes(b, f, cu, "stock_room", "weigh_station", "counter_to_stock_room", "counter_to_weigh_station", weighU, weighV))
    {
        return false;
    }
    SfFrontDoorAnchor(b);
    if (!SfUnit(b, f.Sales, leftStrip, true, wallShelf, in.U0, in.V0 + 110, "browse_shelf", true) &&
        !SfUnit(b, f.Sales, rightStrip, true, bin, in.U1, in.V0 + 40, "browse_bins", true))
    {
        return false;
    }
    // Dressing: more bins and sacks. Never required.
    SfDress(b, f.Sales, rightStrip, true, SfItem("bin_b", "asset.lane.furniture.bin_grain", "goods_yellow", FurnitureKind::Display, 50, 80, 0, 70), in.U1,
            in.V0 + 40);
    SfDress(b, f.Sales, Rect{in.U1 - 30, in.U1, in.V0, f.Cv1}, true,
            SfItem("shelf_wall_b", "asset.lane.furniture.shelf_wall", "wood_light", FurnitureKind::Shelf, SfShelfDepth, 150, 0, 180), in.U1, in.V0 + 110);
    const Rect stockArea{in.U0, in.U1, f.Pv1, in.V1 - SfRackDepth};
    SfDress(b, f.Back, stockArea, false, SfItem("sacks_a", "asset.lane.furniture.sack_stack", "goods_orange", FurnitureKind::Carton, 60, 50, 0, 70),
            in.U0 + 30, in.V1 - 70);
    SfDress(b, f.Back, stockArea, false, SfItem("sacks_b", "asset.lane.furniture.sack_stack", "goods_yellow", FurnitureKind::Carton, 60, 50, 0, 70),
            in.U0 + 100, in.V1 - 70);
    SfDress(b, f.Back, stockArea, false, SfItem("carton_a", "asset.lane.furniture.carton_stack", "goods_yellow", FurnitureKind::Carton, 60, 50, 0, 70),
            in.U0 + 170, in.V1 - 70);
    return true;
}

bool FurnishWorkshop(Builder& b, const ShopFrame& f)
{
    const Rect in = b.Inner;
    int cu = 0;
    if (!SfCounter(b, f, "asset.lane.furniture.counter_shop", cu))
    {
        return false;
    }
    SfShelfBehindCounter(b, f, "shelf_jobs"); // finished jobs waiting for collection
    if (!SfBackRack(b, f, "rack_tools", "asset.lane.furniture.rack_tools", "shutter_steel", "tool_rack"))
    {
        return false;
    }
    // The main bench against the partition, its working side facing the room.
    const Item bench = SfItem("bench_main", "asset.lane.furniture.bench_work", "wood_light", FurnitureKind::Desk, 180, 70, 0, 85);
    if (!b.TryPlace(f.Back, bench, Rect{f.Sd - 180, f.Sd, f.Pv1, f.Pv1 + 70}, SfStandUse(0, 1, "workbench", false, 0)))
    {
        return false;
    }
    // The finishing table: a second work position (sewing, sanding, assembly) against the partition on the far side from the staff
    // gap, with a stand point. A footprint too narrow for the bench and the table side by side cannot hold a workshop.
    const Item table = SfItem("table_finishing", "asset.lane.furniture.table_finishing", "door_wood", FurnitureKind::Table, 120, 60, 0, 80);
    if (!b.TryPlace(f.Back, table, Rect{in.U0, in.U0 + 120, f.Pv1, f.Pv1 + 60}, SfStandUse(0, 1, "finishing_table", false, 0)))
    {
        return false;
    }
    const int workV = f.Pv1 + 70 + SfStand / 2;
    if (!SfRoutes(b, f, cu, "tool_rack", "workbench", "counter_to_tool_rack", "counter_to_workbench", f.Sd - 90, workV))
    {
        return false;
    }
    const int counterV = f.Cv1 + SfCounterDepth + SfStand / 2;
    if (!b.AddRoute("counter_to_finishing_table", "staff_counter", "finishing_table",
                    {{cu, counterV}, {f.GapC, counterV}, {f.GapC, workV}, {in.U0 + 60, workV}, {in.U0 + 60, f.Pv1 + 60 + SfStand / 2}}))
    {
        return false;
    }
    SfFrontDoorAnchor(b);
    const Rect floorArea{in.U0, in.U1, f.Pv1, in.V1 - SfRackDepth};
    // Dressing: a short display of samples by the entrance side wall, cloth bolts and an offcut bin on the work floor.
    SfDress(b, f.Sales, Rect{in.U0, in.U1, in.V0, f.Cv1}, false,
            SfItem("display_samples", "asset.lane.furniture.display_table", "wood_light", FurnitureKind::Display, 80, 50, 0, 70), in.U0 + 60, in.V0 + 60);
    SfDress(b, f.Back, floorArea, false, SfItem("bolts_a", "asset.lane.furniture.bolts_cloth", "cloth_teal", FurnitureKind::Carton, 60, 50, 0, 70),
            in.U0 + 30, in.V1 - 70);
    SfDress(b, f.Back, floorArea, false, SfItem("offcut_bin", "asset.lane.furniture.bin_offcut", "trim_dark", FurnitureKind::Carton, 50, 50, 0, 60),
            in.U0 + 100, in.V1 - 70);
    return true;
}
} // namespace PerfectWorld::LaneInterior::Detail
