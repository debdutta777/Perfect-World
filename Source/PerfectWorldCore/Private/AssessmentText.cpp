#include "Assessment.h"

#include "StructureInternal.h"

// The text form of AssessmentState (ROADMAP M3-07): what a save taken mid-paper carries.
// Integers and stable IDs only; the parser is strict, so a truncated or edited save is
// refused rather than half-read. The grammar is documented in Public/Assessment.h.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
constexpr std::string_view AssessmentTextHeader = "pw.assessment/1";

AssessmentParse AssessmentRefuse(std::string message)
{
    AssessmentParse result;
    result.Error = std::move(message);
    return result;
}

std::string AssessmentFlagText(bool flag)
{
    return flag ? "1" : "0";
}

bool AssessmentParseGroup(std::string_view value, AssessmentGroupState& group)
{
    const std::vector<std::string_view> parts = Split(value);
    if (parts.size() != 8)
        return false;
    return ParseFlag(parts[0], group.Known) && ParseFlag(parts[1], group.Shaky) && ParseInt(parts[2], group.Allocated) &&
           ParseFlag(parts[3], group.Answered) && ModelParseName(AssessmentModeNames, parts[4], group.Mode) &&
           ParseFlag(parts[5], group.Confirm) && ParseFlag(parts[6], group.Hedge) && ParseInt(parts[7], group.Spent);
}
} // namespace

std::string SerialiseAssessment(const AssessmentState& state)
{
    if (!IsAssessmentStateValid(state))
        return {};
    std::string text(AssessmentTextHeader);
    text += "\npaper=" + (state.PaperId.empty() ? std::string(TextNone) : state.PaperId) + "\n";
    if (!state.PaperId.empty())
    {
        text += "state=" + AssessmentFlagText(state.Surveyed) + "," + AssessmentFlagText(state.Finished) + "," + std::to_string(state.UnitsLeft) + "\n";
        text += "groups=" + std::to_string(state.Groups.size()) + "\n";
        for (const AssessmentGroupState& group : state.Groups)
        {
            text += "group=" + AssessmentFlagText(group.Known) + "," + AssessmentFlagText(group.Shaky) + "," + std::to_string(group.Allocated) + "," +
                    AssessmentFlagText(group.Answered) + "," + std::string(ModelNameOf(AssessmentModeNames, group.Mode)) + "," +
                    AssessmentFlagText(group.Confirm) + "," + AssessmentFlagText(group.Hedge) + "," + std::to_string(group.Spent) + "\n";
        }
    }
    text += TextEnd;
    text += "\n";
    return text;
}

AssessmentParse DeserialiseAssessment(std::string_view text)
{
    LineReader reader(text);
    std::string_view line;
    std::string_view value;
    if (!reader.Next(line) || line != AssessmentTextHeader)
        return AssessmentRefuse("The assessment text has no recognised header.");

    AssessmentState state;
    if (!reader.Pair("paper", value) || (value != TextNone && !ModelIdWellFormed(value)))
        return AssessmentRefuse("The assessment text has a malformed paper line.");
    if (value != TextNone)
    {
        state.PaperId = std::string(value);
        if (!reader.Pair("state", value))
            return AssessmentRefuse("The assessment text has no state line.");
        const std::vector<std::string_view> parts = Split(value);
        if (parts.size() != 3 || !ParseFlag(parts[0], state.Surveyed) || !ParseFlag(parts[1], state.Finished) || !ParseInt(parts[2], state.UnitsLeft))
            return AssessmentRefuse("The assessment text has a malformed state line.");
        std::size_t count = 0;
        if (!reader.Pair("groups", value) || !ParseInt(value, count) || count != AssessmentGroupCount)
            return AssessmentRefuse("The assessment text has a malformed groups line.");
        for (std::size_t i = 0; i < count; ++i)
        {
            AssessmentGroupState group;
            if (!reader.Pair("group", value) || !AssessmentParseGroup(value, group))
                return AssessmentRefuse("The assessment text has a malformed group line.");
            state.Groups.push_back(group);
        }
    }
    if (!reader.Next(line) || line != TextEnd)
        return AssessmentRefuse("The assessment text is truncated or has no end line.");
    if (!reader.AtEnd())
        return AssessmentRefuse("The assessment text has data after the end line.");
    if (!IsAssessmentStateValid(state))
        return AssessmentRefuse("The assessment text describes an inconsistent state.");

    AssessmentParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
