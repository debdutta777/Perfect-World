// pwcontent: validates content and asset registries with the same C++
// validators the game runs at boot (SYSTEMS.md §5, ROADMAP M1-06 and M1-07).
//
//   pwcontent validate <dir>                          exit 0 when the content is valid
//   pwcontent assets <dir>                            exit 0 when <dir>/Contracts and <dir>/Assets
//                                                     (pw.contract/1 and pw.asset/1) are valid
//   ... --expect-issue <text>                         exit 0 only when validation fails with an
//                                                     issue containing <text> that names a file
//                                                     (used by CTest for the bad fixtures)
#include "AssetRegistry.h"
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
} // namespace

int main(int argc, char** argv)
{
    const std::string command = argc >= 2 ? argv[1] : "";
    if (argc < 3 || (command != "validate" && command != "assets"))
    {
        std::cerr << "usage: pwcontent validate|assets <dir> [--expect-issue <text>]\n";
        return 2;
    }
    const fs::path root = argv[2];
    std::string expected;
    if (argc >= 5 && std::string(argv[3]) == "--expect-issue")
    {
        expected = argv[4];
    }
    std::error_code error;
    if (!fs::is_directory(root, error))
    {
        std::cerr << "not a directory: " << root.string() << '\n';
        return 2;
    }
    return command == "validate" ? ValidateContent(root, expected) : ValidateAssets(root, expected);
}
