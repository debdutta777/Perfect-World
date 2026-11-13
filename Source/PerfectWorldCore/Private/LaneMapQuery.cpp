#include "LaneMap.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <set>

namespace PerfectWorld::LaneMap
{
namespace
{
bool MpOnFrontEdge(const Polygon& rect, Point p) noexcept
{
    // The door sits on the rectangle's front (road-facing) edge: its y is one of the rectangle's two y values and its x is within.
    double minX = rect[0].X;
    double maxX = rect[0].X;
    double minY = rect[0].Y;
    double maxY = rect[0].Y;
    for (const Point& q : rect)
    {
        minX = std::min(minX, q.X);
        maxX = std::max(maxX, q.X);
        minY = std::min(minY, q.Y);
        maxY = std::max(maxY, q.Y);
    }
    return p.X >= minX && p.X <= maxX && (p.Y == minY || p.Y == maxY);
}

/** The two end masses (Side 0) are entered from the lane: the entrance is on the left or right edge, with y inside it. */
bool MpOnLaneFacingEdge(const Polygon& rect, Point p) noexcept
{
    double minX = rect[0].X;
    double maxX = rect[0].X;
    double minY = rect[0].Y;
    double maxY = rect[0].Y;
    for (const Point& q : rect)
    {
        minX = std::min(minX, q.X);
        maxX = std::max(maxX, q.X);
        minY = std::min(minY, q.Y);
        maxY = std::max(maxY, q.Y);
    }
    return p.Y >= minY && p.Y <= maxY && (p.X == minX || p.X == maxX);
}

bool MpOverlap(const Polygon& a, const Polygon& b) noexcept
{
    const auto bounds = [](const Polygon& p, double& x0, double& y0, double& x1, double& y1) {
        x0 = x1 = p[0].X;
        y0 = y1 = p[0].Y;
        for (const Point& q : p)
        {
            x0 = std::min(x0, q.X);
            x1 = std::max(x1, q.X);
            y0 = std::min(y0, q.Y);
            y1 = std::max(y1, q.Y);
        }
    };
    double ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
    bounds(a, ax0, ay0, ax1, ay1);
    bounds(b, bx0, by0, bx1, by1);
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1; // touching edges do not overlap
}
} // namespace

bool IsRectangle(const Polygon& polygon) noexcept
{
    if (polygon.size() != 4)
    {
        return false;
    }
    const bool horizontalFirst = polygon[0].Y == polygon[1].Y && polygon[1].X == polygon[2].X && polygon[2].Y == polygon[3].Y && polygon[3].X == polygon[0].X;
    const bool verticalFirst = polygon[0].X == polygon[1].X && polygon[1].Y == polygon[2].Y && polygon[2].X == polygon[3].X && polygon[3].Y == polygon[0].Y;
    if (!horizontalFirst && !verticalFirst)
    {
        return false;
    }
    const double area = (polygon[1].X - polygon[0].X) * (polygon[2].Y - polygon[1].Y) - (polygon[1].Y - polygon[0].Y) * (polygon[2].X - polygon[1].X);
    return area > 0.0; // counter-clockwise and not degenerate
}

bool IsMapValid(const Map& map) noexcept
{
    std::set<std::string_view> ids;
    for (const Building& b : map.Buildings)
    {
        if (b.Id.empty() || !ids.insert(b.Id).second || !IsRectangle(b.Footprint))
        {
            return false;
        }
        if (b.bEnterable != b.bHasEntrance ||
            (b.bHasEntrance && !(b.Side == 0 ? MpOnLaneFacingEdge(b.Footprint, b.Entrance) : MpOnFrontEdge(b.Footprint, b.Entrance))))
        {
            return false;
        }
        if (b.Label.empty() != b.LabelKey.empty())
        {
            return false;
        }
    }
    for (std::size_t i = 0; i < map.Buildings.size(); ++i)
    {
        for (std::size_t j = i + 1; j < map.Buildings.size(); ++j)
        {
            if (MpOverlap(map.Buildings[i].Footprint, map.Buildings[j].Footprint))
            {
                return false;
            }
        }
    }
    for (const Surface& s : map.Surfaces)
    {
        if (!IsRectangle(s.Outline))
        {
            return false;
        }
    }
    std::set<std::string_view> doorIds;
    for (const MapDoor& d : map.Doors)
    {
        const Building* owner = FindBuilding(map, d.BuildingId);
        if (owner == nullptr || !doorIds.insert(d.Id).second || FindDoor(d.Id) == nullptr || !MpOnFrontEdge(owner->Footprint, d.Position))
        {
            return false;
        }
        if (std::abs(d.Outward.X) + std::abs(d.Outward.Y) != 1.0)
        {
            return false;
        }
    }
    std::set<std::string_view> poiIds;
    for (const PointOfInterest& p : map.Pois)
    {
        if (p.Id.empty() || !poiIds.insert(p.Id).second || p.Label.empty() || p.LabelKey.empty())
        {
            return false;
        }
        if (!p.BuildingId.empty() && FindBuilding(map, p.BuildingId) == nullptr)
        {
            return false;
        }
    }
    return !map.Surfaces.empty() && !map.Buildings.empty() && map.BoundsMin.X < map.BoundsMax.X && map.BoundsMin.Y < map.BoundsMax.Y;
}

const Building* FindBuilding(const Map& map, std::string_view buildingId) noexcept
{
    for (const Building& b : map.Buildings)
    {
        if (b.Id == buildingId)
        {
            return &b;
        }
    }
    return nullptr;
}

const Building* FindBuildingAt(const Map& map, Point world) noexcept
{
    for (const Building& b : map.Buildings)
    {
        if (b.Footprint.size() != 4)
        {
            continue;
        }
        const double x0 = std::min({b.Footprint[0].X, b.Footprint[2].X});
        const double x1 = std::max({b.Footprint[0].X, b.Footprint[2].X});
        const double y0 = std::min({b.Footprint[0].Y, b.Footprint[2].Y});
        const double y1 = std::max({b.Footprint[0].Y, b.Footprint[2].Y});
        if (world.X >= x0 && world.X <= x1 && world.Y >= y0 && world.Y <= y1)
        {
            return &b;
        }
    }
    return nullptr;
}

const MapDoor* FindMapDoor(const Map& map, std::string_view doorId) noexcept
{
    for (const MapDoor& d : map.Doors)
    {
        if (d.Id == doorId)
        {
            return &d;
        }
    }
    return nullptr;
}

std::string_view BuildingKindId(BuildingKind kind) noexcept
{
    switch (kind)
    {
    case BuildingKind::Home: return "home";
    case BuildingKind::Shop: return "shop";
    case BuildingKind::School: return "school";
    case BuildingKind::Workshop: return "workshop";
    case BuildingKind::Sealed: return "sealed";
    case BuildingKind::Civic: return "civic";
    }
    return "sealed";
}

std::string_view PoiKindId(PoiKind kind) noexcept
{
    switch (kind)
    {
    case PoiKind::Home: return "home";
    case PoiKind::Shop: return "shop";
    case PoiKind::Workshop: return "workshop";
    case PoiKind::School: return "school";
    case PoiKind::Alley: return "alley";
    case PoiKind::LaneEnd: return "lane_end";
    case PoiKind::Civic: return "civic";
    }
    return "shop";
}

DoorStatus QueryDoorStatus(const MapDoor& door, const DoorState* live, int minuteOfDay) noexcept
{
    DoorStatus status;
    status.bLeafOpen = door.bInitiallyOpen;
    if (live != nullptr)
    {
        if (const std::optional<bool> open = QueryDoorOpen(*live, door.Id))
        {
            status.bLeafOpen = *open;
        }
    }
    const DoorDef* authored = FindDoor(door.Id);
    if (authored == nullptr)
    {
        status.bLeafOpen = false;
        status.Availability = DoorAvailability::Locked;
        return status;
    }
    const int minute = (minuteOfDay >= 0 && minuteOfDay < DoorMinutesPerDay) ? minuteOfDay : 0;
    switch (authored->Access)
    {
    case DoorAccess::Public:
        status.Availability = DoorAvailability::OpenToAll;
        return status;
    case DoorAccess::Private:
        status.Availability = DoorAvailability::PrivateKnock;
        return status;
    case DoorAccess::Locked:
        status.Availability = DoorAvailability::Locked;
        return status;
    case DoorAccess::BusinessHours:
        break;
    }

    // The window containing the minute, else the next to open (the first tomorrow when none is left today).
    const DoorOpeningWindow legacy[1] = {{authored->OpenMinute, authored->CloseMinute}};
    const std::span<const DoorOpeningWindow> windows = authored->OpeningWindows.empty() ? std::span<const DoorOpeningWindow>(legacy)
                                                                                         : authored->OpeningWindows;
    const DoorOpeningWindow* chosen = nullptr;
    for (const DoorOpeningWindow& w : windows)
    {
        if (w.OpenMinute <= minute && minute < w.CloseMinute)
        {
            chosen = &w;
            break;
        }
    }
    const bool open = chosen != nullptr;
    if (!open)
    {
        chosen = &windows.front();
        for (const DoorOpeningWindow& w : windows)
        {
            if (w.OpenMinute > minute)
            {
                chosen = &w;
                break;
            }
        }
    }
    status.Availability = open ? DoorAvailability::OpenToAll : DoorAvailability::ClosedForNow;
    status.WindowOpenMinute = chosen->OpenMinute;
    status.WindowCloseMinute = chosen->CloseMinute;
    status.MinutesUntilOpening = open ? -1 : DoorMinutesUntilOpening(*authored, minute);
    return status;
}
} // namespace PerfectWorld::LaneMap
