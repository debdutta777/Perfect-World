#include "WorldSaveInternal.h"

#include "WorldCountdown.h"

#include <algorithm>

// R9-04: the semantic rules the Awakening slices keep inside their world, checked by the writer before encoding and by the
// reader after decoding (the doors and school precedent). The R9-01/R9-02/R9-05 contracts own the domain rules; this file
// authenticates a stored state against them and against the world it claims to belong to.
namespace PerfectWorld
{
namespace
{
bool R94RDefaultAwakening(const AwakeningState& a)
{
    const AwakeningState fresh;
    return !a.Seeded && a.WorldSeed == 0 && a.Stats == fresh.Stats && a.AttributeEvidenceMilli == fresh.AttributeEvidenceMilli &&
           a.ProjectionEvidence == fresh.ProjectionEvidence && a.AppliedActivityInstances.empty() && a.Conduct == fresh.Conduct &&
           !a.Granted && a.AppointmentAt == fresh.AppointmentAt && a.ExaminationAt == fresh.ExaminationAt &&
           a.MakeUp.Phase == fresh.MakeUp.Phase && a.MakeUp.At == fresh.MakeUp.At;
}

bool R94RStats(const LatentStats& stats)
{
    if (std::any_of(stats.Attributes.begin(), stats.Attributes.end(), [](std::int32_t v) { return v < 0; }))
        return false;
    for (std::size_t i = 0; i < stats.Skills.size(); ++i)
    {
        const SkillLevel& skill = stats.Skills[i];
        if (skill.SkillId.empty() || !WorldSaveAwakeningSafeText(skill.SkillId) || skill.LevelMilli < 0 ||
            (i > 0 && !(stats.Skills[i - 1].SkillId < skill.SkillId)))
            return false;
    }
    return true;
}

bool R94RNotFuture(const std::optional<GameInstant>& at, GameInstant now)
{
    return !at || *at <= now;
}

bool R94RProfile(const AwakenedProfile& p)
{
    std::int32_t affinity = 0;
    for (const std::int32_t percent : p.AffinityPercent)
    {
        if (percent < 0 || percent > 100)
            return false;
        affinity += percent;
    }
    if (affinity != 100)
        return false;
    for (std::size_t d = 0; d < static_cast<std::size_t>(PowerDimensionCount); ++d)
    {
        if (p.Rating[d] < 0 || p.Rating[d] > MaxRatingCentipoints || p.Peak[d] < p.Rating[d] || p.Peak[d] > MaxRatingCentipoints)
            return false;
    }
    return static_cast<std::int32_t>(p.Grade) < AwakenedGradeCount && p.SupervisedPracticeMinutes >= 0;
}

bool R94RGrant(const AwakeningState& a, GameInstant now)
{
    const AwakeningGrant& g = *a.Granted;
    if (g.At > now || !g.Result.Ok() || g.Input.WorldSeed != a.WorldSeed || !(g.Input.Conduct == SummariseConduct(a.Conduct)) ||
        !R94RStats(g.Input.Stats))
        return false;
    // The committed starting profile is exactly what the committed result gives on the grant's day: never re-resolved.
    const std::optional<AwakenedProfile> starting = AwakenedStartingProfile(g.Result, GameClock::DayNumber(g.At));
    return starting && WorldSaveAwakeningProfileText(*starting) == WorldSaveAwakeningProfileText(g.StartingProfile);
}

bool R94RTutorial(const TutorialDungeonState& t, const AwakeningState& a, const WorldState& world)
{
    const GameInstant now = world.Clock.Now();
    if (!IsTutorialDungeonStateValid(t, a) || !R94RNotFuture(t.StartedAt, now) || !R94RNotFuture(t.LastActionAt, now) ||
        !R94RNotFuture(t.CompletedAt, now))
        return false;
    // The world clock is paused while the session is played: an unfinished session stands at its entry instant.
    if (t.Phase == TutorialDungeonPhase::Active || t.Phase == TutorialDungeonPhase::ReadyToGrant)
        return t.StartedAt && *t.StartedAt == now && !world.DayLoop.Runner.Active() && !world.DayLoop.Asleep;
    return true;
}
} // namespace

bool WorldSaveAwakeningValid(const WorldState& world)
{
    const AwakeningState& a = world.Awakening;
    if (!a.Seeded)
        return R94RDefaultAwakening(a) && world.Tutorial == TutorialDungeonState{} && world.CountdownText.empty() &&
               !world.LiveAwakenedProfile;
    const GameInstant now = world.Clock.Now();
    if (a.WorldSeed != world.WorldSeed || a.AppointmentAt != AwakeningState{}.AppointmentAt ||
        a.ExaminationAt != AwakeningState{}.ExaminationAt || !R94RStats(a.Stats))
        return false;
    if (std::any_of(a.AttributeEvidenceMilli.begin(), a.AttributeEvidenceMilli.end(), [](std::int64_t v) { return v < 0; }))
        return false;
    if (!IsAptitudeLedgerValid(a.ProjectionEvidence) ||
        std::any_of(a.ProjectionEvidence.Items.begin(), a.ProjectionEvidence.Items.end(),
                    [now](const AptitudeEvidence& item) { return item.At > now; }))
        return false;
    // Consumed runner instances: ascending, unique, nonzero, and never beyond the activities this life has started.
    for (std::size_t i = 0; i < a.AppliedActivityInstances.size(); ++i)
    {
        const std::uint64_t instance = a.AppliedActivityInstances[i];
        if (instance == 0 || instance > world.DayLoop.Runner.Instance || (i > 0 && a.AppliedActivityInstances[i - 1] >= instance))
            return false;
    }
    const std::optional<ConductLedger> conduct = ParseConductLedger(SerialiseConductLedger(a.Conduct));
    if (!conduct || !(*conduct == a.Conduct) || !R94RTutorial(world.Tutorial, a, world))
        return false;
    if (a.Granted && !R94RGrant(a, now))
        return false;
    // The live practice owner exists exactly once the grant does, keeps the grant's affinity and never invents a grade.
    if (a.Granted.has_value() != world.LiveAwakenedProfile.has_value())
        return false;
    if (world.LiveAwakenedProfile &&
        (!R94RProfile(*world.LiveAwakenedProfile) ||
         world.LiveAwakenedProfile->AffinityPercent != a.Granted->StartingProfile.AffinityPercent ||
         world.LiveAwakenedProfile->Grade != a.Granted->StartingProfile.Grade))
        return false;
    // No stored countdown: then no countdown history either (its Gate exists only once the ten days opened).
    if (world.CountdownText.empty())
        return world.Gates.find(CountdownWorldEvent().GateId) == world.Gates.end();
    // The stored countdown is canonical and agrees with this world: its fixed span and seed, the clock and its Gate history.
    const std::optional<CountdownState> countdown = ParseCountdown(world.CountdownText);
    if (!countdown || SerialiseCountdown(*countdown) != world.CountdownText)
        return false;
    return QueryWorldCountdown(world, a, countdown).Issue.Ok();
}
} // namespace PerfectWorld
