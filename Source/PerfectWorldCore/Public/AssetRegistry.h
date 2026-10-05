#pragma once

#include "ContentRegistry.h"
#include "Json.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
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
 * The asset registry (ASSET_PIPELINE.md §3, ROADMAP M1-07).
 *
 * Gameplay names every visual by asset ID. Each ID has one file,
 * Assets/<category>/<subcategory>/<name>.json, with schema "pw.asset/1": real
 * dimensions, a presentation contract, a block model and gameplay data
 * (collision, interactions, seats, sockets, camera clearance). Each asset
 * family has one contract, Contracts/<family>.json, with schema
 * "pw.contract/1", which says what its assets must declare and how big they
 * may be.
 *
 * Every length is an integer number of millimetres, in Unreal axes scaled to
 * millimetres (+X forward, +Y right, +Z up). Files are passed in memory, as
 * with LoadContent, so the game, the pwcontent tool and the tests share one
 * validator. Validation is shape (known fields only, integers, required fields),
 * identity (ID format, path agreement, uniqueness), contract (known, matching
 * category, required fields present, size limits) and geometry (everything the
 * asset places lies inside its bounds).
 */
struct AssetContract
{
    std::string File;
    /** "prop.bed" */
    std::string Family;
    int Version = 1;
    /** "prop.bed@1": what an asset's "contract" field names. */
    std::string Key;
    std::string Category;
    std::vector<std::string> Subcategories;
    /** Texel class from ASSET_PIPELINE.md §1.2: "A", "P" or "S". */
    std::string TexelClass;
    std::array<std::int64_t, 3> MinDimensionsMm{};
    std::array<std::int64_t, 3> MaxDimensionsMm{};
    /** When set, the asset's height (Z) must equal it exactly: doors are 2,100 mm. */
    std::optional<std::int64_t> ExactHeightMm;

    bool Solid = false;
    std::int64_t MinInteractions = 0;
    std::int64_t MinSeats = 0;
    bool RequiresStandingAnchor = false;
    bool RequiresCameraClearance = false;
    bool RequiresSeatedClearance = false;
    bool RequiresGripSocket = false;
    std::int64_t MinTargetRadiusMm = 0;
    /** Minimum free height and depth under the item (a desk's knee space). */
    std::optional<std::array<std::int64_t, 2>> MinKneeClearanceMm;
    /** Allowed Z range of a standing anchor above the floor. */
    std::optional<std::array<std::int64_t, 2>> AnchorHeightMm;

    /** The camera probe radius both modes must declare. */
    std::int64_t CameraProbeMm = 0;
    /** First person: the smallest allowed gap between the camera and any visible surface. */
    std::int64_t MinCameraGapMm = 0;
    /** Third person: the boom length the asset was cleared with. */
    std::int64_t BoomMm = 0;
    /** Third person: the smallest room the boom must fit in (X, Y). */
    std::array<std::int64_t, 2> MinRoomMm{};
    /** Headroom above a seated pelvis, required of every seat in both modes. */
    std::int64_t MinSeatedHeadroomMm = 0;

    std::vector<std::string> SeatPoses;
    std::vector<std::string> Grips;
};

struct AssetRecord
{
    std::string Id;
    std::string File;
    std::string ContractKey;
    std::string Category;
    std::string Subcategory;
    JsonValue Data;
};

class AssetDb
{
public:
    PERFECTWORLDCORE_API const AssetRecord* Find(std::string_view id) const;
    PERFECTWORLDCORE_API const AssetContract* FindContract(std::string_view key) const;
    PERFECTWORLDCORE_API std::size_t AssetCount() const;
    PERFECTWORLDCORE_API std::size_t ContractCount() const;
    PERFECTWORLDCORE_API std::size_t CountInCategory(std::string_view category) const;

    std::map<std::string, AssetRecord, std::less<>> Assets;
    std::map<std::string, AssetContract, std::less<>> Contracts;
};

struct AssetLoadResult
{
    AssetDb Db;
    std::vector<ContentIssue> Issues;
    bool Ok() const { return Issues.empty(); }
};

/** True when text is a well-formed asset ID: category.subcategory.name[.variant], ASSET_PIPELINE.md §3.1. */
PERFECTWORLDCORE_API bool IsValidAssetId(std::string_view id);

/**
 * Loads and validates contract files (schema "pw.contract/1") and asset files
 * (schema "pw.asset/1"). Paths use '/' and end in Contracts/<family>.json or
 * Assets/<category>/<subcategory>/<name>.json.
 */
PERFECTWORLDCORE_API AssetLoadResult LoadAssets(const std::vector<ContentFile>& files);
} // namespace PerfectWorld
