// ROADMAP M4-03: a reload never rerolls. The seed and the odds are stored when something is committed,
// the outcome is a pure function of them, and the saved text cannot be edited into another outcome.
#include "CountdownTestSupport.h"

#include <algorithm>
#include <functional>
#include <map>

using namespace PerfectWorld;

namespace
{
void Reload(CountdownState& state)
{
    const std::string text = SerialiseCountdown(state);
    const std::optional<CountdownState> back = ParseCountdown(text);
    CdExpect(back.has_value() && *back == state && SerialiseCountdown(*back) == text, "the saved state loads to the same state and the same bytes");
    state = *back;
}

/** The state that scripted choices leave: four commitments and two negotiations. */
void Choose(CdSession& session, const CountdownContext& context)
{
    for (const char* id : {"opp.study_circle", "opp.mock_debate", "opp.teacher_review", "opp.paper_marathon"})
        CdTake(session, context, id);
    const CountdownTakeResult declined = NegotiateObligation(session.World, session.State, context, "oblig.teacher_hall_setup", CountdownProposal::Decline);
    CdExpect(declined.Ok(), "an obligation is declined");
    session.State = declined.State;
}

CountdownOutcome ByHand(const CountdownOdds& odds, std::int32_t roll)
{
    return roll < odds[0] ? CountdownOutcome::Good : roll < odds[0] + odds[1] ? CountdownOutcome::Fair : CountdownOutcome::Poor;
}
} // namespace

void TestCountdownReload()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);
    const CdStyle style;

    // Straight through, and again saving and loading before every command and resolving a day at a time.
    CdSession straight = CdOpen(context);
    Choose(straight, context);
    CdFinish(straight, style);

    CdSession reloaded = CdOpen(context);
    Reload(reloaded.State);
    for (const char* id : {"opp.study_circle", "opp.mock_debate", "opp.teacher_review", "opp.paper_marathon"})
    {
        CdTake(reloaded, context, id);
        Reload(reloaded.State);
    }
    const CountdownTakeResult declined =
        NegotiateObligation(reloaded.World, reloaded.State, context, "oblig.teacher_hall_setup", CountdownProposal::Decline);
    CdExpect(declined.Ok(), "the obligation is declined");
    reloaded.State = declined.State;
    Reload(reloaded.State);
    while (!reloaded.State.Closed)
    {
        std::vector<WeekDayPlan> one = CdDays(reloaded.State, CdSpanDay(reloaded.World), style, reloaded.World.DayLoop.Now);
        one.resize(1);
        CountdownResolveResult step = ResolveCountdownDays(reloaded.World, reloaded.State, one, CdCats());
        CdExpect(step.Ok(), "a day resolves after a reload");
        reloaded.World = std::move(step.World);
        reloaded.State = std::move(step.State);
        Reload(reloaded.State);
    }
    if (!(straight.State == reloaded.State))
    {
        const std::string a = SerialiseCountdown(straight.State);
        const std::string b = SerialiseCountdown(reloaded.State);
        std::size_t at = 0;
        while (at < a.size() && at < b.size() && a[at] == b[at])
            ++at;
        const std::size_t lineStart = a.rfind('\n', at) + 1;
        std::cerr << "first difference at byte " << at << ":" << std::endl
                  << a.substr(lineStart, 200) << std::endl
                  << b.substr(lineStart, 200) << std::endl;
    }
    CdExpect(straight.State == reloaded.State && SerialiseCountdown(straight.State) == SerialiseCountdown(reloaded.State) &&
                 HashCountdown(straight.State) == HashCountdown(reloaded.State),
             "reloading before every step, a day at a time, gives the very same ten days as playing straight through");
    CdExpect(straight.World.DayLoop.Condition == reloaded.World.DayLoop.Condition && straight.World.DayLoop.Now == reloaded.World.DayLoop.Now,
             "and the same condition on the morning");
    for (const CountdownCommitment& commitment : straight.State.Commitments)
    {
        CdExpect(commitment.Resolved && commitment.Roll >= 0 && commitment.Roll < 1000 && commitment.Outcome == ByHand(commitment.Odds, commitment.Roll),
                 "every outcome is what its stored odds give its stored draw");
        const CountdownCommitment* same = CountdownFindCommitment(reloaded.State, commitment.Id);
        CdExpect(same != nullptr && same->Seed == commitment.Seed && same->Roll == commitment.Roll && same->Outcome == commitment.Outcome,
                 "the reloaded life has the same seed, draw and outcome");
    }

    // Asking again is refused: a second commit or a second negotiation would be a reroll.
    for (const CountdownCommitment& commitment : straight.State.Commitments)
    {
        const CountdownTakeResult again = TakeOpportunity(straight.World, straight.State, context, commitment.Id);
        CdExpect(again.Issue.Error == CountdownError::Closed, "after the close nothing is committed");
    }
    CdSession midway = CdOpen(context);
    Choose(midway, context);
    Reload(midway.State);
    for (const char* id : {"opp.study_circle", "opp.mock_debate"})
        CdExpect(TakeOpportunity(midway.World, midway.State, context, id).Issue.Error == CountdownError::Taken, "an opportunity cannot be committed again after a reload");
    CdExpect(NegotiateObligation(midway.World, midway.State, context, "oblig.teacher_hall_setup", CountdownProposal::Share).Issue.Error == CountdownError::Negotiated,
             "an obligation cannot be negotiated again after a reload");
    const CountdownCommitment before = *CountdownFindCommitment(midway.State, "opp.study_circle");
    std::vector<WeekDayPlan> plan = CdDays(midway.State, 0, style, midway.World.DayLoop.Now);
    plan.resize(2);
    const CountdownResolveResult first = ResolveCountdownDays(midway.World, midway.State, plan, CdCats());
    const CountdownResolveResult second = ResolveCountdownDays(midway.World, midway.State, plan, CdCats());
    CdExpect(first.Ok() && first.State == second.State && first.State.Ledger == second.State.Ledger, "resolving the same state twice gives the same answer");
    const CountdownCommitment* resolved = CountdownFindCommitment(first.State, "opp.study_circle");
    CdExpect(resolved->Resolved && resolved->Seed == before.Seed && resolved->Odds == before.Odds, "resolving changes neither the seed nor the odds");
    CdExpect(TakeOpportunity(first.World, first.State, context, "opp.study_circle").Issue.Error == CountdownError::Taken, "and it cannot be taken again once resolved");

    // The relationship can change after the commitment: what was shown and stored stands.
    std::vector<CdTie> drifted = ties;
    drifted[0].Trust = 6000;
    const RelationshipBook driftedBook = CdBook(drifted);
    const CountdownContext driftedContext = CdContext(&driftedBook, drifted);
    CdSession committed = CdOpen(context);
    CdTake(committed, context, "opp.study_circle");
    const CountdownOdds shown = CountdownFindCommitment(committed.State, "opp.study_circle")->Odds;
    CdExpect(shown == CountdownOdds({400, 550, 50}), "the odds shown to a close friend");
    CdSession fresh = CdOpen(driftedContext);
    CdExpect(CountdownOffers(fresh.World, fresh.State, driftedContext)[0].Odds == CountdownOdds({300, 550, 150}), "a cooler friendship would have been shown other odds");
    CdFinish(committed, style);
    const CountdownCommitment* kept = CountdownFindCommitment(committed.State, "opp.study_circle");
    CdExpect(kept->Odds == shown && kept->Outcome == ByHand(shown, kept->Roll), "once committed, the odds that were shown are the odds that apply");

    // The seed does not depend on the order of the choices.
    CdSession forward = CdOpen(context);
    CdTake(forward, context, "opp.study_circle");
    CdTake(forward, context, "opp.mock_debate");
    CdSession backward = CdOpen(context);
    CdTake(backward, context, "opp.mock_debate");
    CdTake(backward, context, "opp.study_circle");
    for (const char* id : {"opp.study_circle", "opp.mock_debate"})
        CdExpect(CountdownFindCommitment(forward.State, id)->Seed == CountdownFindCommitment(backward.State, id)->Seed, "the same question in the same life has the same seed");

    // Lives differ and the odds are honest: over many lives the outcomes land near the odds shown.
    std::map<CountdownOutcome, int> counts;
    std::uint64_t firstSeed = 0;
    bool seedsVary = false;
    const int lives = 600;
    for (int life = 1; life <= lives; ++life)
    {
        CdSession session = CdOpen(context, static_cast<std::uint64_t>(life) * 7919);
        CdTake(session, context, "opp.study_circle");
        std::vector<WeekDayPlan> two = CdDays(session.State, 0, style, session.World.DayLoop.Now);
        two.resize(2);
        const CountdownResolveResult done = ResolveCountdownDays(session.World, session.State, two, CdCats());
        CdExpect(done.Ok() && done.State.Commitments[0].Resolved, "the first weekend resolves");
        ++counts[done.State.Commitments[0].Outcome];
        const std::uint64_t seed = done.State.Commitments[0].Seed;
        firstSeed = firstSeed == 0 ? seed : firstSeed;
        seedsVary = seedsVary || seed != firstSeed;
    }
    CdExpect(seedsVary, "each life draws its own seed");
    const auto near = [lives](int count, int permille) { return std::abs(count * 1000 / lives - permille) <= 70; };
    CdExpect(near(counts[CountdownOutcome::Good], 400) && near(counts[CountdownOutcome::Fair], 550) && near(counts[CountdownOutcome::Poor], 50),
             "across six hundred lives the outcomes land within seven points of the odds that were shown");
    CdExpect(counts[CountdownOutcome::Good] > 0 && counts[CountdownOutcome::Fair] > 0 && counts[CountdownOutcome::Poor] > 0, "and every outcome occurs");
}

void TestCountdownSave()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);

    // A state with a little of everything: commitments, a decline, a refused or accepted defer, the news and a response.
    CdSession session = CdOpen(context);
    Choose(session, context);
    const CountdownTakeResult deferred = NegotiateObligation(session.World, session.State, context, "oblig.neighbour_shop_shift", CountdownProposal::Defer);
    CdExpect(deferred.Ok(), "a deferral is asked for");
    const CountdownTakeResult shared = NegotiateObligation(session.World, deferred.State, context, "oblig.sibling_pickup", CountdownProposal::Share);
    CdExpect(shared.Ok(), "and a share");
    session.State = shared.State;
    std::vector<WeekDayPlan> plan = CdDays(session.State, 0, CdStyle{}, session.World.DayLoop.Now);
    plan.resize(7);
    CountdownResolveResult news = ResolveCountdownDays(session.World, session.State, plan, CdCats());
    CdExpect(news.Ok() && news.StoppedForNews, "the news arrives");
    CdSession rich{news.World, news.State};
    CdTake(rich, context, "opp.relief_shift");
    CdSession closed = rich;
    CdFinish(closed, CdStyle{});
    const CdSession opened = CdOpen(context);

    for (const CountdownState* state : {&opened.State, static_cast<const CountdownState*>(&rich.State), static_cast<const CountdownState*>(&closed.State)})
    {
        const std::string text = SerialiseCountdown(*state);
        const std::optional<CountdownState> back = ParseCountdown(text);
        CdExpect(back.has_value() && *back == *state, "every kind of state saves and loads");
        CdExpect(SerialiseCountdown(*state) == text && HashCountdown(*state) == HashCountdown(*back), "the same state always gives the same bytes and hash");
        CdExpect(text.rfind("pw_countdown 1\n", 0) == 0 && text.find("\nend ") != std::string::npos, "the text begins with its header and ends with its hash");
    }
    CdExpect(HashCountdown(opened.State) != HashCountdown(rich.State) && HashCountdown(rich.State) != HashCountdown(closed.State), "different states have different hashes");

    // Truncation at any byte and a corrupted byte are refused.
    const std::string text = SerialiseCountdown(rich.State);
    for (std::size_t length = 0; length < text.size(); ++length)
        CdExpect(!ParseCountdown(std::string_view(text).substr(0, length)).has_value(), "truncated text is refused");
    for (std::size_t at = 0; at < text.size(); at += 5)
    {
        std::string corrupt = text;
        corrupt[at] = corrupt[at] == '7' ? '8' : '7';
        if (corrupt != text)
            CdExpect(!ParseCountdown(corrupt).has_value(), "corrupted text is refused");
    }
    CdExpect(!ParseCountdown(text + "x\n").has_value() && !ParseCountdown(text + "\n").has_value() && !ParseCountdown("").has_value(), "extra text is refused");

    // Edits that keep the hash honest are still refused, because the parse re-derives what it can.
    const auto refused = [&rich](const char* message, const std::function<void(CountdownState&)>& edit) {
        CountdownState edited = rich.State;
        edit(edited);
        CdExpect(edited != rich.State, "the edit changes something");
        CdExpect(!ParseCountdown(SerialiseCountdown(edited)).has_value(), message);
    };
    const std::size_t resolvedIndex = 0;
    CdExpect(rich.State.Commitments[resolvedIndex].Resolved, "the first commitment has resolved");
    refused("a different draw", [&](CountdownState& s) { s.Commitments[resolvedIndex].Roll = (s.Commitments[resolvedIndex].Roll + 1) % 1000; });
    refused("a different outcome", [&](CountdownState& s) {
        s.Commitments[resolvedIndex].Outcome = s.Commitments[resolvedIndex].Outcome == CountdownOutcome::Good ? CountdownOutcome::Poor : CountdownOutcome::Good;
    });
    refused("a different seed", [&](CountdownState& s) { s.Commitments[resolvedIndex].Seed += 1; });
    refused("odds that do not sum", [&](CountdownState& s) { s.Commitments[resolvedIndex].Odds[0] += 1; });
    refused("a different window", [&](CountdownState& s) { s.Commitments[resolvedIndex].Start += 1; });
    refused("unresolving", [&](CountdownState& s) { s.Commitments[resolvedIndex].Resolved = false; });
    refused("an unknown opportunity", [&](CountdownState& s) { s.Commitments[resolvedIndex].Id = "opp.not_real"; });
    refused("a different preparation total", [&](CountdownState& s) { s.OutcomePrep[1] += 1; });
    refused("a different standing", [&](CountdownState& s) { s.Goodwill += 1; });
    refused("a different route", [&](CountdownState& s) { s.RouteMinutes += 1; });
    refused("another span", [&](CountdownState& s) { s.End += 1; });
    refused("another onset", [&](CountdownState& s) { s.EventOnset += 1; });
    refused("a closed countdown with no news", [&](CountdownState& s) { s.Closed = true; s.EventFired = false; });
    refused("a declined obligation with hours", [&](CountdownState& s) {
        for (CountdownObligationRecord& record : s.Obligations)
            record.Minutes = record.Disposition == CountdownDisposition::Declined ? 30 : record.Minutes;
    });
    refused("a negotiation that never happened", [&](CountdownState& s) { s.Obligations[0].Negotiated = !s.Obligations[0].Negotiated; });
    refused("a ledger out of time", [&](CountdownState& s) { std::swap(s.Ledger.front(), s.Ledger.back()); });
    refused("a bad player", [&](CountdownState& s) { s.Player = "NOT AN ID"; });
    refused("a second Saturday", [&](CountdownState& s) {
        CdSession other = CdOpen(context);
        CdTake(other, context, "opp.ground_trial");
        s.Commitments.push_back(other.State.Commitments[0]);
    });

    // The receipt is stable and says what each choice cost.
    const std::string receipt = RenderCountdownReceipt(closed.State);
    CdExpect(receipt == RenderCountdownReceipt(closed.State) && receipt.find("committed opp.study_circle: 120 minutes, closes 2 other options") != std::string::npos &&
                 receipt.find("resolved opp.study_circle: ") != std::string::npos && receipt.find("news news.canal_gate_break: the road would add 90 minutes") != std::string::npos &&
                 receipt.find("closed exam_morning") != std::string::npos && receipt.find("honoured oblig.sibling_pickup") != std::string::npos,
             "the receipt shows what each choice closed, each draw, the news, what was kept and the close");
}
