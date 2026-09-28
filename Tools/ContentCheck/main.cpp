// pwcontent: validates a content directory with the same C++ validator the
// game runs at boot (SYSTEMS.md §5, ROADMAP M1-06).
//
//   pwcontent validate <dir>                          exit 0 when the content is valid
//   pwcontent validate <dir> --expect-issue <text>    exit 0 only when validation fails
//                                                     with an issue containing <text>
//                                                     that names a file (used by CTest
//                                                     for the bad fixtures)
#include "ContentRegistry.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

using namespace PerfectWorld;
namespace fs = std::filesystem;

int main(int argc, char** argv)
{
    if (argc < 3 || std::string(argv[1]) != "validate")
    {
        std::cerr << "usage: pwcontent validate <dir> [--expect-issue <text>]\n";
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
        files.push_back({fs::relative(path, root).generic_string(), text.str()});
        files.back().Path = "/" + files.back().Path;
    }

    const ContentLoadResult result = LoadContent(files);
    for (const ContentIssue& issue : result.Issues)
    {
        std::cout << issue.File << (issue.Id.empty() ? "" : ": " + issue.Id) << ": " << issue.Message << '\n';
    }

    if (!expected.empty())
    {
        const bool found = std::any_of(result.Issues.begin(), result.Issues.end(), [&](const ContentIssue& issue) {
            return !issue.File.empty() && issue.Message.find(expected) != std::string::npos;
        });
        std::cout << (found ? "EXPECTED FAILURE FOUND: " : "EXPECTED FAILURE MISSING: ") << expected << '\n';
        return found ? 0 : 1;
    }
    if (!result.Ok())
    {
        std::cout << "FAIL: " << result.Issues.size() << " issue(s) in " << files.size() << " file(s)\n";
        return 1;
    }
    std::cout << "PASS: " << files.size() << " files, " << result.Db.Count() << " records";
    for (const std::string& type : KnownContentTypes())
    {
        std::cout << (type == KnownContentTypes().front() ? " (" : ", ") << type << ' ' << result.Db.CountOfType(type);
    }
    std::cout << ", district " << result.Db.CountOfType("district") << ", gate_site " << result.Db.CountOfType("gate_site")
              << ")\n";
    return 0;
}
