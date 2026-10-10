// ROADMAP M3-09: the calendar content rules, each exercised by editing the real record and expecting the
// validator to refuse it for exactly that reason. Part of PerfectWorldSchoolCalendarTests.
#include "SchoolCalendar.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int* Count = nullptr;

void Expect(bool condition, const std::string& message)
{
    ++*Count;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr const char* CalendarPath = "calendar.nabapara_2042_43.json";

bool HasIssue(const ContentLoadResult& result, const std::string& text)
{
    return std::any_of(result.Issues.begin(), result.Issues.end(),
                       [&](const ContentIssue& issue) { return issue.Message.find(text) != std::string::npos; });
}

/** The real content with the first `from` in the calendar file replaced by `to`. */
std::vector<ContentFile> Mutated(const std::vector<ContentFile>& files, const std::string& from, const std::string& to)
{
    std::vector<ContentFile> copy = files;
    bool found = false;
    for (ContentFile& file : copy)
    {
        if (file.Path.find(CalendarPath) == std::string::npos)
            continue;
        const std::size_t at = file.Text.find(from);
        Expect(at != std::string::npos, "the mutation target exists: " + from);
        if (at != std::string::npos)
            file.Text.replace(at, from.size(), to);
        found = true;
    }
    Expect(found, "the calendar file exists");
    return copy;
}

struct Mutation
{
    const char* From;
    const char* To;
    const char* Issue;
};

// Every row edits one thing in Data/Content/calendar/calendar.nabapara_2042_43.json.
const Mutation Mutations[] = {
    // Dates exist and are well formed.
    {R"("start": "2042-08-15", "end": "2042-08-15")", R"("start": "2042-02-30", "end": "2042-08-15")", "is not a date that exists"},
    {R"("start": "2042-08-15", "end": "2042-08-15")", R"("start": "2042-8-15", "end": "2042-08-15")", "is not written YYYY-MM-DD"},
    {R"("yearEnd": "2043-02-27")", R"("yearEnd": "2043-02-31")", "the calendar yearEnd '2043-02-31' is not a date that exists"},
    {R"("date": "2042-12-05")", R"("date": "5 December 2042")", "event sports_day date '5 December 2042' is not written YYYY-MM-DD"},
    // Ranges are ordered, inside the window, in date order, and do not overlap.
    {R"("start": "2042-10-20", "end": "2042-10-24")", R"("start": "2042-10-26", "end": "2042-10-24")", "holiday durga_puja ends before it starts"},
    {R"("start": "2043-01-05", "end": "2043-02-27")", R"("start": "2043-01-05", "end": "2043-03-05")", "lies outside the calendar's window"},
    {R"("start": "2042-11-03", "end": "2042-12-23")", R"("start": "2042-10-10", "end": "2042-12-23")", "term term_2 overlaps term term_1"},
    {R"("start": "2042-11-03", "end": "2042-12-23")", R"("start": "2042-05-01", "end": "2042-12-23")", "is listed after term_1 but starts earlier"},
    {R"("yearStart": "2042-06-02")", R"("yearStart": "2042-06-09")", "the calendar must start on the campaign start, 2042-06-02"},
    // Terms, breaks and holidays tile the window and explain every closure.
    {R"("start": "2042-11-03", "end": "2042-12-23")", R"("start": "2042-11-10", "end": "2042-12-23")", "2042-11-03 is in neither a term nor a break"},
    {R"("start": "2042-10-18", "end": "2042-11-02")", R"("start": "2042-10-10", "end": "2042-11-02")", "straddles the edge of term term_1"},
    {R"("start": "2042-10-20", "end": "2042-10-24")", R"("start": "2042-10-20", "end": "2042-11-04")", "is not wholly inside a term or a break"},
    {R"("id": "term_2")", R"("id": "term_1")", "two terms are both called term_1"},
    {R"("id": "term_1", "name")", R"("id": "Term 1", "name")", "must be lowercase letters, digits and underscores"},
    // Assessments sit in their terms and have school days; the sports day is a school day.
    {R"("term": "term_1", "start": "2042-07-28")", R"("term": "term_2", "start": "2042-07-28")", "is not inside its term term_2"},
    {R"("term": "term_1", "start": "2042-07-28")", R"("term": "term_9", "start": "2042-07-28")", "names the term 'term_9', which the calendar does not have"},
    {R"("start": "2042-12-08", "end": "2042-12-12")", R"("start": "2042-12-06", "end": "2042-12-07")", "assessment unit_test_2 has no school day in it"},
    {R"("date": "2042-12-05")", R"("date": "2042-12-06")", "event sports_day on 2042-12-06 is not on a school day"},
    {R"("date": "2042-12-05")", R"("date": "2042-08-15")", "event sports_day on 2042-08-15 is not on a school day"},
    // The documented entries are all there.
    {R"("id": "independence_day")", R"("id": "indep_day")", "must declare the holiday independence_day on 2042-08-15"},
    {R"("start": "2042-08-15", "end": "2042-08-15")", R"("start": "2042-08-14", "end": "2042-08-14")", "must declare the holiday independence_day on 2042-08-15"},
    {R"("id": "durga_puja")", R"("id": "puja_x")", "must declare the holiday durga_puja"},
    {R"("id": "kali_puja_diwali")", R"("id": "puja_y")", "must declare the holiday kali_puja_diwali"},
    {R"("id": "saraswati_puja")", R"("id": "puja_z")", "must declare the holiday saraswati_puja"},
    {R"("id": "winter_break")", R"("id": "cold_break")", "must declare the break winter_break"},
    {R"("id": "sports_day", "name")", R"("id": "field_day", "name")", "must declare the event sports_day"},
    // A lunisolar date must say so; a fixed date cannot be approximate.
    {"The date is approximate, to be confirmed. Lunisolar: worked", "The date is a guess. Lunisolar: worked", "must stay unverified and its note must say approximate, to be confirmed"},
    {R"("dateBasis": "lunisolar_approximate", "status": "unverified")", R"("dateBasis": "lunisolar_approximate", "status": "reviewed")", "must stay unverified and its note must say approximate, to be confirmed"},
    {R"("dateBasis": "fixed")", R"("dateBasis": "lunisolar_approximate")", "is a fixed-date national holiday and cannot be approximate"},
    // The Awakening, the examination and the ten days.
    {R"("awakeningDate": "2043-01-23")", R"("awakeningDate": "2043-01-22")", "the Awakening must be on 2043-01-23"},
    {R"("examinationDate": "2043-02-02")", R"("examinationDate": "2043-02-03")", "the examination must be exactly 10 days after the Awakening"},
    {R"("start": "2043-01-26", "end": "2043-01-26")", R"("start": "2043-01-23", "end": "2043-01-23")", "must be a school day: the tutorial dungeon is held at the school"},
    {R"("start": "2043-01-24", "end": "2043-02-02")", R"("start": "2043-01-24", "end": "2043-01-30")", "the ten days after the Awakening have no school, but 2043-02-02 is a school day"},
    // School weekdays.
    {R"("schoolWeekdays": [1, 2, 3, 4, 5])", R"("schoolWeekdays": [1, 1, 2, 3, 4, 5])", "school weekday 1 is listed twice"},
    {R"("schoolWeekdays": [1, 2, 3, 4, 5])", R"("schoolWeekdays": [])", "the calendar declares no school weekdays"},
    {R"("schoolWeekdays": [1, 2, 3, 4, 5])", R"("schoolWeekdays": [1, 2, 3, 4, 8])", "outside 0..6"},
    // Shape and references.
    {R"("kind": "national")", R"("kind": "feast")", "field \"kind\" has an unknown value"},
    {R"("institution": "institution.nabapara_vidyalaya")", R"("institution": "institution.nga")", "is not a school"},
    {R"("stage": "stage.higher_secondary")", R"("stage": "stage.secondary")", "does not offer stage.secondary"},
    {R"("institution": "institution.nabapara_vidyalaya")", R"("institution": "institution.nowhere")", "which does not exist"},
};

void TestMutations(const std::vector<ContentFile>& files)
{
    for (const Mutation& m : Mutations)
    {
        const ContentLoadResult result = LoadContent(Mutated(files, m.From, m.To));
        if (!HasIssue(result, m.Issue))
        {
            for (const ContentIssue& found : result.Issues)
                std::cerr << "  issue: " << found.Message << '\n';
        }
        Expect(HasIssue(result, m.Issue), std::string("expected: ") + m.Issue);
        Expect(!result.Ok(), "a mutated record is not valid content");
    }
}

void TestLoaderRefusesAndSecondCalendar(const std::vector<ContentFile>& files)
{
    // Content that passes the shape rules but fails a calendar rule is refused by the loader with the record named.
    const ContentLoadResult shapeOk = LoadContent(files);
    Expect(shapeOk.Ok(), "the real content loads");
    SchoolCalendar broken = *LoadSchoolCalendars(shapeOk.Db).Set.Find("institution.nabapara_vidyalaya", "stage.higher_secondary");
    Expect(ValidateSchoolCalendar(broken).empty(), "the unedited calendar has no problems");
    broken.Terms.clear();
    Expect(!ValidateSchoolCalendar(broken).empty(), "a calendar without terms is invalid");
    broken = *LoadSchoolCalendars(shapeOk.Db).Set.Calendars.data();
    broken.Holidays[2].Last += 20;
    Expect(!ValidateSchoolCalendar(broken).empty(), "stretching Durga Puja out of its break is invalid");
    broken = *LoadSchoolCalendars(shapeOk.Db).Set.Calendars.data();
    broken.Breaks.push_back(broken.Breaks[0]);
    broken.Breaks.back().Id = "puja_again";
    const std::vector<std::string> overlapping = ValidateSchoolCalendar(broken);
    Expect(std::any_of(overlapping.begin(), overlapping.end(),
                       [](const std::string& p) { return p.find("overlaps break puja_vacation") != std::string::npos; }),
           "two breaks may not overlap");

    // Saturdays as school days: still valid, and the extra weekday shows up in the queries.
    const ContentLoadResult saturdays = LoadContent(Mutated(files, R"("schoolWeekdays": [1, 2, 3, 4, 5])", R"("schoolWeekdays": [1, 2, 3, 4, 5, 6])"));
    Expect(saturdays.Ok(), "a six-day school week is valid content");
    const SchoolCalendarResult sixDays = LoadSchoolCalendars(saturdays.Db);
    Expect(sixDays.Ok() && sixDays.Set.Calendars.size() == 1, "a six-day calendar loads");
    const SchoolCalendar& six = sixDays.Set.Calendars.front();
    Expect(CalendarIsSchoolDay(six, 5) && CalendarCountSchoolDays(six, six.FirstDay, six.LastDay) > 168, "a Saturday is a school day when listed");
    Expect(!CalendarIsSchoolDay(six, 6) && !CalendarIsSchoolDay(six, 74) && CalendarIsSchoolDay(six, 75),
           "a Sunday never is, Independence Day still is not, and the Saturday after it is");

    // A second calendar for the same school and stage is refused.
    std::vector<ContentFile> twice = files;
    for (const ContentFile& file : files)
    {
        if (file.Path.find(CalendarPath) == std::string::npos)
            continue;
        ContentFile copy = file;
        copy.Path = "/calendar/calendar.copy.json";
        const std::string from = "calendar.nabapara_2042_43";
        copy.Text.replace(copy.Text.find(from), from.size(), "calendar.copy");
        twice.push_back(copy);
    }
    const ContentLoadResult doubled = LoadContent(twice);
    Expect(HasIssue(doubled, "a second calendar for institution.nabapara_vidyalaya at stage.higher_secondary"), "one calendar per school and stage");

    // The loader names the record and refuses it.
    std::vector<ContentFile> edited = Mutated(files, R"("awakeningDate": "2043-01-23")", R"("awakeningDate": "2043-01-22")");
    ContentLoadResult bad = LoadContent(edited);
    Expect(!bad.Ok(), "the edited record is invalid content");
    bad.Issues.clear();
    const SchoolCalendarResult refused = LoadSchoolCalendars(bad.Db);
    Expect(!refused.Ok() && refused.Error.find("calendar.nabapara_2042_43: ") == 0 && refused.Set.Calendars.empty(),
           "LoadSchoolCalendars refuses an invalid calendar and names it");
}
} // namespace

void TestCalendarMutations(const std::vector<ContentFile>& files, int& checks)
{
    Count = &checks;
    TestMutations(files);
    TestLoaderRefusesAndSecondCalendar(files);
}
