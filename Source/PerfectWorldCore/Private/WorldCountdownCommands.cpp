#include "WorldReducer.h"

#include <utility>

namespace PerfectWorld
{
namespace
{
Rejection P37CountdownRefusal(const CountdownIssue& issue)
{
    return {std::string(CountdownErrorCode(issue.Error)), issue.Detail};
}

bool P37SameClock(const GameClock& left, const GameClock& right)
{
    const auto a = left.CaptureSave();
    const auto b = right.CaptureSave();
    if (a.Now != b.Now || a.NextHandle != b.NextHandle || a.Pending.size() != b.Pending.size())
        return false;
    for (std::size_t i = 0; i < a.Pending.size(); ++i)
        if (a.Pending[i].Handle != b.Pending[i].Handle || a.Pending[i].When != b.Pending[i].When ||
            a.Pending[i].Id != b.Pending[i].Id)
            return false;
    return true;
}

bool P37SameConsequences(const WorldState& charged, const WorldState& planned)
{
    if (!P37SameClock(charged.Clock, planned.Clock) || charged.Flags != planned.Flags ||
        charged.Gates.size() != planned.Gates.size())
        return false;
    auto other = planned.Gates.begin();
    for (const auto& [id, gate] : charged.Gates)
    {
        if (id != other->first || gate.Rank != other->second.Rank || gate.Status != other->second.Status ||
            gate.StatusSince != other->second.StatusSince || gate.OnsetCount != other->second.OnsetCount)
            return false;
        ++other;
    }
    return true;
}
} // namespace

bool IsCountdownCommand(const CommandPayload& payload) noexcept
{
    return std::holds_alternative<CountdownTakeOpportunity>(payload) ||
           std::holds_alternative<CountdownNegotiateObligation>(payload) ||
           std::holds_alternative<CountdownResolveDays>(payload);
}

std::optional<Rejection> WorldSim::ValidateCountdownCommand(const Command&) const
{
    if (!CountdownRouted())
        return P37CountdownRefusal({CountdownError::BadContext, "The countdown context is not bound."});
    if (!IsWorldDayInStep(State_))
        return P37CountdownRefusal({CountdownError::InvalidWorld, "The world and day must share one valid clock."});
    if (!IsWorldQuiescent(State_) || !ServiceShop_.empty() || !OpenConversationPersonId(State_).empty())
        return P37CountdownRefusal({CountdownError::WorldBusy, "Finish the open interaction and settle due events first."});
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ApplyCountdownCommand(const Command& command, CommandResult& result)
{
    const auto countdown = Countdown();
    if (!countdown)
        return P37CountdownRefusal({State_.CountdownText.empty() ? CountdownError::Closed : CountdownError::InvalidWorld,
                                   "An open, readable countdown is required."});
    CountdownTakeResult changed;
    if (const auto* take = std::get_if<CountdownTakeOpportunity>(&command.Payload))
        changed = TakeOpportunity(State_, *countdown, Awakening_.Countdown, take->OpportunityId);
    else if (const auto* negotiate = std::get_if<CountdownNegotiateObligation>(&command.Payload))
        changed = NegotiateObligation(State_, *countdown, Awakening_.Countdown, negotiate->ObligationId, negotiate->Proposal);
    else if (const auto* resolve = std::get_if<CountdownResolveDays>(&command.Payload))
    {
        // The contract runs an unbound helper sim on a copy. Its reserved blocks are executed there, so no clock-only
        // observation runs between them. Only the outer charge publishes clock events and allowance settlement.
        auto resolved = ResolveCountdownDays(State_, *countdown, resolve->Days, School_);
        if (!resolved.Ok())
            return P37CountdownRefusal(resolved.Issue);
        const GameInstant end = resolved.World.Clock.Now();
        if (end < State_.Clock.Now() || !IsWorldDayInStep(resolved.World))
            return P37CountdownRefusal({CountdownError::InvalidWorld, "The resolved day and world clocks disagree."});
        ChargeTime(end - State_.Clock.Now(), result);
        if (result.Partial || State_.Clock.Now() != end)
            return P37CountdownRefusal({CountdownError::WorldBusy, "A time boundary interrupted the plan; nothing was published."});
        if (!P37SameConsequences(State_, resolved.World))
            return P37CountdownRefusal({CountdownError::InvalidWorld, "The plan's world events differ from the outer clock."});
        if (auto refusal = CreditActivities(resolved.Week.ActivityEvents))
            return refusal;
        State_.DayLoop = resolved.World.DayLoop;
        State_.CountdownText = SerialiseCountdown(resolved.State);
        for (const auto& event : resolved.Week.ActivityEvents)
            result.Events.push_back({0, event.At, event});
        StagedCountdown_ = std::move(resolved);
        return std::nullopt;
    }
    else
        return Rejection{"command.unknown", "Unknown countdown command."};
    if (!changed.Ok())
        return P37CountdownRefusal(changed.Issue);
    State_.CountdownText = SerialiseCountdown(changed.State);
    return std::nullopt;
}
} // namespace PerfectWorld
