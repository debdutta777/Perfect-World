#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The clock everything else hangs off.
 *
 * One monotonic count of game minutes since the campaign epoch. Exploration
 * advances it at one game minute per real second; activities and sleep jump
 * through the same call, so there is exactly one path by which time moves and
 * exactly one place where scheduled things fire. Time never runs backwards.
 *
 * The clock is pure data and arithmetic. It has no engine dependency, does not
 * know what a frame is, and is fully covered by the portable test suite.
 */
using GameInstant = std::int64_t;

constexpr std::int64_t MinutesPerHour = 60;
constexpr std::int64_t MinutesPerDay = 24 * MinutesPerHour;

/** The campaign opens on the first Monday of Class XI. */
constexpr std::int32_t CampaignStartYear = 2042;
constexpr std::int32_t CampaignStartMonth = 6;
constexpr std::int32_t CampaignStartDay = 2;
/** Play opens at 06:00, before school. */
constexpr std::int32_t CampaignStartHour = 6;

/**
 * A single advance may not exceed ten years. Anything larger is a defect in
 * the caller rather than a long sleep, and silently accepting it would let one
 * bad number destroy a save.
 */
constexpr std::int64_t MaxSingleAdvanceMinutes = 10 * 366 * MinutesPerDay;

enum class Weekday
{
    Sunday = 0,
    Monday,
    Tuesday,
    Wednesday,
    Thursday,
    Friday,
    Saturday
};

struct CalendarTime
{
    std::int32_t Year = CampaignStartYear;
    std::int32_t Month = 1;
    std::int32_t Day = 1;
    std::int32_t Hour = 0;
    std::int32_t Minute = 0;
    Weekday DayOfWeek = Weekday::Monday;
};

/**
 * Something scheduled to happen at an instant. The clock stores the ID only;
 * what it means is the caller's business, which keeps the clock free of every
 * other system.
 */
struct ScheduledEvent
{
    std::int64_t Handle = 0;
    GameInstant When = 0;
    std::string Id;
};

/** What an advance passed through, in the order it happened. */
struct AdvanceResult
{
    GameInstant From = 0;
    GameInstant To = 0;
    /** Events whose instant fell in (From, To], in chronological order. */
    std::vector<ScheduledEvent> Fired;
    /** Midnights crossed, so callers can run daily rollover exactly once. */
    std::int32_t DaysCrossed = 0;
    bool Rejected = false;
    std::string RejectionReason;
};

struct GameClockSave
{
    GameInstant Now = 0;
    std::int64_t NextHandle = 1;
    std::vector<ScheduledEvent> Pending;
};

[[nodiscard]] PERFECTWORLDCORE_API std::string_view WeekdayName(Weekday day) noexcept;

/** True for a calendar date that exists, including leap-day handling. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsValidCivilDate(std::int32_t year, std::int32_t month,
                                                          std::int32_t day) noexcept;

class PERFECTWORLDCORE_API GameClock
{
public:
    GameClock();

    [[nodiscard]] GameInstant Now() const noexcept { return Now_; }
    [[nodiscard]] CalendarTime Calendar() const noexcept { return ToCalendar(Now_); }

    /** Minutes since the campaign epoch for a civil date and time. */
    [[nodiscard]] static GameInstant FromCalendar(std::int32_t year, std::int32_t month, std::int32_t day,
                                                   std::int32_t hour, std::int32_t minute) noexcept;
    [[nodiscard]] static CalendarTime ToCalendar(GameInstant instant) noexcept;

    /** The instant midnight opens on the day containing `instant`. */
    [[nodiscard]] static GameInstant StartOfDay(GameInstant instant) noexcept;
    [[nodiscard]] static std::int64_t DayNumber(GameInstant instant) noexcept;

    /**
     * Move time forward. Rejects a negative or absurd advance rather than
     * accepting it, because time running backwards would corrupt every
     * schedule that depends on this clock.
     */
    [[nodiscard]] AdvanceResult Advance(std::int64_t minutes);
    [[nodiscard]] AdvanceResult AdvanceTo(GameInstant instant);

    /** Schedules an event. An instant already past is rejected, not silently fired. */
    [[nodiscard]] std::int64_t Schedule(GameInstant when, std::string id);
    bool Cancel(std::int64_t handle);
    [[nodiscard]] const std::vector<ScheduledEvent>& Pending() const noexcept { return Pending_; }
    [[nodiscard]] GameInstant NextEventInstant() const noexcept;

    [[nodiscard]] GameClockSave CaptureSave() const;
    [[nodiscard]] bool RestoreSave(const GameClockSave& save);

private:
    void SortPending();

    GameInstant Now_ = 0;
    std::int64_t NextHandle_ = 1;
    std::vector<ScheduledEvent> Pending_;
};
} // namespace PerfectWorld
