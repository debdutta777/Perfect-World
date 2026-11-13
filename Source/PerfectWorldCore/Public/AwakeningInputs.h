#pragma once

#include "Awakening.h"
#include "DayLoop.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace PerfectWorld
{
/**
 * R91 / R9-01 FROZEN CONTRACT, 2026-10-23 (Round 9 orders 02/03/04/05/08).
 * Integrator: include this header in WorldState.h and add exactly
 *     AwakeningState Awakening;
 * Seed once on creation confirmation with SeedAwakeningInputs(content, creation,
 * world.WorldSeed), then assign the successful result's State. Never reseed on
 * load, attendance, tutorial entry, make-up, grant or countdown opening.
 * Default construction is deliberately unseeded, not a zero-trait character.
 *
 * Persistent fields below are authoritative and must all be saved and hashed.
 * Stats is the one latent-stat owner, throughout life. AttributeEvidenceMilli
 * accumulates actual EvidenceCredit; evidence is NOT attribute growth. No
 * conversion rule exists yet, so this adapter never adds it to Attributes.
 * ProjectionEvidence holds only observed AptitudeEvidence, added through the
 * existing AptitudeAddEvidence rule by the system that actually observed it.
 * Creation, skill XP, attendance sign-in and elapsed frames invent no evidence.
 * BuildAwakeningInput calls AptitudeProject: unobserved remains unknown.
 *
 * AppliedActivityInstances is an ascending set of nonzero runner Instance IDs,
 * scoped to this life's single DayLoop runner (which must never reset). Consume
 * every new Completed DayLoopEvent atomically with the originating command,
 * including completions with no learning. Use Credited AFTER PracticeWindow,
 * never ActivityEvent::Learning, forecasts, receipts' aggregate totals or
 * WeekResult's totals in addition to the individual events. Persist this set;
 * out-of-order delivery is supported, first consumption stands, replay is a
 * no-op. Opportunity ReceiptSeq is not a key: it can be zero or be folded.
 *
 * Conduct uses RecordConduct/SummariseConduct unchanged. R9-02 owns tutorial
 * orchestration in its own slice: three tests, safe withdrawal and supervision.
 * Neither attendance nor Build/Resolve grants anything. Even a successful
 * zero-conduct fallback is only a calculation, NEVER a session skip path.
 * R9-02 atomically writes Granted only after supervised completion, using
 * ResolveAwakening(BuildAwakeningInput(state), loadedRecipes) followed by
 * AwakenedStartingProfile(result, GameClock::DayNumber(grantedAt)). Both must
 * succeed. Once present, Granted and its input/result/starting profile are
 * immutable; later practice needs a separate live progression profile.
 *
 * AppointmentAt defaults to 23 Jan 2043 09:00 (chosen campus slot; Round 9
 * specifies only its date). ExaminationAt is 2 Feb 2043 06:00, immutable.
 * Make-up never rewrites either instant. Required has no appointment;
 * Scheduled has At >= AppointmentAt and At < ExaminationAt, with sufficient
 * room for the supervised session. Completed retains that At for history.
 * NotRequired has no At. Absence sets Required, scheduling sets Scheduled,
 * supervised completion sets Completed when this was a make-up. No decline,
 * refusal, capability penalty or deadline extension is represented here.
 * R9-05 owns the separate 18:00 countdown boundary and late entry policy.
 *
 * Legacy migration (R9-04): seed from the saved creation only, empty conduct,
 * empty projection/evidence and no Granted. No reconstructed past activities.
 * Mark runner instances 1..Runner.AppliedInstance as consumed, or arrange that
 * historical events are never delivered: old receipts cannot award new XP.
 * Codec validates state, committed output, scheduling and tutorial consistency;
 * these adapters do not authenticate saves or external/presentation payloads.
 */
enum class AwakeningMakeUpPhase : std::uint8_t
{
    NotRequired = 0,
    Required,
    Scheduled,
    Completed
};

struct AwakeningMakeUpState
{
    AwakeningMakeUpPhase Phase = AwakeningMakeUpPhase::NotRequired;
    std::optional<GameInstant> At;
};

struct AwakeningGrant
{
    GameInstant At = 0;
    /** Snapshot at grant, so later ordinary learning cannot rewrite its causes. */
    AwakeningInput Input;
    AwakeningResult Result;
    /** The value returned by AwakenedStartingProfile, not a new profile type. */
    AwakenedProfile StartingProfile;
};

struct AwakeningState
{
    bool Seeded = false;
    /** Must equal the owning World's WorldSeed; used only by resolver ties. */
    std::uint64_t WorldSeed = 0;
    LatentStats Stats;
    std::array<std::int64_t, AttributeCount> AttributeEvidenceMilli{};
    AptitudeLedger ProjectionEvidence;
    std::vector<std::uint64_t> AppliedActivityInstances;
    ConductLedger Conduct;
    std::optional<AwakeningGrant> Granted;
    GameInstant AppointmentAt = GameClock::FromCalendar(2043, 1, 23, 9, 0);
    GameInstant ExaminationAt = GameClock::FromCalendar(2043, 2, 2, 6, 0);
    AwakeningMakeUpState MakeUp;
};

struct AwakeningInputsSeedResult
{
    std::string Error;
    AwakeningState State;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Validated content and creation ranks; all non-trait creation fields ignored. */
[[nodiscard]] PERFECTWORLDCORE_API AwakeningInputsSeedResult SeedAwakeningInputs(
    const ContentDb& content, const CharacterCreationSave& creation, std::uint64_t worldSeed);

struct AwakeningCreditsResult
{
    std::string Error;
    std::size_t Applied = 0;
    std::size_t Replayed = 0;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/**
 * Pure reducer over trusted DayLoop completion receipts, no IO/clock/RNG draws.
 * Non-Completed events are ignored. Refuses malformed new completions, unknown
 * or repeated skills/attributes, negative credits and an invalid latent owner.
 * Whole batch applies or nothing changes; counts are zero on refusal.
 * Skill XP saturates at int32 max, attribute evidence at int64 max (storage
 * bounds, not invented growth ceilings). Projection, conduct, grant, dates and
 * make-up are unchanged. A consumed instance is ignored even if redelivered
 * with different payload: its first credit stands.
 */
PERFECTWORLDCORE_API AwakeningCreditsResult ApplyActivityCredits(
    AwakeningState& state, std::span<const DayLoopEvent> receipts);

/**
 * Precondition: successfully seeded, valid authoritative state. Copies Stats,
 * SummariseConduct(Conduct), AptitudeProject(ProjectionEvidence) and WorldSeed
 * exactly. Does not mutate state, resolve, grant, schedule or advance time.
 */
[[nodiscard]] PERFECTWORLDCORE_API AwakeningInput BuildAwakeningInput(const AwakeningState& state);
} // namespace PerfectWorld
