// F2: the lane map projection (LaneMap.h). Proves, portably, that the map is built from the authoritative lane data and nothing else:
// the generator port reproduces the frontage catalogue's pins, every kit-built and own-code building lands on its pinned footprint with
// its kind, name, entrance and door, the sealed ground floors stay sealed, door status follows the core's access rules, the result is
// deterministic and structurally valid, and a building appended through MapOptions is mapped like any other.

#include "Doors.h"
#include "LaneFrontageCatalogue.h"
#include "LaneMap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

using namespace PerfectWorld;
namespace Lm = PerfectWorld::LaneMap;

namespace
{
int MpAssertions = 0;
void MpExpect(bool condition, const std::string& message)
{
    ++MpAssertions;
    if (condition)
    {
        return;
    }
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

std::string MpSignature(const Lm::Map& map)
{
    std::ostringstream out;
    for (const Lm::Surface& s : map.Surfaces)
    {
        out << 's' << static_cast<int>(s.Kind);
        for (const Lm::Point& p : s.Outline) out << ' ' << p.X << ',' << p.Y;
        out << '\n';
    }
    for (const Lm::Building& b : map.Buildings)
    {
        out << 'b' << b.Id << '|' << b.Label << '|' << b.LabelKey << '|' << static_cast<int>(b.Kind) << '|' << b.Side << '|' << b.Storeys << '|' << b.Entrance.X
            << ',' << b.Entrance.Y << '|' << b.bEnterable;
        for (const Lm::Point& p : b.Footprint) out << ' ' << p.X << ',' << p.Y;
        out << '\n';
    }
    for (const Lm::MapDoor& d : map.Doors) out << 'd' << d.Id << '|' << d.BuildingId << '|' << d.Position.X << ',' << d.Position.Y << '|' << static_cast<int>(d.Access) << '\n';
    for (const Lm::PointOfInterest& p : map.Pois) out << 'p' << p.Id << '|' << p.Label << '|' << static_cast<int>(p.Kind) << '|' << p.Position.X << ',' << p.Position.Y << '\n';
    out << map.BoundsMin.X << ',' << map.BoundsMin.Y << ' ' << map.BoundsMax.X << ',' << map.BoundsMax.Y;
    return out.str();
}

const Lm::Building& MpBuilding(const Lm::Map& map, std::string_view id)
{
    const Lm::Building* b = Lm::FindBuilding(map, id);
    MpExpect(b != nullptr, "building exists: " + std::string(id));
    return *b;
}

void MpTestGenerator()
{
    MpExpect(Lm::LaneMapHash(138, 2, 11) == 2673104671u, "the generator hash reproduces the documented stationery seed");
    const std::vector<Lm::Frontage> rows = Lm::EnumerateFrontages();
    MpExpect(rows.size() == 19, "nineteen near frontages");
    MpExpect(std::count_if(rows.begin(), rows.end(), [](const Lm::Frontage& r) { return r.Side < 0; }) == 10, "ten on the -Y side");
    MpExpect(std::count_if(rows.begin(), rows.end(), [](const Lm::Frontage& r) { return r.Side > 0; }) == 9, "nine on the +Y side");

    for (const LaneInterior::BuiltFootprint& b : LaneInterior::BuiltFootprints())
    {
        const auto it = std::find_if(rows.begin(), rows.end(), [&b](const Lm::Frontage& r) { return r.Side == b.Side && r.Index == b.RowIndex; });
        MpExpect(it != rows.end() && it->X0Cm == b.X0Cm && it->X1Cm == b.X1Cm, "built footprint matches the generator: " + std::string(b.BuildingId));
    }
    for (const LaneInterior::FrontageEntry& e : LaneInterior::KitFrontageEntries())
    {
        const auto it = std::find_if(rows.begin(), rows.end(), [&e](const Lm::Frontage& r) { return r.Side == e.Side && r.Index == e.RowIndex; });
        MpExpect(it != rows.end(), "kit row exists: " + std::string(e.BuildingId));
        MpExpect(LaneInterior::FindFrontageByFootprint(it->Side, it->X0Cm, it->FrontAbsYCm(), it->WidthCm, it->DepthCm, it->SetbackCm, it->Storeys, it->Seed) == &e,
                 "the catalogue recognises the generated footprint: " + std::string(e.BuildingId));
    }
    for (const Lm::Frontage& r : rows)
    {
        MpExpect(r.X1Cm == r.X0Cm + r.WidthCm && r.WidthCm >= 300, "frontage width is consistent");
        const int alleyA = r.Side < 0 ? Lm::AlleyMinusAXCm : Lm::AlleyPlusAXCm;
        const int alleyB = r.Side < 0 ? Lm::AlleyMinusBXCm : Lm::AlleyPlusBXCm;
        MpExpect(r.X1Cm <= alleyA || r.X0Cm >= alleyB, "no frontage stands in its side's alley");
        MpExpect(r.X0Cm >= Lm::LaneX0Cm && r.X1Cm <= Lm::LaneX1Cm, "frontages stay within the lane");
    }
}

void MpTestMapShape()
{
    const Lm::Map map = Lm::BuildLaneMap();
    MpExpect(Lm::IsMapValid(map), "the lane map is structurally valid");
    MpExpect(MpSignature(map) == MpSignature(Lm::BuildLaneMap()), "the map is deterministic");

    MpExpect(map.Buildings.size() == 21, "nineteen frontages and two end masses");
    std::size_t byKind[Lm::BuildingKindCount] = {};
    for (const Lm::Building& b : map.Buildings) ++byKind[static_cast<std::size_t>(b.Kind)];
    MpExpect(byKind[static_cast<std::size_t>(Lm::BuildingKind::Home)] == 6, "six homes (care home, opening home, caretaker flat, tall flats, row house, old house)");
    MpExpect(byKind[static_cast<std::size_t>(Lm::BuildingKind::Shop)] == 8, "eight shops");
    MpExpect(byKind[static_cast<std::size_t>(Lm::BuildingKind::Workshop)] == 2, "two workshops (repair, tailor)");
    MpExpect(byKind[static_cast<std::size_t>(Lm::BuildingKind::School)] == 0, "no school on the lane yet");
    MpExpect(byKind[static_cast<std::size_t>(Lm::BuildingKind::Sealed)] == 5, "three sealed frontages and two end masses");

    std::size_t roads = 0, alleys = 0, footpaths = 0;
    for (const Lm::Surface& s : map.Surfaces)
    {
        roads += s.Kind == Lm::SurfaceKind::Road;
        alleys += s.Kind == Lm::SurfaceKind::Alley;
        footpaths += s.Kind == Lm::SurfaceKind::Footpath;
        MpExpect(Lm::IsRectangle(s.Outline), "every surface is a counter-clockwise rectangle");
    }
    MpExpect(roads == 1 && alleys == 2 && footpaths == 19, "one road, two alleys, a footpath per frontage");

    MpExpect(map.BoundsMin.X == -3300.0 && map.BoundsMax.X == 7300.0, "bounds span the end masses");
    MpExpect(map.BoundsMin.Y == -1500.0 && map.BoundsMax.Y == 1500.0, "bounds span the alleys");

    // Footprints never intrude on the road, an alley or a footpath.
    for (const Lm::Building& b : map.Buildings)
    {
        const double x0 = b.Footprint[0].X, y0 = b.Footprint[0].Y, x1 = b.Footprint[2].X, y1 = b.Footprint[2].Y;
        for (const Lm::Surface& s : map.Surfaces)
        {
            const double sx0 = s.Outline[0].X, sy0 = s.Outline[0].Y, sx1 = s.Outline[2].X, sy1 = s.Outline[2].Y;
            const bool overlap = x0 < sx1 && sx0 < x1 && y0 < sy1 && sy0 < y1;
            MpExpect(!overlap, "building " + b.Id + " stays off every surface");
        }
        MpExpect(b.Centre.X == (x0 + x1) / 2.0 && b.Centre.Y == (y0 + y1) / 2.0, "the centre is the footprint's centre");
    }
}

void MpTestBuildings()
{
    const Lm::Map map = Lm::BuildLaneMap();

    const Lm::Building& stationery = MpBuilding(map, "building.nabapara.stationery");
    MpExpect(stationery.Kind == Lm::BuildingKind::Shop && stationery.Label == "Stationery shop" && stationery.LabelKey == "map.building.stationery",
             "stationery: kind and label");
    MpExpect(stationery.Footprint[0] == Lm::Point{-100.0, 460.0} && stationery.Footprint[2] == Lm::Point{600.0, 1160.0},
             "stationery: 700 x 700 on the +Y side behind a 60 cm setback");
    MpExpect(stationery.Entrance == Lm::Point{150.0, 460.0} && stationery.Side == 1 && stationery.bEnterable, "stationery: entrance at U 250");

    const Lm::Building& home = MpBuilding(map, "building.nabapara.opening_home");
    MpExpect(home.Kind == Lm::BuildingKind::Home && home.Side == -1 && home.Storeys == 5, "opening home: a five-storey home on the -Y side");
    MpExpect(home.Footprint[0] == Lm::Point{-300.0, -1300.0} && home.Footprint[2] == Lm::Point{700.0, -400.0}, "opening home: 1000 x 900, no setback");
    MpExpect(home.Entrance == Lm::Point{400.0, -400.0}, "opening home: the front door at U 700");

    const Lm::Building& tea = MpBuilding(map, "building.nabapara.shanti_chai");
    MpExpect(tea.Label == "Shanti Chai Corner" && tea.Kind == Lm::BuildingKind::Shop && tea.Entrance == Lm::Point{950.0, 520.0}, "tea shop: name and entrance at U 350, 120 cm setback");

    const Lm::Building& care = MpBuilding(map, "building.nabapara.care_home");
    MpExpect(care.Kind == Lm::BuildingKind::Home && care.Entrance == Lm::Point{-1165.0, -400.0}, "care home: a home with its door at U 235");

    const Lm::Building& workshop = MpBuilding(map, "building.nabapara.repair_workshop");
    MpExpect(workshop.Kind == Lm::BuildingKind::Workshop && MpBuilding(map, "building.nabapara.tailor_shop").Kind == Lm::BuildingKind::Workshop,
             "workshop furniture sets map to workshops");
    MpExpect(MpBuilding(map, "building.nabapara.hardware_store").Kind == Lm::BuildingKind::Shop, "the hardware store is a shop");
    MpExpect(MpBuilding(map, "building.nabapara.tall_flats").Kind == Lm::BuildingKind::Home &&
                 MpBuilding(map, "building.nabapara.old_house").Kind == Lm::BuildingKind::Home,
             "a shared entry and a compact residence are homes");

    // The mirrored row house keeps its entrance at U 450 in the frame (the layout is what is mirrored).
    const Lm::Building& row = MpBuilding(map, "building.nabapara.tea_row_house");
    MpExpect(row.Entrance == Lm::Point{1850.0, 520.0}, "the mirrored row house has its door at U 450 from the frontage's low-X corner");

    // Sealed ground floors: three frontages (generic ids) and the two end masses.
    std::set<std::string> sealed;
    for (const Lm::Building& b : map.Buildings)
    {
        if (b.Kind != Lm::BuildingKind::Sealed) continue;
        sealed.insert(b.Id);
        MpExpect(!b.bEnterable && !b.bHasEntrance && b.Label.empty() && b.LabelKey.empty(), "sealed buildings have no door and no label: " + b.Id);
    }
    MpExpect(sealed.count("building.nabapara.sealed_minus_7") == 1 && sealed.count("building.nabapara.sealed_minus_8") == 1 &&
                 sealed.count("building.nabapara.sealed_plus_0") == 1 && sealed.count("building.nabapara.west_end_block") == 1 &&
                 sealed.count("building.nabapara.east_end_block") == 1,
             "the sealed set is the three inventory frontages and the two end masses");
    const Lm::Building& west = MpBuilding(map, "building.nabapara.west_end_block");
    MpExpect(west.Side == 0 && west.Footprint[0] == Lm::Point{-3300.0, -1300.0} && west.Footprint[2] == Lm::Point{-2400.0, 1300.0}, "the west end mass closes the lane");

    // Every named building is enterable and every enterable building is named.
    for (const Lm::Building& b : map.Buildings)
    {
        MpExpect(b.bEnterable == !b.Label.empty() || b.Kind == Lm::BuildingKind::Sealed, "named exactly when enterable: " + b.Id);
        MpExpect(Lm::FindBuildingAt(map, b.Centre) == &b, "FindBuildingAt finds the building at its centre: " + b.Id);
    }
    MpExpect(Lm::FindBuildingAt(map, {0.0, 0.0}) == nullptr, "the road is no building");
}

void MpTestDoorsAndPois()
{
    const Lm::Map map = Lm::BuildLaneMap();
    MpExpect(map.Doors.size() == 16, "sixteen exterior front doors");
    for (const Lm::MapDoor& d : map.Doors)
    {
        const DoorDef* authored = FindDoor(d.Id);
        MpExpect(authored != nullptr && authored->Access == d.Access && authored->bInitiallyOpen == d.bInitiallyOpen, "map door mirrors the authored door: " + d.Id);
        const Lm::Building& b = MpBuilding(map, d.BuildingId);
        MpExpect(d.Position == b.Entrance, "the door is at the building's entrance: " + d.Id);
        MpExpect(d.Outward.X == 0.0 && d.Outward.Y == -static_cast<double>(b.Side), "the door faces the road: " + d.Id);
    }
    MpExpect(Lm::FindMapDoor(map, "door.nabapara.care_front")->Access == DoorAccess::Private, "the care home front door is private");
    MpExpect(Lm::FindMapDoor(map, "door.nabapara.shanti_chai_shutter")->Access == DoorAccess::BusinessHours, "the tea shop shutter keeps business hours");
    MpExpect(Lm::FindMapDoor(map, "door.nabapara.tall_flats_front")->Access == DoorAccess::Public, "the tall flats' stair passage is public");
    MpExpect(Lm::FindMapDoor(map, "door.nabapara.tall_flats_unit_a") == nullptr, "interior doors are not on the map");

    MpExpect(map.Pois.size() == 20, "a point of interest per named building (16) plus two alleys and two lane ends");
    std::size_t homes = 0, shops = 0, workshops = 0, alleys = 0, ends = 0;
    for (const Lm::PointOfInterest& p : map.Pois)
    {
        homes += p.Kind == Lm::PoiKind::Home;
        shops += p.Kind == Lm::PoiKind::Shop;
        workshops += p.Kind == Lm::PoiKind::Workshop;
        alleys += p.Kind == Lm::PoiKind::Alley;
        ends += p.Kind == Lm::PoiKind::LaneEnd;
        MpExpect(p.LabelKey.starts_with("map."), "poi label key: " + p.Id);
    }
    MpExpect(homes == 6 && shops == 8 && workshops == 2 && alleys == 2 && ends == 2, "poi kinds follow the buildings");
}

void MpTestDoorStatus()
{
    const Lm::Map map = Lm::BuildLaneMap();
    const Lm::MapDoor& stationery = *Lm::FindMapDoor(map, "door.nabapara.stationery_front"); // 08:00-13:00 and 15:00-19:30

    const Lm::DoorStatus open = Lm::QueryDoorStatus(stationery, nullptr, 600);
    MpExpect(open.Availability == Lm::DoorAvailability::OpenToAll && open.MinutesUntilOpening == -1 && !open.bLeafOpen, "open at 10:00");
    MpExpect(open.WindowOpenMinute == 480 && open.WindowCloseMinute == 780, "the morning window is reported");

    const Lm::DoorStatus noon = Lm::QueryDoorStatus(stationery, nullptr, 800);
    MpExpect(noon.Availability == Lm::DoorAvailability::ClosedForNow && noon.MinutesUntilOpening == 100, "closed at 13:20, opens in 100 minutes");
    MpExpect(noon.WindowOpenMinute == 900 && noon.WindowCloseMinute == 1170, "the next window is the afternoon one");

    const Lm::DoorStatus night = Lm::QueryDoorStatus(stationery, nullptr, 1300);
    MpExpect(night.Availability == Lm::DoorAvailability::ClosedForNow && night.MinutesUntilOpening == 620, "closed at 21:40, opens tomorrow in 620 minutes");
    MpExpect(night.WindowOpenMinute == 480, "tomorrow's first window");

    MpExpect(Lm::QueryDoorStatus(stationery, nullptr, -5).Availability == Lm::DoorAvailability::ClosedForNow, "an invalid minute is read as midnight");
    MpExpect(Lm::QueryDoorStatus(*Lm::FindMapDoor(map, "door.nabapara.care_front"), nullptr, 600).Availability == Lm::DoorAvailability::PrivateKnock, "a dwelling offers a knock");
    MpExpect(Lm::QueryDoorStatus(*Lm::FindMapDoor(map, "door.nabapara.tall_flats_front"), nullptr, 30).Availability == Lm::DoorAvailability::OpenToAll, "a public door is open at night");

    // The live state wins over the authored start; an unknown id reads as locked and closed.
    DoorState live = MakeInitialDoorState();
    MpExpect(!Lm::QueryDoorStatus(stationery, &live, 600).bLeafOpen, "the live leaf starts closed");
    MpExpect(ApplyDoorSettlement(live, "door.nabapara.stationery_front", true, 1) == DoorResult::Ok, "the leaf settles open");
    MpExpect(Lm::QueryDoorStatus(stationery, &live, 600).bLeafOpen, "the live state is read");
    Lm::MapDoor stranger = stationery;
    stranger.Id = "door.nabapara.nowhere";
    const Lm::DoorStatus unknown = Lm::QueryDoorStatus(stranger, &live, 600);
    MpExpect(!unknown.bLeafOpen && unknown.Availability == Lm::DoorAvailability::Locked, "an unknown door is locked");

    // Every business door reports a sane window for every minute of the day.
    for (const Lm::MapDoor& d : map.Doors)
    {
        for (int minute = 0; minute < DoorMinutesPerDay; minute += 7)
        {
            const Lm::DoorStatus s = Lm::QueryDoorStatus(d, nullptr, minute);
            if (d.Access != DoorAccess::BusinessHours)
            {
                MpExpect(s.MinutesUntilOpening == -1 && s.WindowOpenMinute == -1, "non-business doors have no hours: " + d.Id);
                continue;
            }
            MpExpect(s.WindowOpenMinute >= 0 && s.WindowCloseMinute > s.WindowOpenMinute, "a business door reports a window: " + d.Id);
            MpExpect((s.Availability == Lm::DoorAvailability::OpenToAll) == (s.MinutesUntilOpening == -1), "open exactly when nothing is awaited: " + d.Id);
            if (s.Availability == Lm::DoorAvailability::OpenToAll)
            {
                MpExpect(minute >= s.WindowOpenMinute && minute < s.WindowCloseMinute, "an open minute is inside its window: " + d.Id);
            }
            else
            {
                MpExpect(s.MinutesUntilOpening == DoorMinutesUntilOpening(*FindDoor(d.Id), minute), "the wait is the core's: " + d.Id);
            }
        }
    }
}

void MpTestExtraBuilding()
{
    const Lm::ExtraBuilding school[] = {{"building.nabapara.school", "School", "map.building.school", Lm::BuildingKind::School, 7400.0, 200.0, 8200.0, 900.0, 3, true, 7800.0, 200.0, {}}};
    Lm::MapOptions options;
    options.Extra = school;
    const Lm::Map map = Lm::BuildLaneMap(options);
    MpExpect(Lm::IsMapValid(map), "a map with an extra building is valid");
    const Lm::Building& b = MpBuilding(map, "building.nabapara.school");
    MpExpect(b.Kind == Lm::BuildingKind::School && b.Label == "School" && b.bEnterable && b.Side == 1, "the school is mapped as a school");
    MpExpect(map.BoundsMax.X == 8200.0, "the bounds grow to hold it");
    MpExpect(std::any_of(map.Pois.begin(), map.Pois.end(), [](const Lm::PointOfInterest& p) { return p.Kind == Lm::PoiKind::School && p.BuildingId == "building.nabapara.school"; }),
             "and it gets its point of interest");
    MpExpect(map.Buildings.size() == 22 && map.Doors.size() == 16, "one building more; no door was authored for it");

    // A building placed on top of another is refused by the validity check.
    const Lm::ExtraBuilding clash[] = {{"building.nabapara.clash", "Clash", "map.building.clash", Lm::BuildingKind::School, -200.0, 500.0, 300.0, 900.0, 1, false, 0.0, 0.0, {}}};
    options.Extra = clash;
    MpExpect(!Lm::IsMapValid(Lm::BuildLaneMap(options)), "overlapping footprints are invalid");
}

void MpTestKindIds()
{
    for (const auto kind : {Lm::BuildingKind::Home, Lm::BuildingKind::Shop, Lm::BuildingKind::School, Lm::BuildingKind::Workshop, Lm::BuildingKind::Sealed})
    {
        MpExpect(!Lm::BuildingKindId(kind).empty(), "building kind id");
    }
    for (const auto kind : {Lm::PoiKind::Home, Lm::PoiKind::Shop, Lm::PoiKind::Workshop, Lm::PoiKind::School, Lm::PoiKind::Alley, Lm::PoiKind::LaneEnd})
    {
        MpExpect(!Lm::PoiKindId(kind).empty(), "poi kind id");
    }
    MpExpect(!Lm::IsRectangle({{0, 0}, {1, 0}, {1, 1}}), "three points are no rectangle");
    MpExpect(!Lm::IsRectangle({{0, 0}, {0, 1}, {1, 1}, {1, 0}}), "a clockwise rectangle is refused");
    MpExpect(Lm::IsRectangle({{0, 0}, {1, 0}, {1, 1}, {0, 1}}), "a counter-clockwise rectangle is accepted");
}
} // namespace

int main()
{
    MpExpect(ValidateDoorTable(), "the authored door table is valid");
    MpTestGenerator();
    MpTestMapShape();
    MpTestBuildings();
    MpTestDoorsAndPois();
    MpTestDoorStatus();
    MpTestExtraBuilding();
    MpTestKindIds();
    std::cout << "LaneMapTests: " << MpAssertions << " assertions passed\n";
    return 0;
}
