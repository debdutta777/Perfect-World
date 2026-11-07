#include "ContentManifest.h"

#include "ContentRegistry.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using namespace PerfectWorld;

int S01Assertions = 0;

void S01Expect(bool condition, std::string_view message)
{
    ++S01Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

ContentFile S01StratumFile(const std::string& path, const std::string& id, int rank)
{
    return {path, "{\"type\":\"stratum\",\"id\":\"" + id + "\",\"householdResourcesRank\":" + std::to_string(rank) +
                      ",\"name\":\"N\",\"housing\":\"h\",\"buffer\":\"small\"}"};
}

std::vector<ContentFile> S01SmallWorldFiles()
{
    return {
        {"/nation/nation.t.json",
         R"({"type":"nation","id":"nation.t","name":"T","capitalCity":"city.t","gateAuthority":"institution.t_nga",
             "realmHomeRegion":"realm.region.t","stageOneCities":["city.t"],
             "towerExamination":{"centralHall":"institution.t_hall","certifiedHallPerCity":true}})"},
        {"/realm_region/realm.region.t.json",
         R"({"type":"realm_region","id":"realm.region.t","name":"T home","floor":1,"anchors":["gate.t.anchor"]})"},
        {"/institution/institution.t_nga.json",
         R"({"type":"institution","id":"institution.t_nga","name":"NGA","kind":"gate_authority","city":"city.t"})"},
        {"/institution/institution.t_hall.json",
         R"({"type":"institution","id":"institution.t_hall","name":"Hall","kind":"examination_hall","city":"city.t"})"},
        {"/city/city.t.json",
         R"({"type":"city","id":"city.t","name":"Testville","nation":"nation.t","state":"Test State","role":"A test city",
             "districts":[
               {"id":"district.t.river","name":"River","kind":"generated","archetype":"riverfront","seed":7},
               {"id":"district.t.home","name":"Home","kind":"authored","fictional":true}],
             "gateSites":[
               {"id":"gate.t.dock","name":"Dock","district":"district.t.river","rank":"B","status":"open","realmAnchor":false},
               {"id":"gate.t.anchor","name":"Anchor","district":"district.t.river","rank":"D","status":"contained","realmAnchor":true}]})"},
        S01StratumFile("/stratum/stratum.b.json", "stratum.b", 1),
        S01StratumFile("/stratum/stratum.a.json", "stratum.a", 0),
    };
}

std::vector<ContentFile> S01ReadDirectory(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root, error))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            if (IsContentManifestPath(entry.path().generic_string()))
            {
                continue;
            }
            paths.push_back(entry.path());
        }
    }
    S01Expect(!error, "content directory opens cleanly");
    std::sort(paths.begin(), paths.end());

    std::vector<ContentFile> files;
    files.reserve(paths.size());
    for (const fs::path& path : paths)
    {
        std::ifstream in(path, std::ios::binary);
        S01Expect(in.good(), "content file opens");
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

void S01TestOrderingAndParsing()
{
    S01Expect(IsContentManifestPath("manifest.json"), "bare manifest filename is recognized");
    S01Expect(IsContentManifestPath("/manifest.json"), "leading-slash manifest filename is recognized");
    S01Expect(IsContentManifestPath("D:\\PWWork\\Data\\Content\\manifest.json"), "Windows path to manifest.json is recognized");
    S01Expect(!IsContentManifestPath("/stratum/stratum.manifest.json"), "regular content file is not manifest.json");

    S01Expect(NormalizeContentManifestPath("D:\\PWWork\\Data\\Content\\city\\city.kolkata.json") == "/city/city.kolkata.json",
              "Windows Data/Content prefix normalizes to /type/id.json");
    S01Expect(NormalizeContentManifestPath("./Data/Content//stratum/stratum.a.json") == "/stratum/stratum.a.json",
              "relative Data/Content prefix and duplicate slashes normalize");

    std::vector<ContentFile> files = S01SmallWorldFiles();
    files.push_back({"/manifest.json", "{\"schema\":\"pw.content_manifest/1\",\"files\":[]}"});

    const ContentLoadResult loaded = LoadContent(files);
    S01Expect(loaded.Ok(), "small world validates even when manifest.json is included in the file list");

    const ContentManifest fromFiles = BuildContentManifest(files);
    const ContentManifest fromDb = BuildContentManifest(loaded.Db);
    const ContentManifest fromLoaded = BuildContentManifest(loaded);
    S01Expect(fromFiles == fromDb && fromFiles == fromLoaded, "manifest built from files, Db and load result are identical");
    S01Expect(fromFiles.FileCount() == 7 && fromFiles.RecordCount() == 11, "file and record counts include nested districts and Gate sites");
    S01Expect(fromFiles.CountOfKind("stratum") == 2 && fromFiles.CountOfKind("district") == 2 && fromFiles.CountOfKind("gate_site") == 2,
              "per-kind counts cover top-level and nested records");

    for (std::size_t i = 1; i < fromFiles.Files.size(); ++i)
    {
        S01Expect(fromFiles.Files[i - 1].Path < fromFiles.Files[i].Path, "manifest files are sorted strictly by path");
    }
    const ContentManifestFile* cityFile = fromFiles.FindFile("city/city.t.json");
    S01Expect(cityFile != nullptr && cityFile->Records.size() == 5, "city file lists the city, two districts and two Gate sites");
    S01Expect(cityFile->Records[0].Id == "city.t" && cityFile->Records[1].Id == "district.t.home" &&
                  cityFile->Records[2].Id == "district.t.river" && cityFile->Records[3].Id == "gate.t.anchor" &&
                  cityFile->Records[4].Id == "gate.t.dock",
              "nested records within a file are sorted by ID");

    const std::string json = FormatContentManifestJson(fromFiles);
    const ContentManifestParseResult parsed = ParseContentManifestJson(json);
    S01Expect(parsed.Ok() && *parsed.Manifest == fromFiles, "manifest JSON round-trips losslessly");

    const ContentManifestParseResult withUtf8Bom = ParseContentManifestJson("\xEF\xBB\xBF" + json);
    S01Expect(withUtf8Bom.Ok() && *withUtf8Bom.Manifest == fromFiles, "UTF-8 BOM is tolerated");

    std::string utf16Le("\xFF\xFE", 2);
    for (const unsigned char c : json)
    {
        utf16Le.push_back(static_cast<char>(c));
        utf16Le.push_back('\0');
    }
    const ContentManifestParseResult withUtf16Bom = ParseContentManifestJson(utf16Le);
    S01Expect(withUtf16Bom.Ok() && *withUtf16Bom.Manifest == fromFiles, "UTF-16LE BOM from Windows shell redirection is tolerated");

    S01Expect(!ParseContentManifestJson(R"({"schema":"wrong/1","files":[]})").Ok(), "wrong schema is refused");
    S01Expect(!ParseContentManifestJson(R"({"schema":"pw.content_manifest/1","fileCount":2,"files":[]})").Ok(),
              "mismatched fileCount is refused");
    S01Expect(!ParseContentManifestJson(R"({"schema":"pw.content_manifest/1","recordCount":1,"files":[]})").Ok(),
              "mismatched recordCount is refused");
    S01Expect(!ParseContentManifestJson(R"({"schema":"pw.content_manifest/1","files":[
        {"path":"/stratum/stratum.b.json","records":[{"id":"stratum.b","kind":"stratum"}]},
        {"path":"/stratum/stratum.a.json","records":[{"id":"stratum.a","kind":"stratum"}]}]})")
                   .Ok(),
              "unsorted files in manifest JSON are refused");
    S01Expect(!ParseContentManifestJson(R"({"schema":"pw.content_manifest/1","files":[
        {"path":"/city/city.t.json","records":[{"id":"gate.t.b","kind":"gate_site"},{"id":"gate.t.a","kind":"gate_site"}]}]})")
                   .Ok(),
              "unsorted records in manifest JSON are refused");
}

void S01TestDuplicates()
{
    const std::vector<ContentFile> baselineFiles = {
        S01StratumFile("/stratum/stratum.a.json", "stratum.a", 0),
        S01StratumFile("/stratum/stratum.b.json", "stratum.b", 1),
    };
    const ContentManifest baseline = BuildContentManifest(baselineFiles);
    S01Expect(FindContentManifestDuplicates(baseline).empty(), "clean manifest has no duplicates");

    // Duplicate ID across two files.
    const std::vector<ContentFile> crossFileDup = {
        S01StratumFile("/stratum/stratum.a.json", "stratum.a", 0),
        S01StratumFile("/stratum/stratum.a_copy.json", "stratum.a", 1),
        S01StratumFile("/stratum/stratum.b.json", "stratum.b", 2),
    };
    const ContentManifest builtFromFiles = BuildContentManifest(crossFileDup);
    const ContentManifest builtFromLoaded = BuildContentManifest(LoadContent(crossFileDup));
    S01Expect(builtFromFiles == builtFromLoaded, "both BuildContentManifest overloads capture cross-file duplicate IDs");

    const ContentManifestDiff crossDiff = DiffContentManifests(baseline, builtFromFiles);
    S01Expect(!crossDiff.Empty() && crossDiff.Duplicates.size() == 1, "cross-file duplicate is reported");
    S01Expect(crossDiff.Duplicates[0] ==
                  ContentManifestDuplicate{"stratum.a", "stratum", "/stratum/stratum.a.json", "/stratum/stratum.a_copy.json"},
              "cross-file duplicate names both files and the record ID and kind");
    S01Expect(FormatContentManifestDiff(crossDiff).find(
                  "duplicate: /stratum/stratum.a_copy.json: stratum.a (stratum) (first in /stratum/stratum.a.json)") != std::string::npos,
              "formatted diff includes readable duplicate line");

    // Duplicate nested ID within the same file.
    const std::vector<ContentFile> withinFileDup = {
        {"/city/city.t.json",
         R"({"type":"city","id":"city.t","name":"T","nation":"nation.t","state":"S","role":"R",
             "districts":[{"id":"district.t.a","name":"A","kind":"authored"},{"id":"district.t.a","name":"B","kind":"authored"}],
             "gateSites":[]})"}};
    const ContentManifest withinManifest = BuildContentManifest(withinFileDup);
    const std::vector<ContentManifestDuplicate> withinDups = FindContentManifestDuplicates(withinManifest);
    S01Expect(withinDups.size() == 1 &&
                  withinDups[0] == ContentManifestDuplicate{"district.t.a", "district", "/city/city.t.json", "/city/city.t.json"},
              "within-file duplicate nested record ID is caught");
}

void S01TestAdditionsRemovalsAndRenames()
{
    const std::vector<ContentFile> baseFiles = S01SmallWorldFiles();
    const ContentManifest expected = BuildContentManifest(baseFiles);
    S01Expect(DiffContentManifests(expected, expected).Empty(), "identical manifests produce an empty diff");

    // Removal of a top-level file and a nested record, plus addition of a new file and a rename of an existing record's file.
    std::vector<ContentFile> mutated;
    for (const ContentFile& file : baseFiles)
    {
        if (file.Path == "/stratum/stratum.b.json")
        {
            continue; // remove stratum.b
        }
        if (file.Path == "/stratum/stratum.a.json")
        {
            mutated.push_back(S01StratumFile("/stratum/stratum.a_moved.json", "stratum.a", 0)); // rename file for stratum.a
            continue;
        }
        if (file.Path == "/city/city.t.json")
        {
            ContentFile edited = file;
            const std::string target = R"({"id":"district.t.home","name":"Home","kind":"authored","fictional":true})";
            const std::string replacement = R"({"id":"district.t.new_ward","name":"New Ward","kind":"authored","fictional":true})";
            const std::size_t pos = edited.Text.find(target);
            S01Expect(pos != std::string::npos, "city target text found");
            edited.Text.replace(pos, target.size(), replacement);
            mutated.push_back(std::move(edited));
            continue;
        }
        mutated.push_back(file);
    }
    mutated.push_back(S01StratumFile("/stratum/stratum.c.json", "stratum.c", 2)); // add stratum.c

    const ContentManifest actual = BuildContentManifest(mutated);
    const ContentManifestDiff diff = DiffContentManifests(expected, actual);
    S01Expect(!diff.Empty(), "mutated manifest is not equal to expected");
    S01Expect(diff.Added.size() == 2 &&
                  diff.Added[0] == ContentManifestEntryRef{"district.t.new_ward", "district", "/city/city.t.json"} &&
                  diff.Added[1] == ContentManifestEntryRef{"stratum.c", "stratum", "/stratum/stratum.c.json"},
              "added records are sorted and identify their file and kind");
    S01Expect(diff.Removed.size() == 2 &&
                  diff.Removed[0] == ContentManifestEntryRef{"district.t.home", "district", "/city/city.t.json"} &&
                  diff.Removed[1] == ContentManifestEntryRef{"stratum.b", "stratum", "/stratum/stratum.b.json"},
              "removed records are sorted and identify their file and kind");
    S01Expect(diff.Renamed.size() == 1 &&
                  diff.Renamed[0] ==
                      ContentManifestRename{"stratum.a", "stratum", "stratum", "/stratum/stratum.a.json", "/stratum/stratum.a_moved.json"},
              "renamed file for an existing record ID is reported in Renamed");

    const std::string formatted = FormatContentManifestDiff(diff);
    S01Expect(formatted.find("added: /city/city.t.json: district.t.new_ward (district)\n") != std::string::npos &&
                  formatted.find("added: /stratum/stratum.c.json: stratum.c (stratum)\n") != std::string::npos &&
                  formatted.find("removed: /city/city.t.json: district.t.home (district)\n") != std::string::npos &&
                  formatted.find("removed: /stratum/stratum.b.json: stratum.b (stratum)\n") != std::string::npos &&
                  formatted.find("renamed: /stratum/stratum.a.json -> /stratum/stratum.a_moved.json: stratum.a (stratum)\n") !=
                      std::string::npos,
              "formatted diff prints every added, removed and renamed item");
}

void S01TestDeterminismAndRealContent(const std::filesystem::path& contentDir)
{
    std::vector<ContentFile> files = S01ReadDirectory(contentDir);
    S01Expect(!files.empty(), "Data/Content contains JSON files");

    const ContentLoadResult loaded = LoadContent(files);
    for (const ContentIssue& issue : loaded.Issues)
    {
        std::cerr << issue.File << " " << issue.Id << ": " << issue.Message << '\n';
    }
    S01Expect(loaded.Ok(), "Data/Content validates cleanly");

    const ContentManifest fromFiles = BuildContentManifest(files);
    const ContentManifest fromDb = BuildContentManifest(loaded.Db);
    S01Expect(fromFiles == fromDb, "real Data/Content manifest from files matches manifest from ContentDb");
    S01Expect(fromFiles.FileCount() == files.size(), "manifest fileCount equals the number of content JSON files");
    S01Expect(fromFiles.RecordCount() == loaded.Db.Count(), "manifest recordCount equals ContentDb::Count()");

    std::vector<ContentFile> reversed = files;
    std::reverse(reversed.begin(), reversed.end());
    const ContentManifest fromReversed = BuildContentManifest(reversed);
    S01Expect(fromReversed == fromFiles, "file enumeration order does not affect the built manifest");

    const std::string jsonA = FormatContentManifestJson(fromFiles);
    const std::string jsonB = FormatContentManifestJson(fromReversed);
    S01Expect(jsonA == jsonB && jsonA.find("timestamp") == std::string::npos, "serialized JSON is deterministic with no timestamps");

    const ContentManifestParseResult roundTrip = ParseContentManifestJson(jsonA);
    S01Expect(roundTrip.Ok() && DiffContentManifests(fromFiles, *roundTrip.Manifest).Empty(),
              "real Data/Content manifest round-trips through JSON without diffs");

    const std::filesystem::path committedPath = contentDir / std::string(ContentManifestFilename);
    std::error_code ec;
    if (std::filesystem::is_regular_file(committedPath, ec))
    {
        std::ifstream in(committedPath, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        const ContentManifestParseResult committed = ParseContentManifestJson(text.str());
        S01Expect(committed.Ok(), "committed Data/Content/manifest.json parses cleanly");
        const ContentManifestDiff diff = DiffContentManifests(*committed.Manifest, fromFiles);
        if (!diff.Empty())
        {
            std::cerr << FormatContentManifestDiff(diff);
        }
        S01Expect(diff.Empty() && *committed.Manifest == fromFiles, "committed Data/Content/manifest.json matches Data/Content");
    }
}
} // namespace

int main(int argc, char** argv)
{
    S01TestOrderingAndParsing();
    S01TestDuplicates();
    S01TestAdditionsRemovalsAndRenames();
    if (argc >= 2)
    {
        S01TestDeterminismAndRealContent(argv[1]);
    }
    std::cout << "PASS: " << S01Assertions << " content manifest assertions\n";
    return 0;
}
