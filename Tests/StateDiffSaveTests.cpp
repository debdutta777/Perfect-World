// ROADMAP M3-02: the diff of whole saves (the world, the placement, the creation record) and the
// pwstatediff command line.
#include "WeekTestSupport.h"

#include "CharacterCreation.h"
#include "WorldSave.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#ifndef _WIN32
#include <sys/wait.h>
#endif

using namespace PerfectWorld;

namespace
{
CharacterCreationSave WeekSaveCreation()
{
    CharacterCreator creator;
    WeekExpect(creator.ApplyPreset("balanced_scholar").Succeeded(), "preset applies");
    WeekExpect(creator.SetHouseholdTemplate("parent_household").Succeeded(), "household applies");
    WeekExpect(creator.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded(), "identity applies");
    WeekExpect(creator.StartLife().Succeeded(), "life starts");
    return creator.CaptureSave();
}

WorldSaveData WeekSaveOf(const WorldState& world)
{
    WorldSaveData data;
    WeekExpect(BeginWorldFromCreation(WeekSaveCreation(), 77, data).Succeeded(), "a life begins");
    data.World = world;
    return data;
}

/** A week in the life, saved: the saves the tool is given. */
WorldSaveData WeekSavedLife()
{
    WorldSim sim(WeekMorningWorld(77));
    WeekExpect(sim.Submit({CommandSource::Player, SetWorldFlag{"week.flag", true}}).Accepted, "a flag is set");
    WeekPlan plan;
    plan.Days.push_back({WeekSchoolDay()});
    plan.Days.push_back({WeekHomeDay()});
    const WeekResult week = CompressWeek(sim.State(), plan, WeekCats());
    WeekExpect(week.Ok(), "the days resolve");
    WorldSaveData data = WeekSaveOf(week.World);
    data.Player.ZoneId = "zone.street.nabapara_lane";
    data.Player.PositionCm = {120, -3400, 20};
    return data;
}

std::string WeekWriteSave(const WorldSaveData& data, const char* name)
{
    const std::string text = SerializeWorldSave(data);
    std::ofstream out(name, std::ios::binary);
    out << text;
    return text;
}

void TestSaveTexts()
{
    const WorldSaveData life = WeekSavedLife();
    const std::string text = SerializeWorldSave(life);
    SaveTextDiff same = DiffWorldSaveTexts(text, text);
    WeekExpect(same.Ok() && same.Report.Identical(), "a save compared with itself differs nowhere");
    WeekExpect(same.Report.HashA == HashWorldState(life.World), "the report's hash is the world's hash");

    WorldSaveData other = life;
    other.World.Flags["week.flag"] = false;
    other.Player.PositionCm[2] += 10;
    other.Player.Camera = CameraMode::ThirdPerson;
    other.Creation.Identity.GivenName = "Anaya";
    other.World.DayLoop.Condition.Energy -= 1;
    SaveTextDiff diff = DiffWorldSaveTexts(text, SerializeWorldSave(other));
    WeekExpect(diff.Ok() && StateDiffPaths(diff.Report) == std::vector<std::string>({"flags.week.flag", "dayloop.core.condition.energy",
                                                                                   "player.position_cm.z", "player.camera",
                                                                                   "creation.given_name"}),
               "the world, then the placement, then the creation record, each field named");
    WeekExpect(diff.Report.SectionNames() == std::vector<std::string>({"flags", "dayloop", "player", "creation"}),
               "and the sections they are in");
    WeekExpect(diff.Report.InSection("player")[1].A == "first_person" && diff.Report.InSection("player")[1].B == "third_person",
               "with their values");
    const StateDiffReport viaStructs = DiffWorldSaves(life, other);
    WeekExpect(viaStructs.Entries == diff.Report.Entries, "comparing the loaded saves and their texts agree");

    // The whole creation record is covered: a change the itemised fields do not name is still reported.
    WorldSaveData look = life;
    look.Creation.Appearance.SkinTone = {1, 2, 3};
    diff = DiffWorldSaveTexts(text, SerializeWorldSave(look));
    WeekExpect(diff.Ok() && StateDiffPaths(diff.Report) == std::vector<std::string>({"creation.record"}), "an appearance change is reported as the record");

    // A save that does not load is an error, not a difference.
    diff = DiffWorldSaveTexts("{", text);
    WeekExpect(!diff.Ok() && diff.Error.rfind("a: ", 0) == 0, "a damaged first save is named as the first");
    diff = DiffWorldSaveTexts(text, text.substr(0, text.size() - 1));
    WeekExpect(!diff.Ok() && diff.Error.rfind("b: ", 0) == 0, "a truncated second save is named as the second");
    diff = DiffWorldSaveTexts("", "");
    WeekExpect(!diff.Ok(), "an empty file is not a save");

    // The command counters can be ignored, as the equivalence rule does.
    WorldSaveData counted = life;
    counted.World.Seq += 5;
    StateDiffOptions options;
    options.IgnorePaths = {"meta.seq"};
    WeekExpect(DiffWorldSaveTexts(text, SerializeWorldSave(counted), options).Report.Identical() &&
                   StateDiffPaths(DiffWorldSaveTexts(text, SerializeWorldSave(counted)).Report) == std::vector<std::string>({"meta.seq"}),
               "an ignored path is left out of a save diff");
}

int WeekRunTool(const std::string& tool, const std::string& arguments, const char* outputFile)
{
    std::string command = "\"" + tool + "\" " + arguments + " > \"" + outputFile + "\" 2>&1";
#ifdef _WIN32
    command = "\"" + command + "\""; // cmd /c strips the outer pair of quotes
#endif
    const int status = std::system(command.c_str());
#ifdef _WIN32
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

std::string WeekReadText(const char* name)
{
    // The tool writes text-mode output, so a Windows console redirect holds CR LF line ends; the report is LF.
    std::ifstream in(name, std::ios::binary);
    std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}

void TestTool(const char* toolPath)
{
    if (toolPath == nullptr)
    {
        std::cout << "  (pwstatediff path not given: command-line checks skipped)\n";
        return;
    }
    const std::string tool = toolPath;
    const WorldSaveData life = WeekSavedLife();
    WeekWriteSave(life, "pwstatediff_a.save");
    WeekWriteSave(life, "pwstatediff_same.save");
    WorldSaveData changed = life;
    changed.World.Flags["week.flag"] = false;
    changed.World.Gates["gate.a.one"] = GateSiteState{GateRank::E, GateStatus::Active, 5, 1};
    changed.World.Seq += 1;
    WeekWriteSave(changed, "pwstatediff_b.save");

    WeekExpect(WeekRunTool(tool, "\"pwstatediff_a.save\" \"pwstatediff_same.save\"", "pwstatediff_out.txt") == 0,
               "identical saves exit 0");
    WeekExpect(WeekReadText("pwstatediff_out.txt") == RenderStateDiff(DiffWorldSaves(life, life)) &&
                   WeekReadText("pwstatediff_out.txt").rfind("pwstatediff: identical (world hash 0x", 0) == 0,
               "and print the identical report");

    WeekExpect(WeekRunTool(tool, "\"pwstatediff_a.save\" \"pwstatediff_b.save\"", "pwstatediff_out.txt") == 1, "saves that differ exit 1");
    const std::string report = WeekReadText("pwstatediff_out.txt");
    WeekExpect(report == RenderStateDiff(DiffWorldSaves(life, changed)), "and print exactly the library's report");
    WeekExpect(report.find("section meta\n  meta.seq: ") != std::string::npos &&
                   report.find("section flags\n  flags.week.flag: true -> false\n") != std::string::npos &&
                   report.find("section gates\n  gates.gate.a.one: (absent) -> rank=E status=active since=5 onsets=1\n") != std::string::npos,
               "naming the sections and the fields");

    WeekExpect(WeekRunTool(tool, "\"pwstatediff_a.save\" \"pwstatediff_b.save\" --ignore meta.seq --ignore flags --ignore gates",
                           "pwstatediff_out.txt") == 0,
               "ignoring every difference exits 0");
    WeekExpect(WeekReadText("pwstatediff_out.txt").find("no differences outside the ignored fields") != std::string::npos,
               "and says why");

    WeekExpect(WeekRunTool(tool, "\"pwstatediff_a.save\" \"pwstatediff_missing.save\"", "pwstatediff_out.txt") == 2 &&
                   WeekReadText("pwstatediff_out.txt").find("cannot read pwstatediff_missing.save") != std::string::npos,
               "a missing file exits 2");
    {
        std::ofstream broken("pwstatediff_broken.save", std::ios::binary);
        broken << "{ \"format\": \"perfectworld.save\"";
    }
    WeekExpect(WeekRunTool(tool, "\"pwstatediff_a.save\" \"pwstatediff_broken.save\"", "pwstatediff_out.txt") == 2 &&
                   WeekReadText("pwstatediff_out.txt").find("pwstatediff: b: ") == 0,
               "a damaged save exits 2 and names which one");
    WeekExpect(WeekRunTool(tool, "\"pwstatediff_a.save\"", "pwstatediff_out.txt") == 2, "one file is a usage error");
    WeekExpect(WeekRunTool(tool, "\"pwstatediff_a.save\" \"pwstatediff_b.save\" --ignore", "pwstatediff_out.txt") == 2,
               "--ignore without a path is a usage error");
    for (const char* name : {"pwstatediff_a.save", "pwstatediff_b.save", "pwstatediff_same.save", "pwstatediff_broken.save", "pwstatediff_out.txt"})
        std::remove(name);
}
} // namespace

void TestStateDiffSaves(const char* toolPath)
{
    TestSaveTexts();
    TestTool(toolPath);
}
