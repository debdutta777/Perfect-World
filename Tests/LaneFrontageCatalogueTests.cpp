// LL-03 submission 2: the kit-built frontage catalogue (LaneFrontageCatalogue.h). Proves, portably, that every pinned row
// matches what the lane generator produces (a test-local port of BuildFirstRows, checked against the documented stationery
// hash), that no row touches a ground floor built by its own code, and that every row generates a valid, furnished layout with
// 90 x 210 doorways and 120 cm circulation, in both a plain and a perturbed form.

#include "LaneFrontageCatalogue.h"
#include "Doors.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace PerfectWorld::LaneInterior;

namespace
{
int F5Assertions = 0;
void F5Expect(bool condition, const std::string& message)
{
    ++F5Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

// ---- A test-local port of PerfectWorldLaneBuildings.cpp BuildFirstRows (uint32 arithmetic, exact). ----
std::uint32_t F5FcHash(std::int32_t a, std::int32_t b, std::int32_t c)
{
    std::uint32_t h = static_cast<std::uint32_t>(a) * 0x9E3779B1u ^ static_cast<std::uint32_t>(b) * 0x85EBCA77u ^ static_cast<std::uint32_t>(c) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

struct F5FcRow
{
    int Side = 0;
    int Index = 0;
    int X0 = 0;
    int X1 = 0;
    int Width = 0;
    int Depth = 0;
    int Setback = 0;
    int Storeys = 0;
    std::uint32_t Seed = 0;
};

std::vector<F5FcRow> F5FcEnumerate()
{
    constexpr int laneX0 = -2400;
    constexpr int laneX1 = 6400;
    std::vector<F5FcRow> rows;
    for (const int side : {-1, 1})
    {
        const int alleyA = side < 0 ? 4300 : 2800;
        const int alleyB = side < 0 ? 4900 : 3400;
        int cursor = laneX0;
        int index = 0;
        while (cursor < laneX1 - 250)
        {
            if (cursor >= alleyA && cursor < alleyB)
            {
                cursor = alleyB;
                continue;
            }
            const std::uint32_t h = F5FcHash(side * 131 + 7, index, 11);
            int width = 600 + 100 * static_cast<int>(h % 6u);
            if (cursor < alleyA && cursor + width > alleyA) width = alleyA - cursor;
            if (laneX1 - (cursor + width) < 500) width = laneX1 - cursor;
            if (width < 300)
            {
                cursor += width;
                continue;
            }
            const std::uint32_t kind = (h >> 12) % 4u;
            F5FcRow row;
            row.Side = side;
            row.Index = index;
            row.X0 = cursor;
            row.X1 = cursor + width;
            row.Width = width;
            row.Depth = 700 + 100 * static_cast<int>((h >> 16) % 3u);
            row.Setback = kind == 0u ? 120 : (kind == 1u ? 60 : 0);
            row.Storeys = 3 + static_cast<int>((h >> 8) % 3u);
            row.Seed = h;
            rows.push_back(row);
            cursor += width + (((h >> 24) % 7u == 0u) ? 100 : 0);
            ++index;
        }
    }
    return rows;
}

const F5FcRow* F5FcFind(const std::vector<F5FcRow>& rows, int side, int index)
{
    for (const F5FcRow& r : rows)
        if (r.Side == side && r.Index == index) return &r;
    return nullptr;
}

bool F5Solid(const InteriorLayout& l, const Rect& r)
{
    return std::any_of(l.Pieces.begin(), l.Pieces.end(), [&](const Furniture& f) { return f.Solid && f.Footprint.Overlaps(r); });
}

void F5TestGenerator()
{
    F5Expect(F5FcHash(138, 2, 11) == 2673104671u, "the port reproduces the documented stationery seed");
    const std::vector<F5FcRow> rows = F5FcEnumerate();
    F5Expect(rows.size() == 19, "the generator yields nineteen side frontages (10 on -Y, 9 on +Y)");
    F5Expect(std::count_if(rows.begin(), rows.end(), [](const F5FcRow& r) { return r.Side < 0; }) == 10, "ten on the -Y side");

    for (const BuiltFootprint& b : BuiltFootprints())
    {
        const F5FcRow* r = F5FcFind(rows, b.Side, b.RowIndex);
        F5Expect(r != nullptr && r->X0 == b.X0Cm && r->X1 == b.X1Cm, std::string("built footprint matches the generator: ") + std::string(b.BuildingId));
    }
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const F5FcRow* r = F5FcFind(rows, e.Side, e.RowIndex);
        const std::string id(e.BuildingId);
        F5Expect(r != nullptr, "row exists: " + id);
        F5Expect(r->X0 == e.OriginXCm && r->Width == e.WidthCm && r->Depth == e.DepthCm && r->Setback == e.SetbackCm && r->Storeys == e.Storeys &&
                   r->Seed == e.Seed,
               "pinned footprint equals the generated one: " + id);
        F5Expect(e.FrontAbsYCm == 400 + e.SetbackCm, "front line is 400 plus the setback: " + id);
        F5Expect(e.Side == -1 || e.Side == 1, "side: " + id);
    }
}

void F5TestCatalogueShape()
{
    std::set<std::string> ids;
    std::set<std::string> planned;
    int sides[2] = {0, 0};
    int compact = 0;
    int shared = 0;
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const std::string id(e.BuildingId);
        F5Expect(id.rfind("building.nabapara.", 0) == 0 && id.size() > 18, "id namespace: " + id);
        F5Expect(std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || c == '.' || c == '_' || (c >= '0' && c <= '9'); }),
               "id characters: " + id);
        F5Expect(ids.insert(id).second, "ids are unique: " + id);
        ++sides[e.Side > 0 ? 1 : 0];
        compact += e.Kind == Variant::CompactResidence ? 1 : 0;
        shared += e.Kind == Variant::SharedEntry ? 1 : 0;
        F5Expect(FindFrontageById(e.BuildingId) == &e, "lookup by id: " + id);

        // Planned ids may precede their roster migration; only hung leaves require an authored definition.
        if (!e.EntranceDoorId.empty())
        {
            const auto doors = PerfectWorld::AuthoredDoors();
            F5Expect(std::any_of(doors.begin(), doors.end(), [&](const auto& d) { return d.Id == e.EntranceDoorId; }), "a hung door is authored: " + id);
        }
        F5Expect(e.PlannedDoorId.empty() || (e.PlannedDoorId.rfind("door.nabapara.", 0) == 0 && planned.insert(std::string(e.PlannedDoorId)).second),
               "planned door id is namespaced and unique: " + id);
    }
    F5Expect(KitFrontageEntries().size() == 9, "three batches of three frontages");
    F5Expect(sides[0] >= 1 && sides[1] >= 1, "at least one frontage on each side of the lane");
    F5Expect(compact >= 1 && shared >= 1, "at least one compact residence and one shared entry");

    // No row equals or overlaps a footprint that is built by its own code, and no two rows overlap each other.
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        for (const BuiltFootprint& b : BuiltFootprints())
        {
            const bool sameSide = e.Side == b.Side;
            const bool overlap = e.OriginXCm < b.X1Cm && b.X0Cm < e.OriginXCm + e.WidthCm;
            F5Expect(!(sameSide && overlap), std::string(e.BuildingId) + " overlaps " + std::string(b.BuildingId));
            F5Expect(!(sameSide && e.RowIndex == b.RowIndex), "row index differs from a built row");
        }
        for (const FrontageEntry& o : KitFrontageEntries())
        {
            if (&o == &e) continue;
            F5Expect(!(o.Side == e.Side && e.OriginXCm < o.OriginXCm + o.WidthCm && o.OriginXCm < e.OriginXCm + e.WidthCm), "kit rows do not overlap");
        }
    }
}

void F5TestPins()
{
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const std::string id(e.BuildingId);
        const FrontageEntry* hit = FindFrontageByFootprint(e.Side, e.OriginXCm, e.FrontAbsYCm, e.WidthCm, e.DepthCm, e.SetbackCm, e.Storeys, e.Seed);
        F5Expect(hit == &e, "the exact footprint is recognised: " + id);
        // Every single perturbation is refused.
        for (int field = 0; field < 8; ++field)
        {
            int v[7] = {e.OriginXCm, e.FrontAbsYCm, e.WidthCm, e.DepthCm, e.SetbackCm, e.Storeys, 0};
            int side = e.Side;
            std::uint32_t seed = e.Seed;
            switch (field)
            {
            case 0: side = -side; break;
            case 1: v[0] += 100; break;
            case 2: v[1] += 60; break;
            case 3: v[2] += 100; break;
            case 4: v[3] += 100; break;
            case 5: v[4] = v[4] == 0 ? 60 : 0; break;
            case 6: v[5] += 1; break;
            default: seed ^= 1u; break;
            }
            F5Expect(FindFrontageByFootprint(side, v[0], v[1], v[2], v[3], v[4], v[5], seed) == nullptr,
                   "a perturbed footprint (field " + std::to_string(field) + ") is refused: " + id);
        }
    }
    F5Expect(FindFrontageById("building.nabapara.stationery") == nullptr, "the stationery shop is not a kit row");
    F5Expect(FindFrontageById("") == nullptr, "an empty id finds nothing");
    // The built footprints are never recognised either (the stationery shop's exact pin).
    F5Expect(FindFrontageByFootprint(1, -100, 460, 700, 700, 60, 3, 2673104671u) == nullptr, "the stationery pin is not a kit row");
}

void F5TestLayouts()
{
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const std::string id(e.BuildingId);
        const InteriorInput in = MakeFrontageInput(e, 15, 300);
        F5Expect(in.BuildingId == id && in.Kind == e.Kind && in.WidthCm == e.WidthCm && in.DepthCm == e.DepthCm && in.MirrorU == e.MirrorU &&
                   in.EntranceCentreU == e.EntranceCentreU && in.EntranceDoorId == e.EntranceDoorId,
               "input mirrors the row: " + id);
        const InteriorLayout l = GenerateInteriorLayout(in);
        F5Expect(l.Ok(), "layout generates: " + id + " (" + std::string(RefusalCode(l.Status)) + ")");
        F5Expect(ValidateInteriorLayout(l) == Refusal::None, "layout validates: " + id);
        F5Expect(DescribeLayout(l) == DescribeLayout(GenerateInteriorLayout(in)), "deterministic: " + id);
        F5Expect(l.Mirrored == e.MirrorU && l.FinishedFloorCm == 15 && l.ClearHeightCm == 274, "datum and mirror: " + id);

        F5Expect(e.EntranceDoorId.empty() || e.EntranceDoorId == e.PlannedDoorId, "a hung front door uses the planned id: " + id);
        F5Expect(in.UnitDoorIdA.empty() == (e.Kind != Variant::SharedEntry) && in.UnitDoorIdB.empty() == (e.Kind != Variant::SharedEntry),
               "flat doors only on the shared entry: " + id);
        // The front entrance is a hung door, 90 x 210 clear, centred on the pinned entrance.
        const Opening* entrance = nullptr;
        for (const Opening& o : l.Openings)
            if (o.RoomA == "outside" && o.Kind != OpeningKind::Window) entrance = &o; // windows face outside too
        F5Expect(entrance != nullptr, "an entrance from outside: " + id);
        F5Expect(entrance->Kind == (e.EntranceDoorId.empty() ? OpeningKind::Doorway : OpeningKind::Door) && entrance->DoorId == e.EntranceDoorId,
               "the entrance matches the current migration stage: " + id);
        F5Expect(entrance->ClearWidthCm >= 90 && entrance->W0 == 0 && entrance->W1 >= 210, "entrance is 90 x 210 clear: " + id);
        F5Expect(std::abs((entrance->Plan.U0 + entrance->Plan.U1) / 2 - e.EntranceCentreU) <= 1, "entrance is where the row says: " + id);
        F5Expect(entrance->Plan.V0 == 0, "the entrance is in the front wall: " + id);
        int leaves = 0;
        for (const Opening& o : l.Openings)
        {
            leaves += o.DoorId.empty() ? 0 : 1;
        }
        F5Expect(leaves == (e.Kind == Variant::SharedEntry ? 2 : 0) + (e.EntranceDoorId.empty() ? 0 : 1), "one leaf per authored door: " + id);
        if (e.Kind == Variant::SharedEntry)
        {
            for (const char* suffix : {"unit_a", "unit_b"})
            {
                const std::string want = std::string("door.nabapara.tall_flats_") + suffix;
                const Opening* unit = nullptr;
                for (const Opening& o : l.Openings)
                    if (o.DoorId == want) unit = &o;
                F5Expect(unit != nullptr && unit->Kind == OpeningKind::Door, std::string("the flat door is hung: ") + want);
                if (unit == nullptr) continue;
                F5Expect(unit->ClearWidthCm >= 90 && unit->W0 == 0 && unit->W1 >= 210, std::string("flat door 90 x 210 clear: ") + want);
                const bool unitIsB = std::string(suffix) == "unit_b";
                // The leaf closes along V in the partition; it swings into the flat, and the hinge sits on the low-V jamb.
                F5Expect(unit->Plan.V1 - unit->Plan.V0 > unit->Plan.U1 - unit->Plan.U0, "a flat door runs along V: " + want);
                F5Expect(unit->HingeV == unit->Plan.V0 && unit->HingeU > unit->Plan.U0 && unit->HingeU < unit->Plan.U1, "hinge on the low-V jamb: " + want);
                F5Expect(unit->SweepSign == (unitIsB ? 1 : -1), "sweep sign points into the flat: " + want);
                const Zone* sweep = nullptr;
                for (const Zone& z : l.Zones)
                    if (z.Kind == ZoneKind::DoorSweep && z.OwnerId == unit->Id) sweep = &z;
                F5Expect(sweep != nullptr, "the flat door has a sweep zone: " + want);
                if (sweep == nullptr) continue;
                const Room* dwelling = FindRoom(l, sweep->RoomId);
                F5Expect(dwelling != nullptr && dwelling->Kind == RoomKind::Dwelling, "the sweep lies in the flat: " + want);
                F5Expect(dwelling != nullptr && dwelling->Area.Contains(sweep->Area), "the sweep fits inside the flat: " + want);
                F5Expect(sweep->Area.Width() >= 90 && !F5Solid(l, sweep->Area), "the sweep is 90 deep and clear: " + want);
            }
        }
        for (const Opening& o : l.Openings)
        {
            if (o.Kind == OpeningKind::Window) continue;
            F5Expect(o.ClearWidthCm >= 90 && o.W0 == 0 && o.W1 >= 210, "every doorway is 90 x 210: " + id);
        }
        for (const Zone& z : l.Zones)
        {
            if (z.Kind == ZoneKind::Circulation)
                F5Expect(std::min(z.Area.Width(), z.Area.Depth()) >= 120, "120 cm circulation: " + id);
            if (z.Kind != ZoneKind::Route)
                F5Expect(!F5Solid(l, z.Area), "clear space stays unoccupied: " + z.Id);
        }
        F5Expect(!l.Connections.empty() && l.Rooms.size() >= 2, "rooms are connected to the entrance: " + id);

        // Furnished: at least a bed and a dining set (two seats) in the residence sets, per dwelling in the shared entry.
        const int beds = static_cast<int>(std::count_if(l.Pieces.begin(), l.Pieces.end(), [](const Furniture& f) { return f.Kind == FurnitureKind::Bed; }));
        const int seats = static_cast<int>(std::count_if(l.Anchors.begin(), l.Anchors.end(), [](const Anchor& a) { return a.Kind == AnchorKind::Seat; }));
        const int dwellings = e.Kind == Variant::SharedEntry ? static_cast<int>(std::count_if(l.Rooms.begin(), l.Rooms.end(), [](const Room& r) { return r.Kind == RoomKind::Dwelling; })) : 1;
        if (e.Furniture == FurnitureSet::Residence)
            F5Expect(beds >= dwellings && seats >= 2 * dwellings, "furnished: a bed and two seats per dwelling: " + id);
        F5Expect(l.Pieces.size() >= 4 && !l.Lights.empty(), "furniture and lights present: " + id);
        if (e.Kind == Variant::SharedEntry)
            F5Expect(dwellings == 2, "the shared entry has a flat to each side: " + id);
        // Every room is lit, so a walk-through is never dark.
        for (const Room& r : l.Rooms)
        {
            F5Expect(std::any_of(l.Lights.begin(), l.Lights.end(), [&](const LightSpec& s) { return s.RoomId == r.Id; }), "room is lit: " + r.Id);
        }
    }
}

void F5TestNewRows()
{
    struct F5Pin { const char* Name; int Side; int Index; int X; int Width; int Depth; Variant Kind; FurnitureSet Set; };
    const F5Pin pins[]{
        {"repair_workshop", -1, 3, 800, 900, 900, Variant::ShopWithStockroom, FurnitureSet::Workshop},
        {"hardware_store", 1, 8, 5200, 1200, 700, Variant::ShopWithStockroom, FurnitureSet::GeneralGoods},
        {"old_house", 1, 7, 4500, 700, 800, Variant::CompactResidence, FurnitureSet::Residence},
        {"market_stalls_a", -1, 4, 1700, 900, 800, Variant::ShopWithStockroom, FurnitureSet::GeneralGoods},
        {"market_stalls_b", 1, 6, 3400, 1100, 700, Variant::ShopWithStockroom, FurnitureSet::GeneralGoods},
        {"end_shop", -1, 9, 5700, 700, 900, Variant::ShopWithStockroom, FurnitureSet::GeneralGoods}};
    for (const auto& pin : pins)
    {
        const std::string id = std::string("building.nabapara.") + pin.Name;
        const FrontageEntry* e = FindFrontageById(id);
        F5Expect(e != nullptr, "NR-05 stable identity: " + id);
        F5Expect(e->Side == pin.Side && e->RowIndex == pin.Index && e->OriginXCm == pin.X && e->WidthCm == pin.Width && e->DepthCm == pin.Depth &&
                   e->Kind == pin.Kind && e->Furniture == pin.Set && e->EntranceCentreU == 250 && !e->MirrorU, "NR-05 policy pin: " + id);
        F5Expect(e->PlannedDoorId == std::string("door.nabapara.") + pin.Name + "_front" && e->EntranceDoorId == e->PlannedDoorId &&
                   PerfectWorld::FindDoor(e->EntranceDoorId) != nullptr, "the 16 -> 20 migration hangs the planned, authored door: " + id);
        const auto in = MakeFrontageInput(*e, 15, 300);
        // The hung leaf's 90 cm sweep must fit without moving the furniture.
        F5Expect(in.EntranceDoorId == e->PlannedDoorId, "the layout input carries the hung door: " + id);
        const auto l = GenerateInteriorLayout(in);
        F5Expect(l.Ok() && ValidateInteriorLayout(l) == Refusal::None, "hung leaf preserves furnished circulation: " + id);
        int sweeps = 0;
        for (const auto& z : l.Zones)
            if (z.Kind == ZoneKind::DoorSweep)
            {
                ++sweeps;
                F5Expect(!F5Solid(l, z.Area), "leaf sweep stays clear: " + id);
            }
        F5Expect(sweeps >= 1, "hung leaf reserves its sweep: " + id);
        const std::string prefix = id + ".anchor.";
        if (pin.Set == FurnitureSet::Workshop)
        {
            F5Expect(FindAnchor(l, prefix + "workbench") != nullptr, "workshop has a work anchor");
            F5Expect(l.Routes.size() >= 4, "workshop routes connect work floor and entrance");
        }
        if (pin.Set == FurnitureSet::GeneralGoods)
            F5Expect(FindAnchor(l, prefix + "browse_shelf") != nullptr || FindAnchor(l, prefix + "browse_bins") != nullptr, "store has a browse anchor");
        std::vector<RoutePoint> path;
        if (pin.Set == FurnitureSet::Residence)
        {
            const auto* a = FindAnchor(l, prefix + "bed_side");
            F5Expect(a != nullptr, "residence has a furnished browse stand");
            path = {{250, -45}, {250, a->V}, {a->U, a->V}};
        }
        for (const auto& route : l.Routes)
            for (size_t i = 1; i < route.Points.size(); ++i)
            {
                const auto a = route.Points[i - 1];
                const auto b = route.Points[i];
                const Rect leg{std::min(a.U, b.U) - 34, std::max(a.U, b.U) + 34, std::min(a.V, b.V) - 34, std::max(a.V, b.V) + 34};
                F5Expect(!F5Solid(l, leg), "authored shop route clears furniture: " + route.Id);
                for (const auto& wall : l.Walls)
                    if (wall.W0 < 165 && wall.W1 > 0)
                        F5Expect(!wall.Plan.Overlaps(leg), "authored shop route clears the walls: " + route.Id);
            }
        for (size_t i = 1; i < path.size(); ++i)
        {
            const auto a = path[i - 1];
            const auto b = path[i];
            const Rect leg{std::min(a.U, b.U) - 34, std::max(a.U, b.U) + 34, std::min(a.V, b.V) - 34, std::max(a.V, b.V) + 34};
            F5Expect(!F5Solid(l, leg), "old-house browse walk clears furniture");
            for (const auto& wall : l.Walls)
                if (wall.W0 < 165 && wall.W1 > 0)
                    F5Expect(!wall.Plan.Overlaps(leg), "old-house browse walk clears walls");
        }
    }
}

void F5TestDoorDrafts()
{
    using namespace PerfectWorld;
    const DoorOpeningWindow workshop[]{{480, 720}, {840, 1110}};
    const DoorOpeningWindow store[]{{540, 780}, {900, 1200}};
    const DoorOpeningWindow visiting[]{{960, 1170}};
    const DoorDef drafts[]{
        {"door.nabapara.repair_workshop_front", "location.nabapara.repair_workshop", false, DoorAccess::BusinessHours, 0, 0, workshop, false, 1, {}},
        {"door.nabapara.hardware_store_front", "location.nabapara.hardware_store", false, DoorAccess::BusinessHours, 0, 0, store, false, 1, {}},
        {"door.nabapara.old_house_front", "location.nabapara.old_house", false, DoorAccess::Private, 0, 0, {}, false, 1, visiting}};
    for (const auto& door : drafts)
    {
        F5Expect(IsDoorDefinitionValid(door), "migration draft is valid");
        // The roster authors exactly the NR-05 drafts.
        const DoorDef* authored = FindDoor(door.Id);
        F5Expect(authored != nullptr && authored->LocationId == door.LocationId && authored->Access == door.Access && !authored->bInitiallyOpen &&
                   !authored->bTwoWay && authored->PreferredSwingSide == 1 && authored->OpeningWindows.size() == door.OpeningWindows.size() &&
                   authored->VisitingWindows.size() == door.VisitingWindows.size(), "the authored door matches its draft");
        for (std::size_t i = 0; authored != nullptr && i < door.OpeningWindows.size(); ++i)
            F5Expect(authored->OpeningWindows[i].OpenMinute == door.OpeningWindows[i].OpenMinute &&
                       authored->OpeningWindows[i].CloseMinute == door.OpeningWindows[i].CloseMinute, "authored business hours match");
        for (std::size_t i = 0; authored != nullptr && i < door.VisitingWindows.size(); ++i)
            F5Expect(authored->VisitingWindows[i].OpenMinute == door.VisitingWindows[i].OpenMinute &&
                       authored->VisitingWindows[i].CloseMinute == door.VisitingWindows[i].CloseMinute, "authored visiting hours match");
        DoorUseContext exit;
        exit.Direction = DoorDirection::Exit;
        F5Expect(EvaluateDoorPassage(door, 0, exit) == DoorResult::Ok, "unrestricted exit at midnight");
        if (door.Access == DoorAccess::BusinessHours)
        {
            F5Expect(EvaluateDoorPassage(door, 630, {}) == DoorResult::Ok, "acceptance business time is open");
            for (const auto& window : door.OpeningWindows)
            {
                F5Expect(EvaluateDoorPassage(door, window.OpenMinute, {}) == DoorResult::Ok, "inclusive opening boundary");
                F5Expect(EvaluateDoorPassage(door, window.CloseMinute, {}) == DoorResult::Closed, "exclusive closing boundary");
            }
        }
        else
        {
            for (const int minute : {0, 959, 960, 1020, 1169, 1170, 1439})
                F5Expect(EvaluateDoorKnock(door, minute, {}).Answer == DoorKnockAnswer::NoAnswer, "no resident: NoAnswer regardless of visiting window");
            F5Expect(EvaluateDoorPassage(door, 1020, {}) == DoorResult::Private, "an empty house grants no visitor admission");
            DoorUseContext knocked;
            knocked.HasKnocked = true;
            F5Expect(EvaluateDoorPassage(door, 1020, knocked) == DoorResult::Private, "an unanswered knock grants no admission");
            DoorUseContext invited;
            invited.IsInvited = true;
            F5Expect(EvaluateDoorPassage(door, 1020, invited) == DoorResult::Ok, "legitimate invitation permits the future walk");
        }
    }
}
void R1FrontageSwingSides()
{
    for (const char* R1Name : {"market_stalls_a", "market_stalls_b", "end_shop"})
    {
        const auto* R1Row = FindFrontageById(std::string("building.nabapara.") + R1Name);
        F5Expect(R1Row != nullptr, "round4 row exists");
        const auto R1Layout = GenerateInteriorLayout(MakeFrontageInput(*R1Row, 15, 300));
        const auto* R1Door = PerfectWorld::FindDoor(R1Row->EntranceDoorId);
        F5Expect(R1Layout.Ok() && R1Door != nullptr, "round4 layout and door exist");
        for (const auto& R1Opening : R1Layout.Openings)
        {
            if (R1Opening.DoorId != R1Row->EntranceDoorId) continue;
            F5Expect(R1Opening.HingeU == R1Opening.Plan.U0 && R1Opening.SweepSign == 1 && !R1Row->MirrorU,
                "round4 low-U hinge swings into +V");
            F5Expect(R1Door->PreferredSwingSide == R1Row->Side && !R1Door->bTwoWay,
                "core physical side matches KitDoorSpec on both lane orientations");
        }
    }
}
} // namespace

int main()
{
    F5TestGenerator();
    F5TestCatalogueShape();
    F5TestPins();
    F5TestLayouts();
    F5TestNewRows();
    F5TestDoorDrafts();
    R1FrontageSwingSides();
    std::cout << "LaneFrontageCatalogueTests: " << F5Assertions << " assertions passed\n";
    return 0;
}
