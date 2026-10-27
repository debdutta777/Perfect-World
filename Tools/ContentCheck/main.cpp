// pwcontent: validates content and asset registries with the same C++
// validators the game runs at boot (SYSTEMS.md §5, ROADMAP M1-06 and M1-07),
// and generates or compares the deterministic content manifest (Order S01).
//
//   pwcontent validate <dir>                          exit 0 when the content is valid
//   pwcontent assets <dir>                            exit 0 when <dir>/Contracts and <dir>/Assets
//                                                     (pw.contract/1 and pw.asset/1) are valid
//   pwcontent manifest <dir> [out-file]               print (or write) the deterministic manifest for <dir>
//   pwcontent check-manifest <dir> [manifest-file]    exit 0 when <dir> matches <dir>/manifest.json
//   ... --expect-issue <text>                         exit 0 only when validation fails with an
//                                                     issue containing <text> that names a file
//                                                     (used by CTest for the bad fixtures)
#include "AssetRegistry.h"
#include "ContentManifest.h"
#include "ContentRegistry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

using namespace PerfectWorld;
namespace fs = std::filesystem;

namespace
{
/** Every .json file under each subfolder of root named in `subfolders` (or under root itself when empty), as /relative/paths. */
std::vector<ContentFile> ReadJsonFiles(const fs::path& root, const std::vector<std::string>& subfolders)
{
    std::vector<fs::path> paths;
    std::error_code error;
    const auto collect = [&](const fs::path& folder) {
        if (!fs::is_directory(folder, error))
        {
            return;
        }
        for (const fs::directory_entry& entry : fs::recursive_directory_iterator(folder))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                if (subfolders.empty() && IsContentManifestPath(entry.path().generic_string()))
                {
                    continue;
                }
                paths.push_back(entry.path());
            }
        }
    };
    if (subfolders.empty())
    {
        collect(root);
    }
    for (const std::string& subfolder : subfolders)
    {
        collect(root / subfolder);
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
    return files;
}

/** Prints the issues and turns them into the exit code, honouring --expect-issue. */
int Report(const std::vector<ContentIssue>& issues, const std::string& expected, const std::string& passLine, std::size_t fileCount)
{
    for (const ContentIssue& issue : issues)
    {
        std::cout << issue.File << (issue.Id.empty() ? "" : ": " + issue.Id) << ": " << issue.Message << '\n';
    }
    if (!expected.empty())
    {
        const bool found = std::any_of(issues.begin(), issues.end(), [&](const ContentIssue& issue) {
            return !issue.File.empty() && issue.Message.find(expected) != std::string::npos;
        });
        std::cout << (found ? "EXPECTED FAILURE FOUND: " : "EXPECTED FAILURE MISSING: ") << expected << '\n';
        return found ? 0 : 1;
    }
    if (!issues.empty())
    {
        std::cout << "FAIL: " << issues.size() << " issue(s) in " << fileCount << " file(s)\n";
        return 1;
    }
    std::cout << passLine << '\n';
    return 0;
}

int ValidateContent(const fs::path& root, const std::string& expected)
{
    std::vector<ContentFile> files = ReadJsonFiles(root, {});
    const ContentLoadResult result = LoadContent(files);
    std::ostringstream pass;
    pass << "PASS: " << files.size() << " files, " << result.Db.Count() << " records";
    for (const std::string& type : KnownContentTypes())
    {
        pass << (type == KnownContentTypes().front() ? " (" : ", ") << type << ' ' << result.Db.CountOfType(type);
    }
    pass << ", district " << result.Db.CountOfType("district") << ", gate_site " << result.Db.CountOfType("gate_site") << ")";
    return Report(result.Issues, expected, pass.str(), files.size());
}

int ValidateAssets(const fs::path& root, const std::string& expected)
{
    std::vector<ContentFile> files = ReadJsonFiles(root, {"Contracts", "Assets"});
    const AssetLoadResult result = LoadAssets(files);
    std::ostringstream pass;
    pass << "PASS: " << files.size() << " files, " << result.Db.ContractCount() << " contracts, " << result.Db.AssetCount() << " assets";
    for (const char* category : {"arch", "ground", "prop", "item"})
    {
        pass << (std::string(category) == "arch" ? " (" : ", ") << category << ' ' << result.Db.CountInCategory(category);
    }
    pass << ")";
    return Report(result.Issues, expected, pass.str(), files.size());
}

int EmitContentManifest(const fs::path& root, const std::string& outputPath)
{
    const std::vector<ContentFile> files = ReadJsonFiles(root, {});
    const ContentLoadResult loaded = LoadContent(files);
    if (!loaded.Ok())
    {
        for (const ContentIssue& issue : loaded.Issues)
        {
            std::cerr << issue.File << (issue.Id.empty() ? "" : ": " + issue.Id) << ": " << issue.Message << '\n';
        }
        return 1;
    }
    const ContentManifest manifest = BuildContentManifest(files);
    const std::string json = FormatContentManifestJson(manifest);
    if (!outputPath.empty())
    {
        std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            std::cerr << "cannot write manifest: " << outputPath << '\n';
            return 1;
        }
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        return out.good() ? 0 : 1;
    }
    std::cout << json;
    return 0;
}

int CompareManifestDirectory(const fs::path& root, const fs::path& manifestPath, const std::string& expected)
{
    std::ifstream in(manifestPath, std::ios::binary);
    if (!in)
    {
        const std::string message = manifestPath.generic_string() + ": cannot open committed manifest";
        std::cout << message << '\n';
        if (!expected.empty())
        {
            const bool found = message.find(expected) != std::string::npos;
            std::cout << (found ? "EXPECTED FAILURE FOUND: " : "EXPECTED FAILURE MISSING: ") << expected << '\n';
            return found ? 0 : 1;
        }
        return 1;
    }
    std::ostringstream rawManifest;
    rawManifest << in.rdbuf();
    const ContentManifestParseResult parsed = ParseContentManifestJson(rawManifest.str());
    if (!parsed.Ok())
    {
        const std::string message = manifestPath.generic_string() + ": " + parsed.Error;
        std::cout << message << '\n';
        if (!expected.empty())
        {
            const bool found = message.find(expected) != std::string::npos;
            std::cout << (found ? "EXPECTED FAILURE FOUND: " : "EXPECTED FAILURE MISSING: ") << expected << '\n';
            return found ? 0 : 1;
        }
        return 1;
    }

    const std::vector<ContentFile> files = ReadJsonFiles(root, {});
    const ContentManifest actual = BuildContentManifest(files);
    const ContentManifestDiff diff = DiffContentManifests(*parsed.Manifest, actual);
    if (!diff.Empty() || *parsed.Manifest != actual)
    {
        const std::string report = !diff.Empty() ? FormatContentManifestDiff(diff)
                                                 : (manifestPath.generic_string() + ": manifest does not match directory\n");
        std::cout << report;
        if (!expected.empty())
        {
            const bool found = report.find(expected) != std::string::npos;
            std::cout << (found ? "EXPECTED FAILURE FOUND: " : "EXPECTED FAILURE MISSING: ") << expected << '\n';
            return found ? 0 : 1;
        }
        std::cout << "FAIL: manifest mismatch (" << diff.Added.size() << " added, " << diff.Removed.size() << " removed, "
                  << diff.Renamed.size() << " renamed, " << diff.Duplicates.size() << " duplicate(s))\n";
        return 1;
    }
    if (!expected.empty())
    {
        std::cout << "EXPECTED FAILURE MISSING: " << expected << '\n';
        return 1;
    }
    std::cout << "PASS: manifest matches (" << actual.FileCount() << " files, " << actual.RecordCount() << " records)\n";
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    const std::string command = argc >= 2 ? argv[1] : "";
    if (argc < 3 || (command != "validate" && command != "assets" && command != "manifest" && command != "check-manifest"))
    {
        std::cerr << "usage: pwcontent validate|assets|manifest|check-manifest <dir> [--expect-issue <text>]\n";
        return 2;
    }
    const fs::path root = argv[2];
    std::string expected;
    std::string extraArg;
    for (int i = 3; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--expect-issue" && i + 1 < argc)
        {
            expected = argv[++i];
        }
        else if (extraArg.empty())
        {
            extraArg = argv[i];
        }
    }
    std::error_code error;
    if (!fs::is_directory(root, error))
    {
        std::cerr << "not a directory: " << root.string() << '\n';
        return 2;
    }
    if (command == "validate")
    {
        return ValidateContent(root, expected);
    }
    if (command == "assets")
    {
        return ValidateAssets(root, expected);
    }
    if (command == "manifest")
    {
        return EmitContentManifest(root, extraArg);
    }
    const fs::path manifestPath = extraArg.empty() ? (root / std::string(ContentManifestFilename)) : fs::path(extraArg);
    return CompareManifestDirectory(root, manifestPath, expected);
}
