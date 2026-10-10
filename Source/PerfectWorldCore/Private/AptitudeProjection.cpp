#include "AptitudeProjection.h"

#include "AptitudeInternal.h"

#include <algorithm>
#include <functional>
#include <map>
#include <tuple>

// Aptitude Projection (ROADMAP M3-05). The rules and their sources are in the header;
// this file holds the tables (paths, facets, names), the item rules and the ledger.
namespace PerfectWorld
{
namespace
{
using FacetList = std::array<AptitudeFacet, AptitudeFacetsPerPath>;

constexpr AptitudeFacet AptitudeAnalysis = AptitudeFacet::Analysis;
constexpr AptitudeFacet AptitudeInterpretation = AptitudeFacet::Interpretation;
constexpr AptitudeFacet AptitudeFaultFinding = AptitudeFacet::FaultFinding;
constexpr AptitudeFacet AptitudeTeamwork = AptitudeFacet::Teamwork;
constexpr AptitudeFacet AptitudeHandsOn = AptitudeFacet::HandsOn;
constexpr AptitudeFacet AptitudePhysical = AptitudeFacet::Physical;
constexpr AptitudeFacet AptitudeAccounts = AptitudeFacet::Accounts;
constexpr AptitudeFacet AptitudeScheduling = AptitudeFacet::Scheduling;
constexpr AptitudeFacet AptitudeCreative = AptitudeFacet::Creative;
constexpr AptitudeFacet AptitudeSteadiness = AptitudeFacet::Steadiness;

/**
 * The documented evidence kinds of each path, from the verbs, ladders and printout
 * examples of PATHS_AND_PROGRESSION. Teamwork is in every list: each ladder names a
 * team, a lab, a crew, a coalition or an ensemble at its second stage.
 */
constexpr std::array<FacetList, AptitudePathCount> AptitudePathFacets = {{
    // Hunter: read, engage, contain, retreat; "withdrawal, rescue, team communication".
    {AptitudePhysical, AptitudeFaultFinding, AptitudeTeamwork, AptitudeHandsOn, AptitudeSteadiness},
    // Scholar: predict, discriminate, falsify; "analysis established; experimental teamwork unobserved".
    {AptitudeAnalysis, AptitudeInterpretation, AptitudeFaultFinding, AptitudeTeamwork, AptitudeHandsOn},
    // Engineer: instrument, fabricate, stress-test; "good fault diagnosis; no structural project completed".
    {AptitudeFaultFinding, AptitudeHandsOn, AptitudeAnalysis, AptitudeTeamwork, AptitudeSteadiness},
    // Enterprise: bid, bargain, allocate; "keep accounts, deliver a real order, resolve a complaint".
    {AptitudeAccounts, AptitudeAnalysis, AptitudeInterpretation, AptitudeTeamwork, AptitudeSteadiness},
    // Athlete: pace, position, execute, recover; precision, endurance, team and power sports.
    {AptitudePhysical, AptitudeAnalysis, AptitudeTeamwork, AptitudeSteadiness, AptitudeScheduling},
    // Clinician: triage, differentiate, intervene, monitor; consent and communication.
    {AptitudeFaultFinding, AptitudeInterpretation, AptitudeHandsOn, AptitudeTeamwork, AptitudeSteadiness},
    // Public leader: represent, investigate, bargain, ratify.
    {AptitudeInterpretation, AptitudeTeamwork, AptitudeFaultFinding, AptitudeAnalysis, AptitudeSteadiness},
    // Logistician: sequence, buffer, route, prioritise, dispatch, recover.
    {AptitudeScheduling, AptitudeAnalysis, AptitudeTeamwork, AptitudeFaultFinding, AptitudeSteadiness},
    // Artist: compose, interpret, stage, improvise, connect.
    {AptitudeCreative, AptitudeInterpretation, AptitudeTeamwork, AptitudeHandsOn, AptitudeSteadiness},
    // Ecologist: survey, cultivate, graft, balance, restore, steward.
    {AptitudeHandsOn, AptitudeAnalysis, AptitudeFaultFinding, AptitudeTeamwork, AptitudeScheduling},
}};

struct AptitudeSkillFacet
{
    std::string_view SkillId;
    AptitudeFacet Facet;
};

constexpr std::array<AptitudeSkillFacet, 10> AptitudeSkillFacets = {{
    {"skill.academics", AptitudeAnalysis},
    {"skill.commerce", AptitudeAccounts},
    {"skill.communication", AptitudeInterpretation},
    {"skill.computing", AptitudeFaultFinding},
    {"skill.creative", AptitudeCreative},
    {"skill.fitness", AptitudePhysical},
    {"skill.household_tasks", AptitudeHandsOn},
    {"skill.logistics", AptitudeScheduling},
    {"skill.practical_repair", AptitudeHandsOn},
    {"skill.self_management", AptitudeSteadiness},
}};

constexpr std::array<std::string_view, AptitudePathCount> AptitudePathIds = {
    "path.hunter",     "path.scholar",   "path.engineer",   "path.enterprise", "path.athlete",
    "path.clinician",  "path.public_leader", "path.logistician", "path.artist", "path.ecologist"};

constexpr std::array<std::string_view, AptitudePathCount> AptitudePathNames = {
    "Hunting",   "Research",          "Engineering", "Enterprise", "Athletics",
    "Clinical work", "Public leadership", "Logistics",   "The arts",   "Ecology"};

constexpr std::array<std::string_view, AptitudeFacetCount> AptitudeFacetIds = {
    "analysis", "interpretation", "fault_finding", "teamwork", "hands_on",
    "physical", "accounts",       "scheduling",    "creative", "steadiness"};

constexpr std::array<std::string_view, AptitudeFacetCount> AptitudeFacetNames = {
    "analysis",        "interpretation", "fault finding", "teamwork",  "hands-on work",
    "physical performance", "accounts",  "scheduling",    "creative work", "steadiness"};

constexpr std::array<std::string_view, 7> AptitudeSourceIds = {
    "structure_model", "structure_explain", "structure_investigate", "structure_coordinate",
    "practice",        "activity",          "attendance"};

constexpr std::array<std::string_view, 3> AptitudeResultIds = {"unsound", "partial", "sound"};

bool AptitudeIsStructure(AptitudeSource source)
{
    return source == AptitudeSource::StructureModel || source == AptitudeSource::StructureExplain ||
           source == AptitudeSource::StructureInvestigate || source == AptitudeSource::StructureCoordinate;
}

/** The one facet each classroom structure can evidence. */
AptitudeFacet AptitudeStructureFacet(AptitudeSource source)
{
    switch (source)
    {
    case AptitudeSource::StructureModel:
        return AptitudeFacet::Analysis;
    case AptitudeSource::StructureExplain:
        return AptitudeFacet::Interpretation;
    case AptitudeSource::StructureInvestigate:
        return AptitudeFacet::FaultFinding;
    default:
        return AptitudeFacet::Teamwork;
    }
}

bool AptitudeEnumsInRange(const AptitudeEvidence& item)
{
    return static_cast<std::size_t>(item.Source) < AptitudeSourceIds.size() &&
           static_cast<std::size_t>(item.Facet) < AptitudeFacetCount &&
           static_cast<std::size_t>(item.Result) < AptitudeResultIds.size();
}

/** Sorted (source, facet, subject) keys of a ledger's items, to count a group's size. */
bool AptitudeGroupsWithinKeep(const std::vector<AptitudeEvidence>& items)
{
    std::map<std::tuple<AptitudeSource, AptitudeFacet, std::string_view>, std::size_t> counts;
    for (const AptitudeEvidence& item : items)
    {
        if (++counts[{item.Source, item.Facet, item.Subject}] > AptitudeGroupKeep)
            return false;
    }
    return true;
}
} // namespace

namespace AptitudeInternal
{
bool IdWellFormed(std::string_view id)
{
    if (id.empty() || id.size() > AptitudeMaxIdLength)
        return false;
    for (const char c : id)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok)
            return false;
    }
    return true;
}

std::int32_t ItemPoints(const AptitudeEvidence& item)
{
    if (item.Result == AptitudeResult::Unsound)
        return 0;
    const bool sound = item.Result == AptitudeResult::Sound;
    switch (item.Source)
    {
    case AptitudeSource::PracticeConsolidation:
    case AptitudeSource::ActivityCompletion:
        return sound ? 4 : 2;
    case AptitudeSource::Attendance:
        return sound ? 2 : 1;
    default:
        // A routine exposure is documented evidence of an established method, at half a foreground Sound.
        return item.Routine ? 4 : (sound ? 8 : 4);
    }
}

bool ItemBefore(const AptitudeEvidence& a, const AptitudeEvidence& b)
{
    const std::int32_t pointsA = ItemPoints(a);
    const std::int32_t pointsB = ItemPoints(b);
    if (pointsA != pointsB)
        return pointsA > pointsB;
    return std::tie(a.At, a.Id, a.Source, a.Facet, a.Subject, a.Result, a.Routine) <
           std::tie(b.At, b.Id, b.Source, b.Facet, b.Subject, b.Result, b.Routine);
}

bool SameGroup(const AptitudeEvidence& a, const AptitudeEvidence& b)
{
    return a.Source == b.Source && a.Facet == b.Facet && a.Subject == b.Subject;
}

std::optional<AptitudeSource> SourceFromId(std::string_view id)
{
    for (std::size_t i = 0; i < AptitudeSourceIds.size(); ++i)
    {
        if (AptitudeSourceIds[i] == id)
            return static_cast<AptitudeSource>(i);
    }
    return std::nullopt;
}

std::optional<AptitudeFacet> FacetFromId(std::string_view id)
{
    for (std::size_t i = 0; i < AptitudeFacetIds.size(); ++i)
    {
        if (AptitudeFacetIds[i] == id)
            return static_cast<AptitudeFacet>(i);
    }
    return std::nullopt;
}

std::optional<AptitudeResult> ResultFromId(std::string_view id)
{
    for (std::size_t i = 0; i < AptitudeResultIds.size(); ++i)
    {
        if (AptitudeResultIds[i] == id)
            return static_cast<AptitudeResult>(i);
    }
    return std::nullopt;
}
} // namespace AptitudeInternal

std::string_view AptitudePathId(AptitudePath path) noexcept
{
    const std::size_t index = static_cast<std::size_t>(path);
    return index < AptitudePathCount ? AptitudePathIds[index] : std::string_view{};
}

std::string_view AptitudePathName(AptitudePath path) noexcept
{
    const std::size_t index = static_cast<std::size_t>(path);
    return index < AptitudePathCount ? AptitudePathNames[index] : std::string_view{};
}

std::string_view AptitudeFacetId(AptitudeFacet facet) noexcept
{
    const std::size_t index = static_cast<std::size_t>(facet);
    return index < AptitudeFacetCount ? AptitudeFacetIds[index] : std::string_view{};
}

std::string_view AptitudeFacetName(AptitudeFacet facet) noexcept
{
    const std::size_t index = static_cast<std::size_t>(facet);
    return index < AptitudeFacetCount ? AptitudeFacetNames[index] : std::string_view{};
}

std::string_view AptitudeSourceId(AptitudeSource source) noexcept
{
    const std::size_t index = static_cast<std::size_t>(source);
    return index < AptitudeSourceIds.size() ? AptitudeSourceIds[index] : std::string_view{};
}

std::string_view AptitudeResultId(AptitudeResult result) noexcept
{
    const std::size_t index = static_cast<std::size_t>(result);
    return index < AptitudeResultIds.size() ? AptitudeResultIds[index] : std::string_view{};
}

std::string_view AptitudeBandText(AptitudeBand band) noexcept
{
    switch (band)
    {
    case AptitudeBand::InsufficientObservation:
        return "Insufficient observation";
    case AptitudeBand::Developing:
        return "Early evidence";
    case AptitudeBand::Plausible:
        return "Plausible";
    case AptitudeBand::Strong:
        return "Strong evidence";
    }
    return {};
}

std::string_view AptitudeCoverageText(AptitudeCoverage coverage) noexcept
{
    switch (coverage)
    {
    case AptitudeCoverage::Unknown:
        return "Not yet observed";
    case AptitudeCoverage::Sparse:
        return "Little observed";
    case AptitudeCoverage::Partial:
        return "Partly observed";
    case AptitudeCoverage::Broad:
        return "Broadly observed";
    }
    return {};
}

std::string_view AptitudeFacetStateText(AptitudeFacetState state) noexcept
{
    switch (state)
    {
    case AptitudeFacetState::Unobserved:
        return "Unobserved";
    case AptitudeFacetState::Observed:
        return "Observed, not yet demonstrated";
    case AptitudeFacetState::Shown:
        return "Shown";
    case AptitudeFacetState::Established:
        return "Established";
    }
    return {};
}

const std::array<AptitudeFacet, AptitudeFacetsPerPath>& AptitudeFacetsForPath(AptitudePath path) noexcept
{
    const std::size_t index = static_cast<std::size_t>(path);
    return AptitudePathFacets[index < AptitudePathCount ? index : 0];
}

std::vector<AptitudePath> AptitudePathsForFacet(AptitudeFacet facet)
{
    std::vector<AptitudePath> paths;
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
    {
        const FacetList& facets = AptitudePathFacets[p];
        if (std::find(facets.begin(), facets.end(), facet) != facets.end())
            paths.push_back(static_cast<AptitudePath>(p));
    }
    return paths;
}

bool AptitudeFacetForSkill(std::string_view skillId, AptitudeFacet& facet) noexcept
{
    for (const AptitudeSkillFacet& entry : AptitudeSkillFacets)
    {
        if (entry.SkillId == skillId)
        {
            facet = entry.Facet;
            return true;
        }
    }
    return false;
}

std::string_view AptitudeErrorCode(AptitudeError error) noexcept
{
    switch (error)
    {
    case AptitudeError::None:
        return {};
    case AptitudeError::InvalidLedger:
        return "aptitude.invalid_ledger";
    case AptitudeError::InvalidEvidence:
        return "aptitude.invalid_evidence";
    case AptitudeError::TooMany:
        return "aptitude.too_many";
    }
    return {};
}

bool IsAptitudeEvidenceValid(const AptitudeEvidence& item)
{
    if (!AptitudeEnumsInRange(item) || !AptitudeInternal::IdWellFormed(item.Id) ||
        !AptitudeInternal::IdWellFormed(item.Subject) || item.At < 0)
        return false;
    if (AptitudeIsStructure(item.Source))
    {
        // A routine exposure is a Sound result by definition.
        return item.Facet == AptitudeStructureFacet(item.Source) && (!item.Routine || item.Result == AptitudeResult::Sound);
    }
    if (item.Routine)
        return false;
    if (item.Source == AptitudeSource::Attendance && item.Facet != AptitudeFacet::Steadiness)
        return false;
    // Practice and attendance never record weakness: a missed class or a failed attempt is simply not evidence.
    const bool neverUnsound = item.Source == AptitudeSource::PracticeConsolidation || item.Source == AptitudeSource::Attendance;
    return !(neverUnsound && item.Result == AptitudeResult::Unsound);
}

namespace
{
/** The linear part of ledger validity: bounded, every item valid, IDs strictly ascending. */
bool AptitudeLedgerShapeValid(const AptitudeLedger& ledger)
{
    if (ledger.Items.size() > AptitudeMaxItems)
        return false;
    for (std::size_t i = 0; i < ledger.Items.size(); ++i)
    {
        if (!IsAptitudeEvidenceValid(ledger.Items[i]) || (i > 0 && !(ledger.Items[i - 1].Id < ledger.Items[i].Id)))
            return false;
    }
    return true;
}
} // namespace

bool IsAptitudeLedgerValid(const AptitudeLedger& ledger)
{
    return AptitudeLedgerShapeValid(ledger) && AptitudeGroupsWithinKeep(ledger.Items);
}

AptitudeAddResult AptitudeAddEvidence(AptitudeLedger& ledger, const AptitudeEvidence& item)
{
    using namespace AptitudeInternal;
    // Only the linear checks per add; the group sizes are re-counted for the item's own group below.
    if (!AptitudeLedgerShapeValid(ledger))
        return {AptitudeError::InvalidLedger, AptitudeAddOutcome::Saturated};
    if (!IsAptitudeEvidenceValid(item))
        return {AptitudeError::InvalidEvidence, AptitudeAddOutcome::Saturated};

    std::vector<AptitudeEvidence>& items = ledger.Items;
    const auto idPosition = std::lower_bound(items.begin(), items.end(), item.Id,
                                             [](const AptitudeEvidence& e, const std::string& key) { return e.Id < key; });
    const bool hasOld = idPosition != items.end() && idPosition->Id == item.Id;
    if (hasOld && (*idPosition == item || !ItemBefore(item, *idPosition)))
        return {AptitudeError::None, AptitudeAddOutcome::AlreadyKept};
    const std::size_t oldIndex = hasOld ? static_cast<std::size_t>(idPosition - items.begin()) : items.size();

    // The group's other members, and the worst of them.
    std::size_t members = 0;
    std::size_t worst = items.size();
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        if (i == oldIndex || !SameGroup(items[i], item))
            continue;
        ++members;
        if (worst == items.size() || ItemBefore(items[worst], items[i]))
            worst = i;
    }

    std::size_t evict = items.size();
    if (members >= AptitudeGroupKeep)
    {
        if (!ItemBefore(item, items[worst]))
            return {AptitudeError::None, AptitudeAddOutcome::Saturated};
        evict = worst;
    }
    else if (!hasOld && items.size() >= AptitudeMaxItems)
        return {AptitudeError::TooMany, AptitudeAddOutcome::Saturated};

    // Erase the replaced copy and the evicted worst, higher index first so the lower stays valid.
    std::vector<std::size_t> erase;
    if (hasOld)
        erase.push_back(oldIndex);
    if (evict != items.size())
        erase.push_back(evict);
    std::sort(erase.begin(), erase.end(), std::greater<std::size_t>());
    for (const std::size_t index : erase)
        items.erase(items.begin() + static_cast<std::ptrdiff_t>(index));
    const auto position = std::lower_bound(items.begin(), items.end(), item.Id,
                                           [](const AptitudeEvidence& e, const std::string& key) { return e.Id < key; });
    items.insert(position, item);
    return {AptitudeError::None, AptitudeAddOutcome::Added};
}
} // namespace PerfectWorld
