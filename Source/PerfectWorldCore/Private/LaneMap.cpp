#include "LaneMap.h"

#include "LaneFrontageCatalogue.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace PerfectWorld::LaneMap
{
namespace
{
using LaneInterior::BuiltFootprint;
using LaneInterior::FrontageEntry;
using LaneInterior::FurnitureSet;
using LaneInterior::Variant;

/** A building's authored display name. Names for the people's shops come from the lane documents; an unnamed trade is named for what it is. */
struct MpName
{
    std::string_view Id;
    std::string_view Label;
};

constexpr std::array<MpName, 19> MpNames{{
    {"building.nabapara.care_home", "Children's home"},
    {"building.nabapara.opening_home", "Family flat"},
    {"building.nabapara.stationery", "Stationery shop"},
    {"building.nabapara.shanti_chai", "Shanti Chai Corner"},
    {"building.nabapara.caretaker_flat", "Caretaker's flat"},
    {"building.nabapara.tall_flats", "Tall flats"},
    {"building.nabapara.tea_row_house", "Row house"},
    {"building.nabapara.repair_workshop", "Repair workshop"},
    {"building.nabapara.hardware_store", "Hardware store"},
    {"building.nabapara.old_house", "Old house"},
    {"building.nabapara.market_stalls_a", "Market stalls"},
    {"building.nabapara.market_stalls_b", "Market stalls"},
    {"building.nabapara.end_shop", "End shop"},
    {"building.nabapara.cloth_merchant", "Cloth merchant"},
    {"building.nabapara.narrow_shop", "Narrow shop"},
    {"building.nabapara.tailor_shop", "Tailor"},
    {"building.nabapara.alley_house", "Alley house"},
    {"building.nabapara.stair_flats", "Stair flats"},
    {"building.nabapara.corner_stalls", "Corner stalls"},
}};

/** The four ground floors built by their own code: the entrance centre along the frontage (each file's published constant) and the front door id. */
struct MpBuiltDoor
{
    std::string_view BuildingId;
    BuildingKind Kind;
    int EntranceU;
    std::string_view DoorId;
};

constexpr std::array<MpBuiltDoor, 4> MpBuiltDoors{{
    {"building.nabapara.care_home", BuildingKind::Home, 235, "door.nabapara.care_front"},               // CareInterior FrontDoorU
    {"building.nabapara.opening_home", BuildingKind::Home, 700, "door.nabapara.home_front"},            // LaneInteriors HomeFrontDoorU
    {"building.nabapara.stationery", BuildingKind::Shop, 250, "door.nabapara.stationery_front"},        // StationeryEntranceU
    {"building.nabapara.shanti_chai", BuildingKind::Shop, 350, "door.nabapara.shanti_chai_shutter"},    // TeaShop DoorCentreU
}};

std::string_view MpLabelFor(std::string_view id) noexcept
{
    for (const MpName& name : MpNames)
    {
        if (name.Id == id)
        {
            return name.Label;
        }
    }
    return {};
}

/** "building.nabapara.care_home" -> "map.building.care_home"; empty when the id is not a lane building id. */
std::string MpLabelKey(std::string_view id)
{
    constexpr std::string_view prefix = "building.nabapara.";
    if (!id.starts_with(prefix) || id.size() == prefix.size())
    {
        return {};
    }
    return "map.building." + std::string(id.substr(prefix.size()));
}

Polygon MpRect(double x0, double y0, double x1, double y1)
{
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

Point MpCentre(const Polygon& polygon) noexcept
{
    double x = 0.0;
    double y = 0.0;
    for (const Point& p : polygon)
    {
        x += p.X;
        y += p.Y;
    }
    const double n = polygon.empty() ? 1.0 : static_cast<double>(polygon.size());
    return {x / n, y / n};
}

BuildingKind MpKindOf(const FrontageEntry& entry) noexcept
{
    if (entry.Kind == Variant::ShopWithStockroom)
    {
        return entry.Furniture == FurnitureSet::Workshop ? BuildingKind::Workshop : BuildingKind::Shop;
    }
    return BuildingKind::Home;
}

PoiKind MpPoiKindOf(BuildingKind kind) noexcept
{
    switch (kind)
    {
    case BuildingKind::Home: return PoiKind::Home;
    case BuildingKind::School: return PoiKind::School;
    case BuildingKind::Workshop: return PoiKind::Workshop;
    default: return PoiKind::Shop;
    }
}

void MpAddDoor(Map& map, const Building& building, std::string_view doorId, Point position, Point outward)
{
    const DoorDef* authored = FindDoor(doorId);
    if (authored == nullptr)
    {
        return;
    }
    MapDoor door;
    door.Id = std::string(doorId);
    door.BuildingId = building.Id;
    door.Position = position;
    door.Outward = outward;
    door.Access = authored->Access;
    door.bInitiallyOpen = authored->bInitiallyOpen;
    map.Doors.push_back(std::move(door));
}

void MpAddPoi(Map& map, const Building& building)
{
    if (building.Label.empty() || !building.bHasEntrance)
    {
        return;
    }
    PointOfInterest poi;
    poi.Id = "poi." + building.Id;
    poi.Label = building.Label;
    poi.LabelKey = building.LabelKey;
    poi.Kind = MpPoiKindOf(building.Kind);
    poi.Position = building.Entrance;
    poi.BuildingId = building.Id;
    map.Pois.push_back(std::move(poi));
}

void MpAddSurfaces(Map& map, const std::vector<Frontage>& frontages)
{
    map.Surfaces.push_back({SurfaceKind::Road, MpRect(LaneX0Cm, -RoadHalfWidthCm, LaneX1Cm, RoadHalfWidthCm)});
    map.Surfaces.push_back({SurfaceKind::Alley, MpRect(AlleyPlusAXCm, RoadHalfWidthCm, AlleyPlusBXCm, AlleyReachCm)});
    map.Surfaces.push_back({SurfaceKind::Alley, MpRect(AlleyMinusAXCm, -AlleyReachCm, AlleyMinusBXCm, -RoadHalfWidthCm)});
    // The footpath in front of each near building runs from the road edge to the building's front line (BuildFull).
    for (const Frontage& f : frontages)
    {
        const double edge = RoadHalfWidthCm;
        const double front = f.FrontAbsYCm();
        map.Surfaces.push_back({SurfaceKind::Footpath, f.Side > 0 ? MpRect(f.X0Cm, edge, f.X1Cm, front) : MpRect(f.X0Cm, -front, f.X1Cm, -edge)});
    }
}

/** Resolves one frontage into a building: built by its own code, a kit row, or sealed. Pins must match exactly, as the Unreal side requires. */
Building MpMakeBuilding(Map& map, const Frontage& f)
{
    Building b;
    b.Side = f.Side;
    b.Storeys = f.Storeys;
    const double frontY = static_cast<double>(f.Side) * f.FrontAbsYCm();
    const double backY = static_cast<double>(f.Side) * (f.FrontAbsYCm() + f.DepthCm);
    b.Footprint = MpRect(f.X0Cm, std::min(frontY, backY), f.X1Cm, std::max(frontY, backY));
    b.Centre = MpCentre(b.Footprint);

    const BuiltFootprint* built = nullptr;
    for (const BuiltFootprint& row : LaneInterior::BuiltFootprints())
    {
        if (row.Side == f.Side && row.RowIndex == f.Index && row.X0Cm == f.X0Cm && row.X1Cm == f.X1Cm)
        {
            built = &row;
        }
    }
    int entranceU = -1;
    std::string_view doorId;
    if (built != nullptr)
    {
        b.Id = std::string(built->BuildingId);
        for (const MpBuiltDoor& door : MpBuiltDoors)
        {
            if (door.BuildingId == built->BuildingId)
            {
                b.Kind = door.Kind;
                entranceU = door.EntranceU;
                doorId = door.DoorId;
            }
        }
    }
    else if (const FrontageEntry* kit = LaneInterior::FindFrontageByFootprint(f.Side, f.X0Cm, f.FrontAbsYCm(), f.WidthCm, f.DepthCm,
                                                                           f.SetbackCm, f.Storeys, f.Seed))
    {
        b.Id = std::string(kit->BuildingId);
        b.Kind = MpKindOf(*kit);
        entranceU = kit->EntranceCentreU;
        doorId = kit->EntranceDoorId;
    }
    else
    {
        b.Id = std::string("building.nabapara.sealed_") + (f.Side < 0 ? "minus_" : "plus_") + std::to_string(f.Index);
        b.Kind = BuildingKind::Sealed;
    }

    b.Label = std::string(MpLabelFor(b.Id));
    b.LabelKey = b.Label.empty() ? std::string() : MpLabelKey(b.Id);
    if (entranceU >= 0)
    {
        b.bEnterable = true;
        b.bHasEntrance = true;
        b.Entrance = {static_cast<double>(f.X0Cm + entranceU), frontY};
        if (!doorId.empty())
        {
            MpAddDoor(map, b, doorId, b.Entrance, {0.0, -static_cast<double>(f.Side)});
        }
    }
    return b;
}

/** The two masses that close the lane: sealed, 2600 wide and 900 deep (BuildClosingRows). */
Building MpMakeEndMass(bool east)
{
    Building b;
    b.Side = 0;
    b.Kind = BuildingKind::Sealed;
    b.Id = east ? "building.nabapara.east_end_block" : "building.nabapara.west_end_block";
    b.Storeys = east ? 5 : 4;
    b.Footprint = east ? MpRect(LaneX1Cm, -1300.0, LaneX1Cm + 900.0, 1300.0) : MpRect(LaneX0Cm - 900.0, -1300.0, LaneX0Cm, 1300.0);
    b.Centre = MpCentre(b.Footprint);
    return b;
}

Building MpMakeExtra(Map& map, const ExtraBuilding& extra)
{
    Building b;
    b.Id = std::string(extra.Id);
    b.Label = std::string(extra.Label);
    b.LabelKey = std::string(extra.LabelKey);
    b.Kind = extra.Kind;
    b.Storeys = extra.Storeys;
    b.Footprint = MpRect(std::min(extra.X0Cm, extra.X1Cm), std::min(extra.Y0Cm, extra.Y1Cm), std::max(extra.X0Cm, extra.X1Cm),
                         std::max(extra.Y0Cm, extra.Y1Cm));
    b.Centre = MpCentre(b.Footprint);
    b.Side = b.Centre.Y < 0.0 ? -1 : 1;
    if (extra.HasEntrance)
    {
        b.bEnterable = true;
        b.bHasEntrance = true;
        b.Entrance = {extra.EntranceXCm, extra.EntranceYCm};
        if (!extra.DoorId.empty())
        {
            MpAddDoor(map, b, extra.DoorId, b.Entrance, {0.0, -static_cast<double>(b.Side)});
        }
    }
    return b;
}

void MpAddLandmarks(Map& map)
{
    const auto add = [&map](std::string id, std::string label, std::string key, PoiKind kind, Point at) {
        PointOfInterest poi;
        poi.Id = std::move(id);
        poi.Label = std::move(label);
        poi.LabelKey = std::move(key);
        poi.Kind = kind;
        poi.Position = at;
        map.Pois.push_back(std::move(poi));
    };
    add("poi.alley.plus", "Side alley", "map.poi.alley", PoiKind::Alley, {(AlleyPlusAXCm + AlleyPlusBXCm) / 2.0, 900.0});
    add("poi.alley.minus", "Side alley", "map.poi.alley", PoiKind::Alley, {(AlleyMinusAXCm + AlleyMinusBXCm) / 2.0, -900.0});
    add("poi.lane_end.west", "West end of the lane", "map.poi.lane_end_west", PoiKind::LaneEnd, {LaneX0Cm, 0.0});
    add("poi.lane_end.east", "East end of the lane", "map.poi.lane_end_east", PoiKind::LaneEnd, {LaneX1Cm, 0.0});
}

void MpFitBounds(Map& map)
{
    bool first = true;
    const auto grow = [&map, &first](const Polygon& polygon) {
        for (const Point& p : polygon)
        {
            if (first)
            {
                map.BoundsMin = p;
                map.BoundsMax = p;
                first = false;
                continue;
            }
            map.BoundsMin.X = std::min(map.BoundsMin.X, p.X);
            map.BoundsMin.Y = std::min(map.BoundsMin.Y, p.Y);
            map.BoundsMax.X = std::max(map.BoundsMax.X, p.X);
            map.BoundsMax.Y = std::max(map.BoundsMax.Y, p.Y);
        }
    };
    for (const Surface& s : map.Surfaces)
    {
        grow(s.Outline);
    }
    for (const Building& b : map.Buildings)
    {
        grow(b.Footprint);
    }
}

} // namespace

std::uint32_t LaneMapHash(std::int32_t a, std::int32_t b, std::int32_t c) noexcept
{
    std::uint32_t h = static_cast<std::uint32_t>(a) * 0x9E3779B1u ^ static_cast<std::uint32_t>(b) * 0x85EBCA77u ^ static_cast<std::uint32_t>(c) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

std::vector<Frontage> EnumerateFrontages()
{
    std::vector<Frontage> rows;
    for (const int side : {-1, 1})
    {
        const int alleyA = side < 0 ? AlleyMinusAXCm : AlleyPlusAXCm;
        const int alleyB = side < 0 ? AlleyMinusBXCm : AlleyPlusBXCm;
        int cursor = LaneX0Cm;
        int index = 0;
        while (cursor < LaneX1Cm - 250)
        {
            if (cursor >= alleyA && cursor < alleyB)
            {
                cursor = alleyB;
                continue;
            }
            const std::uint32_t h = LaneMapHash(side * 131 + 7, index, 11);
            int width = 600 + 100 * static_cast<int>(h % 6u);
            if (cursor < alleyA && cursor + width > alleyA)
            {
                width = alleyA - cursor;
            }
            if (LaneX1Cm - (cursor + width) < 500)
            {
                width = LaneX1Cm - cursor;
            }
            if (width < 300)
            {
                cursor += width; // too narrow to be a building: a gap, and the index does not advance
                continue;
            }
            const std::uint32_t kind = (h >> 12) % 4u;
            Frontage row;
            row.Side = side;
            row.Index = index;
            row.X0Cm = cursor;
            row.X1Cm = cursor + width;
            row.WidthCm = width;
            row.DepthCm = 700 + 100 * static_cast<int>((h >> 16) % 3u);
            row.SetbackCm = kind == 0u ? 120 : (kind == 1u ? 60 : 0);
            row.Storeys = 3 + static_cast<int>((h >> 8) % 3u);
            row.Seed = h;
            rows.push_back(row);
            cursor += width + (((h >> 24) % 7u == 0u) ? 100 : 0);
            ++index;
        }
    }
    return rows;
}

Map BuildLaneMap(const MapOptions& options)
{
    Map map;
    const std::vector<Frontage> frontages = EnumerateFrontages();
    MpAddSurfaces(map, frontages);
    for (const Frontage& f : frontages)
    {
        map.Buildings.push_back(MpMakeBuilding(map, f));
    }
    map.Buildings.push_back(MpMakeEndMass(false));
    map.Buildings.push_back(MpMakeEndMass(true));
    for (const ExtraBuilding& extra : options.Extra)
    {
        map.Buildings.push_back(MpMakeExtra(map, extra));
    }
    for (const Building& b : map.Buildings)
    {
        MpAddPoi(map, b);
    }
    MpAddLandmarks(map);
    MpFitBounds(map);
    return map;
}

} // namespace PerfectWorld::LaneMap
