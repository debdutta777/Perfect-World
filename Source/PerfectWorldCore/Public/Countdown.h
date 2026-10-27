#pragma once

#include "CountdownContent.h"
#include "DayLoop.h"
#include "GameClock.h"
#include "Relationships.h"
#include "WeekCompression.h"
#include "WorldState.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The ten days (ROADMAP M4-03; GAME_PLAN §4 Phase 4; AWAKENING_AND_EXAM.md "The ten days": "a hard
 * countdown ... exactly ten in-game days", "the clock is public, identical for every student, and
 * cannot be extended", "the scheduled onset of a hazard cannot be prevented ... its consequences
 * can be changed by preparation, expertise and the player's own action"; CalendarAwakeningToExamDays).
 *
 * THE SPAN. From 18:00 on Awakening Day (Friday 23 January 2043, the evening phase of the day loop)
 * to 06:00 on examination morning (Monday 2 February 2043): ten calendar days. Nothing here moves
 * time. Days are resolved through the week machinery (WeekCompression.h, which is the day loop and
 * the reducer), up to WeekMaxDays at a time, and a plan that would run past 06:00 on examination
 * morning is refused. The countdown closes when the clock reaches that minute; if the plan covers
 * the last day but leaves hours unspent, those hours are spent awake on the player's behalf and
 * DISCLOSED (CountdownState::PaddedMinutes, a ledger line, the order id CountdownPadOrderId).
 * The countdown needs the school calendar in the catalogues (DayLoopCatalogues::Calendar): the
 * calendar's break "ten_days" is what stops these days being marked as absences.
 *
 * WHAT IS OFFERED (authored tables in CountdownContent.cpp, content-validated against the real
 * activity catalogue by ValidateCountdownContent):
 *   OPPORTUNITIES. A named window of time with a block of activity or time, in a MUTUAL-EXCLUSION
 *   GROUP (at most one opportunity per group; two whose windows overlap cannot both be taken), that
 *   may need a CONTACT (a role such as "friend" or "teacher", filled by a person from the context)
 *   whose relationship with the player (Trust, Warmth or Respect from the RelationshipBook) must
 *   reach a minimum. The relationship also shapes the odds: above the minimum, probability moves
 *   from the poor outcome to the good one. Three outcomes (good, fair, poor), each with a full
 *   CountdownEffect. The player is shown ALL of it before committing (CountdownOffers): the window,
 *   the time it costs, the odds of each outcome as permille and as a band (unlikely, possible,
 *   likely, very likely), the effect of each outcome, and what taking it closes (the rest of its
 *   group). Opportunities in the "response" group need the news first.
 *   OBLIGATIONS. Standing commitments to another person, present when the context has a contact in
 *   the role. By default an obligation is kept: its block is put into the plan, so it costs its
 *   hours. It can be NEGOTIATED once (the first attempt stands): shared with a helper (fewer
 *   hours), deferred to another slot, or declined. Each proposal's cost is disclosed first
 *   (CountdownProposals): the minutes, the odds that it is accepted, and the standing it costs
 *   if accepted or refused. Nothing is dropped silently: a plan entry that lands on an
 *   obligation's block is refused, and a declined obligation is a ledger line with its cost.
 *   THE WORLD EVENT. One scheduled Gate break (the Gate system: GateRegister and
 *   GateScheduleOnset, so the reducer fires it from the clock). Its onset is fixed and cannot be
 *   prevented. When it has fired the news is shown (CountdownNews) with the delay it would cost on
 *   examination morning and the responses now open; the delay shrinks with the responses taken
 *   and with logistics preparation (the route consequence). It is the span's one interrupt:
 *   resolving days stops after the day it arrives (CountdownResolveResult::StoppedForNews).
 *
 * UNCERTAINTY NEVER REROLLS. An opportunity's roll seed is drawn when it is COMMITTED (a keyed
 * draw from the world seed on the Opportunity stream, a pure function of the seed and the id)
 * and STORED in the state with the odds that were shown; an obligation negotiation does the
 * same. The outcome is a pure function of that stored seed and those stored odds, resolved when
 * the window has passed. Saving and loading (SerialiseCountdown), copying the state, resolving
 * the days in a different batching, or changing a relationship afterwards cannot change it; a
 * second commit or negotiation is refused (CountdownError::Taken, ::Negotiated).
 *
 * THE PROFILE (CountdownProfile). Six preparation scores that mirror the six tower floor types
 * (so every kind of life has somewhere to put its ten days), composure (condition on examination
 * morning, plus outcomes, less the route delay) and goodwill (standing with the people met).
 * CountdownProfilesComparable says what "comparable" means; CountdownProfilesDiffer, what
 * "different" means. Several strategies are defensible (GAME_PLAN §4 Phase 4): the authored
 * tables give each of study, people and rest a way to spend the ten days.
 *
 * STANDALONE. Plain data and free functions, not yet a member of WorldState, the world hash or
 * the save. To integrate: hold a CountdownState in the world, store SerialiseCountdown's text in
 * the save, apply CountdownState::Requests (relationship acts) through the event log, and let the
 * tower (M4-04) read the profile and the route delay. Integers only. Nothing throws.
 *
 * CHOSEN (the documents leave them open), marked "Design choice (M4-03)" in the code: the tables'
 * numbers; the six preparation dimensions; the three-outcome shape; the odds bands; that
 * obligations stand by default; that a negotiation is attempted once; that the countdown opens
 * at 18:00 (WorldCountdown.h connects the scheduled boundary and late make-up entry);
 * the day-loop credit mapping; and the comparability measure.
 */

constexpr std::uint32_t CountdownFormatVersion = 1;
inline constexpr std::string_view CountdownTextHeader = "pw_countdown";

// ---- The context, the state ----------------------------------------------------

/** A person the player knows, by role. */
struct CountdownContact
{
    std::string Role;
    /** An identity id (IsValidIdentityId). */
    std::string Person;
};

/** What the countdown reads about the player's people. Borrowed; the relationship book belongs to the caller. */
struct CountdownContext
{
    const RelationshipBook* Relationships = nullptr;
    /** The player's identity id. */
    std::string Player;
    /** At most one person per role (the first listed wins). */
    std::vector<CountdownContact> Contacts;
};

enum class CountdownDisposition : std::uint8_t
{
    Standing,
    Shared,
    Deferred,
    Declined
};
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownDispositionName(CountdownDisposition disposition) noexcept;

struct CountdownCommitment
{
    std::string Id;
    /** The contact whose relationship shaped the odds (empty when none was needed). */
    std::string Contact;
    /** Drawn when committed; the only source of this opportunity's chance. */
    std::uint64_t Seed = 0;
    /** The odds that were shown. */
    CountdownOdds Odds{};
    GameInstant CommittedAt = 0;
    GameInstant Start = 0;
    GameInstant End = 0;
    bool Resolved = false;
    /** Meaningful only when Resolved. */
    CountdownOutcome Outcome = CountdownOutcome::Fair;
    /** The draw, 0 to 999, meaningful only when Resolved. */
    std::int32_t Roll = 0;

    friend bool operator==(const CountdownCommitment&, const CountdownCommitment&) = default;
};

struct CountdownObligationRecord
{
    std::string Id;
    /** The person owed, fixed when the countdown opened. */
    std::string Person;
    CountdownDisposition Disposition = CountdownDisposition::Standing;
    /** A negotiation has been attempted; the first stands. */
    bool Negotiated = false;
    CountdownProposal Attempt = CountdownProposal::Honour;
    bool Accepted = false;
    std::uint64_t Seed = 0;
    std::int32_t AcceptPermille = 0;
    /** The helper who took part of a shared obligation, else empty. */
    std::string Helper;
    /** The block now in force; Minutes is 0 once declined. */
    GameInstant Start = 0;
    std::int32_t Minutes = 0;
    bool Done = false;

    friend bool operator==(const CountdownObligationRecord&, const CountdownObligationRecord&) = default;
};

/** A relationship act the countdown asks for; applying it needs an event log entry, which the integration owns. */
struct CountdownRequest
{
    std::string Actor;
    std::string Other;
    RelationshipActKind Kind = RelationshipActKind::Help;
    /** The opportunity or obligation id behind it. */
    std::string Reason;
    GameInstant At = 0;

    friend bool operator==(const CountdownRequest&, const CountdownRequest&) = default;
};

enum class CountdownLedgerKind : std::uint8_t
{
    Opened,
    Committed,
    Resolved,
    Negotiated,
    Honoured,
    News,
    Closed
};
/** "opened", "committed", "resolved", "negotiated", "honoured", "news", "closed". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownLedgerKindName(CountdownLedgerKind kind) noexcept;

/** One thing that happened, with what it cost or earned. A and B by kind: committed (displaced options, minutes), resolved (outcome, roll), negotiated (proposal, accepted), honoured (goodwill, minutes), news (delay, 0), closed (padded minutes, 0). */
struct CountdownLedgerLine
{
    GameInstant At = 0;
    CountdownLedgerKind Kind = CountdownLedgerKind::Opened;
    std::string Id;
    std::int32_t A = 0;
    std::int32_t B = 0;

    friend bool operator==(const CountdownLedgerLine&, const CountdownLedgerLine&) = default;
};

struct CountdownState
{
    /** Stored when opened: a keyed draw from the world seed. */
    std::uint64_t Seed = 0;
    std::string Player;
    GameInstant Start = 0;
    GameInstant End = 0;
    GameInstant EventOnset = 0;
    bool EventFired = false;
    bool Closed = false;
    /** Minutes spent awake on the player's behalf to reach 06:00. */
    std::int32_t PaddedMinutes = 0;
    std::vector<CountdownCommitment> Commitments;
    std::vector<CountdownObligationRecord> Obligations;
    /** Raw sums: the day loop's learning (mapped) and the opportunity outcomes. */
    CountdownPoints DayLoopPrep{};
    CountdownPoints OutcomePrep{};
    std::int32_t Composure = 0;
    std::int32_t Goodwill = 0;
    std::int32_t RouteMinutes = 0;
    std::vector<CountdownRequest> Requests;
    std::vector<CountdownLedgerLine> Ledger;

    friend bool operator==(const CountdownState&, const CountdownState&) = default;
};

enum class CountdownError : std::uint8_t
{
    None = 0,
    InvalidWorld,
    WorldBusy,
    MissingCatalogue,
    MissingCalendar,
    NotAtStart,
    BadContext,
    EventExists,
    Closed,
    UnknownOpportunity,
    UnknownObligation,
    /** The opportunity is already committed (or resolved): a second commit would be a reroll. */
    Taken,
    WindowClosed,
    NeedsNews,
    NoContact,
    RelationTooLow,
    /** Another opportunity of its group is taken: they are mutually exclusive. */
    GroupTaken,
    /** Its window overlaps a committed opportunity or an obligation's block. */
    TimeConflict,
    /** The obligation's one negotiation has been used. */
    Negotiated,
    ProposalUnavailable,
    NoDays,
    TooManyDays,
    /** A plan entry marked first-of-kind or needing a choice: the countdown resolves routine days. */
    NeedsPlay,
    /** A plan entry lands on a block the player has committed to or owes. */
    PlanConflict,
    /** The week machinery refused the plan; Detail says why. */
    BadPlan,
    /** The plan runs past 06:00 on examination morning. */
    BeyondExam,
    ContentInvalid
};
/** A stable code such as "countdown.group_taken". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownErrorCode(CountdownError error) noexcept;

struct CountdownIssue
{
    CountdownError Error = CountdownError::None;
    std::string Detail;

    [[nodiscard]] bool Ok() const noexcept { return Error == CountdownError::None; }
};

/** The record of an obligation by id, or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const CountdownObligationRecord* CountdownFindRecord(const CountdownState& state, std::string_view id);
/** The commitment to an opportunity by id, or nullptr. */
[[nodiscard]] PERFECTWORLDCORE_API const CountdownCommitment* CountdownFindCommitment(const CountdownState& state, std::string_view id);

// ---- Opening, disclosure, committing -------------------------------------------

struct CountdownOpenResult
{
    CountdownIssue Issue;
    /** The world with the Gate registered and its onset scheduled; the world as given when refused. */
    WorldState World;
    CountdownState State;

    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};

/**
 * Starts the countdown. The world must be in step and idle, at exactly CountdownStartInstant(),
 * and not already hold the event's Gate. Stores the seed, finds each obligation's person from the
 * context's contacts, registers the Gate and schedules its onset (System commands through the reducer).
 */
[[nodiscard]] PERFECTWORLDCORE_API CountdownOpenResult OpenCountdown(const WorldState& world, const CountdownContext& context,
                                                                     const DayLoopCatalogues& catalogues);

enum class CountdownAvailability : std::uint8_t
{
    Open,
    Taken,
    WindowClosed,
    NeedsNews,
    NoContact,
    RelationTooLow,
    GroupTaken,
    TimeConflict
};
/** "open", "taken", "window_closed", "needs_news", "no_contact", "relation_too_low", "group_taken", "time_conflict". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownAvailabilityName(CountdownAvailability availability) noexcept;

/** Everything shown about an opportunity before it is committed. */
struct CountdownOffer
{
    const CountdownOpportunityDef* Def = nullptr;
    CountdownAvailability Availability = CountdownAvailability::Open;
    GameInstant Start = 0;
    GameInstant End = 0;
    /** The time it costs. */
    std::int32_t Minutes = 0;
    /** The contact, empty when none is needed or there is none. */
    std::string Contact;
    /** The contact's relationship value that counts (0 with no contact). */
    std::int32_t Relation = 0;
    /** The odds that would be stored if committed now, and their bands. */
    CountdownOdds Odds{};
    std::array<CountdownBand, CountdownOutcomeCount> Bands{};
    /** The ids of the other opportunities of its group that taking it closes (those still open now). */
    std::vector<std::string> Closes;
};

/** Every opportunity, in table order, with its availability and disclosure. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<CountdownOffer> CountdownOffers(const WorldState& world, const CountdownState& state,
                                                                              const CountdownContext& context);

struct CountdownTakeResult
{
    CountdownIssue Issue;
    CountdownState State;

    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};

/** Commits to an opportunity: stores its seed and the odds shown. A refusal returns the state as given. */
[[nodiscard]] PERFECTWORLDCORE_API CountdownTakeResult TakeOpportunity(const WorldState& world, const CountdownState& state,
                                                                      const CountdownContext& context, std::string_view id);

/** What a proposal about an obligation would cost, shown before it is made. */
struct CountdownProposalOffer
{
    CountdownProposal Proposal = CountdownProposal::Honour;
    bool Available = false;
    /** Why not, when it is not (a code such as "countdown.negotiated"). */
    std::string Unavailable;
    /** The minutes the obligation would then take, and when. */
    std::int32_t MinutesIfAccepted = 0;
    GameInstant StartIfAccepted = 0;
    /** The chance it is accepted (1000 when certain) and its band. */
    std::int32_t AcceptPermille = 0;
    CountdownBand Band = CountdownBand::VeryLikely;
    /** The standing it costs (or earns) if accepted, and if refused. */
    std::int32_t GoodwillIfAccepted = 0;
    std::int32_t GoodwillIfRefused = 0;
    /** Share: the helper who would take the rest. */
    std::string Helper;
};

/** The four proposals for an obligation (Honour first: keeping it, which needs nothing). Empty for an unknown obligation. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<CountdownProposalOffer> CountdownProposals(const WorldState& world,
                                                                                         const CountdownState& state,
                                                                                         const CountdownContext& context,
                                                                                         std::string_view obligationId);

/** Makes the obligation's one negotiation. Honour is not a negotiation and is refused. Stores the seed and the odds shown. */
[[nodiscard]] PERFECTWORLDCORE_API CountdownTakeResult NegotiateObligation(const WorldState& world, const CountdownState& state,
                                                                          const CountdownContext& context,
                                                                          std::string_view obligationId, CountdownProposal proposal);

/** One block the player has committed to or owes, with the plan entry that fills it. */
struct CountdownReservedBlock
{
    std::string Id;
    bool IsObligation = false;
    GameInstant Start = 0;
    GameInstant End = 0;
    WeekEntry Entry;
};

/** Blocks not yet done, in time order. Plan around these: the resolver adds them to the plan itself. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<CountdownReservedBlock> CountdownReservedBlocks(const CountdownState& state);

// ---- Resolving days --------------------------------------------------------------

struct CountdownResolveResult
{
    CountdownIssue Issue;
    WorldState World;
    CountdownState State;
    /** Calendar days of the given plan that were resolved (fewer than given when it stopped for the news). */
    std::int32_t DaysResolved = 0;
    /** The world event's news arrived in these days: the plan was cut after its day so the player can respond. */
    bool StoppedForNews = false;
    bool Closed = false;
    /** The week machinery's own ledger and disclosures for what was resolved. */
    WeekResult Week;
    /** The ledger lines this call added. */
    std::vector<CountdownLedgerLine> Events;

    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};

/**
 * Resolves up to WeekMaxDays calendar days from the world's current day (day 0 of `days` is the day
 * the world's clock is in) through CompressWeek, after adding the blocks the player is committed to
 * and owes to the plan. Then resolves every opportunity and obligation whose block has ended (from
 * its stored seed), notes the news when the Gate has fired, accrues the day loop's learning, and
 * closes the countdown at 06:00 on examination morning. A refusal returns the world and state as given.
 */
[[nodiscard]] PERFECTWORLDCORE_API CountdownResolveResult ResolveCountdownDays(const WorldState& world, const CountdownState& state,
                                                                              const std::vector<WeekDayPlan>& days,
                                                                              const DayLoopCatalogues& catalogues);

// ---- News, profile ----------------------------------------------------------------

struct CountdownNews
{
    bool Fired = false;
    GameInstant At = 0;
    /** The delay on examination morning if nothing more were done, and with nothing done at all. */
    std::int32_t DelayNow = 0;
    std::int32_t DelayUnmitigated = 0;
    /** Response opportunities still open to take. */
    std::vector<std::string> Responses;
};

[[nodiscard]] PERFECTWORLDCORE_API CountdownNews CountdownNewsOf(const WorldState& world, const CountdownState& state,
                                                                const CountdownContext& context);

/** The examination-morning delay in minutes after the responses taken and the logistics preparation; 0 before the event fires. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CountdownRouteDelay(const CountdownState& state);

/** Where the ten days leave the player. Final once the countdown has closed. */
struct CountdownProfile
{
    bool Final = false;
    CountdownPoints Preparation{};
    std::int32_t Composure = 0;
    std::int32_t Goodwill = 0;
    std::int32_t RouteDelayMinutes = 0;
    std::int32_t Honoured = 0;
    std::int32_t Shared = 0;
    std::int32_t Deferred = 0;
    std::int32_t Declined = 0;
    /** Preparation, composure and goodwill added: the number comparability is measured on, never shown as a grade. */
    std::int32_t Total = 0;

    friend bool operator==(const CountdownProfile&, const CountdownProfile&) = default;
};

[[nodiscard]] PERFECTWORLDCORE_API CountdownProfile CountdownProfileOf(const WorldState& world, const CountdownState& state);

/** Totals within CountdownComparableSpreadPermille of the larger. */
[[nodiscard]] PERFECTWORLDCORE_API bool CountdownProfilesComparable(const CountdownProfile& a, const CountdownProfile& b) noexcept;
/** The sum of absolute differences of the eight scores (six preparations, composure, goodwill). */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t CountdownProfileDistance(const CountdownProfile& a, const CountdownProfile& b) noexcept;
/** Distance at least CountdownDistinctDistance. */
[[nodiscard]] PERFECTWORLDCORE_API bool CountdownProfilesDiffer(const CountdownProfile& a, const CountdownProfile& b) noexcept;
/** Every one of the eight scores of `a` is at least `b`'s. */
[[nodiscard]] PERFECTWORLDCORE_API bool CountdownProfileDominates(const CountdownProfile& a, const CountdownProfile& b) noexcept;

// ---- Saving ------------------------------------------------------------------------

[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashCountdown(const CountdownState& state);

/**
 * Text form: header, seed, span, flags, accruals, then the commitments, obligations, requests and
 * ledger, one line each, then "end <hash>". Integers canonical; the same state always gives the same bytes.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseCountdown(const CountdownState& state);

/** Strict inverse: nothing for truncation at any byte, unknown fields or ids, odds that do not sum, a span that is not this build's, or a bad hash. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<CountdownState> ParseCountdown(std::string_view text);

/** The receipt as text: what was taken and what it cost, what was negotiated, the news, the profile so far. Stable. */
[[nodiscard]] PERFECTWORLDCORE_API std::string RenderCountdownReceipt(const CountdownState& state);
} // namespace PerfectWorld
