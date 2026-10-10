#pragma once

#include "GameClock.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
struct IdentityRegistry;

/**
 * Conduct ledger (ROADMAP M4-01; SYSTEMS_CATALOGUE.md "Tutorial dungeon" and
 * "Awakening profile and abilities"; AWAKENING_AND_EXAM.md "The tutorial dungeon").
 *
 * The tutorial dungeon "reads what the player actually did, not what they said"
 * and presents "situations with no correct answer". This module is the hidden
 * behavioural ledger it writes to: one bounded evidence item per conduct test,
 * from which M4-02 (Awakening) reads a summary. It is standalone plain data plus
 * free functions; it is not part of WorldState, the reducer, the world hash or
 * the save (a later integration step). To integrate: add a ConductLedger member
 * to WorldState, mix HashConductLedger into the world hash as a "conduct"
 * section, and store SerialiseConductLedger's text in the save.
 *
 * Which dimensions? The documents define none: they name only the dungeon's
 * verbs (observe, assist, evade, improvise; "withdrawing to the supervised safe
 * area"; "a classmate who freezes, a shortcut that abandons someone, an
 * objective that competes with a rescue"). So this module documents a minimal
 * set, chosen to be descriptive and never ranked:
 *
 *   dimensions: Care, Steadfastness, Restraint, Resourcefulness, Attentiveness
 *   actions:    Observe, Assist, Evade, Improvise, Withdraw, Hold
 *
 * "No free-text verdict" (ROADMAP M4-01): a conduct test records an ActionTag
 * (ConductAction) and nothing else the ledger interprets. Evidence is a fixed
 * table keyed on the action alone (ConductActionEvidence). Every action carries
 * the same total evidence (ConductActionTotal), so withdrawing is exactly as
 * weighty as rescuing: differentiated, never morally ranked, and no path is the
 * lesser one. Evidence is never negative; nothing in the ledger is a penalty.
 *
 * Bounds, all named constants below:
 *   - each test id counts once; the first resolution stands, a replay is refused;
 *   - one test adds at most ConductTestDimensionCap to a dimension and
 *     ConductTestTotalCap across all dimensions, so no single test dominates;
 *   - a dimension's total saturates at ConductDimensionCap;
 *   - the ledger holds at most ConductMaxTests entries (the dungeon has three);
 *   - ConductStrongThreshold exceeds ConductTestDimensionCap, so one test can
 *     never alone make any dimension Strong.
 *
 * Fairness (SCHOOL_AND_PROGRESSION.md: religion, language, family and stream never
 * carry multipliers): evidence is keyed only on the action. Test IDs, choice IDs
 * and party IDs that contain a forbidden word (religion, caste, language, stream,
 * gender, family, household, background and close relatives; see
 * IsConductTokenFair) are refused. Nothing about a student's household, stream,
 * appearance or origin is an input.
 *
 * Integer arithmetic only. Nothing here throws.
 */

/** Serialised-format version; bump on any layout change. */
constexpr std::uint32_t ConductLedgerFormatVersion = 1;

constexpr std::size_t ConductDimensionCount = 5;
constexpr std::size_t ConductActionCount = 6;

/** The conduct dimensions. Names are stable text; they are in saves. Ordered as the evidence arrays are. */
enum class ConductDimension : std::uint8_t
{
    /** Putting another person's safety or need ahead of one's own objective. */
    Care,
    /** Staying with a commitment, post or plan under pressure. */
    Steadfastness,
    /** Holding back: avoiding a risk, keeping out of the way, not forcing an outcome. */
    Restraint,
    /** Finding a way with what is to hand. */
    Resourcefulness,
    /** Noticing and reading the situation before acting (curiosity about how it works). */
    Attentiveness,
};

/** The recorded action tag. The dungeon's verbs plus Withdraw and Hold. Names are stable text. */
enum class ConductAction : std::uint8_t
{
    Observe,
    Assist,
    Evade,
    Improvise,
    /** Withdrawing to the supervised safe area. Conduct like any other, never a failure. */
    Withdraw,
    /** Holding position or keeping to the objective despite pressure. */
    Hold,
};

/** "care", "steadfastness", "restraint", "resourcefulness", "attentiveness". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ConductDimensionName(ConductDimension dimension);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<ConductDimension> ParseConductDimension(std::string_view name);
/** "observe", "assist", "evade", "improvise", "withdraw", "hold". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ConductActionName(ConductAction action);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<ConductAction> ParseConductAction(std::string_view name);

/** Evidence points per dimension, in ConductDimension order. */
using ConductPoints = std::array<std::int32_t, ConductDimensionCount>;

/** Stakes of a test: how much the situation put at risk, 1..3, authored with the test, never player-chosen. */
constexpr std::int32_t ConductMinStakes = 1;
constexpr std::int32_t ConductMaxStakes = 3;
/** Raw evidence per action before stakes and caps: a primary dimension and a secondary one (4 + 2 = 6). */
constexpr std::int32_t ConductPrimaryPoints = 4;
constexpr std::int32_t ConductSecondaryPoints = 2;
/** The most one test adds to any one dimension. */
constexpr std::int32_t ConductTestDimensionCap = 10;
/** The most one test adds across all dimensions together; larger raw evidence is scaled down proportionally (floor). */
constexpr std::int32_t ConductTestTotalCap = 14;
/** A dimension's ledger total saturates here. */
constexpr std::int32_t ConductDimensionCap = 24;
/** Band thresholds on a dimension total: 0 NotObserved, 1..7 Slight, 8..15 Clear, 16.. Strong. */
constexpr std::int32_t ConductClearThreshold = 8;
constexpr std::int32_t ConductStrongThreshold = 16;
/** Most tests the ledger holds (the tutorial dungeon has three; the rest is headroom for make-up sessions). */
constexpr std::size_t ConductMaxTests = 6;
/** Most people named in one record. */
constexpr std::size_t ConductMaxParties = 4;
/** Evidence references named per dimension in a summary ("every output names its top three causes"). */
constexpr std::size_t ConductTopEvidenceCount = 3;

/**
 * The evidence one resolved test adds: the action's table row times stakes, each
 * dimension clamped to ConductTestDimensionCap, then scaled down if the sum
 * exceeds ConductTestTotalCap. All zeros for an invalid action or stakes.
 */
[[nodiscard]] PERFECTWORLDCORE_API ConductPoints ConductActionEvidence(ConductAction action, std::int32_t stakes);

/** The sum of ConductActionEvidence; identical for every action at the same stakes. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ConductActionTotal(std::int32_t stakes);

/**
 * True when the text is a lowercase stable token (letters, digits, '_', '.', ':',
 * '-', 1 to 96 characters) none of whose words (split at '_', '.', ':' and '-')
 * is a forbidden fairness word: religion(s), religious, faith, caste, language(s),
 * tongue, stream, gender, sex, male, female, family, household, background.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsConductTokenFair(std::string_view text);

/** What a conduct test reports when it resolves. */
struct ConductRecord
{
    /** Which test (a fair stable token such as "dungeon_freezing_classmate"). Each test counts once. */
    std::string TestId;
    /** Which choice the player made (a fair stable token); kept as the named evidence. */
    std::string ChoiceId;
    ConductAction Action = ConductAction::Observe;
    std::int32_t Stakes = ConductMinStakes;
    GameInstant At = 0;
    /** Identity-registry IDs of the people involved; optional, at most ConductMaxParties, no duplicates. */
    std::vector<std::string> Parties;

    friend bool operator==(const ConductRecord&, const ConductRecord&) = default;
};

/** One accepted test: the record (parties in ascending order) and the evidence it added. */
struct ConductEntry
{
    ConductRecord Record;
    ConductPoints Points{};

    friend bool operator==(const ConductEntry&, const ConductEntry&) = default;
};

/** The whole ledger: entries in the order they resolved (times never decrease). */
struct ConductLedger
{
    std::vector<ConductEntry> Entries;

    friend bool operator==(const ConductLedger&, const ConductLedger&) = default;
};

enum class ConductStatus : std::uint8_t
{
    /** The test was recorded. */
    Recorded,
    /** The test already resolved; its first resolution stands. Nothing changed. */
    Replay,
    /** The test id is not a fair stable token. */
    BadTest,
    /** The choice id is not a fair stable token. */
    BadChoice,
    /** The action is out of range. */
    BadAction,
    /** The stakes are outside ConductMinStakes..ConductMaxStakes. */
    BadStakes,
    /** The time is negative or earlier than the last recorded test. */
    BadTime,
    /** A party is not a valid identity ID, is repeated, or there are too many. */
    BadParties,
    /** A party contains a forbidden fairness word. */
    UnfairToken,
    /** A registry was given and a party is not in it. */
    UnknownIdentity,
    /** The ledger already holds ConductMaxTests tests. */
    LedgerFull,
};

struct ConductResult
{
    ConductStatus Status = ConductStatus::BadTest;
    /** The evidence added for Recorded; the existing entry's evidence for Replay; zeros otherwise. */
    ConductPoints Points{};

    [[nodiscard]] bool Ok() const { return Status == ConductStatus::Recorded; }
};

/**
 * Records one resolved test. Refusal of any kind (including a replay and a full
 * ledger) changes nothing. When `registry` is given, every party must be an
 * identity it holds. Parties are stored in ascending order.
 */
PERFECTWORLDCORE_API ConductResult RecordConduct(ConductLedger& ledger, const ConductRecord& record, const IdentityRegistry* registry = nullptr);

/** The accepted entry for a test id, or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const ConductEntry* FindConductEntry(const ConductLedger& ledger, std::string_view testId);

/** Per-dimension totals: the sum of every entry's evidence, each saturating at ConductDimensionCap. */
[[nodiscard]] PERFECTWORLDCORE_API ConductPoints ConductTotals(const ConductLedger& ledger);

/** A qualitative reading of one dimension. Not a rank and not a verdict. */
enum class ConductBand : std::uint8_t
{
    NotObserved,
    Slight,
    Clear,
    Strong,
};
constexpr std::size_t ConductBandCount = 4;

/** "not_observed", "slight", "clear", "strong". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ConductBandName(ConductBand band);
[[nodiscard]] PERFECTWORLDCORE_API ConductBand ConductBandFor(std::int32_t dimensionTotal);

/** A test and choice that contributed to a dimension, and how much. */
struct ConductEvidenceRef
{
    std::string TestId;
    std::string ChoiceId;
    std::int32_t Points = 0;

    friend bool operator==(const ConductEvidenceRef&, const ConductEvidenceRef&) = default;
};

/** One dimension of the summary M4-02 reads. */
struct ConductDimensionSummary
{
    ConductBand Band = ConductBand::NotObserved;
    /** The saturated total scaled to 0..1000 (total * 1000 / ConductDimensionCap). */
    std::int32_t Milli = 0;
    /** The up to ConductTopEvidenceCount entries that added most, ties broken by earlier resolution. */
    std::vector<ConductEvidenceRef> Evidence;

    friend bool operator==(const ConductDimensionSummary&, const ConductDimensionSummary&) = default;
};

struct ConductSummary
{
    std::array<ConductDimensionSummary, ConductDimensionCount> Dimensions{};
    /** How many tests the summary rests on. */
    std::uint32_t TestsRecorded = 0;

    friend bool operator==(const ConductSummary&, const ConductSummary&) = default;
};

/**
 * The bounded summary for M4-02: a band, a 0..1000 reading and named evidence
 * per dimension. A pure function of the ledger. There is no free text and no
 * overall verdict; the only inputs are the recorded actions.
 */
[[nodiscard]] PERFECTWORLDCORE_API ConductSummary SummariseConduct(const ConductLedger& ledger);

/** Fingerprint of the whole ledger. */
[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashConductLedger(const ConductLedger& ledger);

/**
 * Text form, canonical decimal integers, a closing "end <hash>" line:
 *
 *   pw_conduct 1
 *   count <n>
 *   test <testId> <choiceId> <action> <stakes> <at> <parties comma-joined or -> <care> <steadfastness> <restraint> <resourcefulness> <attentiveness>
 *   ...                                    (resolution order)
 *   end <u64 hash of the ledger>
 *
 * Serialising the same ledger always gives the same bytes.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseConductLedger(const ConductLedger& ledger);

/**
 * Strict inverse. Returns nothing for any text that is not exactly a serialised
 * ledger: truncation at any byte, unknown fields, a bad count, an entry whose
 * stored evidence differs from the table, a repeated test, a time going
 * backwards, parties out of ascending order, an unfair token, or a hash that
 * does not match the content.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<ConductLedger> ParseConductLedger(std::string_view text);
} // namespace PerfectWorld
