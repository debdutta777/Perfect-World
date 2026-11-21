#include "Countdown.h"

#include "CountdownInternal.h"
#include "RandomStreams.h"

#include <algorithm>
#include <utility>

// Names, odds, rolls and the small helpers every part of the countdown shares (ROADMAP M4-03).
namespace PerfectWorld
{
std::string_view CountdownDimensionName(CountdownDimension dimension) noexcept
{
    switch (dimension)
    {
    case CountdownDimension::Combat: return "combat";
    case CountdownDimension::Analysis: return "analysis";
    case CountdownDimension::Endurance: return "endurance";
    case CountdownDimension::Construction: return "construction";
    case CountdownDimension::Negotiation: return "negotiation";
    case CountdownDimension::Logistics: return "logistics";
    }
    return {};
}

std::string_view CountdownOutcomeName(CountdownOutcome outcome) noexcept
{
    switch (outcome)
    {
    case CountdownOutcome::Good: return "good";
    case CountdownOutcome::Fair: return "fair";
    case CountdownOutcome::Poor: return "poor";
    }
    return {};
}

// Design choice (M4-03): four bands on the permille chance, so the player reads words and can still see the number.
CountdownBand CountdownBandOf(std::int32_t permille) noexcept
{
    if (permille < 250)
        return CountdownBand::Unlikely;
    if (permille < 500)
        return CountdownBand::Possible;
    if (permille < 800)
        return CountdownBand::Likely;
    return CountdownBand::VeryLikely;
}

std::string_view CountdownBandName(CountdownBand band) noexcept
{
    switch (band)
    {
    case CountdownBand::Unlikely: return "unlikely";
    case CountdownBand::Possible: return "possible";
    case CountdownBand::Likely: return "likely";
    case CountdownBand::VeryLikely: return "very_likely";
    }
    return {};
}

std::string_view CountdownProposalName(CountdownProposal proposal) noexcept
{
    switch (proposal)
    {
    case CountdownProposal::Honour: return "honour";
    case CountdownProposal::Share: return "share";
    case CountdownProposal::Defer: return "defer";
    case CountdownProposal::Decline: return "decline";
    }
    return {};
}

std::string_view CountdownDispositionName(CountdownDisposition disposition) noexcept
{
    switch (disposition)
    {
    case CountdownDisposition::Standing: return "standing";
    case CountdownDisposition::Shared: return "shared";
    case CountdownDisposition::Deferred: return "deferred";
    case CountdownDisposition::Declined: return "declined";
    }
    return {};
}

std::string_view CountdownLedgerKindName(CountdownLedgerKind kind) noexcept
{
    switch (kind)
    {
    case CountdownLedgerKind::Opened: return "opened";
    case CountdownLedgerKind::Committed: return "committed";
    case CountdownLedgerKind::Resolved: return "resolved";
    case CountdownLedgerKind::Negotiated: return "negotiated";
    case CountdownLedgerKind::Honoured: return "honoured";
    case CountdownLedgerKind::News: return "news";
    case CountdownLedgerKind::Closed: return "closed";
    }
    return {};
}

std::string_view CountdownAvailabilityName(CountdownAvailability availability) noexcept
{
    switch (availability)
    {
    case CountdownAvailability::Open: return "open";
    case CountdownAvailability::Taken: return "taken";
    case CountdownAvailability::WindowClosed: return "window_closed";
    case CountdownAvailability::NeedsNews: return "needs_news";
    case CountdownAvailability::NoContact: return "no_contact";
    case CountdownAvailability::RelationTooLow: return "relation_too_low";
    case CountdownAvailability::GroupTaken: return "group_taken";
    case CountdownAvailability::TimeConflict: return "time_conflict";
    }
    return {};
}

std::string_view CountdownErrorCode(CountdownError error) noexcept
{
    switch (error)
    {
    case CountdownError::None: return {};
    case CountdownError::InvalidWorld: return "countdown.invalid_world";
    case CountdownError::WorldBusy: return "countdown.world_busy";
    case CountdownError::MissingCatalogue: return "countdown.missing_catalogue";
    case CountdownError::MissingCalendar: return "countdown.missing_calendar";
    case CountdownError::NotAtStart: return "countdown.not_at_start";
    case CountdownError::BadContext: return "countdown.bad_context";
    case CountdownError::EventExists: return "countdown.event_exists";
    case CountdownError::Closed: return "countdown.closed";
    case CountdownError::UnknownOpportunity: return "countdown.unknown_opportunity";
    case CountdownError::UnknownObligation: return "countdown.unknown_obligation";
    case CountdownError::Taken: return "countdown.taken";
    case CountdownError::WindowClosed: return "countdown.window_closed";
    case CountdownError::NeedsNews: return "countdown.needs_news";
    case CountdownError::NoContact: return "countdown.no_contact";
    case CountdownError::RelationTooLow: return "countdown.relation_too_low";
    case CountdownError::GroupTaken: return "countdown.group_taken";
    case CountdownError::TimeConflict: return "countdown.time_conflict";
    case CountdownError::Negotiated: return "countdown.negotiated";
    case CountdownError::ProposalUnavailable: return "countdown.proposal_unavailable";
    case CountdownError::NoDays: return "countdown.no_days";
    case CountdownError::TooManyDays: return "countdown.too_many_days";
    case CountdownError::NeedsPlay: return "countdown.needs_play";
    case CountdownError::PlanConflict: return "countdown.plan_conflict";
    case CountdownError::BadPlan: return "countdown.bad_plan";
    case CountdownError::BeyondExam: return "countdown.beyond_exam";
    case CountdownError::ContentInvalid: return "countdown.content_invalid";
    }
    return {};
}

CountdownIssue CountdownMakeIssue(CountdownError error, std::string detail)
{
    CountdownIssue issue;
    issue.Error = error;
    issue.Detail = std::move(detail);
    return issue;
}

std::string CountdownContactFor(const CountdownContext& context, std::string_view role)
{
    for (const CountdownContact& contact : context.Contacts)
    {
        if (contact.Role == role)
            return contact.Person;
    }
    return {};
}

std::int32_t CountdownRelationValue(const CountdownContext& context, std::string_view person, CountdownRelation relation)
{
    if (context.Relationships == nullptr || person.empty())
        return 0;
    const Relationship* pair = context.Relationships->Find(context.Player, person);
    if (pair == nullptr)
        return 0;
    switch (relation)
    {
    case CountdownRelation::Trust: return pair->Trust;
    case CountdownRelation::Warmth: return pair->Warmth;
    case CountdownRelation::Respect: return pair->Respect;
    }
    return 0;
}

// Design choice (M4-03): above the minimum, every full 10,000 of relationship moves SkewStep permille from the poor outcome
// to the good one, up to SkewCap. Closeness makes the good result likelier and the poor one rarer; it never makes anything certain.
CountdownOdds CountdownOddsFor(const CountdownOpportunityDef& def, std::int32_t relation)
{
    CountdownOdds odds = def.BaseOdds;
    if (def.Role.empty())
        return odds;
    const std::int32_t excess = std::max(0, relation - def.MinRelation);
    const std::int32_t shift = std::min(def.SkewCap, (excess / 10000) * def.SkewStep);
    const std::int32_t moved = std::min(shift, odds[static_cast<std::size_t>(CountdownOutcome::Poor)]);
    odds[static_cast<std::size_t>(CountdownOutcome::Good)] += moved;
    odds[static_cast<std::size_t>(CountdownOutcome::Poor)] -= moved;
    return odds;
}

std::int32_t CountdownSkewedPermille(std::int32_t base, std::int32_t step, std::int32_t cap, std::int32_t relation)
{
    const std::int32_t shift = std::min(cap, (std::max(0, relation) / 10000) * step);
    return std::clamp(base + shift, 0, 1000);
}

std::uint64_t CountdownOpenSeed(std::uint64_t worldSeed, GameInstant start)
{
    const std::int64_t key[] = {start};
    return KeyedRandomU64(worldSeed, RandomStreamId::Opportunity, key, "countdown");
}

std::uint64_t CountdownCommitSeed(std::uint64_t stateSeed, std::string_view id)
{
    return KeyedRandomU64(stateSeed, RandomStreamId::Opportunity, {}, id);
}

std::uint64_t CountdownNegotiationSeed(std::uint64_t stateSeed, std::string_view id, CountdownProposal proposal)
{
    const std::int64_t key[] = {static_cast<std::int64_t>(proposal)};
    return KeyedRandomU64(stateSeed, RandomStreamId::Opportunity, key, id);
}

std::int32_t CountdownRoll(std::uint64_t seed)
{
    RandomStream stream(seed);
    return static_cast<std::int32_t>(stream.NextBelow(1000));
}

CountdownOutcome CountdownOutcomeFor(const CountdownOdds& odds, std::int32_t roll)
{
    std::int32_t edge = 0;
    for (std::size_t i = 0; i < CountdownOutcomeCount; ++i)
    {
        edge += odds[i];
        if (roll < edge)
            return static_cast<CountdownOutcome>(i);
    }
    return CountdownOutcome::Poor;
}

void CountdownApplyEffect(CountdownState& state, const CountdownEffect& effect)
{
    for (std::size_t i = 0; i < CountdownDimensionCount; ++i)
        state.OutcomePrep[i] += effect.Preparation[i];
    state.Composure += effect.Composure;
    state.Goodwill += effect.Goodwill;
    state.RouteMinutes += effect.RouteMinutes;
}

void CountdownAddLedger(CountdownState& state, GameInstant at, CountdownLedgerKind kind, std::string id, std::int32_t a, std::int32_t b)
{
    state.Ledger.push_back({at, kind, std::move(id), a, b});
}

std::int32_t CountdownMinuteOfDay(GameInstant instant)
{
    return static_cast<std::int32_t>(instant - GameClock::StartOfDay(instant));
}

bool CountdownOverlaps(GameInstant startA, GameInstant endA, GameInstant startB, GameInstant endB)
{
    return startA < endB && startB < endA;
}

const CountdownObligationRecord* CountdownFindRecord(const CountdownState& state, std::string_view id)
{
    for (const CountdownObligationRecord& record : state.Obligations)
    {
        if (record.Id == id)
            return &record;
    }
    return nullptr;
}

const CountdownCommitment* CountdownFindCommitment(const CountdownState& state, std::string_view id)
{
    for (const CountdownCommitment& commitment : state.Commitments)
    {
        if (commitment.Id == id)
            return &commitment;
    }
    return nullptr;
}

CountdownIssue CountdownCheckWorld(const WorldState& world)
{
    if (!IsWorldDayInStep(world))
        return CountdownMakeIssue(CountdownError::InvalidWorld, "the player's day is not in step with the world clock");
    if (world.DayLoop.Asleep || world.DayLoop.Runner.Active() || world.DayLoop.HasCommitment)
        return CountdownMakeIssue(CountdownError::WorldBusy, "finish the commitment and wake first");
    return {};
}
} // namespace PerfectWorld
