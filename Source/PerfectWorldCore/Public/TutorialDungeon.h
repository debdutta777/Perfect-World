#pragma once

#include "AwakeningInputs.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld
{
// R92 frozen IDs/API for R9-03/04/05/06/08. No UI, movement, RNG or wall clock.
constexpr std::size_t TutorialSituationCount = 3;
constexpr std::int32_t TutorialConductStakes = 2;
constexpr GameInstant TutorialSessionMinutes = 60;
inline constexpr std::string_view TutorialSafeStationId = "tutorial.station.safe_area";
inline constexpr std::string_view TutorialGrantStationId = "tutorial.station.awakening";

struct TutorialChoiceDefinition
{
    std::string_view Id;
    ConductAction Action;
    /** Stable observable outcome key for authored text/presentation. */
    std::string_view ConsequenceId;
};

struct TutorialSituationDefinition
{
    /** Also the ConductRecord::TestId; never substitute the station ID. */
    std::string_view Id;
    std::string_view StationId;
    std::array<TutorialChoiceDefinition, 3> Choices;
    std::string_view WithdrawalChoiceId;
};

[[nodiscard]] PERFECTWORLDCORE_API std::span<const TutorialSituationDefinition> TutorialSituations();

enum class TutorialDungeonPhase : std::uint8_t
{
    NotStarted = 0,
    Active,
    ReadyToGrant,
    Completed
};

/** Persist/hash every field. No absence is represented as a played session.
 * NextSituation is a completed prefix (0..3); withdrawal records only the
 * current situation, never invented choices for unseen situations. On withdrawal
 * NextSituation includes that situation, and Withdrawn disambiguates the prefix.
 * RecoveryPending adds no conduct or penalties; no failure state exists.
 */
struct TutorialDungeonState
{
    TutorialDungeonPhase Phase = TutorialDungeonPhase::NotStarted;
    std::uint8_t NextSituation = 0;
    bool RecoveryPending = false;
    bool Withdrawn = false;
    std::optional<GameInstant> StartedAt;
    std::optional<GameInstant> LastActionAt;
    std::optional<GameInstant> CompletedAt;

    friend bool operator==(const TutorialDungeonState&, const TutorialDungeonState&) = default;
};

struct TutorialDungeonResult
{
    /** Stable tutorial.* rejection code; every refusal changes nothing. */
    std::string Error;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Cross-slice structural validation for codec/integrator, not save authentication.
 * Validates the tutorial's exact conduct prefix, make-up, times and grant presence.
 * The R9-04 codec must additionally authenticate the latent owner and grant data.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsTutorialDungeonStateValid(
    const TutorialDungeonState& tutorial, const AwakeningState& awakening);

/** Absence requires make-up, never grants. May mark a missed make-up absent too. */
PERFECTWORLDCORE_API TutorialDungeonResult MarkTutorialAbsence(
    TutorialDungeonState& tutorial, AwakeningState& awakening, GameInstant now);
/** Explicit supervised slot, at >= now, enough room for the entire hour BEFORE
 * the fixed exam. Original appointment and examination instants never change.
 */
PERFECTWORLDCORE_API TutorialDungeonResult ScheduleTutorialMakeUp(
    TutorialDungeonState& tutorial, AwakeningState& awakening, GameInstant at, GameInstant now);
/** Authority confirms supervision/campus access before calling. Enter at the
 * appointment instant (original or make-up). Late arrivals use make-up scheduling.
 * Disclose TutorialSessionMinutes before entry. Attendance alone grants nothing.
 */
PERFECTWORLDCORE_API TutorialDungeonResult BeginTutorialDungeon(
    TutorialDungeonState& tutorial, const AwakeningState& awakening, GameInstant now);
/** A COMPLETED contextual action, not a dialogue selection/animation callback.
 * Match current station and one authored choice. Integrator supplies actual
 * involved identity IDs from authoritative cast bindings, never player payload.
 * RecordConduct adds equal-stakes evidence and progress atomically. No precision,
 * duration, camera, accessibility setting or frame count is accepted as input.
 */
PERFECTWORLDCORE_API TutorialDungeonResult CompleteTutorialChoice(
    TutorialDungeonState& tutorial, AwakeningState& awakening,
    std::string_view stationId, std::string_view choiceId, GameInstant now,
    const std::vector<std::string>& parties = {});
/** Mistake/obstruction -> assistance -> retry same situation, without evidence.
 * Withdrawal remains available during assistance. No recovery count is scored.
 */
PERFECTWORLDCORE_API TutorialDungeonResult RequestTutorialAssistance(
    TutorialDungeonState& tutorial, const AwakeningState& awakening, GameInstant now);
PERFECTWORLDCORE_API TutorialDungeonResult RecoverTutorialDungeon(
    TutorialDungeonState& tutorial, const AwakeningState& awakening, GameInstant now);
/** Immediately returns to supervised safety and becomes eligible for grant.
 * Also allowed after all three situations; then no extra conduct is invented.
 * Still costs the disclosed session hour. Calls grant separately in same world
 * transaction as mission exit/time charging, including on withdrawal.
 */
PERFECTWORLDCORE_API TutorialDungeonResult WithdrawTutorialDungeon(
    TutorialDungeonState& tutorial, AwakeningState& awakening, GameInstant now);
/** Only supervised ReadyToGrant, at >= StartedAt + fixed hour and < exam.
 * Calls ResolveAwakening once, then AwakenedStartingProfile, and commits grant,
 * tutorial completion and make-up completion together. Failure commits nothing.
 * A stored Granted rejects BEFORE resolution; never reroll on load/result view.
 * The enclosing reducer must charge/synchronise world time in the same transaction.
 */
PERFECTWORLDCORE_API TutorialDungeonResult GrantTutorialAwakening(
    TutorialDungeonState& tutorial, AwakeningState& awakening,
    const AbilityRecipeSet& recipes, GameInstant now);
} // namespace PerfectWorld
