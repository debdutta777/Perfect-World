#include "DayLoopTestSupport.h"

using namespace PerfectWorld;

namespace
{
constexpr int Sequences = 1000;
constexpr int CommandsPerSequence = 70;

std::vector<ReceiptCandidate> RandomAlternatives(DayLoopRng& rng, GameInstant now)
{
    std::vector<ReceiptCandidate> alternatives;
    const std::int64_t count = rng.Pick(0, 3);
    for (std::int64_t i = 0; i < count; ++i)
        alternatives.push_back(Candidate("alt." + std::string(1, static_cast<char>('a' + rng.Pick(0, 3))), rng.Pick(0, now + 100),
                                         now + rng.Pick(0, 200), static_cast<std::int32_t>(rng.Pick(10, 90)),
                                         rng.Pick(0, 4) != 0, static_cast<std::int32_t>(rng.Pick(0, 900))));
    return alternatives;
}

std::string RandomActivity(DayLoopRng& rng)
{
    const std::vector<ActivityDef>& defs = DayLoopCats().Activities->Defs;
    return defs[static_cast<std::size_t>(rng.Pick(0, static_cast<std::int64_t>(defs.size()) - 1))].Id;
}

DayCommand CommitFor(DayLoopRng& rng, const DayLoopState& state, bool sensible)
{
    const std::string id = RandomActivity(rng);
    const ActivityDef* def = DayLoopCats().Activities->Find(id);
    std::string place = def->Place == ActivityAnyPlace ? "garden" : def->Place;
    if (!sensible && rng.Pick(0, 3) == 0)
        place = "elsewhere";
    return CommitCmd(id, place, RandomAlternatives(rng, state.Now));
}

DayCommand RandomAnything(DayLoopRng& rng, const DayLoopState& state)
{
    DayCommand c;
    c.Kind = static_cast<DayCommandKind>(rng.Pick(0, 8));
    c.Minutes = rng.Pick(-5, 700);
    c.Pass = static_cast<DayPassKind>(rng.Pick(0, 3));
    c.DepartureMinute = rng.Pick(0, 1) == 0 ? AttendanceStaysUntilDismissal : static_cast<std::int32_t>(rng.Pick(0, 1500));
    if (c.Kind == DayCommandKind::Commit)
        c = CommitFor(rng, state, false);
    if (c.Kind == DayCommandKind::PlanDay)
        c.Plan = {{static_cast<std::int32_t>(rng.Pick(-1, 1500)), RandomActivity(rng)}};
    return c;
}

DayCommand NextCommand(DayLoopRng& rng, const DayLoopState& state)
{
    if (rng.Pick(0, 99) < 15)
        return RandomAnything(rng, state);
    if (state.Asleep)
        return SimpleCmd(DayCommandKind::Wake, rng.Pick(300, 600));
    if (!state.Planned)
    {
        std::vector<DayPlanItem> plan;
        if (rng.Pick(0, 1) == 0)
            plan = {{static_cast<std::int32_t>(rng.Pick(360, 700)), RandomActivity(rng)}};
        return PlanCmd(plan);
    }
    if (state.Runner.Active())
        return rng.Pick(0, 3) != 0 ? SimpleCmd(DayCommandKind::Complete) : SimpleCmd(DayCommandKind::Cancel, rng.Pick(0, 40));
    if (state.HasCommitment)
        return rng.Pick(0, 9) != 0 ? SimpleCmd(DayCommandKind::Start) : SimpleCmd(DayCommandKind::Cancel);
    switch (rng.Pick(0, 9))
    {
    case 0:
        return SimpleCmd(DayCommandKind::ArriveSchool);
    case 1:
        return SimpleCmd(DayCommandKind::GoToSleep);
    case 2:
    case 3:
    case 4:
        return PassCmd(rng.Pick(1, 300), static_cast<DayPassKind>(rng.Pick(0, 2)));
    default:
        return CommitFor(rng, state, true);
    }
}

struct Trace
{
    std::vector<std::pair<int, std::uint64_t>> Steps;
    int Accepted = 0;
    int Completed = 0;
    int Wakes = 0;
    int Refused = 0;
    DayLoopState Final;

    bool operator==(const Trace& other) const { return Steps == other.Steps && Final == other.Final; }
};

/** Runs one random sequence; when `roundTripAt` is not negative the state is saved and reloaded before that command. */
Trace RunSequence(std::uint64_t seed, int roundTripAt)
{
    DayLoopRng rng{seed * 7919 + 13};
    Trace trace;
    DayLoopState state = NewDayLoopState();
    for (int step = 0; step < CommandsPerSequence; ++step)
    {
        if (step == roundTripAt)
        {
            const DayLoopParse parsed = ParseDayLoop(SerialiseDayLoop(state));
            Check(parsed.Ok(), "a random boundary saves and loads");
            state = parsed.State;
        }
        const DayCommand command = NextCommand(rng, state);
        const DayLoopState before = state;
        const DayLoopResult result = ApplyDayCommand(state, DayLoopCats(), command);
        Check(IsDayLoopStateValid(state), "every state along a random run is valid");
        if (result.Ok())
        {
            ++trace.Accepted;
            Check(state.Seq == before.Seq + 1 && state.Now >= before.Now, "an accepted command is dense and time never runs backwards");
            Check(result.CheckpointDue == (command.Kind == DayCommandKind::Wake), "a checkpoint is due exactly at a wake");
            Check(state.Runner.AppliedInstance >= before.Runner.AppliedInstance, "an activity's outcome is never taken back");
            for (const DayLoopEvent& event : result.Events)
            {
                trace.Completed += event.Kind == DayLoopEventKind::Completed ? 1 : 0;
                trace.Wakes += event.Kind == DayLoopEventKind::Woke ? 1 : 0;
            }
        }
        else
        {
            ++trace.Refused;
            Check(state == before, "a refused command changes nothing");
        }
        trace.Steps.push_back({static_cast<int>(result.Error), HashDayLoop(state)});
    }
    trace.Final = state;
    return trace;
}

} // namespace

void TestDayLoopRandom()
{
    int accepted = 0;
    int completed = 0;
    int wakes = 0;
    int refused = 0;
    for (int i = 1; i <= Sequences; ++i)
    {
        const Trace first = RunSequence(static_cast<std::uint64_t>(i), -1);
        const Trace second = RunSequence(static_cast<std::uint64_t>(i), -1);
        Check(first == second, "a random command sequence replays deterministically");
        // Saving and reloading at a varying boundary changes nothing about the run.
        const Trace resumed = RunSequence(static_cast<std::uint64_t>(i), i % CommandsPerSequence);
        Check(resumed == first, "saving and resuming mid-sequence gives the same hashes and final state");
        accepted += first.Accepted;
        completed += first.Completed;
        wakes += first.Wakes;
        refused += first.Refused;
    }
    Check(accepted > 20000 && refused > 5000, "the generator mixes accepted and refused commands");
    Check(completed > 3000 && wakes > 1000, "the random runs complete many activities and wake many times");
    std::cout << "day loop random runs: " << Sequences << " sequences, " << accepted << " accepted, " << refused << " refused, "
              << completed << " completions, " << wakes << " wakes\n";
}
