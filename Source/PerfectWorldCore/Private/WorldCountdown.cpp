#include "WorldCountdown.h"

#include "CountdownInternal.h"
#include "GateSites.h"
#include "IdentityRegistry.h"
#include "TimeContract.h"
#include "WorldReducer.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
CountdownIssue R95Inputs(const WorldState& world, const AwakeningState& awakening)
{
    if (!IsWorldDayInStep(world))
        return CountdownMakeIssue(CountdownError::InvalidWorld, "world and day clocks disagree");
    if (!awakening.Seeded || awakening.WorldSeed != world.WorldSeed ||
        awakening.AppointmentAt != GameClock::FromCalendar(2043, 1, 23, 9, 0) ||
        awakening.ExaminationAt != CountdownEndInstant())
        return CountdownMakeIssue(CountdownError::InvalidWorld, "unseeded inputs, seed mismatch or changed examination contract");
    if (awakening.Granted && (awakening.Granted->At < awakening.AppointmentAt ||
                             awakening.Granted->At >= awakening.ExaminationAt ||
                             awakening.Granted->At > world.Clock.Now()))
        return CountdownMakeIssue(CountdownError::InvalidWorld, "grant is outside its supervised session window");
    return {};
}

CountdownIssue R95State(const WorldState& world, const CountdownState& state)
{
    const auto& event = CountdownWorldEvent();
    if (state.Start != CountdownStartInstant() || state.End != CountdownEndInstant() ||
        state.EventOnset != CountdownInstantOf(event.Day, event.Minute) ||
        state.Seed != CountdownOpenSeed(world.WorldSeed, CountdownStartInstant()) ||
        !ParseCountdown(SerialiseCountdown(state)))
        return CountdownMakeIssue(CountdownError::InvalidWorld, "countdown span, seed or persisted state is invalid");
    if (world.Clock.Now() < state.Start || (state.Closed && world.Clock.Now() < state.End))
        return CountdownMakeIssue(CountdownError::InvalidWorld, "countdown and clock disagree");
    if (state.Ledger.empty() || state.Ledger.back().At > world.Clock.Now())
        return CountdownMakeIssue(CountdownError::InvalidWorld, "countdown contains future history");
    const auto gate = world.Gates.find(event.GateId);
    if (gate == world.Gates.end() || gate->second.Rank != event.Rank ||
        gate->second.OnsetCount < 0 || gate->second.OnsetCount > 1 ||
        (state.EventFired && gate->second.OnsetCount != 1) ||
        (gate->second.OnsetCount == 1 && world.Clock.Now() < state.EventOnset))
        return CountdownMakeIssue(CountdownError::InvalidWorld, "countdown Gate history disagrees with the world");
    const auto onsetId = GateOnsetEventId(event.GateId);
    std::size_t pending = 0;
    for (const auto& scheduled : world.Clock.Pending())
    {
        if (scheduled.Id != onsetId)
            continue;
        if (scheduled.When != state.EventOnset)
            return CountdownMakeIssue(CountdownError::InvalidWorld, "countdown onset was moved");
        ++pending;
    }
    if ((gate->second.OnsetCount == 0 && (pending != 1 || world.Clock.Now() > state.EventOnset)) ||
        (gate->second.OnsetCount == 1 && pending != 0))
        return CountdownMakeIssue(CountdownError::InvalidWorld, "countdown onset is missing or repeated");
    return {};
}

WorldCountdownResult R95Unchanged(const WorldState& world, const std::optional<CountdownState>& state)
{
    WorldCountdownResult result;
    result.World = world;
    result.State = state;
    return result;
}

ReceiptCommitment R95PracticeCommitment(GameInstant at, std::int32_t minutes)
{
    ReceiptCommitment commitment;
    commitment.ActivityId = WorldCountdownPracticeId;
    commitment.DecidedAt = at;
    commitment.Start = at;
    commitment.DurationMinutes = minutes;
    return commitment;
}

std::vector<ReceiptCandidate> R95Candidates(const WorldState& world, const CountdownState& state,
    const ActivityCatalogue& activities, std::span<const ReceiptCandidate> known)
{
    std::vector<ReceiptCandidate> result(known.begin(), known.end());
    const GameInstant midnight = GameClock::StartOfDay(world.Clock.Now());
    for (const DayPlanItem& item : world.DayLoop.Plan)
    {
        const ActivityDef* def = activities.Find(item.ActivityId);
        if (def == nullptr)
            continue;
        ReceiptCandidate candidate;
        candidate.Id = item.ActivityId;
        candidate.Source = "source.day_plan";
        candidate.KnownAt = world.Clock.Now();
        candidate.Start = midnight + item.StartMinute;
        candidate.DurationMinutes = def->DurationMinutes;
        candidate.Forecast = ForecastDayActivity(world.DayLoop, activities, item.ActivityId, candidate.Start).Receipt;
        result.push_back(std::move(candidate));
    }
    for (const CountdownReservedBlock& block : CountdownReservedBlocks(state))
    {
        ReceiptCandidate candidate;
        candidate.Id = block.Id;
        candidate.Source = "source.countdown";
        candidate.KnownAt = world.Clock.Now();
        candidate.Start = block.Start;
        candidate.DurationMinutes = static_cast<std::int32_t>(block.End - block.Start);
        result.push_back(std::move(candidate));
    }
    return result;
}

bool R95SameDisclosure(const WorldCountdownPracticeOffer& a, const WorldCountdownPracticeOffer& b)
{
    return a.WorldSeq == b.WorldSeq && a.CountdownHash == b.CountdownHash &&
        a.Start == b.Start && a.End == b.End && a.Dimension == b.Dimension && a.Minutes == b.Minutes &&
        a.Supervisor == b.Supervisor && a.Displaced == b.Displaced && a.BlockingCommitments == b.BlockingCommitments;
}
} // namespace

WorldCountdownResult SynchroniseWorldCountdown(const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const CountdownContext& context, const DayLoopCatalogues& catalogues)
{
    auto result = R95Unchanged(world, state);
    result.Issue = R95Inputs(world, awakening);
    if (!result.Ok())
        return result;
    if (!state)
    {
        if (world.Clock.Now() < CountdownStartInstant())
            return result;
        const auto opened = OpenCountdownAtCurrentTime(world, context, catalogues, &result.Events);
        result.Issue = opened.Issue;
        if (!result.Ok())
            return result;
        result.World = opened.World;
        result.State = opened.State;
    }
    else
    {
        result.Issue = R95State(world, *state);
        if (!result.Ok())
            return result;
        if (state->Player != context.Player)
        {
            result.Issue = CountdownMakeIssue(CountdownError::BadContext, "countdown belongs to another player");
            return result;
        }
    }
    const auto observed = ObserveCountdownTime(result.World, *result.State);
    if (!observed.Ok())
    {
        result = R95Unchanged(world, state);
        result.Issue = observed.Issue;
        return result;
    }
    result.State = observed.State;
    return result;
}

WorldCountdownResult AdvanceWorldCountdown(const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const CountdownContext& context,
    const DayLoopCatalogues& catalogues, std::int64_t minutes)
{
    auto refused = R95Unchanged(world, state);
    if (minutes <= 0 || minutes > MaxSingleAdvanceMinutes || world.Clock.Now() > DayLoopMaxInstant - minutes)
    {
        refused.Issue = CountdownMakeIssue(CountdownError::BadPlan, "invalid ordinary-time duration");
        return refused;
    }
    auto result = SynchroniseWorldCountdown(world, awakening, state, context, catalogues);
    if (!result.Ok())
        return result;
    const GameInstant target = world.Clock.Now() + minutes;
    while (result.World.Clock.Now() < target)
    {
        GameInstant boundary = target;
        if (result.World.Clock.Now() < CountdownStartInstant())
            boundary = std::min(target, CountdownStartInstant());
        WorldSim sim(result.World, MaxEventsPerAdvance, catalogues);
        const auto advanced = sim.Submit({CommandSource::System, AdvanceTime{boundary - result.World.Clock.Now()}});
        if (!advanced.Accepted || advanced.Partial)
        {
            refused.Issue = CountdownMakeIssue(CountdownError::WorldBusy,
                advanced.Partial ? "event cap stopped ordinary time; transaction not published" : advanced.Why.Code);
            return refused;
        }
        auto next = SynchroniseWorldCountdown(sim.State(), awakening, result.State, context, catalogues);
        if (!next.Ok())
        {
            refused.Issue = next.Issue;
            return refused;
        }
        result.Events.insert(result.Events.end(), advanced.Events.begin(), advanced.Events.end());
        result.World = std::move(next.World);
        result.State = std::move(next.State);
    }
    return result;
}

WorldCountdownView QueryWorldCountdown(const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state)
{
    WorldCountdownView view;
    view.OpeningAt = CountdownStartInstant();
    view.ExaminationAt = CountdownEndInstant();
    view.Issue = R95Inputs(world, awakening);
    if (view.Issue.Ok() && state)
        view.Issue = R95State(world, *state);
    if (!view.Issue.Ok())
        return view;
    const auto now = world.Clock.Now();
    view.RemainingMinutes = std::max<GameInstant>(0, view.ExaminationAt - now);
    view.MinutesUntilOpening = std::max<GameInstant>(0, view.OpeningAt - now);
    view.Awakened = awakening.Granted.has_value();
    view.ExaminationDue = now >= view.ExaminationAt;
    view.PreparationAvailable = view.Awakened && state && !state->Closed && !view.ExaminationDue;
    view.OrdinaryDayAvailable = true;
    return view;
}

WorldCountdownPracticeOffer PreviewWorldCountdownPractice(const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const CountdownContext& context, const DayLoopCatalogues& catalogues,
    PowerDimension dimension, std::int32_t minutes, std::span<const ReceiptCandidate> knownAlternatives)
{
    WorldCountdownPracticeOffer offer;
    offer.WorldSeq = world.Seq;
    offer.Start = world.Clock.Now();
    offer.Dimension = dimension;
    offer.Minutes = minutes;
    const auto view = QueryWorldCountdown(world, awakening, state);
    offer.Issue = view.Issue;
    if (!offer.Issue.Ok())
        return offer;
    if (!view.PreparationAvailable)
        offer.Issue = CountdownMakeIssue(CountdownError::Closed, "supervised Awakening and the countdown opening are required");
    else if (const auto busy = CountdownCheckWorld(world); !busy.Ok())
        offer.Issue = busy;
    else if (catalogues.Activities == nullptr)
        offer.Issue = CountdownMakeIssue(CountdownError::MissingCatalogue);
    else if (static_cast<std::uint8_t>(dimension) >= PowerDimensionCount || minutes < 1 || minutes > TimeSceneMaxCostMinutes)
        offer.Issue = CountdownMakeIssue(CountdownError::BadPlan, "invalid practice declaration");
    if (!offer.Issue.Ok())
        return offer;
    offer.End = offer.Start + minutes;
    if (offer.End > view.ExaminationAt || GameClock::DayNumber(offer.End - 1) != GameClock::DayNumber(offer.Start))
    {
        offer.Issue = CountdownMakeIssue(CountdownError::BeyondExam, "practice must fit this calendar day before the fixed examination");
        return offer;
    }
    offer.Supervisor = CountdownContactFor(context, "teacher");
    if (context.Player != state->Player || !IsValidIdentityId(offer.Supervisor) || offer.Supervisor == context.Player)
    {
        offer.Issue = CountdownMakeIssue(CountdownError::NoContact, "no authorised supervising teacher");
        return offer;
    }
    offer.CountdownHash = HashCountdown(*state);
    const auto candidates = R95Candidates(world, *state, *catalogues.Activities, knownAlternatives);
    if (candidates.size() > ReceiptMaxCandidates ||
        std::any_of(candidates.begin(), candidates.end(), [](const ReceiptCandidate& c) { return !IsReceiptCandidateValid(c); }))
    {
        offer.Issue = CountdownMakeIssue(CountdownError::BadPlan, "invalid or excessive known alternatives");
        return offer;
    }
    offer.Displaced = DecideDisplaced(R95PracticeCommitment(offer.Start, minutes), candidates);
    for (const auto& block : CountdownReservedBlocks(*state))
    {
        if (CountdownOverlaps(offer.Start, offer.End, block.Start, block.End))
            offer.BlockingCommitments.push_back(block.Id);
    }
    return offer;
}

WorldCountdownPracticeResult RunWorldCountdownPractice(const WorldState& world, const AwakeningState& awakening,
    const std::optional<CountdownState>& state, const AwakenedProfile& liveProfile, const CountdownContext& context,
    const DayLoopCatalogues& catalogues, const WorldCountdownPracticeOffer& accepted,
    std::span<const ReceiptCandidate> knownAlternatives)
{
    WorldCountdownPracticeResult result;
    result.Transition = R95Unchanged(world, state);
    result.Profile = liveProfile;
    const auto offer = PreviewWorldCountdownPractice(world, awakening, state, context, catalogues,
        accepted.Dimension, accepted.Minutes, knownAlternatives);
    if (!offer.Available())
    {
        result.Transition.Issue = offer.Issue.Ok()
            ? CountdownMakeIssue(CountdownError::TimeConflict, offer.BlockingCommitments.front()) : offer.Issue;
        return result;
    }
    if (!R95SameDisclosure(offer, accepted))
    {
        result.Transition.Issue = CountdownMakeIssue(CountdownError::BadPlan, "practice disclosure changed; present it again");
        return result;
    }
    auto advanced = AdvanceWorldCountdown(world, awakening, state, context, catalogues, offer.Minutes);
    if (!advanced.Ok())
    {
        result.Transition.Issue = advanced.Issue;
        return result;
    }
    const auto candidates = R95Candidates(world, *state, *catalogues.Activities, knownAlternatives);
    const auto receipt = RecordCommitment(advanced.World.DayLoop.Receipts,
        R95PracticeCommitment(offer.Start, offer.Minutes), candidates);
    if (!receipt.Ok())
    {
        result.Transition.Issue = CountdownMakeIssue(CountdownError::BadPlan, std::string(ReceiptErrorCode(receipt.Error)));
        return result;
    }
    result.Practice = Practise(result.Profile, offer.Dimension, offer.Minutes,
        GameClock::DayNumber(offer.Start), 100, true);
    result.Transition = std::move(advanced);
    return result;
}
} // namespace PerfectWorld
