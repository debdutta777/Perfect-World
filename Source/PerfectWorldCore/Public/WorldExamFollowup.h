#pragma once

#include "GameClock.h"
#include "OpportunityReceipts.h"
#include "SchoolFollowupContent.h"

#include <optional>

namespace PerfectWorld
{
enum class P407UFollowupAction : std::uint8_t { Book, Attend, Cancel, P502Start, P502Leave, P502Resume, P502Resolve };
/** Stable identity, disclosed appointment instant, revision and choice cross the command boundary. */
struct P407UFollowupCommand
{
    static constexpr std::string_view TypeId = "exam.followup.apply";
    P407UFollowupAction Kind = P407UFollowupAction::Book;
    std::string ActionId;
    GameInstant ExpectedAt = 0;
    /** P502Resolve only. IDs come from the current authoritative task choices. */
    std::string P502ChoiceId;
    std::uint64_t P502ExpectedRevision = 0;
};
enum class P502TaskStage : std::uint8_t { None, Offered, Arrived, Active, Paused, Completed, LegacyAttendance };
struct P502TaskChoice
{
    std::string Id;
    /** Stable record values, presented as plans/enquiries, never institutional awards. */
    std::vector<std::string> Records;
};
struct P407UFollowupView
{
    SchoolFollowupAction Action;
    GameInstant At = 0;
    bool Booked = false;
    bool Attended = false;
    bool CanBook = false;
    bool CanAttend = false;
    P502TaskStage P502Stage = P502TaskStage::None;
    std::vector<P502TaskChoice> P502Choices;
    std::optional<P502TaskChoice> P502Record;
    /** Conflicting known IDs and the next free slot; zero means no slot within 32 days. */
    std::vector<std::string> P502Conflicts;
    GameInstant P502AlternativeAt = 0;
    bool P502CanStart = false;
    bool P502CanResume = false;
    bool P502CanResolve = false;
    std::vector<ReceiptCandidate> P502Alternatives;
    /** Changes on each accepted task transition, including a same-instant cancel/rebook. */
    std::uint64_t P502Revision = 0;
};
/** Reservations and attendance use the existing saved/hashed world flag map.
 * Keys: p407u.followup.<appointmentId>.book.<instant> and .done.<instant>.
 * Book is free; attendance-only records charge the authored duration once. An
 * expired booking can be replaced; Cancel releases an optional reservation.
 * No admissions, retake registration, learning or financial awards are implied.
 */
inline constexpr std::string_view P407UFollowupFlagPrefix = "p407u.followup.";
/** P502 task flags are provisional, pending P5-03 validation/migration. No save schema or hash change here.
 * Selected task attendance is arrival only; Resolve charges the disclosed duration and records one receipt.
 * Legacy .done flags remain paid attendance and never imply a completed task. Cancel/Leave earn nothing.
 */
} // namespace PerfectWorld
