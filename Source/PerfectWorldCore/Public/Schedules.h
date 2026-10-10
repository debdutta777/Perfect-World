#pragma once

#include "GameClock.h"

#include <cstdint>
#include <map>
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
 * Schedules and minimal off-screen progress (ROADMAP M2-06; SYSTEMS.md principle 5,
 * "declarative schedules resolved at event boundaries"; SYSTEMS_CATALOGUE.md "Schedules").
 *
 * Where a person is comes from a pure function of their schedule, their
 * exceptions, a travel table and a game instant. Nobody walks around off screen:
 * ResolveSchedule(person, travel, t) is the whole truth, so the answer is the
 * same live and after a year skip. This module is standalone integer-only plain
 * data plus free functions; it is not yet part of WorldState, the reducer, the
 * world hash or the save. To integrate: store PersonSchedule (the routine is
 * content, the Exceptions are the catalogue's `Overrides`) and SchedulePersonState
 * per tier-0 identity, hash them with the two Hash functions below, and call
 * AdvanceSchedulePerson from the reducer's time step.
 *
 * Time. Instants are GameClock minutes (0 = Monday 06:00, 2 June 2042). A
 * routine entry repeats weekly on the weekdays in its mask, between two minutes
 * of the calendar day; it never crosses midnight (an overnight sleep is two
 * entries). An exception is an absolute range of instants.
 *
 * Priority and overlap. Routine entries and exceptions both carry a priority
 * (0..ScheduleMaxPriority). At any minute the winner is the covering item with
 * the highest priority; ties go to an exception over a routine entry, then
 * between exceptions to the one that starts LATER (the newer commitment
 * supersedes), then to the smaller exception ID. So an exception with a
 * priority equal to or above the routine beats it, and an exception below it
 * does not. Two routine entries that overlap on a shared weekday must differ in
 * priority (the higher wins); equal-priority overlap is refused by validation
 * as ambiguous. A higher item cuts a lower one into pieces; each piece is a stay.
 *
 * The resolved day. Winning items become stays, in time order. Between two stays
 * the person lingers (idle) in the earlier stay's zone, then leaves in time to
 * arrive exactly when the next stay starts, taking the travel-table time between
 * the two zones (no time when the zones are equal). Travel is never instant. If
 * the gap is shorter than the trip, the deficit is taken from the stay with the
 * LOWER priority (equal priority: the earlier stay, who leaves early): the
 * earlier stay ends sooner, or the later one starts later (arrives late). A stay
 * squeezed to nothing disappears and the check repeats against the one before.
 * The trip and any shortened stay carry the Squeezed flag, and a shortened
 * activity earns proportionally less. So the walk to an assembly is taken out of
 * the school day, the walk back delays the rest of the day, and sleep is not cut
 * short to make a morning trip. ValidateScheduleRoutineTravel refuses a routine that would
 * squeeze anything. The first stay of the working window has no predecessor and
 * is never squeezed. With no stays the person is idle in HomeZone.
 *
 * Locality. A resolution looks at the 8 days either side of the instant's day
 * (ScheduleHorizonDays), so a result is a function of (schedule, exceptions,
 * travel, instant) alone, whatever came before. Past that horizon a missing
 * predecessor means HomeZone and a missing successor means no departure.
 *
 * Event boundaries. AdvanceSchedulePerson emits an event only where the resolved
 * identity changes (status, zone, activity or role, source, squeezed flag), found
 * from the day plans, never minute by minute. Progress is a pure function of the
 * boundaries it crosses, so advancing T1->T3 equals T1->T2->T3 and equals
 * stepping every minute.
 *
 * Numbers chosen here because the docs leave them open (listed in the M2-06
 * report): the constants below.
 */

/** Serialised-format version for all three text forms; bump on any layout change. */
constexpr std::uint32_t ScheduleFormatVersion = 1;
/** Entries a person's routine may hold. */
constexpr std::size_t ScheduleMaxRoutineEntries = 128;
/** Exceptions a person may hold at once; callers prune the ones long past. */
constexpr std::size_t ScheduleMaxExceptions = 4096;
/** Directed legs a travel table may hold. */
constexpr std::size_t ScheduleMaxTravelLegs = 4096;
/** Priorities run 0..1000. */
constexpr std::int32_t ScheduleMaxPriority = 1000;
/** A single travel leg takes 1..720 minutes (twelve hours). */
constexpr std::int32_t ScheduleMaxTravelMinutes = 720;
/** Used by a resolution when the table has no leg for a pair; validation refuses a table that lacks one. */
constexpr std::int32_t ScheduleFallbackTravelMinutes = 60;
/** Days looked at either side of the instant's day. */
constexpr std::int32_t ScheduleHorizonDays = 8;
/** Latest instant any schedule, exception or advance may name: one hundred years. */
constexpr GameInstant ScheduleMaxInstant = 100 * 366 * MinutesPerDay;
/** An activity stay counts as one session when the minutes realised are at least this share of its nominal length. */
constexpr std::int32_t ScheduleSessionMinPartsPerThousand = 500;
/** How far NextScheduleBoundary searches before concluding the person never changes (a little over a year). */
constexpr std::int32_t ScheduleBoundaryScanDays = 400;
/** Source ID prefix of routine entries ("routine:<index>"); exception IDs may not use it. */
constexpr std::string_view ScheduleRoutineSourcePrefix = "routine:";
/** Weekday mask with every day set. */
constexpr std::uint8_t ScheduleAllWeekdays = 0x7F;

/** The mask bit for a weekday (Sunday is bit 0, as in GameClock's Weekday). */
constexpr std::uint8_t ScheduleWeekdayBit(Weekday day)
{
    return static_cast<std::uint8_t>(1u << static_cast<unsigned>(day));
}

/** Whether Doing names a content activity (which is tallied off screen) or a plain role such as "sleeping". */
enum class ScheduleDoingKind : std::uint8_t
{
    Activity,
    Role,
};

/** One weekly routine block. Times are minutes of the calendar day; End is exclusive and at most 1440. */
struct ScheduleEntry
{
    std::uint8_t WeekdayMask = ScheduleAllWeekdays;
    std::int32_t StartMinute = 0;
    std::int32_t EndMinute = 0;
    std::string Zone;
    ScheduleDoingKind Kind = ScheduleDoingKind::Role;
    /** An activity ID (such as "activity.study_at_home") or a role token. */
    std::string Doing;
    std::int32_t Priority = 0;

    friend bool operator==(const ScheduleEntry&, const ScheduleEntry&) = default;
};

/** An override for an absolute range [From, To): a holiday, a sick week, a one-off appointment. */
struct ScheduleException
{
    /** A unique token; it also breaks priority ties. Never starts with "routine:". */
    std::string Id;
    GameInstant From = 0;
    GameInstant To = 0;
    std::string Zone;
    ScheduleDoingKind Kind = ScheduleDoingKind::Role;
    std::string Doing;
    std::int32_t Priority = 0;

    friend bool operator==(const ScheduleException&, const ScheduleException&) = default;
};

/** One person's whole schedule. */
struct PersonSchedule
{
    /** An identity ID from IdentityRegistry. */
    std::string PersonId;
    /** Where the person is when nothing else places them. */
    std::string HomeZone;
    std::vector<ScheduleEntry> Routine;
    std::vector<ScheduleException> Exceptions;

    friend bool operator==(const PersonSchedule&, const PersonSchedule&) = default;
};

/** Minutes to go from one zone to another. A leg also serves the reverse trip unless the table lists that leg itself. */
struct ScheduleTravelLeg
{
    std::string From;
    std::string To;
    std::int32_t Minutes = 0;

    friend bool operator==(const ScheduleTravelLeg&, const ScheduleTravelLeg&) = default;
};

/** The caller's explicit travel table. Canonical order is strictly ascending by (From, To), which also refuses duplicates. */
struct ScheduleTravelTable
{
    std::vector<ScheduleTravelLeg> Legs;

    friend bool operator==(const ScheduleTravelTable&, const ScheduleTravelTable&) = default;
};

enum class ScheduleStatus : std::uint8_t
{
    /** At Zone doing Doing. */
    Stay,
    /** Moving from FromZone to Zone, on the way to do Doing. */
    Travel,
    /** At Zone with nothing scheduled. */
    Idle,
};

/** The answer to "where is this person and what are they doing". */
struct ScheduleResolution
{
    bool Valid = false;
    ScheduleStatus Status = ScheduleStatus::Idle;
    /** Where the person is (Stay, Idle) or is heading (Travel). */
    std::string Zone;
    /** Travel only: where the trip began. */
    std::string FromZone;
    ScheduleDoingKind Kind = ScheduleDoingKind::Role;
    /** Stay: what they are doing. Travel: what they are going to do. Idle: empty. */
    std::string Doing;
    /** "routine:<index>" or an exception ID; for Travel, the stay being travelled to; empty for Idle. */
    std::string SourceId;
    /** The segment containing the instant: [SegmentStart, SegmentEnd). A stay's bounds are its piece of the block, not the block. */
    GameInstant SegmentStart = 0;
    GameInstant SegmentEnd = 0;
    /** Share of the segment elapsed at the instant, floor(1000 * elapsed / length); 0 for a zero-length segment. */
    std::int32_t ProgressPermille = 0;
    /** The trip needed more time than the gap, so this trip or this stay was shortened to make room (see the header comment). */
    bool Squeezed = false;
    /** Stay only: the full length of the routine block or exception this piece belongs to. */
    std::int64_t NominalMinutes = 0;
};

enum class ScheduleEventKind : std::uint8_t
{
    /** A stay of an activity finished; Minutes and Counted say what it earned. Listed before the began-event at the same instant. */
    ActivityEnded,
    StayBegan,
    TravelBegan,
    IdleBegan,
};

/** One boundary the person crossed. For *Began kinds the fields describe the new segment. */
struct ScheduleEvent
{
    GameInstant At = 0;
    ScheduleEventKind Kind = ScheduleEventKind::IdleBegan;
    std::string Zone;
    std::string FromZone;
    ScheduleDoingKind DoingKind = ScheduleDoingKind::Role;
    std::string Doing;
    std::string SourceId;
    /** ActivityEnded: minutes actually spent. Otherwise 0. */
    std::int64_t Minutes = 0;
    /** ActivityEnded: length of the block this stay was cut from. Otherwise 0. */
    std::int64_t NominalMinutes = 0;
    /** ActivityEnded: whether it counts as a session (Minutes reached ScheduleSessionMinPartsPerThousand of nominal). */
    bool Counted = false;
    bool Squeezed = false;

    friend bool operator==(const ScheduleEvent&, const ScheduleEvent&) = default;
};

/** What a person has done off screen, per activity ID. */
struct ScheduleActivityTally
{
    std::int64_t Sessions = 0;
    std::int64_t Minutes = 0;

    friend bool operator==(const ScheduleActivityTally&, const ScheduleActivityTally&) = default;
};

/** The only per-person state off-screen progress adds: how far time has been resolved, and what it produced. */
struct SchedulePersonState
{
    std::string PersonId;
    /** Everything up to and including this instant has been resolved. */
    GameInstant At = 0;
    std::map<std::string, ScheduleActivityTally> Tally;

    friend bool operator==(const SchedulePersonState&, const SchedulePersonState&) = default;
};

struct ScheduleAdvanceResult
{
    bool Ok = false;
    /** Why an advance was refused; empty when Ok. */
    std::string Error;
    /** Boundary events in (From, To], in time order. */
    std::vector<ScheduleEvent> Events;
};

/** "" when valid, otherwise the first problem found. Resolution and advance assume validated data. */
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidatePersonSchedule(const PersonSchedule& person);
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateScheduleTravel(const ScheduleTravelTable& table);
/** "" when every pair of distinct zones the person can be in has a leg (either direction) in the table. */
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateScheduleTravelCoverage(const PersonSchedule& person,
                                                                              const ScheduleTravelTable& table);
/** "" when the routine alone (no exceptions) never squeezes a trip or a stay, week after week, wrap-around included. */
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateScheduleRoutineTravel(const PersonSchedule& person,
                                                                              const ScheduleTravelTable& table);
/** Whether the routine alone covers all 7 x 1440 minutes of the week (SYSTEMS_CATALOGUE.md: every schedule covers 24 hours on every day type). */
[[nodiscard]] PERFECTWORLDCORE_API bool ScheduleCoversWholeWeek(const PersonSchedule& person);

/** Minutes from one zone to another: 0 for the same zone, the listed leg, else the reverse leg, else the fallback. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ScheduleTravelMinutes(const ScheduleTravelTable& table,
                                                                       std::string_view from, std::string_view to);

/** Pure. Valid is false for an instant outside [0, ScheduleMaxInstant]. */
[[nodiscard]] PERFECTWORLDCORE_API ScheduleResolution ResolveSchedule(const PersonSchedule& person,
                                                                       const ScheduleTravelTable& travel, GameInstant at);

/** The first instant after `after` at which the resolved identity changes, or nothing within ScheduleBoundaryScanDays. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<GameInstant> NextScheduleBoundary(const PersonSchedule& person,
                                                                                   const ScheduleTravelTable& travel,
                                                                                   GameInstant after);

/** A fresh state that has resolved up to `at` and tallied nothing. */
[[nodiscard]] PERFECTWORLDCORE_API SchedulePersonState StartSchedulePerson(std::string_view personId, GameInstant at);

/**
 * Advance the person from state.At to `to`, resolving only at event boundaries,
 * tallying finished activity stays and returning the boundary events. Refused
 * (state untouched) when the person IDs differ, `to` is before state.At, past
 * ScheduleMaxInstant, or more than MaxSingleAdvanceMinutes ahead.
 */
PERFECTWORLDCORE_API ScheduleAdvanceResult AdvanceSchedulePerson(const PersonSchedule& person,
                                                                 const ScheduleTravelTable& travel,
                                                                 SchedulePersonState& state, GameInstant to);

[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashPersonSchedule(const PersonSchedule& person);
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashScheduleTravel(const ScheduleTravelTable& table);
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashSchedulePersonState(const SchedulePersonState& state);

/**
 * Text forms: one line per record, plain decimal integers, lowercase tokens,
 * a count before every list and a closing "end <hash>" line. Serialising the
 * same value always gives the same bytes. Each Parse is a strict inverse: it
 * returns nothing for truncation at any byte, unknown or reordered fields, a
 * bad count, invalid data, or a hash that does not match.
 *
 *   pw.schedule 1                          pw.schedule_travel 1         pw.schedule_state 1
 *   person <id>                            legs <n>                     person <id>
 *   home <zone>                            leg <from> <to> <minutes>    at <instant>
 *   routine <n>                            ...                          tally <n>
 *   entry <mask> <start> <end> <priority>  end <hash>                   activity <id> <sessions> <minutes>
 *         <activity|role> <zone> <doing>                                ...   (ascending ID order)
 *   ...                                                                 end <hash>
 *   exceptions <n>
 *   exception <id> <from> <to> <priority>
 *             <activity|role> <zone> <doing>
 *   ...
 *   end <hash>
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialisePersonSchedule(const PersonSchedule& person);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<PersonSchedule> ParsePersonSchedule(std::string_view text);
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseScheduleTravel(const ScheduleTravelTable& table);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<ScheduleTravelTable> ParseScheduleTravel(std::string_view text);
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseSchedulePersonState(const SchedulePersonState& state);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<SchedulePersonState> ParseSchedulePersonState(std::string_view text);
} // namespace PerfectWorld
