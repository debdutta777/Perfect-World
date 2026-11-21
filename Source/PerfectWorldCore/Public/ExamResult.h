#pragma once

#include "SchoolFollowupContent.h"

#include <limits>

namespace PerfectWorld
{
inline constexpr std::uint32_t P403ExamResultVersion = 1;
inline constexpr std::size_t P403NoEvidence = std::numeric_limits<std::size_t>::max();

/** Stable localization keys. Arguments are structured IDs/integers, never prose.
 * Evidence indexes address Attempts; index Attempts.size() names retained Active.
 * Global reasons use P403NoEvidence. TargetId names a credential, offer, outcome
 * or follow-up; Dimension is a stable ID such as discipline or score_points.
 * Value is the observed/output value; WeightPercent is nonzero only for the two
 * raw quality inputs. Weighted rounding is explained by the combined quality row.
 */
struct P403ExamReason
{
    std::string_view TextKey;
    std::string_view Dimension;
    std::size_t Evidence = P403NoEvidence;
    std::string TargetId;
    std::int64_t Value = 0;
    std::int32_t WeightPercent = 0;
    friend bool operator==(const P403ExamReason&, const P403ExamReason&) = default;
};

struct P403ExamResult
{
    SchoolIssue Issue;
    TowerParticipation Participation = TowerParticipation::Unset;
    TowerResult Score;
    std::string_view Outcome;
    /** Owned provenance, including choices. Safe after source state destruction. */
    std::vector<TowerSessionAttempt> Attempts;
    std::optional<TowerRunnerRun> Active;
    std::vector<P403ExamReason> Reasons;
    std::vector<SchoolFollowupAction> NextSteps;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};

[[nodiscard]] PERFECTWORLDCORE_API std::span<const std::string_view> P403ExamReasonKeys() noexcept;
/** Terminal read-only report over authoritative TowerSession evidence. Delegates
 * scoring/time to TowerSession/Tower and classification/availability to school
 * follow-ups. No marks, practice bonus, RNG, rewards, bookings or clock mutation.
 * A usable outcome-specific appointment is mandatory; missing content refuses.
 * Live/inconsistent receipts refuse with no partial report. This is not a saved
 * choice-provenance authenticator: P4-02 must replay/validate state before use.
 * Result, reasons and next steps are derived, never separate persisted authority.
 */
[[nodiscard]] PERFECTWORLDCORE_API P403ExamResult P403ResolveExamResult(
    const TowerSessionState& state, const SchoolFollowupContent& followups);
} // namespace PerfectWorld
