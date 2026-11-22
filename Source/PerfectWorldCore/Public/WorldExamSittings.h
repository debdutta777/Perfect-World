#pragma once

#include "WorldExam.h"

#include <cstdint>
#include <optional>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * P4-09: the examination's remaining rules, so no life is stranded around exam day.
 *
 * P413: GAME_PLAN.md §10, owner decision 2026-11-22 supersedes annual retakes: one sitting per life, one deferral to the
 * immediately following annual sitting, still at most twenty. Enter, Decline and an awakened candidate's missed window use
 * the chance. The saved Awakening grant instant determines the first sitting at whose opening the candidate was awakened.
 * No grant by an opening preserves eligibility for a later sitting after Awakening; it does not invent a grant.
 *
 * Sittings. Sitting k opens at 2 February (2043 + k) 06:00 (sitting 0 is CountdownEndInstant()). WorldState::Examination
 * belongs to sitting ExaminationEarlier.size(). A decision can archive only a single deferral or sittings before Awakening,
 * then applies to a fresh session. Legacy histories still load byte-for-byte, but a used chance anywhere bars further entry.
 *
 * Age limit. Sitting k is open to a candidate aged at most twenty (AgeOnInstant) at its opening. The date of birth is a
 * binding (P402WorldExamCatalogues::P409Birth, from the life's creation record, bound on new life, Continue and replay),
 * not saved world state; unbound, the earliest legal birth (3 June 2025) is assumed, which every life satisfies through
 * 2046. Refusal: exam.age_limit.
 *
 * Late arrival. Enter is accepted from the opening until P409LateEntryMinutes after it; after that an awakened candidate's
 * chance is used (exam.sitting_used). The hall's deadline does not move: the examination's outside-world footprint ends at most
 * TowerWorldCapMinutes after the opening. A candidate who entered L minutes late has the reduced allowance
 * TowerWorldCapMinutes - L outside minutes: when a step would exceed that allowance, including a completing or ejecting
 * Choose, the same transaction withdraws it ("time called"). A floor-finishing choice is discarded; only earlier completed
 * floors earn credit, and the remaining allowance is spent at the hall deadline. Every other exit charges at most the
 * allowance. L is the clock minus the opening, exact while Entered because the clock stands still then, so nothing new is
 * stored. For an on-time candidate the pending minutes are already capped at TowerWorldCapMinutes, so P4-01's rule (the cap
 * is not a mission-duration limit) is unchanged.
 */
inline constexpr std::int32_t P409FirstSittingYear = 2043;
inline constexpr std::int64_t P409LateEntryMinutes = 60;
inline constexpr std::int64_t P409AgeLimitYears = 20;
inline constexpr std::int32_t P413SittingsPerLife = 1;
inline constexpr std::int32_t P413DeferralsPerLife = 1;
/** The last sitting any legal date of birth reaches (2 February 2047, for a birth on 2 June 2026): the codec's bound. */
inline constexpr std::uint32_t P409MaxSitting = 4;

/** 2 February (2043 + sitting) 06:00. */
[[nodiscard]] PERFECTWORLDCORE_API GameInstant P409SittingAt(std::uint32_t sitting) noexcept;
/** The latest sitting whose opening is at or before `now` (capped at P409MaxSitting + 1); empty before the first. */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<std::uint32_t> P409SittingOpenedBy(GameInstant now) noexcept;
/** True when a candidate born on `birth` (or on EarliestDateOfBirth when null) is at most twenty at the sitting's opening. */
[[nodiscard]] PERFECTWORLDCORE_API bool P409SittingAllowsAge(std::uint32_t sitting, const DateOfBirth* birth) noexcept;
/** The sitting WorldState::Examination belongs to. */
[[nodiscard]] PERFECTWORLDCORE_API std::uint32_t P409CurrentSitting(const WorldState& world) noexcept;
/** While Entered: minutes after the current sitting opened that the candidate entered (the standing clock). Else 0. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t P409EntryLateness(const WorldState& world) noexcept;
/** While Entered: the reduced outside-world allowance, TowerWorldCapMinutes - P409EntryLateness. Else TowerWorldCapMinutes. */
[[nodiscard]] PERFECTWORLDCORE_API std::int64_t P409OutsideAllowance(const WorldState& world) noexcept;
/**
 * The preparation a Sit at `sitting` freezes: P402FinalPreparation (the closed ten days, or the P3-14 zero snapshot), with
 * no route delay after the first sitting (the delay was a world event of the ten days before 2 February 2043).
 */
[[nodiscard]] PERFECTWORLDCORE_API P402ExamPreparation P409SittingPreparation(const WorldState& world, std::uint32_t sitting);
/**
 * P413: the sole remaining sitting, before applying the age limit; empty after use, a missed awakened window, or a legacy
 * history that already spent the chance. Pure, derived from the saved sessions and Awakening.Granted->At, never flags.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<std::uint32_t> P413AvailableSitting(const WorldState& world, GameInstant now) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool P413DeferralUsed(const WorldState& world) noexcept;
/** The available sitting once it opens, otherwise the stored sitting. Never advances a used candidate. */
[[nodiscard]] PERFECTWORLDCORE_API std::uint32_t P409DecisionTarget(const WorldState& world, GameInstant now) noexcept;
/** Archive only toward the world's available sitting; a used chance cannot create fresh sessions. */
PERFECTWORLDCORE_API void P409ArchiveUntil(const WorldState& world, std::vector<TowerSessionState>& earlier, TowerSessionState& session,
    std::uint32_t target);
/**
 * Content-free validity of one stored session at `sitting` (the save rules). `current` selects the live-session rules (the
 * clock, the entry window, the allowance, composure); an earlier sitting must be closed or Unset (absent).
 */
[[nodiscard]] PERFECTWORLDCORE_API bool P409SessionValidInWorld(const WorldState& world, const TowerSessionState& session,
    std::uint32_t sitting, bool current);
} // namespace PerfectWorld
