#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * A small, strict JSON document model for content files (SYSTEMS.md §5).
 *
 * Strict means: standard JSON only (no comments, no trailing commas), and a
 * duplicate key inside one object is an error, because in content data it
 * always means two authors disagreed. It uses no exceptions, so the same code
 * runs inside Unreal and in the standalone tools.
 */
struct JsonValue
{
    enum class Kind
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    Kind Type = Kind::Null;
    bool Bool = false;
    double Number = 0.0;
    /** Set when the number was written without a fraction or exponent and fits in 64 bits. */
    std::optional<std::int64_t> Integer;
    std::string String;
    std::vector<JsonValue> Array;
    /** Members in the order written. */
    std::vector<std::pair<std::string, JsonValue>> Object;

    bool IsNull() const { return Type == Kind::Null; }
    bool IsString() const { return Type == Kind::String; }
    bool IsObject() const { return Type == Kind::Object; }
    bool IsArray() const { return Type == Kind::Array; }
    PERFECTWORLDCORE_API const JsonValue* Find(std::string_view key) const;
};

struct JsonParseResult
{
    std::optional<JsonValue> Value;
    std::string Error;
    std::size_t Line = 0;
};

/** Parses one complete JSON document. Nesting deeper than 64 levels is refused. */
PERFECTWORLDCORE_API JsonParseResult ParseJson(std::string_view text);
} // namespace PerfectWorld
