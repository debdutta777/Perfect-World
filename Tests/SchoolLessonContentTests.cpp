#include "SchoolLessonContent.h"

#include "P2CTestSupport.h"

#include <iterator>
#include <set>

using namespace PerfectWorld;
namespace
{
int A1Assertions = 0;
void A1Expect(bool value, std::string_view message)
{
    ++A1Assertions;
    P2CExpect(value, message);
}
std::vector<ContentFile> A1Read(const char* root)
{
    std::vector<ContentFile> files;
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(root, error), end;
    A1Expect(!error, "content directory opens");
    while (it != end)
    {
        if (it->is_regular_file(error) && it->path().extension() == ".json")
        {
            std::ifstream input(it->path(), std::ios::binary);
            files.push_back({it->path().generic_string(), {std::istreambuf_iterator<char>(input), {}}});
        }
        it.increment(error);
    }
    return files;
}
std::string A1ReadText(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    A1Expect(input.good(), "text file opens");
    return {std::istreambuf_iterator<char>(input), {}};
}
std::vector<ContentFile> A1Mutated(std::vector<ContentFile> files, std::string_view file, std::string_view from, std::string_view to)
{
    bool found = false;
    for (auto& f : files)
    {
        if (!f.Path.ends_with("/" + std::string(file))) continue;
        const auto at = f.Text.find(from);
        A1Expect(at != std::string::npos, "mutation text exists");
        f.Text.replace(at, from.size(), to);
        found = true;
    }
    A1Expect(found, "mutation file exists");
    return files;
}
void A1Refused(const std::vector<ContentFile>& files, std::string_view file, std::string_view from, std::string_view to, std::string_view reason)
{
    const auto result = LoadContent(A1Mutated(files, file, from, to));
    A1Expect(!result.Ok(), "bad school lesson content is refused");
    bool said = false;
    for (const auto& issue : result.Issues) said = said || issue.Message.find(reason) != std::string::npos;
    if (!said) for (const auto& issue : result.Issues) std::cerr << issue.File << " " << issue.Message << '\n';
    A1Expect(said, reason);
    const auto typed = DecodeSchoolLessonContent(result.Db);
    if (typed.Ok()) std::cerr << "typed decoder accepted the mutation: " << reason << std::endl;
    A1Expect(!typed.Ok() && typed.Catalogue == SchoolLessonContent{}, "typed decoder refuses atomically too");
}
void A1TextRefused(const ContentDb& db, std::string text, std::string_view reason)
{
    const auto result = LoadSchoolLessonContent(db, {"/Data/Text/en/phase2_a1.json", std::move(text)});
    A1Expect(!result.Ok() && result.Catalogue == SchoolLessonContent{}, "bad text table is refused atomically");
    bool said = false;
    for (const auto& issue : result.Issues) said = said || issue.Message.find(reason) != std::string::npos;
    A1Expect(said, reason);
}
void A1PassTo(DayLoopState& day, const SchoolSessionCatalogues& cats, std::int64_t calendarDay, std::int32_t period)
{
    const auto target = CalendarDayStartInstant(calendarDay) + P2CBellMinute(*cats.Day.School, period);
    if (day.Now < target)
    {
        DayCommand pass; pass.Kind = DayCommandKind::Pass; pass.Minutes = target - day.Now;
        A1Expect(ApplyDayCommand(day, cats.Day, pass).Ok(), "pass to bell");
    }
}
bool A1HasText(const SchoolLessonContent& c, const std::string& key) { return !key.empty() && FindSchoolLessonText(c, key) != nullptr; }
void A1AllRowTextResolves(const SchoolLessonContent& c, const SchoolDayView& v)
{
    for (const auto& row : v.Rows)
    {
        A1Expect(A1HasText(c, row.TitleTextKey) && A1HasText(c, row.BriefTextKey) && A1HasText(c, row.RoleTextKey), "row words resolve");
        for (const auto* key : {&row.ReasonTextKey, &row.RecoveryTextKey, &row.ForegroundReasonTextKey, &row.RoutineReasonTextKey})
            A1Expect(key->empty() || A1HasText(c, *key), "explanation words resolve");
    }
    A1Expect(v.ClosureTextKey.empty() || A1HasText(c, v.ClosureTextKey), "closure words resolve");
}

void A1Catalogue(const P2CFixture& f, const SchoolLessonContent& content)
{
    A1Expect(content.Streams.size() == 3, "all three streams");
    std::set<std::string> ids, structures;
    for (const auto& stream : content.Streams)
    {
        const auto cats = f.Cats(stream.Stream);
        A1Expect(stream.Lessons.size() == static_cast<std::size_t>(BellPeriodCount(*cats.Day.School)), "a lesson for every period");
        std::set<std::string> subjects;
        for (std::size_t p = 0; p < stream.Lessons.size(); ++p)
        {
            const auto& b = stream.Lessons[p].Binding;
            A1Expect(b.Period == static_cast<std::int32_t>(p) && b.Stream == stream.Stream, "period order and stream");
            const auto issue = ValidateSchoolLessonBinding(b, cats);
            if (!issue.Ok()) std::cerr << b.Id << " " << issue.Code << '\n';
            A1Expect(issue.Ok(), "the frozen contract accepts every binding");
            A1Expect(ids.insert(b.Id).second, "binding ids are unique");
            structures.insert(std::string(AssessmentStructureNames[static_cast<std::size_t>(b.Structure)]));
            A1Expect(FindSchoolLesson(content, stream.Stream, b.Period) == &stream.Lessons[p], "lookup by period");
            A1Expect(FindSchoolLessonBinding(content, b.Id) != nullptr && *FindSchoolLessonBinding(content, b.Id) == b, "lookup by id");
            subjects.insert(b.Subject);
        }
        A1Expect(subjects.size() >= 5, "a varied timetable");
        A1Expect(SchoolLessonBindings(content, stream.Stream).size() == stream.Lessons.size(), "bindings listed");
    }
    A1Expect(structures.size() == 4, "all four structure families are bound");
    A1Expect(FindSchoolLessonStream(content, "science") != nullptr && FindSchoolLessonStream(content, "latin") == nullptr, "stream lookup");
    // Science keeps all four structures; the lab practical is the investigation, as the bell schedule's lab period.
    A1Expect(FindSchoolLesson(content, "science", 4)->Binding.Structure == SchoolStructure::Investigate, "lab period investigates");
    for (const auto& code : SchoolLessonExplanationCodes) A1Expect(FindSchoolLessonExplanation(content, code) != nullptr, "every reason is explained");
    for (const auto kind : {SchoolStructure::Model, SchoolStructure::Explain, SchoolStructure::Investigate, SchoolStructure::Coordinate})
    {
        const auto goals = SchoolLessonGoalOptions(content, kind);
        A1Expect(goals.size() >= 3 && A1HasText(content, goals.front().TextKey), "every structure offers authored goals");
    }
    for (const auto mode : {SchoolLessonMode::Foreground, SchoolLessonMode::Routine, SchoolLessonMode::Attend})
        A1Expect(FindSchoolLessonModeText(content, mode) != nullptr, "mode words exist");
}

void A1Mutations(const std::vector<ContentFile>& files)
{
    const std::string science = "p2.science.json", school = "p2.school.json";
    A1Refused(files, science, "\"subject\": \"chemistry\"", "\"subject\": \"english\"", "does not match the bell schedule");
    A1Refused(files, science, "\"familySubject\": \"practicals\"", "\"familySubject\": \"debate\"", "no authored case");
    A1Refused(files, science, "\"familySubject\": \"practicals\"", "\"familySubject\": \"cooking\"", "does not teach");
    A1Refused(files, science, "\"family\": \"coordinate_family.xi_group_work\"", "\"family\": \"explain_family.xi_claim_evidence_limit\"", "wrong type");
    A1Refused(files, science, "\"period\": 5", "\"period\": 4", "two lessons bind period 4");
    A1Refused(files, science, "\"id\": \"school.lesson.science.p1_physics\"", "\"id\": \"school.lesson.science.p0_mathematics\"", "duplicated");
    A1Refused(files, science, "\"titleKey\": \"phase2.a1.lesson.science.p0_mathematics.title\"", "\"titleKey\": \"lesson.mathematics.title\"", "phase2.a1");
    A1Refused(files, science, "\"briefKey\": \"phase2.a1.lesson.science.p0_mathematics.brief\"", "\"briefKey\": \"phase2.a1.lesson.science.p0_mathematics.title\"", "used twice");
    A1Refused(files, science, "\"bellSchedule\": \"bell_schedule.nabapara_xi_science\"", "\"bellSchedule\": \"bell_schedule.nabapara_xi_commerce\"", "another stream");
    A1Refused(files, science, "\"note\":", "\"mystery\": 1, \"note\":", "unknown field");
    A1Refused(files, school, "\"span\": \"christmas\"", "\"span\": \"boxing_day\"", "span the calendar does not have");
    A1Refused(files, school, "\"span\": \"christmas\"", "\"span\": \"republic_day\"", "duplicate closure");
    A1Refused(files, school, "\"code\": \"busy\"", "\"code\": \"closed_day\"", "duplicate explanation");
    A1Refused(files, school, "\"goal\": \"inspect\"", "\"goal\": \"peek\"", "not a runner goal");
    A1Refused(files, school, "\"mode\": \"attend\"", "\"mode\": \"routine\"", "duplicate lesson mode");
    A1Refused(files, school, "\"calendar\": \"calendar.nabapara_2042_43\"", "\"calendar\": \"calendar.nowhere\"", "calendar");
    A1Expect(!LoadSchoolLessonContent(ContentDb{}, {"x", "{}"}).Ok(), "no records is not a catalogue");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 3, "content root and text file arguments");
    const P2CFixture f(argv[1]);
    const auto textBytes = A1ReadText(argv[2]);
    const auto loaded = LoadSchoolLessonContent(f.Content, {"/Data/Text/en/phase2_a1.json", textBytes});
    for (const auto& issue : loaded.Issues) std::cerr << issue.File << " " << issue.Id << " " << issue.Message << '\n';
    A1Expect(loaded.Ok(), "real school lesson content loads with its text");
    const auto& content = loaded.Catalogue;
    A1Catalogue(f, content);
    A1Mutations(A1Read(argv[1]));

    // The text table: missing, unreferenced, malformed and empty entries are all refused.
    A1TextRefused(f.Content, "[]", "JSON object");
    A1TextRefused(f.Content, "{", "JSON object");
    A1TextRefused(f.Content, "{\"phase2.a1.extra.line\": \"nobody uses this\"}", "missing text key");
    {
        auto extra = textBytes;
        extra.insert(extra.find('{') + 1, "\n  \"phase2.a1.zz.unused\": \"unused\",");
        A1TextRefused(f.Content, extra, "unreferenced text key");
        auto emptied = textBytes;
        const auto at = emptied.find("\"phase2.a1.mode.attend.title\": \"") + std::string("\"phase2.a1.mode.attend.title\": \"").size();
        emptied.replace(at, emptied.find('"', at) - at, "   ");
        A1TextRefused(f.Content, emptied, "empty translation");
        auto foreign = textBytes;
        foreign.insert(foreign.find('{') + 1, "\n  \"wrong.prefix\": \"x\",");
        A1TextRefused(f.Content, foreign, "invalid text key");
    }

    // The school day, stream by stream.
    for (const auto* streamName : {"science", "commerce", "humanities"})
    {
        const auto cats = f.Cats(streamName);
        const auto bindings = SchoolLessonBindings(content, streamName);
        SchoolSessionsState sessions;
        auto day = P2CDayAtBell(cats, 0, 0);
        auto view = BuildSchoolDayView(content, sessions, day, cats);
        A1Expect(view.Ok() && view.SchoolDay && view.Rows.size() == 6 && view.Stream == streamName && view.ForegroundUsed == 0, "morning view");
        A1Expect(view.Rows[0].Status == SchoolDayRowStatus::Open && view.Rows[0].ForegroundAvailable && view.Rows[0].AttendAvailable &&
            !view.Rows[0].RoutineAvailable && !view.Rows[0].RoutineReasonTextKey.empty() && view.Rows[0].ForegroundReasonTextKey.empty(),
            "the first period offers foreground and attend; routine says why not");
        for (std::size_t p = 1; p < 6; ++p)
            A1Expect(view.Rows[p].Status == SchoolDayRowStatus::Upcoming && !view.Rows[p].ReasonTextKey.empty() &&
                view.Rows[p].StartMinute == P2CBellMinute(*cats.Day.School, static_cast<std::int32_t>(p)), "later periods wait for their bells");
        A1AllRowTextResolves(content, view);
        A1Expect(view.FindRow(bindings[0].Id) == &view.Rows[0] && view.FindRow("school.lesson.none") == nullptr, "row lookup");

        // Period 0 is cancelled (still uses a foreground slot), period 1 is played, period 2 is refused a third foreground.
        const auto goalFor = [&](const SchoolLessonBinding& b) { return SchoolLessonGoalOptions(content, b.Structure).front().Goal; };
        auto opened = OpenSchoolLesson(sessions, day, cats, bindings[0], SchoolLessonMode::Foreground, goalFor(bindings[0]), 100000, 12);
        A1Expect(opened.Ok(), "content binding opens through the frozen command");
        view = BuildSchoolDayView(content, sessions, day, cats);
        A1Expect(view.LessonOpen && view.ActiveBindingId == bindings[0].Id && view.Rows[0].Status == SchoolDayRowStatus::Active, "active lesson shown");
        A1Expect(CloseSchoolLesson(sessions, day, cats, opened.Session, true, 1).Ok(), "cancel");
        view = BuildSchoolDayView(content, sessions, day, cats);
        A1Expect(!view.LessonOpen && view.Rows[0].Status == SchoolDayRowStatus::Cancelled && view.Rows[0].DoneMode == SchoolLessonMode::Foreground &&
            view.ForegroundUsed == 1 && !view.Rows[0].ReasonTextKey.empty(), "cancelled period used its slot");
        A1PassTo(day, cats, 0, 1);
        opened = OpenSchoolLesson(sessions, day, cats, bindings[1], SchoolLessonMode::Foreground, goalFor(bindings[1]), 100000, 13);
        A1Expect(opened.Ok(), "second foreground opens");
        P2CPlayLesson(sessions, cats, opened.Session);
        A1Expect(CloseSchoolLesson(sessions, day, cats, opened.Session).Ok(), "second foreground closes");
        A1PassTo(day, cats, 0, 2);
        view = BuildSchoolDayView(content, sessions, day, cats);
        A1Expect(view.ForegroundUsed == 2 && view.Rows[1].Status == SchoolDayRowStatus::Done && view.Rows[1].DoneMode == SchoolLessonMode::Foreground, "second period done");
        A1Expect(view.Rows[2].Status == SchoolDayRowStatus::Open && !view.Rows[2].ForegroundAvailable && view.Rows[2].AttendAvailable &&
            view.Rows[2].ForegroundReasonTextKey == FindSchoolLessonExplanation(content, "foreground_limit")->TextKey,
            "the two-foreground limit is explained at the third period");
        const auto refused = OpenSchoolLesson(sessions, day, cats, bindings[2], SchoolLessonMode::Foreground, goalFor(bindings[2]), 100000, 14);
        A1Expect(!refused.Ok() && refused.Issue.Code == "school.session.foreground_limit", "the contract agrees with the view");
        A1Expect(OpenSchoolLesson(sessions, day, cats, bindings[2], SchoolLessonMode::Attend, goalFor(bindings[2]), 100000, 14).Ok(), "attend is still open");
        A1AllRowTextResolves(content, BuildSchoolDayView(content, sessions, day, cats));

        // A period whose bell has passed without a lesson is missed, and recovery depends on attendance.
        auto late = P2CDayAtBell(cats, 7, 0);
        A1PassTo(late, cats, 7, 2);
        view = BuildSchoolDayView(content, SchoolSessionsState{}, late, cats);
        A1Expect(view.Rows[0].Status == SchoolDayRowStatus::Missed && view.Rows[1].Status == SchoolDayRowStatus::Missed &&
            view.Rows[0].ReasonTextKey == FindSchoolLessonExplanation(content, "missed_attended")->TextKey && !view.Rows[0].RecoveryTextKey.empty() &&
            view.Rows[2].Status == SchoolDayRowStatus::Open, "attended but unplayed periods are missed with recovery");

        // The player did not come at all: every started period is missed as absent.
        auto away = NewDayLoopState(CalendarDayStartInstant(8) + 14 * 60);
        view = BuildSchoolDayView(content, SchoolSessionsState{}, away, cats);
        A1Expect(view.Ok() && view.Rows[0].Status == SchoolDayRowStatus::Missed && view.Rows[5].Status == SchoolDayRowStatus::Upcoming &&
            view.Rows[0].ReasonTextKey == FindSchoolLessonExplanation(content, "missed_absent")->TextKey, "absent periods are missed as absent");

        // Recovery after an absence: three closed absent days, then back at school but not yet planned.
        DayLoopState back = NewDayLoopState(CalendarDayStartInstant(0) + 360);
        A1Expect(CloseAttendanceBefore(back.Attendance, *cats.Day.School, CalendarDayStartInstant(3), cats.Day.Calendar).Ok, "three absent days closed");
        back.Now = CalendarDayStartInstant(3) + P2CBellMinute(*cats.Day.School, 0);
        back.Phase = DayLoopPhaseOf(back);
        view = BuildSchoolDayView(content, SchoolSessionsState{}, back, cats);
        A1Expect(view.Ok() && view.RecentAbsentDays == 3 && A1HasText(content, view.AbsenceTextKey) && A1HasText(content, view.AbsenceRecoveryTextKey), "absence recovery is offered");
        A1Expect(view.Rows[0].Status == SchoolDayRowStatus::Unavailable && view.Rows[0].ReasonTextKey == FindSchoolLessonExplanation(content, "busy")->TextKey,
            "an unplanned morning explains itself");
    }

    // Closures: every kind of closed day names itself and says when school returns.
    const auto cats = f.Cats();
    const auto closedOn = [&](int year, int month, int dayOfMonth) {
        const auto d = *CalendarDayFromDate(year, month, dayOfMonth);
        return BuildSchoolDayView(content, SchoolSessionsState{}, NewDayLoopState(CalendarDayStartInstant(d) + 600), cats);
    };
    struct Closed { int Y, M, D; SchoolDayKind Kind; const char* Key; };
    const Closed closed[] = {
        {2042, 6, 7, SchoolDayKind::NonSchoolWeekday, "phase2.a1.closure.weekend.generic"},
        {2042, 8, 15, SchoolDayKind::Holiday, "phase2.a1.closure.holiday.independence_day"},
        {2042, 10, 2, SchoolDayKind::Holiday, "phase2.a1.closure.holiday.gandhi_jayanti"},
        {2042, 10, 22, SchoolDayKind::Holiday, "phase2.a1.closure.holiday.durga_puja"},
        {2042, 10, 28, SchoolDayKind::Break, "phase2.a1.closure.break.puja_vacation"},
        {2042, 11, 12, SchoolDayKind::Holiday, "phase2.a1.closure.holiday.kali_puja_diwali"},
        {2042, 12, 29, SchoolDayKind::Break, "phase2.a1.closure.break.winter_break"},
        {2043, 1, 26, SchoolDayKind::Holiday, "phase2.a1.closure.holiday.republic_day"},
        {2043, 1, 28, SchoolDayKind::Break, "phase2.a1.closure.break.ten_days"},
        {2043, 6, 1, SchoolDayKind::OutsideYear, "phase2.a1.closure.outside_year.generic"},
    };
    for (const auto& c : closed)
    {
        const auto view = closedOn(c.Y, c.M, c.D);
        A1Expect(view.Ok() && !view.SchoolDay && view.Kind == c.Kind && view.ClosureTextKey == c.Key && view.Rows.empty(), c.Key);
        A1Expect(A1HasText(content, view.ClosureTextKey), "closure words resolve");
        if (c.Kind != SchoolDayKind::OutsideYear) A1Expect(view.NextSchoolDay > view.Day, "the next school day is named");
    }
    A1Expect(closedOn(2042, 6, 7).NextSchoolDay == 7, "Saturday points at Monday");
    A1Expect(closedOn(2043, 6, 1).NextSchoolDay == -1, "no school after the calendar ends");
    {
        const auto missingScience = [&] { auto c = content; c.Streams.erase(c.Streams.begin() + 2); return c; }();
        A1Expect(BuildSchoolDayView(missingScience, SchoolSessionsState{}, NewDayLoopState(600), cats).Issue == "school.day.no_timetable", "unknown stream is reported, not guessed");
    }
    std::cout << "SchoolLessonContentTests passed: " << A1Assertions << " assertions, " << content.Streams.size() << " streams, "
        << content.Text.size() << " text keys\n";
    return 0;
}
