// ROADMAP M4-03: negotiable obligations (never dropped silently) and the one world event (fixed onset,
// changeable consequence).
#include "CountdownTestSupport.h"

#include "GateSites.h"

#include <algorithm>

using namespace PerfectWorld;

namespace
{
CountdownProposalOffer Proposal(const std::vector<CountdownProposalOffer>& offers, CountdownProposal proposal)
{
    for (const CountdownProposalOffer& offer : offers)
    {
        if (offer.Proposal == proposal)
            return offer;
    }
    CdExpect(false, "the proposal is listed");
    return offers.front();
}

CountdownTakeResult Negotiate(const CdSession& session, const CountdownContext& context, const char* id, CountdownProposal proposal)
{
    return NegotiateObligation(session.World, session.State, context, id, proposal);
}

const CountdownObligationRecord& Record(const CountdownState& state, std::string_view id)
{
    const CountdownObligationRecord* record = CountdownFindRecord(state, id);
    CdExpect(record != nullptr, "the obligation has a record");
    return *record;
}

/** The first world seed (from 1) whose negotiation of `proposal` on `id` is accepted (or refused). */
std::uint64_t SeedWhere(const CountdownContext& context, const char* id, CountdownProposal proposal, bool accepted)
{
    for (std::uint64_t seed = 1; seed < 2000; ++seed)
    {
        const CdSession session = CdOpen(context, seed);
        const CountdownTakeResult result = Negotiate(session, context, id, proposal);
        if (result.Ok() && Record(result.State, id).Accepted == accepted)
            return seed;
    }
    CdExpect(false, "a seed with that answer exists");
    return 0;
}

void Run(CdSession& session, int days)
{
    std::vector<WeekDayPlan> plan = CdDays(session.State, CdSpanDay(session.World), CdStyle{}, session.World.DayLoop.Now);
    plan.resize(static_cast<std::size_t>(days));
    CountdownResolveResult resolved = ResolveCountdownDays(session.World, session.State, plan, CdCats());
    CdExpect(resolved.Ok(), "the days resolve");
    session.World = std::move(resolved.World);
    session.State = std::move(resolved.State);
}

bool HasBlock(const CountdownState& state, std::string_view id)
{
    const std::vector<CountdownReservedBlock> blocks = CountdownReservedBlocks(state);
    return std::any_of(blocks.begin(), blocks.end(), [&](const CountdownReservedBlock& block) { return block.Id == id; });
}
} // namespace

void TestCountdownObligations()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);
    const char* const pickup = "oblig.sibling_pickup";
    const CountdownObligationDef& def = *CountdownFindObligation(pickup);

    // The proposals and their costs are shown before anything is said.
    const CdSession session = CdOpen(context);
    const std::vector<CountdownProposalOffer> offers = CountdownProposals(session.World, session.State, context, pickup);
    CdExpect(offers.size() == CountdownProposalCount && offers[0].Proposal == CountdownProposal::Honour, "four proposals, keeping first");
    const CountdownProposalOffer share = Proposal(offers, CountdownProposal::Share);
    const CountdownProposalOffer defer = Proposal(offers, CountdownProposal::Defer);
    const CountdownProposalOffer decline = Proposal(offers, CountdownProposal::Decline);
    CdExpect(offers[0].Available && offers[0].MinutesIfAccepted == def.Minutes && offers[0].GoodwillIfAccepted == def.HonourGoodwill,
             "keeping it costs its hours and earns its standing");
    CdExpect(share.Available && share.Helper == "npc_friend" && share.MinutesIfAccepted == def.ShareMinutes && share.MinutesIfAccepted < def.Minutes &&
                 share.GoodwillIfAccepted == def.ShareAcceptedGoodwill && share.GoodwillIfRefused == def.ShareRefusedGoodwill,
             "sharing names the helper, the fewer minutes and both costs");
    CdExpect(share.AcceptPermille == 500 && share.Band == CountdownBand::Likely && defer.AcceptPermille == 500, "and the chance it is accepted: the sibling's trust adds fifty to 450");
    CdExpect(defer.Available && defer.StartIfAccepted == CountdownInstantOf(def.DeferDay, def.DeferMinute) && defer.StartIfAccepted > offers[0].StartIfAccepted &&
                 defer.MinutesIfAccepted == def.Minutes && defer.GoodwillIfRefused < 0,
             "deferring names the later slot and costs standing even when refused");
    CdExpect(decline.Available && decline.AcceptPermille == 1000 && decline.GoodwillIfAccepted == def.DeclineGoodwill && decline.GoodwillIfAccepted < 0,
             "declining is certain and its cost is shown");
    std::vector<CdTie> distant = ties;
    distant[4].Trust = 0; // the sibling
    const RelationshipBook distantBook = CdBook(distant);
    CdExpect(Proposal(CountdownProposals(session.World, session.State, CdContext(&distantBook, distant), pickup), CountdownProposal::Defer).AcceptPermille == 450,
             "less trust, less chance");

    // Declining is a recorded decision with its disclosed cost; it is not a silent drop.
    CdExpect(HasBlock(session.State, pickup), "unless something is said the hours stand");
    const CountdownTakeResult declined = Negotiate(session, context, pickup, CountdownProposal::Decline);
    CdExpect(declined.Ok(), "declining is accepted");
    const CountdownObligationRecord& gone = Record(declined.State, pickup);
    CdExpect(gone.Disposition == CountdownDisposition::Declined && gone.Negotiated && gone.Accepted && gone.Minutes == 0 && !HasBlock(declined.State, pickup),
             "the obligation is declined and its block is gone");
    CdExpect(declined.State.Goodwill == def.DeclineGoodwill, "the standing changed by exactly what was shown");
    CdExpect(declined.State.Ledger.back().Kind == CountdownLedgerKind::Negotiated && declined.State.Ledger.back().Id == pickup &&
                 declined.State.Ledger.back().A == static_cast<std::int32_t>(CountdownProposal::Decline) && declined.State.Ledger.back().B == 1,
             "and the ledger says so");
    CdExpect(CountdownProfileOf(session.World, declined.State).Goodwill == CountdownGoodwillStart + def.DeclineGoodwill && CountdownProfileOf(session.World, declined.State).Declined == 1,
             "the profile counts it");
    CdExpect(RenderCountdownReceipt(declined.State).find("negotiated oblig.sibling_pickup: decline, accepted") != std::string::npos,
             "and the receipt prints it");
    CdSession after = session;
    after.State = declined.State;
    CdExpect(!Negotiate(after, context, pickup, CountdownProposal::Share).Ok() && Negotiate(after, context, pickup, CountdownProposal::Share).Issue.Error == CountdownError::Negotiated,
             "the one negotiation is used");
    CdExpect(!Proposal(CountdownProposals(after.World, after.State, context, pickup), CountdownProposal::Defer).Available, "and the proposals say so");
    CdExpect(Negotiate(session, context, pickup, CountdownProposal::Honour).Issue.Error == CountdownError::ProposalUnavailable,
             "keeping an obligation is not a negotiation");
    CdExpect(Negotiate(session, context, "oblig.nobody", CountdownProposal::Decline).Issue.Error == CountdownError::UnknownObligation, "an unknown obligation is refused");

    // Kept by default: the hours go into the plan, a plan that lands on them is refused, and keeping earns what was shown.
    CdSession kept = session;
    std::vector<WeekDayPlan> clash = CdDays(kept.State, 0, CdStyle{}, kept.World.DayLoop.Now);
    WeekEntry study;
    study.Kind = WeekEntryKind::Activity;
    study.StartMinute = 930;
    study.ActivityId = "activity.study_at_home";
    study.PlaceKind = "home";
    clash.resize(2);
    clash[1].Entries.push_back(study);
    std::stable_sort(clash[1].Entries.begin(), clash[1].Entries.end(), [](const WeekEntry& a, const WeekEntry& b) { return a.StartMinute < b.StartMinute; });
    const CountdownResolveResult refused = ResolveCountdownDays(kept.World, kept.State, clash, CdCats());
    CdExpect(refused.Issue.Error == CountdownError::PlanConflict && refused.Issue.Detail == pickup && refused.State == kept.State &&
                 refused.World.DayLoop.Now == kept.World.DayLoop.Now,
             "a plan that lands on the obligation is refused and nothing is half done");
    Run(kept, 2);
    const CountdownObligationRecord& done = Record(kept.State, pickup);
    CdExpect(done.Done && done.Disposition == CountdownDisposition::Standing && kept.State.Goodwill == def.HonourGoodwill, "kept, it earned its standing");
    CdExpect(std::any_of(kept.State.Ledger.begin(), kept.State.Ledger.end(),
                         [&](const CountdownLedgerLine& line) { return line.Kind == CountdownLedgerKind::Honoured && line.Id == pickup && line.B == def.Minutes; }),
             "and the ledger shows the hours it cost");
    CdExpect(std::any_of(kept.State.Requests.begin(), kept.State.Requests.end(), [](const CountdownRequest& request) {
                 return request.Other == "npc_sibling" && request.Kind == RelationshipActKind::Help && request.Reason == "oblig.sibling_pickup";
             }),
             "and asks the relationship system for a Help act");

    // Sharing and deferring are uncertain and disclosed; the answer is stored and both answers are possible.
    const std::uint64_t yes = SeedWhere(context, pickup, CountdownProposal::Share, true);
    const std::uint64_t no = SeedWhere(context, pickup, CountdownProposal::Share, false);
    const CdSession sharedSession = CdOpen(context, yes);
    const CountdownTakeResult shared = Negotiate(sharedSession, context, pickup, CountdownProposal::Share);
    const CountdownObligationRecord& sharedRecord = Record(shared.State, pickup);
    CdExpect(sharedRecord.Disposition == CountdownDisposition::Shared && sharedRecord.Minutes == def.ShareMinutes && sharedRecord.Helper == "npc_friend" &&
                 sharedRecord.AcceptPermille == 500 && sharedRecord.Seed != 0 && shared.State.Goodwill == def.ShareAcceptedGoodwill,
             "an accepted share halves the hours, names the helper, stores the seed and the odds and costs what was shown");
    const CountdownTakeResult refusedShare = Negotiate(CdOpen(context, no), context, pickup, CountdownProposal::Share);
    const CountdownObligationRecord& refusedRecord = Record(refusedShare.State, pickup);
    CdExpect(refusedRecord.Negotiated && !refusedRecord.Accepted && refusedRecord.Disposition == CountdownDisposition::Standing && refusedRecord.Minutes == def.Minutes &&
                 refusedShare.State.Goodwill == def.ShareRefusedGoodwill && HasBlock(refusedShare.State, pickup),
             "a refused share leaves the hours standing and costs what was shown");
    CdExpect(Negotiate(CdSession{CdOpen(context, no).World, refusedShare.State}, context, pickup, CountdownProposal::Decline).Issue.Error == CountdownError::Negotiated,
             "a refusal is not a reason to ask again");
    // The same question in the same life always gets the same answer.
    CdExpect(Negotiate(CdOpen(context, yes), context, pickup, CountdownProposal::Share).State == shared.State, "asking again in the same life gives the same answer");
    CdSession sharedRun = sharedSession;
    sharedRun.State = shared.State;
    Run(sharedRun, 2);
    CdExpect(Record(sharedRun.State, pickup).Done && sharedRun.State.Goodwill == def.ShareAcceptedGoodwill + def.HonourGoodwill, "a shared obligation is still kept, for fewer hours");
    const std::uint64_t deferYes = SeedWhere(context, pickup, CountdownProposal::Defer, true);
    const CountdownTakeResult deferred = Negotiate(CdOpen(context, deferYes), context, pickup, CountdownProposal::Defer);
    CdExpect(Record(deferred.State, pickup).Disposition == CountdownDisposition::Deferred && Record(deferred.State, pickup).Start == defer.StartIfAccepted &&
                 deferred.State.Goodwill == def.DeferAcceptedGoodwill,
             "an accepted deferral moves the block");

    // What cannot be offered is said: a deferral into a slot already committed, a share with no helper.
    CdSession busy = session;
    CdTake(busy, context, "opp.mock_debate");
    const CountdownProposalOffer blocked = Proposal(CountdownProposals(busy.World, busy.State, context, pickup), CountdownProposal::Defer);
    CdExpect(!blocked.Available && blocked.Unavailable == "countdown.time_conflict" &&
                 Negotiate(busy, context, pickup, CountdownProposal::Defer).Issue.Error == CountdownError::ProposalUnavailable,
             "a deferral into a committed slot is unavailable, with the reason");
    std::vector<CdTie> alone = ties;
    alone.erase(alone.begin()); // no friend
    const RelationshipBook aloneBook = CdBook(alone);
    const CountdownContext aloneContext = CdContext(&aloneBook, alone);
    const CdSession solo = CdOpen(aloneContext);
    CdExpect(Proposal(CountdownProposals(solo.World, solo.State, aloneContext, pickup), CountdownProposal::Share).Unavailable == "countdown.no_contact",
             "without a friend there is no one to share with");
    std::vector<CdTie> strangers = ties;
    strangers[0].Trust = 1000;
    const RelationshipBook strangersBook = CdBook(strangers);
    CdExpect(Proposal(CountdownProposals(session.World, session.State, CdContext(&strangersBook, strangers), pickup), CountdownProposal::Share).Unavailable == "countdown.relation_too_low",
             "a helper who does not trust the player enough is unavailable");
    std::vector<CdTie> nobody = ties;
    nobody.erase(nobody.begin() + 4); // no sibling
    const RelationshipBook nobodyBook = CdBook(nobody);
    const CountdownContext nobodyContext = CdContext(&nobodyBook, nobody);
    const CdSession orphaned = CdOpen(nobodyContext);
    CdExpect(CountdownFindRecord(orphaned.State, pickup) == nullptr && CountdownProposals(orphaned.World, orphaned.State, nobodyContext, pickup).empty() &&
                 Negotiate(orphaned, nobodyContext, pickup, CountdownProposal::Decline).Issue.Error == CountdownError::UnknownObligation,
             "an obligation to nobody does not exist");
    // Once the hour has come, the answer is no longer open.
    CdSession late = session;
    Run(late, 3);
    const std::vector<CountdownProposalOffer> gone3 = CountdownProposals(late.World, late.State, context, "oblig.neighbour_shop_shift");
    CdExpect(!Proposal(gone3, CountdownProposal::Honour).Available && Proposal(gone3, CountdownProposal::Decline).Unavailable == "countdown.window_closed" &&
                 Proposal(gone3, CountdownProposal::Share).Unavailable == "countdown.window_closed",
             "a proposal about hours already kept is unavailable, with the reason");
}

void TestCountdownEvent()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);
    const CountdownWorldEventDef& event = CountdownWorldEvent();
    const std::string gate(event.GateId);

    CdSession session = CdOpen(context);
    CdExpect(session.World.Gates.at(gate).Status == GateStatus::Contained && session.World.Gates.at(gate).Rank == event.Rank,
             "the Gate is registered, contained, before its hour");
    const std::vector<ScheduledEvent>& pending = session.World.Clock.Pending();
    CdExpect(std::any_of(pending.begin(), pending.end(), [&](const ScheduledEvent& e) { return e.Id == GateOnsetEventId(gate) && e.When == session.State.EventOnset; }),
             "its onset is on the clock at the fixed instant");
    const CalendarTime onset = GameClock::ToCalendar(session.State.EventOnset);
    CdExpect(onset.Year == 2043 && onset.Month == 1 && onset.Day == 28 && onset.Hour == 19 && onset.Minute == 40, "Wednesday 28 January, 19:40");

    // Days before the hour: no news, the Gate holds.
    std::vector<WeekDayPlan> plan = CdDays(session.State, 0, CdStyle{}, session.World.DayLoop.Now);
    plan.resize(3);
    CountdownResolveResult early = ResolveCountdownDays(session.World, session.State, plan, CdCats());
    CdExpect(early.Ok() && !early.StoppedForNews && !early.State.EventFired && early.DaysResolved == 3 && early.World.Gates.at(gate).Status == GateStatus::Contained,
             "nothing happens before the hour");
    CdExpect(!CountdownNewsOf(early.World, early.State, context).Fired, "and there is no news");
    CdSession mid{early.World, early.State};

    // A plan of a whole week is cut after the day the news arrives: that is the span's one interrupt.
    plan = CdDays(mid.State, CdSpanDay(mid.World), CdStyle{}, mid.World.DayLoop.Now);
    plan.resize(7);
    const CountdownResolveResult news = ResolveCountdownDays(mid.World, mid.State, plan, CdCats());
    CdExpect(news.Ok() && news.StoppedForNews && news.DaysResolved == 3 && !news.Closed, "the plan is cut after Wednesday");
    const GateSiteState& gateState = news.World.Gates.at(gate);
    CdExpect(gateState.Status == GateStatus::Active && gateState.OnsetCount == 1 && gateState.StatusSince == session.State.EventOnset,
             "the Gate broke at exactly the fixed instant, whatever the player did");
    CdExpect(news.State.EventFired && news.Events.size() == 1 && news.Events[0].Kind == CountdownLedgerKind::News && news.Events[0].At == session.State.EventOnset &&
                 news.Events[0].A == event.BaseDelayMinutes,
             "the news is a ledger line at the onset naming the delay");
    CdExpect(news.World.DayLoop.Now == CountdownInstantOf(6, 360), "the clock stands at Thursday morning, with Thursday's hours still to plan");

    // The player cannot stop the onset; the weather is not theirs to change.
    WorldSim tamper(session.World);
    CdExpect(!tamper.Submit({CommandSource::Player, GateSeal{gate}}).Accepted && !tamper.Submit({CommandSource::Player, GateContain{gate}}).Accepted,
             "a player cannot seal or contain the Gate");

    // The news shows the cost and the responses; the cost is changed by what is done.
    const CdSession informed{news.World, news.State};
    const CountdownNews shown = CountdownNewsOf(informed.World, informed.State, context);
    CdExpect(shown.Fired && shown.At == session.State.EventOnset && shown.DelayUnmitigated == event.BaseDelayMinutes && shown.DelayNow == event.BaseDelayMinutes &&
                 shown.Responses == std::vector<std::string>({"opp.relief_shift", "opp.scout_route", "opp.friend_ride"}),
             "the news names the delay and the three responses");
    CdExpect(CountdownRouteDelay(informed.State) == event.BaseDelayMinutes, "with nothing done the road costs its full delay");
    std::int32_t best = event.BaseDelayMinutes;
    for (const char* response : {"opp.relief_shift", "opp.scout_route", "opp.friend_ride"})
    {
        CdSession acted = informed;
        CdTake(acted, context, response);
        CdFinish(acted, CdStyle{});
        const std::int32_t delay = CountdownRouteDelay(acted.State);
        CdExpect(delay < event.BaseDelayMinutes && delay >= 0 && acted.State.RouteMinutes > 0, "every response shortens the road");
        CdExpect(acted.World.Gates.at(gate).StatusSince == session.State.EventOnset, "and none of them moved the onset");
        best = std::min(best, delay);
    }
    CdSession idle = informed;
    CdFinish(idle, CdStyle{});
    CdExpect(CountdownRouteDelay(idle.State) >= event.BaseDelayMinutes - CountdownWorldEvent().LogisticsMaxMinutes && CountdownRouteDelay(idle.State) > best,
             "doing nothing leaves the delay for the morning, only preparation softens it");
    // Preparation changes it too: a student who spent ten days on logistics meets the same road with less of it.
    CdSession prepared = informed;
    prepared.State.OutcomePrep[static_cast<std::size_t>(CountdownDimension::Logistics)] = 500;
    CdExpect(CountdownRouteDelay(prepared.State) == event.BaseDelayMinutes - event.LogisticsMaxMinutes, "logistics preparation is worth minutes on the road");
    // The consequence reaches the profile through composure.
    CdSession quiet = informed;
    CdFinish(quiet, CdStyle{});
    CdSession helped = informed;
    CdTake(helped, context, "opp.relief_shift");
    CdFinish(helped, CdStyle{});
    CdExpect(CountdownProfileOf(quiet.World, quiet.State).RouteDelayMinutes == CountdownRouteDelay(quiet.State) &&
                 CountdownProfileOf(helped.World, helped.State).RouteDelayMinutes < CountdownProfileOf(quiet.World, quiet.State).RouteDelayMinutes,
             "the profile carries the delay");
    // Before the hour no response is on offer, and stepping a day at a time reaches the same news on the same day.
    CdSession daily = CdOpen(context);
    int calls = 0;
    while (!daily.State.EventFired)
    {
        std::vector<WeekDayPlan> one = CdDays(daily.State, CdSpanDay(daily.World), CdStyle{}, daily.World.DayLoop.Now);
        one.resize(1);
        CountdownResolveResult step = ResolveCountdownDays(daily.World, daily.State, one, CdCats());
        CdExpect(step.Ok(), "a day resolves");
        daily.World = std::move(step.World);
        daily.State = std::move(step.State);
        ++calls;
    }
    CdExpect(calls == 6 && daily.World.DayLoop.Now == news.World.DayLoop.Now && daily.State.Ledger.back().At == session.State.EventOnset,
             "a day at a time the news arrives on the sixth day, at the same minute");
}
