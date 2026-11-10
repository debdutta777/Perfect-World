#include "WorldReducer.h"

#include <utility>

// R9-04: the R9-05 countdown wiring inside WorldSim. The scheduled 18:00 opening on Awakening Day is a boundary of every
// time path (RunTimeTo clamps to it and opens the countdown there through OpenCountdownAtCurrentTime, R95OpenBoundary), and
// an open countdown is synchronised once per accepted command after the final day and allowance settlement
// (SynchroniseWorldCountdown, R95SyncSnapshot). Only a sim bound with the countdown context and a seeded world does either;
// nested helper sims never do, so nothing submits commands recursively.
namespace PerfectWorld
{
namespace
{
Rejection R94RouteRefusal(const CountdownIssue& issue)
{
    return Rejection{std::string(CountdownErrorCode(issue.Error)),
                     issue.Detail.empty() ? std::string("The countdown refused this moment.") : issue.Detail};
}
} // namespace

bool WorldSim::CountdownRouted() const noexcept
{
    return State_.Awakening.Seeded && !Awakening_.Countdown.Player.empty();
}

std::optional<Rejection> WorldSim::OpenCountdownBoundary(CommandResult& result)
{
    // Only AT the boundary. A world already past 18:00 without a countdown (one driven by an unbound sim) is never
    // bootstrapped here: past the Gate onset that would refuse every later moment of the life.
    if (!CountdownRouted() || !State_.CountdownText.empty() || State_.Clock.Now() != CountdownStartInstant())
        return std::nullopt;
    std::vector<DomainEvent> opened;
    CountdownOpenResult open = OpenCountdownAtCurrentTime(State_, Awakening_.Countdown, School_, &opened);
    if (!open.Ok())
        return R94RouteRefusal(open.Issue);
    std::string text = SerialiseCountdown(open.State);
    if (text.empty() || !ParseCountdown(text))
        return Rejection{"countdown.invalid_world", "The opened countdown cannot be stored."};
    State_ = std::move(open.World);
    State_.CountdownText = std::move(text);
    result.Events.insert(result.Events.end(), opened.begin(), opened.end());
    return std::nullopt;
}

std::optional<Rejection> WorldSim::SyncCountdown(CommandResult& result)
{
    if (!CountdownRouted() || State_.CountdownText.empty())
        return std::nullopt;
    const std::optional<CountdownState> stored = ParseCountdown(State_.CountdownText);
    if (!stored)
        return Rejection{"countdown.invalid_world", "The stored countdown cannot be read."};
    WorldCountdownResult synced = SynchroniseWorldCountdown(State_, State_.Awakening, stored, Awakening_.Countdown, School_);
    if (!synced.Ok())
        return R94RouteRefusal(synced.Issue);
    if (!synced.State)
        return Rejection{"countdown.invalid_world", "The countdown disappeared while synchronising."};
    std::string text = SerialiseCountdown(*synced.State);
    if (text.empty())
        return Rejection{"countdown.invalid_world", "The synchronised countdown cannot be stored."};
    State_ = std::move(synced.World);
    State_.CountdownText = std::move(text);
    result.Events.insert(result.Events.end(), synced.Events.begin(), synced.Events.end());
    return std::nullopt;
}

std::optional<CountdownState> WorldSim::Countdown() const
{
    if (State_.CountdownText.empty())
        return std::nullopt;
    return ParseCountdown(State_.CountdownText);
}

WorldCountdownView WorldSim::CountdownView() const
{
    return QueryWorldCountdown(State_, State_.Awakening, Countdown());
}

WorldCountdownPracticeOffer WorldSim::PreviewCountdownPractice(PowerDimension dimension, std::int32_t minutes) const
{
    return PreviewWorldCountdownPractice(State_, State_.Awakening, Countdown(), Awakening_.Countdown, School_, dimension, minutes);
}
} // namespace PerfectWorld
