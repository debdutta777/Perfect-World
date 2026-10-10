#pragma once

// Shared by ActivitiesTests.cpp (the runner) and ActivitiesContentTests.cpp (the content rules).
#include "ContentRegistry.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

inline int ActivityTestChecks = 0;

inline void Check(bool condition, const char* message)
{
    ++ActivityTestChecks;
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

void TestContentRules(const std::vector<PerfectWorld::ContentFile>& files);
