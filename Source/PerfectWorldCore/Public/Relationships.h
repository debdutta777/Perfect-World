#pragma once

#include "EventLog.h"
#include "GameClock.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Relationships and remembered events, minimal (ROADMAP M2-10;
 * SYSTEMS_CATALOGUE.md "Relationships": Pairs map<(PersonId, PersonId),
 * {Stage, Trust, Warmth, Respect, Owed: Milli; FirstActs bitset; Memories[<=32]
 * {EventRef, At, Salience}}>).
 *
 * THE PAIR IS UNDIRECTED. The catalogue sizes NPC pairs at "at most 1,225",
 * which is C(50,2), so a pair is one record shared by both people, keyed by the
 * two identity IDs in ascending string order (Low < High). Trust, Warmth and
 * Respect describe the bond. Owed is the one directional number: it is stored
 * as "High owes Low" and read through RelationshipOwedBy.
 *
 * ACTS. A relationship changes only through Apply, which takes an act: one
 * EventLog entry (by Seq), the two people (both must be parties of that entry),
 * and a RelationshipActKind. The deltas come from a fixed table keyed on the
 * kind alone (RelationshipActEffect), every one within +-RelationshipMaxActDelta.
 * Nothing is keyed on religion, language, caste, stream or family background:
 * the inputs are opaque IDs, an enum and integers, and the event type, which
 * is only recorded in the memory, is refused if it names one of those things.
 *
 * FIRST ACT ONLY (SCHOOL_AND_PROGRESSION.md "Generosity farming": the first
 * meaningful act can change a relationship; repeated gifts cannot). Each pair
 * has a FirstActs bitset, one bit per kind. The first act of a kind applies
 * its deltas and records a memory. A later act of the same kind on that pair
 * changes no dimension (status Repeat); it is still remembered, but at a
 * quarter of the salience (RelationshipRepeatSalienceDivisor), so repeating
 * can never displace an important memory. This holds for harmful acts as well,
 * so there is no grinding in either direction. The catalogue gives no other rule, so the
 * module also keeps Apply order-dependent: acts on a pair must arrive in
 * ascending log Seq (otherwise the clamp and the "first" would depend on
 * arrival order and a replay could differ).
 *
 * MEMORIES. At most RelationshipMaxMemories per pair, kept in ascending event
 * Seq (chronological). Each one references a real log entry (EventSeq), copies
 * its time and type, and carries a base Salience set by the kind. Effective
 * salience decays by half every RelationshipSalienceHalfLifeMinutes (linear
 * between halvings, integer arithmetic). When a pair is full, the memory with
 * the lowest effective salience at the new event's time is dropped, the lower
 * Seq (older) losing a tie, and the new candidate is included in the contest,
 * so a trivial new event never displaces an important old one.
 *
 * MEMORIAL. A Bereave act turns the pair into Stage::Memorial and freezes it:
 * dimensions and memories never change again and the pair is never deleted.
 *
 * Standalone plain data, not yet wired into WorldState, the reducer, the world
 * hash or the save. To integrate: hold a RelationshipBook in WorldState, mix
 * HashRelationships into the hash as a "relationships" section and store
 * SerialiseRelationships in the save. Integers only. Nothing throws.
 */

constexpr std::uint32_t RelationshipFormatVersion = 1;

/** Trust, Warmth, Respect and Owed are Milli and always in [Min, Max]. A new pair starts at 0 (strangers). */
constexpr std::int32_t RelationshipMilliMin = -100000;
constexpr std::int32_t RelationshipMilliMax = 100000;
/** No act moves any one dimension by more than this. */
constexpr std::int32_t RelationshipMaxActDelta = 20000;
constexpr std::size_t RelationshipMaxMemories = 32;
/** Base salience is in [0, RelationshipSalienceMax]. */
constexpr std::int32_t RelationshipSalienceMax = 1000;
/** Effective salience halves every 180 game days. */
constexpr std::int64_t RelationshipSalienceHalfLifeMinutes = 180 * MinutesPerDay;
/** A repeated act is remembered at its kind's salience divided by this. */
constexpr std::int32_t RelationshipRepeatSalienceDivisor = 4;
/** A parse and Apply refuse more pairs than this. */
constexpr std::size_t RelationshipMaxPairs = 8192;

/** The catalogue's stage list. M2-10 only produces Acquaintance and Memorial; the rest wait for M5. */
enum class RelationshipStage : std::uint8_t
{
    Acquaintance,
    Friend,
    Close,
    Strained,
    Estranged,
    Reconciled,
    Family,
    Partner,
    Memorial,
};
constexpr std::size_t RelationshipStageCount = 9;

/** The kinds of act that count as meaningful; the bit index in FirstActs is the enum value. */
enum class RelationshipActKind : std::uint8_t
{
    Help,
    Gift,
    Confide,
    Defend,
    Apologise,
    ShareTime,
    Insult,
    Betray,
    Bereave,
};
constexpr std::size_t RelationshipActKindCount = 9;

/** "acquaintance", "friend", ... "memorial". Empty for a bad value. */
PERFECTWORLDCORE_API std::string_view RelationshipStageName(RelationshipStage stage);
PERFECTWORLDCORE_API std::optional<RelationshipStage> ParseRelationshipStage(std::string_view name);
/** "help", "gift", "confide", "defend", "apologise", "share_time", "insult", "betray", "bereave". */
PERFECTWORLDCORE_API std::string_view RelationshipActKindName(RelationshipActKind kind);
PERFECTWORLDCORE_API std::optional<RelationshipActKind> ParseRelationshipActKind(std::string_view name);

/** What one first act of a kind does. Owed is positive when the OTHER person comes to owe the actor. */
struct RelationshipEffect
{
    std::int32_t Trust = 0;
    std::int32_t Warmth = 0;
    std::int32_t Respect = 0;
    std::int32_t Owed = 0;
    std::int32_t Salience = 0;
};
/** The fixed table; a bad kind gives all zeros. Every delta is within +-RelationshipMaxActDelta. */
PERFECTWORLDCORE_API RelationshipEffect RelationshipActEffect(RelationshipActKind kind);

/**
 * True when the event type is acceptable to remember: a stable token that does
 * not contain religion, religious, faith, caste, language, tongue, stream or
 * background as a word (words split on _ . : -).
 */
PERFECTWORLDCORE_API bool IsRelationshipEventTypeFair(std::string_view type);

struct RelationshipMemory
{
    /** Seq of the EventLog entry this remembers. */
    std::uint64_t EventSeq = 0;
    /** Copied from the log entry. */
    GameInstant At = 0;
    /** Copied from the log entry. */
    std::string Type;
    RelationshipActKind Kind = RelationshipActKind::Help;
    /** Base salience at the time, 0..RelationshipSalienceMax. */
    std::int32_t Salience = 0;

    friend bool operator==(const RelationshipMemory&, const RelationshipMemory&) = default;
};

struct Relationship
{
    /** Identity IDs with Low < High. */
    std::string Low;
    std::string High;
    RelationshipStage Stage = RelationshipStage::Acquaintance;
    std::int32_t Trust = 0;
    std::int32_t Warmth = 0;
    std::int32_t Respect = 0;
    /** Positive: High owes Low. Use RelationshipOwedBy to read it for a person. */
    std::int32_t Owed = 0;
    /** Bit (1 << kind) set once an act of that kind has applied. */
    std::uint32_t FirstActs = 0;
    /** Seq of the last act that applied or repeated; later acts must have a greater Seq. */
    std::uint64_t LastSeq = 0;
    /** Ascending EventSeq, at most RelationshipMaxMemories. */
    std::vector<RelationshipMemory> Memories;

    friend bool operator==(const Relationship&, const Relationship&) = default;
};

/** Effective salience of a memory at `now`: the base halved per half-life, never below 0. A memory dated after `now` counts as brand new. */
PERFECTWORLDCORE_API std::int32_t RelationshipEffectiveSalience(const RelationshipMemory& memory, GameInstant now);
/** How much `person` owes the other (negative: the other owes `person`); 0 if `person` is not in the pair. */
PERFECTWORLDCORE_API std::int32_t RelationshipOwedBy(const Relationship& relationship, std::string_view person);
/** The memories ordered by effective salience at `now`, highest first; ties newest (higher Seq) first. */
PERFECTWORLDCORE_API std::vector<RelationshipMemory> RelationshipMemoriesBySalience(const Relationship& relationship, GameInstant now);

/** One proposed act. */
struct RelationshipAct
{
    /** Seq of the EventLog entry that records it. */
    std::uint64_t EventSeq = 0;
    /** The person who did it. */
    std::string Actor;
    /** The person it was done to. */
    std::string Other;
    RelationshipActKind Kind = RelationshipActKind::Help;
};

enum class RelationshipStatus : std::uint8_t
{
    /** The deltas applied (a new pair is created on the first act). */
    Applied,
    /** This kind of act already applied to this pair: no dimension changed, only a low-salience memory was added. */
    Repeat,
    /** No such log entry. */
    BadEvent,
    /** Actor or Other is not a valid ID, they are the same, or one is not a party of the entry. */
    BadParties,
    /** Kind is out of range. */
    BadKind,
    /** The entry's type is not fair to remember (see IsRelationshipEventTypeFair). */
    BadEventType,
    /** EventSeq is not greater than the pair's LastSeq. */
    OutOfOrder,
    /** The pair is a memorial and no longer changes. */
    Frozen,
    /** RelationshipMaxPairs reached. */
    Full,
};

struct RelationshipApplyResult
{
    RelationshipStatus Status = RelationshipStatus::BadEvent;
    /** Applied and Repeat: the memory was kept (false when it lost the eviction contest at once). */
    bool MemoryKept = false;
    /** Applied and Repeat: the EventSeq of the memory dropped to make room, or 0. */
    std::uint64_t EvictedSeq = 0;

    [[nodiscard]] bool Ok() const { return Status == RelationshipStatus::Applied; }
};

class PERFECTWORLDCORE_API RelationshipBook
{
public:
    using Key = std::pair<std::string, std::string>;

    /** Validates the act against the log and applies it. A refused act changes nothing; a repeated one changes no dimension. */
    RelationshipApplyResult Apply(const EventLog& log, const RelationshipAct& act);

    /** The pair of these two people (either order), or nullptr. */
    [[nodiscard]] const Relationship* Find(std::string_view a, std::string_view b) const;
    [[nodiscard]] std::size_t Size() const { return Pairs_.size(); }
    /** Read-only, in ascending (Low, High) order. */
    [[nodiscard]] const std::map<Key, Relationship>& Pairs() const { return Pairs_; }

    friend bool operator==(const RelationshipBook&, const RelationshipBook&) = default;

    /**
     * For ParseRelationships only: adds an already-validated pair. Returns false
     * (and adds nothing) if the key is not strictly ascending after the last pair
     * or the pair limit is reached. It does not check the pair's contents.
     */
    bool RestorePair(Relationship relationship);

private:
    std::map<Key, Relationship> Pairs_;
};

/** True when every memory names a real log entry with the same time and type, in which both people are parties. */
[[nodiscard]] PERFECTWORLDCORE_API bool RelationshipsMatchLog(const RelationshipBook& book, const EventLog& log);

[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashRelationships(const RelationshipBook& book);

/**
 * Text form, integers in plain decimal, one line per field group:
 *
 *   pw_relationships 1
 *   count <pairs>
 *   pair <low> <high> <stage> <trust> <warmth> <respect> <owed> <firstActsMask> <lastSeq> <memoryCount>
 *   mem <eventSeq> <at> <type> <kind> <salience>        (memoryCount lines, ascending eventSeq)
 *   ...
 *   end <u64 hash of the book>
 *
 * Pairs are in ascending (low, high) order. Serialising the same book always
 * gives the same bytes.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseRelationships(const RelationshipBook& book);

/**
 * Strict inverse: nothing for truncation at any byte, unknown fields, wrong
 * counts, unsorted or repeated pairs, an out-of-range dimension, a memory whose
 * kind is not in FirstActs, a memorial that is not a bereavement (or the
 * reverse), a type that is not fair, non-canonical numbers, or a hash that
 * does not match.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<RelationshipBook> ParseRelationships(std::string_view text);
} // namespace PerfectWorld
