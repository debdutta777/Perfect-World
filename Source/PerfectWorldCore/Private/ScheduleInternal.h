#pragma once

// Private helpers shared by Schedules.cpp, SchedulePlan.cpp and ScheduleText.cpp:
// calendar arithmetic, the resolved-day plan, the FNV-1a hash writer and the
// strict line format. Not part of the public API.

#include "Schedules.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld::ScheduleInternal
{
/** Minutes from midnight to the epoch's 06:00: instant 0 is 06:00 on a Monday. */
constexpr GameInstant LocalOffset = CampaignStartHour * MinutesPerHour;

constexpr std::int64_t FloorDiv(std::int64_t value, std::int64_t divisor)
{
    std::int64_t quotient = value / divisor;
    if (value % divisor < 0)
        --quotient;
    return quotient;
}

/** Days since the epoch's calendar day (day 0 is the Monday of the campaign start), floor for negatives. */
constexpr std::int64_t RelativeDay(GameInstant at) { return FloorDiv(at + LocalOffset, MinutesPerDay); }
/** The instant midnight opens on a relative day. */
constexpr GameInstant DayStart(std::int64_t day) { return day * MinutesPerDay - LocalOffset; }
/** 0 = Sunday .. 6 = Saturday, matching Weekday; day 0 is a Monday. */
constexpr unsigned WeekdayOfDay(std::int64_t day)
{
    std::int64_t weekday = (day + 1) % 7;
    if (weekday < 0)
        weekday += 7;
    return static_cast<unsigned>(weekday);
}

/** One piece of a resolved day: an unbroken stretch of staying, travelling or idling. */
struct Segment
{
    ScheduleStatus Status = ScheduleStatus::Idle;
    GameInstant Start = 0;
    GameInstant End = 0;
    std::string Zone;
    std::string FromZone;
    ScheduleDoingKind Kind = ScheduleDoingKind::Role;
    std::string Doing;
    std::string SourceId;
    bool Squeezed = false;
    std::int64_t Nominal = 0;
};

/** The segments covering a window of ScheduleHorizonDays either side of one day, contiguous and in order. */
struct Plan
{
    std::int64_t Day = 0;
    std::vector<Segment> Segments;
};

/** A change of resolved identity at instant At, between segments of one or two adjacent plans. */
struct Boundary
{
    GameInstant At = 0;
    const Segment* Before = nullptr;
    const Segment* After = nullptr;
};

Plan BuildPlan(const PersonSchedule& person, const ScheduleTravelTable& travel, std::int64_t day);
/** The segment containing `at`, or nullptr when the plan does not reach it. */
const Segment* FindSegment(const Plan& plan, GameInstant at);
/** Same status, place, activity or role, source and squeezed flag (times and progress ignored). */
bool SameIdentity(const Segment& a, const Segment& b);
/** The identity changes inside one day, ascending. `previous` (the day before's plan, or null) enables the midnight boundary. */
std::vector<Boundary> DayBoundaries(const Plan& plan, const Plan* previous);
ScheduleResolution ToResolution(const Segment& segment, GameInstant at);

/** FNV-1a 64 over tagged fields; every integer little-endian, byte by byte (same scheme as WorldHash). */
class HashWriter
{
public:
    void Byte(std::uint8_t value)
    {
        Hash_ ^= value;
        Hash_ *= 0x100000001b3ULL;
    }
    void Unsigned(std::uint64_t value)
    {
        Byte('u');
        Raw64(value);
    }
    void Signed(std::int64_t value)
    {
        Byte('i');
        Raw64(static_cast<std::uint64_t>(value));
    }
    void Text(std::string_view value)
    {
        Byte('s');
        Raw64(static_cast<std::uint64_t>(value.size()));
        for (const char c : value)
            Byte(static_cast<std::uint8_t>(c));
    }
    [[nodiscard]] std::uint64_t Result() const { return Hash_; }

private:
    void Raw64(std::uint64_t value)
    {
        for (int shift = 0; shift < 64; shift += 8)
            Byte(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
    std::uint64_t Hash_ = 0xcbf29ce484222325ULL;
};

/** Canonical decimal only: digits, no sign, no leading zero (except "0"), no overflow. */
inline std::optional<std::uint64_t> ParseUnsigned(std::string_view text)
{
    if (text.empty() || text.size() > 20 || (text.size() > 1 && text[0] == '0'))
        return std::nullopt;
    std::uint64_t value = 0;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            return std::nullopt;
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (value > (UINT64_MAX - digit) / 10)
            return std::nullopt;
        value = value * 10 + digit;
    }
    return value;
}

/** Canonical decimal in 0..INT64_MAX. Every schedule integer is non-negative. */
inline std::optional<std::int64_t> ParseNonNegative(std::string_view text)
{
    const std::optional<std::uint64_t> value = ParseUnsigned(text);
    if (!value || *value > static_cast<std::uint64_t>(INT64_MAX))
        return std::nullopt;
    return static_cast<std::int64_t>(*value);
}

/** Splits text into lines. The text must end in a newline and may not contain a carriage return. */
inline std::optional<std::vector<std::string_view>> SplitLines(std::string_view text)
{
    if (text.empty() || text.back() != '\n' || text.find('\r') != std::string_view::npos)
        return std::nullopt;
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

/** Splits on single spaces; an empty field (double, leading or trailing space) refuses the line. */
inline std::optional<std::vector<std::string_view>> SplitFields(std::string_view line)
{
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t end = line.find(' ', start);
        const std::string_view field = line.substr(start, end == std::string_view::npos ? end : end - start);
        if (field.empty())
            return std::nullopt;
        fields.push_back(field);
        if (end == std::string_view::npos)
            return fields;
        start = end + 1;
    }
}
} // namespace PerfectWorld::ScheduleInternal
