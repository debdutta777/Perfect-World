// LL-03: the parametric interior kit's portable layout (LaneInteriorLayout.h). Covers narrow and wide footprints, mirrored
// orientation, deterministic output, invalid layouts, stable ids and disconnected rooms, plus the physical contracts:
// 90 x 210 doorways, 120 cm circulation, unoccupied sweep / stand / seat spaces and floor-relative heights.

#include "LaneInteriorLayout.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>

using namespace PerfectWorld::LaneInterior;

namespace
{
int Assertions = 0;
void Expect(bool condition, const char* message)
{
    ++Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

InteriorInput ShopInput(int width, int depth, int entrance)
{
    InteriorInput in;
    in.BuildingId = "building.test.shop";
    in.Kind = Variant::ShopWithStockroom;
    in.Furniture = FurnitureSet::Stationery;
    in.WidthCm = width;
    in.DepthCm = depth;
    in.EntranceCentreU = entrance;
    in.EntranceDoorId = "door.test.front";
    return in;
}

InteriorInput HomeInput(Variant kind, int width, int depth, int entrance)
{
    InteriorInput in;
    in.BuildingId = kind == Variant::SharedEntry ? "building.test.shared" : "building.test.home";
    in.Kind = kind;
    in.Furniture = FurnitureSet::Residence;
    in.WidthCm = width;
    in.DepthCm = depth;
    in.EntranceCentreU = entrance;
    return in;
}

bool Solid(const InteriorLayout& l, const Rect& r)
{
    return std::any_of(l.Pieces.begin(), l.Pieces.end(), [&](const Furniture& f) { return f.Solid && f.Footprint.Overlaps(r); });
}

/** The contracts, re-derived here rather than trusted from the validator. */
void ExpectContracts(const InteriorLayout& l, const char* what)
{
    Expect(l.Ok(), what);
    Expect(ValidateInteriorLayout(l) == Refusal::None, what);
    Expect(l.ClearHeightCm == l.StoreyHeightCm - SlabAndPlasterCm, "clear height is floor-relative");
    bool hasEntrance = false;
    for (const Opening& o : l.Openings)
    {
        if (o.Kind == OpeningKind::Window)
        {
            Expect(o.W0 >= 60 && o.W1 <= l.ClearHeightCm, "window heights are floor-relative and inside the storey");
            continue;
        }
        Expect(o.ClearWidthCm >= 90, "doorway at least 90 clear");
        Expect(o.W0 == 0 && o.W1 - o.W0 >= 210, "doorway at least 210 clear, from the floor");
        hasEntrance = hasEntrance || o.RoomA == "outside";
    }
    Expect(hasEntrance, "an entrance to the outside");
    for (const Zone& z : l.Zones)
    {
        if (z.Kind == ZoneKind::Circulation)
            Expect(z.Area.Width() >= 120 && z.Area.Depth() >= 120, "circulation at least 120 each way");
        Expect(!Solid(l, z.Area), "zones stay free of solid furniture");
        if (z.Kind == ZoneKind::DoorSweep)
            Expect(z.Area.Width() >= 90 && z.Area.Depth() >= 90, "a door sweep covers the full leaf");
    }
    for (const Anchor& a : l.Anchors)
    {
        if (a.Kind == AnchorKind::Stand)
            Expect(!Solid(l, Rect{a.U - 40, a.U + 40, a.V - 40, a.V + 40}), "stand points have an 80 cm free square");
        if (a.Kind == AnchorKind::Seat)
        {
            const Furniture* f = nullptr;
            for (const Furniture& p : l.Pieces) if (p.Id == a.FurnitureId) f = &p;
            Expect(f != nullptr && f->Footprint.ContainsPoint(a.U, a.V), "a seat point lies on its seat");
            Expect(f->H1 > 45 && f->H1 <= 50, "seats stand just above the 45 cm step-up so nothing can be climbed");
        }
    }
    for (const Furniture& f : l.Pieces)
    {
        Expect(f.H0 >= 0 && f.H1 <= l.ClearHeightCm, "furniture heights are floor-relative");
        Expect(!f.Solid || f.H1 > 45, "no climbable solid below the 45 cm step-up");
    }
    for (const LightSpec& s : l.Lights)
    {
        Expect(s.HeightCm < l.ClearHeightCm && s.RadiusCm >= 150 && s.Lumens >= 600, "light specs are sane");
    }
}

std::set<std::string> AllIds(const InteriorLayout& l)
{
    std::set<std::string> ids;
    for (const auto& x : l.Rooms) ids.insert(x.Id);
    for (const auto& x : l.Openings) ids.insert(x.Id);
    for (const auto& x : l.Walls) ids.insert(x.Id);
    for (const auto& x : l.Connections) ids.insert(x.Id);
    for (const auto& x : l.Pieces) ids.insert(x.Id);
    for (const auto& x : l.Anchors) ids.insert(x.Id);
    for (const auto& x : l.Zones) ids.insert(x.Id);
    for (const auto& x : l.Lights) ids.insert(x.Id);
    for (const auto& x : l.Routes) ids.insert(x.Id);
    return ids;
}

void ShopReference()
{
    const InteriorInput in = ShopInput(700, 700, 250);
    const InteriorLayout l = GenerateInteriorLayout(in);
    ExpectContracts(l, "stationery reference footprint");
    Expect(l.Rooms.size() == 2 && l.Connections.size() == 2, "sales room and stock room, two connections");
    const char* names[] = {"staff_counter", "customer_counter", "stock_room", "packing_desk", "front_door"};
    for (const char* n : names)
        Expect(FindAnchor(l, std::string("building.test.shop.anchor.") + n) != nullptr, "all shop anchors exist");
    const Anchor* door = FindAnchor(l, "building.test.shop.anchor.front_door");
    Expect(door->Kind == AnchorKind::Threshold && door->U == 250 && door->V < 0 && door->FaceV == 1, "front door anchor outside the door, facing in");
    const Anchor* staff = FindAnchor(l, "building.test.shop.anchor.staff_counter");
    const Anchor* customer = FindAnchor(l, "building.test.shop.anchor.customer_counter");
    Expect(staff->FaceV == -1 && customer->FaceV == 1 && staff->V > customer->V, "staff behind the counter facing the door, customer in front");
    for (const char* r : {"counter_to_stock_room", "counter_to_packing_desk", "counter_to_door"})
    {
        const Route* route = FindRoute(l, std::string("building.test.shop.route.") + r);
        Expect(route != nullptr && route->LengthCm() > 100 && route->LengthCm() < 1200, "shop routes exist with plausible lengths");
    }
    int doors = 0;
    for (const Opening& o : l.Openings) doors += o.DoorId == "door.test.front" ? 1 : 0;
    Expect(doors == 1, "exactly one hung leaf, the lane door id passed in");
    const Opening* entrance = nullptr;
    for (const Opening& o : l.Openings) if (o.Id == "building.test.shop.opening.entrance") entrance = &o;
    Expect(entrance != nullptr && entrance->Kind == OpeningKind::Door && entrance->Plan.U0 == 203 && entrance->Plan.U1 == 297, "entrance 94 wide around U 250");
    Expect(entrance->HingeU == 203 && entrance->HingeV == 12, "hinge on the low jamb, mid-wall");
    // The counter spans the room from one side wall and leaves the 120 cm staff access at the far side.
    const Furniture* counter = nullptr;
    for (const Furniture& f : l.Pieces) if (f.Id == "building.test.shop.piece.counter") counter = &f;
    Expect(counter != nullptr && counter->Footprint.U0 == 25 && counter->Footprint.U1 == 675 - 120 && counter->H1 == 90, "counter 90 high with a staff gap");
}

void NarrowAndWide()
{
    const struct { Variant Kind; int W; int D; int C; } cases[] = {
        {Variant::ShopWithStockroom, 420, 680, 100}, {Variant::ShopWithStockroom, 1100, 900, 300},
        {Variant::CompactResidence, 420, 520, 100}, {Variant::CompactResidence, 1000, 900, 500},
        {Variant::SharedEntry, 700, 700, 350},      {Variant::SharedEntry, 1100, 800, 550},
        // The footprints the Unreal kit automation uses.
        {Variant::ShopWithStockroom, 700, 700, 250}, {Variant::CompactResidence, 1000, 900, 300}, {Variant::SharedEntry, 640, 600, 320},
        {Variant::ShopWithStockroom, 420, 680, 300}};
    for (const auto& c : cases)
    {
        const InteriorInput in = c.Kind == Variant::ShopWithStockroom ? ShopInput(c.W, c.D, c.C) : HomeInput(c.Kind, c.W, c.D, c.C);
        const InteriorLayout l = GenerateInteriorLayout(in);
        if (!l.Ok()) std::cerr << "case " << c.W << 'x' << c.D << " c=" << c.C << " -> " << RefusalCode(l.Status) << '\n';
        ExpectContracts(l, "narrow and wide footprints");
        Expect(l.Rooms.size() >= 2 && !l.Lights.empty() && !l.Walls.empty(), "rooms, walls and lights are produced");
        if (c.Kind != Variant::ShopWithStockroom)
            Expect(std::count_if(l.Anchors.begin(), l.Anchors.end(), [](const Anchor& a) { return a.Kind == AnchorKind::Seat; }) >= 2,
                   "residences have usable seats");
    }
    // A wide shop has more room lights than a narrow one.
    Expect(GenerateInteriorLayout(ShopInput(1100, 900, 300)).Lights.size() > GenerateInteriorLayout(ShopInput(420, 680, 100)).Lights.size(), "wider rooms get more lights");
}

void MirroredAndDeterministic()
{
    for (const InteriorInput& base : {ShopInput(700, 700, 250), ShopInput(700, 700, 450), HomeInput(Variant::CompactResidence, 700, 700, 150),
                                      HomeInput(Variant::SharedEntry, 900, 800, 300)})
    {
        const InteriorLayout a = GenerateInteriorLayout(base);
        const InteriorLayout again = GenerateInteriorLayout(base);
        Expect(a.Ok(), "base layout builds");
        Expect(DescribeLayout(a) == DescribeLayout(again), "output is deterministic");
        InteriorInput mirrored = base;
        mirrored.MirrorU = true;
        mirrored.EntranceCentreU = base.WidthCm - base.EntranceCentreU;
        const InteriorLayout m = GenerateInteriorLayout(mirrored);
        ExpectContracts(m, "mirrored layout");
        Expect(m.Mirrored && !a.Mirrored, "mirror flag recorded");
        InteriorLayout reflected = a;
        ReflectLayoutU(reflected);
        reflected.Mirrored = true;
        Expect(DescribeLayout(reflected) == DescribeLayout(m), "a mirrored layout is the reflection of the unmirrored one");
        Expect(AllIds(a) == AllIds(m), "mirroring keeps every id");
        // Heights and the vertical datum never depend on the reflection.
        Expect(m.ClearHeightCm == a.ClearHeightCm && m.Rooms.size() == a.Rooms.size(), "mirroring changes no counts or heights");
    }
}

void FloorDatumAndStorey()
{
    InteriorInput low = ShopInput(700, 700, 250);
    low.FinishedFloorCm = 0;
    InteriorInput high = low;
    high.FinishedFloorCm = 15;
    const InteriorLayout a = GenerateInteriorLayout(low);
    const InteriorLayout b = GenerateInteriorLayout(high);
    Expect(a.Ok() && b.Ok() && a.FinishedFloorCm == 0 && b.FinishedFloorCm == 15, "datum is echoed");
    InteriorLayout same = b;
    same.FinishedFloorCm = 0;
    Expect(DescribeLayout(a) == DescribeLayout(same), "geometry is floor-relative: the datum changes nothing else");
    InteriorInput tall = low;
    tall.StoreyHeightCm = 360;
    const InteriorLayout t = GenerateInteriorLayout(tall);
    Expect(t.Ok() && t.ClearHeightCm == 334, "an older 360 storey gives 334 clear");
    for (const WallSegment& s : t.Walls) Expect(s.W1 <= 334, "walls follow the storey height");
    Expect(t.Lights[0].HeightCm == 334 - 17, "light fixtures hang from the ceiling");
}

void StableIds()
{
    const InteriorLayout l = GenerateInteriorLayout(ShopInput(700, 700, 250));
    const std::set<std::string> ids = AllIds(l);
    for (const char* id : {"building.test.shop.room.sales", "building.test.shop.room.stock", "building.test.shop.opening.entrance",
                           "building.test.shop.opening.internal", "building.test.shop.piece.counter", "building.test.shop.piece.rack_stock",
                           "building.test.shop.piece.desk_packing", "building.test.shop.anchor.staff_counter",
                           "building.test.shop.connection.entrance", "building.test.shop.wall.front.0", "building.test.shop.light.sales.0"})
        Expect(ids.count(id) == 1, "documented ids exist");
    // Changing size, storey, datum or entrance position keeps the ids that name roles.
    InteriorInput other = ShopInput(900, 800, 150);
    other.FinishedFloorCm = 30;
    const InteriorLayout o = GenerateInteriorLayout(other);
    Expect(o.Ok() && o.Anchors.size() == l.Anchors.size(), "same anchors on another footprint");
    for (size_t i = 0; i < l.Anchors.size(); ++i) Expect(l.Anchors[i].Id == o.Anchors[i].Id, "anchor ids are footprint independent");
    // Two buildings never share an id.
    InteriorInput second = ShopInput(700, 700, 250);
    second.BuildingId = "building.test.shop2";
    const std::set<std::string> ids2 = AllIds(GenerateInteriorLayout(second));
    for (const std::string& id : ids2) Expect(ids.count(id) == 0, "ids are prefixed by the building id");
}

void InvalidInputs()
{
    const auto code = [](InteriorInput in) { return GenerateInteriorLayout(in).Status; };
    Expect(code(ShopInput(380, 700, 150)) == Refusal::WidthTooNarrow, "shop too narrow");
    Expect(code(ShopInput(700, 600, 250)) == Refusal::DepthTooShallow, "shop too shallow");
    Expect(code(ShopInput(3000, 700, 250)) == Refusal::FootprintTooLarge, "footprint too large");
    Expect(code(HomeInput(Variant::CompactResidence, 300, 700, 150)) == Refusal::WidthTooNarrow, "home too narrow");
    Expect(code(HomeInput(Variant::SharedEntry, 420, 700, 200)) == Refusal::WidthTooNarrow, "shared entry too narrow");
    Expect(code(ShopInput(700, 700, 40)) == Refusal::EntranceOffFacade, "entrance too close to the corner");
    Expect(code(ShopInput(700, 700, 672)) == Refusal::EntranceOffFacade, "entrance across the far corner");
    Expect(code(ShopInput(700, 700, -50)) == Refusal::EntranceOffFacade, "entrance left of the facade");
    Expect(code(ShopInput(700, 700, 900)) == Refusal::EntranceOffFacade, "entrance right of the facade");
    InteriorInput in = HomeInput(Variant::CompactResidence, 700, 700, 350);
    in.InternalDoorCentreU = 40;
    Expect(code(in) == Refusal::InternalDoorOffWall, "internal doorway through the corner");
    in.InternalDoorCentreU = 690;
    Expect(code(in) == Refusal::InternalDoorOffWall, "internal doorway off the partition");
    in.InternalDoorCentreU = 200;
    Expect(code(in) == Refusal::None, "an explicit internal doorway is honoured");
    const InteriorLayout custom = GenerateInteriorLayout(in);
    bool found = false;
    for (const Opening& o : custom.Openings) found = found || (o.Id == "building.test.home.opening.internal" && o.Plan.U0 == 153 && o.Plan.U1 == 247);
    Expect(found, "the internal doorway sits where it was asked");
    InteriorInput bad = ShopInput(700, 700, 250);
    bad.FinishedFloorCm = -1;
    Expect(code(bad) == Refusal::FloorDatumInvalid, "negative datum");
    bad.FinishedFloorCm = 15;
    bad.StoreyHeightCm = 250;
    Expect(code(bad) == Refusal::StoreyHeightInvalid, "storey too low for a 210 door");
    bad.StoreyHeightCm = 300;
    bad.BuildingId.clear();
    Expect(code(bad) == Refusal::InvalidBuildingId, "empty building id");
    bad = ShopInput(700, 700, 250);
    bad.Furniture = FurnitureSet::Residence;
    Expect(code(bad) == Refusal::FurnitureSetMismatch, "residence set in a shop");
    InteriorInput home = HomeInput(Variant::CompactResidence, 700, 700, 350);
    home.Furniture = FurnitureSet::Stationery;
    Expect(code(home) == Refusal::FurnitureSetMismatch, "stationery set in a home");
    // A refusal carries no geometry and never repairs the input.
    const InteriorLayout refused = GenerateInteriorLayout(ShopInput(380, 700, 150));
    Expect(!refused.Ok() && refused.Rooms.empty() && refused.Pieces.empty() && RefusalCode(refused.Status) == "width_too_narrow", "refusal is empty with a code");
    // A shared-entry building whose passage leaves no unit wide enough.
    Expect(code(HomeInput(Variant::SharedEntry, 440, 700, 220)) == Refusal::NoRoomForUnits, "passage with no room for a dwelling");
    // A bare shell (no furniture set) is allowed and still valid.
    InteriorInput bare = ShopInput(700, 700, 250);
    bare.Furniture = FurnitureSet::None;
    const InteriorLayout shell = GenerateInteriorLayout(bare);
    Expect(shell.Ok() && shell.Pieces.empty() && shell.Anchors.empty() && shell.Routes.empty(), "bare shell has no furniture or anchors");
}

void DisconnectedAndBrokenLayouts()
{
    InteriorInput goodIn = HomeInput(Variant::CompactResidence, 700, 700, 350);
    goodIn.EntranceDoorId = "door.test.home";
    const InteriorLayout good = GenerateInteriorLayout(goodIn);
    ExpectContracts(good, "residence for the broken-layout tests");
    Expect(std::any_of(good.Zones.begin(), good.Zones.end(), [](const Zone& z) { return z.Kind == ZoneKind::DoorSweep; }), "a hung leaf has a sweep zone");
    const InteriorLayout open = GenerateInteriorLayout(HomeInput(Variant::CompactResidence, 700, 700, 350));
    Expect(!std::any_of(open.Zones.begin(), open.Zones.end(), [](const Zone& z) { return z.Kind == ZoneKind::DoorSweep; }),
           "an open doorway has no sweep");
    InteriorLayout cut = good;
    cut.Connections.erase(std::remove_if(cut.Connections.begin(), cut.Connections.end(), [](const Connection& c) { return c.RoomA != "outside"; }), cut.Connections.end());
    Expect(ValidateInteriorLayout(cut) == Refusal::RoomsDisconnected, "back room cut off from the front room");
    InteriorLayout noEntrance = good;
    noEntrance.Connections.erase(noEntrance.Connections.begin());
    Expect(ValidateInteriorLayout(noEntrance) == Refusal::RoomsDisconnected, "no way in from outside");
    InteriorLayout dangling = good;
    dangling.Connections[1].OpeningId = "no.such.opening";
    Expect(ValidateInteriorLayout(dangling) == Refusal::RoomsDisconnected, "connection without an opening");
    const InteriorLayout shared = GenerateInteriorLayout(HomeInput(Variant::SharedEntry, 900, 800, 300));
    ExpectContracts(shared, "shared entry for the broken-layout tests");
    Expect(std::count_if(shared.Rooms.begin(), shared.Rooms.end(), [](const Room& r) { return r.Kind == RoomKind::Dwelling; }) == 2 ||
           std::count_if(shared.Rooms.begin(), shared.Rooms.end(), [](const Room& r) { return r.Kind == RoomKind::Dwelling; }) == 1, "dwellings off the passage");
    InteriorLayout orphan = shared;
    orphan.Connections.pop_back();
    Expect(ValidateInteriorLayout(orphan) == Refusal::RoomsDisconnected, "a dwelling with no door to the passage");

    InteriorLayout small = good;
    for (Opening& o : small.Openings) if (o.Kind != OpeningKind::Window) { o.ClearWidthCm = 80; break; }
    Expect(ValidateInteriorLayout(small) == Refusal::DoorwayTooSmall, "80 cm doorway");
    InteriorLayout low = good;
    for (Opening& o : low.Openings) if (o.Kind != OpeningKind::Window) { o.W1 = 190; break; }
    Expect(ValidateInteriorLayout(low) == Refusal::DoorwayTooSmall, "190 cm doorway");

    InteriorLayout blocked = good;
    Furniture box = blocked.Pieces.front();
    box.Id = "intruder";
    for (const Zone& z : blocked.Zones)
        if (z.Id.find("back_approach") != std::string::npos)
        {
            box.Footprint = Rect{z.Area.U0 + 10, z.Area.U0 + 60, z.Area.V0 + 10, z.Area.V0 + 60};
            box.RoomId = z.RoomId;
        }
    blocked.Pieces.push_back(box);
    Expect(ValidateInteriorLayout(blocked) == Refusal::CirculationBlocked, "furniture in a circulation corridor");

    InteriorLayout swing = good;
    Furniture inSweep = good.Pieces.front();
    inSweep.Id = "swing_intruder";
    for (const Zone& z : swing.Zones) if (z.Kind == ZoneKind::DoorSweep) { inSweep.Footprint = Rect{z.Area.U0 + 20, z.Area.U0 + 60, z.Area.V0 + 20, z.Area.V0 + 60}; inSweep.RoomId = z.RoomId; break; }
    swing.Pieces.push_back(inSweep);
    Expect(ValidateInteriorLayout(swing) == Refusal::SweepBlocked, "furniture in the door swing");

    InteriorLayout overlap = good;
    Furniture twin = overlap.Pieces.front();
    twin.Id = "twin";
    overlap.Pieces.push_back(twin);
    Expect(ValidateInteriorLayout(overlap) == Refusal::FurnitureOverlap, "two solids in one place");
    InteriorLayout dup = good;
    dup.Pieces.push_back(dup.Pieces.front());
    Expect(ValidateInteriorLayout(dup) == Refusal::DuplicateId, "duplicate id");
    InteriorLayout outside = good;
    outside.Pieces.front().Footprint = Rect{-100, -50, 0, 50};
    Expect(ValidateInteriorLayout(outside) == Refusal::FurnitureOutsideRoom, "furniture outside its room");
    InteriorLayout badAnchor = good;
    for (Anchor& a : badAnchor.Anchors) if (a.Kind == AnchorKind::Stand) { a.U = 1; a.V = 1; break; }
    Expect(ValidateInteriorLayout(badAnchor) == Refusal::AnchorInvalid, "stand point inside a wall");
    InteriorLayout badSeat = good;
    for (Anchor& a : badSeat.Anchors) if (a.Kind == AnchorKind::Seat) { a.U += 200; break; }
    Expect(ValidateInteriorLayout(badSeat) != Refusal::None, "seat point off its seat");

    const InteriorLayout shop = GenerateInteriorLayout(ShopInput(700, 700, 250));
    InteriorLayout onRoute = shop;
    Furniture block = shop.Pieces.front();
    block.Id = "route_intruder";
    const Route& route = shop.Routes.front();
    block.Footprint = Rect{route.Points[1].U - 10, route.Points[1].U + 10, route.Points[1].V - 10, route.Points[1].V + 10};
    block.RoomId = shop.Rooms.front().Id;
    onRoute.Pieces.push_back(block);
    Expect(ValidateInteriorLayout(onRoute) != Refusal::None, "furniture on a staff route");
}

/** Every combination either builds a layout that satisfies all contracts, or is refused with a specific code. */
void SweepFootprints()
{
    int built = 0;
    int refused = 0;
    std::map<std::string, int> reasons;
    for (const Variant kind : {Variant::ShopWithStockroom, Variant::CompactResidence, Variant::SharedEntry})
    {
        for (int width = 400; width <= 1300; width += 50)
        {
            for (int depth = 500; depth <= 1100; depth += 50)
            {
                for (const int fraction : {12, 25, 38, 50, 62, 75, 88})
                {
                    const int entrance = width * fraction / 100;
                    for (const bool mirror : {false, true})
                    {
                        InteriorInput in = kind == Variant::ShopWithStockroom ? ShopInput(width, depth, entrance) : HomeInput(kind, width, depth, entrance);
                        in.MirrorU = mirror;
                        const InteriorLayout l = GenerateInteriorLayout(in);
                        if (l.Ok())
                        {
                            ++built;
                            ExpectContracts(l, "sweep layout");
                        }
                        else
                        {
                            ++refused;
                            if (l.Status != Refusal::WidthTooNarrow && l.Status != Refusal::DepthTooShallow && l.Status != Refusal::FurnitureDoesNotFit &&
                                l.Status != Refusal::EntranceOffFacade && l.Status != Refusal::NoRoomForUnits && l.Status != Refusal::FootprintTooLarge)
                                std::cerr << "unexpected " << RefusalCode(l.Status) << " kind=" << static_cast<int>(kind) << " " << width << 'x' << depth
                                          << " c=" << entrance << " mirror=" << mirror << '\n';
                            ++reasons[std::string(RefusalCode(l.Status))];
                            Expect(l.Status != Refusal::CirculationBlocked && l.Status != Refusal::SweepBlocked && l.Status != Refusal::ZoneBlocked &&
                                       l.Status != Refusal::RoomsDisconnected && l.Status != Refusal::DuplicateId && l.Status != Refusal::WallInvalid &&
                                       l.Status != Refusal::AnchorInvalid && l.Status != Refusal::FurnitureOverlap && l.Status != Refusal::RouteBlocked,
                                   "the generator never emits a layout its own validator refuses");
                        }
                    }
                }
            }
        }
    }
    for (const auto& r : reasons) std::cerr << "refused " << r.first << " x" << r.second << '\n';
    std::cerr << "sweep: " << built << " built, " << refused << " refused\n";
    Expect(built > 1500, "most plausible footprints build");
}
} // namespace

int main(int argc, char** argv)
{
    if (argc > 1) // diagnostic: dump the stationery reference layout (width depth entrance) and stop
    {
        const int w = argc > 2 ? std::atoi(argv[2]) : 700;
        const int d = argc > 3 ? std::atoi(argv[3]) : 700;
        const int c = argc > 4 ? std::atoi(argv[4]) : 250;
        std::cout << DescribeLayout(GenerateInteriorLayout(ShopInput(w, d, c)));
        return 0;
    }
    ShopReference();
    NarrowAndWide();
    MirroredAndDeterministic();
    FloorDatumAndStorey();
    StableIds();
    InvalidInputs();
    DisconnectedAndBrokenLayouts();
    SweepFootprints();
    std::cout << "LaneInteriorLayoutTests: " << Assertions << " assertions passed\n";
    return 0;
}
