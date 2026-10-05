#include "Condition.h"

#include "GameClock.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
std::int32_t ConditionClamp(std::int64_t value)
{
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(value, 0, MeterMax));
}

bool ConditionIsRestful(ConditionActivity activity)
{
    return activity == ConditionActivity::Resting || activity == ConditionActivity::Sleeping;
}

/**
 * A linear penalty from 0 at `freeAt` to `maxPenalty` at `fullAt`. `fullAt` may
 * be above or below `freeAt`; a value on the free side costs nothing and a
 * value beyond `fullAt` costs the maximum.
 */
std::int32_t ConditionRamp(std::int32_t value, std::int32_t freeAt, std::int32_t fullAt,
                           std::int32_t maxPenalty)
{
    const std::int64_t direction = fullAt > freeAt ? 1 : -1;
    const std::int64_t span = direction * (static_cast<std::int64_t>(fullAt) - freeAt);
    const std::int64_t progress = direction * (static_cast<std::int64_t>(value) - freeAt);
    if (progress <= 0)
    {
        return 0;
    }
    if (progress >= span)
    {
        return maxPenalty;
    }
    return static_cast<std::int32_t>(progress * maxPenalty / span);
}

std::int32_t ConditionBadness(ConditionMeter meter, std::int32_t value)
{
    const bool highIsGood = meter == ConditionMeter::Energy || meter == ConditionMeter::Health;
    return highIsGood ? MeterMax - value : value;
}
} // namespace

ConditionRates ConditionRatesFor(ConditionActivity activity)
{
    switch (activity)
    {
    case ConditionActivity::Awake:
        return {-80, 60, -3, 6};
    case ConditionActivity::Resting:
        return {30, 40, -30, 9};
    case ConditionActivity::Sleeping:
        return {200, 30, -40, 12};
    case ConditionActivity::LightExertion:
        return {-140, 100, 5, 3};
    case ConditionActivity::HeavyExertion:
        return {-260, 180, 15, 0};
    case ConditionActivity::Count:
        break;
    }
    return {};
}

std::int32_t MealHungerRelief(MealSize size)
{
    switch (size)
    {
    case MealSize::Snack:
        return 15000;
    case MealSize::Light:
        return 30000;
    case MealSize::Full:
        return 55000;
    case MealSize::Feast:
        return 80000;
    case MealSize::Count:
        break;
    }
    return 0;
}

ConditionEffects ComputeConditionEffects(const ConditionState& state)
{
    const std::int32_t energyPenalty = ConditionRamp(state.Energy, 50000, 0, 250);
    const std::int32_t hungerPenalty = ConditionRamp(state.Hunger, 40000, MeterMax, 250);
    const std::int32_t stressPenalty = ConditionRamp(state.Stress, 40000, MeterMax, 250);
    const std::int32_t healthPenalty = ConditionRamp(state.Health, 60000, HealthFloor, 250);

    const std::int32_t maxPenalty = EffectNeutralPermille - EffectMinPermille;
    ConditionEffects effects;
    effects.FocusPermille =
        EffectNeutralPermille -
        std::min(maxPenalty, energyPenalty + stressPenalty + hungerPenalty / 2);
    effects.PhysicalPermille =
        EffectNeutralPermille - std::min(maxPenalty, energyPenalty + hungerPenalty + healthPenalty);
    return effects;
}

ConditionBand ConditionBandOf(ConditionMeter meter, std::int32_t value)
{
    const std::int32_t badness = ConditionBadness(meter, std::clamp(value, 0, MeterMax));
    if (badness >= 80000)
    {
        return ConditionBand::Critical;
    }
    if (badness >= 50000)
    {
        return ConditionBand::Poor;
    }
    if (badness >= 25000)
    {
        return ConditionBand::Fair;
    }
    return ConditionBand::Good;
}

bool IsConditionValid(const ConditionState& state)
{
    const auto inRange = [](std::int32_t value) { return value >= 0 && value <= MeterMax; };
    return inRange(state.Energy) && inRange(state.Hunger) && inRange(state.Health) &&
           inRange(state.Stress);
}

bool ConditionNeedsCare(const ConditionState& state)
{
    return state.Health <= CareSceneHealthAt;
}

void StepCondition(ConditionState& state, ConditionActivity activity)
{
    const ConditionRates rates = ConditionRatesFor(activity);

    // Neglect is judged on the state at the start of the minute. Hunger always
    // counts; exhaustion does not while sleeping or resting, which is the cure.
    const bool starving = state.Hunger >= HungerNeglectAt;
    const bool exhausted = state.Energy <= EnergyNeglectAt && !ConditionIsRestful(activity);
    const std::int32_t neglect = (starving ? 1 : 0) + (exhausted ? 1 : 0);

    if (neglect > 0)
    {
        // Never below the floor, and never push an already-lower value further.
        const std::int32_t room = std::max(0, state.Health - HealthFloor);
        state.Health -= std::min(neglect * NeglectHealthDrainPerMinute, room);
    }
    else
    {
        state.Health = ConditionClamp(static_cast<std::int64_t>(state.Health) + rates.HealthRecovery);
    }

    state.Stress = ConditionClamp(static_cast<std::int64_t>(state.Stress) + rates.Stress +
                                  neglect * NeglectStressGainPerMinute);
    state.Energy = ConditionClamp(static_cast<std::int64_t>(state.Energy) + rates.Energy);
    state.Hunger = ConditionClamp(static_cast<std::int64_t>(state.Hunger) + rates.Hunger);
}

bool AdvanceCondition(ConditionState& state, ConditionActivity activity, std::int64_t minutes)
{
    if (minutes < 0 || minutes > MaxSingleAdvanceMinutes || activity >= ConditionActivity::Count ||
        !IsConditionValid(state))
    {
        return false;
    }
    for (std::int64_t minute = 0; minute < minutes; ++minute)
    {
        const ConditionState before = state;
        StepCondition(state, activity);
        if (state == before)
        {
            // A step is a pure function of (state, activity), so a fixed point
            // stays fixed for the rest of the stretch.
            break;
        }
    }
    return true;
}

bool ApplyMeal(ConditionState& state, MealSize size)
{
    if (size >= MealSize::Count || !IsConditionValid(state))
    {
        return false;
    }
    state.Hunger = ConditionClamp(static_cast<std::int64_t>(state.Hunger) - MealHungerRelief(size));
    return true;
}

bool AdjustStress(ConditionState& state, std::int32_t delta)
{
    if (delta < -MeterMax || delta > MeterMax || !IsConditionValid(state))
    {
        return false;
    }
    state.Stress = ConditionClamp(static_cast<std::int64_t>(state.Stress) + delta);
    return true;
}
} // namespace PerfectWorld
