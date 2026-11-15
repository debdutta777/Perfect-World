#pragma once

#include "SchoolFollowupContent.h"
#include "TowerScenarioContent.h"

#include <array>
#include <span>

namespace PerfectWorld
{
/** P4-01: pure contract projections. Tower, TowerSession and TimeContract remain
 * the rule owners. No world, clock, save, reward or appointment is mutated here.
 * Six TowerMethods are examination methods; the ten AptitudePaths are credentials.
 */
inline constexpr std::uint32_t P401ExamContractVersion = 1;
/** Sit needs final authoritative preparation at/after examination morning.
 * Withdraw is available between floors and during an active floor. Defer and
 * Decline need no preparation; they are distinct exits before entry, not failures.
 * Annual re-entry and the age-twenty limit belong to later qualification rules.
 */
enum class P401ExamEntry : std::uint8_t { Sit, Withdraw, Defer, Decline };
struct P401ExamEntryRule
{
    P401ExamEntry Entry;
    TowerParticipation From;
    TowerSessionAction Action;
    TowerParticipation To;
};
struct P401ExamTransition
{
    TowerParticipation From;
    TowerSessionAction Action;
    TowerParticipation To;
};
struct P401ExamExitRule
{
    TowerParticipation Participation;
    /** Completed/ejected use SchoolFollowupOutcome's decile bands. */
    bool UsesRankBand = false;
    std::string_view Outcome;
};
[[nodiscard]] PERFECTWORLDCORE_API std::span<const P401ExamEntryRule> P401ExamEntries() noexcept;
/** Possible accepted transitions, not a replacement for ApplyTowerSession guards. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const P401ExamTransition> P401ExamTransitions() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::span<const P401ExamExitRule> P401ExamExits() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool P401ExamIsExit(TowerParticipation participation) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API GameInstant P401ExamMorning() noexcept;
/** Outside-time cap, not a mission-duration limit. Charge once on terminal exit;
 * active-floor elapsed time and frozen route delay count, reading/pause do not.
 */
[[nodiscard]] PERFECTWORLDCORE_API InteractionDeclaration P401ExamMissionClock();
struct P401ExamScoreDimensions
{
    std::array<TowerScenarioMetric, 2> Evidence;
    std::int32_t DisciplinePercent = TowerDisciplineSharePercent;
    std::int32_t JudgmentPercent = 100 - TowerDisciplineSharePercent;
    std::int32_t EvidenceCeiling = TowerScoreMaxMilli;
    std::int32_t DepthPerClear = TowerDepthPoints;
    std::int32_t DefeatPenalty = TowerDefeatPenaltyPoints;
    std::span<const TowerCredentialDefinition> Credentials;
};
[[nodiscard]] PERFECTWORLDCORE_API P401ExamScoreDimensions P401ExamScoring() noexcept;

/** Required persistence semantics for P4-02, not a codec implementation.
 * Persist the complete TowerSessionState after each accepted command: preparation,
 * nonces, active choice prefix, reserve, sums, elapsed time and completed attempts.
 * Resume is continuation, never Enter/OpenRoute again. No credit for unfinished work.
 * These are automatic recovery snapshots; TimeContract's mission manual-save rule
 * still applies. Finishing is the last accepted Choose, not a second Finish command.
 */
struct P401ExamResumePolicy
{
    bool AfterEveryAcceptedCommand = true;
    bool PreserveActiveFloor = true;
    bool PreserveFrozenPreparation = true;
    bool PreserveRunnerAndBeat = true;
    bool ReplayChoiceProvenance = true;
    bool PublishExitAndOutsideChargeTogether = true;
    bool TerminalStateIsImmutable = true;
};
[[nodiscard]] constexpr P401ExamResumePolicy P401ExamResumeRules() noexcept { return {}; }
struct P401ExamStepResult
{
    TowerSessionState State;
    TowerSessionResult Resolution;
};
/** Copy-in/copy-out adapter for the later atomic world transaction. On refusal
 * State equals input. All validation, evidence and time arithmetic stay in TowerSession.
 * Charge Resolution.OutsideMinutes only; Resolution.Result.OutsideMinutes is also
 * populated on nonterminal previews and must never move the world clock there.
 */
[[nodiscard]] PERFECTWORLDCORE_API P401ExamStepResult P401ApplyExamChoice(
    const TowerSessionState& state, const TowerRunnerCatalogue& runners,
    const TowerSessionCommand& command, const CountdownProfile& preparation, GameInstant now);

struct P401ExamRouteRow
{
    std::string_view RouteId; // static Tower identity
    std::string ScenarioId; // owned, independent of the input catalogue's lifetime
    std::int32_t Floor = 0;
    std::array<bool, TowerMethodCount> Methods{};
    std::uint32_t DisciplineMask = 0;
    bool NonCombatCompletes = false;
    bool CombatCompletes = false;
};
[[nodiscard]] PERFECTWORLDCORE_API std::vector<P401ExamRouteRow> P401ExamRouteMatrix(
    const TowerScenarioCatalogue& scenarios);
struct P401ExamValidation
{
    std::vector<ContentIssue> Issues;
    std::vector<P401ExamRouteRow> Routes;
    std::size_t DecisionSequences = 0;
    std::size_t RouteSequences = 0;
    bool NonCombatClimb = false;
    /** Authored scenario evaluation has no practice prerequisite. This is NOT
     * proof that the separate TowerRunnerCatalogue has a production content adapter.
     */
    bool ZeroPracticeContentViable = false;
    [[nodiscard]] bool Ok() const noexcept { return Issues.empty(); }
};
/** Requires full real tower_scenario and school_followup content. Delegates shape,
 * reference, scoring and outcome rules; explores EVERY local decision sequence
 * (including recovery), all route triples, partial exits, and all terminal bands.
 * The offer proof uses rank-independent offers, so it also covers mixed-quality
 * route triples not enumerated explicitly. Does no IO and never alters the Db.
 */
[[nodiscard]] PERFECTWORLDCORE_API P401ExamValidation P401ValidateExamContract(const ContentDb& db);
/** Separate runner-layer proof: actually completes every route/method from final
 * ZERO preparation via ApplyTowerSession. The caller supplies trusted runner content;
 * this function never invents a conversion from tower_scenario's different model.
 */
[[nodiscard]] PERFECTWORLDCORE_API SchoolIssue P401ValidateZeroPracticeRunner(
    const TowerRunnerCatalogue& runners);
} // namespace PerfectWorld
