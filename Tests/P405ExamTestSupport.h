#pragma once

#include "P402ExamTestSupport.h"
#include "TowerRunnerContent.h"

#include <tuple>

struct P405World
{
    const P402World& Base;
    PerfectWorld::TowerRunnerCatalogue Runners;
    PerfectWorld::SchoolFollowupContent Followups;

    P405World(const P402World& base, const PerfectWorld::ContentDb& db) : Base(base)
    {
        auto runners = PerfectWorld::LoadTowerRunners(db);
        auto followups = PerfectWorld::LoadSchoolFollowupContent(db);
        P2CExpect(runners.Ok() && followups.Ok() && followups.Catalogue.Actions.size() == 31,
            "real runners and all authored follow-ups load");
        Runners = std::move(runners.Catalogue);
        Followups = std::move(followups.Catalogue);
    }

    [[nodiscard]] PerfectWorld::WorldSim Sim(PerfectWorld::WorldState state) const
    {
        return PerfectWorld::WorldSim(std::move(state), PerfectWorld::MaxEventsPerAdvance,
            Base.W.School.Cats.Day, {}, Base.W.School.School(), Base.W.Bind(), {&Runners, &Followups});
    }
    [[nodiscard]] PerfectWorld::ReplayOptions Replay() const
    {
        auto options = Base.Replay(&Runners);
        options.Examination.Followups = &Followups;
        return options;
    }
};

inline const PerfectWorld::TowerRunnerScenario& P405Active(
    const PerfectWorld::WorldSim& sim, const PerfectWorld::TowerRunnerCatalogue& runners)
{
    const auto& active = sim.State().Examination.Active;
    P2CExpect(active.has_value(), "active real scenario");
    const auto found = std::find_if(runners.Scenarios.begin(), runners.Scenarios.end(),
        [&](const auto& scenario) { return scenario.Id == active->ScenarioId; });
    P2CExpect(found != runners.Scenarios.end(), "stored scenario identity resolves");
    return *found;
}

inline void P405Beat(PerfectWorld::WorldSim& sim, const PerfectWorld::TowerRunnerCatalogue& runners, bool mistake = false)
{
    const auto& beat = P405Active(sim, runners).Beats[sim.ExaminationView().Beat];
    const auto chosen = std::find_if(beat.Choices.begin(), beat.Choices.end(), [&](const auto& choice) {
        return !choice.P405CombatOnly && (mistake ? choice.Strain == 4 :
            choice.Strain == 0 && choice.DisciplineMilli == 1000 && choice.CommonJudgmentMilli == 1000);
    });
    P2CExpect(chosen != beat.Choices.end(), "authored non-combat action exists");
    P402Choose(sim, chosen->Id, "real-content beat accepted");
}

inline void P405Floor(PerfectWorld::WorldSim& sim, const PerfectWorld::TowerRunnerCatalogue& runners,
    std::string_view route, bool mistakes = false)
{
    const auto* definition = PerfectWorld::FindTowerRoute(route);
    S04Accept(sim, P402Cmd(PerfectWorld::TowerSessionAction::OpenRoute, std::string(route), definition->Method),
        "real route opens with its non-combat method");
    for (std::size_t guard = 0; sim.State().Examination.Active && guard < PerfectWorld::TowerRunnerBeatLimit; ++guard)
        P405Beat(sim, runners, mistakes);
    P2CExpect(!sim.State().Examination.Active, "real floor reached its boundary");
}

inline void P405Next(const PerfectWorld::P402ExamFinish& result, std::string_view outcome)
{
    P2CExpect(result.Ok() && result.Outcome == outcome && !result.Reasons.empty() &&
        std::any_of(result.NextSteps.begin(), result.NextSteps.end(), [&](const auto& action) {
            return action.Outcome == outcome && action.Kind == "appointment" && !action.AppointmentId.empty();
        }) && std::any_of(result.NextSteps.begin(), result.NextSteps.end(),
            [](const auto& action) { return !action.OfferId.empty(); }),
        "each outcome has its authored appointment and an eligible offer");
}

inline auto P405ActionFields(const PerfectWorld::SchoolFollowupAction& action)
{
    return std::tie(action.Id, action.Outcome, action.OfferId, action.AppointmentId, action.Kind,
        action.TitleTextKey, action.BriefTextKey, action.Destination, action.DayOffset, action.Minute, action.DurationMinutes);
}

inline void P405Same(const PerfectWorld::P402ExamFinish& a, const PerfectWorld::P402ExamFinish& b)
{
    P2CExpect(a.Issue.Code == b.Issue.Code && a.Issue.Detail == b.Issue.Detail && a.Participation == b.Participation &&
        a.Score == b.Score && a.Outcome == b.Outcome && a.Attempts == b.Attempts && a.Active == b.Active &&
        a.Reasons == b.Reasons && a.NextSteps.size() == b.NextSteps.size(), "entire result repeats after reload/replay");
    for (std::size_t i = 0; i < a.NextSteps.size(); ++i)
        P2CExpect(P405ActionFields(a.NextSteps[i]) == P405ActionFields(b.NextSteps[i]), "all next-step fields repeat");
}
