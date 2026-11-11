#pragma once

#include "AwakeningInputs.h"
#include "SchoolWork.h"

namespace PerfectWorld
{
inline constexpr std::int32_t AcademicPriorPreparationMaxMilli = 70 * MilliPerXp;

struct AcademicPriorPreparationResult
{
    std::string Error;
    /** Prior preparation in milli-XP, above the common Academics baseline. */
    std::int32_t MilliXp = 0;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Uses the ONE existing academic_foundation table in skill.academics content via
 * ComputeStartingStats. All five ranks must strictly increase within 0..70 XP.
 * Rank 0 adds 0 XP, ranks 1..4 use the authored xpByRank values.
 * No observations, lessons, attribute evidence or conduct are manufactured. */
[[nodiscard]] PERFECTWORLDCORE_API AcademicPriorPreparationResult AcademicPriorPreparation(
    const ContentDb& content, std::int32_t rank);

/** New-life seed only. Validates bounded prior preparation, then delegates the
 * entire seed to R9. The content seed is already included, never added twice.
 * Caller commits once at creation confirmation. Loading never calls this. */
[[nodiscard]] PERFECTWORLDCORE_API AwakeningInputsSeedResult SeedLearningInputs(
    const ContentDb& content, const CharacterCreationSave& creation, std::uint64_t worldSeed);

/** Trusted accepted outcomes only. Credited is already after PracticeWindow.
 * Preserve runner Instance IDs, including zero-learning completions. R9 owns
 * replay handling and atomic batch refusal. Refused outcomes change nothing. */
PERFECTWORLDCORE_API AwakeningCreditsResult ApplyLearningCredits(
    AwakeningState& state, const SchoolSessionResult& accepted);
PERFECTWORLDCORE_API AwakeningCreditsResult ApplyLearningCredits(
    AwakeningState& state, const DayLoopResult& accepted);
/** For accepted compressed.ActivityEvents, never segment/receipt aggregates. */
PERFECTWORLDCORE_API AwakeningCreditsResult ApplyLearningCredits(
    AwakeningState& state, std::span<const DayLoopEvent> acceptedEvents);

struct SchoolWorkLearningResult
{
    std::string Error;
    std::size_t Applied = 0;
    std::size_t Replayed = 0;
    /** Exact post-limit completion input passed to ApplyActivityCredits. */
    std::vector<DayLoopEvent> Events;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Pure transaction over an accepted ApplySchoolWork Submit transition.
 * record is the current authoritative record, submitted is its accepted copy.
 * Submit may only finish the existing draft, preserving all paid answers.
 * The stable Brief.Id + persisted SubmittedAt make replay inert BEFORE practice
 * or runner allocation. Commit record, runner, practice and R9 together.
 *
 * study is the loaded activity.study_at_home definition. Answer effort earns its
 * allowance prorated by answer minutes / study duration, then RecordPractice in
 * the SAME family as ordinary study. Marks, grades, correctness and household
 * are not inputs. Survey-only/empty papers consume an ID with zero learning.
 * No attribute or projection evidence is invented for a written submission.
 *
 * Allocates one ID from the idle life's shared runner and immediately settles
 * it, preserving Instance == AppliedInstance. No separate ID namespace, clock,
 * condition or receipt charge. The caller charges Submit's WorldMinutes once
 * and rolls everything back if that charge or this transaction refuses.
 * Historical submitted records stay inert, with no reconstructed learning. */
PERFECTWORLDCORE_API SchoolWorkLearningResult ApplySchoolWorkLearning(
    AwakeningState& state, ActivityRunnerState& runner, PracticeState& practice,
    SchoolWorkRecord& record, const SchoolWorkRecord& submitted, const ActivityDef& study);

/** Read the existing latent owner at the two S04 skill seams. Unknown family or
 * skill reads zero. Family.SkillId, rather than a guessed subject mapping. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t LearningLessonSkillMilli(
    const AwakeningState& state, const SchoolLessonBinding& binding, const AssessmentStructures& structures);
[[nodiscard]] PERFECTWORLDCORE_API AssessmentSkills LearningWorkSkills(const AwakeningState& state);
} // namespace PerfectWorld
