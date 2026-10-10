// ROADMAP M3-09: the 2042-43 school calendar, its content rules and its queries.
// Usage: PerfectWorldSchoolCalendarTests <path to Data/Content>
#include "Attendance.h"
#include "SchoolCalendar.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <utility>
#include <sstream>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int Checks = 0;

void Expect(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<ContentFile> Files;
SchoolCalendarSet Real;
const SchoolCalendar* Cal = nullptr;

constexpr const char* School = "institution.nabapara_vidyalaya";
constexpr const char* Stage = "stage.higher_secondary";
constexpr const char* CalendarFile = "calendar.nabapara_2042_43.json";

CalendarDay Day(const char* iso)
{
    const std::optional<CalendarDay> day = ParseCalendarDate(iso);
    Expect(day.has_value(), "a test date parses");
    return day.value_or(0);
}

std::vector<ContentFile> ReadDirectory(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    }
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const fs::path& path : paths)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

// ---- An oracle that shares no code with the module -----------------------------------

struct RawSpan
{
    const char* From;
    const char* To;
};

constexpr RawSpan RawTerms[] = {{"2042-06-02", "2042-10-17"}, {"2042-11-03", "2042-12-23"}, {"2043-01-05", "2043-02-27"}};
constexpr RawSpan RawBreaks[] = {{"2042-10-18", "2042-11-02"}, {"2042-12-24", "2043-01-04"}, {"2043-01-24", "2043-02-02"}};
constexpr RawSpan RawHolidays[] = {{"2042-08-15", "2042-08-15"}, {"2042-10-02", "2042-10-02"}, {"2042-10-20", "2042-10-24"},
                                   {"2042-11-12", "2042-11-12"}, {"2042-12-25", "2042-12-25"}, {"2043-01-26", "2043-01-26"},
                                   {"2043-02-14", "2043-02-14"}};

/** The raw spans as day numbers, parsed once. */
template <std::size_t N>
std::vector<std::pair<CalendarDay, CalendarDay>> Resolve(const RawSpan (&spans)[N])
{
    std::vector<std::pair<CalendarDay, CalendarDay>> out;
    for (const RawSpan& span : spans)
        out.emplace_back(Day(span.From), Day(span.To));
    return out;
}

bool InAny(const std::vector<std::pair<CalendarDay, CalendarDay>>& spans, CalendarDay day)
{
    return std::any_of(spans.begin(), spans.end(), [&](const auto& s) { return day >= s.first && day <= s.second; });
}

bool OracleIsSchoolDay(CalendarDay day)
{
    static const auto terms = Resolve(RawTerms);
    static const auto breaks = Resolve(RawBreaks);
    static const auto holidays = Resolve(RawHolidays);
    const std::int64_t weekdayFromMonday = ((day % 7) + 7) % 7; // day 0 is a Monday
    return weekdayFromMonday < 5 && InAny(terms, day) && !InAny(breaks, day) && !InAny(holidays, day);
}

struct Rng
{
    std::uint64_t S;
    std::int64_t Pick(std::int64_t low, std::int64_t high)
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return low + static_cast<std::int64_t>(((S * 2685821657736338717ULL) >> 16) % static_cast<std::uint64_t>(high - low + 1));
    }
};

// ---- Tests ---------------------------------------------------------------------------

void TestDates()
{
    Expect(CalendarDayFromDate(2042, 6, 2) == 0, "2 June 2042 is day 0");
    Expect(CalendarWeekdayOf(0) == Weekday::Monday, "day 0 is a Monday");
    Expect(FormatCalendarDate(0) == "2042-06-02" && FormatCalendarDate(-1) == "2042-06-01", "dates format");
    Expect(ParseCalendarDate("2044-02-29") && !ParseCalendarDate("2043-02-29"), "leap days are handled");
    for (const char* bad : {"2042-02-30", "2042-13-01", "2042-00-10", "2042-06-00", "2042-6-2", "20420602", "2042/06/02", "2042-06-02 ",
                            " 2042-06-02", "2042-06-0a", "", "42-06-02", "2042-06-021"})
        Expect(!ParseCalendarDate(bad), bad);
    for (CalendarDay day = -400; day <= 800; ++day)
    {
        const std::optional<CalendarDay> back = ParseCalendarDate(FormatCalendarDate(day));
        Expect(back && *back == day, "every day round-trips through its text");
        Expect(CalendarDayOfInstant(CalendarDayStartInstant(day)) == day, "a day's first instant is in that day");
        Expect(CalendarDayOfInstant(CalendarDayStartInstant(day) - 1) == day - 1, "the minute before it is the day before");
        Expect(CalendarWeekdayOf(day) == static_cast<Weekday>(((day + 1) % 7 + 7) % 7), "weekdays cycle with day 0 a Monday");
    }
    const GameInstant independence = GameClock::FromCalendar(2042, 8, 15, 0, 0);
    Expect(CalendarDayOfInstant(independence) == 74, "15 August 2042 is day 74");
    Expect(CalendarDayOfInstant(GameClock::FromCalendar(2042, 8, 15, 23, 59)) == 74, "23:59 is still that day");
    Expect(CalendarDayOfInstant(0) == 0 && CalendarDayOfInstant(GameClock::FromCalendar(2043, 1, 23, 6, 0)) == 235,
           "the campaign opens on day 0 and ends on day 235");
}

void TestContent()
{
    const ContentLoadResult loaded = LoadContent(Files);
    for (const ContentIssue& issue : loaded.Issues)
        std::cerr << "  issue: " << issue.File << ' ' << issue.Message << '\n';
    Expect(loaded.Ok(), "Data/Content validates with the calendar");
    Expect(loaded.Db.CountOfType("calendar") == 1, "one calendar record");
    const SchoolCalendarResult result = LoadSchoolCalendars(loaded.Db);
    Expect(result.Ok(), result.Error.c_str());
    Real = result.Set;
    Expect(Real.Calendars.size() == 1, "one calendar loaded");
    Cal = Real.Find(School, Stage);
    Expect(Cal != nullptr && Cal == Real.FindById("calendar.nabapara_2042_43"), "found by school and stage and by ID");
    Expect(Real.Find(School, "stage.secondary") == nullptr && Real.FindById("calendar.none") == nullptr, "no calendar for another stage or ID");
    Expect(Cal->State == "West Bengal" && Cal->InstitutionId == School && Cal->StageId == Stage, "the record's identity is read");
    Expect(Cal->FirstDay == 0 && Cal->LastDay == 270 && Cal->AwakeningDay == 235 && Cal->ExaminationDay == 245, "window, Awakening, exam");
    Expect(Cal->Terms.size() == 3 && Cal->Breaks.size() == 3 && Cal->Holidays.size() == 7 && Cal->Assessments.size() == 4 &&
               Cal->Events.size() == 1,
           "the entries are read");
    Expect(Cal->SchoolWeekdays == AttendanceSchoolWeekdays, "the calendar's weekdays are the attendance module's Monday to Friday");
    Expect(ValidateSchoolCalendar(*Cal).empty(), "the real calendar validates");
    Expect(std::all_of(Cal->Breaks.begin(), Cal->Breaks.end(), [](const CalendarBreak& b) { return b.Unverified; }) &&
               std::all_of(Cal->Events.begin(), Cal->Events.end(), [](const CalendarEvent& e) { return e.Unverified; }),
           "every date is flagged unverified until the reviewer clears it");
    Expect(std::all_of(Cal->Holidays.begin(), Cal->Holidays.end(), [](const CalendarHoliday& h) { return h.Unverified; }) &&
               std::all_of(Cal->Terms.begin(), Cal->Terms.end(), [](const CalendarTerm& t) { return t.Unverified; }) &&
               std::all_of(Cal->Assessments.begin(), Cal->Assessments.end(), [](const CalendarAssessment& a) { return a.Unverified; }),
           "no entry is marked reviewed");
    std::set<std::string> approximate;
    for (const CalendarHoliday& holiday : Cal->Holidays)
    {
        if (holiday.Approximate)
            approximate.insert(holiday.Id);
    }
    Expect(approximate == std::set<std::string>{"durga_puja", "kali_puja_diwali", "saraswati_puja"}, "exactly the lunisolar festivals are approximate");
}

struct DocCase
{
    const char* Date;
    SchoolDayKind Kind;
    const char* Term;
    const char* Break;
    const char* Holiday;
    const char* Assessment;
    const char* Event;
};

void TestDocumentedDates()
{
    using K = SchoolDayKind;
    const DocCase cases[] = {
        {"2042-06-01", K::OutsideYear, nullptr, nullptr, nullptr, nullptr, nullptr},
        {"2042-06-02", K::SchoolDay, "term_1", nullptr, nullptr, nullptr, nullptr},
        {"2042-06-06", K::SchoolDay, "term_1", nullptr, nullptr, nullptr, nullptr},
        {"2042-06-07", K::NonSchoolWeekday, "term_1", nullptr, nullptr, nullptr, nullptr},
        {"2042-07-28", K::SchoolDay, "term_1", nullptr, nullptr, "unit_test_1", nullptr},
        {"2042-08-01", K::SchoolDay, "term_1", nullptr, nullptr, "unit_test_1", nullptr},
        {"2042-08-15", K::Holiday, "term_1", nullptr, "independence_day", nullptr, nullptr},
        {"2042-09-15", K::SchoolDay, "term_1", nullptr, nullptr, "half_yearly", nullptr},
        {"2042-10-02", K::Holiday, "term_1", nullptr, "gandhi_jayanti", nullptr, nullptr},
        {"2042-10-17", K::SchoolDay, "term_1", nullptr, nullptr, nullptr, nullptr},
        {"2042-10-18", K::NonSchoolWeekday, nullptr, "puja_vacation", nullptr, nullptr, nullptr},
        {"2042-10-19", K::NonSchoolWeekday, nullptr, "puja_vacation", nullptr, nullptr, nullptr},
        {"2042-10-20", K::Holiday, nullptr, "puja_vacation", "durga_puja", nullptr, nullptr},
        {"2042-10-22", K::Holiday, nullptr, "puja_vacation", "durga_puja", nullptr, nullptr},
        {"2042-10-24", K::Holiday, nullptr, "puja_vacation", "durga_puja", nullptr, nullptr},
        {"2042-10-27", K::Break, nullptr, "puja_vacation", nullptr, nullptr, nullptr},
        {"2042-11-02", K::NonSchoolWeekday, nullptr, "puja_vacation", nullptr, nullptr, nullptr},
        {"2042-11-03", K::SchoolDay, "term_2", nullptr, nullptr, nullptr, nullptr},
        {"2042-11-12", K::Holiday, "term_2", nullptr, "kali_puja_diwali", nullptr, nullptr},
        {"2042-11-13", K::SchoolDay, "term_2", nullptr, nullptr, nullptr, nullptr},
        {"2042-12-05", K::SchoolDay, "term_2", nullptr, nullptr, nullptr, "sports_day"},
        {"2042-12-08", K::SchoolDay, "term_2", nullptr, nullptr, "unit_test_2", nullptr},
        {"2042-12-23", K::SchoolDay, "term_2", nullptr, nullptr, nullptr, nullptr},
        {"2042-12-24", K::Break, nullptr, "winter_break", nullptr, nullptr, nullptr},
        {"2042-12-25", K::Holiday, nullptr, "winter_break", "christmas", nullptr, nullptr},
        {"2043-01-01", K::Break, nullptr, "winter_break", nullptr, nullptr, nullptr},
        {"2043-01-04", K::NonSchoolWeekday, nullptr, "winter_break", nullptr, nullptr, nullptr},
        {"2043-01-05", K::SchoolDay, "term_3", nullptr, nullptr, nullptr, nullptr},
        {"2043-01-12", K::SchoolDay, "term_3", nullptr, nullptr, "january_class_test", nullptr},
        {"2043-01-23", K::SchoolDay, "term_3", nullptr, nullptr, nullptr, nullptr},
        {"2043-01-24", K::NonSchoolWeekday, "term_3", "ten_days", nullptr, nullptr, nullptr},
        {"2043-01-26", K::Holiday, "term_3", "ten_days", "republic_day", nullptr, nullptr},
        {"2043-01-27", K::Break, "term_3", "ten_days", nullptr, nullptr, nullptr},
        {"2043-02-02", K::Break, "term_3", "ten_days", nullptr, nullptr, nullptr},
        {"2043-02-03", K::SchoolDay, "term_3", nullptr, nullptr, nullptr, nullptr},
        {"2043-02-14", K::NonSchoolWeekday, "term_3", nullptr, "saraswati_puja", nullptr, nullptr},
        {"2043-02-27", K::SchoolDay, "term_3", nullptr, nullptr, nullptr, nullptr},
        {"2043-02-28", K::OutsideYear, nullptr, nullptr, nullptr, nullptr, nullptr},
    };
    const auto matches = [](const CalendarSpan* found, const char* id) { return id == nullptr ? found == nullptr : found != nullptr && found->Id == id; };
    for (const DocCase& c : cases)
    {
        const CalendarDay day = Day(c.Date);
        Expect(ClassifyCalendarDay(*Cal, day) == c.Kind, c.Date);
        Expect(CalendarIsSchoolDay(*Cal, day) == (c.Kind == SchoolDayKind::SchoolDay), c.Date);
        Expect(matches(CalendarTermOn(*Cal, day), c.Term) && matches(CalendarBreakOn(*Cal, day), c.Break), c.Date);
        Expect(matches(CalendarHolidayOn(*Cal, day), c.Holiday) && matches(CalendarAssessmentOn(*Cal, day), c.Assessment), c.Date);
        Expect(matches(CalendarEventOn(*Cal, day), c.Event), c.Date);
    }
    Expect(CalendarHolidayOn(*Cal, Day("2042-08-15"))->Kind == "national" && !CalendarHolidayOn(*Cal, Day("2042-08-15"))->Approximate,
           "Independence Day is a fixed national holiday");
    Expect(CalendarHolidayOn(*Cal, Day("2042-10-22"))->Kind == "festival" && CalendarHolidayOn(*Cal, Day("2042-10-22"))->Approximate,
           "Durga Puja is an approximate festival");
    Expect(CalendarAssessmentOn(*Cal, Day("2042-09-15"))->TermId == "term_1", "an assessment names its term");
    Expect(SchoolDayKindName(SchoolDayKind::Holiday) == "holiday" && SchoolDayKindName(SchoolDayKind::SchoolDay) == "school_day", "kind names");
}

void TestClosures()
{
    for (CalendarDay day = Cal->FirstDay; day <= Cal->LastDay; ++day)
    {
        const Weekday weekday = CalendarWeekdayOf(day);
        if (weekday == Weekday::Saturday || weekday == Weekday::Sunday)
            Expect(!CalendarIsSchoolDay(*Cal, day), "a weekend is not a school day");
        Expect(CalendarWeekdayIsSchoolDay(*Cal, weekday) == (weekday != Weekday::Saturday && weekday != Weekday::Sunday), "weekday mask");
    }
    for (const CalendarHoliday& holiday : Cal->Holidays)
        for (CalendarDay day = holiday.First; day <= holiday.Last; ++day)
            Expect(!CalendarIsSchoolDay(*Cal, day) && CalendarHolidayOn(*Cal, day) == &holiday, "school is closed on every declared holiday");
    for (const CalendarBreak& item : Cal->Breaks)
        for (CalendarDay day = item.First; day <= item.Last; ++day)
            Expect(!CalendarIsSchoolDay(*Cal, day), "school is closed in every break");
    for (CalendarDay day = Day("2043-01-24"); day <= Day("2043-02-02"); ++day)
        Expect(!CalendarIsSchoolDay(*Cal, day), "no school in the ten days");
    Expect(CalendarIsSchoolDay(*Cal, Day("2043-01-23")) && !CalendarIsSchoolDay(*Cal, Cal->ExaminationDay), "Awakening Day is a school day, the exam is not");
    Expect(!CalendarIsSchoolDay(*Cal, -5) && !CalendarIsSchoolDay(*Cal, 100000) && !CalendarIsSchoolDay(*Cal, INT64_MIN + 1), "outside the year");
    for (CalendarDay day = -3; day <= 275; ++day)
    {
        const GameInstant start = CalendarDayStartInstant(day);
        for (const GameInstant instant : {start, start + 6 * 60, start + 12 * 60, start + 1439})
            Expect(CalendarIsSchoolDayAt(*Cal, instant) == CalendarIsSchoolDay(*Cal, day), "every instant of a day gives the day's answer");
    }
    const SchoolCalendar empty;
    Expect(!CalendarIsSchoolDay(empty, 0) && !CalendarNextSchoolDay(empty, -1) && CalendarCountSchoolDays(empty, -10, 10) == 0, "an empty calendar has no school");
}

void TestNavigationAndCounts()
{
    Expect(CalendarNextSchoolDay(*Cal, Day("2042-08-14")) == Day("2042-08-18"), "next school day skips Independence Day and the weekend");
    Expect(CalendarNextSchoolDay(*Cal, Day("2042-10-17")) == Day("2042-11-03"), "next school day skips the Puja vacation");
    Expect(CalendarNextSchoolDay(*Cal, Day("2042-11-11")) == Day("2042-11-13"), "next school day skips Kali Puja and Diwali");
    Expect(CalendarNextSchoolDay(*Cal, Day("2042-12-23")) == Day("2043-01-05"), "next school day skips the winter break and Christmas");
    Expect(CalendarNextSchoolDay(*Cal, Day("2043-01-23")) == Day("2043-02-03"), "next school day skips the ten days and Republic Day");
    Expect(!CalendarNextSchoolDay(*Cal, Day("2043-02-27")) && !CalendarNextSchoolDay(*Cal, INT64_MAX), "none after the last school day");
    Expect(CalendarNextSchoolDay(*Cal, -50) == 0 && CalendarNextSchoolDay(*Cal, INT64_MIN + 1) == 0, "from before the year the first school day is day 0");
    Expect(CalendarSchoolDayOnOrAfter(*Cal, Day("2042-08-18")) == Day("2042-08-18"), "on or after keeps a school day");
    Expect(CalendarSchoolDayOnOrAfter(*Cal, Day("2042-10-20")) == Day("2042-11-03"), "on or after skips a holiday");
    Expect(CalendarPreviousSchoolDay(*Cal, Day("2042-11-03")) == Day("2042-10-17"), "previous school day crosses the vacation");
    Expect(CalendarPreviousSchoolDay(*Cal, Day("2042-08-18")) == Day("2042-08-14"), "previous school day skips the holiday");
    Expect(!CalendarPreviousSchoolDay(*Cal, 0) && !CalendarPreviousSchoolDay(*Cal, -4), "none before the first school day");

    // Walking next-school-day from before the year visits exactly the school days a brute-force walk finds.
    std::vector<CalendarDay> walked;
    for (std::optional<CalendarDay> day = CalendarNextSchoolDay(*Cal, -1); day; day = CalendarNextSchoolDay(*Cal, *day))
        walked.push_back(*day);
    std::vector<CalendarDay> brute;
    for (CalendarDay day = -20; day <= 300; ++day)
    {
        Expect(CalendarIsSchoolDay(*Cal, day) == OracleIsSchoolDay(day), "the module agrees with the independent oracle on every day");
        if (OracleIsSchoolDay(day))
            brute.push_back(day);
    }
    Expect(walked == brute && walked.size() == 168, "the whole campaign year has 168 school days");
    for (std::size_t i = 1; i < walked.size(); ++i)
        Expect(CalendarPreviousSchoolDay(*Cal, walked[i]) == walked[i - 1], "previous inverts next");

    Expect(CalendarCountSchoolDays(*Cal, Cal->FirstDay, Cal->LastDay) == 168, "168 school days over the window");
    Expect(CalendarCountSchoolDays(*Cal, -1000, 5000) == 168, "a wider range counts only the window");
    Expect(CalendarCountSchoolDays(*Cal, 0, Day("2043-01-23")) == 149, "149 school days from day 0 to Awakening Day");
    Expect(CalendarCountSchoolDays(*Cal, 10, 9) == 0 && CalendarCountSchoolDays(*Cal, Day("2042-08-15"), Day("2042-08-15")) == 0, "empty and holiday ranges");
    Expect(CalendarCountSchoolDays(*Cal, 0, 0) == 1, "a single school day counts one");
    const std::int64_t termDays[] = {98, 36, 34};
    for (std::size_t i = 0; i < 3; ++i)
        Expect(CalendarCountSchoolDays(*Cal, Cal->Terms[i].First, Cal->Terms[i].Last) == termDays[i], "school days per term");
    const std::int64_t assessmentDays[] = {5, 10, 5, 5};
    for (std::size_t i = 0; i < 4; ++i)
        Expect(CalendarCountSchoolDays(*Cal, Cal->Assessments[i].First, Cal->Assessments[i].Last) == assessmentDays[i], "school days per assessment window");
    struct Month
    {
        const char* From;
        const char* To;
        std::int64_t Days;
    };
    for (const Month& m : {Month{"2042-06-01", "2042-06-30", 21}, Month{"2042-08-01", "2042-08-31", 20}, Month{"2042-10-01", "2042-10-31", 12},
                           Month{"2042-11-01", "2042-11-30", 19}, Month{"2043-01-01", "2043-01-31", 15}, Month{"2043-02-01", "2043-02-28", 19}})
        Expect(CalendarCountSchoolDays(*Cal, Day(m.From), Day(m.To)) == m.Days, m.From);

    Rng rng{0x2042'0602'2043'0123ULL};
    for (int round = 0; round < 3000; ++round)
    {
        const CalendarDay first = rng.Pick(-30, 300);
        const CalendarDay last = rng.Pick(-30, 300);
        std::int64_t expected = 0;
        for (CalendarDay day = first; day <= last; ++day)
            expected += OracleIsSchoolDay(day) ? 1 : 0;
        Expect(CalendarCountSchoolDays(*Cal, first, last) == expected, "a random range counts what a brute-force walk counts");
        if (first <= last && first < 270)
        {
            const std::optional<CalendarDay> next = CalendarNextSchoolDay(*Cal, first);
            CalendarDay probe = std::max<CalendarDay>(first + 1, 0);
            while (probe <= 270 && !OracleIsSchoolDay(probe))
                ++probe;
            Expect(next == (probe <= 270 ? std::optional<CalendarDay>(probe) : std::nullopt), "next school day matches a brute-force probe");
        }
    }
}
} // namespace

void TestCalendarMutations(const std::vector<ContentFile>& files, int& checks);

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldSchoolCalendarTests <Data/Content>\n";
        return 2;
    }
    Files = ReadDirectory(argv[1]);
    TestDates();
    TestContent();
    TestDocumentedDates();
    TestClosures();
    TestNavigationAndCounts();
    TestCalendarMutations(Files, Checks);
    std::cout << "PerfectWorldSchoolCalendarTests: " << Checks << " checks passed\n";
    return 0;
}
