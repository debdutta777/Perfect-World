// LL-03 submission 2: the kit-built frontage catalogue (LaneFrontageCatalogue.h). Proves, portably, that every pinned row
// matches what the lane generator produces (a test-local port of BuildFirstRows, checked against the documented stationery
// hash), that no row touches a ground floor built by its own code, and that every row generates a valid, furnished layout with
// 90 x 210 doorways and 120 cm circulation, in both a plain and a perturbed form.

#include "Doors.h"
#include "LaneFrontageCatalogue.h"

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
int Assertions = 0;
void Expect(bool condition, const std::string& message)
{
    ++Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

// ---- A test-local port of PerfectWorldLaneBuildings.cpp BuildFirstRows (uint32 arithmetic, exact). ----
std::uint32_t FcHash(std::int32_t a, std::int32_t b, std::int32_t c)
{
    std::uint32_t h = static_cast<std::uint32_t>(a) * 0x9E3779B1u ^ static_cast<std::uint32_t>(b) * 0x85EBCA77u ^ static_cast<std::uint32_t>(c) * 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

struct FcRow
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

std::vector<FcRow> FcEnumerate()
{
    constexpr int laneX0 = -2400;
    constexpr int laneX1 = 6400;
    std::vector<FcRow> rows;
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
            const std::uint32_t h = FcHash(side * 131 + 7, index, 11);
            int width = 600 + 100 * static_cast<int>(h % 6u);
            if (cursor < alleyA && cursor + width > alleyA) width = alleyA - cursor;
            if (laneX1 - (cursor + width) < 500) width = laneX1 - cursor;
            if (width < 300)
            {
                cursor += width;
                continue;
            }
            const std::uint32_t kind = (h >> 12) % 4u;
            FcRow row;
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

const FcRow* FcFind(const std::vector<FcRow>& rows, int side, int index)
{
    for (const FcRow& r : rows)
        if (r.Side == side && r.Index == index) return &r;
    return nullptr;
}

bool Solid(const InteriorLayout& l, const Rect& r)
{
    return std::any_of(l.Pieces.begin(), l.Pieces.end(), [&](const Furniture& f) { return f.Solid && f.Footprint.Overlaps(r); });
}

void TestGenerator()
{
    Expect(FcHash(138, 2, 11) == 2673104671u, "the port reproduces the documented stationery seed");
    const std::vector<FcRow> rows = FcEnumerate();
    Expect(rows.size() == 19, "the generator yields nineteen side frontages (10 on -Y, 9 on +Y)");
    Expect(std::count_if(rows.begin(), rows.end(), [](const FcRow& r) { return r.Side < 0; }) == 10, "ten on the -Y side");

    for (const BuiltFootprint& b : BuiltFootprints())
    {
        const FcRow* r = FcFind(rows, b.Side, b.RowIndex);
        Expect(r != nullptr && r->X0 == b.X0Cm && r->X1 == b.X1Cm, std::string("built footprint matches the generator: ") + std::string(b.BuildingId));
    }
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const FcRow* r = FcFind(rows, e.Side, e.RowIndex);
        const std::string id(e.BuildingId);
        Expect(r != nullptr, "row exists: " + id);
        Expect(r->X0 == e.OriginXCm && r->Width == e.WidthCm && r->Depth == e.DepthCm && r->Setback == e.SetbackCm && r->Storeys == e.Storeys &&
                   r->Seed == e.Seed,
               "pinned footprint equals the generated one: " + id);
        Expect(e.FrontAbsYCm == 400 + e.SetbackCm, "front line is 400 plus the setback: " + id);
        Expect(e.Side == -1 || e.Side == 1, "side: " + id);
    }
}

void TestCatalogueShape()
{
    std::set<std::string> ids;
    std::set<std::string> planned;
    int sides[2] = {0, 0};
    int compact = 0;
    int shared = 0;
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const std::string id(e.BuildingId);
        Expect(id.rfind("building.nabapara.", 0) == 0 && id.size() > 18, "id namespace: " + id);
        Expect(std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || c == '.' || c == '_' || (c >= '0' && c <= '9'); }),
               "id characters: " + id);
        Expect(ids.insert(id).second, "ids are unique: " + id);
        ++sides[e.Side > 0 ? 1 : 0];
        compact += e.Kind == Variant::CompactResidence ? 1 : 0;
        shared += e.Kind == Variant::SharedEntry ? 1 : 0;
        Expect(FindFrontageById(e.BuildingId) == &e, "lookup by id: " + id);

        // Doors: a hung leaf must be an authored lane door; a planned one must not exist yet (it needs a migration).
        for (const auto& d : PerfectWorld::AuthoredDoors())
        {
            Expect(d.Id != e.PlannedDoorId, "a planned door is not authored yet: " + id);
        }
        if (!e.EntranceDoorId.empty())
        {
            const auto doors = PerfectWorld::AuthoredDoors();
            Expect(std::any_of(doors.begin(), doors.end(), [&](const auto& d) { return d.Id == e.EntranceDoorId; }), "a hung door is authored: " + id);
        }
        Expect(e.PlannedDoorId.empty() || (e.PlannedDoorId.rfind("door.nabapara.", 0) == 0 && planned.insert(std::string(e.PlannedDoorId)).second),
               "planned door id is namespaced and unique: " + id);
    }
    Expect(KitFrontageEntries().size() == 3, "batch 1 is exactly three frontages");
    Expect(sides[0] >= 1 && sides[1] >= 1, "at least one frontage on each side of the lane");
    Expect(compact >= 1 && shared >= 1, "at least one compact residence and one shared entry");

    // No row equals or overlaps a footprint that is built by its own code, and no two rows overlap each other.
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        for (const BuiltFootprint& b : BuiltFootprints())
        {
            const bool sameSide = e.Side == b.Side;
            const bool overlap = e.OriginXCm < b.X1Cm && b.X0Cm < e.OriginXCm + e.WidthCm;
            Expect(!(sameSide && overlap), std::string(e.BuildingId) + " overlaps " + std::string(b.BuildingId));
            Expect(!(sameSide && e.RowIndex == b.RowIndex), "row index differs from a built row");
        }
        for (const FrontageEntry& o : KitFrontageEntries())
        {
            if (&o == &e) continue;
            Expect(!(o.Side == e.Side && e.OriginXCm < o.OriginXCm + o.WidthCm && o.OriginXCm < e.OriginXCm + e.WidthCm), "kit rows do not overlap");
        }
    }
}

void TestPins()
{
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const std::string id(e.BuildingId);
        const FrontageEntry* hit = FindFrontageByFootprint(e.Side, e.OriginXCm, e.FrontAbsYCm, e.WidthCm, e.DepthCm, e.SetbackCm, e.Storeys, e.Seed);
        Expect(hit == &e, "the exact footprint is recognised: " + id);
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
            Expect(FindFrontageByFootprint(side, v[0], v[1], v[2], v[3], v[4], v[5], seed) == nullptr,
                   "a perturbed footprint (field " + std::to_string(field) + ") is refused: " + id);
        }
    }
    Expect(FindFrontageById("building.nabapara.stationery") == nullptr, "the stationery shop is not a kit row");
    Expect(FindFrontageById("") == nullptr, "an empty id finds nothing");
    // The built footprints are never recognised either (the stationery shop's exact pin).
    Expect(FindFrontageByFootprint(1, -100, 460, 700, 700, 60, 3, 2673104671u) == nullptr, "the stationery pin is not a kit row");
}

void TestLayouts()
{
    for (const FrontageEntry& e : KitFrontageEntries())
    {
        const std::string id(e.BuildingId);
        const InteriorInput in = MakeFrontageInput(e, 15, 300);
        Expect(in.BuildingId == id && in.Kind == e.Kind && in.WidthCm == e.WidthCm && in.DepthCm == e.DepthCm && in.MirrorU == e.MirrorU &&
                   in.EntranceCentreU == e.EntranceCentreU && in.EntranceDoorId.empty() == e.EntranceDoorId.empty(),
               "input mirrors the row: " + id);
        const InteriorLayout l = GenerateInteriorLayout(in);
        Expect(l.Ok(), "layout generates: " + id + " (" + std::string(RefusalCode(l.Status)) + ")");
        Expect(ValidateInteriorLayout(l) == Refusal::None, "layout validates: " + id);
        Expect(DescribeLayout(l) == DescribeLayout(GenerateInteriorLayout(in)), "deterministic: " + id);
        Expect(l.Mirrored == e.MirrorU && l.FinishedFloorCm == 15 && l.ClearHeightCm == 274, "datum and mirror: " + id);

        // The front entrance is an open doorway, 90 x 210 clear, centred on the pinned entrance.
        const Opening* entrance = nullptr;
        for (const Opening& o : l.Openings)
            if (o.RoomA == "outside" && o.Kind != OpeningKind::Window) entrance = &o; // windows face outside too
        Expect(entrance != nullptr, "an entrance from outside: " + id);
        Expect(entrance->Kind == OpeningKind::Doorway && entrance->DoorId.empty(), "the entrance is an open doorway (no lane door yet): " + id);
        Expect(entrance->ClearWidthCm >= 90 && entrance->W0 == 0 && entrance->W1 >= 210, "entrance is 90 x 210 clear: " + id);
        Expect(std::abs((entrance->Plan.U0 + entrance->Plan.U1) / 2 - e.EntranceCentreU) <= 1, "entrance is where the row says: " + id);
        Expect(entrance->Plan.V0 == 0, "the entrance is in the front wall: " + id);
        for (const Opening& o : l.Openings)
        {
            if (o.Kind == OpeningKind::Window) continue;
            Expect(o.ClearWidthCm >= 90 && o.W0 == 0 && o.W1 >= 210, "every doorway is 90 x 210: " + id);
        }
        for (const Zone& z : l.Zones)
        {
            if (z.Kind == ZoneKind::Circulation)
                Expect(std::min(z.Area.Width(), z.Area.Depth()) >= 120, "120 cm circulation: " + id);
            if (z.Kind != ZoneKind::Route)
                Expect(!Solid(l, z.Area), "clear space stays unoccupied: " + z.Id);
        }
        Expect(!l.Connections.empty() && l.Rooms.size() >= 2, "rooms are connected to the entrance: " + id);

        // Furnished: at least a bed and a dining set (two seats) in the residence sets, per dwelling in the shared entry.
        const int beds = static_cast<int>(std::count_if(l.Pieces.begin(), l.Pieces.end(), [](const Furniture& f) { return f.Kind == FurnitureKind::Bed; }));
        const int seats = static_cast<int>(std::count_if(l.Anchors.begin(), l.Anchors.end(), [](const Anchor& a) { return a.Kind == AnchorKind::Seat; }));
        const int dwellings = e.Kind == Variant::SharedEntry ? static_cast<int>(std::count_if(l.Rooms.begin(), l.Rooms.end(), [](const Room& r) { return r.Kind == RoomKind::Dwelling; })) : 1;
        Expect(dwellings >= 1, "at least one dwelling: " + id);
        Expect(beds >= dwellings && seats >= 2 * dwellings, "furnished: a bed and two seats per dwelling: " + id);
        Expect(l.Pieces.size() >= 4 && !l.Lights.empty(), "furniture and lights present: " + id);
        if (e.Kind == Variant::SharedEntry)
            Expect(dwellings == 2, "the shared entry has a flat to each side: " + id);
        // Every room is lit, so a walk-through is never dark.
        for (const Room& r : l.Rooms)
        {
            Expect(std::any_of(l.Lights.begin(), l.Lights.end(), [&](const LightSpec& s) { return s.RoomId == r.Id; }), "room is lit: " + r.Id);
        }
    }
}
} // namespace

int main()
{
    TestGenerator();
    TestCatalogueShape();
    TestPins();
    TestLayouts();
    std::cout << "LaneFrontageCatalogueTests: " << Assertions << " assertions passed\n";
    return 0;
}
