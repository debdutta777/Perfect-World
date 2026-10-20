#pragma once

#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

inline void R5BRequire(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

inline PerfectWorld::NpcSchedule R5BLoadSchedule(const std::string& path)
{
    std::ifstream file(path);
    R5BRequire(file.good(), "schedule file opens");
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    auto parsed = PerfectWorld::ParseJson(text);
    R5BRequire(parsed.Value.has_value(), "schedule JSON parses");
    const auto* id = parsed.Value->Find("id");
    R5BRequire(id != nullptr && id->IsString(), "schedule has an ID");
    PerfectWorld::ContentDb db;
    const std::string name = id->String;
    db.Records.emplace(name, PerfectWorld::ContentRecord{"npc_schedule", name, path, *parsed.Value, {}});
    auto loaded = PerfectWorld::LoadNpcSchedule(db, name);
    if (!loaded.Ok()) { std::cerr << loaded.Error << '\n'; }
    R5BRequire(loaded.Ok(), "authored schedule loads through its frozen contract");
    return std::move(*loaded.Value);
}

inline PerfectWorld::GameInstant R5BAt(int minute, int day = 0)
{
    return (day + 7) * PerfectWorld::MinutesPerDay + minute - 360;
}
