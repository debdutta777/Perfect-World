#pragma once

// Shared by the assessment tests (ROADMAP M3-07): the real catalogues and paper loaded once
// in main, skill sets for a prepared and an unprepared character, a helper that plays a
// command and holds quote == charge, and the counters. The content helpers (Check, Edit,
// LoadEdited, Refuses, OthersRng) come from the structure tests' support header.
#include "Assessment.h"
#include "SchoolCalendar.h"
#include "StructureOthersTestSupport.h"

#include <string>
#include <vector>

struct AssessmentWorld
{
    std::vector<PerfectWorld::ContentFile> Files;
    PerfectWorld::ContentLoadResult Content;
    OthersCatalogues Structures;
    PerfectWorld::AssessmentCatalogue Papers;
    PerfectWorld::SchoolCalendarSet Calendars;
    const PerfectWorld::AssessmentPaperDef* Paper = nullptr;
};
inline const AssessmentWorld* TestWorld = nullptr;

inline PerfectWorld::AssessmentStructures StructuresOf(const OthersCatalogues& c)
{
    PerfectWorld::AssessmentStructures s;
    s.Model = &c.Model;
    s.Explain = &c.Explain;
    s.Investigate = &c.Investigate;
    s.Coordinate = &c.Coordinate;
    return s;
}

/** Every skill the paper's questions use, at one level. */
inline PerfectWorld::AssessmentSkills SkillsAt(const PerfectWorld::AssessmentPaperDef& paper, std::int32_t milli)
{
    PerfectWorld::AssessmentSkills skills;
    for (const PerfectWorld::AssessmentQuestion& question : paper.Questions)
    {
        const bool listed = std::any_of(skills.begin(), skills.end(), [&](const PerfectWorld::AssessmentSkillLevel& s) {
            return s.SkillId == question.SkillId;
        });
        if (!listed)
            skills.push_back({question.SkillId, milli});
    }
    return skills;
}

inline PerfectWorld::AssessmentSkills PreparedSkills(const PerfectWorld::AssessmentPaperDef& paper)
{
    return SkillsAt(paper, FullSkill);
}

/** No skill listed: every method is unfamiliar. */
inline PerfectWorld::AssessmentSkills UnpreparedSkills()
{
    return {};
}

inline PerfectWorld::AssessmentState Begun(const PerfectWorld::AssessmentSkills& skills)
{
    PerfectWorld::AssessmentState state;
    Check(PerfectWorld::BeginAssessment(state, *TestWorld->Paper, skills) == PerfectWorld::AssessmentError::None, "the paper begins");
    return state;
}

/** Quotes, applies and holds that the charge equals the quote and the units fall by exactly it. */
inline PerfectWorld::AssessmentStepResult Play(PerfectWorld::AssessmentState& state, const PerfectWorld::AssessmentAnswer& answer)
{
    const PerfectWorld::AssessmentQuote quote = PerfectWorld::QuoteAssessmentAnswer(state, answer);
    const std::int32_t before = state.UnitsLeft;
    const PerfectWorld::AssessmentStepResult result = PerfectWorld::ApplyAssessmentAnswer(state, answer);
    Check(result.Ok() == quote.Ok() && result.Error == quote.Error, "the answer succeeds exactly when the quote said it would");
    if (result.Ok())
    {
        Check(result.Charged == quote.Units, "an answer is charged exactly its quote");
        Check(state.UnitsLeft == before - quote.Units && state.UnitsLeft == quote.UnitsAfter, "the units fall by exactly the quote");
    }
    else
    {
        Check(result.Charged == 0 && state.UnitsLeft == before, "a refused answer charges nothing");
    }
    Check(PerfectWorld::IsAssessmentStateValid(state), "the state stays valid");
    return result;
}

inline PerfectWorld::AssessmentAnswer Answer(std::size_t group, PerfectWorld::AssessmentMode mode, bool confirm, bool hedge)
{
    PerfectWorld::AssessmentAnswer answer;
    answer.Group = group;
    answer.Mode = mode;
    answer.Confirm = confirm;
    answer.Hedge = hedge;
    return answer;
}

/** Gives every unanswered group exactly the units it needs for `answer`'s shape, then plays it. */
inline PerfectWorld::AssessmentStepResult AllocateAndPlay(PerfectWorld::AssessmentState& state, const PerfectWorld::AssessmentAnswer& answer)
{
    const PerfectWorld::AssessmentGroupState& group = state.Groups[answer.Group];
    const std::int32_t need = PerfectWorld::AssessmentAnswerUnits(group.Known, answer.Mode, answer.Confirm);
    PerfectWorld::AllocateAssessmentUnits(state, answer.Group, need);
    return Play(state, answer);
}

void TestPaperShape();
void TestEconomy();
void TestSurveyAndAllocation();
void TestMarkingKinds();
void TestMarkingPaper();
void TestSaveAndLoad();
void TestRandomSequences();
void TestContentRules();
