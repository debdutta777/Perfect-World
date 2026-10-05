#pragma once

#include "CharacterCreation.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

// Shared by CreationSaveSerialization.cpp (the record and its trait and appearance
// sections) and CreationIdentity.cpp (the identity, school and migration sections
// added in schema v5). Not part of the public API.
namespace PerfectWorld
{
inline void CreationTextAppendEscaped(std::string& out, std::string_view value)
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

inline void CreationTextAppendStringField(std::string& out, std::string_view key, std::string_view value,
                                          const char* suffix)
{
    CreationTextAppendEscaped(out, key);
    out += ": ";
    CreationTextAppendEscaped(out, value);
    out += suffix;
}

/** Single-pass reader over the deliberate JSON subset this format uses. */
class CreationTextReader
{
public:
    explicit CreationTextReader(std::string_view text) : Text_(text) {}

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

/**
 * What the reader found in the v5 sections. A section the file did not contain
 * stays unseen, so a v5 file that omits a required one is refused rather than
 * silently given the defaults.
 */
struct CreationTextProfile
{
    bool SawDateOfBirth = false;
    bool SawSchoolStream = false;
};

/** Writes `"identity": {...}, "school": {...}, "identityMigrated": ...,` and a trailing newline-indent for the next key. */
void CreationTextAppendProfile(std::string& out, const CharacterCreationSave& save);

/** Reads the identity object: the five original strings plus additionalLanguages and dateOfBirth. */
bool CreationTextReadIdentity(CreationTextReader& reader, CharacterCreationSave& save, CreationTextProfile& seen);

/** Reads the school object: streamId and reason. */
bool CreationTextReadSchool(CreationTextReader& reader, CharacterCreationSave& save, CreationTextProfile& seen);
} // namespace PerfectWorld
