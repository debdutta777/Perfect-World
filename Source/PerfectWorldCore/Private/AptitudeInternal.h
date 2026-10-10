#pragma once

// Private helpers shared by AptitudeProjection.cpp, AptitudeProjectionProject.cpp and
// AptitudeProjectionText.cpp (ROADMAP M3-05). Not part of the public API. The points are
// the private tally of the module: they never appear in a public type.

#include "AptitudeProjection.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace PerfectWorld::AptitudeInternal
{
/** A well-formed ID: [a-z0-9_.]+, at most AptitudeMaxIdLength. */
bool IdWellFormed(std::string_view id);

/** The points one item is worth at rank zero of its group (0 for Unsound). */
std::int32_t ItemPoints(const AptitudeEvidence& item);

/** The total order within a group: more points first, then earlier, then smaller ID, then every other field. */
bool ItemBefore(const AptitudeEvidence& a, const AptitudeEvidence& b);

/** True when both items are in one saturation group (same source, facet and subject). */
bool SameGroup(const AptitudeEvidence& a, const AptitudeEvidence& b);

std::optional<AptitudeSource> SourceFromId(std::string_view id);
std::optional<AptitudeFacet> FacetFromId(std::string_view id);
std::optional<AptitudeResult> ResultFromId(std::string_view id);
} // namespace PerfectWorld::AptitudeInternal
