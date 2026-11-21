#include "WorldExam.h"

#include "ExamContract.h"
#include "LegacyRecovery.h"
#include "WorldExamSittings.h"
#include "WorldReducer.h"

#include <algorithm>
#include <utility>
#include <vector>

// P4-02: the tower examination through WorldSim. Every step runs on a staged copy through P4-01's P401ApplyExamChoice, so
// TowerSession stays the only owner of guards, evidence and mission arithmetic. The world adds the opening instant, the
// Awakening and preparation authority, the exam.active guard, the examination-instant time boundary and the single terminal
// outside-world charge through ChargeTime; Submit's snapshot restores any late refusal, so nothing is ever half published.
// P4-09 (WorldExamSittings.h): annual sittings with the age-twenty limit, the late-entry window with its reduced allowance
// ("time called"), the deferred candidate's next-sitting stop, and one ExamTransition event per accepted transition.
namespace PerfectWorld
{
namespace
{
Rejection P402Refusal(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}

Rejection P402FromIssue(const SchoolIssue& issue)
{
    return Rejection{issue.Code, issue.Detail.empty() ? std::string("The examination refused that action.") : issue.Detail};
}

const TowerRunnerCatalogue& P402Runners(const P402WorldExamCatalogues& catalogues)
{
    static const TowerRunnerCatalogue empty;
    return catalogues.Runners != nullptr ? *catalogues.Runners : empty;
}

bool P402Flag(const WorldState& world, std::string_view id)
{
    const auto found = world.Flags.find(id);
    return found != world.Flags.end() && found->second;
}

TowerSessionCommand P402Command(TowerSessionAction kind)
{
    TowerSessionCommand command;
    command.Kind = kind;
    return command;
}
} // namespace

bool P402ExamClosed(TowerParticipation participation) noexcept
{
    return participation != TowerParticipation::Unset && participation != TowerParticipation::Entered;
}

P402ExamPreparation P402FinalPreparation(const WorldState& world)
{
    P402ExamPreparation out;
    if (!world.Awakening.Granted)
    {
        out.Issue = {"exam.not_awakened", "The compulsory Awakening was not granted to this life."};
        return out;
    }
    if (!world.CountdownText.empty())
    {
        const std::optional<CountdownState> countdown = ParseCountdown(world.CountdownText);
        if (countdown && countdown->Closed)
            out.Profile = CountdownProfileOf(world, *countdown);
        if (!countdown || !out.Profile.Final)
            out.Issue = {"exam.no_preparation", "The ten days have not closed into a final preparation."};
        return out;
    }
    // P3-14: the ten days of this life could not be reconstructed. It sits with the explicit zero preparation (reserve 4,
    // discount 0), the zero-practice runner P4-01 proves completable; nothing is fabricated in its favour.
    if (!P402Flag(world, AwakeningCountdownUnavailableFlag))
    {
        out.Issue = {"exam.no_preparation", "The ten days have not opened for this life."};
        return out;
    }
    out.Profile.Final = true;
    out.ZeroByCompatibility = true;
    return out;
}

P402ExamFinish P402ExamFinishSeam(const TowerSessionState& terminal, const SchoolFollowupContent* P405Followups)
{
    P402ExamFinish out;
    out.Participation = terminal.Participation;
    if (!P402ExamClosed(terminal.Participation))
    {
        out.Issue = {"exam.not_terminal", {}};
        return out;
    }
    static const SchoolFollowupContent P405NoFollowups;
    auto P405Report = P403ResolveExamResult(terminal, P405Followups ? *P405Followups : P405NoFollowups);
    if (P405Followups || P405Report.Issue.Code != "P403.exam.missing_followup")
        return P405Report;
    // Terminal transactions and the codec have no content binding. Only absence of a follow-up is
    // tolerated here: P403 already accepted the evidence, exit shape and time. No appointment is invented.
    out.Score = QueryTowerSession(terminal, {}).Result;
    return out;
}

bool ExamStateMatchesRunners(const TowerSessionState& state, const TowerRunnerCatalogue& runners)
{
    if (state.Participation == TowerParticipation::Unset || state.Participation == TowerParticipation::Deferred ||
        state.Participation == TowerParticipation::Declined)
    {
        TowerSessionState empty;
        empty.Participation = state.Participation;
        return state == empty;
    }
    CountdownProfile frozen;
    frozen.Final = true;
    frozen.Preparation = state.Preparation.Preparation;
    frozen.Composure = state.Preparation.Composure;
    frozen.RouteDelayMinutes = state.Preparation.RouteDelayMinutes;
    TowerSessionState rebuilt;
    const auto apply = [&](const TowerSessionCommand& command) {
        return ApplyTowerSession(rebuilt, runners, command, frozen, CountdownEndInstant()).Ok();
    };
    const auto floor = [&](const std::string& route, TowerMethod method, const std::vector<std::string>& choices) {
        TowerSessionCommand open = P402Command(TowerSessionAction::OpenRoute);
        open.RouteId = route;
        open.Method = method;
        if (!apply(open))
            return false;
        for (std::size_t beat = 0; beat < choices.size(); ++beat)
        {
            TowerSessionCommand choose = P402Command(TowerSessionAction::Choose);
            choose.Runner = rebuilt.Runner;
            choose.Beat = beat;
            choose.ChoiceId = choices[beat];
            if (!apply(choose))
                return false;
        }
        return true;
    };
    if (!apply(P402Command(TowerSessionAction::Enter)))
        return false;
    for (const TowerSessionAttempt& attempt : state.Attempts)
    {
        if (!floor(attempt.RouteId, attempt.Method, attempt.ChoiceIds))
            return false;
    }
    if (state.Active && !floor(state.Active->RouteId, state.Active->Method, state.Active->Choices))
        return false;
    if (state.Participation == TowerParticipation::Withdrawn && !apply(P402Command(TowerSessionAction::Withdraw)))
        return false;
    return rebuilt == state;
}

bool IsExamCommand(const CommandPayload& payload) noexcept
{
    return std::holds_alternative<TowerSessionCommand>(payload);
}

std::optional<Rejection> ExamOpenRefusal(const WorldState& state)
{
    if (state.Examination.Participation == TowerParticipation::Entered)
        return P402Refusal("exam.active", "The examination is in progress; finish the floor or withdraw first.");
    return std::nullopt;
}

GameInstant WorldSim::ExamDecisionAt() const noexcept
{
    if (!CountdownRouted() || !State_.Awakening.Granted.has_value())
        return -1;
    const std::uint32_t current = P409CurrentSitting(State_);
    switch (State_.Examination.Participation)
    {
    case TowerParticipation::Unset: return P409SittingAt(current);
    case TowerParticipation::Deferred:
        // P4-09: a deferral is "ask me next year": the next sitting the age limit allows meets the candidate the same way.
        return P409SittingAllowsAge(current + 1, Examination_.P409Birth) ? P409SittingAt(current + 1) : -1;
    default: return -1;
    }
}

bool WorldSim::ExamDecisionPending() const noexcept
{
    return ExamDecisionAt() >= 0;
}

GameInstant WorldSim::ExamTimeBoundary(GameInstant target) const
{
    const GameInstant at = ExamDecisionAt();
    return at >= 0 && State_.Clock.Now() < at && target > at ? at : target;
}

bool WorldSim::StopForExamBoundary(GameInstant target, CommandResult& result)
{
    const GameInstant at = ExamDecisionAt();
    if (at < 0 || State_.Clock.Now() != at || target <= State_.Clock.Now())
        return false;
    // A path that arrived here in this command publishes its stop; one that starts at the instant cannot pass it.
    if (result.From >= State_.Clock.Now())
        Boundary_ = P402Refusal("exam.decision_required", "Sit, defer or decline the examination before time moves on.");
    result.Partial = true;
    return true;
}

std::optional<Rejection> WorldSim::ValidateExamCommand(const Command& command) const
{
    const auto& exam = std::get<TowerSessionCommand>(command.Payload);
    if (!IsWorldDayInStep(State_))
        return P402Refusal("exam.invalid_state", "The world and day must be valid and share one clock.");
    const GameInstant now = State_.Clock.Now();
    // P4-09: a decision may address a later sitting (a retake, or a morning missed); it then starts from a fresh session.
    const std::uint32_t target = P409DecisionTarget(State_, now);
    const bool later = target > P409CurrentSitting(State_);
    const bool undecided = later || State_.Examination.Participation == TowerParticipation::Unset;
    const GameInstant at = P409SittingAt(target);
    switch (exam.Kind)
    {
    case TowerSessionAction::Withdraw:
        return std::nullopt; // The guaranteed exit: no content, quiescence or binding is needed (contract: not_entered/closed).
    case TowerSessionAction::Defer:
    case TowerSessionAction::Decline:
        if (undecided && now < at)
            return P402Refusal("exam.not_open", "The examination opens at 06:00 on 2 February 2043.");
        if (later && !P409SittingAllowsAge(target, Examination_.P409Birth))
            return P402Refusal("exam.age_limit", "The examination is open to candidates up to the age of twenty.");
        return std::nullopt;
    case TowerSessionAction::Enter:
    {
        if (!undecided)
            return std::nullopt; // tower.session.already_entered / closed
        if (now < at)
            return P402Refusal("exam.not_open", "The examination opens at 06:00 on 2 February 2043.");
        if (!P409SittingAllowsAge(target, Examination_.P409Birth))
            return P402Refusal("exam.age_limit", "The examination is open to candidates up to the age of twenty.");
        if (now > at + P409LateEntryMinutes)
            return P402Refusal("exam.missed", "Entry to this sitting closed an hour after 06:00; the next sitting remains.");
        if (!IsWorldQuiescent(State_))
            return P402Refusal("world.unsettled", "Events are still due; settle them first.");
        if (!OpenConversationPersonId(State_).empty())
            return P402Refusal("time.conversation_open", "Finish the conversation first.");
        if (!ServiceShop_.empty())
            return P402Refusal("commerce.service_active", "Close the counter service first.");
        if (std::optional<Rejection> open = SchoolPlayOpenRefusal(State_))
            return open;
        if (Examination_.Runners == nullptr)
            return P402Refusal("exam.missing_content", "The examination floors are not bound to the world.");
        if (const P402ExamPreparation preparation = P409SittingPreparation(State_, target); !preparation.Ok())
            return P402FromIssue(preparation.Issue);
        // The whole (possibly reduced) charge must be payable at the exit: awake, no activity, inside the calendar.
        return ValidateTimeCharge(TowerWorldCapMinutes - (now - at));
    }
    case TowerSessionAction::OpenRoute:
    case TowerSessionAction::Choose:
        if (P402ExamClosed(State_.Examination.Participation))
            return std::nullopt; // tower.session.closed: a finished session refuses with the contract's code, content or not
        if (Examination_.Runners == nullptr)
            return P402Refusal("exam.missing_content", "The examination floors are not bound to the world.");
        if (State_.Examination.Participation == TowerParticipation::Entered &&
            !ExamStateMatchesRunners(State_.Examination, *Examination_.Runners))
            return P402Refusal("exam.invalid_state", "The stored examination does not match the bound floors.");
        return std::nullopt;
    }
    return P402Refusal("tower.session.bad_command", "Unknown examination action.");
}

std::optional<Rejection> WorldSim::ApplyExamCommand(const TowerSessionCommand& command, CommandResult& result)
{
    const GameInstant now = State_.Clock.Now();
    std::vector<TowerSessionState> earlier = State_.ExaminationEarlier;
    TowerSessionState session = State_.Examination;
    const bool decision = command.Kind == TowerSessionAction::Enter || command.Kind == TowerSessionAction::Defer ||
                          command.Kind == TowerSessionAction::Decline;
    if (decision)
        P409ArchiveUntil(earlier, session, P409DecisionTarget(State_, now));
    const auto sitting = static_cast<std::uint32_t>(earlier.size());
    const GameInstant at = P409SittingAt(sitting);
    CountdownProfile preparation;
    if (command.Kind == TowerSessionAction::Enter && session.Participation == TowerParticipation::Unset)
    {
        const P402ExamPreparation frozen = P409SittingPreparation(State_, sitting);
        if (!frozen.Ok())
            return P402FromIssue(frozen.Issue);
        preparation = frozen.Profile;
    }
    const TowerParticipation before = session.Participation;
    P401ExamStepResult step = P401ApplyExamChoice(session, P402Runners(Examination_), command, preparation, now);
    if (!step.Resolution.Ok())
        return P402FromIssue(step.Resolution.Issue);
    std::int64_t minutes = step.Resolution.OutsideMinutes;
    const bool sat = step.State.Participation != TowerParticipation::Deferred &&
                     step.State.Participation != TowerParticipation::Declined;
    const std::int64_t late = sat ? now - at : 0;
    const std::int64_t allowance = TowerWorldCapMinutes - late;
    bool timeCalled = false;
    if (step.State.Participation == TowerParticipation::Entered && QueryTowerSession(step.State, {}).Result.OutsideMinutes > allowance)
    {
        // P4-09: the late candidate's reduced allowance ran out; the hall's deadline does not move, so time is called.
        const TowerSessionResult called =
            ApplyTowerSession(step.State, P402Runners(Examination_), P402Command(TowerSessionAction::Withdraw), {}, now);
        if (!called.Ok())
            return P402FromIssue(called.Issue);
        minutes = called.OutsideMinutes;
        timeCalled = true;
    }
    const bool finished = !P402ExamClosed(before) && P402ExamClosed(step.State.Participation);
    ExamTransition transition{ExamTransitionKind::Sat, sitting, step.State.Participation, late, 0, timeCalled};
    if (command.Kind == TowerSessionAction::Enter)
        result.Events.push_back({0, now, ExamTransition{ExamTransitionKind::Sat, sitting, TowerParticipation::Entered, late, 0, false}});
    if (!finished)
    {
        // Contract: only the first terminal command carries a charge; a preview's nested result never moves the clock.
        if (minutes != 0)
            return P402Refusal("exam.clock_cap", "Only the examination's exit spends outside time.");
    }
    else
    {
        const P402ExamFinish seam = P402ExamFinishSeam(step.State);
        if (!seam.Ok())
            return P402FromIssue(seam.Issue);
        if (minutes < 0 || minutes > TowerWorldCapMinutes)
            return P402Refusal("exam.clock_cap", "The examination's outside time exceeds its cap.");
        minutes = std::min(minutes, allowance);
        transition.OutsideMinutes = minutes;
        switch (step.State.Participation)
        {
        case TowerParticipation::Withdrawn: transition.Kind = ExamTransitionKind::Withdrew; break;
        case TowerParticipation::Deferred: transition.Kind = ExamTransitionKind::Deferred; break;
        case TowerParticipation::Declined: transition.Kind = ExamTransitionKind::Declined; break;
        default: transition.Kind = ExamTransitionKind::Finished; break;
        }
        // Published at the instant the examination stood at; the exit's TimeAdvanced (ChargeTime) follows it.
        result.Events.push_back({0, now, transition});
        if (minutes > 0)
        {
            if (std::optional<Rejection> refusal = ValidateTimeCharge(minutes))
                return refusal;
            ChargeTime(minutes, result);
            if (result.Partial || State_.Clock.Now() != now + minutes)
                return P402Refusal("exam.charge_interrupted", "The examination's time could not settle; nothing was published.");
        }
    }
    State_.ExaminationEarlier = std::move(earlier);
    State_.Examination = std::move(step.State);
    return std::nullopt;
}

TowerSessionView WorldSim::ExaminationView() const
{
    return QueryTowerSession(State_.Examination, P402Runners(Examination_));
}

P402ExamWorldView WorldSim::ExaminationWorldView() const
{
    P402ExamWorldView view;
    const GameInstant now = State_.Clock.Now();
    const std::uint32_t current = P409CurrentSitting(State_);
    const std::uint32_t target = P409DecisionTarget(State_, now);
    const bool later = target > current;
    view.ExaminationAt = P409SittingAt(target);
    view.Session = ExaminationView();
    const TowerParticipation participation = State_.Examination.Participation;
    view.DecisionRequired = ExamDecisionPending() && now == ExamDecisionAt();
    view.Finished = P402ExamClosed(participation);
    view.CanWithdraw = participation == TowerParticipation::Entered;
    view.Sittings.Sitting = current;
    view.Sittings.SitTarget = target;
    view.Sittings.OpensAt = view.ExaminationAt;
    view.Sittings.EntryClosesAt = view.ExaminationAt + P409LateEntryMinutes;
    view.Sittings.LateMinutes = P409EntryLateness(State_);
    view.Sittings.AllowanceMinutes = P409OutsideAllowance(State_);
    for (std::uint32_t next = std::max(target, current + 1); next <= P409MaxSitting && !view.Sittings.HasNextSitting; ++next)
    {
        if (P409SittingAt(next) > now && P409SittingAllowsAge(next, Examination_.P409Birth))
        {
            view.Sittings.HasNextSitting = true;
            view.Sittings.NextSittingAt = P409SittingAt(next);
        }
    }
    if (participation != TowerParticipation::Unset)
        view.OutsideMinutes = view.Session.Result.OutsideMinutes;
    if (participation == TowerParticipation::Entered)
        view.OutsideMinutes = std::min(view.OutsideMinutes, view.Sittings.AllowanceMinutes);
    const auto refusal = [this](TowerSessionAction kind) {
        return Validate(Command{CommandSource::Player, P402Command(kind)});
    };
    if (participation == TowerParticipation::Unset || later)
    {
        view.CanDefer = !refusal(TowerSessionAction::Defer);
        view.CanDecline = !refusal(TowerSessionAction::Decline);
        if (const std::optional<Rejection> sit = refusal(TowerSessionAction::Enter))
            view.SitRefusal = sit->Code;
        else
        {
            // Dry run on a fresh copy: the contract's own guards (catalogue, preparation bounds) decide the rest.
            const P402ExamPreparation preparation = P409SittingPreparation(State_, target);
            const P401ExamStepResult dry = P401ApplyExamChoice(later ? TowerSessionState{} : State_.Examination,
                P402Runners(Examination_), P402Command(TowerSessionAction::Enter), preparation.Profile, now);
            view.SitRefusal = dry.Resolution.Issue.Code;
        }
        view.CanSit = view.SitRefusal.empty();
    }
    else
        view.SitRefusal = participation == TowerParticipation::Entered ? "tower.session.already_entered" : "tower.session.closed";
    return view;
}
} // namespace PerfectWorld
