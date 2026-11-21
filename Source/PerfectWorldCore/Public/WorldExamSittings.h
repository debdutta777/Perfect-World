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
 * Canon (GAME_PLAN.md §1, owner decision 2026-09-25; AWAKENING_AND_EXAM.md open questions, answered 2026-09-23;
 * PATHS_AND_PROGRESSION.md): the examination is annual and repeatable up to age twenty, and may be deferred to a later year
 * or declined. Canon is silent on late arrival and on the date of later sittings; the humane choices below are documented in
 * the P4-09 notes and here.
 *
 * Sittings. Sitting k opens at 2 February (2043 + k) 06:00 (sitting 0 is CountdownEndInstant()). WorldState::Examination
 * belongs to sitting ExaminationEarlier.size(). An accepted Enter, Defer or Decline whose target sitting (the latest one
 * opened by now) is later than the current one, while the current session is Unset or closed, first archives the current
 * session (and an Unset "absent" record for every sitting skipped) and then applies to a fresh session: a deferred,
 * declined, poor, ejected or withdrawn candidate, or one who missed a morning, sits again the next year.
 *
 * Age limit. Sitting k is open to a candidate aged at most twenty (AgeOnInstant) at its opening. The date of birth is a
 * binding (P402WorldExamCatalogues::P409Birth, from the life's creation record, bound on new life, Continue and replay),
 * not saved world state; unbound, the earliest legal birth (3 June 2025) is assumed, which every life satisfies through
 * 2046. Refusal: exam.age_limit.
 *
 * Late arrival. Enter is accepted from the opening until P409LateEntryMinutes after it; later it is exam.missed (the next
 * sitting remains). The hall's deadline does not move: the examination's outside-world footprint still ends at most
 * TowerWorldCapMinutes after the opening. A candidate who entered L minutes late has the reduced allowance
 * TowerWorldCapMinutes - L outside minutes: when an accepted step leaves the session Entered with more pending outside
 * minutes than that, the same transaction withdraws it ("time called": completed floors kept, the unfinished floor's time
 * counted, no credit for it), and every exit charges at most the allowance. L is the clock minus the opening, exact while
 * Entered because the clock stands still then, so nothing new is stored. For an on-time candidate the pending minutes are
 * already capped at TowerWorldCapMinutes, so P4-01's rule (the cap is not a mission-duration limit) is unchanged.
 */
inline constexpr std::int32_t P409FirstSittingYear = 2043;
inline constexpr std::int64_t P409LateEntryMinutes = 60;
inline constexpr std::int64_t P409AgeLimitYears = 20;
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
 * The sitting an Enter, Defer or Decline at `now` addresses: the latest opened sitting when it is later than the current one
 * and the current session is Unset or closed (a retake, or a morning missed), otherwise the current sitting.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::uint32_t P409DecisionTarget(const WorldState& world, GameInstant now) noexcept;
/** Archive `session` (and an Unset absence for each skipped sitting) until `earlier` reaches `target`; session becomes fresh. */
PERFECTWORLDCORE_API void P409ArchiveUntil(std::vector<TowerSessionState>& earlier, TowerSessionState& session,
    std::uint32_t target);
/**
 * Content-free validity of one stored session at `sitting` (the save rules). `current` selects the live-session rules (the
 * clock, the entry window, the allowance, composure); an earlier sitting must be closed or Unset (absent).
 */
[[nodiscard]] PERFECTWORLDCORE_API bool P409SessionValidInWorld(const WorldState& world, const TowerSessionState& session,
    std::uint32_t sitting, bool current);
} // namespace PerfectWorld
