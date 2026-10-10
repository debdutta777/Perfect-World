#pragma once

// Private helpers shared by IdentityRegistry.cpp and EventLog.cpp: the stable-ID
// token rule, the FNV-1a hash writer, and the strict line format they both use.
// Not part of the public API; nothing outside those two files includes it.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld::IdentityText
{
constexpr std::size_t MaxTokenLength = 96;

/** [a-z0-9] first, then [a-z0-9_.:-], 1..96 characters. No spaces, so a token is one field of a line. */
inline bool IsToken(std::string_view text)
{
    if (text.empty() || text.size() > MaxTokenLength)
        return false;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        const char c = text[i];
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (alnum)
            continue;
        if (i > 0 && (c == '_' || c == '.' || c == ':' || c == '-'))
            continue;
        return false;
    }
    return true;
}

/** FNV-1a 64 over tagged fields; every integer little-endian, byte by byte (same scheme as WorldHash). */
class HashWriter
{
public:
    void Byte(std::uint8_t value)
    {
        Hash_ ^= value;
        Hash_ *= 0x100000001b3ULL;
    }
    void Unsigned(std::uint64_t value)
    {
        Byte('u');
        Raw64(value);
    }
    void Signed(std::int64_t value)
    {
        Byte('i');
        Raw64(static_cast<std::uint64_t>(value));
    }
    void Text(std::string_view value)
    {
        Byte('s');
        Raw64(static_cast<std::uint64_t>(value.size()));
        for (const char c : value)
            Byte(static_cast<std::uint8_t>(c));
    }
    [[nodiscard]] std::uint64_t Result() const { return Hash_; }

private:
    void Raw64(std::uint64_t value)
    {
        for (int shift = 0; shift < 64; shift += 8)
            Byte(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
    std::uint64_t Hash_ = 0xcbf29ce484222325ULL;
};

/** Canonical decimal only: digits, no sign, no leading zero (except "0"), no overflow. */
inline std::optional<std::uint64_t> ParseUnsigned(std::string_view text)
{
    if (text.empty() || text.size() > 20 || (text.size() > 1 && text[0] == '0'))
        return std::nullopt;
    std::uint64_t value = 0;
    for (const char c : text)
    {
        if (c < '0' || c > '9')
            return std::nullopt;
        const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
        if (value > (UINT64_MAX - digit) / 10)
            return std::nullopt;
        value = value * 10 + digit;
    }
    return value;
}

/** Canonical decimal with an optional '-' (never "-0"); the whole int64 range. */
inline std::optional<std::int64_t> ParseSigned(std::string_view text)
{
    const bool negative = !text.empty() && text[0] == '-';
    const std::optional<std::uint64_t> magnitude = ParseUnsigned(negative ? text.substr(1) : text);
    if (!magnitude)
        return std::nullopt;
    if (negative)
    {
        if (*magnitude == 0 || *magnitude > static_cast<std::uint64_t>(INT64_MAX) + 1)
            return std::nullopt;
        return static_cast<std::int64_t>(0 - *magnitude);
    }
    if (*magnitude > static_cast<std::uint64_t>(INT64_MAX))
        return std::nullopt;
    return static_cast<std::int64_t>(*magnitude);
}

/**
 * Splits text into lines. The text must end in a newline (so a cut-off final
 * line is never silently complete) and may not contain a carriage return.
 */
inline std::optional<std::vector<std::string_view>> SplitLines(std::string_view text)
{
    if (text.empty() || text.back() != '\n' || text.find('\r') != std::string_view::npos)
        return std::nullopt;
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start < text.size())
    {
        const std::size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

/** Splits on single spaces; an empty field (double space, leading or trailing space) refuses the line. */
inline std::optional<std::vector<std::string_view>> SplitFields(std::string_view line)
{
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t end = line.find(' ', start);
        const std::string_view field = line.substr(start, end == std::string_view::npos ? end : end - start);
        if (field.empty())
            return std::nullopt;
        fields.push_back(field);
        if (end == std::string_view::npos)
            return fields;
        start = end + 1;
    }
}
} // namespace PerfectWorld::IdentityText
