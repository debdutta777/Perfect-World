#include "CreationSaveSerialization.h"

#include <cctype>
#include <cstdio>
#include <limits>
#include <map>
#include <string>

namespace PerfectWorld
{
namespace
{
void AppendEscaped(std::string& out, std::string_view value)
{
    out.push_back('"');
    for (const char raw : value)
    {
        const unsigned char character = static_cast<unsigned char>(raw);
        switch (raw)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (character < 0x20)
            {
                char escape[7];
                std::snprintf(escape, sizeof(escape), "\\u%04x", character);
                out += escape;
            }
            else
            {
                out.push_back(raw);
            }
            break;
        }
    }
    out.push_back('"');
}

void AppendStringField(std::string& out, std::string_view key, std::string_view value, const char* suffix)
{
    AppendEscaped(out, key);
    out += ": ";
    AppendEscaped(out, value);
    out += suffix;
}

/** Single-pass reader over the deliberate JSON subset this format uses. */
class Reader
{
public:
    explicit Reader(std::string_view text) : Text_(text) {}

    void SkipWhitespace()
    {
        while (Index_ < Text_.size() && std::isspace(static_cast<unsigned char>(Text_[Index_])) != 0)
        {
            ++Index_;
        }
    }

    bool Consume(char expected)
    {
        SkipWhitespace();
        if (Index_ < Text_.size() && Text_[Index_] == expected)
        {
            ++Index_;
            return true;
        }
        return false;
    }

    bool Peek(char expected)
    {
        SkipWhitespace();
        return Index_ < Text_.size() && Text_[Index_] == expected;
    }

    bool AtEnd()
    {
        SkipWhitespace();
        return Index_ >= Text_.size();
    }

    bool ReadString(std::string& out)
    {
        SkipWhitespace();
        if (Index_ >= Text_.size() || Text_[Index_] != '"')
        {
            return false;
        }
        ++Index_;
        std::string value;
        while (Index_ < Text_.size())
        {
            const char character = Text_[Index_++];
            if (character == '"')
            {
                out = std::move(value);
                return true;
            }
            if (character != '\\')
            {
                value.push_back(character);
                continue;
            }
            if (Index_ >= Text_.size())
            {
                return false;
            }
            const char escape = Text_[Index_++];
            switch (escape)
            {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u':
            {
                if (Index_ + 4 > Text_.size())
                {
                    return false;
                }
                std::int32_t code = 0;
                for (int digit = 0; digit < 4; ++digit)
                {
                    const char hex = Text_[Index_++];
                    const int hexValue = hex >= '0' && hex <= '9'   ? hex - '0'
                                         : hex >= 'a' && hex <= 'f' ? hex - 'a' + 10
                                         : hex >= 'A' && hex <= 'F' ? hex - 'A' + 10
                                                                    : -1;
                    if (hexValue < 0)
                    {
                        return false;
                    }
                    code = code * 16 + hexValue;
                }
                // The writer only escapes control characters this way; anything
                // above ASCII is passed through as UTF-8 bytes instead.
                if (code > 0x7f)
                {
                    return false;
                }
                value.push_back(static_cast<char>(code));
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool ReadInt(std::int32_t& out)
    {
        SkipWhitespace();
        const std::size_t start = Index_;
        if (Index_ < Text_.size() && Text_[Index_] == '-')
        {
            ++Index_;
        }
        const bool negative = Index_ > start;
        std::size_t digits = 0;
        long long value = 0;
        while (Index_ < Text_.size() && Text_[Index_] >= '0' && Text_[Index_] <= '9')
        {
            value = value * 10 + (Text_[Index_] - '0');
            ++Index_;
            ++digits;
            if (value > 4294967296LL)
            {
                // Stop well before overflow; the value is already far outside
                // every field this format accepts.
                return false;
            }
        }
        if (digits == 0)
        {
            return false;
        }
        const long long signedValue = negative ? -value : value;
        if (signedValue < std::numeric_limits<std::int32_t>::min() ||
            signedValue > std::numeric_limits<std::int32_t>::max())
        {
            return false;
        }
        out = static_cast<std::int32_t>(signedValue);
        return true;
    }

    bool ReadBool(bool& out)
    {
        SkipWhitespace();
        if (Text_.compare(Index_, 4, "true") == 0)
        {
            Index_ += 4;
            out = true;
            return true;
        }
        if (Text_.compare(Index_, 5, "false") == 0)
        {
            Index_ += 5;
            out = false;
            return true;
        }
        return false;
    }

private:
    std::string_view Text_;
    std::size_t Index_ = 0;
};

OperationResult Malformed(std::string detail)
{
    return OperationResult::Failure(CreationError::MalformedSaveText,
                                    "This save file could not be read: " + std::move(detail));
}

/** Reads a flat object of string values into `out`. */
bool ReadStringObject(Reader& reader, std::map<std::string, std::string>& out)
{
    if (!reader.Consume('{'))
    {
        return false;
    }
    if (reader.Consume('}'))
    {
        return true;
    }
    while (true)
    {
        std::string key;
        std::string value;
        if (!reader.ReadString(key) || !reader.Consume(':') || !reader.ReadString(value))
        {
            return false;
        }
        out.insert_or_assign(std::move(key), std::move(value));
        if (reader.Consume(','))
        {
            continue;
        }
        return reader.Consume('}');
    }
}

/** Reads a flat object of integer values into `out`. */
bool ReadIntObject(Reader& reader, std::map<std::string, std::int32_t>& out)
{
    if (!reader.Consume('{'))
    {
        return false;
    }
    if (reader.Consume('}'))
    {
        return true;
    }
    while (true)
    {
        std::string key;
        std::int32_t value = 0;
        if (!reader.ReadString(key) || !reader.Consume(':') || !reader.ReadInt(value))
        {
            return false;
        }
        out.insert_or_assign(std::move(key), value);
        if (reader.Consume(','))
        {
            continue;
        }
        return reader.Consume('}');
    }
}

std::string LookUp(const std::map<std::string, std::string>& values, const char* key)
{
    const auto found = values.find(key);
    return found == values.end() ? std::string{} : found->second;
}
} // namespace

bool IsValidSaveSlot(std::int32_t slotIndex) noexcept
{
    return slotIndex >= 0 && slotIndex < SaveSlotCount;
}

std::string SaveSlotFileName(std::int32_t slotIndex)
{
    if (!IsValidSaveSlot(slotIndex))
    {
        return {};
    }
    return "creation_slot_" + std::to_string(slotIndex + 1) + ".json";
}

std::string SerializeCreationSave(const CharacterCreationSave& save)
{
    std::string out;
    out.reserve(1024);
    out += "{\n  \"schemaVersion\": ";
    out += std::to_string(SaveSchemaVersion);
    out += ",\n  \"age\": ";
    out += std::to_string(save.Age);
    out += ",\n  \"hasStarted\": ";
    out += save.HasStarted ? "true" : "false";
    out += ",\n  ";
    AppendStringField(out, "householdTemplateId", save.HouseholdTemplateId, ",\n");

    out += "  \"identity\": {\n    ";
    AppendStringField(out, "givenName", save.Identity.GivenName, ",\n    ");
    AppendStringField(out, "familyName", save.Identity.FamilyName, ",\n    ");
    AppendStringField(out, "pronouns", save.Identity.Pronouns, ",\n    ");
    AppendStringField(out, "homeCity", save.Identity.HomeCity, ",\n    ");
    AppendStringField(out, "primaryLanguage", save.Identity.PrimaryLanguage, "\n  },\n");

    out += "  \"cosmetics\": {\n    ";
    AppendStringField(out, "bodyPreset", save.Cosmetics.BodyPreset, ",\n    ");
    AppendStringField(out, "skinTone", save.Cosmetics.SkinTone, ",\n    ");
    AppendStringField(out, "hairStyle", save.Cosmetics.HairStyle, ",\n    ");
    AppendStringField(out, "hairColor", save.Cosmetics.HairColor, ",\n    ");
    AppendStringField(out, "outfit", save.Cosmetics.Outfit, "\n  },\n");

    out += "  \"traitRanks\": {\n";
    const auto& definitions = CharacterCreator::TraitDefinitions();
    for (std::size_t index = 0; index < TraitCount; ++index)
    {
        out += "    ";
        AppendEscaped(out, definitions[index].Id);
        out += ": ";
        out += std::to_string(save.TraitRanks[index]);
        out += index + 1 < TraitCount ? ",\n" : "\n";
    }
    out += "  }\n}\n";
    return out;
}

OperationResult DeserializeCreationSave(std::string_view text, CharacterCreationSave& outSave)
{
    Reader reader(text);
    if (!reader.Consume('{'))
    {
        return Malformed("it does not begin with a record.");
    }

    CharacterCreationSave parsed;
    bool sawSchemaVersion = false;
    bool sawTraitRanks = false;
    std::map<std::string, std::string> identity;
    std::map<std::string, std::string> cosmetics;
    std::map<std::string, std::int32_t> traitRanks;

    if (!reader.Peek('}'))
    {
        while (true)
        {
            std::string key;
            if (!reader.ReadString(key) || !reader.Consume(':'))
            {
                return Malformed("a field name is missing or malformed.");
            }
            if (key == "schemaVersion")
            {
                if (!reader.ReadInt(parsed.SchemaVersion))
                {
                    return Malformed("the schema version is not a whole number.");
                }
                sawSchemaVersion = true;
            }
            else if (key == "age")
            {
                if (!reader.ReadInt(parsed.Age))
                {
                    return Malformed("the age is not a whole number.");
                }
            }
            else if (key == "hasStarted")
            {
                if (!reader.ReadBool(parsed.HasStarted))
                {
                    return Malformed("the started flag is not true or false.");
                }
            }
            else if (key == "householdTemplateId")
            {
                if (!reader.ReadString(parsed.HouseholdTemplateId))
                {
                    return Malformed("the household template is not text.");
                }
            }
            else if (key == "identity")
            {
                if (!ReadStringObject(reader, identity))
                {
                    return Malformed("the identity section is malformed.");
                }
            }
            else if (key == "cosmetics")
            {
                if (!ReadStringObject(reader, cosmetics))
                {
                    return Malformed("the appearance section is malformed.");
                }
            }
            else if (key == "traitRanks")
            {
                if (!ReadIntObject(reader, traitRanks))
                {
                    return Malformed("the trait section is malformed.");
                }
                sawTraitRanks = true;
            }
            else
            {
                return Malformed("it contains an unexpected field: " + key);
            }

            if (reader.Consume(','))
            {
                continue;
            }
            break;
        }
    }
    if (!reader.Consume('}') || !reader.AtEnd())
    {
        return Malformed("it has trailing content after the record.");
    }
    if (!sawSchemaVersion)
    {
        return Malformed("it does not state a schema version.");
    }
    if (!sawTraitRanks)
    {
        return Malformed("it does not contain trait ranks.");
    }
    if (traitRanks.size() != TraitCount)
    {
        return Malformed("it lists " + std::to_string(traitRanks.size()) + " traits instead of " +
                         std::to_string(TraitCount) + ".");
    }

    // Match by stable ID so a reordered or renamed trait fails loudly here
    // rather than shifting every rank by one position.
    const auto& definitions = CharacterCreator::TraitDefinitions();
    for (std::size_t index = 0; index < TraitCount; ++index)
    {
        const auto found = traitRanks.find(std::string(definitions[index].Id));
        if (found == traitRanks.end())
        {
            return Malformed("it is missing the trait " + std::string(definitions[index].Id) + ".");
        }
        parsed.TraitRanks[index] = found->second;
    }

    parsed.Identity.GivenName = LookUp(identity, "givenName");
    parsed.Identity.FamilyName = LookUp(identity, "familyName");
    parsed.Identity.Pronouns = LookUp(identity, "pronouns");
    parsed.Identity.HomeCity = LookUp(identity, "homeCity");
    parsed.Identity.PrimaryLanguage = LookUp(identity, "primaryLanguage");
    parsed.Cosmetics.BodyPreset = LookUp(cosmetics, "bodyPreset");
    parsed.Cosmetics.SkinTone = LookUp(cosmetics, "skinTone");
    parsed.Cosmetics.HairStyle = LookUp(cosmetics, "hairStyle");
    parsed.Cosmetics.HairColor = LookUp(cosmetics, "hairColor");
    parsed.Cosmetics.Outfit = LookUp(cosmetics, "outfit");

    outSave = std::move(parsed);
    return OperationResult::Success();
}
} // namespace PerfectWorld
