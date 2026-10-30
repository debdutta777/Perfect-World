#pragma once

#include "GameClock.h"

#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld
{
struct WorldState;

struct LaneJournalEncounter
{
    /** GameClock::DayNumber, never a civil date or wall time. */
    std::int64_t Day = 0;
    std::string PersonId;
    /** Absolute game minute of the first accepted authored choice that day, before its time charge. */
    GameInstant FirstMetMinute = 0;
    friend bool operator==(const LaneJournalEncounter&, const LaneJournalEncounter&) = default;
};

struct LaneJournalState
{
    /** Ascending (Day, PersonId), unique per person/day. No pruning; older dates remain authoritative. */
    std::vector<LaneJournalEncounter> Encounters;
    friend bool operator==(const LaneJournalState&, const LaneJournalState&) = default;
};

struct LaneJournalPerson
{
    std::string PersonId;
    std::string DisplayName;
    GameInstant FirstMetMinute = 0;
    friend bool operator==(const LaneJournalPerson&, const LaneJournalPerson&) = default;
};

struct LaneJournalAgreement
{
    std::string PersonId;
    /** The current agreement fact's stable ID. */
    std::string AgreementId;
    /** Stable text lookup key: "journal.agreement." + AgreementId without "fact.". */
    std::string ShortTextKey;
    friend bool operator==(const LaneJournalAgreement&, const LaneJournalAgreement&) = default;
};

struct LaneJournalView
{
    std::int64_t Day = 0;
    std::vector<LaneJournalPerson> PeopleMetToday;
    std::vector<LaneJournalAgreement> AgreementsRemembered;
    friend bool operator==(const LaneJournalView&, const LaneJournalView&) = default;
};

/**
 * Presentation-free snapshot for the Today menu. Pass GameClock::DayNumber(world.Clock.Now()) for today.
 * People are ordered by PersonId; agreements by (PersonId, AgreementId). Agreements are CURRENT remembered
 * arrangements across all days, derived solely from Conversations.Facts, even when the requested day has no encounters.
 * A replacement suppresses the old fact immediately; the household's implied generic agreement is shown only when
 * no specific arrangement exists. Completed favours, introductions and confirmation flags are not agreements.
 * Names are authored identity/role labels. UI owns text lookup, calendar/minute formatting, empty states and pausing.
 * Opening, declining, Leave, and refused choices do not record an encounter; accepting any authored choice does,
 * including free greetings. A paid choice spanning midnight belongs to its start day. Rollback removes its encounter.
 */
[[nodiscard]] PERFECTWORLDCORE_API LaneJournalView LaneJournalToday(const WorldState& world, std::int64_t day);
[[nodiscard]] PERFECTWORLDCORE_API std::string_view LaneJournalDisplayName(std::string_view personId) noexcept;

/** Reducer-only mutation, called for an accepted authored conversation choice before charging its minutes. */
PERFECTWORLDCORE_API void RecordLaneJournalEncounter(WorldState& world, std::string_view personId);
/** Known people with conversation history; nonnegative, nonfuture minutes on the recorded game day, canonical order. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsLaneJournalValid(const WorldState& world) noexcept;
/** Canonical pw.lane_journal/1 text; empty if the world journal is invalid. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseLaneJournal(const WorldState& world);
/** Strict envelope/order/count/calendar validation; failure leaves world.Journal untouched. */
[[nodiscard]] PERFECTWORLDCORE_API bool DeserialiseLaneJournal(std::string_view text, WorldState& world);
} // namespace PerfectWorld
