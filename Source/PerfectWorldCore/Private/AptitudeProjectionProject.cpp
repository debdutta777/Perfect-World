#include "AptitudeProjection.h"

#include "AptitudeInternal.h"

#include <algorithm>
#include <tuple>

// The projection itself (ROADMAP M3-05): the private tallies, the facet states, the bands and
// the printout. The tallies are points inside this file; nothing returned carries one.
namespace PerfectWorld
{
namespace
{
struct AptitudeFacetTally
{
    bool Observed = false;
    std::int32_t Points = 0;
};

using AptitudeTallies = std::array<AptitudeFacetTally, AptitudeFacetCount>;

/** Per facet: observed at all, and the points of every source kind (each capped), summed. */
AptitudeTallies AptitudeTally(const std::vector<AptitudeEvidence>& kept)
{
    using namespace AptitudeInternal;
    std::vector<const AptitudeEvidence*> sorted;
    sorted.reserve(kept.size());
    for (const AptitudeEvidence& item : kept)
        sorted.push_back(&item);
    std::sort(sorted.begin(), sorted.end(),
              [](const AptitudeEvidence* a, const AptitudeEvidence* b)
              {
                  const auto keyA = std::tie(a->Facet, a->Source, a->Subject);
                  const auto keyB = std::tie(b->Facet, b->Source, b->Subject);
                  if (keyA != keyB)
                      return keyA < keyB;
                  return ItemBefore(*a, *b);
              });

    AptitudeTallies tallies{};
    // One source kind's points for the facet being walked, capped when the source changes.
    std::int32_t sourcePoints = 0;
    std::size_t rank = 0;
    for (std::size_t i = 0; i < sorted.size(); ++i)
    {
        const AptitudeEvidence& item = *sorted[i];
        const bool newGroup = i == 0 || !SameGroup(*sorted[i - 1], item);
        const bool newSource = i == 0 || sorted[i - 1]->Source != item.Source || sorted[i - 1]->Facet != item.Facet;
        if (newSource && i > 0)
        {
            tallies[static_cast<std::size_t>(sorted[i - 1]->Facet)].Points += std::min(sourcePoints, AptitudeSourceFacetCap);
            sourcePoints = 0;
        }
        rank = newGroup ? 0 : rank + 1;
        AptitudeFacetTally& tally = tallies[static_cast<std::size_t>(item.Facet)];
        tally.Observed = true;
        sourcePoints += ItemPoints(item) >> std::min<std::size_t>(rank, 30);
    }
    if (!sorted.empty())
        tallies[static_cast<std::size_t>(sorted.back()->Facet)].Points += std::min(sourcePoints, AptitudeSourceFacetCap);
    return tallies;
}

AptitudeFacetState AptitudeStateOf(const AptitudeFacetTally& tally)
{
    if (!tally.Observed)
        return AptitudeFacetState::Unobserved;
    if (tally.Points >= AptitudeEstablishedMin)
        return AptitudeFacetState::Established;
    return tally.Points >= AptitudeShownMin ? AptitudeFacetState::Shown : AptitudeFacetState::Observed;
}

AptitudeCoverage AptitudeCoverageOf(std::size_t observed)
{
    if (observed == 0)
        return AptitudeCoverage::Unknown;
    if (observed == 1)
        return AptitudeCoverage::Sparse;
    return observed <= 3 ? AptitudeCoverage::Partial : AptitudeCoverage::Broad;
}

/** `signature` is the state of the path's first listed facet. */
AptitudeBand AptitudeBandOf(std::size_t observed, std::size_t shown, std::size_t established, AptitudeFacetState signature)
{
    if (observed < AptitudePathMinObserved || signature == AptitudeFacetState::Unobserved)
        return AptitudeBand::InsufficientObservation;
    if (signature == AptitudeFacetState::Established && established >= 2 && shown >= 3)
        return AptitudeBand::Strong;
    return signature >= AptitudeFacetState::Shown && shown >= 2 ? AptitudeBand::Plausible : AptitudeBand::Developing;
}

/** The strongest items, best first, bearing on any of the path's facets. Unsound items never support. */
std::vector<std::string> AptitudeStrongest(const std::vector<AptitudeEvidence>& kept, AptitudePath path)
{
    const auto& facets = AptitudeFacetsForPath(path);
    std::vector<const AptitudeEvidence*> bearing;
    for (const AptitudeEvidence& item : kept)
    {
        if (AptitudeInternal::ItemPoints(item) > 0 && std::find(facets.begin(), facets.end(), item.Facet) != facets.end())
            bearing.push_back(&item);
    }
    std::sort(bearing.begin(), bearing.end(),
              [](const AptitudeEvidence* a, const AptitudeEvidence* b) { return AptitudeInternal::ItemBefore(*a, *b); });
    std::vector<std::string> ids;
    for (std::size_t i = 0; i < bearing.size() && i < AptitudeMaxReferences; ++i)
        ids.push_back(bearing[i]->Id);
    return ids;
}

/** "analysis, teamwork" for the facets of the path with this state. Empty when there are none. */
std::string AptitudeListWith(const AptitudePathProjection& path, AptitudeFacetState state)
{
    std::string list;
    for (const AptitudeFacetStatement& statement : path.Facets)
    {
        if (statement.State != state)
            continue;
        list += list.empty() ? "" : ", ";
        list += AptitudeFacetName(statement.Facet);
    }
    return list;
}
} // namespace

const AptitudePathProjection* AptitudeProjection::Find(AptitudePath path) const noexcept
{
    const std::size_t index = static_cast<std::size_t>(path);
    return index < Paths.size() ? &Paths[index] : nullptr;
}

AptitudeProjection AptitudeProject(const AptitudeLedger& ledger)
{
    AptitudeProjection projection;
    if (!IsAptitudeLedgerValid(ledger))
        return projection;
    const AptitudeTallies tallies = AptitudeTally(ledger.Items);
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
    {
        const AptitudePath path = static_cast<AptitudePath>(p);
        AptitudePathProjection result;
        result.Path = path;
        std::size_t observed = 0;
        std::size_t shown = 0;
        std::size_t established = 0;
        for (const AptitudeFacet facet : AptitudeFacetsForPath(path))
        {
            const AptitudeFacetState state = AptitudeStateOf(tallies[static_cast<std::size_t>(facet)]);
            result.Facets.push_back({facet, state});
            observed += state != AptitudeFacetState::Unobserved ? 1 : 0;
            shown += state >= AptitudeFacetState::Shown ? 1 : 0;
            established += state == AptitudeFacetState::Established ? 1 : 0;
            if (state == AptitudeFacetState::Unobserved)
                result.Unobserved.push_back(facet);
        }
        result.Coverage = AptitudeCoverageOf(observed);
        result.Band = AptitudeBandOf(observed, shown, established, result.Facets.front().State);
        result.Strongest = AptitudeStrongest(ledger.Items, path);
        projection.Paths.push_back(std::move(result));
    }
    return projection;
}

std::string AptitudePrintoutLine(const AptitudePathProjection& path)
{
    std::string line(AptitudePathName(path.Path));
    line += ": ";
    line += AptitudeBandText(path.Band);
    line += ".";
    const std::pair<AptitudeFacetState, std::string_view> parts[] = {
        {AptitudeFacetState::Established, "Established"},
        {AptitudeFacetState::Shown, "Shown"},
        {AptitudeFacetState::Observed, "Observed, not yet demonstrated"},
        {AptitudeFacetState::Unobserved, "Unobserved"}};
    for (const auto& [state, label] : parts)
    {
        const std::string list = AptitudeListWith(path, state);
        if (!list.empty())
            line += " " + std::string(label) + ": " + list + ".";
    }
    if (path.Band == AptitudeBand::InsufficientObservation)
        line += " This means unknown, not unsuitable.";
    return line;
}
} // namespace PerfectWorld
