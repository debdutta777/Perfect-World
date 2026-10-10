#pragma once

#include "GameClock.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Aptitude Projection: the school terminal's evidence per path (ROADMAP M3-05;
 * PATHS_AND_PROGRESSION "Legibility"; SCHOOL_AND_PROGRESSION innovation 2, "a
 * projection that names missing evidence"; SYSTEMS_CATALOGUE "Aptitude
 * Projection").
 *
 * A pure, integer-only module over a small plain ledger (AptitudeLedger). The
 * caller supplies evidence items as plain data; the module reads no clock, owns
 * no stats, and is not a member of WorldState, the reducer, the hash or WorldSave.
 *
 * What it answers. For each of the ten paths of PATHS_AND_PROGRESSION: how strong
 * the evidence is (a band, never a number), how much of the path has been seen at
 * all (coverage), the strongest items behind the projection (references), and
 * what has NOT been observed. "Unobserved" and "weak demonstrated performance"
 * are different states with different words: "Athletics: insufficient observation.
 * This means unknown, not unsuitable." A band is never an admission restriction.
 * The module exposes no score: the tallies are private to AptitudeProjection*.cpp
 * and the public output is enums, tokens, evidence IDs and text.
 *
 * Facets. The "documented evidence kinds" of a path are facets, the things a path's
 * verbs need (AptitudeFacet). Each path documents five (AptitudeFacetsForPath),
 * read from the verb lists, ladders and printout examples of PATHS_AND_PROGRESSION;
 * a facet bears on every path that lists it (AptitudePathsForFacet). The four
 * classroom structures give four facets (analysis, interpretation, fault finding,
 * teamwork); practice consolidation, activity completion and attendance give the
 * others (hands-on work, physical performance, accounts, scheduling, creative
 * work, steadiness). AptitudeFacetForSkill maps the ten skills to a facet.
 *
 * An evidence item (AptitudeEvidence). A stable ID; a source (structure exposure,
 * practice consolidation, activity completion, attendance); the facet it bears on;
 * a subject (the problem family, skill or activity, which is what "near-identical"
 * means); a result; whether a structure exposure was routine-abbreviated; and the
 * game instant. Results are Unsound (observed, the central claim not borne out),
 * Partial and Sound. A missed class or a failed attempt is never recorded as
 * weakness by attendance or practice: those sources accept only Partial and Sound.
 * A routine-abbreviated exposure is Sound by definition. Structure sources can
 * evidence only their own facet; attendance only steadiness.
 *
 * Saturation (anti-farming). Items with the same source, facet and subject form a
 * group, ranked best first (by points, then earlier, then ID). The k-th item of a
 * group (k from 0) counts for its points shifted right by k bits, so eight points
 * become 8, 4, 2, 1, then nothing: repeating the same evidence cannot raise a band
 * beyond the first AptitudeGroupKeep items, and the ledger keeps only those, so
 * 10,000 repeats occupy four slots. Points per item: a foreground structure
 * exposure 8 (Sound) or 4 (Partial); a routine-abbreviated exposure counts as
 * documented evidence of an established method, at 4 (a routine exposure presents
 * no case, so it never earns the foreground's points); a consolidated practice or
 * an activity 4 (Sound) or 2 (Partial); attendance 2 (Sound) or 1 (Partial);
 * Unsound counts for nothing but observation. For one facet, one source kind adds
 * at most AptitudeSourceFacetCap points, so "twenty barely-different families"
 * cannot be cycled either: a facet is Shown at AptitudeShownMin points and
 * Established at AptitudeEstablishedMin, which no single source kind can reach.
 *
 * Facet states: Unobserved (no item); Observed (an item, nothing demonstrated:
 * "narrowing uncertainty without improving outcomes"); Shown; Established.
 *
 * Bands, over a path's five facets. The first facet listed for a path is its
 * signature, the verb it cannot do without (research: analysis; engineering:
 * fault finding; athletics: physical performance). A life that never touched a
 * path's signature is unknown for it, however much steadiness or teamwork it shows.
 *   InsufficientObservation  the signature is Unobserved, or fewer than
 *                            AptitudePathMinObserved facets are observed
 *   Developing               enough observed but not yet plausible ("Early
 *                            evidence": visibly not "unknown", and no verdict)
 *   Plausible                the signature Shown or better and two facets Shown
 *   Strong                   the signature Established, two facets Established and
 *                            three Shown or better
 * Coverage (how much of the path has been seen, the qualitative uncertainty):
 * Unknown (none observed), Sparse (one), Partial (two or three), Broad (four or
 * five). Adding evidence never lowers a band or a facet state, and observing a
 * facet through a group activity narrows coverage and shortens the Unobserved list
 * even when its result is Unsound and no band changes.
 *
 * Order independence. A ledger is a set: adding the same items in any order gives
 * the same ledger, the same text and the same projection. Re-adding an ID is a
 * no-op; for an ID reused with different content the best-ranked copy is kept.
 * Exact while below AptitudeMaxItems (past it, a new group is refused).
 */

constexpr std::size_t AptitudePathCount = 10;
constexpr std::size_t AptitudeFacetCount = 10;
constexpr std::size_t AptitudeFacetsPerPath = 5;
/** Items kept per group: the points halve each rank, so the fifth and later count for nothing. */
constexpr std::size_t AptitudeGroupKeep = 4;
/** The most points one source kind adds to one facet (private tally; shown here to document the rule). */
constexpr std::int32_t AptitudeSourceFacetCap = 12;
/** Points at which a facet is Shown and Established; no single source kind reaches the second. */
constexpr std::int32_t AptitudeShownMin = 4;
constexpr std::int32_t AptitudeEstablishedMin = 16;
/** Facets of a path that must be observed for any band above InsufficientObservation. */
constexpr std::size_t AptitudePathMinObserved = 2;
/** Evidence IDs named as a path's strongest support. */
constexpr std::size_t AptitudeMaxReferences = 3;
/** Not documented: bounds that keep a ledger and its text finite. */
constexpr std::size_t AptitudeMaxItems = 4096;
constexpr std::size_t AptitudeMaxIdLength = 128;

/** The ten paths of PATHS_AND_PROGRESSION, in the order of its table. */
enum class AptitudePath : std::uint8_t
{
    Hunter,
    Scholar,
    Engineer,
    Enterprise,
    Athlete,
    Clinician,
    PublicLeader,
    Logistician,
    Artist,
    Ecologist
};

/** The documented evidence kinds (what a path's verbs need). */
enum class AptitudeFacet : std::uint8_t
{
    Analysis,       // model a system: connect quantities, predict what a change does
    Interpretation, // construct an explanation: claim, evidence, limitation
    FaultFinding,   // investigate a fault: choose tests, eliminate causes
    Teamwork,       // coordinate a group: assign, exchange, settle, submit together
    HandsOn,        // practical work: repair, build, tend, treat, carry out
    Physical,       // sport, fitness and endurance
    Accounts,       // records, stock, payments
    Scheduling,     // sequencing, routes, timetables
    Creative,       // making things that did not exist
    Steadiness      // dependable attendance and self-management over weeks
};

enum class AptitudeSource : std::uint8_t
{
    StructureModel,
    StructureExplain,
    StructureInvestigate,
    StructureCoordinate,
    PracticeConsolidation,
    ActivityCompletion,
    Attendance
};

enum class AptitudeResult : std::uint8_t
{
    Unsound,
    Partial,
    Sound
};

enum class AptitudeBand : std::uint8_t
{
    InsufficientObservation,
    Developing,
    Plausible,
    Strong
};

enum class AptitudeCoverage : std::uint8_t
{
    Unknown,
    Sparse,
    Partial,
    Broad
};

enum class AptitudeFacetState : std::uint8_t
{
    Unobserved,
    Observed,
    Shown,
    Established
};

/** Stable tokens and display text. Empty for a value outside the enum. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudePathId(AptitudePath path) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudePathName(AptitudePath path) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeFacetId(AptitudeFacet facet) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeFacetName(AptitudeFacet facet) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeSourceId(AptitudeSource source) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeResultId(AptitudeResult result) noexcept;
/** For example "Strong evidence", "Plausible", "Early evidence", "Insufficient observation". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeBandText(AptitudeBand band) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeCoverageText(AptitudeCoverage coverage) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeFacetStateText(AptitudeFacetState state) noexcept;

/** The five facets a path documents, in the order they are listed. */
[[nodiscard]] PERFECTWORLDCORE_API const std::array<AptitudeFacet, AptitudeFacetsPerPath>& AptitudeFacetsForPath(
    AptitudePath path) noexcept;
/** The paths that document the facet, in path order. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<AptitudePath> AptitudePathsForFacet(AptitudeFacet facet);
/** The facet a skill's practice or activity bears on ("skill.computing" is fault finding). False for a skill not mapped. */
[[nodiscard]] PERFECTWORLDCORE_API bool AptitudeFacetForSkill(std::string_view skillId, AptitudeFacet& facet) noexcept;

struct AptitudeEvidence
{
    /** A stable ID of the form [a-z0-9_.]+. The same ID is the same evidence. */
    std::string Id;
    AptitudeSource Source = AptitudeSource::StructureModel;
    AptitudeFacet Facet = AptitudeFacet::Analysis;
    /** The problem family, skill or activity ([a-z0-9_.]+): items with one subject are near-identical. */
    std::string Subject;
    AptitudeResult Result = AptitudeResult::Sound;
    /** True for a structure exposure resolved "using established routine". Structure sources only. */
    bool Routine = false;
    GameInstant At = 0;

    friend bool operator==(const AptitudeEvidence&, const AptitudeEvidence&) = default;
};

/** True when the item is well formed and its source can evidence its facet (see the header comment). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsAptitudeEvidenceValid(const AptitudeEvidence& item);

struct AptitudeLedger
{
    /** Sorted by Id, no duplicates, at most AptitudeGroupKeep per (source, facet, subject), at most AptitudeMaxItems. */
    std::vector<AptitudeEvidence> Items;

    friend bool operator==(const AptitudeLedger&, const AptitudeLedger&) = default;
};

[[nodiscard]] PERFECTWORLDCORE_API bool IsAptitudeLedgerValid(const AptitudeLedger& ledger);

enum class AptitudeError : std::uint8_t
{
    None = 0,
    InvalidLedger,
    InvalidEvidence,
    TooMany
};

/** The stable rejection code, for example "aptitude.invalid_evidence". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AptitudeErrorCode(AptitudeError error) noexcept;

enum class AptitudeAddOutcome : std::uint8_t
{
    /** Kept: it is among the group's best items. */
    Added,
    /** The ID is already kept (identical, or a better-ranked copy is). */
    AlreadyKept,
    /** Accepted but past the group's saturation: nothing changed. */
    Saturated
};

struct AptitudeAddResult
{
    AptitudeError Error = AptitudeError::None;
    AptitudeAddOutcome Outcome = AptitudeAddOutcome::Saturated;

    [[nodiscard]] bool Ok() const noexcept { return Error == AptitudeError::None; }
};

/** Adds one item. A refusal changes nothing. Saturated and AlreadyKept change nothing either. */
PERFECTWORLDCORE_API AptitudeAddResult AptitudeAddEvidence(AptitudeLedger& ledger, const AptitudeEvidence& item);

struct AptitudeFacetStatement
{
    AptitudeFacet Facet = AptitudeFacet::Analysis;
    AptitudeFacetState State = AptitudeFacetState::Unobserved;

    friend bool operator==(const AptitudeFacetStatement&, const AptitudeFacetStatement&) = default;
};

/** One path's projection. Qualitative only: bands, states, tokens and evidence IDs. */
struct AptitudePathProjection
{
    AptitudePath Path = AptitudePath::Hunter;
    AptitudeBand Band = AptitudeBand::InsufficientObservation;
    AptitudeCoverage Coverage = AptitudeCoverage::Unknown;
    /** The path's five facets, in AptitudeFacetsForPath order, each with its state. */
    std::vector<AptitudeFacetStatement> Facets;
    /** The facets with no item yet, in the same order. "Unobserved", never "weak". */
    std::vector<AptitudeFacet> Unobserved;
    /** The strongest supporting evidence, best first, at most AptitudeMaxReferences. Unsound items never support. */
    std::vector<std::string> Strongest;

    friend bool operator==(const AptitudePathProjection&, const AptitudePathProjection&) = default;
};

struct AptitudeProjection
{
    /** One entry per path, in AptitudePath order. */
    std::vector<AptitudePathProjection> Paths;

    [[nodiscard]] PERFECTWORLDCORE_API const AptitudePathProjection* Find(AptitudePath path) const noexcept;

    friend bool operator==(const AptitudeProjection&, const AptitudeProjection&) = default;
};

/** The projection of a ledger. Pure; an invalid ledger gives an empty projection. */
[[nodiscard]] PERFECTWORLDCORE_API AptitudeProjection AptitudeProject(const AptitudeLedger& ledger);

/**
 * The printout for one path, for example
 *   "Research: Strong evidence. Established: analysis. Shown: fault finding.
 *    Observed, not yet demonstrated: teamwork. Unobserved: hands-on work."
 * An InsufficientObservation path ends "This means unknown, not unsuitable." Only
 * words; no digits, no score.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string AptitudePrintoutLine(const AptitudePathProjection& path);

/**
 * The ledger as text, integers and stable tokens only, one record per line:
 *   pw.aptitude/1 / items=N / item=<id>,<source>,<facet>,<subject>,<result>,<routine 0|1>,<at> ... / end
 * Each line ends in a newline, items are in ID order, and the same ledger always
 * gives the same text. Empty for an inconsistent ledger.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseAptitude(const AptitudeLedger& ledger);

struct AptitudeParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    AptitudeLedger Ledger;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialiseAptitude's text. Refuses truncation, extra text, malformed numbers and an inconsistent ledger. */
[[nodiscard]] PERFECTWORLDCORE_API AptitudeParse DeserialiseAptitude(std::string_view text);
} // namespace PerfectWorld
