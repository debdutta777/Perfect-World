#include "StateDiff.h"

#include "CharacterCreation.h"
#include "CreationSaveSerialization.h"
#include "GateSites.h"
#include "RandomStreams.h"
#include "StateDiffInternal.h"
#include "WorldHash.h"

#include <algorithm>
#include <map>

// The persistent-state diff (ROADMAP M3-02): the world sections of the world hash, then the
// save-level sections. The day loop's own sections are compared in StateDiffLines.cpp.
namespace PerfectWorld
{
namespace
{
std::string StateDiffNumber(std::int64_t value)
{
    return std::to_string(value);
}

std::string StateDiffUnsigned(std::uint64_t value)
{
    return std::to_string(value);
}

std::string StateDiffBoolean(bool value)
{
    return value ? "true" : "false";
}

std::string StateDiffGateSummary(const GateSiteState& gate)
{
    return "rank=" + std::string(GateRankName(gate.Rank)) + " status=" + std::string(GateStatusName(gate.Status)) +
           " since=" + std::to_string(gate.StatusSince) + " onsets=" + std::to_string(gate.OnsetCount);
}

void StateDiffMeta(StateDiffSink& sink, const WorldState& a, const WorldState& b)
{
    sink.Field("meta", "meta.seed", StateDiffUnsigned(a.WorldSeed), StateDiffUnsigned(b.WorldSeed));
    sink.Field("meta", "meta.seq", StateDiffUnsigned(a.Seq), StateDiffUnsigned(b.Seq));
}

std::string StateDiffPendingSummary(const ScheduledEvent& event)
{
    return "when=" + std::to_string(event.When) + " id=" + event.Id;
}

void StateDiffClock(StateDiffSink& sink, const WorldState& a, const WorldState& b)
{
    const GameClockSave saveA = a.Clock.CaptureSave();
    const GameClockSave saveB = b.Clock.CaptureSave();
    sink.Field("clock", "clock.now", StateDiffNumber(saveA.Now), StateDiffNumber(saveB.Now));
    sink.Field("clock", "clock.next_handle", StateDiffNumber(saveA.NextHandle), StateDiffNumber(saveB.NextHandle));

    std::map<std::int64_t, const ScheduledEvent*> byHandleB;
    for (const ScheduledEvent& event : saveB.Pending)
        byHandleB[event.Handle] = &event;
    for (const ScheduledEvent& event : saveA.Pending)
    {
        const std::string base = "clock.pending[" + std::to_string(event.Handle) + "]";
        const auto other = byHandleB.find(event.Handle);
        if (other == byHandleB.end())
        {
            sink.Add("clock", base, StateDiffPendingSummary(event), std::string(StateDiffAbsent));
            continue;
        }
        sink.Field("clock", base + ".when", StateDiffNumber(event.When), StateDiffNumber(other->second->When));
        sink.Field("clock", base + ".id", event.Id, other->second->Id);
        byHandleB.erase(other);
    }
    for (const ScheduledEvent& event : saveB.Pending)
    {
        if (byHandleB.count(event.Handle) != 0)
            sink.Add("clock", "clock.pending[" + std::to_string(event.Handle) + "]", std::string(StateDiffAbsent),
                     StateDiffPendingSummary(event));
    }
}

void StateDiffFlags(StateDiffSink& sink, const WorldState& a, const WorldState& b)
{
    for (const auto& [id, value] : a.Flags)
    {
        const auto other = b.Flags.find(id);
        sink.Field("flags", "flags." + id, StateDiffBoolean(value),
                   other == b.Flags.end() ? std::string(StateDiffAbsent) : StateDiffBoolean(other->second));
    }
    for (const auto& [id, value] : b.Flags)
    {
        if (a.Flags.find(id) == a.Flags.end())
            sink.Add("flags", "flags." + id, std::string(StateDiffAbsent), StateDiffBoolean(value));
    }
}

void StateDiffGates(StateDiffSink& sink, const WorldState& a, const WorldState& b)
{
    for (const auto& [id, gate] : a.Gates)
    {
        const std::string base = "gates." + id;
        const auto other = b.Gates.find(id);
        if (other == b.Gates.end())
        {
            sink.Add("gates", base, StateDiffGateSummary(gate), std::string(StateDiffAbsent));
            continue;
        }
        const GateSiteState& g = other->second;
        sink.Field("gates", base + ".rank", std::string(GateRankName(gate.Rank)), std::string(GateRankName(g.Rank)));
        sink.Field("gates", base + ".status", std::string(GateStatusName(gate.Status)), std::string(GateStatusName(g.Status)));
        sink.Field("gates", base + ".since", StateDiffNumber(gate.StatusSince), StateDiffNumber(g.StatusSince));
        sink.Field("gates", base + ".onsets", StateDiffNumber(gate.OnsetCount), StateDiffNumber(g.OnsetCount));
    }
    for (const auto& [id, gate] : b.Gates)
    {
        if (a.Gates.find(id) == a.Gates.end())
            sink.Add("gates", "gates." + id, std::string(StateDiffAbsent), StateDiffGateSummary(gate));
    }
}

void StateDiffRng(StateDiffSink& sink, const WorldState& a, const WorldState& b)
{
    for (std::size_t i = 0; i < RandomStreamCount; ++i)
    {
        const std::string base = "rng." + std::string(RandomStreamName(static_cast<RandomStreamId>(i)));
        const RandomStreamState& x = a.Rng[i];
        const RandomStreamState& y = b.Rng[i];
        for (std::size_t word = 0; word < x.Words.size(); ++word)
            sink.Field("rng", base + ".word" + std::to_string(word), StateDiffHex(x.Words[word]), StateDiffHex(y.Words[word]));
        sink.Field("rng", base + ".draws", StateDiffUnsigned(x.Draws), StateDiffUnsigned(y.Draws));
    }
}

void StateDiffPlayer(StateDiffSink& sink, const PlayerPlacement& a, const PlayerPlacement& b)
{
    sink.Field("player", "player.zone", a.ZoneId, b.ZoneId);
    static constexpr const char* axes[3] = {"x", "y", "z"};
    for (std::size_t i = 0; i < 3; ++i)
        sink.Field("player", std::string("player.position_cm.") + axes[i], StateDiffNumber(a.PositionCm[i]),
                   StateDiffNumber(b.PositionCm[i]));
    sink.Field("player", "player.yaw_cdeg", StateDiffNumber(a.YawCdeg), StateDiffNumber(b.YawCdeg));
    sink.Field("player", "player.pitch_cdeg", StateDiffNumber(a.PitchCdeg), StateDiffNumber(b.PitchCdeg));
    sink.Field("player", "player.camera", std::string(CameraModeName(a.Camera)), std::string(CameraModeName(b.Camera)));
}

std::uint64_t StateDiffFnv(std::string_view text)
{
    std::uint64_t hash = 0xcbf29ce484222325ULL;
    for (const char c : text)
    {
        hash ^= static_cast<unsigned char>(c);
        hash *= 0x100000001b3ULL;
    }
    return hash;
}

void StateDiffCreation(StateDiffSink& sink, const CharacterCreationSave& a, const CharacterCreationSave& b)
{
    const std::size_t before = sink.Count();
    sink.Field("creation", "creation.age", StateDiffNumber(a.Age), StateDiffNumber(b.Age));
    sink.Field("creation", "creation.household", a.HouseholdTemplateId, b.HouseholdTemplateId);
    sink.Field("creation", "creation.given_name", a.Identity.GivenName, b.Identity.GivenName);
    sink.Field("creation", "creation.family_name", a.Identity.FamilyName, b.Identity.FamilyName);
    sink.Field("creation", "creation.has_started", StateDiffBoolean(a.HasStarted), StateDiffBoolean(b.HasStarted));
    const auto& traits = CharacterCreator::TraitDefinitions();
    for (std::size_t i = 0; i < a.TraitRanks.size() && i < traits.size(); ++i)
        sink.Field("creation", "creation.trait." + std::string(traits[i].Id), StateDiffNumber(a.TraitRanks[i]),
                   StateDiffNumber(b.TraitRanks[i]));
    // The rest of the record (appearance, school, languages, birth) is not itemised: a difference there is
    // reported as the record as a whole, with the hash of each side's text, so it cannot go unseen.
    const std::string textA = SerializeCreationSave(a);
    const std::string textB = SerializeCreationSave(b);
    if (sink.Count() == before && textA != textB)
        sink.Add("creation", "creation.record", StateDiffHex(StateDiffFnv(textA)), StateDiffHex(StateDiffFnv(textB)));
}

void StateDiffWorld(StateDiffSink& sink, const WorldState& a, const WorldState& b, bool plainOptions)
{
    const std::vector<WorldSectionHash> hashesA = HashWorldSections(a);
    const std::vector<WorldSectionHash> hashesB = HashWorldSections(b);
    using Compare = void (*)(StateDiffSink&, const WorldState&, const WorldState&);
    struct Step
    {
        const char* Name;
        Compare Run;
    };
    static constexpr Step steps[] = {{"meta", StateDiffMeta}, {"clock", StateDiffClock}, {"flags", StateDiffFlags},
                                     {"gates", StateDiffGates}, {"rng", StateDiffRng}};
    for (const Step& step : steps)
    {
        const std::size_t before = sink.Count();
        step.Run(sink, a, b);
        for (std::size_t i = 0; i < hashesA.size() && i < hashesB.size(); ++i)
        {
            if (hashesA[i].Name == step.Name && hashesA[i].Hash != hashesB[i].Hash && sink.Count() == before && plainOptions)
                sink.Add(step.Name, std::string(step.Name) + ".hash", StateDiffHex(hashesA[i].Hash), StateDiffHex(hashesB[i].Hash));
        }
    }
    const std::size_t before = sink.Count();
    StateDiffDayLoop(sink, a.DayLoop, b.DayLoop);
    for (std::size_t i = 0; i < hashesA.size() && i < hashesB.size(); ++i)
    {
        if (hashesA[i].Name == "dayloop" && hashesA[i].Hash != hashesB[i].Hash && sink.Count() == before && plainOptions)
            sink.Add("dayloop", "dayloop.hash", StateDiffHex(hashesA[i].Hash), StateDiffHex(hashesB[i].Hash));
    }
}
} // namespace

bool StateDiffSink::Ignored(std::string_view path) const
{
    for (const std::string& ignored : Options_.IgnorePaths)
    {
        if (path == ignored)
            return true;
        if (path.size() > ignored.size() && path.compare(0, ignored.size(), ignored) == 0 &&
            (path[ignored.size()] == '.' || path[ignored.size()] == '['))
            return true;
    }
    return false;
}

void StateDiffSink::Add(std::string_view section, std::string_view path, std::string a, std::string b)
{
    if (Ignored(path))
        return;
    Report_.Entries.push_back({std::string(section), std::string(path), std::move(a), std::move(b)});
}

void StateDiffSink::Field(std::string_view section, std::string_view path, const std::string& a, const std::string& b)
{
    if (a != b)
        Add(section, path, a, b);
}

std::string StateDiffHex(std::uint64_t value)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string text = "0x0000000000000000";
    for (int i = 17; i >= 2; --i)
    {
        text[static_cast<std::size_t>(i)] = digits[value & 0xFu];
        value >>= 4;
    }
    return text;
}

std::vector<std::string> StateDiffReport::SectionNames() const
{
    std::vector<std::string> names;
    for (const StateDiffEntry& entry : Entries)
    {
        if (std::find(names.begin(), names.end(), entry.Section) == names.end())
            names.push_back(entry.Section);
    }
    return names;
}

std::vector<StateDiffEntry> StateDiffReport::InSection(std::string_view section) const
{
    std::vector<StateDiffEntry> found;
    for (const StateDiffEntry& entry : Entries)
    {
        if (entry.Section == section)
            found.push_back(entry);
    }
    return found;
}

StateDiffReport DiffWorldStates(const WorldState& a, const WorldState& b, const StateDiffOptions& options)
{
    StateDiffReport report;
    report.HashA = HashWorldState(a);
    report.HashB = HashWorldState(b);
    StateDiffSink sink(report, options);
    StateDiffWorld(sink, a, b, options.IgnorePaths.empty());
    return report;
}

StateDiffReport DiffWorldSaves(const WorldSaveData& a, const WorldSaveData& b, const StateDiffOptions& options)
{
    StateDiffReport report;
    report.HashA = HashWorldState(a.World);
    report.HashB = HashWorldState(b.World);
    StateDiffSink sink(report, options);
    StateDiffWorld(sink, a.World, b.World, options.IgnorePaths.empty());
    StateDiffPlayer(sink, a.Player, b.Player);
    StateDiffCreation(sink, a.Creation, b.Creation);
    return report;
}

SaveTextDiff DiffWorldSaveTexts(std::string_view textA, std::string_view textB, const StateDiffOptions& options)
{
    SaveTextDiff diff;
    WorldSaveData a;
    WorldSaveData b;
    const WorldSaveResult readA = DeserializeWorldSave(textA, a);
    if (!readA.Succeeded())
    {
        diff.Error = "a: " + readA.Message;
        return diff;
    }
    const WorldSaveResult readB = DeserializeWorldSave(textB, b);
    if (!readB.Succeeded())
    {
        diff.Error = "b: " + readB.Message;
        return diff;
    }
    diff.Report = DiffWorldSaves(a, b, options);
    return diff;
}

std::string RenderStateDiff(const StateDiffReport& report)
{
    std::string text = "pwstatediff: ";
    if (report.Identical())
    {
        if (report.HashA == report.HashB)
            text += "identical (world hash " + StateDiffHex(report.HashA) + ")\n";
        else
            text += "no differences outside the ignored fields (world hash a=" + StateDiffHex(report.HashA) +
                    " b=" + StateDiffHex(report.HashB) + ")\n";
        return text;
    }
    const std::vector<std::string> sections = report.SectionNames();
    text += std::to_string(report.Entries.size()) + (report.Entries.size() == 1 ? " difference in " : " differences in ") +
            std::to_string(sections.size()) + (sections.size() == 1 ? " section" : " sections") + " (world hash a=" +
            StateDiffHex(report.HashA) + " b=" + StateDiffHex(report.HashB) + ")\n";
    for (const std::string& section : sections)
    {
        text += "section " + section + "\n";
        for (const StateDiffEntry& entry : report.Entries)
        {
            if (entry.Section == section)
                text += "  " + entry.Path + ": " + entry.A + " -> " + entry.B + "\n";
        }
    }
    return text;
}

std::vector<std::string> StateDiffPaths(const StateDiffReport& report)
{
    std::vector<std::string> paths;
    paths.reserve(report.Entries.size());
    for (const StateDiffEntry& entry : report.Entries)
        paths.push_back(entry.Path);
    return paths;
}
} // namespace PerfectWorld
