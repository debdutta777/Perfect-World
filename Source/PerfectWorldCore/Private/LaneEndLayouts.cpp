#include "LaneEndLayouts.h"

#include "LaneEndHallDetail.h"

#include <algorithm>
#include <vector>

namespace PerfectWorld::LaneEnds
{
namespace
{
namespace Li = PerfectWorld::LaneInterior;
namespace Ld = PerfectWorld::LaneInterior::Detail;

constexpr int LeGridCm = 5;
constexpr int LeSnapCm = 3; // an anchor or zone centre that is not on the grid still meets a cell inside +-3 cm

constexpr std::array<HallInfo, EndHallCount> LeHalls{{
    {EndHall::CommunityHall, "building.nabapara.west_end_block", "Community hall", "map.building.community_hall",
     "building.nabapara.west_end_block.opening.entrance", false, 4, WestLaneFaceXCm, -1, 1300, 300, 240, {HoursWindow{480, 1260}, HoursWindow{}}, 1},
    {EndHall::MarketHall, "building.nabapara.east_end_block", "Market hall", "map.building.market_hall",
     "building.nabapara.east_end_block.opening.entrance", true, 5, EastLaneFaceXCm, 1, 1300, 400, 250, {HoursWindow{360, 780}, HoursWindow{960, 1260}}, 2},
}};

Li::InteriorInput LeInput(const HallInfo& info)
{
    Li::InteriorInput in;
    in.BuildingId = std::string(info.BuildingId);
    // A public, undivided ground floor: SharedEntry is the nearest of the kit's variants and is only metadata here (never regenerated).
    in.Kind = Li::Variant::SharedEntry;
    in.Furniture = Li::FurnitureSet::None;
    in.WidthCm = HallWidthCm;
    in.DepthCm = HallDepthCm;
    in.FinishedFloorCm = FinishedFloorCm;
    in.StoreyHeightCm = StoreyHeightCm;
    in.EntranceCentreU = info.EntranceCentreU;
    return in;
}

int LeClampMinute(int minuteOfDay) noexcept
{
    return minuteOfDay >= 0 && minuteOfDay < 1440 ? minuteOfDay : 0;
}

/** Cells of a 5 cm grid over the whole footprint; a cell is blocked when a body of `radius` centred on it would touch a wall or a solid. */
struct LeGrid
{
    int Width = 0;
    int Height = 0;
    std::vector<std::uint8_t> Blocked;
    [[nodiscard]] std::size_t Index(int i, int j) const noexcept { return static_cast<std::size_t>(j) * static_cast<std::size_t>(Width) + static_cast<std::size_t>(i); }
};

LeGrid LeMakeGrid(const Li::InteriorLayout& l, int radius)
{
    LeGrid g;
    g.Width = l.WidthCm / LeGridCm + 1;
    g.Height = l.DepthCm / LeGridCm + 1;
    g.Blocked.assign(static_cast<std::size_t>(g.Width) * static_cast<std::size_t>(g.Height), 0);
    for (int j = 0; j < g.Height; ++j)
    {
        for (int i = 0; i < g.Width; ++i)
        {
            const int u = i * LeGridCm;
            const int v = j * LeGridCm;
            const bool inside = u >= l.Interior.U0 + radius && u <= l.Interior.U1 - radius && v >= l.Interior.V0 + radius && v <= l.Interior.V1 - radius;
            g.Blocked[g.Index(i, j)] = inside ? 0 : 1;
        }
    }
    for (const Li::Furniture& f : l.Pieces)
    {
        if (!f.Solid)
        {
            continue;
        }
        const int i0 = std::max(0, (f.Footprint.U0 - radius) / LeGridCm - 1);
        const int i1 = std::min(g.Width - 1, (f.Footprint.U1 + radius) / LeGridCm + 1);
        const int j0 = std::max(0, (f.Footprint.V0 - radius) / LeGridCm - 1);
        const int j1 = std::min(g.Height - 1, (f.Footprint.V1 + radius) / LeGridCm + 1);
        for (int j = j0; j <= j1; ++j)
        {
            for (int i = i0; i <= i1; ++i)
            {
                const int u = i * LeGridCm;
                const int v = j * LeGridCm;
                if (u > f.Footprint.U0 - radius && u < f.Footprint.U1 + radius && v > f.Footprint.V0 - radius && v < f.Footprint.V1 + radius)
                {
                    g.Blocked[g.Index(i, j)] = 1;
                }
            }
        }
    }
    return g;
}

/** Flood fill (4-connected) from a point; returns the reached cells. Empty when the start cell is blocked. */
std::vector<std::uint8_t> LeReach(const LeGrid& g, int u, int v)
{
    std::vector<std::uint8_t> seen(g.Blocked.size(), 0);
    const int si = (u + LeGridCm / 2) / LeGridCm;
    const int sj = (v + LeGridCm / 2) / LeGridCm;
    if (si < 0 || sj < 0 || si >= g.Width || sj >= g.Height || g.Blocked[g.Index(si, sj)] != 0)
    {
        return seen;
    }
    std::vector<std::pair<int, int>> open;
    open.emplace_back(si, sj);
    seen[g.Index(si, sj)] = 1;
    while (!open.empty())
    {
        const auto [i, j] = open.back();
        open.pop_back();
        constexpr int di[4] = {1, -1, 0, 0};
        constexpr int dj[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; ++k)
        {
            const int ni = i + di[k];
            const int nj = j + dj[k];
            if (ni < 0 || nj < 0 || ni >= g.Width || nj >= g.Height || seen[g.Index(ni, nj)] != 0 || g.Blocked[g.Index(ni, nj)] != 0)
            {
                continue;
            }
            seen[g.Index(ni, nj)] = 1;
            open.emplace_back(ni, nj);
        }
    }
    return seen;
}

/** True when some cell whose centre lies in the rectangle (edges inclusive) was reached. */
bool LeReachedIn(const LeGrid& g, const std::vector<std::uint8_t>& seen, const Li::Rect& r)
{
    for (int j = (r.V0 + LeGridCm - 1) / LeGridCm; j * LeGridCm <= r.V1 && j < g.Height; ++j)
    {
        for (int i = (r.U0 + LeGridCm - 1) / LeGridCm; i * LeGridCm <= r.U1 && i < g.Width; ++i)
        {
            if (i >= 0 && j >= 0 && seen[g.Index(i, j)] != 0)
            {
                return true;
            }
        }
    }
    return false;
}

/** Where a person has to stand to use an anchor: the anchor point itself, or for a seat anywhere in its approach zone. False when it has none. */
bool LeUseArea(const Li::InteriorLayout& l, const Li::Anchor& a, Li::Rect& area)
{
    if (a.Kind == Li::AnchorKind::Stand)
    {
        area = Li::Rect{a.U - LeSnapCm, a.U + LeSnapCm, a.V - LeSnapCm, a.V + LeSnapCm};
        return true;
    }
    for (const Li::Zone& z : l.Zones)
    {
        if (z.Kind == Li::ZoneKind::SeatApproach && z.OwnerId == a.FurnitureId)
        {
            area = z.Area;
            return true;
        }
    }
    return false;
}

/** Everything the hall layouts share: the shell (four walls, the archway, windows), the entry aisle, the threshold and the hall_entry stand. */
Li::Refusal LeBuildShell(Ld::Builder& b, const HallInfo& info, const Li::Room& hall)
{
    const int width = b.W;
    const int depth = b.D;
    const Li::Rect inner = b.Inner;
    const auto windows = Furnish::HallWindows(info.Hall);

    Ld::Run frontWall{"front", true, 0, Li::WallThicknessCm, 0, width, {}};
    Ld::Run backWall{"back", true, depth - Li::WallThicknessCm, depth, 0, width, {}};
    Ld::Run sideA{"side_a", false, 0, Li::WallThicknessCm, inner.V0, inner.V1, {}};
    Ld::Run sideB{"side_b", false, width - Li::WallThicknessCm, width, inner.V0, inner.V1, {}};

    const int half = info.EntranceWidthCm / 2;
    const std::string entranceId = b.Key("opening", "entrance");
    b.AddOpening(frontWall, "entrance", Li::OpeningKind::Doorway, info.EntranceCentreU - half, info.EntranceCentreU + half, 0, info.EntranceHeightCm,
                 Ld::OutsideId, hall.Id);
    b.AddConnection("entrance", Ld::OutsideId, hall.Id, entranceId, info.EntranceWidthCm);
    int n = 0;
    for (const Furnish::WindowSpan& w : windows.first)
    {
        b.AddOpening(frontWall, "window_front_" + std::to_string(n++), Li::OpeningKind::Window, w.U0, w.U1, Li::WindowSillCm, Li::WindowHeadCm,
                     Ld::OutsideId, hall.Id);
    }
    n = 0;
    for (const Furnish::WindowSpan& w : windows.second)
    {
        b.AddOpening(backWall, "window_back_" + std::to_string(n++), Li::OpeningKind::Window, w.U0, w.U1, Li::WindowSillCm, Li::WindowHeadCm,
                     hall.Id, Ld::OutsideId);
    }
    b.EndRun(frontWall);
    b.EndRun(backWall);
    b.EndRun(sideA);
    b.EndRun(sideB);

    // The full-width aisle behind the archway, then the person's two standing places: outside the archway and just inside it.
    b.AddZone(Li::ZoneKind::Circulation, hall.Id, Li::Rect{inner.U0, inner.U1, inner.V0, inner.V0 + Li::CirculationCm}, entranceId, "entry_aisle");
    Li::Anchor threshold;
    threshold.Id = b.Key("anchor", "threshold");
    threshold.Kind = Li::AnchorKind::Threshold;
    threshold.RoomId = std::string(Ld::OutsideId);
    threshold.U = info.EntranceCentreU;
    threshold.V = -60;
    threshold.FaceV = 1;
    b.L.Anchors.push_back(std::move(threshold));
    Li::Anchor entry;
    entry.Id = b.Key("anchor", "hall_entry");
    entry.Kind = Li::AnchorKind::Stand;
    entry.RoomId = hall.Id;
    entry.U = info.EntranceCentreU;
    entry.V = inner.V0 + Li::CirculationCm / 2;
    entry.FaceV = 1;
    b.L.Anchors.push_back(std::move(entry));
    return Li::Refusal::None;
}
} // namespace

namespace Furnish
{
std::pair<std::vector<WindowSpan>, std::vector<WindowSpan>> HallWindows(EndHall hall)
{
    // Front windows stand only where the first-row buildings leave the lane-facing wall exposed (|Y| < 400, so U 900..1700); back windows
    // keep clear of the stage curtain (community) and the wall counters (market).
    if (hall == EndHall::CommunityHall)
    {
        return {{{930, 1070}, {1530, 1670}}, {{350, 490}, {690, 830}, {1770, 1910}, {2110, 2250}}};
    }
    return {{{940, 1060}, {1540, 1660}}, {{575, 715}, {1230, 1370}, {1885, 2025}}};
}
} // namespace Furnish

const HallInfo& GetHallInfo(EndHall hall) noexcept
{
    return LeHalls[static_cast<std::size_t>(hall)];
}

const HallInfo* FindHallInfo(std::string_view buildingId) noexcept
{
    for (const HallInfo& info : LeHalls)
    {
        if (info.BuildingId == buildingId)
        {
            return &info;
        }
    }
    return nullptr;
}

std::span<const HoursWindow> HallHours(EndHall hall) noexcept
{
    const HallInfo& info = GetHallInfo(hall);
    return std::span<const HoursWindow>(info.Hours.data(), static_cast<std::size_t>(info.HoursCount));
}

WorldPoint HallWorldPoint(EndHall hall, int u, int v) noexcept
{
    const HallInfo& info = GetHallInfo(hall);
    return WorldPoint{info.LaneFaceXCm + info.FaceXSign * v, OriginYCm + u};
}

WorldPoint HallEntrancePoint(EndHall hall) noexcept
{
    return HallWorldPoint(hall, GetHallInfo(hall).EntranceCentreU, 0);
}

std::string PostedHoursText(EndHall hall)
{
    std::string text;
    for (const HoursWindow& w : HallHours(hall))
    {
        const auto clock = [](int minute) {
            std::string s;
            s += static_cast<char>('0' + minute / 600);
            s += static_cast<char>('0' + (minute / 60) % 10);
            s += ':';
            s += static_cast<char>('0' + (minute % 60) / 10);
            s += static_cast<char>('0' + minute % 10);
            return s;
        };
        text += (text.empty() ? "" : " and ") + clock(w.OpenMinute) + "-" + clock(w.CloseMinute);
    }
    return text;
}

bool IsOpenAt(EndHall hall, int minuteOfDay) noexcept
{
    const int minute = LeClampMinute(minuteOfDay);
    for (const HoursWindow& w : HallHours(hall))
    {
        if (minute >= w.OpenMinute && minute < w.CloseMinute)
        {
            return true;
        }
    }
    return false;
}

int MinutesUntilOpening(EndHall hall, int minuteOfDay) noexcept
{
    if (IsOpenAt(hall, minuteOfDay))
    {
        return -1;
    }
    const int minute = LeClampMinute(minuteOfDay);
    int best = 1440;
    for (const HoursWindow& w : HallHours(hall))
    {
        const int wait = w.OpenMinute > minute ? w.OpenMinute - minute : w.OpenMinute + 1440 - minute;
        best = std::min(best, wait);
    }
    return best;
}

const Li::Opening* FindEndEntrance(const Li::InteriorLayout& layout)
{
    for (const Li::Opening& o : layout.Openings)
    {
        if (o.RoomA == Ld::OutsideId && o.Kind != Li::OpeningKind::Window)
        {
            return &o;
        }
    }
    return nullptr;
}

Li::Refusal ValidateEndLayout(EndHall hall, const Li::InteriorLayout& layout)
{
    const HallInfo& info = GetHallInfo(hall);
    if (const Li::Refusal generic = Li::ValidateInteriorLayout(layout); generic != Li::Refusal::None)
    {
        return generic;
    }
    if (layout.BuildingId != info.BuildingId || layout.WidthCm != HallWidthCm || layout.DepthCm != HallDepthCm || layout.FinishedFloorCm != FinishedFloorCm ||
        layout.StoreyHeightCm != StoreyHeightCm)
    {
        return Li::Refusal::InvalidBuildingId;
    }

    // Exactly one public archway, on the lane-facing wall, inside the exposed part of it, wide and high enough, with no leaf and no sweep.
    int archways = 0;
    for (const Li::Opening& o : layout.Openings)
    {
        if (o.Kind == Li::OpeningKind::Door || !o.DoorId.empty())
        {
            return Li::Refusal::UnitDoorIdInvalid;
        }
        archways += (o.RoomA == Ld::OutsideId && o.Kind != Li::OpeningKind::Window) ? 1 : 0;
    }
    const Li::Opening* entrance = FindEndEntrance(layout);
    if (archways != 1 || entrance == nullptr || entrance->Id != info.EntranceId || entrance->Plan.V0 != 0 || entrance->Plan.V1 != Li::WallThicknessCm)
    {
        return Li::Refusal::EntranceOffFacade;
    }
    const int laneAxisU = -OriginYCm;                           // U 1300: the road's centre line
    const int exposed0 = laneAxisU - ExposedFrontHalfWidthCm;   // U 900
    const int exposed1 = laneAxisU + ExposedFrontHalfWidthCm;   // U 1700
    if (entrance->Plan.U0 < exposed0 || entrance->Plan.U1 > exposed1 || entrance->Plan.Width() < MinArchwayWidthCm || entrance->W0 != 0 ||
        entrance->W1 < Li::DoorOpeningHeightCm || entrance->ClearWidthCm < MinArchwayWidthCm - 8)
    {
        return Li::Refusal::DoorwayTooSmall;
    }
    for (const Li::Zone& z : layout.Zones)
    {
        if (z.Kind == Li::ZoneKind::DoorSweep)
        {
            return Li::Refusal::SweepBlocked;
        }
    }
    // The aisle behind the archway is a full-width 120 cm circulation zone that covers it.
    const bool entryAisle = std::any_of(layout.Zones.begin(), layout.Zones.end(), [&](const Li::Zone& z) {
        return z.Kind == Li::ZoneKind::Circulation && z.Area.V0 == layout.Interior.V0 && z.Area.Depth() >= Li::CirculationCm &&
               z.Area.U0 <= entrance->Plan.U0 && z.Area.U1 >= entrance->Plan.U1;
    });
    if (!entryAisle)
    {
        return Li::Refusal::CirculationBlocked;
    }
    // Physical walking: from just inside the archway, a 34 cm capsule reaches every standing and sitting place; a 60 cm body
    // reaches the middle of every circulation zone, so each aisle is a true 120 cm channel, joined to the archway.
    const int startU = info.EntranceCentreU;
    const int startV = layout.Interior.V0 + Li::CirculationCm / 2;
    const LeGrid body = LeMakeGrid(layout, CapsuleRadiusCm);
    const std::vector<std::uint8_t> bodyReach = LeReach(body, startU, startV);
    for (const Li::Anchor& a : layout.Anchors)
    {
        Li::Rect area;
        if (a.Kind == Li::AnchorKind::Threshold)
        {
            continue;
        }
        if (!LeUseArea(layout, a, area) || !LeReachedIn(body, bodyReach, area))
        {
            return Li::Refusal::CirculationBlocked;
        }
    }
    const LeGrid aisle = LeMakeGrid(layout, AisleRadiusCm);
    const std::vector<std::uint8_t> aisleReach = LeReach(aisle, startU, startV);
    for (const Li::Zone& z : layout.Zones)
    {
        const int cu = (z.Area.U0 + z.Area.U1) / 2;
        const int cv = (z.Area.V0 + z.Area.V1) / 2;
        if (z.Kind == Li::ZoneKind::Circulation && !LeReachedIn(aisle, aisleReach, Li::Rect{cu - LeSnapCm, cu + LeSnapCm, cv - LeSnapCm, cv + LeSnapCm}))
        {
            return Li::Refusal::CirculationBlocked;
        }
    }
    return Li::Refusal::None;
}

Li::InteriorLayout GenerateEndLayout(EndHall hall)
{
    const HallInfo& info = GetHallInfo(hall);
    const Li::InteriorInput in = LeInput(info);
    Ld::Builder builder(in);
    const bool community = hall == EndHall::CommunityHall;
    const Li::Room room = builder.AddRoom("hall", community ? Li::RoomKind::Living : Li::RoomKind::Sales, builder.Inner,
                                          community ? "floor_tile" : "concrete", community ? "plaster_pale" : "plaster_warm");
    Li::Refusal built = LeBuildShell(builder, info, room);
    if (built == Li::Refusal::None)
    {
        const bool furnished = community ? Furnish::FurnishCommunityHall(builder, room) : Furnish::FurnishMarketHall(builder, room);
        built = furnished ? Li::Refusal::None : Li::Refusal::FurnitureDoesNotFit;
    }
    Li::InteriorLayout refused;
    refused.BuildingId = in.BuildingId;
    refused.Kind = in.Kind;
    refused.WidthCm = in.WidthCm;
    refused.DepthCm = in.DepthCm;
    if (built != Li::Refusal::None)
    {
        refused.Status = built;
        return refused;
    }
    if (const Li::Refusal checked = ValidateEndLayout(hall, builder.L); checked != Li::Refusal::None)
    {
        refused.Status = checked;
        return refused;
    }
    return std::move(builder.L);
}
} // namespace PerfectWorld::LaneEnds
