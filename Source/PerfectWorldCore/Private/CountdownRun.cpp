#include "Countdown.h"

#include "ConductLedger.h"
#include "CountdownInternal.h"
#include "GateSites.h"
#include "IdentityRegistry.h"
#include "WorldReducer.h"

#include <algorithm>
#include <utility>

// Opening the countdown and resolving its days (ROADMAP M4-03). Time passes only through CompressWeek,
// which is the day loop and the reducer; this file adds the committed blocks to the plan, decides nothing
// about what the player does with the rest, and then settles what the elapsed time resolved.
namespace PerfectWorld
{
namespace
{
std::int64_t CountdownEntryMinutes(const WeekEntry& entry, const ActivityCatalogue& activities)
{
    switch (entry.Kind)
    {
    case WeekEntryKind::Activity:
    {
        const ActivityDef* def = activities.Find(entry.ActivityId);
        return def == nullptr ? 0 : def->DurationMinutes;
    }
    case WeekEntryKind::School: return 0;
    case WeekEntryKind::Sleep:
    case WeekEntryKind::Spend: return entry.Minutes;
    }
    return 0;
}

CountdownIssue CountdownCheckContext(const CountdownContext& context)
{
    if (!IsValidIdentityId(context.Player))
        return CountdownMakeIssue(CountdownError::BadContext, "the player's identity id is not valid");
    for (const CountdownContact& contact : context.Contacts)
    {
        if (!IsConductTokenFair(contact.Role) || !IsValidIdentityId(contact.Person) || contact.Person == context.Player)
            return CountdownMakeIssue(CountdownError::BadContext, "a contact has a bad role or identity id: " + contact.Role);
    }
    return {};
}

/** Puts the blocks that start inside the plan's days into the plan and refuses a player entry that lands on one. */
CountdownIssue CountdownAddBlocks(const WorldState& world, const CountdownState& state, const DayLoopCatalogues& cats, WeekPlan& plan)
{
    const GameInstant dayZero = GameClock::StartOfDay(world.DayLoop.Now);
    std::vector<std::pair<GameInstant, GameInstant>> player;
    for (std::size_t d = 0; d < plan.Days.size(); ++d)
    {
        for (const WeekEntry& entry : plan.Days[d].Entries)
        {
            const GameInstant start = dayZero + static_cast<GameInstant>(d) * MinutesPerDay + entry.StartMinute;
            const std::int64_t minutes = CountdownEntryMinutes(entry, *cats.Activities);
            if (minutes > 0)
                player.emplace_back(start, start + minutes);
        }
    }
    for (const CountdownReservedBlock& block : CountdownReservedBlocks(state))
    {
        const std::int64_t index = GameClock::DayNumber(block.Start) - GameClock::DayNumber(dayZero);
        if (index < 0 || index >= static_cast<std::int64_t>(plan.Days.size()))
            continue;
        for (const auto& [start, end] : player)
        {
            if (CountdownOverlaps(block.Start, block.End, start, end))
                return CountdownMakeIssue(CountdownError::PlanConflict, block.Id);
        }
        plan.Days[static_cast<std::size_t>(index)].Entries.push_back(block.Entry);
    }
    for (WeekDayPlan& day : plan.Days)
    {
        std::stable_sort(day.Entries.begin(), day.Entries.end(),
                         [](const WeekEntry& a, const WeekEntry& b) { return a.StartMinute < b.StartMinute; });
    }
    return {};
}

/** Spends whatever is left of the span awake, in pieces that stay inside one calendar day, as a standing order the receipt discloses. */
CountdownIssue CountdownAddPad(const WorldState& world, const CountdownState& state, GameInstant planEnd, WeekPlan& plan, std::int32_t& padded)
{
    const GameInstant dayZero = GameClock::StartOfDay(world.DayLoop.Now);
    for (GameInstant cursor = planEnd; cursor < state.End;)
    {
        const std::int32_t minute = CountdownMinuteOfDay(cursor);
        const std::int64_t chunk = std::min<std::int64_t>(state.End - cursor, MinutesPerDay - minute);
        const std::size_t index = static_cast<std::size_t>(GameClock::DayNumber(cursor) - GameClock::DayNumber(dayZero));
        if (index >= static_cast<std::size_t>(WeekMaxDays))
            return CountdownMakeIssue(CountdownError::TooManyDays, "closing the countdown needs more than a week of days");
        if (plan.Days.size() <= index)
            plan.Days.resize(index + 1);
        WeekEntry entry;
        entry.Kind = WeekEntryKind::Spend;
        entry.StartMinute = minute;
        entry.Minutes = static_cast<std::int32_t>(chunk);
        entry.Pass = DayPassKind::Wait;
        entry.DecidedByOrder = true;
        entry.OrderId = std::string(CountdownPadOrderId);
        plan.Days[index].Entries.push_back(std::move(entry));
        padded += static_cast<std::int32_t>(chunk);
        cursor += chunk;
    }
    return {};
}

/** What the elapsed time resolved: learning, the news, outcomes, obligations kept and the close. */
void CountdownSettle(const WorldState& after, CountdownState& state, const WeekResult& week, std::int32_t padded)
{
    const GameInstant now = after.DayLoop.Now;
    const CountdownWorldEventDef& event = CountdownWorldEvent();
    const CountdownPoints learned = CountdownMapCredits(week.Ledger.Credits);
    for (std::size_t i = 0; i < CountdownDimensionCount; ++i)
        state.DayLoopPrep[i] += learned[i];
    const std::size_t firstNew = state.Ledger.size();
    const std::size_t firstRequest = state.Requests.size();

    const auto gate = after.Gates.find(event.GateId);
    if (!state.EventFired && gate != after.Gates.end() && gate->second.OnsetCount > 0)
    {
        state.EventFired = true;
        CountdownAddLedger(state, state.EventOnset, CountdownLedgerKind::News, std::string(event.NewsId), event.BaseDelayMinutes, 0);
    }
    for (CountdownCommitment& commitment : state.Commitments)
    {
        const CountdownOpportunityDef* def = CountdownFindOpportunity(commitment.Id);
        if (commitment.Resolved || def == nullptr || commitment.End > now)
            continue;
        commitment.Roll = CountdownRoll(commitment.Seed);
        commitment.Outcome = CountdownOutcomeFor(commitment.Odds, commitment.Roll);
        commitment.Resolved = true;
        CountdownApplyEffect(state, def->Effects[static_cast<std::size_t>(commitment.Outcome)]);
        CountdownAddLedger(state, commitment.End, CountdownLedgerKind::Resolved, commitment.Id, static_cast<std::int32_t>(commitment.Outcome),
                           commitment.Roll);
        if (!commitment.Contact.empty() && commitment.Outcome != CountdownOutcome::Poor && state.Requests.size() < CountdownMaxRequests)
            state.Requests.push_back({state.Player, commitment.Contact, def->Act, commitment.Id, commitment.End});
    }
    for (CountdownObligationRecord& record : state.Obligations)
    {
        const CountdownObligationDef* def = CountdownFindObligation(record.Id);
        if (record.Done || record.Disposition == CountdownDisposition::Declined || def == nullptr || record.Start + record.Minutes > now)
            continue;
        record.Done = true;
        state.Goodwill += def->HonourGoodwill;
        CountdownAddLedger(state, record.Start + record.Minutes, CountdownLedgerKind::Honoured, record.Id, def->HonourGoodwill, record.Minutes);
        if (state.Requests.size() < CountdownMaxRequests)
            state.Requests.push_back({state.Player, record.Person, RelationshipActKind::Help, record.Id, record.Start + record.Minutes});
    }
    state.PaddedMinutes += padded;
    if (now == state.End)
    {
        state.Closed = true;
        CountdownAddLedger(state, now, CountdownLedgerKind::Closed, "exam_morning", state.PaddedMinutes, 0);
    }
    // Chronological whatever the batching: the days resolved in one call or several give the same state.
    std::stable_sort(state.Ledger.begin() + static_cast<std::ptrdiff_t>(firstNew), state.Ledger.end(),
                     [](const CountdownLedgerLine& a, const CountdownLedgerLine& b) { return a.At < b.At; });
    std::stable_sort(state.Requests.begin() + static_cast<std::ptrdiff_t>(firstRequest), state.Requests.end(),
                     [](const CountdownRequest& a, const CountdownRequest& b) { return a.At < b.At; });
}
} // namespace

CountdownOpenResult OpenCountdown(const WorldState& world, const CountdownContext& context, const DayLoopCatalogues& catalogues)
{
    CountdownOpenResult result;
    result.World = world;
    const auto refuse = [&result](CountdownIssue issue) {
        result.Issue = std::move(issue);
        return std::move(result);
    };
    if (catalogues.Activities == nullptr)
        return refuse(CountdownMakeIssue(CountdownError::MissingCatalogue, "no activity catalogue"));
    if (catalogues.Calendar == nullptr)
        return refuse(CountdownMakeIssue(CountdownError::MissingCalendar, "the school calendar is what keeps the ten days from being marked as absences"));
    if (const CountdownIssue busy = CountdownCheckWorld(world); !busy.Ok())
        return refuse(busy);
    // Design choice (M4-03): the countdown opens at exactly 18:00 on Awakening Day. Whatever the tutorial dungeon leaves of the day
    // before that is the integration's to spend (the day loop's evening phase starts at 18:00 as well).
    if (world.Clock.Now() != CountdownStartInstant())
        return refuse(CountdownMakeIssue(CountdownError::NotAtStart, "the countdown opens at 18:00 on Awakening Day"));
    if (const CountdownIssue bad = CountdownCheckContext(context); !bad.Ok())
        return refuse(bad);
    if (catalogues.Calendar->AwakeningDay != CalendarDayOfInstant(CountdownStartInstant()) ||
        catalogues.Calendar->ExaminationDay != CalendarDayOfInstant(CountdownEndInstant()))
        return refuse(CountdownMakeIssue(CountdownError::ContentInvalid, "the calendar's Awakening Day and examination are not the countdown's"));
    if (const std::vector<std::string> issues = ValidateCountdownContent(*catalogues.Activities); !issues.empty())
        return refuse(CountdownMakeIssue(CountdownError::ContentInvalid, issues.front()));
    const CountdownWorldEventDef& event = CountdownWorldEvent();
    const std::string gate(event.GateId);
    if (world.Gates.find(gate) != world.Gates.end())
        return refuse(CountdownMakeIssue(CountdownError::EventExists, gate));

    CountdownState state;
    state.Player = context.Player;
    state.Start = CountdownStartInstant();
    state.End = CountdownEndInstant();
    state.EventOnset = CountdownInstantOf(event.Day, event.Minute);
    state.Seed = CountdownOpenSeed(world.WorldSeed, state.Start);
    for (const CountdownObligationDef& def : CountdownObligations())
    {
        const std::string person = CountdownContactFor(context, def.Role);
        if (person.empty())
            continue;
        CountdownObligationRecord record;
        record.Id = std::string(def.Id);
        record.Person = person;
        record.Start = CountdownInstantOf(def.Day, def.StartMinute);
        record.Minutes = def.Minutes;
        state.Obligations.push_back(std::move(record));
    }
    CountdownAddLedger(state, state.Start, CountdownLedgerKind::Opened, "awakening_evening", static_cast<std::int32_t>(state.Obligations.size()), 0);

    // The event is the Gate system's: registered contained, its onset on the clock. It cannot be prevented.
    WorldSim sim(world);
    const CommandResult registered = sim.Submit({CommandSource::System, GateRegister{gate, event.Rank, GateStatus::Contained}});
    const CommandResult scheduled =
        registered.Accepted ? sim.Submit({CommandSource::System, GateScheduleOnset{gate, state.EventOnset}}) : registered;
    if (!scheduled.Accepted)
        return refuse(CountdownMakeIssue(CountdownError::ContentInvalid, scheduled.Why.Code));
    result.World = sim.State();
    result.State = std::move(state);
    return std::move(result);
}

CountdownResolveResult ResolveCountdownDays(const WorldState& world, const CountdownState& state, const std::vector<WeekDayPlan>& days,
                                            const DayLoopCatalogues& catalogues)
{
    CountdownResolveResult result;
    result.World = world;
    result.State = state;
    const auto refuse = [&result](CountdownIssue issue) {
        result.Issue = std::move(issue);
        return std::move(result);
    };
    if (state.Closed)
        return refuse(CountdownMakeIssue(CountdownError::Closed));
    if (catalogues.Activities == nullptr)
        return refuse(CountdownMakeIssue(CountdownError::MissingCatalogue, "no activity catalogue"));
    if (catalogues.Calendar == nullptr)
        return refuse(CountdownMakeIssue(CountdownError::MissingCalendar, "the school calendar is required"));
    if (const CountdownIssue busy = CountdownCheckWorld(world); !busy.Ok())
        return refuse(busy);
    const GameInstant now = world.DayLoop.Now;
    if (now < state.Start || now >= state.End)
        return refuse(CountdownMakeIssue(CountdownError::InvalidWorld, "the clock is outside the ten days"));
    if (days.empty())
        return refuse(CountdownMakeIssue(CountdownError::NoDays));
    if (days.size() > static_cast<std::size_t>(WeekMaxDays))
        return refuse(CountdownMakeIssue(CountdownError::TooManyDays));

    // The news is the span's one interrupt: stop after the day it arrives so the player can answer it.
    std::size_t use = days.size();
    const std::int64_t baseDay = GameClock::DayNumber(now);
    if (!state.EventFired)
    {
        const std::int64_t onsetIndex = GameClock::DayNumber(state.EventOnset) - baseDay;
        if (onsetIndex >= 0 && onsetIndex < static_cast<std::int64_t>(use))
            use = static_cast<std::size_t>(onsetIndex) + 1;
    }
    WeekPlan plan;
    plan.Seed = state.Seed;
    plan.Days.assign(days.begin(), days.begin() + static_cast<std::ptrdiff_t>(use));
    for (const WeekDayPlan& day : plan.Days)
    {
        for (const WeekEntry& entry : day.Entries)
        {
            if (entry.FirstOfKind || entry.NeedsChoice)
                return refuse(CountdownMakeIssue(CountdownError::NeedsPlay, "the ten days resolve routine days; play a first or a choice by hand"));
        }
    }
    if (const CountdownIssue conflict = CountdownAddBlocks(world, state, catalogues, plan); !conflict.Ok())
        return refuse(conflict);
    std::size_t entryCount = 0;
    for (const WeekDayPlan& day : plan.Days)
        entryCount += day.Entries.size();
    if (entryCount == 0)
        return refuse(CountdownMakeIssue(CountdownError::NoDays, "nothing is planned and nothing is committed"));

    const auto planIssue = [](const WeekPlanIssue& issue) {
        return CountdownMakeIssue(CountdownError::BadPlan, std::string(WeekErrorCode(issue.Error)) + ": " + issue.Detail);
    };
    const WeekTimePlanResult shape = BuildWeekTimePlan(world, plan, catalogues);
    if (!shape.Ok())
        return refuse(planIssue(shape.Issue));
    // The clock is public and cannot be extended: a plan that would run past 06:00 on examination morning is refused.
    const GameInstant planEnd = now + shape.Plan.SpanMinutes;
    if (planEnd > state.End)
        return refuse(CountdownMakeIssue(CountdownError::BeyondExam, "the plan ends after 06:00 on examination morning"));
    // Design choice (M4-03): when the plan covers the last day and leaves hours before 06:00 unspent, they are spent awake as a
    // standing order (CountdownPadOrderId), counted in PaddedMinutes and printed on the receipt: the countdown always ends at 06:00.
    std::int32_t padded = 0;
    if (baseDay + static_cast<std::int64_t>(use) - 1 >= GameClock::DayNumber(state.End) - 1 && planEnd < state.End)
    {
        if (const CountdownIssue pad = CountdownAddPad(world, state, planEnd, plan, padded); !pad.Ok())
            return refuse(pad);
    }

    WeekResult week = CompressWeek(world, plan, catalogues);
    if (!week.Ok())
        return refuse(planIssue(week.Issue));
    if (week.Stop != WeekStop::None)
        return refuse(CountdownMakeIssue(CountdownError::NeedsPlay, std::string(WeekStopName(week.Stop))));

    result.World = std::move(week.World);
    CountdownSettle(result.World, result.State, week, padded);
    result.DaysResolved = static_cast<std::int32_t>(use);
    result.StoppedForNews = !state.EventFired && result.State.EventFired;
    result.Closed = result.State.Closed;
    result.Events.assign(result.State.Ledger.begin() + static_cast<std::ptrdiff_t>(state.Ledger.size()), result.State.Ledger.end());
    result.Week = std::move(week);
    return result;
}
} // namespace PerfectWorld
