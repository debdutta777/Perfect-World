#include "CreationSaveSerialization.h"
#include "Appearance.h"
#include "CreationText.h"

#include <map>
#include <string>

namespace PerfectWorld
{
namespace
{
void AppendColorField(std::string& out, std::string_view key, const ColorValue& color, const char* suffix)
{
    CreationTextAppendEscaped(out, key);
    out += ": { \"r\": " + std::to_string(color.R);
    out += ", \"g\": " + std::to_string(color.G);
    out += ", \"b\": " + std::to_string(color.B);
    out += " }";
    out += suffix;
}

OperationResult Malformed(std::string detail)
{
    return OperationResult::Failure(CreationError::MalformedSaveText,
                                    "This save file could not be read: " + std::move(detail));
}

/** Reads a flat object of string values into `out`. */
bool ReadStringObject(CreationTextReader& reader, std::map<std::string, std::string>& out)
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
bool ReadIntObject(CreationTextReader& reader, std::map<std::string, std::int32_t>& out)
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

/**
 * Reads the appearance object. Its members are a mix of strings, one integer
 * and three nested colour objects, so it gets its own reader rather than being
 * forced into the flat string/int helpers.
 */
bool ReadAppearanceObject(CreationTextReader& reader, AppearanceRecord& out)
{
    const auto ReadColor = [&reader](ColorValue& color)
    {
        if (!reader.Consume('{')) return false;
        bool sawR = false, sawG = false, sawB = false;
        while (true)
        {
            std::string channel;
            std::int32_t value = 0;
            if (!reader.ReadString(channel) || !reader.Consume(':') || !reader.ReadInt(value)) return false;
            if (channel == "r") { color.R = value; sawR = true; }
            else if (channel == "g") { color.G = value; sawG = true; }
            else if (channel == "b") { color.B = value; sawB = true; }
            else return false;
            if (reader.Consume(',')) continue;
            break;
        }
        return reader.Consume('}') && sawR && sawG && sawB;
    };

    if (!reader.Consume('{')) return false;
    if (reader.Consume('}')) return true;
    while (true)
    {
        std::string key;
        if (!reader.ReadString(key) || !reader.Consume(':')) return false;
        if (key == "schemaVersion")
        {
            if (!reader.ReadInt(out.SchemaVersion)) return false;
        }
        else if (key == "faceShapeId") { if (!reader.ReadString(out.FaceShapeId)) return false; }
        else if (key == "hairStyleId") { if (!reader.ReadString(out.HairStyleId)) return false; }
        else if (key == "buildId") { if (!reader.ReadString(out.BuildId)) return false; }
        else if (key == "uniformVariantId") { if (!reader.ReadString(out.UniformVariantId)) return false; }
        else if (key == "skinTone") { if (!ReadColor(out.SkinTone)) return false; }
        else if (key == "hairColor") { if (!ReadColor(out.HairColor)) return false; }
        else if (key == "eyeColor") { if (!ReadColor(out.EyeColor)) return false; }
        else return false;

        if (reader.Consume(',')) continue;
        break;
    }
    return reader.Consume('}');
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
    CreationTextAppendStringField(out, "resumePoint", ResumePointId(save.Resume), ",\n  ");
    CreationTextAppendStringField(out, "householdTemplateId", save.HouseholdTemplateId, ",\n");

    // The identity, school and migration sections live in CreationIdentity.cpp.
    CreationTextAppendProfile(out, save);

    out += "  \"appearance\": {\n    \"schemaVersion\": ";
    out += std::to_string(save.Appearance.SchemaVersion);
    out += ",\n    ";
    CreationTextAppendStringField(out, "faceShapeId", save.Appearance.FaceShapeId, ",\n    ");
    CreationTextAppendStringField(out, "hairStyleId", save.Appearance.HairStyleId, ",\n    ");
    CreationTextAppendStringField(out, "buildId", save.Appearance.BuildId, ",\n    ");
    CreationTextAppendStringField(out, "uniformVariantId", save.Appearance.UniformVariantId, ",\n    ");
    AppendColorField(out, "skinTone", save.Appearance.SkinTone, ",\n    ");
    AppendColorField(out, "hairColor", save.Appearance.HairColor, ",\n    ");
    AppendColorField(out, "eyeColor", save.Appearance.EyeColor, "\n  },\n");

    out += "  \"traitRanks\": {\n";
    const auto& definitions = CharacterCreator::TraitDefinitions();
    for (std::size_t index = 0; index < TraitCount; ++index)
    {
        out += "    ";
        CreationTextAppendEscaped(out, definitions[index].Id);
        out += ": ";
        out += std::to_string(save.TraitRanks[index]);
        out += index + 1 < TraitCount ? ",\n" : "\n";
    }
    out += "  }\n}\n";
    return out;
}

OperationResult DeserializeCreationSave(std::string_view text, CharacterCreationSave& outSave)
{
    CreationTextReader reader(text);
    if (!reader.Consume('{'))
    {
        return Malformed("it does not begin with a record.");
    }

    CharacterCreationSave parsed;
    CreationTextProfile seenProfile;
    bool sawSchemaVersion = false;
    bool sawTraitRanks = false;
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
            else if (key == "resumePoint")
            {
                std::string resumeId;
                if (!reader.ReadString(resumeId))
                {
                    return Malformed("the resume point is not text.");
                }
                if (!ParseResumePoint(resumeId, parsed.Resume))
                {
                    return Malformed("it names an unknown resume point: " + resumeId);
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
                if (!CreationTextReadIdentity(reader, parsed, seenProfile))
                {
                    return Malformed("the identity section is malformed.");
                }
            }
            else if (key == "school")
            {
                if (!CreationTextReadSchool(reader, parsed, seenProfile))
                {
                    return Malformed("the school section is malformed.");
                }
            }
            else if (key == "identityMigrated")
            {
                if (!reader.ReadBool(parsed.IdentityMigrated))
                {
                    return Malformed("the migrated flag is not true or false.");
                }
            }
            else if (key == "appearance")
            {
                if (!ReadAppearanceObject(reader, parsed.Appearance))
                {
                    return Malformed("the appearance section is malformed.");
                }
            }
            else if (key == "cosmetics")
            {
                // Version 1 and 2 wrote free-form cosmetic strings. Accept and
                // discard them: they have no defined mapping to the option IDs,
                // so the record migrates to the default appearance instead of a
                // guess. Rejecting the file outright would lose the whole save.
                if (!ReadStringObject(reader, cosmetics))
                {
                    return Malformed("the legacy cosmetics section is malformed.");
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
    // A file from a newer build is reported as unsupported by RestoreSave, not as damaged here.
    const bool hasProfile = parsed.SchemaVersion >= 5 && parsed.SchemaVersion <= SaveSchemaVersion;
    if (hasProfile && !seenProfile.SawDateOfBirth)
    {
        return Malformed("it does not state a date of birth.");
    }
    if (hasProfile && !seenProfile.SawSchoolStream)
    {
        return Malformed("it does not state a school stream.");
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

    // `cosmetics` is the discarded version 1 and 2 section; see the reader.
    (void)cosmetics;

    outSave = std::move(parsed);
    return OperationResult::Success();
}
} // namespace PerfectWorld
