#include "LearningCredits.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace PerfectWorld
{
namespace
{
std::size_t P32FoundationIndex()
{
    const auto& traits = CharacterCreator::TraitDefinitions();
    const auto found = std::find_if(traits.begin(), traits.end(), [](const TraitDefinition& trait) {
        return trait.Id == "academic_foundation";
    });
    return static_cast<std::size_t>(found - traits.begin());
}

template <typename Catalogue>
std::string_view P32FamilySkill(const Catalogue* catalogue, std::string_view familyId)
{
    const auto* family = catalogue != nullptr ? catalogue->FindFamily(familyId) : nullptr;
    return family != nullptr ? std::string_view(family->SkillId) : std::string_view{};
}

std::string P32SubmissionIssue(const SchoolWorkRecord& record, const SchoolWorkRecord& submitted)
{
    AssessmentState finished = record.Paper;
    finished.Finished = true;
    if (!(record.Brief == submitted.Brief) || record.IssuedAt != submitted.IssuedAt ||
        !record.StartedAt || record.StartedAt != submitted.StartedAt || !submitted.SubmittedAt ||
        *submitted.StartedAt < 0 || *submitted.StartedAt > DayLoopMaxInstant - AssessmentPaperMinutes ||
        *submitted.SubmittedAt != *submitted.StartedAt + AssessmentPaperMinutes ||
        record.Paper.PaperId != submitted.Paper.PaperId || submitted.Paper.PaperId != submitted.Brief.PaperId ||
        !(finished == submitted.Paper) || !IsAssessmentStateValid(record.Paper) ||
        !IsAssessmentStateValid(submitted.Paper) ||
        !IsReceiptIdWellFormed(submitted.Brief.Id))
        return "learning.invalid_submission";
    return {};
}
} // namespace

AcademicPriorPreparationResult AcademicPriorPreparation(const ContentDb& content, std::int32_t rank)
{
    const std::size_t index = P32FoundationIndex();
    if (rank < 0 || rank > MaxRank || index == TraitCount)
        return {"learning.invalid_foundation", 0};
    TraitRankArray ranks{};
    const auto baseline = ComputeStartingStats(content, ranks);
    if (!baseline.Ok())
        return {baseline.Error, 0};
    const auto* baseSkill = baseline.Stats.FindSkill("skill.academics");
    if (baseSkill == nullptr)
        return {"learning.missing_academics", 0};
    std::int64_t previous = 0;
    std::int32_t selected = 0;
    for (std::int32_t candidate = 1; candidate <= MaxRank; ++candidate)
    {
        ranks[index] = candidate;
        const auto seeded = ComputeStartingStats(content, ranks);
        if (!seeded.Ok())
            return {seeded.Error, 0};
        const auto* skill = seeded.Stats.FindSkill("skill.academics");
        if (skill == nullptr)
            return {"learning.missing_academics", 0};
        const std::int64_t preparation = static_cast<std::int64_t>(skill->LevelMilli) - baseSkill->LevelMilli;
        if (preparation <= previous || preparation > AcademicPriorPreparationMaxMilli)
            return {"learning.invalid_preparation_table", 0};
        previous = preparation;
        if (candidate == rank)
            selected = static_cast<std::int32_t>(preparation);
    }
    return {{}, selected};
}

AwakeningInputsSeedResult SeedLearningInputs(const ContentDb& content, const CharacterCreationSave& creation,
                                            std::uint64_t worldSeed)
{
    const std::size_t index = P32FoundationIndex();
    if (index == TraitCount)
        return {"learning.invalid_foundation", {}};
    const auto preparation = AcademicPriorPreparation(content, creation.TraitRanks[index]);
    if (!preparation.Ok())
        return {preparation.Error, {}};
    return SeedAwakeningInputs(content, creation, worldSeed);
}

AwakeningCreditsResult ApplyLearningCredits(AwakeningState& state, const SchoolSessionResult& accepted)
{
    if (!accepted.Ok())
        return {accepted.Issue.Code, 0, 0};
    return ApplyActivityCredits(state, accepted.Events);
}

AwakeningCreditsResult ApplyLearningCredits(AwakeningState& state, const DayLoopResult& accepted)
{
    if (!accepted.Ok())
        return {std::string(DayLoopErrorCode(accepted.Error)), 0, 0};
    return ApplyActivityCredits(state, accepted.Events);
}

AwakeningCreditsResult ApplyLearningCredits(AwakeningState& state, std::span<const DayLoopEvent> acceptedEvents)
{
    return ApplyActivityCredits(state, acceptedEvents);
}

SchoolWorkLearningResult ApplySchoolWorkLearning(AwakeningState& state, ActivityRunnerState& runner,
    PracticeState& practice, SchoolWorkRecord& record, const SchoolWorkRecord& submitted, const ActivityDef& study)
{
    if (record.Brief.Id != submitted.Brief.Id)
        return {"learning.work_id_mismatch", 0, 0, {}};
    if (record.SubmittedAt)
        return {{}, 0, 1, {}};
    const std::string issue = P32SubmissionIssue(record, submitted);
    if (!issue.empty())
        return {issue, 0, 0, {}};
    if (!IsActivityRunnerStateValid(runner) || runner.Active() ||
        runner.Instance == std::numeric_limits<std::uint64_t>::max() ||
        !IsPracticeStateValid(practice) || study.Id != "activity.study_at_home" ||
        study.DurationMinutes <= 0 || study.AllowanceMilliXp <= 0)
        return {"learning.invalid_paper_context", 0, 0, {}};

    // Reserve from the same dense runner sequence that lessons and routines use.
    ActivityRunnerState nextRunner = runner;
    ++nextRunner.Instance;
    nextRunner.AppliedInstance = nextRunner.Instance;
    if (std::binary_search(state.AppliedActivityInstances.begin(), state.AppliedActivityInstances.end(), nextRunner.Instance))
        return {"learning.runner_reused", 0, 0, {}};
    DayLoopEvent event;
    event.Kind = DayLoopEventKind::Completed;
    event.ActivityId = submitted.Brief.Id;
    event.Instance = nextRunner.Instance;
    event.At = *submitted.SubmittedAt;
    event.Minutes = AssessmentPaperMinutes;
    event.Outcome = "submitted_school_work";
    event.Label = PracticeLabel::LittleNewLearning;

    std::int32_t answerUnits = 0;
    for (const auto& group : submitted.Paper.Groups)
        answerUnits += group.Spent;
    // Credit the finite effort allowance, independently of diagnostic marking.
    ActivityDef effort = study;
    const std::int64_t allowance = static_cast<std::int64_t>(study.AllowanceMilliXp) * answerUnits *
        AssessmentUnitMinutes / study.DurationMinutes;
    if (allowance > PracticeMaxBaseMilli)
        return {"learning.invalid_paper_allowance", 0, 0, {}};
    effort.AllowanceMilliXp = static_cast<std::int32_t>(allowance);
    const auto base = ActivityLearningCredits(effort);
    if (effort.AllowanceMilliXp > 0 && base.empty())
        return {"learning.invalid_paper_allowance", 0, 0, {}};
    PracticeState nextPractice = practice;
    if (!base.empty())
    {
        const auto limited = RecordPractice(nextPractice, {DayLoopPracticeFamily(study), base, *submitted.StartedAt});
        if (!limited.Ok())
            return {std::string(PracticeErrorCode(limited.Error)), 0, 0, {}};
        event.Credited = limited.Credit;
        event.Label = limited.Label;
        event.Consolidates = limited.Consolidates;
    }
    AwakeningState nextState = state;
    const auto credited = ApplyActivityCredits(nextState, std::span(&event, 1));
    if (!credited.Ok())
        return {credited.Error, 0, 0, {}};
    // submitted may alias record. Copy before committing any of the owners.
    SchoolWorkRecord nextRecord = submitted;
    state = std::move(nextState);
    runner = std::move(nextRunner);
    practice = std::move(nextPractice);
    record = std::move(nextRecord);
    return {{}, credited.Applied, credited.Replayed, {std::move(event)}};
}

std::int32_t LearningLessonSkillMilli(const AwakeningState& state, const SchoolLessonBinding& binding,
                                     const AssessmentStructures& structures)
{
    std::string_view skillId;
    switch (binding.Structure)
    {
    case SchoolStructure::Model: skillId = P32FamilySkill(structures.Model, binding.FamilyId); break;
    case SchoolStructure::Explain: skillId = P32FamilySkill(structures.Explain, binding.FamilyId); break;
    case SchoolStructure::Investigate: skillId = P32FamilySkill(structures.Investigate, binding.FamilyId); break;
    case SchoolStructure::Coordinate: skillId = P32FamilySkill(structures.Coordinate, binding.FamilyId); break;
    }
    const auto* skill = state.Stats.FindSkill(skillId);
    return state.Seeded && skill != nullptr ? skill->LevelMilli : 0;
}

AssessmentSkills LearningWorkSkills(const AwakeningState& state)
{
    AssessmentSkills result;
    if (!state.Seeded)
        return result;
    for (const auto& skill : state.Stats.Skills)
        result.push_back({skill.SkillId, skill.LevelMilli});
    return result;
}
} // namespace PerfectWorld
