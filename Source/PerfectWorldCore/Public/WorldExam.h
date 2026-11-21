#pragma once

#include "Countdown.h"
#include "ExamResult.h"
#include "TowerSession.h"
#include "WorldState.h"

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
 * P4-02: the national tower examination of 2 February 2043 06:00 as authoritative world state (WorldState::Examination).
 *
 * One command, P4-01's frozen TowerSessionCommand ("tower.session.apply"), runs every step as an atomic WorldSim transaction
 * through P401ApplyExamChoice: Enter (sit), OpenRoute (choose a route), Choose (act), Withdraw, Defer and Decline. There is no
 * separate Finish or Checkpoint command (P4-01): the accepted Choose that clears floor 3 or exhausts the reserve finishes, and
 * every accepted command is itself the recovery checkpoint, saved whole. Refusals carry the contract's "tower.session.*" codes;
 * the world adds only these "exam.*" codes:
 *   exam.not_open          before the fixed instant, for every entry choice;
 *   exam.missed            Enter after the fixed instant (late arrival belongs to a later qualification order);
 *   exam.decision_required a time path starting at the instant in a routed, awakened, undecided life;
 *   exam.active            any unrelated command while Entered (the tutorial.active precedent);
 *   exam.not_awakened      Enter without the compulsory Awakening grant (legacy post-deadline lives);
 *   exam.no_preparation    Enter without a closed countdown (and not a P3-14 countdown_unavailable life);
 *   exam.missing_content   Enter/OpenRoute/Choose with no runner catalogue bound (Withdraw/Defer/Decline need none);
 *   exam.invalid_state     the world or the stored session disagrees with the bound runners;
 *   exam.clock_cap         a terminal charge outside 0..TowerWorldCapMinutes (defensive);
 *   exam.charge_interrupted the terminal charge did not complete (the whole transaction is refused).
 * The world clock does not move while Entered: the mission's outside-world cost (TimeContract, capped at 180 minutes for the
 * whole examination) is charged once, through ChargeTime, by the accepted terminal command, in the same transaction that
 * publishes the terminal participation. A terminal session is immutable, so reload and replay cannot charge twice.
 */
struct P402WorldExamCatalogues
{
    /** Immutable, caller-owned runner content (ValidateTowerRunnerCatalogue), alive as long as the sim; bind on Continue too. */
    const TowerRunnerCatalogue* Runners = nullptr;
    /** P405: validated school_followup content; caller-owned and bound on Continue/replay as well. */
    const SchoolFollowupContent* Followups = nullptr;
};

/** The final preparation a Sit freezes, read from world authority only (never from presentation). */
struct P402ExamPreparation
{
    SchoolIssue Issue;
    CountdownProfile Profile;
    /** True for the explicit P3-14 policy: a granted life whose countdown was unavailable sits with zero preparation. */
    bool ZeroByCompatibility = false;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
/**
 * The closed stored countdown's CountdownProfileOf (exam.no_preparation when there is none or it has not closed), or, for a
 * granted life marked AwakeningCountdownUnavailableFlag, the explicit zero snapshot (Final, all zeros: reserve 4, discount 0,
 * the zero-practice runner P4-01 proves completable). Requires the grant (exam.not_awakened).
 */
[[nodiscard]] PERFECTWORLDCORE_API P402ExamPreparation P402FinalPreparation(const WorldState& world);

/** Presentation projection over the stored session. Pure: reading costs no time. Codes, never prose. */
struct P402ExamWorldView
{
    GameInstant ExaminationAt = 0;
    /** The undecided routed life stands at the instant: Sit, Defer or Decline before time can move on. */
    bool DecisionRequired = false;
    bool CanSit = false;
    /** Why Sit is unavailable now (empty when CanSit): one of the exam.* or tower.session.* codes. */
    std::string SitRefusal;
    bool CanDefer = false;
    bool CanDecline = false;
    /** Always true while Entered: withdrawal is the guaranteed exit. */
    bool CanWithdraw = false;
    bool Finished = false;
    /** While Entered: what withdrawing now would charge. Terminal: what the exit charged. Never authority to charge. */
    std::int64_t OutsideMinutes = 0;
    TowerSessionView Session;
};

/**
 * P4-03 SEAM. The one place a finished examination's result is produced. WorldSim calls it inside the terminal
 * transaction (a refusal refuses that whole transaction, so no exit is published without a derivable result) and
 * WorldSim::ExaminationResult() calls it for reads after Continue. Per P4-01 and P4-03 NOTHING it returns is stored:
 * the terminal TowerSessionState is the only authority. P405 uses P403ResolveExamResult for bound reports.
 * Without followups, the terminal transaction and save validation retain the content-free score projection:
 * missing appointments must never trap a coherent Withdraw or invalidate an otherwise valid save. Such a
 * projection has no next steps; bind Followups for the complete presentation report. An explicitly supplied
 * incomplete catalogue reports P403.exam.missing_followup, without affecting the accepted exit.
 */
using P402ExamFinish = P403ExamResult;
[[nodiscard]] PERFECTWORLDCORE_API P402ExamFinish P402ExamFinishSeam(const TowerSessionState& terminal,
    const SchoolFollowupContent* P405Followups = nullptr);

/** True for any participation after the session closed (Completed, Ejected, Withdrawn, Deferred, Declined). */
[[nodiscard]] PERFECTWORLDCORE_API bool P402ExamClosed(TowerParticipation participation) noexcept;

/**
 * Content check (the save reader has no runners): rebuilds the whole session from Unset through ApplyTowerSession with
 * the stored frozen preparation, every attempt's and the active floor's choice provenance and a final Withdraw, and
 * compares the result with `state` field for field. Unset, Deferred and Declined must equal their empty form.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool ExamStateMatchesRunners(const TowerSessionState& state, const TowerRunnerCatalogue& runners);

/**
 * The content-free rules the examination keeps inside `world` (save write and read): canonical empty form when Unset;
 * Deferred/Declined empty and at/after the instant; a sat session only with the grant, preparation in bounds and equal to
 * P402FinalPreparation (composure exactly only while Entered, the clock standing still), the clock exactly at the instant
 * while Entered and at least instant + charged minutes once finished; runner nonces counting the routes opened; attempts in
 * floor order on allowed methods, a defeat only last and only when Ejected; an active floor only while Entered or Withdrawn.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsExamStateValidInWorld(const WorldState& world);
/**
 * Canonical pw.examination/1 text; empty when IsExamStateValidInWorld fails. Lines ending "\n", numbers canonical decimal,
 * enums by declared ordinal, IDs IsReceiptIdWellFormed, a ';'-joined choice list one field:
 *   pw.examination/1, participation=<p>, preparation=<six>,<composure>,<routeDelay>, runner=<next>,<runner>, attempts=<n>
 *   then n attempt=<route>,<method>,<discipline>,<judgment>,<minutes>,<defeated>,<scenario>,<choices>,
 *   active=none | active=<scenario>,<route>,<method>,<beat>,<reserve>,<discount>,<disciplineSum>,<judgmentSum>,<minutes>,
 *   <choices>, end.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseExamState(const WorldState& world);
/** Strict canonical parse into world.Examination, validated in `world`; false and untouched otherwise. */
[[nodiscard]] PERFECTWORLDCORE_API bool DeserialiseExamState(std::string_view text, WorldState& world);
} // namespace PerfectWorld
