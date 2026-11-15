#include "WorldExam.h"

#include "ExamContract.h"
#include "LegacyRecovery.h"
#include "WorldReducer.h"

#include <utility>

// P4-02: the tower examination through WorldSim. Every step runs on a staged copy through P4-01's P401ApplyExamChoice, so
// TowerSession stays the only owner of guards, evidence and mission arithmetic. The world adds the opening instant, the
// Awakening and preparation authority, the exam.active guard, the examination-instant time boundary and the single terminal
// outside-world charge through ChargeTime; Submit's snapshot restores any late refusal, so nothing is ever half published.
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

P402ExamFinish P402ExamFinishSeam(const TowerSessionState& terminal)
{
    P402ExamFinish out;
    out.Participation = terminal.Participation;
    if (!P402ExamClosed(terminal.Participation))
    {
        out.Issue = {"exam.not_terminal", {}};
        return out;
    }
    static const TowerRunnerCatalogue noRunners;
    out.Score = QueryTowerSession(terminal, noRunners).Result;
    if (!out.Score.Ok())
        out.Issue = {std::string(TowerErrorCode(out.Score.Error)), {}};
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

bool WorldSim::ExamDecisionPending() const noexcept
{
    return CountdownRouted() && State_.Awakening.Granted.has_value() &&
           State_.Examination.Participation == TowerParticipation::Unset;
}

GameInstant WorldSim::ExamTimeBoundary(GameInstant target) const
{
    const GameInstant at = CountdownEndInstant();
    return ExamDecisionPending() && State_.Clock.Now() < at && target > at ? at : target;
}

bool WorldSim::StopForExamBoundary(GameInstant target, CommandResult& result)
{
    if (!ExamDecisionPending() || State_.Clock.Now() != CountdownEndInstant() || target <= State_.Clock.Now())
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
    const bool undecided = State_.Examination.Participation == TowerParticipation::Unset;
    switch (exam.Kind)
    {
    case TowerSessionAction::Withdraw:
        return std::nullopt; // The guaranteed exit: no content, quiescence or binding is needed (contract: not_entered/closed).
    case TowerSessionAction::Defer:
    case TowerSessionAction::Decline:
        if (undecided && now < CountdownEndInstant())
            return P402Refusal("exam.not_open", "The examination opens at 06:00 on 2 February 2043.");
        return std::nullopt;
    case TowerSessionAction::Enter:
    {
        if (!undecided)
            return std::nullopt; // tower.session.already_entered / closed
        if (now < CountdownEndInstant())
            return P402Refusal("exam.not_open", "The examination opens at 06:00 on 2 February 2043.");
        if (now > CountdownEndInstant())
            return P402Refusal("exam.missed", "The examination began at 06:00; this sitting has passed.");
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
        if (const P402ExamPreparation preparation = P402FinalPreparation(State_); !preparation.Ok())
            return P402FromIssue(preparation.Issue);
        // The whole capped charge must be payable at the exit: awake, no activity, inside the calendar.
        return ValidateTimeCharge(TowerWorldCapMinutes);
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
    CountdownProfile preparation;
    if (command.Kind == TowerSessionAction::Enter && State_.Examination.Participation == TowerParticipation::Unset)
    {
        const P402ExamPreparation frozen = P402FinalPreparation(State_);
        if (!frozen.Ok())
            return P402FromIssue(frozen.Issue);
        preparation = frozen.Profile;
    }
    P401ExamStepResult step = P401ApplyExamChoice(State_.Examination, P402Runners(Examination_), command, preparation, now);
    if (!step.Resolution.Ok())
        return P402FromIssue(step.Resolution.Issue);
    const std::int64_t minutes = step.Resolution.OutsideMinutes;
    const bool finished = !P402ExamClosed(State_.Examination.Participation) && P402ExamClosed(step.State.Participation);
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
        if (minutes > 0)
        {
            if (std::optional<Rejection> refusal = ValidateTimeCharge(minutes))
                return refusal;
            ChargeTime(minutes, result);
            if (result.Partial || State_.Clock.Now() != now + minutes)
                return P402Refusal("exam.charge_interrupted", "The examination's time could not settle; nothing was published.");
        }
    }
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
    view.ExaminationAt = CountdownEndInstant();
    view.Session = ExaminationView();
    const TowerParticipation participation = State_.Examination.Participation;
    view.DecisionRequired = ExamDecisionPending() && State_.Clock.Now() == view.ExaminationAt;
    view.Finished = P402ExamClosed(participation);
    view.CanWithdraw = participation == TowerParticipation::Entered;
    if (participation != TowerParticipation::Unset)
        view.OutsideMinutes = view.Session.Result.OutsideMinutes;
    const auto refusal = [this](TowerSessionAction kind) {
        return Validate(Command{CommandSource::Player, P402Command(kind)});
    };
    if (participation == TowerParticipation::Unset)
    {
        view.CanDefer = !refusal(TowerSessionAction::Defer);
        view.CanDecline = !refusal(TowerSessionAction::Decline);
        if (const std::optional<Rejection> sit = refusal(TowerSessionAction::Enter))
            view.SitRefusal = sit->Code;
        else
        {
            // Dry run on a copy: the contract's own guards (catalogue, preparation bounds) decide the rest.
            const P402ExamPreparation preparation = P402FinalPreparation(State_);
            const P401ExamStepResult dry = P401ApplyExamChoice(State_.Examination, P402Runners(Examination_),
                P402Command(TowerSessionAction::Enter), preparation.Profile, State_.Clock.Now());
            view.SitRefusal = dry.Resolution.Issue.Code;
        }
        view.CanSit = view.SitRefusal.empty();
    }
    else
        view.SitRefusal = participation == TowerParticipation::Entered ? "tower.session.already_entered" : "tower.session.closed";
    return view;
}
} // namespace PerfectWorld
