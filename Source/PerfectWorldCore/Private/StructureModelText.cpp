#include "StructureModel.h"

#include <algorithm>
#include <charconv>

// The validity rules and text form of StructureModelState (ROADMAP M2-08): what a
// save taken mid-exposure carries. Integers and stable IDs only; the parser is
// strict, so a truncated or edited save is refused rather than half-read.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view ModelTextHeader = "pw.structure_model/1";
constexpr std::string_view ModelTextEnd = "end";
constexpr std::string_view ModelTextNone = "-";

bool ModelSortedUniqueIds(const std::vector<std::string>& ids, std::size_t limit)
{
    if (ids.size() > limit)
        return false;
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        if (!ModelIdWellFormed(ids[i]) || (i > 0 && !(ids[i - 1] < ids[i])))
            return false;
    }
    return true;
}

bool ModelFamilyRecordValid(const ModelFamilyRecord& record)
{
    if (!ModelIdWellFormed(record.FamilyId) || !ModelSortedUniqueIds(record.Seen, ModelMaxSeen) || record.Recent.size() > ModelRecentKept ||
        record.SoundExposures > record.Exposures)
        return false;
    for (const std::string& id : record.Recent)
    {
        if (!std::binary_search(record.Seen.begin(), record.Seen.end(), id))
            return false;
    }
    // Mastery is awarded when an exposure finishes, so the counts may be ahead of the flag but never behind it.
    return !record.Secure || (record.SoundExposures >= ModelSecureSoundExposures && record.Seen.size() >= ModelSecureDistinctCases);
}

bool ModelExposureValid(const StructureModelState& state)
{
    const ModelExposure& e = state.Exposure;
    if (!e.Active())
        return e == ModelExposure{};
    if (!ModelIdWellFormed(e.FamilyId) || !ModelIdWellFormed(e.CaseId) || static_cast<std::size_t>(e.Goal) >= ModelGoalNames.size() ||
        static_cast<std::size_t>(e.Beat) >= ModelBeatNames.size() ||
        static_cast<std::size_t>(e.Investigation) >= ModelInvestigationNames.size())
        return false;
    const ModelFamilyRecord* record = state.FindFamily(e.FamilyId);
    if (record == nullptr || record->Recent.empty() || record->Recent.back() != e.CaseId)
        return false;
    if (e.UnitsTotal < 1 || e.UnitsTotal > ModelWorkUnits || e.UnitsLeft < 0 || e.UnitsLeft > e.UnitsTotal)
        return false;
    if (e.MethodKnown && e.ExampleTaken)
        return false;
    if (!ModelSortedUniqueIds(e.Links, ModelMaxLinks))
        return false;
    switch (e.Beat)
    {
    case ModelBeat::BuildModel:
        return e.Links.empty() && e.Investigation == ModelInvestigation::None;
    case ModelBeat::ResolveDiscrepancy:
        return !e.Links.empty() && e.Investigation == ModelInvestigation::None;
    case ModelBeat::PresentClaim:
        return !e.Links.empty() && e.Investigation != ModelInvestigation::None;
    }
    return false;
}

std::string ModelJoin(const std::vector<std::string>& ids)
{
    if (ids.empty())
        return std::string(ModelTextNone);
    std::string text;
    for (std::size_t i = 0; i < ids.size(); ++i)
        text += (i > 0 ? "," : "") + ids[i];
    return text;
}

std::vector<std::string_view> ModelSplit(std::string_view text)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t comma = text.find(',', start);
        if (comma == std::string_view::npos)
        {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, comma - start));
        start = comma + 1;
    }
}

/** The next line without its newline. False when no newline remains, which is what a truncated text looks like. */
bool ModelNextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false;
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

bool ModelKeyValue(std::string_view line, std::string_view key, std::string_view& value)
{
    if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Canonical non-negative decimal: no sign, no leading zeros, nothing trailing. */
template <typename Int>
bool ModelParseInt(std::string_view text, Int& out)
{
    if (text.empty() || (text.size() > 1 && text.front() == '0'))
        return false;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            return false;
    }
    Int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

bool ModelParseFlag(std::string_view text, bool& out)
{
    if (text != "0" && text != "1")
        return false;
    out = text == "1";
    return true;
}

bool ModelParseIds(std::string_view text, std::vector<std::string>& out)
{
    out.clear();
    if (text == ModelTextNone)
        return true;
    for (const std::string_view part : ModelSplit(text))
    {
        if (!ModelIdWellFormed(part))
            return false;
        out.emplace_back(part);
    }
    return true;
}

StructureModelParse ModelRefuse(std::string message)
{
    StructureModelParse result;
    result.Error = std::move(message);
    return result;
}

bool ModelParseFamily(std::string_view line, ModelFamilyRecord& record)
{
    std::string_view value;
    if (!ModelKeyValue(line, "family", value))
        return false;
    const std::vector<std::string_view> parts = ModelSplit(value);
    if (parts.size() != 4 || !ModelIdWellFormed(parts[0]))
        return false;
    record.FamilyId = std::string(parts[0]);
    return ModelParseInt(parts[1], record.Exposures) && ModelParseInt(parts[2], record.SoundExposures) &&
           ModelParseFlag(parts[3], record.Secure);
}

bool ModelParseExposure(std::string_view value, ModelExposure& exposure)
{
    const std::vector<std::string_view> parts = ModelSplit(value);
    if (parts.size() != 9 || !ModelIdWellFormed(parts[0]) || !ModelIdWellFormed(parts[1]))
        return false;
    exposure.FamilyId = std::string(parts[0]);
    exposure.CaseId = std::string(parts[1]);
    return ModelParseName(ModelGoalNames, parts[2], exposure.Goal) && ModelParseName(ModelBeatNames, parts[3], exposure.Beat) &&
           ModelParseInt(parts[4], exposure.UnitsTotal) && ModelParseInt(parts[5], exposure.UnitsLeft) &&
           ModelParseFlag(parts[6], exposure.MethodKnown) && ModelParseFlag(parts[7], exposure.ExampleTaken) &&
           ModelParseName(ModelInvestigationNames, parts[8], exposure.Investigation);
}
} // namespace

bool IsStructureModelStateValid(const StructureModelState& state)
{
    if (state.Families.size() > ModelMaxFamilies)
        return false;
    for (std::size_t i = 0; i < state.Families.size(); ++i)
    {
        if (!ModelFamilyRecordValid(state.Families[i]) || (i > 0 && !(state.Families[i - 1].FamilyId < state.Families[i].FamilyId)))
            return false;
    }
    return ModelExposureValid(state);
}

std::string SerialiseStructureModel(const StructureModelState& state)
{
    if (!IsStructureModelStateValid(state))
        return {};
    std::string text(ModelTextHeader);
    text += "\nfamilies=" + std::to_string(state.Families.size());
    for (const ModelFamilyRecord& record : state.Families)
    {
        text += "\nfamily=" + record.FamilyId + "," + std::to_string(record.Exposures) + "," + std::to_string(record.SoundExposures) +
                "," + (record.Secure ? "1" : "0");
        text += "\nseen=" + ModelJoin(record.Seen);
        text += "\nrecent=" + ModelJoin(record.Recent);
    }
    const ModelExposure& e = state.Exposure;
    text += "\nexposure=";
    if (!e.Active())
        text += ModelTextNone;
    else
        text += e.FamilyId + "," + e.CaseId + "," + std::string(ModelNameOf(ModelGoalNames, e.Goal)) + "," +
                std::string(ModelNameOf(ModelBeatNames, e.Beat)) + "," + std::to_string(e.UnitsTotal) + "," + std::to_string(e.UnitsLeft) +
                "," + (e.MethodKnown ? "1" : "0") + "," + (e.ExampleTaken ? "1" : "0") + "," +
                std::string(ModelNameOf(ModelInvestigationNames, e.Investigation));
    text += "\nlinks=" + ModelJoin(e.Links);
    text += "\n";
    text += ModelTextEnd;
    text += "\n";
    return text;
}

StructureModelParse DeserialiseStructureModel(std::string_view text)
{
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!ModelNextLine(text, position, line) || line != ModelTextHeader)
        return ModelRefuse("The structure model text has no recognised header.");

    StructureModelState state;
    std::size_t count = 0;
    if (!ModelNextLine(text, position, line) || !ModelKeyValue(line, "families", value) || !ModelParseInt(value, count) ||
        count > ModelMaxFamilies)
        return ModelRefuse("The structure model text has no valid families line.");
    for (std::size_t i = 0; i < count; ++i)
    {
        ModelFamilyRecord record;
        if (!ModelNextLine(text, position, line) || !ModelParseFamily(line, record))
            return ModelRefuse("The structure model text has a malformed family line.");
        if (!ModelNextLine(text, position, line) || !ModelKeyValue(line, "seen", value) || !ModelParseIds(value, record.Seen))
            return ModelRefuse("The structure model text has a malformed seen line.");
        if (!ModelNextLine(text, position, line) || !ModelKeyValue(line, "recent", value) || !ModelParseIds(value, record.Recent))
            return ModelRefuse("The structure model text has a malformed recent line.");
        state.Families.push_back(std::move(record));
    }
    if (!ModelNextLine(text, position, line) || !ModelKeyValue(line, "exposure", value))
        return ModelRefuse("The structure model text has no exposure line.");
    if (value != ModelTextNone && !ModelParseExposure(value, state.Exposure))
        return ModelRefuse("The structure model text has a malformed exposure line.");
    if (!ModelNextLine(text, position, line) || !ModelKeyValue(line, "links", value) || !ModelParseIds(value, state.Exposure.Links))
        return ModelRefuse("The structure model text has a malformed links line.");
    if (!ModelNextLine(text, position, line) || line != ModelTextEnd)
        return ModelRefuse("The structure model text is truncated or has no end line.");
    if (position != text.size())
        return ModelRefuse("The structure model text has data after the end line.");
    if (!IsStructureModelStateValid(state))
        return ModelRefuse("The structure model text describes an inconsistent state.");

    StructureModelParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
