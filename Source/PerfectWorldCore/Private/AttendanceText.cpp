#include "Attendance.h"

#include "AttendanceInternal.h"
#include "ScheduleInternal.h"

#include <algorithm>
#include <bit>

// Validation, hash and the strict text form of the attendance record
// (ROADMAP M2-07). The layout is documented in Attendance.h.
namespace PerfectWorld
{
namespace
{
namespace Text = ScheduleInternal;

constexpr std::string_view AttendanceHeader = "pw.attendance";
constexpr std::uint64_t AttendanceHashTag = 0x5057415454454e44ULL;
constexpr std::int64_t AttendanceMaxDayNumber = ScheduleMaxInstant / MinutesPerDay + 1;
constexpr std::int32_t AttendanceNoArrivalMinute = static_cast<std::int32_t>(MinutesPerDay);

struct AttendanceTally
{
    std::int64_t OnTime = 0;
    std::int64_t Late = 0;
    std::int64_t VeryLate = 0;
    std::int64_t Absent = 0;
    std::int64_t Held = 0;
    std::int64_t Attended = 0;
};

std::string AttendanceCheckEntry(const AttendanceRecord& record, const AttendanceDay& day, std::size_t index)
{
    const std::string label = "day entry " + std::to_string(index + 1);
    if (day.Day < record.FirstDay || day.Day >= record.NextDay)
        return label + " lies outside the record's range";
    if (!AttendanceIsSchoolDay(day.Day))
        return label + " is not a school day";
    if (day.PeriodsHeld < 1 || day.PeriodsHeld > AttendanceMaxPeriods)
        return label + " holds a number of periods outside 1.." + std::to_string(AttendanceMaxPeriods);
    if ((day.PeriodMask >> day.PeriodsHeld) != 0)
        return label + " attends a period that was not held";
    if ((day.Outcome == AttendanceOutcome::Absent) != (day.PeriodMask == 0))
        return label + " is absent exactly when no period is attended";
    if (day.Outcome == AttendanceOutcome::Absent ? day.ArrivalMinute != AttendanceNoArrivalMinute
                                                  : day.ArrivalMinute < 0 || day.ArrivalMinute >= AttendanceNoArrivalMinute)
        return label + " has an arrival minute that does not fit its outcome";
    return {};
}

void AttendanceCount(AttendanceTally& tally, const AttendanceDay& day)
{
    switch (day.Outcome)
    {
    case AttendanceOutcome::OnTime: ++tally.OnTime; break;
    case AttendanceOutcome::Late: ++tally.Late; break;
    case AttendanceOutcome::VeryLate: ++tally.VeryLate; break;
    case AttendanceOutcome::Absent: ++tally.Absent; break;
    }
    tally.Held += day.PeriodsHeld;
    tally.Attended += std::popcount(day.PeriodMask);
}

void AttendanceAppend(std::string& out, std::int64_t value)
{
    out += ' ';
    out += std::to_string(value);
}

std::optional<std::string_view> AttendanceKeyed(const std::vector<std::string_view>& lines, std::size_t index, std::string_view word,
                                                std::size_t fields)
{
    // Returns the whole line when it has the word first and the right number of fields.
    if (index >= lines.size())
        return std::nullopt;
    const auto parts = Text::SplitFields(lines[index]);
    if (!parts || parts->size() != fields || (*parts)[0] != word)
        return std::nullopt;
    return lines[index];
}

/** Non-negative integers of one line after its first word, or nothing if any is not canonical. */
std::optional<std::vector<std::int64_t>> AttendanceNumbers(std::string_view line, std::size_t count)
{
    const auto parts = Text::SplitFields(line);
    if (!parts || parts->size() != count + 1)
        return std::nullopt;
    std::vector<std::int64_t> numbers;
    for (std::size_t i = 1; i < parts->size(); ++i)
    {
        const std::optional<std::int64_t> value = Text::ParseNonNegative((*parts)[i]);
        if (!value)
            return std::nullopt;
        numbers.push_back(*value);
    }
    return numbers;
}
} // namespace

std::string ValidateAttendanceRecord(const AttendanceRecord& record)
{
    if (record.FirstDay < 0 || record.NextDay < record.FirstDay || record.NextDay > AttendanceMaxDayNumber)
        return "the day range is invalid";
    const std::int64_t totals[] = {record.SchoolDays,  record.OnTimeDays,  record.LateDays,       record.VeryLateDays,
                                   record.AbsentDays,  record.PeriodsHeld, record.PeriodsAttended};
    if (std::any_of(std::begin(totals), std::end(totals), [](std::int64_t value) { return value < 0; }))
        return "a total is negative";
    if (record.SchoolDays > record.NextDay - record.FirstDay)
        return "there are more school days than days in the range";
    if (record.SchoolDays != record.OnTimeDays + record.LateDays + record.VeryLateDays + record.AbsentDays)
        return "the school days are not the sum of the outcomes";
    if (record.PeriodsAttended > record.PeriodsHeld)
        return "more periods attended than held";
    if (record.PeriodsHeld > record.SchoolDays * AttendanceMaxPeriods)
        return "more periods held than the school days allow";
    const std::size_t expectedEntries = static_cast<std::size_t>(std::min<std::int64_t>(record.SchoolDays, AttendanceKeptDays));
    if (record.Days.size() != expectedEntries)
        return "the record keeps " + std::to_string(record.Days.size()) + " day entries, expected " + std::to_string(expectedEntries);

    AttendanceTally tally;
    for (std::size_t i = 0; i < record.Days.size(); ++i)
    {
        const AttendanceDay& day = record.Days[i];
        if (std::string problem = AttendanceCheckEntry(record, day, i); !problem.empty())
            return problem;
        if (i > 0 && day.Day <= record.Days[i - 1].Day)
            return "day entry " + std::to_string(i + 1) + " is not in strictly ascending order";
        AttendanceCount(tally, day);
    }
    const bool complete = record.Days.size() == static_cast<std::size_t>(record.SchoolDays);
    const bool matches = complete ? tally.OnTime == record.OnTimeDays && tally.Late == record.LateDays &&
                                        tally.VeryLate == record.VeryLateDays && tally.Absent == record.AbsentDays &&
                                        tally.Held == record.PeriodsHeld && tally.Attended == record.PeriodsAttended
                                  : tally.OnTime <= record.OnTimeDays && tally.Late <= record.LateDays &&
                                        tally.VeryLate <= record.VeryLateDays && tally.Absent <= record.AbsentDays &&
                                        tally.Held <= record.PeriodsHeld && tally.Attended <= record.PeriodsAttended;
    if (!matches)
        return complete ? "the totals differ from the day entries" : "the day entries exceed the totals";
    return {};
}

std::uint64_t HashAttendanceRecord(const AttendanceRecord& record)
{
    ScheduleInternal::HashWriter out;
    out.Unsigned(AttendanceHashTag);
    out.Signed(record.FirstDay);
    out.Signed(record.NextDay);
    out.Signed(record.SchoolDays);
    out.Signed(record.OnTimeDays);
    out.Signed(record.LateDays);
    out.Signed(record.VeryLateDays);
    out.Signed(record.AbsentDays);
    out.Signed(record.PeriodsHeld);
    out.Signed(record.PeriodsAttended);
    out.Unsigned(record.Days.size());
    for (const AttendanceDay& day : record.Days)
    {
        out.Signed(day.Day);
        out.Unsigned(static_cast<std::uint8_t>(day.Outcome));
        out.Signed(day.ArrivalMinute);
        out.Signed(day.PeriodsHeld);
        out.Unsigned(day.PeriodMask);
    }
    return out.Result();
}

std::string SerialiseAttendanceRecord(const AttendanceRecord& record)
{
    std::string out = std::string(AttendanceHeader) + ' ' + std::to_string(AttendanceFormatVersion) + '\n';
    out += "range";
    AttendanceAppend(out, record.FirstDay);
    AttendanceAppend(out, record.NextDay);
    out += "\ntotals";
    for (const std::int64_t value : {record.SchoolDays, record.OnTimeDays, record.LateDays, record.VeryLateDays, record.AbsentDays,
                                     record.PeriodsHeld, record.PeriodsAttended})
        AttendanceAppend(out, value);
    out += "\ndays " + std::to_string(record.Days.size()) + '\n';
    for (const AttendanceDay& day : record.Days)
    {
        out += "day";
        AttendanceAppend(out, day.Day);
        out += ' ';
        out += AttendanceInternal::OutcomeName(day.Outcome);
        AttendanceAppend(out, day.ArrivalMinute);
        AttendanceAppend(out, day.PeriodsHeld);
        AttendanceAppend(out, day.PeriodMask);
        out += '\n';
    }
    out += "end " + std::to_string(HashAttendanceRecord(record)) + '\n';
    return out;
}

std::optional<AttendanceRecord> ParseAttendanceRecord(std::string_view text)
{
    const auto lines = Text::SplitLines(text);
    if (!lines || lines->size() < 5)
        return std::nullopt;
    const auto header = Text::SplitFields((*lines)[0]);
    if (!header || header->size() != 2 || (*header)[0] != AttendanceHeader || (*header)[1] != std::to_string(AttendanceFormatVersion))
        return std::nullopt;

    AttendanceRecord record;
    const auto rangeLine = AttendanceKeyed(*lines, 1, "range", 3);
    const auto totalsLine = AttendanceKeyed(*lines, 2, "totals", 8);
    const auto daysLine = AttendanceKeyed(*lines, 3, "days", 2);
    if (!rangeLine || !totalsLine || !daysLine)
        return std::nullopt;
    const auto range = AttendanceNumbers(*rangeLine, 2);
    const auto totals = AttendanceNumbers(*totalsLine, 7);
    const auto count = AttendanceNumbers(*daysLine, 1);
    if (!range || !totals || !count || static_cast<std::uint64_t>((*count)[0]) > AttendanceKeptDays)
        return std::nullopt;
    record.FirstDay = (*range)[0];
    record.NextDay = (*range)[1];
    record.SchoolDays = (*totals)[0];
    record.OnTimeDays = (*totals)[1];
    record.LateDays = (*totals)[2];
    record.VeryLateDays = (*totals)[3];
    record.AbsentDays = (*totals)[4];
    record.PeriodsHeld = (*totals)[5];
    record.PeriodsAttended = (*totals)[6];

    const std::size_t days = static_cast<std::size_t>((*count)[0]);
    if (lines->size() != 4 + days + 1)
        return std::nullopt;
    for (std::size_t i = 0; i < days; ++i)
    {
        const std::string_view line = (*lines)[4 + i];
        const auto parts = Text::SplitFields(line);
        if (!parts || parts->size() != 6 || (*parts)[0] != "day")
            return std::nullopt;
        const std::optional<AttendanceOutcome> outcome = AttendanceInternal::OutcomeFromName((*parts)[2]);
        const std::optional<std::int64_t> day = Text::ParseNonNegative((*parts)[1]);
        const std::optional<std::int64_t> arrival = Text::ParseNonNegative((*parts)[3]);
        const std::optional<std::int64_t> held = Text::ParseNonNegative((*parts)[4]);
        const std::optional<std::int64_t> mask = Text::ParseNonNegative((*parts)[5]);
        if (!outcome || !day || !arrival || !held || !mask || *arrival > AttendanceNoArrivalMinute || *held > AttendanceMaxPeriods ||
            *mask > 0xFFFFFFFFLL)
            return std::nullopt;
        AttendanceDay entry;
        entry.Day = *day;
        entry.Outcome = *outcome;
        entry.ArrivalMinute = static_cast<std::int32_t>(*arrival);
        entry.PeriodsHeld = static_cast<std::int32_t>(*held);
        entry.PeriodMask = static_cast<std::uint32_t>(*mask);
        record.Days.push_back(entry);
    }
    if (!ValidateAttendanceRecord(record).empty())
        return std::nullopt;

    const auto endParts = Text::SplitFields((*lines)[4 + days]);
    if (!endParts || endParts->size() != 2 || (*endParts)[0] != "end")
        return std::nullopt;
    const std::optional<std::uint64_t> hash = Text::ParseUnsigned((*endParts)[1]);
    if (!hash || *hash != HashAttendanceRecord(record))
        return std::nullopt;
    return record;
}
} // namespace PerfectWorld
