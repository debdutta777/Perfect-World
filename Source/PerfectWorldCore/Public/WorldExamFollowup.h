#pragma once

#include "GameClock.h"
#include "OpportunityReceipts.h"
#include "SchoolFollowupContent.h"

#include <optional>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

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
/**
 * P5-03: follow-up progress lives in WorldState::ExaminationFollowups, one record per (sitting, appointment), saved inside
 * the examination section (world v14) and hashed with it. Rows show the current sitting's result only; records of earlier
 * sittings stay as history and never reopen. Book is free and appends a session (an expired open session becomes Missed);
 * Cancel ends the open session; attendance-only records charge the authored duration once (Paid). For a task, Attend is
 * arrival only and Resolve charges the disclosed duration once, recording one receipt and an immutable completion snapshot.
 * A task appointment completes at most once in the whole life. A migrated pre-v14 Paid session is paid attendance, shown
 * as LegacyAttendance on a task row, and never a completion; a fresh task session can be booked after it. Every accepted
 * task transition advances the record's revision. No admissions, retake registration, learning or financial awards.
 */
/** The reserved pre-v14 flag namespace: generic SetWorldFlag refuses it and a v14 world may not hold it. */
inline constexpr std::string_view P407UFollowupFlagPrefix = "p407u.followup.";
/** True when `choice` is one of the supported P502 task's stable choice IDs (content-free; used by the save rules). */
[[nodiscard]] PERFECTWORLDCORE_API bool P503IsTaskChoice(std::string_view task, std::string_view choice) noexcept;
} // namespace PerfectWorld
