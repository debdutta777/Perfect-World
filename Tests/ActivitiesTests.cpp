// ROADMAP M2-03: activities. Usage: PerfectWorldActivitiesTests <path to Data/Content>
#include "Activities.h"
#include "ActivitiesTestSupport.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>

using namespace PerfectWorld;

namespace
{
// A small deterministic generator for the tests (xorshift64*).
struct Rng
{
    std::uint64_t S;
    std::uint32_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return static_cast<std::uint32_t>((S * 2685821657736338717ULL) >> 32);
    }
};

const ActivityCatalogue* Cat = nullptr;

ConditionState Advanced(ConditionState state, ConditionActivity activity, std::int64_t minutes)
{
    Check(AdvanceCondition(state, activity, minutes), "the oracle advance is accepted");
    return state;
}

// Everything the outside world keeps while an activity runs: the runner state, the condition, and the sums of what the
// events granted. The events are the only way learning and evidence leave the runner.
struct World
{
    ActivityRunnerState State;
    ConditionState Condition;
    std::map<std::string, std::int64_t> Skills;
    std::array<std::int64_t, AttributeCount> Evidence{};
    std::map<std::uint64_t, int> Ends;
    int Completed = 0;
    int Cancelled = 0;
    GameInstant Now = 0;

    friend bool operator==(const World&, const World&) = default;
};

void Absorb(World& world, const ActivityResult& result)
{
    for (const ActivityEvent& event : result.Events)
    {
        if (event.Kind == ActivityEventKind::Started)
            continue;
        ++world.Ends[event.Instance];
        if (event.Kind == ActivityEventKind::Cancelled)
        {
            ++world.Cancelled;
            Check(event.Learning.empty() && event.Evidence.empty() && event.Outcome.empty(), "a cancel grants no outcome");
            continue;
        }
        ++world.Completed;
        for (const SkillCredit& credit : event.Learning)
            world.Skills[credit.SkillId] += credit.MilliXp;
        for (const EvidenceCredit& credit : event.Evidence)
            world.Evidence[static_cast<std::size_t>(credit.Attr)] += credit.Milli;
    }
}

ActivityResult Begin(World& world, const char* id, GameInstant at, const char* place)
{
    const ActivityResult result = StartActivity(world.State, *Cat, id, at, place);
    Absorb(world, result);
    return result;
}

ActivityResult Finish(World& world, GameInstant at)
{
    const ActivityResult result = CompleteActivity(world.State, *Cat, world.Condition, at);
    Absorb(world, result);
    return result;
}

ActivityResult Abort(World& world, GameInstant at)
{
    const ActivityResult result = CancelActivity(world.State, *Cat, world.Condition, at);
    Absorb(world, result);
    return result;
}

World Reloaded(const World& world)
{
    const ActivityRunnerParse parsed = DeserialiseActivityRunner(SerialiseActivityRunner(world.State));
    Check(parsed.Ok() && parsed.State == world.State, "a saved runner state loads back equal");
    World copy = world;
    copy.State = parsed.State;
    return copy;
}

void TestCatalogue(const ContentDb& db)
{
    Check(Cat->Defs.size() == 7, "seven activities are authored");
    for (const char* id : {"activity.attend_class", "activity.eat_meal", "activity.exercise_run", "activity.household_chores",
                           "activity.rest", "activity.sleep_night", "activity.study_at_home"})
        Check(Cat->Find(id) != nullptr, "an authored activity is in the catalogue");
    Check(Cat->Find("activity.nothing") == nullptr, "an unknown ID is not found");
    Check(std::is_sorted(Cat->Defs.begin(), Cat->Defs.end(), [](const ActivityDef& a, const ActivityDef& b) { return a.Id < b.Id; }),
          "definitions are sorted by ID");

    for (const ActivityDef& def : Cat->Defs)
    {
        Check(def.DurationMinutes >= 1 && def.DurationMinutes <= 1440, "a duration is within a day");
        std::int32_t sum = 0;
        for (const ActivityShare& share : def.Split)
        {
            sum += share.Permille;
            const ContentRecord* skill = db.Find(share.SkillId);
            Check(skill != nullptr && skill->Type == "skill", "every split names a skill in the catalogue");
        }
        Check(def.Split.empty() ? def.AllowanceMilliXp == 0 : sum == ActivitySplitWhole, "allowance splits sum to one");
        std::int32_t credited = 0;
        for (const SkillCredit& credit : ActivityLearningCredits(def))
            credited += credit.MilliXp;
        Check(credited == def.AllowanceMilliXp, "the credits sum to exactly the allowance");
        std::int32_t evidence = 0;
        for (const ActivityEvidenceDef& entry : def.Evidence)
            evidence += entry.Milli;
        Check(evidence * ActivityEvidenceDivisor <= def.AllowanceMilliXp, "evidence is at most a twentieth of the allowance");
        Check(def.HasMeal == (def.Outcome == "ate"), "a meal is eaten exactly when the outcome is ate");
    }
    const ActivityDef* study = Cat->Find("activity.study_at_home");
    Check(study->Split.size() == 2 && study->Split[0].SkillId == "skill.academics" && study->Split[0].Permille == 800,
          "study at home divides 800/200");
}

void TestSplitAllowance()
{
    const std::vector<std::int32_t> odd = SplitAllowance(1001, {333, 333, 334});
    Check(odd.size() == 3 && odd[0] + odd[1] + odd[2] == 1001, "an awkward allowance still sums exactly");
    Check(SplitAllowance(1, {500, 500}) == std::vector<std::int32_t>({1, 0}), "a tie goes to the earlier share");
    Check(SplitAllowance(1, {1, 999}) == std::vector<std::int32_t>({0, 1}), "the largest remainder wins the leftover");
    Check(SplitAllowance(0, {500, 500}) == std::vector<std::int32_t>({0, 0}), "no allowance gives nothing");
    Check(SplitAllowance(3000, {800, 200}) == std::vector<std::int32_t>({2400, 600}), "an exact split is exact");
    Check(SplitAllowance(100, {500, 400}).empty(), "parts that do not sum to the whole are refused");
    Check(SplitAllowance(100, {1100, -100}).empty(), "a negative part is refused");
    Check(SplitAllowance(-1, {1000}).empty(), "a negative allowance is refused");
    Rng rng{0x9E3779B97F4A7C15ULL};
    for (int i = 0; i < 3000; ++i)
    {
        const std::size_t parts = 1 + rng.Next() % 4;
        std::vector<std::int32_t> permille(parts, 0);
        std::int32_t left = ActivitySplitWhole;
        for (std::size_t p = 0; p + 1 < parts; ++p)
            left -= permille[p] = static_cast<std::int32_t>(rng.Next() % (left + 1));
        permille.back() = left;
        const std::int64_t allowance = rng.Next() % 20001;
        const std::vector<std::int32_t> shares = SplitAllowance(allowance, permille);
        std::int64_t sum = 0;
        for (std::size_t p = 0; p < shares.size(); ++p)
        {
            sum += shares[p];
            const std::int64_t exact = allowance * permille[p];
            Check(shares[p] >= exact / 1000 && shares[p] <= exact / 1000 + 1, "a share is its floor or one more");
        }
        Check(shares.size() == parts && sum == allowance, "random splits sum exactly to the allowance");
    }
}

void TestStart()
{
    World w;
    Check(!Begin(w, "activity.nothing", 0, "home").Ok(), "an unknown activity is refused");
    Check(Begin(w, "activity.nothing", 0, "home").Error == ActivityError::UnknownActivity, "the refusal says unknown");
    Check(Begin(w, "activity.study_at_home", 0, "classroom").Error == ActivityError::WrongPlace, "the wrong place is refused");
    Check(Begin(w, "activity.study_at_home", -1, "home").Error == ActivityError::InvalidTime, "a negative start is refused");
    Check(w.State == ActivityRunnerState{}, "refusals leave the state alone");
    Check(Begin(w, "activity.rest", 5, "anywhere at all").Ok(), "an 'any' activity starts in any place");
    w = World{};
    const ActivityResult started = Begin(w, "activity.study_at_home", 600, "home");
    Check(started.Ok() && started.Events.size() == 1 && started.Events[0].Kind == ActivityEventKind::Started, "a start emits Started");
    Check(started.Events[0].Instance == 1 && started.Events[0].Minutes == 120 && started.Events[0].At == 600, "the start event carries the instance and duration");
    Check(w.State.Active() && w.State.StartedAt == 600 && w.State.Instance == 1 && w.State.AppliedInstance == 0, "the state shows one running activity");
    const ActivityRunnerState running = w.State;
    Check(Begin(w, "activity.rest", 610, "home").Error == ActivityError::Busy, "a second activity is refused while one runs");
    Check(w.State == running, "a busy refusal changes nothing");
    Check(std::string(ActivityErrorCode(ActivityError::Busy)) == "activity.busy" && ActivityErrorCode(ActivityError::None).empty(), "rejection codes are stable");
}

void TestCompleteExactlyOnce()
{
    World w;
    Begin(w, "activity.study_at_home", 100, "home");
    const ConditionState before = w.Condition;
    const ActivityRunnerState running = w.State;
    Check(Finish(w, 100 + 119).Error == ActivityError::NotFinished, "completing a minute early is refused");
    Check(w.State == running && w.Condition == before && w.Completed == 0, "an early completion changes nothing");
    Check(Finish(w, 99).Error == ActivityError::NotFinished, "completing before the start is refused");

    const ActivityResult done = Finish(w, 100 + 120);
    Check(done.Ok() && done.Events.size() == 1 && done.Events[0].Kind == ActivityEventKind::Completed, "completion emits Completed");
    const ActivityEvent& event = done.Events[0];
    Check(event.Instance == 1 && event.Outcome == "studied" && event.Minutes == 120, "the event names the instance and outcome");
    Check(event.Learning == std::vector<SkillCredit>({{"skill.academics", 2400}, {"skill.self_management", 600}}), "learning is the 800/200 split of 3000");
    Check(event.Evidence == std::vector<EvidenceCredit>({{Attribute::Intelligence, 150}}), "the evidence is declared");
    Check(w.Condition == Advanced(before, ConditionActivity::Awake, 120), "the condition cost is two hours awake");
    Check(!w.State.Active() && w.State.Instance == 1 && w.State.AppliedInstance == 1 && w.State.StartedAt == 0, "the state is idle and records the instance applied");

    const World applied = w;
    for (int i = 0; i < 3; ++i)
    {
        Check(Finish(w, 100 + 120 + i * 1000).Error == ActivityError::NotActive, "a repeat completion is refused");
        Check(Abort(w, 400 + i).Error == ActivityError::NotActive, "a cancel after completion is refused");
    }
    Check(w == applied, "repeat completions apply nothing a second time");
    Check(w.Skills["skill.academics"] == 2400 && w.Evidence[2] == 150 && w.Completed == 1 && w.Ends[1] == 1, "the learning and evidence landed once");

    // A late completion is still one completion; the condition is charged the duration, not the delay.
    World late;
    Begin(late, "activity.attend_class", 0, "classroom");
    Check(Finish(late, 100000).Ok() && late.Condition == Advanced(ConditionState{}, ConditionActivity::Awake, 45), "a late completion charges only the duration");

    World meal;
    Begin(meal, "activity.eat_meal", 0, "dining");
    Check(Finish(meal, 20).Ok(), "a meal completes");
    ConditionState expectedMeal = Advanced(ConditionState{}, ConditionActivity::Awake, 20);
    Check(ApplyMeal(expectedMeal, MealSize::Full) && meal.Condition == expectedMeal, "a meal applies the Full relief after its minutes");
    Check(meal.Skills.empty() && meal.Evidence == std::array<std::int64_t, AttributeCount>{}, "a meal teaches nothing");

    World run;
    Begin(run, "activity.exercise_run", 0, "outdoors");
    Check(Finish(run, 45).Ok(), "a run completes");
    ConditionState expectedRun = Advanced(ConditionState{}, ConditionActivity::HeavyExertion, 45);
    Check(AdjustStress(expectedRun, -3000) && run.Condition == expectedRun, "a run applies its stress relief after its minutes");
    Check(run.Skills["skill.fitness"] == 1500 && run.Evidence[0] == 50 && run.Evidence[1] == 25, "a run trains fitness and shows body and agility");

    World bad;
    Begin(bad, "activity.rest", 0, "home");
    bad.Condition.Energy = MeterMax + 1;
    const ActivityRunnerState inFlight = bad.State;
    Check(Finish(bad, 30).Error == ActivityError::InvalidCondition, "an invalid condition is refused");
    Check(bad.State == inFlight && bad.Condition.Energy == MeterMax + 1, "an invalid-condition refusal changes nothing");
}

void TestCancel()
{
    World w;
    Check(Abort(w, 0).Error == ActivityError::NotActive, "cancelling nothing is refused");
    Begin(w, "activity.exercise_run", 1000, "outdoors");
    const ConditionState before = w.Condition;
    Check(Abort(w, 999).Error == ActivityError::InvalidTime, "cancelling before the start is refused");
    Check(Abort(w, 1045).Error == ActivityError::Finished && w.State.Active(), "cancelling at the end is refused: complete it");
    const ActivityResult cancelled = Abort(w, 1020);
    Check(cancelled.Ok() && cancelled.Events.size() == 1 && cancelled.Events[0].Kind == ActivityEventKind::Cancelled, "a cancel emits Cancelled");
    Check(cancelled.Events[0].Minutes == 20 && cancelled.Events[0].Instance == 1, "the cancel event carries the elapsed minutes");
    Check(w.Condition == Advanced(before, ConditionActivity::HeavyExertion, 20), "the condition is charged the elapsed minutes pro rata");
    Check(w.Skills.empty() && w.Completed == 0 && w.Cancelled == 1, "a cancel teaches nothing");
    Check(!w.State.Active() && w.State.AppliedInstance == 1, "a cancel ends the instance");
    Check(Finish(w, 1045).Error == ActivityError::NotActive && Abort(w, 1030).Error == ActivityError::NotActive, "neither end can follow a cancel");
    Check(w.Ends[1] == 1, "the instance ended once");

    // No meal is eaten and no stress relief is granted by a cancelled meal or run.
    World meal;
    Begin(meal, "activity.eat_meal", 0, "dining");
    Check(Abort(meal, 10).Ok() && meal.Condition == Advanced(ConditionState{}, ConditionActivity::Awake, 10), "a cancelled meal relieves no hunger");
    World instant;
    Begin(instant, "activity.sleep_night", 50, "home");
    Check(Abort(instant, 50).Ok() && instant.Condition == ConditionState{}, "cancelling at once costs nothing");
    Check(Begin(instant, "activity.rest", 60, "home").Events[0].Instance == 2, "the next activity gets the next instance");
}

void TestSaveLoadMidActivity()
{
    for (const ActivityDef& def : Cat->Defs)
    {
        const std::string place = def.Place == "any" ? "home" : def.Place;
        World straight;
        Begin(straight, def.Id.c_str(), 1234, place.c_str());
        World loaded = Reloaded(straight);
        Check(loaded == straight && loaded.State.Active(), "a mid-activity save restores the running activity");
        const ActivityResult a = Finish(straight, 1234 + def.DurationMinutes);
        const ActivityResult b = Finish(loaded, 1234 + def.DurationMinutes);
        Check(a.Ok() && b.Ok() && a.Events == b.Events && straight == loaded, "completing after a load equals completing without one");
        Check(loaded.Completed == 1 && loaded.Ends[1] == 1, "the completion applied exactly once across the save");
        Check(Finish(loaded, 1234 + def.DurationMinutes).Error == ActivityError::NotActive, "a second completion after the load is refused");

        // Saving after completion and loading must not bring the activity back.
        World after = Reloaded(loaded);
        Check(!after.State.Active() && Finish(after, 99999).Error == ActivityError::NotActive && after == loaded, "a save after completion does not resurrect it");

        // A save taken before the end, then loaded in a fresh process, cancels and completes consistently.
        World second;
        Begin(second, def.Id.c_str(), 50, place.c_str());
        World mid = Reloaded(second);
        Check(Abort(mid, 50).Ok() && Abort(second, 50).Ok() && mid == second, "cancelling after a load equals cancelling without one");
    }

    // Many saves and loads during one activity change nothing.
    World w;
    Begin(w, "activity.sleep_night", 0, "home");
    World reference = w;
    for (int i = 0; i < 10; ++i)
        w = Reloaded(w);
    Check(w == reference, "repeated save and load leaves the state alone");
    Check(Finish(w, 480).Ok() && Finish(reference, 480).Ok() && w == reference && w.Completed == 1, "and it completes once");
}

void TestText()
{
    ActivityRunnerState idle;
    Check(SerialiseActivityRunner(idle) == "pw.activity_runner/1\ninstance=0\napplied=0\nstarted_at=0\nactivity=-\nend\n", "the idle text is exact");
    ActivityRunnerState running{"activity.rest", 77, 3, 2};
    const std::string text = SerialiseActivityRunner(running);
    Check(text == "pw.activity_runner/1\ninstance=3\napplied=2\nstarted_at=77\nactivity=activity.rest\nend\n", "the running text is exact");
    Check(SerialiseActivityRunner(running) == text, "serialising is deterministic");
    const ActivityRunnerParse parsed = DeserialiseActivityRunner(text);
    Check(parsed.Ok() && parsed.State == running && SerialiseActivityRunner(parsed.State) == text, "the text round-trips");
    Check(DeserialiseActivityRunner(SerialiseActivityRunner(idle)).State == idle, "idle round-trips");

    for (std::size_t len = 0; len < text.size(); ++len)
        Check(!DeserialiseActivityRunner(std::string_view(text).substr(0, len)).Ok(), "every truncation is refused");
    Check(!DeserialiseActivityRunner(text + "x").Ok() && !DeserialiseActivityRunner(text + text).Ok(), "trailing data is refused");

    const auto refuses = [&](const std::string& from, const std::string& to, const char* what) {
        std::string copy = text;
        const std::size_t at = copy.find(from);
        Check(at != std::string::npos, "the edit target exists");
        copy.replace(at, from.size(), to);
        Check(!DeserialiseActivityRunner(copy).Ok(), what);
    };
    refuses("runner/1", "runner/2", "an unknown version is refused");
    refuses("instance=3", "instance=03", "a leading zero is refused");
    refuses("instance=3", "instance=+3", "a plus sign is refused");
    refuses("instance=3", "instance=-3", "a negative instance is refused");
    refuses("instance=3", "instance=3.0", "a fraction is refused");
    refuses("instance=3", "instance=", "an empty number is refused");
    refuses("instance=3", "instance=99999999999999999999999", "an overflowing number is refused");
    refuses("started_at=77", "started_at=-1", "a running activity with a negative start is refused");
    refuses("started_at=77", "started_at=0x4d", "hex is refused");
    refuses("applied=2", "applied=1", "two instances in flight are refused");
    refuses("applied=2", "applied=4", "an applied count past the instance is refused");
    refuses("activity=activity.rest", "activity=-", "a running state with no activity is refused");
    refuses("activity=activity.rest", "activity=Activity.Rest", "an upper-case ID is refused");
    refuses("activity=activity.rest", "activity=activity rest", "a space in the ID is refused");
    refuses("\nend\n", "\r\nend\r\n", "CRLF is refused");
    refuses("instance=3\napplied=2", "applied=2\ninstance=3", "reordered lines are refused");
    refuses("activity=activity.rest", "activity=", "an empty activity value is refused");
    Check(!DeserialiseActivityRunner("pw.activity_runner/1\ninstance=2\napplied=2\nstarted_at=5\nactivity=-\nend\n").Ok(), "an idle state with a start time is refused");
    Check(!DeserialiseActivityRunner("pw.activity_runner/1\ninstance=2\napplied=2\nstarted_at=0\nactivity=activity.rest\nend\n").Ok(), "an idle state with an activity is refused");
    Check(!DeserialiseActivityRunner("").Ok() && !DeserialiseActivityRunner("\n").Ok(), "empty text is refused");
    Check(SerialiseActivityRunner(ActivityRunnerState{"", 0, 1, 0}).empty(), "an inconsistent state does not serialise");
    Check(!IsActivityRunnerStateValid(ActivityRunnerState{"x", 0, 0, 0}), "an activity on an idle count is invalid");

    ActivityRunnerState big{"activity.rest", 9'000'000'000'000LL, 18'446'744'073'709'551'615ULL, 18'446'744'073'709'551'614ULL};
    Check(DeserialiseActivityRunner(SerialiseActivityRunner(big)).State == big, "the largest counters round-trip");
    ActivityRunnerState wrapped{"", 0, 18'446'744'073'709'551'615ULL, 18'446'744'073'709'551'615ULL};
    Check(StartActivity(wrapped, *Cat, "activity.rest", 0, "home").Error == ActivityError::InvalidState, "an exhausted instance counter is refused");
    ActivityRunnerState broken{"", 0, 5, 1};
    Check(StartActivity(broken, *Cat, "activity.rest", 0, "home").Error == ActivityError::InvalidState, "an inconsistent state refuses to start");
}

// One random script of operations, run with or without save/load in the middle.
World RunScript(std::uint64_t seed, bool reload, int steps)
{
    Rng rng{seed * 7919 + 17};
    World w;
    for (int i = 0; i < steps; ++i)
    {
        const std::uint32_t op = rng.Next() % 8;
        const std::uint32_t arg = rng.Next();
        if (op <= 1)
            w.Now += arg % 400;
        else if (op <= 3)
        {
            const ActivityDef& def = Cat->Defs[arg % Cat->Defs.size()];
            const bool wrong = (arg / 7) % 5 == 0;
            const std::string place = wrong ? "nowhere" : def.Place == "any" ? "home" : def.Place;
            const bool expectOk = !w.State.Active() && (!wrong || def.Place == ActivityAnyPlace);
            Check(Begin(w, def.Id.c_str(), w.Now, place.c_str()).Ok() == expectOk, "a start succeeds exactly when idle in the right place");
        }
        else if (op == 4 || op == 5)
        {
            const World before = w;
            const bool due = w.State.Active() && w.Now >= w.State.StartedAt + Cat->Find(w.State.ActivityId)->DurationMinutes;
            const ActivityResult r = Finish(w, w.Now);
            Check(r.Ok() == due, "a completion succeeds exactly when a running activity is due");
            if (!r.Ok())
                Check(w == before, "a refused completion changes nothing");
        }
        else if (op == 6)
        {
            const World before = w;
            const bool ok = Abort(w, w.Now).Ok();
            Check(ok == (before.State.Active() && !(w.Now >= before.State.StartedAt + Cat->Find(before.State.ActivityId)->DurationMinutes)), "a cancel succeeds exactly when running and not yet due");
            if (!ok)
                Check(w == before, "a refused cancel changes nothing");
        }
        else if (reload)
            w = Reloaded(w);
        Check(IsActivityRunnerStateValid(w.State) && IsConditionValid(w.Condition), "state and condition stay valid");
        for (const auto& [instance, ends] : w.Ends)
            Check(ends == 1 && instance <= w.State.Instance, "every instance ends at most once");
        Check(w.Ends.size() == static_cast<std::size_t>(w.Completed + w.Cancelled), "ends match the events");
        Check(w.State.Instance - w.State.AppliedInstance == (w.State.Active() ? 1u : 0u), "the instance counters match the activity");
    }
    return w;
}

void TestRandomSequences()
{
    int completed = 0;
    for (std::uint64_t seed = 1; seed <= 60; ++seed)
    {
        const World plain = RunScript(seed, false, 400);
        const World reloading = RunScript(seed, true, 400);
        Check(plain == reloading, "save/load at any point changes no outcome");
        Check(RunScript(seed, true, 400) == reloading, "a script replays identically");
        completed += plain.Completed;
    }
    Check(completed > 100, "the scripts complete a healthy number of activities");
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldActivitiesTests <path to Data/Content>\n";
        return 2;
    }
    const std::vector<ContentFile> files = ReadContentDirectory(argv[1]);
    const ContentLoadResult content = LoadContent(files);
    Check(content.Ok(), "the real content validates");
    const ActivityCatalogueResult loaded = LoadActivityCatalogue(content.Db);
    Check(loaded.Ok(), "the activity catalogue loads");
    Cat = &loaded.Catalogue;

    TestCatalogue(content.Db);
    TestSplitAllowance();
    TestStart();
    TestCompleteExactlyOnce();
    TestCancel();
    TestSaveLoadMidActivity();
    TestText();
    TestRandomSequences();
    TestContentRules(files);
    std::cout << "PerfectWorld activities: " << ActivityTestChecks << " checks passed\n";
    return 0;
}
