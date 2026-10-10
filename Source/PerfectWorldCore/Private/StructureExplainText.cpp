#include "StructureExplain.h"

#include "StructureInternal.h"

#include <algorithm>

// The validity rules and text form of ExplainState (ROADMAP M2-09): what a save taken
// mid-exposure carries. Integers and stable IDs only; the parser is strict, so a
// truncated or edited save is refused rather than half-read.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
constexpr std::string_view ExplainTextHeader = "pw.structure_explain/1";

bool ExplainSortedUniqueIds(const std::vector<std::string>& ids, std::size_t limit)
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

bool ExplainExposureValid(const ExplainState& state)
{
    const ExplainExposure& e = state.Exposure;
    if (!e.Active())
        return e == ExplainExposure{};
    if (!BaseExposureValid(state.Families, e) || static_cast<std::size_t>(e.Goal) >= ExplainGoalNames.size() ||
        static_cast<std::size_t>(e.Beat) >= ExplainBeatNames.size() || (e.MethodKnown && e.ParagraphTaken))
        return false;
    if (!e.ClaimId.empty() && !ModelIdWellFormed(e.ClaimId))
        return false;
    if (!ExplainSortedUniqueIds(e.EvidenceIds, ExplainEvidenceSlots))
        return false;
    switch (e.Beat)
    {
    case ExplainBeat::Claim:
        return e.ClaimId.empty() && e.EvidenceIds.empty();
    case ExplainBeat::Evidence:
        return !e.ClaimId.empty() && e.EvidenceIds.empty();
    case ExplainBeat::Limitation:
        return !e.ClaimId.empty() && !e.EvidenceIds.empty();
    }
    return false;
}

ExplainParse ExplainRefuse(std::string message)
{
    ExplainParse result;
    result.Error = std::move(message);
    return result;
}

bool ExplainParseExposure(std::string_view value, ExplainExposure& exposure)
{
    const std::vector<std::string_view> parts = Split(value);
    if (parts.size() != 8 || !ModelIdWellFormed(parts[0]) || !ModelIdWellFormed(parts[1]))
        return false;
    exposure.FamilyId = std::string(parts[0]);
    exposure.CaseId = std::string(parts[1]);
    return ModelParseName(ExplainGoalNames, parts[2], exposure.Goal) && ModelParseName(ExplainBeatNames, parts[3], exposure.Beat) &&
           ParseInt(parts[4], exposure.UnitsTotal) && ParseInt(parts[5], exposure.UnitsLeft) && ParseFlag(parts[6], exposure.MethodKnown) &&
           ParseFlag(parts[7], exposure.ParagraphTaken);
}
} // namespace

bool IsExplainStateValid(const ExplainState& state)
{
    return IsStructureFamilyListValid(state.Families) && ExplainExposureValid(state);
}

std::string SerialiseStructureExplain(const ExplainState& state)
{
    if (!IsExplainStateValid(state))
        return {};
    std::string text(ExplainTextHeader);
    text += "\n";
    AppendFamilies(text, state.Families);
    const ExplainExposure& e = state.Exposure;
    text += "exposure=";
    if (!e.Active())
        text += TextNone;
    else
        text += e.FamilyId + "," + e.CaseId + "," + std::string(ModelNameOf(ExplainGoalNames, e.Goal)) + "," +
                std::string(ModelNameOf(ExplainBeatNames, e.Beat)) + "," + std::to_string(e.UnitsTotal) + "," + std::to_string(e.UnitsLeft) +
                "," + (e.MethodKnown ? "1" : "0") + "," + (e.ParagraphTaken ? "1" : "0");
    text += "\nclaim=" + (e.ClaimId.empty() ? std::string(TextNone) : e.ClaimId);
    text += "\nevidence=" + Join(e.EvidenceIds) + "\n";
    text += TextEnd;
    text += "\n";
    return text;
}

ExplainParse DeserialiseStructureExplain(std::string_view text)
{
    LineReader reader(text);
    std::string_view line;
    std::string_view value;
    if (!reader.Next(line) || line != ExplainTextHeader)
        return ExplainRefuse("The structure explain text has no recognised header.");

    ExplainState state;
    if (!ReadFamilies(reader, state.Families))
        return ExplainRefuse("The structure explain text has a malformed families section.");
    if (!reader.Pair("exposure", value))
        return ExplainRefuse("The structure explain text has no exposure line.");
    if (value != TextNone && !ExplainParseExposure(value, state.Exposure))
        return ExplainRefuse("The structure explain text has a malformed exposure line.");
    if (!reader.Pair("claim", value) || (value != TextNone && !ModelIdWellFormed(value)))
        return ExplainRefuse("The structure explain text has a malformed claim line.");
    if (value != TextNone)
        state.Exposure.ClaimId = std::string(value);
    if (!reader.Pair("evidence", value) || !ParseIds(value, state.Exposure.EvidenceIds))
        return ExplainRefuse("The structure explain text has a malformed evidence line.");
    if (!reader.Next(line) || line != TextEnd)
        return ExplainRefuse("The structure explain text is truncated or has no end line.");
    if (!reader.AtEnd())
        return ExplainRefuse("The structure explain text has data after the end line.");
    if (!IsExplainStateValid(state))
        return ExplainRefuse("The structure explain text describes an inconsistent state.");

    ExplainParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
