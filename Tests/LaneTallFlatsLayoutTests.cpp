#include "LaneTallFlatsLayout.h"

#include "LaneFrontageCatalogue.h"

#include <algorithm>
#include <cmath>
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
    const auto* R52Bed = T6LI::FindAnchor(l.Upper, l.Upper.BuildingId + ".anchor.bed_side");
    if (!R52Bed) return false;
    const std::vector<T6LI::RoutePoint> path{{550, 715}, {715, 715}, {715, 500},
        {R52Bed->U, 500}, {R52Bed->U, R52Bed->V}, {R52Bed->U, 500}, {965, 500}, {965, 300}};
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
    for (const auto& R52Anchor : l.Upper.Anchors)
        std::cout << "R52 upper anchor " << R52Anchor.Id << " U" << R52Anchor.U << " V" << R52Anchor.V << '\n';
    const auto* R52Bed = T6LI::FindAnchor(l.Upper, l.Upper.BuildingId + ".anchor.bed_side");
    T6Expect(R52Bed != nullptr, "R52 real resident standing anchor");
    double R52Length = std::abs(R52Bed->V - 500) + std::abs(R52Bed->U - 715) + 215 + 165 + 60;
    double R52V = 655, R52Z = l.Stair.UpperFloorCm;
    for (auto R52It = l.Stair.Steps.rbegin(); R52It != l.Stair.Steps.rend(); ++R52It)
    {
        const double R52NextZ = l.Stair.LowerFloorCm + R52It->TopMm * 0.1;
        R52Length += std::hypot(R52V - R52It->Plan.V0, R52Z - R52NextZ);
        R52V = R52It->Plan.V0; R52Z = R52NextZ;
    }
    R52Length += std::hypot(R52V - 195, R52Z - l.Stair.LowerFloorCm) + 240;
    std::cout << "R52 upper-to-front polyline " << R52Length << " cm; allowance 1680 cm\n";
    T6Expect(R52Length <= 1680 && std::abs(R52Length - 1639.14153793) < 1,
        "R52 complete bed-side, side-opening, landing, nosings and shared-front route fits allowance");
    const auto* R52OldRow = T6LI::FindFrontageById("building.nabapara.old_house");
    T6Expect(R52OldRow != nullptr, "R52 old-house catalogue row");
    const auto R52Old = T6LI::GenerateInteriorLayout(T6LI::MakeFrontageInput(*R52OldRow, 15, 300));
    const auto* R52OldBed = T6LI::FindAnchor(R52Old, R52Old.BuildingId + ".anchor.bed_side");
    T6Expect(R52Old.Ok() && R52OldBed != nullptr, "R52 Prabir real standing anchor");
    const std::vector<T6LI::RoutePoint> R52OldPath{{R52OldBed->U, R52OldBed->V}, {250, R52OldBed->V}, {250, 365}, {250, -45}};
    int R52OldLength = 0;
    for (size_t R52Index = 1; R52Index < R52OldPath.size(); ++R52Index)
    {
        const auto R52From = R52OldPath[R52Index - 1], R52To = R52OldPath[R52Index];
        const int R52Span = std::abs(R52To.U - R52From.U) + std::abs(R52To.V - R52From.V);
        R52OldLength += R52Span;
        for (int R52Along = 0; R52Along <= R52Span; ++R52Along)
        {
            const int R52U = R52From.U + (R52To.U - R52From.U) * R52Along / std::max(1, R52Span);
            const int R52Depth = R52From.V + (R52To.V - R52From.V) * R52Along / std::max(1, R52Span);
            const T6LI::Rect R52Body{R52U - 34, R52U + 34, R52Depth - 34, R52Depth + 34};
            for (const auto& R52Wall : R52Old.Walls)
                T6Expect(R52Wall.W0 >= 169 || !R52Wall.Plan.Overlaps(R52Body), "R52 Prabir route clears real walls and head");
            for (const auto& R52Piece : R52Old.Pieces)
                T6Expect(!R52Piece.Solid || !R52Piece.Footprint.Overlaps(R52Body), "R52 Prabir route clears solid furniture");
        }
    }
    std::cout << "R52 old-house bed U" << R52OldBed->U << " V" << R52OldBed->V << "; visible route " << R52OldLength
              << " cm; authored errand allowance 1680 cm\n";
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
        {
            T6Expect(o.ClearWidthCm >= 90 && o.W1 - o.W0 >= 210, "90 by 210 clear doorways");
            if (o.RoomA == "outside")
                T6Expect(o.Kind == T6LI::OpeningKind::Door && o.DoorId == T6TF::UpperDoorId &&
                         o.Plan == T6LI::Rect{615, 640, 668, 762} && o.HingeU == 627 && o.HingeV == 668 &&
                         o.SweepSign == 1, "R52 existing upper door hangs on real side opening into sleeping room");
            else T6Expect(o.DoorId.empty(), "R52 internal doorway remains unhung");
        }
    T6Expect(std::count_if(l.Upper.Openings.begin(), l.Upper.Openings.end(), [](const auto& o) {
        return o.DoorId == T6TF::UpperDoorId;
    }) == 1, "R52 exactly one upper door");
    T6Expect(std::any_of(l.Upper.Zones.begin(), l.Upper.Zones.end(), [](const auto& z) {
        return z.Kind == T6LI::ZoneKind::DoorSweep && z.Area == T6LI::Rect{640, 730, 668, 762};
    }), "R52 side-door sweep reserved inside dwelling");
    auto R52BlockedSweep = l.Upper;
    R52BlockedSweep.Pieces.push_back({"r52.obstruction", "asset.lane.furniture.trunk", "wood_light",
        "building.nabapara.tall_flats.upper_a.room.sleeping", T6LI::FurnitureKind::Trunk,
        {650, 680, 690, 720}, 0, 80, true});
    T6Expect(T6LI::ValidateInteriorLayout(R52BlockedSweep) != T6LI::Refusal::None, "R52 furniture cannot occupy side-door sweep");
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
