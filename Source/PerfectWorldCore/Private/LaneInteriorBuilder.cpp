#include "LaneInteriorDetail.h"

#include <algorithm>
#include <cstdlib>

namespace PerfectWorld::LaneInterior::Detail
{
namespace
{
constexpr int LiRouteHalfWidth = 40; // reserved corridor either side of a route centre line
constexpr int LiRouteSolidHalfWidth = 34; // the walking capsule radius

std::string LiLastPart(const std::string& id)
{
    const auto dot = id.rfind('.');
    return dot == std::string::npos ? id : id.substr(dot + 1);
}
} // namespace

int ClampInt(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

Builder::Builder(const InteriorInput& in) : In(in), W(in.WidthCm), D(in.DepthCm)
{
    L.BuildingId = in.BuildingId;
    L.Kind = in.Kind;
    L.WidthCm = in.WidthCm;
    L.DepthCm = in.DepthCm;
    L.FinishedFloorCm = in.FinishedFloorCm;
    L.StoreyHeightCm = in.StoreyHeightCm;
    L.ClearHeightCm = in.StoreyHeightCm - SlabAndPlasterCm;
    Inner = Rect{WallThicknessCm, W - WallThicknessCm, WallThicknessCm, D - WallThicknessCm};
    L.Interior = Inner;
}

std::string Builder::Key(std::string_view kind, std::string_view name) const
{
    std::string id = L.BuildingId;
    id += '.';
    id += kind;
    id += '.';
    id += name;
    return id;
}

Room Builder::AddRoom(std::string_view name, RoomKind kind, const Rect& area, std::string_view floor, std::string_view wall)
{
    Room room;
    room.Id = Key("room", name);
    room.Kind = kind;
    room.Area = area;
    room.FloorPalette = std::string(floor);
    room.WallPalette = std::string(wall);
    L.Rooms.push_back(room);
    return room;
}

void Builder::AddZone(ZoneKind kind, std::string_view roomId, const Rect& area, std::string_view ownerId, std::string_view name)
{
    Zone zone;
    zone.Id = Key("zone", name);
    zone.Kind = kind;
    zone.RoomId = std::string(roomId);
    zone.Area = area;
    zone.OwnerId = std::string(ownerId);
    L.Zones.push_back(std::move(zone));
}

void Builder::AddConnection(std::string_view name, std::string_view roomA, std::string_view roomB, const std::string& openingId,
                            int clearWidth)
{
    Connection c;
    c.Id = Key("connection", name);
    c.RoomA = std::string(roomA);
    c.RoomB = std::string(roomB);
    c.OpeningId = openingId;
    c.ClearWidthCm = clearWidth;
    L.Connections.push_back(std::move(c));
}

void Builder::AddOpening(Run& run, std::string_view name, OpeningKind kind, int a0, int a1, int w0, int w1, std::string_view roomA,
                         std::string_view roomB, const std::string& doorId, bool hingeAtLow)
{
    Opening o;
    o.Id = Key("opening", name);
    o.WallId = Key("wall", run.Name);
    o.Kind = kind;
    o.Plan = run.AlongU ? Rect{a0, a1, run.P0, run.P1} : Rect{run.P0, run.P1, a0, a1};
    o.W0 = w0;
    o.W1 = w1;
    o.ClearWidthCm = kind == OpeningKind::Window ? a1 - a0 : a1 - a0 - 4;
    o.RoomA = std::string(roomA);
    o.RoomB = std::string(roomB);
    if (kind == OpeningKind::Door && !doorId.empty())
    {
        o.DoorId = doorId;
        const int mid = run.P0 + (run.P1 - run.P0) / 2;
        const int hinge = hingeAtLow ? a0 : a1;
        o.HingeU = run.AlongU ? hinge : mid;
        o.HingeV = run.AlongU ? mid : hinge;
        o.SweepSign = 1;
        const Rect sweep = run.AlongU ? Rect{a0, a1, run.P1, run.P1 + DoorClearWidthCm} : Rect{run.P1, run.P1 + DoorClearWidthCm, a0, a1};
        AddZone(ZoneKind::DoorSweep, roomB, sweep, o.Id, std::string("sweep.") + std::string(name));
    }
    run.Open.push_back(std::move(o));
}

void Builder::EndRun(Run& run)
{
    std::sort(run.Open.begin(), run.Open.end(), [&](const Opening& a, const Opening& b) {
        return (run.AlongU ? a.Plan.U0 : a.Plan.V0) < (run.AlongU ? b.Plan.U0 : b.Plan.V0);
    });
    const std::string wallId = Key("wall", run.Name);
    const int height = L.ClearHeightCm;
    int n = 0;
    const auto emit = [&](int a0, int a1, int w0, int w1) {
        if (a1 <= a0 || w1 <= w0)
        {
            return;
        }
        WallSegment s;
        s.Id = wallId + "." + std::to_string(n++);
        s.WallId = wallId;
        s.Plan = run.AlongU ? Rect{a0, a1, run.P0, run.P1} : Rect{run.P0, run.P1, a0, a1};
        s.W0 = w0;
        s.W1 = w1;
        L.Walls.push_back(std::move(s));
    };
    int cursor = run.A0;
    for (const Opening& o : run.Open)
    {
        const int a0 = run.AlongU ? o.Plan.U0 : o.Plan.V0;
        const int a1 = run.AlongU ? o.Plan.U1 : o.Plan.V1;
        emit(cursor, a0, 0, height);
        emit(a0, a1, 0, o.W0);
        emit(a0, a1, o.W1, height);
        cursor = a1;
    }
    emit(cursor, run.A1, 0, height);
    for (Opening& o : run.Open)
    {
        L.Openings.push_back(std::move(o));
    }
    run.Open.clear();
}

bool Builder::HitsSolid(const Rect& r) const
{
    for (const Furniture& f : L.Pieces)
    {
        if (f.Solid && f.Footprint.Overlaps(r))
        {
            return true;
        }
    }
    return false;
}

bool Builder::HitsZone(const Rect& r) const
{
    for (const Zone& z : L.Zones)
    {
        if (z.Area.Overlaps(r))
        {
            return true;
        }
    }
    return false;
}

Rect Builder::UseRect(const Rect& fp, const Use& use)
{
    if (use.SideV != 0)
    {
        const int lateral = use.Lateral > 0 ? use.Lateral : ClampInt(fp.Width(), StandZoneCm, 120);
        const int centre = use.HasCentre ? use.Centre : (fp.U0 + fp.U1) / 2;
        const int u0 = centre - lateral / 2;
        const int v0 = use.SideV > 0 ? fp.V1 : fp.V0 - use.Depth;
        return Rect{u0, u0 + lateral, v0, v0 + use.Depth};
    }
    const int lateral = use.Lateral > 0 ? use.Lateral : ClampInt(fp.Depth(), StandZoneCm, 120);
    const int centre = use.HasCentre ? use.Centre : (fp.V0 + fp.V1) / 2;
    const int v0 = centre - lateral / 2;
    const int u0 = use.SideU > 0 ? fp.U1 : fp.U0 - use.Depth;
    return Rect{u0, u0 + use.Depth, v0, v0 + lateral};
}

Mark Builder::MarkNow() const
{
    return Mark{L.Pieces.size(), L.Anchors.size(), L.Zones.size()};
}

void Builder::Rollback(const Mark& mark)
{
    L.Pieces.resize(mark.Pieces);
    L.Anchors.resize(mark.Anchors);
    L.Zones.resize(mark.Zones);
}

bool Builder::TryPlace(const Room& room, const Item& item, const Rect& fp, const Use& use)
{
    if (!room.Area.Contains(fp))
    {
        return false;
    }
    if (item.Solid && (HitsSolid(fp) || HitsZone(fp)))
    {
        return false;
    }
    const bool hasUse = use.Depth > 0 && (use.SideU != 0 || use.SideV != 0);
    Rect zone;
    if (hasUse)
    {
        zone = UseRect(fp, use);
        if (!room.Area.Contains(zone) || HitsSolid(zone))
        {
            return false;
        }
    }
    Furniture f;
    f.Id = Key("piece", item.Name);
    f.AssetId = item.Asset;
    f.Palette = item.Palette;
    f.RoomId = room.Id;
    f.Kind = item.Kind;
    f.Footprint = fp;
    f.H0 = item.H0;
    f.H1 = item.H1;
    f.Solid = item.Solid;
    L.Pieces.push_back(f);
    if (!hasUse)
    {
        return true;
    }
    AddZone(use.Zone, room.Id, zone, f.Id, item.Name + ".use");
    if (use.Anchor)
    {
        Anchor a;
        a.Id = Key("anchor", use.AnchorName);
        a.Kind = use.Kind;
        a.RoomId = room.Id;
        a.FaceU = -use.SideU;
        a.FaceV = -use.SideV;
        a.FurnitureId = f.Id;
        if (use.Kind == AnchorKind::Seat)
        {
            a.U = (fp.U0 + fp.U1) / 2;
            a.V = (fp.V0 + fp.V1) / 2;
        }
        else
        {
            a.U = (zone.U0 + zone.U1) / 2;
            a.V = (zone.V0 + zone.V1) / 2;
        }
        L.Anchors.push_back(std::move(a));
    }
    return true;
}

bool Builder::ScanPlace(const Rect& area, int w, int h, int refU, int refV, const std::function<bool(int, int)>& place)
{
    struct Cand
    {
        long long Dist;
        int V0;
        int U0;
    };
    std::vector<Cand> cands;
    for (int v0 = area.V0; v0 + h <= area.V1; v0 += 5)
    {
        for (int u0 = area.U0; u0 + w <= area.U1; u0 += 5)
        {
            const long long du = (u0 + w / 2) - refU;
            const long long dv = (v0 + h / 2) - refV;
            cands.push_back(Cand{du * du + dv * dv, v0, u0});
        }
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) {
        return a.Dist != b.Dist ? a.Dist < b.Dist : (a.V0 != b.V0 ? a.V0 < b.V0 : a.U0 < b.U0);
    });
    for (const Cand& c : cands)
    {
        const Mark mark = MarkNow();
        if (place(c.U0, c.V0))
        {
            return true;
        }
        Rollback(mark);
    }
    return false;
}

void Builder::AddLights(const Room& room)
{
    const long long area = static_cast<long long>(room.Area.Width()) * room.Area.Depth();
    const int count = static_cast<int>(ClampInt(static_cast<int>((area + 119999) / 120000), 1, 4));
    const bool alongU = room.Area.Width() >= room.Area.Depth();
    const int longSide = alongU ? room.Area.Width() : room.Area.Depth();
    const int shortSide = alongU ? room.Area.Depth() : room.Area.Width();
    const std::string name = LiLastPart(room.Id);
    for (int k = 0; k < count; ++k)
    {
        const int along = (alongU ? room.Area.U0 : room.Area.V0) + longSide * (2 * k + 1) / (2 * count);
        LightSpec light;
        light.Id = Key("light", name + "." + std::to_string(k));
        light.RoomId = room.Id;
        light.U = alongU ? along : (room.Area.U0 + room.Area.U1) / 2;
        light.V = alongU ? (room.Area.V0 + room.Area.V1) / 2 : along;
        light.HeightCm = L.ClearHeightCm - 17;
        light.RadiusCm = ClampInt(shortSide * 3 / 4, 150, 300);
        light.Lumens = ClampInt(static_cast<int>(area / 10000) * 45, 600, 1000);
        L.Lights.push_back(std::move(light));
    }
}

void Builder::MaybeWindow(Run& run, std::string_view name, int spanA0, int spanA1, int maxWidth, std::string_view room)
{
    const int avail = spanA1 - spanA0 - 40; // 20 cm of plain wall either side
    const int width = std::min(avail, maxWidth);
    if (width < 120)
    {
        return;
    }
    const int a0 = spanA0 + (spanA1 - spanA0 - width) / 2;
    const int a1 = a0 + width;
    const Rect probe = Rect{a0, a1, run.P0 - 35, run.P1 + 35};
    for (const Furniture& f : L.Pieces)
    {
        if (f.Solid && f.H1 > WindowSillCm && f.Footprint.Overlaps(probe))
        {
            return; // something tall stands against this wall
        }
    }
    const bool front = run.P0 == 0;
    AddOpening(run, name, OpeningKind::Window, a0, a1, WindowSillCm, WindowHeadCm, front ? OutsideId : room, front ? room : OutsideId);
}

bool Builder::AddRoute(std::string_view name, std::string_view fromAnchor, std::string_view toAnchor, std::vector<RoutePoint> points)
{
    Route route;
    route.Id = Key("route", name);
    route.FromAnchorId = Key("anchor", fromAnchor);
    route.ToAnchorId = Key("anchor", toAnchor);
    route.Points = std::move(points);
    for (size_t i = 0; i + 1 < route.Points.size(); ++i)
    {
        const RoutePoint p = route.Points[i];
        const RoutePoint q = route.Points[i + 1];
        const Rect core{std::min(p.U, q.U) - LiRouteSolidHalfWidth, std::max(p.U, q.U) + LiRouteSolidHalfWidth,
                        std::min(p.V, q.V) - LiRouteSolidHalfWidth, std::max(p.V, q.V) + LiRouteSolidHalfWidth};
        if (HitsSolid(core))
        {
            return false;
        }
    }
    for (size_t i = 0; i + 1 < route.Points.size(); ++i)
    {
        const RoutePoint p = route.Points[i];
        const RoutePoint q = route.Points[i + 1];
        const Rect zone{std::min(p.U, q.U) - LiRouteHalfWidth, std::max(p.U, q.U) + LiRouteHalfWidth,
                        std::min(p.V, q.V) - LiRouteHalfWidth, std::max(p.V, q.V) + LiRouteHalfWidth};
        AddZone(ZoneKind::Route, std::string_view(), zone, route.Id, std::string(name) + "." + std::to_string(i));
    }
    L.Routes.push_back(std::move(route));
    return true;
}
} // namespace PerfectWorld::LaneInterior::Detail
