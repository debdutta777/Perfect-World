#include "LaneInteriorLayout.h"

#include "LaneInteriorDetail.h"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>

namespace PerfectWorld::LaneInterior
{
namespace
{
constexpr int LlMinWidthShop = 420;
constexpr int LlMinWidthResidence = 400;
constexpr int LlMinWidthShared = 440;
constexpr int LlMinDepthShop = 680;
constexpr int LlMinDepthResidence = 520;
constexpr int LlMinDepthShared = 520;
constexpr int LlMaxWidth = 2400;
constexpr int LlMaxDepth = 1500;
constexpr int LlHalfDoor = DoorOpeningWidthCm / 2;
constexpr int LlCapsuleRadius = 34;

Rect LlReflect(const Rect& r, int width)
{
    return Rect{width - r.U1, width - r.U0, r.V0, r.V1};
}

InteriorLayout LlRefuse(const InteriorInput& in, Refusal why)
{
    InteriorLayout out;
    out.Status = why;
    out.BuildingId = in.BuildingId;
    out.Kind = in.Kind;
    out.WidthCm = in.WidthCm;
    out.DepthCm = in.DepthCm;
    return out;
}

Refusal LlCheckInput(const InteriorInput& in)
{
    if (in.BuildingId.empty())
    {
        return Refusal::InvalidBuildingId;
    }
    const bool shop = in.Kind == Variant::ShopWithStockroom;
    const bool shared = in.Kind == Variant::SharedEntry;
    if (in.WidthCm < (shop ? LlMinWidthShop : (shared ? LlMinWidthShared : LlMinWidthResidence)))
    {
        return Refusal::WidthTooNarrow;
    }
    if (in.DepthCm < (shop ? LlMinDepthShop : (shared ? LlMinDepthShared : LlMinDepthResidence)))
    {
        return Refusal::DepthTooShallow;
    }
    if (in.WidthCm > LlMaxWidth || in.DepthCm > LlMaxDepth)
    {
        return Refusal::FootprintTooLarge;
    }
    if (in.StoreyHeightCm < 280 || in.StoreyHeightCm > 400)
    {
        return Refusal::StoreyHeightInvalid;
    }
    if (in.FinishedFloorCm < 0 || in.FinishedFloorCm > 150)
    {
        return Refusal::FloorDatumInvalid;
    }
    if (in.EntranceCentreU - LlHalfDoor < WallThicknessCm || in.EntranceCentreU + LlHalfDoor > in.WidthCm - WallThicknessCm)
    {
        return Refusal::EntranceOffFacade;
    }
    if (!shared && in.InternalDoorCentreU != 0 &&
        (in.InternalDoorCentreU - LlHalfDoor < WallThicknessCm || in.InternalDoorCentreU + LlHalfDoor > in.WidthCm - WallThicknessCm))
    {
        return Refusal::InternalDoorOffWall;
    }
    if (in.Furniture == FurnitureSet::Stationery && !shop)
    {
        return Refusal::FurnitureSetMismatch;
    }
    if (in.Furniture == FurnitureSet::Residence && shop)
    {
        return Refusal::FurnitureSetMismatch;
    }
    return Refusal::None;
}

bool LlSolidHit(const InteriorLayout& l, const Rect& r)
{
    return std::any_of(l.Pieces.begin(), l.Pieces.end(), [&](const Furniture& f) { return f.Solid && f.Footprint.Overlaps(r); });
}

const Room* LlRoom(const InteriorLayout& l, const std::string& id)
{
    return FindRoom(l, id);
}

Refusal LlZoneCode(ZoneKind kind)
{
    switch (kind)
    {
    case ZoneKind::Circulation: return Refusal::CirculationBlocked;
    case ZoneKind::DoorSweep: return Refusal::SweepBlocked;
    case ZoneKind::Route: return Refusal::RouteBlocked;
    default: return Refusal::ZoneBlocked;
    }
}

bool LlConnected(const InteriorLayout& l)
{
    std::unordered_map<std::string, std::vector<std::string>> graph;
    for (const Connection& c : l.Connections)
    {
        graph[c.RoomA].push_back(c.RoomB);
        graph[c.RoomB].push_back(c.RoomA);
    }
    std::set<std::string> seen{std::string(Detail::OutsideId)};
    std::queue<std::string> open;
    open.push(std::string(Detail::OutsideId));
    while (!open.empty())
    {
        const std::string at = open.front();
        open.pop();
        for (const std::string& next : graph[at])
        {
            if (seen.insert(next).second)
            {
                open.push(next);
            }
        }
    }
    return std::all_of(l.Rooms.begin(), l.Rooms.end(), [&](const Room& r) { return seen.count(r.Id) > 0; });
}
} // namespace

std::string_view RefusalCode(Refusal refusal) noexcept
{
    switch (refusal)
    {
    case Refusal::None: return "ok";
    case Refusal::InvalidBuildingId: return "invalid_building_id";
    case Refusal::WidthTooNarrow: return "width_too_narrow";
    case Refusal::DepthTooShallow: return "depth_too_shallow";
    case Refusal::FootprintTooLarge: return "footprint_too_large";
    case Refusal::StoreyHeightInvalid: return "storey_height_invalid";
    case Refusal::FloorDatumInvalid: return "floor_datum_invalid";
    case Refusal::EntranceOffFacade: return "entrance_off_facade";
    case Refusal::InternalDoorOffWall: return "internal_door_off_wall";
    case Refusal::FurnitureSetMismatch: return "furniture_set_mismatch";
    case Refusal::NoRoomForUnits: return "no_room_for_units";
    case Refusal::FurnitureDoesNotFit: return "furniture_does_not_fit";
    case Refusal::RoomsDisconnected: return "rooms_disconnected";
    case Refusal::DoorwayTooSmall: return "doorway_too_small";
    case Refusal::CirculationBlocked: return "circulation_blocked";
    case Refusal::SweepBlocked: return "sweep_blocked";
    case Refusal::ZoneBlocked: return "zone_blocked";
    case Refusal::FurnitureOverlap: return "furniture_overlap";
    case Refusal::FurnitureOutsideRoom: return "furniture_outside_room";
    case Refusal::DuplicateId: return "duplicate_id";
    case Refusal::AnchorInvalid: return "anchor_invalid";
    case Refusal::RouteBlocked: return "route_blocked";
    case Refusal::WallInvalid: return "wall_invalid";
    }
    return "unknown";
}

const Anchor* FindAnchor(const InteriorLayout& layout, std::string_view id)
{
    const auto it = std::find_if(layout.Anchors.begin(), layout.Anchors.end(), [&](const Anchor& a) { return a.Id == id; });
    return it == layout.Anchors.end() ? nullptr : &*it;
}

const Room* FindRoom(const InteriorLayout& layout, std::string_view id)
{
    const auto it = std::find_if(layout.Rooms.begin(), layout.Rooms.end(), [&](const Room& r) { return r.Id == id; });
    return it == layout.Rooms.end() ? nullptr : &*it;
}

const Furniture* FindFurniture(const InteriorLayout& layout, std::string_view id)
{
    const auto it = std::find_if(layout.Pieces.begin(), layout.Pieces.end(), [&](const Furniture& f) { return f.Id == id; });
    return it == layout.Pieces.end() ? nullptr : &*it;
}

const Route* FindRoute(const InteriorLayout& layout, std::string_view id)
{
    const auto it = std::find_if(layout.Routes.begin(), layout.Routes.end(), [&](const Route& r) { return r.Id == id; });
    return it == layout.Routes.end() ? nullptr : &*it;
}

void ReflectLayoutU(InteriorLayout& l)
{
    const int w = l.WidthCm;
    l.Interior = LlReflect(l.Interior, w);
    for (Room& r : l.Rooms) r.Area = LlReflect(r.Area, w);
    for (Opening& o : l.Openings)
    {
        o.Plan = LlReflect(o.Plan, w);
        o.HingeU = w - o.HingeU;
    }
    for (WallSegment& s : l.Walls) s.Plan = LlReflect(s.Plan, w);
    for (Furniture& f : l.Pieces) f.Footprint = LlReflect(f.Footprint, w);
    for (Anchor& a : l.Anchors)
    {
        a.U = w - a.U;
        a.FaceU = -a.FaceU;
    }
    for (Zone& z : l.Zones) z.Area = LlReflect(z.Area, w);
    for (LightSpec& s : l.Lights) s.U = w - s.U;
    for (Route& r : l.Routes)
    {
        for (RoutePoint& p : r.Points) p.U = w - p.U;
    }
}

Refusal ValidateInteriorLayout(const InteriorLayout& l)
{
    std::set<std::string> ids;
    const auto fresh = [&](const std::string& id) { return !id.empty() && ids.insert(id).second; };
    for (const Room& x : l.Rooms) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const Opening& x : l.Openings) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const WallSegment& x : l.Walls) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const Connection& x : l.Connections) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const Furniture& x : l.Pieces) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const Anchor& x : l.Anchors) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const Zone& x : l.Zones) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const LightSpec& x : l.Lights) if (!fresh(x.Id)) return Refusal::DuplicateId;
    for (const Route& x : l.Routes) if (!fresh(x.Id)) return Refusal::DuplicateId;

    const Rect outer{0, l.WidthCm, 0, l.DepthCm};
    for (size_t i = 0; i < l.Rooms.size(); ++i)
    {
        if (!l.Interior.Contains(l.Rooms[i].Area)) return Refusal::WallInvalid;
        for (size_t j = i + 1; j < l.Rooms.size(); ++j)
        {
            if (l.Rooms[i].Area.Overlaps(l.Rooms[j].Area)) return Refusal::WallInvalid;
        }
    }
    for (size_t i = 0; i < l.Walls.size(); ++i)
    {
        const WallSegment& s = l.Walls[i];
        if (!outer.Contains(s.Plan) || s.W0 < 0 || s.W1 > l.ClearHeightCm || s.W0 >= s.W1) return Refusal::WallInvalid;
        for (const Room& r : l.Rooms)
        {
            if (r.Area.Overlaps(s.Plan)) return Refusal::WallInvalid;
        }
        for (size_t j = i + 1; j < l.Walls.size(); ++j)
        {
            const WallSegment& t = l.Walls[j];
            if (s.Plan.Overlaps(t.Plan) && s.W0 < t.W1 && t.W0 < s.W1) return Refusal::WallInvalid;
        }
    }
    for (const Opening& o : l.Openings)
    {
        if (!outer.Contains(o.Plan)) return Refusal::WallInvalid;
        if (o.Kind == OpeningKind::Window) continue;
        const int along = std::max(o.Plan.Width(), o.Plan.Depth());
        if (o.ClearWidthCm < DoorClearWidthCm || along < o.ClearWidthCm || o.W0 != 0 || o.W1 - o.W0 < DoorClearHeightCm)
        {
            return Refusal::DoorwayTooSmall;
        }
        for (const WallSegment& s : l.Walls)
        {
            if (s.Plan.Overlaps(o.Plan) && s.W0 < DoorClearHeightCm) return Refusal::WallInvalid; // a wall inside the doorway
        }
    }

    for (const Connection& c : l.Connections)
    {
        const bool openingKnown = std::any_of(l.Openings.begin(), l.Openings.end(), [&](const Opening& o) { return o.Id == c.OpeningId; });
        const auto known = [&](const std::string& id) { return id == Detail::OutsideId || FindRoom(l, id) != nullptr; };
        if (!openingKnown || !known(c.RoomA) || !known(c.RoomB)) return Refusal::RoomsDisconnected;
    }
    if (!LlConnected(l)) return Refusal::RoomsDisconnected;

    for (size_t i = 0; i < l.Pieces.size(); ++i)
    {
        const Furniture& f = l.Pieces[i];
        const Room* room = LlRoom(l, f.RoomId);
        if (room == nullptr || !room->Area.Contains(f.Footprint)) return Refusal::FurnitureOutsideRoom;
        if (!f.Solid) continue;
        for (size_t j = i + 1; j < l.Pieces.size(); ++j)
        {
            if (l.Pieces[j].Solid && f.Footprint.Overlaps(l.Pieces[j].Footprint)) return Refusal::FurnitureOverlap;
        }
    }
    for (const Zone& z : l.Zones)
    {
        if (z.Kind == ZoneKind::Circulation && std::min(z.Area.Width(), z.Area.Depth()) < CirculationCm) return Refusal::CirculationBlocked;
        if (z.Kind != ZoneKind::Route)
        {
            const Room* room = LlRoom(l, z.RoomId);
            if (room == nullptr || !room->Area.Contains(z.Area)) return LlZoneCode(z.Kind);
        }
        if (LlSolidHit(l, z.Area)) return LlZoneCode(z.Kind);
    }
    for (const Anchor& a : l.Anchors)
    {
        if (a.FaceU < -1 || a.FaceU > 1 || a.FaceV < -1 || a.FaceV > 1 || (a.FaceU == 0 && a.FaceV == 0)) return Refusal::AnchorInvalid;
        if (a.Kind == AnchorKind::Threshold)
        {
            if (a.RoomId != Detail::OutsideId || a.V >= 0) return Refusal::AnchorInvalid;
            continue;
        }
        const Room* room = LlRoom(l, a.RoomId);
        if (room == nullptr || !room->Area.ContainsPoint(a.U, a.V)) return Refusal::AnchorInvalid;
        if (a.Kind == AnchorKind::Seat)
        {
            const Furniture* f = FindFurniture(l, a.FurnitureId);
            if (f == nullptr || f->Kind != FurnitureKind::Chair || !f->Footprint.ContainsPoint(a.U, a.V)) return Refusal::AnchorInvalid;
            continue;
        }
        const Rect square{a.U - StandZoneCm / 2, a.U + StandZoneCm / 2, a.V - StandZoneCm / 2, a.V + StandZoneCm / 2};
        if (!room->Area.Contains(square) || LlSolidHit(l, square)) return Refusal::AnchorInvalid;
    }
    for (const LightSpec& s : l.Lights)
    {
        const Room* room = LlRoom(l, s.RoomId);
        if (room == nullptr || !room->Area.ContainsPoint(s.U, s.V) || s.HeightCm <= 0 || s.HeightCm >= l.ClearHeightCm) return Refusal::WallInvalid;
    }
    for (const Route& r : l.Routes)
    {
        if (r.Points.size() < 2 || FindAnchor(l, r.FromAnchorId) == nullptr || FindAnchor(l, r.ToAnchorId) == nullptr) return Refusal::RouteBlocked;
        for (size_t i = 0; i + 1 < r.Points.size(); ++i)
        {
            const RoutePoint p = r.Points[i];
            const RoutePoint q = r.Points[i + 1];
            if (p.U != q.U && p.V != q.V) return Refusal::RouteBlocked;
            const Rect leg{std::min(p.U, q.U) - LlCapsuleRadius, std::max(p.U, q.U) + LlCapsuleRadius, std::min(p.V, q.V) - LlCapsuleRadius,
                           std::max(p.V, q.V) + LlCapsuleRadius};
            if (LlSolidHit(l, leg)) return Refusal::RouteBlocked;
        }
    }
    return Refusal::None;
}

InteriorLayout GenerateInteriorLayout(const InteriorInput& in)
{
    const Refusal bad = LlCheckInput(in);
    if (bad != Refusal::None)
    {
        return LlRefuse(in, bad);
    }
    const auto flipped = [&](bool mirror) {
        InteriorInput other = in;
        other.MirrorU = mirror;
        other.EntranceCentreU = in.WidthCm - in.EntranceCentreU;
        if (in.InternalDoorCentreU != 0)
        {
            other.InternalDoorCentreU = in.WidthCm - in.InternalDoorCentreU;
        }
        return other;
    };
    if (in.MirrorU)
    {
        InteriorInput other = flipped(false);
        InteriorLayout out = GenerateInteriorLayout(other);
        if (out.Ok())
        {
            ReflectLayoutU(out);
            out.Mirrored = true;
        }
        return out;
    }
    if (in.Kind == Variant::ShopWithStockroom && in.EntranceCentreU > in.WidthCm / 2)
    {
        // The shop is authored with its staff gap on the side away from a left-half entrance; a right-half entrance is its reflection.
        InteriorLayout out = GenerateInteriorLayout(flipped(false));
        if (out.Ok())
        {
            ReflectLayoutU(out);
        }
        return out;
    }
    Detail::Builder builder(in);
    Refusal built = Refusal::None;
    switch (in.Kind)
    {
    case Variant::CompactResidence: built = Detail::BuildTwoRoom(builder, false); break;
    case Variant::ShopWithStockroom: built = Detail::BuildTwoRoom(builder, true); break;
    case Variant::SharedEntry: built = Detail::BuildSharedEntry(builder); break;
    }
    if (built != Refusal::None)
    {
        return LlRefuse(in, built);
    }
    const Refusal checked = ValidateInteriorLayout(builder.L);
    if (checked != Refusal::None)
    {
        return LlRefuse(in, checked);
    }
    return std::move(builder.L);
}

std::string DescribeLayout(const InteriorLayout& l)
{
    std::ostringstream o;
    const auto rect = [&](const Rect& r) { o << '[' << r.U0 << ',' << r.U1 << ',' << r.V0 << ',' << r.V1 << ']'; };
    o << l.BuildingId << " status=" << RefusalCode(l.Status) << " " << l.WidthCm << 'x' << l.DepthCm << " ff=" << l.FinishedFloorCm
      << " clear=" << l.ClearHeightCm << " mirrored=" << l.Mirrored << '\n';
    for (const Room& r : l.Rooms) { o << "room " << r.Id << ' '; rect(r.Area); o << ' ' << r.FloorPalette << '\n'; }
    for (const Opening& x : l.Openings)
    {
        o << "opening " << x.Id << ' ' << static_cast<int>(x.Kind) << ' ';
        rect(x.Plan);
        o << ' ' << x.W0 << '-' << x.W1 << ' ' << x.RoomA << '>' << x.RoomB << ' ' << x.DoorId << ' ' << x.HingeU << ',' << x.HingeV << '\n';
    }
    for (const WallSegment& s : l.Walls) { o << "wall " << s.Id << ' '; rect(s.Plan); o << ' ' << s.W0 << '-' << s.W1 << '\n'; }
    for (const Connection& c : l.Connections) o << "conn " << c.Id << ' ' << c.RoomA << '>' << c.RoomB << ' ' << c.OpeningId << '\n';
    for (const Furniture& f : l.Pieces)
    {
        o << "piece " << f.Id << ' ' << f.AssetId << ' ';
        rect(f.Footprint);
        o << ' ' << f.H0 << '-' << f.H1 << ' ' << f.Solid << '\n';
    }
    for (const Anchor& a : l.Anchors)
    {
        o << "anchor " << a.Id << ' ' << static_cast<int>(a.Kind) << " (" << a.U << ',' << a.V << ") face(" << a.FaceU << ',' << a.FaceV << ")\n";
    }
    for (const Zone& z : l.Zones) { o << "zone " << z.Id << ' ' << static_cast<int>(z.Kind) << ' '; rect(z.Area); o << '\n'; }
    for (const LightSpec& s : l.Lights) o << "light " << s.Id << " (" << s.U << ',' << s.V << ',' << s.HeightCm << ") " << s.RadiusCm << ' ' << s.Lumens << '\n';
    for (const Route& r : l.Routes)
    {
        o << "route " << r.Id << ' ' << r.LengthCm();
        for (const RoutePoint& p : r.Points) o << " (" << p.U << ',' << p.V << ')';
        o << '\n';
    }
    return o.str();
}
} // namespace PerfectWorld::LaneInterior
