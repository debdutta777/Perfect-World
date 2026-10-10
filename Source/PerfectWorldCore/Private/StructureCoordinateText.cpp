#include "StructureCoordinate.h"

#include "StructureInternal.h"

#include <algorithm>

// The validity rules and text form of CoordinateState (ROADMAP M2-09): what a save
// taken mid-exposure carries. Integers and stable IDs only; the parser is strict, so a
// truncated or edited save is refused rather than half-read.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
constexpr std::string_view CoordinateTextHeader = "pw.structure_coordinate/1";

bool CoordinateSortedUniqueIds(const std::vector<std::string>& ids, std::size_t limit)
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

bool CoordinateAssignmentValid(const std::vector<CoordinateAssignment>& assignment)
{
    if (assignment.size() > StructureMaxListed)
        return false;
    for (std::size_t i = 0; i < assignment.size(); ++i)
    {
        if (!ModelIdWellFormed(assignment[i].Task) || !ModelIdWellFormed(assignment[i].Member) ||
            (i > 0 && !(assignment[i - 1].Task < assignment[i].Task)))
            return false;
    }
    return true;
}

bool CoordinateExposureValid(const CoordinateState& state)
{
    const CoordinateExposure& e = state.Exposure;
    if (!e.Active())
        return e == CoordinateExposure{};
    if (!BaseExposureValid(state.Families, e) || static_cast<std::size_t>(e.Goal) >= CoordinateGoalNames.size() ||
        static_cast<std::size_t>(e.Beat) >= CoordinateBeatNames.size() ||
        static_cast<std::size_t>(e.Resolution) >= CoordinateResolutionNames.size() || !CoordinateAssignmentValid(e.Assignment) ||
        !CoordinateSortedUniqueIds(e.Heard, StructureMaxListed))
        return false;
    switch (e.Beat)
    {
    case CoordinateBeat::Assign:
        return e.Assignment.empty() && e.Heard.empty() && e.Resolution == CoordinateResolution::None;
    case CoordinateBeat::Exchange:
    case CoordinateBeat::Settle:
        return !e.Assignment.empty() && e.Resolution == CoordinateResolution::None;
    case CoordinateBeat::Submit:
        return !e.Assignment.empty() && e.Resolution != CoordinateResolution::None;
    }
    return false;
}

CoordinateParse CoordinateRefuse(std::string message)
{
    CoordinateParse result;
    result.Error = std::move(message);
    return result;
}

bool CoordinateParseExposure(std::string_view value, CoordinateExposure& exposure)
{
    const std::vector<std::string_view> parts = Split(value);
    if (parts.size() != 7 || !ModelIdWellFormed(parts[0]) || !ModelIdWellFormed(parts[1]))
        return false;
    exposure.FamilyId = std::string(parts[0]);
    exposure.CaseId = std::string(parts[1]);
    return ModelParseName(CoordinateGoalNames, parts[2], exposure.Goal) && ModelParseName(CoordinateBeatNames, parts[3], exposure.Beat) &&
           ParseInt(parts[4], exposure.UnitsTotal) && ParseInt(parts[5], exposure.UnitsLeft) && ParseFlag(parts[6], exposure.MethodKnown);
}

bool CoordinateParseAssignment(std::string_view value, std::vector<CoordinateAssignment>& out)
{
    out.clear();
    if (value == TextNone)
        return true;
    for (const std::string_view part : Split(value))
    {
        const std::vector<std::string_view> pair = Split(part, ':');
        if (pair.size() != 2 || !ModelIdWellFormed(pair[0]) || !ModelIdWellFormed(pair[1]))
            return false;
        out.push_back({std::string(pair[0]), std::string(pair[1])});
    }
    return true;
}
} // namespace

bool IsCoordinateStateValid(const CoordinateState& state)
{
    return IsStructureFamilyListValid(state.Families) && CoordinateExposureValid(state);
}

std::string SerialiseStructureCoordinate(const CoordinateState& state)
{
    if (!IsCoordinateStateValid(state))
        return {};
    std::string text(CoordinateTextHeader);
    text += "\n";
    AppendFamilies(text, state.Families);
    const CoordinateExposure& e = state.Exposure;
    text += "exposure=";
    if (!e.Active())
        text += TextNone;
    else
        text += e.FamilyId + "," + e.CaseId + "," + std::string(ModelNameOf(CoordinateGoalNames, e.Goal)) + "," +
                std::string(ModelNameOf(CoordinateBeatNames, e.Beat)) + "," + std::to_string(e.UnitsTotal) + "," + std::to_string(e.UnitsLeft) +
                "," + (e.MethodKnown ? "1" : "0");
    text += "\nassign=";
    if (e.Assignment.empty())
        text += TextNone;
    for (std::size_t i = 0; i < e.Assignment.size(); ++i)
        text += (i > 0 ? "," : "") + e.Assignment[i].Task + ":" + e.Assignment[i].Member;
    text += "\nheard=" + Join(e.Heard);
    text += "\nresolution=" + std::string(ModelNameOf(CoordinateResolutionNames, e.Resolution)) + "\n";
    text += TextEnd;
    text += "\n";
    return text;
}

CoordinateParse DeserialiseStructureCoordinate(std::string_view text)
{
    LineReader reader(text);
    std::string_view line;
    std::string_view value;
    if (!reader.Next(line) || line != CoordinateTextHeader)
        return CoordinateRefuse("The structure coordinate text has no recognised header.");

    CoordinateState state;
    if (!ReadFamilies(reader, state.Families))
        return CoordinateRefuse("The structure coordinate text has a malformed families section.");
    if (!reader.Pair("exposure", value))
        return CoordinateRefuse("The structure coordinate text has no exposure line.");
    if (value != TextNone && !CoordinateParseExposure(value, state.Exposure))
        return CoordinateRefuse("The structure coordinate text has a malformed exposure line.");
    if (!reader.Pair("assign", value) || !CoordinateParseAssignment(value, state.Exposure.Assignment))
        return CoordinateRefuse("The structure coordinate text has a malformed assign line.");
    if (!reader.Pair("heard", value) || !ParseIds(value, state.Exposure.Heard))
        return CoordinateRefuse("The structure coordinate text has a malformed heard line.");
    if (!reader.Pair("resolution", value) || !ModelParseName(CoordinateResolutionNames, value, state.Exposure.Resolution))
        return CoordinateRefuse("The structure coordinate text has a malformed resolution line.");
    if (!reader.Next(line) || line != TextEnd)
        return CoordinateRefuse("The structure coordinate text is truncated or has no end line.");
    if (!reader.AtEnd())
        return CoordinateRefuse("The structure coordinate text has data after the end line.");
    if (!IsCoordinateStateValid(state))
        return CoordinateRefuse("The structure coordinate text describes an inconsistent state.");

    CoordinateParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
