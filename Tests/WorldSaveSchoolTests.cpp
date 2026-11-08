// S04: pw.school/1 persistence - save round trips at every school state, continuation after reload, canonical-only and
// rule-checked decoding, the frozen v10 fixture (written by the unchanged generator built against ef218b8, schema 10, hash 7)
// migrating to empty school slices, and a fresh-process restart that must reach the same bytes as the in-process run.
// world_v10_doors_29_migrated_v11.json is the expected migrated result; PW_WRITE_MIGRATED=<path> writes it (only when the
// migration is meant to change).
#include "WorldSave.h"

#include "Doors.h"
#include "S04SchoolTestSupport.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

using namespace PerfectWorld;

namespace
{
std::string S04ReadFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    P2CExpect(static_cast<bool>(in), "a test file is readable");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void S04WriteFile(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    P2CExpect(static_cast<bool>(out), "a test file is written");
}

std::string S04Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    if (at == std::string::npos)
        std::cerr << "missing: " << from << '\n';
    P2CExpect(at != std::string::npos, "the test text contains the edited part");
    return text.replace(at, from.size(), to);
}

WorldSaveError S04ErrorOf(const std::string& text)
{
    WorldSaveData out;
    return DeserializeWorldSave(text, out).Error;
}

std::string S04Hex(std::uint64_t value)
{
    char buffer[19];
    std::snprintf(buffer, sizeof buffer, "0x%016llx", static_cast<unsigned long long>(value));
    return buffer;
}

/** Saves, loads (certified v11), and requires the school slices and the whole world back exactly, and the same bytes again. */
WorldState S04RoundTrip(const S04World& w, const WorldState& world, const char* what)
{
    const std::string text = SerializeWorldSave(w.Save(world));
    P2CExpect(!text.empty() && text.find("\"schemaVersion\": 11,") != std::string::npos && text.find("\"hashVersion\": 8,") != std::string::npos &&
                  text.find("\"school\": \"pw.school/1\\n") != std::string::npos,
              what);
    WorldSaveData back;
    const WorldSaveResult result = DeserializeWorldSave(text, back);
    P2CExpect(result.Succeeded() && result.HashVerified && result.SourceVersion == 11, what);
    P2CExpect(back.World.SchoolSessions == world.SchoolSessions && back.World.SchoolWork == world.SchoolWork &&
                  SerialiseSchoolState(back.World) == SerialiseSchoolState(world) && back.World.DayLoop == world.DayLoop &&
                  HashWorldState(back.World) == HashWorldState(world) && SerializeWorldSave(back) == text,
              what);
    return back.World;
}

/** The original and the reloaded sim take the same command and stay identical. */
void S04Twin(WorldSim& a, WorldSim& b, const CommandPayload& payload, const char* what)
{
    const CommandResult ra = a.Submit({CommandSource::Player, payload});
    const CommandResult rb = b.Submit({CommandSource::Player, payload});
    P2CExpect(ra.Accepted && rb.Accepted && HashWorldState(a.State()) == HashWorldState(b.State()), what);
}

/** The continuation both the in-process run and the fresh process play from the restart save. */
void S04Script(const S04World& w, WorldSim& sim)
{
    S04PlayLesson(sim, w.Cats);
    S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "the restarted lesson is recorded");
    SchoolWorkCommand work;
    work.WorkId = "school.work.restart";
    S04Accept(sim, work, "the paper begins after the restart");
    work.Kind = SchoolWorkActionKind::Survey;
    S04Accept(sim, work, "the paper is surveyed");
    work.Kind = SchoolWorkActionKind::Submit;
    S04Accept(sim, work, "the paper is handed in");
    SchoolStandingOrder order;
    order.Id = "order.after_restart";
    order.Weekdays = 0x3e;
    S04Accept(sim, SchoolWeekOrder{order, false}, "a standing order after the restart");
}

void S04SaveLessons(const S04World& w)
{
    WorldSim sim = w.Sim(0x5401);
    const WorldState fresh = S04RoundTrip(w, sim.State(), "an empty school saves and loads");
    P2CExpect(fresh.SchoolSessions == SchoolSessionsState{} && fresh.SchoolWork == SchoolWorkState{} && fresh.SchoolWeek.Orders.empty(),
              "a new life has empty school slices");
    S04Morning(sim, w.Cats);
    S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, 0));
    const SchoolLessonBinding& first = w.Binding(0);
    S04Accept(sim, SchoolLessonOpen{first.Id, SchoolLessonMode::Foreground, S04Goal(first.Structure)}, "the lesson opens");
    S04RoundTrip(w, sim.State(), "a just-opened foreground lesson saves");
    S04ChooseOnce(sim, w.Cats);
    const WorldState mid = S04RoundTrip(w, sim.State(), "a lesson mid-exposure saves");
    WorldSim reloaded = w.Continue(mid);
    for (int guard = 0; guard < 32 && !sim.State().SchoolSessions.Active->ReadyToClose; ++guard)
    {
        const std::uint64_t session = sim.State().SchoolSessions.Active->Session;
        S04Twin(sim, reloaded, SchoolLessonChoose{session, S04Standard(sim.State().SchoolSessions, w.Cats)}, "the reloaded lesson continues");
    }
    S04RoundTrip(w, sim.State(), "a ready foreground lesson saves before its paid close");
    const std::uint64_t session = sim.State().SchoolSessions.Active->Session;
    S04Twin(sim, reloaded, SchoolLessonClose{session, false, 0}, "the reloaded lesson records identically");
    S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, 1));
    S04PassTo(reloaded, P2CBellMinute(*w.Cats.Day.School, 1));
    S04Twin(sim, reloaded, SchoolLessonOpen{w.Binding(1).Id, SchoolLessonMode::Attend, {}}, "attend opens in both");
    const WorldState attending = S04RoundTrip(w, sim.State(), "an open attend lesson saves");
    WorldSim again = w.Continue(attending);
    S04Refused(again, SchoolLessonClose{session, false, 0}, "school.session.stale_session", "a close from before the reload is stale");
    S04Accept(again, SchoolLessonClose{session + 1, true, 3}, "the reloaded attend lesson can be left early");
    S04RoundTrip(w, again.State(), "a cancelled period saves");
}

void S04SavePapers(const S04World& w)
{
    WorldState state = MakeWorldState(0x5402, w.Creator);
    const GameInstant now = state.Clock.Now();
    for (const char* id : {"school.work.one", "school.work.two"})
        P2CExpect(IssueSchoolWork(state.SchoolWork, w.Brief(id, now), w.F.Papers, now).Ok(), "a brief is issued");
    WorldSim sim = w.Continue(state);
    S04RoundTrip(w, sim.State(), "issued briefs save");
    SchoolWorkCommand one;
    one.WorkId = "school.work.one";
    S04Accept(sim, one, "paper one begins");
    one.Kind = SchoolWorkActionKind::Survey;
    S04Accept(sim, one, "paper one is surveyed");
    const WorldState open = S04RoundTrip(w, sim.State(), "an open paper saves at its paused instant");
    WorldSim reloaded = w.Continue(open);
    one.Kind = SchoolWorkActionKind::Submit;
    S04Twin(sim, reloaded, one, "the reloaded paper hands in identically");
    S04RoundTrip(w, sim.State(), "a submitted paper saves");
    SchoolWorkCommand two;
    two.WorkId = "school.work.two";
    S04Accept(sim, two, "paper two begins");
    two.Kind = SchoolWorkActionKind::Cancel;
    S04Accept(sim, two, "paper two is set aside");
    S04RoundTrip(w, sim.State(), "a set-aside draft saves");

    // The writer refuses a world whose open paper is not at the world clock.
    WorldState broken = open;
    P2CExpect(!broken.Clock.AdvanceTo(broken.Clock.Now() + 5).Rejected, "the clock moves in the test copy");
    broken.DayLoop.Now = broken.Clock.Now();
    broken.Commerce.AllowanceSettledThrough = broken.Clock.Now();
    P2CExpect(SerializeWorldSave(w.Save(broken)).empty() && !IsSchoolStateValidInWorld(broken), "an impossible open paper is never written");
}

/** A week of real school, a paper and an order: the rich save the decoder and the tamper checks run over. */
std::string S04RichSave(const S04World& w)
{
    WorldSim sim = w.Sim(0x5403);
    S04Morning(sim, w.Cats);
    for (std::int32_t period = 0; period < BellPeriodCount(*w.Cats.Day.School); ++period)
    {
        S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, period));
        S04Accept(sim, SchoolLessonOpen{w.Binding(period).Id, SchoolLessonMode::Attend, {}}, "a first lesson opens");
        S04Accept(sim, SchoolLessonClose{sim.State().SchoolSessions.Active->Session, false, 0}, "a first lesson closes");
    }
    S04Accept(sim, AdvanceTime{CalendarDayStartInstant(7) + 360 - sim.State().Clock.Now()}, "to next Monday morning");
    SchoolStandingOrder order;
    order.Id = "order.school";
    S04Accept(sim, SchoolWeekOrder{order, false}, "a standing order");
    WeekPlan base;
    base.Days.resize(7);
    S04Accept(sim, SchoolWeekResolve{base}, "the week resolves");
    P2CExpect(sim.LastSchoolWeekResult()->Stop == WeekStop::None, "the whole week compressed");
    const WorldState week = S04RoundTrip(w, sim.State(), "a compressed week saves");
    return SerializeWorldSave(w.Save(week));
}

void S04Tamper(const S04World& w)
{
    const std::string text = S04RichSave(w);
    WorldSaveData loaded;
    P2CExpect(DeserializeWorldSave(text, loaded).HashVerified, "the rich save is certified");
    const std::string school = SerialiseSchoolState(loaded.World);
    P2CExpect(school.find("period=0,0,") != std::string::npos, "the school text records the first day");
    for (std::size_t cut = 0; cut < school.size(); ++cut)
    {
        WorldState target = loaded.World;
        P2CExpect(!DeserialiseSchoolState(school.substr(0, cut), target), "every prefix of the school text is refused");
    }
    for (std::size_t cut = 0; cut < text.size(); cut += 7)
        P2CExpect(S04ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "prefixes of the save are refused");
    WorldState target = loaded.World;
    P2CExpect(!DeserialiseSchoolState(S04Replace(school, "orders=", "extra=1\norders="), target) &&
                  !DeserialiseSchoolState(S04Replace(school, "next_session=", "next_session=0"), target) &&
                  !DeserialiseSchoolState(school + "x", target),
              "unknown lines, non-canonical numbers and trailing text are refused");
    const std::string session = "next_session=" + std::to_string(loaded.World.SchoolSessions.NextSession) + "\\n";
    const std::string bumped = "next_session=" + std::to_string(loaded.World.SchoolSessions.NextSession + 1) + "\\n";
    P2CExpect(S04ErrorOf(S04Replace(text, session, bumped)) == WorldSaveError::HashMismatch, "a plausible edit fails the checksum");
    P2CExpect(S04ErrorOf(S04Replace(text, ",attend,1,0,", ",attend,1,1,")) == WorldSaveError::InvalidWorld,
              "a record both completed and cancelled is refused");
    P2CExpect(S04ErrorOf(S04Replace(text, "\\nperiod=0,0,", "\\nperiod=0,1,")) == WorldSaveError::InvalidWorld,
              "a duplicated (day, period) is refused");
    P2CExpect(S04ErrorOf(S04Replace(text, "order=order.school,62,1,attend", "order=order.school,63,1,attend")) == WorldSaveError::InvalidWorld,
              "a standing order on Sunday is refused");
    const std::size_t key = text.find(",\n    \"school\": ");
    const std::size_t end = text.find("\n  }", key);
    P2CExpect(key != std::string::npos && end != std::string::npos, "the school member is found");
    P2CExpect(S04ErrorOf(text.substr(0, key) + text.substr(end)) == WorldSaveError::Malformed, "a v11 world must have its school");
}

void S04Legacy(const S04World& w)
{
    const std::string dir = PW_SCHOOL_FIXTURE_DIR;
    const std::string text = S04ReadFile(dir + "/world_v10_doors_29.json");
    P2CExpect(text.size() == 4976 && text.find("\"schemaVersion\": 10,") != std::string::npos &&
                  text.find("\"hashVersion\": 7,") != std::string::npos && text.find("pw.school/1") == std::string::npos &&
                  text.find("\"stateHash\": \"0xbbf29a27855841cd\"") != std::string::npos,
              "the frozen v10 fixture is the untouched historical writer's output");
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    P2CExpect(result.Succeeded() && result.SourceVersion == 10 && !result.HashVerified && !result.Migrated,
              "frozen v10 authenticates its hash-7 checksum and loads honestly unverified");
    P2CExpect(loaded.World.SchoolSessions == SchoolSessionsState{} && loaded.World.SchoolWork == SchoolWorkState{} &&
                  loaded.World.SchoolWeek.Orders.empty() && loaded.World.SchoolWeek.SceneDays.empty(),
              "a v10 world migrates to empty school slices");
    P2CExpect(loaded.World.Clock.Now() == 760 && loaded.World.Seq == 13 && loaded.World.Conversations.size() == 1 &&
                  loaded.World.Commerce.AllowanceSettledThrough == 760 && loaded.World.Commerce.Funding.HouseholdResourcesRank == 1,
              "the rest of the v10 world, commerce included, is kept as written");
    const std::string migrated = SerializeWorldSave(loaded);
    if (const char* path = std::getenv("PW_WRITE_MIGRATED"); path != nullptr && path[0] != '\0')
        S04WriteFile(path, migrated);
    const std::string frozen = S04ReadFile(dir + "/world_v10_doors_29_migrated_v11.json");
    P2CExpect(migrated == frozen, "the migration produces exactly the frozen expected v11 save");
    WorldSaveData frozenLoaded;
    const WorldSaveResult frozenResult = DeserializeWorldSave(frozen, frozenLoaded);
    P2CExpect(frozenResult.Succeeded() && frozenResult.HashVerified && frozenResult.SourceVersion == 11 &&
                  HashWorldState(frozenLoaded.World) == HashWorldState(loaded.World),
              "the expected migrated save is a certified v11 world");
    for (std::size_t cut = 0; cut < text.size(); ++cut)
        P2CExpect(S04ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "every prefix of the v10 fixture is refused");
    P2CExpect(S04ErrorOf(S04Replace(text, "home_front,1,-1", "home_front,1,1")) == WorldSaveError::HashMismatch &&
                  S04ErrorOf(S04Replace(text, "\"stateHash\": \"0xbbf29a27855841cd\"", "\"stateHash\": \"0xbbf29a27855841ce\"")) ==
                      WorldSaveError::HashMismatch &&
                  S04ErrorOf(S04Replace(text, "wallet=", "wallet=1")) != WorldSaveError::None,
              "the historical world, commerce included, is authenticated before migrating");
    P2CExpect(S04ErrorOf(S04Replace(text, "\"commerce\": ", "\"school\": \"pw.school/1\\n\",\n    \"commerce\": ")) == WorldSaveError::Malformed,
              "a v10 record cannot smuggle in school state");
    P2CExpect(S04ErrorOf(S04Replace(text, "\"schemaVersion\": 10,", "\"schemaVersion\": 11,")) == WorldSaveError::Malformed,
              "a v10 record relabelled v11 lacks its school");
    WorldSaveData relabelled;
    const WorldSaveResult relabelledResult = DeserializeWorldSave(S04Replace(text, "\"hashVersion\": 7,", "\"hashVersion\": 8,"), relabelled);
    P2CExpect(relabelledResult.Succeeded() && !relabelledResult.HashVerified, "relabelling the hash version never earns certification");

    // The commerce era's own v10 output and the v9 fixture also load with empty school slices.
    for (const char* name : {"world_v9_doors_29_migrated_v10.json", "world_v9_doors_29.json"})
    {
        WorldSaveData old;
        const WorldSaveResult oldResult = DeserializeWorldSave(S04ReadFile(dir + "/" + name), old);
        P2CExpect(oldResult.Succeeded() && !oldResult.HashVerified && old.World.SchoolSessions == SchoolSessionsState{},
                  "an earlier fixture loads authenticated, unverified, with empty school slices");
    }

    // A v10 save of a life that attended lessons: re-encoded as v10 (hash 7, no school), it still loads with EMPTY slices.
    WorldSaveData lived;
    P2CExpect(DeserializeWorldSave(S04RichSave(w), lived).HashVerified, "a lived save");
    std::string v10 = SerializeWorldSave(lived);
    const std::size_t key = v10.find(",\n    \"school\": ");
    v10 = v10.substr(0, key) + v10.substr(v10.find("\n  }", key));
    v10 = S04Replace(v10, "\"schemaVersion\": 11,", "\"schemaVersion\": 10,");
    v10 = S04Replace(v10, "\"hashVersion\": 8,", "\"hashVersion\": 7,");
    const std::uint64_t old = HashWorldStateWithDoorsText(lived.World, SerialiseDoorState(lived.World.Doors), 7);
    v10 = S04Replace(v10, "\"stateHash\": \"" + S04Hex(HashWorldState(lived.World)) + "\"", "\"stateHash\": \"" + S04Hex(old) + "\"");
    WorldSaveData migratedLife;
    const WorldSaveResult lifeResult = DeserializeWorldSave(v10, migratedLife);
    P2CExpect(lifeResult.Succeeded() && !lifeResult.HashVerified && migratedLife.World.SchoolSessions == SchoolSessionsState{} &&
                  migratedLife.World.DayLoop == lived.World.DayLoop && !migratedLife.World.DayLoop.Attendance.Days.empty(),
              "attendance and receipts are never evidence of a lesson: the migrated life starts with no school history");
    P2CExpect(DeserializeWorldSave(SerializeWorldSave(migratedLife), migratedLife).HashVerified, "its first v11 save is certified");
}

void S04FreshProcess(const S04World& w, const char* self, const char* contentRoot)
{
    WorldSim sim = w.Sim(0x5404);
    S04Morning(sim, w.Cats);
    S04PassTo(sim, P2CBellMinute(*w.Cats.Day.School, 0));
    const SchoolLessonBinding& first = w.Binding(0);
    S04Accept(sim, SchoolLessonOpen{first.Id, SchoolLessonMode::Foreground, S04Goal(first.Structure)}, "the lesson opens");
    S04ChooseOnce(sim, w.Cats);
    WorldState state = sim.State();
    P2CExpect(IssueSchoolWork(state.SchoolWork, w.Brief("school.work.restart", state.Clock.Now()), w.F.Papers, state.Clock.Now()).Ok(),
              "a brief for after the restart");
    S04WriteFile("s04_restart.save", SerializeWorldSave(w.Save(state)));

    WorldSaveData here;
    P2CExpect(DeserializeWorldSave(S04ReadFile("s04_restart.save"), here).HashVerified, "the restart save loads here");
    WorldSim continued = w.Continue(here.World);
    S04Script(w, continued);
    here.World = continued.State();
    const std::string expected = SerializeWorldSave(here);
    P2CExpect(!expected.empty(), "the in-process continuation saves");

    std::remove("s04_restart.out");
    std::string command = "\"\"" + std::string(self) + "\" \"" + contentRoot + "\" --continue s04_restart.save s04_restart.out\"";
#ifndef _WIN32
    command = command.substr(1, command.size() - 2);
#endif
    P2CExpect(std::system(command.c_str()) == 0, "the fresh process continues the save");
    P2CExpect(S04ReadFile("s04_restart.out") == expected, "a fresh process reaches exactly the same world and bytes");
}

int S04Child(const S04World& w, const char* in, const char* out)
{
    WorldSaveData data;
    P2CExpect(DeserializeWorldSave(S04ReadFile(in), data).HashVerified, "the child loads the restart save");
    WorldSim sim = w.Continue(data.World);
    S04Script(w, sim);
    data.World = sim.State();
    S04WriteFile(out, SerializeWorldSave(data));
    return 0;
}

void S04HashSection(const S04World& w)
{
    const WorldState empty = MakeWorldState(0x5405, w.Creator);
    WorldState ordered = empty;
    SchoolStandingOrder order;
    order.Id = "order.hash";
    P2CExpect(SetSchoolStandingOrder(ordered.SchoolWeek, order).Ok(), "an order in the test copy");
    // The empty envelope, pinned literally (independently of the encoder): what every new and migrated world hashes.
    P2CExpect(SerialiseSchoolState(empty) ==
                  "pw.school/1\nnext_session=1\nactive=none\nperiods=0\n"
                  "model=55\npw.structure_model/1\nfamilies=0\nexposure=-\nlinks=-\nend\n"
                  "explain=68\npw.structure_explain/1\nfamilies=0\nexposure=-\nclaim=-\nevidence=-\nend\n"
                  "investigate=61\npw.structure_investigate/1\nfamilies=0\nexposure=-\ntests=-\nend\n"
                  "coordinate=85\npw.structure_coordinate/1\nfamilies=0\nexposure=-\nassign=-\nheard=-\nresolution=none\nend\n"
                  "work=0\norders=0\nscene_days=0\nend\n",
              "the empty pw.school/1 envelope is exactly its documented form");
    const auto a = HashWorldSections(empty);
    const auto b = HashWorldSections(ordered);
    P2CExpect(a.size() == 11 && a.back().Name == "school" && WorldHashVersion == 8, "the school section is the eleventh, hash v8");
    for (std::size_t i = 0; i < a.size(); ++i)
        P2CExpect((a[i].Hash != b[i].Hash) == (a[i].Name == "school"), "a school change moves only the school section");
    P2CExpect(HashWorldState(empty) != HashWorldState(ordered) &&
                  HashWorldStateWithDoorsText(empty, SerialiseDoorState(empty.Doors), 7) ==
                      HashWorldStateWithDoorsText(ordered, SerialiseDoorState(ordered.Doors), 7),
              "hash v7 (historical v10) never covers the school");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2 || (argc == 5 && std::string(argv[2]) == "--continue"), "content root [--continue <in> <out>]");
    const P2CFixture fixture(argv[1]);
    const S04World world(fixture);
    if (argc == 5)
        return S04Child(world, argv[3], argv[4]);
    S04HashSection(world);
    S04SaveLessons(world);
    S04SavePapers(world);
    S04Tamper(world);
    S04Legacy(world);
    S04FreshProcess(world, argv[0], argv[1]);
    std::cout << "S04 WorldSaveSchool passed\n";
    return 0;
}
