#pragma once

#include "P405ExamTestSupport.h"
#include "TowerScenarioSelection.h"

#include <set>

inline void P405DPopulation(const P2CFixture& fixture, const PerfectWorld::TowerRunnerCatalogue& runners)
{
    using namespace PerfectWorld;
    std::set<std::string> reached;
    std::set<std::uint64_t> seeds;
    for (const auto& target : runners.Scenarios)
    {
        // Find a bounded witness for this variant at the first examination sitting.
        std::uint64_t seed = 0;
        for (; seed < 64; ++seed)
            if (P404BSelectRouteScenario(runners.Scenarios, target.RouteId, seed, 0)->Id == target.Id) break;
        P2CExpect(seed < 64, "each authored variant has a population seed at sitting zero");
        seeds.insert(seed);
        const P402World base(fixture, seed);
        const P405World world(base, fixture.Content);
        WorldSim sim = world.Sim(base.Morning);
        const auto rng = sim.State().Rng;
        S04Accept(sim, P402Cmd(TowerSessionAction::Enter), "seeded life sits");
        const auto* route = FindTowerRoute(target.RouteId);
        for (std::int32_t floor = 1; floor < route->Floor; ++floor)
            P405Floor(sim, runners, P402Climb[floor - 1]);
        WorldSim beforeOpen = world.Sim(P402Reload(base, sim.State(), "seeded floor boundary round trip"));
        const auto open = P402Cmd(TowerSessionAction::OpenRoute, target.RouteId, route->Method);
        S04Accept(sim, open, "seeded route opens");
        S04Accept(beforeOpen, open, "same route opens after recovery");
        P2CExpect(P405Active(sim, runners).Id == target.Id &&
            P405Active(beforeOpen, runners).Id == target.Id &&
            HashWorldState(sim.State()) == HashWorldState(beforeOpen.State()),
            "saved seed selects the same variant before and after opening");
        reached.insert(P405Active(sim, runners).Id);
        P405Beat(sim, runners);
        const auto saved = P402Reload(base, sim.State(), "seeded mid-floor round trip");
        P2CExpect(saved.WorldSeed == seed && saved.Rng == rng && saved.Examination.Active &&
            saved.Examination.Active->ScenarioId == target.Id &&
            ExamStateMatchesRunners(saved.Examination, runners, saved.WorldSeed),
            "mid-floor recovery preserves selection and its choice provenance");
        WorldSim resumed = world.Sim(saved);
        for (auto* current : {&sim, &resumed})
        {
            while (current->State().Examination.Active) P405Beat(*current, runners);
            for (std::int32_t floor = route->Floor + 1; floor <= TowerFloorCount; ++floor)
                P405Floor(*current, runners, P402Climb[floor - 1]);
        }
        P2CExpect(sim.State().Examination.Participation == TowerParticipation::Completed &&
            HashWorldState(sim.State()) == HashWorldState(resumed.State()) &&
            ExamStateMatchesRunners(sim.State().Examination, runners, seed),
            "all later floors resume identically with the same sitting ordinal");
        P405Same(sim.ExaminationResult(), resumed.ExaminationResult());
        const auto replay = ReplayCommandLogFromSnapshot(base.Morning, sim.Log(), world.Replay());
        P2CExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()),
            "population life's replay has identical selection and final state");
        const WorldSim terminal = world.Sim(P402Reload(base, sim.State(), "seeded terminal round trip"));
        P405Same(sim.ExaminationResult(), terminal.ExaminationResult());
    }
    P2CExpect(reached.size() == 15 && reached.size() == runners.Scenarios.size() && seeds.size() > 1,
        "different population seeds reach every one of the fifteen variants through WorldSim");
}
