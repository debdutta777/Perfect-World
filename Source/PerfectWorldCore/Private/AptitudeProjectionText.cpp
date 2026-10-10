#include "AptitudeProjection.h"

#include "AptitudeInternal.h"

#include <charconv>

// The strict text form of the ledger (ROADMAP M3-05). Canonical decimals only, one record per
// line, and every truncation or extra line is refused.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view AptitudeTextHeader = "pw.aptitude/1";
constexpr std::string_view AptitudeTextEnd = "end";
constexpr std::size_t AptitudeItemFields = 7;

bool AptitudeNextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false;
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

bool AptitudeKeyValue(std::string_view line, std::string_view key, std::string_view& value)
{
    if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Canonical decimal only: no '+', no leading zeros, no "-0", nothing trailing. */
template <typename Int>
bool AptitudeParseInt(std::string_view text, Int& out)
{
    if (text.empty())
        return false;
    const std::string_view digits = text.front() == '-' ? text.substr(1) : text;
    if (digits.empty() || (digits.size() > 1 && digits.front() == '0') || (text.front() == '-' && digits == "0"))
        return false;
    for (const char c : digits)
    {
        if (c < '0' || c > '9')
            return false;
    }
    Int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

std::vector<std::string_view> AptitudeSplit(std::string_view text)
{
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    for (;;)
    {
        const std::size_t comma = text.find(',', start);
        if (comma == std::string_view::npos)
        {
            fields.push_back(text.substr(start));
            return fields;
        }
        fields.push_back(text.substr(start, comma - start));
        start = comma + 1;
    }
}

AptitudeParse AptitudeRefuseText(std::string message)
{
    AptitudeParse result;
    result.Error = std::move(message);
    return result;
}

/** Reads one item line's value; false when any field is malformed. */
bool AptitudeParseItem(std::string_view value, AptitudeEvidence& item)
{
    using namespace AptitudeInternal;
    const std::vector<std::string_view> fields = AptitudeSplit(value);
    if (fields.size() != AptitudeItemFields)
        return false;
    const auto source = SourceFromId(fields[1]);
    const auto facet = FacetFromId(fields[2]);
    const auto result = ResultFromId(fields[4]);
    if (!source || !facet || !result || (fields[5] != "0" && fields[5] != "1"))
        return false;
    item.Id = std::string(fields[0]);
    item.Source = *source;
    item.Facet = *facet;
    item.Subject = std::string(fields[3]);
    item.Result = *result;
    item.Routine = fields[5] == "1";
    return AptitudeParseInt(fields[6], item.At);
}
} // namespace

std::string SerialiseAptitude(const AptitudeLedger& ledger)
{
    if (!IsAptitudeLedgerValid(ledger))
        return {};
    std::string text(AptitudeTextHeader);
    text += "\nitems=" + std::to_string(ledger.Items.size());
    for (const AptitudeEvidence& item : ledger.Items)
    {
        text += "\nitem=" + item.Id + "," + std::string(AptitudeSourceId(item.Source)) + "," +
                std::string(AptitudeFacetId(item.Facet)) + "," + item.Subject + "," +
                std::string(AptitudeResultId(item.Result)) + "," + (item.Routine ? "1" : "0") + "," +
                std::to_string(item.At);
    }
    text += "\n";
    text += AptitudeTextEnd;
    text += "\n";
    return text;
}

AptitudeParse DeserialiseAptitude(std::string_view text)
{
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!AptitudeNextLine(text, position, line) || line != AptitudeTextHeader)
        return AptitudeRefuseText("The aptitude text has no recognised header.");

    std::size_t count = 0;
    if (!AptitudeNextLine(text, position, line) || !AptitudeKeyValue(line, "items", value) ||
        !AptitudeParseInt(value, count) || count > AptitudeMaxItems)
        return AptitudeRefuseText("The aptitude text has no valid items line.");

    AptitudeLedger ledger;
    for (std::size_t i = 0; i < count; ++i)
    {
        AptitudeEvidence item;
        if (!AptitudeNextLine(text, position, line) || !AptitudeKeyValue(line, "item", value))
            return AptitudeRefuseText("The aptitude text is truncated or has a bad item line.");
        if (!AptitudeParseItem(value, item))
            return AptitudeRefuseText("An aptitude item line has a malformed field.");
        ledger.Items.push_back(std::move(item));
    }

    if (!AptitudeNextLine(text, position, line) || line != AptitudeTextEnd)
        return AptitudeRefuseText("The aptitude text is truncated or has no end line.");
    if (position != text.size())
        return AptitudeRefuseText("The aptitude text has data after the end line.");
    if (!IsAptitudeLedgerValid(ledger))
        return AptitudeRefuseText("The aptitude text describes an inconsistent ledger.");

    AptitudeParse result;
    result.Ledger = std::move(ledger);
    return result;
}
} // namespace PerfectWorld
