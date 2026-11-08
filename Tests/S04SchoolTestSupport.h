#pragma once

// S04: a WorldSim bound to the real school content (science stream) and helpers that drive it only through commands.

#include "P2CTestSupport.h"
#include "SchoolLessonContent.h"
#include "WorldHash.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <string>
#include <utility>
#include <vector>

struct S04World
{
    const P2CFixture& F;
    PerfectWorld::SchoolLessonContent Lessons;
    PerfectWorld::SchoolSessionCatalogues Cats;
    /** One life (its household funds the commerce slice the save writer authenticates). */
    PerfectWorld::CharacterCreator Creator;

    explicit S04World(const P2CFixture& f) : F(f), Cats(f.Cats("science"))
    {
        auto decoded = PerfectWorld::DecodeSchoolLessonContent(f.Content);
        P2CExpect(decoded.Ok(), "the real A1 lesson catalogue decodes");
        Lessons = std::move(decoded.Catalogue);
        P2CExpect(Creator.ApplyPreset("balanced_scholar").Succeeded() &&
                      Creator.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded() && Creator.StartLife().Succeeded(),
                  "a creation record");
    }
    S04World(const S04World&) = delete;
    S04World& operator=(const S04World&) = delete;

    [[nodiscard]] PerfectWorld::WorldSchoolCatalogues School() const { return {Cats.Structures, &Lessons, &F.Papers}; }
    [[nodiscard]] PerfectWorld::WorldSim Sim(std::uint64_t seed, std::size_t cap = PerfectWorld::MaxEventsPerAdvance) const
    {
        return PerfectWorld::WorldSim(seed, Creator, cap, Cats.Day, {}, School());
    }
    [[nodiscard]] PerfectWorld::WorldSaveData Save(const PerfectWorld::WorldState& world) const
    {
        PerfectWorld::WorldSaveData data;
        data.Creation = Creator.CaptureSave();
        data.World = world;
        data.Player.ZoneId = "zone.school.classroom";
        return data;
    }
    [[nodiscard]] PerfectWorld::WorldSim Continue(PerfectWorld::WorldState state, std::size_t cap = PerfectWorld::MaxEventsPerAdvance) const
    {
        return PerfectWorld::WorldSim(std::move(state), cap, Cats.Day, {}, School());
    }
    [[nodiscard]] const PerfectWorld::SchoolLessonBinding& Binding(std::int32_t period) const
    {
        const auto* entry = PerfectWorld::FindSchoolLesson(Lessons, "science", period);
        P2CExpect(entry != nullptr, "the science timetable has this period");
        return entry->Binding;
    }
    [[nodiscard]] PerfectWorld::SchoolWorkBrief Brief(std::string id, PerfectWorld::GameInstant now) const
    {
        return {std::move(id), F.Papers.Papers.front().Id, PerfectWorld::SchoolWorkKind::Homework, now, now + 1440,
                "phase2.test.title", "phase2.test.brief", "phase2.test.criteria", "phase2.test.recovery"};
    }
};

inline PerfectWorld::CommandResult S04Send(PerfectWorld::WorldSim& sim, PerfectWorld::CommandPayload payload,
                                          PerfectWorld::CommandSource source = PerfectWorld::CommandSource::Player)
{
    return sim.Submit({source, std::move(payload)});
}

inline PerfectWorld::CommandResult S04Accept(PerfectWorld::WorldSim& sim, PerfectWorld::CommandPayload payload, std::string_view what,
                                            PerfectWorld::CommandSource source = PerfectWorld::CommandSource::Player)
{
    auto result = S04Send(sim, std::move(payload), source);
    if (!result.Accepted)
        std::cerr << "refused " << result.Why.Code << " (" << result.Why.Message << ")\n";
    P2CExpect(result.Accepted, what);
    return result;
}

/** Refused with exactly `code`, and the world's hash, sequence and clock are unchanged. */
inline void S04Refused(PerfectWorld::WorldSim& sim, PerfectWorld::CommandPayload payload, std::string_view code, std::string_view what)
{
    const std::uint64_t before = PerfectWorld::HashWorldState(sim.State());
    const auto result = S04Send(sim, std::move(payload));
    if (result.Accepted || result.Why.Code != code)
        std::cerr << "expected " << code << " got " << (result.Accepted ? std::string("accepted") : result.Why.Code) << '\n';
    P2CExpect(!result.Accepted && result.Why.Code == code && PerfectWorld::HashWorldState(sim.State()) == before, what);
}

inline void S04Pass(PerfectWorld::WorldSim& sim, std::int64_t minutes)
{
    using namespace PerfectWorld;
    while (minutes > 0)
    {
        DayCommand pass;
        pass.Kind = DayCommandKind::Pass;
        pass.Minutes = std::min<std::int64_t>(minutes, DayLoopMaxPassMinutes);
        S04Accept(sim, WorldDayCommand{pass}, "time passes through the day loop");
        minutes -= pass.Minutes;
    }
}

inline void S04PassTo(PerfectWorld::WorldSim& sim, std::int32_t minuteOfDay)
{
    const auto now = sim.State().Clock.Now();
    const auto target = PerfectWorld::GameClock::StartOfDay(now) + minuteOfDay;
    if (target > now)
        S04Pass(sim, target - now);
}

/** Today's plan, then the walk to the gate at the first bell (on time). */
inline void S04Morning(PerfectWorld::WorldSim& sim, const PerfectWorld::SchoolSessionCatalogues& cats)
{
    using namespace PerfectWorld;
    S04Accept(sim, SchoolPlanDay{}, "the morning plan");
    S04PassTo(sim, BellFirstBell(*cats.Day.School));
    S04Accept(sim, SchoolArrive{}, "arrival at the school gate");
}

inline PerfectWorld::SchoolLessonGoal S04Goal(PerfectWorld::SchoolStructure structure)
{
    using namespace PerfectWorld;
    switch (structure)
    {
    case SchoolStructure::Explain: return ExplainGoal::ArgueClaim;
    case SchoolStructure::Investigate: return InvestigateGoal::Inspect;
    case SchoolStructure::Coordinate: return CoordinateGoal::OrganiseWork;
    default: return ModelGoal::UnderstandModel;
    }
}

inline PerfectWorld::SchoolLessonAction S04Standard(const PerfectWorld::SchoolSessionsState& s, const PerfectWorld::SchoolSessionCatalogues& cats)
{
    using namespace PerfectWorld;
    switch (s.Active->Binding.Structure)
    {
    case SchoolStructure::Explain: return ExplainStandardAction(*cats.Structures.Explain->FindCase(s.Explain.Exposure.CaseId), s.Explain.Exposure);
    case SchoolStructure::Investigate:
        return InvestigateStandardAction(*cats.Structures.Investigate->FindCase(s.Investigate.Exposure.CaseId), s.Investigate.Exposure);
    case SchoolStructure::Coordinate:
        return CoordinateStandardAction(*cats.Structures.Coordinate->FindCase(s.Coordinate.Exposure.CaseId), s.Coordinate.Exposure);
    default: return ModelStandardAction(*cats.Structures.Model->FindCase(s.Model.Exposure.CaseId), s.Model.Exposure);
    }
}

/** One quoted runner action through school.lesson.choose; the core's quote equals the charge and no world time passes. */
inline void S04ChooseOnce(PerfectWorld::WorldSim& sim, const PerfectWorld::SchoolSessionCatalogues& cats)
{
    using namespace PerfectWorld;
    const auto& s = sim.State().SchoolSessions;
    const std::uint64_t session = s.Active->Session;
    const SchoolLessonAction action = S04Standard(s, cats);
    const SchoolLessonQuote quote = QuoteSchoolLessonAction(s, cats, session, action);
    const GameInstant before = sim.State().Clock.Now();
    S04Accept(sim, SchoolLessonChoose{session, action}, "a lesson action is accepted");
    P2CExpect(quote.Issue.Ok() && sim.LastSchoolLessonResult() && sim.LastSchoolLessonResult()->ChargedUnits == quote.Units &&
                  sim.State().Clock.Now() == before,
              "the quote equals the charge and no world time passes");
}

inline void S04PlayLesson(PerfectWorld::WorldSim& sim, const PerfectWorld::SchoolSessionCatalogues& cats)
{
    for (int guard = 0; guard < 32 && sim.State().SchoolSessions.Active && !sim.State().SchoolSessions.Active->ReadyToClose; ++guard)
        S04ChooseOnce(sim, cats);
    P2CExpect(sim.State().SchoolSessions.Active && sim.State().SchoolSessions.Active->ReadyToClose, "the lesson finishes");
}

/** The Completed day-loop events a result published, in order. */
inline std::vector<PerfectWorld::DayLoopEvent> S04Completions(const PerfectWorld::CommandResult& result)
{
    std::vector<PerfectWorld::DayLoopEvent> out;
    for (const auto& event : result.Events)
        if (const auto* day = std::get_if<PerfectWorld::DayLoopEvent>(&event.Payload); day && day->Kind == PerfectWorld::DayLoopEventKind::Completed)
            out.push_back(*day);
    return out;
}
