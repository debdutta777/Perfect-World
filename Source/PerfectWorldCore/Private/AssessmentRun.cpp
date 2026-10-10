#include "Assessment.h"

#include <algorithm>

// Allocating units, answering a group and diagnostic marking (ROADMAP M3-07). The rules are
// documented in Public/Assessment.h. Every command works on a copy and replaces the state
// only on success, so a refusal changes nothing.
namespace PerfectWorld
{
namespace
{
constexpr std::uint8_t AssessmentUnsoundKinds = AssessmentFailWrongModel | AssessmentFailUnsupported;

/** None while the paper can be worked on, else why not. */
AssessmentError AssessmentWorkError(const AssessmentState& state) noexcept
{
    if (!IsAssessmentStateValid(state))
        return AssessmentError::InvalidState;
    if (state.PaperId.empty())
        return AssessmentError::NotActive;
    return state.Finished ? AssessmentError::Finished : AssessmentError::None;
}

AssessmentGrade AssessmentGradeOf(bool answered, std::uint8_t kinds) noexcept
{
    if (!answered)
        return AssessmentGrade::Unattempted;
    if ((kinds & AssessmentUnsoundKinds) != 0)
        return AssessmentGrade::Unsound;
    return kinds == 0 ? AssessmentGrade::Sound : AssessmentGrade::Partial;
}

std::int32_t AssessmentPermilleOf(AssessmentGrade grade) noexcept
{
    switch (grade)
    {
    case AssessmentGrade::Sound:
        return AssessmentSoundPermille;
    case AssessmentGrade::Partial:
        return AssessmentPartialPermille;
    case AssessmentGrade::Unsound:
        return AssessmentUnsoundPermille;
    case AssessmentGrade::Unattempted:
        break;
    }
    return 0;
}
} // namespace

AssessmentError AllocateAssessmentUnits(AssessmentState& state, std::size_t group, std::int32_t units)
{
    if (const AssessmentError error = AssessmentWorkError(state); error != AssessmentError::None)
        return error;
    if (group >= state.Groups.size())
        return AssessmentError::UnknownGroup;
    if (state.Groups[group].Answered)
        return AssessmentError::AlreadyAnswered;
    if (units < 0 || units > AssessmentPaperUnits)
        return AssessmentError::BadAllocation;
    if (AssessmentUnallocated(state) + state.Groups[group].Allocated < units)
        return AssessmentError::UnitsShort;
    state.Groups[group].Allocated = units;
    return AssessmentError::None;
}

AssessmentQuote QuoteAssessmentAnswer(const AssessmentState& state, const AssessmentAnswer& answer)
{
    AssessmentQuote quote;
    quote.Error = AssessmentWorkError(state);
    if (quote.Error != AssessmentError::None)
        return quote;
    if (answer.Group >= state.Groups.size())
    {
        quote.Error = AssessmentError::UnknownGroup;
        return quote;
    }
    const AssessmentGroupState& group = state.Groups[answer.Group];
    if (group.Answered)
    {
        quote.Error = AssessmentError::AlreadyAnswered;
        return quote;
    }
    if (static_cast<std::size_t>(answer.Mode) >= AssessmentModeNames.size())
    {
        quote.Error = AssessmentError::BadChoice;
        return quote;
    }
    quote.Units = AssessmentAnswerUnits(group.Known, answer.Mode, answer.Confirm);
    quote.UnitsAfter = state.UnitsLeft - quote.Units;
    if (quote.Units > group.Allocated)
        quote.Error = AssessmentError::AllocationShort;
    else if (quote.Units > state.UnitsLeft)
        quote.Error = AssessmentError::UnitsShort;
    return quote;
}

AssessmentStepResult ApplyAssessmentAnswer(AssessmentState& state, const AssessmentAnswer& answer)
{
    AssessmentStepResult result;
    const AssessmentQuote quote = QuoteAssessmentAnswer(state, answer);
    if (!quote.Ok())
    {
        result.Error = quote.Error;
        return result;
    }
    AssessmentState next = state;
    AssessmentGroupState& group = next.Groups[answer.Group];
    group.Answered = true;
    group.Mode = answer.Mode;
    group.Confirm = answer.Confirm;
    group.Hedge = answer.Hedge;
    group.Spent = quote.Units;
    next.UnitsLeft = quote.UnitsAfter;
    next.Finished = std::all_of(next.Groups.begin(), next.Groups.end(), [](const AssessmentGroupState& g) { return g.Answered; });
    result.Charged = quote.Units;
    result.Finished = next.Finished;
    state = std::move(next);
    return result;
}

std::uint8_t AssessmentNativeFailures(const AssessmentQuestion& question, const AssessmentGroupState& group) noexcept
{
    const bool work = group.Mode == AssessmentMode::Work;
    // A trap neither confirmed nor hedged is claimed past what the answer supports.
    const bool claimedTooMuch = question.Trap && !group.Confirm && !group.Hedge;
    // A hedge on an answer that needed none, unless the player's seat is the critic's.
    const bool hedgedForNothing = group.Hedge && work && (!question.Trap || group.Confirm) && !question.Critic;

    std::uint8_t native = 0;
    const auto set = [&native](bool condition, std::uint8_t bit) {
        if (condition)
            native = static_cast<std::uint8_t>(native | bit);
    };
    switch (question.Structure)
    {
    case AssessmentStructure::Model:
        set(!work, ModelFailWrongModel);
        set(claimedTooMuch, ModelFailUnsupported);
        set(hedgedForNothing, ModelFailWastedTime);
        break;
    case AssessmentStructure::Explain:
        set(!work, ExplainFailIndefensible);
        set(claimedTooMuch, ExplainFailOverreach);
        set(hedgedForNothing, ExplainFailPadding);
        break;
    case AssessmentStructure::Investigate:
        set(!work, InvestigateFailWrongCause);
        set(claimedTooMuch, InvestigateFailUnsupported);
        set(hedgedForNothing, InvestigateFailAbandoned);
        break;
    case AssessmentStructure::Coordinate:
        set(!work, CoordinateFailOverloaded);
        set(claimedTooMuch, CoordinateFailSeam);
        set(hedgedForNothing, CoordinateFailWasted);
        break;
    }
    return native;
}

AssessmentMarking MarkAssessment(const AssessmentPaperDef& paper, const AssessmentState& state)
{
    AssessmentMarking marking;
    if (!IsAssessmentStateValid(state) || state.PaperId.empty() || state.PaperId != paper.Id || paper.Questions.size() != state.Groups.size())
    {
        marking.Error = AssessmentError::InvalidState;
        return marking;
    }
    if (!state.Finished)
    {
        marking.Error = AssessmentError::NotFinished;
        return marking;
    }
    for (std::size_t i = 0; i < paper.Questions.size(); ++i)
    {
        const AssessmentQuestion& question = paper.Questions[i];
        const AssessmentGroupState& group = state.Groups[i];
        AssessmentGroupMark mark;
        mark.Group = i;
        mark.GroupId = question.GroupId;
        mark.Structure = question.Structure;
        mark.CaseId = question.CaseId;
        mark.Marks = question.Marks;
        mark.Familiarity = AssessmentFamiliarityOf(group);
        mark.Allocated = group.Allocated;
        mark.Spent = group.Spent;
        if (group.Answered)
        {
            mark.Native = AssessmentNativeFailures(question, group);
            mark.Failures = AssessmentKindsOf(question.Structure, mark.Native);
        }
        else
        {
            // Work never reached is unfinished work: the time went elsewhere.
            mark.Failures = AssessmentFailWastedTime;
        }
        mark.Grade = AssessmentGradeOf(group.Answered, mark.Failures);
        mark.Awarded = question.Marks * AssessmentPermilleOf(mark.Grade) / 1000;
        marking.Total += mark.Awarded;
        marking.MaxTotal += question.Marks;
        marking.UnitsSpent += group.Spent;
        marking.Groups.push_back(std::move(mark));
    }
    marking.UnitsSpent += state.Surveyed ? AssessmentSurveyUnits : 0;
    marking.UnitsUnused = state.UnitsLeft;
    return marking;
}
} // namespace PerfectWorld
