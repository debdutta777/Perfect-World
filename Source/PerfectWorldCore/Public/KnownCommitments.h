#pragma once

#include "ContentRegistry.h"
#include "LaneJournal.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace PerfectWorld
{
/** Content only; never persisted. Loaded from thread.evening_rota.eveningCommitment. */
struct EveningRotaDefinition
{
    std::int32_t StartMinute = -1;
    std::int32_t Minutes = 0;
    std::string ActivityId;
    friend bool operator==(const EveningRotaDefinition&, const EveningRotaDefinition&) = default;
};

struct KnownCommitmentContent
{
    /** Absent when a partial content database contains no evening-rota template. */
    std::optional<EveningRotaDefinition> EveningRota;
    friend bool operator==(const KnownCommitmentContent&, const KnownCommitmentContent&) = default;
};

struct KnownCommitmentContentResult
{
    KnownCommitmentContent Content;
    std::string Error;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Refuses missing/malformed metadata, unknown activities, duration mismatch and a window crossing midnight. */
[[nodiscard]] PERFECTWORLDCORE_API KnownCommitmentContentResult LoadKnownCommitmentContent(const ContentDb& content);

struct KnownCommitment
{
    GameInstant Start = 0;
    std::int32_t Minutes = 0;
    std::string ActivityId;
    friend bool operator==(const KnownCommitment&, const KnownCommitment&) = default;
};

struct KnownPromiseWindow
{
    std::string PersonId;
    std::string AgreementId;
    KnownCommitment Commitment;
    friend bool operator==(const KnownPromiseWindow&, const KnownPromiseWindow&) = default;
};

/**
 * Pure projection for journal.Day (GameClock::DayNumber, NOT campaign-relative day).
 * Reads CURRENT remembered arrangements, including on future planner days. Weekday dishes
 * recur Monday-Friday; sweep/cook recurs daily. The generic legacy agreement and alternate
 * days lack a determinable player turn and are omitted. Conflicting arrangements are omitted.
 * No completion history exists for these chores: this reports the standing arrangement only.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<KnownCommitment> KnownEveningCommitment(
    const LaneJournalView& journal, const KnownCommitmentContent& content);

/**
 * Current promises with an authored, determinable window on journal.Day, in (start, person,
 * agreement) order, without duplicates. End = Commitment.Start + Commitment.Minutes.
 * Today only the rota has such metadata. Visiting/opening hours, encounter times, dialogue
 * costs and untimed promises never supply appointment times or durations.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<KnownPromiseWindow> KnownTimedPromises(
    const LaneJournalView& journal, const KnownCommitmentContent& content);
} // namespace PerfectWorld
