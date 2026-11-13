#pragma once

#include "Countdown.h"

#include <cstdint>
#include <string>
#include <string_view>

// Shared by CountdownRules.cpp (odds, rolls, relationships), CountdownProfile.cpp, CountdownOffers.cpp
// (disclosure, committing, negotiating), CountdownRun.cpp (opening and resolving) and CountdownText.cpp.
namespace PerfectWorld
{
/** A refusal: the error and a detail. */
[[nodiscard]] CountdownIssue CountdownMakeIssue(CountdownError error, std::string detail = {});

/** The person in `role` from the context, or empty. */
[[nodiscard]] std::string CountdownContactFor(const CountdownContext& context, std::string_view role);

/** The relationship dimension between the player and `person` (0 with no book or no pair); strangers read 0. */
[[nodiscard]] std::int32_t CountdownRelationValue(const CountdownContext& context, std::string_view person, CountdownRelation relation);

/** The odds shown for an opportunity given the contact's relationship value. Sums to 1000. */
[[nodiscard]] CountdownOdds CountdownOddsFor(const CountdownOpportunityDef& def, std::int32_t relation);

/** base plus the relationship skew, clamped to 0..1000 (permille). */
[[nodiscard]] std::int32_t CountdownSkewedPermille(std::int32_t base, std::int32_t step, std::int32_t cap, std::int32_t relation);

/** The seeds drawn when something is committed: keyed draws from the state's stored seed. */
[[nodiscard]] std::uint64_t CountdownCommitSeed(std::uint64_t stateSeed, std::string_view id);
[[nodiscard]] std::uint64_t CountdownNegotiationSeed(std::uint64_t stateSeed, std::string_view id, CountdownProposal proposal);
[[nodiscard]] std::uint64_t CountdownOpenSeed(std::uint64_t worldSeed, GameInstant start);

/** The draw 0..999 a stored seed gives, and the outcome it lands on under stored odds. */
[[nodiscard]] std::int32_t CountdownRoll(std::uint64_t seed);
[[nodiscard]] CountdownOutcome CountdownOutcomeFor(const CountdownOdds& odds, std::int32_t roll);

/** Adds an effect's points to the accruals. */
void CountdownApplyEffect(CountdownState& state, const CountdownEffect& effect);

void CountdownAddLedger(CountdownState& state, GameInstant at, CountdownLedgerKind kind, std::string id, std::int32_t a = 0,
                        std::int32_t b = 0);

/** The skill credits of a resolved stretch mapped to preparation points. */
[[nodiscard]] CountdownPoints CountdownMapCredits(const std::vector<SkillCredit>& credits);

/** The minute of the day an instant falls on. */
[[nodiscard]] std::int32_t CountdownMinuteOfDay(GameInstant instant);

/** True when [startA, endA) and [startB, endB) share a minute. */
[[nodiscard]] bool CountdownOverlaps(GameInstant startA, GameInstant endA, GameInstant startB, GameInstant endB);

/** Checks a world is fit for the countdown's commands: in step, awake, idle. */
[[nodiscard]] CountdownIssue CountdownCheckWorld(const WorldState& world);

/** The delay a state implies given the logistics preparation (before the clamp to 0). */
[[nodiscard]] std::int32_t CountdownLogisticsRelief(const CountdownState& state);
} // namespace PerfectWorld
