#include "CountdownContent.h"

#include "ConductLedger.h"

#include <algorithm>
#include <set>

// The authored tables of the ten days and the rules that keep them sound (ROADMAP M4-03).
// Design choice (M4-03): authored content lives in constexpr tables here, as the other portable
// modules hold theirs, not in Data/Content records. Moving them into content records needs a
// schema in ContentSchemas; the fields are already plain data, so the move is mechanical.
namespace PerfectWorld
{
namespace
{
using Pass = DayPassKind;

/** One outcome: six preparation points (combat, analysis, endurance, construction, negotiation, logistics), then composure, goodwill and route minutes. */
constexpr CountdownEffect Fx(std::int32_t combat, std::int32_t analysis, std::int32_t endurance, std::int32_t construction,
                             std::int32_t negotiation, std::int32_t logistics, std::int32_t composure, std::int32_t goodwill,
                             std::int32_t route)
{
    CountdownEffect effect;
    effect.Preparation = {combat, analysis, endurance, construction, negotiation, logistics};
    effect.Composure = composure;
    effect.Goodwill = goodwill;
    effect.RouteMinutes = route;
    return effect;
}

constexpr std::int32_t Saturday = 1;
constexpr std::int32_t Sunday = 2;
constexpr std::int32_t RepublicDay = 3;
constexpr std::int32_t Tuesday = 4;
constexpr std::int32_t NewsDay = 5;
constexpr std::int32_t Thursday = 6;
constexpr std::int32_t Friday = 7;
constexpr std::int32_t LastSaturday = 8;
constexpr std::int32_t LastSunday = 9;

constexpr CountdownBlockDef Study{"activity.study_at_home", "home", 120, Pass::Wait};
constexpr CountdownBlockDef Run{"activity.exercise_run", "outdoors", 45, Pass::Wait};
constexpr CountdownBlockDef Chores{"activity.household_chores", "home", 60, Pass::Wait};

constexpr std::string_view NoRole{};

// Design choice (M4-03): all numbers below are production assumptions. The documents fix the shape
// (opportunities shaped by relationships, uncertain but disclosed outcomes, negotiable obligations,
// one world event) and the requirement that several strategies are defensible, not any value.
// Odds are permille, summing to 1000. A good outcome is worth more than a fair one and a poor one still
// yields something small, so a bad draw is a smaller gain, not a lost afternoon with nothing.
constexpr CountdownOpportunityDef Opportunities[] = {
    // Saturday, one invitation only.
    {"opp.study_circle", "Revision circle at a friend's table", "saturday", Saturday, 540, Study, "friend", CountdownRelation::Trust,
     5000, {300, 550, 150}, 50, 250,
     {Fx(0, 90, 0, 0, 20, 0, 30, 25, 0), Fx(0, 60, 0, 0, 10, 0, 10, 10, 0), Fx(0, 25, 0, 0, 0, 0, -20, 0, 0)}, false,
     RelationshipActKind::ShareTime},
    {"opp.ground_trial", "Open trial at the sports ground", "saturday", Saturday, 600, Run, NoRole, CountdownRelation::Trust, 0,
     {250, 500, 250}, 0, 0, {Fx(90, 0, 60, 0, 0, 0, 10, 0, 0), Fx(55, 0, 40, 0, 0, 0, 0, 0, 0), Fx(20, 0, 20, 0, 0, 0, -30, 0, 0)}, false,
     RelationshipActKind::ShareTime},
    {"opp.repair_workshop", "Bench-repair afternoon at the neighbour's workshop", "saturday", Saturday, 840, Chores, "neighbour",
     CountdownRelation::Respect, 6000, {300, 500, 200}, 40, 250,
     {Fx(0, 0, 0, 100, 10, 20, 20, 20, 0), Fx(0, 0, 0, 65, 0, 10, 10, 10, 0), Fx(0, 0, 0, 25, 0, 0, -10, 0, 0)}, false,
     RelationshipActKind::Help},
    // Sunday.
    {"opp.community_kitchen", "Shift at the ward relief kitchen", "sunday", Sunday, 600, {"", "", 180, Pass::Walk}, NoRole,
     CountdownRelation::Trust, 0, {250, 550, 200}, 0, 0,
     {Fx(0, 0, 30, 0, 60, 90, 10, 40, 0), Fx(0, 0, 20, 0, 35, 60, 0, 25, 0), Fx(0, 0, 10, 0, 10, 25, -20, 5, 0)}, false,
     RelationshipActKind::Help},
    {"opp.mock_debate", "Inter-school debate evening at the youth club", "sunday", Sunday, 900, {"", "", 90, Pass::Wait}, "rival",
     CountdownRelation::Respect, 3000, {250, 450, 300}, 40, 250,
     {Fx(0, 25, 0, 0, 100, 0, 15, 15, 0), Fx(0, 15, 0, 0, 60, 0, 0, 5, 0), Fx(0, 5, 0, 0, 20, 0, -35, -10, 0)}, false,
     RelationshipActKind::ShareTime},
    // Midweek.
    {"opp.teacher_review", "Office hour with a teacher", "midweek", Tuesday, 990, {"", "", 90, Pass::Wait}, "teacher",
     CountdownRelation::Respect, 8000, {350, 500, 150}, 60, 300,
     {Fx(0, 115, 0, 0, 15, 0, 25, 20, 0), Fx(0, 80, 0, 0, 5, 0, 10, 5, 0), Fx(0, 30, 0, 0, 0, 0, -10, 0, 0)}, false,
     RelationshipActKind::ShareTime},
    {"opp.practical_bench", "Open practical bench at the science lab", "midweek", Tuesday, 900, {"", "", 120, Pass::Wait}, NoRole,
     CountdownRelation::Trust, 0, {300, 500, 200}, 0, 0,
     {Fx(0, 25, 0, 90, 0, 30, 10, 0, 0), Fx(0, 15, 0, 60, 0, 20, 0, 0, 0), Fx(0, 5, 0, 20, 0, 0, -15, 0, 0)}, false,
     RelationshipActKind::ShareTime},
    // The last Saturday: a hard paper or a quiet day, one or the other.
    {"opp.paper_marathon", "A full mock paper under examination conditions", "closing", LastSaturday, 540, Study, NoRole,
     CountdownRelation::Trust, 0, {250, 450, 300}, 0, 0,
     {Fx(0, 140, 0, 0, 0, 0, -10, 0, 0), Fx(0, 95, 0, 0, 0, 0, -25, 0, 0), Fx(0, 40, 0, 0, 0, 0, -60, 0, 0)}, false,
     RelationshipActKind::ShareTime},
    {"opp.quiet_day", "A quiet day: rest and a walk by the river", "closing", LastSaturday, 600, {"", "", 240, Pass::Rest}, NoRole,
     CountdownRelation::Trust, 0, {800, 200, 0}, 0, 0,
     {Fx(0, 0, 10, 0, 0, 0, 120, 0, 0), Fx(0, 0, 0, 0, 0, 0, 70, 0, 0), Fx(0, 0, 0, 0, 0, 0, 30, 0, 0)}, false,
     RelationshipActKind::ShareTime},
    // Responses to the news, open only once the Gate has broken. They shorten the examination-morning journey.
    {"opp.relief_shift", "Relief shift at the ward hall", "response", Thursday, 960, {"", "", 150, Pass::Walk}, NoRole,
     CountdownRelation::Trust, 0, {300, 550, 150}, 0, 0,
     {Fx(0, 0, 20, 0, 40, 70, 0, 35, 60), Fx(0, 0, 15, 0, 25, 50, 0, 20, 45), Fx(0, 0, 10, 0, 10, 20, -20, 5, 25)}, true,
     RelationshipActKind::Help},
    {"opp.scout_route", "Walk and cycle the alternative routes yourself", "response", LastSaturday, 570, {"", "", 120, Pass::Walk}, NoRole,
     CountdownRelation::Trust, 0, {300, 500, 200}, 0, 0,
     {Fx(0, 0, 50, 0, 0, 50, 15, 0, 70), Fx(0, 0, 35, 0, 0, 30, 5, 0, 50), Fx(0, 0, 25, 0, 0, 10, -10, 0, 30)}, true,
     RelationshipActKind::ShareTime},
    {"opp.friend_ride", "Walk the route with a friend and agree the morning", "response", LastSunday, 1020, {"", "", 45, Pass::Wait},
     "friend", CountdownRelation::Trust, 20000, {500, 400, 100}, 50, 300,
     {Fx(0, 0, 0, 0, 20, 30, 30, 25, 80), Fx(0, 0, 0, 0, 10, 15, 15, 10, 60), Fx(0, 0, 0, 0, 0, 0, 0, 0, 25)}, true,
     RelationshipActKind::Help},
};

// Obligations: kept by default (the block goes into the plan). Each can be shared, deferred or declined once.
constexpr CountdownObligationDef Obligations[] = {
    {"oblig.sibling_pickup", "Collect your sibling from tuition and see them home", "sibling", Saturday, 960, 60, Pass::Walk, 15, -45,
     CountdownRelation::Trust, 50, 300, Sunday, 960, 450, -10, -10, "friend", 8000, 30, 450, -15, -5},
    {"oblig.neighbour_shop_shift", "Cover a shift at the neighbour's shop", "neighbour", Sunday, 660, 120, Pass::Walk, 20, -50,
     CountdownRelation::Trust, 50, 300, RepublicDay, 660, 450, -10, -10, "friend", 8000, 60, 450, -15, -5},
    {"oblig.teacher_hall_setup", "Help set out the examination hall", "teacher", Thursday, 900, 60, Pass::Walk, 15, -40,
     CountdownRelation::Respect, 50, 300, Friday, 900, 450, -10, -10, "friend", 8000, 30, 450, -15, -5},
};

// The Gate breaks on the evening of Wednesday 28 January, as the news bulletin ends. The closure of the canal road
// would add ninety minutes to the journey on examination morning if nothing were done.
constexpr CountdownWorldEventDef WorldEvent{"gate.kolkata_canal_ward9", "news.canal_gate_break", "A Gate breaks in the canal ward",
                                            GateRank::E, NewsDay, 1180, 90, 25, 20, 60};

constexpr std::size_t OpportunityCount = sizeof(Opportunities) / sizeof(Opportunities[0]);
constexpr std::size_t ObligationCount = sizeof(Obligations) / sizeof(Obligations[0]);
static_assert(OpportunityCount <= CountdownMaxCommitments, "every opportunity can be committed");
static_assert(ObligationCount <= CountdownMaxObligations, "every obligation fits the state");

bool CountdownTokenOk(std::string_view id)
{
    return IsConductTokenFair(id);
}

bool CountdownSlotInside(std::int32_t day, std::int32_t minute, std::int32_t minutes)
{
    return day >= 0 && day < CountdownDays && minute >= 0 && minutes >= 1 && minute + minutes <= MinutesPerDay &&
           CountdownInstantOf(day, minute) >= CountdownStartInstant() && CountdownInstantOf(day, minute) + minutes <= CountdownEndInstant();
}

void CountdownCheckOpportunity(const CountdownOpportunityDef& def, const ActivityCatalogue& activities, std::vector<std::string>& issues)
{
    const std::string id(def.Id);
    const auto fail = [&](const std::string& what) { issues.push_back(id + ": " + what); };
    if (!CountdownTokenOk(def.Id) || def.Id.rfind("opp.", 0) != 0)
        fail("the id is not a fair stable token starting opp.");
    if (!CountdownTokenOk(def.Group))
        fail("the group is not a fair stable token");
    if (!def.Role.empty() && !CountdownTokenOk(def.Role))
        fail("the role is not a fair stable token");
    if (def.Role.empty() && (def.MinRelation != 0 || def.SkewStep != 0 || def.SkewCap != 0))
        fail("an opportunity with no contact has no relationship needs or skew");
    if (def.SkewStep < 0 || def.SkewCap < 0 || def.SkewCap > 1000 || def.MinRelation < 0 || def.MinRelation > RelationshipMilliMax)
        fail("the relationship needs or skew are out of range");
    std::int32_t sum = 0;
    for (const std::int32_t odds : def.BaseOdds)
    {
        if (odds < 0)
            fail("an odd is negative");
        sum += odds;
    }
    if (sum != 1000)
        fail("the odds sum to " + std::to_string(sum) + ", not 1000");
    if (!CountdownSlotInside(def.Day, def.StartMinute, def.Block.Minutes))
        fail("the window is not inside one calendar day of the span");
    if (def.NeedsNews && CountdownInstantOf(def.Day, def.StartMinute) <= CountdownInstantOf(WorldEvent.Day, WorldEvent.Minute))
        fail("a response must start after the news");
    if (def.Group == "response" && !def.NeedsNews)
        fail("a response needs the news");
    if (def.Block.ActivityId.empty())
    {
        if (!def.Block.PlaceKind.empty())
            fail("time spent names no place");
    }
    else
    {
        const ActivityDef* activity = activities.Find(def.Block.ActivityId);
        if (activity == nullptr)
            fail("the activity does not exist");
        else if (activity->DurationMinutes != def.Block.Minutes)
            fail("the block is " + std::to_string(def.Block.Minutes) + " minutes, the activity " + std::to_string(activity->DurationMinutes));
        else if (activity->Place != def.Block.PlaceKind)
            fail("the place kind is not the activity's");
    }
    for (const CountdownEffect& effect : def.Effects)
    {
        for (const std::int32_t points : effect.Preparation)
        {
            if (points < 0 || points > CountdownScoreMax)
                fail("a preparation effect is out of range");
        }
        if (effect.RouteMinutes < 0 || effect.RouteMinutes > WorldEvent.BaseDelayMinutes || effect.Goodwill < -200 || effect.Goodwill > 200 ||
            effect.Composure < -200 || effect.Composure > 200)
            fail("an effect is out of range");
        if (def.Group != "response" && effect.RouteMinutes != 0)
            fail("only a response changes the route");
    }
    if (def.Effects[0].Preparation == CountdownPoints{} && def.Effects[0].Composure <= 0 && def.Effects[0].Goodwill <= 0 &&
        def.Effects[0].RouteMinutes == 0)
        fail("a good outcome gives nothing");
}

void CountdownCheckObligation(const CountdownObligationDef& def, std::vector<std::string>& issues)
{
    const std::string id(def.Id);
    const auto fail = [&](const std::string& what) { issues.push_back(id + ": " + what); };
    if (!CountdownTokenOk(def.Id) || def.Id.rfind("oblig.", 0) != 0 || !CountdownTokenOk(def.Role) || !CountdownTokenOk(def.HelperRole))
        fail("an id or role is not a fair stable token");
    if (!CountdownSlotInside(def.Day, def.StartMinute, def.Minutes))
        fail("the slot is not inside one calendar day of the span");
    if (!CountdownSlotInside(def.DeferDay, def.DeferMinute, def.Minutes) ||
        CountdownInstantOf(def.DeferDay, def.DeferMinute) <= CountdownInstantOf(def.Day, def.StartMinute))
        fail("the deferred slot must be later and inside the span");
    if (def.ShareMinutes < 1 || def.ShareMinutes >= def.Minutes)
        fail("sharing must leave fewer minutes than the whole");
    if (def.DeferBase < 0 || def.DeferBase > 1000 || def.ShareBase < 0 || def.ShareBase > 1000 || def.SkewStep < 0 || def.SkewCap < 0)
        fail("a negotiation chance is out of range");
    if (def.HonourGoodwill <= 0 || def.DeclineGoodwill >= 0 || def.DeferAcceptedGoodwill > 0 || def.DeferRefusedGoodwill > 0 ||
        def.ShareAcceptedGoodwill > 0 || def.ShareRefusedGoodwill > 0)
        fail("keeping earns standing and every negotiation costs some or none");
    if (def.HelperMinRelation < 0 || def.HelperMinRelation > RelationshipMilliMax)
        fail("the helper's relationship need is out of range");
    if (def.Pass != Pass::Wait && def.Pass != Pass::Walk && def.Pass != Pass::Rest)
        fail("the pass kind is unknown");
}
} // namespace

std::span<const CountdownOpportunityDef> CountdownOpportunities() noexcept
{
    return {Opportunities, OpportunityCount};
}

std::span<const CountdownObligationDef> CountdownObligations() noexcept
{
    return {Obligations, ObligationCount};
}

const CountdownWorldEventDef& CountdownWorldEvent() noexcept
{
    return WorldEvent;
}

const CountdownOpportunityDef* CountdownFindOpportunity(std::string_view id) noexcept
{
    for (const CountdownOpportunityDef& def : Opportunities)
    {
        if (def.Id == id)
            return &def;
    }
    return nullptr;
}

const CountdownObligationDef* CountdownFindObligation(std::string_view id) noexcept
{
    for (const CountdownObligationDef& def : Obligations)
    {
        if (def.Id == id)
            return &def;
    }
    return nullptr;
}

GameInstant CountdownStartInstant() noexcept
{
    return GameClock::FromCalendar(2043, 1, 23, 18, 0);
}

GameInstant CountdownEndInstant() noexcept
{
    return GameClock::FromCalendar(2043, 2, 2, 6, 0);
}

GameInstant CountdownInstantOf(std::int32_t day, std::int32_t minute) noexcept
{
    return GameClock::FromCalendar(2043, 1, 23, 0, 0) + static_cast<GameInstant>(day) * MinutesPerDay + minute;
}

std::vector<std::string> ValidateCountdownContent(const ActivityCatalogue& activities)
{
    std::vector<std::string> issues;
    if (CountdownEndInstant() - CountdownStartInstant() != (CountdownDays - 1) * MinutesPerDay + 12 * MinutesPerHour)
        issues.push_back("the span is not 18:00 on Awakening Day to 06:00 ten calendar days later");
    if (CountdownDays != CalendarAwakeningToExamDays)
        issues.push_back("the span is not the calendar's ten days");
    std::set<std::string_view> ids;
    for (const CountdownOpportunityDef& def : Opportunities)
    {
        if (!ids.insert(def.Id).second)
            issues.push_back(std::string(def.Id) + ": the id is repeated");
        CountdownCheckOpportunity(def, activities, issues);
    }
    for (const CountdownObligationDef& def : Obligations)
    {
        if (!ids.insert(def.Id).second)
            issues.push_back(std::string(def.Id) + ": the id is repeated");
        CountdownCheckObligation(def, issues);
    }
    if (WorldEvent.GateId.rfind("gate.", 0) != 0 || !CountdownTokenOk(WorldEvent.GateId) || !CountdownTokenOk(WorldEvent.NewsId) ||
        !CountdownSlotInside(WorldEvent.Day, WorldEvent.Minute, 1) || WorldEvent.BaseDelayMinutes < 1 || WorldEvent.LogisticsPointsPerMinute < 1)
        issues.push_back("the world event is malformed or outside the span");
    // Only one event is scheduled, and the response group must have somewhere to go after it.
    std::int32_t responses = 0;
    for (const CountdownOpportunityDef& def : Opportunities)
        responses += def.Group == "response" ? 1 : 0;
    if (responses < 2)
        issues.push_back("the news has fewer than two responses");
    return issues;
}
} // namespace PerfectWorld
