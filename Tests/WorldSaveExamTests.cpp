#include "WorldExam.h"

#include "P402ExamTestSupport.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace PerfectWorld;

namespace
{
using A = TowerSessionAction;

std::string P402ReadFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    P2CExpect(static_cast<bool>(in), "file readable");
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void P402WriteFile(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    P2CExpect(static_cast<bool>(out), "file writable");
}

/** The second floor open after one beat: the state every continuation test starts from. */
WorldState P402MidBeat(const P402World& w)
{
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    P402Floor(sim, P402Climb[0], "careful");
    S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[1]), TowerMethod::Analysis), "the second floor opens");
    P402Choose(sim, "careful", "one beat");
    return sim.State();
}

/** The rest of the climb from P402MidBeat: two beats, then the last floor. */
void P402FinishScript(WorldSim& sim)
{
    P402Choose(sim, "careful", "beat two");
    P402Choose(sim, "careful", "beat three");
    P402Floor(sim, P402Climb[2], "careful");
    P2CExpect(sim.State().Examination.Participation == TowerParticipation::Completed, "the continuation finishes the climb");
}

void P402RoundTrips(const P402World& w)
{
    std::vector<std::pair<std::string, WorldState>> states;
    states.emplace_back("undecided", w.Morning);
    for (const A exit : {A::Defer, A::Decline})
    {
        WorldSim sim = w.Sim(w.Morning);
        S04Accept(sim, P402Cmd(exit), "an exit before entry");
        states.emplace_back(exit == A::Defer ? "deferred" : "declined", sim.State());
    }
    WorldSim sim = w.Sim(w.Morning);
    S04Accept(sim, P402Cmd(A::Enter), "sit");
    states.emplace_back("entered", sim.State());
    S04Accept(sim, P402Cmd(A::OpenRoute, std::string(P402Climb[0]), TowerMethod::Endurance), "open");
    states.emplace_back("floor open, no beat", sim.State());
    P402Choose(sim, "careful", "beat");
    states.emplace_back("mid-beat", sim.State());
    P402Choose(sim, "careful", "beat");
    P402Choose(sim, "careful", "beat");
    states.emplace_back("floor boundary", sim.State());
    WorldSim withdrawn = w.Sim(P402MidBeat(w));
    S04Accept(withdrawn, P402Cmd(A::Withdraw), "withdraw mid-floor");
    states.emplace_back("withdrawn with an open floor", withdrawn.State());
    WorldSim ejected = w.Sim(sim.State());
    S04Accept(ejected, P402Cmd(A::OpenRoute, std::string(P402Climb[1]), TowerMethod::Analysis), "open");
    for (int i = 0; i < 3 && ejected.State().Examination.Participation == TowerParticipation::Entered; ++i)
        P402Choose(ejected, "rush", "rush");
    states.emplace_back("ejected", ejected.State());
    WorldSim completed = w.Sim(P402MidBeat(w));
    P402FinishScript(completed);
    states.emplace_back("completed", completed.State());
    for (const auto& [name, world] : states)
    {
        const WorldState loaded = P402Reload(w, world, "the examination saves and reloads exactly: " + name);
        WorldSim continued = w.Sim(loaded);
        P2CExpect(continued.ExaminationWorldView().Session.Runner == world.Examination.Runner, "the reloaded view continues");
    }
    P2CExpect(SerialiseExamState(w.Morning) == "pw.examination/1\nparticipation=0\npreparation=0,0,0,0,0,0,0,0\nrunner=1,0\n"
                                                 "attempts=0\nactive=none\nend\n",
              "the undecided examination is the minimal canonical empty text");
}

void P402Continuation(const P402World& w)
{
    const WorldState mid = P402MidBeat(w);
    WorldSim original = w.Sim(mid);
    const std::uint64_t seq = original.State().Seq;
    P402FinishScript(original);
    WorldSim reloaded = w.Sim(P402Reload(w, mid, "mid-beat reload"));
    P402FinishScript(reloaded);
    P2CExpect(HashWorldState(reloaded.State()) == HashWorldState(original.State()) && original.State().Seq == seq + 6,
              "continuing after a reload reaches the same world, one charge, no duplicate outcome");
    // The terminal command resent after a reload is refused: no second outcome, no clock reset or second charge.
    WorldSim again = w.Sim(P402Reload(w, original.State(), "terminal reload"));
    const TowerSessionCommand last = std::get<TowerSessionCommand>(original.Log().back().Payload);
    S04Refused(again, last, "tower.session.closed", "the finishing choice cannot apply twice");
    P2CExpect(again.State().Clock.Now() == original.State().Clock.Now(), "the clock never resets");
}

void P402CodecRules(const P402World& w)
{
    const WorldState mid = P402MidBeat(w);
    const std::string text = SerialiseExamState(mid);
    P2CExpect(!text.empty(), "the mid-beat examination serialises");
    const auto replaced = [&](std::string_view from, std::string_view to) {
        std::string out = text;
        const std::size_t at = out.find(from);
        P2CExpect(at != std::string::npos, "tamper anchor exists");
        out.replace(at, from.size(), to);
        return out;
    };
    std::string composure = text;
    {
        const std::size_t line = composure.find("preparation=");
        const std::size_t end = composure.find('\n', line);
        const std::size_t comma = composure.rfind(',', end);
        const std::size_t prev = composure.rfind(',', comma - 1);
        const int value = std::stoi(composure.substr(prev + 1, comma - prev - 1));
        composure.replace(prev + 1, comma - prev - 1, std::to_string(value == 0 ? 1 : value - 1));
    }
    const std::vector<std::pair<std::string, std::string>> bad = {
        {"non-canonical number", replaced("participation=1", "participation=01")},
        {"unknown participation", replaced("participation=1", "participation=7")},
        {"runner nonce", replaced("runner=3,2", "runner=4,2")},
        {"truncated", text.substr(0, text.size() - 1)},
        {"missing end", replaced("end\n", "")},
        {"extra line", text + "extra\n"},
        {"active on a later floor", replaced("changed_rule,tower.route.changed_rule,", "changed_rule,tower.route.alliance,")},
        {"composure differs from authority", composure},
        {"empty", ""},
    };
    for (const auto& [name, candidate] : bad)
    {
        WorldState target = mid;
        target.Examination = {};
        const TowerSessionState untouched = target.Examination;
        P2CExpect(!DeserialiseExamState(candidate, target) && target.Examination == untouched, "refused: " + name);
    }
    WorldState target = mid;
    target.Examination = {};
    P2CExpect(DeserialiseExamState(text, target) && target.Examination == mid.Examination, "the canonical text decodes exactly");
    WorldState moved = mid;
    moved.Clock = GameClock(); // a different clock: an entered examination stands still at the instant
    P2CExpect(!IsExamStateValidInWorld(moved), "an entered session away from the instant is impossible");
    WorldState forged = mid;
    forged.Examination.Attempts.front().Defeated = true;
    P2CExpect(!IsExamStateValidInWorld(forged) && P402SaveText(w, forged).empty(), "an impossible session is never written");
    WorldState early = w.Before;
    early.Examination.Participation = TowerParticipation::Deferred;
    P2CExpect(!IsExamStateValidInWorld(early), "no decision before the instant");
}

void P402Legacy(const P402World& w, const std::string& dir)
{
    const std::string v12 = P402ReadFile(dir + "/world_v12_doors_29.json");
    WorldSaveData data;
    const WorldSaveResult loaded = DeserializeWorldSave(v12, data);
    P2CExpect(loaded.Succeeded() && loaded.SourceVersion == 12 && !loaded.HashVerified &&
                  data.World.Examination == TowerSessionState{},
              "a frozen v12 save authenticates with hash 9 and migrates to an undecided examination");
    std::string tampered = v12;
    const std::size_t hash = tampered.rfind("\"stateHash\": \"0x");
    P2CExpect(hash != std::string::npos, "hash anchor");
    char& digit = tampered[hash + 16];
    digit = digit == '0' ? '1' : '0';
    WorldSaveData refused;
    P2CExpect(DeserializeWorldSave(tampered, refused).Error == WorldSaveError::HashMismatch,
              "a v12 save whose checksum disagrees is refused before migration");
    const std::size_t close = v12.find("\n  },\n  \"player\"");
    P2CExpect(close != std::string::npos, "world close anchor");
    std::string sneaked = v12;
    sneaked.insert(close, ",\n    \"examination\": \"pw.examination/1\\nparticipation=5\\n\"");
    P2CExpect(DeserializeWorldSave(sneaked, refused).Error == WorldSaveError::Malformed, "a v12 save cannot carry an examination");
    const std::string resaved = SerializeWorldSave(data);
    WorldSaveData again;
    const WorldSaveResult certified = DeserializeWorldSave(resaved, again);
    P2CExpect(!resaved.empty() && certified.Succeeded() && certified.HashVerified && certified.SourceVersion == 13 &&
                  resaved.find("\"examination\": \"pw.examination/1") != std::string::npos,
              "its first v13 save is certified");
    std::string missing = resaved;
    const std::size_t key = missing.find(",\n    \"examination\"");
    missing.erase(key, missing.find("\n  },\n  \"player\"") - key);
    P2CExpect(DeserializeWorldSave(missing, refused).Error == WorldSaveError::Malformed, "v13 requires the examination key");
    (void)w;
}

void P402FreshProcess(const P402World& w, const char* self, const char* contentRoot)
{
    WorldSaveData slot = w.W.Save(P402MidBeat(w));
    P402WriteFile("p402_restart.save", SerializeWorldSave(slot));
    WorldSaveData here;
    P2CExpect(DeserializeWorldSave(P402ReadFile("p402_restart.save"), here).HashVerified, "the mid-examination save loads");
    WorldSim continued = w.Sim(here.World);
    P402FinishScript(continued);
    here.World = continued.State();
    const std::string expected = SerializeWorldSave(here);
    std::remove("p402_restart.out");
    std::string command = "\"\"" + std::string(self) + "\" \"" + contentRoot + "\" --continue p402_restart.save p402_restart.out\"";
#ifndef _WIN32
    command = command.substr(1, command.size() - 2);
#endif
    P2CExpect(std::system(command.c_str()) == 0, "a fresh process continues mid-examination");
    P2CExpect(P402ReadFile("p402_restart.out") == expected, "the fresh process reaches exactly the same world and bytes");
}

int P402Child(const P402World& w, const char* in, const char* out)
{
    WorldSaveData data;
    P2CExpect(DeserializeWorldSave(P402ReadFile(in), data).HashVerified, "the child loads the mid-examination save");
    P2CExpect(data.World.Examination.Participation == TowerParticipation::Entered && data.World.Examination.Active,
              "the child resumes the open floor, never re-entering");
    WorldSim sim = w.Sim(data.World);
    P402FinishScript(sim);
    data.World = sim.State();
    P402WriteFile(out, SerializeWorldSave(data));
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2 || (argc == 5 && std::string(argv[2]) == "--continue"), "content root [--continue <in> <out>]");
    const P2CFixture fixture(argv[1]);
    const P402World world(fixture);
    if (argc == 5)
        return P402Child(world, argv[3], argv[4]);
    P402RoundTrips(world);
    P402Continuation(world);
    P402CodecRules(world);
    P402Legacy(world, PW_EXAM_FIXTURE_DIR);
    P402FreshProcess(world, argv[0], argv[1]);
    std::cout << "P4-02 WorldSaveExam passed\n";
    return 0;
}
