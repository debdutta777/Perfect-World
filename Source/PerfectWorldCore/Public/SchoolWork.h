#pragma once

#include "SchoolSessions.h"

namespace PerfectWorld
{
inline constexpr std::uint32_t SchoolWorkContractVersion = 1;
inline constexpr std::size_t SchoolWorkLimit = 512;
enum class SchoolWorkKind : std::uint8_t { Homework, AcademicAssessment };
/** One stable ID per assignment/sitting, never per submit attempt. Recovery remains
 * available after DueAt; lateness is disclosed separately from diagnostic marks. */
struct SchoolWorkBrief
{
    std::string Id;
    std::string PaperId;
    SchoolWorkKind Kind = SchoolWorkKind::Homework;
    GameInstant AvailableAt = 0;
    GameInstant DueAt = 0;
    std::string TitleTextKey;
    std::string BriefTextKey;
    std::string CriteriaTextKey;
    std::string RecoveryTextKey;
    friend bool operator==(const SchoolWorkBrief&, const SchoolWorkBrief&) = default;
};
struct SchoolWorkRecord
{
    SchoolWorkBrief Brief;
    GameInstant IssuedAt = 0;
    /** World remains paused at this instant while the paper is played. */
    std::optional<GameInstant> StartedAt;
    std::optional<GameInstant> SubmittedAt;
    AssessmentState Paper;
    /** Derived by MarkAssessment; persisted paper is the authority on reload. */
    friend bool operator==(const SchoolWorkRecord&, const SchoolWorkRecord&) = default;
};
struct SchoolWorkState
{
    /** Sorted by Brief.Id. One paper at a time may be active across all records. */
    std::vector<SchoolWorkRecord> Records;
    friend bool operator==(const SchoolWorkState&, const SchoolWorkState&) = default;
};
enum class SchoolWorkActionKind : std::uint8_t { Begin, Survey, Allocate, Answer, Submit, Cancel };
struct SchoolWorkCommand
{
    static constexpr std::string_view TypeId = "school.work.apply";
    std::string WorkId;
    SchoolWorkActionKind Kind = SchoolWorkActionKind::Begin;
    std::size_t Group = 0;
    std::int32_t Units = 0;
    AssessmentAnswer Answer;
};
struct SchoolWorkResult
{
    SchoolIssue Issue;
    std::int32_t ChargedUnits = 0;
    std::optional<AssessmentMarking> Marking;
    /** Submit spends the declared 90-minute paper block once; units measure choices
     * within it, not extra world charges. Integrator reserves the block at Begin. */
    std::int32_t WorldMinutes = 0;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
struct SchoolWorkView
{
    SchoolWorkBrief Brief;
    bool Available = false;
    bool Overdue = false;
    bool LateSubmission = false;
    bool CanBegin = false;
    bool CanSubmit = false;
    AssessmentState Paper;
    std::vector<AssessmentSurveyLine> Survey;
    std::optional<AssessmentMarking> Marking;
    std::string StatusTextKey;
};
/** Academic records contain academic marks only; tower rank is never a mark. */
struct SchoolAcademicRecordView { std::vector<SchoolWorkView> Work; };
/** school.work.issue: system/content-trigger only. No retroactive issue before now. */
PERFECTWORLDCORE_API SchoolIssue IssueSchoolWork(SchoolWorkState& state, const SchoolWorkBrief& brief,
    const AssessmentCatalogue& papers, GameInstant now);
/** Skills supplied by accepted R9 owner. Submission finishes unanswered groups and
 * calls MarkAssessment. Duplicate submission refuses without charging time. Cancel
 * drops only the unsent draft; it never removes a submitted academic record. */
PERFECTWORLDCORE_API SchoolWorkResult ApplySchoolWork(SchoolWorkState& state, const AssessmentCatalogue& papers,
    const SchoolWorkCommand& command, const AssessmentSkills& skills, GameInstant now);
[[nodiscard]] PERFECTWORLDCORE_API AssessmentQuote QuoteSchoolWorkAnswer(const SchoolWorkState& state,
    std::string_view workId, const AssessmentAnswer& answer);
[[nodiscard]] PERFECTWORLDCORE_API SchoolAcademicRecordView QuerySchoolAcademicRecord(const SchoolWorkState& state,
    const AssessmentCatalogue& papers, GameInstant now);
} // namespace PerfectWorld
