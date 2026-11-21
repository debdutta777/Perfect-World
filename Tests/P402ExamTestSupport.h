#pragma once

// P4-02: a routed, awakened life driven only through commands to 05:00 and 06:00 on 2 February 2043, a synthetic runner
// catalogue (P4-01: no production scenario-to-runner adapter exists yet) and helpers for tower.session.apply.

#include "R94AwakeningTestSupport.h"
#include "WorldExam.h"
#include "WorldHash.h"

#include <string>
#include <utility>

/** Every route, `beats` beats; choices: careful (5 min, strain 1, recovery 1, perfect), rush (2 min, strain 4, recovery 0,
 * weak) and slow (60 min, no strain, perfect) for the mission-clock cap. */
inline PerfectWorld::TowerRunnerCatalogue P402RunnerCatalogue(int beats = 3)
{
    using namespace PerfectWorld;
    TowerRunnerCatalogue c;
    for (const auto& route : TowerRoutes())
    {
        TowerRunnerScenario s;
        s.Id = "p402.scenario." + std::string(route.Id);
        s.RouteId = route.Id;
        s.TitleTextKey = "p402.title";
        for (int beat = 0; beat < beats; ++beat)
        {
            TowerRunnerBeat b;
            b.Id = "p402.beat." + std::to_string(beat);
            b.PromptTextKey = "p402.prompt";
            b.Choices = {{"careful", "p402.careful", 5, 1, 1, 1000, 1000}, {"rush", "p402.rush", 2, 4, 0, 200, 200},
                         {"slow", "p402.slow", 60, 0, 0, 1000, 1000}};
            s.Beats.push_back(std::move(b));
        }
        c.Scenarios.push_back(std::move(s));
    }
    return c;
}

inline PerfectWorld::TowerSessionCommand P402Cmd(PerfectWorld::TowerSessionAction kind, std::string route = {},
                                                 PerfectWorld::TowerMethod method = PerfectWorld::TowerMethod::Analysis)
{
    PerfectWorld::TowerSessionCommand command;
    command.Kind = kind;
    command.RouteId = std::move(route);
    command.Method = method;
    return command;
}

struct P402World
{
    R94World W;
    PerfectWorld::TowerRunnerCatalogue Runners = P402RunnerCatalogue();
    PerfectWorld::TowerRunnerCatalogue LongRunners = P402RunnerCatalogue(16);
    /** 05:00 and 06:00 on examination morning: awakened (safe withdrawal), the ten days closed only at 06:00. */
    PerfectWorld::WorldState Before;
    PerfectWorld::WorldState Morning;

    explicit P402World(const P2CFixture& f, std::uint64_t P405DWorldSeed = 0x402) : W(f)
    {
        using namespace PerfectWorld;
        WorldSim sim = Sim(W.Seeded(R94At(2043, 1, 23, 9, 0), P405DWorldSeed));
        S04Accept(sim, TutorialEnter{}, "the compulsory supervised session opens");
        S04Accept(sim, TutorialWithdraw{}, "safe withdrawal grants the Awakening");
        S04Accept(sim, AdvanceTime{CountdownStartInstant() - sim.State().Clock.Now()}, "ordinary life reaches the opening");
        S04Accept(sim, AdvanceTime{CountdownEndInstant() - 60 - sim.State().Clock.Now()}, "the ten days pass to 05:00");
        Before = sim.State();
        S04Accept(sim, AdvanceTime{60}, "the last hour passes to the examination instant");
        Morning = sim.State();
        P2CExpect(Morning.Clock.Now() == CountdownEndInstant() && sim.Countdown() && sim.Countdown()->Closed &&
                      Morning.Awakening.Granted && Morning.Examination == TowerSessionState{},
                  "examination morning: awakened, the countdown closed, the examination undecided");
    }
    P402World(const P402World&) = delete;
    P402World& operator=(const P402World&) = delete;

    [[nodiscard]] PerfectWorld::WorldSim Sim(PerfectWorld::WorldState state, const PerfectWorld::TowerRunnerCatalogue* runners,
                                             std::size_t cap = PerfectWorld::MaxEventsPerAdvance) const
    {
        return PerfectWorld::WorldSim(std::move(state), cap, W.School.Cats.Day, {}, W.School.School(), W.Bind(),
                                      PerfectWorld::P402WorldExamCatalogues{runners});
    }
    [[nodiscard]] PerfectWorld::WorldSim Sim(PerfectWorld::WorldState state) const { return Sim(std::move(state), &Runners); }
    [[nodiscard]] PerfectWorld::ReplayOptions Replay(const PerfectWorld::TowerRunnerCatalogue* runners) const
    {
        PerfectWorld::ReplayOptions options;
        options.School = W.School.Cats.Day;
        options.Lessons = W.School.School();
        options.Awakening = W.Bind();
        options.Examination.Runners = runners;
        return options;
    }
};

/** Choose `choice` at the current beat (runner nonce and beat from the view, as the panel would). */
inline PerfectWorld::CommandResult P402Choose(PerfectWorld::WorldSim& sim, const std::string& choice, std::string_view what)
{
    using namespace PerfectWorld;
    const TowerSessionView view = sim.ExaminationView();
    TowerSessionCommand command = P402Cmd(TowerSessionAction::Choose);
    command.Runner = view.Runner;
    command.Beat = view.Beat;
    command.ChoiceId = choice;
    return S04Accept(sim, command, what);
}

/** Open `route` with its own method and play every beat with `choice`. */
inline void P402Floor(PerfectWorld::WorldSim& sim, std::string_view route, const std::string& choice, int beats = 3)
{
    using namespace PerfectWorld;
    const TowerRouteDefinition* def = FindTowerRoute(route);
    P2CExpect(def != nullptr, "known route");
    S04Accept(sim, P402Cmd(TowerSessionAction::OpenRoute, std::string(route), def->Method), "the route opens");
    for (int beat = 0; beat < beats && sim.State().Examination.Active; ++beat)
        P402Choose(sim, choice, "a beat is played");
}

inline constexpr std::string_view P402Climb[] = {"tower.route.pace", "tower.route.changed_rule", "tower.route.alliance"};

inline std::string P402SaveText(const P402World& w, const PerfectWorld::WorldState& world)
{
    return PerfectWorld::SerializeWorldSave(w.W.Save(world));
}

/** Save, load, demand v13 and a verified hash, and return the loaded world. */
inline PerfectWorld::WorldState P402Reload(const P402World& w, const PerfectWorld::WorldState& world, std::string_view what)
{
    using namespace PerfectWorld;
    const std::string text = P402SaveText(w, world);
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    if (!result.Succeeded())
        std::cerr << "reload: " << result.Message << '\n';
    P2CExpect(!text.empty() && result.Succeeded() && result.HashVerified && result.SourceVersion == 13 &&
                  HashWorldState(loaded.World) == HashWorldState(world) && loaded.World.Examination == world.Examination,
              what);
    return loaded.World;
}
