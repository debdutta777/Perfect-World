#include "SchoolSessions.h"

#include "P2CTestSupport.h"
#include "SchoolLessonContent.h"

#include <set>

using namespace PerfectWorld;
namespace
{
template <typename Catalogue, std::size_t N>
std::vector<std::string> S02MatchingIds(const Catalogue& catalogue, const SchoolLessonBinding& binding,
    const std::array<std::string_view, N>& roleNames)
{
    const auto* family = catalogue.FindFamily(binding.FamilyId);
    P2CExpect(family != nullptr, "authored lesson family exists");
    std::vector<std::string> ids;
    for (const auto& def : family->Cases)
        if (def.Subject == binding.FamilySubject &&
            (binding.Role.empty() || ModelNameOf(roleNames, def.Role) == binding.Role)) ids.push_back(def.Id);
    return ids;
}
std::vector<std::string> S02LessonIds(const P2CFixture& f, const SchoolLessonBinding& binding)
{
    switch (binding.Structure)
    {
    case SchoolStructure::Model: return S02MatchingIds(f.Model, binding, ModelRoleNames);
    case SchoolStructure::Explain: return S02MatchingIds(f.Explain, binding, ExplainRoleNames);
    case SchoolStructure::Investigate: return S02MatchingIds(f.Investigate, binding, InvestigateRoleNames);
    case SchoolStructure::Coordinate: return S02MatchingIds(f.Coordinate, binding, CoordinateRoleNames);
    }
    return {};
}
std::string S02ActiveCase(const SchoolSessionsState& state)
{
    switch (state.Active->Binding.Structure)
    {
    case SchoolStructure::Model: return state.Model.Exposure.CaseId;
    case SchoolStructure::Explain: return state.Explain.Exposure.CaseId;
    case SchoolStructure::Investigate: return state.Investigate.Exposure.CaseId;
    case SchoolStructure::Coordinate: return state.Coordinate.Exposure.CaseId;
    }
    return {};
}
void S02BoundLessons(const P2CFixture& f)
{
    const auto decoded = DecodeSchoolLessonContent(f.Content);
    P2CExpect(decoded.Ok(), "A1 still guarantees every authored binding has a matching case");
    const auto& content = decoded.Catalogue;
    std::size_t lessonCount = 0;
    std::set<SchoolStructure> structures;
    for (const auto& stream : content.Streams)
    {
        const auto cats = f.Cats(stream.Stream);
        for (const auto& entry : stream.Lessons)
        {
            ++lessonCount;
            structures.insert(entry.Binding.Structure);
            P2CExpect(entry.Binding.FamilySubject == entry.FamilySubject && entry.Binding.Role == entry.Role,
                "decoded constraints reach the command binding");
            const auto goals = SchoolLessonGoalOptions(content, entry.Binding.Structure);
            P2CExpect(!goals.empty(), "authored lesson has goals");
            for (const bool constrainRole : {true, false})
            {
                auto binding = entry.Binding;
                if (!constrainRole) binding.Role.clear();
                const auto eligible = S02LessonIds(f, binding);
                P2CExpect(!eligible.empty(), "each binding has an eligible subject and role pair");
                std::set<std::string> drawn;
                std::vector<std::string> prior;
                SchoolSessionsState state;
                std::int64_t calendarDay = 0;
                for (std::uint64_t seed = 0; seed < 128; ++seed)
                {
                    while (!CalendarHasOrdinaryPeriods(*cats.Day.Calendar, calendarDay)) ++calendarDay;
                    auto day = P2CDayAtBell(cats, calendarDay++, binding.Period);
                    auto repeated = state;
                    auto repeatedDay = day;
                    const auto opened = OpenSchoolLesson(state, day, cats, binding,
                        SchoolLessonMode::Foreground, goals.front().Goal, 100000, seed);
                    const auto same = OpenSchoolLesson(repeated, repeatedDay, cats, binding,
                        SchoolLessonMode::Foreground, goals.front().Goal, 100000, seed);
                    P2CExpect(opened.Ok() && same.Ok() && state == repeated && day == repeatedDay,
                        "same seed, instant and history produce identical bound lesson state");
                    const auto id = S02ActiveCase(state);
                    P2CExpect(std::binary_search(eligible.begin(), eligible.end(), id),
                        "lesson draws only its subject and any constrained role across 128 seeds");
                    if (drawn.size() < eligible.size())
                        P2CExpect(!drawn.contains(id), "matching unseen cases precede repeats");
                    const auto window = std::min({StructureRecentKept, eligible.size() - 1, prior.size()});
                    P2CExpect(std::find(prior.end() - static_cast<std::ptrdiff_t>(window), prior.end(), id) == prior.end(),
                        "recent matching cases are avoided when another matching case is available");
                    drawn.insert(id);
                    prior.push_back(id);
                    P2CPlayLesson(state, cats, opened.Session);
                    P2CExpect(CloseSchoolLesson(state, day, cats, opened.Session).Ok(),
                        "bound case completes through the normal runner and history");
                }
                P2CExpect(drawn.size() == eligible.size(), "unseen preference reaches every eligible case");
            }
            for (const bool missingSubject : {true, false})
            {
                auto binding = entry.Binding;
                if (missingSubject) binding.FamilySubject = "s02_missing_subject";
                else binding.Role = "s02_missing_role";
                SchoolSessionsState state;
                auto day = P2CDayAtBell(cats, 0, binding.Period);
                const auto before = state;
                const auto beforeDay = day;
                const auto result = OpenSchoolLesson(state, day, cats, binding,
                    SchoolLessonMode::Foreground, goals.front().Goal, 100000, 42);
                P2CExpect(!result.Ok() && result.Issue.Code == "school.session.no_matching_case" &&
                    result.Issue.Detail.find(binding.FamilySubject) != std::string::npos &&
                    result.Issue.Detail.find(binding.Role) != std::string::npos && state == before && day == beforeDay,
                    "unmatched subject or role refuses clearly without changing session, time or attendance");
            }
        }
    }
    P2CExpect(lessonCount == 18 && structures.size() == 4, "all 18 authored lessons and all four runners exercised");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root argument");
    const P2CFixture f(argv[1]);
    S02BoundLessons(f);
    for (const auto stream : {"science", "commerce", "humanities"})
    {
        const auto cats = f.Cats(stream);
        const auto bindings = f.Bindings(stream);
        SchoolSessionsState state;
        auto day = P2CDayAtBell(cats);
        for (std::int32_t p = 0; p < 3; ++p)
        {
            const auto target = CalendarDayStartInstant(0) + P2CBellMinute(*cats.Day.School, p);
            if (day.Now < target)
            {
                DayCommand pass; pass.Kind = DayCommandKind::Pass; pass.Minutes = target - day.Now;
                P2CExpect(ApplyDayCommand(day, cats.Day, pass).Ok(), "passing period");
            }
            const auto before = state;
            const auto beforeDay = day;
            const auto opened = OpenSchoolLesson(state, day, cats, bindings[p], SchoolLessonMode::Foreground, ModelGoal::UnderstandModel, 100000, 12);
            if (p == 2)
            {
                P2CExpect(!opened.Ok() && opened.Issue.Code == "school.session.foreground_limit" && state == before && day == beforeDay,
                    "third foreground refuses atomically for all streams");
                break;
            }
            P2CExpect(opened.Ok(), "foreground opens");
            if (p == 0)
            {
                const auto close = CloseSchoolLesson(state, day, cats, opened.Session, true, 1);
                P2CExpect(close.Ok() && state.Periods.back().Cancelled && !state.Periods.back().Completed, "cancel consumes foreground admission");
                P2CExpect(std::none_of(close.Events.begin(), close.Events.end(), [](const auto& e) { return e.Kind == DayLoopEventKind::Completed; }), "cancel has no learning");
            }
            else
            {
                P2CPlayLesson(state, cats, opened.Session);
                // Value copy models the exact state boundary that the codec must preserve.
                auto reload = state; auto reloadDay = day;
                const auto close = CloseSchoolLesson(reload, reloadDay, cats, opened.Session);
                P2CExpect(close.Ok() && close.Events.size() == 1 && !close.Events.front().Credited.empty(), "close emits bounded learning once");
                const auto saved = reload; const auto savedDay = reloadDay;
                P2CExpect(!CloseSchoolLesson(reload, reloadDay, cats, opened.Session).Ok() && reload == saved && reloadDay == savedDay, "duplicate close is atomic");
                state = std::move(reload); day = std::move(reloadDay);
            }
        }
        auto sunday = NewDayLoopState(CalendarDayStartInstant(6) + 600);
        P2CExpect(QuerySchoolPeriod({}, sunday, bindings.front(), cats).Issue.Code == "school.session.closed_day", "weekend unavailable reason");
        P2CExpect(!QuerySchoolPeriod({}, NewDayLoopState(0), bindings.front(), cats).Issue.Ok(), "outside bell cannot fabricate attendance");
    }
    const auto cats = f.Cats();
    for (const auto kind : {SchoolStructure::Model, SchoolStructure::Explain, SchoolStructure::Investigate, SchoolStructure::Coordinate})
    {
        auto binding = f.Bindings().front(); binding.Structure = kind;
        SchoolLessonGoal goal = ModelGoal::UnderstandModel;
        if (kind == SchoolStructure::Explain) { binding.FamilyId = f.Explain.Families.front().Id; goal = ExplainGoal::ArgueClaim; }
        if (kind == SchoolStructure::Investigate) { binding.FamilyId = f.Investigate.Families.front().Id; goal = InvestigateGoal::Inspect; }
        if (kind == SchoolStructure::Coordinate) { binding.FamilyId = f.Coordinate.Families.front().Id; goal = CoordinateGoal::OrganiseWork; }
        SchoolSessionsState state;
        std::int64_t calendarDay = 0;
        // Forty actual foreground exposures per family; no synthesized historical mastery.
        for (int exposure = 0; exposure < 40; ++exposure)
        {
            while (!CalendarHasOrdinaryPeriods(*cats.Day.Calendar, calendarDay)) ++calendarDay;
            auto day = P2CDayAtBell(cats, calendarDay++);
            const auto opened = OpenSchoolLesson(state, day, cats, binding, SchoolLessonMode::Foreground, goal, 100000, 17);
            P2CExpect(opened.Ok(), "forty-exposure admission");
            P2CPlayLesson(state, cats, opened.Session);
            P2CExpect(CloseSchoolLesson(state, day, cats, opened.Session).Ok(), "forty-exposure close");
        }
        while (!CalendarHasOrdinaryPeriods(*cats.Day.Calendar, calendarDay)) ++calendarDay;
        auto routineDay = P2CDayAtBell(cats, calendarDay);
        auto foregroundDay = routineDay;
        auto foregroundState = state;
        const auto routine = OpenSchoolLesson(state, routineDay, cats, binding, SchoolLessonMode::Routine, goal, 100000, 17);
        P2CExpect(routine.Ok(), "mastery enables routine");
        const auto manual = OpenSchoolLesson(foregroundState, foregroundDay, cats, binding, SchoolLessonMode::Foreground, goal, 100000, 17);
        P2CExpect(manual.Ok(), "manual parity admission"); P2CPlayLesson(foregroundState, cats, manual.Session);
        const auto r = CloseSchoolLesson(state, routineDay, cats, routine.Session);
        const auto m = CloseSchoolLesson(foregroundState, foregroundDay, cats, manual.Session);
        P2CExpect(r.Ok() && m.Ok() && routineDay == foregroundDay && r.Events.front().Credited == m.Events.front().Credited,
            "routine and careful foreground have identical normal learning/condition/time");
    }
    std::cout << "P2C SchoolSessions passed\n";
}
