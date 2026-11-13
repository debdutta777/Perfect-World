// ROADMAP M4-03: three scripted strategies finish with different but comparable profiles, the
// countdown ends at exactly 06:00 on examination morning, and the whole thing is deterministic.
#include "CountdownTestSupport.h"

#include <algorithm>
#include <numeric>

using namespace PerfectWorld;

namespace
{
enum class Strategy
{
    Study,
    People,
    Rest
};

struct Played
{
    CdSession Session;
    CountdownProfile Profile;
};

void Negotiate(CdSession& session, const CountdownContext& context, const char* id, CountdownProposal proposal, bool* accepted = nullptr)
{
    const CountdownTakeResult result = NegotiateObligation(session.World, session.State, context, id, proposal);
    CdExpect(result.Ok(), "the negotiation is made");
    session.State = result.State;
    if (accepted != nullptr)
        *accepted = CountdownFindRecord(session.State, id)->Accepted;
}

/** Takes the response if it is still on offer; a strategy that cannot (a window gone, a conflict) goes without. */
void TryTake(CdSession& session, const CountdownContext& context, const char* id)
{
    const CountdownTakeResult taken = TakeOpportunity(session.World, session.State, context, id);
    if (taken.Ok())
        session.State = taken.State;
}

CdStyle StyleOf(Strategy strategy)
{
    CdStyle style;
    switch (strategy)
    {
    case Strategy::Study:
        style.Study = 2;
        style.SleepStart = 1380;
        break;
    case Strategy::People:
        style.Study = 0;
        style.Chores = true;
        style.Rest = true;
        break;
    case Strategy::Rest:
        style.Study = 0;
        style.Run = true;
        style.Rest = true;
        style.SleepStart = 1290;
        break;
    }
    return style;
}

/** The choices made on Awakening evening, with the offers and proposals in front of the player. */
void ChooseAtStart(CdSession& session, const CountdownContext& context, Strategy strategy)
{
    switch (strategy)
    {
    case Strategy::Study:
        for (const char* id : {"opp.study_circle", "opp.teacher_review", "opp.paper_marathon"})
            CdTake(session, context, id);
        Negotiate(session, context, "oblig.sibling_pickup", CountdownProposal::Share);
        Negotiate(session, context, "oblig.neighbour_shop_shift", CountdownProposal::Decline);
        break;
    case Strategy::People:
    {
        CdTake(session, context, "opp.repair_workshop");
        bool freed = false;
        Negotiate(session, context, "oblig.neighbour_shop_shift", CountdownProposal::Defer, &freed);
        CdTake(session, context, freed ? "opp.community_kitchen" : "opp.mock_debate");
        CdTake(session, context, "opp.teacher_review");
        CdTake(session, context, "opp.quiet_day");
        break;
    }
    case Strategy::Rest:
        for (const char* id : {"opp.ground_trial", "opp.practical_bench", "opp.quiet_day"})
            CdTake(session, context, id);
        break;
    }
}

Played Play(Strategy strategy, const CountdownContext& context, std::uint64_t seed = 4243)
{
    CdSession session = CdOpen(context, seed);
    ChooseAtStart(session, context, strategy);
    const CdStyle style = StyleOf(strategy);
    for (int guard = 0; !session.State.Closed && guard < 20; ++guard)
    {
        std::vector<WeekDayPlan> days = CdDays(session.State, CdSpanDay(session.World), style, session.World.DayLoop.Now);
        days.resize(std::min<std::size_t>(days.size(), 7));
        CountdownResolveResult resolved = ResolveCountdownDays(session.World, session.State, days, CdCats());
        if (!resolved.Ok())
            std::cerr << "resolve refused: " << CountdownErrorCode(resolved.Issue.Error) << ' ' << resolved.Issue.Detail << '\n';
        CdExpect(resolved.Ok(), "the days of a strategy resolve");
        session.World = std::move(resolved.World);
        session.State = std::move(resolved.State);
        if (resolved.StoppedForNews)
        {
            // The news has arrived: each strategy answers it in its own way.
            switch (strategy)
            {
            case Strategy::Study: TryTake(session, context, "opp.friend_ride"); break;
            case Strategy::People: TryTake(session, context, "opp.relief_shift"); break;
            case Strategy::Rest: TryTake(session, context, "opp.friend_ride"); break;
            }
        }
    }
    CdExpect(session.State.Closed, "the strategy reaches examination morning");
    const CountdownProfile profile = CountdownProfileOf(session.World, session.State);
    return {std::move(session), profile};
}

void Print(const char* name, const CountdownProfile& p)
{
    std::cout << name << ": prep";
    for (const std::int32_t v : p.Preparation)
        std::cout << ' ' << v;
    std::cout << " composure " << p.Composure << " goodwill " << p.Goodwill << " delay " << p.RouteDelayMinutes << " total " << p.Total << '\n';
}

std::size_t Strongest(const CountdownProfile& profile)
{
    return static_cast<std::size_t>(std::max_element(profile.Preparation.begin(), profile.Preparation.end()) - profile.Preparation.begin());
}
} // namespace

void TestCountdownStrategies()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);

    const Played study = Play(Strategy::Study, context);
    const Played people = Play(Strategy::People, context);
    const Played rest = Play(Strategy::Rest, context);
    if (std::getenv("PW_COUNTDOWN_PRINT") != nullptr)
    {
        Print("study ", study.Profile);
        Print("people", people.Profile);
        Print("rest  ", rest.Profile);
        for (const Played* played : {&study, &people, &rest})
        {
            std::cout << "  dayloop";
            for (const std::int32_t v : played->Session.State.DayLoopPrep)
                std::cout << ' ' << v;
            std::cout << "  outcome";
            for (const std::int32_t v : played->Session.State.OutcomePrep)
                std::cout << ' ' << v;
            std::cout << "  composure " << played->Session.State.Composure << " energy " << played->Session.World.DayLoop.Condition.Energy << " stress "
                      << played->Session.World.DayLoop.Condition.Stress << " hunger " << played->Session.World.DayLoop.Condition.Hunger << std::endl;
            for (const CountdownCommitment& c : played->Session.State.Commitments)
                std::cout << "    " << c.Id << ' ' << CountdownOutcomeName(c.Outcome) << std::endl;
        }
    }

    if (std::getenv("PW_COUNTDOWN_PRINT") != nullptr)
    {
        for (int sessions = 0; sessions <= 5; ++sessions)
        {
            CdSession plain = CdOpen(context);
            CdStyle style;
            style.Study = sessions;
            CdFinish(plain, style);
            std::cout << "plain study " << sessions << ": analysis " << plain.State.DayLoopPrep[1] << " composure "
                      << CountdownProfileOf(plain.World, plain.State).Composure << " energy " << plain.World.DayLoop.Condition.Energy << std::endl;
        }
    }

    // Every strategy ends at exactly 06:00 on examination morning, with the clocks together and nobody marked absent.
    for (const Played* played : {&study, &people, &rest})
    {
        const WorldState& world = played->Session.World;
        const CountdownState& state = played->Session.State;
        CdExpect(state.Closed && world.DayLoop.Now == CountdownEndInstant() && world.Clock.Now() == CountdownEndInstant() && IsWorldDayInStep(world),
                 "the countdown ends exactly at 06:00 on examination morning");
        CdExpect(played->Profile.Final && state.Ledger.back().Kind == CountdownLedgerKind::Closed && state.Ledger.back().At == CountdownEndInstant(),
                 "its ledger ends with the close at that minute");
        CdExpect(world.DayLoop.Attendance.AbsentDays == 0 && world.DayLoop.Attendance.SchoolDays == 0, "no day of the ten is a school day, so none is marked absent");
        CdExpect(world.DayLoop.Wakes == 10, "ten nights, ten mornings, the last at six on examination day");
        CdExpect(state.PaddedMinutes == 0 && state.Commitments.size() >= 3, "nothing was spent on the player's behalf, and several things were chosen");
        for (const CountdownCommitment& commitment : state.Commitments)
            CdExpect(commitment.Resolved, "every commitment resolved");
        for (const CountdownObligationRecord& record : state.Obligations)
            CdExpect(record.Done || record.Disposition == CountdownDisposition::Declined, "every obligation was kept or openly declined");
        CdExpect(state.EventFired && world.Gates.at(std::string(CountdownWorldEvent().GateId)).StatusSince == CountdownInstantOf(5, 1180),
                 "the one world event happened at its fixed minute in every strategy");
        const CountdownProfile& p = played->Profile;
        CdExpect(p.Total == std::accumulate(p.Preparation.begin(), p.Preparation.end(), 0) + p.Composure + p.Goodwill, "the total is the sum of the scores");
        for (const std::int32_t score : p.Preparation)
            CdExpect(score >= 0 && score <= CountdownScoreMax, "each preparation score is in range");
        CdExpect(p.Composure >= 0 && p.Composure <= CountdownScoreMax && p.Goodwill >= 0 && p.Goodwill <= CountdownScoreMax, "composure and goodwill are in range");
    }

    // Different: each strategy leaves a clearly different person, with a different strongest preparation.
    CdExpect(CountdownProfilesDiffer(study.Profile, people.Profile) && CountdownProfilesDiffer(study.Profile, rest.Profile) &&
                 CountdownProfilesDiffer(people.Profile, rest.Profile),
             "the three profiles differ by at least the distinct distance");
    CdExpect(Strongest(study.Profile) != Strongest(people.Profile) && Strongest(study.Profile) != Strongest(rest.Profile) &&
                 Strongest(people.Profile) != Strongest(rest.Profile),
             "each has a different strongest preparation");
    CdExpect(Strongest(study.Profile) == static_cast<std::size_t>(CountdownDimension::Analysis) &&
                 Strongest(people.Profile) == static_cast<std::size_t>(CountdownDimension::Logistics) &&
                 Strongest(rest.Profile) == static_cast<std::size_t>(CountdownDimension::Endurance),
             "analysis, logistics and endurance are the strongest of the studious, the sociable and the rested");
    CdExpect(study.Profile.Preparation[1] > people.Profile.Preparation[1] && study.Profile.Preparation[1] > rest.Profile.Preparation[1],
             "and has the most analysis");
    CdExpect(people.Profile.Preparation[4] > study.Profile.Preparation[4] && people.Profile.Preparation[4] > rest.Profile.Preparation[4] &&
                 people.Profile.Preparation[5] > study.Profile.Preparation[5] && people.Profile.Preparation[5] > rest.Profile.Preparation[5],
             "the sociable one has the most negotiation and logistics");
    CdExpect(people.Profile.Goodwill > study.Profile.Goodwill, "and more standing than the one who declined and shared");
    CdExpect(rest.Profile.Composure > study.Profile.Composure && people.Profile.Composure > study.Profile.Composure,
             "those who rested or spent time with people arrive with more composure than the one who crammed");
    CdExpect(rest.Profile.Preparation[2] > study.Profile.Preparation[2], "and the most endurance");
    CdExpect(study.Profile.Declined == 1 && study.Profile.Shared + study.Profile.Honoured >= 1 && rest.Profile.Honoured == 3 && rest.Profile.Declined == 0,
             "the obligations went the way the strategies chose");

    // Comparable: the totals are within the stated spread, and no profile is better in every score than another.
    CdExpect(CountdownProfilesComparable(study.Profile, people.Profile) && CountdownProfilesComparable(study.Profile, rest.Profile) &&
                 CountdownProfilesComparable(people.Profile, rest.Profile),
             "the totals are within a quarter of each other");
    for (const Played* a : {&study, &people, &rest})
    {
        for (const Played* b : {&study, &people, &rest})
            CdExpect(a == b || !CountdownProfileDominates(a->Profile, b->Profile), "no strategy is better in every score than another");
    }
    CdExpect(CountdownProfilesComparable(study.Profile, study.Profile) && !CountdownProfilesDiffer(study.Profile, study.Profile) &&
                 CountdownProfileDistance(study.Profile, study.Profile) == 0,
             "a profile is comparable to, and not different from, itself");
    CountdownProfile tiny;
    CdExpect(!CountdownProfilesComparable(study.Profile, tiny), "a very different total is not comparable");

    // The same strategies remain comparable and distinct for other lives, whatever their draws.
    for (const std::uint64_t seed : {11ULL, 29ULL, 101ULL, 777ULL, 9001ULL, 31337ULL})
    {
        const CountdownProfile a = Play(Strategy::Study, context, seed).Profile;
        const CountdownProfile b = Play(Strategy::People, context, seed).Profile;
        const CountdownProfile c = Play(Strategy::Rest, context, seed).Profile;
        CdExpect(CountdownProfilesComparable(a, b) && CountdownProfilesComparable(a, c) && CountdownProfilesComparable(b, c),
                 "in another life the three are still comparable");
        CdExpect(CountdownProfilesDiffer(a, b) && CountdownProfilesDiffer(a, c) && CountdownProfilesDiffer(b, c), "and still different");
    }

    // Deterministic: the same strategy twice gives the same bytes, the same world and the same profile.
    const Played again = Play(Strategy::Study, context);
    CdExpect(SerialiseCountdown(again.Session.State) == SerialiseCountdown(study.Session.State) && again.Profile == study.Profile &&
                 HashWorldState(again.Session.World) == HashWorldState(study.Session.World),
             "the same strategy twice is identical to the byte and to the world hash");
    CdExpect(RenderCountdownReceipt(again.Session.State) == RenderCountdownReceipt(study.Session.State), "and so is its receipt");
}

namespace
{
/** Plays the default style until the clock is in span day `day` (0 to 9). */
CdSession ToDay(const CountdownContext& context, int day)
{
    CdSession session = CdOpen(context);
    for (int guard = 0; CdSpanDay(session.World) < day && guard < 20; ++guard)
    {
        std::vector<WeekDayPlan> days = CdDays(session.State, CdSpanDay(session.World), CdStyle{}, session.World.DayLoop.Now);
        days.resize(static_cast<std::size_t>(std::min<int>(7, day - CdSpanDay(session.World))));
        CountdownResolveResult resolved = ResolveCountdownDays(session.World, session.State, days, CdCats());
        CdExpect(resolved.Ok(), "the days up to the last resolve");
        session.World = std::move(resolved.World);
        session.State = std::move(resolved.State);
    }
    CdExpect(CdSpanDay(session.World) == day && !session.State.Closed, "the clock is in the day asked for");
    return session;
}

std::vector<WeekDayPlan> LastDay(const CdSession& session, int sleepMinutes, bool onlyMeal)
{
    CdStyle style;
    style.Study = 0;
    std::vector<WeekDayPlan> days = CdDays(session.State, 9, style, session.World.DayLoop.Now);
    for (WeekEntry& entry : days[0].Entries)
    {
        if (entry.Kind == WeekEntryKind::Sleep)
            entry.Minutes = sleepMinutes;
    }
    if (onlyMeal)
    {
        days[0].Entries.erase(std::remove_if(days[0].Entries.begin(), days[0].Entries.end(),
                                             [](const WeekEntry& entry) { return entry.Kind == WeekEntryKind::Sleep || entry.StartMinute > 420; }),
                              days[0].Entries.end());
    }
    return days;
}
} // namespace

void TestCountdownClose()
{
    const std::vector<CdTie> ties = CdDefaultTies();
    const RelationshipBook book = CdBook(ties);
    const CountdownContext context = CdContext(&book, ties);
    const CdSession eve = ToDay(context, 9);
    CdExpect(eve.World.DayLoop.Now == CountdownInstantOf(9, 360), "the last day opens at 06:00 on Sunday 1 February");

    // The clock is public and cannot be extended: a plan that runs past 06:00 is refused whole.
    const CountdownResolveResult over = ResolveCountdownDays(eve.World, eve.State, LastDay(eve, 540, false), CdCats());
    CdExpect(over.Issue.Error == CountdownError::BeyondExam && over.State == eve.State && over.World.DayLoop.Now == eve.World.DayLoop.Now && !over.Closed,
             "a night that ends at seven is refused and changes nothing");

    // Sleeping exactly to 06:00 closes with nothing done on the player's behalf.
    const CountdownResolveResult exact = ResolveCountdownDays(eve.World, eve.State, LastDay(eve, 480, false), CdCats());
    CdExpect(exact.Ok() && exact.Closed && exact.State.PaddedMinutes == 0 && exact.World.DayLoop.Now == CountdownEndInstant() && exact.Week.Ledger.DecidedByOrder == 0,
             "a night that ends at six closes the countdown with nothing spent for the player");

    // Sleeping short leaves hours, which are spent awake and disclosed under a standing order.
    const CountdownResolveResult padded = ResolveCountdownDays(eve.World, eve.State, LastDay(eve, 420, false), CdCats());
    CdExpect(padded.Ok() && padded.Closed && padded.World.DayLoop.Now == CountdownEndInstant() && padded.State.PaddedMinutes == 60,
             "a night that ends at five is topped up by an hour to reach six");
    CdExpect(padded.Week.Ledger.DecidedByOrder == 1 && padded.Week.Disclosures.size() == 1 && padded.Week.Disclosures[0].OrderId == CountdownPadOrderId &&
                 padded.State.Ledger.back().Kind == CountdownLedgerKind::Closed && padded.State.Ledger.back().A == 60,
             "and the hour is disclosed under its order and in the ledger");
    CdExpect(RenderCountdownReceipt(padded.State).find("60 minutes spent awake on your behalf") != std::string::npos, "and on the receipt");
    // A day that leaves most of the day unplanned is topped up across midnight, in pieces that stay inside a calendar day.
    const std::vector<WeekDayPlan> meal = LastDay(eve, 480, true);
    const CountdownResolveResult wide = ResolveCountdownDays(eve.World, eve.State, meal, CdCats());
    CdExpect(wide.Ok() && wide.Closed && wide.World.DayLoop.Now == CountdownEndInstant() && wide.State.PaddedMinutes == CountdownEndInstant() - CountdownInstantOf(9, 410) &&
                 wide.Week.Disclosures.size() == 2,
             "a day left empty is spent awake to the minute, in two pieces either side of midnight");
    CdExpect(padded.World.DayLoop.Condition.Energy > wide.World.DayLoop.Condition.Energy, "and staying awake is felt in the morning's condition");

    // After the close nothing more resolves; before the last day a plan does not close it.
    CdExpect(ResolveCountdownDays(exact.World, exact.State, LastDay(eve, 480, false), CdCats()).Issue.Error == CountdownError::Closed, "a closed countdown resolves nothing");
    const CdSession early = ToDay(context, 8);
    CdExpect(!early.State.Closed && early.State.PaddedMinutes == 0, "a plan that stops before the last day does not close the countdown");
    CdExpect(ResolveCountdownDays(early.World, early.State, CdDays(early.State, 8, CdStyle{}, early.World.DayLoop.Now), CdCats()).Closed,
             "the last two days close it");
}
