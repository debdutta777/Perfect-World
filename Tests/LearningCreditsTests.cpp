#include "LearningCredits.h"

#include "P2CTestSupport.h"

#include <limits>

using namespace PerfectWorld;
namespace
{
void P32Expect(bool condition, std::string_view message)
{
    if (!condition)
    {
        std::cerr << "P32 failure: " << message << '\n';
        std::exit(1);
    }
}

std::size_t P32Trait(std::string_view id)
{
    const auto& traits = CharacterCreator::TraitDefinitions();
    const auto found = std::find_if(traits.begin(), traits.end(), [&](const auto& trait) { return trait.Id == id; });
    P32Expect(found != traits.end(), "stable trait exists");
    return static_cast<std::size_t>(found - traits.begin());
}

AwakeningState P32Seed(const P2CFixture& fixture, std::int32_t foundation = 0, std::int32_t household = 0)
{
    CharacterCreationSave creation;
    creation.TraitRanks[P32Trait("academic_foundation")] = foundation;
    creation.TraitRanks[P32Trait("household_resources")] = household;
    const auto seeded = SeedLearningInputs(fixture.Content, creation, 3202);
    P32Expect(seeded.Ok(), "new-life seed accepted");
    const auto existing = SeedAwakeningInputs(fixture.Content, creation, 3202);
    P32Expect(existing.Ok() && seeded.State.Stats == existing.State.Stats, "preparation is included exactly once by R9");
    return seeded.State;
}

std::int32_t P32Academics(const AwakeningState& state)
{
    const auto* skill = state.Stats.FindSkill("skill.academics");
    P32Expect(skill != nullptr, "Academics exists");
    return skill->LevelMilli;
}

void P32SameLearning(const AwakeningState& left, const AwakeningState& right)
{
    P32Expect(left.Stats == right.Stats && left.AttributeEvidenceMilli == right.AttributeEvidenceMilli &&
        left.AppliedActivityInstances == right.AppliedActivityInstances &&
        left.ProjectionEvidence.Items == right.ProjectionEvidence.Items, "all learning owners agree");
}

DayLoopResult P32Study(DayLoopState& day, const DayLoopCatalogues& cats)
{
    DayCommand command;
    if (!day.Planned)
    {
        command.Kind = DayCommandKind::PlanDay;
        P32Expect(ApplyDayCommand(day, cats, command).Ok(), "routine morning plan");
    }
    command.Kind = DayCommandKind::Commit;
    command.ActivityId = "activity.study_at_home";
    command.PlaceKind = "home";
    P32Expect(ApplyDayCommand(day, cats, command).Ok(), "routine study committed");
    command = {};
    command.Kind = DayCommandKind::Start;
    P32Expect(ApplyDayCommand(day, cats, command).Ok(), "routine study started");
    command.Kind = DayCommandKind::Complete;
    auto done = ApplyDayCommand(day, cats, command);
    P32Expect(done.Ok(), "routine study completed through DayLoop");
    return done;
}

void P32Foundation(const P2CFixture& fixture)
{
    std::int32_t previous = -1;
    for (std::int32_t rank = 0; rank <= MaxRank; ++rank)
    {
        const auto preparation = AcademicPriorPreparation(fixture.Content, rank);
        P32Expect(preparation.Ok() && preparation.MilliXp > previous && preparation.MilliXp >= 0 &&
            preparation.MilliXp <= AcademicPriorPreparationMaxMilli, "every foundation rank strictly moves bounded prior preparation");
        previous = preparation.MilliXp;
        const auto baseline = P32Seed(fixture);
        const auto seeded = P32Seed(fixture, rank);
        P32Expect(P32Academics(seeded) == P32Academics(baseline) + preparation.MilliXp,
            "content table is the only preparation contribution");
        P32Expect(seeded.ProjectionEvidence.Items.empty() && seeded.AppliedActivityInstances.empty() &&
            seeded.Conduct.Entries.empty() && !seeded.Granted && seeded.AttributeEvidenceMilli == baseline.AttributeEvidenceMilli,
            "prior preparation creates no fictional lesson or observed evidence");
        for (std::int32_t household = 0; household <= MaxRank; ++household)
            P32SameLearning(P32Seed(fixture, rank, household), seeded);
    }
    P32Expect(!AcademicPriorPreparation(fixture.Content, -1).Ok() &&
        !AcademicPriorPreparation(fixture.Content, MaxRank + 1).Ok(), "invalid foundation ranks refuse");
    const auto cats = fixture.Cats();
    const auto state = P32Seed(fixture, 4);
    auto bindings = fixture.Bindings();
    P32Expect(LearningLessonSkillMilli(state, bindings.front(), cats.Structures) == P32Academics(state),
        "lesson seam reads family skill from R9");
    bindings.front().FamilyId = "problem_family.missing";
    P32Expect(LearningLessonSkillMilli(state, bindings.front(), cats.Structures) == 0 &&
        LearningLessonSkillMilli({}, fixture.Bindings().front(), cats.Structures) == 0, "unknown or unseeded lesson skill is zero");
    const auto skills = LearningWorkSkills(state);
    P32Expect(skills.size() == state.Stats.Skills.size() && LearningWorkSkills({}).empty(), "paper seam reads one owner");
    for (std::size_t i = 0; i < skills.size(); ++i)
        P32Expect(skills[i].SkillId == state.Stats.Skills[i].SkillId && skills[i].Milli == state.Stats.Skills[i].LevelMilli,
            "paper skill names and units match AssessmentSkills");
}

void P32Lessons(const P2CFixture& fixture)
{
    auto state = P32Seed(fixture, 4);
    const auto cats = fixture.Cats();
    const auto binding = fixture.Bindings().front();
    auto day = P2CDayAtBell(cats);
    SchoolSessionsState sessions;
    const auto opened = OpenSchoolLesson(sessions, day, cats, binding, SchoolLessonMode::Foreground,
        ModelGoal::UnderstandModel, LearningLessonSkillMilli(state, binding, cats.Structures), 3202);
    P32Expect(opened.Ok() && ApplyLearningCredits(state, opened).Applied == 0, "opening emits no learning");
    const auto baseline = state;
    P2CPlayLesson(sessions, cats, opened.Session);
    const auto closed = CloseSchoolLesson(sessions, day, cats, opened.Session);
    const auto credited = ApplyLearningCredits(state, closed);
    P32Expect(closed.Ok() && credited.Ok() && credited.Applied == 1 && credited.Replayed == 0,
        "paid foreground close credits exactly once");
    const auto after = state;
    P32Expect(ApplyLearningCredits(state, closed).Replayed == 1, "same close event is inert on replay");
    P32SameLearning(state, after);
    const auto duplicate = CloseSchoolLesson(sessions, day, cats, opened.Session);
    P32Expect(!duplicate.Ok() && !ApplyLearningCredits(state, duplicate).Ok(), "duplicate close refuses without credit");
    P32SameLearning(state, after);
    P32Expect(state.Stats.Attributes == baseline.Stats.Attributes && state.ProjectionEvidence.Items.empty(),
        "attribute evidence never silently becomes attribute growth or aptitude");
    const std::array repeated{closed.Events.front(), closed.Events.front()};
    auto batch = baseline;
    const auto once = ApplyLearningCredits(batch, repeated);
    P32Expect(once.Ok() && once.Applied == 1 && once.Replayed == 1, "duplicate within batch consumes one runner ID");
    P32SameLearning(batch, after);
    auto malformed = closed.Events.front();
    ++malformed.Instance;
    malformed.Credited = {{"skill.missing", 1}};
    const std::array refusedBatch{closed.Events.front(), malformed};
    auto atomic = baseline;
    P32Expect(!ApplyLearningCredits(atomic, refusedBatch).Ok(), "invalid batch refuses all credits");
    P32SameLearning(atomic, baseline);
    malformed.Instance = closed.Events.front().Instance;
    P32Expect(ApplyLearningCredits(state, std::span(&malformed, 1)).Replayed == 1,
        "consumed instance stays inert even with a changed payload");
    P32SameLearning(state, after);
    auto bad = closed;
    bad.Issue = {"school.session.unfinished", {}};
    auto refused = baseline;
    P32Expect(!ApplyLearningCredits(refused, bad).Ok(), "refused outcome cannot smuggle completion events");
    P32SameLearning(refused, baseline);
    auto cancelledDay = P2CDayAtBell(cats);
    SchoolSessionsState cancelledSessions;
    const auto attending = OpenSchoolLesson(cancelledSessions, cancelledDay, cats, binding, SchoolLessonMode::Attend,
        ModelGoal::UnderstandModel, 0, 3202);
    P32Expect(attending.Ok(), "attend opens");
    const auto cancelled = CloseSchoolLesson(cancelledSessions, cancelledDay, cats, attending.Session, true, 1);
    P32Expect(cancelled.Ok() && ApplyLearningCredits(refused, cancelled).Applied == 0, "cancelled lesson earns no learning");
    P32SameLearning(refused, baseline);
    // Attend and foreground receive the same normal completion allowance.
    auto attendedDay = P2CDayAtBell(cats);
    SchoolSessionsState attendedSessions;
    const auto attend = OpenSchoolLesson(attendedSessions, attendedDay, cats, binding, SchoolLessonMode::Attend,
        ModelGoal::UnderstandModel, 0, 3202);
    P32Expect(attend.Ok(), "supervised routine instruction opens");
    const auto attended = CloseSchoolLesson(attendedSessions, attendedDay, cats, attend.Session);
    auto routine = baseline;
    P32Expect(attended.Ok() && ApplyLearningCredits(routine, attended).Applied == 1, "routine instruction credits once");
    P32SameLearning(routine, after);
}

void P32Limits(const P2CFixture& fixture)
{
    auto state = P32Seed(fixture);
    const auto baseline = state;
    auto day = NewDayLoopState();
    const DayLoopCatalogues cats{&fixture.Activities};
    const std::array<std::int32_t, 4> expected{2400, 1200, 240, 0};
    for (const auto credit : expected)
    {
        const auto before = P32Academics(state);
        const auto done = P32Study(day, cats);
        const auto limitedPractice = day.Practice;
        P32Expect(ApplyLearningCredits(state, done).Applied == 1 && P32Academics(state) - before == credit,
            "routine credits use full half tenth none after practice limits");
        const auto after = state;
        P32Expect(ApplyLearningCredits(state, done).Replayed == 1 && day.Practice == limitedPractice,
            "replay spends neither learning nor practice a second time");
        P32SameLearning(state, after);
        for (std::int32_t household = 0; household <= MaxRank; ++household)
        {
            auto other = P32Seed(fixture, 0, household);
            P32Expect(ApplyLearningCredits(other, done).Applied == 1 &&
                P32Academics(other) - P32Academics(baseline) == credit, "household rank never changes academic credit");
        }
    }
    auto activities = fixture.Activities;
    auto budgetDay = NewDayLoopState();
    auto budgetOwner = baseline;
    const DayLoopCatalogues varied{&activities};
    for (int i = 0; i < 4; ++i)
    {
        auto activity = std::find_if(activities.Defs.begin(), activities.Defs.end(), [](const auto& def) {
            return def.Id == "activity.study_at_home";
        });
        activity->Outcome = "p32_studied_" + std::to_string(i);
        P32Expect(ApplyLearningCredits(budgetOwner, P32Study(budgetDay, varied)).Applied == 1,
            "distinct practice families still consume shared per-skill budget");
    }
    P32Expect(P32Academics(budgetOwner) - P32Academics(baseline) == PracticeSkillDailyBudgetMilli,
        "family cycling cannot exceed daily academic budget");
}

struct P32PaperPair
{
    SchoolWorkRecord Before;
    SchoolWorkRecord Submitted;
    AssessmentMarking Marking;
};

P32PaperPair P32Paper(const P2CFixture& fixture, std::string id, GameInstant at, bool hedge, bool answer = true)
{
    SchoolWorkState work;
    const auto& paper = fixture.Papers.Papers.front();
    SchoolWorkBrief brief{std::move(id), paper.Id, SchoolWorkKind::Homework, at, at + AssessmentPaperMinutes,
        "phase2.test.title", "phase2.test.brief", "phase2.test.criteria", "phase2.test.recovery"};
    P32Expect(IssueSchoolWork(work, brief, fixture.Papers, at).Ok(), "assignment issued through school rules");
    auto skills = LearningWorkSkills(P32Seed(fixture, 4));
    for (auto& skill : skills) skill.Milli = 200000;
    SchoolWorkCommand command;
    command.WorkId = brief.Id;
    P32Expect(ApplySchoolWork(work, fixture.Papers, command, skills, at).Ok(), "paper begins through school rules");
    if (answer)
    {
        for (std::size_t group = 0; group < AssessmentGroupCount; ++group)
        {
            command.Kind = SchoolWorkActionKind::Allocate;
            command.Group = group;
            command.Units = AssessmentClearKnownUnits;
            P32Expect(ApplySchoolWork(work, fixture.Papers, command, skills, at).Ok(), "answer allocation accepted");
            command.Kind = SchoolWorkActionKind::Answer;
            command.Answer = {group, AssessmentMode::Work, false, hedge};
            P32Expect(ApplySchoolWork(work, fixture.Papers, command, skills, at).Ok(), "paper answer accepted");
        }
    }
    P32PaperPair pair;
    pair.Before = work.Records.front();
    command.Kind = SchoolWorkActionKind::Submit;
    const auto done = ApplySchoolWork(work, fixture.Papers, command, skills, at);
    P32Expect(done.Ok() && done.Marking && done.WorldMinutes == AssessmentPaperMinutes, "paper submission accepted");
    pair.Submitted = work.Records.front();
    pair.Marking = *done.Marking;
    return pair;
}

void P32Papers(const P2CFixture& fixture)
{
    const auto* study = fixture.Activities.Find("activity.study_at_home");
    P32Expect(study != nullptr, "study learning policy exists");
    auto state = P32Seed(fixture);
    const auto baseline = state;
    ActivityRunnerState runner;
    PracticeState practice;
    auto paper = P32Paper(fixture, "school.work.p32_first", 0, false);
    const auto applied = ApplySchoolWorkLearning(state, runner, practice, paper.Before, paper.Submitted, *study);
    P32Expect(applied.Ok() && applied.Applied == 1 && applied.Events.size() == 1 &&
        applied.Events.front().ActivityId == paper.Submitted.Brief.Id && runner.Instance == 1 && runner.AppliedInstance == 1,
        "accepted paper becomes a real shared-runner completion");
    P32Expect(P32Academics(state) - P32Academics(baseline) == 1000 && paper.Before == paper.Submitted,
        "paper receives finite effort credit and commits the stable submission marker");
    const auto after = state;
    const auto afterPractice = practice;
    const auto afterRunner = runner;
    const auto replay = ApplySchoolWorkLearning(state, runner, practice, paper.Before, paper.Submitted, *study);
    P32Expect(replay.Ok() && replay.Applied == 0 && replay.Replayed == 1 && replay.Events.empty() &&
        practice == afterPractice && runner == afterRunner, "paper replay consumes no runner ID or practice budget");
    P32SameLearning(state, after);
    P32Expect(ApplyLearningCredits(state, applied.Events).Replayed == 1, "redelivered paper completion is inert at R9");
    P32SameLearning(state, after);
    auto alternative = P32Paper(fixture, "school.work.p32_other", 0, true);
    P32Expect(alternative.Marking.Total != paper.Marking.Total, "equal-effort answers can have different diagnostic marks");
    auto alternativeOwner = baseline;
    ActivityRunnerState alternativeRunner;
    PracticeState alternativePractice;
    P32Expect(ApplySchoolWorkLearning(alternativeOwner, alternativeRunner, alternativePractice,
        alternative.Before, alternative.Submitted, *study).Applied == 1, "second diagnostic outcome credits");
    P32SameLearning(alternativeOwner, after);
    for (std::int32_t household = 0; household <= MaxRank; ++household)
    {
        auto householdOwner = P32Seed(fixture, 0, household);
        ActivityRunnerState householdRunner;
        PracticeState householdPractice;
        auto householdRecord = alternative.Submitted;
        householdRecord.SubmittedAt.reset();
        P32Expect(ApplySchoolWorkLearning(householdOwner, householdRunner, householdPractice,
            householdRecord, alternative.Submitted, *study).Applied == 1,
            "household rank never changes submission credit");
        P32SameLearning(householdOwner, after);
    }
    for (int i = 1; i <= 3; ++i)
    {
        auto next = P32Paper(fixture, "school.work.p32_repeat_" + std::to_string(i), i * AssessmentPaperMinutes, false);
        const auto before = P32Academics(state);
        const auto expected = i == 1 ? 500 : i == 2 ? 100 : 0;
        P32Expect(ApplySchoolWorkLearning(state, runner, practice, next.Before, next.Submitted, *study).Applied == 1 &&
            P32Academics(state) - before == expected, "new assignment IDs share the rolling study practice family");
    }
    auto empty = P32Paper(fixture, "school.work.p32_empty", 400, false, false);
    const auto beforeEmpty = state;
    const auto beforeEmptyPractice = practice;
    P32Expect(ApplySchoolWorkLearning(state, runner, practice, empty.Before, empty.Submitted, *study).Applied == 1 &&
        state.Stats == beforeEmpty.Stats && practice == beforeEmptyPractice, "empty submission consumes identity without learning");
    auto invalid = P32Paper(fixture, "school.work.p32_invalid", 500, false);
    invalid.Submitted.SubmittedAt.reset();
    const auto beforeFailure = state;
    const auto runnerBeforeFailure = runner;
    const auto practiceBeforeFailure = practice;
    const auto recordBeforeFailure = invalid.Before;
    P32Expect(!ApplySchoolWorkLearning(state, runner, practice, invalid.Before, invalid.Submitted, *study).Ok() &&
        runner == runnerBeforeFailure && practice == practiceBeforeFailure && invalid.Before == recordBeforeFailure,
        "invalid submission leaves all transaction participants unchanged");
    P32SameLearning(state, beforeFailure);
    auto changed = P32Paper(fixture, "school.work.p32_changed", 500, false);
    changed.Submitted.Paper.Groups.front().Hedge = true;
    P32Expect(IsAssessmentStateValid(changed.Submitted.Paper) &&
        !ApplySchoolWorkLearning(state, runner, practice, changed.Before, changed.Submitted, *study).Ok() &&
        runner == runnerBeforeFailure && practice == practiceBeforeFailure && !changed.Before.SubmittedAt,
        "submission cannot replace the paid draft with different answers");
    P32SameLearning(state, beforeFailure);
    auto unknown = P32Paper(fixture, "school.work.p32_unknown", 600, false);
    auto brokenStudy = *study;
    brokenStudy.Split.front().SkillId = "skill.missing";
    P32Expect(!ApplySchoolWorkLearning(state, runner, practice, unknown.Before, unknown.Submitted, brokenStudy).Ok() &&
        runner == runnerBeforeFailure && practice == practiceBeforeFailure && !unknown.Before.SubmittedAt,
        "R9 refusal rolls back paper marker, practice and runner allocation");
    P32SameLearning(state, beforeFailure);
    auto resumed = state;
    auto resumedPractice = practice;
    auto resumedRunner = runner;
    auto resumedRecord = paper.Submitted;
    P32Expect(ApplySchoolWorkLearning(resumed, resumedRunner, resumedPractice, resumedRecord, paper.Submitted, *study).Replayed == 1,
        "persisted submission marker keeps resumed and historical papers inert");
    P32SameLearning(resumed, state);
}

void P32SharedPractice(const P2CFixture& fixture)
{
    auto state = P32Seed(fixture);
    auto day = NewDayLoopState();
    const DayLoopCatalogues cats{&fixture.Activities};
    const auto* study = fixture.Activities.Find("activity.study_at_home");
    P32Expect(study != nullptr, "shared study policy exists");
    P32Expect(ApplyLearningCredits(state, P32Study(day, cats)).Applied == 1, "routine precedes paper");
    auto paper = P32Paper(fixture, "school.work.p32_shared", day.Now, false);
    const auto before = P32Academics(state);
    const auto credited = ApplySchoolWorkLearning(state, day.Runner, day.Practice,
        paper.Before, paper.Submitted, *study);
    P32Expect(credited.Ok() && credited.Applied == 1 && credited.Events.front().Instance == 2 &&
        P32Academics(state) - before == 500, "paper and routine share practice window and runner IDs");

    auto budgetOwner = P32Seed(fixture);
    ActivityRunnerState budgetRunner;
    PracticeState budget;
    const auto spent = RecordPractice(budget, {"family.p32_other", {{"skill.academics", 7900}}, 0});
    P32Expect(spent.Ok(), "prior practice spends academic budget");
    auto limited = P32Paper(fixture, "school.work.p32_budget", 0, false);
    const auto baseline = P32Academics(budgetOwner);
    P32Expect(ApplySchoolWorkLearning(budgetOwner, budgetRunner, budget,
        limited.Before, limited.Submitted, *study).Applied == 1 &&
        P32Academics(budgetOwner) - baseline == 100, "paper respects budget already spent in another family");
}
} // namespace

int main(int argc, char** argv)
{
    P32Expect(argc == 2, "content root argument");
    const P2CFixture fixture(argv[1]);
    P32Foundation(fixture);
    P32Lessons(fixture);
    P32Limits(fixture);
    P32Papers(fixture);
    P32SharedPractice(fixture);
    std::cout << "P32 LearningCredits passed\n";
}
