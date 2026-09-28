#pragma once

#include "Json.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The content registry and its one validator (SYSTEMS.md §5, ROADMAP M1-06).
 *
 * Content lives in Data/Content/<type>/<id>.json, one record per file. Each
 * record declares its "type" and "id". Records may contain nested records
 * with their own IDs (a city's districts and Gate sites); those IDs live in
 * the same global namespace, so every reference is checked against one table.
 *
 * Validation is shape (known fields only, the right kinds, values in range,
 * required fields present), identity (unique, correctly prefixed IDs),
 * references (every reference names an existing record of the right type)
 * and semantics (the cross-record rules each type declares). File reading is
 * the caller's job, so this runs unchanged in the game, the pwcontent tool
 * and the tests.
 */
struct ContentFile
{
    std::string Path;
    std::string Text;
};

struct ContentIssue
{
    std::string File;
    std::string Id;
    std::string Message;
};

struct ContentRecord
{
    std::string Type;
    std::string Id;
    std::string File;
    /** The record's own JSON object. For a nested record, its object inside the parent. */
    JsonValue Data;
    /** The top-level record that contains this one, or empty. */
    std::string ParentId;
};

class ContentDb
{
public:
    PERFECTWORLDCORE_API const ContentRecord* Find(std::string_view id) const;
    PERFECTWORLDCORE_API std::size_t Count() const;
    PERFECTWORLDCORE_API std::size_t CountOfType(std::string_view type) const;
    PERFECTWORLDCORE_API std::vector<const ContentRecord*> OfType(std::string_view type) const;

    std::map<std::string, ContentRecord, std::less<>> Records;
};

struct ContentLoadResult
{
    ContentDb Db;
    std::vector<ContentIssue> Issues;
    bool Ok() const { return Issues.empty(); }
};

/** The record types this build understands, for tools and messages. */
PERFECTWORLDCORE_API std::vector<std::string> KnownContentTypes();

PERFECTWORLDCORE_API ContentLoadResult LoadContent(const std::vector<ContentFile>& files);
} // namespace PerfectWorld
