#include "Countdown.h"

#include "CountdownInternal.h"

#include <algorithm>
#include <cstdlib>

// The route consequence, the news, and the profile the ten days leave (ROADMAP M4-03).
namespace PerfectWorld
{
namespace
{
struct CreditRule
{
    std::string_view Skill;
    CountdownDimension Dimension;
    /** Milli-XP per preparation point. */
    std::int32_t MilliPerPoint;
};

// Design choice (M4-03): how the day loop's learning feeds the six dimensions. The documents name the
// links only in words (reading feeds analysis, running endurance, repair construction, negotiating
// negotiation; AWAKENING_AND_EXAM.md "Latent stats"). Fitness also feeds combat at a third of the rate:
// no combat skill exists yet, and the ten days cannot make a fighter, only a fitter student. Self-management
// and the creative skill feed nothing here: composure comes from condition, and no activity makes the creative skill.
// The practice window (PracticeWindow.h) already makes cramming pay less than regular practice, so a student who studies once or
// twice a day banks more than one who studies five times; the divisors put a steady ten days near 220 points and let a cap
// (CountdownDayLoopPrepCap) stop any one habit from filling a dimension alone.
// Settled with M4-04: the tower does not re-map skills; it reads runner metrics per TowerMethod, whose order and ids
// equal CountdownDimension's (TowerTests checks), so this day-loop credit mapping stays the countdown's own.
constexpr CreditRule CreditRules[] = {
    {"skill.academics", CountdownDimension::Analysis, 100},
    {"skill.computing", CountdownDimension::Analysis, 100},
    {"skill.fitness", CountdownDimension::Endurance, 60},
    {"skill.fitness", CountdownDimension::Combat, 200},
    {"skill.practical_repair", CountdownDimension::Construction, 100},
    {"skill.household_tasks", CountdownDimension::Logistics, 120},
    {"skill.logistics", CountdownDimension::Logistics, 100},
    {"skill.commerce", CountdownDimension::Logistics, 150},
    {"skill.communication", CountdownDimension::Negotiation, 100},
};

CountdownPoints CountdownPreparationOf(const CountdownState& state)
{
    CountdownPoints points{};
    for (std::size_t i = 0; i < CountdownDimensionCount; ++i)
        points[i] = std::clamp(std::min(state.DayLoopPrep[i], CountdownDayLoopPrepCap) + state.OutcomePrep[i], 0, CountdownScoreMax);
    return points;
}
} // namespace

CountdownPoints CountdownMapCredits(const std::vector<SkillCredit>& credits)
{
    CountdownPoints points{};
    for (const SkillCredit& credit : credits)
    {
        for (const CreditRule& rule : CreditRules)
        {
            if (rule.Skill == credit.SkillId && credit.MilliXp > 0)
                points[static_cast<std::size_t>(rule.Dimension)] += credit.MilliXp / rule.MilliPerPoint;
        }
    }
    return points;
}

std::int32_t CountdownLogisticsRelief(const CountdownState& state)
{
    const CountdownWorldEventDef& event = CountdownWorldEvent();
    const std::int32_t logistics = CountdownPreparationOf(state)[static_cast<std::size_t>(CountdownDimension::Logistics)];
    return std::min(event.LogisticsMaxMinutes, logistics / event.LogisticsPointsPerMinute);
}

std::int32_t CountdownRouteDelay(const CountdownState& state)
{
    if (!state.EventFired)
        return 0;
    return std::max(0, CountdownWorldEvent().BaseDelayMinutes - state.RouteMinutes - CountdownLogisticsRelief(state));
}

CountdownNews CountdownNewsOf(const WorldState& world, const CountdownState& state, const CountdownContext& context)
{
    CountdownNews news;
    news.Fired = state.EventFired;
    if (!news.Fired)
        return news;
    news.At = state.EventOnset;
    news.DelayNow = CountdownRouteDelay(state);
    news.DelayUnmitigated = CountdownWorldEvent().BaseDelayMinutes;
    for (const CountdownOffer& offer : CountdownOffers(world, state, context))
    {
        if (offer.Def->Group == "response" && offer.Availability == CountdownAvailability::Open)
            news.Responses.emplace_back(offer.Def->Id);
    }
    return news;
}

// Design choice (M4-03): composure on the morning is three quarters of the condition the day loop leaves (energy weighs most, then
// stress, then hunger), plus what outcomes added, less the route delay still standing. The quarter held back is what a quiet day or a
// good conversation can add, so a well-rested student still has something to gain from the last days.
CountdownProfile CountdownProfileOf(const WorldState& world, const CountdownState& state)
{
    CountdownProfile profile;
    profile.Final = state.Closed;
    profile.Preparation = CountdownPreparationOf(state);
    const ConditionState& condition = world.DayLoop.Condition;
    const std::int64_t badness =
        (static_cast<std::int64_t>(MeterMax - condition.Energy) * 500 + static_cast<std::int64_t>(condition.Stress) * 300 +
         static_cast<std::int64_t>(condition.Hunger) * 200) /
        MeterMax;
    profile.RouteDelayMinutes = CountdownRouteDelay(state);
    const CountdownWorldEventDef& event = CountdownWorldEvent();
    const std::int32_t routeCost = std::min(event.RouteComposureCap, profile.RouteDelayMinutes * 2 / 3);
    profile.Composure = std::clamp(static_cast<std::int32_t>((CountdownScoreMax - badness) * 3 / 4) + state.Composure - routeCost, 0, CountdownScoreMax);
    profile.Goodwill = std::clamp(CountdownGoodwillStart + state.Goodwill, 0, CountdownScoreMax);
    for (const CountdownObligationRecord& record : state.Obligations)
    {
        switch (record.Disposition)
        {
        case CountdownDisposition::Standing: profile.Honoured += record.Done ? 1 : 0; break;
        case CountdownDisposition::Shared: ++profile.Shared; break;
        case CountdownDisposition::Deferred: ++profile.Deferred; break;
        case CountdownDisposition::Declined: ++profile.Declined; break;
        }
    }
    for (const std::int32_t points : profile.Preparation)
        profile.Total += points;
    profile.Total += profile.Composure + profile.Goodwill;
    return profile;
}

// Design choice (M4-03): "comparable" is a spread of at most CountdownComparableSpreadPermille (a quarter) between the
// two totals of preparation, composure and goodwill. "Different" is a distance of at least CountdownDistinctDistance between the eight
// scores. Neither is a ranking: the game never shows the total, and a profile that dominates another is a bug in the tables.
bool CountdownProfilesComparable(const CountdownProfile& a, const CountdownProfile& b) noexcept
{
    const std::int64_t larger = std::max(a.Total, b.Total);
    const std::int64_t difference = std::abs(static_cast<std::int64_t>(a.Total) - b.Total);
    return difference * 1000 <= larger * CountdownComparableSpreadPermille;
}

std::int32_t CountdownProfileDistance(const CountdownProfile& a, const CountdownProfile& b) noexcept
{
    std::int32_t distance = std::abs(a.Composure - b.Composure) + std::abs(a.Goodwill - b.Goodwill);
    for (std::size_t i = 0; i < CountdownDimensionCount; ++i)
        distance += std::abs(a.Preparation[i] - b.Preparation[i]);
    return distance;
}

bool CountdownProfilesDiffer(const CountdownProfile& a, const CountdownProfile& b) noexcept
{
    return CountdownProfileDistance(a, b) >= CountdownDistinctDistance;
}

bool CountdownProfileDominates(const CountdownProfile& a, const CountdownProfile& b) noexcept
{
    if (a.Composure < b.Composure || a.Goodwill < b.Goodwill)
        return false;
    for (std::size_t i = 0; i < CountdownDimensionCount; ++i)
    {
        if (a.Preparation[i] < b.Preparation[i])
            return false;
    }
    return true;
}
} // namespace PerfectWorld
