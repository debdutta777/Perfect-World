// Phase 2 order A1: the school-day window. Run in the editor under "PerfectWorld.SchoolDay". The model is built from a hand-made
// portable SchoolDayView and SchoolLessonContent (no content files, no world), the widgets are the real ones built without a window
// (SlatePrepass evaluates bound text and visibility; buttons are clicked with SButton::SimulateClick), and the world is a fake
// IPWSchoolDayWorld. What needs the real core (view rules, closures from the real calendar) is proved by SchoolLessonContentTests.
// NOT RUN by the author: written without an Unreal build; the integrator compiles and runs it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldQuestPanelTestSupport.h"
#include "SchoolLessonContent.h"
#include "UI/SPWSchoolDay.h"

namespace
{
using namespace PerfectWorld;
using namespace QwTests;

/** The portable content an authored catalogue would hold, with a few words each. */
SchoolLessonContent SdContent()
{
    SchoolLessonContent C;
    const auto Add = [&C](const char* Key, const char* Text) { C.Text.emplace(Key, Text); };
    Add("t.title.maths", "Mathematics: stock and flow");
    Add("t.brief.maths", "A quantity gathers or drains over time.");
    Add("t.role.maths", "You usually sit as the solver.");
    Add("t.title.civics", "Political science: the debate");
    Add("t.brief.civics", "A debate needs a speaker and a timekeeper.");
    Add("t.role.civics", "You usually sit as the mediator.");
    Add("t.reason.bell", "A lesson opens at its bell.");
    Add("t.recovery.bell", "Be in the classroom when the bell rings.");
    Add("t.reason.limit", "You have already played two lessons in the foreground today.");
    Add("t.reason.secure", "Routine needs mastery you have not shown yet.");
    Add("t.closure.holiday", "Independence Day. The school is closed.");
    Add("t.absence", "You have been away from school on recent days.");
    Add("t.absence.recovery", "Missed lessons are not replayed. Start with the next period.");
    Add("t.mode.fg.title", "Play it as a foreground lesson");
    Add("t.mode.fg.brief", "You work the lesson yourself.");
    Add("t.mode.rt.title", "Follow it as routine");
    Add("t.mode.rt.brief", "You follow along without the full effort.");
    Add("t.mode.at.title", "Attend only");
    Add("t.mode.at.brief", "You sit through the lesson.");
    Add("t.goal.understand", "Understand the model.");
    Add("t.goal.test", "Test the measurements.");
    Add("t.goal.explain", "Explain it to the group.");
    C.Modes.push_back({SchoolLessonMode::Foreground, "t.mode.fg.title", "t.mode.fg.brief"});
    C.Modes.push_back({SchoolLessonMode::Routine, "t.mode.rt.title", "t.mode.rt.brief"});
    C.Modes.push_back({SchoolLessonMode::Attend, "t.mode.at.title", "t.mode.at.brief"});
    C.Goals.push_back({SchoolStructure::Model, "understand_model", "t.goal.understand"});
    C.Goals.push_back({SchoolStructure::Model, "test_measurements", "t.goal.test"});
    C.Goals.push_back({SchoolStructure::Model, "explain_to_group", "t.goal.explain"});
    return C;
}

SchoolDayRow SdRow(const char* Id, const int32 Period, const char* Subject, const SchoolDayRowStatus Status, const bool bCivics)
{
    SchoolDayRow Row;
    Row.BindingId = Id;
    Row.Period = Period;
    Row.Subject = Subject;
    Row.StartMinute = 620 + Period * 50;
    Row.EndMinute = Row.StartMinute + 45;
    Row.Structure = SchoolStructure::Model;
    Row.TitleTextKey = bCivics ? "t.title.civics" : "t.title.maths";
    Row.BriefTextKey = bCivics ? "t.brief.civics" : "t.brief.maths";
    Row.RoleTextKey = bCivics ? "t.role.civics" : "t.role.maths";
    Row.Status = Status;
    return Row;
}

/** Three periods: one done, one open now (foreground and attend; routine not secure), one still to come. */
SchoolDayView SdSchoolView()
{
    SchoolDayView V;
    V.Stream = "science";
    V.Day = 0;
    V.MinuteOfDay = 670;
    V.SchoolDay = true;
    V.ForegroundUsed = 1;
    SchoolDayRow Done = SdRow("school.lesson.science.p0_mathematics", 0, "mathematics", SchoolDayRowStatus::Done, false);
    Done.DoneMode = SchoolLessonMode::Foreground;
    SchoolDayRow Open = SdRow("school.lesson.science.p1_political_science", 1, "political_science", SchoolDayRowStatus::Open, true);
    Open.ForegroundAvailable = true;
    Open.AttendAvailable = true;
    Open.RoutineReasonTextKey = "t.reason.secure";
    SchoolDayRow Later = SdRow("school.lesson.science.p2_chemistry", 2, "chemistry", SchoolDayRowStatus::Upcoming, false);
    Later.ReasonTextKey = "t.reason.bell";
    Later.RecoveryTextKey = "t.recovery.bell";
    V.Rows = {Done, Open, Later};
    return V;
}

SchoolDayView SdClosedView()
{
    SchoolDayView V;
    V.Stream = "science";
    V.Day = 74;
    V.MinuteOfDay = 600;
    V.Kind = SchoolDayKind::Holiday;
    V.SchoolDay = false;
    V.ClosureTextKey = "t.closure.holiday";
    V.NextSchoolDay = 77;
    return V;
}

class FSdFakeWorld final : public IPWSchoolDayWorld
{
public:
    virtual bool ReadModel(FPWSchoolDayModel& OutModel, FString& OutWhy) override
    {
        ++Reads;
        if (!bReadable)
        {
            OutWhy = TEXT("no world");
            return false;
        }
        OutModel = Model;
        return true;
    }
    virtual FText OpenLesson(const FString& BindingId, const SchoolLessonMode Mode, const FString& GoalName) override
    {
        ++Opens;
        LastBinding = BindingId;
        LastMode = Mode;
        LastGoal = GoalName;
        return Refusal;
    }

    FPWSchoolDayModel Model;
    FText Refusal;
    bool bReadable = true;
    int32 Reads = 0;
    int32 Opens = 0;
    FString LastBinding;
    FString LastGoal;
    SchoolLessonMode LastMode = SchoolLessonMode::Attend;
};

TSharedRef<FSdFakeWorld> SdWorld(const SchoolDayView& View)
{
    const TSharedRef<FSdFakeWorld> World = MakeShared<FSdFakeWorld>();
    World->Model = PWSchoolDay::BuildModel(View, SdContent());
    return World;
}

TSharedRef<SPWSchoolDay> SdPanel(const TSharedRef<FSdFakeWorld>& World, const FVector2D& Space, int32* Closed = nullptr, int32* Opened = nullptr)
{
    return SNew(SPWSchoolDay)
        .World(World)
        .Space(Space)
        .OnClose(FSimpleDelegate::CreateLambda([Closed]() { if (Closed != nullptr) ++*Closed; }))
        .OnOpened(FSimpleDelegate::CreateLambda([Opened]() { if (Opened != nullptr) ++*Opened; }));
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSdWordsTest, "PerfectWorld.SchoolDay.WordsNeverShowKeys",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSdWordsTest::RunTest(const FString& Parameters)
{
    const SchoolLessonContent Content = SdContent();
    TestEqual(TEXT("an authored key reads its words"), PWSchoolDay::Words(Content, "t.title.maths").ToString(), FString(TEXT("Mathematics: stock and flow")));
    for (const char* Unknown : {"", "t.no.such.key", "phase2.a1.lesson.science.p0_mathematics.title"})
    {
        const FString Text = PWSchoolDay::Words(Content, Unknown).ToString();
        TestFalse(TEXT("an unknown key reads neutral text"), Text.IsEmpty());
        TestFalse(TEXT("and never the key"), Text.Contains(TEXT("phase2")) || Text.Contains(TEXT("t.no")) || Text.Contains(TEXT("_")));
    }
    TestEqual(TEXT("time of day"), PWSchoolDay::TimeText(625).ToString(), FString(TEXT("10:25")));
    TestEqual(TEXT("time of day is clamped"), PWSchoolDay::TimeText(5000).ToString(), FString(TEXT("23:59")));
    TestEqual(TEXT("the first Monday"), PWSchoolDay::ClockText(0, 620).ToString(), FString(TEXT("Monday 2042-06-02, 10:20")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSdModelTest, "PerfectWorld.SchoolDay.ModelFormatsTheProjection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSdModelTest::RunTest(const FString& Parameters)
{
    const FPWSchoolDayModel Model = PWSchoolDay::BuildModel(SdSchoolView(), SdContent());
    TestTrue(TEXT("available"), Model.bAvailable && Model.bSchoolDay);
    if (!TestEqual(TEXT("one row per period"), Model.Rows.Num(), 3)) return false;
    TestEqual(TEXT("the foreground limit line"), Model.ForegroundLine.ToString(), FString(TEXT("Foreground lessons today: 1 of 2")));
    TestEqual(TEXT("a subject token reads as a name"), Model.Rows[1].Subject.ToString(), FString(TEXT("Political science")));
    TestEqual(TEXT("the lesson window"), Model.Rows[0].TimeLabel.ToString(), FString(TEXT("10:20 to 11:05")));
    TestEqual(TEXT("an open period is open"), Model.Rows[1].StatusLabel.ToString(), FString(TEXT("Open now")));
    TestEqual(TEXT("only the open period carries goal chips"), Model.Rows[1].Goals.Num(), 3);
    TestEqual(TEXT("the first goal is the runner's first"), Model.Rows[1].Goals[0].Name, FString(TEXT("understand_model")));
    TestEqual(TEXT("no chips on a done period"), Model.Rows[0].Goals.Num(), 0);
    TestTrue(TEXT("routine says why not"), Model.Rows[1].RoutineReason.ToString().Contains(TEXT("mastery")));
    TestTrue(TEXT("a waiting period explains and recovers"), !Model.Rows[2].Reason.IsEmpty() && !Model.Rows[2].Recovery.IsEmpty());
    TestEqual(TEXT("the open period is where the choice starts"), Model.StartingRow()->BindingId, FString(TEXT("school.lesson.science.p1_political_science")));
    TestEqual(TEXT("mode words"), Model.ModeTitle[2].ToString(), FString(TEXT("Attend only")));

    const FPWSchoolDayModel Closed = PWSchoolDay::BuildModel(SdClosedView(), SdContent());
    TestTrue(TEXT("a closed day is available but not a school day"), Closed.bAvailable && !Closed.bSchoolDay && Closed.Rows.IsEmpty());
    TestTrue(TEXT("with its closure and the next school day"), Closed.Closure.ToString().Contains(TEXT("Independence")) && Closed.NextSchoolDay.ToString().Contains(TEXT("2042-")));

    SchoolDayView Broken;
    Broken.Issue = "school.day.no_timetable";
    TestFalse(TEXT("an unreadable view is unavailable"), PWSchoolDay::BuildModel(Broken, SdContent()).bAvailable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSdPanelTest, "PerfectWorld.SchoolDay.PanelShowsChoicesAndCallsTheWorld",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSdPanelTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FSdFakeWorld> World = SdWorld(SdSchoolView());
    int32 Closed = 0, Opened = 0;
    const TSharedRef<SPWSchoolDay> Panel = SdPanel(World, FVector2D(1280.0, 720.0), &Closed, &Opened);
    TestTrue(TEXT("the wide window shows two panes"), Panel->IsTwoPane());
    TestEqual(TEXT("one row per period"), QwCountTag(Panel, PWSchoolDay::RowTag), 3);
    TestEqual(TEXT("the open period is chosen"), Panel->GetSelectedId(), FString(TEXT("school.lesson.science.p1_political_science")));
    const TArray<FString> Texts = QwShownTexts(Panel);
    TestTrue(TEXT("the chosen lesson is described"), QwHas(Texts, TEXT("Political science: the debate")) && QwHas(Texts, TEXT("You usually sit as the mediator.")));
    TestEqual(TEXT("three ways to spend the period"), QwCountTag(Panel, PWSchoolDay::ModeTag), 3);
    TestEqual(TEXT("three goals for the foreground lesson"), QwCountTag(Panel, PWSchoolDay::GoalTag), 3);
    TestTrue(TEXT("why routine is not offered is written down"), QwHas(Texts, TEXT("Routine needs mastery you have not shown yet.")));
    TestTrue(TEXT("the foreground count is shown"), QwHas(Texts, TEXT("Foreground lessons today: 1 of 2")));

    // Choosing a goal and playing it sends the runner's goal name for exactly this binding.
    TestTrue(TEXT("a goal chip can be clicked"), QwClick(Panel, TEXT("Test the measurements.")));
    TestTrue(TEXT("foreground opens"), QwClick(Panel, TEXT("Play it as a foreground lesson")));
    TestEqual(TEXT("the world was asked once"), World->Opens, 1);
    TestEqual(TEXT("for this binding"), World->LastBinding, FString(TEXT("school.lesson.science.p1_political_science")));
    TestTrue(TEXT("in foreground mode"), World->LastMode == SchoolLessonMode::Foreground);
    TestEqual(TEXT("with the chosen goal"), World->LastGoal, FString(TEXT("test_measurements")));
    TestEqual(TEXT("and the host was told"), Opened, 1);

    // Attend takes no goal.
    TestTrue(TEXT("attend opens"), QwClick(Panel, TEXT("Attend only")));
    TestTrue(TEXT("attend mode with no goal"), World->LastMode == SchoolLessonMode::Attend && World->LastGoal.IsEmpty());

    // A mode the view does not offer is a disabled button: clicking it asks nothing.
    TestFalse(TEXT("routine is not offered"), Panel->RequestOpen(SchoolLessonMode::Routine));
    TestEqual(TEXT("and the world was not asked for it"), World->Opens, 2);

    // Keys: Up and Down choose, the ends fall through, Escape closes once.
    TestTrue(TEXT("Up chooses the previous period"), Panel->OnKeyDown(FGeometry(), QwKey(EKeys::Up)).IsEventHandled());
    TestEqual(TEXT("the done period"), Panel->GetSelectedId(), FString(TEXT("school.lesson.science.p0_mathematics")));
    TestFalse(TEXT("Up from the first period is left to the focus navigation"), Panel->OnKeyDown(FGeometry(), QwKey(EKeys::Up)).IsEventHandled());
    Panel->OnKeyDown(FGeometry(), QwKey(EKeys::Down));
    Panel->OnKeyDown(FGeometry(), QwKey(EKeys::Down));
    TestEqual(TEXT("the waiting period"), Panel->GetSelectedId(), FString(TEXT("school.lesson.science.p2_chemistry")));
    const TArray<FString> Waiting = QwShownTexts(Panel);
    TestTrue(TEXT("a waiting period says when it opens"), QwHas(Waiting, TEXT("A lesson opens at its bell.")) && QwHas(Waiting, TEXT("Be in the classroom when the bell rings.")));
    TestEqual(TEXT("and offers no modes"), QwCountTag(Panel, PWSchoolDay::ModeTag), 0);
    TestTrue(TEXT("Escape closes"), Panel->OnKeyDown(FGeometry(), QwKey(EKeys::Escape)).IsEventHandled());
    Panel->OnKeyDown(FGeometry(), QwKey(EKeys::Escape, true));
    TestEqual(TEXT("once, however long it is held"), Closed, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSdRefusalTest, "PerfectWorld.SchoolDay.ACoreRefusalIsShownNotHidden",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSdRefusalTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FSdFakeWorld> World = SdWorld(SdSchoolView());
    World->Refusal = FText::FromString(TEXT("The bell has gone."));
    int32 Opened = 0;
    const TSharedRef<SPWSchoolDay> Panel = SdPanel(World, FVector2D(1280.0, 720.0), nullptr, &Opened);
    const int32 ReadsBefore = World->Reads;
    TestFalse(TEXT("the open is refused"), Panel->RequestOpen(SchoolLessonMode::Attend));
    TestEqual(TEXT("the host is not told it opened"), Opened, 0);
    TestTrue(TEXT("the world is read again so the view is current"), World->Reads > ReadsBefore);
    TestEqual(TEXT("the refusal is kept"), Panel->GetNotice().ToString(), FString(TEXT("The bell has gone.")));
    TestTrue(TEXT("and shown in the window"), QwHas(QwShownTexts(Panel), TEXT("The bell has gone.")));
    TestEqual(TEXT("the choice stays on the same period"), Panel->GetSelectedId(), FString(TEXT("school.lesson.science.p1_political_science")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSdStatesTest, "PerfectWorld.SchoolDay.ClosedAbsentAndUnreadableStates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSdStatesTest::RunTest(const FString& Parameters)
{
    {
        const TSharedRef<SPWSchoolDay> Panel = SdPanel(SdWorld(SdClosedView()), FVector2D(1280.0, 720.0));
        TestEqual(TEXT("a closed day shows its closure"), QwCountTag(Panel, PWSchoolDay::ClosureTag), 1);
        TestEqual(TEXT("and no periods"), QwCountTag(Panel, PWSchoolDay::RowTag), 0);
        TestTrue(TEXT("in words"), QwHas(QwShownTexts(Panel), TEXT("Independence Day. The school is closed.")));
    }
    {
        SchoolDayView View = SdSchoolView();
        View.RecentAbsentDays = 2;
        View.AbsenceTextKey = "t.absence";
        View.AbsenceRecoveryTextKey = "t.absence.recovery";
        const TSharedRef<SPWSchoolDay> Panel = SdPanel(SdWorld(View), FVector2D(1280.0, 720.0));
        TestEqual(TEXT("recent absences show the recovery note"), QwCountTag(Panel, PWSchoolDay::AbsenceTag), 1);
        TestTrue(TEXT("with what to do"), QwHas(QwShownTexts(Panel), TEXT("Missed lessons are not replayed. Start with the next period.")));
        TestEqual(TEXT("and the periods are still there"), QwCountTag(Panel, PWSchoolDay::RowTag), 3);
    }
    {
        const TSharedRef<FSdFakeWorld> World = MakeShared<FSdFakeWorld>();
        World->bReadable = false;
        const TSharedRef<SPWSchoolDay> Panel = SdPanel(World, FVector2D(1280.0, 720.0));
        TestEqual(TEXT("an unreadable world says so"), QwCountTag(Panel, PWSchoolDay::UnavailableTag), 1);
        TestFalse(TEXT("and offers nothing to open"), Panel->RequestOpen(SchoolLessonMode::Attend));
    }
    {
        const TSharedRef<SPWSchoolDay> Narrow = SdPanel(SdWorld(SdSchoolView()), FVector2D(600.0, 700.0));
        TestFalse(TEXT("a narrow window stacks the panes"), Narrow->IsTwoPane());
        TestEqual(TEXT("with every period still listed"), QwCountTag(Narrow, PWSchoolDay::RowTag), 3);
        TestTrue(TEXT("and the modes still offered"), QwCountTag(Narrow, PWSchoolDay::ModeTag) == 3);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
