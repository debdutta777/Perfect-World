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

namespace
{
constexpr int N = DistrictGridEdgeCells;
constexpr int SeedCount = 200;

std::uint64_t SeedOf(int i)
{
    return static_cast<std::uint64_t>(i) * 40503ULL + 17ULL;
}

bool Is(const GeneratedDistrict& d, int x, int y, DistrictCellKind kind)
{
    return x >= 0 && y >= 0 && x < N && y < N && d.At(x, y).Kind == kind;
}

bool Same(const GeneratedDistrict& a, const GeneratedDistrict& b)
{
    for (int i = 0; i < DistrictCellCount; ++i)
    {
        if (a.Cells[static_cast<std::size_t>(i)].Kind != b.Cells[static_cast<std::size_t>(i)].Kind ||
            a.Cells[static_cast<std::size_t>(i)].Storeys != b.Cells[static_cast<std::size_t>(i)].Storeys)
        {
            return false;
        }
    }
    return true;
}

/** Size of the 4-connected component of `kind` cells containing (x, y); fills `seen`. */
int Flood(const GeneratedDistrict& d, int x, int y, DistrictCellKind kind, std::vector<char>& seen, int* minX = nullptr, int* maxX = nullptr,
          int* minY = nullptr, int* maxY = nullptr)
{
    int size = 0;
    std::vector<int> stack{y * N + x};
    seen[static_cast<std::size_t>(y * N + x)] = 1;
    while (!stack.empty())
    {
        const int at = stack.back();
        stack.pop_back();
        ++size;
        const int cx = at % N;
        const int cy = at / N;
        if (minX != nullptr)
        {
            *minX = std::min(*minX, cx);
            *maxX = std::max(*maxX, cx);
            *minY = std::min(*minY, cy);
            *maxY = std::max(*maxY, cy);
        }
        const int nx[4] = {cx - 1, cx + 1, cx, cx};
        const int ny[4] = {cy, cy, cy - 1, cy + 1};
        for (int k = 0; k < 4; ++k)
        {
            if (Is(d, nx[k], ny[k], kind) && seen[static_cast<std::size_t>(ny[k] * N + nx[k])] == 0)
            {
                seen[static_cast<std::size_t>(ny[k] * N + nx[k])] = 1;
                stack.push_back(ny[k] * N + nx[k]);
            }
        }
    }
    return size;
}

struct Measure
{
    int Streets = 0;
    int Buildings = 0;
    int Open = 0;
    int MinStoreys = 255;
    int MaxStoreys = 0;
    int FullStreetRows = 0;
    int FullStreetColumns = 0;
    int LargestOpen = 0;
    int OpenCentreX2 = 0; // twice the centre of the largest open area's bounding box
    int OpenCentreY2 = 0;
};

/** The guarantees every archetype makes; returns the figures the per-archetype ranges are about. */
Measure CheckGuarantees(const GeneratedDistrict& d)
{
    Measure m;
    for (int y = 0; y < N; ++y)
    {
        for (int x = 0; x < N; ++x)
        {
            const DistrictCell& cell = d.At(x, y);
            Check(cell.Kind != DistrictCellKind::Water && cell.Kind != DistrictCellKind::Rail, "no layout places water or rail yet");
            if (cell.Kind == DistrictCellKind::Building)
            {
                ++m.Buildings;
                m.MinStoreys = std::min(m.MinStoreys, static_cast<int>(cell.Storeys));
                m.MaxStoreys = std::max(m.MaxStoreys, static_cast<int>(cell.Storeys));
                Check(cell.Storeys >= 1, "a building has at least one storey");
                Check(Is(d, x - 1, y, DistrictCellKind::Street) || Is(d, x + 1, y, DistrictCellKind::Street) ||
                          Is(d, x, y - 1, DistrictCellKind::Street) || Is(d, x, y + 1, DistrictCellKind::Street),
                      "every building lot touches a street");
            }
            else
            {
                Check(cell.Storeys == 0, "only a building has storeys");
            }
            if (cell.Kind == DistrictCellKind::Open)
            {
                ++m.Open;
            }
            if (cell.Kind == DistrictCellKind::Street)
            {
                ++m.Streets;
                Check(Is(d, x - 1, y, DistrictCellKind::Street) || Is(d, x + 1, y, DistrictCellKind::Street) ||
                          Is(d, x, y - 1, DistrictCellKind::Street) || Is(d, x, y + 1, DistrictCellKind::Street),
                      "no street cell is isolated");
            }
        }
    }
    Check(m.Streets + m.Buildings + m.Open == DistrictCellCount, "every cell is street, building or open");
    Check(m.Streets == CountDistrictCells(d, DistrictCellKind::Street), "CountDistrictCells agrees");

    // One 4-connected street network.
    std::vector<char> seen(DistrictCellCount, 0);
    bool found = false;
    for (int i = 0; i < DistrictCellCount && !found; ++i)
    {
        if (d.Cells[static_cast<std::size_t>(i)].Kind == DistrictCellKind::Street)
        {
            found = true;
            Check(Flood(d, i % N, i / N, DistrictCellKind::Street, seen) == m.Streets, "all street cells are 4-connected");
        }
    }
    Check(found, "a district has streets");

    // The network reaches all four edges.
    bool edge[4] = {false, false, false, false};
    for (int i = 0; i < N; ++i)
    {
        edge[0] |= Is(d, i, 0, DistrictCellKind::Street);
        edge[1] |= Is(d, i, N - 1, DistrictCellKind::Street);
        edge[2] |= Is(d, 0, i, DistrictCellKind::Street);
        edge[3] |= Is(d, N - 1, i, DistrictCellKind::Street);
    }
    Check(edge[0] && edge[1] && edge[2] && edge[3], "streets reach all four edges");

    for (int i = 0; i < N; ++i)
    {
        bool row = true;
        bool column = true;
        for (int k = 0; k < N; ++k)
        {
            row = row && Is(d, k, i, DistrictCellKind::Street);
            column = column && Is(d, i, k, DistrictCellKind::Street);
        }
        m.FullStreetRows += row ? 1 : 0;
        m.FullStreetColumns += column ? 1 : 0;
    }

    std::vector<char> openSeen(DistrictCellCount, 0);
    for (int i = 0; i < DistrictCellCount; ++i)
    {
        if (d.Cells[static_cast<std::size_t>(i)].Kind == DistrictCellKind::Open && openSeen[static_cast<std::size_t>(i)] == 0)
        {
            int x0 = N, x1 = 0, y0 = N, y1 = 0;
            const int size = Flood(d, i % N, i / N, DistrictCellKind::Open, openSeen, &x0, &x1, &y0, &y1);
            if (size > m.LargestOpen)
            {
                m.LargestOpen = size;
                m.OpenCentreX2 = x0 + x1;
                m.OpenCentreY2 = y0 + y1;
            }
        }
    }
    return m;
}

struct Totals
{
    long Streets = 0;
    long Buildings = 0;
    long Open = 0;
    long StoreySum = 0;
    int MinStreets = 1 << 20, MaxStreets = 0;
    int MinBuildings = 1 << 20, MaxBuildings = 0;
    int MinOpen = 1 << 20, MaxOpen = 0;
    int MinStoreys = 255, MaxStoreys = 0;
    int MinFullRows = 1 << 20, MaxFullRows = 0;
    int MinLargestOpen = 1 << 20, MaxLargestOpen = 0;
    int MaxCentreOffset = 0;
};

Totals Sweep(DistrictArchetype archetype, bool printRanges)
{
    Totals t;
    for (int i = 0; i < SeedCount; ++i)
    {
        const GeneratedDistrict d = GenerateDistrict(archetype, SeedOf(i));
        Check(d.Archetype == archetype && d.Seed == SeedOf(i), "the district records its recipe");
        Check(Same(d, GenerateDistrict(archetype, SeedOf(i))), "the same archetype and seed regenerate identically");
        const Measure m = CheckGuarantees(d);

        t.Streets += m.Streets;
        t.Buildings += m.Buildings;
        t.Open += m.Open;
        t.StoreySum += static_cast<long>(m.MinStoreys + m.MaxStoreys);
        t.MinStreets = std::min(t.MinStreets, m.Streets);
        t.MaxStreets = std::max(t.MaxStreets, m.Streets);
        t.MinBuildings = std::min(t.MinBuildings, m.Buildings);
        t.MaxBuildings = std::max(t.MaxBuildings, m.Buildings);
        t.MinOpen = std::min(t.MinOpen, m.Open);
        t.MaxOpen = std::max(t.MaxOpen, m.Open);
        t.MinStoreys = std::min(t.MinStoreys, m.MinStoreys);
        t.MaxStoreys = std::max(t.MaxStoreys, m.MaxStoreys);
        t.MinFullRows = std::min(t.MinFullRows, m.FullStreetRows);
        t.MaxFullRows = std::max(t.MaxFullRows, m.FullStreetRows);
        t.MinLargestOpen = std::min(t.MinLargestOpen, m.LargestOpen);
        t.MaxLargestOpen = std::max(t.MaxLargestOpen, m.LargestOpen);
        t.MaxCentreOffset = std::max({t.MaxCentreOffset, std::abs(m.OpenCentreX2 - N), std::abs(m.OpenCentreY2 - N)});
    }
    if (printRanges)
    {
        std::cout << DistrictArchetypeName(archetype) << ": streets " << t.MinStreets << ".." << t.MaxStreets << ", buildings "
                  << t.MinBuildings << ".." << t.MaxBuildings << ", open " << t.MinOpen << ".." << t.MaxOpen << ", storeys "
                  << t.MinStoreys << ".." << t.MaxStoreys << ", full street rows " << t.MinFullRows << ".." << t.MaxFullRows
                  << ", largest open " << t.MinLargestOpen << ".." << t.MaxLargestOpen << ", centre offset x2 " << t.MaxCentreOffset
                  << '\n';
    }
    return t;
}

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
        Check(IsDistrictLayoutImplemented(DistrictArchetype::OldDenseCore) && IsDistrictLayoutImplemented(DistrictArchetype::ApartmentBelt) &&
                  IsDistrictLayoutImplemented(DistrictArchetype::Market) && !IsDistrictLayoutImplemented(DistrictArchetype::Port),
              "three layouts are implemented");
        Check(DistrictFallbackLayout(DistrictArchetype::Riverfront) == DistrictArchetype::OldDenseCore &&
                  DistrictFallbackLayout(DistrictArchetype::Industrial) == DistrictArchetype::ApartmentBelt &&
                  DistrictFallbackLayout(DistrictArchetype::Port) == DistrictArchetype::Market,
              "the documented fallbacks");
    }

    // The three implemented archetypes: guarantees and per-archetype ranges over 200 seeds each.
    const Totals core = Sweep(DistrictArchetype::OldDenseCore, true);
    const Totals belt = Sweep(DistrictArchetype::ApartmentBelt, true);
    const Totals market = Sweep(DistrictArchetype::Market, true);

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

    // Measurably different: street share, building share, height.
    Check(core.Streets > belt.Streets, "old dense core has more street than the apartment belt");
    Check(core.MinStreets > belt.MaxStreets, "the street share ranges of the two do not overlap");
    Check(belt.StoreySum > 2 * core.StoreySum, "apartment towers are more than twice as tall as old core lots");
    Check(core.StoreySum > market.StoreySum, "old core lots are taller than market stalls");

    // Different seeds differ; different archetypes differ on the same seed.
    for (const DistrictArchetype a : {DistrictArchetype::OldDenseCore, DistrictArchetype::ApartmentBelt, DistrictArchetype::Market})
    {
        for (int i = 0; i + 1 < SeedCount; ++i)
        {
            Check(!Same(GenerateDistrict(a, SeedOf(i)), GenerateDistrict(a, SeedOf(i + 1))), "different seeds give different districts");
        }
    }
    for (int i = 0; i < SeedCount; ++i)
    {
        const GeneratedDistrict a = GenerateDistrict(DistrictArchetype::OldDenseCore, SeedOf(i));
        const GeneratedDistrict b = GenerateDistrict(DistrictArchetype::Market, SeedOf(i));
        const GeneratedDistrict c = GenerateDistrict(DistrictArchetype::ApartmentBelt, SeedOf(i));
        Check(!Same(a, b) && !Same(b, c) && !Same(a, c), "different archetypes give different districts");
    }

    // The nine fallbacks still keep every guarantee, name their layout, and honour their storey cap.
    for (int i = 0; i < DistrictArchetypeCount; ++i)
    {
        const auto a = static_cast<DistrictArchetype>(i);
        if (IsDistrictLayoutImplemented(a))
        {
            continue;
        }
        for (int s = 0; s < 20; ++s)
        {
            const GeneratedDistrict d = GenerateDistrict(a, SeedOf(s));
            const Measure m = CheckGuarantees(d);
            Check(d.Archetype == a && d.Layout == DistrictFallbackLayout(a), "a fallback reports the layout it used");
            Check(m.MaxStoreys <= DistrictFallbackStoreyCap(a), "a fallback honours its storey cap");
            Check(!Same(d, GenerateDistrict(DistrictFallbackLayout(a), SeedOf(s))), "a fallback does not copy the layout it borrows");
            Check(Same(d, GenerateDistrict(a, SeedOf(s))), "a fallback is deterministic too");
        }
    }

    // Edge seeds.
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{4294967295ULL}, ~std::uint64_t{0}})
    {
        for (const DistrictArchetype a : {DistrictArchetype::OldDenseCore, DistrictArchetype::ApartmentBelt, DistrictArchetype::Market})
        {
            CheckGuarantees(GenerateDistrict(a, seed));
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
