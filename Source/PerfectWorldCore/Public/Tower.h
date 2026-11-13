#pragma once

#include "AptitudeProjection.h"
#include "TimeContract.h"

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/** M4-04, AWAKENING_AND_EXAM "The Tower Examination", SYSTEMS_CATALOGUE "Tower".
 * Standalone resolution of an exam receipt, not a world reducer or a save format.
 * Disciplines are the existing ten AptitudePath identities; floor types are methods.
 * Performance and common judgment are supplied by later floor runners (M4-10..12), not
 * inferred from Tattva, capability or conduct (that would reward the awakening twice).
 * Common judgment must contain no combat evidence: the caller owns that contract.
 * Authored tables are constexpr; no variation or RNG is needed for this slice.
 *
 * VOCABULARY. TowerMethod is, in order and by id, the six floor types of the document and
 * the six preparation dimensions of the ten days (Countdown.h, CountdownDimension: combat,
 * analysis, endurance, construction, negotiation, logistics). The two are therefore the
 * same enumeration: a preparation score at index i is preparation for the method at
 * index i, and the integration may static_cast between them (TowerMethodFromId maps the
 * countdown's names). The countdown's credit mapping (day-loop skill to dimension) stays
 * the countdown's; the tower does not re-map skills, it only receives runner metrics.
 * TODO integration: how preparation and the countdown's route delay shape a runner's
 * starting conditions is a runner decision (M4-09..12); nothing here reads either.
 *
 * Chosen tuning (not prescribed by the docs): three ordered floors, 1000 depth points plus
 * 0..1000 quality per clear (ceiling 6000), a 500 point loss on terminal defeat, best
 * completed evidence per discipline, and conservative cohort ties. The score is signed so
 * that a first-floor defeat is still worse than withdrawing before it: defeat costs rank.
 * Every route shares one ceiling; combat on the encounter route is no richer than
 * endurance. A cleared route supplies evidence for every discipline in its authored mask;
 * zero-quality completion is observed evidence, distinct from no credential.
 * Support values are provisional application envelopes, not money awarded.
 * The 80:20 discipline/common judgment split is prescribed by SYSTEMS_CATALOGUE.
 * TODO M4-10..12: validate runner metrics against their playable performance envelopes.
 */
constexpr std::int32_t TowerFloorCount = 3;
constexpr std::int32_t TowerScoreMaxMilli = 1000;
constexpr std::int32_t TowerDisciplineSharePercent = 80;
constexpr std::int32_t TowerDepthPoints = 1000;
constexpr std::int32_t TowerDefeatPenaltyPoints = 500;
constexpr std::int32_t TowerCohortCount = 1000; // includes the player
/** The synthetic cohort spans the possible scores, a defeated first floor to a perfect climb. */
constexpr std::int32_t TowerCohortMinPoints = -TowerDefeatPenaltyPoints;
constexpr std::int32_t TowerWorldCapMinutes = 180;
constexpr std::int32_t TowerWorldRatePermille = 250;

enum class TowerMethod : std::uint8_t
{
    Combat, Analysis, Endurance, Construction, Negotiation, Logistics
};
constexpr std::size_t TowerMethodCount = 6;

struct TowerFloorDefinition
{
    std::string_view Id;
    std::int32_t Number = 0;
};

/** Each of three routes per floor supports a non-combat method. Encounter also
 * allows combat; choosing endurance earns the identical ceiling and credentials.
 */
struct TowerRouteDefinition
{
    std::string_view Id;
    std::int32_t Floor = 0;
    TowerMethod Method = TowerMethod::Analysis;
    bool AllowsCombat = false;
    std::uint32_t DisciplineMask = 0;
    std::string_view Task;
};

struct TowerCredentialDefinition
{
    std::string_view Id;
    AptitudePath Discipline = AptitudePath::Scholar;
};

struct TowerOfferDefinition
{
    std::string_view Id;
    std::string_view NextAction;
    /** 1 = top decile; 10 = any result, including no floors attempted. */
    std::int32_t MaxDecile = 10;
    /** One support envelope, the maximum qualifying allowance, never summed. */
    std::int64_t SupportPaise = 0;
    /** When set, the portfolio must hold this discipline's credential (reachable without combat). */
    bool NeedsCredential = false;
    AptitudePath Discipline = AptitudePath::Hunter;
};

[[nodiscard]] PERFECTWORLDCORE_API std::span<const TowerFloorDefinition> TowerFloors() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::span<const TowerRouteDefinition> TowerRoutes() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::span<const TowerCredentialDefinition> TowerCredentials() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::span<const TowerOfferDefinition> TowerOffers() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const TowerRouteDefinition* FindTowerRoute(std::string_view id) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool TowerRouteCovers(const TowerRouteDefinition& route, AptitudePath discipline) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view TowerMethodId(TowerMethod method) noexcept;
/** "combat" ... "logistics" (the countdown's dimension names). Unknown ids are refused and `out` is untouched. */
[[nodiscard]] PERFECTWORLDCORE_API bool TowerMethodFromId(std::string_view id, TowerMethod& out) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API InteractionDeclaration TowerMissionDeclaration();

enum class TowerError : std::uint8_t
{
    None, UnknownRoute, UnknownDiscipline, UnknownMethod, MethodNotAllowed,
    InvalidScore, InvalidMinutes, FloorOutOfOrder, AfterDefeat, InvalidClock
};
[[nodiscard]] PERFECTWORLDCORE_API std::string_view TowerErrorCode(TowerError error) noexcept;

struct TowerFloorAttempt
{
    std::string_view RouteId;
    TowerMethod Method = TowerMethod::Analysis;
    std::int32_t DisciplineMilli = 0;
    /** Non-combat judgment only, e.g. supply stewardship and team communication. */
    std::int32_t CommonJudgmentMilli = 0;
    std::int64_t MissionMinutes = 0;
    bool Defeated = false;
    friend bool operator==(const TowerFloorAttempt&, const TowerFloorAttempt&) = default;
};

struct TowerCredential
{
    std::string_view Id;
    AptitudePath Discipline = AptitudePath::Scholar;
    /** Best completed evidence: floor(80% discipline + 20% common judgment). */
    std::int32_t ScoreMilli = 0;
    std::string_view RouteId;
    friend bool operator==(const TowerCredential&, const TowerCredential&) = default;
};

struct TowerResult
{
    TowerError Error = TowerError::None;
    std::int32_t FloorsCleared = 0;
    bool Ejected = false;
    /** All floors cleared. */
    bool Completed = false;
    /** The receipt ended early without a defeat: declined, deferred or withdrawn. Score keeps what was earned. */
    bool Withdrew = false;
    /** May be negative: a defeat costs more than a shallow climb earned. */
    std::int32_t ScorePoints = 0;
    std::int32_t NationalRank = 0;
    std::int32_t Decile = 0; // 1 is top; 10 is bottom
    std::int64_t MissionMinutes = 0;
    std::int64_t OutsideMinutes = 0;
    std::int64_t SupportEnvelopePaise = 0;
    std::vector<TowerCredential> Credentials;
    std::vector<std::string_view> OfferIds;
    [[nodiscard]] bool Ok() const noexcept { return Error == TowerError::None; }
    friend bool operator==(const TowerResult&, const TowerResult&) = default;
};

/** Ordered prefix of floors 1..3; empty/partial receipts mean declined/withdrawn.
 * Defeat is terminal but preserves earlier credentials and always yields offers (at least
 * three at any decile; the bottom decile's are rank-independent).
 * Refusals return only Error, with no partial result. No death/injury state exists.
 */
[[nodiscard]] PERFECTWORLDCORE_API TowerResult ResolveTower(const std::vector<TowerFloorAttempt>& attempts);
/** Refuses IDs outside AptitudePathId's existing registry. */
[[nodiscard]] PERFECTWORLDCORE_API TowerError TowerDisciplineFromId(std::string_view id, AptitudePath& out) noexcept;
} // namespace PerfectWorld
