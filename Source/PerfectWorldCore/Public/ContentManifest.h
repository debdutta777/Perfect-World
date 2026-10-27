#pragma once

#include "ContentRegistry.h"
#include "Json.h"

#include <cstddef>
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
 * Deterministic content inventory manifest and diff helpers (Order S01).
 *
 * The manifest lists every content file path (normalized as /<type>/<id>.json)
 * and the sorted (id, kind) records defined in that file, including nested
 * records such as city districts and Gate sites. It contains no timestamps or
 * machine-specific paths so it can be committed at Data/Content/manifest.json
 * and regenerated deterministically with `pwcontent manifest Data/Content`.
 */
inline constexpr std::string_view ContentManifestSchema = "pw.content_manifest/1";
inline constexpr std::string_view ContentManifestFilename = "manifest.json";

struct ContentManifestRecord
{
    std::string Id;
    std::string Kind;

    bool operator==(const ContentManifestRecord&) const = default;
};

struct ContentManifestFile
{
    std::string Path;
    std::vector<ContentManifestRecord> Records;

    bool operator==(const ContentManifestFile&) const = default;
};

struct ContentManifest
{
    std::vector<ContentManifestFile> Files;

    PERFECTWORLDCORE_API std::size_t FileCount() const;
    PERFECTWORLDCORE_API std::size_t RecordCount() const;
    PERFECTWORLDCORE_API std::size_t CountOfKind(std::string_view kind) const;
    PERFECTWORLDCORE_API const ContentManifestFile* FindFile(std::string_view path) const;

    bool operator==(const ContentManifest&) const = default;
};

struct ContentManifestEntryRef
{
    std::string Id;
    std::string Kind;
    std::string File;

    bool operator==(const ContentManifestEntryRef&) const = default;
};

struct ContentManifestRename
{
    std::string Id;
    std::string FromKind;
    std::string ToKind;
    std::string FromFile;
    std::string ToFile;

    bool operator==(const ContentManifestRename&) const = default;
};

struct ContentManifestDuplicate
{
    std::string Id;
    std::string Kind;
    std::string FirstFile;
    std::string SecondFile;

    bool operator==(const ContentManifestDuplicate&) const = default;
};

struct ContentManifestDiff
{
    std::vector<ContentManifestEntryRef> Added;
    std::vector<ContentManifestEntryRef> Removed;
    std::vector<ContentManifestRename> Renamed;
    std::vector<ContentManifestDuplicate> Duplicates;

    bool Empty() const
    {
        return Added.empty() && Removed.empty() && Renamed.empty() && Duplicates.empty();
    }
    bool Ok() const { return Empty(); }

    bool operator==(const ContentManifestDiff&) const = default;
};

struct ContentManifestParseResult
{
    std::optional<ContentManifest> Manifest;
    std::string Error;

    bool Ok() const { return Manifest.has_value() && Error.empty(); }
};

/** True when `path` names the manifest file itself (`manifest.json` or `.../manifest.json`). */
PERFECTWORLDCORE_API bool IsContentManifestPath(std::string_view path);

/** Normalizes a content file path to a leading-slash forward-slash path relative to Data/Content. */
PERFECTWORLDCORE_API std::string NormalizeContentManifestPath(std::string_view path);

/** Builds a deterministically sorted manifest from a loaded ContentDb. */
PERFECTWORLDCORE_API ContentManifest BuildContentManifest(const ContentDb& db);

/** Builds a deterministically sorted manifest from a ContentLoadResult, preserving any duplicate-ID diagnostics. */
PERFECTWORLDCORE_API ContentManifest BuildContentManifest(const ContentLoadResult& loaded);

/** Builds a deterministically sorted manifest directly from raw ContentFiles, preserving duplicate IDs across or within files. */
PERFECTWORLDCORE_API ContentManifest BuildContentManifest(const std::vector<ContentFile>& files);

/** Returns all duplicate record IDs (or duplicate file entries) present within a single manifest. */
PERFECTWORLDCORE_API std::vector<ContentManifestDuplicate> FindContentManifestDuplicates(const ContentManifest& manifest);

/** Compares an expected (committed) manifest against an actual (directory) manifest. */
PERFECTWORLDCORE_API ContentManifestDiff DiffContentManifests(const ContentManifest& expected, const ContentManifest& actual);

/** Formats a human-readable, deterministically ordered report of all additions, removals, renames and duplicates. */
PERFECTWORLDCORE_API std::string FormatContentManifestDiff(const ContentManifestDiff& diff);

/** Serializes a manifest to canonical, deterministic JSON with LF newlines and no timestamps. */
PERFECTWORLDCORE_API std::string FormatContentManifestJson(const ContentManifest& manifest);

/** Parses and validates a manifest JSON document. */
PERFECTWORLDCORE_API ContentManifestParseResult ParseContentManifestJson(std::string_view text);
} // namespace PerfectWorld
