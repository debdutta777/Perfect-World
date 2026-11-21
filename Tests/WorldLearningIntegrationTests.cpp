// P3-06: the creation seed and learning credits routed through WorldSim. SeedNewLifeLearning is the one creation seeding
// point: it applies the academic foundation's prior preparation once (inside R9's seed, never added twice), never reseeds a
// seeded life, and refuses a world that has already been played, so a load can never reapply it. Lesson closes, paper
// hand-ins and compressed weeks each grow the one latent owner by exactly the post-limit credits they publish, once.
#include "P36HomeworkTestSupport.h"

#include <set>

using namespace PerfectWorld;

namespace
{
std::size_t P36LFoundationIndex()
{
    const auto& traits = CharacterCreator::TraitDefinitions();
    for (std::size_t i = 0; i < traits.size(); ++i)
        if (traits[i].Id == "academic_foundation")
            return i;
    P2CExpect(false, "the academic foundation trait exists");
    return 0;
}

/** A newly created life (no command yet), seeded at its creation through the one seeding point. */
WorldState P36LNewLife(const P36World& w, std::uint64_t seed)
{
    WorldState world = w.Sim(seed).State();
    const auto seeded = SeedNewLifeLearning(world, w.F.Content, w.School.Creator.CaptureSave());
    P2CExpect(seeded.Ok() && seeded.Applied && world.Awakening.Seeded, "creation confirmation seeds the life once");
    return world;
}

/** The published completions credit the owner by exactly their Credited total, each instance consumed exactly once. */
void P36LCreditedOnce(const WorldState& before, const WorldState& after, const std::vector<DayLoopEvent>& published, const char* what)
{
    std::set<std::uint64_t> instances(before.Awakening.AppliedActivityInstances.begin(), before.Awakening.AppliedActivityInstances.end());
    for (const auto& event : published)
        P2CExpect(event.Instance != 0 && instances.insert(event.Instance).second, what);
    P2CExpect(std::vector<std::uint64_t>(instances.begin(), instances.end()) == after.Awakening.AppliedActivityInstances, what);
    P2CExpect(P36SkillTotal(after) - P36SkillTotal(before) == P36Credited(published), what);
}

void P36LSeed(const P36World& w)
{
    const CharacterCreationSave creation = w.School.Creator.CaptureSave();
    WorldState world = w.Sim(0x3611).State();
    P2CExpect(world.Seq == 0 && !world.Awakening.Seeded, "a new life starts unseeded, before any command");
    const auto seeded = SeedNewLifeLearning(world, w.F.Content, creation);
    const auto prior = AcademicPriorPreparation(w.F.Content, creation.TraitRanks[P36LFoundationIndex()]);
    P2CExpect(seeded.Ok() && seeded.Applied && prior.Ok() && seeded.PriorPreparationMilli == prior.MilliXp,
              "the seed reports its prior preparation from the one foundation table");
    WorldState reference = w.Sim(0x3611).State();
    const auto r9 = SeedAwakeningInputs(w.F.Content, creation, reference.WorldSeed);
    P2CExpect(r9.Ok(), "R9's own seed");
    reference.Awakening = r9.State;
    P2CExpect(SerialiseAwakeningState(world) == SerialiseAwakeningState(reference),
              "the foundation is inside R9's seed exactly once, with no second addition");
    const std::string once = SerialiseAwakeningState(world);
    const auto again = SeedNewLifeLearning(world, w.F.Content, creation);
    P2CExpect(again.Ok() && !again.Applied && SerialiseAwakeningState(world) == once, "a seeded life is never reseeded");

    // Played lives are never (re)seeded: a loaded world has commands behind it.
    WorldSim played = w.Sim(0x3612);
    S04Accept(played, AdvanceTime{60}, "an unseeded life is played");
    WorldState legacy = played.State();
    const std::uint64_t hash = HashWorldState(legacy);
    const auto refused = SeedNewLifeLearning(legacy, w.F.Content, creation);
    P2CExpect(!refused.Ok() && refused.Error == "learning.not_new_life" && HashWorldState(legacy) == hash,
              "a played (or loaded) unseeded life is not seeded after the fact");

    // A seeded life that has learned keeps its learning across a save and load; seeding stays inert there.
    WorldSim sim = w.Continue(world);
    S04Morning(sim, w.School.Cats);
    S04PassTo(sim, P2CBellMinute(*w.School.Cats.Day.School, 0));
    S04Accept(sim, SchoolLessonOpen{w.School.Binding(0).Id, SchoolLessonMode::Attend, {}}, "a lesson opens");
    S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "a lesson closes");
    P2CExpect(P36SkillTotal(sim.State()) > P36SkillTotal(world), "the lesson credited the owner");
    const std::string text = SerializeWorldSave(w.School.Save(sim.State()));
    WorldSaveData loaded;
    P2CExpect(DeserializeWorldSave(text, loaded).HashVerified, "the seeded life loads certified");
    P2CExpect(SerialiseAwakeningState(loaded.World) == SerialiseAwakeningState(sim.State()), "the load restores learning as saved");
    WorldSim continued = w.Continue(loaded.World);
    P2CExpect(SerialiseAwakeningState(continued.State()) == SerialiseAwakeningState(sim.State()), "binding a sim reseeds nothing");
    WorldState probe = loaded.World;
    const auto reload = SeedNewLifeLearning(probe, w.F.Content, creation);
    P2CExpect(reload.Ok() && !reload.Applied && SerialiseAwakeningState(probe) == SerialiseAwakeningState(sim.State()),
              "the seed is never reapplied on load");
}

void P36LLesson(const P36World& w)
{
    WorldSim sim = w.Continue(P36LNewLife(w, 0x3613));
    S04Morning(sim, w.School.Cats);
    S04PassTo(sim, P2CBellMinute(*w.School.Cats.Day.School, 0));
    S04Accept(sim, SchoolLessonOpen{w.School.Binding(0).Id, SchoolLessonMode::Attend, {}}, "a lesson opens");
    const WorldState before = sim.State();
    const std::uint64_t session = before.SchoolSessions.Active->Session;
    const auto closed = S04Accept(sim, SchoolLessonClose{session, false, 0}, "the lesson closes");
    const auto published = S04Completions(closed);
    P2CExpect(!published.empty() && P36Credited(published) > 0, "the close publishes credited completions");
    P36LCreditedOnce(before, sim.State(), published, "a lesson close credits exactly its completions once");
    const WorldState after = sim.State();
    P2CExpect(!S04Send(sim, SchoolLessonClose{session, false, 0}).Accepted &&
                  SerialiseAwakeningState(sim.State()) == SerialiseAwakeningState(after),
              "a repeated close credits nothing");
    // Routine day time afterwards adds no lesson credit again.
    S04Accept(sim, AdvanceTime{120}, "time passes");
    P2CExpect(sim.State().Awakening.AppliedActivityInstances == after.Awakening.AppliedActivityInstances &&
                  P36SkillTotal(sim.State()) == P36SkillTotal(after),
              "ordinary time re-credits nothing");
}

void P36LPaper(const P36World& w)
{
    const auto first = w.Science().front();
    WorldSim sim = w.Continue(P36LNewLife(w, 0x3614));
    P36To(sim, first.AvailableAt + 30);
    P2CExpect(P36Record(sim.State(), first.Id) != nullptr, "the bound catalogue issued the brief");
    P36AnswerPaper(sim, first.Id);
    const WorldState open = sim.State();

    // An interrupted hand-in refuses whole: no learning, runner or practice change.
    WorldSim capped = w.Continue(open, 2);
    for (int i = 1; i <= 4; ++i)
        S04Accept(capped, ScheduleWorldEvent{open.Clock.Now() + 10 + i, "test/p36/learn" + std::to_string(i)}, "an event",
                  CommandSource::System);
    const WorldState cappedOpen = capped.State();
    S04Refused(capped, P36Work(first.Id, SchoolWorkActionKind::Submit), "school.work.charge_interrupted",
               "an interrupted hand-in refuses whole");
    P2CExpect(SerialiseAwakeningState(capped.State()) == SerialiseAwakeningState(cappedOpen) &&
                  capped.State().DayLoop == cappedOpen.DayLoop,
              "a refused hand-in credits nothing");

    const auto sent = S04Accept(sim, P36Work(first.Id, SchoolWorkActionKind::Submit), "the paper is handed in");
    const auto published = S04Completions(sent);
    P2CExpect(published.size() == 1 && published.front().ActivityId == first.Id &&
                  published.front().Instance == sim.State().DayLoop.Runner.Instance &&
                  published.front().Instance == open.DayLoop.Runner.Instance + 1 &&
                  published.front().At == *P36Record(sim.State(), first.Id)->SubmittedAt && P36Credited(published) > 0,
              "the hand-in publishes one completion from the shared runner, credited for its answer effort");
    P36LCreditedOnce(open, sim.State(), published, "a paper credits exactly its one completion once");
    const WorldState after = sim.State();
    S04Refused(sim, P36Work(first.Id, SchoolWorkActionKind::Submit), "school.work.already_submitted",
               "a duplicate hand-in credits and charges nothing");
    P2CExpect(SerialiseAwakeningState(sim.State()) == SerialiseAwakeningState(after), "the duplicate leaves learning unchanged");

    // An unseeded life hands in the same paper with no growth owner: marks, but no runner or learning change.
    WorldSim plain = w.Sim(0x3615);
    P36To(plain, first.AvailableAt + 30);
    P36AnswerPaper(plain, first.Id);
    const WorldState plainOpen = plain.State();
    const auto plainSent = S04Accept(plain, P36Work(first.Id, SchoolWorkActionKind::Submit), "an unseeded hand-in");
    P2CExpect(S04Completions(plainSent).empty() && plain.State().DayLoop.Runner.Instance == plainOpen.DayLoop.Runner.Instance &&
                  !plain.State().Awakening.Seeded,
              "an unseeded life earns no learning from a paper");
}

void P36LWeek(const P36World& w)
{
    WorldSim sim = w.Continue(P36LNewLife(w, 0x3616));
    const WeekPlan base = P36KnownWeek(w, sim);
    P36To(sim, CalendarDayStartInstant(7) + 360);
    const WorldState before = sim.State();
    const auto resolved = S04Accept(sim, SchoolWeekResolve{base}, "the known week resolves");
    const auto published = S04Completions(resolved);
    std::size_t completed = 0;
    for (const auto& event : sim.LastSchoolWeekResult()->ActivityEvents)
        completed += event.Kind == DayLoopEventKind::Completed ? 1 : 0;
    P2CExpect(!published.empty() && published.size() == completed && P36Credited(published) > 0,
              "the week publishes each actual completion once");
    P36LCreditedOnce(before, sim.State(), published, "a compressed week credits exactly its completions once, never its totals");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root argument");
    const P2CFixture fixture(argv[1]);
    const P36World world(fixture);
    P36LSeed(world);
    P36LLesson(world);
    P36LPaper(world);
    P36LWeek(world);
    std::cout << "P3-06 WorldLearningIntegration passed\n";
    return 0;
}
