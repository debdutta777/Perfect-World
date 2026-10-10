#include "ConductLedger.h"

#include "IdentityRegistry.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, ConductDimensionCount> ConductDimensionNames = {
    "care", "steadfastness", "restraint", "resourcefulness", "attentiveness"};

constexpr std::array<std::string_view, ConductActionCount> ConductActionNames = {
    "observe", "assist", "evade", "improvise", "withdraw", "hold"};

constexpr std::array<std::string_view, ConductBandCount> ConductBandNames = {"not_observed", "slight", "clear", "strong"};

// Words that may not appear in a test, choice or party token (fairness: nothing is keyed on them).
constexpr std::array<std::string_view, 16> ConductForbiddenWords = {
    "religion", "religions", "religious", "faith",  "caste",  "language", "languages", "tongue",
    "stream",   "gender",    "sex",       "male",   "female", "family",   "household", "background"};

struct ConductActionRow
{
    ConductDimension Primary;
    ConductDimension Secondary;
};

// One row per ConductAction, in enum order. Primary gets ConductPrimaryPoints, secondary ConductSecondaryPoints.
// Every row therefore totals the same, so no action is the lesser one.
constexpr std::array<ConductActionRow, ConductActionCount> ConductRows = {{
    {ConductDimension::Attentiveness, ConductDimension::Restraint},       // observe
    {ConductDimension::Care, ConductDimension::Steadfastness},            // assist
    {ConductDimension::Restraint, ConductDimension::Resourcefulness},     // evade
    {ConductDimension::Resourcefulness, ConductDimension::Attentiveness}, // improvise
    {ConductDimension::Restraint, ConductDimension::Attentiveness},       // withdraw
    {ConductDimension::Steadfastness, ConductDimension::Care},            // hold
}};

bool ConductActionInRange(ConductAction action)
{
    return static_cast<std::size_t>(action) < ConductActionCount;
}

bool ConductWordForbidden(std::string_view word)
{
    return std::find(ConductForbiddenWords.begin(), ConductForbiddenWords.end(), word) != ConductForbiddenWords.end();
}

bool ConductIsSeparator(char c)
{
    return c == '_' || c == '.' || c == ':' || c == '-';
}

std::int32_t ConductSum(const ConductPoints& points)
{
    std::int32_t total = 0;
    for (const std::int32_t value : points)
        total += value;
    return total;
}
} // namespace

std::string_view ConductDimensionName(ConductDimension dimension)
{
    const std::size_t index = static_cast<std::size_t>(dimension);
    return index < ConductDimensionCount ? ConductDimensionNames[index] : std::string_view();
}

std::optional<ConductDimension> ParseConductDimension(std::string_view name)
{
    for (std::size_t i = 0; i < ConductDimensionCount; ++i)
    {
        if (ConductDimensionNames[i] == name)
            return static_cast<ConductDimension>(i);
    }
    return std::nullopt;
}

std::string_view ConductActionName(ConductAction action)
{
    return ConductActionInRange(action) ? ConductActionNames[static_cast<std::size_t>(action)] : std::string_view();
}

std::optional<ConductAction> ParseConductAction(std::string_view name)
{
    for (std::size_t i = 0; i < ConductActionCount; ++i)
    {
        if (ConductActionNames[i] == name)
            return static_cast<ConductAction>(i);
    }
    return std::nullopt;
}

std::string_view ConductBandName(ConductBand band)
{
    const std::size_t index = static_cast<std::size_t>(band);
    return index < ConductBandCount ? ConductBandNames[index] : std::string_view();
}

ConductBand ConductBandFor(std::int32_t dimensionTotal)
{
    if (dimensionTotal >= ConductStrongThreshold)
        return ConductBand::Strong;
    if (dimensionTotal >= ConductClearThreshold)
        return ConductBand::Clear;
    return dimensionTotal > 0 ? ConductBand::Slight : ConductBand::NotObserved;
}

ConductPoints ConductActionEvidence(ConductAction action, std::int32_t stakes)
{
    ConductPoints points{};
    if (!ConductActionInRange(action) || stakes < ConductMinStakes || stakes > ConductMaxStakes)
        return points;
    const ConductActionRow& row = ConductRows[static_cast<std::size_t>(action)];
    points[static_cast<std::size_t>(row.Primary)] = std::min(ConductPrimaryPoints * stakes, ConductTestDimensionCap);
    points[static_cast<std::size_t>(row.Secondary)] = std::min(ConductSecondaryPoints * stakes, ConductTestDimensionCap);
    const std::int32_t sum = ConductSum(points);
    if (sum > ConductTestTotalCap)
    {
        for (std::int32_t& value : points)
            value = value * ConductTestTotalCap / sum;
    }
    return points;
}

std::int32_t ConductActionTotal(std::int32_t stakes)
{
    return ConductSum(ConductActionEvidence(ConductAction::Observe, stakes));
}

bool IsConductTokenFair(std::string_view text)
{
    if (!IsStableIdToken(text))
        return false;
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t end = start;
        while (end < text.size() && !ConductIsSeparator(text[end]))
            ++end;
        if (ConductWordForbidden(text.substr(start, end - start)))
            return false;
        start = end + 1;
    }
    return true;
}

const ConductEntry* FindConductEntry(const ConductLedger& ledger, std::string_view testId)
{
    for (const ConductEntry& entry : ledger.Entries)
    {
        if (entry.Record.TestId == testId)
            return &entry;
    }
    return nullptr;
}

ConductResult RecordConduct(ConductLedger& ledger, const ConductRecord& record, const IdentityRegistry* registry)
{
    ConductResult result;
    if (!IsConductTokenFair(record.TestId))
    {
        result.Status = ConductStatus::BadTest;
        return result;
    }
    if (const ConductEntry* existing = FindConductEntry(ledger, record.TestId))
    {
        result.Status = ConductStatus::Replay;
        result.Points = existing->Points;
        return result;
    }
    if (ledger.Entries.size() >= ConductMaxTests)
    {
        result.Status = ConductStatus::LedgerFull;
        return result;
    }
    if (!IsConductTokenFair(record.ChoiceId))
    {
        result.Status = ConductStatus::BadChoice;
        return result;
    }
    if (!ConductActionInRange(record.Action))
    {
        result.Status = ConductStatus::BadAction;
        return result;
    }
    if (record.Stakes < ConductMinStakes || record.Stakes > ConductMaxStakes)
    {
        result.Status = ConductStatus::BadStakes;
        return result;
    }
    if (record.At < 0 || (!ledger.Entries.empty() && record.At < ledger.Entries.back().Record.At))
    {
        result.Status = ConductStatus::BadTime;
        return result;
    }
    if (record.Parties.size() > ConductMaxParties)
    {
        result.Status = ConductStatus::BadParties;
        return result;
    }
    std::vector<std::string> parties = record.Parties;
    std::sort(parties.begin(), parties.end());
    for (std::size_t i = 0; i < parties.size(); ++i)
    {
        if ((i > 0 && parties[i] == parties[i - 1]) || !IsValidIdentityId(parties[i]))
        {
            result.Status = ConductStatus::BadParties;
            return result;
        }
        if (!IsConductTokenFair(parties[i]))
        {
            result.Status = ConductStatus::UnfairToken;
            return result;
        }
        if (registry != nullptr && FindIdentity(*registry, parties[i]) == nullptr)
        {
            result.Status = ConductStatus::UnknownIdentity;
            return result;
        }
    }

    ConductEntry entry;
    entry.Record = record;
    entry.Record.Parties = std::move(parties);
    entry.Points = ConductActionEvidence(record.Action, record.Stakes);
    result.Points = entry.Points;
    result.Status = ConductStatus::Recorded;
    ledger.Entries.push_back(std::move(entry));
    return result;
}

ConductPoints ConductTotals(const ConductLedger& ledger)
{
    std::array<std::int64_t, ConductDimensionCount> sums{};
    for (const ConductEntry& entry : ledger.Entries)
    {
        for (std::size_t d = 0; d < ConductDimensionCount; ++d)
            sums[d] += entry.Points[d];
    }
    ConductPoints totals{};
    for (std::size_t d = 0; d < ConductDimensionCount; ++d)
        totals[d] = static_cast<std::int32_t>(std::min<std::int64_t>(sums[d], ConductDimensionCap));
    return totals;
}

ConductSummary SummariseConduct(const ConductLedger& ledger)
{
    ConductSummary summary;
    summary.TestsRecorded = static_cast<std::uint32_t>(ledger.Entries.size());
    const ConductPoints totals = ConductTotals(ledger);
    for (std::size_t d = 0; d < ConductDimensionCount; ++d)
    {
        ConductDimensionSummary& out = summary.Dimensions[d];
        out.Band = ConductBandFor(totals[d]);
        out.Milli = totals[d] * 1000 / ConductDimensionCap;
        for (const ConductEntry& entry : ledger.Entries)
        {
            if (entry.Points[d] > 0)
                out.Evidence.push_back({entry.Record.TestId, entry.Record.ChoiceId, entry.Points[d]});
        }
        // Stable: equal points keep resolution order.
        std::stable_sort(out.Evidence.begin(), out.Evidence.end(),
                         [](const ConductEvidenceRef& a, const ConductEvidenceRef& b) { return a.Points > b.Points; });
        if (out.Evidence.size() > ConductTopEvidenceCount)
            out.Evidence.resize(ConductTopEvidenceCount);
    }
    return summary;
}
} // namespace PerfectWorld
