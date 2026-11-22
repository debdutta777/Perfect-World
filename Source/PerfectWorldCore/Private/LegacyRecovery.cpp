#include "LegacyRecovery.h"

#include "CountdownContent.h"

// P3-14: the pure half of compatibility recovery - the recovery plan, the presentation projection and the save rule for the
// reserved compatibility flags. The command itself is WorldSim's (WorldLegacyRecovery.cpp).
namespace PerfectWorld
{
namespace
{
bool P314Flag(const WorldState& world, std::string_view id)
{
    const auto found = world.Flags.find(id);
    return found != world.Flags.end() && found->second;
}

GameInstant P314Onset()
{
    const CountdownWorldEventDef& event = CountdownWorldEvent();
    return CountdownInstantOf(event.Day, event.Minute);
}

/** No session was ever entered and no grant exists: the only state compatibility recovery may act on. */
bool P314NoSession(const WorldState& world)
{
    return !world.Awakening.Granted && world.Tutorial.Phase == TutorialDungeonPhase::NotStarted;
}
} // namespace

GameInstant AwakeningLastSessionStart(const AwakeningState& awakening) noexcept
{
    return awakening.ExaminationAt - TutorialSessionMinutes - 1;
}

AwakeningRecoveryPlan PlanAwakeningRecovery(const WorldState& world)
{
    AwakeningRecoveryPlan plan;
    // An unseeded world holds the default Awakening (fixed instants, no make-up), the same one seeding leaves in place.
    const AwakeningState& a = world.Awakening;
    const GameInstant now = world.Clock.Now();
    plan.Seed = !a.Seeded;
    if (P314NoSession(world))
    {
        if (now > AwakeningLastSessionStart(a))
            plan.MarkPostDeadline = !P314Flag(world, AwakeningPostDeadlineFlag);
        else if ((a.MakeUp.Phase == AwakeningMakeUpPhase::NotRequired || a.MakeUp.Phase == AwakeningMakeUpPhase::Scheduled) &&
                 now > a.MakeUp.At.value_or(a.AppointmentAt))
            plan.OpenMakeUp = true;
    }
    if (world.CountdownText.empty() && now >= CountdownStartInstant())
    {
        if (now < P314Onset() && now < CountdownEndInstant())
            plan.OpenCountdown = true;
        else
            plan.MarkCountdownUnavailable = !P314Flag(world, AwakeningCountdownUnavailableFlag);
    }
    return plan;
}

AwakeningCompatibilityView QueryAwakeningCompatibility(const WorldState& world)
{
    AwakeningCompatibilityView view;
    view.Pending = PlanAwakeningRecovery(world);
    view.SeededOnRecovery = P314Flag(world, AwakeningSeededOnRecoveryFlag);
    view.PostDeadline = P314Flag(world, AwakeningPostDeadlineFlag);
    view.CountdownUnavailable = P314Flag(world, AwakeningCountdownUnavailableFlag);
    view.MakeUpRequired = world.Awakening.Seeded && !world.Awakening.Granted &&
                          world.Awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required;
    view.LastSessionStart = AwakeningLastSessionStart(world.Awakening);
    view.ExaminationAt = world.Awakening.ExaminationAt;
    return view;
}

bool IsAwakeningCompatFlag(std::string_view flagId) noexcept
{
    return flagId.starts_with(AwakeningCompatFlagPrefix);
}

bool AreAwakeningCompatFlagsValid(const WorldState& world)
{
    const GameInstant now = world.Clock.Now();
    const bool seeded = world.Awakening.Seeded;
    for (auto it = world.Flags.lower_bound(AwakeningCompatFlagPrefix); it != world.Flags.end(); ++it)
    {
        const std::string_view id = it->first;
        if (!IsAwakeningCompatFlag(id))
            break;
        if (!it->second || !seeded)
            return false;
        if (id == AwakeningSeededOnRecoveryFlag)
            continue;
        if (id == AwakeningPostDeadlineFlag)
        {
            if (!P314NoSession(world) || now <= AwakeningLastSessionStart(world.Awakening))
                return false;
            continue;
        }
        if (id == AwakeningCountdownUnavailableFlag)
        {
            if (!world.CountdownText.empty() || now < P314Onset())
                return false;
            continue;
        }
        return false;
    }
    return true;
}
} // namespace PerfectWorld
