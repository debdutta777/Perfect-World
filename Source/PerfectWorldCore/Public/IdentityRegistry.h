#pragma once

#include "GameClock.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Persistent identity registry (ROADMAP M2-05; SYSTEMS_CATALOGUE.md "Identity registry").
 *
 * A person gets an identity at their first consequential reference (named in
 * dialogue, news, a ranking, a letter, a family history, any record the player
 * can see), not at first physical meeting. Once made, an identity never
 * changes and is never renumbered or removed. This module is standalone plain
 * data plus free functions; it is not yet part of WorldState, the reducer, the
 * world hash or the save (a later integration step). To integrate: add an
 * IdentityRegistry member to WorldState, mix HashIdentityRegistry into the
 * world hash as an "identities" section, and store SerialiseIdentityRegistry's
 * lines (or the same fields as JSON) in the save.
 *
 * ID scheme (SYSTEMS_CATALOGUE.md): a generated person is "p_" + 16 lowercase
 * hex digits of a 64-bit keyed hash of (world seed, origin key). The origin
 * key is a cohort key text plus a slot number, so "the third member of cohort
 * X" is the same person in every run, whatever order things are referenced in.
 * Authored cast use their content IDs (lowercase tokens that do not start "p_").
 * The hash is the first draw of KeyedRandomStream(worldSeed, Crystallisation,
 * {slot}, cohort), so it is stateless and never touches the sequential streams.
 * Changing the derivation is a save-breaking change.
 *
 * Integer arithmetic only. Nothing here throws.
 */

/** Longest cohort key or reference ID; keeps every field one short token. */
constexpr std::size_t IdentityMaxTokenLength = 96;
/** Serialised-format version; bump on any layout change. */
constexpr std::uint32_t IdentityRegistryFormatVersion = 1;
/** Upper bound a parse accepts, well above the 25,000-by-age-80 budget; refuses absurd counts. */
constexpr std::size_t IdentityMaxCount = 1u << 20;

/** What kind of visible record first named the person. Names are stable text; they are in saves. */
enum class IdentityReferenceKind : std::uint8_t
{
    Dialogue,
    News,
    Ranking,
    Correspondence,
    FamilyHistory,
    Record,
};
constexpr std::size_t IdentityReferenceKindCount = 6;

/** "dialogue", "news", "ranking", "correspondence", "family_history", "record". Empty for a bad value. */
PERFECTWORLDCORE_API std::string_view IdentityReferenceKindName(IdentityReferenceKind kind);
PERFECTWORLDCORE_API std::optional<IdentityReferenceKind> ParseIdentityReferenceKind(std::string_view name);

/**
 * A lowercase token: [a-z0-9] then [a-z0-9_.:-], 1 to 96 characters, no spaces.
 * The rule for cohort keys, reference IDs, authored IDs and event type IDs.
 */
PERFECTWORLDCORE_API bool IsStableIdToken(std::string_view text);

/** A valid person ID: "p_" + 16 lowercase hex digits, or an authored token that does not start with "p_". */
PERFECTWORLDCORE_API bool IsValidIdentityId(std::string_view id);

/** Where an unmet person comes from: a cohort key and a zero-based slot within it. */
struct IdentityOrigin
{
    /** A token such as "nation_bra.chess.age_16_20.y2026". Never empty for a generated person. */
    std::string Cohort;
    std::int64_t Slot = 0;

    friend bool operator==(const IdentityOrigin&, const IdentityOrigin&) = default;
};

/** The first record that named the person. Fixed once written. */
struct IdentityFirstRef
{
    IdentityReferenceKind Kind = IdentityReferenceKind::Record;
    GameInstant At = 0;
    /** A token naming the record (a content or event ID); may be empty. */
    std::string Ref;

    friend bool operator==(const IdentityFirstRef&, const IdentityFirstRef&) = default;
};

/** One person. Every field is immutable once the registry holds it. */
struct Identity
{
    std::string Id;
    /** True for authored cast (content ID, no origin); false for a generated cohort member. */
    bool Authored = false;
    /** Empty cohort and slot 0 for authored cast. */
    IdentityOrigin Origin;
    IdentityFirstRef FirstRef;

    friend bool operator==(const Identity&, const Identity&) = default;
};

/**
 * Every identity of one world, keyed by ID. The map's sorted order is the
 * canonical order, so the hash and the serialised form do not depend on the
 * order references arrived in.
 */
struct IdentityRegistry
{
    std::uint64_t WorldSeed = 0;
    std::map<std::string, Identity> People;

    friend bool operator==(const IdentityRegistry&, const IdentityRegistry&) = default;
};

enum class IdentityStatus : std::uint8_t
{
    /** A new identity was made. */
    Created,
    /** The person already had an identity; nothing changed (their first reference stands). */
    Existing,
    /** The origin or authored ID is not valid. Nothing changed. */
    BadOrigin,
    /** The first-reference record is not valid. Nothing changed. */
    BadReference,
    /** A different origin already holds this ID (a 1 in 2^64 event). Nothing changed. */
    IdCollision,
};

struct IdentityResult
{
    IdentityStatus Status = IdentityStatus::BadOrigin;
    /** The person's ID for Created and Existing; empty otherwise. */
    std::string Id;

    [[nodiscard]] bool Ok() const { return Status == IdentityStatus::Created || Status == IdentityStatus::Existing; }
};

/** The ID of a generated person, or empty when the origin is invalid (bad cohort token or negative slot). */
[[nodiscard]] PERFECTWORLDCORE_API std::string DeriveGeneratedIdentityId(std::uint64_t worldSeed, const IdentityOrigin& origin);

/**
 * The person's own seed, for later crystallisation (names, birth date, history):
 * a pure function of the world seed and the identity, never stored, so the
 * same person is reproduced byte for byte on every promotion.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t DeriveIdentityRollSeed(std::uint64_t worldSeed, const Identity& identity);

/**
 * The first consequential reference to an unmet cohort member. Creates exactly
 * one identity the first time and returns the same ID, changing nothing, on
 * every later reference, in any order and across any number of saves.
 */
PERFECTWORLDCORE_API IdentityResult ReferenceCohortMember(IdentityRegistry& registry,
                                                           const IdentityOrigin& origin,
                                                           const IdentityFirstRef& firstRef);

/** Registers a member of the authored cast under a content ID; same idempotence as above. */
PERFECTWORLDCORE_API IdentityResult RegisterAuthoredIdentity(IdentityRegistry& registry,
                                                              std::string_view contentId,
                                                              const IdentityFirstRef& firstRef);

/** The identity with this ID, or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const Identity* FindIdentity(const IdentityRegistry& registry, std::string_view id);

/** Fingerprint of the whole registry: the seed, then every person in ID order. Order-of-arrival independent. */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashIdentityRegistry(const IdentityRegistry& registry);

/**
 * Text form: one canonical line per field group, integers in plain decimal, a
 * count, and a closing "end <hash>" line.
 *
 *   pw_identities 1
 *   seed <u64>
 *   count <n>
 *   person <id> <authored 0|1> <cohort or -> <slot> <kind> <at> <ref or ->
 *   ...                                    (ascending ID order)
 *   end <u64 hash of the registry>
 *
 * Serialising the same registry always gives the same bytes.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseIdentityRegistry(const IdentityRegistry& registry);

/**
 * Strict inverse. Returns nothing for any text that is not exactly a
 * serialised registry: truncation at any byte, unknown fields, a bad count,
 * duplicate or unsorted people, a generated ID that does not match its origin
 * for the file's seed, or a hash that does not match the content.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<IdentityRegistry> ParseIdentityRegistry(std::string_view text);
} // namespace PerfectWorld
