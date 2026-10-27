#include "WorldCountdown.h"

#include "ContentRegistry.h"
#include "SchoolCalendar.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace PerfectWorld;

namespace
{
int R95Assertions = 0;

void R95Expect(bool condition, const char* message)
{
    ++R95Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<ContentFile> R95ReadContent(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const auto& path : paths)
    {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream text;
        text << input.rdbuf();
        files.push_back({"/" + std::filesystem::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

WorldState R95World(GameInstant at)
{
    WorldSim sim(9505);
    R95Expect(sim.Submit({CommandSource::System, AdvanceTime{at}}).Accepted, "fixture reaches its calendar instant");
    auto world = sim.State();
    world.DayLoop.Condition.Energy = MeterMax;
    world.DayLoop.Attendance = StartAttendanceRecord(CalendarDayOfInstant(at) + 1);
    R95Expect(IsWorldDayInStep(world), "fixture clocks agree");
    return world;
}

AwakeningState R95Inputs(const ContentDb& content)
{
    const auto seeded = SeedAwakeningInputs(content, CharacterCreationSave{}, 9505);
    R95Expect(seeded.Ok(), "real creation inputs seed");
    return seeded.State;
}

// A trusted R9-02 completion fixture, never a production session-skip adapter.
void R95Grant(AwakeningState& awakening, const AbilityRecipeSet& recipes, GameInstant at, bool makeUp = false)
{
    const auto input = BuildAwakeningInput(awakening);
    const auto resolved = ResolveAwakening(input, recipes);
    const auto profile = AwakenedStartingProfile(resolved, GameClock::DayNumber(at));
    R95Expect(resolved.Ok() && profile.has_value(), "existing resolver supplies the supervised-completion fixture");
    awakening.Granted = AwakeningGrant{at, input, resolved, *profile};
    if (makeUp)
        awakening.MakeUp = {AwakeningMakeUpPhase::Completed, at - 60};
}

CountdownContext R95Context(bool teacher = false)
{
    CountdownContext context;
    context.Player = "player";
    if (teacher)
        context.Contacts.push_back({"teacher", "npc_teacher"});
    return context;
}

WorldCountdownResult R95Open(const AwakeningState& awakening, const CountdownContext& context, const DayLoopCatalogues& cats)
{
    const auto opened = SynchroniseWorldCountdown(R95World(CountdownStartInstant()), awakening, {}, context, cats);
    R95Expect(opened.Ok() && opened.State.has_value(), "scheduled countdown opens without requiring a grant");
    return opened;
}

void R95Boundary(const ContentDb& content, const AbilityRecipeSet& recipes, const DayLoopCatalogues& cats)
{
    auto awakening = R95Inputs(content);
    const auto at = awakening.AppointmentAt + 60;
    R95Grant(awakening, recipes, at);
    const auto world = R95World(at);
    const auto context = R95Context();
    const auto view = QueryWorldCountdown(world, awakening, {});
    R95Expect(view.Issue.Ok() && view.Awakened && view.OrdinaryDayAvailable && !view.PreparationAvailable,
        "morning tutorial hands off immediately to an ordinary day");
    R95Expect(view.ExaminationAt == GameClock::FromCalendar(2043, 2, 2, 6, 0) &&
        view.RemainingMinutes == view.ExaminationAt - at, "deadline is the fixed calendar instant");
    const auto before = AdvanceWorldCountdown(world, awakening, {}, context, cats, CountdownStartInstant() - at - 1);
    R95Expect(before.Ok() && !before.State && IsWorldDayInStep(before.World), "17:59 does not open early");
    const auto opened = AdvanceWorldCountdown(before.World, awakening, before.State, context, cats, 1);
    R95Expect(opened.Ok() && opened.State && opened.World.Clock.Now() == CountdownStartInstant(), "18:00 opens at its boundary");
    const auto crossing = AdvanceWorldCountdown(world, awakening, {}, context, cats, CountdownStartInstant() - at + 1);
    R95Expect(crossing.Ok() && crossing.State && crossing.World.Clock.Now() == CountdownStartInstant() + 1 &&
        IsWorldDayInStep(crossing.World), "one advance crossing evening opens and keeps both clocks equal");
    const auto repeated = SynchroniseWorldCountdown(opened.World, awakening, opened.State, context, cats);
    R95Expect(repeated.Ok() && repeated.State == opened.State && HashWorldState(repeated.World) == HashWorldState(opened.World),
        "repeat synchronisation neither reopens nor changes the world");
    R95Expect(opened.World.Rng == world.Rng && opened.State->End == awakening.ExaminationAt,
        "opening draws no mutable RNG and never changes the deadline");

    auto pending = R95Inputs(content);
    pending.MakeUp.Phase = AwakeningMakeUpPhase::Required;
    const auto unawakened = R95Open(pending, context, cats);
    R95Expect(!QueryWorldCountdown(unawakened.World, pending, unawakened.State).PreparationAvailable && !pending.Granted,
        "absence still opens the calendar but never grants or enables practice");
    auto busy = R95World(CountdownStartInstant());
    busy.DayLoop.Asleep = true;
    busy.DayLoop.SleptAt = busy.DayLoop.Now;
    busy.DayLoop.Phase = DayLoopPhaseOf(busy.DayLoop);
    R95Expect(SynchroniseWorldCountdown(busy, pending, {}, context, cats).Ok(), "scheduled opening can occur while asleep");
    auto outOfStep = opened.World;
    --outOfStep.DayLoop.Now;
    const auto refused = SynchroniseWorldCountdown(outOfStep, awakening, opened.State, context, cats);
    R95Expect(!refused.Ok() && refused.State == opened.State && refused.World.DayLoop.Now == outOfStep.DayLoop.Now,
        "clock mismatch is refused without repairing history");
    auto extended = awakening;
    ++extended.ExaminationAt;
    R95Expect(!SynchroniseWorldCountdown(opened.World, extended, opened.State, context, cats).Ok(), "deadline tampering is refused");
    auto missingGate = opened.World;
    missingGate.Gates.clear();
    R95Expect(!SynchroniseWorldCountdown(missingGate, awakening, opened.State, context, cats).Ok(),
        "a retained countdown cannot recreate missing world history");
    auto missingOnset = opened.World;
    R95Expect(missingOnset.Clock.Cancel(missingOnset.Clock.Pending().front().Handle), "fixture removes the scheduled onset");
    R95Expect(!SynchroniseWorldCountdown(missingOnset, awakening, opened.State, context, cats).Ok(),
        "a cancelled onset cannot silently erase scheduled news");

    auto active = R95World(CountdownStartInstant() - 15);
    DayCommand plan;
    plan.Kind = DayCommandKind::PlanDay;
    R95Expect(ApplyDayCommand(active.DayLoop, cats, plan).Ok(), "boundary fixture plans its ordinary day");
    DayCommand commit;
    commit.Kind = DayCommandKind::Commit;
    commit.ActivityId = "activity.study_at_home";
    commit.PlaceKind = "home";
    R95Expect(ApplyDayCommand(active.DayLoop, cats, commit).Ok(), "boundary fixture commits to ordinary study");
    DayCommand start;
    start.Kind = DayCommandKind::Start;
    R95Expect(ApplyDayCommand(active.DayLoop, cats, start).Ok(), "boundary fixture starts ordinary study");
    // Model RunTimeTo's transient event boundary. The completed day is staged
    // by WorldDayCommand and published after ChargeTime; it is not yet here.
    R95Expect(!active.Clock.Advance(15).Rejected, "boundary fixture reaches 18:00");
    active.DayLoop.Now = active.Clock.Now();
    active.DayLoop.Phase = DayLoopPhaseOf(active.DayLoop);
    std::vector<DomainEvent> boundaryEvents;
    const auto boundary = OpenCountdownAtCurrentTime(active, context, cats, &boundaryEvents);
    R95Expect(boundary.Ok() && boundary.World.DayLoop == active.DayLoop &&
        boundary.World.Clock.Now() == boundary.World.DayLoop.Now && boundary.State.Start == CountdownStartInstant(),
        "scheduled opening preserves an in-flight ordinary activity at the exact boundary");
    R95Expect(boundary.World.Seq == active.Seq && boundaryEvents.size() == 1 &&
        std::holds_alternative<GateRegistered>(boundaryEvents.front().Payload),
        "boundary emits the Gate registration without nested command sequences");
    const auto eventCount = boundaryEvents.size();
    R95Expect(!OpenCountdownAtCurrentTime(boundary.World, context, cats, &boundaryEvents).Ok() &&
        boundaryEvents.size() == eventCount, "refused reopening emits no partial events");
    R95Expect(!SynchroniseWorldCountdown(active, awakening, {}, context, cats).Ok(),
        "external snapshot adapter still refuses an unpublished transient day");
}

void R95LateEntry(const ContentDb& content, const AbilityRecipeSet& recipes, const DayLoopCatalogues& cats)
{
    auto awakening = R95Inputs(content);
    const auto context = R95Context();
    const auto scheduled = R95Open(awakening, context, cats);
    const auto lateAt = GameClock::FromCalendar(2043, 1, 31, 10, 0);
    const auto late = AdvanceWorldCountdown(scheduled.World, awakening, scheduled.State, context, cats,
        lateAt - scheduled.World.Clock.Now());
    R95Expect(late.Ok() && late.State->EventFired && IsWorldDayInStep(late.World), "scheduled news survives late make-up arrival");
    R95Grant(awakening, recipes, lateAt, true);
    const auto entered = SynchroniseWorldCountdown(late.World, awakening, late.State, context, cats);
    const auto view = QueryWorldCountdown(entered.World, awakening, entered.State);
    R95Expect(entered.Ok() && entered.State == late.State && view.PreparationAvailable &&
        view.RemainingMinutes == awakening.ExaminationAt - lateAt && view.RemainingMinutes < 2 * MinutesPerDay,
        "late grant keeps existing offers, news and reduced remaining time");
    auto lateContext = context;
    lateContext.Contacts.push_back({"teacher", "npc_teacher"});
    const auto practice = PreviewWorldCountdownPractice(entered.World, awakening, entered.State, lateContext, cats,
        PowerDimension::Precision, 30);
    const auto practised = RunWorldCountdownPractice(entered.World, awakening, entered.State,
        awakening.Granted->StartingProfile, lateContext, cats, practice);
    R95Expect(practised.Ok() && IsWorldDayInStep(practised.Transition.World) &&
        practised.Transition.State->End == awakening.ExaminationAt &&
        practised.Profile.SupervisedPracticeMinutes == awakening.Granted->StartingProfile.SupervisedPracticeMinutes + 30,
        "late make-up permits optional practice with the same capability rules and original deadline");
    const auto finished = AdvanceWorldCountdown(entered.World, awakening, entered.State, context, cats, view.RemainingMinutes);
    R95Expect(finished.Ok() && finished.State->Closed && finished.State->End == awakening.ExaminationAt &&
        finished.World.Clock.Now() == awakening.ExaminationAt && IsWorldDayInStep(finished.World), "late entry still closes at 2 February 06:00");
    const auto ordinary = AdvanceWorldCountdown(finished.World, awakening, finished.State, context, cats, 60);
    const auto after = QueryWorldCountdown(ordinary.World, awakening, ordinary.State);
    R95Expect(ordinary.Ok() && after.ExaminationDue && after.RemainingMinutes == 0 && after.OrdinaryDayAvailable &&
        !after.PreparationAvailable && ordinary.State == finished.State, "ordinary life continues without forcing examination participation");

    const auto catchUpAt = GameClock::FromCalendar(2043, 1, 25, 10, 0);
    const auto pending = R95Inputs(content);
    const auto catchUp = SynchroniseWorldCountdown(R95World(catchUpAt), pending, {}, context, cats);
    R95Expect(catchUp.Ok() && catchUp.State->Seed == scheduled.State->Seed && catchUp.State->Start == scheduled.State->Start &&
        catchUp.State->End == scheduled.State->End, "pre-news bootstrap uses the original span and seed");
    const auto offers = CountdownOffers(catchUp.World, *catchUp.State, context);
    R95Expect(offers.front().Availability == CountdownAvailability::WindowClosed, "past invitations do not move to the late entry day");
    R95Expect(ParseCountdown(SerialiseCountdown(*catchUp.State)) == catchUp.State,
        "late bootstrap retains a canonical persistent countdown");
    const auto missed = SynchroniseWorldCountdown(R95World(lateAt), awakening, {}, context, cats);
    R95Expect(!missed.Ok() && !missed.State, "missing historical countdown refuses rather than fabricating past news");
}

void R95Finish(WorldCountdownResult& session, const DayLoopCatalogues& cats, int batch)
{
    for (int calls = 0; !session.State->Closed && calls < 12; ++calls)
    {
        const auto day = GameClock::DayNumber(session.World.Clock.Now());
        const auto last = GameClock::DayNumber(session.State->End) - 1;
        std::vector<WeekDayPlan> days(static_cast<std::size_t>(std::min<std::int64_t>(batch, last - day + 1)));
        for (std::size_t d = 0; d < days.size(); ++d)
        {
            const auto midnight = GameClock::StartOfDay(session.World.Clock.Now()) + static_cast<GameInstant>(d) * MinutesPerDay;
            for (int minute : {420, 750, 1140, 1320})
            {
                if (midnight + minute < session.World.Clock.Now())
                    continue;
                WeekEntry entry;
                entry.StartMinute = minute;
                if (minute == 420 || minute == 750)
                {
                    entry.Kind = WeekEntryKind::Activity;
                    entry.ActivityId = minute == 420 ? "activity.study_at_home" : "activity.eat_meal";
                    entry.PlaceKind = minute == 420 ? "home" : "dining";
                }
                else
                {
                    entry.Kind = minute == 1320 ? WeekEntryKind::Sleep : WeekEntryKind::Spend;
                    entry.Minutes = minute == 1320 ? 480 : 60;
                    entry.Pass = DayPassKind::Rest;
                }
                days[d].Entries.push_back(entry);
            }
        }
        const auto resolved = ResolveCountdownDays(session.World, *session.State, days, cats);
        if (!resolved.Ok())
            std::cerr << resolved.Issue.Detail << '\n';
        R95Expect(resolved.Ok() && IsWorldDayInStep(resolved.World), "ordinary study, meals, rest, promises and sleep keep clocks equal");
        session.World = resolved.World;
        session.State = resolved.State;
    }
    R95Expect(session.State->Closed, "a complete no-practice calendar reaches examination morning");
    R95Expect(session.World.DayLoop.Attendance.AbsentDays == 0, "preparation break never invents school absences");
    for (const auto& receipt : session.World.DayLoop.Receipts.Entries)
        R95Expect(receipt.ActivityId != WorldCountdownPracticeId, "ordinary commitments need no supernatural practice");
}

void R95StableOffers(const ContentDb& content, const DayLoopCatalogues& cats)
{
    const auto awakening = R95Inputs(content);
    auto context = R95Context(true);
    context.Contacts.push_back({"friend", "npc_friend"});
    context.Contacts.push_back({"sibling", "npc_sibling"});
    RelationshipBook book;
    Relationship pair;
    pair.Low = "npc_friend";
    pair.High = "player";
    pair.Trust = 28000;
    R95Expect(book.RestorePair(pair), "known friend relationship loads");
    context.Relationships = &book;
    auto session = R95Open(awakening, context, cats);
    const auto taken = TakeOpportunity(session.World, *session.State, context, "opp.study_circle");
    R95Expect(taken.Ok(), "one disclosed invitation is committed");
    session.State = taken.State;
    const auto stored = session.State->Commitments.front();
    const auto negotiated = NegotiateObligation(session.World, *session.State, context, "oblig.sibling_pickup", CountdownProposal::Share);
    R95Expect(negotiated.Ok(), "one obligation negotiation is committed");
    session.State = negotiated.State;
    const auto loaded = ParseCountdown(SerialiseCountdown(*session.State));
    R95Expect(loaded && *loaded == *session.State, "committed uncertainty round-trips exactly");
    session.State = loaded;
    RelationshipBook changed;
    context.Relationships = &changed;
    context.Contacts[1].Person = "npc_other_friend";
    const auto offers = CountdownOffers(session.World, *session.State, context);
    const auto& shown = offers.front();
    R95Expect(shown.Def->Id == stored.Id && shown.Odds == stored.Odds && shown.Contact == stored.Contact &&
        shown.Availability == CountdownAvailability::Taken, "committed offer shows its stored odds and person after relationship changes");
    const auto duplicate = TakeOpportunity(session.World, *session.State, context, stored.Id);
    const auto retry = NegotiateObligation(session.World, *session.State, context, "oblig.sibling_pickup", CountdownProposal::Defer);
    R95Expect(duplicate.Issue.Error == CountdownError::Taken && duplicate.State == *session.State &&
        retry.Issue.Error == CountdownError::Negotiated && retry.State == *session.State, "reload and changed context cannot reroll either decision");
    const auto* storedProposal = CountdownFindRecord(*session.State, "oblig.sibling_pickup");
    const auto proposals = CountdownProposals(session.World, *session.State, context, "oblig.sibling_pickup");
    R95Expect(proposals[1].AcceptPermille == storedProposal->AcceptPermille &&
        proposals[1].Helper == storedProposal->Helper && !proposals[1].Available,
        "the used negotiation continues to show the persisted chance and helper");
    auto daily = session;
    R95Finish(session, cats, 7);
    R95Finish(daily, cats, 1);
    R95Expect(session.State == daily.State && session.World.Clock.Now() == daily.World.Clock.Now(),
        "daily and batched ordinary life yield identical offers, rolls and deadline");
    const auto& outcome = session.State->Commitments.front();
    R95Expect(outcome.Resolved && outcome.Seed == stored.Seed && outcome.Odds == stored.Odds,
        "resolution uses the original stored uncertainty");
    const auto synced = SynchroniseWorldCountdown(session.World, awakening, session.State, context, cats);
    R95Expect(synced.Ok() && synced.State == session.State, "closed countdown synchronisation never reapplies outcomes");
}

void R95Practice(const ContentDb& content, const AbilityRecipeSet& recipes, const DayLoopCatalogues& cats)
{
    auto awakening = R95Inputs(content);
    R95Grant(awakening, recipes, awakening.AppointmentAt + 60);
    auto context = R95Context(true);
    auto session = R95Open(awakening, context, cats);
    session.World.DayLoop.Plan = {{1080, "activity.study_at_home"}};
    session.World.DayLoop.Planned = true;
    session.World.DayLoop.Phase = DayLoopPhaseOf(session.World.DayLoop);
    const auto snapshot = HashWorldState(session.World);
    const auto profile = awakening.Granted->StartingProfile;
    const auto at = session.World.Clock.Now();
    std::vector<ReceiptCandidate> alternatives = {
        {"promise.friend", "source.friend", at, at + 10, 60},
        {"errand.home", "source.household", at, at, 60},
        {"rest.evening", "source.day_plan", at, at + 60, 30},
        {"unknown.invitation", "source.friend", at + 1, at, 30}};
    const auto offer = PreviewWorldCountdownPractice(session.World, awakening, session.State, context, cats,
        PowerDimension::Coordination, 60, alternatives);
    R95Expect(offer.Available() && offer.Displaced.size() == 3, "practice discloses study, errand and promise, excluding unknown and back-to-back alternatives");
    R95Expect(HashWorldState(session.World) == snapshot,
        "viewing and declining practice cost no time or state");
    auto hiddenPromise = offer;
    hiddenPromise.Displaced.pop_back();
    const auto refused = RunWorldCountdownPractice(session.World, awakening, session.State, profile, context, cats, hiddenPromise, alternatives);
    R95Expect(!refused.Ok() && HashWorldState(refused.Transition.World) == snapshot && refused.Transition.State == session.State,
        "confirmation with an altered disclosure is atomic refusal");
    const auto practised = RunWorldCountdownPractice(session.World, awakening, session.State, profile, context, cats, offer, alternatives);
    R95Expect(practised.Ok() && practised.Transition.World.Clock.Now() == at + 60 && IsWorldDayInStep(practised.Transition.World),
        "confirmed practice charges exactly its duration to both clocks");
    R95Expect(practised.Profile.SupervisedPracticeMinutes == profile.SupervisedPracticeMinutes + 60 &&
        practised.Profile.Grade == AwakenedGrade::F && awakening.Granted->StartingProfile.Rating == profile.Rating,
        "practice uses the live profile without rewriting the grant or promoting grade");
    const auto& receipts = practised.Transition.World.DayLoop.Receipts.Entries;
    R95Expect(!receipts.empty() && receipts.back().ActivityId == WorldCountdownPracticeId && receipts.back().Displaced == offer.Displaced,
        "the accepted practice disclosure appears on the ordinary receipt");
    R95Expect(practised.Transition.State == session.State && practised.Transition.World.Rng == session.World.Rng,
        "practice leaves countdown offers and RNG unchanged");
    R95Expect(IsDayLoopStateValid(practised.Transition.World.DayLoop), "practice receipts preserve day-loop invariants");
    const auto replay = RunWorldCountdownPractice(practised.Transition.World, awakening, practised.Transition.State,
        practised.Profile, context, cats, offer, alternatives);
    R95Expect(!replay.Ok() && HashWorldState(replay.Transition.World) == HashWorldState(practised.Transition.World), "stale confirmation cannot repeat practice");

    const auto obligationAt = CountdownInstantOf(6, 900);
    const auto reached = AdvanceWorldCountdown(session.World, awakening, session.State, context, cats, obligationAt - at);
    R95Expect(reached.Ok(), "ordinary time can reach the start of an existing promise");
    const auto blocked = PreviewWorldCountdownPractice(reached.World, awakening, reached.State, context, cats, PowerDimension::Output, 60);
    R95Expect(!blocked.Available() && !blocked.BlockingCommitments.empty() && !blocked.Displaced.empty(),
        "owed promise is disclosed and cannot silently be replaced by practice");
    const auto skipped = AdvanceWorldCountdown(reached.World, awakening, reached.State, context, cats, 60);
    R95Expect(!skipped.Ok() && skipped.State == reached.State && HashWorldState(skipped.World) == HashWorldState(reached.World),
        "ordinary clock adapter cannot reward an unplayed promise");

    const auto negotiated = NegotiateObligation(session.World, *session.State, context, "oblig.teacher_hall_setup", CountdownProposal::Decline);
    R95Expect(negotiated.Ok(), "fixture explicitly declines the outstanding promise");
    WorldSim finalSim(session.World, MaxEventsPerAdvance, cats);
    R95Expect(finalSim.Submit({CommandSource::System, AdvanceTime{CountdownEndInstant() - 30 - at}}).Accepted, "fixture follows the scheduled news to the final half hour");
    const auto final = SynchroniseWorldCountdown(finalSim.State(), awakening, negotiated.State, context, cats);
    R95Expect(final.Ok(), "final half-hour state synchronises");
    R95Expect(PreviewWorldCountdownPractice(final.World, awakening, final.State, context, cats, PowerDimension::Output, 31).Issue.Error ==
        CountdownError::BeyondExam, "practice cannot extend the deadline by one minute");
}
} // namespace

int main(int argc, char** argv)
{
    R95Expect(argc == 2, "pass Data/Content directory");
    const auto content = LoadContent(R95ReadContent(argv[1]));
    R95Expect(content.Ok(), "real content validates");
    const auto activities = LoadActivityCatalogue(content.Db);
    const auto bells = LoadBellSchedules(content.Db);
    const auto calendars = LoadSchoolCalendars(content.Db);
    const auto recipes = LoadAbilityRecipes(content.Db);
    R95Expect(activities.Ok() && bells.Ok() && calendars.Ok() && recipes.Ok(), "real catalogues load");
    const auto* school = bells.Set.Find("institution.nabapara_vidyalaya", "science");
    const auto* calendar = calendars.Set.Find("institution.nabapara_vidyalaya", "stage.higher_secondary");
    R95Expect(school != nullptr && calendar != nullptr, "Class XI schedule and preparation-break calendar exist");
    const DayLoopCatalogues cats{&activities.Catalogue, school, calendar};
    R95Boundary(content.Db, recipes.Set, cats);
    R95LateEntry(content.Db, recipes.Set, cats);
    R95StableOffers(content.Db, cats);
    R95Practice(content.Db, recipes.Set, cats);
    std::cout << "World countdown: " << R95Assertions << " assertions passed\n";
}
