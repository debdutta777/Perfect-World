#include "WorldReducer.h"

#include "GateSites.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
bool R83IsLowerIdChar(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
}

/** "system/kind/key": three non-empty parts of stable-ID characters. */
bool R83IsScheduledEventId(std::string_view id)
{
    int parts = 1;
    std::size_t partLength = 0;
    for (const char c : id)
    {
        if (c == '/')
        {
            if (partLength == 0)
                return false;
            ++parts;
            partLength = 0;
        }
        else if (R83IsLowerIdChar(c))
        {
            ++partLength;
        }
        else
        {
            return false;
        }
    }
    return parts == 3 && partLength > 0;
}

bool R83IsFlagId(std::string_view id)
{
    return !id.empty() && id.size() <= 128 && std::all_of(id.begin(), id.end(), R83IsLowerIdChar) && id.front() != '.' && id.back() != '.';
}

Rejection R83Reject(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}
} // namespace

WorldSim::WorldSim(std::uint64_t worldSeed, std::size_t eventCap, DayLoopCatalogues school, WorldCommerceCatalogues commerce)
    : EventCap_(eventCap == 0 ? MaxEventsPerAdvance : eventCap), School_(school), Commerce_(commerce)
{
    State_ = MakeWorldState(worldSeed);
}

WorldSim::WorldSim(std::uint64_t worldSeed, const CharacterCreator& creation, std::size_t eventCap,
                   DayLoopCatalogues school, WorldCommerceCatalogues commerce)
    : State_(MakeWorldState(worldSeed, creation)), EventCap_(eventCap == 0 ? MaxEventsPerAdvance : eventCap),
      School_(school), Commerce_(commerce)
{
}

WorldSim::WorldSim(WorldState state, std::size_t eventCap, DayLoopCatalogues school, WorldCommerceCatalogues commerce)
    : State_(std::move(state)), EventCap_(eventCap == 0 ? MaxEventsPerAdvance : eventCap), School_(school), Commerce_(commerce)
{
    // R8-04: the save reader supplies authenticated commerce (v10) or the one-time household initialisation (v6 to v9), so
    // the snapshot is taken exactly as given; a slice out of step with the clock refuses time commands rather than being repaired.
}

std::optional<Rejection> WorldSim::ValidateTimeCharge(std::int64_t minutes) const
{
    if (!ServiceShop_.empty() && CommerceService(ServiceShop_).Ready())
        return R83Reject("time.service_open", "Close the counter panel before spending time.");
    // The player's day keeps the same clock (WorldState::DayLoop). While an activity runs the
    // day stands at its start, and while the player sleeps only waking moves time: a bare
    // advance would leave the day invalid or count the sleep twice. Those time commands move
    // the clock through the day loop's own commands.
    if (State_.DayLoop.Runner.Active())
        return R83Reject("time.activity_running", "An activity is running; finish or cancel it before time moves on.");
    if (State_.DayLoop.Asleep)
        return R83Reject("time.asleep", "The player is asleep; time moves on when they wake.");
    if (minutes > DayLoopMaxInstant - State_.Clock.Now())
        return R83Reject("time.beyond_calendar", "The calendar of a life ends one hundred years after it began.");
    return std::nullopt;
}

std::optional<Rejection> WorldSim::Validate(const Command& command) const
{
    if (const auto* advance = std::get_if<AdvanceTime>(&command.Payload))
    {
        if (advance->Minutes <= 0)
            return R83Reject("time.non_positive", "Time can only move forward, by at least one minute.");
        if (advance->Minutes > MaxSingleAdvanceMinutes)
            return R83Reject("time.too_long", "A single advance may not exceed ten years.");
        // Reading a conversation is free: its only time is what its choices charge.
        if (!OpenConversationPersonId(State_).empty())
            return R83Reject("time.conversation_open", "A conversation is open; only its choices spend time.");
        return ValidateTimeCharge(advance->Minutes);
    }
    if (std::holds_alternative<SettleDueEvents>(command.Payload))
    {
        if (!State_.Clock.HasDueEvents())
            return R83Reject("time.nothing_due", "Nothing is due at this instant.");
        return std::nullopt;
    }
    if (const auto* schedule = std::get_if<ScheduleWorldEvent>(&command.Payload))
    {
        if (command.Source != CommandSource::System && command.Source != CommandSource::Debug)
            return R83Reject("schedule.source", "Only systems schedule world events.");
        if (!R83IsScheduledEventId(schedule->EventId))
            return R83Reject("schedule.bad_id", "Scheduled event IDs are \"system/kind/key\".");
        if (schedule->When < State_.Clock.Now())
            return R83Reject("schedule.in_past", "An event cannot be scheduled in the past.");
        return std::nullopt;
    }
    if (const auto* flag = std::get_if<SetWorldFlag>(&command.Payload))
    {
        if (!R83IsFlagId(flag->FlagId))
            return R83Reject("flag.bad_id", "Flag IDs are lowercase letters, digits, '_', '-' and '.'.");
        return std::nullopt;
    }
    if (IsGateCommand(command.Payload))
        return ValidateGateCommand(State_, command);
    if (std::holds_alternative<ConversationOpen>(command.Payload) && !ServiceShop_.empty())
        return R83Reject("commerce.service_active", "Close the counter service first.");
    if (IsLaneCommand(command.Payload))
        return ValidateLaneCommand(command);
    if (std::holds_alternative<SchoolPlanDay>(command.Payload) || std::holds_alternative<SchoolArrive>(command.Payload))
        return ValidateSchoolCommand(command);
    if (std::holds_alternative<CommerceServiceRequest>(command.Payload) ||
        std::holds_alternative<CommerceServiceClose>(command.Payload) ||
        std::holds_alternative<CommerceBuy>(command.Payload) || std::holds_alternative<InventoryConsume>(command.Payload) ||
        std::holds_alternative<WorldDayCommand>(command.Payload))
        return std::nullopt; // Application stages these slices before mutation.
    return R83Reject("command.unknown", "Unknown command.");
}

void WorldSim::RunTimeTo(GameInstant target, CommandResult& result)
{
    std::size_t fired = 0;
    const auto record = [&](const AdvanceResult& advanced) {
        for (const ScheduledEvent& event : advanced.Fired)
        {
            result.Events.push_back({0, event.When, ScheduledEventFired{event.Handle, event.Id}});
            // Owning systems follow the fired event at its own instant: "gates/..." goes to the gate system.
            if (IsGateEventId(event.Id))
                RouteGateEvent(State_, event, result.Events);
            ++fired;
        }
    };
    const auto syncDay = [this] {
        // One clock: the day follows the world clock at every boundary, so a system reacting to an event sees both agree.
        State_.DayLoop.Now = State_.Clock.Now();
        State_.DayLoop.Phase = DayLoopPhaseOf(State_.DayLoop);
    };
    for (;;)
    {
        // First whatever is due at this very instant (scheduled for now, left by a capped advance, or scheduled by a
        // system reacting to an event here), without a minute passing.
        if (State_.Clock.HasDueEvents())
        {
            record(State_.Clock.DrainDue(EventCap_ - fired));
            if (State_.Clock.HasDueEvents() && fired >= EventCap_)
            {
                result.Partial = true;
                return;
            }
            continue;
        }
        const GameInstant now = State_.Clock.Now();
        if (now >= target)
            return;
        // A spent cap stops the advance where it is (the caller resubmits the rest), as it always has.
        if (fired >= EventCap_)
        {
            result.Partial = true;
            return;
        }
        // Step to whichever comes first: the next scheduled event, the next midnight, or the target. Midnight's
        // DayStarted comes before any event scheduled for that same instant.
        const GameInstant nextMidnight = GameClock::StartOfDay(now) + MinutesPerDay;
        GameInstant step = std::min(target, nextMidnight);
        const GameInstant nextEvent = State_.Clock.NextEventInstant();
        if (nextEvent >= 0 && nextEvent < step)
            step = nextEvent;
        const AdvanceResult advanced = State_.Clock.AdvanceToCapped(step, EventCap_ - fired);
        if (State_.Clock.Now() == now)
        {
            // Defensive: with room left the clock always reaches the step. Stop here, losing nothing.
            result.Partial = true;
            return;
        }
        syncDay();
        if (step == nextMidnight)
            result.Events.push_back({0, step, DayStarted{GameClock::DayNumber(step)}});
        record(advanced);
        if (advanced.Capped)
        {
            result.Partial = true;
            return;
        }
    }
}

void WorldSim::ChargeTime(std::int64_t minutes, CommandResult& result)
{
    const GameInstant from = State_.Clock.Now();
    const std::size_t firstEvent = result.Events.size();
    RunTimeTo(from + minutes, result);
    // The day follows the clock to wherever the advance stopped (a capped advance may stop short).
    State_.DayLoop.Now = State_.Clock.Now();
    State_.DayLoop.Phase = DayLoopPhaseOf(State_.DayLoop);
    result.Events.insert(result.Events.begin() + static_cast<std::ptrdiff_t>(firstEvent),
                         DomainEvent{0, from, TimeAdvanced{from, State_.Clock.Now()}});
}

std::optional<Rejection> WorldSim::Apply(const Command& command, CommandResult& result)
{
    if (const auto* advance = std::get_if<AdvanceTime>(&command.Payload))
    {
        ChargeTime(advance->Minutes, result);
    }
    else if (std::holds_alternative<SettleDueEvents>(command.Payload))
    {
        RunTimeTo(State_.Clock.Now(), result);
    }
    else if (const auto* schedule = std::get_if<ScheduleWorldEvent>(&command.Payload))
    {
        (void)State_.Clock.Schedule(schedule->When, schedule->EventId);
    }
    else if (const auto* flag = std::get_if<SetWorldFlag>(&command.Payload))
    {
        const auto existing = State_.Flags.find(flag->FlagId);
        if (existing == State_.Flags.end() || existing->second != flag->Value)
        {
            State_.Flags[flag->FlagId] = flag->Value;
            result.Events.push_back({0, State_.Clock.Now(), WorldFlagChanged{flag->FlagId, flag->Value}});
        }
    }
    else if (IsGateCommand(command.Payload))
    {
        ApplyGateCommand(State_, command, result.Events);
    }
    else if (IsLaneCommand(command.Payload))
    {
        return ApplyLaneCommand(command, result);
    }
    else if (std::holds_alternative<SchoolPlanDay>(command.Payload) || std::holds_alternative<SchoolArrive>(command.Payload))
    {
        return ApplySchoolCommand(command, result);
    }
    else if (const auto* day = std::get_if<WorldDayCommand>(&command.Payload))
        return ApplyWorldDayCommand(*day, result);
    else
        return ApplyCommerceCommand(command);
    return std::nullopt;
}

CommandResult WorldSim::Submit(const Command& command)
{
    CommandResult result;
    result.From = State_.Clock.Now();
    result.To = result.From;
    result.Seq = State_.Seq;
    if (const std::optional<Rejection> rejection = Validate(command))
    {
        result.Why = *rejection;
        return result;
    }
    // Only a command whose application can still be refused pays for a copy of the world.
    std::optional<WorldState> before;
    if (NeedsRollbackSnapshot(command) || std::holds_alternative<AdvanceTime>(command.Payload) ||
        std::holds_alternative<WorldDayCommand>(command.Payload))
        before = State_;
    std::optional<Rejection> late = Apply(command, result);
    if (!late && State_.Clock.Now() != result.From)
    {
        // One settlement for the ACTUAL interval, including event-capped advances. Paid choices,
        // day passes, sleep, activities and existing week compression all reach this commit gate.
        auto settled = SettleAllowance(State_.Commerce, result.From, State_.Clock.Now());
        if (!settled.Ok())
            late = R83Reject(std::string(CommerceErrorCode(settled.Error)), "Allowance settlement refused.");
        else
            State_.Commerce = std::move(settled.State);
    }
    if (late)
    {
        if (before)
            State_ = std::move(*before);
        CommandResult refused;
        refused.From = State_.Clock.Now();
        refused.To = refused.From;
        refused.Seq = State_.Seq;
        refused.Why = *late;
        return refused;
    }
    State_.Seq += 1;
    Log_.push_back(command);
    result.Accepted = true;
    result.Seq = State_.Seq;
    result.To = State_.Clock.Now();
    for (DomainEvent& event : result.Events)
    {
        event.Seq = State_.Seq;
    }
    return result;
}
} // namespace PerfectWorld
