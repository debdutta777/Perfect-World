#include "Attendance.h"

#include "AttendanceInternal.h"
#include "IdentityRegistry.h"

#include <algorithm>
#include <array>
#include <set>

// Bell schedules: loading, validation, lookups and the M2-06 routine (ROADMAP M2-07).
// The rules are documented in Attendance.h.
namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, 3> AttendanceStreamNames{"science", "commerce", "humanities"};

// Subjects every stream shares, then each stream's own (WBCHSE-style; a production
// assumption pending the consultant check INDIA_WORLD_BIBLE.md asks for).
constexpr std::array<std::string_view, 4> AttendanceSharedSubjects{"bengali", "english", "environmental_education",
                                                                   "physical_education"};
constexpr std::array<std::string_view, 5> AttendanceScienceSubjects{"physics", "chemistry", "mathematics", "biology",
                                                                    "computer_science"};
constexpr std::array<std::string_view, 4> AttendanceCommerceSubjects{"accountancy", "business_studies", "economics",
                                                                     "mathematics"};
constexpr std::array<std::string_view, 5> AttendanceHumanitiesSubjects{"history", "geography", "political_science",
                                                                       "sociology", "philosophy"};

template <std::size_t N>
bool AttendanceListed(const std::array<std::string_view, N>& list, std::string_view value)
{
    return std::find(list.begin(), list.end(), value) != list.end();
}

std::string AttendanceBlockLabel(std::size_t index)
{
    return "block " + std::to_string(index + 1);
}

void AttendanceCheckBlock(const BellBlock& block, const std::string& stream, const std::string& label,
                          std::vector<std::string>& problems)
{
    const std::int32_t length = block.EndMinute - block.StartMinute;
    if (!IsStableIdToken(block.Room))
        problems.push_back(label + " needs a lowercase-token room");
    switch (block.Kind)
    {
    case BellBlockKind::Assembly:
        if (length < AttendanceMinAssemblyMinutes || length > AttendanceMaxAssemblyMinutes)
            problems.push_back(label + " is an assembly of " + std::to_string(length) + " minutes, outside " +
                               std::to_string(AttendanceMinAssemblyMinutes) + ".." + std::to_string(AttendanceMaxAssemblyMinutes));
        if (!block.Subject.empty())
            problems.push_back(label + " is an assembly and has a subject; only a period has one");
        break;
    case BellBlockKind::Period:
        if (length < AttendanceMinPeriodMinutes || length > AttendanceMaxPeriodMinutes)
            problems.push_back(label + " is a period of " + std::to_string(length) + " minutes, outside " +
                               std::to_string(AttendanceMinPeriodMinutes) + ".." + std::to_string(AttendanceMaxPeriodMinutes));
        if (block.Subject.empty())
            problems.push_back(label + " is a period and has no subject");
        else if (!AttendanceSubjectTaught(stream, block.Subject))
            problems.push_back(label + " names the subject " + block.Subject + ", which is not taught in the " + stream + " stream");
        break;
    case BellBlockKind::Break:
        if (!block.Subject.empty())
            problems.push_back(label + " is a break and has a subject; only a period has one");
        break;
    }
}

void AttendanceCheckOrder(const BellSchedule& schedule, std::vector<std::string>& problems)
{
    for (std::size_t i = 1; i < schedule.Blocks.size(); ++i)
    {
        const BellBlock& before = schedule.Blocks[i - 1];
        const BellBlock& block = schedule.Blocks[i];
        const std::string label = AttendanceBlockLabel(i);
        if (block.StartMinute < before.StartMinute)
        {
            problems.push_back(label + " starts before block " + std::to_string(i) + ": the blocks are out of order");
            continue;
        }
        if (block.StartMinute < before.EndMinute)
        {
            problems.push_back(label + " overlaps block " + std::to_string(i));
            continue;
        }
        const std::int32_t gap = block.StartMinute - before.EndMinute;
        if (gap > AttendanceMaxPassingMinutes)
            problems.push_back(label + " starts " + std::to_string(gap) + " minutes after block " + std::to_string(i) +
                               ", over the " + std::to_string(AttendanceMaxPassingMinutes) + "-minute passing limit");
        if (block.Room != before.Room && gap < AttendanceRoomWalkMinutes)
            problems.push_back(label + " is in another room than block " + std::to_string(i) + " but leaves " + std::to_string(gap) +
                               " minutes, under the " + std::to_string(AttendanceRoomWalkMinutes) + " needed to walk");
    }
}

void AttendanceCheckShape(const BellSchedule& schedule, std::vector<std::string>& problems)
{
    int assemblies = 0;
    int periods = 0;
    bool longBreak = false;
    bool english = false;
    bool bengali = false;
    for (const BellBlock& block : schedule.Blocks)
    {
        assemblies += block.Kind == BellBlockKind::Assembly ? 1 : 0;
        periods += block.Kind == BellBlockKind::Period ? 1 : 0;
        longBreak = longBreak || (block.Kind == BellBlockKind::Break && block.EndMinute - block.StartMinute >= AttendanceMinBreakMinutes);
        english = english || (block.Kind == BellBlockKind::Period && block.Subject == "english");
        bengali = bengali || (block.Kind == BellBlockKind::Period && block.Subject == "bengali");
    }
    if (schedule.Blocks.front().Kind != BellBlockKind::Assembly)
        problems.push_back("the day must open with the assembly");
    if (assemblies != 1)
        problems.push_back("the day needs exactly one assembly, found " + std::to_string(assemblies));
    if (schedule.Blocks.back().Kind != BellBlockKind::Period)
        problems.push_back("the day must end with a period");
    if (periods < AttendanceMinPeriods || periods > AttendanceMaxPeriods)
        problems.push_back("the day has " + std::to_string(periods) + " periods, outside " + std::to_string(AttendanceMinPeriods) + ".." +
                           std::to_string(AttendanceMaxPeriods) + ": not a full day");
    if (!longBreak)
        problems.push_back("the day needs a break of at least " + std::to_string(AttendanceMinBreakMinutes) + " minutes");
    if (!english)
        problems.push_back("the day has no english period");
    if (!bengali)
        problems.push_back("the day has no bengali period");
}

void AttendanceCheckHours(const BellSchedule& schedule, std::vector<std::string>& problems)
{
    const std::int32_t first = schedule.Blocks.front().StartMinute;
    const std::int32_t last = schedule.Blocks.back().EndMinute;
    if (first < AttendanceDayEarliestMinute)
        problems.push_back("the first bell at " + AttendanceInternal::ClockText(first) + " is before the school hours begin at " +
                           AttendanceInternal::ClockText(AttendanceDayEarliestMinute));
    if (last > AttendanceDayLatestMinute)
        problems.push_back("the last bell at " + AttendanceInternal::ClockText(last) + " is after the school hours end at " +
                           AttendanceInternal::ClockText(AttendanceDayLatestMinute));
    const std::int32_t span = last - first;
    if (span < AttendanceMinDaySpanMinutes || span > AttendanceMaxDaySpanMinutes)
        problems.push_back("the day lasts " + std::to_string(span) + " minutes, outside " + std::to_string(AttendanceMinDaySpanMinutes) + ".." +
                           std::to_string(AttendanceMaxDaySpanMinutes));
    if (schedule.GatesOpenMinute < AttendanceDayEarliestMinute || schedule.GatesOpenMinute >= first)
        problems.push_back("the gate opens at " + AttendanceInternal::ClockText(schedule.GatesOpenMinute) +
                           ", which must be inside the school hours and before the first bell");
}

void AttendanceCheckBells(const BellSchedule& schedule, std::vector<std::string>& problems)
{
    bool ascending = true;
    for (std::size_t i = 1; i < schedule.Bells.size(); ++i)
    {
        if (schedule.Bells[i] <= schedule.Bells[i - 1])
        {
            problems.push_back("the bells are not in ascending order: " + AttendanceInternal::ClockText(schedule.Bells[i]) + " follows " +
                               AttendanceInternal::ClockText(schedule.Bells[i - 1]));
            ascending = false;
            break;
        }
    }
    if (!ascending)
        return;
    std::set<std::int32_t> boundaries;
    for (const BellBlock& block : schedule.Blocks)
    {
        boundaries.insert(block.StartMinute);
        boundaries.insert(block.EndMinute);
    }
    const std::set<std::int32_t> bells(schedule.Bells.begin(), schedule.Bells.end());
    for (const std::int32_t bell : bells)
    {
        if (boundaries.count(bell) == 0)
            problems.push_back("a bell rings at " + AttendanceInternal::ClockText(bell) + " where no block starts or ends");
    }
    for (const std::int32_t boundary : boundaries)
    {
        if (bells.count(boundary) == 0)
            problems.push_back("no bell rings at " + AttendanceInternal::ClockText(boundary) + " where a block starts or ends");
    }
}
} // namespace

namespace AttendanceInternal
{
std::string ClockText(std::int32_t minuteOfDay)
{
    const std::int32_t hour = minuteOfDay / 60;
    const std::int32_t minute = minuteOfDay % 60;
    std::string out = hour < 10 ? "0" : "";
    out += std::to_string(hour);
    out += minute < 10 ? ":0" : ":";
    out += std::to_string(minute);
    return out;
}

unsigned WeekdayOfDay(std::int64_t day)
{
    std::int64_t weekday = (day + 1) % 7;
    if (weekday < 0)
        weekday += 7;
    return static_cast<unsigned>(weekday);
}

BellSchedule ParseBellScheduleRecord(const ContentRecord& record)
{
    const auto text = [](const JsonValue& object, std::string_view name) {
        const JsonValue* value = object.Find(name);
        return value != nullptr && value->IsString() ? value->String : std::string();
    };
    const auto number = [](const JsonValue& object, std::string_view name) {
        const JsonValue* value = object.Find(name);
        return value != nullptr && value->Integer ? static_cast<std::int32_t>(*value->Integer) : 0;
    };
    BellSchedule schedule;
    schedule.Id = record.Id;
    schedule.InstitutionId = text(record.Data, "institution");
    schedule.StageId = text(record.Data, "stage");
    schedule.Stream = text(record.Data, "stream");
    schedule.GatesOpenMinute = number(record.Data, "gatesOpen");
    if (const JsonValue* bells = record.Data.Find("bells"); bells != nullptr && bells->IsArray())
    {
        for (const JsonValue& bell : bells->Array)
            schedule.Bells.push_back(bell.Integer ? static_cast<std::int32_t>(*bell.Integer) : 0);
    }
    if (const JsonValue* blocks = record.Data.Find("blocks"); blocks != nullptr && blocks->IsArray())
    {
        for (const JsonValue& entry : blocks->Array)
        {
            BellBlock block;
            const std::string kind = text(entry, "kind");
            block.Kind = kind == "assembly" ? BellBlockKind::Assembly : kind == "break" ? BellBlockKind::Break : BellBlockKind::Period;
            block.StartMinute = number(entry, "start");
            block.EndMinute = number(entry, "end");
            block.Room = text(entry, "room");
            block.Subject = text(entry, "subject");
            schedule.Blocks.push_back(std::move(block));
        }
    }
    return schedule;
}
} // namespace AttendanceInternal

bool AttendanceIsStream(std::string_view stream)
{
    return AttendanceListed(AttendanceStreamNames, stream);
}

bool AttendanceSubjectTaught(std::string_view stream, std::string_view subject)
{
    if (!AttendanceIsStream(stream))
        return false;
    if (AttendanceListed(AttendanceSharedSubjects, subject))
        return true;
    if (stream == "science")
        return AttendanceListed(AttendanceScienceSubjects, subject);
    if (stream == "commerce")
        return AttendanceListed(AttendanceCommerceSubjects, subject);
    if (stream == "humanities")
        return AttendanceListed(AttendanceHumanitiesSubjects, subject);
    return false;
}

std::vector<std::string> ValidateBellSchedule(const BellSchedule& schedule)
{
    std::vector<std::string> problems;
    if (!AttendanceIsStream(schedule.Stream))
        problems.push_back("the stream \"" + schedule.Stream + "\" is not science, commerce or humanities");
    if (schedule.Blocks.empty())
    {
        problems.push_back("the day has no blocks");
        return problems;
    }
    bool wellFormed = true;
    for (std::size_t i = 0; i < schedule.Blocks.size(); ++i)
    {
        const BellBlock& block = schedule.Blocks[i];
        const std::string label = AttendanceBlockLabel(i);
        if (block.StartMinute < 0 || block.EndMinute > MinutesPerDay || block.StartMinute >= block.EndMinute)
        {
            problems.push_back(label + " needs 0 <= start < end <= 1440");
            wellFormed = false;
            continue;
        }
        AttendanceCheckBlock(block, schedule.Stream, label, problems);
    }
    if (!wellFormed)
        return problems;
    AttendanceCheckOrder(schedule, problems);
    AttendanceCheckShape(schedule, problems);
    AttendanceCheckHours(schedule, problems);
    AttendanceCheckBells(schedule, problems);
    return problems;
}

const BellSchedule* BellScheduleSet::FindById(std::string_view id) const noexcept
{
    for (const BellSchedule& schedule : Schedules)
    {
        if (schedule.Id == id)
            return &schedule;
    }
    return nullptr;
}

const BellSchedule* BellScheduleSet::Find(std::string_view institutionId, std::string_view stream) const noexcept
{
    for (const BellSchedule& schedule : Schedules)
    {
        if (schedule.InstitutionId == institutionId && schedule.Stream == stream)
            return &schedule;
    }
    return nullptr;
}

BellScheduleResult LoadBellSchedules(const ContentDb& content)
{
    BellScheduleResult result;
    // OfType walks an ordered map, so the schedules come out sorted by ID.
    for (const ContentRecord* record : content.OfType("bell_schedule"))
    {
        BellSchedule schedule = AttendanceInternal::ParseBellScheduleRecord(*record);
        const std::vector<std::string> problems = ValidateBellSchedule(schedule);
        if (!problems.empty())
        {
            result.Error = record->Id + ": " + problems.front();
            result.Set = {};
            return result;
        }
        result.Set.Schedules.push_back(std::move(schedule));
    }
    return result;
}

std::int32_t BellPeriodCount(const BellSchedule& schedule)
{
    return static_cast<std::int32_t>(std::count_if(schedule.Blocks.begin(), schedule.Blocks.end(),
                                                   [](const BellBlock& b) { return b.Kind == BellBlockKind::Period; }));
}

std::int32_t BellFirstBell(const BellSchedule& schedule)
{
    return schedule.Blocks.empty() ? 0 : schedule.Blocks.front().StartMinute;
}

const BellBlock* BellBlockAt(const BellSchedule& schedule, std::int32_t minuteOfDay)
{
    for (const BellBlock& block : schedule.Blocks)
    {
        if (minuteOfDay >= block.StartMinute && minuteOfDay < block.EndMinute)
            return &block;
    }
    return nullptr;
}

std::int32_t BellPeriodIndex(const BellSchedule& schedule, const BellBlock& block)
{
    std::int32_t index = 0;
    for (const BellBlock& candidate : schedule.Blocks)
    {
        if (candidate.Kind != BellBlockKind::Period)
            continue;
        if (&candidate == &block)
            return index;
        ++index;
    }
    return -1;
}

std::vector<ScheduleEntry> BuildSchoolRoutine(const BellSchedule& schedule, const AttendanceRoutineOptions& options)
{
    std::vector<ScheduleEntry> routine;
    for (const BellBlock& block : schedule.Blocks)
    {
        ScheduleEntry entry;
        entry.WeekdayMask = options.Weekdays;
        entry.StartMinute = block.StartMinute;
        entry.EndMinute = block.EndMinute;
        entry.Zone = block.Room;
        entry.Priority = options.Priority;
        if (block.Kind == BellBlockKind::Period)
        {
            entry.Kind = ScheduleDoingKind::Activity;
            entry.Doing = std::string(AttendanceClassActivityId);
        }
        else
        {
            entry.Kind = ScheduleDoingKind::Role;
            entry.Doing = std::string(block.Kind == BellBlockKind::Assembly ? AttendanceAssemblyRole : AttendanceBreakRole);
        }
        routine.push_back(std::move(entry));
    }
    return routine;
}

PersonSchedule BuildStudentSchedule(std::string_view personId, std::string_view homeZone, const BellSchedule& schedule,
                                    const AttendanceRoutineOptions& options)
{
    PersonSchedule person;
    person.PersonId = std::string(personId);
    person.HomeZone = std::string(homeZone);
    person.Routine = BuildSchoolRoutine(schedule, options);
    return person;
}

ScheduleTravelTable AttendanceSchoolTravel(const BellScheduleSet& set)
{
    std::set<std::string> rooms;
    for (const BellSchedule& schedule : set.Schedules)
    {
        for (const BellBlock& block : schedule.Blocks)
            rooms.insert(block.Room);
    }
    ScheduleTravelTable table;
    // Ascending outer and inner loops give strictly ascending (from, to) order.
    for (auto from = rooms.begin(); from != rooms.end(); ++from)
    {
        for (auto to = std::next(from); to != rooms.end(); ++to)
            table.Legs.push_back({*from, *to, AttendanceRoomWalkMinutes});
    }
    return table;
}

bool AttendanceAddTravelLeg(ScheduleTravelTable& table, std::string_view from, std::string_view to, std::int32_t minutes)
{
    if (!IsStableIdToken(from) || !IsStableIdToken(to) || from == to || minutes < 1 || minutes > ScheduleMaxTravelMinutes)
        return false;
    if (table.Legs.size() >= ScheduleMaxTravelLegs)
        return false;
    ScheduleTravelLeg leg{std::string(from), std::string(to), minutes};
    const auto position = std::lower_bound(table.Legs.begin(), table.Legs.end(), leg, [](const ScheduleTravelLeg& a, const ScheduleTravelLeg& b) {
        return a.From != b.From ? a.From < b.From : a.To < b.To;
    });
    if (position != table.Legs.end() && position->From == leg.From && position->To == leg.To)
        position->Minutes = minutes;
    else
        table.Legs.insert(position, std::move(leg));
    return true;
}
} // namespace PerfectWorld
