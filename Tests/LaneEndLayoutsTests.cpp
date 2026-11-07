// R6-05: the two masses that close the lane, opened as a furnished community hall (west) and a covered market hall (east).
// Covers the validated layouts (walls, doorless archway, windows, furniture, anchors, zones, lights, routes), the end-hall contracts
// (one archway on the lane-facing wall inside the exposed 800 cm, no door id or sweep, 120 cm aisles proven by a 60 cm body, every anchor
// reachable), the posted hours, the world frames of both masses, determinism, stable ids and that the validator refuses broken layouts.

#include "LaneEndLayouts.h"

#include "NeighbourhoodPlan.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>

namespace
{
namespace Le = PerfectWorld::LaneEnds;
namespace Li = PerfectWorld::LaneInterior;

int LtAssertions = 0;
void LtExpect(bool ok, const std::string& message)
{
    ++LtAssertions;
    if (ok)
    {
        return;
    }
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

std::uint64_t LtHash(const std::string& text)
{
    std::uint64_t h = 1469598103934665603ull;
    for (const unsigned char c : text)
    {
        h = (h ^ c) * 1099511628211ull;
    }
    return h;
}

constexpr Le::EndHall LtHalls[] = {Le::EndHall::CommunityHall, Le::EndHall::MarketHall};

std::string LtId(Le::EndHall hall, const std::string& tail)
{
    return std::string(Le::GetHallInfo(hall).BuildingId) + "." + tail;
}

void LtTestInfo()
{
    const Le::HallInfo& west = Le::GetHallInfo(Le::EndHall::CommunityHall);
    const Le::HallInfo& east = Le::GetHallInfo(Le::EndHall::MarketHall);
    LtExpect(west.BuildingId == "building.nabapara.west_end_block" && west.Label == "Community hall" && !west.East && west.Storeys == 4, "west mass is the community hall");
    LtExpect(east.BuildingId == "building.nabapara.east_end_block" && east.Label == "Market hall" && east.East && east.Storeys == 5, "east mass is the market hall");
    LtExpect(Le::FindHallInfo("building.nabapara.west_end_block") == &west && Le::FindHallInfo("building.nabapara.east_end_block") == &east &&
                 Le::FindHallInfo("building.nabapara.stationery") == nullptr,
             "halls are found by building id");
    LtExpect(west.LabelKey == "map.building.community_hall" && east.LabelKey == "map.building.market_hall", "label keys follow the map's scheme");
    for (const Le::EndHall hall : LtHalls)
    {
        const Le::HallInfo& info = Le::GetHallInfo(hall);
        LtExpect(info.EntranceCentreU == 1300 && info.EntranceWidthCm >= Le::MinArchwayWidthCm && info.EntranceHeightCm >= Li::DoorClearHeightCm + 2,
                 "the archway is centred on the lane axis, wide and high enough");
        LtExpect(info.EntranceId == std::string(info.BuildingId) + ".opening.entrance", "the archway's stable opening id");
    }
}

void LtTestFrames()
{
    // U runs +Y from Y -1300 in both masses; V runs from the lane-facing wall away from the lane.
    LtExpect(Le::HallWorldPoint(Le::EndHall::CommunityHall, 0, 0) == Le::WorldPoint{-2400, -1300}, "west: origin on the lane-facing wall at Y -1300");
    LtExpect(Le::HallWorldPoint(Le::EndHall::CommunityHall, 2600, 900) == Le::WorldPoint{-3300, 1300}, "west: V runs toward -X to the back wall");
    LtExpect(Le::HallWorldPoint(Le::EndHall::MarketHall, 0, 0) == Le::WorldPoint{6400, -1300}, "east: origin on the lane-facing wall at Y -1300");
    LtExpect(Le::HallWorldPoint(Le::EndHall::MarketHall, 2600, 900) == Le::WorldPoint{7300, 1300}, "east: V runs toward +X to the back wall");
    LtExpect(Le::HallEntrancePoint(Le::EndHall::CommunityHall) == Le::WorldPoint{-2400, 0} && Le::HallEntrancePoint(Le::EndHall::MarketHall) == Le::WorldPoint{6400, 0},
             "both archways open straight onto the lane's axis");

    // The neighbourhood plan's footprints for the two masses are the halls' footprints (nothing outside X -3300..-2400 and 6400..7300).
    const auto plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    for (const Le::EndHall hall : LtHalls)
    {
        const Le::WorldPoint a = Le::HallWorldPoint(hall, 0, 0);
        const Le::WorldPoint b = Le::HallWorldPoint(hall, Le::HallWidthCm, Le::HallDepthCm);
        const auto* feature = PerfectWorld::FindNeighbourhoodFeature(plan, Le::GetHallInfo(hall).BuildingId);
        LtExpect(feature != nullptr, "the plan knows the mass");
        if (feature == nullptr)
        {
            continue;
        }
        LtExpect(feature->Bounds.X0 == std::min(a.X, b.X) && feature->Bounds.X1 == std::max(a.X, b.X) && feature->Bounds.Y0 == std::min(a.Y, b.Y) &&
                     feature->Bounds.Y1 == std::max(a.Y, b.Y),
                 "the hall fills the plan's footprint of its mass exactly");
    }
}

void LtTestHours()
{
    using Le::EndHall;
    LtExpect(Le::PostedHoursText(EndHall::CommunityHall) == "08:00-21:00", "community hall hours text");
    LtExpect(Le::PostedHoursText(EndHall::MarketHall) == "06:00-13:00 and 16:00-21:00", "market hall hours text");
    for (const EndHall hall : LtHalls)
    {
        const auto windows = Le::HallHours(hall);
        LtExpect(!windows.empty() && windows.size() <= 2, "one or two windows");
        for (std::size_t i = 0; i < windows.size(); ++i)
        {
            LtExpect(windows[i].OpenMinute >= 0 && windows[i].OpenMinute < windows[i].CloseMinute && windows[i].CloseMinute <= 1440, "a window is inside the day");
            LtExpect(i == 0 || windows[i - 1].CloseMinute < windows[i].OpenMinute, "windows are ordered and apart");
        }
    }
    LtExpect(!Le::IsOpenAt(EndHall::CommunityHall, 479) && Le::IsOpenAt(EndHall::CommunityHall, 480) && Le::IsOpenAt(EndHall::CommunityHall, 1259) &&
                 !Le::IsOpenAt(EndHall::CommunityHall, 1260),
             "community hall opens at 08:00 and closes at 21:00");
    LtExpect(Le::MinutesUntilOpening(EndHall::CommunityHall, 479) == 1 && Le::MinutesUntilOpening(EndHall::CommunityHall, 1260) == 660 &&
                 Le::MinutesUntilOpening(EndHall::CommunityHall, 600) == -1,
             "community hall waits: one minute, tomorrow morning, none while open");
    LtExpect(!Le::IsOpenAt(EndHall::MarketHall, 359) && Le::IsOpenAt(EndHall::MarketHall, 360) && Le::IsOpenAt(EndHall::MarketHall, 779) &&
                 !Le::IsOpenAt(EndHall::MarketHall, 780) && !Le::IsOpenAt(EndHall::MarketHall, 959) && Le::IsOpenAt(EndHall::MarketHall, 960) &&
                 Le::IsOpenAt(EndHall::MarketHall, 1259) && !Le::IsOpenAt(EndHall::MarketHall, 1260),
             "market hall has a morning and an evening session");
    LtExpect(Le::MinutesUntilOpening(EndHall::MarketHall, 780) == 180 && Le::MinutesUntilOpening(EndHall::MarketHall, 1260) == 540 &&
                 Le::MinutesUntilOpening(EndHall::MarketHall, 959) == 1,
             "market hall waits to the next session, across midnight");
    LtExpect(!Le::IsOpenAt(EndHall::CommunityHall, -5) && Le::MinutesUntilOpening(EndHall::CommunityHall, 1440) == 480, "an invalid minute is read as midnight");
}

void LtTestLayout(Le::EndHall hall)
{
    const Le::HallInfo& info = Le::GetHallInfo(hall);
    const Li::InteriorLayout l = Le::GenerateEndLayout(hall);
    LtExpect(l.Ok() && l.BuildingId == info.BuildingId, "the layout is valid: " + std::string(Li::RefusalCode(l.Status)));
    LtExpect(Le::ValidateEndLayout(hall, l) == Li::Refusal::None && Li::ValidateInteriorLayout(l) == Li::Refusal::None, "both validators pass");
    LtExpect(l.WidthCm == 2600 && l.DepthCm == 900 && l.FinishedFloorCm == 15 && l.StoreyHeightCm == 300 && l.ClearHeightCm == 274, "footprint, datum and heights");
    LtExpect(l.Interior == Li::Rect{25, 2575, 25, 875} && l.Rooms.size() == 1 && l.Rooms[0].Area == l.Interior, "one open room fills the interior");

    // Entrance: one doorless archway on the lane-facing wall, inside the exposed |Y| < 400, no door id and no sweep anywhere.
    const Li::Opening* entrance = Le::FindEndEntrance(l);
    LtExpect(entrance != nullptr && entrance->Id == info.EntranceId, "the archway has its stable id");
    if (entrance == nullptr)
    {
        return;
    }
    LtExpect(entrance->Kind == Li::OpeningKind::Doorway && entrance->DoorId.empty() && entrance->RoomA == "outside", "the archway is a doorway with no leaf");
    LtExpect(entrance->Plan.V0 == 0 && entrance->Plan.V1 == 25 && entrance->Plan.U0 >= 900 && entrance->Plan.U1 <= 1700, "on the lane-facing wall inside the exposed 800 cm");
    LtExpect(entrance->Plan.Width() == info.EntranceWidthCm && entrance->W0 == 0 && entrance->W1 == info.EntranceHeightCm && entrance->ClearWidthCm >= Li::DoorClearWidthCm * 2,
             "archway size");
    LtExpect((entrance->Plan.U0 + entrance->Plan.U1) / 2 == 1300, "centred on the lane axis");
    for (const Li::Opening& o : l.Openings)
    {
        LtExpect(o.DoorId.empty() && o.Kind != Li::OpeningKind::Door, "no opening hangs a door");
    }
    for (const Li::Zone& z : l.Zones)
    {
        LtExpect(z.Kind != Li::ZoneKind::DoorSweep, "no door sweep exists");
    }
    const auto connection = std::find_if(l.Connections.begin(), l.Connections.end(), [&](const Li::Connection& c) { return c.OpeningId == info.EntranceId; });
    LtExpect(connection != l.Connections.end() && connection->RoomA == "outside" && connection->ClearWidthCm == info.EntranceWidthCm, "outside connects to the room through the archway");

    // The wall at the archway: a lintel above it and full walls either side.
    int lintels = 0;
    for (const Li::WallSegment& w : l.Walls)
    {
        if (w.Plan.Overlaps(entrance->Plan))
        {
            LtExpect(w.W0 >= info.EntranceHeightCm && w.W1 == l.ClearHeightCm, "only a lintel stands over the archway");
            ++lintels;
        }
    }
    LtExpect(lintels == 1, "one lintel");

    // Every wall and piece lies inside the mass's world footprint; every anchor, light and route point does too.
    const auto inside = [&](int u, int v) {
        const Le::WorldPoint p = Le::HallWorldPoint(hall, u, v);
        const int x0 = std::min(info.LaneFaceXCm, info.LaneFaceXCm + info.FaceXSign * 900);
        const int x1 = std::max(info.LaneFaceXCm, info.LaneFaceXCm + info.FaceXSign * 900);
        return p.X >= x0 && p.X <= x1 && p.Y >= -1300 && p.Y <= 1300;
    };
    for (const Li::WallSegment& w : l.Walls) LtExpect(inside(w.Plan.U0, w.Plan.V0) && inside(w.Plan.U1, w.Plan.V1), "wall inside the mass: " + w.Id);
    for (const Li::Furniture& f : l.Pieces) LtExpect(inside(f.Footprint.U0, f.Footprint.V0) && inside(f.Footprint.U1, f.Footprint.V1), "piece inside the mass: " + f.Id);
    for (const Li::LightSpec& s : l.Lights) LtExpect(inside(s.U, s.V) && s.HeightCm < l.ClearHeightCm && s.RadiusCm <= 300, "light inside and below the ceiling: " + s.Id);

    // Ids are unique, prefixed with the building and carry their kind.
    std::set<std::string> ids;
    const auto add = [&](const std::string& id, const char* kind) {
        LtExpect(id.starts_with(std::string(info.BuildingId) + "." + kind + "."), "id has its kind: " + id);
        LtExpect(ids.insert(id).second, "id is unique: " + id);
    };
    for (const auto& x : l.Rooms) add(x.Id, "room");
    for (const auto& x : l.Openings) add(x.Id, "opening");
    for (const auto& x : l.Walls) add(x.Id, "wall");
    for (const auto& x : l.Pieces) add(x.Id, "piece");
    for (const auto& x : l.Anchors) add(x.Id, "anchor");
    for (const auto& x : l.Zones) add(x.Id, "zone");
    for (const auto& x : l.Lights) add(x.Id, "light");
    for (const auto& x : l.Routes) add(x.Id, "route");

    // Circulation: every aisle zone is at least 120 wide, the entry aisle spans the hall, the threshold and entry anchors exist.
    int aisles = 0;
    for (const Li::Zone& z : l.Zones)
    {
        if (z.Kind == Li::ZoneKind::Circulation)
        {
            LtExpect(std::min(z.Area.Width(), z.Area.Depth()) >= Li::CirculationCm, "aisle at least 120 wide: " + z.Id);
            ++aisles;
        }
    }
    LtExpect(aisles >= 3, "several aisles");
    LtExpect(Li::FindAnchor(l, LtId(hall, "anchor.threshold")) != nullptr && Li::FindAnchor(l, LtId(hall, "anchor.hall_entry")) != nullptr, "threshold and entry anchors");
    LtExpect(l.Routes.size() >= 2, "authored walking routes exist");
    for (const Li::Route& r : l.Routes)
    {
        LtExpect(r.LengthCm() > 0 && r.Points.front().V < 0 && r.Points.size() >= 2, "route starts outside the archway: " + r.Id);
    }

    // Determinism and a pinned shape: the dump of the layout is a pure function of the hall.
    LtExpect(Li::DescribeLayout(l) == Li::DescribeLayout(Le::GenerateEndLayout(hall)), "deterministic");
    LtExpect(l.Lights.size() == 6, "six ceiling lights");
}

void LtTestCommunity()
{
    const Le::EndHall hall = Le::EndHall::CommunityHall;
    const Li::InteriorLayout l = Le::GenerateEndLayout(hall);
    for (const char* id : {"piece.stage", "piece.notice_board", "piece.programme_board", "piece.carrom_table", "piece.carrom_board", "piece.committee_table", "piece.cupboard",
                           "piece.bench_r1_l1", "piece.bench_r3_r2", "anchor.notice_board", "anchor.stage_front", "anchor.seat_carrom_w", "anchor.seat_r2_l1",
                           "route.door_to_stage_front", "opening.entrance", "opening.window_back_3"})
    {
        const std::string full = LtId(hall, id);
        const bool known = Li::FindFurniture(l, full) || Li::FindAnchor(l, full) || Li::FindRoute(l, full) ||
                           std::any_of(l.Openings.begin(), l.Openings.end(), [&](const Li::Opening& o) { return o.Id == full; });
        LtExpect(known, "community hall has " + full);
    }
    int benches = 0;
    int seats = 0;
    for (const Li::Furniture& f : l.Pieces) benches += f.Id.find(".piece.bench_") != std::string::npos ? 1 : 0;
    for (const Li::Anchor& a : l.Anchors) seats += a.Kind == Li::AnchorKind::Seat ? 1 : 0;
    LtExpect(benches == 12 && seats == 12 + 2 + 2, "twelve benches, two carrom seats, two committee chairs");
    const Li::Furniture* stage = Li::FindFurniture(l, LtId(hall, "piece.stage"));
    LtExpect(stage != nullptr && stage->Solid && stage->H1 == 40 && stage->Footprint.Width() == 700 && stage->Footprint.V1 == 875,
             "a 40 cm stage across the middle of the back wall (a step a walker can take)");
    const Li::Anchor* front = Li::FindAnchor(l, LtId(hall, "anchor.stage_front"));
    LtExpect(front != nullptr && front->U == 1300 && front->FaceV == 1 && front->Kind == Li::AnchorKind::Stand, "the stage-front stand faces the stage on the axis");
    const Li::Furniture* board = Li::FindFurniture(l, LtId(hall, "piece.notice_board"));
    LtExpect(board != nullptr && board->Kind == Li::FurnitureKind::Display && board->H0 == 110 && board->Footprint.Depth() == 180 && board->Footprint.Width() <= 15,
             "a wall-hung notice board 180 cm wide");
    LtExpect(Li::DescribeLayout(l).size() > 3000, "the layout has real content");
}

void LtTestMarket()
{
    const Le::EndHall hall = Le::EndHall::MarketHall;
    const Li::InteriorLayout l = Le::GenerateEndLayout(hall);
    int tables = 0;
    int counters = 0;
    int browse = 0;
    int heaps = 0;
    for (const Li::Furniture& f : l.Pieces)
    {
        tables += f.Id.find(".piece.produce_table_") != std::string::npos ? 1 : 0;
        counters += f.Kind == Li::FurnitureKind::Counter ? 1 : 0;
        heaps += f.Id.find(".piece.goods_") != std::string::npos ? 1 : 0;
    }
    for (const Li::Anchor& a : l.Anchors) browse += a.Id.find(".anchor.browse_") != std::string::npos ? 1 : 0;
    LtExpect(tables == 12 && counters == 4 && heaps == 36 && browse == 12, "four bays of three produce tables, four wall counters, three heaps a table, a browse place a table");
    for (const Li::Furniture& f : l.Pieces)
    {
        if (f.Id.find(".piece.produce_table_") != std::string::npos)
        {
            LtExpect(f.Solid && f.H1 == 75 && f.Footprint.Width() == 420 && f.Footprint.Depth() == 80, "a 420 x 80 produce table, 75 high: " + f.Id);
        }
        if (f.Id.find(".piece.goods_") != std::string::npos || f.Id.find(".piece.awning_") != std::string::npos)
        {
            LtExpect(!f.Solid, "goods and awnings are visual only: " + f.Id);
        }
    }
    // Aisles are exactly 120 clear between table rows: the gap between consecutive rows of one bay.
    int aisles120 = 0;
    for (const Li::Zone& z : l.Zones)
    {
        aisles120 += (z.Kind == Li::ZoneKind::Circulation && std::min(z.Area.Width(), z.Area.Depth()) == 120) ? 1 : 0;
    }
    LtExpect(aisles120 >= 5, "five exact 120 cm aisles or more (entry, two row aisles, three cross aisles share them)");
    // Every stall can be walked up to from a 120 cm channel: reach the browse anchors of all rows.
    for (const char* id : {"anchor.browse_b0", "anchor.browse_b3_r3", "anchor.counter_b0", "anchor.counter_b3", "route.door_to_counter_b1", "route.door_to_browse_b3"})
    {
        const std::string full = LtId(hall, id);
        LtExpect(Li::FindAnchor(l, full) != nullptr || Li::FindRoute(l, full) != nullptr, "market hall has " + full);
    }
}

Li::InteriorLayout LtWithoutRoutes(Li::InteriorLayout l)
{
    l.Routes.clear();
    l.Zones.erase(std::remove_if(l.Zones.begin(), l.Zones.end(), [](const Li::Zone& z) { return z.Kind == Li::ZoneKind::Route; }), l.Zones.end());
    return l;
}

void LtTestRefusals()
{
    for (const Le::EndHall hall : LtHalls)
    {
        const Li::InteriorLayout good = Le::GenerateEndLayout(hall);
        const Le::HallInfo& info = Le::GetHallInfo(hall);

        // A door id on the archway is a roster change this order avoids.
        Li::InteriorLayout door = good;
        for (Li::Opening& o : door.Openings)
        {
            if (o.Id == info.EntranceId) o.DoorId = "door.nabapara.west_end_front";
        }
        LtExpect(Le::ValidateEndLayout(hall, door) == Li::Refusal::UnitDoorIdInvalid, "a hung door id is refused");

        // A layout for the wrong building, or the wrong size, is refused.
        Li::InteriorLayout other = good;
        other.BuildingId = "building.nabapara.stationery";
        LtExpect(Le::ValidateEndLayout(hall, other) != Li::Refusal::None, "another building's layout is refused");
        LtExpect(Le::ValidateEndLayout(hall == Le::EndHall::CommunityHall ? Le::EndHall::MarketHall : Le::EndHall::CommunityHall, good) != Li::Refusal::None,
                 "a hall's layout is not the other hall's");

        // A narrow archway is refused.
        Li::InteriorLayout narrow = good;
        for (Li::Opening& o : narrow.Openings)
        {
            if (o.Id == info.EntranceId) { o.Plan.U0 += 100; o.Plan.U1 -= 100; }
        }
        LtExpect(Le::ValidateEndLayout(hall, narrow) != Li::Refusal::None, "an archway squeezed narrower is refused");

        // A solid piece standing in an aisle is refused (the aisle zone is no longer free).
        Li::InteriorLayout blocked = good;
        Li::Furniture crate;
        crate.Id = std::string(info.BuildingId) + ".piece.crate_in_the_aisle";
        crate.RoomId = good.Rooms[0].Id;
        crate.Kind = Li::FurnitureKind::Carton;
        crate.Footprint = Li::Rect{1260, 1340, 60, 120};
        crate.H0 = 0;
        crate.H1 = 60;
        blocked.Pieces.push_back(crate);
        LtExpect(Le::ValidateEndLayout(hall, blocked) != Li::Refusal::None, "a crate in the entry aisle is refused");

        // Three shelves boxing in the notice board's standing place leave every authored zone free (they only touch the zone's edges) but
        // cut the anchor off from the archway: the reachability rule, not the zone rules, refuses it.
        if (hall == Le::EndHall::CommunityHall)
        {
            Li::InteriorLayout fenced = LtWithoutRoutes(good);
            const Li::Rect fences[] = {Li::Rect{117, 150, 290, 390}, Li::Rect{37, 150, 255, 290}, Li::Rect{37, 150, 390, 425}};
            int n = 0;
            for (const Li::Rect& r : fences)
            {
                Li::Furniture fence;
                fence.Id = std::string(info.BuildingId) + ".piece.fence_" + std::to_string(n++);
                fence.RoomId = good.Rooms[0].Id;
                fence.Kind = Li::FurnitureKind::Shelf;
                fence.Footprint = r;
                fence.H0 = 0;
                fence.H1 = 200;
                fenced.Pieces.push_back(fence);
            }
            LtExpect(Li::ValidateInteriorLayout(fenced) == Li::Refusal::None, "the fenced layout passes the generic rules");
            LtExpect(Le::ValidateEndLayout(hall, fenced) == Li::Refusal::CirculationBlocked, "an anchor cut off from the archway is refused");
        }
    }
}
} // namespace

int main()
{
    LtTestInfo();
    LtTestFrames();
    LtTestHours();
    for (const Le::EndHall hall : LtHalls)
    {
        LtTestLayout(hall);
    }
    LtTestCommunity();
    LtTestMarket();
    LtTestRefusals();

    // Pinned content: an unintended edit of either hall changes these. Update deliberately with the layout.
    const std::uint64_t west = LtHash(Li::DescribeLayout(Le::GenerateEndLayout(Le::EndHall::CommunityHall)));
    const std::uint64_t east = LtHash(Li::DescribeLayout(Le::GenerateEndLayout(Le::EndHall::MarketHall)));
    std::cout << "LaneEndLayouts hashes west=" << west << " east=" << east << '\n';
    std::cout << "LaneEndLayoutsTests: " << LtAssertions << " assertions passed\n";
    return 0;
}
