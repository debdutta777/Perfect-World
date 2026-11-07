#pragma once

#include "SchoolWeekState.h"
#include "WeekCompression.h"

namespace PerfectWorld
{
struct SchoolWeekLesson
{
    std::int32_t Day = 0;
    std::int32_t Entry = 0;
    SchoolLessonBinding Binding;
    SchoolLessonMode Mode = SchoolLessonMode::Attend;
};
struct SchoolWeekPlan
{
    WeekPlan Week;
    std::vector<SchoolWeekLesson> Lessons;
};
struct SchoolWeekBuild
{
    SchoolIssue Issue;
    SchoolWeekPlan Plan;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
struct SchoolWeekResult
{
    SchoolIssue Issue;
    WorldState World;
    SchoolSessionsState Sessions;
    WeekStop Stop = WeekStop::None;
    std::int32_t StopDay = -1;
    std::int32_t StopEntry = -1;
    std::int32_t InterruptsUsed = 0;
    /** Actual underlying results only. Receipt UI renders these in order; no
     * synthetic historic alternatives, attendance, learning or scene receipts. */
    std::vector<WeekResult> Segments;
    std::vector<SchoolSessionResult> Lessons;
    /** Raw events from actual activity applications (including lessons), for R9's
     * ApplyActivityCredits. Never also feed segment aggregate credits to that owner. */
    std::vector<DayLoopEvent> ActivityEvents;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
/** school.week.order: upsert/remove by ID. No two orders may cover the same weekday. */
PERFECTWORLDCORE_API SchoolIssue SetSchoolStandingOrder(SchoolWeekState& state,
    const SchoolStandingOrder& order, bool remove = false);
/** Expands the current school's actual bells into school arrival + lesson entries.
 * Extra commitments/known alternatives come from base, captured BEFORE compression.
 * Conflicting commitments refuse; firsts/choices retain WeekCompression semantics. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolWeekBuild BuildSchoolWeekPlan(const WorldState& world,
    const SchoolWeekState& standing, const SchoolSessionsState& sessions, const WeekPlan& base,
    const std::vector<SchoolLessonBinding>& bindings, const SchoolSessionCatalogues& catalogues);
/** school.week.resolve: each ordinary entry through CompressWeek; each lesson through
 * SchoolSessions and the existing DayLoop. No latent-growth owner is introduced.
 * Result learning must bind to R9 once in the SAME reducer transaction. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolWeekResult CompressSchoolWeek(const WorldState& world,
    const SchoolSessionsState& sessions, const SchoolWeekPlan& plan, const SchoolSessionCatalogues& catalogues);
} // namespace PerfectWorld
