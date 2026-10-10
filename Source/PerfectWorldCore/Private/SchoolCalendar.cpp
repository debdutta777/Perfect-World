#include "SchoolCalendar.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
template <typename Entry>
const Entry* CalFindSpan(const std::vector<Entry>& list, CalendarDay day)
{
    for (const Entry& entry : list)
    {
        if (day >= entry.First && day <= entry.Last)
            return &entry;
    }
    return nullptr;
}

void CalAppendDigits(std::string& out, std::int64_t value, int width)
{
    std::string digits;
    for (int i = 0; i < width; ++i)
    {
        digits.insert(digits.begin(), static_cast<char>('0' + value % 10));
        value /= 10;
    }
    out += digits;
}
} // namespace

const SchoolCalendar* SchoolCalendarSet::FindById(std::string_view id) const noexcept
{
    for (const SchoolCalendar& calendar : Calendars)
    {
        if (calendar.Id == id)
            return &calendar;
    }
    return nullptr;
}

const SchoolCalendar* SchoolCalendarSet::Find(std::string_view institutionId, std::string_view stageId) const noexcept
{
    for (const SchoolCalendar& calendar : Calendars)
    {
        if (calendar.InstitutionId == institutionId && calendar.StageId == stageId)
            return &calendar;
    }
    return nullptr;
}

// ---- Dates ---------------------------------------------------------------------

std::optional<CalendarDay> CalendarDayFromDate(std::int32_t year, std::int32_t month, std::int32_t day)
{
    if (!IsValidCivilDate(year, month, day))
        return std::nullopt;
    return GameClock::DayNumber(GameClock::FromCalendar(year, month, day, 0, 0)) - GameClock::DayNumber(0);
}

std::optional<CalendarDay> ParseCalendarDate(std::string_view text)
{
    if (text.size() != 10 || text[4] != '-' || text[7] != '-')
        return std::nullopt;
    std::int32_t parts[3] = {0, 0, 0};
    const std::size_t starts[3] = {0, 5, 8};
    const std::size_t widths[3] = {4, 2, 2};
    for (int part = 0; part < 3; ++part)
    {
        for (std::size_t i = 0; i < widths[part]; ++i)
        {
            const char c = text[starts[part] + i];
            if (c < '0' || c > '9')
                return std::nullopt;
            parts[part] = parts[part] * 10 + (c - '0');
        }
    }
    return CalendarDayFromDate(parts[0], parts[1], parts[2]);
}

std::string FormatCalendarDate(CalendarDay day)
{
    const CalendarTime time = GameClock::ToCalendar(day * MinutesPerDay);
    std::string out;
    CalAppendDigits(out, time.Year, 4);
    out += '-';
    CalAppendDigits(out, time.Month, 2);
    out += '-';
    CalAppendDigits(out, time.Day, 2);
    return out;
}

CalendarDay CalendarDayOfInstant(GameInstant instant)
{
    return GameClock::DayNumber(instant) - GameClock::DayNumber(0);
}

GameInstant CalendarDayStartInstant(CalendarDay day)
{
    return GameClock::StartOfDay(day * MinutesPerDay);
}

Weekday CalendarWeekdayOf(CalendarDay day)
{
    return GameClock::ToCalendar(day * MinutesPerDay).DayOfWeek;
}

// ---- Queries -------------------------------------------------------------------

std::string_view SchoolDayKindName(SchoolDayKind kind) noexcept
{
    switch (kind)
    {
    case SchoolDayKind::OutsideYear: return "outside_year";
    case SchoolDayKind::NonSchoolWeekday: return "non_school_weekday";
    case SchoolDayKind::Holiday: return "holiday";
    case SchoolDayKind::Break: return "break";
    case SchoolDayKind::OutsideTerm: return "outside_term";
    case SchoolDayKind::SchoolDay: return "school_day";
    }
    return "unknown";
}

bool CalendarWeekdayIsSchoolDay(const SchoolCalendar& calendar, Weekday weekday)
{
    const unsigned index = static_cast<unsigned>(weekday);
    return index < 7 && ((calendar.SchoolWeekdays >> index) & 1u) != 0;
}

const CalendarTerm* CalendarTermOn(const SchoolCalendar& calendar, CalendarDay day)
{
    return CalFindSpan(calendar.Terms, day);
}

const CalendarBreak* CalendarBreakOn(const SchoolCalendar& calendar, CalendarDay day)
{
    return CalFindSpan(calendar.Breaks, day);
}

const CalendarHoliday* CalendarHolidayOn(const SchoolCalendar& calendar, CalendarDay day)
{
    return CalFindSpan(calendar.Holidays, day);
}

const CalendarAssessment* CalendarAssessmentOn(const SchoolCalendar& calendar, CalendarDay day)
{
    return CalFindSpan(calendar.Assessments, day);
}

const CalendarEvent* CalendarEventOn(const SchoolCalendar& calendar, CalendarDay day)
{
    return CalFindSpan(calendar.Events, day);
}

SchoolDayKind ClassifyCalendarDay(const SchoolCalendar& calendar, CalendarDay day)
{
    if (day < calendar.FirstDay || day > calendar.LastDay)
        return SchoolDayKind::OutsideYear;
    if (!CalendarWeekdayIsSchoolDay(calendar, CalendarWeekdayOf(day)))
        return SchoolDayKind::NonSchoolWeekday;
    if (CalendarHolidayOn(calendar, day) != nullptr)
        return SchoolDayKind::Holiday;
    if (CalendarBreakOn(calendar, day) != nullptr)
        return SchoolDayKind::Break;
    if (CalendarTermOn(calendar, day) == nullptr)
        return SchoolDayKind::OutsideTerm;
    return SchoolDayKind::SchoolDay;
}

bool CalendarIsSchoolDay(const SchoolCalendar& calendar, CalendarDay day)
{
    return ClassifyCalendarDay(calendar, day) == SchoolDayKind::SchoolDay;
}

bool CalendarIsSchoolDayAt(const SchoolCalendar& calendar, GameInstant instant)
{
    return CalendarIsSchoolDay(calendar, CalendarDayOfInstant(instant));
}

std::optional<CalendarDay> CalendarNextSchoolDay(const SchoolCalendar& calendar, CalendarDay day)
{
    if (day >= calendar.LastDay)
        return std::nullopt;
    for (CalendarDay candidate = std::max(day + 1, calendar.FirstDay); candidate <= calendar.LastDay; ++candidate)
    {
        if (CalendarIsSchoolDay(calendar, candidate))
            return candidate;
    }
    return std::nullopt;
}

std::optional<CalendarDay> CalendarSchoolDayOnOrAfter(const SchoolCalendar& calendar, CalendarDay day)
{
    if (CalendarIsSchoolDay(calendar, day))
        return day;
    return CalendarNextSchoolDay(calendar, day);
}

std::optional<CalendarDay> CalendarPreviousSchoolDay(const SchoolCalendar& calendar, CalendarDay day)
{
    if (day <= calendar.FirstDay)
        return std::nullopt;
    for (CalendarDay candidate = std::min(day - 1, calendar.LastDay); candidate >= calendar.FirstDay; --candidate)
    {
        if (CalendarIsSchoolDay(calendar, candidate))
            return candidate;
    }
    return std::nullopt;
}

std::int64_t CalendarCountSchoolDays(const SchoolCalendar& calendar, CalendarDay first, CalendarDay last)
{
    std::int64_t count = 0;
    for (CalendarDay day = std::max(first, calendar.FirstDay); day <= std::min(last, calendar.LastDay); ++day)
        count += CalendarIsSchoolDay(calendar, day) ? 1 : 0;
    return count;
}

// ---- Content -------------------------------------------------------------------

SchoolCalendarResult LoadSchoolCalendars(const ContentDb& content)
{
    SchoolCalendarResult result;
    for (const ContentRecord* record : content.OfType("calendar"))
    {
        std::vector<std::string> problems;
        SchoolCalendar calendar = ParseSchoolCalendarRecord(*record, problems);
        if (problems.empty())
            problems = ValidateSchoolCalendar(calendar);
        if (problems.empty() && result.Set.Find(calendar.InstitutionId, calendar.StageId) != nullptr)
            problems.push_back("a second calendar for " + calendar.InstitutionId + " at " + calendar.StageId);
        if (!problems.empty())
        {
            result.Error = record->Id + ": " + problems.front();
            result.Set = {};
            return result;
        }
        result.Set.Calendars.push_back(std::move(calendar));
    }
    return result;
}
} // namespace PerfectWorld
