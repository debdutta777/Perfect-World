#include "StructureInvestigate.h"

#include "StructureInternal.h"

#include <algorithm>

// The validity rules and text form of InvestigateState (ROADMAP M2-09): what a save
// taken mid-exposure carries. Integers and stable IDs only; the parser is strict, so a
// truncated or edited save is refused rather than half-read.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
constexpr std::string_view InvestigateTextHeader = "pw.structure_investigate/1";

/** The tests are kept in the order run, so they need only be well-formed, distinct and few. */
bool InvestigateTestsValid(const std::vector<std::string>& tests)
{
    if (tests.size() > InvestigateMaxTests)
        return false;
    for (std::size_t i = 0; i < tests.size(); ++i)
    {
        if (!ModelIdWellFormed(tests[i]) || std::find(tests.begin(), tests.begin() + static_cast<std::ptrdiff_t>(i), tests[i]) != tests.begin() + static_cast<std::ptrdiff_t>(i))
            return false;
    }
    return true;
}

bool InvestigateExposureValid(const InvestigateState& state)
{
    const InvestigateExposure& e = state.Exposure;
    if (!e.Active())
        return e == InvestigateExposure{};
    return BaseExposureValid(state.Families, e) && static_cast<std::size_t>(e.Goal) < InvestigateGoalNames.size() &&
           InvestigateTestsValid(e.TestsRun);
}

InvestigateParse InvestigateRefuse(std::string message)
{
    InvestigateParse result;
    result.Error = std::move(message);
    return result;
}

bool InvestigateParseExposure(std::string_view value, InvestigateExposure& exposure)
{
    const std::vector<std::string_view> parts = Split(value);
    if (parts.size() != 6 || !ModelIdWellFormed(parts[0]) || !ModelIdWellFormed(parts[1]))
        return false;
    exposure.FamilyId = std::string(parts[0]);
    exposure.CaseId = std::string(parts[1]);
    return ModelParseName(InvestigateGoalNames, parts[2], exposure.Goal) && ParseInt(parts[3], exposure.UnitsTotal) &&
           ParseInt(parts[4], exposure.UnitsLeft) && ParseFlag(parts[5], exposure.MethodKnown);
}
} // namespace

bool IsInvestigateStateValid(const InvestigateState& state)
{
    return IsStructureFamilyListValid(state.Families) && InvestigateExposureValid(state);
}

std::string SerialiseStructureInvestigate(const InvestigateState& state)
{
    if (!IsInvestigateStateValid(state))
        return {};
    std::string text(InvestigateTextHeader);
    text += "\n";
    AppendFamilies(text, state.Families);
    const InvestigateExposure& e = state.Exposure;
    text += "exposure=";
    if (!e.Active())
        text += TextNone;
    else
        text += e.FamilyId + "," + e.CaseId + "," + std::string(ModelNameOf(InvestigateGoalNames, e.Goal)) + "," +
                std::to_string(e.UnitsTotal) + "," + std::to_string(e.UnitsLeft) + "," + (e.MethodKnown ? "1" : "0");
    text += "\ntests=" + Join(e.TestsRun) + "\n";
    text += TextEnd;
    text += "\n";
    return text;
}

InvestigateParse DeserialiseStructureInvestigate(std::string_view text)
{
    LineReader reader(text);
    std::string_view line;
    std::string_view value;
    if (!reader.Next(line) || line != InvestigateTextHeader)
        return InvestigateRefuse("The structure investigate text has no recognised header.");

    InvestigateState state;
    if (!ReadFamilies(reader, state.Families))
        return InvestigateRefuse("The structure investigate text has a malformed families section.");
    if (!reader.Pair("exposure", value))
        return InvestigateRefuse("The structure investigate text has no exposure line.");
    if (value != TextNone && !InvestigateParseExposure(value, state.Exposure))
        return InvestigateRefuse("The structure investigate text has a malformed exposure line.");
    if (!reader.Pair("tests", value) || !ParseIds(value, state.Exposure.TestsRun))
        return InvestigateRefuse("The structure investigate text has a malformed tests line.");
    if (!reader.Next(line) || line != TextEnd)
        return InvestigateRefuse("The structure investigate text is truncated or has no end line.");
    if (!reader.AtEnd())
        return InvestigateRefuse("The structure investigate text has data after the end line.");
    if (!IsInvestigateStateValid(state))
        return InvestigateRefuse("The structure investigate text describes an inconsistent state.");

    InvestigateParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
