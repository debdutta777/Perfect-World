#pragma once

#include "ContentRegistry.h"
#include "DistrictGenerator.h"
#include "RealmGenerator.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Typed city records (WORLD_SCALE.md §2, ROADMAP M1-29).
 *
 * The content validator has already checked a loaded ContentDb, so this only
 * turns the JSON of a "city" record into plain structs. It still refuses a
 * record whose shape it cannot read (nullopt) rather than guessing, and it
 * never throws. A Gate site's rank uses the same F-to-S scale as the realm's
 * hazard rank.
 */
enum class DistrictKind : std::uint8_t
{
    Authored = 0,
    Generated
};

enum class GateSiteStatus : std::uint8_t
{
    Contained = 0,
    Sealed,
    Open
};

struct DistrictRecord
{
    std::string Id;
    std::string Name;
    DistrictKind Kind = DistrictKind::Authored;
    bool Fictional = false;
    /** Set for a generated district only. */
    std::optional<DistrictArchetype> Archetype;
    std::uint32_t Seed = 0;
};

struct GateSiteRecord
{
    std::string Id;
    std::string Name;
    std::string DistrictId;
    /** An authored location ID inside the district, or empty. */
    std::string Location;
    HazardRank Rank = HazardRank::F;
    GateSiteStatus Status = GateSiteStatus::Contained;
    /** The city's fixed, surveyed entry point into the Deep Realm. */
    bool RealmAnchor = false;
};

struct CityRecord
{
    std::string Id;
    std::string Name;
    std::string NationId;
    std::string State;
    std::string Role;
    std::vector<DistrictRecord> Districts;
    std::vector<GateSiteRecord> GateSites;

    PERFECTWORLDCORE_API const DistrictRecord* FindDistrict(std::string_view id) const;
    PERFECTWORLDCORE_API std::vector<const GateSiteRecord*> GateSitesIn(std::string_view districtId) const;
};

PERFECTWORLDCORE_API std::string_view GateSiteStatusName(GateSiteStatus status);

/** Nullopt when the ID is not a city record or the record cannot be read. */
PERFECTWORLDCORE_API std::optional<CityRecord> ReadCity(const ContentDb& db, std::string_view id);
/** Every readable city, in ID order. */
PERFECTWORLDCORE_API std::vector<CityRecord> ReadAllCities(const ContentDb& db);

/** The district's grid. Nullopt for an authored district, which is a layout rather than a recipe. */
PERFECTWORLDCORE_API std::optional<GeneratedDistrict> GenerateCityDistrict(const DistrictRecord& district);
} // namespace PerfectWorld
