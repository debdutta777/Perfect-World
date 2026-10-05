#include "Condition.h"
#include "GameClock.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <utility>

using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
constexpr std::int64_t ThirtyDays = 30 * MinutesPerDay;

/** The distribution classes are implementation-defined; this is not. */
std::int64_t Pick(std::mt19937_64& rng, std::int64_t lowInclusive, std::int64_t highInclusive)
{
    return lowInclusive + static_cast<std::int64_t>(rng() % static_cast<std::uint64_t>(highInclusive - lowInclusive + 1));
}

ConditionActivity PickActivity(std::mt19937_64& rng)
{
    return static_cast<ConditionActivity>(Pick(rng, 0, static_cast<int>(ConditionActivity::Count) - 1));
}

ConditionState RandomState(std::mt19937_64& rng)
{
    ConditionState state;
    state.Energy = static_cast<std::int32_t>(Pick(rng, 0, MeterMax));
    state.Hunger = static_cast<std::int32_t>(Pick(rng, 0, MeterMax));
    state.Health = static_cast<std::int32_t>(Pick(rng, 0, MeterMax));
    state.Stress = static_cast<std::int32_t>(Pick(rng, 0, MeterMax));
    return state;
}

bool EffectsInRange(const ConditionState& state)
{
    const ConditionEffects effects = ComputeConditionEffects(state);
    return effects.FocusPermille >= EffectMinPermille && effects.FocusPermille <= EffectNeutralPermille &&
           effects.PhysicalPermille >= EffectMinPermille && effects.PhysicalPermille <= EffectNeutralPermille;
}

/** One random event; returns false if the call rejected a valid request. */
bool ApplyRandomEvent(std::mt19937_64& rng, ConditionState& state)
{
    switch (Pick(rng, 0, 3))
    {
    case 0:
        return AdvanceCondition(state, PickActivity(rng), Pick(rng, 0, 1500));
    case 1:
        return ApplyMeal(state, static_cast<MealSize>(Pick(rng, 0, static_cast<int>(MealSize::Count) - 1)));
    case 2:
        return AdjustStress(state, static_cast<std::int32_t>(Pick(rng, -MeterMax, MeterMax)));
    default:
        return AdvanceCondition(state, ConditionActivity::Sleeping, Pick(rng, 0, 720));
    }
}

void TestDefaultsAndTable()
{
    const ConditionState fresh;
    Check(IsConditionValid(fresh), "a new state is valid");
    Check(fresh.Health == MeterMax && fresh.Energy == StartEnergy && fresh.Hunger == StartHunger &&
              fresh.Stress == StartStress,
          "a new state has the documented starting values");
    Check(HealthFloor == 20000, "the neglect floor is the catalogue's 20,000");
    Check(!ConditionNeedsCare(fresh), "a healthy start needs no care scene");

    for (int index = 0; index < static_cast<int>(ConditionActivity::Count); ++index)
    {
        const ConditionRates rates = ConditionRatesFor(static_cast<ConditionActivity>(index));
        Check(rates.HealthRecovery >= 0, "no activity drains health by itself");
        Check(rates.Hunger > 0, "every activity makes you hungrier");
    }
    Check(ConditionRatesFor(ConditionActivity::Sleeping).Energy > 0, "sleep restores energy");
    Check(ConditionRatesFor(ConditionActivity::Awake).Energy < 0, "waking time spends energy");
    Check(ConditionRatesFor(ConditionActivity::HeavyExertion).Energy < ConditionRatesFor(ConditionActivity::LightExertion).Energy &&
              ConditionRatesFor(ConditionActivity::LightExertion).Energy < ConditionRatesFor(ConditionActivity::Awake).Energy,
          "harder activity costs more energy");
    Check(ConditionRatesFor(ConditionActivity::Resting).Stress < 0, "rest eases stress");
}

void TestSleepRecovers()
{
    // Six hours of sleep from 20,000 energy: exactly 200 a minute.
    ConditionState state;
    state.Energy = 20000;
    Check(AdvanceCondition(state, ConditionActivity::Sleeping, 360), "sleep applies");
    Check(state.Energy == 20000 + 360 * 200, "sleep restores energy at 200 per minute");
    Check(state.Hunger == StartHunger + 360 * 30, "a sleeper still gets slowly hungry");
    Check(state.Stress == 0, "six hours of sleep clears the starting stress (40 per minute, floored at zero)");

    // Eight hours from empty restores nearly everything and never overflows.
    ConditionState empty;
    empty.Energy = 0;
    AdvanceCondition(empty, ConditionActivity::Sleeping, 8 * MinutesPerHour);
    Check(empty.Energy == 96000, "eight hours of sleep restores 96,000 energy from empty");
    AdvanceCondition(empty, ConditionActivity::Sleeping, 2 * MinutesPerHour);
    Check(empty.Energy == MeterMax, "energy caps at the maximum");

    // Sleep heals an unneglected body faster than waking does.
    ConditionState hurt;
    hurt.Health = 50000;
    ConditionState hurtAwake = hurt;
    AdvanceCondition(hurt, ConditionActivity::Sleeping, 60);
    AdvanceCondition(hurtAwake, ConditionActivity::Awake, 60);
    Check(hurt.Health == 50000 + 60 * 12, "sleeping heals 12 per minute");
    Check(hurtAwake.Health == 50000 + 60 * 6, "waking heals 6 per minute");

    // Resting recovers energy and calms stress, slower than sleep.
    ConditionState rested;
    rested.Energy = 30000;
    rested.Stress = 60000;
    AdvanceCondition(rested, ConditionActivity::Resting, 60);
    Check(rested.Energy == 30000 + 60 * 30, "resting restores 30 energy per minute");
    Check(rested.Stress == 60000 - 60 * 30, "resting eases stress 30 per minute");

    // Exertion costs, and cost grows with intensity.
    ConditionState light;
    ConditionState heavy;
    AdvanceCondition(light, ConditionActivity::LightExertion, 60);
    AdvanceCondition(heavy, ConditionActivity::HeavyExertion, 60);
    Check(light.Energy == StartEnergy - 60 * 140 && heavy.Energy == StartEnergy - 60 * 260,
          "exertion spends energy at its documented rate");
    Check(heavy.Hunger > light.Hunger && light.Hunger > StartHunger, "exertion builds hunger");
}

void TestMealsRecover()
{
    const std::pair<MealSize, std::int32_t> meals[] = {{MealSize::Snack, 15000},
                                                       {MealSize::Light, 30000},
                                                       {MealSize::Full, 55000},
                                                       {MealSize::Feast, 80000}};
    for (const auto& [size, relief] : meals)
    {
        Check(MealHungerRelief(size) == relief, "meal relief matches the documented table");
        ConditionState state;
        state.Hunger = 90000;
        Check(ApplyMeal(state, size), "a meal applies");
        Check(state.Hunger == 90000 - relief, "a meal lowers hunger by its relief");
    }

    ConditionState nearlyFull;
    nearlyFull.Hunger = 10000;
    ApplyMeal(nearlyFull, MealSize::Feast);
    Check(nearlyFull.Hunger == 0, "hunger floors at zero, never negative");
    Check(nearlyFull.Energy == StartEnergy && nearlyFull.Health == StartHealth,
          "a meal changes only hunger");

    // The way out of starvation: eat, and health begins to recover.
    ConditionState starving;
    starving.Hunger = MeterMax;
    starving.Health = HealthFloor;
    AdvanceCondition(starving, ConditionActivity::Awake, 10);
    Check(starving.Health == HealthFloor, "a starving body stays at the floor");
    ApplyMeal(starving, MealSize::Full);
    Check(starving.Hunger == MeterMax - 55000, "the meal relieves the hunger");
    AdvanceCondition(starving, ConditionActivity::Awake, 100);
    Check(starving.Health == HealthFloor + 100 * 6, "once fed, health recovers 6 per minute");
}

void TestNeglectNeverKills()
{
    const ConditionState start;

    // Thirty days with no food and no sleep, awake, stepped by the minute and by
    // the hour: same result, and health never goes below its floor on the way.
    ConditionState byMinute = start;
    for (std::int64_t minute = 0; minute < ThirtyDays; ++minute)
    {
        StepCondition(byMinute, ConditionActivity::Awake);
        if (minute % 97 == 0 || minute > ThirtyDays - 5)
        {
            Check(IsConditionValid(byMinute) && byMinute.Health >= HealthFloor,
                  "minute stepping: state valid, health at or above the floor");
        }
    }
    ConditionState byHour = start;
    for (std::int64_t hour = 0; hour < 30 * 24; ++hour)
    {
        Check(AdvanceCondition(byHour, ConditionActivity::Awake, MinutesPerHour), "hour step applies");
        Check(IsConditionValid(byHour) && byHour.Health >= HealthFloor,
              "hour stepping: state valid, health at or above the floor");
    }
    Check(byMinute == byHour, "thirty days by the minute equals thirty days by the hour");
    ConditionState byBlock = start;
    AdvanceCondition(byBlock, ConditionActivity::Awake, ThirtyDays);
    Check(byBlock == byMinute, "one 30-day jump equals stepping");

    Check(byMinute.Health == HealthFloor, "neglect wears health to exactly the floor and no further");
    Check(byMinute.Energy == 0 && byMinute.Hunger == MeterMax && byMinute.Stress == MeterMax,
          "the other meters are pinned at their bad ends");
    Check(ConditionNeedsCare(byMinute), "neglect to the floor triggers the care scene");
    Check(EffectsInRange(byMinute), "effects stay bounded at the worst state");
    const ConditionEffects worst = ComputeConditionEffects(byMinute);
    Check(worst.FocusPermille == EffectMinPermille && worst.PhysicalPermille == EffectMinPermille,
          "the worst state is slowed to the documented minimum, not to zero");

    // The same, asleep without food (sleep cures exhaustion but not starvation),
    // and alternating a night's sleep with a hungry day.
    ConditionState asleep = start;
    AdvanceCondition(asleep, ConditionActivity::Sleeping, ThirtyDays);
    Check(IsConditionValid(asleep) && asleep.Health == HealthFloor, "30 days asleep and unfed: floor, valid");
    ConditionState cycle = start;
    for (int day = 0; day < 30; ++day)
    {
        AdvanceCondition(cycle, ConditionActivity::Awake, 16 * MinutesPerHour);
        AdvanceCondition(cycle, ConditionActivity::Sleeping, 8 * MinutesPerHour);
        Check(IsConditionValid(cycle) && cycle.Health >= HealthFloor, "30 unfed days of ordinary rhythm stay valid");
    }
    ConditionState worked = start;
    AdvanceCondition(worked, ConditionActivity::HeavyExertion, ThirtyDays);
    Check(IsConditionValid(worked) && worked.Health >= HealthFloor, "30 days of heavy exertion: valid, floor held");

    // An already-injured character is not pushed lower by neglect.
    ConditionState injured = start;
    injured.Health = 7000;
    injured.Hunger = MeterMax;
    AdvanceCondition(injured, ConditionActivity::Awake, 1000);
    Check(injured.Health == 7000, "neglect never reduces health that is already below the floor");
    AdvanceCondition(injured, ConditionActivity::Awake, ThirtyDays);
    Check(injured.Health == 7000, "...however long it lasts");

    // A day or two of ordinary neglect costs little.
    ConditionState allNighter = start;
    AdvanceCondition(allNighter, ConditionActivity::Awake, 24 * MinutesPerHour);
    Check(allNighter.Health == 94350, "a single day awake and unfed costs under six percent of health");
}

void TestAdditivity()
{
    std::mt19937_64 rng(0xC04D1710ull);
    for (int trial = 0; trial < 2000; ++trial)
    {
        const ConditionState start = RandomState(rng);
        const ConditionActivity activity = PickActivity(rng);
        const std::int64_t first = Pick(rng, 0, 3000);
        const std::int64_t second = Pick(rng, 0, 3000);

        ConditionState stepped = start;
        for (std::int64_t i = 0; i < first + second; ++i)
        {
            StepCondition(stepped, activity);
        }
        ConditionState together = start;
        AdvanceCondition(together, activity, first + second);
        ConditionState split = start;
        AdvanceCondition(split, activity, first);
        AdvanceCondition(split, activity, second);
        Check(together == stepped, "advancing N minutes equals N steps");
        Check(split == together, "advancing a then b equals advancing a+b");
    }
}

void TestRandomSequences()
{
    std::mt19937_64 rng(0x5EEDC0DEull);
    for (int sequence = 0; sequence < 10000; ++sequence)
    {
        // Half the sequences start from the default, half from any valid state.
        ConditionState state = (sequence % 2 == 0) ? ConditionState{} : RandomState(rng);
        const std::int32_t healthLowerBound = std::min(state.Health, HealthFloor);
        const int length = static_cast<int>(Pick(rng, 5, 40));
        for (int event = 0; event < length; ++event)
        {
            Check(ApplyRandomEvent(rng, state), "a valid random event is accepted");
            Check(IsConditionValid(state), "every meter stays in range");
            Check(state.Health >= healthLowerBound, "health never falls below the floor through neglect");
            Check(EffectsInRange(state), "effects stay within their bounds");
        }
    }
}

void TestDeterminism()
{
    ConditionState first;
    ConditionState second;
    std::mt19937_64 rngA(0xDE7E12ull);
    std::mt19937_64 rngB(0xDE7E12ull);
    for (int event = 0; event < 5000; ++event)
    {
        ApplyRandomEvent(rngA, first);
        ApplyRandomEvent(rngB, second);
        Check(first == second, "the same inputs give the same state at every step");
    }
}

void TestRejectsBadInput()
{
    ConditionState state;
    const ConditionState before = state;
    Check(!AdvanceCondition(state, ConditionActivity::Awake, -1), "negative durations are refused");
    Check(!AdvanceCondition(state, ConditionActivity::Awake, MaxSingleAdvanceMinutes + 1),
          "durations over the clock's single-advance cap are refused");
    Check(!AdvanceCondition(state, ConditionActivity::Count, 10), "unknown activities are refused");
    Check(!ApplyMeal(state, MealSize::Count), "unknown meal sizes are refused");
    Check(!AdjustStress(state, MeterMax + 1), "an absurd stress delta is refused");
    Check(state == before, "refused calls change nothing");
    Check(AdvanceCondition(state, ConditionActivity::Awake, 0) && state == before, "zero minutes is a no-op");

    ConditionState broken;
    broken.Health = MeterMax + 1;
    Check(!IsConditionValid(broken), "an out-of-range meter is invalid");
    broken.Health = 0;
    broken.Energy = -1;
    Check(!IsConditionValid(broken), "a negative meter is invalid");
    const ConditionState brokenBefore = broken;
    Check(!AdvanceCondition(broken, ConditionActivity::Awake, 10) && !ApplyMeal(broken, MealSize::Snack) &&
              !AdjustStress(broken, 1) && broken == brokenBefore,
          "an invalid state is refused and left untouched");

    ConditionState stress;
    AdjustStress(stress, MeterMax);
    Check(stress.Stress == MeterMax, "stress clamps at the maximum");
    AdjustStress(stress, -MeterMax);
    Check(stress.Stress == 0, "stress clamps at zero");
}

void TestEffectsAndBands()
{
    const ConditionState fresh;
    const ConditionEffects calm = ComputeConditionEffects(fresh);
    Check(calm.FocusPermille == EffectNeutralPermille && calm.PhysicalPermille == EffectNeutralPermille,
          "a rested, fed, healthy character has no penalty");

    ConditionState tired = fresh;
    tired.Energy = 25000;
    const ConditionEffects tiredEffects = ComputeConditionEffects(tired);
    Check(tiredEffects.FocusPermille == 875 && tiredEffects.PhysicalPermille == 875,
          "half-way to empty energy costs 125 permille on both");

    ConditionState stressed = fresh;
    stressed.Stress = MeterMax;
    const ConditionEffects stressedEffects = ComputeConditionEffects(stressed);
    Check(stressedEffects.FocusPermille == 750 && stressedEffects.PhysicalPermille == EffectNeutralPermille,
          "stress costs focus but not physical output");

    Check(ConditionBandOf(ConditionMeter::Energy, MeterMax) == ConditionBand::Good, "full energy is good");
    Check(ConditionBandOf(ConditionMeter::Energy, EnergyNeglectAt) == ConditionBand::Critical,
          "the energy neglect threshold is the critical band");
    Check(ConditionBandOf(ConditionMeter::Hunger, HungerNeglectAt) == ConditionBand::Critical,
          "the hunger neglect threshold is the critical band");
    Check(ConditionBandOf(ConditionMeter::Health, HealthFloor) == ConditionBand::Critical,
          "the health floor is the critical band");
    Check(ConditionBandOf(ConditionMeter::Hunger, 30000) == ConditionBand::Fair, "hunger 30000 is fair");
    Check(ConditionBandOf(ConditionMeter::Stress, 60000) == ConditionBand::Poor, "stress 60000 is poor");
    Check(ConditionBandOf(ConditionMeter::Hunger, 0) == ConditionBand::Good, "hunger 0 is good");
}
} // namespace

int main()
{
    TestDefaultsAndTable();
    TestSleepRecovers();
    TestMealsRecover();
    TestNeglectNeverKills();
    TestAdditivity();
    TestRandomSequences();
    TestDeterminism();
    TestRejectsBadInput();
    TestEffectsAndBands();
    std::cout << "ConditionTests passed (" << Checks << " checks)\n";
    return 0;
}
