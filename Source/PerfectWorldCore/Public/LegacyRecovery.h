#pragma once

#include "WorldState.h"

#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * P3-14: compatibility recovery, so no legacy life is silently stranded (PHASE4_PLAN section 1, risks 1 and 2).
 *
 * A life saved before schema 12 loads unseeded, and a seeded life driven past 18:00 on Awakening Day by a sim without the
 * countdown context holds no countdown. One System command, awakening.recover (Commands.h AwakeningRecover), repairs either
 * once, from what the slot really holds. Its plan is a pure function of the world (PlanAwakeningRecovery), so it is inert on
 * an already recovered world (refused "awakening.recovery_not_needed", nothing changes) and across reloads.
 *
 *  - Unseeded: seed exactly as creation confirmation does (SeedLearningInputs over the slot's own creation record, the
 *    world's seed), plus the projection evidence of the lessons the school slice really recorded as resolved (the same rule
 *    and id the lesson close uses, "school.lesson.<instance>"). Nothing else is reconstructed: skill XP of past completions
 *    was never stored, attendance is never Awakening evidence (R9-01), and no conduct, session or grant is invented.
 *  - Before the last legal session start (exam - 61 minutes, so the hour completes strictly before 2 Feb 2043 06:00): a
 *    missed appointment or make-up slot is marked absent, which opens the supervised make-up path (TutorialMakeUp). The
 *    appointment, the deadline and the preparation span never move.
 *  - Past that instant with no session: the explicit legacy compatibility mode (flag AwakeningPostDeadlineFlag). The life
 *    continues without Awakening, its slot preserved; presentation says so and offers a new life.
 *  - The countdown (R9-05's late-bootstrap question, decided here): this command is the ONLY late bootstrap. Between the
 *    18:00 opening and the Gate onset it opens the countdown at the current instant (OpenCountdownAtCurrentTime: commitments
 *    already begun are omitted, never invented or rewarded). After the onset a past Gate break cannot be invented, so the
 *    life is marked AwakeningCountdownUnavailableFlag instead. Bound sims still never bootstrap on their own.
 *
 * The markers are entries of the existing, saved and hashed world flags (no new section, schema or hash version). The
 * "awakening.compat." prefix is reserved: SetWorldFlag refuses it, and the save rules authenticate each marker against the
 * world (AreAwakeningCompatFlagsValid).
 */
inline constexpr std::string_view AwakeningCompatFlagPrefix = "awakening.compat.";
/** Set when awakening.recover seeded this life from its creation record (not at creation confirmation). */
inline constexpr std::string_view AwakeningSeededOnRecoveryFlag = "awakening.compat.seeded_on_recovery";
/** Legacy compatibility mode: no supervised session can start before the fixed examination any more. */
inline constexpr std::string_view AwakeningPostDeadlineFlag = "awakening.compat.post_deadline";
/** The countdown was missing and its Gate onset had already passed: the ten days cannot be reconstructed. */
inline constexpr std::string_view AwakeningCountdownUnavailableFlag = "awakening.compat.countdown_unavailable";

/** The last instant a supervised session may start: its fixed hour must end strictly before the examination. */
[[nodiscard]] PERFECTWORLDCORE_API GameInstant AwakeningLastSessionStart(const AwakeningState& awakening) noexcept;

/** What awakening.recover would do to this world now. Pure; no content, clock or binding is read. */
struct AwakeningRecoveryPlan
{
    bool Seed = false;
    bool OpenMakeUp = false;
    bool MarkPostDeadline = false;
    bool OpenCountdown = false;
    bool MarkCountdownUnavailable = false;
    [[nodiscard]] bool Needed() const noexcept
    {
        return Seed || OpenMakeUp || MarkPostDeadline || OpenCountdown || MarkCountdownUnavailable;
    }
};

[[nodiscard]] PERFECTWORLDCORE_API AwakeningRecoveryPlan PlanAwakeningRecovery(const WorldState& world);

/** Presentation projection: what to tell the player about this life's Awakening compatibility. Pure. */
struct AwakeningCompatibilityView
{
    /** Non-empty plan: submit awakening.recover (System) before any other command. */
    AwakeningRecoveryPlan Pending;
    bool SeededOnRecovery = false;
    /** Legacy compatibility mode: Awakening is no longer reachable; offer a new life, keep this slot. */
    bool PostDeadline = false;
    bool CountdownUnavailable = false;
    /** A supervised make-up is owed: schedule one (TutorialMakeUp) no later than LastSessionStart. */
    bool MakeUpRequired = false;
    GameInstant LastSessionStart = 0;
    GameInstant ExaminationAt = 0;
};

[[nodiscard]] PERFECTWORLDCORE_API AwakeningCompatibilityView QueryAwakeningCompatibility(const WorldState& world);

/** True for any id under the reserved "awakening.compat." prefix. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsAwakeningCompatFlag(std::string_view flagId) noexcept;

/**
 * Save rule: every reserved flag is one of the three markers, set true, and holds in this world (seeded_on_recovery: the
 * life is seeded; post_deadline: seeded, not granted, no session, past the last session start; countdown_unavailable:
 * seeded, no stored countdown, at or after the Gate onset). True when there are none.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool AreAwakeningCompatFlagsValid(const WorldState& world);
} // namespace PerfectWorld
