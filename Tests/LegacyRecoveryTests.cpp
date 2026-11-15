// P3-14: Awakening compatibility recovery (LegacyRecovery.h, awakening.recover). The frozen v9, v10 and v11 fixtures
// recover from their own creation records and reach a supervised grant before the fixed deadline; a life recovered after
// its appointment gets the make-up path (with the countdown opened late, or marked unavailable after its Gate onset); a
// life past the last session start is marked legacy compatibility mode and keeps flowing; nothing is invented; recovery
// is inert when repeated and across reload; the markers are reserved and authenticated; replay and a fresh process agree.
#include "LegacyRecovery.h"

#include "LearningCredits.h"
#include "R94AwakeningTestSupport.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

using namespace PerfectWorld;

namespace
{
const GameInstant P314Appointment = R94At(2043, 1, 23, 9, 0);
const GameInstant P314Exam = R94At(2043, 2, 2, 6, 0);

std::string P314Read(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    P2CExpect(static_cast<bool>(in), "a test file is readable");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void P314Write(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    P2CExpect(static_cast<bool>(out), "a test file is written");
}

/** The production binding: recipes, a countdown context with no obligations, and the validated content. */
WorldAwakeningCatalogues P314Bind(const R94World& w)
{
    WorldAwakeningCatalogues bind = w.Bind(false);
    bind.Content = &w.F.Content;
    return bind;
}

WorldSim P314Sim(const R94World& w, WorldState state)
{
    return WorldSim(std::move(state), MaxEventsPerAdvance, w.School.Cats.Day, {}, w.School.School(), P314Bind(w));
}

ReplayOptions P314Replay(const R94World& w)
{
    ReplayOptions options;
    options.School = w.School.Cats.Day;
    options.Lessons = w.School.School();
    options.Awakening = P314Bind(w);
    return options;
}

bool P314Flag(const WorldState& world, std::string_view id)
{
    const auto found = world.Flags.find(id);
    return found != world.Flags.end() && found->second;
}

WorldSaveData P314Legacy(const std::string& name, std::int32_t version)
{
    WorldSaveData data;
    const WorldSaveResult loaded = DeserializeWorldSave(P314Read(std::string(PW_AWAKENING_FIXTURE_DIR) + "/" + name), data);
    P2CExpect(loaded.Succeeded() && loaded.SourceVersion == version && !data.World.Awakening.Seeded,
              "a frozen legacy fixture loads unseeded");
    return data;
}

void P314To(WorldSim& sim, GameInstant at)
{
    if (at > sim.State().Clock.Now())
        S04Accept(sim, AdvanceTime{at - sim.State().Clock.Now()}, "time reaches the instant", CommandSource::System);
    P2CExpect(sim.State().Clock.Now() == at, "the life stands at the instant");
}

/** Refused with exactly `code` from `source`; the world's hash and sequence are unchanged. */
void P314Refused(WorldSim& sim, CommandPayload payload, CommandSource source, std::string_view code, std::string_view what)
{
    const std::uint64_t before = HashWorldState(sim.State());
    const std::uint64_t seq = sim.State().Seq;
    const CommandResult result = sim.Submit({source, std::move(payload)});
    if (result.Accepted || result.Why.Code != code)
        std::cerr << "expected " << code << " got " << (result.Accepted ? std::string("accepted") : result.Why.Code) << '\n';
    P2CExpect(!result.Accepted && result.Why.Code == code && HashWorldState(sim.State()) == before && sim.State().Seq == seq, what);
}

/** Every hash section except the command count, the flags and the Awakening slices is unchanged: history preserved. */
void P314SameHistory(const WorldState& before, const WorldState& after, bool countdownOpened)
{
    const auto a = HashWorldSections(before);
    const auto b = HashWorldSections(after);
    P2CExpect(a.size() == b.size(), "the same sections");
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const std::string& name = a[i].Name;
        const bool mayChange = name == "meta" || name == "flags" || name == "awakening" ||
                               (countdownOpened && (name == "gates" || name == "clock"));
        if (!mayChange && a[i].Hash != b[i].Hash)
            std::cerr << "section changed: " << name << '\n';
        P2CExpect(mayChange || a[i].Hash == b[i].Hash, "recovery preserves every authenticated history section");
    }
}

/** Saves, loads certified, and returns the loaded world: recovery must survive reload unchanged. */
WorldState P314Reload(const WorldSaveData& slot, const WorldState& world)
{
    WorldSaveData data = slot;
    data.World = world;
    const std::string text = SerializeWorldSave(data);
    P2CExpect(!text.empty(), "the recovered life saves");
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    P2CExpect(result.Succeeded() && result.HashVerified && HashWorldState(loaded.World) == HashWorldState(world),
              "the recovered life reloads certified");
    return loaded.World;
}

/** A recovered life brought to `at` by a sim that does not route anything (the life is unseeded until recovery). */
WorldState P314Legacy(const R94World& w, const WorldSaveData& data, GameInstant at)
{
    WorldSim sim = P314Sim(w, data.World);
    P314To(sim, at);
    return sim.State();
}

void P314LegacyGrant(const R94World& w, const std::string& name, std::int32_t version)
{
    const WorldSaveData data = P314Legacy(name, version);
    const WorldState before = data.World;
    const AwakeningCompatibilityView view = QueryAwakeningCompatibility(before);
    P2CExpect(view.Pending.Seed && !view.Pending.OpenMakeUp && !view.Pending.MarkPostDeadline && !view.Pending.OpenCountdown,
              "an unseeded legacy life before Awakening Day needs seeding only");
    WorldSim sim = P314Sim(w, before);
    P314Refused(sim, AwakeningRecover{data.Creation}, CommandSource::Player, "recovery.source", "only the system recovers");
    CharacterCreationSave unstarted = data.Creation;
    unstarted.HasStarted = false;
    P314Refused(sim, AwakeningRecover{unstarted}, CommandSource::System, "awakening.recovery_bad_creation",
                "an unstarted creation record is refused");
    WorldSim noContent(before, MaxEventsPerAdvance, w.School.Cats.Day, {}, w.School.School(), w.Bind(false));
    P314Refused(noContent, AwakeningRecover{data.Creation}, CommandSource::System, "awakening.recovery_missing_content",
                "seeding needs the validated content");

    S04Accept(sim, AwakeningRecover{data.Creation}, "the legacy life recovers", CommandSource::System);
    const WorldState after = sim.State();
    const auto expected = SeedLearningInputs(w.F.Content, data.Creation, before.WorldSeed);
    P2CExpect(expected.Ok() && after.Awakening.Seeded && after.Awakening.Stats == expected.State.Stats &&
                  after.Awakening.AttributeEvidenceMilli == expected.State.AttributeEvidenceMilli &&
                  after.Awakening.Conduct.Entries.empty() && !after.Awakening.Granted &&
                  after.Awakening.AppliedActivityInstances.empty() && after.Tutorial == TutorialDungeonState{} &&
                  after.Awakening.MakeUp.Phase == AwakeningMakeUpPhase::NotRequired && after.CountdownText.empty() &&
                  !after.LiveAwakenedProfile,
              "seeded exactly as creation confirmation would; no conduct, session, grant or countdown is invented");
    for (const AptitudeEvidence& item : after.Awakening.ProjectionEvidence.Items)
        P2CExpect(item.Id.rfind("school.lesson.", 0) == 0, "the only evidence is recorded lessons");
    P2CExpect(after.Awakening.ProjectionEvidence.Items.size() <= before.SchoolSessions.Periods.size(),
              "no more evidence than recorded lessons");
    P2CExpect(P314Flag(after, AwakeningSeededOnRecoveryFlag) && !P314Flag(after, AwakeningPostDeadlineFlag) &&
                  after.Seq == before.Seq + 1,
              "the recovery is marked and counted once");
    P314SameHistory(before, after, false);
    P314Refused(sim, AwakeningRecover{data.Creation}, CommandSource::System, "awakening.recovery_not_needed",
                "repeated recovery is inert");

    const WorldState reloaded = P314Reload(data, after);
    P2CExpect(!PlanAwakeningRecovery(reloaded).Needed() && QueryAwakeningCompatibility(reloaded).SeededOnRecovery,
              "reload keeps the recovery and needs none");
    WorldSim live = P314Sim(w, reloaded);
    P314Refused(live, AwakeningRecover{data.Creation}, CommandSource::System, "awakening.recovery_not_needed",
                "recovery after reload is inert");
    P314To(live, P314Appointment);
    S04Accept(live, TutorialEnter{}, "the recovered life enters its supervised session");
    R94PlayTutorial(live);
    const AwakeningState& granted = live.State().Awakening;
    P2CExpect(granted.Granted && granted.Granted->At < P314Exam && granted.ExaminationAt == P314Exam &&
                  granted.AppointmentAt == P314Appointment && live.State().LiveAwakenedProfile,
              "the recovered legacy life reaches a grant before the fixed deadline");
}

/** Recovered after its appointment: the make-up path, the deadline fixed. Returns the world just after recovery. */
WorldState P314MakeUp(const R94World& w, const WorldSaveData& data, GameInstant at, bool countdownOpens)
{
    const WorldState before = P314Legacy(w, data, at);
    const AwakeningRecoveryPlan plan = PlanAwakeningRecovery(before);
    P2CExpect(plan.Seed && plan.OpenMakeUp && !plan.MarkPostDeadline && plan.OpenCountdown == countdownOpens &&
                  plan.MarkCountdownUnavailable == !countdownOpens,
              "the plan opens the make-up and either opens or marks the countdown");
    WorldSim sim = P314Sim(w, before);
    S04Accept(sim, AwakeningRecover{data.Creation}, "the late legacy life recovers", CommandSource::System);
    const WorldState after = sim.State();
    P2CExpect(after.Awakening.Seeded && after.Awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required &&
                  !after.Awakening.MakeUp.At && after.Awakening.AppointmentAt == P314Appointment &&
                  after.Awakening.ExaminationAt == P314Exam && after.Awakening.Conduct.Entries.empty() && !after.Awakening.Granted,
              "the make-up path opens; appointment and deadline unchanged; no conduct invented");
    P314SameHistory(before, after, countdownOpens);
    if (countdownOpens)
    {
        const std::optional<CountdownState> countdown = sim.Countdown();
        P2CExpect(countdown && countdown->Start == CountdownStartInstant() && countdown->End == CountdownEndInstant() &&
                      !countdown->Ledger.empty() && countdown->Ledger.front().At == at && !P314Flag(after, AwakeningCountdownUnavailableFlag),
                  "the countdown opens late at the current instant over its fixed span");
    }
    else
    {
        P2CExpect(after.CountdownText.empty() && after.Gates.find(CountdownWorldEvent().GateId) == after.Gates.end() &&
                      P314Flag(after, AwakeningCountdownUnavailableFlag) && QueryAwakeningCompatibility(after).CountdownUnavailable,
                  "after the Gate onset no countdown or Gate break is invented; the life is marked instead");
    }
    P2CExpect(QueryAwakeningCompatibility(after).MakeUpRequired, "presentation sees the make-up owed");
    P314Refused(sim, TutorialEnter{}, CommandSource::Player, "tutorial.make_up_required", "entry waits for a scheduled make-up");
    return after;
}

/** The make-up as played: schedule now, enter, play; the grant lands before the unchanged deadline. */
void P314Script(WorldSim& sim)
{
    const GameInstant now = sim.State().Clock.Now();
    S04Accept(sim, TutorialMakeUp{now}, "a supervised make-up is scheduled now");
    S04Accept(sim, TutorialEnter{}, "the make-up session begins");
    R94PlayTutorial(sim);
    S04Accept(sim, AdvanceTime{30}, "life continues", CommandSource::System);
}

void P314MakeUpGrant(const R94World& w, const WorldSaveData& data, GameInstant at, bool countdownOpens)
{
    WorldSim sim = P314Sim(w, P314Reload(data, P314MakeUp(w, data, at, countdownOpens)));
    P314Script(sim);
    const AwakeningState& a = sim.State().Awakening;
    P2CExpect(a.Granted && a.Granted->At == at + TutorialSessionMinutes && a.Granted->At < P314Exam &&
                  a.MakeUp.Phase == AwakeningMakeUpPhase::Completed && a.ExaminationAt == P314Exam,
              "the make-up grants before the fixed deadline without extending it");
}

void P314PostDeadline(const R94World& w, const WorldSaveData& data, GameInstant at)
{
    const WorldState before = P314Legacy(w, data, at);
    WorldSim sim = P314Sim(w, before);
    S04Accept(sim, AwakeningRecover{data.Creation}, "a post-deadline legacy life recovers", CommandSource::System);
    const WorldState after = sim.State();
    const AwakeningCompatibilityView view = QueryAwakeningCompatibility(after);
    P2CExpect(after.Awakening.Seeded && !after.Awakening.Granted && view.PostDeadline && view.SeededOnRecovery &&
                  view.CountdownUnavailable && !view.MakeUpRequired && !view.Pending.Needed() &&
                  after.Awakening.MakeUp.Phase == AwakeningMakeUpPhase::NotRequired,
              "past the last session start the life is marked legacy compatibility mode, never a fake grant");
    P314SameHistory(before, after, false);
    P314Refused(sim, TutorialMakeUp{at}, CommandSource::Player, "tutorial.make_up_not_required", "no make-up is possible");
    const CommandResult flows = S04Accept(sim, AdvanceTime{2 * MinutesPerDay}, "the compatibility life keeps flowing",
                                          CommandSource::System);
    P2CExpect(!flows.Partial && sim.State().Clock.Now() == at + 2 * MinutesPerDay, "time is not stranded");
    const WorldState reloaded = P314Reload(data, sim.State());
    P2CExpect(QueryAwakeningCompatibility(reloaded).PostDeadline && !PlanAwakeningRecovery(reloaded).Needed(),
              "the slot is preserved in compatibility mode across reload");
}

/** A seeded life driven past 18:00 by an unrouted sim: only this command bootstraps its countdown (R9-05 decision). */
void P314SeededLate(const R94World& w)
{
    const WorldState seeded = w.Seeded(R94At(2043, 1, 24, 10, 0), 0x314);
    WorldSim plain = w.Bound(seeded);
    S04Accept(plain, AdvanceTime{60}, "a bound sim passes time", CommandSource::System);
    P2CExpect(plain.State().CountdownText.empty(), "a bound sim still never bootstraps on its own");
    const AwakeningRecoveryPlan plan = PlanAwakeningRecovery(plain.State());
    P2CExpect(!plan.Seed && !plan.OpenMakeUp && plan.OpenCountdown, "only the countdown is missing");
    WorldAwakeningCatalogues unrouted = P314Bind(w);
    unrouted.Countdown = {};
    WorldSim noContext(plain.State(), MaxEventsPerAdvance, w.School.Cats.Day, {}, w.School.School(), unrouted);
    P314Refused(noContext, AwakeningRecover{w.School.Creator.CaptureSave()}, CommandSource::System, "awakening.recovery_unbound",
                "opening needs the trusted countdown context");
    WorldSim sim = P314Sim(w, plain.State());
    S04Accept(sim, AwakeningRecover{w.School.Creator.CaptureSave()}, "the late countdown opens", CommandSource::System);
    P2CExpect(sim.Countdown() && !P314Flag(sim.State(), AwakeningSeededOnRecoveryFlag) &&
                  sim.State().Awakening.Stats == seeded.Awakening.Stats,
              "an already seeded life keeps its inputs; the countdown opens");
    P314Refused(sim, AwakeningRecover{w.School.Creator.CaptureSave()}, CommandSource::System, "awakening.recovery_not_needed",
                "and recovery is then inert");
}

/** The trap fix: a seeded routed life already past the last session start can still pass time without recovery. */
void P314NoTrap(const R94World& w)
{
    WorldSim sim = w.Bound(w.Seeded(R94At(2043, 2, 3, 12, 0), 0x315));
    const CommandResult flows = S04Accept(sim, AdvanceTime{60}, "time passes past the deadline", CommandSource::System);
    P2CExpect(!flows.Partial, "no compulsory supervision can stop a life that has no legal slot left");
}

void P314Reserved(const R94World& w, const WorldSaveData& data)
{
    WorldSim sim = P314Sim(w, data.World);
    P314Refused(sim, SetWorldFlag{std::string(AwakeningPostDeadlineFlag), true}, CommandSource::System, "flag.reserved",
                "the markers cannot be set by hand");
    S04Accept(sim, AwakeningRecover{data.Creation}, "recovery", CommandSource::System);
    WorldState forged = sim.State();
    forged.Flags[std::string(AwakeningPostDeadlineFlag)] = true;
    P2CExpect(!IsAwakeningStateValidInWorld(forged) && SerializeWorldSave({data.Creation, forged, data.Player}).empty(),
              "a forged compatibility marker is refused by the save rules");
    WorldState unseeded = data.World;
    unseeded.Flags[std::string(AwakeningSeededOnRecoveryFlag)] = true;
    P2CExpect(!IsAwakeningStateValidInWorld(unseeded), "a marker on an unseeded life is refused");
    WorldState unknown = sim.State();
    unknown.Flags["awakening.compat.other"] = true;
    P2CExpect(!IsAwakeningStateValidInWorld(unknown), "unknown reserved flags are refused");
}

void P314ReplayMatches(const R94World& w, const WorldSaveData& data)
{
    const WorldState start = P314Legacy(w, data, R94At(2043, 1, 25, 12, 0));
    WorldSim sim = P314Sim(w, start);
    S04Accept(sim, AwakeningRecover{data.Creation}, "recovery", CommandSource::System);
    P314Script(sim);
    const ReplayResult replay = ReplayCommandLogFromSnapshot(start, sim.Log(), P314Replay(w));
    P2CExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()), "replay from the snapshot reaches the same world");
}

void P314FreshProcess(const R94World& w, const WorldSaveData& data, const char* self, const char* contentRoot)
{
    WorldSaveData slot = data;
    slot.World = P314MakeUp(w, data, R94At(2043, 1, 25, 12, 0), true);
    P314Write("p314_restart.save", SerializeWorldSave(slot));
    WorldSaveData here;
    P2CExpect(DeserializeWorldSave(P314Read("p314_restart.save"), here).HashVerified, "the restart save loads here");
    WorldSim continued = P314Sim(w, here.World);
    P314Script(continued);
    here.World = continued.State();
    const std::string expected = SerializeWorldSave(here);
    P2CExpect(!expected.empty(), "the in-process continuation saves");
    std::remove("p314_restart.out");
    std::string command = "\"\"" + std::string(self) + "\" \"" + contentRoot + "\" --continue p314_restart.save p314_restart.out\"";
#ifndef _WIN32
    command = command.substr(1, command.size() - 2);
#endif
    P2CExpect(std::system(command.c_str()) == 0, "the fresh process continues the save");
    P2CExpect(P314Read("p314_restart.out") == expected, "a fresh process reaches exactly the same world and bytes");
}

int P314Child(const R94World& w, const char* in, const char* out)
{
    WorldSaveData data;
    P2CExpect(DeserializeWorldSave(P314Read(in), data).HashVerified, "the child loads the restart save");
    P2CExpect(!PlanAwakeningRecovery(data.World).Needed(), "the child needs no second recovery");
    WorldSim sim = P314Sim(w, data.World);
    P314Script(sim);
    data.World = sim.State();
    P314Write(out, SerializeWorldSave(data));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2 || (argc == 5 && std::string(argv[2]) == "--continue"), "content root [--continue <in> <out>]");
    const P2CFixture fixture(argv[1]);
    const R94World world(fixture);
    if (argc == 5)
        return P314Child(world, argv[3], argv[4]);
    P314LegacyGrant(world, "world_v9_doors_29.json", 9);
    P314LegacyGrant(world, "world_v10_doors_29.json", 10);
    P314LegacyGrant(world, "world_v11_doors_29.json", 11);
    const WorldSaveData v9 = P314Legacy("world_v9_doors_29.json", 9);
    const WorldSaveData v10 = P314Legacy("world_v10_doors_29.json", 10);
    const WorldSaveData v11 = P314Legacy("world_v11_doors_29.json", 11);
    P314MakeUpGrant(world, v11, R94At(2043, 1, 25, 12, 0), true);
    P314MakeUpGrant(world, v10, R94At(2043, 1, 30, 10, 0), false);
    P314MakeUpGrant(world, v9, P314Exam - TutorialSessionMinutes - 1, false);
    P314PostDeadline(world, v9, P314Exam - TutorialSessionMinutes);
    P314PostDeadline(world, v11, R94At(2043, 2, 3, 12, 0));
    P314SeededLate(world);
    P314NoTrap(world);
    P314Reserved(world, v10);
    P314ReplayMatches(world, v11);
    P314FreshProcess(world, v11, argv[0], argv[1]);
    std::cout << "P3-14 LegacyRecovery passed\n";
    return 0;
}
