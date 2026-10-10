#pragma once

// Shared by StructureModelTests.cpp (the runner) and StructureModelContentTests.cpp
// (the content rules and the catalogue), ROADMAP M2-08.
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

inline int ModelTestChecks = 0;

inline void Check(bool condition, const char* message)
{
    ++ModelTestChecks;
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
struct ModelRng
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

/** The real catalogue, loaded once in main. */
inline const PerfectWorld::ModelCatalogue* ModelTestCatalogue = nullptr;
inline const char* const ModelTestFamily = "problem_family.xi_stock_and_flow";

inline const PerfectWorld::ModelCatalogue& Catalogue()
{
    return *ModelTestCatalogue;
}

/** A case by its short name, for example "water_tank_level". */
inline const PerfectWorld::ModelCaseDef& CaseOf(const std::string& shortName)
{
    const PerfectWorld::ModelCaseDef* def = Catalogue().FindCase("problem_case.xi_" + shortName);
    Check(def != nullptr, "the named case exists");
    return *def;
}

inline constexpr std::int32_t NoSkill = 0;
inline constexpr std::int32_t FullSkill = 200000;

inline constexpr std::array<PerfectWorld::ModelGoal, 3> AllGoals{PerfectWorld::ModelGoal::UnderstandModel,
                                                                  PerfectWorld::ModelGoal::TestMeasurements,
                                                                  PerfectWorld::ModelGoal::ExplainToGroup};

inline PerfectWorld::ModelAction ExampleAction()
{
    PerfectWorld::ModelAction action;
    action.Kind = PerfectWorld::ModelActionKind::WorkedExample;
    return action;
}

inline PerfectWorld::ModelAction CommitAction(std::vector<std::string> links)
{
    PerfectWorld::ModelAction action;
    action.Kind = PerfectWorld::ModelActionKind::CommitModel;
    action.Links = std::move(links);
    return action;
}

inline PerfectWorld::ModelAction InvestigateAction(PerfectWorld::ModelInvestigation investigation)
{
    PerfectWorld::ModelAction action;
    action.Kind = PerfectWorld::ModelActionKind::Investigate;
    action.Investigation = investigation;
    return action;
}

inline PerfectWorld::ModelAction ClaimAction(PerfectWorld::ModelClaim claim, bool explain)
{
    PerfectWorld::ModelAction action;
    action.Kind = PerfectWorld::ModelActionKind::PresentClaim;
    action.Claim = claim;
    action.Explain = explain;
    return action;
}

inline std::vector<std::string> SoundLinks(const PerfectWorld::ModelCaseDef& def)
{
    std::vector<std::string> out;
    for (const PerfectWorld::ModelLink& link : def.Links)
        if (link.Sound)
            out.push_back(link.Id);
    return out;
}

/** Quotes, applies and checks the charge equals the quote shown before. */
inline PerfectWorld::ModelStepResult Do(PerfectWorld::StructureModelState& state, const PerfectWorld::ModelAction& action)
{
    const PerfectWorld::StructureModelState before = state;
    const PerfectWorld::ModelQuote quote = PerfectWorld::QuoteModelAction(state, Catalogue(), action);
    Check(state == before, "quoting changes nothing");
    Check(quote.Ok(), "the action is legal");
    const std::int32_t left = state.Exposure.UnitsLeft;
    const PerfectWorld::ModelStepResult step = PerfectWorld::ApplyModelAction(state, Catalogue(), action);
    Check(step.Ok() && step.Charged == quote.Units, "the charge equals the quote shown before");
    Check(step.Finished || state.Exposure.UnitsLeft == left - quote.Units, "the units fall by exactly the quote");
    Check(step.Finished || state.Exposure.UnitsLeft == quote.UnitsAfter, "UnitsAfter is what remains");
    Check(PerfectWorld::IsStructureModelStateValid(state), "the state stays valid");
    return step;
}

/** A fresh state with `def` begun for a character of `skill`. */
inline PerfectWorld::StructureModelState Started(const PerfectWorld::ModelCaseDef& def, PerfectWorld::ModelGoal goal, std::int32_t skill)
{
    PerfectWorld::StructureModelState state;
    const PerfectWorld::ModelBeginResult begin = PerfectWorld::BeginModelExposure(state, Catalogue(), def.Id, goal, skill);
    Check(begin.Ok() && begin.Units == PerfectWorld::ModelBudgetUnits(def.Scarce) && begin.MethodKnown == (skill >= def.MethodKnownAtMilli),
          "an exposure begins with its budget");
    return state;
}

/** Plays the standard policy to the end of the running exposure. */
inline PerfectWorld::ModelOutcome PlayStandard(PerfectWorld::StructureModelState& state)
{
    for (int i = 0; i < 6; ++i)
    {
        const PerfectWorld::ModelCaseDef* def = Catalogue().FindCase(state.Exposure.CaseId);
        Check(def != nullptr, "the running case exists");
        const PerfectWorld::ModelStepResult step = Do(state, PerfectWorld::ModelStandardAction(*def, state.Exposure));
        if (step.Finished)
        {
            Check(!state.Exposure.Active(), "a finished exposure leaves the runner idle");
            return step.Outcome;
        }
    }
    Check(false, "the standard policy finishes within six steps");
    return {};
}

void TestContentRules(const std::vector<PerfectWorld::ContentFile>& files);
void TestCatalogueRules(const std::vector<PerfectWorld::ContentFile>& files);
