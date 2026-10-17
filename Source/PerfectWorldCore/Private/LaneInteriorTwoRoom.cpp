#include "LaneInteriorDetail.h"

#include <algorithm>

namespace PerfectWorld::LaneInterior::Detail
{
namespace
{
constexpr int LtHalfDoor = DoorOpeningWidthCm / 2; // 47
constexpr int LtCounterDepth = 60;
constexpr int LtStaffDepth = 150;                  // behind the counter: 30 of wall shelf and a 120 corridor
constexpr int LtShelfDepth = 30;
constexpr int LtStandDepth = StandZoneCm;

Item LtItem(const char* name, const char* asset, const char* palette, FurnitureKind kind, int su, int sv, int h0, int h1)
{
    return Item{name, asset, palette, kind, su, sv, h0, h1, true};
}

Use LtStandUse(int sideV, const char* anchorName, int centre, bool hasCentre, int lateral)
{
    Use u;
    u.SideV = sideV;
    u.Depth = LtStandDepth;
    u.Anchor = anchorName != nullptr;
    u.AnchorName = anchorName != nullptr ? anchorName : "";
    u.Lateral = lateral;
    u.HasCentre = hasCentre;
    u.Centre = centre;
    return u;
}

/** Sales room fittings of the stationery set. Required pieces return false when they do not fit. */
bool LtFurnishShop(Builder& b, const Room& sales, const Room& stock, int cv1, int pv0, int pv1, int gapC, int sd, int s)
{
    const Rect in = b.Inner;
    const int counterU1 = in.U1 - StaffGapCm;
    // The staff stand point faces the customer's: someone talking across the counter is 140 cm from the other.
    const int cu = ClampInt(s + CirculationCm / 2, in.U0 + LtStandDepth / 2, counterU1 - LtStandDepth / 2);
    const Item counter = LtItem("counter", "asset.lane.furniture.counter_shop", "wood_light", FurnitureKind::Counter, counterU1 - in.U0,
                                LtCounterDepth, 0, 90);
    if (!b.TryPlace(sales, counter, Rect{in.U0, counterU1, cv1, cv1 + LtCounterDepth}, LtStandUse(1, "staff_counter", cu, true, LtStandDepth)))
    {
        return false;
    }
    // The customer's side of the counter: a stand point 40 cm from its face.
    const int customerU = cu;
    const Rect customerZone{customerU - LtStandDepth / 2, customerU + LtStandDepth / 2, cv1 - LtStandDepth, cv1};
    Anchor customer;
    customer.Id = b.Key("anchor", "customer_counter");
    customer.Kind = AnchorKind::Stand;
    customer.RoomId = sales.Id;
    customer.U = customerU;
    customer.V = cv1 - LtStandDepth / 2;
    customer.FaceV = 1;
    customer.FurnitureId = b.Key("piece", "counter");
    b.L.Anchors.push_back(customer);
    b.AddZone(ZoneKind::Standing, sales.Id, customerZone, customer.Id, "customer_counter.use");

    const int shelfEnd = gapC - LtHalfDoor - 20; // 20 cm clear of the doorway jamb
    if (shelfEnd - in.U0 >= 100)
    {
        const Item back = LtItem("shelf_back", "asset.lane.furniture.shelf_wall", "wood_light", FurnitureKind::Shelf, shelfEnd - in.U0,
                                 LtShelfDepth, 0, 180);
        const Mark mark = b.MarkNow();
        if (!b.TryPlace(sales, back, Rect{in.U0, shelfEnd, pv0 - LtShelfDepth, pv0}, LtStandUse(-1, nullptr, 0, false, 0)))
        {
            b.Rollback(mark);
        }
    }

    const Item rack = LtItem("rack_stock", "asset.lane.furniture.rack_steel", "shutter_steel", FurnitureKind::Rack, in.Width(), 40, 0, 180);
    if (!b.TryPlace(stock, rack, Rect{in.U0, in.U1, in.V1 - 40, in.V1}, LtStandUse(-1, "stock_room", gapC, true, LtStandDepth)))
    {
        return false;
    }
    const Item desk = LtItem("desk_packing", "asset.lane.furniture.desk_packing", "wood_light", FurnitureKind::Desk, 160, 60, 0, 78);
    if (!b.TryPlace(stock, desk, Rect{sd - 160, sd, pv1, pv1 + 60}, LtStandUse(1, "packing_desk", 0, false, 0)))
    {
        return false;
    }
    Anchor door;
    door.Id = b.Key("anchor", "front_door");
    door.Kind = AnchorKind::Threshold;
    door.RoomId = std::string(OutsideId);
    door.U = b.In.EntranceCentreU;
    door.V = -45;
    door.FaceV = 1;
    b.L.Anchors.push_back(door);

    // Staff walking lines. Reserved before the optional fittings so that nothing gets placed on them.
    const int counterV = cv1 + LtCounterDepth + LtStandDepth / 2;
    const int deskV = pv1 + 60 + LtStandDepth / 2;
    const int deskU = sd - 80;
    const int c = b.In.EntranceCentreU;
    const int crossV = cv1 - 60;
    if (!b.AddRoute("counter_to_stock_room", "staff_counter", "stock_room",
                    {{cu, counterV}, {gapC, counterV}, {gapC, in.V1 - 40 - LtStandDepth / 2}}) ||
        !b.AddRoute("counter_to_packing_desk", "staff_counter", "packing_desk",
                    {{cu, counterV}, {gapC, counterV}, {gapC, deskV}, {deskU, deskV}}) ||
        !b.AddRoute("counter_to_door", "staff_counter", "front_door",
                    {{cu, counterV}, {gapC, counterV}, {gapC, crossV}, {c, crossV}, {c, door.V}}))
    {
        return false;
    }

    // Optional dressing: wall shelves of exercise books on the side walls, a pen stand, cartons in the stock room.
    const Rect stripA{in.U0, in.U0 + LtShelfDepth, in.V0, cv1};
    const Rect stripB{in.U1 - LtShelfDepth, in.U1, in.V0, cv1};
    PlaceWallUnit(b, sales, stripA, true,
                  LtItem("shelf_wall_a", "asset.lane.furniture.shelf_wall", "wood_light", FurnitureKind::Shelf, LtShelfDepth, 150, 0, 180), 60,
                  in.U0, in.V0 + 110);
    PlaceWallUnit(b, sales, stripB, true,
                  LtItem("shelf_wall_b", "asset.lane.furniture.shelf_wall", "wood_light", FurnitureKind::Shelf, LtShelfDepth, 150, 0, 180), 60,
                  in.U1, in.V0 + 110);
    PlaceWallUnit(b, sales, Rect{in.U0, in.U1, in.V0, cv1}, false,
                  LtItem("display_pens", "asset.lane.furniture.display_table", "wood_light", FurnitureKind::Display, 80, 50, 0, 70), 60,
                  in.U0 + 60, in.V0 + 60);
    const Rect cartonArea{in.U0, in.U1, pv1, in.V1 - 40};
    PlaceWallUnit(b, stock, cartonArea, false,
                  LtItem("carton_a", "asset.lane.furniture.carton_stack", "goods_yellow", FurnitureKind::Carton, 60, 50, 0, 70), 60,
                  in.U0 + 30, in.V1 - 70);
    PlaceWallUnit(b, stock, cartonArea, false,
                  LtItem("carton_b", "asset.lane.furniture.carton_stack", "goods_yellow", FurnitureKind::Carton, 60, 50, 0, 70), 60,
                  in.U0 + 100, in.V1 - 70);
    return true;
}

bool LtFurnishResidence(Builder& b, const Room& living, const Room& sleeping, int s, int sd)
{
    const Rect in = b.Inner;
    const int leftSpace = s - in.U0;
    const int rightSpace = in.U1 - (s + CirculationCm);
    const int refU = leftSpace >= rightSpace ? in.U0 : in.U1;
    if (!PlaceDiningSet(b, living, "", refU, in.V0))
    {
        return false;
    }
    if (!PlaceBed(b, sleeping, "", in.U0, in.V1))
    {
        return false;
    }
    (void)sd;
    PlaceWallUnit(b, living, Rect{in.U0, in.U0 + LtShelfDepth, living.Area.V0, living.Area.V1}, true,
                  LtItem("shelf_wall", "asset.lane.furniture.shelf_wall", "wood_light", FurnitureKind::Shelf, LtShelfDepth, 100, 0, 160), 60,
                  in.U0, in.V0 + 100);
    PlaceWallUnit(b, sleeping, sleeping.Area, false,
                  LtItem("trunk", "asset.lane.furniture.trunk_wood", "door_wood", FurnitureKind::Trunk, 60, 40, 0, 48), 60, in.U1, in.V1);
    return true;
}
} // namespace

Refusal BuildTwoRoom(Builder& b, bool shop)
{
    const InteriorInput& in = b.In;
    const Rect Inner = b.Inner;
    const int W = b.W;
    const int D = b.D;
    const int c = in.EntranceCentreU;
    const int inner = Inner.Depth();
    const bool stationery = in.Furniture == FurnitureSet::Stationery;
    const bool residence = in.Furniture == FurnitureSet::Residence;

    int cv1 = 0; // shop: the counter's customer face
    int pv0 = 0; // the partition's front face
    if (shop)
    {
        const int remaining = inner - (LtCounterDepth + LtStaffDepth + PartitionThicknessCm);
        cv1 = Inner.V0 + std::max(200, remaining * 45 / 100);
        pv0 = cv1 + LtCounterDepth + LtStaffDepth;
    }
    else
    {
        const int front = (inner - PartitionThicknessCm) * 55 / 100;
        pv0 = Inner.V0 + std::max(220, std::min(front, inner - PartitionThicknessCm - 220));
    }
    const int pv1 = pv0 + PartitionThicknessCm;
    const int gapC = Inner.U1 - StaffGapCm / 2;
    const int d = shop ? gapC : (in.InternalDoorCentreU > 0 ? in.InternalDoorCentreU : ClampInt(c, Inner.U0 + LtHalfDoor, Inner.U1 - LtHalfDoor));

    const Room front = b.AddRoom(shop ? "sales" : "living", shop ? RoomKind::Sales : RoomKind::Living, Rect{Inner.U0, Inner.U1, Inner.V0, pv0},
                                 "floor_oxide", "plaster_warm");
    const Room back = b.AddRoom(shop ? "stock" : "sleeping", shop ? RoomKind::Stock : RoomKind::Sleeping, Rect{Inner.U0, Inner.U1, pv1, Inner.V1},
                                shop ? "floor_tile" : "floor_oxide", "plaster_pale");

    Run frontWall{"front", true, 0, WallThicknessCm, 0, W, {}};
    Run backWall{"back", true, D - WallThicknessCm, D, 0, W, {}};
    Run sideA{"side_a", false, 0, WallThicknessCm, Inner.V0, Inner.V1, {}};
    Run sideB{"side_b", false, W - WallThicknessCm, W, Inner.V0, Inner.V1, {}};
    Run partition{"partition", true, pv0, pv1, Inner.U0, Inner.U1, {}};

    const std::string entranceId = b.Key("opening", "entrance");
    b.AddOpening(frontWall, "entrance", in.EntranceDoorId.empty() ? OpeningKind::Doorway : OpeningKind::Door, c - LtHalfDoor, c + LtHalfDoor, 0,
                 DoorOpeningHeightCm, OutsideId, front.Id, in.EntranceDoorId, true);
    const std::string internalId = b.Key("opening", "internal");
    b.AddOpening(partition, "internal", OpeningKind::Doorway, d - LtHalfDoor, d + LtHalfDoor, 0, DoorOpeningHeightCm, front.Id, back.Id);
    b.AddConnection("entrance", OutsideId, front.Id, entranceId, DoorClearWidthCm);
    b.AddConnection("internal", front.Id, back.Id, internalId, DoorClearWidthCm);

    // Circulation: a 120 stem from the entrance, the doorway's approach on both sides, and (shop) the staff corridor and gap.
    const int s = ClampInt(c - CirculationCm / 2, Inner.U0, Inner.U1 - CirculationCm);
    const int sd = ClampInt(d - CirculationCm / 2, Inner.U0, Inner.U1 - CirculationCm);
    b.AddZone(ZoneKind::Circulation, front.Id, Rect{s, s + CirculationCm, Inner.V0, shop ? cv1 : pv0}, entranceId, "stem");
    b.AddZone(ZoneKind::Circulation, back.Id, Rect{sd, sd + CirculationCm, pv1, pv1 + CirculationCm}, internalId, "back_approach");
    if (shop)
    {
        b.AddZone(ZoneKind::Circulation, front.Id, Rect{Inner.U0, Inner.U1, cv1 + LtCounterDepth, pv0 - LtShelfDepth}, internalId, "staff_corridor");
        b.AddZone(ZoneKind::Circulation, front.Id, Rect{Inner.U1 - StaffGapCm, Inner.U1, cv1, pv0}, internalId, "staff_gap");
        b.AddZone(ZoneKind::Circulation, back.Id, Rect{sd, sd + CirculationCm, pv1, Inner.V1 - 40}, internalId, "stock_column");
    }
    else
    {
        b.AddZone(ZoneKind::Circulation, front.Id, Rect{std::min(s, sd), std::max(s, sd) + CirculationCm, pv0 - CirculationCm, pv0}, internalId,
                  "band");
    }

    if (stationery && shop)
    {
        if (!LtFurnishShop(b, front, back, cv1, pv0, pv1, gapC, sd, s))
        {
            return Refusal::FurnitureDoesNotFit;
        }
    }
    else if (residence && !shop)
    {
        if (!LtFurnishResidence(b, front, back, s, sd))
        {
            return Refusal::FurnitureDoesNotFit;
        }
    }

    const int windowWidth = shop ? 200 : 120;
    b.MaybeWindow(frontWall, "window_a", Inner.U0, c - LtHalfDoor, windowWidth, front.Id);
    b.MaybeWindow(frontWall, "window_b", c + LtHalfDoor, Inner.U1, windowWidth, front.Id);
    if (!shop)
    {
        b.MaybeWindow(backWall, "window_back", Inner.U0, Inner.U1, 120, back.Id);
    }
    b.EndRun(frontWall);
    b.EndRun(backWall);
    b.EndRun(sideA);
    b.EndRun(sideB);
    b.EndRun(partition);
    b.AddLights(front);
    b.AddLights(back);
    return Refusal::None;
}
} // namespace PerfectWorld::LaneInterior::Detail
