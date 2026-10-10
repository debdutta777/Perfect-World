#include "SchoolCalendar.h"

#include <algorithm>
#include <set>

// Reading the calendar record and the rules a calendar must meet (ROADMAP M3-09). The
// queries and the date helpers are in SchoolCalendar.cpp.
namespace PerfectWorld
{
namespace
{
using CalProblems = std::vector<std::string>;

// ---- Reading ---------------------------------------------------------------------

std::string CalReadText(const JsonValue& object, std::string_view name)
{
    const JsonValue* value = object.Find(name);
    return value != nullptr && value->IsString() ? value->String : std::string();
}

bool CalLooksLikeDate(const std::string& text)
{
    if (text.size() != 10 || text[4] != '-' || text[7] != '-')
        return false;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (i != 4 && i != 7 && (text[i] < '0' || text[i] > '9'))
            return false;
    }
    return true;
}

/** A date field, or nothing and a problem naming `label` and the field. */
std::optional<CalendarDay> CalReadDate(const JsonValue& object, std::string_view field, const std::string& label,
                                       CalProblems& problems)
{
    const std::string text = CalReadText(object, field);
    if (const std::optional<CalendarDay> day = ParseCalendarDate(text))
        return day;
    const std::string where = label + " " + std::string(field) + " '" + text + "'";
    problems.push_back(CalLooksLikeDate(text) ? where + " is not a date that exists"
                                              : where + " is not written YYYY-MM-DD");
    return std::nullopt;
}

/** The ID, name, status and the two ends of one entry. */
CalendarSpan CalReadSpan(const JsonValue& entry, std::string_view startField, std::string_view endField, const std::string& what,
                         CalProblems& problems)
{
    CalendarSpan span;
    span.Id = CalReadText(entry, "id");
    span.Name = CalReadText(entry, "name");
    span.Unverified = CalReadText(entry, "status") != "reviewed";
    const std::string label = what + " " + span.Id;
    span.First = CalReadDate(entry, startField, label, problems).value_or(0);
    span.Last = startField == endField ? span.First : CalReadDate(entry, endField, label, problems).value_or(0);
    return span;
}

template <typename Entry, typename Fill>
void CalReadList(const JsonValue& root, std::string_view field, const std::string& what, std::string_view startField,
                 std::string_view endField, std::vector<Entry>& out, CalProblems& problems, Fill fill)
{
    const JsonValue* list = root.Find(field);
    if (list == nullptr || !list->IsArray())
        return;
    for (const JsonValue& item : list->Array)
    {
        Entry entry;
        static_cast<CalendarSpan&>(entry) = CalReadSpan(item, startField, endField, what, problems);
        fill(entry, item);
        out.push_back(std::move(entry));
    }
}

// ---- Rules -----------------------------------------------------------------------

constexpr CalendarDay CalMaxWindowDays = 366;

bool CalIdIsToken(const std::string& id)
{
    return !id.empty() && std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

bool CalOverlaps(const CalendarSpan& a, const CalendarSpan& b)
{
    return a.First <= b.Last && b.First <= a.Last;
}

bool CalEncloses(const CalendarSpan& outer, const CalendarSpan& inner)
{
    return outer.First <= inner.First && inner.Last <= outer.Last;
}

template <typename Entry>
const Entry* CalFindEnclosing(const std::vector<Entry>& list, const CalendarSpan& inner)
{
    for (const Entry& entry : list)
    {
        if (CalEncloses(entry, inner))
            return &entry;
    }
    return nullptr;
}

template <typename Entry>
const Entry* CalFindTouching(const std::vector<Entry>& list, const CalendarSpan& span)
{
    for (const Entry& entry : list)
    {
        if (CalOverlaps(entry, span))
            return &entry;
    }
    return nullptr;
}

template <typename Entry>
const Entry* CalFindId(const std::vector<Entry>& list, std::string_view id)
{
    for (const Entry& entry : list)
    {
        if (entry.Id == id)
            return &entry;
    }
    return nullptr;
}

std::string CalRange(const CalendarSpan& span)
{
    return FormatCalendarDate(span.First) + " to " + FormatCalendarDate(span.Last);
}

/** The rules every list shares: unique token IDs, ordered ends, inside the window, in date order, no overlap. */
template <typename Entry>
void CalRuleList(const SchoolCalendar& calendar, const std::string& what, const std::vector<Entry>& list, CalProblems& problems)
{
    std::set<std::string> ids;
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        const Entry& entry = list[i];
        const std::string label = what + " " + entry.Id;
        if (!CalIdIsToken(entry.Id))
            problems.push_back(what + " ID '" + entry.Id + "' must be lowercase letters, digits and underscores");
        else if (!ids.insert(entry.Id).second)
            problems.push_back("two " + what + "s are both called " + entry.Id);
        const bool ordered = entry.First <= entry.Last;
        if (!ordered)
            problems.push_back(label + " ends before it starts");
        else if (entry.First < calendar.FirstDay || entry.Last > calendar.LastDay)
            problems.push_back(label + " (" + CalRange(entry) + ") lies outside the calendar's window");
        if (i == 0 || !ordered || list[i - 1].Last < list[i - 1].First)
            continue;
        if (entry.First < list[i - 1].First)
            problems.push_back(label + " is listed after " + list[i - 1].Id + " but starts earlier: list them in date order");
        for (std::size_t j = 0; j < i; ++j)
        {
            if (list[j].Last >= list[j].First && CalOverlaps(entry, list[j]))
            {
                problems.push_back(label + " overlaps " + what + " " + list[j].Id);
                break;
            }
        }
    }
}

void CalRuleWindow(const SchoolCalendar& calendar, CalProblems& problems)
{
    const std::optional<CalendarDay> start = CalendarDayFromDate(CampaignStartYear, CampaignStartMonth, CampaignStartDay);
    if (calendar.FirstDay != start.value_or(0))
        problems.push_back("the calendar must start on the campaign start, " + FormatCalendarDate(start.value_or(0)) + ", not " +
                           FormatCalendarDate(calendar.FirstDay));
    if (calendar.LastDay < calendar.FirstDay)
        problems.push_back("the calendar ends before it starts");
    else if (calendar.LastDay - calendar.FirstDay >= CalMaxWindowDays)
        problems.push_back("the calendar's window is longer than a year");
    if (calendar.AwakeningDay < calendar.FirstDay || calendar.AwakeningDay > calendar.LastDay)
        problems.push_back("the Awakening Day " + FormatCalendarDate(calendar.AwakeningDay) + " is outside the calendar's window");
    if (calendar.AwakeningDay != CalendarDayFromDate(2043, 1, 23).value_or(-1))
        problems.push_back("the Awakening must be on 2043-01-23, the end of the Class XI year, not " +
                           FormatCalendarDate(calendar.AwakeningDay));
    if (calendar.ExaminationDay != calendar.AwakeningDay + CalendarAwakeningToExamDays)
        problems.push_back("the examination must be exactly " + std::to_string(CalendarAwakeningToExamDays) +
                           " days after the Awakening, not " + FormatCalendarDate(calendar.ExaminationDay));
    else if (calendar.ExaminationDay > calendar.LastDay)
        problems.push_back("the examination day " + FormatCalendarDate(calendar.ExaminationDay) + " is after the end of the calendar");
    if (calendar.SchoolWeekdays == 0)
        problems.push_back("the calendar declares no school weekdays");
    if (calendar.Terms.empty())
        problems.push_back("the calendar has no terms");
}

void CalRuleBreaksAndHolidays(const SchoolCalendar& calendar, CalProblems& problems)
{
    for (const CalendarBreak& item : calendar.Breaks)
    {
        if (item.Last < item.First)
            continue;
        const CalendarTerm* touching = CalFindTouching(calendar.Terms, item);
        if (touching != nullptr && CalFindEnclosing(calendar.Terms, item) == nullptr)
            problems.push_back("break " + item.Id + " straddles the edge of term " + touching->Id +
                               ": a break lies wholly inside a term or wholly between terms");
    }
    for (const CalendarHoliday& item : calendar.Holidays)
    {
        if (item.Last < item.First)
            continue;
        if (CalFindEnclosing(calendar.Terms, item) == nullptr && CalFindEnclosing(calendar.Breaks, item) == nullptr)
            problems.push_back("holiday " + item.Id + " (" + CalRange(item) + ") is not wholly inside a term or a break");
        if (item.Kind != "festival" && item.Approximate)
            problems.push_back("holiday " + item.Id + " is a fixed-date " + item.Kind + " holiday and cannot be approximate");
        if (item.Approximate && (!item.Unverified || item.Note.find("approximate, to be confirmed") == std::string::npos))
            problems.push_back("festival " + item.Id +
                               " has an approximate date: it must stay unverified and its note must say approximate, to be confirmed");
    }
}

void CalRuleCoverage(const SchoolCalendar& calendar, CalProblems& problems)
{
    for (CalendarDay day = calendar.FirstDay; day <= calendar.LastDay; ++day)
    {
        if (CalendarTermOn(calendar, day) == nullptr && CalendarBreakOn(calendar, day) == nullptr)
        {
            problems.push_back(FormatCalendarDate(day) + " is in neither a term nor a break");
            return;
        }
    }
}

void CalRuleAssessments(const SchoolCalendar& calendar, CalProblems& problems)
{
    for (const CalendarAssessment& item : calendar.Assessments)
    {
        const CalendarTerm* term = CalFindId(calendar.Terms, item.TermId);
        if (term == nullptr)
        {
            problems.push_back("assessment " + item.Id + " names the term '" + item.TermId + "', which the calendar does not have");
            continue;
        }
        if (item.Last < item.First)
            continue;
        if (!CalEncloses(*term, item))
            problems.push_back("assessment " + item.Id + " (" + CalRange(item) + ") is not inside its term " + term->Id);
        else if (CalendarCountSchoolDays(calendar, item.First, item.Last) == 0)
            problems.push_back("assessment " + item.Id + " has no school day in it");
    }
    for (const CalendarEvent& item : calendar.Events)
    {
        if (item.Last < item.First)
            continue;
        if (!CalendarIsSchoolDay(calendar, item.First))
            problems.push_back("event " + item.Id + " on " + FormatCalendarDate(item.First) + " is not on a school day");
    }
}

void CalRuleRequired(const SchoolCalendar& calendar, CalProblems& problems)
{
    const CalendarDay independence = CalendarDayFromDate(2042, 8, 15).value_or(-1);
    const CalendarHoliday* holiday = CalFindId(calendar.Holidays, CalendarIndependenceDayId);
    if (holiday == nullptr || independence < holiday->First || independence > holiday->Last)
        problems.push_back("the calendar must declare the holiday independence_day on 2042-08-15");
    for (const std::string_view id : {CalendarDurgaPujaId, CalendarKaliPujaDiwaliId, CalendarSaraswatiPujaId})
    {
        if (CalFindId(calendar.Holidays, id) == nullptr)
            problems.push_back("the calendar must declare the holiday " + std::string(id));
    }
    if (CalFindId(calendar.Breaks, CalendarWinterBreakId) == nullptr)
        problems.push_back("the calendar must declare the break " + std::string(CalendarWinterBreakId));
    if (CalFindId(calendar.Events, CalendarSportsDayId) == nullptr)
        problems.push_back("the calendar must declare the event " + std::string(CalendarSportsDayId));
}

void CalRuleTenDays(const SchoolCalendar& calendar, CalProblems& problems)
{
    if (!CalendarIsSchoolDay(calendar, calendar.AwakeningDay))
        problems.push_back("the Awakening Day " + FormatCalendarDate(calendar.AwakeningDay) +
                           " must be a school day: the tutorial dungeon is held at the school");
    for (CalendarDay day = calendar.AwakeningDay + 1; day <= std::min(calendar.ExaminationDay, calendar.LastDay); ++day)
    {
        if (CalendarIsSchoolDay(calendar, day))
        {
            problems.push_back("the ten days after the Awakening have no school, but " + FormatCalendarDate(day) + " is a school day");
            return;
        }
    }
}
} // namespace

SchoolCalendar ParseSchoolCalendarRecord(const ContentRecord& record, std::vector<std::string>& problems)
{
    const JsonValue& data = record.Data;
    SchoolCalendar calendar;
    calendar.Id = record.Id;
    calendar.InstitutionId = CalReadText(data, "institution");
    calendar.StageId = CalReadText(data, "stage");
    calendar.State = CalReadText(data, "state");
    const std::string label = "the calendar";
    calendar.FirstDay = CalReadDate(data, "yearStart", label, problems).value_or(0);
    calendar.LastDay = CalReadDate(data, "yearEnd", label, problems).value_or(0);
    calendar.AwakeningDay = CalReadDate(data, "awakeningDate", label, problems).value_or(0);
    calendar.ExaminationDay = CalReadDate(data, "examinationDate", label, problems).value_or(0);
    if (const JsonValue* weekdays = data.Find("schoolWeekdays"); weekdays != nullptr && weekdays->IsArray())
    {
        for (const JsonValue& item : weekdays->Array)
        {
            if (!item.Integer || *item.Integer < 0 || *item.Integer > 6)
                continue;
            const std::uint8_t bit = static_cast<std::uint8_t>(1u << *item.Integer);
            if ((calendar.SchoolWeekdays & bit) != 0)
                problems.push_back("school weekday " + std::to_string(*item.Integer) + " is listed twice");
            calendar.SchoolWeekdays = static_cast<std::uint8_t>(calendar.SchoolWeekdays | bit);
        }
    }
    const auto none = [](auto&, const JsonValue&) {};
    CalReadList(data, "terms", "term", "start", "end", calendar.Terms, problems, none);
    CalReadList(data, "breaks", "break", "start", "end", calendar.Breaks, problems, none);
    CalReadList(data, "holidays", "holiday", "start", "end", calendar.Holidays, problems,
                [](CalendarHoliday& holiday, const JsonValue& item) {
                    holiday.Kind = CalReadText(item, "kind");
                    holiday.Approximate = CalReadText(item, "dateBasis") == "lunisolar_approximate";
                    holiday.Note = CalReadText(item, "note");
                });
    CalReadList(data, "assessments", "assessment", "start", "end", calendar.Assessments, problems,
                [](CalendarAssessment& assessment, const JsonValue& item) { assessment.TermId = CalReadText(item, "term"); });
    CalReadList(data, "events", "event", "date", "date", calendar.Events, problems,
                [](CalendarEvent& event, const JsonValue& item) { event.Kind = CalReadText(item, "kind"); });
    return calendar;
}

std::vector<std::string> ValidateSchoolCalendar(const SchoolCalendar& calendar)
{
    CalProblems problems;
    CalRuleWindow(calendar, problems);
    CalRuleList(calendar, "term", calendar.Terms, problems);
    CalRuleList(calendar, "break", calendar.Breaks, problems);
    CalRuleList(calendar, "holiday", calendar.Holidays, problems);
    CalRuleList(calendar, "assessment", calendar.Assessments, problems);
    CalRuleList(calendar, "event", calendar.Events, problems);
    CalRuleBreaksAndHolidays(calendar, problems);
    const bool windowOk = calendar.FirstDay <= calendar.LastDay && calendar.LastDay - calendar.FirstDay < CalMaxWindowDays;
    if (windowOk)
        CalRuleCoverage(calendar, problems);
    CalRuleAssessments(calendar, problems);
    CalRuleRequired(calendar, problems);
    if (windowOk)
        CalRuleTenDays(calendar, problems);
    return problems;
}
} // namespace PerfectWorld
