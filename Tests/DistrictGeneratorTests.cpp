#include "CityData.h"
#include "ContentRegistry.h"
#include "DistrictGenerator.h"
#include "Json.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// The measuring helpers (CheckGuarantees, Sweep, the range structs) live in the support header.
#include "DistrictTestSupport.h"

namespace
{
const char* const SmallWorld[] = {
    R"({"type":"nation","id":"nation.t","name":"T","capitalCity":"city.t","gateAuthority":"institution.t_nga",
        "realmHomeRegion":"realm.region.t","stageOneCities":["city.t"],
        "towerExamination":{"centralHall":"institution.t_hall","certifiedHallPerCity":true}})",
    R"({"type":"realm_region","id":"realm.region.t","name":"T home","floor":1,"anchors":["gate.t.anchor"]})",
    R"({"type":"institution","id":"institution.t_nga","name":"NGA","kind":"gate_authority","city":"city.t"})",
    R"({"type":"institution","id":"institution.t_hall","name":"Hall","kind":"examination_hall","city":"city.t"})",
    R"({"type":"city","id":"city.t","name":"Testville","nation":"nation.t","state":"Test State","role":"A test city",
        "districts":[
          {"id":"district.t.home","name":"Home","kind":"authored","fictional":true},
          {"id":"district.t.core","name":"Core","kind":"generated","archetype":"old_dense_core","seed":11},
          {"id":"district.t.market","name":"Market","kind":"generated","archetype":"market","seed":4294967295},
          {"id":"district.t.belt","name":"Belt","kind":"generated","archetype":"apartment_belt","seed":0},
          {"id":"district.t.river","name":"River","kind":"generated","archetype":"riverfront","seed":7},
          {"id":"district.t.park","name":"Park","kind":"generated","archetype":"park","seed":8}],
        "gateSites":[
          {"id":"gate.t.anchor","name":"Anchor Gate","district":"district.t.river","rank":"D","status":"contained","realmAnchor":true},
          {"id":"gate.t.canal","name":"Canal","district":"district.t.home","location":"loc_canal","rank":"F","status":"sealed","realmAnchor":false},
          {"id":"gate.t.dock","name":"Dock","district":"district.t.river","rank":"B","status":"open","realmAnchor":false}]})"};
} // namespace

int main(int argc, char** argv)
{
    // Names and the fallback table.
    {
        for (int i = 0; i < DistrictArchetypeCount; ++i)
        {
            const auto a = static_cast<DistrictArchetype>(i);
            Check(ParseDistrictArchetype(DistrictArchetypeName(a)) == a, "an archetype name round-trips");
            Check(IsDistrictLayoutImplemented(DistrictFallbackLayout(a)), "a fallback is an implemented layout");
        }
        Check(!ParseDistrictArchetype("castle").has_value() && !ParseDistrictArchetype("").has_value(), "unknown names are refused");
        // Every archetype now has its own layout, so no fallback remains: each is its own layout, uncapped.
        for (int i = 0; i < DistrictArchetypeCount; ++i)
        {
            const auto a = static_cast<DistrictArchetype>(i);
            Check(IsDistrictLayoutImplemented(a) && DistrictFallbackLayout(a) == a && DistrictFallbackStoreyCap(a) == 255,
                  "all twelve layouts are implemented and none falls back");
        }
    }

    // Every archetype: guarantees and per-archetype ranges over 200 seeds each.
    const Totals core = Sweep(DistrictArchetype::OldDenseCore, true);
    const Totals belt = Sweep(DistrictArchetype::ApartmentBelt, true);
    const Totals market = Sweep(DistrictArchetype::Market, true);
    const Totals river = Sweep(DistrictArchetype::Riverfront, true);
    const Totals rail = Sweep(DistrictArchetype::RailYard, true);
    const Totals port = Sweep(DistrictArchetype::Port, true);
    const Totals campus = Sweep(DistrictArchetype::Campus, true);
    const Totals industrial = Sweep(DistrictArchetype::Industrial, true);
    const Totals suburb = Sweep(DistrictArchetype::Suburb, true);
    const Totals government = Sweep(DistrictArchetype::GovernmentQuarter, true);
    const Totals informal = Sweep(DistrictArchetype::InformalSettlement, true);
    const Totals park = Sweep(DistrictArchetype::Park, true);

    Check(core.MinStoreys >= 2 && core.MaxStoreys <= 5 && core.MaxStoreys == 5, "old dense core lots are 2 to 5 storeys");
    Check(belt.MinStoreys >= 6 && belt.MaxStoreys <= 14, "apartment belt lots are 6 to 14 storeys");
    Check(market.MinStoreys >= 1 && market.MaxStoreys <= 3, "market lots are 1 to 3 storeys");

    Check(belt.MinFullRows >= 12 && belt.MaxFullRows <= 16, "the apartment belt is a regular grid: 12 to 16 full cross streets");
    Check(core.MaxFullRows <= 2, "old dense core lanes wander, so almost no row is one straight street");

    Check(core.MinOpen == 0 && core.MaxOpen == 0, "old dense core has no open ground: every lot is built");
    Check(belt.MinOpen >= 300, "apartment belt has podium gardens between its towers");
    Check(market.MinLargestOpen >= 100 && market.MaxLargestOpen <= 256, "the market has a central square of 100 to 256 cells");
    Check(market.MaxCentreOffset <= 6, "the square is within 3 cells of the centre");
    Check(core.MaxLargestOpen == 0, "old dense core has no square");

    // Storey ranges of the nine layouts added after the first three (documented in DistrictGenerator.h).
    Check(river.MinStoreys >= 2 && river.MaxStoreys <= 6 && river.MaxStoreys == 6, "riverfront lots are 2 to 6 storeys");
    Check(rail.MinStoreys >= 1 && rail.MaxStoreys <= 3 && rail.MaxStoreys == 3, "rail yard sheds are 1 to 3 storeys");
    Check(port.MinStoreys >= 1 && port.MaxStoreys <= 5 && port.MaxStoreys == 5, "port buildings are 1 to 5 storeys");
    Check(campus.MinStoreys >= 2 && campus.MaxStoreys <= 6 && campus.MaxStoreys == 6, "campus halls are 2 to 6 storeys");
    Check(industrial.MinStoreys >= 1 && industrial.MaxStoreys <= 4 && industrial.MaxStoreys == 4, "industrial sheds are 1 to 4 storeys");
    Check(suburb.MinStoreys >= 1 && suburb.MaxStoreys <= 2 && suburb.MaxStoreys == 2, "suburban houses are 1 to 2 storeys");
    Check(government.MinStoreys >= 3 && government.MaxStoreys <= 7 && government.MaxStoreys == 7, "government buildings are 3 to 7 storeys");
    Check(informal.MinStoreys >= 1 && informal.MaxStoreys <= 3 && informal.MaxStoreys == 3, "informal settlement huts are 1 to 3 storeys");
    Check(park.MinStoreys >= 1 && park.MaxStoreys <= 2 && park.MaxStoreys == 2, "park pavilions are 1 to 2 storeys");

    // Riverfront: a river band 6 to 11 wide along one edge, with a promenade, ghat steps and a bridge.
    Check(river.Water.Min >= 300 && river.Water.Max <= 700 && river.WaterOnOneEdge && river.WaterSpan.Max <= 12,
          "riverfront water is one band, 6 to 11 cells wide, along one edge");
    Check(river.StreetsByWater.Min >= 40, "riverfront has a promenade street along the water");
    Check(river.OpenByWater.Min >= 20, "riverfront has ghat steps (open cells) going down to the water");
    Check(river.Bridges.Min >= 4 && river.Rail.Max == 0, "riverfront has a bridge across the water to the edge");

    // Port: a wider sea, piers with dead-end tips in it, big blocks of yards.
    Check(port.Water.Min >= 800 && port.Water.Min > river.Water.Max && port.WaterOnOneEdge && port.WaterSpan.Min >= 14,
          "port sea is 14 to 20 cells wide along one edge, wider than any river");
    Check(port.PierTips.Min >= 4, "the port has piers whose tips are dead ends in the water");
    Check(port.StreetsByWater.Min >= 90 && port.Open > 0 && port.MinOpen >= 1000, "the port has a quay and wide open yards");
    Check(port.Rail.Max == 0 && river.PierTips.Max == 0, "piers belong to the port, not the riverfront");

    // Rail yard: a band of rail right across, platforms, and level crossings that keep the streets joined.
    Check(rail.Rail.Min >= 340 && rail.Rail.Max <= 500 && rail.RailLines.Min >= 6 && rail.RailLines.Max <= 8,
          "the rail yard is a band of 6 to 8 rail lines right across the district");
    Check(rail.Crossings.Min >= 6, "the rail yard has level crossings (street cells with rail either side)");
    Check(rail.MinOpen >= 100, "the rail yard has platforms (open cells in the band)");

    // Campus: a great quad, lawns.
    Check(campus.MinLargestOpen >= 256 && campus.MaxLargestOpen <= 400 && campus.MaxCentreOffset <= 6,
          "the campus quad is 256 to 400 cells at the centre, larger than any market square");
    Check(campus.MinOpen > 1500 && campus.MaxStreets < market.MinStreets, "a campus is mostly lawn, with fewer streets than a market");

    // Industrial: long sheds.
    Check(industrial.LongestRun.Min >= 12 && industrial.LongestRun.Min > core.LongestRun.Max,
          "industrial sheds run 12 or more cells in a straight line, longer than any old core block");
    Check(industrial.MinOpen > 1500, "industrial blocks hold open yards");

    // Suburb: cul-de-sacs, detached houses in gardens.
    Check(suburb.DeadEnds.Min >= 3 && core.DeadEnds.Max == 0 && belt.DeadEnds.Max == 0 && market.DeadEnds.Max == 0,
          "the suburb has cul-de-sac dead ends that the grid layouts never have");
    Check(suburb.MinOpen >= 2500 && suburb.MaxBuildings <= 700, "a suburb is mostly open garden between few houses");

    // Government quarter: a 3-wide boulevard and long lawns.
    Check(government.BoulevardLines.Min >= 1 && belt.BoulevardLines.Max == 0 && core.BoulevardLines.Max == 0,
          "the government quarter has a boulevard three cells wide running right across");
    Check(government.MinLargestOpen >= 250, "the government quarter has a long lawn of at least 250 cells");

    // Informal settlement: dense, narrow, dead-end nubs, nothing tall.
    Check(informal.DeadEnds.Min >= 8, "the informal settlement has many dead-end nubs");
    Check(informal.MaxLargestOpen <= 5 && informal.MinOpen >= 20 && informal.MaxOpen <= 100,
          "the informal settlement has only small open yards");
    Check(informal.MinStreets > belt.MaxStreets && informal.MaxStoreys < core.MaxStoreys, "informal lanes are tight and the roofs low");

    // Park: nearly all open.
    Check(park.MinOpen >= 3000 && park.MaxBuildings <= 120 && park.MinBuildings >= 20, "a park is nearly all open, with a few pavilions");

    // Water and rail only where the archetype owns them.
    for (const Totals* t : {&core, &belt, &market, &campus, &industrial, &suburb, &government, &informal, &park})
    {
        Check(t->Water.Max == 0 && t->Rail.Max == 0, "dry archetypes have no water or rail");
    }
    Check(river.Water.Min > 0 && port.Water.Min > 0 && rail.Rail.Min > 0 && rail.Water.Max == 0, "water and rail appear in their own archetypes");

    // Measurably different: street share, building share, height.
    Check(core.Streets > belt.Streets, "old dense core has more street than the apartment belt");
    Check(core.MinStreets > belt.MaxStreets, "the street share ranges of the two do not overlap");
    Check(belt.StoreySum > 2 * core.StoreySum, "apartment towers are more than twice as tall as old core lots");
    Check(core.StoreySum > market.StoreySum, "old core lots are taller than market stalls");

    // Different seeds differ; different archetypes differ on the same seed.
    for (int k = 0; k < DistrictArchetypeCount; ++k)
    {
        const auto a = static_cast<DistrictArchetype>(k);
        for (int i = 0; i + 1 < SeedCount; ++i)
        {
            Check(!Same(GenerateDistrict(a, SeedOf(i)), GenerateDistrict(a, SeedOf(i + 1))), "different seeds give different districts");
        }
        const GeneratedDistrict d = GenerateDistrict(a, SeedOf(3));
        Check(d.Archetype == a && d.Layout == a, "every archetype reports its own layout");
    }
    for (int i = 0; i < SeedCount; ++i)
    {
        std::vector<GeneratedDistrict> all;
        for (int k = 0; k < DistrictArchetypeCount; ++k)
        {
            all.push_back(GenerateDistrict(static_cast<DistrictArchetype>(k), SeedOf(i)));
        }
        for (std::size_t a = 0; a < all.size(); ++a)
        {
            for (std::size_t b = a + 1; b < all.size(); ++b)
            {
                Check(!Same(all[a], all[b]), "different archetypes give different districts");
            }
        }
    }

    // Edge seeds.
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{4294967295ULL}, ~std::uint64_t{0}})
    {
        for (int k = 0; k < DistrictArchetypeCount; ++k)
        {
            const GeneratedDistrict d = GenerateDistrict(static_cast<DistrictArchetype>(k), seed);
            CheckGuarantees(d);
            Check(Same(d, GenerateDistrict(static_cast<DistrictArchetype>(k), seed)), "edge seeds are deterministic");
        }
    }

    // A small city from JSON text, read with CityData, every generated district generated.
    {
        // The loader wants /<type>/<id>.json, so build each path from the record's own type and id.
        std::vector<ContentFile> files;
        for (const char* text : SmallWorld)
        {
            const JsonParseResult parsed = ParseJson(text);
            Check(parsed.Value.has_value(), "the small world's JSON parses");
            files.push_back({"/memory/" + parsed.Value->Find("type")->String + "/" + parsed.Value->Find("id")->String + ".json", text});
        }
        const ContentLoadResult loaded = LoadContent(files);
        for (const ContentIssue& issue : loaded.Issues)
        {
            std::cerr << issue.File << " " << issue.Id << ": " << issue.Message << '\n';
        }
        Check(loaded.Ok(), "the small world passes the content validator");

        const std::optional<CityRecord> city = ReadCity(loaded.Db, "city.t");
        Check(city.has_value(), "the city is read");
        Check(city->Id == "city.t" && city->Name == "Testville" && city->NationId == "nation.t" && city->State == "Test State" &&
                  city->Role == "A test city",
              "city fields");
        Check(city->Districts.size() == 6 && city->GateSites.size() == 3, "its districts and Gate sites are all read");

        const DistrictRecord* home = city->FindDistrict("district.t.home");
        Check(home != nullptr && home->Kind == DistrictKind::Authored && home->Fictional && !home->Archetype.has_value(), "authored district");
        Check(!GenerateCityDistrict(*home).has_value(), "an authored district has no generated grid");
        const DistrictRecord* market = city->FindDistrict("district.t.market");
        Check(market != nullptr && market->Kind == DistrictKind::Generated && !market->Fictional &&
                  market->Archetype == DistrictArchetype::Market && market->Seed == 4294967295U,
              "generated district, with the largest seed");
        Check(city->FindDistrict("district.t.nowhere") == nullptr, "an unknown district is not found");

        const GateSiteRecord* anchor = city->GateSitesIn("district.t.river")[0];
        Check(city->GateSitesIn("district.t.river").size() == 2 && city->GateSitesIn("district.t.home").size() == 1, "Gate sites by district");
        Check(anchor->Id == "gate.t.anchor" && anchor->Rank == HazardRank::D && anchor->Status == GateSiteStatus::Contained && anchor->RealmAnchor &&
                  anchor->Location.empty(),
              "an anchor Gate site");
        const GateSiteRecord* canal = city->GateSitesIn("district.t.home")[0];
        Check(canal->Location == "loc_canal" && canal->Rank == HazardRank::F && canal->Status == GateSiteStatus::Sealed && !canal->RealmAnchor,
              "a sealed Gate site with a location");
        Check(GateSiteStatusName(GateSiteStatus::Open) == "open", "Gate status names");

        int generated = 0;
        for (const DistrictRecord& district : city->Districts)
        {
            const std::optional<GeneratedDistrict> grid = GenerateCityDistrict(district);
            Check(grid.has_value() == (district.Kind == DistrictKind::Generated), "exactly the generated districts have a grid");
            if (grid)
            {
                ++generated;
                CheckGuarantees(*grid);
                Check(Same(*grid, GenerateDistrict(*district.Archetype, district.Seed)), "the grid is the pure function of the record");
            }
        }
        Check(generated == 5, "all five generated districts were generated");

        Check(ReadAllCities(loaded.Db).size() == 1 && ReadAllCities(loaded.Db)[0].Id == "city.t", "ReadAllCities finds the city");
        Check(!ReadCity(loaded.Db, "nation.t").has_value() && !ReadCity(loaded.Db, "city.missing").has_value(),
              "a non-city or missing ID is not a city");

        // A record that cannot be read is refused, not guessed.
        const ContentLoadResult broken = LoadContent({{"/city/city.b.json",
                                                       R"({"type":"city","id":"city.b","name":"B","nation":"nation.t","state":"S","role":"R",
            "districts":[{"id":"district.b.a","name":"A","kind":"generated","archetype":"castle","seed":1}],"gateSites":[]})"}});
        Check(!ReadCity(broken.Db, "city.b").has_value(), "an unreadable district makes the city unreadable");
    }

    // The real Data/Content, when CTest passes its path: every city reads and every generated district builds.
    if (argc > 1)
    {
        namespace fs = std::filesystem;
        const fs::path root = argv[1];
        std::vector<fs::path> paths;
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                paths.push_back(entry.path());
            }
        }
        std::sort(paths.begin(), paths.end());
        std::vector<ContentFile> files;
        for (const fs::path& path : paths)
        {
            std::ifstream in(path, std::ios::binary);
            std::ostringstream text;
            text << in.rdbuf();
            files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
        }
        const ContentLoadResult loaded = LoadContent(files);
        Check(loaded.Ok(), "the real content passes the validator");
        const std::size_t cityRecords = loaded.Db.CountOfType("city");
        const std::vector<CityRecord> cities = ReadAllCities(loaded.Db);
        Check(cityRecords > 0 && cities.size() == cityRecords, "every real city record is readable");
        const std::optional<CityRecord> kolkata = ReadCity(loaded.Db, "city.kolkata");
        Check(kolkata.has_value() && !kolkata->Districts.empty(), "Kolkata reads with its districts");
        for (const CityRecord& city : cities)
        {
            for (const DistrictRecord& district : city.Districts)
            {
                if (const std::optional<GeneratedDistrict> grid = GenerateCityDistrict(district))
                {
                    CheckGuarantees(*grid);
                    Check(grid->Layout == grid->Archetype, "a real district is built by its own archetype's layout, not a fallback");
                }
            }
            for (const GateSiteRecord& site : city.GateSites)
            {
                Check(city.FindDistrict(site.DistrictId) != nullptr, "a real Gate site names a district of its own city");
            }
        }
    }

    std::cout << "PASS: " << Checks << " district generator checks\n";
    return 0;
}
