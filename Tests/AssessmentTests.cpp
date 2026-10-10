// ROADMAP M3-07: assessments, the shape of a paper and its time economy (SCHOOL_AND_PROGRESSION
// "Time economy": a 90-minute paper is eighteen five-minute units, five groups worth twenty
// marks each; a practised method clears a group in two units, deriving takes four,
// verification costs one; a prepared character clears and checks everything, an unprepared
// one must triage). The paper, the structure catalogues and the calendar are the real
// Data/Content.
#include "AssessmentTestSupport.h"

#include <set>

using namespace PerfectWorld;

static_assert(AssessmentPaperMinutes / AssessmentUnitMinutes == 18, "a 90-minute paper is eighteen five-minute units");
static_assert(AssessmentGroupCount == 5 && AssessmentGroupMarks == 20 && AssessmentPaperMarks == 100, "five groups of twenty marks");
static_assert(AssessmentClearKnownUnits == 2 && AssessmentClearDeriveUnits == 4 && AssessmentVerifyUnits == 1, "the documented prices");

void TestPaperShape()
{
    const AssessmentPaperDef& paper = *TestWorld->Paper;
    Check(paper.Id == "assessment_paper.xi_half_yearly_mixed", "the Class XI half-yearly paper is authored");
    Check(paper.Units == AssessmentPaperUnits && paper.Questions.size() == AssessmentGroupCount, "eighteen units in five groups");

    std::set<std::string> structures;
    std::set<std::string> groupIds;
    std::set<std::string> caseIds;
    std::int32_t marks = 0;
    for (const AssessmentQuestion& question : paper.Questions)
    {
        Check(question.Marks == AssessmentGroupMarks, "every group is worth twenty marks");
        marks += question.Marks;
        structures.insert(std::string(AssessmentStructureNames[static_cast<std::size_t>(question.Structure)]));
        Check(groupIds.insert(question.GroupId).second && caseIds.insert(question.CaseId).second, "groups and cases are distinct");
        Check(!question.SkillId.empty() && question.MethodKnownAtMilli > 0 && !question.Name.empty(), "each question carries its case's facts");
        const ContentRecord* record = TestWorld->Content.Db.Find(question.CaseId);
        Check(record != nullptr && record->Type == AssessmentCaseTypes[static_cast<std::size_t>(question.Structure)],
              "each question points at a case of its own structure");
    }
    Check(marks == AssessmentPaperMarks, "the paper is worth a hundred marks");
    Check(structures.size() == 4, "the paper draws on all four structures");

    // What each case hides, from its own structure's content.
    const std::vector<bool> traps{true, true, true, true, false};
    for (std::size_t i = 0; i < paper.Questions.size(); ++i)
        Check(paper.Questions[i].Trap == traps[i] && !paper.Questions[i].Critic, "the traps are the cases' own");

    // Linked to the calendar by ID.
    const SchoolCalendar* calendar = TestWorld->Calendars.FindById(paper.CalendarId);
    Check(calendar != nullptr, "the paper's calendar exists");
    const auto window = std::find_if(calendar->Assessments.begin(), calendar->Assessments.end(),
                                     [&](const CalendarAssessment& a) { return a.Id == paper.WindowId; });
    Check(window != calendar->Assessments.end(), "the paper's window is one of the calendar's assessment windows");
    Check(CalendarCountSchoolDays(*calendar, window->First, window->Last) > 0, "the window has school days to sit it on");

    // The economy in numbers: a prepared character fits, an unprepared one does not.
    const std::int32_t prepared = AssessmentSurveyUnits + 5 * AssessmentAnswerUnits(true, AssessmentMode::Work, true);
    const std::int32_t unprepared = 5 * AssessmentAnswerUnits(false, AssessmentMode::Work, true);
    Check(AssessmentAnswerUnits(true, AssessmentMode::Work, false) == 2 && AssessmentAnswerUnits(false, AssessmentMode::Work, false) == 4,
          "clearing costs two units practised and four unfamiliar");
    Check(AssessmentAnswerUnits(true, AssessmentMode::Work, true) == 3 && AssessmentAnswerUnits(false, AssessmentMode::Work, true) == 5,
          "confirming costs one more");
    Check(AssessmentAnswerUnits(false, AssessmentMode::Guess, false) == 1, "a guess costs one unit");
    Check(prepared == 16 && prepared <= paper.Units, "a prepared character's paper fits in the eighteen units");
    Check(unprepared == 25 && unprepared > paper.Units, "an unprepared character's does not");
}

void TestEconomy()
{
    const AssessmentPaperDef& paper = *TestWorld->Paper;

    // A prepared character clears and checks every group, in any order.
    {
        AssessmentState state = Begun(PreparedSkills(paper));
        Check(state.Active() && state.UnitsLeft == 18 && state.Groups.size() == 5, "a begun paper has eighteen units and five groups");
        for (const AssessmentGroupState& group : state.Groups)
            Check(group.Known && !group.Shaky && AssessmentFamiliarityOf(group) == AssessmentFamiliarity::Practised, "every method is practised");
        Check(SurveyAssessment(state, paper).Charged == AssessmentSurveyUnits && state.UnitsLeft == 17, "the survey costs one unit");
        for (std::size_t g = 0; g < 5; ++g)
            Check(AllocateAssessmentUnits(state, g, 3) == AssessmentError::None, "three units are allocated to each group");
        Check(AssessmentUnallocated(state) == 2, "two units are left unpromised");
        for (const std::size_t g : {std::size_t(2), std::size_t(0), std::size_t(4), std::size_t(1), std::size_t(3)})
        {
            const AssessmentQuote quote = QuoteAssessmentAnswer(state, Answer(g, AssessmentMode::Work, true, false));
            Check(quote.Ok() && quote.Units == 3, "a practised group clears and confirms for three units");
            const AssessmentStepResult result = Play(state, Answer(g, AssessmentMode::Work, true, false));
            Check(result.Ok() && result.Charged == 3 && result.Finished == (g == 3), "the last answer hands the paper in");
        }
        Check(state.Finished && !state.Active() && state.UnitsLeft == 2, "the paper is finished with two units to spare");
        const AssessmentMarking marking = MarkAssessment(paper, state);
        Check(marking.Ok() && marking.Total == 100 && marking.MaxTotal == 100, "a prepared character earns every mark");
        Check(marking.UnitsSpent == 16 && marking.UnitsUnused == 2, "sixteen units were spent");
        for (const AssessmentGroupMark& mark : marking.Groups)
            Check(mark.Grade == AssessmentGrade::Sound && mark.Failures == 0 && mark.Native == 0 && mark.Awarded == 20, "every group is sound");
    }

    // An unprepared character cannot afford all of it and must triage.
    {
        AssessmentState state = Begun(UnpreparedSkills());
        for (const AssessmentGroupState& group : state.Groups)
            Check(!group.Known && !group.Shaky && AssessmentFamiliarityOf(group) == AssessmentFamiliarity::Unfamiliar, "no method is practised");
        for (std::size_t g = 0; g < 3; ++g)
            Check(AllocateAssessmentUnits(state, g, 5) == AssessmentError::None, "five units each for the first three groups");
        const AssessmentState before = state;
        Check(AllocateAssessmentUnits(state, 3, 5) == AssessmentError::UnitsShort && state == before, "the fourth group cannot also be given five");
        Check(AssessmentUnallocated(state) == 3, "three units remain to promise");
        Check(AllocateAssessmentUnits(state, 3, 1) == AssessmentError::None && AllocateAssessmentUnits(state, 4, 1) == AssessmentError::None,
              "the last two groups can only be guessed at");
        Check(AssessmentUnallocated(state) == 1, "one unit is left over");

        // Triage: three groups worked and confirmed, two put down as guesses.
        for (std::size_t g = 0; g < 3; ++g)
        {
            const AssessmentQuote quote = QuoteAssessmentAnswer(state, Answer(g, AssessmentMode::Work, true, false));
            Check(quote.Ok() && quote.Units == 5, "an unfamiliar group costs five units to clear and confirm");
            Check(Play(state, Answer(g, AssessmentMode::Work, true, false)).Ok(), "the worked group is answered");
        }
        const AssessmentQuote derive = QuoteAssessmentAnswer(state, Answer(3, AssessmentMode::Work, false, false));
        Check(derive.Error == AssessmentError::AllocationShort && derive.Units == 4, "deriving the fourth group is refused: it was promised only one unit");
        Check(Play(state, Answer(3, AssessmentMode::Guess, false, false)).Ok() && Play(state, Answer(4, AssessmentMode::Guess, false, false)).Finished,
              "the guesses are put down");
        Check(state.Finished && state.UnitsLeft == 1, "one unit was left unspent");
        const AssessmentMarking marking = MarkAssessment(paper, state);
        Check(marking.Ok() && marking.Total == 70, "three sound groups and two wrong ones earn seventy marks");
    }
}

void TestSurveyAndAllocation()
{
    const AssessmentPaperDef& paper = *TestWorld->Paper;
    const AssessmentQuestion& first = paper.Questions[0];

    // The survey is free to read once paid for, and shows the estimate with honest uncertainty.
    AssessmentState state = Begun(PreparedSkills(paper));
    Check(AssessmentSurveyLines(state, paper).empty(), "nothing is surveyed yet");
    const AssessmentQuote quote = QuoteAssessmentSurvey(state);
    Check(quote.Ok() && quote.Units == AssessmentSurveyUnits && quote.UnitsAfter == 17, "the survey's price is quoted first");
    const AssessmentSurveyResult survey = SurveyAssessment(state, paper);
    Check(survey.Ok() && survey.Charged == quote.Units && state.UnitsLeft == quote.UnitsAfter && state.Surveyed, "the survey is charged exactly its quote");
    Check(survey.Lines.size() == 5 && survey.Lines == AssessmentSurveyLines(state, paper), "five lines, readable again for free");
    for (std::size_t i = 0; i < 5; ++i)
    {
        const AssessmentSurveyLine& line = survey.Lines[i];
        Check(line.Group == i && line.GroupId == paper.Questions[i].GroupId && line.Marks == 20 && line.Structure == paper.Questions[i].Structure,
              "a line shows the group, its marks and its structure");
        Check(line.Familiarity == AssessmentFamiliarity::Practised && line.EstimateLow == 2 && line.EstimateHigh == 2, "a practised method is estimated at two units");
    }
    const AssessmentState surveyed = state;
    Check(QuoteAssessmentSurvey(state).Error == AssessmentError::AlreadySurveyed, "a second survey is quoted as refused");
    Check(SurveyAssessment(state, paper).Error == AssessmentError::AlreadySurveyed && state == surveyed, "and refused, changing nothing");

    // The estimate always contains the true clearing price, and is wide only where the character cannot tell.
    for (const std::int32_t offset : {0, -1, -AssessmentShakyMilli, -AssessmentShakyMilli - 1, 1000})
    {
        AssessmentState probe;
        AssessmentSkills skills = SkillsAt(paper, 0);
        for (AssessmentSkillLevel& level : skills)
            level.Milli = std::max<std::int32_t>(0, first.MethodKnownAtMilli + offset);
        Check(BeginAssessment(probe, paper, skills) == AssessmentError::None, "a character at the threshold plus an offset begins");
        const AssessmentGroupState& group = probe.Groups[0];
        const bool known = first.MethodKnownAtMilli + offset >= first.MethodKnownAtMilli;
        const bool shaky = !known && first.MethodKnownAtMilli + offset + AssessmentShakyMilli >= first.MethodKnownAtMilli;
        Check(group.Known == known && group.Shaky == shaky, "familiarity follows the skill: practised at the threshold, shaky within the margin below");
        const auto [low, high] = AssessmentEstimate(AssessmentFamiliarityOf(group));
        const std::int32_t truth = AssessmentAnswerUnits(group.Known, AssessmentMode::Work, false);
        Check(low <= truth && truth <= high, "the estimate contains the true price");
        Check((low == high) == (AssessmentFamiliarityOf(group) != AssessmentFamiliarity::Shaky), "only a shaky method is estimated as a range");
    }

    // The survey is paid from units not already promised, and only on a paper being sat.
    AssessmentState promised = Begun(PreparedSkills(paper));
    Check(AllocateAssessmentUnits(promised, 0, 18) == AssessmentError::None, "every unit can be promised to one group");
    const AssessmentState fullyPromised = promised;
    Check(SurveyAssessment(promised, paper).Error == AssessmentError::UnitsShort && promised == fullyPromised, "the survey cannot be paid from promised units");
    AssessmentState idle;
    Check(QuoteAssessmentSurvey(idle).Error == AssessmentError::NotActive && SurveyAssessment(idle, paper).Error == AssessmentError::NotActive,
          "an idle state surveys nothing");
    AssessmentState wrong = Begun(PreparedSkills(paper));
    AssessmentPaperDef other = paper;
    other.Id = "assessment_paper.other";
    Check(SurveyAssessment(wrong, other).Error == AssessmentError::UnknownPaper && !wrong.Surveyed, "a survey of another paper is refused");

    // Allocation.
    AssessmentState alloc = Begun(PreparedSkills(paper));
    Check(AllocateAssessmentUnits(alloc, 5, 1) == AssessmentError::UnknownGroup, "there is no sixth group");
    Check(AllocateAssessmentUnits(alloc, 0, -1) == AssessmentError::BadAllocation && AllocateAssessmentUnits(alloc, 0, 19) == AssessmentError::BadAllocation,
          "an allocation is zero to eighteen");
    Check(AllocateAssessmentUnits(alloc, 0, 10) == AssessmentError::None && AllocateAssessmentUnits(alloc, 1, 8) == AssessmentError::None,
          "ten and eight units are promised");
    Check(AssessmentUnallocated(alloc) == 0 && AllocateAssessmentUnits(alloc, 2, 1) == AssessmentError::UnitsShort, "nothing is left to promise");
    Check(AllocateAssessmentUnits(alloc, 0, 4) == AssessmentError::None && AssessmentUnallocated(alloc) == 6, "an allocation can be lowered, freeing the rest");
    Check(AllocateAssessmentUnits(alloc, 0, 16) == AssessmentError::UnitsShort && AssessmentUnallocated(alloc) == 6, "raising past the pool is refused");
    Check(AllocateAssessmentUnits(alloc, 0, 10) == AssessmentError::None && AssessmentUnallocated(alloc) == 0, "raising within the pool is allowed");
    alloc = Begun(PreparedSkills(paper));
    Check(AllocateAssessmentUnits(alloc, 0, 2) == AssessmentError::None, "two units are promised to the first group");
    const AssessmentQuote tight = QuoteAssessmentAnswer(alloc, Answer(0, AssessmentMode::Work, true, false));
    Check(tight.Error == AssessmentError::AllocationShort && tight.Units == 3, "confirming costs more than was promised, and says so");
    const AssessmentState untouched = alloc;
    Check(ApplyAssessmentAnswer(alloc, Answer(0, AssessmentMode::Work, true, false)).Error == AssessmentError::AllocationShort && alloc == untouched,
          "an answer past its allocation changes nothing");
    Check(Play(alloc, Answer(0, AssessmentMode::Work, false, false)).Ok(), "clearing alone fits the two units");
    Check(AssessmentUnallocated(alloc) == 16 && AllocateAssessmentUnits(alloc, 0, 3) == AssessmentError::AlreadyAnswered,
          "an answered group's promise is released and cannot be reopened");
    Check(AllocateAssessmentUnits(alloc, 1, 16) == AssessmentError::None, "the released units can be promised again");
    Check(QuoteAssessmentAnswer(alloc, Answer(0, AssessmentMode::Work, false, false)).Error == AssessmentError::AlreadyAnswered, "a group is answered once");
    Check(QuoteAssessmentAnswer(alloc, Answer(7, AssessmentMode::Work, false, false)).Error == AssessmentError::UnknownGroup, "an unknown group is refused");
    AssessmentAnswer bad = Answer(1, AssessmentMode::Work, false, false);
    bad.Mode = static_cast<AssessmentMode>(9);
    Check(QuoteAssessmentAnswer(alloc, bad).Error == AssessmentError::BadChoice, "a mode outside the enum is refused");

    // The paper can be handed in early; nothing works after it.
    Check(FinishAssessment(alloc) == AssessmentError::None && alloc.Finished, "the paper is handed in early");
    Check(FinishAssessment(alloc) == AssessmentError::Finished && SurveyAssessment(alloc, paper).Error == AssessmentError::Finished &&
              AllocateAssessmentUnits(alloc, 1, 1) == AssessmentError::Finished &&
              QuoteAssessmentAnswer(alloc, Answer(1, AssessmentMode::Work, false, false)).Error == AssessmentError::Finished,
          "no command works on a finished paper");
    Check(FinishAssessment(idle) == AssessmentError::NotActive, "an idle state has nothing to hand in");
}

void TestBeginAndErrors()
{
    const AssessmentPaperDef& paper = *TestWorld->Paper;
    AssessmentState state;
    AssessmentSkills negative = PreparedSkills(paper);
    negative[0].Milli = -1;
    Check(BeginAssessment(state, paper, negative) == AssessmentError::InvalidSkill && state == AssessmentState{}, "a negative skill is refused");
    AssessmentSkills twice = PreparedSkills(paper);
    twice.push_back(twice[0]);
    Check(BeginAssessment(state, paper, twice) == AssessmentError::InvalidSkill, "a skill listed twice is refused");
    AssessmentPaperDef shortPaper = paper;
    shortPaper.Questions.pop_back();
    Check(BeginAssessment(state, shortPaper, {}) == AssessmentError::UnknownPaper, "a paper without five groups is refused");

    Check(BeginAssessment(state, paper, {}) == AssessmentError::None && state.Active(), "a paper begins");
    const AssessmentState running = state;
    Check(BeginAssessment(state, paper, {}) == AssessmentError::Busy && state == running, "a second paper is refused while one is being sat");
    Check(FinishAssessment(state) == AssessmentError::None, "the paper is handed in");
    Check(BeginAssessment(state, paper, PreparedSkills(paper)) == AssessmentError::None && state.Active() && state.Groups[0].Known,
          "a finished paper is replaced by a new one");
    Check(CancelAssessment(state) == AssessmentError::None && state == AssessmentState{}, "cancelling returns to idle");
    Check(CancelAssessment(state) == AssessmentError::NotActive, "an idle state has nothing to cancel");

    AssessmentState broken = running;
    broken.UnitsLeft = 3;
    Check(!IsAssessmentStateValid(broken) && BeginAssessment(broken, paper, {}) == AssessmentError::Busy, "an invalid running state is not replaced");
    Check(QuoteAssessmentSurvey(broken).Error == AssessmentError::InvalidState && MarkAssessment(paper, broken).Error == AssessmentError::InvalidState,
          "an invalid state is reported, not worked on");

    std::set<std::string_view> codes;
    for (std::uint8_t e = 1; e <= static_cast<std::uint8_t>(AssessmentError::InvalidState); ++e)
    {
        const std::string_view code = AssessmentErrorCode(static_cast<AssessmentError>(e));
        Check(code.rfind("assessment.", 0) == 0 && codes.insert(code).second, "each refusal has its own stable code");
    }
    Check(AssessmentErrorCode(AssessmentError::None).empty(), "success has no code");
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldAssessmentTests <Data/Content>\n";
        return 2;
    }
    static AssessmentWorld world;
    world.Files = ReadContentDirectory(argv[1]);
    world.Content = LoadContent(world.Files);
    for (const ContentIssue& issue : world.Content.Issues)
        std::cerr << issue.File << ": " << issue.Message << '\n';
    Check(world.Content.Ok(), "the real content validates");

    const ModelCatalogueResult model = LoadModelCatalogue(world.Content.Db);
    const ExplainCatalogueResult explain = LoadExplainCatalogue(world.Content.Db);
    const InvestigateCatalogueResult investigate = LoadInvestigateCatalogue(world.Content.Db);
    const CoordinateCatalogueResult coordinate = LoadCoordinateCatalogue(world.Content.Db);
    Check(model.Ok() && explain.Ok() && investigate.Ok() && coordinate.Ok(), "the four structure catalogues load");
    world.Structures.Model = model.Catalogue;
    world.Structures.Explain = explain.Catalogue;
    world.Structures.Investigate = investigate.Catalogue;
    world.Structures.Coordinate = coordinate.Catalogue;
    const AssessmentCatalogueResult papers = LoadAssessmentCatalogue(world.Content.Db, StructuresOf(world.Structures));
    Check(papers.Ok(), papers.Error.c_str());
    world.Papers = papers.Catalogue;
    const SchoolCalendarResult calendars = LoadSchoolCalendars(world.Content.Db);
    Check(calendars.Ok(), calendars.Error.c_str());
    world.Calendars = calendars.Set;
    Check(world.Papers.Papers.size() == 1, "one paper is authored");
    world.Paper = world.Papers.FindPaper("assessment_paper.xi_half_yearly_mixed");
    Check(world.Paper != nullptr, "the paper is found by ID");
    TestWorld = &world;

    TestPaperShape();
    TestEconomy();
    TestSurveyAndAllocation();
    TestBeginAndErrors();
    TestMarkingKinds();
    TestMarkingPaper();
    TestSaveAndLoad();
    TestRandomSequences();
    TestContentRules();
    std::cout << "AssessmentTests passed (" << OthersTestChecks << " checks)\n";
    return 0;
}
