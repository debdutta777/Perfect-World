#pragma once

#include "AwakeningInputs.h"
#include "Countdown.h"

#include <optional>
#include <span>

namespace PerfectWorld
{
/** R95: reducer adapters over the existing countdown, never a second clock.
 * Persist the optional CountdownState (its canonical text avoids a WorldState
 * include cycle) and a separate optional<AwakenedProfile> live practice owner.
 * The grant's StartingProfile is an immutable snapshot.
 * All inputs are authoritative reducer state, not presentation-supplied grants,
 * contacts, live profiles or alternative commitments. See R9-05_notes.md for wiring.
 */
struct WorldCountdownResult
{
    CountdownIssue Issue;
    WorldState World;
    std::optional<CountdownState> State;
    std::vector<DomainEvent> Events;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};

/** Late bootstrap before the fixed event onset; ordinary integration opens at
 * the scheduled boundary even while the tutorial is pending or an activity runs.
 * Past obligations are not invented or rewarded. No clock movement. Lower-level
 * reducer boundary entry: requires equal clocks, permits the transient running
 * day during RunTimeTo, and leaves its runner untouched. The enclosing reducer
 * transaction must publish a fully valid final day or roll back. External callers
 * use SynchroniseWorldCountdown, which requires a fully valid world snapshot.
 * Appends Gate events only on success; the enclosing command owns their sequence.
 * Does not submit nested commands or increment the world's command sequence.
 */
[[nodiscard]] PERFECTWORLDCORE_API CountdownOpenResult OpenCountdownAtCurrentTime(
    const WorldState& world, const CountdownContext& context, const DayLoopCatalogues& catalogues,
    std::vector<DomainEvent>* events = nullptr);

/** Settles clock-only news/close; refuses unplayed reserved blocks. No learning
 * credits and no padding. Routine plans use ResolveCountdownDays instead.
 */
[[nodiscard]] PERFECTWORLDCORE_API CountdownTakeResult ObserveCountdownTime(
    const WorldState& world, const CountdownState& state);

/** Call on valid snapshots at 18:00 and after authoritative time changes, in the same atomic
 * transaction. Opens independently of Granted; neither this nor viewing grants
 * Awakening. Reuses existing state without resetting its seed, offers or rolls.
 * A refusal returns the supplied world and optional state unchanged.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldCountdownResult SynchroniseWorldCountdown(
    const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const CountdownContext& context,
    const DayLoopCatalogues& catalogues);

/** Clock-only ordinary play through WorldSim, split at the scheduled opening.
 * Does not wait for evening on tutorial exit, force practice, select an exam
 * route, or extend the deadline. Reserved blocks require their explicit plan or
 * negotiation. Event-cap partial results refuse atomically; integrator time
 * paths can instead call SynchroniseWorldCountdown at each reached boundary.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldCountdownResult AdvanceWorldCountdown(
    const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const CountdownContext& context,
    const DayLoopCatalogues& catalogues, std::int64_t minutes);

struct WorldCountdownView
{
    CountdownIssue Issue;
    GameInstant OpeningAt = 0;
    GameInstant ExaminationAt = 0;
    std::int64_t RemainingMinutes = 0;
    std::int64_t MinutesUntilOpening = 0;
    bool Awakened = false;
    bool PreparationAvailable = false;
    bool ExaminationDue = false;
    bool OrdinaryDayAvailable = false;
};

/** Pure handoff/phone projection. Reading or declining practice consumes no time. */
[[nodiscard]] PERFECTWORLDCORE_API WorldCountdownView QueryWorldCountdown(
    const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state);

inline constexpr std::string_view WorldCountdownPracticeId = "activity.supervised_awakening_practice";

struct WorldCountdownPracticeOffer
{
    CountdownIssue Issue;
    std::uint64_t WorldSeq = 0;
    std::uint64_t CountdownHash = 0;
    GameInstant Start = 0;
    GameInstant End = 0;
    PowerDimension Dimension = PowerDimension::Coordination;
    std::int32_t Minutes = 0;
    std::string Supervisor;
    /** All overlaps, uncapped, through the receipt module's existing rules. */
    std::vector<ReceiptDisplaced> Displaced;
    /** Promises already owed/taken cannot silently be dropped by practising. */
    std::vector<std::string> BlockingCommitments;
    [[nodiscard]] bool Available() const noexcept { return Issue.Ok() && BlockingCommitments.empty(); }
};

/** Optional supervised session starting now. Duration is the authored/session
 * declaration, 1..TimeSceneMaxCostMinutes, contained in one calendar day and
 * ending no later than the original deadline. Teacher comes from trusted cast
 * context. Includes today's plan and reducer-supplied known promises/errands.
 * No RNG, grant mutation or progression; existing offers retain their IDs.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldCountdownPracticeOffer PreviewWorldCountdownPractice(
    const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const CountdownContext& context,
    const DayLoopCatalogues& catalogues, PowerDimension dimension, std::int32_t minutes,
    std::span<const ReceiptCandidate> knownAlternatives = {});

struct WorldCountdownPracticeResult
{
    WorldCountdownResult Transition;
    AwakenedProfile Profile;
    PracticeResult Practice;
    [[nodiscard]] bool Ok() const noexcept { return Transition.Ok(); }
};

/** Explicit confirmation only. Recomputes the disclosure and refuses a stale
 * preview. Charges both clocks through the reducer, records displaced intentions
 * in DayLoop.Receipts, and calls existing Practise on the live profile at quality
 * 100, supervised. Atomic refusal leaves world/countdown/live profile unchanged.
 * No executable ability, new rating formula or automatic grade promotion.
 */
[[nodiscard]] PERFECTWORLDCORE_API WorldCountdownPracticeResult RunWorldCountdownPractice(
    const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const AwakenedProfile& liveProfile,
    const CountdownContext& context, const DayLoopCatalogues& catalogues,
    const WorldCountdownPracticeOffer& accepted,
    std::span<const ReceiptCandidate> knownAlternatives = {});
} // namespace PerfectWorld
