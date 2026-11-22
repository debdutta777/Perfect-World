#pragma once

#include "Attendance.h"
#include "GameClock.h"
#include "SchoolCalendar.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
struct WorldState;

/**
 * The quest log projection (order F3, "a quest window").
 *
 * There is no quest system and this adds none. Everything here is DERIVED, on demand, from state that is already
 * authoritative, so it persists nothing, hashes nothing and cannot drift from the world:
 *   Today        obligations: be at school by the first bell on a school day (read from the day loop's attendance record),
 *                the household or care home's evening jobs when an arrangement is remembered, the day loop's morning plan
 *                and its current commitment;
 *   Promises     each agreement the lane journal remembers (LaneJournalToday, its ids and its ShortTextKey);
 *   Discoveries  optional: places on the lane where nobody has been spoken to yet, with their visiting or opening hours
 *                (read from the authored door table, Doors.h).
 * Threads (Threads.h) are not in WorldState yet, so no thread appears; when they are wired in, a thread's current stage
 * can become another Today or Promises row without changing this shape.
 *
 * Presentation-free. Items carry stable ids, stable text keys and plain data (people, place keys, minutes of day); the UI
 * owns every sentence, the clock format and the empty states. Deterministic: the same world and instant give the same log,
 * in a fixed order (Today by due time then id; Promises in the journal's order; Discoveries in table order).
 *
 * Status (derived, never stored):
 *   Open    not yet due, or an open-ended arrangement with nothing to do right now;
 *   Due     do it now: its time has come, or a promise's person is receiving visitors (inside a door window);
 *   Done    satisfied today (the attendance record shows the day, the planned activity was completed);
 *   Missed  the chance for today has passed (school was not attended and the school day is over).
 * Evening jobs and the planned activities never become Done or Missed from the household side: no per-day completion
 * evidence exists in the model yet, so they stay Open or Due and say nothing untrue. A plan item is the player's
 * intention, so it is never Missed either.
 *
 * Tracking is NOT state of the world. Items start untracked. The UI keeps its tracked id for the session only and applies
 * it to a fresh projection with QuestLogTrack; at most one item is tracked at a time, and an id that no longer exists
 * simply tracks nothing.
 */

enum class QuestSection : std::uint8_t
{
    Today = 0,
    Promises,
    Discoveries
};

enum class QuestStatus : std::uint8_t
{
    Open = 0,
    Due,
    Done,
    Missed
};

/** What the Windows of an item mean. */
enum class QuestWindowKind : std::uint8_t
{
    None = 0,
    /** A private home's visiting hours (Doors.h VisitingWindows). */
    Visiting,
    /** A business's opening hours (Doors.h OpeningWindows or the single legacy window). */
    Opening
};

/** A daily half-open window in minutes of the day. */
struct QuestWindow
{
    std::int32_t OpenMinute = 0;
    std::int32_t CloseMinute = 0;
    friend bool operator==(const QuestWindow&, const QuestWindow&) = default;
};

struct QuestItem
{
    /** Stable across refreshes: "quest.today.school_bell", "quest.promise.rupa_ghosh.visit_promised", "quest.discover.tea_row_house". */
    std::string Id;
    QuestSection Section = QuestSection::Today;
    QuestStatus Status = QuestStatus::Open;
    /**
     * The text lookup key the UI resolves. Today and Discoveries: the Id's own family ("quest.today.school_bell",
     * "quest.today.plan.study_at_home", "quest.discover.tea_row_house"). Promises: the journal's ShortTextKey unchanged
     * ("journal.agreement.rupa_ghosh.visit_promised"). An unknown key must read as neutral text, never as the key.
     */
    std::string TextKey;
    /** The person an arrangement is with ("person.rupa_ghosh"); empty for school, plans and discoveries. */
    std::string PersonId;
    /** The authored label for PersonId (LaneJournalDisplayName); empty when there is no person or no label. */
    std::string WhoLabel;
    /** Where: an authored location id ("location.nabapara.tea_row_house"), "place.home" or "place.care_home" for the household, "place.school"; empty when unknown. */
    std::string PlaceKey;
    /** The activity id for a plan item or commitment ("activity.study_at_home"); empty otherwise. */
    std::string ActivityId;
    /** Absolute game minute something is due, or -1 when it has no time of its own. */
    GameInstant DueAt = -1;
    /** True when the item exists only Monday to Friday (a weekday arrangement); informational for the UI. */
    bool WeekdaysOnly = false;
    QuestWindowKind WindowKind = QuestWindowKind::None;
    /** Daily windows (minutes of the day) in which the person or place can be reached; empty when none are authored. */
    std::vector<QuestWindow> Windows;
    /** Set only by QuestLogTrack; never by the projection. */
    bool Tracked = false;

    friend bool operator==(const QuestItem&, const QuestItem&) = default;
};

struct QuestLogView
{
    /** The instant the log was projected at, and its GameClock::DayNumber. */
    GameInstant Now = 0;
    std::int64_t Day = 0;
    std::vector<QuestItem> Today;
    std::vector<QuestItem> Promises;
    std::vector<QuestItem> Discoveries;

    [[nodiscard]] bool Empty() const noexcept { return Today.empty() && Promises.empty() && Discoveries.empty(); }
    [[nodiscard]] std::size_t Count() const noexcept { return Today.size() + Promises.size() + Discoveries.size(); }
    friend bool operator==(const QuestLogView&, const QuestLogView&) = default;
};

/**
 * What the school obligation reads when the caller has it. Both pointers are borrowed and may be null: a null Calendar is the
 * Monday to Friday rule (AttendanceIsSchoolDay), a null School uses the Nabapara Vidyalaya numbers in the constants below
 * (the bell_schedule content for all three Class XI streams), which the tests check against the content.
 */
struct QuestLogContext
{
    const BellSchedule* School = nullptr;
    const SchoolCalendar* Calendar = nullptr;
};

/** The shipped bell times as minutes of the day: gates open 09:30, first bell 10:00, last bell (dismissal) 15:50. */
constexpr std::int32_t QuestDefaultGatesOpenMinute = 570;
constexpr std::int32_t QuestDefaultFirstBellMinute = 600;
constexpr std::int32_t QuestDefaultDismissalMinute = 950;

/** Evening jobs are due when evening opens (DayLoopEveningStartMinute); breakfast jobs at 07:00, when the care worker's day begins. */
constexpr std::int32_t QuestBreakfastJobsMinute = 7 * 60;

/** Stable ids of the Today rows that are not per plan item. */
inline constexpr std::string_view QuestIdSchoolBell = "quest.today.school_bell";
inline constexpr std::string_view QuestIdCommitment = "quest.today.commitment";

/** The log for `now`. Pass world.Clock.Now() for the live world. A `now` before 0 is treated as 0. */
[[nodiscard]] PERFECTWORLDCORE_API QuestLogView MakeQuestLog(const WorldState& world, GameInstant now);
[[nodiscard]] PERFECTWORLDCORE_API QuestLogView MakeQuestLog(const WorldState& world, GameInstant now, const QuestLogContext& context);

/** The item with this id in any section, or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const QuestItem* QuestLogFind(const QuestLogView& log, std::string_view id) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API QuestItem* QuestLogFind(QuestLogView& log, std::string_view id) noexcept;

/**
 * Marks exactly this item tracked and clears every other flag. An empty or unknown id clears all and returns false.
 * Done and Missed items can be tracked (the UI decides what to show for them).
 */
PERFECTWORLDCORE_API bool QuestLogTrack(QuestLogView& log, std::string_view id) noexcept;

/** The tracked item, or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const QuestItem* QuestLogTracked(const QuestLogView& log) noexcept;

/** The stable lowercase name of a section ("today", "promises", "discoveries") and status ("open", "due", "done", "missed"). */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view QuestSectionName(QuestSection section) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view QuestStatusName(QuestStatus status) noexcept;
} // namespace PerfectWorld
