#pragma once

#include "AssetRegistry.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Shared by AssetRegistry.cpp (files, identity, contracts) and AssetGeometry.cpp
// (block, collision, interactions, seats, clearance). Private to the core.
namespace PerfectWorld::AssetDetail
{
/** No asset coordinate may exceed 200 m, so arithmetic on them cannot overflow. */
constexpr std::int64_t AssetCoordLimitMm = 200000;

struct AssetVec
{
    std::int64_t X = 0;
    std::int64_t Y = 0;
    std::int64_t Z = 0;
};

struct AssetBox
{
    AssetVec Min;
    AssetVec Max;
};

bool AssetContains(const AssetBox& box, const AssetVec& point);
bool AssetBoxInside(const AssetBox& inner, const AssetBox& outer);

/** Collects issues against one file and one record ID. */
class AssetSink
{
public:
    AssetSink(std::vector<ContentIssue>& issues, std::string file, std::string id)
        : Issues(issues), File(std::move(file)), Id(std::move(id))
    {
    }
    void Add(std::string message) { Issues.push_back({File, Id, std::move(message)}); }

    std::vector<ContentIssue>& Issues;
    std::string File;
    std::string Id;
};

/**
 * Reads the members of one JSON object, remembering which were asked for so
 * Finish() can refuse the rest as unknown fields. Every message is prefixed
 * with the object's place in the record ("gameplay.seats[0]: ").
 */
class AssetReader
{
public:
    AssetReader(const JsonValue& object, std::string context, AssetSink& sink)
        : Object(&object), Context(std::move(context)), Sink(&sink)
    {
    }

    /** The member, or null. A missing required member is reported. Marks the name as known. */
    const JsonValue* Member(std::string_view name, bool required);
    std::optional<std::int64_t> Int(std::string_view name, bool required, std::int64_t lo, std::int64_t hi);
    std::optional<std::string> String(std::string_view name, bool required);
    std::optional<bool> Bool(std::string_view name, bool required);
    std::optional<AssetVec> Vec3(std::string_view name, bool required);
    std::optional<std::array<std::int64_t, 2>> Pair(std::string_view name, bool required, std::int64_t lo, std::int64_t hi);
    /** An array member, or null; reported when present but not an array. */
    const JsonValue* Array(std::string_view name, bool required);
    /** A nested object member as its own reader; reported when present but not an object. */
    std::optional<AssetReader> Child(std::string_view name, bool required);
    /** Reports every member that was never asked for. */
    void Finish();

    void Report(const std::string& message) const { Sink->Add(Context + message); }
    AssetSink& SinkRef() const { return *Sink; }
    const std::string& Where() const { return Context; }

private:
    const JsonValue* Object;
    std::string Context;
    AssetSink* Sink;
    std::vector<std::string> Known;
};

/** Reads an array item that must be three whole numbers. */
std::optional<AssetVec> AssetReadVec3(const JsonValue& value, const std::string& where, AssetSink& sink);
/** Reads {"min": [..], "max": [..]} (keys given by the caller) and requires min < max on every axis. */
std::optional<AssetBox> AssetReadBox(AssetReader& parent, std::string_view name, bool required);

bool AssetIsKnownRamp(std::string_view name);
bool AssetIsReservedRamp(std::string_view name);
bool AssetIsKnownPose(std::string_view name);
bool AssetIsKnownGrip(std::string_view name);
bool AssetIsLowerId(std::string_view text);
/** Pelvis height range of a sitting or lying pose in mm; empty for poses that are not seat poses. */
std::optional<std::array<std::int64_t, 2>> AssetSeatPelvisRange(std::string_view pose);
bool AssetContainsText(const std::vector<std::string>& list, std::string_view text);
/** Splits on one character, keeping empty parts ("a..b" gives three). */
std::vector<std::string> AssetSplit(std::string_view text, char separator);
/** One ID segment: 1-32 characters, lowercase letters, digits and '_'; the first segment starts with a letter. */
bool AssetSegmentOk(const std::string& segment, bool first);
/** Reads and validates one pw.contract/1 object; empty when it is too broken to name (issues are reported either way). */
std::optional<AssetContract> AssetParseContract(const JsonValue& json, const std::string& file, std::vector<ContentIssue>& issues);

/** What the block and gameplay checks need to know about the record they are inside. */
struct AssetBodyContext
{
    const AssetContract* Contract = nullptr;
    std::string Id;
    std::string Subcategory;
    /** The nominal box from dimensions_mm and the pivot, when those were valid. */
    bool HaveDims = false;
    AssetBox Dims;
    /** The outer envelope, when valid. Everything an asset places must lie inside it. */
    bool HaveOuter = false;
    AssetBox Outer;
};

/**
 * Checks everything in a pw.asset/1 record after its identity fields:
 * dimensions, pivot, front, envelope, block, gameplay. Marks those members as
 * known on root. Never throws and never stops at the first problem.
 */
void AssetCheckBody(AssetReader& root, const AssetContract& contract, const std::string& assetId, const std::string& subcategory);
/** The block model: cuboids, tiles, tints; they must span exactly the declared dimensions. */
void AssetCheckBlock(AssetReader& root, const AssetBodyContext& context);
/** Collision, navigation, interactions, seats, sockets, held-item data and camera clearance. */
void AssetCheckGameplay(AssetReader& root, const AssetBodyContext& context);
} // namespace PerfectWorld::AssetDetail
