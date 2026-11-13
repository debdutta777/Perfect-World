#pragma once

// P3-06: a WorldSim bound to the real school content AND the real homework brief catalogue (science stream), plus helpers
// that read the science briefs in their issue order and hand a paper in only through commands.

#include "LearningCredits.h"
#include "S04SchoolTestSupport.h"
#include "SchoolWorkBriefContent.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct P36World
{
    const P2CFixture& F;
    S04World School;
    PerfectWorld::SchoolWorkBriefCatalogue Briefs;

    explicit P36World(const P2CFixture& f) : F(f), School(f)
    {
        auto decoded = PerfectWorld::DecodeSchoolWorkBriefContent(f.Content);
        P2CExpect(decoded.Ok() && !decoded.Catalogue.Briefs.empty(), "the real homework brief catalogue decodes");
        Briefs = std::move(decoded.Catalogue);
    }
    P36World(const P36World&) = delete;
    P36World& operator=(const P36World&) = delete;

    /** The school catalogues the sim already takes, with the brief catalogue bound through them. */
    [[nodiscard]] PerfectWorld::WorldSchoolCatalogues Lessons() const
    {
        PerfectWorld::WorldSchoolCatalogues lessons = School.School();
        lessons.WorkBriefs = &Briefs;
        return lessons;
    }
    [[nodiscard]] PerfectWorld::WorldSim Sim(std::uint64_t seed, std::size_t cap = PerfectWorld::MaxEventsPerAdvance) const
    {
        return PerfectWorld::WorldSim(seed, School.Creator, cap, School.Cats.Day, {}, Lessons());
    }
    [[nodiscard]] PerfectWorld::WorldSim Continue(PerfectWorld::WorldState state, std::size_t cap = PerfectWorld::MaxEventsPerAdvance) const
    {
        return PerfectWorld::WorldSim(std::move(state), cap, School.Cats.Day, {}, Lessons());
    }
    [[nodiscard]] PerfectWorld::ReplayOptions Replay() const
    {
        PerfectWorld::ReplayOptions options;
        options.School = School.Cats.Day;
        options.Lessons = Lessons();
        return options;
    }
    /** This school's (science) briefs, in issue order. */
    [[nodiscard]] std::vector<PerfectWorld::SchoolWorkBrief> Science() const
    {
        std::vector<PerfectWorld::SchoolWorkBrief> out;
        for (const auto& def : Briefs.Briefs)
            if (def.Stream == "science")
                out.push_back(def.Brief);
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
            return a.AvailableAt != b.AvailableAt ? a.AvailableAt < b.AvailableAt : a.Id < b.Id;
        });
        P2CExpect(out.size() >= 3, "the science stream has several briefs");
        return out;
    }
};

/** Moves a sim to exactly `at` through accepted (System) time. */
inline void P36To(PerfectWorld::WorldSim& sim, PerfectWorld::GameInstant at)
{
    using namespace PerfectWorld;
    if (at > sim.State().Clock.Now())
        S04Accept(sim, AdvanceTime{at - sim.State().Clock.Now()}, "time reaches the instant", CommandSource::System);
    P2CExpect(sim.State().Clock.Now() == at, "the sim stands at the instant");
}

inline const PerfectWorld::SchoolWorkRecord* P36Record(const PerfectWorld::WorldState& state, std::string_view id)
{
    for (const auto& record : state.SchoolWork.Records)
        if (record.Brief.Id == id)
            return &record;
    return nullptr;
}

inline PerfectWorld::SchoolWorkCommand P36Work(std::string id, PerfectWorld::SchoolWorkActionKind kind)
{
    PerfectWorld::SchoolWorkCommand command;
    command.WorkId = std::move(id);
    command.Kind = kind;
    return command;
}

/**
 * Begin, survey, then for each group allocate exactly what the core quotes for a worked answer and answer it, while the
 * paper's units last; all through commands. At least one group is answered. The paper is left open.
 */
inline void P36AnswerPaper(PerfectWorld::WorldSim& sim, const std::string& id)
{
    using namespace PerfectWorld;
    S04Accept(sim, P36Work(id, SchoolWorkActionKind::Begin), "the paper begins");
    S04Accept(sim, P36Work(id, SchoolWorkActionKind::Survey), "the paper is surveyed");
    std::size_t answered = 0;
    for (std::size_t group = 0; group < AssessmentGroupCount; ++group)
    {
        auto answer = P36Work(id, SchoolWorkActionKind::Answer);
        answer.Answer = {group, AssessmentMode::Work, true, false};
        const AssessmentQuote quote = QuoteSchoolWorkAnswer(sim.State().SchoolWork, id, answer.Answer);
        auto allocate = P36Work(id, SchoolWorkActionKind::Allocate);
        allocate.Group = group;
        allocate.Units = quote.Units;
        if (quote.Units <= 0 || !S04Send(sim, allocate).Accepted)
            break;
        S04Accept(sim, answer, "a group is answered at its quoted price");
        ++answered;
    }
    P2CExpect(answered > 0, "the paper has answered work");
}

/** The next 06:00 strictly after `now`. */
inline PerfectWorld::GameInstant P36NextMorning(PerfectWorld::GameInstant now)
{
    const PerfectWorld::GameInstant today = PerfectWorld::GameClock::StartOfDay(now) + 360;
    return today > now ? today : today + PerfectWorld::MinutesPerDay;
}

/**
 * As a student would (S04's pattern): a standing school order, a first week that stops at its first unplayed lesson, that
 * day's lessons attended through the commands. Returns the week plan the later, known weeks resolve with.
 */
inline PerfectWorld::WeekPlan P36KnownWeek(const P36World& w, PerfectWorld::WorldSim& sim)
{
    using namespace PerfectWorld;
    SchoolStandingOrder order;
    order.Id = "order.school";
    S04Accept(sim, SchoolWeekOrder{order, false}, "a standing school order");
    WeekPlan base;
    base.Days.resize(7);
    WeekEntry endWeek;
    endWeek.Kind = WeekEntryKind::Spend;
    endWeek.StartMinute = 1439;
    endWeek.Minutes = 1;
    base.Days.back().Entries.push_back(endWeek);
    S04Accept(sim, SchoolWeekResolve{base}, "an unplayed week resolves up to its first lesson");
    P2CExpect(sim.LastSchoolWeekResult()->Stop == WeekStop::FirstNeedsPlay, "the first week stops at its first lesson");
    if (!sim.State().DayLoop.Planned)
        S04Morning(sim, w.School.Cats);
    else if (sim.State().DayLoop.SchoolUntil <= sim.State().Clock.Now())
        S04Accept(sim, SchoolArrive{}, "arrival");
    for (std::int32_t period = 0; period < BellPeriodCount(*w.School.Cats.Day.School); ++period)
    {
        S04PassTo(sim, P2CBellMinute(*w.School.Cats.Day.School, period));
        S04Accept(sim, SchoolLessonOpen{w.School.Binding(period).Id, SchoolLessonMode::Attend, {}}, "a first lesson opens");
        S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "a first lesson closes");
    }
    return base;
}

/** Sum of every latent skill level (milli-XP): what one credit batch must move by exactly its Credited total. */
inline std::int64_t P36SkillTotal(const PerfectWorld::WorldState& state)
{
    std::int64_t total = 0;
    for (const auto& skill : state.Awakening.Stats.Skills)
        total += skill.LevelMilli;
    return total;
}

inline std::int64_t P36Credited(const std::vector<PerfectWorld::DayLoopEvent>& events)
{
    std::int64_t total = 0;
    for (const auto& event : events)
        for (const auto& credit : event.Credited)
            total += credit.MilliXp;
    return total;
}
