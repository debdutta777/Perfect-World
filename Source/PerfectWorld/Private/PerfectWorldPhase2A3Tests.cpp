// Order A3: the Investigate and Coordinate lesson screens (SPWClassInvestigate, SPWClassCoordinate) over the frozen SchoolSessions contract.
// Run in the editor under "PerfectWorld.Phase2.A3". The widgets are the real ones, built without a window; the core is the real portable
// SchoolSessions over the real Data/Content catalogues, reached through PWClassA3::FCoreLessonBridge. What needs a real window (focus
// travel, the pause menu hosting) is for the integrator's acceptance scenario, not here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "ContentRegistry.h"
#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/SPWClassCoordinate.h"
#include "UI/SPWClassInvestigate.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
using namespace PerfectWorld;

/** The real content, the catalogues the sessions borrow, and a morning at the first bell. Not movable: the catalogues point into it. */
struct FA3TstFixture
{
    ContentDb Db;
    ActivityCatalogue Activities;
    BellScheduleSet Bells;
    SchoolCalendarSet Calendars;
    ModelCatalogue Model;
    ExplainCatalogue Explain;
    InvestigateCatalogue Investigate;
    CoordinateCatalogue Coordinate;
    SchoolSessionCatalogues Cats;

    FA3TstFixture() = default;
    FA3TstFixture(const FA3TstFixture&) = delete;
    FA3TstFixture& operator=(const FA3TstFixture&) = delete;

    bool Init(FAutomationTestBase& Test)
    {
        const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Content")));
        TArray<FString> Paths;
        IFileManager::Get().FindFilesRecursive(Paths, *Root, TEXT("*.json"), true, false);
        Paths.Sort();
        std::vector<ContentFile> Files;
        for (FString Path : Paths)
        {
            FPaths::NormalizeFilename(Path);
            FString Text;
            if (!FFileHelper::LoadFileToString(Text, *Path))
            {
                Test.AddError(FString::Printf(TEXT("cannot read %s"), *Path));
                return false;
            }
            Files.push_back({std::string(TCHAR_TO_UTF8(*Path)), std::string(TCHAR_TO_UTF8(*Text))});
        }
        ContentLoadResult Loaded = LoadContent(Files);
        if (Paths.IsEmpty() || !Loaded.Ok())
        {
            Test.AddError(TEXT("the content does not load"));
            return false;
        }
        Db = std::move(Loaded.Db);
        auto A = LoadActivityCatalogue(Db);
        auto B = LoadBellSchedules(Db);
        auto C = LoadSchoolCalendars(Db);
        auto M = LoadModelCatalogue(Db);
        auto E = LoadExplainCatalogue(Db);
        auto I = LoadInvestigateCatalogue(Db);
        auto K = LoadCoordinateCatalogue(Db);
        if (!A.Ok() || !B.Ok() || !C.Ok() || !M.Ok() || !E.Ok() || !I.Ok() || !K.Ok())
        {
            Test.AddError(TEXT("a catalogue does not load"));
            return false;
        }
        Activities = std::move(A.Catalogue);
        Bells = std::move(B.Set);
        Calendars = std::move(C.Set);
        Model = std::move(M.Catalogue);
        Explain = std::move(E.Catalogue);
        Investigate = std::move(I.Catalogue);
        Coordinate = std::move(K.Catalogue);
        const auto Bell = std::find_if(Bells.Schedules.begin(), Bells.Schedules.end(), [](const BellSchedule& S) { return S.Stream == "science"; });
        if (Bell == Bells.Schedules.end() || Calendars.Find(Bell->InstitutionId, Bell->StageId) == nullptr)
        {
            Test.AddError(TEXT("no science bell schedule or calendar"));
            return false;
        }
        Cats = {{&Activities, &*Bell, Calendars.Find(Bell->InstitutionId, Bell->StageId)}, {&Model, &Explain, &Investigate, &Coordinate}};
        return true;
    }

    SchoolLessonBinding Binding(const SchoolStructure Structure, const int32 Period = 0) const
    {
        SchoolLessonBinding Out;
        int32 Index = 0;
        for (const auto& Block : Cats.Day.School->Blocks)
        {
            if (Block.Kind != BellBlockKind::Period || Index++ != Period) continue;
            Out.Subject = Block.Subject;
        }
        Out.Id = "school.lesson.a3_test";
        Out.Stream = "science";
        Out.Period = Period;
        Out.Structure = Structure;
        Out.FamilyId = Structure == SchoolStructure::Investigate ? Investigate.Families.front().Id : Coordinate.Families.front().Id;
        Out.TitleTextKey = "phase2.a3.test.title";
        Out.BriefTextKey = "phase2.a3.test.brief";
        Out.RoleTextKey = "phase2.a3.test.role";
        return Out;
    }

    /** A planned morning, signed in, waiting at the bell of the period. */
    DayLoopState DayAtBell(const int32 Period = 0) const
    {
        const GameInstant Midnight = CalendarDayStartInstant(0);
        DayLoopState State = NewDayLoopState(std::max<GameInstant>(0, Midnight + BellFirstBell(*Cats.Day.School)));
        DayCommand Command;
        Command.Kind = DayCommandKind::PlanDay;
        ApplyDayCommand(State, Cats.Day, Command);
        Command.Kind = DayCommandKind::ArriveSchool;
        ApplyDayCommand(State, Cats.Day, Command);
        int32 Index = 0;
        for (const auto& Block : Cats.Day.School->Blocks)
        {
            if (Block.Kind != BellBlockKind::Period || Index++ != Period) continue;
            if (Midnight + Block.StartMinute > State.Now)
            {
                Command.Kind = DayCommandKind::Pass;
                Command.Minutes = Midnight + Block.StartMinute - State.Now;
                ApplyDayCommand(State, Cats.Day, Command);
            }
        }
        return State;
    }
};

void A3TstWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
    {
        A3TstWalk(Children->GetChildAt(Index), bShown, Visit);
    }
}

TArray<FString> A3TstShownTexts(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    TArray<FString> Texts;
    A3TstWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock"))
        {
            Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        }
    });
    return Texts;
}

bool A3TstShows(const TArray<FString>& Texts, const FString& Part)
{
    return Texts.ContainsByPredicate([&Part](const FString& Text) { return Text.Contains(Part); });
}

int32 A3TstCountTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    Root->SlatePrepass(1.0f);
    int32 Count = 0;
    A3TstWalk(Root, true, [&Count, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTag() == FName(Tag)) ++Count;
    });
    return Count;
}

FKeyEvent A3TstKey(const FKey Key, const bool bRepeat = false)
{
    return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
}

/** A bridge that refuses everything the core could, to prove a screen only reports. */
class FA3TstRefusingBridge final : public PWClassA3::ILessonBridge
{
public:
    SchoolLessonBinding Binding;
    SchoolSessionCatalogues Cats;
    int32 Opens = 0;

    virtual const SchoolLessonBinding& GetBinding() const override { return Binding; }
    virtual const SchoolSessionCatalogues& GetCatalogues() const override { return Cats; }
    virtual SchoolPeriodView QueryPeriod() const override
    {
        SchoolPeriodView View;
        View.Period = 0;
        View.StartMinute = 600;
        View.EndMinute = 645;
        View.AttendAvailable = true;
        View.ForegroundAvailable = true;
        return View;
    }
    virtual SchoolLessonView QueryLesson() const override { return {}; }
    virtual SchoolSessionResult OpenLesson(SchoolLessonMode, const SchoolLessonGoal&) override
    {
        ++Opens;
        SchoolSessionResult Result;
        Result.Issue = {"school.session.busy", {}};
        return Result;
    }
    virtual SchoolLessonQuote QuoteAction(uint64, const SchoolLessonAction&) const override { return {}; }
    virtual SchoolSessionResult ChooseAction(uint64, const SchoolLessonAction&) override { return {}; }
    virtual SchoolSessionResult CloseLesson(uint64, bool, int32) override { return {}; }
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA3TstWordsTest, "PerfectWorld.Phase2.A3.Words", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA3TstWordsTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("a phone-sized logical window is compact"), PWClassA3::Compact(569.0f, 320.0f));
    TestTrue(TEXT("a 720p window at 150% text is compact"), PWClassA3::Compact(1280.0f, 720.0f));
    TestFalse(TEXT("a 1080p window at 100% is not compact"), PWClassA3::Compact(1920.0f, 1080.0f));
    for (const FVector2D Size : {FVector2D(569.0, 320.0), FVector2D(1280.0, 720.0), FVector2D(1920.0, 1080.0)})
    {
        TestTrue(TEXT("the panel fits its window"), PWClassA3::PanelWidth(Size.X, Size.Y) <= Size.X);
        TestTrue(TEXT("the panel keeps clear of the window height"), PWClassA3::PanelMaxHeight(Size.X, Size.Y) <= Size.Y);
    }
    for (const char* Code : {"school.session.closed_day", "school.session.period_used", "school.session.not_at_bell", "school.session.foreground_limit",
                             "school.session.stale_session", "school.session.unfinished", "structure.units_short", "structure.already_done"})
    {
        const FText Text = PWClassA3::IssueText({Code, {}});
        TestFalse(*FString::Printf(TEXT("%hs has words"), Code), Text.IsEmpty());
        TestFalse(*FString::Printf(TEXT("%hs is not shown as its code"), Code), Text.ToString().Contains(TEXT("school.")) || Text.ToString().Contains(TEXT("structure.")));
    }
    TestFalse(TEXT("an unknown code reads as neutral text"), PWClassA3::IssueText({"dayloop.something_new", {}}).IsEmpty());
    TestTrue(TEXT("an unknown key reads as empty, never as the key"), PWClassA3::Str(TEXT("phase2.a3.no.such.key")).IsEmpty());
    TestEqual(TEXT("free"), PWClassA3::CostText(0).ToString(), PWClassA3::Str(TEXT("phase2.a3.common.cost.free")).ToString());
    TestEqual(TEXT("clock"), PWClassA3::ClockText(575).ToString(), FString(TEXT("09:35")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA3TstInvestigateFlowTest, "PerfectWorld.Phase2.A3.InvestigateFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA3TstInvestigateFlowTest::RunTest(const FString& Parameters)
{
    FA3TstFixture Fixture;
    if (!Fixture.Init(*this)) return false;
    SchoolSessionsState Sessions;
    DayLoopState Day = Fixture.DayAtBell();
    const SchoolLessonBinding Binding = Fixture.Binding(SchoolStructure::Investigate);
    const TSharedRef<PWClassA3::FCoreLessonBridge> Bridge = MakeShared<PWClassA3::FCoreLessonBridge>(Sessions, Day, Fixture.Cats, Binding, 100000, 17ULL);
    int32 Closed = 0;
    bool bClosedCancelled = true;
    int32 Backs = 0;
    const TSharedRef<SPWClassInvestigate> Panel = SNew(SPWClassInvestigate)
        .Bridge(Bridge)
        .Space(FVector2D(1280.0, 720.0))
        .OnClose(FSimpleDelegate::CreateLambda([&Backs]() { ++Backs; }))
        .OnLessonClosed(FPWClassLessonClosed::CreateLambda([&Closed, &bClosedCancelled](const bool bCancelled) { ++Closed; bClosedCancelled = bCancelled; }));

    // Idle: how to take the lesson, the focus, and Start. Nothing is charged by looking.
    TestTrue(TEXT("the period is open at the bell"), Panel->GetFrame().Period.ForegroundAvailable);
    for (const TCHAR* Key : {TEXT("mode.foreground"), TEXT("mode.attend"), TEXT("goal.0"), TEXT("goal.1"), TEXT("goal.2"), TEXT("open")})
    {
        TestTrue(*FString::Printf(TEXT("action %s is offered"), Key), Panel->HasAction(Key));
    }
    TestFalse(TEXT("routine is closed until the family is mastered"), Panel->IsActionEnabled(TEXT("mode.routine")));
    TestTrue(TEXT("choosing a goal changes the selection"), Panel->PressAction(TEXT("goal.1")) && Panel->GetGoalIndex() == 1);
    TestFalse(TEXT("nothing opened yet"), Sessions.Active.has_value());

    // Start. The core opens the exercise; the screen reads it back.
    TestTrue(TEXT("open is pressable"), Panel->PressAction(TEXT("open")));
    TestTrue(TEXT("the core has an active lesson"), Sessions.Active.has_value() && Sessions.Investigate.Exposure.Active());
    const PWClassInvestigate::FModel& Model = Panel->GetModel();
    TestTrue(TEXT("the model is active"), Model.bActive);
    TestEqual(TEXT("units shown are the core's"), Model.UnitsLeft, Sessions.Investigate.Exposure.UnitsLeft);
    TestTrue(TEXT("there are tests and causes to choose from"), Model.Tests.Num() >= 3 && Model.Causes.Num() >= 3);
    const uint64 Session = Sessions.Active->Session;
    for (const PWClassInvestigate::FTestRow& Row : Model.Tests)
    {
        InvestigateAction Action;
        Action.Kind = InvestigateActionKind::RunTest;
        Action.Test = std::string(TCHAR_TO_UTF8(*Row.Id));
        const SchoolLessonQuote Core = QuoteSchoolLessonAction(Sessions, Fixture.Cats, Session, Action);
        TestEqual(*FString::Printf(TEXT("%s price is the core's quote"), *Row.Id), Row.Units, Core.Units);
        TestTrue(*FString::Printf(TEXT("%s refusal is the core's"), *Row.Id), Row.Issue.Code == Core.Issue.Code);
        TestFalse(TEXT("an unrun test shows no result"), Row.bRun || Row.bPositive);
    }
    for (const PWClassInvestigate::FCauseRow& Row : Model.Causes)
    {
        TestTrue(TEXT("every cause stands before any test"), Row.bStanding);
        TestTrue(TEXT("naming one now would be a guess"), Row.bGuess);
    }

    // Run one affordable test: charged exactly its quote, then it cannot be run again.
    const PWClassInvestigate::FTestRow* First = Model.Tests.FindByPredicate([](const PWClassInvestigate::FTestRow& Row) { return Row.Issue.Ok(); });
    if (!TestNotNull(TEXT("an affordable test exists"), First)) return false;
    const FString TestId = First->Id;
    const int32 Price = First->Units;
    const int32 LeftBefore = Sessions.Investigate.Exposure.UnitsLeft;
    TestTrue(TEXT("the test runs"), Panel->PressAction(TEXT("test.") + TestId));
    TestEqual(TEXT("charged exactly the quote"), Sessions.Investigate.Exposure.UnitsLeft, LeftBefore - Price);
    TestEqual(TEXT("one test is on the record"), static_cast<int32>(Sessions.Investigate.Exposure.TestsRun.size()), 1);
    TestFalse(TEXT("the same test is not offered twice"), Panel->PressAction(TEXT("test.") + TestId));
    TestEqual(TEXT("the evidence line appears"), Panel->GetModel().Evidence.Num(), 1);
    TestTrue(TEXT("the evidence is shown"), A3TstShows(A3TstShownTexts(Panel), Panel->GetModel().Evidence[0].ToString().Left(12)));

    // A stale callback is refused by the core and nothing moves.
    const SchoolSessionsState Before = Sessions;
    InvestigateAction Stale;
    Stale.Kind = InvestigateActionKind::ReportInconclusive;
    TestFalse(TEXT("a stale session is refused"), Bridge->ChooseAction(Session + 1, Stale).Ok());
    TestTrue(TEXT("and changes nothing"), Sessions == Before);

    // Conclude honestly, record the period.
    TestTrue(TEXT("report is free and available"), Panel->PressAction(TEXT("report")));
    TestTrue(TEXT("the exercise is finished"), Panel->GetFrame().bReadyToClose && Panel->HasAction(TEXT("record")));
    TestFalse(TEXT("a finished foreground lesson cannot be left for free"), Panel->ExecuteClose(true).Ok());
    TestTrue(TEXT("the period is still open after the refusal"), Sessions.Active.has_value() && Closed == 0);
    TestTrue(TEXT("the typed outcome is the investigate outcome"), std::holds_alternative<InvestigateOutcome>(Panel->GetFrame().Outcome));
    const GameInstant Now = Day.Now;
    TestTrue(TEXT("record is pressable"), Panel->PressAction(TEXT("record")));
    TestFalse(TEXT("the lesson is over"), Sessions.Active.has_value());
    TestTrue(TEXT("the period is completed"), !Sessions.Periods.empty() && Sessions.Periods.back().Completed && !Sessions.Periods.back().Cancelled);
    TestTrue(TEXT("the core advanced the clock by the bell period"), Day.Now > Now);
    TestTrue(TEXT("the host heard once, not cancelled"), Closed == 1 && !bClosedCancelled);
    TestTrue(TEXT("a second close is refused"), !Panel->ExecuteClose(false).Ok() && Closed == 1);
    TestTrue(TEXT("Back reaches the host"), (Panel->OnKeyDown(FGeometry(), A3TstKey(EKeys::Escape)), Backs == 1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA3TstInvestigateLeaveTest, "PerfectWorld.Phase2.A3.InvestigateLeave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA3TstInvestigateLeaveTest::RunTest(const FString& Parameters)
{
    FA3TstFixture Fixture;
    if (!Fixture.Init(*this)) return false;
    SchoolSessionsState Sessions;
    DayLoopState Day = Fixture.DayAtBell();
    const GameInstant Start = Day.Now;
    const TSharedRef<PWClassA3::FCoreLessonBridge> Bridge =
        MakeShared<PWClassA3::FCoreLessonBridge>(Sessions, Day, Fixture.Cats, Fixture.Binding(SchoolStructure::Investigate), 0, 3ULL);
    const TSharedRef<SPWClassInvestigate> Panel = SNew(SPWClassInvestigate).Bridge(Bridge).Space(FVector2D(569.0, 320.0));
    TestTrue(TEXT("open"), Panel->ExecuteOpen().Ok());
    const PWClassInvestigate::FTestRow* First = Panel->GetModel().Tests.FindByPredicate([](const PWClassInvestigate::FTestRow& Row) { return Row.Issue.Ok(); });
    if (!TestNotNull(TEXT("an affordable test"), First)) return false;
    const int32 Spent = First->Units;
    TestTrue(TEXT("run it"), Panel->ExecuteRunTest(First->Id).Ok());
    TestTrue(TEXT("leave early is offered mid-exercise"), Panel->GetFrame().bHasRun && !Panel->GetFrame().bReadyToClose);
    const SchoolSessionResult Left = Panel->ExecuteClose(true);
    TestTrue(TEXT("leaving is accepted"), Left.Ok());
    TestTrue(TEXT("the period is cancelled and used"), !Sessions.Periods.empty() && Sessions.Periods.back().Cancelled && !Sessions.Periods.back().Completed);
    TestTrue(TEXT("no learning event"), std::none_of(Left.Events.begin(), Left.Events.end(), [](const DayLoopEvent& E) { return E.Kind == DayLoopEventKind::Completed; }));
    TestEqual(TEXT("the time spent is charged"), static_cast<int64>(Day.Now - Start), static_cast<int64>(Spent * 5));
    TestTrue(TEXT("the screen says it was left"), Panel->IsLessonClosed() && !Panel->GetBanner().IsEmpty());
    // The foreground admission is gone with it.
    const SchoolPeriodView Next = QuerySchoolPeriod(Sessions, Day, Fixture.Binding(SchoolStructure::Investigate, 1), Fixture.Cats);
    TestEqual(TEXT("one foreground lesson is counted for today"), Next.ForegroundUsed, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA3TstCoordinateFlowTest, "PerfectWorld.Phase2.A3.CoordinateFlow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA3TstCoordinateFlowTest::RunTest(const FString& Parameters)
{
    FA3TstFixture Fixture;
    if (!Fixture.Init(*this)) return false;
    SchoolSessionsState Sessions;
    DayLoopState Day = Fixture.DayAtBell();
    const TSharedRef<PWClassA3::FCoreLessonBridge> Bridge =
        MakeShared<PWClassA3::FCoreLessonBridge>(Sessions, Day, Fixture.Cats, Fixture.Binding(SchoolStructure::Coordinate), 100000, 29ULL);
    int32 Closed = 0;
    const TSharedRef<SPWClassCoordinate> Panel = SNew(SPWClassCoordinate)
        .Bridge(Bridge)
        .Space(FVector2D(1920.0, 1080.0))
        .OnLessonClosed(FPWClassLessonClosed::CreateLambda([&Closed](bool) { ++Closed; }));
    for (const TCHAR* Key : {TEXT("goal.0"), TEXT("goal.1"), TEXT("goal.2"), TEXT("goal.3"), TEXT("open")})
    {
        TestTrue(*FString::Printf(TEXT("action %s is offered"), Key), Panel->HasAction(Key));
    }
    TestTrue(TEXT("open"), Panel->PressAction(TEXT("open")));
    const PWClassCoordinate::FModel& Model = Panel->GetModel();
    if (!TestTrue(TEXT("the model is active at the assign beat"), Model.bActive && Model.Beat == CoordinateBeat::Assign)) return false;
    const CoordinateCaseDef* Def = Fixture.Coordinate.FindCase(Sessions.Coordinate.Exposure.CaseId);
    if (!TestNotNull(TEXT("the case is in the catalogue"), Def)) return false;

    // An incomplete draft is never sent: the screen refuses and the core is untouched.
    const SchoolSessionsState Before = Sessions;
    TestFalse(TEXT("an empty draft is refused"), Panel->ExecuteCommitAssignment().Ok());
    TestTrue(TEXT("and nothing changed"), Sessions == Before && Panel->IsBannerError());
    TestTrue(TEXT("pressing a task pencils it in"), Panel->CycleTaskAssignee(Model.Tasks[0].Id) && !Panel->GetModel().Tasks[0].Assignee.IsEmpty());
    TestTrue(TEXT("a pencil mark is not a decision"), Sessions == Before);

    // The first sound assignment, pencilled task by task, then committed.
    for (const CoordinateAssignment& Pair : CoordinateSoundAssignment(*Def))
    {
        TestTrue(TEXT("pencil"), Panel->SetTaskAssignee(UTF8_TO_TCHAR(Pair.Task.c_str()), UTF8_TO_TCHAR(Pair.Member.c_str())));
    }
    TestTrue(TEXT("the draft is complete and judged by the core"), Panel->GetModel().bDraftComplete && Panel->GetModel().AssignIssue.Ok());
    for (const PWClassCoordinate::FMemberRow& Member : Panel->GetModel().Members)
    {
        TestFalse(TEXT("a sound assignment overloads nobody"), Member.bOver);
    }
    TestTrue(TEXT("commit"), Panel->PressAction(TEXT("commit")));
    TestTrue(TEXT("exchange beat"), Panel->GetModel().Beat == CoordinateBeat::Exchange && Sessions.Coordinate.Exposure.Beat == CoordinateBeat::Exchange);

    for (const std::string& Holder : CoordinateCriticalHolders(*Def))
    {
        TestTrue(TEXT("talk to a holder"), Panel->PressAction(TEXT("talk.") + FString(UTF8_TO_TCHAR(Holder.c_str()))));
    }
    TestFalse(TEXT("learned something"), Panel->GetModel().Facts.IsEmpty());
    TestTrue(TEXT("finish the exchange"), Panel->PressAction(TEXT("finish_exchange")));
    TestTrue(TEXT("settle beat"), Panel->GetModel().Beat == CoordinateBeat::Settle && Panel->GetModel().Resolutions.Num() == 5);
    const CoordinateResolution Matching = CoordinateMatchingResolution(Def->Constraint);
    TestTrue(TEXT("settle"), Panel->PressAction(FString::Printf(TEXT("settle.%d"), static_cast<int32>(Matching))));
    TestTrue(TEXT("submit beat"), Panel->GetModel().Beat == CoordinateBeat::Submit);
    TestTrue(TEXT("submit"), Panel->PressAction(Def->Seam ? TEXT("submit.check") : TEXT("submit.plain")));
    if (!TestTrue(TEXT("finished"), Panel->GetFrame().bReadyToClose && Panel->HasAction(TEXT("record")))) return false;
    const CoordinateOutcome* Outcome = std::get_if<CoordinateOutcome>(&Panel->GetFrame().Outcome);
    if (!TestNotNull(TEXT("typed coordinate outcome"), Outcome)) return false;
    TestTrue(TEXT("the careful policy is sound"), Outcome->Grade == StructureGrade::Sound && Outcome->Failures == 0);
    TestTrue(TEXT("record"), Panel->PressAction(TEXT("record")) && Closed == 1 && !Sessions.Active.has_value());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA3TstScreenTest, "PerfectWorld.Phase2.A3.Screen", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA3TstScreenTest::RunTest(const FString& Parameters)
{
    // A refusing bridge: the screen reports the core's words and stays usable.
    {
        const TSharedRef<FA3TstRefusingBridge> Bridge = MakeShared<FA3TstRefusingBridge>();
        const TSharedRef<SPWClassInvestigate> Panel = SNew(SPWClassInvestigate).Bridge(Bridge).Space(FVector2D(853.0, 480.0));
        TestTrue(TEXT("open is offered"), Panel->HasAction(TEXT("open")));
        const SchoolSessionResult Result = Panel->ExecuteOpen();
        TestFalse(TEXT("the refusal is the bridge's"), Result.Ok());
        TestEqual(TEXT("one open reached the bridge"), Bridge->Opens, 1);
        TestTrue(TEXT("the banner is the issue's words"), Panel->IsBannerError() && Panel->GetBanner().EqualTo(PWClassA3::IssueText(Result.Issue)));
        TestTrue(TEXT("still idle and usable"), Panel->HasAction(TEXT("open")) && !Panel->GetFrame().bHasRun);
        TestTrue(TEXT("one panel is on screen"), A3TstCountTag(Panel, PWClassA3::PanelTag) == 1);
    }
    // The real lesson at each window size: one panel, the Back button reachable, the density follows the window.
    FA3TstFixture Fixture;
    if (!Fixture.Init(*this)) return false;
    for (const FVector2D Size : {FVector2D(569.0, 320.0), FVector2D(1280.0, 720.0), FVector2D(1920.0, 1080.0)})
    {
        SchoolSessionsState Sessions;
        DayLoopState Day = Fixture.DayAtBell();
        const TSharedRef<PWClassA3::FCoreLessonBridge> Bridge =
            MakeShared<PWClassA3::FCoreLessonBridge>(Sessions, Day, Fixture.Cats, Fixture.Binding(SchoolStructure::Investigate), 100000, 5ULL);
        const TSharedRef<SPWClassInvestigate> Panel = SNew(SPWClassInvestigate).Bridge(Bridge).Space(Size);
        TestTrue(TEXT("opened"), Panel->ExecuteOpen().Ok());
        TArray<FString> Texts = A3TstShownTexts(Panel);
        TestEqual(TEXT("one panel"), A3TstCountTag(Panel, PWClassA3::PanelTag), 1);
        TestTrue(TEXT("Back is on screen"), A3TstShows(Texts, PWClassA3::Str(TEXT("phase2.a3.common.back")).ToString()));
        TestTrue(TEXT("the case is named"), A3TstShows(Texts, Panel->GetModel().CaseName.ToString()));
        TestTrue(TEXT("the units are shown"), A3TstShows(Texts, PWClassA3::UnitsText(Panel->GetModel().UnitsLeft, Panel->GetModel().UnitsTotal).ToString()));
        TestTrue(TEXT("every test is listed"), Panel->GetModel().Tests.Num() == 0 || A3TstShows(Texts, Panel->GetModel().Tests[0].Name.ToString()));
        TestTrue(TEXT("the keys a lesson uses are bound: G and M do nothing mid-lesson"), Panel->OnKeyDown(FGeometry(), A3TstKey(EKeys::G)).IsEventHandled() == false);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
