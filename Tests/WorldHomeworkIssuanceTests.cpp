// P3-06: homework issuance routed through WorldSim. Every accepted command issues the bound catalogue's briefs whose
// scheduled boundary lies in exactly the interval the clock moved (one long skip, day-by-day steps, sleep, a paper charge
// and week compression all agree), each brief exactly once; an absent student still retrieves and hands in the work; reading
// never creates work; a refusal (issuance's own or the command's) leaves the whole world unchanged; an event-capped advance
// issues only what it reached; and issued briefs survive a save round trip, a replay and a fresh-process continuation.
#include "P36HomeworkTestSupport.h"

#include <cstdio>
#include <fstream>
#include <iterator>

using namespace PerfectWorld;

namespace
{
std::string P36IRead(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    P2CExpect(static_cast<bool>(in), "a test file is readable");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void P36IWrite(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    P2CExpect(static_cast<bool>(out), "a test file is written");
}

/** Every record is a catalogue brief of this stream, issued at its own boundary, at most once. */
void P36IOnlyScheduled(const P36World& w, const WorldState& state, std::size_t expected, const char* what)
{
    const auto science = w.Science();
    P2CExpect(state.SchoolWork.Records.size() == expected, what);
    for (const auto& record : state.SchoolWork.Records)
    {
        const auto found = std::find_if(science.begin(), science.end(), [&](const auto& b) { return b.Id == record.Brief.Id; });
        P2CExpect(found != science.end() && record.Brief == *found && record.IssuedAt == found->AvailableAt &&
                      !record.StartedAt && !record.SubmittedAt,
                  what);
    }
}

void P36IBoundaryAndLongSkip(const P36World& w)
{
    const auto science = w.Science();
    const auto& first = science.front();
    WorldSim sim = w.Sim(0x3601);
    P36To(sim, first.AvailableAt - 1);
    P36IOnlyScheduled(w, sim.State(), 0, "nothing is issued before the first boundary");
    // Reading the desk never creates work.
    const std::uint64_t before = HashWorldState(sim.State());
    const auto desk = QuerySchoolAcademicRecord(sim.State().SchoolWork, w.F.Papers, sim.State().Clock.Now());
    P2CExpect(desk.Work.empty() && HashWorldState(sim.State()) == before, "opening the desk early creates nothing");
    S04Accept(sim, AdvanceTime{1}, "the clock reaches the boundary");
    P36IOnlyScheduled(w, sim.State(), 1, "the brief issues exactly at its boundary (inclusive end)");
    S04Accept(sim, AdvanceTime{60}, "the next hour");
    P36IOnlyScheduled(w, sim.State(), 1, "a later interval starting at the boundary does not issue it again");

    // One long skip over every science brief, against a fresh life moving day by day: the same records.
    const GameInstant end = science.back().AvailableAt + 2 * MinutesPerDay;
    WorldSim skip = w.Sim(0x3601);
    P36To(skip, end);
    P36IOnlyScheduled(w, skip.State(), science.size(), "one long skip issues every science brief once");
    WorldSim steps = w.Sim(0x3601);
    while (steps.State().Clock.Now() < end)
        S04Accept(steps, AdvanceTime{std::min<GameInstant>(MinutesPerDay, end - steps.State().Clock.Now())}, "a day passes",
                  CommandSource::System);
    P2CExpect(steps.State().SchoolWork == skip.State().SchoolWork, "day-by-day time issues exactly what the long skip issued");
    S04Accept(skip, AdvanceTime{7 * MinutesPerDay}, "a further week", CommandSource::System);
    P36IOnlyScheduled(w, skip.State(), science.size(), "nothing issues twice after the last brief");
}

void P36ISleep(const P36World& w)
{
    const auto first = w.Science().front();
    WorldSim sim = w.Sim(0x3602);
    P36To(sim, first.AvailableAt - 85);
    DayCommand sleep;
    sleep.Kind = DayCommandKind::GoToSleep;
    S04Accept(sim, WorldDayCommand{sleep}, "an afternoon nap begins");
    P36IOnlyScheduled(w, sim.State(), 0, "falling asleep moves no time and issues nothing");
    DayCommand wake;
    wake.Kind = DayCommandKind::Wake;
    wake.Minutes = 600;
    const auto woke = S04Accept(sim, WorldDayCommand{wake}, "the nap ends");
    P2CExpect(woke.From < first.AvailableAt && woke.To > first.AvailableAt, "the sleep spans the boundary");
    P36IOnlyScheduled(w, sim.State(), 1, "the brief issues once, at its boundary, across a sleep");
}

void P36IWeek(const P36World& w)
{
    const auto first = w.Science().front();
    WorldSim sim = w.Sim(0x3603);
    const WeekPlan base = P36KnownWeek(w, sim);
    bool spanned = false;
    for (int guard = 0; guard < 4 && sim.State().Clock.Now() <= first.AvailableAt; ++guard)
    {
        P36To(sim, P36NextMorning(sim.State().Clock.Now()));
        const auto resolved = S04Accept(sim, SchoolWeekResolve{base}, "a known week resolves");
        spanned = spanned || (resolved.From < first.AvailableAt && resolved.To > first.AvailableAt);
        P2CExpect(sim.LastSchoolWeekResult() && sim.LastSchoolWeekResult()->Stop == WeekStop::None, "the week compresses fully");
    }
    P2CExpect(spanned, "one compressed week spans the brief's boundary");
    P36IOnlyScheduled(w, sim.State(), 1, "week compression issues the brief once, at its own boundary");
}

void P36IAbsent(const P36World& w)
{
    const auto science = w.Science();
    const auto& first = science.front();
    WorldSim sim = w.Sim(0x3604);
    P36To(sim, first.DueAt + MinutesPerDay);
    const auto& attendance = sim.State().DayLoop.Attendance;
    P2CExpect(attendance.OnTimeDays + attendance.LateDays + attendance.VeryLateDays == 0, "the student never went to school");
    const auto desk = QuerySchoolAcademicRecord(sim.State().SchoolWork, w.F.Papers, sim.State().Clock.Now());
    const auto view = std::find_if(desk.Work.begin(), desk.Work.end(), [&](const auto& v) { return v.Brief.Id == first.Id; });
    P2CExpect(view != desk.Work.end() && view->Available && view->Overdue && view->CanBegin,
              "an absent student retrieves the issued brief; it is overdue but can still begin");
    P36AnswerPaper(sim, first.Id);
    S04Accept(sim, P36Work(first.Id, SchoolWorkActionKind::Submit), "the late paper is handed in");
    const auto after = QuerySchoolAcademicRecord(sim.State().SchoolWork, w.F.Papers, sim.State().Clock.Now());
    const auto late = std::find_if(after.Work.begin(), after.Work.end(), [&](const auto& v) { return v.Brief.Id == first.Id; });
    P2CExpect(late != after.Work.end() && late->LateSubmission && late->Marking.has_value() &&
                  P36Record(sim.State(), first.Id)->SubmittedAt.has_value(),
              "late recovery is recorded with its marking and the lateness disclosed");
}

void P36IPartialAndRollback(const P36World& w)
{
    const auto first = w.Science().front();
    WorldSim base = w.Sim(0x3605);
    P36To(base, first.AvailableAt - 30);
    const WorldState before = base.State();

    // An event-capped advance issues only what it actually reached; the continuation issues the rest.
    WorldSim capped = w.Continue(before, 2);
    for (int i = 0; i < 3; ++i)
        S04Accept(capped, ScheduleWorldEvent{first.AvailableAt - 20 + 5 * i, "test/p36/cap" + std::to_string(i)}, "an event",
                  CommandSource::System);
    const auto partial = S04Accept(capped, AdvanceTime{60}, "a capped advance");
    P2CExpect(partial.Partial && partial.To < first.AvailableAt && capped.State().SchoolWork.Records.empty(),
              "the capped advance stops short and issues nothing it did not reach");
    S04Accept(capped, AdvanceTime{first.AvailableAt + 10 - capped.State().Clock.Now()}, "the continuation");
    P36IOnlyScheduled(w, capped.State(), 1, "the continuation issues the brief once");

    // A paper hand-in whose charge is interrupted crosses the boundary: the whole command, issuance included, is refused.
    WorldState withPaper = before;
    P2CExpect(IssueSchoolWork(withPaper.SchoolWork, w.School.Brief("school.work.capped", before.Clock.Now()), w.F.Papers,
                              before.Clock.Now()).Ok(),
              "a test brief is issued");
    WorldSim paper = w.Continue(withPaper, 2);
    S04Accept(paper, P36Work("school.work.capped", SchoolWorkActionKind::Begin), "the paper begins");
    const WorldState open = paper.State();
    for (int i = 1; i <= 4; ++i)
        S04Accept(paper, ScheduleWorldEvent{open.Clock.Now() + 10 + i, "test/p36/paper" + std::to_string(i)}, "events inside the paper",
                  CommandSource::System);
    S04Refused(paper, P36Work("school.work.capped", SchoolWorkActionKind::Submit), "school.work.charge_interrupted",
               "an interrupted hand-in refuses and restores the world");
    P2CExpect(paper.State().SchoolWork.Records.size() == 1, "the refused charge issued nothing");
    WorldSim handed = w.Continue(open);
    const auto sent = S04Accept(handed, P36Work("school.work.capped", SchoolWorkActionKind::Submit), "the hand-in");
    P2CExpect(sent.From < first.AvailableAt && sent.To > first.AvailableAt && handed.State().SchoolWork.Records.size() == 2 &&
                  P36Record(handed.State(), first.Id)->IssuedAt == first.AvailableAt,
              "the accepted 90-minute charge issues the brief it crossed, in the same command");

    // Issuance's own refusal (a saved record under the brief's ID that differs from the catalogue) rolls everything back.
    WorldState conflict = before;
    SchoolWorkBrief altered = first;
    altered.AvailableAt = before.Clock.Now();
    altered.DueAt = first.DueAt + 60;
    P2CExpect(IssueSchoolWork(conflict.SchoolWork, altered, w.F.Papers, before.Clock.Now()).Ok(), "a conflicting record is planted");
    WorldSim refused = w.Continue(conflict);
    const std::uint64_t seq = refused.State().Seq;
    S04Refused(refused, AdvanceTime{60}, "school.work.issue.identity_conflict", "a conflicting issued ID refuses the whole command");
    P2CExpect(refused.State().Seq == seq && refused.Log().empty(), "nothing of the refused command is counted");
}

/** Saves, loads certified and requires the issued records and the whole world back exactly. */
WorldSaveData P36IRoundTrip(const P36World& w, const WorldState& world)
{
    const std::string text = SerializeWorldSave(w.School.Save(world));
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    P2CExpect(!text.empty() && result.Succeeded() && result.HashVerified && !result.Migrated, "the world with issued briefs saves and loads");
    P2CExpect(loaded.World.SchoolWork == world.SchoolWork && HashWorldState(loaded.World) == HashWorldState(world) &&
                  SerializeWorldSave(loaded) == text,
              "issued briefs round-trip exactly");
    return loaded;
}

/** The continuation the in-process run and the fresh process both play: cross the next brief, hand in the first late. */
void P36IScript(const P36World& w, WorldSim& sim)
{
    const auto science = w.Science();
    P36To(sim, science[1].AvailableAt + 60);
    P36AnswerPaper(sim, science[0].Id);
    S04Accept(sim, P36Work(science[0].Id, SchoolWorkActionKind::Submit), "the first brief is handed in late");
    S04Accept(sim, AdvanceTime{MinutesPerDay}, "a further day");
}

void P36IPersistence(const P36World& w, const char* self, const char* contentRoot)
{
    const auto science = w.Science();
    WorldSim sim = w.Sim(0x3606);
    const WorldState initial = sim.State();
    P36To(sim, science[0].AvailableAt + MinutesPerDay);
    P36IOnlyScheduled(w, sim.State(), 1, "one brief is issued before the save");
    const WorldSaveData loaded = P36IRoundTrip(w, sim.State());

    WorldSim straight = w.Continue(sim.State());
    P36IScript(w, straight);
    WorldSim resumed = w.Continue(loaded.World);
    P36IScript(w, resumed);
    P2CExpect(HashWorldState(straight.State()) == HashWorldState(resumed.State()) && resumed.State().SchoolWork.Records.size() == 2 &&
                  P36Record(resumed.State(), science[0].Id)->SubmittedAt.has_value() &&
                  P36Record(resumed.State(), science[1].Id)->IssuedAt == science[1].AvailableAt,
              "a reload issues nothing twice and changes nothing about what happens next");
    P36IRoundTrip(w, resumed.State());

    // The log replays to the same world with the same binding.
    P36IScript(w, sim);
    const ReplayResult replay = ReplayCommandLogFromSnapshot(initial, sim.Log(), w.Replay());
    P2CExpect(replay.Complete && replay.Hash == HashWorldState(sim.State()), "the command log replays issuance exactly");
    // An unbound sim (an old caller) issues nothing: production must bind the catalogue.
    WorldSim unbound = w.School.Continue(loaded.World);
    S04Accept(unbound, AdvanceTime{science[1].AvailableAt + 60 - unbound.State().Clock.Now()}, "unbound time");
    P2CExpect(unbound.State().SchoolWork == loaded.World.SchoolWork, "a sim without the catalogue issues nothing");

    // A fresh process continues the same save to exactly the same bytes.
    P36IWrite("p36_restart.save", SerializeWorldSave(w.School.Save(loaded.World)));
    WorldSaveData here;
    P2CExpect(DeserializeWorldSave(P36IRead("p36_restart.save"), here).HashVerified, "the restart save loads here");
    WorldSim continued = w.Continue(here.World);
    P36IScript(w, continued);
    here.World = continued.State();
    const std::string expected = SerializeWorldSave(here);
    P2CExpect(!expected.empty(), "the in-process continuation saves");
    std::remove("p36_restart.out");
    std::string command = "\"\"" + std::string(self) + "\" \"" + contentRoot + "\" --continue p36_restart.save p36_restart.out\"";
#ifndef _WIN32
    command = command.substr(1, command.size() - 2);
#endif
    P2CExpect(std::system(command.c_str()) == 0, "the fresh process continues the save");
    P2CExpect(P36IRead("p36_restart.out") == expected, "a fresh process reaches exactly the same world and bytes");
}

int P36IChild(const P36World& w, const char* in, const char* out)
{
    WorldSaveData data;
    P2CExpect(DeserializeWorldSave(P36IRead(in), data).HashVerified, "the child loads the restart save");
    WorldSim sim = w.Continue(data.World);
    P36IScript(w, sim);
    data.World = sim.State();
    P36IWrite(out, SerializeWorldSave(data));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2 || (argc == 5 && std::string(argv[2]) == "--continue"), "content root [--continue <in> <out>]");
    const P2CFixture fixture(argv[1]);
    const P36World world(fixture);
    if (argc == 5)
        return P36IChild(world, argv[3], argv[4]);
    P36IBoundaryAndLongSkip(world);
    P36ISleep(world);
    P36IWeek(world);
    P36IAbsent(world);
    P36IPartialAndRollback(world);
    P36IPersistence(world, argv[0], argv[1]);
    std::cout << "P3-06 WorldHomeworkIssuance passed\n";
    return 0;
}
