#pragma once

// Shared by the explain, investigate and coordinate tests (ROADMAP M2-09): the
// assertion, reading the real Data/Content, a small deterministic generator, the
// catalogues loaded once in main and the in-memory content edits.
#include "StructureCoordinate.h"
#include "StructureExplain.h"
#include "StructureInvestigate.h"
#include "StructureModel.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

inline int OthersTestChecks = 0;

inline void Check(bool condition, const char* message)
{
    ++OthersTestChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

inline std::vector<PerfectWorld::ContentFile> ReadContentDirectory(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    std::vector<PerfectWorld::ContentFile> files;
    for (const fs::path& path : paths)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

// A small deterministic generator for the tests (xorshift64*).
struct OthersRng
{
    std::uint64_t S;
    std::uint64_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return (S * 2685821657736338717ULL) >> 16;
    }
    std::size_t Below(std::size_t bound) { return static_cast<std::size_t>(Next() % bound); }
};

/** The real catalogues, loaded once in main. */
struct OthersCatalogues
{
    PerfectWorld::ExplainCatalogue Explain;
    PerfectWorld::InvestigateCatalogue Investigate;
    PerfectWorld::CoordinateCatalogue Coordinate;
    PerfectWorld::ModelCatalogue Model;
};
inline const OthersCatalogues* TestCatalogues = nullptr;

inline constexpr std::int32_t NoSkill = 0;
inline constexpr std::int32_t FullSkill = 200000;

// ---- Content edits, made in memory to the real Data/Content ------------------------

struct Edit
{
    std::string Path; // a fragment of the file path
    std::string From;
    std::string To;
};

inline PerfectWorld::ContentLoadResult LoadEdited(std::vector<PerfectWorld::ContentFile> files, const std::vector<Edit>& edits)
{
    for (const Edit& edit : edits)
    {
        bool found = false;
        for (PerfectWorld::ContentFile& file : files)
        {
            if (file.Path.find(edit.Path) == std::string::npos)
                continue;
            const std::size_t at = file.Text.find(edit.From);
            Check(at != std::string::npos, "the edit target text exists in the file");
            if (at != std::string::npos)
                file.Text.replace(at, edit.From.size(), edit.To);
            found = true;
        }
        Check(found, "the edited file exists");
    }
    return PerfectWorld::LoadContent(files);
}

inline bool HasIssue(const PerfectWorld::ContentLoadResult& result, const std::string& text)
{
    return std::any_of(result.Issues.begin(), result.Issues.end(), [&](const PerfectWorld::ContentIssue& issue) {
        return issue.Message.find(text) != std::string::npos && !issue.File.empty();
    });
}

inline void Refuses(const std::vector<PerfectWorld::ContentFile>& files, const std::vector<Edit>& edits, const char* issue)
{
    const PerfectWorld::ContentLoadResult result = LoadEdited(files, edits);
    if (!HasIssue(result, issue))
    {
        std::cerr << "expected: " << issue << '\n';
        for (const PerfectWorld::ContentIssue& found : result.Issues)
            std::cerr << "  got: " << found.Message << '\n';
    }
    Check(HasIssue(result, issue), issue);
}

void TestExplain();
void TestInvestigate();
void TestCoordinate();
void TestContentRules(const std::vector<PerfectWorld::ContentFile>& files);
void TestCatalogueRules(const std::vector<PerfectWorld::ContentFile>& files);
void TestDistinct(const std::vector<PerfectWorld::ContentFile>& files);
