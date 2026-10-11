#pragma once

// Shared by the awakening tests (ROADMAP M4-02 and M1-34): the assertion, the real Data/Content and
// its recipes loaded once in main, and a "Life" that scripts a student as evidence (growth in the
// attributes, skill levels, aptitude facets, the tutorial dungeon's actions) so the tests can say who
// they are testing. A Life is data, not a persona's name: nothing about household, stream, appearance,
// pronouns, languages or names can be written into it.
#include "Awakening.h"
#include "CharacterCreation.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

inline int AwakeningTestChecks = 0;

inline void Check(bool condition, const char* message)
{
    ++AwakeningTestChecks;
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

struct AwakeningWorld
{
    std::vector<PerfectWorld::ContentFile> Files;
    PerfectWorld::ContentLoadResult Content;
    PerfectWorld::AbilityRecipeSet Recipes;
    PerfectWorld::LatentStats Base;
};
inline const AwakeningWorld* TestWorld = nullptr;

/** A student as evidence. */
struct Life
{
    /** Milli above the starting 10,000, per attribute in Attribute order (Body, Agility, Intelligence, Perception, Willpower, Charisma). */
    std::array<std::int32_t, PerfectWorld::AttributeCount> Growth{};
    /** Skill ID and milli-XP; every other skill is zero. */
    std::vector<std::pair<std::string, std::int32_t>> Skills;
    std::vector<std::pair<PerfectWorld::AptitudeFacet, PerfectWorld::AptitudeFacetState>> Facets;
    /** The dungeon's conduct tests in order: the action and the stakes. Empty is a student absent on the day. */
    std::vector<std::pair<PerfectWorld::ConductAction, std::int32_t>> Conduct;
};

inline PerfectWorld::LatentStats StatsOf(const Life& life)
{
    PerfectWorld::LatentStats stats = TestWorld->Base;
    for (std::size_t a = 0; a < PerfectWorld::AttributeCount; ++a)
        stats.Attributes[a] = 10'000 + life.Growth[a];
    for (PerfectWorld::SkillLevel& skill : stats.Skills)
        skill.LevelMilli = 0;
    for (const auto& [id, milli] : life.Skills)
    {
        for (PerfectWorld::SkillLevel& skill : stats.Skills)
        {
            if (skill.SkillId == id)
                skill.LevelMilli = milli;
        }
    }
    return stats;
}

/** Evidence that leaves a facet in the given state: three activities (12 points) are Shown, plus a consolidated practice (16) is Established. */
inline PerfectWorld::AptitudeLedger AptitudeOf(const Life& life)
{
    using namespace PerfectWorld;
    AptitudeLedger ledger;
    int serial = 0;
    for (const auto& [facet, state] : life.Facets)
    {
        const int activities = state == AptitudeFacetState::Observed ? 1 : 3;
        const AptitudeResult result = state == AptitudeFacetState::Observed ? AptitudeResult::Unsound : AptitudeResult::Sound;
        if (state == AptitudeFacetState::Unobserved)
            continue;
        for (int i = 0; i < activities; ++i)
        {
            const AptitudeEvidence item{"ev." + std::to_string(serial++), AptitudeSource::ActivityCompletion, facet,
                                        "subject." + std::to_string(i), result, false, 0};
            Check(AptitudeAddEvidence(ledger, item).Ok(), "the activity evidence is accepted");
        }
        if (state == AptitudeFacetState::Established)
        {
            const AptitudeEvidence item{"ev." + std::to_string(serial++), AptitudeSource::PracticeConsolidation, facet,
                                        "subject.practice", AptitudeResult::Sound, false, 0};
            Check(AptitudeAddEvidence(ledger, item).Ok(), "the practice evidence is accepted");
        }
    }
    return ledger;
}

inline PerfectWorld::ConductLedger ConductOf(const Life& life)
{
    using namespace PerfectWorld;
    ConductLedger ledger;
    GameInstant at = 0;
    int serial = 1;
    for (const auto& [action, stakes] : life.Conduct)
    {
        ConductRecord record;
        record.TestId = "dungeon_test_" + std::to_string(serial++);
        record.ChoiceId = "choice_" + std::string(ConductActionName(action));
        record.Action = action;
        record.Stakes = stakes;
        record.At = at;
        at += 60;
        Check(RecordConduct(ledger, record).Ok(), "the conduct test is recorded");
    }
    return ledger;
}

inline PerfectWorld::AwakeningInput InputOf(const Life& life, std::uint64_t seed = 7)
{
    PerfectWorld::AwakeningInput input;
    input.Conduct = PerfectWorld::SummariseConduct(ConductOf(life));
    input.Stats = StatsOf(life);
    input.Projection = PerfectWorld::AptitudeProject(AptitudeOf(life));
    input.WorldSeed = seed;
    return input;
}

inline PerfectWorld::AwakeningResult AwakenLife(const Life& life, std::uint64_t seed = 7)
{
    return PerfectWorld::ResolveAwakening(InputOf(life, seed), TestWorld->Recipes);
}

/** The Tattva with the largest share (ties to the lower index; the personas are never tied). */
inline PerfectWorld::Tattva LeadingTattva(const PerfectWorld::AwakeningResult& result)
{
    std::size_t best = 0;
    for (std::size_t t = 1; t < PerfectWorld::TattvaCount; ++t)
    {
        if (result.TattvaMilli[t] > result.TattvaMilli[best])
            best = t;
    }
    return static_cast<PerfectWorld::Tattva>(best);
}

// The groups of tests, one file each.
void TestAwakeningRules();
void TestAwakeningPersonas();
void TestAwakeningProfile();
