#pragma once

#include "GameClock.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Append-only event history (ROADMAP M2-05; ALTERNATE_WORLD.md §7: crystallised
 * people have their pasts backfilled from the world's actual event log).
 *
 * Each entry records WHEN (game time), WHAT (a stable event type ID), WHY (the
 * sequence number of the earlier entry that caused it, or 0 for a root cause)
 * and WHO (the identity IDs of the parties, in the order the recorder gave
 * them; the first is the principal by convention). Entries are numbered 1, 2,
 * 3... in the order appended and never renumbered. There is deliberately no
 * remove, edit or insert API: the only mutation is Append.
 *
 * Standalone plain data, not yet wired into WorldState, the reducer, the world
 * hash or the save. To integrate: hold an EventLog in WorldState, mix
 * HashEventLog into the world hash as an "events" section, and store the
 * SerialiseEventLog lines in the save. Integer arithmetic only. Nothing throws.
 */

constexpr std::uint32_t EventLogFormatVersion = 1;
constexpr std::size_t EventLogMaxParties = 8;
/** Upper bound a parse accepts; refuses absurd counts before allocating. */
constexpr std::size_t EventLogMaxEntries = 1u << 24;

struct EventLogEntry
{
    /** 1-based position in the log; dense and never reused. */
    std::uint64_t Seq = 0;
    GameInstant At = 0;
    /** A stable lowercase token such as "person_registered" or "district_flooded" (see IsStableIdToken). */
    std::string Type;
    /** Seq of the earlier entry this one is a consequence of; 0 means none (a root cause). */
    std::uint64_t CauseSeq = 0;
    /** Identity IDs (IsValidIdentityId), at most EventLogMaxParties, no repeats. */
    std::vector<std::string> Parties;

    friend bool operator==(const EventLogEntry&, const EventLogEntry&) = default;
};

/** What a caller proposes to append; the log assigns the Seq. */
struct EventLogDraft
{
    GameInstant At = 0;
    std::string Type;
    std::uint64_t CauseSeq = 0;
    std::vector<std::string> Parties;
};

enum class EventLogStatus : std::uint8_t
{
    Appended,
    /** The log is full (EventLogMaxEntries). */
    Full,
    /** Type is not a valid token. */
    BadType,
    /** At is negative or earlier than the last entry (the log never goes back in time). */
    BadTime,
    /** CauseSeq names an entry that does not exist yet (a cause cannot come after its effect). */
    BadCause,
    /** A party is not a valid identity ID, is repeated, or there are more than EventLogMaxParties. */
    BadParties,
};

struct EventLogAppendResult
{
    EventLogStatus Status = EventLogStatus::BadType;
    /** The new entry's Seq when Appended, else 0. */
    std::uint64_t Seq = 0;

    [[nodiscard]] bool Ok() const { return Status == EventLogStatus::Appended; }
};

class PERFECTWORLDCORE_API EventLog
{
public:
    /** Validates the draft and, if good, adds it. A refused draft changes nothing. */
    EventLogAppendResult Append(const EventLogDraft& draft);

    [[nodiscard]] std::size_t Size() const { return Entries_.size(); }
    /** Read-only. Entries are in Seq order, Entries()[i].Seq == i + 1. */
    [[nodiscard]] const std::vector<EventLogEntry>& Entries() const { return Entries_; }
    /** The entry with this Seq, or nullptr. */
    [[nodiscard]] const EventLogEntry* Find(std::uint64_t seq) const;

    friend bool operator==(const EventLog&, const EventLog&) = default;

private:
    std::vector<EventLogEntry> Entries_;
};

/**
 * Fingerprint of the first `count` entries (clamped to the log size). It is a
 * running chain, so HashEventLogPrefix(log, k) equals HashEventLog of the log
 * as it stood after k appends: replaying the same appends gives the same
 * hash at every step.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashEventLogPrefix(const EventLog& log, std::size_t count);
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashEventLog(const EventLog& log);

/**
 * Text form, integers in plain decimal, one entry per line:
 *
 *   pw_events 1
 *   count <n>
 *   event <seq> <at> <type> <causeSeq> <partyCount> <party>...
 *   ...
 *   end <u64 hash of the log>
 *
 * Serialising the same log always gives the same bytes.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseEventLog(const EventLog& log);

/**
 * Strict inverse. Rebuilds the log by appending every entry through the same
 * validation as live play, so it returns nothing for truncation at any byte,
 * unknown fields, a wrong count, out-of-order Seq or time, a cause that does
 * not precede its effect, or a hash that does not match the content.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<EventLog> ParseEventLog(std::string_view text);
} // namespace PerfectWorld
