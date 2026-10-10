#include "IdentityRegistry.h"

#include "IdentityText.h"
#include "RandomStreams.h"

#include <array>
#include <utility>

namespace PerfectWorld
{
namespace
{
namespace Text = IdentityText;

constexpr std::array<std::string_view, IdentityReferenceKindCount> IdentityKindNames = {
    "dialogue", "news", "ranking", "correspondence", "family_history", "record"};

constexpr std::string_view IdentityGeneratedPrefix = "p_";
constexpr std::string_view IdentityHeader = "pw_identities";
// Mixed into the hash so a registry hash never equals any other module's hash of equal bytes.
constexpr std::uint64_t IdentityHashTag = 0x504944454E544931ULL; // "PIDENTI1"

bool IdentityIsHex16(std::string_view text)
{
    if (text.size() != 16)
        return false;
    for (const char c : text)
    {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    }
    return true;
}

bool IdentityOriginValid(const IdentityOrigin& origin)
{
    return Text::IsToken(origin.Cohort) && origin.Slot >= 0;
}

bool IdentityFirstRefValid(const IdentityFirstRef& ref)
{
    return static_cast<std::size_t>(ref.Kind) < IdentityReferenceKindCount && ref.At >= 0 &&
           (ref.Ref.empty() || Text::IsToken(ref.Ref));
}

std::string IdentityHex16(std::uint64_t value)
{
    constexpr std::string_view digits = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i)
    {
        out[static_cast<std::size_t>(i)] = digits[value & 0xfU];
        value >>= 4;
    }
    return out;
}

/** The keyed stream every identity derivation shares: ID hash first, then the roll seed. */
RandomStream IdentityStreamFor(std::uint64_t worldSeed, const IdentityOrigin& origin)
{
    const std::array<std::int64_t, 1> key = {origin.Slot};
    return KeyedRandomStream(worldSeed, RandomStreamId::Crystallisation, key, origin.Cohort);
}

RandomStream IdentityStreamForAuthored(std::uint64_t worldSeed, std::string_view id)
{
    // Slot -1 is never a valid generated slot, so authored and generated keys cannot meet.
    const std::array<std::int64_t, 1> key = {-1};
    return KeyedRandomStream(worldSeed, RandomStreamId::Crystallisation, key, id);
}

/** Shared tail of both registration paths: insert, or report what is already there. */
IdentityResult IdentityInsert(IdentityRegistry& registry, Identity&& candidate)
{
    const auto found = registry.People.find(candidate.Id);
    if (found != registry.People.end())
    {
        const Identity& held = found->second;
        const bool sameOrigin = held.Authored == candidate.Authored && held.Origin == candidate.Origin;
        return {sameOrigin ? IdentityStatus::Existing : IdentityStatus::IdCollision, held.Id};
    }
    IdentityResult result{IdentityStatus::Created, candidate.Id};
    registry.People.emplace(candidate.Id, std::move(candidate));
    return result;
}

void IdentityHashPerson(Text::HashWriter& out, const Identity& person)
{
    out.Text(person.Id);
    out.Unsigned(person.Authored ? 1 : 0);
    out.Text(person.Origin.Cohort);
    out.Signed(person.Origin.Slot);
    out.Unsigned(static_cast<std::uint64_t>(person.FirstRef.Kind));
    out.Signed(person.FirstRef.At);
    out.Text(person.FirstRef.Ref);
}

std::string IdentityPersonLine(const Identity& person)
{
    std::string line = "person ";
    line += person.Id;
    line += person.Authored ? " 1 " : " 0 ";
    line += person.Authored ? std::string("-") : person.Origin.Cohort;
    line += ' ';
    line += std::to_string(person.Origin.Slot);
    line += ' ';
    line += IdentityReferenceKindName(person.FirstRef.Kind);
    line += ' ';
    line += std::to_string(person.FirstRef.At);
    line += ' ';
    line += person.FirstRef.Ref.empty() ? std::string("-") : person.FirstRef.Ref;
    line += '\n';
    return line;
}

/** Parses "person ..." fields into a validated identity for the given world seed, or nothing. */
std::optional<Identity> IdentityParsePerson(const std::vector<std::string_view>& fields, std::uint64_t worldSeed)
{
    if (fields.size() != 8 || fields[0] != "person")
        return std::nullopt;
    Identity person;
    person.Id = std::string(fields[1]);
    if (fields[2] != "0" && fields[2] != "1")
        return std::nullopt;
    person.Authored = fields[2] == "1";
    const std::optional<std::int64_t> slot = Text::ParseSigned(fields[4]);
    const std::optional<IdentityReferenceKind> kind = ParseIdentityReferenceKind(fields[5]);
    const std::optional<std::int64_t> at = Text::ParseSigned(fields[6]);
    if (!slot || !kind || !at)
        return std::nullopt;
    person.Origin.Slot = *slot;
    person.FirstRef.Kind = *kind;
    person.FirstRef.At = *at;
    if (fields[7] != "-")
        person.FirstRef.Ref = std::string(fields[7]);
    if (!IdentityFirstRefValid(person.FirstRef))
        return std::nullopt;

    if (person.Authored)
    {
        // Authored cast have no origin; their ID is a content token that cannot be mistaken for a generated one.
        if (fields[3] != "-" || *slot != 0 || !IsValidIdentityId(person.Id) || person.Id.starts_with(IdentityGeneratedPrefix))
            return std::nullopt;
        return person;
    }
    person.Origin.Cohort = std::string(fields[3]);
    if (!IdentityOriginValid(person.Origin) || DeriveGeneratedIdentityId(worldSeed, person.Origin) != person.Id)
        return std::nullopt;
    return person;
}
} // namespace

std::string_view IdentityReferenceKindName(IdentityReferenceKind kind)
{
    const std::size_t index = static_cast<std::size_t>(kind);
    return index < IdentityReferenceKindCount ? IdentityKindNames[index] : std::string_view{};
}

std::optional<IdentityReferenceKind> ParseIdentityReferenceKind(std::string_view name)
{
    for (std::size_t index = 0; index < IdentityReferenceKindCount; ++index)
    {
        if (IdentityKindNames[index] == name)
            return static_cast<IdentityReferenceKind>(index);
    }
    return std::nullopt;
}

bool IsStableIdToken(std::string_view text)
{
    return Text::IsToken(text);
}

bool IsValidIdentityId(std::string_view id)
{
    if (id.starts_with(IdentityGeneratedPrefix))
        return IdentityIsHex16(id.substr(IdentityGeneratedPrefix.size()));
    return Text::IsToken(id);
}

std::string DeriveGeneratedIdentityId(std::uint64_t worldSeed, const IdentityOrigin& origin)
{
    if (!IdentityOriginValid(origin))
        return {};
    std::string id(IdentityGeneratedPrefix);
    id += IdentityHex16(IdentityStreamFor(worldSeed, origin).NextU64());
    return id;
}

std::uint64_t DeriveIdentityRollSeed(std::uint64_t worldSeed, const Identity& identity)
{
    RandomStream stream = identity.Authored ? IdentityStreamForAuthored(worldSeed, identity.Id) : IdentityStreamFor(worldSeed, identity.Origin);
    stream.NextU64(); // the first draw is the ID; the second is the roll seed
    return stream.NextU64();
}

IdentityResult ReferenceCohortMember(IdentityRegistry& registry, const IdentityOrigin& origin, const IdentityFirstRef& firstRef)
{
    if (!IdentityOriginValid(origin))
        return {IdentityStatus::BadOrigin, {}};
    if (!IdentityFirstRefValid(firstRef))
        return {IdentityStatus::BadReference, {}};
    Identity candidate;
    candidate.Id = DeriveGeneratedIdentityId(registry.WorldSeed, origin);
    candidate.Authored = false;
    candidate.Origin = origin;
    candidate.FirstRef = firstRef;
    return IdentityInsert(registry, std::move(candidate));
}

IdentityResult RegisterAuthoredIdentity(IdentityRegistry& registry, std::string_view contentId, const IdentityFirstRef& firstRef)
{
    if (!Text::IsToken(contentId) || contentId.starts_with(IdentityGeneratedPrefix))
        return {IdentityStatus::BadOrigin, {}};
    if (!IdentityFirstRefValid(firstRef))
        return {IdentityStatus::BadReference, {}};
    Identity candidate;
    candidate.Id = std::string(contentId);
    candidate.Authored = true;
    candidate.FirstRef = firstRef;
    return IdentityInsert(registry, std::move(candidate));
}

const Identity* FindIdentity(const IdentityRegistry& registry, std::string_view id)
{
    const auto found = registry.People.find(std::string(id));
    return found == registry.People.end() ? nullptr : &found->second;
}

std::uint64_t HashIdentityRegistry(const IdentityRegistry& registry)
{
    Text::HashWriter out;
    out.Unsigned(IdentityHashTag);
    out.Unsigned(IdentityRegistryFormatVersion);
    out.Unsigned(registry.WorldSeed);
    out.Unsigned(registry.People.size());
    for (const auto& [id, person] : registry.People)
        IdentityHashPerson(out, person);
    return out.Result();
}

std::string SerialiseIdentityRegistry(const IdentityRegistry& registry)
{
    std::string out;
    out += IdentityHeader;
    out += ' ';
    out += std::to_string(IdentityRegistryFormatVersion);
    out += "\nseed ";
    out += std::to_string(registry.WorldSeed);
    out += "\ncount ";
    out += std::to_string(registry.People.size());
    out += '\n';
    for (const auto& [id, person] : registry.People)
        out += IdentityPersonLine(person);
    out += "end ";
    out += std::to_string(HashIdentityRegistry(registry));
    out += '\n';
    return out;
}

std::optional<IdentityRegistry> ParseIdentityRegistry(std::string_view text)
{
    const std::optional<std::vector<std::string_view>> lines = Text::SplitLines(text);
    if (!lines || lines->size() < 4)
        return std::nullopt;

    const auto header = Text::SplitFields((*lines)[0]);
    const auto seedLine = Text::SplitFields((*lines)[1]);
    const auto countLine = Text::SplitFields((*lines)[2]);
    if (!header || header->size() != 2 || (*header)[0] != IdentityHeader || (*header)[1] != std::to_string(IdentityRegistryFormatVersion))
        return std::nullopt;
    if (!seedLine || seedLine->size() != 2 || (*seedLine)[0] != "seed")
        return std::nullopt;
    if (!countLine || countLine->size() != 2 || (*countLine)[0] != "count")
        return std::nullopt;
    const std::optional<std::uint64_t> seed = Text::ParseUnsigned((*seedLine)[1]);
    const std::optional<std::uint64_t> count = Text::ParseUnsigned((*countLine)[1]);
    if (!seed || !count || *count > IdentityMaxCount || lines->size() != *count + 4)
        return std::nullopt;

    IdentityRegistry registry;
    registry.WorldSeed = *seed;
    for (std::size_t index = 0; index < *count; ++index)
    {
        const auto fields = Text::SplitFields((*lines)[3 + index]);
        if (!fields)
            return std::nullopt;
        std::optional<Identity> person = IdentityParsePerson(*fields, registry.WorldSeed);
        // Canonical order is strictly ascending, which also refuses duplicates.
        if (!person || (!registry.People.empty() && !(registry.People.rbegin()->first < person->Id)))
            return std::nullopt;
        registry.People.emplace(person->Id, *person);
    }

    const auto endLine = Text::SplitFields(lines->back());
    if (!endLine || endLine->size() != 2 || (*endLine)[0] != "end")
        return std::nullopt;
    const std::optional<std::uint64_t> hash = Text::ParseUnsigned((*endLine)[1]);
    if (!hash || *hash != HashIdentityRegistry(registry))
        return std::nullopt;
    return registry;
}
} // namespace PerfectWorld
