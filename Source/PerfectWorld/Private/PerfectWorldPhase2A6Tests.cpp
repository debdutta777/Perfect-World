// Order A6 (Phase 2): the week planner, the recap of real receipts and the aptitude projection terminal. Run in the editor under
// "PerfectWorld.Phase2A6". The widgets are the real ones, built without a window (SlatePrepass evaluates bound text; buttons are
// clicked with SButton::SimulateClick). The bridges are fakes, so nothing here needs a world or the school content: plans and results
// are built by hand in the shape the portable core returns them (checked against the real core in the order's portable probe).
// What these cannot prove is keyboard focus in a real window, the pause menu / phone / terminal hooks and the live reducer commands.

#include "PerfectWorldQuestPanelTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Json.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/SPWSchoolPlanner.h"
#include "UI/SPWSchoolProjection.h"

namespace A6Tests
{
using namespace PerfectWorld;

/** True when the words show a key, a code or a template brace. */
inline bool A6LooksRaw(const FString& Words)
{
    for (const TCHAR* Part : {TEXT("phase2."), TEXT("school.week"), TEXT("week.overlap"), TEXT("dayloop."), TEXT("activity."), TEXT("path."), TEXT("{"), TEXT("_")})
    {
        if (Words.Contains(Part)) return true;
    }
    return false;
}

inline bool A6Has(const TArray<FString>& Texts, const FString& Part)
{
    return Texts.ContainsByPredicate([&Part](const FString& Text) { return Text.Contains(Part); });
}

inline bool A6AnyNote(const TArray<FText>& Notes, const FString& Part)
{
    return Notes.ContainsByPredicate([&Part](const FText& Note) { return Note.ToString().Contains(Part); });
}

/** The 10:00 Monday plan: school arrival, a first Physics lesson, an ordinary second lesson, and a promise the lessons pass over. */
inline FPWPlannerPreview A6Preview(const bool bSceneDay = false)
{
    FPWPlannerPreview Preview;
    Preview.Now = 0;
    SchoolStandingOrder Order;
    Order.Id = "order.school";
    SetSchoolStandingOrder(Preview.Standing, Order);
    if (bSceneDay) Preview.Standing.SceneDays = {2};
    ActivityDef ClassDef;
    ClassDef.Id = "activity.attend_class";
    ClassDef.DurationMinutes = 45;
    Preview.Activities.Defs.push_back(ClassDef);
    SchoolWeekPlan& Plan = Preview.Build.Plan;
    Plan.Week.Days.resize(2);
    WeekEntry Arrive;
    Arrive.Kind = WeekEntryKind::School;
    Arrive.StartMinute = 600;
    Arrive.DecidedByOrder = true;
    Arrive.OrderId = "order.school";
    WeekEntry First;
    First.Kind = WeekEntryKind::Activity;
    First.StartMinute = 620;
    First.ActivityId = "activity.attend_class";
    First.PlaceKind = "classroom";
    First.DecidedByOrder = true;
    First.OrderId = "order.school";
    First.FirstOfKind = true;
    WeekEntry Second = First;
    Second.StartMinute = 670;
    Second.FirstOfKind = false;
    Plan.Week.Days[0].Entries = {Arrive, First, Second};
    for (int32 Entry = 1; Entry <= 2; ++Entry)
    {
        SchoolWeekLesson Lesson;
        Lesson.Day = 0;
        Lesson.Entry = Entry;
        Lesson.Binding.Id = Entry == 1 ? "school.lesson.test_0" : "school.lesson.test_1";
        Lesson.Binding.Stream = "science";
        Lesson.Binding.Subject = Entry == 1 ? "physics" : "mathematics";
        Lesson.Binding.Period = Entry - 1;
        Lesson.Binding.TitleTextKey = "phase2.test.title";
        Plan.Lessons.push_back(Lesson);
    }
    // The promise the player already knew about, 16 minutes into the first lesson (the clock's frame: instant 0 is 06:00).
    ReceiptCandidate Promise;
    Promise.Id = "activity.exercise_run";
    Promise.Source = "source.friend";
    Promise.KnownAt = 0;
    Promise.Start = GameClock::StartOfDay(0) + 640;
    Promise.DurationMinutes = 30;
    Preview.Known.push_back(Promise);
    return Preview;
}

class FA6FakePlanner final : public IPWSchoolPlannerWorld
{
public:
    FPWPlannerPreview Preview = A6Preview();
    FPWPlannerResolution Next;
    bool bReadable = true;
    int32 Reads = 0;
    int32 OrderCalls = 0;
    int32 ResolveCalls = 0;
    /** Only Resolve moves this: the stand-in for the world clock. */
    GameInstant Clock = 0;

    virtual bool ReadPreview(FPWPlannerPreview& OutPreview, FString& OutWhy) override
    {
        ++Reads;
        if (!bReadable) { OutWhy = TEXT("no world"); return false; }
        OutPreview = Preview;
        return true;
    }
    virtual FPWPlannerOutcome SetOrder(const SchoolStandingOrder& Order, const bool bRemove) override
    {
        ++OrderCalls;
        FPWPlannerOutcome Outcome;
        const SchoolIssue Issue = SetSchoolStandingOrder(Preview.Standing, Order, bRemove);
        Outcome.bAccepted = Issue.Ok();
        Outcome.Code = FString(UTF8_TO_TCHAR(Issue.Code.c_str()));
        return Outcome;
    }
    virtual FPWPlannerResolution Resolve() override
    {
        ++ResolveCalls;
        if (Next.Outcome.bAccepted) Clock += 90;
        return Next;
    }
};

/** A resolution the core could return: arrival, 20 min waiting, the first lesson done by order with its real receipt, then the stop. */
inline FPWPlannerResolution A6Resolution(const FPWPlannerPreview& Preview, const bool bStopped)
{
    FPWPlannerResolution Resolution;
    Resolution.Outcome.bAccepted = true;
    Resolution.Plan = Preview.Build.Plan;
    SchoolWeekResult& Result = Resolution.Result;
    WeekResult Arrival;
    Arrival.Disclosures.push_back({0, 0, WeekEntryKind::School, "school", "order.school", 240});
    Arrival.Outcomes.push_back({0, 0, WeekEntryKind::School, "school", 240, 240, 0});
    WeekResult Waited;
    Waited.Outcomes.push_back({0, 0, WeekEntryKind::Spend, "spend", 240, 260, 0});
    WeekResult Lesson;
    Lesson.Disclosures.push_back({0, 1, WeekEntryKind::Activity, "activity.attend_class", "order.school", 260});
    Lesson.Outcomes.push_back({0, 1, WeekEntryKind::Activity, "activity.attend_class", 260, 305, 0});
    Result.Segments = {Arrival, Waited, Lesson};
    DayLoopEvent Event;
    Event.Kind = DayLoopEventKind::Completed;
    Event.At = 305;
    Event.ActivityId = "activity.attend_class";
    Event.ReceiptSeq = 1;
    Event.Credited.push_back({"skill.academics", 1000});
    Result.ActivityEvents.push_back(Event);
    ReceiptEntry Receipt;
    Receipt.Seq = 1;
    Receipt.ActivityId = "activity.attend_class";
    Receipt.Start = 260;
    Receipt.DurationMinutes = 45;
    ReceiptDisplaced Passed;
    Passed.Id = "activity.exercise_run";
    Passed.OverlapMinutes = 25;
    Receipt.Displaced.push_back(Passed);
    Result.World.DayLoop.Receipts.Entries.push_back(Receipt);
    if (bStopped)
    {
        Result.Stop = WeekStop::FirstNeedsPlay;
        Result.StopDay = 0;
        Result.StopEntry = 2;
    }
    return Resolution;
}

class FA6FakeProjection final : public IPWSchoolProjectionWorld
{
public:
    AptitudeLedger Ledger;
    bool bReadable = true;
    int32 Reads = 0;
    virtual bool ReadLedger(AptitudeLedger& OutLedger, FString& OutWhy) override
    {
        ++Reads;
        if (!bReadable) { OutWhy = TEXT("no world"); return false; }
        OutLedger = Ledger;
        return true;
    }
};

inline AptitudeEvidence A6Evidence(const char* Id, const AptitudeSource Source, const AptitudeFacet Facet, const char* Subject)
{
    AptitudeEvidence Item;
    Item.Id = Id;
    Item.Source = Source;
    Item.Facet = Facet;
    Item.Subject = Subject;
    Item.Result = AptitudeResult::Sound;
    Item.At = 100;
    return Item;
}
} // namespace A6Tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA6TextTableTest, "PerfectWorld.Phase2A6.TheTextTableMatchesItsJsonFragment",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA6TextTableTest::RunTest(const FString& Parameters)
{
    using namespace A6Tests;
    TMap<FString, FString> Built;
    for (int32 Index = 0; Index < PWSchoolPlanner::BuiltInTextCount(); ++Index)
    {
        const FString Key = PWSchoolPlanner::BuiltInTextKey(Index);
        TestTrue(*(Key + TEXT(" is a phase2.a6 dotted lowercase key")), Key.StartsWith(TEXT("phase2.a6.")) && Key.ToLower() == Key && !Key.Contains(TEXT(" ")));
        TestFalse(*(Key + TEXT(" is unique")), Built.Contains(Key));
        TestFalse(*(Key + TEXT(" has words")), FString(PWSchoolPlanner::BuiltInTextValue(Index)).IsEmpty());
        Built.Add(Key, PWSchoolPlanner::BuiltInTextValue(Index));
    }
    FString Raw;
    const FString Path = FPaths::ProjectDir() / TEXT("Data/Text/en/phase2_a6.json");
    if (!TestTrue(TEXT("the fragment is on disk"), FFileHelper::LoadFileToString(Raw, *Path))) return false;
    const JsonParseResult Parsed = ParseJson(TCHAR_TO_UTF8(*Raw));
    if (!TestTrue(TEXT("the fragment is one JSON object"), Parsed.Value.has_value() && Parsed.Value->IsObject())) return false;
    TestEqual(TEXT("the same number of keys"), static_cast<int32>(Parsed.Value->Object.size()), Built.Num());
    for (const auto& Member : Parsed.Value->Object)
    {
        const FString Key = UTF8_TO_TCHAR(Member.first.c_str());
        const FString* Value = Built.Find(Key);
        if (!TestNotNull(*(Key + TEXT(" is in the built-in table")), Value)) continue;
        TestTrue(*(Key + TEXT(" is a string")), Member.second.IsString());
        TestEqual(*(Key + TEXT(" reads the same")), FString(UTF8_TO_TCHAR(Member.second.String.c_str())), *Value);
    }
    // Lookup: the provider is asked first; an unknown or malformed key reads neutrally and never as itself.
    TestTrue(TEXT("an unknown key is empty"), PWSchoolPlanner::FindText(TEXT("phase2.a6.nothing")).IsEmpty());
    TestEqual(TEXT("TextOr falls back"), PWSchoolPlanner::TextOr("phase2.a1.unwritten", FText::FromString(TEXT("A lesson"))).ToString(), FString(TEXT("A lesson")));
    PWSchoolPlanner::SetTextProvider([](const FString& Key, FText& Out) { if (Key != TEXT("phase2.a1.lesson.physics.title")) return false; Out = FText::FromString(TEXT("Physics: motion")); return true; });
    TestEqual(TEXT("another order's key resolves through the provider"), PWSchoolPlanner::TextOr("phase2.a1.lesson.physics.title", FText::GetEmpty()).ToString(), FString(TEXT("Physics: motion")));
    PWSchoolPlanner::SetTextProvider(nullptr);
    for (const TCHAR* Code : {TEXT("school.week.overlapping_orders"), TEXT("school.week.busy"), TEXT("week.overlap"), TEXT("week.world_busy"), TEXT("dayloop.asleep"), TEXT("nonsense")})
    {
        const FString Words = PWSchoolPlanner::RefusalText(Code).ToString();
        TestFalse(*(FString(Code) + TEXT(" reads as words, not a code")), Words.IsEmpty() || A6LooksRaw(Words));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA6PlanModelTest, "PerfectWorld.Phase2A6.ThePlanShowsOrdersFirstsStopsAndKnownDisplacedCommitments",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA6PlanModelTest::RunTest(const FString& Parameters)
{
    using namespace A6Tests;
    {
        const FPWPlannerModel Model = PWSchoolPlanner::BuildModel(A6Preview(), FPWRecapModel(), FText::GetEmpty());
        TestTrue(TEXT("the plan is buildable"), Model.bAvailable && Model.bPlanOk);
        if (!TestEqual(TEXT("one order"), Model.Orders.Num(), 1)) return false;
        TestEqual(TEXT("it is described in words"), Model.Orders[0].Title.ToString(), FString(TEXT("Mon to Fri: go to school; lessons attend only")));
        if (!TestEqual(TEXT("both days are present, the second empty"), Model.Days.Num(), 2)) return false;
        const FPWPlanDayRow& Monday = Model.Days[0];
        if (!TestEqual(TEXT("three entries on the first day"), Monday.Entries.Num(), 3)) return false;
        TestEqual(TEXT("the day is named"), Monday.Heading.ToString(), FString(TEXT("Mon 2 Jun")));
        TestEqual(TEXT("the arrival"), Monday.Entries[0].What.ToString(), FString(TEXT("Arrive at school")));
        TestTrue(TEXT("done by order is disclosed"), A6AnyNote(Monday.Entries[0].Notes, TEXT("Done for you by your standing order.")));
        // The first lesson is a first: the player plays it and compression stops there, saying why.
        const FPWPlanEntryRow& FirstLesson = Monday.Entries[1];
        TestEqual(TEXT("the lesson is named from its subject, never its key"), FirstLesson.What.ToString(), FString(TEXT("Lesson: Physics")));
        TestEqual(TEXT("its time is a range"), FirstLesson.Time.ToString(), FString(TEXT("10:20 to 11:05")));
        TestTrue(TEXT("the player plays it"), FirstLesson.bYouPlay && FirstLesson.bStopsHere);
        TestTrue(TEXT("with its reason"), A6AnyNote(FirstLesson.Notes, TEXT("Your first time at this lesson.")) && A6AnyNote(FirstLesson.Notes, TEXT("Compression stops here.")));
        TestFalse(TEXT("the next lesson is not a stop"), Monday.Entries[2].bStopsHere || Monday.Entries[2].bYouPlay);
        TestTrue(TEXT("the summary says where it stops"), Model.bStopsAtAll && Model.StopSummary.ToString().Contains(TEXT("Compression would stop before Lesson: Physics")));
        // The promise it passes over was known and overlaps: 10:40 to 11:10 against 10:20 to 11:05 is 25 minutes.
        if (TestEqual(TEXT("the known promise is displaced"), FirstLesson.Displaced.Num(), 1))
        {
            TestEqual(TEXT("by name"), FirstLesson.Displaced[0].Name.ToString(), FString(TEXT("Go for a run")));
            TestEqual(TEXT("for the minutes it overlaps"), FirstLesson.Displaced[0].OverlapMinutes, 25);
        }
        TestEqual(TEXT("only the first lesson overlaps it by 25; the next overlaps less"), Monday.Entries[2].Displaced.Num(), 0);
        TestEqual(TEXT("every entry is counted"), Model.EntryCount, 3);
    }
    {
        // A promise made after the decision, or one that could not be taken, displaces nothing: only what was known and available.
        FPWPlannerPreview Preview = A6Preview();
        Preview.Known[0].KnownAt = 5000;
        const FPWPlannerModel Late = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
        TestEqual(TEXT("an alternative learned later is not displaced"), Late.Days[0].Entries[1].Displaced.Num(), 0);
        Preview.Known[0].KnownAt = 0;
        Preview.Known[0].Available = false;
        TestEqual(TEXT("an unavailable alternative is not displaced"), PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty()).Days[0].Entries[1].Displaced.Num(), 0);
    }
    {
        // A played-by-hand order, an unmastered routine and a nominated scene day are all marked, each in its own words.
        FPWPlannerPreview Preview = A6Preview(true);
        Preview.Sessions.Periods.push_back({0, 0, "school.lesson.test_0", SchoolLessonMode::Attend, true, false, 1, {}});
        Preview.Build.Plan.Lessons[0].Mode = SchoolLessonMode::Foreground;
        const FPWPlannerModel Model = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
        TestTrue(TEXT("the scene day is listed"), Model.SceneDays.Num() == 1 && Model.SceneDays[0].Date.ToString() == FString(TEXT("Wed 4 Jun")));
        TestTrue(TEXT("a lesson already completed once, played by order, says so"), A6AnyNote(Model.Days[0].Entries[1].Notes, TEXT("Your order says you play each lesson yourself.")));
        Preview.Build.Plan.Lessons[0].Mode = SchoolLessonMode::Routine;
        TestTrue(TEXT("an unmastered routine says so"), A6AnyNote(PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty()).Days[0].Entries[1].Notes, TEXT("Routine is not ready for this lesson yet.")));
        Preview.Build.Plan.Week.Days[0].Entries[1].FirstOfKind = false;
        Preview.Build.Plan.Week.Days[0].Entries[2].NeedsChoice = true;
        FPWPlannerModel Choice = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
        TestTrue(TEXT("a choice no order covers stops compression"), Choice.Days[0].Entries[2].bStopsHere && A6AnyNote(Choice.Days[0].Entries[2].Notes, TEXT("A choice only you can make.")));
        Preview.Build.Plan.Week.InterruptsUsed = 1;
        Choice = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
        TestTrue(TEXT("with the week's interruption spent it is said so"), A6AnyNote(Choice.Days[0].Entries[2].Notes, TEXT("already used")));
    }
    {
        FPWPlannerPreview Preview = A6Preview();
        for (WeekEntry& Entry : Preview.Build.Plan.Week.Days[0].Entries) Entry.FirstOfKind = false;
        const FPWPlannerModel Calm = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
        TestFalse(TEXT("nothing needs the player"), Calm.bStopsAtAll);
        TestTrue(TEXT("and it says so"), Calm.StopSummary.ToString().Contains(TEXT("it would resolve in full")));
    }
    {
        FPWPlannerPreview Preview = A6Preview();
        Preview.Build.Issue = {"week.overlap", "English detail that must not be shown"};
        const FPWPlannerModel Refused = PWSchoolPlanner::BuildModel(Preview, FPWRecapModel(), FText::GetEmpty());
        TestFalse(TEXT("a refused build is not a plan"), Refused.bPlanOk);
        TestTrue(TEXT("it says why in words"), Refused.PlanProblem.ToString().Contains(TEXT("School time collides")) && !A6LooksRaw(Refused.PlanProblem.ToString()));
        TestFalse(TEXT("and the core's detail is not shown"), Refused.PlanProblem.ToString().Contains(TEXT("English detail")));
        TestEqual(TEXT("no days are invented"), Refused.Days.Num(), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA6RecapTest, "PerfectWorld.Phase2A6.TheRecapShowsOnlyWhatReallyRan",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA6RecapTest::RunTest(const FString& Parameters)
{
    using namespace A6Tests;
    PWSchoolPlanner::ClearRecap();
    TestFalse(TEXT("a fresh session has no recap"), PWSchoolPlanner::LastRecap().bPresent);
    const FPWPlannerPreview Preview = A6Preview();
    {
        // Compression stopped at the very first entry: nothing ran, no time passed, and no segment is invented.
        FPWPlannerResolution Resolution;
        Resolution.Plan = Preview.Build.Plan;
        Resolution.Result.Stop = WeekStop::FirstNeedsPlay;
        Resolution.Result.StopDay = 0;
        Resolution.Result.StopEntry = 1;
        const FPWRecapModel Recap = PWSchoolPlanner::BuildRecap(Resolution.Plan, Resolution.Result);
        TestTrue(TEXT("a real result gives a recap"), Recap.bPresent);
        TestEqual(TEXT("with no rows"), Recap.Rows.Num(), 0);
        TestEqual(TEXT("nothing ran"), Recap.Headline.ToString(), FString(TEXT("Nothing was resolved and no game time passed.")));
        TestTrue(TEXT("it names what the player must play"), Recap.Stop.ToString().Contains(TEXT("Stopped before your first time at Lesson: Physics.")));
        TestTrue(TEXT("and the interruptions used"), Recap.Interrupts.ToString().Contains(TEXT("0 of 1")));
    }
    {
        SchoolWeekResult Refused;
        Refused.Issue = {"school.week.busy", ""};
        TestFalse(TEXT("a refused resolve produces no recap"), PWSchoolPlanner::BuildRecap(Preview.Build.Plan, Refused).bPresent);
        PWSchoolPlanner::StoreResolution(Preview.Build.Plan, Refused);
        TestFalse(TEXT("and nothing is stored"), PWSchoolPlanner::LastRecap().bPresent);
    }
    {
        const FPWPlannerResolution Resolution = A6Resolution(Preview, true);
        const FPWRecapModel Recap = PWSchoolPlanner::BuildRecap(Resolution.Plan, Resolution.Result);
        if (!TestEqual(TEXT("an arrival and a lesson; the wait is not a row"), Recap.Rows.Num(), 2)) return false;
        TestEqual(TEXT("two commitments resolved"), Recap.Headline.ToString(), FString(TEXT("Commitments resolved: 2.")));
        TestTrue(TEXT("the waiting is summed"), Recap.Passed.ToString().Contains(TEXT("20 min")));
        TestTrue(TEXT("the arrival is real, on time, done by order"), A6AnyNote(Recap.Rows[0].Notes, TEXT("You arrived on time.")) && A6AnyNote(Recap.Rows[0].Notes, TEXT("Done for you by your standing order.")));
        TestFalse(TEXT("the arrival carries no invented receipt"), A6AnyNote(Recap.Rows[0].Notes, TEXT("Time reserved")));
        TestEqual(TEXT("the lesson row is named"), Recap.Rows[1].What.ToString(), FString(TEXT("Lesson: Physics")));
        TestTrue(TEXT("it shows the real receipt's minutes and what it passed over"),
                 A6AnyNote(Recap.Rows[1].Notes, TEXT("Time reserved: 45 min.")) && A6AnyNote(Recap.Rows[1].Notes, TEXT("Passed over, as you knew at the time: Go for a run.")));
        TestTrue(TEXT("and that learning was recorded"), A6AnyNote(Recap.Rows[1].Notes, TEXT("Learning recorded.")));
        TestTrue(TEXT("the stop names the lesson to play"), Recap.Stop.ToString().Contains(TEXT("Stopped before your first time at Lesson: Mathematics.")));
        TestTrue(TEXT("no row shows a key or code"), !A6LooksRaw(Recap.Rows[1].Time.ToString()) && !A6LooksRaw(Recap.Rows[1].What.ToString()));
        const uint32 Before = PWSchoolPlanner::RecapRevision();
        PWSchoolPlanner::StoreResolution(Resolution.Plan, Resolution.Result);
        TestTrue(TEXT("a real resolution is stored this session"), PWSchoolPlanner::LastRecap().bPresent && PWSchoolPlanner::RecapRevision() != Before);
        FA6FakePlanner Fake;
        TestTrue(TEXT("the planner model carries it"), PWSchoolPlanner::ReadModel(Fake).Recap.bPresent);
        PWSchoolPlanner::ClearRecap();
        TestFalse(TEXT("after a reload (cleared) none is rebuilt from the plan"), PWSchoolPlanner::ReadModel(Fake).Recap.bPresent);
    }
    {
        // A receipt the day's overflow took (sequence 0) is said to be in the totals; a completion with no event shows no receipt at all.
        FPWPlannerResolution Resolution = A6Resolution(Preview, false);
        Resolution.Result.ActivityEvents[0].ReceiptSeq = 0;
        const FPWRecapModel Overflow = PWSchoolPlanner::BuildRecap(Resolution.Plan, Resolution.Result);
        TestTrue(TEXT("overflow is recorded in the totals"), A6AnyNote(Overflow.Rows[1].Notes, TEXT("Recorded in the day's totals.")));
        Resolution.Result.ActivityEvents.clear();
        const FPWRecapModel None = PWSchoolPlanner::BuildRecap(Resolution.Plan, Resolution.Result);
        TestFalse(TEXT("no event, no receipt line"), A6AnyNote(None.Rows[1].Notes, TEXT("Time reserved")) || A6AnyNote(None.Rows[1].Notes, TEXT("totals")));
        TestTrue(TEXT("and an unstopped week says the whole plan was resolved"), None.Stop.ToString() == FString(TEXT("The whole plan was resolved.")));
    }
    PWSchoolPlanner::ClearRecap();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA6PlannerWidgetTest, "PerfectWorld.Phase2A6.ThePlannerCostsNothingUntilResolve",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA6PlannerWidgetTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace A6Tests;
    PWSchoolPlanner::ClearRecap();
    const TSharedRef<FA6FakePlanner> Fake = MakeShared<FA6FakePlanner>();
    int32 Closed = 0;
    const TSharedRef<SPWSchoolPlanner> Panel = SNew(SPWSchoolPlanner).World(Fake).Space(FVector2D(1280.0, 720.0)).OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
    {
        const TArray<FString> Texts = QwShownTexts(Panel);
        TestTrue(TEXT("the title, the free note and the headings"), QwHas(Texts, TEXT("Week planner")) && A6Has(Texts, TEXT("Planning costs no time")) && QwHas(Texts, TEXT("Standing orders")) && QwHas(Texts, TEXT("Scene days")));
        TestEqual(TEXT("no recap yet, and it says so"), QwCountTag(Panel, PWSchoolPlanner::NoRecapTag), 1);
        TestEqual(TEXT("the order is listed"), QwCountTag(Panel, PWSchoolPlanner::OrderRowTag), 1);
        TestEqual(TEXT("the day and its entries are previewed"), QwCountTag(Panel, PWSchoolPlanner::DayTag), 1);
        TestEqual(TEXT("three entries"), QwCountTag(Panel, PWSchoolPlanner::EntryTag), 3);
        TestTrue(TEXT("the stop is announced"), A6Has(Texts, TEXT("Compression would stop before Lesson: Physics")));
        TestTrue(TEXT("Resolve and Back are offered"), QwHas(Texts, TEXT("Resolve this week")) && QwHas(Texts, TEXT("Back")));
        TestFalse(TEXT("no raw key or code is shown"), Texts.ContainsByPredicate([](const FString& Text) { return A6LooksRaw(Text); }));
    }
    // Pauses cost nothing: browsing, drafting, editing and reading never reach a command.
    Panel->ToggleDraftDay(1);
    Panel->ToggleDraftDay(1);
    Panel->SetDraftMode(SchoolLessonMode::Routine);
    Panel->SetDraftAttend(false);
    Panel->LoadDraft(TEXT("order.school"));
    Panel->Refresh();
    const FGeometry Geometry;
    Panel->OnKeyDown(Geometry, QwKey(EKeys::PageDown));
    TestTrue(TEXT("editing a draft sent no command and moved no clock"), Fake->OrderCalls == 0 && Fake->ResolveCalls == 0 && Fake->Clock == 0);
    TestEqual(TEXT("Edit loaded the order's days"), static_cast<int32>(Panel->GetDraftWeekdays()), static_cast<int32>(AttendanceSchoolWeekdays));

    // Saving with no day chosen is refused by the panel itself.
    const TSharedRef<SPWSchoolPlanner> Plain = SNew(SPWSchoolPlanner).World(Fake).Space(FVector2D(1280.0, 720.0));
    TestFalse(TEXT("no days, no order"), Plain->SaveDraft().bAccepted);
    TestTrue(TEXT("and it says so"), Plain->GetNotice().ToString().Contains(TEXT("Choose at least one day")));
    TestEqual(TEXT("nothing was sent"), Fake->OrderCalls, 0);
    // An order on days another order already covers is refused by the core, in words.
    Plain->ToggleDraftDay(1);
    Plain->ToggleDraftDay(2);
    const FPWPlannerOutcome Overlap = Plain->SaveDraft();
    TestTrue(TEXT("the core refused the overlap"), !Overlap.bAccepted && Overlap.Code == TEXT("school.week.overlapping_orders") && Fake->OrderCalls == 1);
    TestTrue(TEXT("in words"), Plain->GetNotice().ToString().Contains(TEXT("Two orders cannot cover the same day")));
    TestEqual(TEXT("no clock moved"), Fake->Clock, static_cast<GameInstant>(0));
    // Remove, then add one order for two days: removal and saving are free.
    TestTrue(TEXT("removing the order is accepted"), Plain->RemoveOrder(TEXT("order.school")).bAccepted && Plain->GetModel().Orders.IsEmpty());
    TestTrue(TEXT("with its own words"), Plain->GetNotice().ToString().Contains(TEXT("Order removed. No time was spent.")));
    const FPWPlannerOutcome Saved = Plain->SaveDraft();
    TestTrue(TEXT("saving the two days is accepted"), Saved.bAccepted && Plain->GetModel().Orders.Num() == 1 && Plain->GetModel().Orders[0].Id == TEXT("order.school_mon_tue"));
    TestTrue(TEXT("described by its days"), Plain->GetModel().Orders[0].Title.ToString().Contains(TEXT("Mon, Tue")));
    TestEqual(TEXT("orders still cost no time"), Fake->Clock, static_cast<GameInstant>(0));
    TestEqual(TEXT("and Resolve was never called"), Fake->ResolveCalls, 0);

    // Resolve: the one call that can spend time. It runs once, stores the real recap and shows it.
    Fake->Next = A6Resolution(Fake->Preview, true);
    TestTrue(TEXT("Resolve is accepted"), Panel->ResolveWeek().bAccepted);
    TestEqual(TEXT("one call, one resolve"), Fake->ResolveCalls, 1);
    TestEqual(TEXT("and the clock moved only now"), Fake->Clock, static_cast<GameInstant>(90));
    {
        const TArray<FString> Texts = QwShownTexts(Panel);
        TestEqual(TEXT("the recap is shown"), QwCountTag(Panel, PWSchoolPlanner::RecapTag), 1);
        TestEqual(TEXT("with its two rows"), QwCountTag(Panel, PWSchoolPlanner::RecapRowTag), 2);
        TestEqual(TEXT("and no longer the empty note"), QwCountTag(Panel, PWSchoolPlanner::NoRecapTag), 0);
        TestTrue(TEXT("what ran, what it passed over, why it stopped"), A6Has(Texts, TEXT("Commitments resolved: 2.")) && A6Has(Texts, TEXT("Passed over, as you knew at the time: Go for a run.")) && A6Has(Texts, TEXT("Stopped before your first time at")));
    }
    // A reopened window shows the same recap (session state); a refused resolve shows words and stores nothing new.
    const TSharedRef<SPWSchoolPlanner> Reopened = SNew(SPWSchoolPlanner).World(Fake).Space(FVector2D(1280.0, 720.0));
    TestEqual(TEXT("a reopened planner shows the recap"), QwCountTag(Reopened, PWSchoolPlanner::RecapTag), 1);
    const uint32 Revision = PWSchoolPlanner::RecapRevision();
    Fake->Next = FPWPlannerResolution();
    Fake->Next.Outcome.Code = TEXT("school.week.busy");
    TestFalse(TEXT("a refused resolve is refused"), Reopened->ResolveWeek().bAccepted);
    TestTrue(TEXT("in words"), Reopened->GetNotice().ToString().Contains(TEXT("A lesson is open")));
    TestTrue(TEXT("with no new recap and no time"), PWSchoolPlanner::RecapRevision() == Revision && Fake->Clock == 90);
    // A plan that cannot be built cannot be resolved, and the panel does not even ask.
    Fake->Preview.Build.Issue = {"week.overlap", ""};
    Reopened->Refresh();
    const int32 Calls = Fake->ResolveCalls;
    TestFalse(TEXT("nothing to resolve"), Reopened->ResolveWeek().bAccepted);
    TestEqual(TEXT("the bridge was not asked"), Fake->ResolveCalls, Calls);
    TestEqual(TEXT("the problem is shown in words"), QwCountTag(Reopened, PWSchoolPlanner::ProblemTag), 1);

    // Keys: every back key goes back on the first press only; unrelated keys are left alone.
    int32 Expected = 0;
    for (const FKey& Back : {EKeys::Escape, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Right, EKeys::Gamepad_LeftShoulder})
    {
        TestTrue(*(Back.ToString() + TEXT(" is handled")), Panel->OnKeyDown(Geometry, QwKey(Back)).IsEventHandled());
        TestEqual(*(Back.ToString() + TEXT(" goes back")), Closed, ++Expected);
        Panel->OnKeyDown(Geometry, QwKey(Back, true));
        TestEqual(*(Back.ToString() + TEXT(" held does not go back twice")), Closed, Expected);
    }
    TestFalse(TEXT("an unrelated key is left alone"), Panel->OnKeyDown(Geometry, QwKey(EKeys::A)).IsEventHandled());
    TestTrue(TEXT("the Back button goes back"), QwClick(Panel, TEXT("Back")) && Closed == Expected + 1);
    PWSchoolPlanner::ClearRecap();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA6PlannerStatesTest, "PerfectWorld.Phase2A6.ThePlannerFitsEveryWindowAndSaysWhenItCannotRead",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA6PlannerStatesTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace A6Tests;
    PWSchoolPlanner::ClearRecap();
    for (const FVector2D Size : {FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0, 1080.0), FVector2D(300.0, 400.0)})
    {
        const TSharedRef<FA6FakePlanner> Fake = MakeShared<FA6FakePlanner>();
        const TSharedRef<SPWSchoolPlanner> Panel = SNew(SPWSchoolPlanner).World(Fake).Space(Size);
        const TArray<FString> Texts = QwShownTexts(Panel);
        TestTrue(*FString::Printf(TEXT("%.0fx%.0f shows the planner and its buttons"), Size.X, Size.Y), QwHas(Texts, TEXT("Week planner")) && QwHas(Texts, TEXT("Back")) && QwHas(Texts, TEXT("Resolve this week")));
        TestEqual(TEXT("and the whole plan"), QwCountTag(Panel, PWSchoolPlanner::EntryTag), 3);
    }
    {
        const TSharedRef<FA6FakePlanner> Fake = MakeShared<FA6FakePlanner>();
        Fake->bReadable = false;
        const TSharedRef<SPWSchoolPlanner> Panel = SNew(SPWSchoolPlanner).World(Fake).Space(FVector2D(1280.0, 720.0));
        TestEqual(TEXT("an unreadable world says so"), QwCountTag(Panel, PWSchoolPlanner::UnavailableTag), 1);
        TestTrue(TEXT("Back is still offered"), QwHas(QwShownTexts(Panel), TEXT("Back")));
        TestFalse(TEXT("and Resolve does nothing"), Panel->ResolveWeek().bAccepted || Fake->ResolveCalls != 0);
    }
    {
        const TSharedRef<FA6FakePlanner> Fake = MakeShared<FA6FakePlanner>();
        Fake->Preview = FPWPlannerPreview();
        Fake->Preview.Build.Plan.Week.Days.resize(1);
        const TSharedRef<SPWSchoolPlanner> Panel = SNew(SPWSchoolPlanner).World(Fake).Space(FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = QwShownTexts(Panel);
        TestTrue(TEXT("no orders, no scene days, nothing planned: each says so"), A6Has(Texts, TEXT("No standing orders")) && A6Has(Texts, TEXT("No scene days are nominated")) && A6Has(Texts, TEXT("Nothing is planned")));
        TestEqual(TEXT("and no day rows"), QwCountTag(Panel, PWSchoolPlanner::DayTag), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA6ProjectionModelTest, "PerfectWorld.Phase2A6.UnobservedEvidenceIsUnknownNeverWeak",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA6ProjectionModelTest::RunTest(const FString& Parameters)
{
    using namespace A6Tests;
    const TArray<FString> Forbidden = {TEXT("weak"), TEXT("poor"), TEXT("low"), TEXT("fail"), TEXT("bad"), TEXT("lack"), TEXT("unfit")};
    {
        const FPWProjectionModel Model = PWSchoolProjection::BuildModel(AptitudeLedger());
        if (!TestTrue(TEXT("an empty ledger is a real, unknown projection"), Model.bAvailable && Model.bNothingSeen && Model.Paths.Num() == 10)) return false;
        for (const FPWProjectionPathRow& Row : Model.Paths)
        {
            const FString Name = Row.Name.ToString();
            TestTrue(*(Name + TEXT(" is unknown so far")), Row.bUnknown && Row.Band.ToString() == TEXT("Unknown so far") && Row.Coverage.ToString() == TEXT("Nothing seen yet"));
            TestEqual(*(Name + TEXT(" has five facets, all Unknown")), Row.Facets.Num(), 5);
            for (const FPWProjectionFacetRow& Facet : Row.Facets)
            {
                TestTrue(*(Name + TEXT(": ") + Facet.Name.ToString() + TEXT(" is Unknown")), Facet.State.ToString() == TEXT("Unknown") && Facet.StateValue == AptitudeFacetState::Unobserved);
            }
            TestEqual(*(Name + TEXT(" lists all five as not yet seen")), Row.Unknown.Num(), 5);
            TestTrue(*(Name + TEXT(": the printout says unknown, not unsuitable")), Row.Printout.ToString().Contains(TEXT("unknown, not unsuitable")));
            TestEqual(*(Name + TEXT(" has no observations")), Row.ObservationCount, 0);
            TArray<FString> Words = {Name, Row.Band.ToString(), Row.Coverage.ToString(), Row.Printout.ToString()};
            for (const FPWProjectionFacetRow& Facet : Row.Facets) { Words.Add(Facet.Name.ToString()); Words.Add(Facet.State.ToString()); }
            for (const FText& Unknown : Row.Unknown) Words.Add(Unknown.ToString());
            for (const FString& Word : Words)
            {
                for (const FString& Bad : Forbidden) TestFalse(*(Name + TEXT(": '") + Word + TEXT("' never says weak")), Word.ToLower().Contains(Bad));
                TestFalse(*(Name + TEXT(": '") + Word + TEXT("' shows no key")), A6LooksRaw(Word));
            }
        }
    }
    {
        // Analysis and interpretation evidence from the real structures: those facets rise; nothing else is called weak.
        AptitudeLedger Ledger;
        const char* Subjects[] = {"family.alpha", "family.beta", "family.gamma"};
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FString Id = FString::Printf(TEXT("school.lesson.model_%d"), Index);
            const FString Id2 = FString::Printf(TEXT("school.lesson.explain_%d"), Index);
            TestTrue(TEXT("model evidence is accepted"), AptitudeAddEvidence(Ledger, A6Evidence(TCHAR_TO_UTF8(*Id), AptitudeSource::StructureModel, AptitudeFacet::Analysis, Subjects[Index])).Ok());
            TestTrue(TEXT("explain evidence is accepted"), AptitudeAddEvidence(Ledger, A6Evidence(TCHAR_TO_UTF8(*Id2), AptitudeSource::StructureExplain, AptitudeFacet::Interpretation, Subjects[Index])).Ok());
        }
        const AptitudeProjection Core = AptitudeProject(Ledger);
        const FPWProjectionModel Model = PWSchoolProjection::BuildModel(Ledger);
        if (!TestTrue(TEXT("the model has all ten paths"), Model.bAvailable && !Model.bNothingSeen && Model.Paths.Num() == 10)) return false;
        int32 Seen = 0;
        int32 Unseen = 0;
        for (int32 Index = 0; Index < 10; ++Index)
        {
            const AptitudePathProjection& Truth = Core.Paths[Index];
            const FPWProjectionPathRow& Row = Model.Paths[Index];
            TestTrue(*(Row.Name.ToString() + TEXT(" agrees with the core")), Row.BandValue == Truth.Band && Row.bUnknown == (Truth.Band == AptitudeBand::InsufficientObservation) && Row.Unknown.Num() == static_cast<int32>(Truth.Unobserved.size()));
            TestTrue(*(Row.Name.ToString() + TEXT(": unknown is its own words")), (Row.Band.ToString() == TEXT("Unknown so far")) == Row.bUnknown);
            if (Truth.Coverage == AptitudeCoverage::Unknown)
            {
                ++Unseen;
                TestTrue(*(Row.Name.ToString() + TEXT(": a path never touched stays unknown, not weak")), Row.bUnknown && Row.Coverage.ToString() == TEXT("Nothing seen yet") && Row.ObservationCount == 0);
            }
            else
            {
                ++Seen;
                TestTrue(*(Row.Name.ToString() + TEXT(": a touched path counts its observations")), Row.ObservationCount > 0);
            }
            for (const FPWProjectionFacetRow& Facet : Row.Facets)
            {
                if (Facet.StateValue == AptitudeFacetState::Unobserved) TestEqual(*(Facet.Name.ToString() + TEXT(" unobserved reads Unknown")), Facet.State.ToString(), FString(TEXT("Unknown")));
                else TestTrue(*(Facet.Name.ToString() + TEXT(" observed no longer reads Unknown")), Facet.State.ToString() != TEXT("Unknown"));
            }
        }
        TestTrue(TEXT("some paths are touched and some never are"), Seen > 0 && Unseen > 0);
    }
    {
        // An inconsistent ledger is refused, not shown as ten invented unknowns.
        AptitudeLedger Broken;
        Broken.Items.push_back(A6Evidence("school.lesson.z", AptitudeSource::StructureModel, AptitudeFacet::Analysis, "family.a"));
        Broken.Items.push_back(A6Evidence("school.lesson.a", AptitudeSource::StructureModel, AptitudeFacet::Analysis, "family.b"));
        TestFalse(TEXT("an unsorted ledger is unavailable"), PWSchoolProjection::BuildModel(Broken).bAvailable);
        FA6FakeProjection Fake;
        Fake.bReadable = false;
        TestFalse(TEXT("an unreadable world is unavailable"), PWSchoolProjection::ReadModel(Fake).bAvailable);
        TestEqual(TEXT("and it was asked once"), Fake.Reads, 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA6ProjectionWidgetTest, "PerfectWorld.Phase2A6.TheTerminalShowsUnknownKeysAndFits",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA6ProjectionWidgetTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace A6Tests;
    int32 Closed = 0;
    const FPWProjectionModel Model = PWSchoolProjection::BuildModel(AptitudeLedger());
    const TSharedRef<SPWSchoolProjection> Panel = SNew(SPWSchoolProjection).Model(Model).Space(FVector2D(1280.0, 720.0)).OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
    const TArray<FString> Texts = QwShownTexts(Panel);
    TestTrue(TEXT("title and the promise that unknown is not unsuitable"), QwHas(Texts, TEXT("What the school terminal has seen")) && A6Has(Texts, TEXT("It never means unsuitable")));
    TestEqual(TEXT("ten paths"), QwCountTag(Panel, PWSchoolProjection::RowTag), 10);
    TestEqual(TEXT("one detail"), QwCountTag(Panel, PWSchoolProjection::DetailTag), 1);
    TestEqual(TEXT("an empty ledger says nothing has been seen"), QwCountTag(Panel, PWSchoolProjection::NothingSeenTag), 1);
    TestEqual(TEXT("five facets are listed for the chosen path"), QwCountTag(Panel, PWSchoolProjection::FacetTag), 5);
    TestTrue(TEXT("the unknown heading and list are shown"), A6Has(Texts, TEXT("Not yet seen (unknown, not weak)")) && QwCountTag(Panel, PWSchoolProjection::UnknownTag) == 1);
    TestEqual(TEXT("the first path is chosen"), Panel->GetSelectedId(), Model.Paths[0].Id);
    TestTrue(TEXT("choosing another changes the detail"), Panel->SelectRow(Model.Paths[1].Id) && A6Has(QwShownTexts(Panel), Model.Paths[1].Printout.ToString()) && !A6Has(QwShownTexts(Panel), Model.Paths[0].Printout.ToString()));
    TestFalse(TEXT("choosing it again does not"), Panel->SelectRow(Model.Paths[1].Id));
    TestFalse(TEXT("an unknown id is ignored"), Panel->SelectRow(TEXT("path.nothing")));
    const FGeometry Geometry;
    TestTrue(TEXT("Down is handled and moves on"), Panel->OnKeyDown(Geometry, QwKey(EKeys::Down)).IsEventHandled() && Panel->GetSelectedId() == Model.Paths[2].Id);
    Panel->OnKeyDown(Geometry, QwKey(EKeys::Up));
    TestEqual(TEXT("Up goes back"), Panel->GetSelectedId(), Model.Paths[1].Id);
    TestTrue(TEXT("Escape goes back once"), Panel->OnKeyDown(Geometry, QwKey(EKeys::Escape)).IsEventHandled() && Closed == 1);
    Panel->OnKeyDown(Geometry, QwKey(EKeys::Escape, true));
    TestEqual(TEXT("held does not go back twice"), Closed, 1);
    TestTrue(TEXT("the Back button goes back"), QwClick(Panel, TEXT("Back")) && Closed == 2);
    for (const FVector2D Size : {FVector2D(853.0, 480.0), FVector2D(1920.0, 1080.0), FVector2D(400.0, 600.0)})
    {
        const TSharedRef<SPWSchoolProjection> Sized = SNew(SPWSchoolProjection).Model(Model).Space(Size);
        TestEqual(*FString::Printf(TEXT("%.0fx%.0f lists every path"), Size.X, Size.Y), QwCountTag(Sized, PWSchoolProjection::RowTag), 10);
        TestTrue(TEXT("and offers Back"), QwHas(QwShownTexts(Sized), TEXT("Back")));
    }
    const TSharedRef<SPWSchoolProjection> Unavailable = SNew(SPWSchoolProjection).Model(FPWProjectionModel()).Space(FVector2D(1280.0, 720.0));
    TestEqual(TEXT("an unreadable terminal says so"), QwCountTag(Unavailable, PWSchoolProjection::UnavailableTag), 1);
    TestEqual(TEXT("and lists no paths"), QwCountTag(Unavailable, PWSchoolProjection::RowTag), 0);
    // Open requests: nothing happens until the host installs a handler.
    PWSchoolProjection::SetOpenHandler(nullptr);
    PWSchoolPlanner::SetOpenHandler(nullptr);
    TestFalse(TEXT("no handler, no open"), PWSchoolProjection::RequestOpen() || PWSchoolPlanner::RequestOpen());
    int32 Opened = 0;
    PWSchoolProjection::SetOpenHandler([&Opened]() { ++Opened; });
    PWSchoolPlanner::SetOpenHandler([&Opened]() { Opened += 10; });
    TestTrue(TEXT("handlers are called"), PWSchoolProjection::RequestOpen() && PWSchoolPlanner::RequestOpen() && Opened == 11);
    PWSchoolProjection::SetOpenHandler(nullptr);
    PWSchoolPlanner::SetOpenHandler(nullptr);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
