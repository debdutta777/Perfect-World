#include "LaneTallFlatsLayout.h"

#include "LaneFrontageCatalogue.h"

#include <algorithm>

namespace PerfectWorld::LaneTallFlats
{
namespace
{
namespace T6LI = LaneInterior;
using T6Rect = T6LI::Rect;

void T6Shift(T6LI::InteriorLayout& l, int u)
{
    const auto shift = [u](T6Rect& r) { r.U0 += u; r.U1 += u; };
    shift(l.Interior);
    for (auto& r : l.Rooms) shift(r.Area);
    for (auto& w : l.Walls) shift(w.Plan);
    for (auto& o : l.Openings) { shift(o.Plan); o.HingeU += u; }
    for (auto& p : l.Pieces) shift(p.Footprint);
    for (auto& z : l.Zones) shift(z.Area);
    for (auto& a : l.Anchors) a.U += u;
    for (auto& x : l.Lights) x.U += u;
    for (auto& r : l.Routes) for (auto& p : r.Points) p.U += u;
}

T6LI::InteriorLayout T6Dwelling()
{
    T6LI::InteriorInput in;
    in.BuildingId = "building.nabapara.tall_flats.upper_a";
    in.Kind = T6LI::Variant::CompactResidence;
    in.Furniture = T6LI::FurnitureSet::Residence;
    in.WidthCm = 485;
    in.DepthCm = 800;
    in.EntranceCentreU = 350;
    in.MirrorU = true; // Residence bed and dining set go to the east, away from the stair entrance.
    auto l = T6LI::GenerateInteriorLayout(in);
    if (!l.Ok()) return l;
    T6Shift(l, 615);
    l.WidthCm = 1100;
    l.Mirrored = false;
    l.FinishedFloorCm = 315; // the ground-floor kit's input rejects elevated datums; its output is floor-relative.

    // Replace the front entrance by a side doorway from the stair's top landing into the sleeping room.
    const auto found = std::find_if(l.Openings.begin(), l.Openings.end(), [](const auto& o) {
        return o.RoomA == "outside" && o.Kind != T6LI::OpeningKind::Window;
    });
    if (found == l.Openings.end()) { l.Status = T6LI::Refusal::RoomsDisconnected; return l; }
    auto& entrance = *found;
    const auto old = entrance;
    l.Walls.push_back({old.Id + ".infill", old.WallId, old.Plan, 0, old.W1});
    const auto* sleeping = T6LI::FindRoom(l, in.BuildingId + ".room.sleeping");
    if (sleeping == nullptr) { l.Status = T6LI::Refusal::RoomsDisconnected; return l; }
    const std::string sleepingId = sleeping->Id;
    entrance.WallId = in.BuildingId + ".wall.side_b"; // MirrorU reflects geometry, retaining ids.
    entrance.Plan = {615, 640, 668, 762};
    entrance.RoomB = sleepingId;
    entrance.HingeU = 627;
    entrance.HingeV = 668;
    entrance.SweepSign = 1;
    for (auto& c : l.Connections) if (c.OpeningId == entrance.Id) c.RoomB = sleepingId;

    // Split the west wall around the new 94 x 212 rough opening (90 x 210 clear).
    std::vector<T6LI::WallSegment> walls;
    for (const auto& w : l.Walls)
    {
        if (w.Plan.U0 != 615 || w.Plan.U1 != 640 || !w.Plan.Overlaps(entrance.Plan)) { walls.push_back(w); continue; }
        if (w.Plan.V0 < 668) walls.push_back({w.Id + ".near", w.WallId, {615, 640, w.Plan.V0, 668}, w.W0, w.W1});
        if (w.Plan.V1 > 762) walls.push_back({w.Id + ".far", w.WallId, {615, 640, 762, w.Plan.V1}, w.W0, w.W1});
        walls.push_back({w.Id + ".head", w.WallId, entrance.Plan, 212, w.W1});
    }
    l.Walls = std::move(walls);
    // The optional trunk would occupy the new entry approach. Required Residence furniture is retained.
    std::erase_if(l.Pieces, [](const auto& p) { return p.Kind == T6LI::FurnitureKind::Trunk; });
    std::erase_if(l.Zones, [](const auto& z) { return z.OwnerId.find("trunk") != std::string::npos; });
    std::erase_if(l.Anchors, [](const auto& a) { return a.FurnitureId.find("trunk") != std::string::npos; });
    l.Zones.push_back({in.BuildingId + ".zone.upper_entry", T6LI::ZoneKind::Circulation, sleepingId,
                       {640, 760, 655, 775}, entrance.Id});
    l.Zones.push_back({in.BuildingId + ".zone.sleeping_column", T6LI::ZoneKind::Circulation, sleepingId,
                       {655, 775, 442, 775}, entrance.Id});
    l.Zones.push_back({in.BuildingId + ".zone.sleeping_cross", T6LI::ZoneKind::Circulation, sleepingId,
                       {655, 1025, 442, 562}, entrance.Id});
    l.Status = T6LI::ValidateInteriorLayout(l);
    return l;
}

void T6Subtract(std::vector<T6Rect>& tiles, const T6Rect& hole)
{
    std::vector<T6Rect> next;
    for (const auto& r : tiles)
    {
        if (!r.Overlaps(hole)) { next.push_back(r); continue; }
        const T6Rect cut{std::max(r.U0, hole.U0), std::min(r.U1, hole.U1), std::max(r.V0, hole.V0), std::min(r.V1, hole.V1)};
        if (r.U0 < cut.U0) next.push_back({r.U0, cut.U0, r.V0, r.V1});
        if (cut.U1 < r.U1) next.push_back({cut.U1, r.U1, r.V0, r.V1});
        if (r.V0 < cut.V0) next.push_back({cut.U0, cut.U1, r.V0, cut.V0});
        if (cut.V1 < r.V1) next.push_back({cut.U0, cut.U1, cut.V1, r.V1});
    }
    tiles = std::move(next);
}
} // namespace

Layout GenerateLayout()
{
    Layout out;
    const auto* entry = T6LI::FindFrontageById("building.nabapara.tall_flats");
    if (entry == nullptr) { out.Ground.Status = T6LI::Refusal::InvalidBuildingId; return out; }
    out.Ground = T6LI::GenerateInteriorLayout(T6LI::MakeFrontageInput(*entry, 15, 300));
    LaneStair::StairInput stair;
    stair.StairId = "stair.nabapara.tall_flats.first";
    // 640 depth = 120 approach + 16*25 treads + 120 top landing.
    // Unit openings end at V182; the first riser is V255 (73 cm beyond them).
    stair.Footprint = {485, 615, 135, 775};
    stair.Scale.MinLandingCm = T6LI::CirculationCm;
    stair.Walls = {true, true, false, true};
    out.Stair = LaneStair::GenerateStairLayout(stair);
    out.Stair.Status = LaneStair::ValidateStairLayout(out.Stair);
    out.Upper = T6Dwelling();
    if (!out.Ok()) return out;
    out.Slab.push_back({0, 1100, 0, 800});
    for (const auto& o : out.Stair.Openings) T6Subtract(out.Slab, o.Plan);
    return out;
}

DoorDef PlannedUpperDoor() noexcept
{
    return {UpperDoorId, UpperLocationId, false, DoorAccess::Private, 0, 0, {}, false, 1, {}};
}
} // namespace PerfectWorld::LaneTallFlats
