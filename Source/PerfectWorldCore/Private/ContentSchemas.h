#pragma once

#include "ContentRegistry.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld::ContentSchema
{
enum class FieldKind
{
    String,
    Bool,
    Int,
    Enum,
    Ref,
    RefArray,
    StringArray,
    Object,
    ObjectArray
};

struct TypeSchema;

struct FieldSpec
{
    std::string Name;
    FieldKind Kind = FieldKind::String;
    bool Required = false;
    std::int64_t Min = 0;
    std::int64_t Max = 0;
    bool Nullable = false;
    std::vector<std::string> Values;
    std::string RefType;
    const TypeSchema* Nested = nullptr;
};

/**
 * A record type, or a nested record type. An empty IdPrefix means the nested
 * object has no identity of its own (a plain sub-object).
 */
struct TypeSchema
{
    std::string Type;
    std::string IdPrefix;
    std::vector<FieldSpec> Fields;
};

const std::vector<const TypeSchema*>& TopLevelSchemas();
const TypeSchema* FindTopLevel(std::string_view type);

/** Cross-record rules, run after every reference has resolved. */
void CheckSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
} // namespace PerfectWorld::ContentSchema
