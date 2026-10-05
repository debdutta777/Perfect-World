#include "CreationText.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

// The text form of the schema v5 sections: identity (with additionalLanguages
// and dateOfBirth), school, and the migrated flag. Validation lives in
// CreationIdentity.cpp; this file only turns text into fields and back.
namespace PerfectWorld
{
namespace
{
/** Parses exactly YYYY-MM-DD. Whether the date exists and is in range is the validator's business. */
bool IdentityTextParseIsoDate(std::string_view text, DateOfBirth& out) noexcept
{
    if (text.size() != 10 || text[4] != '-' || text[7] != '-')
    {
        return false;
    }
    std::int32_t parts[3] = {0, 0, 0};
    const std::size_t starts[3] = {0, 5, 8};
    const std::size_t lengths[3] = {4, 2, 2};
    for (int part = 0; part < 3; ++part)
    {
        for (std::size_t offset = 0; offset < lengths[part]; ++offset)
        {
            const char digit = text[starts[part] + offset];
            if (digit < '0' || digit > '9')
            {
                return false;
            }
            parts[part] = parts[part] * 10 + (digit - '0');
        }
    }
    out.Year = parts[0];
    out.Month = parts[1];
    out.Day = parts[2];
    return true;
}

bool IdentityTextReadStringArray(CreationTextReader& reader, std::vector<std::string>& out)
{
    if (!reader.Consume('['))
    {
        return false;
    }
    std::vector<std::string> values;
    if (!reader.Consume(']'))
    {
        while (true)
        {
            std::string value;
            if (!reader.ReadString(value))
            {
                return false;
            }
            values.push_back(std::move(value));
            if (reader.Consume(','))
            {
                continue;
            }
            if (!reader.Consume(']'))
            {
                return false;
            }
            break;
        }
    }
    out = std::move(values);
    return true;
}
} // namespace

void CreationTextAppendProfile(std::string& out, const CharacterCreationSave& save)
{
    const IdentityProfile& identity = save.Identity;
    out += "  \"identity\": {\n    ";
    CreationTextAppendStringField(out, "givenName", identity.GivenName, ",\n    ");
    CreationTextAppendStringField(out, "familyName", identity.FamilyName, ",\n    ");
    CreationTextAppendStringField(out, "pronouns", identity.Pronouns, ",\n    ");
    CreationTextAppendStringField(out, "homeCity", identity.HomeCity, ",\n    ");
    CreationTextAppendStringField(out, "primaryLanguage", identity.PrimaryLanguage, ",\n    ");
    out += "\"additionalLanguages\": [";
    for (std::size_t index = 0; index < identity.AdditionalLanguages.size(); ++index)
    {
        out += index == 0 ? "" : ", ";
        CreationTextAppendEscaped(out, identity.AdditionalLanguages[index]);
    }
    out += "],\n    ";
    char date[32];
    std::snprintf(date, sizeof(date), "%04d-%02d-%02d", static_cast<int>(identity.Birth.Year),
                  static_cast<int>(identity.Birth.Month), static_cast<int>(identity.Birth.Day));
    CreationTextAppendStringField(out, "dateOfBirth", date, "\n  },\n");

    out += "  \"school\": {\n    ";
    CreationTextAppendStringField(out, "streamId", save.School.StreamId, ",\n    ");
    CreationTextAppendStringField(out, "reason", save.School.Reason, "\n  },\n");
    out += "  \"identityMigrated\": ";
    out += save.IdentityMigrated ? "true" : "false";
    out += ",\n";
}

bool CreationTextReadIdentity(CreationTextReader& reader, CharacterCreationSave& save, CreationTextProfile& seen)
{
    if (!reader.Consume('{'))
    {
        return false;
    }
    if (reader.Consume('}'))
    {
        return true;
    }
    IdentityProfile& identity = save.Identity;
    while (true)
    {
        std::string key;
        if (!reader.ReadString(key) || !reader.Consume(':'))
        {
            return false;
        }
        if (key == "givenName") { if (!reader.ReadString(identity.GivenName)) return false; }
        else if (key == "familyName") { if (!reader.ReadString(identity.FamilyName)) return false; }
        else if (key == "pronouns") { if (!reader.ReadString(identity.Pronouns)) return false; }
        else if (key == "homeCity") { if (!reader.ReadString(identity.HomeCity)) return false; }
        else if (key == "primaryLanguage") { if (!reader.ReadString(identity.PrimaryLanguage)) return false; }
        else if (key == "additionalLanguages")
        {
            if (!IdentityTextReadStringArray(reader, identity.AdditionalLanguages)) return false;
        }
        else if (key == "dateOfBirth")
        {
            std::string date;
            if (!reader.ReadString(date) || !IdentityTextParseIsoDate(date, identity.Birth))
            {
                return false;
            }
            seen.SawDateOfBirth = true;
        }
        else
        {
            return false;
        }
        if (reader.Consume(','))
        {
            continue;
        }
        return reader.Consume('}');
    }
}

bool CreationTextReadSchool(CreationTextReader& reader, CharacterCreationSave& save, CreationTextProfile& seen)
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
        if (!reader.ReadString(key) || !reader.Consume(':'))
        {
            return false;
        }
        if (key == "streamId")
        {
            if (!reader.ReadString(save.School.StreamId))
            {
                return false;
            }
            seen.SawSchoolStream = true;
        }
        else if (key == "reason") { if (!reader.ReadString(save.School.Reason)) return false; }
        else
        {
            return false;
        }
        if (reader.Consume(','))
        {
            continue;
        }
        return reader.Consume('}');
    }
}
} // namespace PerfectWorld
