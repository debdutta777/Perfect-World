#pragma once

#include "ContentSchemas.h"

#include <string>
#include <utility>
#include <vector>

// Field-spec builders shared by the schema files (ContentSchemas.cpp and
// ContentActivitySchemas.cpp), so each type's schema reads as one table.
namespace PerfectWorld::ContentSchema::Build
{
inline FieldSpec Str(std::string name, bool required)
{
    FieldSpec f;
    f.Name = std::move(name);
    f.Kind = FieldKind::String;
    f.Required = required;
    return f;
}

inline FieldSpec Flag(std::string name, bool required)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Bool;
    return f;
}

inline FieldSpec Int(std::string name, bool required, std::int64_t min, std::int64_t max, bool nullable = false)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Int;
    f.Min = min;
    f.Max = max;
    f.Nullable = nullable;
    return f;
}

inline FieldSpec OneOf(std::string name, bool required, std::vector<std::string> values)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Enum;
    f.Values = std::move(values);
    return f;
}

inline FieldSpec Ref(std::string name, bool required, std::string type)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::Ref;
    f.RefType = std::move(type);
    return f;
}

inline FieldSpec Refs(std::string name, bool required, std::string type)
{
    FieldSpec f = Ref(std::move(name), required, std::move(type));
    f.Kind = FieldKind::RefArray;
    return f;
}

inline FieldSpec Strs(std::string name, bool required)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = FieldKind::StringArray;
    return f;
}

inline FieldSpec Ints(std::string name, bool required, std::int64_t min, std::int64_t max)
{
    FieldSpec f = Int(std::move(name), required, min, max);
    f.Kind = FieldKind::IntArray;
    return f;
}

inline FieldSpec Obj(std::string name, bool required, const TypeSchema* nested, bool many)
{
    FieldSpec f = Str(std::move(name), required);
    f.Kind = many ? FieldKind::ObjectArray : FieldKind::Object;
    f.Nested = nested;
    return f;
}

// Every top-level record has these three; "type" and "id" are checked by the loader.
inline std::vector<FieldSpec> Base(std::vector<FieldSpec> fields)
{
    fields.insert(fields.begin(), {Str("type", true), Str("id", true), Str("note", false)});
    return fields;
}
} // namespace PerfectWorld::ContentSchema::Build
