#include "LaneTallFlatsLayout.h"

#include "LaneFrontageCatalogue.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace
{
namespace T6LI = PerfectWorld::LaneInterior;
namespace T6LS = PerfectWorld::LaneStair;
namespace T6TF = PerfectWorld::LaneTallFlats;
int T6Assertions = 0;
void T6Expect(bool ok, const char* message)
{
    ++T6Assertions;
    if (ok) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

bool T6BodyClearsSlab(const T6TF::Layout& l)
{
    const auto& s = l.Stair;
    const int radius = s.Scale.CapsuleRadiusCm;
    for (int u = 485 + radius; u <= 615 - radius; u += 3)
    {
        for (int v = 25 + radius; v <= 775 - radius; v += 3)
        {
            const T6LI::Rect body{u - radius, u + radius, v - radius, v + radius};
            int feet = 0;
            bool top = false;
            for (const auto& x : s.Steps) if (x.Plan.Overlaps(body)) feet = std::max(feet, x.TopMm);
            for (const auto& x : s.Landings) if (x.Kind == T6LS::LandingKind::Top && x.Plan.Overlaps(body)) top = true;
            if (top) continue;
            for (const auto& tile : l.Slab)
                if (tile.Overlaps(body) && s.SlabUndersideMm - feet < s.Scale.HeadroomCm * 10) return false;
        }
    }
    return true;
}

// Independent physical path through the landing, side door and internal door. The room graph alone
// cannot detect an opening moved away from its partition or a furnishing across the walking line.
bool T6UpperWalkClear(const T6TF::Layout& l)
{
    const std::vector<T6LI::RoutePoint> path{{550, 715}, {715, 715}, {715, 500}, {965, 500}, {965, 300}};
    const int radius = l.Stair.Scale.CapsuleRadiusCm;
    for (size_t i = 1; i < path.size(); ++i)
    {
        const auto a = path[i - 1];
        const auto b = path[i];
        const int length = std::abs(a.U - b.U) + std::abs(a.V - b.V);
        for (int k = 0; k <= length; ++k)
        {
            const int u = a.U + (b.U - a.U) * k / length;
            const int v = a.V + (b.V - a.V) * k / length;
            const T6LI::Rect body{u - radius, u + radius, v - radius, v + radius};
            for (const auto& w : l.Upper.Walls)
                if (w.W0 < l.Stair.Scale.BodyHeightCm && w.Plan.Overlaps(body)) return false;
            for (const auto& f : l.Upper.Pieces)
                if (f.Solid && f.H0 < l.Stair.Scale.BodyHeightCm && f.Footprint.Overlaps(body)) return false;
            for (const auto& g : l.Stair.Guards)
                if (g.TopMm > l.Stair.RiseMm && g.Plan.Overlaps(body)) return false;
            int supported = 0;
            for (const auto& tile : l.Slab)
                supported += std::max(0, std::min(tile.U1, body.U1) - std::max(tile.U0, body.U0)) *
                             std::max(0, std::min(tile.V1, body.V1) - std::max(tile.V0, body.V0));
            if (supported != body.Width() * body.Depth()) return false;
        }
    }
    return true;
}
} // namespace

int main()
{
    const auto l = T6TF::GenerateLayout();
    if (!l.Ok())
        std::cerr << "ground=" << T6LI::RefusalCode(l.Ground.Status) << " upper=" << T6LI::RefusalCode(l.Upper.Status)
                  << " stair=" << T6LS::RefusalCode(l.Stair.Status) << '\n';
    T6Expect(l.Ok(), "production layout fits");
    const auto again = T6TF::GenerateLayout();
    T6Expect(T6LI::DescribeLayout(l.Ground) == T6LI::DescribeLayout(again.Ground) &&
             T6LI::DescribeLayout(l.Upper) == T6LI::DescribeLayout(again.Upper) &&
             T6LS::DescribeStairLayout(l.Stair) == T6LS::DescribeStairLayout(again.Stair) && l.Slab == again.Slab, "deterministic complete geometry");
    const auto* entry = T6LI::FindFrontageById("building.nabapara.tall_flats");
    T6Expect(entry != nullptr, "catalogue row");
    const auto ground = T6LI::GenerateInteriorLayout(T6LI::MakeFrontageInput(*entry, 15, 300));
    T6Expect(T6LI::DescribeLayout(l.Ground) == T6LI::DescribeLayout(ground), "ground walls, units, doors, furniture and anchors unchanged");
    T6Expect(T6LI::ValidateInteriorLayout(l.Ground) == T6LI::Refusal::None, "ground circulation and both unit connections");
    T6Expect(T6LI::ValidateInteriorLayout(l.Upper) == T6LI::Refusal::None, "upper room graph, furniture, anchors and circulation");
    T6Expect(l.Upper.Rooms.size() == 2 && !l.Upper.Pieces.empty() && l.Upper.Lights.size() >= 2, "furnished two-room Residence with lights");
    T6Expect(T6UpperWalkClear(l), "both upper rooms physically reachable from the supported landing with a full capsule");
    auto obstructed = l;
    obstructed.Upper.Walls.push_back({"t6.block", "t6.partition", {640, 660, 668, 762}, 0, 210});
    T6Expect(!T6UpperWalkClear(obstructed), "physical sweep detects a blocked upper entrance");
    auto disconnected = l.Upper;
    disconnected.Connections.pop_back();
    T6Expect(T6LI::ValidateInteriorLayout(disconnected) == T6LI::Refusal::RoomsDisconnected, "broken upper room graph refused");
    T6Expect(l.Stair.RiseMm == 3000 && l.Stair.RiserCount == 17 && l.Stair.RiserMmMin == 176 && l.Stair.RiserMmMax == 177, "exact 15 to 315 floor rise");
    T6Expect(l.Stair.FlightWidthCm == 130, "130 clear flight");
    for (const auto& landing : l.Stair.Landings)
        T6Expect(landing.Plan.Width() >= 120 && landing.Plan.Depth() >= 120, "both landings have 120 circulation");
    T6Expect(l.Stair.Steps.front().Plan.V0 == 255, "first riser behind ground door approaches");
    for (const auto& o : l.Ground.Openings)
        if (o.Kind == T6LI::OpeningKind::Door && o.RoomA != "outside")
            T6Expect(o.Plan.V1 + 2 * l.Stair.Scale.CapsuleRadiusCm <= l.Stair.Steps.front().Plan.V0, "full body reach beyond both unit doorways before riser");
    for (int u = 400; u <= 710; ++u)
    {
        const T6LI::Rect body{u - 34, u + 34, 135 - 34, 135 + 34};
        for (const auto& step : l.Stair.Steps) T6Expect(!body.Overlaps(step.Plan), "unit A to B ground approach misses stair mass");
        for (const auto& wall : l.Ground.Walls)
            if (wall.W0 < 165) T6Expect(!body.Overlaps(wall.Plan), "both ground doorways physically traversable");
    }
    T6Expect(T6BodyClearsSlab(l), "34 cm body sweep clears the actual slab tiles by 220 cm");
    auto broken = l;
    broken.Slab.push_back(l.Stair.Openings.front().Plan);
    T6Expect(!T6BodyClearsSlab(broken), "sweep rejects a filled opening");
    T6Expect(T6LS::ValidateStairLayout(l.Stair) == T6LS::Refusal::None, "headroom volumes and rim guards");
    T6Expect(l.Stair.Openings.front().Plan == T6LI::Rect{485, 615, 237, 655}, "actual slab hole includes 68 cm backwards body reach");
    auto unguarded = l.Stair;
    unguarded.Guards.clear();
    T6Expect(T6LS::ValidateStairLayout(unguarded) == T6LS::Refusal::GuardMissing, "missing guard refused");
    for (const auto& tile : l.Slab)
        for (const auto& hole : l.Stair.Openings) T6Expect(!tile.Overlaps(hole.Plan), "slab excludes opening");
    int area = 0;
    for (const auto& tile : l.Slab) area += tile.Width() * tile.Depth();
    for (const auto& hole : l.Stair.Openings) area += hole.Plan.Width() * hole.Plan.Depth();
    T6Expect(area == 1100 * 800, "slab and opening cover footprint exactly");
    for (const auto& z : l.Upper.Zones)
        if (z.Kind == T6LI::ZoneKind::Circulation) T6Expect(std::min(z.Area.Width(), z.Area.Depth()) >= 120, "upper circulation 120 minimum");
    for (const auto& o : l.Upper.Openings)
        if (o.Kind != T6LI::OpeningKind::Window)
            T6Expect(o.ClearWidthCm >= 90 && o.W1 - o.W0 >= 210 && o.DoorId.empty(), "90 by 210 clear doorways remain unhung");
    T6LS::StairInput in;
    in.StairId = "stair.t6.limit";
    in.Footprint = {485, 615, 176, 775};
    T6Expect(T6LS::GenerateStairLayout(in).Status == T6LS::Refusal::FootprintTooShort, "599 cm straight refused");
    in.Kind = T6LS::Arrangement::Switchback;
    in.Footprint = {485, 615, 155, 775};
    T6Expect(T6LS::GenerateStairLayout(in).Status == T6LS::Refusal::FootprintTooNarrow, "130 cm passage refuses switchback");
    in.Kind = T6LS::Arrangement::Straight;
    in.Scale.MinLandingCm = 120;
    in.Footprint = {485, 615, 136, 775};
    T6Expect(T6LS::GenerateStairLayout(in).Status == T6LS::Refusal::FootprintTooShort, "639 cm refuses two 120 landings");
    in.Kind = T6LS::Arrangement::Switchback;
    in.Scale.MinFlightWidthCm = 120;
    in.Scale.MinLandingCm = 120;
    in.Footprint = {0, 252, 0, 440};
    T6Expect(T6LS::ValidateStairLayout(T6LS::GenerateStairLayout(in)) == T6LS::Refusal::None, "alternative enclosed 252 by 440 switchback supports 120 circulation");
    const auto door = T6TF::PlannedUpperDoor();
    T6Expect(door.Id == T6TF::UpperDoorId && door.LocationId == T6TF::UpperLocationId && !door.bInitiallyOpen &&
             door.Access == PerfectWorld::DoorAccess::Private && door.OpenMinute == 0 && door.CloseMinute == 0 && door.OpeningWindows.empty() &&
             !door.bTwoWay && door.PreferredSwingSide == 1 && door.VisitingWindows.empty(), "exact unoccupied planned private door definition");
    std::cout << "LaneTallFlatsLayout: " << T6Assertions << " assertions passed\n";
}
