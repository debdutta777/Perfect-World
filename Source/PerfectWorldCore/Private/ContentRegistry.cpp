#include "ContentRegistry.h"

#include "ContentSchemas.h"

#include <algorithm>

namespace PerfectWorld
{
using namespace ContentSchema;

const ContentRecord* ContentDb::Find(std::string_view id) const
{
    const auto it = Records.find(id);
    return it == Records.end() ? nullptr : &it->second;
}

std::size_t ContentDb::Count() const
{
    return Records.size();
}

std::size_t ContentDb::CountOfType(std::string_view type) const
{
    return static_cast<std::size_t>(
        std::count_if(Records.begin(), Records.end(), [&](const auto& entry) { return entry.second.Type == type; }));
}

std::vector<const ContentRecord*> ContentDb::OfType(std::string_view type) const
{
    std::vector<const ContentRecord*> out;
    for (const auto& [id, record] : Records)
    {
        if (record.Type == type)
        {
            out.push_back(&record);
        }
    }
    return out;
}

std::vector<std::string> KnownContentTypes()
{
    std::vector<std::string> out;
    for (const TypeSchema* schema : TopLevelSchemas())
    {
        out.push_back(schema->Type);
    }
    return out;
}

namespace
{
struct PendingRef
{
    std::string File;
    std::string OwnerId;
    std::string Field;
    std::string Target;
    std::string ExpectedType;
};

class Loader
{
public:
    ContentLoadResult Result;
    std::vector<PendingRef> Refs;

    void Issue(const std::string& file, const std::string& id, std::string message)
    {
        Result.Issues.push_back({file, id, std::move(message)});
    }

    /** Checks one object against its schema, registers it if it has an ID, and recurses into nested records. */
    void CheckObject(const TypeSchema& schema, const JsonValue& object, const std::string& file, const std::string& ownerId,
                     const std::string& parentId)
    {
        std::string id = ownerId;
        if (!schema.IdPrefix.empty())
        {
            const JsonValue* idValue = object.Find("id");
            if (idValue == nullptr || !idValue->IsString() || idValue->String.empty())
            {
                Issue(file, ownerId, "a " + schema.Type + " record needs a string \"id\"");
                return;
            }
            id = idValue->String;
            if (id.rfind(schema.IdPrefix, 0) != 0)
            {
                Issue(file, id, "a " + schema.Type + " ID must start with \"" + schema.IdPrefix + "\"");
            }
        }

        for (const auto& [name, value] : object.Object)
        {
            const bool known = std::any_of(schema.Fields.begin(), schema.Fields.end(), [&](const FieldSpec& f) { return f.Name == name; });
            if (!known)
            {
                Issue(file, id, "unknown field \"" + name + "\" in " + schema.Type);
            }
        }
        for (const FieldSpec& field : schema.Fields)
        {
            const JsonValue* value = object.Find(field.Name);
            if (value == nullptr)
            {
                if (field.Required)
                {
                    Issue(file, id, "missing required field \"" + field.Name + "\"");
                }
                continue;
            }
            CheckField(field, *value, file, id, schema.IdPrefix.empty() ? parentId : (parentId.empty() ? id : parentId));
        }

        if (!schema.IdPrefix.empty())
        {
            ContentRecord record{schema.Type, id, file, object, parentId};
            if (!Result.Db.Records.emplace(id, std::move(record)).second)
            {
                Issue(file, id, "duplicate ID (first defined in " + Result.Db.Find(id)->File + ")");
            }
        }
    }

    void CheckField(const FieldSpec& field, const JsonValue& value, const std::string& file, const std::string& id,
                    const std::string& topId)
    {
        const std::string where = "field \"" + field.Name + "\"";
        switch (field.Kind)
        {
        case FieldKind::String:
            if (!value.IsString())
                Issue(file, id, where + " must be a string");
            break;
        case FieldKind::Bool:
            if (value.Type != JsonValue::Kind::Bool)
                Issue(file, id, where + " must be true or false");
            break;
        case FieldKind::Int:
            if (value.IsNull() && field.Nullable)
                break;
            if (!value.Integer)
                Issue(file, id, where + " must be a whole number");
            else if (*value.Integer < field.Min || *value.Integer > field.Max)
                Issue(file, id, where + " is " + std::to_string(*value.Integer) + ", outside " + std::to_string(field.Min) +
                                    ".." + std::to_string(field.Max));
            break;
        case FieldKind::Enum:
            if (!value.IsString() || std::find(field.Values.begin(), field.Values.end(), value.String) == field.Values.end())
                Issue(file, id, where + " has an unknown value");
            break;
        case FieldKind::Ref:
            if (!value.IsString())
                Issue(file, id, where + " must be an ID string");
            else
                Refs.push_back({file, id, field.Name, value.String, field.RefType});
            break;
        case FieldKind::RefArray:
        case FieldKind::StringArray:
            if (!value.IsArray())
            {
                Issue(file, id, where + " must be an array");
                break;
            }
            for (const JsonValue& item : value.Array)
            {
                if (!item.IsString())
                    Issue(file, id, where + " must contain only strings");
                else if (field.Kind == FieldKind::RefArray)
                    Refs.push_back({file, id, field.Name, item.String, field.RefType});
            }
            break;
        case FieldKind::IntArray:
            if (!value.IsArray())
            {
                Issue(file, id, where + " must be an array");
                break;
            }
            for (const JsonValue& item : value.Array)
            {
                if (!item.Integer)
                    Issue(file, id, where + " must contain only whole numbers");
                else if (*item.Integer < field.Min || *item.Integer > field.Max)
                    Issue(file, id, where + " has " + std::to_string(*item.Integer) + ", outside " + std::to_string(field.Min) +
                                        ".." + std::to_string(field.Max));
            }
            break;
        case FieldKind::Object:
            if (!value.IsObject())
                Issue(file, id, where + " must be an object");
            else
                CheckObject(*field.Nested, value, file, id, topId);
            break;
        case FieldKind::ObjectArray:
            if (!value.IsArray())
            {
                Issue(file, id, where + " must be an array");
                break;
            }
            for (const JsonValue& item : value.Array)
            {
                if (!item.IsObject())
                    Issue(file, id, where + " must contain only objects");
                else
                    CheckObject(*field.Nested, item, file, id, topId);
            }
            break;
        }
    }

    void LoadFile(const ContentFile& file)
    {
        std::string normalizedPath = file.Path;
        std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
        if (normalizedPath == "manifest.json" ||
            (normalizedPath.size() >= 14 && normalizedPath.compare(normalizedPath.size() - 14, 14, "/manifest.json") == 0))
        {
            return; // Data/Content/manifest.json is the inventory manifest, not a content record file
        }
        std::string_view text = file.Text;
        if (text.substr(0, 3) == "\xEF\xBB\xBF")
        {
            text.remove_prefix(3); // tolerate a UTF-8 byte-order mark
        }
        const JsonParseResult parsed = ParseJson(text);
        if (!parsed.Value)
        {
            Issue(file.Path, "", "not valid JSON at line " + std::to_string(parsed.Line) + ": " + parsed.Error);
            return;
        }
        const JsonValue& root = *parsed.Value;
        const JsonValue* type = root.IsObject() ? root.Find("type") : nullptr;
        if (type == nullptr || !type->IsString())
        {
            Issue(file.Path, "", "a content file is one object with a string \"type\"");
            return;
        }
        const TypeSchema* schema = FindTopLevel(type->String);
        if (schema == nullptr)
        {
            Issue(file.Path, "", "unknown content type \"" + type->String + "\"");
            return;
        }
        const JsonValue* id = root.Find("id");
        if (id != nullptr && id->IsString())
        {
            // Data/Content/<type>/<id>.json: the path must agree with the record.
            std::string path = file.Path;
            std::replace(path.begin(), path.end(), '\\', '/');
            const std::string expected = "/" + schema->Type + "/" + id->String + ".json";
            if (path.size() < expected.size() || path.compare(path.size() - expected.size(), expected.size(), expected) != 0)
            {
                Issue(file.Path, id->String, "the file must be <type>/<id>.json, i.e. ending in " + expected);
            }
        }
        CheckObject(*schema, root, file.Path, "", "");
        CheckFairness(root, file.Path, Result.Issues);
    }

    void ResolveReferences()
    {
        for (const PendingRef& ref : Refs)
        {
            const ContentRecord* target = Result.Db.Find(ref.Target);
            if (target == nullptr)
                Issue(ref.File, ref.OwnerId, "field \"" + ref.Field + "\" names " + ref.Target + ", which does not exist");
            else if (target->Type != ref.ExpectedType)
                Issue(ref.File, ref.OwnerId, "field \"" + ref.Field + "\" names " + ref.Target + ", a " + target->Type +
                                                 ", but needs a " + ref.ExpectedType);
        }
    }
};
} // namespace

ContentLoadResult LoadContent(const std::vector<ContentFile>& files)
{
    Loader loader;
    for (const ContentFile& file : files)
    {
        loader.LoadFile(file);
    }
    loader.ResolveReferences();
    if (loader.Result.Issues.empty())
    {
        CheckSemantics(loader.Result.Db, loader.Result.Issues);
    }
    return std::move(loader.Result);
}
} // namespace PerfectWorld
