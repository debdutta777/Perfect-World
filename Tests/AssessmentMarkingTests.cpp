// ROADMAP M3-07: diagnostic marking. Each question reports which of the four kinds of failure
// occurred (a wrong model, an unsupported conclusion, wasted time, an unclear explanation),
// mapped from its structure's own failure bits, and the marks follow the grade. The first
// test holds the rules over handmade questions in every combination; the second marks
// scripted answers on the real paper.
#include "AssessmentTestSupport.h"

using namespace PerfectWorld;

namespace
{
constexpr std::uint8_t W = AssessmentFailWrongModel;
constexpr std::uint8_t U = AssessmentFailUnsupported;
constexpr std::uint8_t T = AssessmentFailWastedTime;
constexpr std::uint8_t C = AssessmentFailUnclear;

AssessmentQuestion MarkingQuestion(AssessmentStructure structure, bool trap, bool critic)
{
    AssessmentQuestion question;
    question.GroupId = "g";
    question.CaseId = "case";
    question.Structure = structure;
    question.Trap = trap;
    question.Critic = critic;
    return question;
}

AssessmentGroupState MarkingGroup(AssessmentMode mode, bool confirm, bool hedge)
{
    AssessmentGroupState group;
    group.Answered = true;
    group.Mode = mode;
    group.Confirm = confirm;
    group.Hedge = hedge;
    group.Spent = AssessmentAnswerUnits(false, mode, confirm);
    return group;
}

/** One structure's own bits for: a wrong answer, a claim made too freely, and a needless hedge. */
struct NativeSet
{
    std::uint8_t Wrong;
    std::uint8_t Claimed;
    std::uint8_t Waste;
    /** The kind a claim made too freely is reported as. */
    std::uint8_t ClaimedKind;
};

NativeSet NativeOf(AssessmentStructure structure)
{
    switch (structure)
    {
    case AssessmentStructure::Model:
        return {ModelFailWrongModel, ModelFailUnsupported, ModelFailWastedTime, U};
    case AssessmentStructure::Explain:
        return {ExplainFailIndefensible, ExplainFailOverreach, ExplainFailPadding, C};
    case AssessmentStructure::Investigate:
        return {InvestigateFailWrongCause, InvestigateFailUnsupported, InvestigateFailAbandoned, U};
    case AssessmentStructure::Coordinate:
        break;
    }
    return {CoordinateFailOverloaded, CoordinateFailSeam, CoordinateFailWasted, C};
}

const AssessmentStructure AllStructures[] = {AssessmentStructure::Model, AssessmentStructure::Explain, AssessmentStructure::Investigate,
                                             AssessmentStructure::Coordinate};

void TestMapping()
{
    // Every native bit maps to the kind the header documents; bits a structure does not define map to nothing.
    struct Row
    {
        AssessmentStructure Structure;
        std::uint8_t Native;
        std::uint8_t Kind;
    };
    const Row rows[] = {
        {AssessmentStructure::Model, ModelFailWrongModel, W},           {AssessmentStructure::Model, ModelFailUnsupported, U},
        {AssessmentStructure::Model, ModelFailWastedTime, T},           {AssessmentStructure::Model, ModelFailUnclear, C},
        {AssessmentStructure::Explain, ExplainFailIndefensible, W},     {AssessmentStructure::Explain, ExplainFailUnsupported, U},
        {AssessmentStructure::Explain, ExplainFailPadding, T},          {AssessmentStructure::Explain, ExplainFailOverreach, C},
        {AssessmentStructure::Investigate, InvestigateFailWrongCause, W}, {AssessmentStructure::Investigate, InvestigateFailUnsupported, U},
        {AssessmentStructure::Investigate, InvestigateFailRedundant, T}, {AssessmentStructure::Investigate, InvestigateFailAbandoned, T},
        {AssessmentStructure::Coordinate, CoordinateFailOverloaded, W}, {AssessmentStructure::Coordinate, CoordinateFailMismatched, W},
        {AssessmentStructure::Coordinate, CoordinateFailUninformed, U}, {AssessmentStructure::Coordinate, CoordinateFailUnsettled, U},
        {AssessmentStructure::Coordinate, CoordinateFailWasted, T},     {AssessmentStructure::Coordinate, CoordinateFailSeam, C}};
    for (const Row& row : rows)
        Check(AssessmentKindsOf(row.Structure, row.Native) == row.Kind, "a native failure maps to its documented kind");
    for (const AssessmentStructure structure : AllStructures)
    {
        Check(AssessmentKindsOf(structure, 0) == 0, "no failure maps to none");
        std::uint8_t all = 0;
        for (const Row& row : rows)
            all = static_cast<std::uint8_t>(all | (row.Structure == structure ? row.Native : 0));
        // The investigate structure has no failure of its own for an unclear explanation.
        const std::uint8_t covered = structure == AssessmentStructure::Investigate ? (W | U | T) : (W | U | T | C);
        Check(AssessmentKindsOf(structure, all) == covered, "the structure's bits together cover the kinds it can fail in");
    }
    Check(AssessmentKindsOf(AssessmentStructure::Model, 16 | 32 | 64 | 128) == 0, "bits the model structure does not define are ignored");
    Check(AssessmentKindsOf(AssessmentStructure::Explain, 64) == 0, "so are the explain structure's");
    Check(AssessmentStructureNames.size() == 4 && AssessmentFailureNames.size() == 4, "four structures and four kinds");
}
} // namespace

void TestMarkingKinds()
{
    TestMapping();

    // Every combination of structure, trap, seat and answer.
    for (const AssessmentStructure structure : AllStructures)
    {
        const NativeSet native = NativeOf(structure);
        for (const bool trap : {false, true})
        {
            for (const bool critic : {false, true})
            {
                for (const AssessmentMode mode : {AssessmentMode::Work, AssessmentMode::Guess})
                {
                    for (const bool confirm : {false, true})
                    {
                        for (const bool hedge : {false, true})
                        {
                            const AssessmentQuestion question = MarkingQuestion(structure, trap, critic);
                            const AssessmentGroupState group = MarkingGroup(mode, confirm, hedge);
                            const bool work = mode == AssessmentMode::Work;
                            std::uint8_t expected = 0;
                            std::uint8_t expectedKinds = 0;
                            if (!work)
                            {
                                expected |= native.Wrong;
                                expectedKinds |= W;
                            }
                            if (trap && !confirm && !hedge)
                            {
                                expected |= native.Claimed;
                                expectedKinds |= native.ClaimedKind;
                            }
                            if (hedge && work && (!trap || confirm) && !critic)
                            {
                                expected |= native.Waste;
                                expectedKinds |= T;
                            }
                            const std::uint8_t got = AssessmentNativeFailures(question, group);
                            Check(got == expected, "the structure's own failure bits follow the answer");
                            Check(AssessmentKindsOf(structure, got) == expectedKinds, "and map to the documented kinds");
                        }
                    }
                }
            }
        }
    }

    // The documented examples, one by one.
    const AssessmentGroupState worked = MarkingGroup(AssessmentMode::Work, false, false);
    Check(AssessmentKindsOf(AssessmentStructure::Model, AssessmentNativeFailures(MarkingQuestion(AssessmentStructure::Model, true, false), worked)) == U,
          "a firm unconfirmed answer on a model question with a hidden fault is an unsupported conclusion");
    Check(AssessmentKindsOf(AssessmentStructure::Explain, AssessmentNativeFailures(MarkingQuestion(AssessmentStructure::Explain, true, false), worked)) == C,
          "an explanation with no stated limit is an unclear explanation");
    Check(AssessmentKindsOf(AssessmentStructure::Coordinate, AssessmentNativeFailures(MarkingQuestion(AssessmentStructure::Coordinate, true, false), worked)) == C,
          "a hand-over with a seam, unchecked, is unclear");
    Check(AssessmentNativeFailures(MarkingQuestion(AssessmentStructure::Model, false, false), worked) == 0, "a clean question answered firmly has no failure");
    const AssessmentGroupState hedged = MarkingGroup(AssessmentMode::Work, false, true);
    Check(AssessmentNativeFailures(MarkingQuestion(AssessmentStructure::Model, false, false), hedged) == ModelFailWastedTime,
          "hedging a clean question wastes the answer");
    Check(AssessmentNativeFailures(MarkingQuestion(AssessmentStructure::Model, false, true), hedged) == 0, "but not for the critic, whose job it is");
    Check(AssessmentNativeFailures(MarkingQuestion(AssessmentStructure::Model, true, false), hedged) == 0, "and a hedge is the honest cover for a trap not confirmed");
}

void TestMarkingPaper()
{
    const AssessmentPaperDef& paper = *TestWorld->Paper;

    // Scripted answers on a prepared character: every group fails differently.
    {
        AssessmentState state = Begun(PreparedSkills(paper));
        Check(AllocateAndPlay(state, Answer(0, AssessmentMode::Work, false, false)).Ok(), "g1 answered firmly, unconfirmed");
        Check(AllocateAndPlay(state, Answer(1, AssessmentMode::Work, false, false)).Ok(), "g2 answered without its limit");
        Check(AllocateAndPlay(state, Answer(2, AssessmentMode::Work, true, true)).Ok(), "g3 confirmed and then hedged");
        Check(AllocateAndPlay(state, Answer(3, AssessmentMode::Guess, true, false)).Ok(), "g4 guessed, then confirmed");
        Check(AllocateAndPlay(state, Answer(4, AssessmentMode::Work, false, true)).Finished, "g5 hedged though clean");
        const AssessmentMarking marking = MarkAssessment(paper, state);
        Check(marking.Ok() && marking.Groups.size() == 5, "a finished paper is marked");

        const AssessmentGroupMark& g1 = marking.Groups[0];
        Check(g1.Failures == U && g1.Native == ModelFailUnsupported && g1.Grade == AssessmentGrade::Unsound && g1.Awarded == 5,
              "g1: an unsupported conclusion, unsound, a quarter of the marks");
        const AssessmentGroupMark& g2 = marking.Groups[1];
        Check(g2.Failures == C && g2.Native == ExplainFailOverreach && g2.Grade == AssessmentGrade::Partial && g2.Awarded == 15,
              "g2: an unclear explanation, partial, three quarters");
        const AssessmentGroupMark& g3 = marking.Groups[2];
        Check(g3.Failures == T && g3.Native == InvestigateFailAbandoned && g3.Grade == AssessmentGrade::Partial && g3.Awarded == 15,
              "g3: wasted time, partial");
        const AssessmentGroupMark& g4 = marking.Groups[3];
        Check(g4.Failures == W && g4.Native == CoordinateFailOverloaded && g4.Grade == AssessmentGrade::Unsound && g4.Awarded == 5,
              "g4: a wrong model, unsound");
        const AssessmentGroupMark& g5 = marking.Groups[4];
        Check(g5.Failures == T && g5.Native == ModelFailWastedTime && g5.Grade == AssessmentGrade::Partial && g5.Awarded == 15,
              "g5: wasted time, partial");
        Check(marking.Total == 55 && marking.MaxTotal == 100, "fifty-five marks in all");
        for (std::size_t i = 0; i < 5; ++i)
        {
            const AssessmentGroupMark& mark = marking.Groups[i];
            Check(mark.Group == i && mark.GroupId == paper.Questions[i].GroupId && mark.CaseId == paper.Questions[i].CaseId &&
                      mark.Structure == paper.Questions[i].Structure && mark.Marks == 20,
                  "a mark names its question");
            Check(mark.Familiarity == AssessmentFamiliarity::Practised && mark.Spent == mark.Allocated, "and records the familiarity and the units spent");
            Check(mark.Failures == AssessmentKindsOf(mark.Structure, mark.Native), "the kinds are the native bits mapped");
        }
        Check(marking.UnitsSpent == 2 + 2 + 3 + 2 + 2 && marking.UnitsUnused == 18 - marking.UnitsSpent, "the units add up to the paper");
        Check(marking == MarkAssessment(paper, state), "marking is a pure function of the paper and the state");
    }

    // A guess alone also leaves the trap unconfirmed, so it carries both failures.
    {
        AssessmentState state = Begun(PreparedSkills(paper));
        for (std::size_t g = 0; g < 5; ++g)
            Check(AllocateAndPlay(state, Answer(g, AssessmentMode::Guess, false, false)).Ok(), "every group is guessed");
        const AssessmentMarking marking = MarkAssessment(paper, state);
        const std::uint8_t expected[5] = {W | U, W | C, W | U, W | C, W};
        for (std::size_t g = 0; g < 5; ++g)
            Check(marking.Groups[g].Failures == expected[g] && marking.Groups[g].Grade == AssessmentGrade::Unsound, "a bare guess is a wrong model and what it left unconfirmed");
        Check(marking.Total == 25, "five unsound groups earn twenty-five marks");
    }

    // Work never reached is unfinished: wasted time, no native bits, no marks.
    {
        AssessmentState state = Begun(PreparedSkills(paper));
        Check(AllocateAndPlay(state, Answer(0, AssessmentMode::Work, true, false)).Ok() && AllocateAndPlay(state, Answer(1, AssessmentMode::Work, true, false)).Ok(),
              "two groups are done");
        Check(MarkAssessment(paper, state).Error == AssessmentError::NotFinished, "a paper still being sat cannot be marked");
        Check(FinishAssessment(state) == AssessmentError::None, "handed in with three groups unanswered");
        const AssessmentMarking marking = MarkAssessment(paper, state);
        Check(marking.Ok() && marking.Total == 40 && marking.UnitsSpent == 6 && marking.UnitsUnused == 12, "forty marks from six units");
        for (std::size_t g = 0; g < 5; ++g)
        {
            const AssessmentGroupMark& mark = marking.Groups[g];
            if (g < 2)
            {
                Check(mark.Grade == AssessmentGrade::Sound && mark.Failures == 0, "the answered groups are sound");
                continue;
            }
            Check(mark.Grade == AssessmentGrade::Unattempted && mark.Failures == T && mark.Native == 0 && mark.Awarded == 0 && mark.Spent == 0,
                  "an unanswered group is unfinished work");
        }
    }

    // A low stat never changes what a committed answer earns, only what it costs.
    {
        const AssessmentState prepared = Begun(PreparedSkills(paper));
        const AssessmentState unprepared = Begun(UnpreparedSkills());
        for (std::size_t g = 0; g < 5; ++g)
        {
            for (const bool confirm : {false, true})
            {
                AssessmentGroupState a = prepared.Groups[g];
                AssessmentGroupState b = unprepared.Groups[g];
                a.Confirm = b.Confirm = confirm;
                Check(AssessmentNativeFailures(paper.Questions[g], a) == AssessmentNativeFailures(paper.Questions[g], b),
                      "the same answer earns the same failures whatever the skill");
                Check(AssessmentAnswerUnits(b.Known, AssessmentMode::Work, confirm) == AssessmentAnswerUnits(a.Known, AssessmentMode::Work, confirm) + 2,
                      "and costs two units more when the method is unfamiliar");
            }
        }
    }

    // Marking refuses what it cannot read.
    {
        AssessmentState state = Begun(PreparedSkills(paper));
        FinishAssessment(state);
        AssessmentPaperDef other = paper;
        other.Id = "assessment_paper.other";
        Check(MarkAssessment(other, state).Error == AssessmentError::InvalidState, "marking against another paper is refused");
        Check(MarkAssessment(paper, AssessmentState{}).Error == AssessmentError::InvalidState, "an idle state is not a paper");
        const AssessmentMarking none = MarkAssessment(paper, state);
        Check(none.Ok() && none.Total == 0 && none.UnitsUnused == 18, "a paper handed in blank earns nothing and leaves every unit");
    }
}
