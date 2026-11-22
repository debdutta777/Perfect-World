#pragma once

#include "SchoolSessions.h"
#include "SchoolWeek.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

inline void P2CExpect(bool condition, std::string_view message)
{
    if (!condition) { std::cerr << "P2C failure: " << message << '\n'; std::exit(1); }
}
struct P2CFixture
{
    PerfectWorld::ContentDb Content;
    PerfectWorld::ActivityCatalogue Activities;
    PerfectWorld::BellScheduleSet Bells;
    PerfectWorld::SchoolCalendarSet Calendars;
    PerfectWorld::ModelCatalogue Model;
    PerfectWorld::ExplainCatalogue Explain;
    PerfectWorld::InvestigateCatalogue Investigate;
    PerfectWorld::CoordinateCatalogue Coordinate;
    PerfectWorld::AssessmentCatalogue Papers;

    explicit P2CFixture(const char* root)
    {
        namespace fs = std::filesystem;
        using namespace PerfectWorld;
        std::error_code error;
        std::vector<fs::path> paths;
        fs::recursive_directory_iterator it(root, error), end;
        while (!error && it != end)
        {
            if (it->is_regular_file(error) && it->path().extension() == ".json") paths.push_back(it->path());
            it.increment(error);
        }
        P2CExpect(!error, "content tree readable");
        std::sort(paths.begin(), paths.end());
        std::vector<ContentFile> files;
        for (const auto& path : paths)
        {
            std::ifstream input(path, std::ios::binary);
            P2CExpect(input.good(), "content file readable");
            std::ostringstream buffer;
            buffer << input.rdbuf();
            files.push_back({"/" + path.lexically_relative(root).generic_string(), buffer.str()});
        }
        auto content = LoadContent(files);
        for (const auto& issue : content.Issues) std::cerr << issue.File << ": " << issue.Message << '\n';
        P2CExpect(content.Ok(), "real content loaded");
        Content = std::move(content.Db);
        auto activities = LoadActivityCatalogue(Content); P2CExpect(activities.Ok(), "activities loaded"); Activities = std::move(activities.Catalogue);
        auto bells = LoadBellSchedules(Content); P2CExpect(bells.Ok(), "bells loaded"); Bells = std::move(bells.Set);
        auto calendars = LoadSchoolCalendars(Content); P2CExpect(calendars.Ok(), "calendar loaded"); Calendars = std::move(calendars.Set);
        auto model = LoadModelCatalogue(Content); P2CExpect(model.Ok(), "model loaded"); Model = std::move(model.Catalogue);
        auto explain = LoadExplainCatalogue(Content); P2CExpect(explain.Ok(), "explain loaded"); Explain = std::move(explain.Catalogue);
        auto investigate = LoadInvestigateCatalogue(Content); P2CExpect(investigate.Ok(), "investigate loaded"); Investigate = std::move(investigate.Catalogue);
        auto coordinate = LoadCoordinateCatalogue(Content); P2CExpect(coordinate.Ok(), "coordinate loaded"); Coordinate = std::move(coordinate.Catalogue);
        auto papers = LoadAssessmentCatalogue(Content, Cats().Structures); P2CExpect(papers.Ok(), "paper loaded"); Papers = std::move(papers.Catalogue);
    }
    PerfectWorld::SchoolSessionCatalogues Cats(std::string_view stream = "science") const
    {
        const auto bell = std::find_if(Bells.Schedules.begin(), Bells.Schedules.end(),
            [&](const auto& b) { return b.Stream == stream; });
        P2CExpect(bell != Bells.Schedules.end(), "stream bell exists");
        const auto* calendar = Calendars.Find(bell->InstitutionId, bell->StageId);
        P2CExpect(calendar != nullptr, "school calendar exists");
        return {{&Activities, &*bell, calendar}, {&Model, &Explain, &Investigate, &Coordinate}};
    }
    std::vector<PerfectWorld::SchoolLessonBinding> Bindings(std::string_view stream = "science") const
    {
        using namespace PerfectWorld;
        const auto cats = Cats(stream);
        std::vector<SchoolLessonBinding> bindings;
        std::int32_t period = 0;
        for (const auto& block : cats.Day.School->Blocks)
            if (block.Kind == BellBlockKind::Period)
            {
                SchoolLessonBinding b;
                b.Id = "school.lesson.test_" + std::to_string(period);
                b.Stream = stream; b.Subject = block.Subject; b.Period = period++;
                b.FamilyId = Model.Families.front().Id;
                b.TitleTextKey = "phase2.test.title"; b.BriefTextKey = "phase2.test.brief"; b.RoleTextKey = "phase2.test.role";
                bindings.push_back(std::move(b));
            }
        return bindings;
    }
};

inline std::int32_t P2CBellMinute(const PerfectWorld::BellSchedule& school, std::int32_t period)
{
    std::int32_t index = 0;
    for (const auto& block : school.Blocks)
        if (block.Kind == PerfectWorld::BellBlockKind::Period && index++ == period) return block.StartMinute;
    return -1;
}
inline PerfectWorld::DayLoopState P2CDayAtBell(const PerfectWorld::SchoolSessionCatalogues& cats,
    std::int64_t day = 0, std::int32_t period = 0)
{
    using namespace PerfectWorld;
    const auto midnight = CalendarDayStartInstant(day);
    auto state = NewDayLoopState(std::max<GameInstant>(0, midnight + BellFirstBell(*cats.Day.School)));
    DayCommand command;
    command.Kind = DayCommandKind::PlanDay;
    P2CExpect(ApplyDayCommand(state, cats.Day, command).Ok(), "morning planned");
    command.Kind = DayCommandKind::ArriveSchool;
    P2CExpect(ApplyDayCommand(state, cats.Day, command).Ok(), "school arrival");
    const auto target = midnight + P2CBellMinute(*cats.Day.School, period);
    if (target > state.Now)
    {
        command.Kind = DayCommandKind::Pass; command.Minutes = target - state.Now;
        P2CExpect(ApplyDayCommand(state, cats.Day, command).Ok(), "wait to bell");
    }
    return state;
}
inline void P2CPlayLesson(PerfectWorld::SchoolSessionsState& sessions, const PerfectWorld::SchoolSessionCatalogues& cats,
    std::uint64_t session)
{
    using namespace PerfectWorld;
    for (int guard = 0; guard < 32 && sessions.Active && !sessions.Active->ReadyToClose; ++guard)
    {
        SchoolLessonAction action;
        switch (sessions.Active->Binding.Structure)
        {
        case SchoolStructure::Model: action = ModelStandardAction(*cats.Structures.Model->FindCase(sessions.Model.Exposure.CaseId), sessions.Model.Exposure); break;
        case SchoolStructure::Explain: action = ExplainStandardAction(*cats.Structures.Explain->FindCase(sessions.Explain.Exposure.CaseId), sessions.Explain.Exposure); break;
        case SchoolStructure::Investigate: action = InvestigateStandardAction(*cats.Structures.Investigate->FindCase(sessions.Investigate.Exposure.CaseId), sessions.Investigate.Exposure); break;
        case SchoolStructure::Coordinate: action = CoordinateStandardAction(*cats.Structures.Coordinate->FindCase(sessions.Coordinate.Exposure.CaseId), sessions.Coordinate.Exposure); break;
        }
        const auto quote = QuoteSchoolLessonAction(sessions, cats, session, action);
        const auto applied = ChooseSchoolLesson(sessions, cats, session, action);
        P2CExpect(quote.Issue.Ok() && applied.Ok() && quote.Units == applied.ChargedUnits, "runner quote equals charge");
    }
    P2CExpect(sessions.Active && sessions.Active->ReadyToClose, "lesson finishes without stranding");
}
