#include "GameClock.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
/**
 * Days from 1970-01-01 for a civil date, and its inverse. Howard Hinnant's
 * algorithms, which are exact over the whole range we care about and handle
 * leap years without a table. Using a well-known algorithm rather than an
 * invented one matters here: a calendar bug would corrupt every schedule in
 * the game and would be very hard to notice.
 */
constexpr std::int64_t DaysFromCivil(std::int64_t year, std::int64_t month, std::int64_t day) noexcept
{
    year -= month <= 2;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yearOfEra = year - era * 400;                                   // [0, 399]
    const std::int64_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

struct CivilDate
{
    std::int64_t Year;
    std::int64_t Month;
    std::int64_t Day;
};

constexpr CivilDate CivilFromDays(std::int64_t days) noexcept
{
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const std::int64_t dayOfEra = days - era * 146097;                                  // [0, 146096]
    const std::int64_t yearOfEra =
        (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;      // [0, 399]
    const std::int64_t year = yearOfEra + era * 400;
    const std::int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
    const std::int64_t monthPrime = (5 * dayOfYear + 2) / 153;                          // [0, 11]
    const std::int64_t day = dayOfYear - (153 * monthPrime + 2) / 5 + 1;
    const std::int64_t month = monthPrime + (monthPrime < 10 ? 3 : -9);
    return {year + (month <= 2), month, day};
}

/** Minutes from 1970-01-01 00:00 to the campaign epoch. */
constexpr std::int64_t EpochOffsetMinutes()
{
    return DaysFromCivil(CampaignStartYear, CampaignStartMonth, CampaignStartDay) * MinutesPerDay +
           CampaignStartHour * MinutesPerHour;
}

constexpr std::int32_t DaysInMonth(std::int32_t year, std::int32_t month) noexcept
{
    constexpr std::int32_t Lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
    {
        return 0;
    }
    if (month == 2)
    {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return leap ? 29 : 28;
    }
    return Lengths[month - 1];
}
} // namespace

std::string_view WeekdayName(Weekday day) noexcept
{
    switch (day)
    {
    case Weekday::Sunday: return "Sunday";
    case Weekday::Monday: return "Monday";
    case Weekday::Tuesday: return "Tuesday";
    case Weekday::Wednesday: return "Wednesday";
    case Weekday::Thursday: return "Thursday";
    case Weekday::Friday: return "Friday";
    case Weekday::Saturday: return "Saturday";
    }
    return "Unknown";
}

bool IsValidCivilDate(std::int32_t year, std::int32_t month, std::int32_t day) noexcept
{
    if (month < 1 || month > 12 || day < 1)
    {
        return false;
    }
    return day <= DaysInMonth(year, month);
}

GameClock::GameClock() = default;

GameInstant GameClock::FromCalendar(std::int32_t year, std::int32_t month, std::int32_t day,
                                     std::int32_t hour, std::int32_t minute) noexcept
{
    const std::int64_t days = DaysFromCivil(year, month, day);
    return days * MinutesPerDay + hour * MinutesPerHour + minute - EpochOffsetMinutes();
}

CalendarTime GameClock::ToCalendar(GameInstant instant) noexcept
{
    const std::int64_t absolute = instant + EpochOffsetMinutes();
    // Floor division, so instants before the epoch still land on the right day
    // rather than truncating toward zero.
    std::int64_t days = absolute / MinutesPerDay;
    std::int64_t minuteOfDay = absolute % MinutesPerDay;
    if (minuteOfDay < 0)
    {
        minuteOfDay += MinutesPerDay;
        --days;
    }

    const CivilDate date = CivilFromDays(days);
    CalendarTime out;
    out.Year = static_cast<std::int32_t>(date.Year);
    out.Month = static_cast<std::int32_t>(date.Month);
    out.Day = static_cast<std::int32_t>(date.Day);
    out.Hour = static_cast<std::int32_t>(minuteOfDay / MinutesPerHour);
    out.Minute = static_cast<std::int32_t>(minuteOfDay % MinutesPerHour);
    // 1970-01-01 was a Thursday, which is 4 with Sunday as 0.
    std::int64_t weekday = (days + 4) % 7;
    if (weekday < 0)
    {
        weekday += 7;
    }
    out.DayOfWeek = static_cast<Weekday>(weekday);
    return out;
}

std::int64_t GameClock::DayNumber(GameInstant instant) noexcept
{
    const std::int64_t absolute = instant + EpochOffsetMinutes();
    std::int64_t days = absolute / MinutesPerDay;
    if (absolute % MinutesPerDay < 0)
    {
        --days;
    }
    return days;
}

GameInstant GameClock::StartOfDay(GameInstant instant) noexcept
{
    return DayNumber(instant) * MinutesPerDay - EpochOffsetMinutes();
}

AdvanceResult GameClock::Advance(std::int64_t minutes)
{
    AdvanceResult result;
    result.From = Now_;
    result.To = Now_;

    if (minutes < 0)
    {
        result.Rejected = true;
        result.RejectionReason = "Time cannot run backwards.";
        return result;
    }
    if (minutes > MaxSingleAdvanceMinutes)
    {
        result.Rejected = true;
        result.RejectionReason = "That advance is longer than ten years, which is a caller mistake.";
        return result;
    }
    if (minutes == 0)
    {
        return result;
    }

    const GameInstant target = Now_ + minutes;
    const std::int64_t dayBefore = DayNumber(Now_);
    const std::int64_t dayAfter = DayNumber(target);

    // Pending_ is kept sorted, so everything due is at the front and comes out
    // in chronological order. Events exactly on the new instant fire now; the
    // interval is half-open at the start so nothing fires twice.
    auto firstUnfired = Pending_.begin();
    while (firstUnfired != Pending_.end() && firstUnfired->When <= target)
    {
        result.Fired.push_back(*firstUnfired);
        ++firstUnfired;
    }
    Pending_.erase(Pending_.begin(), firstUnfired);

    Now_ = target;
    result.To = target;
    result.DaysCrossed = static_cast<std::int32_t>(dayAfter - dayBefore);
    return result;
}

AdvanceResult GameClock::AdvanceTo(GameInstant instant)
{
    if (instant < Now_)
    {
        AdvanceResult result;
        result.From = Now_;
        result.To = Now_;
        result.Rejected = true;
        result.RejectionReason = "Time cannot run backwards.";
        return result;
    }
    return Advance(instant - Now_);
}

std::int64_t GameClock::Schedule(GameInstant when, std::string id)
{
    if (when < Now_)
    {
        // Refusing rather than firing immediately: a caller scheduling into the
        // past has a bug, and quietly firing would hide it.
        return 0;
    }
    ScheduledEvent event;
    event.Handle = NextHandle_++;
    event.When = when;
    event.Id = std::move(id);
    Pending_.push_back(std::move(event));
    SortPending();
    return Pending_.empty() ? 0 : NextHandle_ - 1;
}

bool GameClock::Cancel(std::int64_t handle)
{
    const auto found = std::find_if(Pending_.begin(), Pending_.end(),
                                    [handle](const ScheduledEvent& e) { return e.Handle == handle; });
    if (found == Pending_.end())
    {
        return false;
    }
    Pending_.erase(found);
    return true;
}

GameInstant GameClock::NextEventInstant() const noexcept
{
    return Pending_.empty() ? -1 : Pending_.front().When;
}

void GameClock::SortPending()
{
    // Stable, and ties break on handle, so two events at the same instant
    // always fire in the order they were scheduled. Determinism here is what
    // lets a save reload produce the same history.
    std::stable_sort(Pending_.begin(), Pending_.end(),
                     [](const ScheduledEvent& a, const ScheduledEvent& b)
                     {
                         if (a.When != b.When)
                         {
                             return a.When < b.When;
                         }
                         return a.Handle < b.Handle;
                     });
}

GameClockSave GameClock::CaptureSave() const
{
    GameClockSave save;
    save.Now = Now_;
    save.NextHandle = NextHandle_;
    save.Pending = Pending_;
    return save;
}

bool GameClock::RestoreSave(const GameClockSave& save)
{
    if (save.NextHandle < 1)
    {
        return false;
    }
    for (const ScheduledEvent& event : save.Pending)
    {
        // A pending event before the clock, a handle that could collide with a
        // future one, or a duplicate handle all mean the record is inconsistent.
        if (event.When < save.Now || event.Handle < 1 || event.Handle >= save.NextHandle)
        {
            return false;
        }
    }
    for (std::size_t i = 0; i < save.Pending.size(); ++i)
    {
        for (std::size_t j = i + 1; j < save.Pending.size(); ++j)
        {
            if (save.Pending[i].Handle == save.Pending[j].Handle)
            {
                return false;
            }
        }
    }

    Now_ = save.Now;
    NextHandle_ = save.NextHandle;
    Pending_ = save.Pending;
    SortPending();
    return true;
}
} // namespace PerfectWorld
