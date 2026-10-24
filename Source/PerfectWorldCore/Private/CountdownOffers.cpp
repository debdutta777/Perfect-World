#include "Countdown.h"

#include "CountdownInternal.h"

#include <algorithm>
#include <utility>

// Disclosure, committing and negotiating (ROADMAP M4-03). Everything shown here is computed from the same
// functions the commit uses, so the odds on the screen are the odds that are stored.
namespace PerfectWorld
{
namespace
{
WeekEntry CountdownBlockEntry(const CountdownBlockDef& block, GameInstant start)
{
    WeekEntry entry;
    entry.StartMinute = CountdownMinuteOfDay(start);
    if (!block.ActivityId.empty())
    {
        entry.Kind = WeekEntryKind::Activity;
        entry.ActivityId = std::string(block.ActivityId);
        entry.PlaceKind = std::string(block.PlaceKind);
    }
    else
    {
        entry.Kind = WeekEntryKind::Spend;
        entry.Minutes = block.Minutes;
        entry.Pass = block.Pass;
    }
    return entry;
}

/** The id of a reserved block that overlaps [start, end), ignoring `skip`; empty when none does. */
std::string CountdownConflictWith(const CountdownState& state, GameInstant start, GameInstant end, std::string_view skip)
{
    for (const CountdownReservedBlock& block : CountdownReservedBlocks(state))
    {
        if (block.Id != skip && CountdownOverlaps(start, end, block.Start, block.End))
            return block.Id;
    }
    return {};
}

struct CountdownAssessment
{
    CountdownAvailability Availability = CountdownAvailability::Open;
    std::string Contact;
    std::int32_t Relation = 0;
};

CountdownAssessment CountdownAssess(const WorldState& world, const CountdownState& state, const CountdownContext& context,
                                    const CountdownOpportunityDef& def)
{
    CountdownAssessment result;
    const GameInstant start = CountdownInstantOf(def.Day, def.StartMinute);
    const GameInstant end = start + def.Block.Minutes;
    if (!def.Role.empty())
    {
        result.Contact = CountdownContactFor(context, def.Role);
        result.Relation = CountdownRelationValue(context, result.Contact, def.Relation);
    }
    if (CountdownFindCommitment(state, def.Id) != nullptr)
        result.Availability = CountdownAvailability::Taken;
    else if (start < world.DayLoop.Now)
        result.Availability = CountdownAvailability::WindowClosed;
    else if (def.NeedsNews && !state.EventFired)
        result.Availability = CountdownAvailability::NeedsNews;
    else if (!def.Role.empty() && result.Contact.empty())
        result.Availability = CountdownAvailability::NoContact;
    else if (!def.Role.empty() && result.Relation < def.MinRelation)
        result.Availability = CountdownAvailability::RelationTooLow;
    else if (std::any_of(state.Commitments.begin(), state.Commitments.end(), [&def](const CountdownCommitment& taken) {
                 const CountdownOpportunityDef* other = CountdownFindOpportunity(taken.Id);
                 return other != nullptr && other->Group == def.Group;
             }))
        result.Availability = CountdownAvailability::GroupTaken;
    else if (!CountdownConflictWith(state, start, end, {}).empty())
        result.Availability = CountdownAvailability::TimeConflict;
    return result;
}

CountdownError CountdownErrorOf(CountdownAvailability availability)
{
    switch (availability)
    {
    case CountdownAvailability::Open: return CountdownError::None;
    case CountdownAvailability::Taken: return CountdownError::Taken;
    case CountdownAvailability::WindowClosed: return CountdownError::WindowClosed;
    case CountdownAvailability::NeedsNews: return CountdownError::NeedsNews;
    case CountdownAvailability::NoContact: return CountdownError::NoContact;
    case CountdownAvailability::RelationTooLow: return CountdownError::RelationTooLow;
    case CountdownAvailability::GroupTaken: return CountdownError::GroupTaken;
    case CountdownAvailability::TimeConflict: return CountdownError::TimeConflict;
    }
    return CountdownError::None;
}

CountdownProposalOffer CountdownMakeProposal(CountdownProposal proposal)
{
    CountdownProposalOffer offer;
    offer.Proposal = proposal;
    return offer;
}

/** Fills the common refusals of a negotiation: used, or too late. Returns true when the proposal is already unavailable. */
bool CountdownProposalClosed(const WorldState& world, const CountdownObligationRecord& record, CountdownProposalOffer& offer)
{
    if (record.Negotiated || record.Disposition == CountdownDisposition::Declined)
        offer.Unavailable = std::string(CountdownErrorCode(CountdownError::Negotiated));
    else if (record.Start < world.DayLoop.Now)
        offer.Unavailable = std::string(CountdownErrorCode(CountdownError::WindowClosed));
    return !offer.Unavailable.empty();
}

CountdownProposalOffer CountdownShareOffer(const WorldState& world, const CountdownContext& context, const CountdownObligationDef& def,
                                           const CountdownObligationRecord& record)
{
    CountdownProposalOffer offer = CountdownMakeProposal(CountdownProposal::Share);
    offer.MinutesIfAccepted = def.ShareMinutes;
    offer.StartIfAccepted = record.Start;
    offer.GoodwillIfAccepted = def.ShareAcceptedGoodwill;
    offer.GoodwillIfRefused = def.ShareRefusedGoodwill;
    offer.Helper = CountdownContactFor(context, def.HelperRole);
    offer.AcceptPermille = CountdownSkewedPermille(def.ShareBase, def.SkewStep, def.SkewCap, CountdownRelationValue(context, record.Person, def.Relation));
    offer.Band = CountdownBandOf(offer.AcceptPermille);
    if (CountdownProposalClosed(world, record, offer))
        return offer;
    if (offer.Helper.empty() || offer.Helper == record.Person)
        offer.Unavailable = std::string(CountdownErrorCode(CountdownError::NoContact));
    else if (CountdownRelationValue(context, offer.Helper, CountdownRelation::Trust) < def.HelperMinRelation)
        offer.Unavailable = std::string(CountdownErrorCode(CountdownError::RelationTooLow));
    offer.Available = offer.Unavailable.empty();
    return offer;
}

CountdownProposalOffer CountdownDeferOffer(const WorldState& world, const CountdownState& state, const CountdownContext& context,
                                           const CountdownObligationDef& def, const CountdownObligationRecord& record)
{
    CountdownProposalOffer offer = CountdownMakeProposal(CountdownProposal::Defer);
    offer.MinutesIfAccepted = record.Minutes;
    offer.StartIfAccepted = CountdownInstantOf(def.DeferDay, def.DeferMinute);
    offer.GoodwillIfAccepted = def.DeferAcceptedGoodwill;
    offer.GoodwillIfRefused = def.DeferRefusedGoodwill;
    offer.AcceptPermille = CountdownSkewedPermille(def.DeferBase, def.SkewStep, def.SkewCap, CountdownRelationValue(context, record.Person, def.Relation));
    offer.Band = CountdownBandOf(offer.AcceptPermille);
    if (CountdownProposalClosed(world, record, offer))
        return offer;
    if (offer.StartIfAccepted < world.DayLoop.Now)
        offer.Unavailable = std::string(CountdownErrorCode(CountdownError::WindowClosed));
    else if (!CountdownConflictWith(state, offer.StartIfAccepted, offer.StartIfAccepted + record.Minutes, def.Id).empty())
        offer.Unavailable = std::string(CountdownErrorCode(CountdownError::TimeConflict));
    offer.Available = offer.Unavailable.empty();
    return offer;
}
} // namespace

std::vector<CountdownReservedBlock> CountdownReservedBlocks(const CountdownState& state)
{
    std::vector<CountdownReservedBlock> blocks;
    for (const CountdownCommitment& commitment : state.Commitments)
    {
        const CountdownOpportunityDef* def = CountdownFindOpportunity(commitment.Id);
        if (commitment.Resolved || def == nullptr)
            continue;
        blocks.push_back({commitment.Id, false, commitment.Start, commitment.End, CountdownBlockEntry(def->Block, commitment.Start)});
    }
    for (const CountdownObligationRecord& record : state.Obligations)
    {
        const CountdownObligationDef* def = CountdownFindObligation(record.Id);
        if (record.Done || record.Disposition == CountdownDisposition::Declined || def == nullptr)
            continue;
        CountdownBlockDef block;
        block.Minutes = record.Minutes;
        block.Pass = def->Pass;
        blocks.push_back({record.Id, true, record.Start, record.Start + record.Minutes, CountdownBlockEntry(block, record.Start)});
    }
    std::stable_sort(blocks.begin(), blocks.end(), [](const CountdownReservedBlock& a, const CountdownReservedBlock& b) {
        return a.Start != b.Start ? a.Start < b.Start : a.Id < b.Id;
    });
    return blocks;
}

std::vector<CountdownOffer> CountdownOffers(const WorldState& world, const CountdownState& state, const CountdownContext& context)
{
    std::vector<CountdownOffer> offers;
    for (const CountdownOpportunityDef& def : CountdownOpportunities())
    {
        const CountdownAssessment assessed = CountdownAssess(world, state, context, def);
        CountdownOffer offer;
        offer.Def = &def;
        offer.Availability = assessed.Availability;
        offer.Start = CountdownInstantOf(def.Day, def.StartMinute);
        offer.End = offer.Start + def.Block.Minutes;
        offer.Minutes = def.Block.Minutes;
        offer.Contact = assessed.Contact;
        offer.Relation = assessed.Relation;
        offer.Odds = CountdownOddsFor(def, assessed.Relation);
        for (std::size_t i = 0; i < CountdownOutcomeCount; ++i)
            offer.Bands[i] = CountdownBandOf(offer.Odds[i]);
        // What taking it closes: the rest of its group that has not been committed and whose window has not passed.
        for (const CountdownOpportunityDef& other : CountdownOpportunities())
        {
            if (&other != &def && other.Group == def.Group && CountdownFindCommitment(state, other.Id) == nullptr &&
                CountdownInstantOf(other.Day, other.StartMinute) >= world.DayLoop.Now)
                offer.Closes.emplace_back(other.Id);
        }
        offers.push_back(std::move(offer));
    }
    return offers;
}

CountdownTakeResult TakeOpportunity(const WorldState& world, const CountdownState& state, const CountdownContext& context, std::string_view id)
{
    CountdownTakeResult result;
    result.State = state;
    if (state.Closed)
    {
        result.Issue = CountdownMakeIssue(CountdownError::Closed);
        return result;
    }
    if (const CountdownIssue busy = CountdownCheckWorld(world); !busy.Ok())
    {
        result.Issue = busy;
        return result;
    }
    const CountdownOpportunityDef* def = CountdownFindOpportunity(id);
    if (def == nullptr)
    {
        result.Issue = CountdownMakeIssue(CountdownError::UnknownOpportunity, std::string(id));
        return result;
    }
    const CountdownAssessment assessed = CountdownAssess(world, state, context, *def);
    if (assessed.Availability != CountdownAvailability::Open)
    {
        result.Issue = CountdownMakeIssue(CountdownErrorOf(assessed.Availability), std::string(id));
        return result;
    }
    if (state.Commitments.size() >= CountdownMaxCommitments)
    {
        result.Issue = CountdownMakeIssue(CountdownError::Taken, "too many commitments");
        return result;
    }
    CountdownCommitment commitment;
    commitment.Id = std::string(def->Id);
    commitment.Contact = assessed.Contact;
    // The one draw. It is stored with the odds that were shown; nothing later can change either.
    commitment.Seed = CountdownCommitSeed(state.Seed, def->Id);
    commitment.Odds = CountdownOddsFor(*def, assessed.Relation);
    commitment.CommittedAt = world.DayLoop.Now;
    commitment.Start = CountdownInstantOf(def->Day, def->StartMinute);
    commitment.End = commitment.Start + def->Block.Minutes;
    std::int32_t closes = 0;
    for (const CountdownOpportunityDef& other : CountdownOpportunities())
        closes += (&other != def && other.Group == def->Group && CountdownFindCommitment(state, other.Id) == nullptr) ? 1 : 0;
    CountdownAddLedger(result.State, world.DayLoop.Now, CountdownLedgerKind::Committed, commitment.Id, closes, def->Block.Minutes);
    result.State.Commitments.push_back(std::move(commitment));
    return result;
}

std::vector<CountdownProposalOffer> CountdownProposals(const WorldState& world, const CountdownState& state, const CountdownContext& context,
                                                       std::string_view obligationId)
{
    const CountdownObligationDef* def = CountdownFindObligation(obligationId);
    const CountdownObligationRecord* record = CountdownFindRecord(state, obligationId);
    if (def == nullptr || record == nullptr)
        return {};
    std::vector<CountdownProposalOffer> offers;
    CountdownProposalOffer keep = CountdownMakeProposal(CountdownProposal::Honour);
    keep.Available = record->Disposition != CountdownDisposition::Declined && !record->Done;
    if (!keep.Available)
        keep.Unavailable = std::string(CountdownErrorCode(CountdownError::Negotiated));
    keep.MinutesIfAccepted = record->Minutes;
    keep.StartIfAccepted = record->Start;
    keep.AcceptPermille = 1000;
    keep.Band = CountdownBand::VeryLikely;
    keep.GoodwillIfAccepted = def->HonourGoodwill;
    offers.push_back(std::move(keep));
    offers.push_back(CountdownShareOffer(world, context, *def, *record));
    offers.push_back(CountdownDeferOffer(world, state, context, *def, *record));
    CountdownProposalOffer decline = CountdownMakeProposal(CountdownProposal::Decline);
    decline.AcceptPermille = 1000;
    decline.Band = CountdownBand::VeryLikely;
    decline.GoodwillIfAccepted = def->DeclineGoodwill;
    decline.StartIfAccepted = record->Start;
    CountdownProposalClosed(world, *record, decline);
    decline.Available = decline.Unavailable.empty();
    offers.push_back(std::move(decline));
    return offers;
}

CountdownTakeResult NegotiateObligation(const WorldState& world, const CountdownState& state, const CountdownContext& context,
                                        std::string_view obligationId, CountdownProposal proposal)
{
    CountdownTakeResult result;
    result.State = state;
    if (state.Closed)
    {
        result.Issue = CountdownMakeIssue(CountdownError::Closed);
        return result;
    }
    if (const CountdownIssue busy = CountdownCheckWorld(world); !busy.Ok())
    {
        result.Issue = busy;
        return result;
    }
    const CountdownObligationDef* def = CountdownFindObligation(obligationId);
    const CountdownObligationRecord* record = CountdownFindRecord(state, obligationId);
    if (def == nullptr || record == nullptr)
    {
        result.Issue = CountdownMakeIssue(CountdownError::UnknownObligation, std::string(obligationId));
        return result;
    }
    if (proposal == CountdownProposal::Honour)
    {
        result.Issue = CountdownMakeIssue(CountdownError::ProposalUnavailable, "keeping an obligation needs no negotiation");
        return result;
    }
    CountdownProposalOffer offer;
    for (const CountdownProposalOffer& candidate : CountdownProposals(world, state, context, obligationId))
    {
        if (candidate.Proposal == proposal)
            offer = candidate;
    }
    if (!offer.Available)
    {
        result.Issue = CountdownMakeIssue(record->Negotiated ? CountdownError::Negotiated : CountdownError::ProposalUnavailable, offer.Unavailable);
        return result;
    }
    CountdownObligationRecord* target = nullptr;
    for (CountdownObligationRecord& candidate : result.State.Obligations)
    {
        if (candidate.Id == obligationId)
            target = &candidate;
    }
    target->Negotiated = true;
    target->Attempt = proposal;
    target->AcceptPermille = offer.AcceptPermille;
    if (proposal == CountdownProposal::Decline)
    {
        target->Accepted = true;
        target->Disposition = CountdownDisposition::Declined;
        target->Minutes = 0;
        result.State.Goodwill += def->DeclineGoodwill;
    }
    else
    {
        // The one draw for this attempt, stored with the chance that was shown.
        target->Seed = CountdownNegotiationSeed(state.Seed, obligationId, proposal);
        target->Accepted = CountdownRoll(target->Seed) < offer.AcceptPermille;
        if (target->Accepted)
        {
            result.State.Goodwill += offer.GoodwillIfAccepted;
            target->Minutes = offer.MinutesIfAccepted;
            target->Start = offer.StartIfAccepted;
            target->Disposition = proposal == CountdownProposal::Share ? CountdownDisposition::Shared : CountdownDisposition::Deferred;
            target->Helper = offer.Helper;
        }
        else
        {
            result.State.Goodwill += offer.GoodwillIfRefused;
        }
    }
    CountdownAddLedger(result.State, world.DayLoop.Now, CountdownLedgerKind::Negotiated, std::string(obligationId),
                       static_cast<std::int32_t>(proposal), target->Accepted ? 1 : 0);
    return result;
}
} // namespace PerfectWorld
