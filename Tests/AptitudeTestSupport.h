#pragma once

// Shared helpers for the Aptitude Projection tests (ROADMAP M3-05).
#include "AptitudeProjection.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace AptitudeTests
{
using namespace PerfectWorld;

inline int Checks = 0;

inline void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

// A small deterministic generator for the tests (xorshift64*).
struct Rng
{
    std::uint64_t S;
    std::uint64_t Next()
    {
        S ^= S >> 12;
        S ^= S << 25;
        S ^= S >> 27;
        return (S * 2685821657736338717ULL) >> 16;
    }
    std::int64_t Pick(std::int64_t low, std::int64_t high)
    {
        return low + static_cast<std::int64_t>(Next() % static_cast<std::uint64_t>(high - low + 1));
    }
};

inline AptitudeEvidence Item(std::string id, AptitudeSource source, AptitudeFacet facet, std::string subject,
                             AptitudeResult result = AptitudeResult::Sound, GameInstant at = 0, bool routine = false)
{
    return AptitudeEvidence{std::move(id), source, facet, std::move(subject), result, routine, at};
}

/** A foreground structure exposure of the structure's own facet. */
inline AptitudeEvidence Structure(std::string id, AptitudeSource source, std::string family,
                                  AptitudeResult result = AptitudeResult::Sound, GameInstant at = 0, bool routine = false)
{
    AptitudeFacet facet = AptitudeFacet::Analysis;
    if (source == AptitudeSource::StructureExplain)
        facet = AptitudeFacet::Interpretation;
    else if (source == AptitudeSource::StructureInvestigate)
        facet = AptitudeFacet::FaultFinding;
    else if (source == AptitudeSource::StructureCoordinate)
        facet = AptitudeFacet::Teamwork;
    return Item(std::move(id), source, facet, std::move(family), result, at, routine);
}

inline void Add(AptitudeLedger& ledger, const AptitudeEvidence& item)
{
    Check(AptitudeAddEvidence(ledger, item).Ok(), "the evidence is accepted");
}

inline const AptitudePathProjection& PathOf(const AptitudeProjection& projection, AptitudePath path)
{
    const AptitudePathProjection* found = projection.Find(path);
    Check(found != nullptr, "the projection has the path");
    return *found;
}

inline AptitudeBand BandOf(const AptitudeLedger& ledger, AptitudePath path)
{
    return PathOf(AptitudeProject(ledger), path).Band;
}

inline AptitudeFacetState StateOf(const AptitudePathProjection& path, AptitudeFacet facet)
{
    for (const AptitudeFacetStatement& statement : path.Facets)
    {
        if (statement.Facet == facet)
            return statement.State;
    }
    Check(false, "the path documents the facet");
    return AptitudeFacetState::Unobserved;
}

/** A random valid item with a unique ID, drawn over every source, facet and a small set of subjects. */
inline AptitudeEvidence RandomItem(Rng& rng, std::uint64_t serial)
{
    const AptitudeSource source = static_cast<AptitudeSource>(rng.Pick(0, 6));
    AptitudeFacet facet = static_cast<AptitudeFacet>(rng.Pick(0, 9));
    AptitudeResult result = static_cast<AptitudeResult>(rng.Pick(0, 2));
    bool routine = false;
    switch (source)
    {
    case AptitudeSource::StructureModel:
        facet = AptitudeFacet::Analysis;
        break;
    case AptitudeSource::StructureExplain:
        facet = AptitudeFacet::Interpretation;
        break;
    case AptitudeSource::StructureInvestigate:
        facet = AptitudeFacet::FaultFinding;
        break;
    case AptitudeSource::StructureCoordinate:
        facet = AptitudeFacet::Teamwork;
        break;
    case AptitudeSource::Attendance:
        facet = AptitudeFacet::Steadiness;
        break;
    default:
        break;
    }
    const bool structure = source <= AptitudeSource::StructureCoordinate;
    if (structure && rng.Pick(0, 4) == 0)
    {
        routine = true;
        result = AptitudeResult::Sound;
    }
    if ((source == AptitudeSource::PracticeConsolidation || source == AptitudeSource::Attendance) &&
        result == AptitudeResult::Unsound)
        result = AptitudeResult::Partial;
    return Item("ev." + std::to_string(serial), source, facet, "subject." + std::to_string(rng.Pick(0, 5)), result,
                rng.Pick(0, 50'000), routine);
}
} // namespace AptitudeTests
