// Order P3-05: the planner's known commitments and the study desk's practice offers. Run in the editor under "PerfectWorld.P35".
// Everything here is a pure reader over a hand-made WorldState and ActivityCatalogue, so no map, content database or subsystem is needed.
// What these cannot prove: the routed practice commands (day.apply commit, start, complete) and the planner/desk widgets on the live world.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "GameClock.h"
#include "PerfectWorldSchoolOffers.h"
#include "UI/SPWSchoolPlanner.h"

namespace P35Tests
{
using namespace PerfectWorld;
using namespace PWSchoolLive;

constexpr int32 P35PlanMinute = 17 * 60;

/** The two activities the tests need, mirroring Data/Content/activity (sorted by Id). */
inline ActivityCatalogue P35Catalogue()
{
    ActivityCatalogue Catalogue;
    ActivityDef Chores;
    Chores.Id = "activity.household_chores";
    Chores.Name = "Household chores";
    Chores.DurationMinutes = 60;
    Chores.Place = "home";
    Chores.Condition = ConditionActivity::LightExertion;
    Chores.Outcome = "chore_done";
    ActivityDef Study;
    Study.Id = "activity.study_at_home";
    Study.Name = "Study at home";
    Study.DurationMinutes = 120;
    Study.Place = "home";
    Study.Condition = ConditionActivity::Awake;
    Study.Outcome = "studied";
    Study.AllowanceMilliXp = 3000;
    Study.Split = {{"skill.academics", 800}, {"skill.self_management", 200}};
    Catalogue.Defs = {Chores, Study};
    return Catalogue;
}

inline WorldState P35Planned(const std::vector<DayPlanItem>& Plan)
{
    WorldState State = MakeWorldState(7);
    State.DayLoop.Planned = true;
    State.DayLoop.Plan = Plan;
    return State;
}

inline bool P35LooksRaw(const FString& Words)
{
    for (const TCHAR* Part : {TEXT("quest."), TEXT("activity."), TEXT("source."), TEXT("school."), TEXT("{"), TEXT("_")})
    {
        if (Words.Contains(Part)) return true;
    }
    return false;
}
} // namespace P35Tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP35KnownTest, "PerfectWorld.P35.KnownCommitmentsAreOnlyWhatTheWorldRecords",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP35KnownTest::RunTest(const FString& Parameters)
{
    using namespace P35Tests;
    const ActivityCatalogue Catalogue = P35Catalogue();

    // A fresh life and an empty plan promise nothing.
    TestEqual(TEXT("a fresh world has no known commitments"), static_cast<int32>(P35KnownCommitments(MakeWorldState(7), Catalogue).Candidates.size()), 0);
    TestEqual(TEXT("an empty plan has none"), static_cast<int32>(P35KnownCommitments(P35Planned({}), Catalogue).Candidates.size()), 0);

    // A planned study session this evening is a real promise with a window, and its line names it with its worth and cost.
    const WorldState Planned = P35Planned({{P35PlanMinute, "activity.study_at_home"}});
    const FP35Known Known = P35KnownCommitments(Planned, Catalogue);
    if (!TestEqual(TEXT("one open plan item gives one candidate"), static_cast<int32>(Known.Candidates.size()), 1)) return false;
    const ReceiptCandidate& Candidate = Known.Candidates.front();
    TestEqual(TEXT("its window starts at the planned minute of today"), Candidate.Start, GameClock::StartOfDay(Planned.Clock.Now()) + P35PlanMinute);
    TestEqual(TEXT("its length is the activity's own"), Candidate.DurationMinutes, 120);
    TestTrue(TEXT("it was known no later than now"), Candidate.KnownAt <= Planned.Clock.Now());
    TestTrue(TEXT("the core's forecast gives it a real learning worth"), Candidate.Forecast.LearningMilliXp > 0);
    const FText* Line = Known.Lines.Find(FString(UTF8_TO_TCHAR(Candidate.Id.c_str())));
    if (!TestNotNull(TEXT("it has a readable line"), Line)) return false;
    const FString Words = Line->ToString();
    TestTrue(TEXT("the line names the activity"), Words.Contains(TEXT("Study at home")));
    TestTrue(TEXT("the line says when"), Words.Contains(TEXT("17:00")));
    TestTrue(TEXT("the line shows the worth"), Words.Contains(TEXT("learning")));
    TestFalse(TEXT("the line shows no key, code or template brace"), P35LooksRaw(Words));

    // A plan item the player already did is gone (the core's quest log matches plans to receipts).
    WorldState Done = Planned;
    ReceiptEntry Entry;
    Entry.Seq = 1;
    Entry.ActivityId = "activity.study_at_home";
    Entry.DecidedAt = Candidate.Start;
    Entry.Start = Candidate.Start;
    Entry.DurationMinutes = 120;
    Done.DayLoop.Receipts.Entries.push_back(Entry);
    TestEqual(TEXT("a plan item with a receipt today is not an open promise"), static_cast<int32>(P35KnownCommitments(Done, Catalogue).Candidates.size()), 0);

    // An activity the player has committed to is one promise; the plan item it carries out is not counted twice.
    WorldState Committed = P35Planned({{400, "activity.household_chores"}});
    Committed.DayLoop.HasCommitment = true;
    Committed.DayLoop.Commitment.ActivityId = "activity.household_chores";
    Committed.DayLoop.Commitment.PlaceKind = "home";
    Committed.DayLoop.Commitment.DecidedAt = Committed.Clock.Now();
    const FP35Known Carrying = P35KnownCommitments(Committed, Catalogue);
    if (!TestEqual(TEXT("a commitment and its own plan item count once"), static_cast<int32>(Carrying.Candidates.size()), 1)) return false;
    TestEqual(TEXT("it is the commitment"), FString(UTF8_TO_TCHAR(Carrying.Candidates.front().Id.c_str())), FString(TEXT("day_commitment.activity.household_chores")));

    // A lesson in progress is the school's own business and is never an alternative.
    WorldState Lesson = P35Planned({});
    Lesson.DayLoop.HasCommitment = true;
    Lesson.DayLoop.Commitment.ActivityId = "activity.attend_class";
    ActivityCatalogue WithClass = Catalogue;
    ActivityDef ClassDef;
    ClassDef.Id = "activity.attend_class";
    ClassDef.Name = "Class";
    ClassDef.DurationMinutes = 45;
    ClassDef.Place = "classroom";
    WithClass.Defs.insert(WithClass.Defs.begin(), ClassDef);
    TestEqual(TEXT("an attended lesson is not a known alternative"), static_cast<int32>(P35KnownCommitments(Lesson, WithClass).Candidates.size()), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP35PlannerLineTest, "PerfectWorld.P35.ThePlannerNamesTheRealPromiseItPassesOver",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP35PlannerLineTest::RunTest(const FString& Parameters)
{
    using namespace P35Tests;
    const ActivityCatalogue Catalogue = P35Catalogue();
    const WorldState State = P35Planned({{P35PlanMinute, "activity.study_at_home"}});
    FP35Known Known = P35KnownCommitments(State, Catalogue);
    if (!TestEqual(TEXT("one known promise"), static_cast<int32>(Known.Candidates.size()), 1)) return false;

    // A one-entry plan whose chores overlap the planned study session by 30 minutes.
    FPWPlannerPreview Preview;
    Preview.Now = State.Clock.Now();
    Preview.Activities = Catalogue;
    SchoolWeekPlan& Plan = Preview.Build.Plan;
    Plan.Week.Days.resize(1);
    WeekEntry Chores;
    Chores.Kind = WeekEntryKind::Activity;
    Chores.StartMinute = P35PlanMinute + 90;
    Chores.ActivityId = "activity.household_chores";
    Chores.PlaceKind = "home";
    Plan.Week.Days[0].Entries = {Chores};

    const FPWPlannerModel Without = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
    if (!TestTrue(TEXT("the plan builds"), Without.bPlanOk && Without.Days.Num() == 1 && Without.Days[0].Entries.Num() == 1)) return false;
    TestEqual(TEXT("with no known promises nothing is passed over"), Without.Days[0].Entries[0].Displaced.Num(), 0);

    Preview.Known = Known.Candidates;
    Preview.KnownNames = Known.Lines;
    const FPWPlannerModel With = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
    if (!TestTrue(TEXT("the plan still builds"), With.bPlanOk && With.Days.Num() == 1 && With.Days[0].Entries.Num() == 1)) return false;
    const TArray<FPWDisplacedRow>& Displaced = With.Days[0].Entries[0].Displaced;
    if (!TestEqual(TEXT("the known promise is named"), Displaced.Num(), 1)) return false;
    TestEqual(TEXT("with the overlap the core computed"), Displaced[0].OverlapMinutes, 30);
    TestTrue(TEXT("and the real name and cost"), Displaced[0].Name.ToString().Contains(TEXT("Study at home")) && Displaced[0].Name.ToString().Contains(TEXT("learning")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP35PracticeTest, "PerfectWorld.P35.PracticeIsOfferedOnlyWithCostsAndHonestReasons",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP35PracticeTest::RunTest(const FString& Parameters)
{
    using namespace P35Tests;
    const ActivityCatalogue Catalogue = P35Catalogue();

    TestEqual(TEXT("no catalogue, no offer"), P35PracticeOffers(MakeWorldState(7), nullptr).Num(), 0);
    const ActivityCatalogue Empty;
    TestEqual(TEXT("a catalogue without the study activity gives no offer"), P35PracticeOffers(MakeWorldState(7), &Empty).Num(), 0);

    const WorldState Awake = MakeWorldState(7);
    const TArray<PWA5::FPracticeOffer> Offers = P35PracticeOffers(Awake, &Catalogue);
    if (!TestEqual(TEXT("one offer"), Offers.Num(), 1)) return false;
    TestEqual(TEXT("it is the existing study activity"), FString(UTF8_TO_TCHAR(Offers[0].Id.c_str())), FString(TEXT("activity.study_at_home")));
    TestTrue(TEXT("an awake, idle player can start it"), Offers[0].bAvailable);
    const FString Label = Offers[0].Label.ToString();
    const FString Detail = Offers[0].Detail.ToString();
    TestTrue(TEXT("the label names it with its length"), Label.Contains(TEXT("Study at home")) && Label.Contains(TEXT("120")));
    TestTrue(TEXT("the detail shows the learning worth"), Detail.Contains(TEXT("learning")));
    TestTrue(TEXT("the detail shows the energy and hunger cost"), Detail.Contains(TEXT("energy and hunger")));
    TestTrue(TEXT("the detail shows the time"), Detail.Contains(TEXT("120 minutes")));
    TestFalse(TEXT("the detail shows no key or code"), P35LooksRaw(Detail));
    TestFalse(TEXT("nothing is passed over when nothing is promised"), Detail.Contains(TEXT("passes over")));

    // A promise in the window is named as passed over, and is the alternative a commit would record.
    const WorldState Promised = P35Planned({{400, "activity.household_chores"}});
    const TArray<PWA5::FPracticeOffer> Passing = P35PracticeOffers(Promised, &Catalogue);
    if (!TestEqual(TEXT("still one offer"), Passing.Num(), 1)) return false;
    TestTrue(TEXT("the offer names what it passes over"), Passing[0].Detail.ToString().Contains(TEXT("passes over")) && Passing[0].Detail.ToString().Contains(TEXT("Household chores")));
    const std::vector<ReceiptCandidate> Alternatives = P35PracticeAlternatives(Promised, Catalogue, *Catalogue.Find("activity.study_at_home"));
    TestEqual(TEXT("the commit's alternatives are those known promises"), static_cast<int32>(Alternatives.size()), 1);

    // Blocked, never actionable, with the reason in words.
    WorldState Asleep = MakeWorldState(7);
    Asleep.DayLoop.Asleep = true;
    const TArray<PWA5::FPracticeOffer> Sleeping = P35PracticeOffers(Asleep, &Catalogue);
    if (!TestEqual(TEXT("asleep: the offer is still shown"), Sleeping.Num(), 1)) return false;
    TestFalse(TEXT("but not actionable"), Sleeping[0].bAvailable);
    TestTrue(TEXT("and says why"), Sleeping[0].Detail.ToString().Contains(TEXT("asleep")));

    WorldState Paper = MakeWorldState(7);
    SchoolWorkRecord Record;
    Record.Brief.Id = "school.work.p35";
    Record.StartedAt = 0;
    Paper.SchoolWork.Records.push_back(Record);
    const TArray<PWA5::FPracticeOffer> Open = P35PracticeOffers(Paper, &Catalogue);
    if (!TestEqual(TEXT("an open paper: the offer is shown"), Open.Num(), 1)) return false;
    TestFalse(TEXT("but not actionable"), Open[0].bAvailable);
    TestTrue(TEXT("and says why"), Open[0].Detail.ToString().Contains(TEXT("paper")));

    WorldState Busy = MakeWorldState(7);
    Busy.DayLoop.Planned = true;
    Busy.DayLoop.HasCommitment = true;
    Busy.DayLoop.Commitment.ActivityId = "activity.household_chores";
    const TArray<PWA5::FPracticeOffer> Occupied = P35PracticeOffers(Busy, &Catalogue);
    if (!TestEqual(TEXT("already committed: the offer is shown"), Occupied.Num(), 1)) return false;
    TestFalse(TEXT("but not actionable"), Occupied[0].bAvailable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP35GroupContextTest, "PerfectWorld.P35.TheGroupPromptIsPublicContextOnly",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP35GroupContextTest::RunTest(const FString& Parameters)
{
    using namespace P35Tests;
    AssessmentCatalogue Papers;
    AssessmentPaperDef Paper;
    Paper.Id = "assessment_paper.p35";
    AssessmentQuestion Question;
    Question.GroupId = "group.one";
    Question.Name = "Pipe flow";
    Question.Structure = AssessmentStructure::Model;
    Question.CaseId = "case.p35";
    Question.Trap = true; // never to appear
    Paper.Questions.push_back(Question);
    Papers.Papers.push_back(Paper);

    ModelCatalogue Models;
    TestEqual(TEXT("with no authored brief the group's own name shows"), P35GroupContext(Papers, Models, "assessment_paper.p35", 0).ToString(), FString(TEXT("Pipe flow")));

    ModelFamilyDef Family;
    Family.Id = "family.p35";
    ModelCaseDef Case;
    Case.Id = "case.p35";
    Case.Brief = "A tank fills while a pipe drains it.";
    Family.Cases.push_back(Case);
    Models.Families.push_back(Family);
    const FString Words = P35GroupContext(Papers, Models, "assessment_paper.p35", 0).ToString();
    TestTrue(TEXT("the model case's authored brief is added"), Words.Contains(TEXT("Pipe flow")) && Words.Contains(TEXT("A tank fills")));
    TestFalse(TEXT("nothing of the trap or marking"), Words.Contains(TEXT("rap")) || Words.Contains(TEXT("true")));

    TestTrue(TEXT("an unknown paper gives nothing, so the panel hides the line"), P35GroupContext(Papers, Models, "assessment_paper.none", 0).IsEmpty());
    TestTrue(TEXT("a group past the end gives nothing"), P35GroupContext(Papers, Models, "assessment_paper.p35", 5).IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
