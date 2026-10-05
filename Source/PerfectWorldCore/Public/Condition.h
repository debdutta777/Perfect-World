#pragma once

#include <cstdint>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Condition: energy, hunger, health and stress (ROADMAP M2-01;
 * SYSTEMS_CATALOGUE "Condition"; GAME_PLAN glossary: "bounded effects; ordinary
 * neglect never kills").
 *
 * A standalone, integer-only model. It is not yet a member of WorldState and is
 * not in the world hash; the integration step adds the persisted ConditionState
 * section (with the activity-set rates, LastIntegratedAt and Injuries that the
 * catalogue lists) around this module.
 *
 * Every meter is an integer "milli" value on 0..MeterMax (the catalogue says
 * Milli 0..100000). Polarity is natural to the name:
 *   Energy  high is good (rested).        Health high is good.
 *   Hunger  high is bad  (0 = comfortably fed, MeterMax = starving).
 *   Stress  high is bad.
 *
 * Time moves by whole game minutes. One minute is one pure step
 * (StepCondition) that depends only on the state and the activity, so
 * advancing by N minutes is exactly N steps: advancing in minutes, hours or one
 * big jump gives identical results (the catalogue's "additivity" test). Bulk
 * advance stops early once a step changes nothing, so long jumps stay cheap.
 *
 * Neglect never kills. Starvation and exhaustion drain Health, but only down to
 * HealthFloor; below that the model refuses to go (a care scene takes over, see
 * ConditionNeedsCare). Death from ageing belongs to a later system.
 */

constexpr std::int32_t MeterMax = 100000;

/** SYSTEMS_CATALOGUE: "Neglect floors Health at 20,000". */
constexpr std::int32_t HealthFloor = 20000;

/** Starting values for a new character (chosen). */
constexpr std::int32_t StartEnergy = 80000;
constexpr std::int32_t StartHunger = 20000;
constexpr std::int32_t StartHealth = MeterMax;
constexpr std::int32_t StartStress = 10000;

/**
 * Neglect thresholds (chosen). Starving at Hunger >= HungerNeglectAt (about 22
 * waking hours without food); exhausted at Energy <= EnergyNeglectAt (about 17
 * waking hours without sleep). Each is "badness 80000", the same distance from
 * the good end as HealthFloor.
 */
constexpr std::int32_t HungerNeglectAt = 80000;
constexpr std::int32_t EnergyNeglectAt = 20000;

/** Care scene threshold: Health has been worn down to the floor (chosen). */
constexpr std::int32_t CareSceneHealthAt = 25000;

/** Per-minute rates, in milli (all chosen). */
constexpr std::int32_t NeglectHealthDrainPerMinute = 5;   // per neglect condition
constexpr std::int32_t NeglectStressGainPerMinute = 10;   // per neglect condition

/** Activities a stretch of time can be spent in. Rates are per game minute. */
enum class ConditionActivity : std::uint8_t
{
    Awake = 0,       // ordinary waking time
    Resting,         // sitting, lying down awake
    Sleeping,
    LightExertion,   // a brisk walk, a shift on one's feet
    HeavyExertion,   // running, lifting, a long dungeon fight
    Count
};

struct ConditionRates
{
    std::int32_t Energy = 0;          // signed change
    std::int32_t Hunger = 0;          // signed change
    std::int32_t Stress = 0;          // signed change, before neglect
    std::int32_t HealthRecovery = 0;  // gain when no neglect applies
};

/**
 * The table (chosen): Awake  -80 energy, +60 hunger, -3 stress, +6 health.
 * Resting +30/+40/-30/+9. Sleeping +200/+30/-40/+12 (eight hours restores
 * 96000 energy). LightExertion -140/+100/+5/+3. HeavyExertion -260/+180/+15/+0.
 */
PERFECTWORLDCORE_API ConditionRates ConditionRatesFor(ConditionActivity activity);

/** Meals reduce Hunger by a fixed amount, to a minimum of 0 (chosen). */
enum class MealSize : std::uint8_t
{
    Snack = 0,   // -15000
    Light,       // -30000
    Full,        // -55000
    Feast,       // -80000
    Count
};

PERFECTWORLDCORE_API std::int32_t MealHungerRelief(MealSize size);

struct ConditionState
{
    std::int32_t Energy = StartEnergy;
    std::int32_t Hunger = StartHunger;
    std::int32_t Health = StartHealth;
    std::int32_t Stress = StartStress;

    bool operator==(const ConditionState&) const = default;
};

/**
 * Bounded effects. Multipliers are permille (1000 = no effect) applied by other
 * systems to focus (learning, concentration) and physical output. Penalties
 * only: never above 1000, never below EffectMinPermille, so a player is slowed
 * by neglect but never locked out of acting.
 */
constexpr std::int32_t EffectNeutralPermille = 1000;
constexpr std::int32_t EffectMinPermille = 500;

struct ConditionEffects
{
    std::int32_t FocusPermille = EffectNeutralPermille;
    std::int32_t PhysicalPermille = EffectNeutralPermille;
};

/**
 * Penalty ramps (chosen), each linear from "free" to "full". Focus: energy
 * (free at 50000, 250 at 0), stress (free at 40000, 250 at 100000), hunger
 * (free at 40000, 125 at 100000). Physical: energy (250), hunger (free at
 * 40000, 250 at 100000), health (free at 60000, 250 at HealthFloor). The sum is
 * clamped to a 500 permille penalty.
 */
PERFECTWORLDCORE_API ConditionEffects ComputeConditionEffects(const ConditionState& state);

enum class ConditionMeter : std::uint8_t
{
    Energy = 0,
    Hunger,
    Health,
    Stress,
    Count
};

/**
 * Bands for the later ConditionThreshold events (SYSTEMS_CATALOGUE). "Badness"
 * is the distance from the good end: MeterMax - value for Energy and Health,
 * value for Hunger and Stress. Good < 25000, Fair < 50000, Poor < 80000,
 * Critical >= 80000 (chosen).
 */
enum class ConditionBand : std::uint8_t
{
    Good = 0,
    Fair,
    Poor,
    Critical
};

PERFECTWORLDCORE_API ConditionBand ConditionBandOf(ConditionMeter meter, std::int32_t value);

/** True when every meter is on 0..MeterMax. */
PERFECTWORLDCORE_API bool IsConditionValid(const ConditionState& state);

/** True when neglect has worn Health to CareSceneHealthAt or below. */
PERFECTWORLDCORE_API bool ConditionNeedsCare(const ConditionState& state);

/** One game minute. The reference definition of how condition changes. */
PERFECTWORLDCORE_API void StepCondition(ConditionState& state, ConditionActivity activity);

/**
 * Spend `minutes` in `activity`. This is also how a sleep (Sleeping) or rest
 * (Resting) of a given duration, or an exertion, is applied. Returns false and
 * leaves the state untouched for a negative or over-long duration
 * (MaxSingleAdvanceMinutes from GameClock.h), an unknown activity, or an
 * invalid state.
 */
PERFECTWORLDCORE_API bool AdvanceCondition(ConditionState& state, ConditionActivity activity,
                                           std::int64_t minutes);

/** Eat a meal. Returns false and changes nothing for an invalid state or size. */
PERFECTWORLDCORE_API bool ApplyMeal(ConditionState& state, MealSize size);

/**
 * Add a signed stress amount (a hard exam, good news), clamped to 0..MeterMax.
 * Returns false and changes nothing for an invalid state or a delta whose
 * magnitude exceeds MeterMax.
 */
PERFECTWORLDCORE_API bool AdjustStress(ConditionState& state, std::int32_t delta);
} // namespace PerfectWorld
