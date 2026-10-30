// NL-03: the general-goods and workshop furniture sets of the two-room shop (LaneInteriorShopFurniture.cpp), the pinned stationery
// output, the narrow-footprint refusals, and the optional shared-entry unit-door id fields on InteriorInput. Portable: deterministic,
// mirrored, anchors usable and reachable on foot, circulation / sweeps / approach zones free of furniture.

#include "LaneInteriorLayout.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace PerfectWorld::LaneInterior;

namespace
{
int Assertions = 0;
void Expect(bool condition, const char* message)
{
    ++Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

InteriorInput ShopInput(int width, int depth, int entrance)
{
    InteriorInput in;
    in.BuildingId = "building.test.shop";
    in.Kind = Variant::ShopWithStockroom;
    in.Furniture = FurnitureSet::Stationery;
    in.WidthCm = width;
    in.DepthCm = depth;
    in.EntranceCentreU = entrance;
    in.EntranceDoorId = "door.test.front";
    return in;
}

InteriorInput HomeInput(Variant kind, int width, int depth, int entrance)
{
    InteriorInput in;
    in.BuildingId = kind == Variant::SharedEntry ? "building.test.shared" : "building.test.home";
    in.Kind = kind;
    in.Furniture = FurnitureSet::Residence;
    in.WidthCm = width;
    in.DepthCm = depth;
    in.EntranceCentreU = entrance;
    return in;
}

bool Solid(const InteriorLayout& l, const Rect& r)
{
    return std::any_of(l.Pieces.begin(), l.Pieces.end(), [&](const Furniture& f) { return f.Solid && f.Footprint.Overlaps(r); });
}

/** The contracts, re-derived here rather than trusted from the validator. */
void ExpectContracts(const InteriorLayout& l, const char* what)
{
    Expect(l.Ok(), what);
    Expect(ValidateInteriorLayout(l) == Refusal::None, what);
    Expect(l.ClearHeightCm == l.StoreyHeightCm - SlabAndPlasterCm, "clear height is floor-relative");
    bool hasEntrance = false;
    for (const Opening& o : l.Openings)
    {
        if (o.Kind == OpeningKind::Window)
        {
            Expect(o.W0 >= 60 && o.W1 <= l.ClearHeightCm, "window heights are floor-relative and inside the storey");
            continue;
        }
        Expect(o.ClearWidthCm >= 90, "doorway at least 90 clear");
        Expect(o.W0 == 0 && o.W1 - o.W0 >= 210, "doorway at least 210 clear, from the floor");
        hasEntrance = hasEntrance || o.RoomA == "outside";
    }
    Expect(hasEntrance, "an entrance to the outside");
    for (const Zone& z : l.Zones)
    {
        if (z.Kind == ZoneKind::Circulation)
            Expect(z.Area.Width() >= 120 && z.Area.Depth() >= 120, "circulation at least 120 each way");
        Expect(!Solid(l, z.Area), "zones stay free of solid furniture");
        if (z.Kind == ZoneKind::DoorSweep)
            Expect(z.Area.Width() >= 90 && z.Area.Depth() >= 90, "a door sweep covers the full leaf");
    }
    for (const Anchor& a : l.Anchors)
    {
        if (a.Kind == AnchorKind::Stand)
            Expect(!Solid(l, Rect{a.U - 40, a.U + 40, a.V - 40, a.V + 40}), "stand points have an 80 cm free square");
        if (a.Kind == AnchorKind::Seat)
        {
            const Furniture* f = nullptr;
            for (const Furniture& p : l.Pieces) if (p.Id == a.FurnitureId) f = &p;
            Expect(f != nullptr && f->Footprint.ContainsPoint(a.U, a.V), "a seat point lies on its seat");
            Expect(f->H1 > 45 && f->H1 <= 50, "seats stand just above the 45 cm step-up so nothing can be climbed");
        }
    }
    for (const Furniture& f : l.Pieces)
    {
        Expect(f.H0 >= 0 && f.H1 <= l.ClearHeightCm, "furniture heights are floor-relative");
        Expect(!f.Solid || f.H1 > 45, "no climbable solid below the 45 cm step-up");
    }
    for (const LightSpec& s : l.Lights)
    {
        Expect(s.HeightCm < l.ClearHeightCm && s.RadiusCm >= 150 && s.Lumens >= 600, "light specs are sane");
    }
}

std::set<std::string> AllIds(const InteriorLayout& l)
{
    std::set<std::string> ids;
    for (const auto& x : l.Rooms) ids.insert(x.Id);
    for (const auto& x : l.Openings) ids.insert(x.Id);
    for (const auto& x : l.Walls) ids.insert(x.Id);
    for (const auto& x : l.Connections) ids.insert(x.Id);
    for (const auto& x : l.Pieces) ids.insert(x.Id);
    for (const auto& x : l.Anchors) ids.insert(x.Id);
    for (const auto& x : l.Zones) ids.insert(x.Id);
    for (const auto& x : l.Lights) ids.insert(x.Id);
    for (const auto& x : l.Routes) ids.insert(x.Id);
    return ids;
}


// ---- NL-03: general-goods and workshop furniture sets, the pinned stationery output, and the unit-door id fields ----

InteriorInput SetInput(FurnitureSet set, int width, int depth, int entrance)
{
    InteriorInput in = ShopInput(width, depth, entrance);
    in.Furniture = set;
    return in;
}

const char* SetName(FurnitureSet set)
{
    return set == FurnitureSet::GeneralGoods ? "general goods" : (set == FurnitureSet::Workshop ? "workshop" : "stationery");
}

std::uint64_t Fnv1a(const std::string& text)
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const unsigned char c : text)
    {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}

/**
 * The stationery shop's output, pinned. The hashes were taken from the code as committed BEFORE the new sets were added (HEAD
 * 8bdd8b6): every id, rectangle, anchor, zone, light and staff route of DescribeLayout. Any change to the stationery layout or
 * to the routes the shop's staff walk changes a hash and fails here.
 */
void StationeryOutputIsPinned()
{
    const struct { int W; int D; int C; bool Mirror; std::uint64_t Hash; } pins[] = {
        {700, 700, 250, false, 0x5c3b338c3655ee1bULL}, {700, 700, 450, false, 0x8520450a9e0928acULL}, {700, 700, 250, true, 0x0273f993493a9903ULL},
        {420, 680, 100, false, 0x538582e2f5b9dd76ULL}, {1100, 900, 300, false, 0x8adca9082991168cULL}, {900, 800, 150, true, 0xd78d767043d66cb8ULL},
        {420, 680, 300, false, 0xe5f16c4251f70703ULL}, {1000, 900, 650, false, 0x6bded74f044bc97dULL}, {640, 700, 200, false, 0x754fa472ae970c64ULL}};
    for (const auto& p : pins)
    {
        InteriorInput in = ShopInput(p.W, p.D, p.Mirror ? p.W - p.C : p.C);
        in.MirrorU = p.Mirror;
        const InteriorLayout l = GenerateInteriorLayout(in);
        Expect(l.Ok(), "pinned stationery layout builds");
        if (Fnv1a(DescribeLayout(l)) != p.Hash)
            std::cerr << "stationery hash changed for " << p.W << 'x' << p.D << " c=" << p.C << " mirror=" << p.Mirror << '\n';
        Expect(Fnv1a(DescribeLayout(l)) == p.Hash, "stationery output is byte-identical to the pre-NL-03 layout");
    }
}

/** True when a point is standable floor: its 68 cm capsule square lies inside rooms, doorways or the footpath and touches no solid. */
bool CapsuleFree(const InteriorLayout& l, int u, int v)
{
    const auto onFloor = [&](int pu, int pv) {
        if (pv <= 0 && pv >= -150 && pu >= -100 && pu <= l.WidthCm + 100) return true;
        for (const Room& r : l.Rooms) if (r.Area.ContainsPoint(pu, pv)) return true;
        for (const Opening& o : l.Openings) if (o.Kind != OpeningKind::Window && o.Plan.ContainsPoint(pu, pv)) return true;
        return false;
    };
    for (const int du : {-34, 0, 34})
        for (const int dv : {-34, 0, 34})
            if (!onFloor(u + du, v + dv)) return false;
    return !Solid(l, Rect{u - 34, u + 34, v - 34, v + 34});
}

/** A flood fill on a 5 cm grid from the footpath threshold; every stand anchor must be reached on foot. */
void ExpectAnchorsReachable(const InteriorLayout& l, const char* what)
{
    constexpr int Step = 5;
    const int u0 = -100;
    const int v0 = -150;
    const int nu = (l.WidthCm + 200) / Step + 1;
    const int nv = (l.DepthCm + 150) / Step + 1;
    std::vector<char> seen(static_cast<size_t>(nu) * nv, 0);
    const auto cell = [&](int u, int v) { return std::pair<int, int>{(u - u0 + Step / 2) / Step, (v - v0 + Step / 2) / Step}; };
    const Anchor* door = nullptr;
    for (const Anchor& a : l.Anchors) if (a.Kind == AnchorKind::Threshold) door = &a;
    Expect(door != nullptr, what);
    std::vector<std::pair<int, int>> open;
    const auto start = cell(door->U, door->V);
    Expect(CapsuleFree(l, door->U, door->V), "the footpath outside the door is walkable");
    seen[static_cast<size_t>(start.second) * nu + start.first] = 1;
    open.push_back(start);
    for (size_t head = 0; head < open.size(); ++head)
    {
        const auto at = open[head];
        for (const auto& d : {std::pair<int, int>{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
        {
            const int cu = at.first + d.first;
            const int cv = at.second + d.second;
            if (cu < 0 || cv < 0 || cu >= nu || cv >= nv || seen[static_cast<size_t>(cv) * nu + cu]) continue;
            if (CapsuleFree(l, u0 + cu * Step, v0 + cv * Step))
            {
                seen[static_cast<size_t>(cv) * nu + cu] = 1;
                open.push_back({cu, cv});
            }
        }
    }
    for (const Anchor& a : l.Anchors)
    {
        if (a.Kind != AnchorKind::Stand) continue;
        const auto c = cell(a.U, a.V);
        if (!seen[static_cast<size_t>(c.second) * nu + c.first]) std::cerr << what << ": unreachable anchor " << a.Id << '\n';
        Expect(seen[static_cast<size_t>(c.second) * nu + c.first] != 0, "every stand anchor is reachable on foot from the footpath");
    }
}

void ExpectPieceFacing(const InteriorLayout& l)
{
    for (const Anchor& a : l.Anchors)
    {
        if (a.Kind != AnchorKind::Stand || a.FurnitureId.empty()) continue;
        const Furniture* f = FindFurniture(l, a.FurnitureId);
        Expect(f != nullptr, "an anchor's piece exists");
        // The anchor looks at its piece: the piece's nearest point lies ahead of the facing.
        const int nu = std::max(f->Footprint.U0, std::min(a.U, f->Footprint.U1)) - a.U;
        const int nv = std::max(f->Footprint.V0, std::min(a.V, f->Footprint.V1)) - a.V;
        Expect(nu * a.FaceU + nv * a.FaceV > 0, "a stand anchor faces the piece it serves");
    }
}

void ShopSetsReference()
{
    for (const FurnitureSet set : {FurnitureSet::GeneralGoods, FurnitureSet::Workshop})
    {
        const InteriorLayout l = GenerateInteriorLayout(SetInput(set, 700, 700, 250));
        ExpectContracts(l, SetName(set));
        ExpectAnchorsReachable(l, SetName(set));
        ExpectPieceFacing(l);
        Expect(l.Rooms.size() == 2 && l.Connections.size() == 2, "front room and back room");
        const bool workshop = set == FurnitureSet::Workshop;
        const Room* back = FindRoom(l, workshop ? "building.test.shop.room.workshop" : "building.test.shop.room.stock");
        Expect(back != nullptr && back->Kind == (workshop ? RoomKind::Workshop : RoomKind::Stock), "the back room is named for the set");
        const std::vector<const char*> anchors = workshop
            ? std::vector<const char*>{"staff_counter", "customer_counter", "tool_rack", "workbench", "finishing_table", "front_door"}
            : std::vector<const char*>{"staff_counter", "customer_counter", "stock_room", "weigh_station", "front_door", "browse_shelf"};
        for (const char* n : anchors) Expect(FindAnchor(l, std::string("building.test.shop.anchor.") + n) != nullptr, "every documented anchor exists");
        const std::vector<const char*> routes = workshop ? std::vector<const char*>{"counter_to_tool_rack", "counter_to_workbench", "counter_to_finishing_table", "counter_to_door"}
                                                         : std::vector<const char*>{"counter_to_stock_room", "counter_to_weigh_station", "counter_to_door"};
        for (const char* r : routes)
        {
            const Route* route = FindRoute(l, std::string("building.test.shop.route.") + r);
            Expect(route != nullptr && route->LengthCm() > 100 && route->LengthCm() < 1400, "every staff route exists with a plausible length");
        }
        // Staff behind the counter facing the door, customer in front; the counter is 90 high with the staff gap on the high-U side.
        const Anchor* staff = FindAnchor(l, "building.test.shop.anchor.staff_counter");
        const Anchor* customer = FindAnchor(l, "building.test.shop.anchor.customer_counter");
        Expect(staff->FaceV == -1 && customer->FaceV == 1 && staff->V - customer->V == 140, "140 cm across the counter, facing each other");
        const Furniture* counter = FindFurniture(l, "building.test.shop.piece.counter");
        Expect(counter != nullptr && counter->H1 == 90 && counter->Footprint.U1 == 675 - 120, "counter 90 high with a 120 staff gap");
        // Stable asset ids: every piece names an asset.lane.furniture.* asset, and the set introduces its own.
        std::set<std::string> assets;
        for (const Furniture& f : l.Pieces)
        {
            Expect(f.AssetId.rfind("asset.lane.furniture.", 0) == 0 && f.AssetId.size() > 22, "stable asset ids");
            assets.insert(f.AssetId);
        }
        Expect(assets.count(workshop ? "asset.lane.furniture.bench_work" : "asset.lane.furniture.table_weigh") == 1, "the set's own asset is used");
        // The work anchors serve distinct pieces.
        std::set<std::string> served;
        for (const Anchor& a : l.Anchors) if (a.Kind == AnchorKind::Stand) served.insert(a.FurnitureId);
        Expect(served.size() >= 4, "work and standing anchors serve at least four pieces");
    }
}

void ShopSetsMirroredAndDeterministic()
{
    for (const FurnitureSet set : {FurnitureSet::GeneralGoods, FurnitureSet::Workshop})
    {
        for (const InteriorInput& base : {SetInput(set, 700, 700, 250), SetInput(set, 700, 700, 450), SetInput(set, 900, 800, 600), SetInput(set, 520, 700, 150)})
        {
            const InteriorLayout a = GenerateInteriorLayout(base);
            ExpectContracts(a, "set layout builds");
            Expect(DescribeLayout(a) == DescribeLayout(GenerateInteriorLayout(base)), "output is deterministic");
            InteriorInput mirrored = base;
            mirrored.MirrorU = true;
            mirrored.EntranceCentreU = base.WidthCm - base.EntranceCentreU;
            const InteriorLayout m = GenerateInteriorLayout(mirrored);
            ExpectContracts(m, "mirrored set layout");
            InteriorLayout reflected = a;
            ReflectLayoutU(reflected);
            reflected.Mirrored = true;
            Expect(DescribeLayout(reflected) == DescribeLayout(m), "a mirrored set is the reflection of the unmirrored one");
            Expect(AllIds(a) == AllIds(m), "mirroring keeps every id");
            ExpectAnchorsReachable(m, "mirrored set");
        }
    }
    // A right-half entrance is the reflection of the matching left-half entrance (the staff gap stays away from the door).
    for (const FurnitureSet set : {FurnitureSet::GeneralGoods, FurnitureSet::Workshop})
    {
        InteriorLayout left = GenerateInteriorLayout(SetInput(set, 700, 700, 250));
        const InteriorLayout right = GenerateInteriorLayout(SetInput(set, 700, 700, 450));
        ReflectLayoutU(left);
        Expect(right.Ok() && DescribeLayout(left) == DescribeLayout(right), "a right-half entrance is the reflection of the left-half one");
    }
}

void ShopSetsNarrowFootprints()
{
    // The workshop needs two work positions: 420 x 680 cannot hold them and is refused cleanly, with no geometry.
    const InteriorLayout refused = GenerateInteriorLayout(SetInput(FurnitureSet::Workshop, 420, 680, 100));
    Expect(!refused.Ok() && refused.Status == Refusal::FurnitureDoesNotFit && RefusalCode(refused.Status) == "furniture_does_not_fit", "narrow workshop refused");
    Expect(refused.Rooms.empty() && refused.Pieces.empty() && refused.Anchors.empty() && refused.Routes.empty(), "a refusal carries no geometry");
    // General goods still fits the narrowest shop and builds a fully valid layout with a browse point.
    const InteriorLayout goods = GenerateInteriorLayout(SetInput(FurnitureSet::GeneralGoods, 420, 680, 100));
    ExpectContracts(goods, "narrow general goods");
    ExpectAnchorsReachable(goods, "narrow general goods");
    Expect(FindAnchor(goods, "building.test.shop.anchor.browse_shelf") != nullptr || FindAnchor(goods, "building.test.shop.anchor.browse_bins") != nullptr,
           "narrow general goods keeps a browse point");
    // Footprints below the shop minima keep their shape refusals.
    Expect(GenerateInteriorLayout(SetInput(FurnitureSet::Workshop, 380, 700, 150)).Status == Refusal::WidthTooNarrow, "too narrow for any shop");
    Expect(GenerateInteriorLayout(SetInput(FurnitureSet::GeneralGoods, 700, 600, 250)).Status == Refusal::DepthTooShallow, "too shallow for any shop");
    // The sets belong to shops only, and shop sets do not belong in homes.
    for (const FurnitureSet set : {FurnitureSet::GeneralGoods, FurnitureSet::Workshop})
    {
        InteriorInput home = HomeInput(Variant::CompactResidence, 700, 700, 350);
        home.Furniture = set;
        Expect(GenerateInteriorLayout(home).Status == Refusal::FurnitureSetMismatch, "shop set in a home");
        InteriorInput shared = HomeInput(Variant::SharedEntry, 900, 800, 300);
        shared.Furniture = set;
        Expect(GenerateInteriorLayout(shared).Status == Refusal::FurnitureSetMismatch, "shop set in a shared-entry building");
    }
    // Every footprint either builds with every contract or is refused for shape or fit: never an invalid layout.
    int built[2] = {0, 0};
    int doesNotFit = 0;
    int index = 0;
    for (const FurnitureSet set : {FurnitureSet::GeneralGoods, FurnitureSet::Workshop})
    {
        for (int width = 400; width <= 1300; width += 25)
        {
            for (int depth = 600; depth <= 1100; depth += 50)
            {
                for (const int fraction : {12, 25, 38, 50, 62, 75, 88})
                {
                    for (const bool mirror : {false, true})
                    {
                        InteriorInput in = SetInput(set, width, depth, width * fraction / 100);
                        in.MirrorU = mirror;
                        const InteriorLayout l = GenerateInteriorLayout(in);
                        if (l.Ok())
                        {
                            ++built[index];
                            ExpectContracts(l, "set sweep layout");
                            if ((width == 450 || width == 600 || width == 1000) && (depth == 700 || depth == 900) && (fraction == 25 || fraction == 75)) ExpectAnchorsReachable(l, "set sweep layout");
                        }
                        else
                        {
                            doesNotFit += l.Status == Refusal::FurnitureDoesNotFit ? 1 : 0;
                            Expect(l.Status == Refusal::WidthTooNarrow || l.Status == Refusal::DepthTooShallow || l.Status == Refusal::EntranceOffFacade ||
                                       l.Status == Refusal::FurnitureDoesNotFit,
                                   "a set is refused only for shape, entrance or fit");
                        }
                    }
                }
            }
        }
        ++index;
    }
    std::cerr << "shop sets sweep: goods built " << built[0] << ", workshop built " << built[1] << ", does-not-fit " << doesNotFit << '\n';
    Expect(built[0] > 800 && built[1] > 800, "most plausible footprints build");
}

void UnitDoorIdFields()
{
    InteriorInput shared = HomeInput(Variant::SharedEntry, 1100, 800, 550);
    shared.EntranceDoorId = "door.test.entry";
    shared.UnitDoorIdA = "door.test.unit_a";
    shared.UnitDoorIdB = "door.test.unit_b";
    Expect(GenerateInteriorLayout(shared).Ok(), "a shared entry accepts two distinct unit door ids");
    InteriorInput only = shared;
    only.UnitDoorIdB.clear();
    Expect(GenerateInteriorLayout(only).Ok(), "either unit door id may be left empty");
    InteriorInput twin = shared;
    twin.UnitDoorIdB = twin.UnitDoorIdA;
    Expect(GenerateInteriorLayout(twin).Status == Refusal::UnitDoorIdInvalid, "the same id on both units");
    InteriorInput clash = shared;
    clash.UnitDoorIdA = clash.EntranceDoorId;
    Expect(GenerateInteriorLayout(clash).Status == Refusal::UnitDoorIdInvalid, "a unit id equal to the entrance id");
    InteriorInput clashB = shared;
    clashB.UnitDoorIdB = clashB.EntranceDoorId;
    Expect(GenerateInteriorLayout(clashB).Status == Refusal::UnitDoorIdInvalid, "the second unit's id equal to the entrance id");
    InteriorInput home = HomeInput(Variant::CompactResidence, 700, 700, 350);
    home.UnitDoorIdA = "door.test.unit_a";
    Expect(GenerateInteriorLayout(home).Status == Refusal::UnitDoorIdInvalid, "unit door ids on a variant without units");
    InteriorInput shop = ShopInput(700, 700, 250);
    shop.UnitDoorIdB = "door.test.unit_b";
    Expect(GenerateInteriorLayout(shop).Status == Refusal::UnitDoorIdInvalid, "unit door ids on a shop");
    Expect(RefusalCode(Refusal::UnitDoorIdInvalid) == "unit_door_id_invalid", "stable refusal code");
    // The enum values the Unreal side and the catalogue rely on are frozen.
    Expect(static_cast<int>(FurnitureSet::None) == 0 && static_cast<int>(FurnitureSet::Residence) == 1 && static_cast<int>(FurnitureSet::Stationery) == 2 &&
               static_cast<int>(FurnitureSet::GeneralGoods) == 3 && static_cast<int>(FurnitureSet::Workshop) == 4,
           "furniture set values are frozen");
}
} // namespace

int main()
{
    StationeryOutputIsPinned();
    ShopSetsReference();
    ShopSetsMirroredAndDeterministic();
    ShopSetsNarrowFootprints();
    UnitDoorIdFields();
    std::cout << "LaneInteriorShopFurnitureTests: " << Assertions << " assertions passed\n";
    return 0;
}
