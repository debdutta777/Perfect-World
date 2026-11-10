#include "WorldReducer.h"

#include <utility>

// R9-04: the R9-02 supervised tutorial session and optional countdown practice through WorldSim, and the one R9 growth path
// (ApplyActivityCredits on a staged copy of the Awakening owner). Every contract call runs on staged copies; a refusal
// returns the contract's own code and nothing is committed. Time is charged through ChargeTime, so the countdown boundary,
// scheduled events, the day loop and allowance settlement stay in step, and Submit's snapshot restores a late refusal.
namespace PerfectWorld
{
namespace
{
Rejection R94Refusal(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}

Rejection R94FromTutorial(const TutorialDungeonResult& result)
{
    return Rejection{result.Error, "The supervised session refused that action."};
}

Rejection R94FromCountdown(CountdownError error, std::string detail)
{
    return Rejection{std::string(CountdownErrorCode(error)), detail.empty() ? std::string("The countdown refused that action.") : detail};
}

bool R94IsTutorialCommand(const CommandPayload& payload) noexcept
{
    return std::holds_alternative<TutorialEnter>(payload) || std::holds_alternative<TutorialAct>(payload) ||
           std::holds_alternative<TutorialAssist>(payload) || std::holds_alternative<TutorialRecover>(payload) ||
           std::holds_alternative<TutorialWithdraw>(payload) || std::holds_alternative<TutorialAbsent>(payload) ||
           std::holds_alternative<TutorialMakeUp>(payload) || std::holds_alternative<TutorialFinish>(payload);
}
} // namespace

bool IsAwakeningCommand(const CommandPayload& payload) noexcept
{
    return R94IsTutorialCommand(payload) || std::holds_alternative<CountdownPracticeConfirm>(payload);
}

std::optional<Rejection> TutorialOpenRefusal(const WorldState& state)
{
    if (state.Tutorial.Phase == TutorialDungeonPhase::Active || state.Tutorial.Phase == TutorialDungeonPhase::ReadyToGrant)
        return R94Refusal("tutorial.active", "The supervised session is in progress; finish or withdraw first.");
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ValidateAwakeningCommand(const Command& command) const
{
    if (!IsWorldDayInStep(State_))
        return R94Refusal("tutorial.invalid_state", "The world and day must be valid and share one clock.");
    if (!State_.Awakening.Seeded)
        return R94Refusal("tutorial.invalid_state", "This life has no Awakening inputs yet.");
    const bool system = command.Source == CommandSource::System;
    if ((std::holds_alternative<TutorialAbsent>(command.Payload) || std::holds_alternative<TutorialFinish>(command.Payload)) && !system)
        return R94Refusal("tutorial.source", "Only the supervising system records absence or finishes a session.");
    if (std::holds_alternative<CountdownPracticeConfirm>(command.Payload))
    {
        if (!CountdownRouted())
            return R94FromCountdown(CountdownError::BadContext, "the countdown is not bound to this world");
        if (!IsWorldQuiescent(State_))
            return R94Refusal("world.unsettled", "Events are still due; settle them first.");
        if (TutorialOpenRefusal(State_))
            return TutorialOpenRefusal(State_);
        return SchoolPlayOpenRefusal(State_);
    }
    if (std::holds_alternative<TutorialEnter>(command.Payload))
    {
        if (!IsWorldQuiescent(State_))
            return R94Refusal("world.unsettled", "Events are still due; settle them first.");
        if (!OpenConversationPersonId(State_).empty())
            return R94Refusal("time.conversation_open", "Finish the conversation first.");
        if (!ServiceShop_.empty())
            return R94Refusal("commerce.service_active", "Close the counter service first.");
        if (std::optional<Rejection> open = SchoolPlayOpenRefusal(State_))
            return open;
        if (Awakening_.Recipes == nullptr)
            return R94Refusal("tutorial.missing_content", "The ability recipes are not bound to the world.");
        // The disclosed hour is charged at exit, so it must be chargeable now: awake, no activity, inside the calendar.
        return ValidateTimeCharge(TutorialSessionMinutes);
    }
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ApplyAwakeningCommand(const Command& command, CommandResult& result)
{
    if (const auto* practice = std::get_if<CountdownPracticeConfirm>(&command.Payload))
        return ApplyCountdownPractice(*practice, result);
    const GameInstant now = State_.Clock.Now();
    TutorialDungeonState tutorial = State_.Tutorial;
    AwakeningState awakening = State_.Awakening;
    TutorialDungeonResult applied;
    if (std::holds_alternative<TutorialEnter>(command.Payload))
        applied = BeginTutorialDungeon(tutorial, awakening, now);
    else if (const auto* act = std::get_if<TutorialAct>(&command.Payload))
        // No cast binding supplies involved identities yet; the contract records the action with no parties.
        applied = CompleteTutorialChoice(tutorial, awakening, act->StationId, act->ChoiceId, now);
    else if (std::holds_alternative<TutorialAssist>(command.Payload))
        applied = RequestTutorialAssistance(tutorial, awakening, now);
    else if (std::holds_alternative<TutorialRecover>(command.Payload))
        applied = RecoverTutorialDungeon(tutorial, awakening, now);
    else if (std::holds_alternative<TutorialWithdraw>(command.Payload))
        applied = WithdrawTutorialDungeon(tutorial, awakening, now);
    else if (std::holds_alternative<TutorialAbsent>(command.Payload))
        applied = MarkTutorialAbsence(tutorial, awakening, now);
    else if (const auto* makeUp = std::get_if<TutorialMakeUp>(&command.Payload))
        applied = ScheduleTutorialMakeUp(tutorial, awakening, makeUp->At, now);
    else if (!std::holds_alternative<TutorialFinish>(command.Payload))
        return R94Refusal("command.unknown", "Unknown command.");
    if (!applied.Ok())
        return R94FromTutorial(applied);
    State_.Tutorial = std::move(tutorial);
    State_.Awakening = std::move(awakening);
    // The act or withdrawal that completes the session exits it in this same command; Finish is the loaded continuation.
    if (State_.Tutorial.Phase == TutorialDungeonPhase::ReadyToGrant)
        return FinishTutorial(result);
    if (std::holds_alternative<TutorialFinish>(command.Payload))
        return R94Refusal("tutorial.not_ready", "Complete the supervised session.");
    return std::nullopt;
}

std::optional<Rejection> WorldSim::FinishTutorial(CommandResult& result)
{
    if (Awakening_.Recipes == nullptr)
        return R94Refusal("tutorial.missing_content", "The ability recipes are not bound to the world.");
    if (State_.Awakening.Granted)
        return R94Refusal("tutorial.already_granted", "Awakening is already committed.");
    if (State_.Tutorial.Phase != TutorialDungeonPhase::ReadyToGrant || !State_.Tutorial.StartedAt)
        return R94Refusal("tutorial.not_ready", "Complete the supervised session.");
    if (State_.Clock.Now() != *State_.Tutorial.StartedAt)
        return R94Refusal("tutorial.clock_mismatch", "The session clock is inconsistent.");
    if (std::optional<Rejection> refusal = ValidateTimeCharge(TutorialSessionMinutes))
        return refusal;
    ChargeTime(TutorialSessionMinutes, result);
    if (result.Partial || State_.Clock.Now() != *State_.Tutorial.StartedAt + TutorialSessionMinutes)
        return R94Refusal("tutorial.charge_interrupted", "The session time could not settle.");
    TutorialDungeonState tutorial = State_.Tutorial;
    AwakeningState awakening = State_.Awakening;
    const TutorialDungeonResult granted = GrantTutorialAwakening(tutorial, awakening, *Awakening_.Recipes, State_.Clock.Now());
    if (!granted.Ok())
        return R94FromTutorial(granted);
    State_.Tutorial = std::move(tutorial);
    State_.Awakening = std::move(awakening);
    // R9-05: the live practice owner starts as the grant's snapshot, exactly once; the snapshot itself never changes.
    if (!State_.LiveAwakenedProfile && State_.Awakening.Granted)
        State_.LiveAwakenedProfile = State_.Awakening.Granted->StartingProfile;
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ApplyCountdownPractice(const CountdownPracticeConfirm& command, CommandResult& result)
{
    const std::optional<CountdownState> countdown = Countdown();
    if (!countdown)
        return R94FromCountdown(CountdownError::Closed, "supervised Awakening and the countdown opening are required");
    if (!State_.LiveAwakenedProfile)
        return R94FromCountdown(CountdownError::Closed, "supervised Awakening is required before practice");
    const WorldCountdownPracticeOffer offer = PreviewCountdownPractice(command.Accepted.Dimension, command.Accepted.Minutes);
    if (!offer.Available())
        return offer.Issue.Ok() ? R94FromCountdown(CountdownError::TimeConflict, offer.BlockingCommitments.front())
                                : R94FromCountdown(offer.Issue.Error, offer.Issue.Detail);
    if (!WorldCountdownSameDisclosure(offer, command.Accepted))
        return R94FromCountdown(CountdownError::BadPlan, "practice disclosure changed; present it again");
    if (std::optional<Rejection> refusal = ValidateTimeCharge(offer.Minutes))
        return refusal;
    // The displaced intentions are the ones disclosed, decided before the minutes pass (RunWorldCountdownPractice's rule).
    const std::vector<ReceiptCandidate> candidates = WorldCountdownPracticeCandidates(State_, *countdown, *School_.Activities, {});
    ChargeTime(offer.Minutes, result);
    if (result.Partial || State_.Clock.Now() != offer.End)
        return R94FromCountdown(CountdownError::WorldBusy, "event cap stopped the practice time; transaction not published");
    ReceiptState receipts = State_.DayLoop.Receipts;
    const auto recorded = RecordCommitment(receipts, WorldCountdownPracticeCommitment(offer.Start, offer.Minutes), candidates);
    if (!recorded.Ok())
        return R94FromCountdown(CountdownError::BadPlan, std::string(ReceiptErrorCode(recorded.Error)));
    AwakenedProfile profile = *State_.LiveAwakenedProfile;
    StagedPractice_ = Practise(profile, offer.Dimension, offer.Minutes, GameClock::DayNumber(offer.Start), 100, true);
    State_.DayLoop.Receipts = std::move(receipts);
    State_.LiveAwakenedProfile = std::move(profile);
    return std::nullopt;
}

std::optional<Rejection> WorldSim::CreditActivities(std::span<const DayLoopEvent> events)
{
    // Unseeded worlds (every pre-v12 save, and a life before creation confirmation) have no growth owner: nothing grows.
    if (!State_.Awakening.Seeded)
        return std::nullopt;
    AwakeningState staged = State_.Awakening;
    const AwakeningCreditsResult credited = ApplyActivityCredits(staged, events);
    if (!credited.Ok())
        return R94Refusal("school.learning_refused", credited.Error);
    State_.Awakening = std::move(staged);
    return std::nullopt;
}
} // namespace PerfectWorld
