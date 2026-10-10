#pragma once

// Shared by AttendanceTests.cpp (content and routine) and AttendanceRecordTests.cpp
// (lateness, the record and its text), ROADMAP M2-07.
#include "Attendance.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

extern int AttendanceChecks;

inline void Check(bool condition, const char* message)
{
    ++AttendanceChecks;
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

struct AttendanceRng
{
    std::uint64_t S;
    std::uint64_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return (S * 2685821657736338717ULL) >> 16;
    }
    std::int64_t Pick(std::int64_t low, std::int64_t high)
    {
        return low + static_cast<std::int64_t>(Next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

/** Day 0 is the Monday of the campaign start; instant 0 is 06:00 that day. */
inline PerfectWorld::GameInstant AttendanceAt(std::int64_t day, int hour, int minute)
{
    return day * PerfectWorld::MinutesPerDay + hour * 60 + minute - 6 * 60;
}
inline PerfectWorld::GameInstant AttendanceAtMinute(std::int64_t day, int minuteOfDay)
{
    return day * PerfectWorld::MinutesPerDay + minuteOfDay - 6 * 60;
}

/** The three real schedules, loaded once from Data/Content. */
const PerfectWorld::BellScheduleSet& RealSchedules();
const std::vector<PerfectWorld::ContentFile>& RealContentFiles();

// AttendanceTests.cpp
void TestContentAndRoutine();
// AttendanceRecordTests.cpp
void TestLateness();
void TestRecord();
void TestText();
