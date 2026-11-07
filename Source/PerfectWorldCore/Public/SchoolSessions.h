#pragma once

#include "Assessment.h"
#include "DayLoop.h"

#include <variant>

namespace PerfectWorld
{
/** P2C contract v1. IDs are stable lowercase dotted tokens; UI strings are text keys,
 * never English fallbacks. school.session.* codes are also localization keys.
 * Borrowed catalogues must outlive a call. No function advances the world clock.
 * The reducer commits SchoolSessionsState + DayLoop + clock + R9 learning atomically.
 * Historical saves start empty: attendance is NOT evidence of a completed lesson. */
inline constexpr std::uint32_t SchoolSessionsContractVersion = 1;
inline constexpr std::int32_t SchoolForegroundLimit = 2;
inline constexpr std::size_t SchoolPeriodHistoryLimit = 4096;

using SchoolStructure = AssessmentStructure;
enum class SchoolLessonMode : std::uint8_t { Foreground, Routine, Attend };
/** Attend is supervised instruction without a new structure observation. Routine
 * requires existing family mastery. Both receive the same normal activity credit. */
struct SchoolLessonBinding
{
    std::string Id;
    std::string Stream;
    std::string Subject;
    /** Zero-based PERIOD index, excluding assembly/break. */
    std::int32_t Period = 0;
    SchoolStructure Structure = SchoolStructure::Model;
    std::string FamilyId;
    std::string TitleTextKey;
    std::string BriefTextKey;
    std::string RoleTextKey;
    friend bool operator==(const SchoolLessonBinding&, const SchoolLessonBinding&) = default;
};
struct SchoolSessionCatalogues
{
    DayLoopCatalogues Day;
    AssessmentStructures Structures;
};
struct SchoolIssue
{
    std::string Code;
    std::string Detail;
    [[nodiscard]] bool Ok() const noexcept { return Code.empty(); }
};
using SchoolLessonGoal = std::variant<ModelGoal, ExplainGoal, InvestigateGoal, CoordinateGoal>;
using SchoolLessonAction = std::variant<ModelAction, ExplainAction, InvestigateAction, CoordinateAction>;
using SchoolLessonOutcome = std::variant<std::monostate, ModelOutcome, ExplainOutcome, InvestigateOutcome, CoordinateOutcome, StructureOutcome>;
struct SchoolPeriodRecord
{
    std::int64_t Day = 0;
    std::int32_t Period = 0;
    std::string BindingId;
    SchoolLessonMode Mode = SchoolLessonMode::Attend;
    bool Completed = false;
    bool Cancelled = false;
    /** DayLoop activity identity: completion credit is emitted only once at Close. */
    std::uint64_t ActivityInstance = 0;
    SchoolLessonOutcome Outcome;
    friend bool operator==(const SchoolPeriodRecord&, const SchoolPeriodRecord&) = default;
};
struct SchoolLessonRun
{
    std::uint64_t Session = 0;
    SchoolLessonBinding Binding;
    SchoolLessonMode Mode = SchoolLessonMode::Attend;
    GameInstant OpenedAt = 0;
    bool ReadyToClose = false;
    SchoolLessonOutcome Outcome;
    friend bool operator==(const SchoolLessonRun&, const SchoolLessonRun&) = default;
};
struct SchoolSessionsState
{
    std::uint64_t NextSession = 1;
    std::optional<SchoolLessonRun> Active;
    /** Append order; one record per (day, period), including cancellations. Never prune
     * during a school year: prevents replays and foreground-limit resets after reload. */
    std::vector<SchoolPeriodRecord> Periods;
    StructureModelState Model;
    ExplainState Explain;
    InvestigateState Investigate;
    CoordinateState Coordinate;
    friend bool operator==(const SchoolSessionsState&, const SchoolSessionsState&) = default;
};
struct SchoolPeriodView
{
    SchoolIssue Issue;
    std::int64_t Day = 0;
    std::int32_t Period = -1;
    std::int32_t StartMinute = 0;
    std::int32_t EndMinute = 0;
    std::int32_t ForegroundUsed = 0;
    bool ForegroundAvailable = false;
    bool RoutineAvailable = false;
    bool AttendAvailable = false;
    std::string Room;
    std::string Subject;
    std::string ReasonTextKey;
};
struct SchoolLessonView
{
    std::optional<SchoolLessonRun> Run;
    /** Existing exposure structs carry the live beat, units and revealed selections.
     * Obtain authored choices from the matching immutable family catalogue. UI must
     * honor each runner's hidden-field documentation; never render solution flags. */
    StructureModelState Model;
    ExplainState Explain;
    InvestigateState Investigate;
    CoordinateState Coordinate;
};
struct SchoolSessionResult
{
    SchoolIssue Issue;
    std::uint64_t Session = 0;
    std::int32_t ChargedUnits = 0;
    bool ReadyToClose = false;
    /** Only Close emits DayLoop completion learning/evidence, after practice limits.
     * Feed these to AwakeningInputs.h's accepted R9 owner, NOT a second stats ledger. */
    std::vector<DayLoopEvent> Events;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
struct SchoolLessonQuote
{
    SchoolIssue Issue;
    std::int32_t Units = 0;
    std::int32_t UnitsAfter = 0;
};
[[nodiscard]] PERFECTWORLDCORE_API SchoolIssue ValidateSchoolLessonBinding(const SchoolLessonBinding& binding,
    const SchoolSessionCatalogues& catalogues);
[[nodiscard]] PERFECTWORLDCORE_API SchoolPeriodView QuerySchoolPeriod(const SchoolSessionsState& state,
    const DayLoopState& day, const SchoolLessonBinding& binding, const SchoolSessionCatalogues& catalogues);
[[nodiscard]] PERFECTWORLDCORE_API SchoolLessonView QuerySchoolLesson(const SchoolSessionsState& state);
/** school.lesson.open: skillMilli comes from R9 authority, never a widget. Open only
 * at the bell, with recorded attendance and a stay to period end. Reading is paused.
 * Foreground admission is consumed even on cancellation. Refusals mutate nothing. */
PERFECTWORLDCORE_API SchoolSessionResult OpenSchoolLesson(SchoolSessionsState& state, DayLoopState& day,
    const SchoolSessionCatalogues& catalogues, const SchoolLessonBinding& binding, SchoolLessonMode mode,
    const SchoolLessonGoal& goal, std::int32_t skillMilli, std::uint64_t worldSeed,
    const std::vector<ReceiptCandidate>& knownAlternatives = {});
[[nodiscard]] PERFECTWORLDCORE_API SchoolLessonQuote QuoteSchoolLessonAction(const SchoolSessionsState& state,
    const SchoolSessionCatalogues& catalogues, std::uint64_t session, const SchoolLessonAction& action);
/** school.lesson.choose: session rejects stale UI commands, including after reopen. */
PERFECTWORLDCORE_API SchoolSessionResult ChooseSchoolLesson(SchoolSessionsState& state,
    const SchoolSessionCatalogues& catalogues, std::uint64_t session, const SchoolLessonAction& action);
/** school.lesson.close: normal close requires a finished exposure. Cancel emits no
 * learning; elapsedMinutes is 0..duration-1 and is charged by DayLoop cancellation.
 * Normal close charges the bell period once. No caller-supplied outcome or marks. */
PERFECTWORLDCORE_API SchoolSessionResult CloseSchoolLesson(SchoolSessionsState& state, DayLoopState& day,
    const SchoolSessionCatalogues& catalogues, std::uint64_t session, bool cancel = false,
    std::int32_t elapsedMinutes = 0);
} // namespace PerfectWorld
