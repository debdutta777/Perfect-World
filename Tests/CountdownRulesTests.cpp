// ROADMAP M4-03: mutual exclusion and disclosure. Opportunities are shaped by relationships, their
// chances are shown before they are taken, and the chance shown is the chance stored.
#include "CountdownTestSupport.h"

#include <algorithm>

using namespace PerfectWorld;

namespace
{
const CountdownOffer* FindOffer(const std::vector<CountdownOffer>& offers, std::string_view id)
{
    for (const CountdownOffer& offer : offers)
    {
        if (offer.Def->Id == id)
            return &offer;
    }
    return nullptr;
}

/** Resolves the first `days` days in the default style; the news may stop it earlier. */
void Advance(CdSession& session, int days, const CdStyle& style = {})
{
    std::vector<WeekDayPlan> plan = CdDays(session.State, CdSpanDay(session.World), style, session.World.DayLoop.Now);
    plan.resize(static_cast<std::size_t>(days));
    CountdownResolveResult resolved = ResolveCountdownDays(session.World, session.State, plan, CdCats());
    if (!resolved.Ok())
        std::cerr << "resolve refused: " << CountdownErrorCode(resolved.Issue.Error) << ' ' << resolved.Issue.Detail << '\n';
    CdExpect(resolved.Ok(), "the days resolve");
    session.World = std::move(resolved.World);
    session.State = std::move(resolved.State);
}

void ExpectRefused(const CdSession& session, const CountdownContext& context, const char* id, CountdownError error, const char* message)
{
    const CountdownTakeResult refused = TakeOpportunity(session.World, session.State, context, id);
    CdExpect(!refused.Ok() && refused.Issue.Error == error, message);
    CdExpect(refused.State == session.State, "a refused commitment changes nothing");
}
} // namespace

void TestCountdownExclusion()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);

    // One opportunity per group: the others of its group are refused, and so is taking the same one twice.
    CdSession session = CdOpen(context);
    CdTake(session, context, "opp.study_circle");
    ExpectRefused(session, context, "opp.study_circle", CountdownError::Taken, "the same opportunity cannot be taken twice");
    ExpectRefused(session, context, "opp.ground_trial", CountdownError::GroupTaken, "a second Saturday invitation is refused");
    ExpectRefused(session, context, "opp.repair_workshop", CountdownError::GroupTaken, "so is a third");
    const std::vector<CountdownOffer> offers = CountdownOffers(session.World, session.State, context);
    CdExpect(FindOffer(offers, "opp.study_circle")->Availability == CountdownAvailability::Taken &&
                 FindOffer(offers, "opp.ground_trial")->Availability == CountdownAvailability::GroupTaken,
             "the offers say which is taken and which is closed by it");
    CdExpect(FindOffer(offers, "opp.mock_debate")->Availability == CountdownAvailability::Open, "another group is still open");
    const CdSession fresh = CdOpen(context);
    CdExpect(FindOffer(CountdownOffers(fresh.World, fresh.State, context), "opp.study_circle")->Closes ==
                 std::vector<std::string>({"opp.ground_trial", "opp.repair_workshop"}),
             "an offer says what taking it closes");
    CdTake(session, context, "opp.mock_debate");
    CdExpect(session.State.Commitments.size() == 2, "two groups, two commitments");
    // Every group allows exactly one: take the first open member of every group and check no second one can be.
    CdSession everything = CdOpen(context);
    std::vector<std::string_view> groups;
    for (const CountdownOpportunityDef& def : CountdownOpportunities())
    {
        if (def.NeedsNews)
            continue;
        const CountdownTakeResult taken = TakeOpportunity(everything.World, everything.State, context, def.Id);
        const bool firstOfGroup = std::find(groups.begin(), groups.end(), def.Group) == groups.end();
        if (firstOfGroup && taken.Ok())
        {
            everything.State = taken.State;
            groups.push_back(def.Group);
        }
        else
        {
            CdExpect(!taken.Ok() && (taken.Issue.Error == CountdownError::GroupTaken || taken.Issue.Error == CountdownError::TimeConflict),
                     "no group allows a second, and no two windows overlap");
        }
    }
    for (std::size_t i = 0; i < everything.State.Commitments.size(); ++i)
    {
        for (std::size_t j = i + 1; j < everything.State.Commitments.size(); ++j)
        {
            const CountdownCommitment& a = everything.State.Commitments[i];
            const CountdownCommitment& b = everything.State.Commitments[j];
            CdExpect(CountdownFindOpportunity(a.Id)->Group != CountdownFindOpportunity(b.Id)->Group && (a.End <= b.Start || b.End <= a.Start),
                     "the commitments are in different groups and do not overlap");
        }
    }

    // Time: an opportunity cannot overlap what is owed. The shop shift stands on Sunday 11:00 to 13:00.
    CdSession kitchen = CdOpen(context);
    ExpectRefused(kitchen, context, "opp.community_kitchen", CountdownError::TimeConflict, "the relief kitchen overlaps the shop shift");
    CdExpect(FindOffer(CountdownOffers(kitchen.World, kitchen.State, context), "opp.community_kitchen")->Availability == CountdownAvailability::TimeConflict,
             "and the offer says so");

    // The relationship gates the offer: no contact, too little respect.
    std::vector<CdTie> noTeacher = ties;
    noTeacher.erase(std::remove_if(noTeacher.begin(), noTeacher.end(), [](const CdTie& tie) { return tie.Role == "teacher"; }), noTeacher.end());
    const CountdownContext withoutTeacher = CdContext(&book, noTeacher);
    CdSession lonely = CdOpen(withoutTeacher);
    ExpectRefused(lonely, withoutTeacher, "opp.teacher_review", CountdownError::NoContact, "no teacher, no office hour");
    CdExpect(lonely.State.Obligations.size() == 2, "and no obligation to the teacher");
    std::vector<CdTie> cold = ties;
    for (CdTie& tie : cold)
        tie.Respect = tie.Role == "teacher" ? 2000 : tie.Respect;
    const RelationshipBook coldBook = CdBook(cold);
    const CountdownContext coldContext = CdContext(&coldBook, cold);
    CdSession distant = CdOpen(coldContext);
    ExpectRefused(distant, coldContext, "opp.teacher_review", CountdownError::RelationTooLow, "too little respect, no office hour");
    CdExpect(FindOffer(CountdownOffers(distant.World, distant.State, coldContext), "opp.teacher_review")->Relation == 2000, "the offer shows the relationship that counts");
    CountdownContext noBook = context;
    noBook.Relationships = nullptr;
    ExpectRefused(CdOpen(noBook), noBook, "opp.study_circle", CountdownError::RelationTooLow, "with no relationship book everyone is a stranger");

    // The news first: a response is not offered before the Gate breaks, and the response group allows one.
    ExpectRefused(session, context, "opp.relief_shift", CountdownError::NeedsNews, "a response needs the news");
    ExpectRefused(session, context, "opp.not_an_opportunity", CountdownError::UnknownOpportunity, "an unknown id is refused");

    // Windows pass: after the weekend begins, Saturday's invitations are gone.
    CdSession later = CdOpen(context);
    Advance(later, 2);
    ExpectRefused(later, context, "opp.ground_trial", CountdownError::WindowClosed, "a window that has passed is closed");

    // After the news, two windows from different groups can still overlap: the last Saturday has a paper, a quiet day and a route to walk.
    CdSession after = CdOpen(context);
    Advance(after, 7);
    CdExpect(after.State.EventFired, "the news has arrived");
    CdTake(after, context, "opp.paper_marathon");
    ExpectRefused(after, context, "opp.quiet_day", CountdownError::GroupTaken, "the paper and the quiet day are one choice");
    ExpectRefused(after, context, "opp.scout_route", CountdownError::TimeConflict, "walking the route overlaps the paper");
    CdSession quiet = CdOpen(context);
    Advance(quiet, 7);
    CdTake(quiet, context, "opp.quiet_day");
    ExpectRefused(quiet, context, "opp.scout_route", CountdownError::TimeConflict, "walking the route overlaps the quiet day");
    CdTake(quiet, context, "opp.relief_shift");
    ExpectRefused(quiet, context, "opp.friend_ride", CountdownError::GroupTaken, "the responses are one choice");
    // A closed countdown takes nothing more.
    CdSession closed = CdOpen(context);
    CdFinish(closed, CdStyle{});
    ExpectRefused(closed, context, "opp.quiet_day", CountdownError::Closed, "nothing is committed once the countdown has closed");
}

void TestCountdownDisclosure()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);
    const CdSession session = CdOpen(context);
    const std::vector<CountdownOffer> offers = CountdownOffers(session.World, session.State, context);
    CdExpect(offers.size() == CountdownOpportunities().size(), "every opportunity is listed with its availability");

    for (const CountdownOffer& offer : offers)
    {
        const CountdownOpportunityDef& def = *offer.Def;
        CdExpect(offer.Odds[0] + offer.Odds[1] + offer.Odds[2] == 1000, "the odds shown sum to one");
        for (std::size_t i = 0; i < CountdownOutcomeCount; ++i)
            CdExpect(offer.Bands[i] == CountdownBandOf(offer.Odds[i]), "each odds has its band");
        CdExpect(offer.Minutes == def.Block.Minutes && offer.End - offer.Start == offer.Minutes && offer.Start == CountdownInstantOf(def.Day, def.StartMinute),
                 "the window and the time it costs are shown");
        CdExpect(offer.Odds[2] <= def.BaseOdds[2] && offer.Odds[0] >= def.BaseOdds[0] && offer.Odds[1] == def.BaseOdds[1],
                 "closeness moves chance from poor to good and nowhere else");
        if (def.Role.empty())
            CdExpect(offer.Odds == def.BaseOdds && offer.Contact.empty(), "an opportunity with no contact has its table's odds");
        // The chance shown is the chance stored, whoever is committed to.
        if (offer.Availability == CountdownAvailability::Open)
        {
            CdSession copy = session;
            CdTake(copy, context, std::string(def.Id).c_str());
            const CountdownCommitment& stored = copy.State.Commitments.back();
            CdExpect(stored.Odds == offer.Odds && stored.Seed != 0 && !stored.Resolved && stored.Start == offer.Start && stored.End == offer.End &&
                         stored.Contact == offer.Contact,
                     "committing stores exactly the odds, window and contact that were shown, and a seed");
        }
    }

    // The relationship shapes the odds in the stated way: 28,000 of trust is 2 steps above 5,000 of need... 23,000 over, two full 10,000s, 100 permille.
    const CountdownOffer* circle = FindOffer(offers, "opp.study_circle");
    CdExpect(circle->Relation == 28000 && circle->Contact == "npc_friend" && circle->Odds == CountdownOdds({400, 550, 50}),
             "a close friend moves a hundred permille from poor to good");
    CdExpect(circle->Bands[0] == CountdownBand::Possible && circle->Bands[1] == CountdownBand::Likely && circle->Bands[2] == CountdownBand::Unlikely,
             "and the bands read possible, likely, unlikely");
    for (const std::int32_t trust : {0, 5000, 14999, 15000, 100000})
    {
        std::vector<CdTie> varied = ties;
        varied[0].Trust = trust;
        const RelationshipBook variedBook = CdBook(varied);
        const CountdownContext variedContext = CdContext(&variedBook, varied);
        const CountdownOffer shown = *FindOffer(CountdownOffers(session.World, session.State, variedContext), "opp.study_circle");
        const std::int32_t expectedShift = trust < 5000 ? 0 : std::min(250, ((trust - 5000) / 10000) * 50);
        const std::int32_t moved = std::min(expectedShift, 150);
        CdExpect(shown.Odds[0] == 300 + moved && shown.Odds[2] == 150 - moved && shown.Odds[1] == 550, "the odds follow the stated rule");
        CdExpect(shown.Availability == (trust >= 5000 ? CountdownAvailability::Open : CountdownAvailability::RelationTooLow), "and the offer opens at the minimum");
    }
    // No amount of closeness makes anything certain or removes the middle.
    std::vector<CdTie> devoted = ties;
    devoted[0].Trust = RelationshipMilliMax;
    const RelationshipBook devotedBook = CdBook(devoted);
    const CountdownOffer best = *FindOffer(CountdownOffers(session.World, session.State, CdContext(&devotedBook, devoted)), "opp.study_circle");
    CdExpect(best.Odds[0] < 1000 && best.Odds[1] == 550 && best.Odds == CountdownOdds({450, 550, 0}), "closeness is capped by what the poor outcome had to give");

    // The effect of every outcome is on the offer, so the player sees what could happen.
    CdExpect(circle->Def->Effects[0].Preparation[static_cast<std::size_t>(CountdownDimension::Analysis)] >
                 circle->Def->Effects[2].Preparation[static_cast<std::size_t>(CountdownDimension::Analysis)] &&
                 circle->Def->Effects[2].Composure < 0,
             "the good outcome teaches more than the poor one, and the poor one can cost composure");
    // The quiet day is near certain; the paper is a gamble.
    CdExpect(FindOffer(offers, "opp.quiet_day")->Bands[0] == CountdownBand::VeryLikely && FindOffer(offers, "opp.paper_marathon")->Bands[0] == CountdownBand::Possible,
             "a quiet day is very likely good, a mock paper only possibly");
}
