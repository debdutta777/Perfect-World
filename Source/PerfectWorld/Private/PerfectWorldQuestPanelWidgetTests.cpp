// The quest window (order F3), part 2: its sections, detail, empty and unavailable states, tracking, the HUD line, its keys and its fit at every
// window size and UI scale. Run in the editor under "PerfectWorld.QuestPanel". The widgets are the real ones, built without a window:
// SlatePrepass evaluates bound text and visibility and buttons are clicked with SButton::SimulateClick.

#include "PerfectWorldQuestPanelTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "UI/SPWQuestPanel.h"
#include "UI/SPWQuestTracker.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQwPanelStatesTest, "PerfectWorld.QuestPanel.SectionsDetailAndEmptyStatesAreShown",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQwPanelStatesTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace PerfectWorld;
    PWQuest::SetTrackedId(FString());
    {
        const FPWQuestModel Model = PWQuest::BuildModel(QwRichLog(), FText::FromString(TEXT("Mon 2 Jun · 09:00")), FString());
        const TSharedRef<SPWQuestPanel> Panel = SNew(SPWQuestPanel).Model(Model).Space(FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = QwShownTexts(Panel);
        TestTrue(TEXT("the title and clock"), QwHas(Texts, TEXT("Quests")) && QwHas(Texts, TEXT("Mon 2 Jun · 09:00")));
        TestTrue(TEXT("the three section headings"), QwHas(Texts, TEXT("Today")) && QwHas(Texts, TEXT("Promises")) && QwHas(Texts, TEXT("Discoveries")));
        TestEqual(TEXT("every row is listed"), QwCountTag(Panel, PWQuest::RowTag), Model.RowCount());
        TestEqual(TEXT("no empty states beside rows"), QwCountTag(Panel, PWQuest::EmptySectionTag), 0);
        TestTrue(TEXT("the first row is chosen, the first of Today"), Panel->GetSelectedId() == Model.FirstRow()->Id);
        TestTrue(TEXT("Back and Track are offered"), QwHas(Texts, TEXT("Back")) && QwHas(Texts, TEXT("Track this")));
        TestEqual(TEXT("the detail is shown"), QwCountTag(Panel, PWQuest::DetailTag), 1);
        const FPWQuestRow& First = *Model.FirstRow();
        for (const FText& Text : {First.Title, First.What, First.Who, First.Where, First.When})
        {
            TestTrue(*(Text.ToString() + TEXT(" is shown for the chosen quest")), QwHas(Texts, Text.ToString()));
        }
        TestTrue(TEXT("the detail's field labels"), QwHas(Texts, TEXT("WHAT")) && QwHas(Texts, TEXT("WHO")) && QwHas(Texts, TEXT("WHERE")) && QwHas(Texts, TEXT("WHEN")) && QwHas(Texts, TEXT("STATUS")));

        // Choosing another quest moves the detail with it.
        const FPWQuestRow* Rupa = Model.FindRow(TEXT("quest.promise.rupa_ghosh.visit_promised"));
        if (!TestNotNull(TEXT("rupa's promise is listed"), Rupa)) return false;
        TestTrue(TEXT("choosing a row changes the choice"), Panel->SelectRow(Rupa->Id));
        TestFalse(TEXT("choosing it again does not"), Panel->SelectRow(Rupa->Id));
        TestFalse(TEXT("an unknown id is ignored"), Panel->SelectRow(TEXT("quest.nothing")));
        const TArray<FString> After = QwShownTexts(Panel);
        TestTrue(TEXT("the detail now reads the promise"), QwHas(After, Rupa->What.ToString()) && QwHas(After, Rupa->Who.ToString()) && QwHas(After, Rupa->Where.ToString()) && QwHas(After, Rupa->When.ToString()));
        TestFalse(TEXT("and no longer the first quest"), QwHas(After, First.What.ToString()));

        // The Track button tracks it, and says so.
        TestTrue(TEXT("Track this tracks"), QwClick(Panel, TEXT("Track this")));
        TestEqual(TEXT("the session tracks the chosen id"), PWQuest::GetTrackedId(), Rupa->Id);
        TestTrue(TEXT("the button now offers to stop and the detail says tracked"), QwHas(QwShownTexts(Panel), TEXT("Stop tracking")) && QwHas(QwShownTexts(Panel), TEXT("Tracked: shown on your screen")));
        TestTrue(TEXT("Stop tracking stops"), QwClick(Panel, TEXT("Stop tracking")) && PWQuest::GetTrackedId().IsEmpty());
        PWQuest::SetTrackedId(TEXT("quest.today.school_bell"));
        const TSharedRef<SPWQuestPanel> Reopened = SNew(SPWQuestPanel).Model(PWQuest::BuildModel(QwRichLog(), FText::GetEmpty(), PWQuest::GetTrackedId())).Space(FVector2D(1280.0, 720.0));
        TestEqual(TEXT("a reopened window starts on the tracked quest"), Reopened->GetSelectedId(), FString(TEXT("quest.today.school_bell")));
        PWQuest::SetTrackedId(FString());
    }
    {
        // A weekend with nothing agreed or planned: Today and Promises are empty, the places on the lane remain.
        const FPWQuestModel Model = PWQuest::BuildModel(MakeQuestLog(MakeWorldState(2), QwAt(7, 8, 0)), FText::GetEmpty(), FString());
        const TSharedRef<SPWQuestPanel> Panel = SNew(SPWQuestPanel).Model(Model).Space(FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = QwShownTexts(Panel);
        TestEqual(TEXT("two empty states"), QwCountTag(Panel, PWQuest::EmptySectionTag), 2);
        TestTrue(TEXT("with their notes"), QwHas(Texts, PWQuest::SectionEmptyNote(QuestSection::Today).ToString()) && QwHas(Texts, PWQuest::SectionEmptyNote(QuestSection::Promises).ToString()));
        TestEqual(TEXT("and the discoveries as rows"), QwCountTag(Panel, PWQuest::RowTag), static_cast<int32>(Model.Sections[2].Rows.Num()));
    }
    {
        // Nothing at all: three empty states, a prompt, Track disabled but shown, and Back.
        FPWQuestModel Bare;
        Bare.bAvailable = true;
        for (const QuestSection Section : {QuestSection::Today, QuestSection::Promises, QuestSection::Discoveries})
        {
            FPWQuestSectionModel& Part = Bare.Sections.AddDefaulted_GetRef();
            Part.Section = Section;
            Part.Heading = PWQuest::SectionHeading(Section);
            Part.EmptyNote = PWQuest::SectionEmptyNote(Section);
        }
        const TSharedRef<SPWQuestPanel> Panel = SNew(SPWQuestPanel).Model(Bare).Space(FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = QwShownTexts(Panel);
        TestEqual(TEXT("every section says why it is empty"), QwCountTag(Panel, PWQuest::EmptySectionTag), 3);
        TestEqual(TEXT("no rows"), QwCountTag(Panel, PWQuest::RowTag), 0);
        TestTrue(TEXT("the detail explains the window"), Texts.ContainsByPredicate([](const FString& Text) { return Text.StartsWith(TEXT("Nothing here yet.")); }));
        TestTrue(TEXT("nothing is chosen"), Panel->GetSelectedId().IsEmpty());
        TestFalse(TEXT("so Track does nothing"), QwClick(Panel, TEXT("Track this")) && !PWQuest::GetTrackedId().IsEmpty());
        TestTrue(TEXT("Back is offered"), QwHas(Texts, TEXT("Back")));
    }
    {
        const TSharedRef<SPWQuestPanel> Panel = SNew(SPWQuestPanel).Model(FPWQuestModel()).Space(FVector2D(1280.0, 720.0));
        TestEqual(TEXT("an unreadable world says so"), QwCountTag(Panel, PWQuest::UnavailableTag), 1);
        TestEqual(TEXT("and shows no rows or sections"), QwCountTag(Panel, PWQuest::RowTag) + QwCountTag(Panel, PWQuest::EmptySectionTag), 0);
        TestTrue(TEXT("Back is still offered"), QwHas(QwShownTexts(Panel), TEXT("Back")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQwTrackedLineTest, "PerfectWorld.QuestPanel.TheHudLineShowsTheTrackedQuest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQwTrackedLineTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace PerfectWorld;
    PWQuest::SetTrackedId(FString());
    // The portable rules for the line.
    const QuestLogView Log = QwRichLog();
    TestFalse(TEXT("nothing tracked, nothing shown"), PWQuest::BuildTrackedLine(Log, FString()).bVisible);
    TestFalse(TEXT("an unknown id shows nothing"), PWQuest::BuildTrackedLine(Log, TEXT("quest.nothing")).bVisible);
    const FPWQuestTrackedLine School = PWQuest::BuildTrackedLine(Log, TEXT("quest.today.school_bell"));
    TestTrue(TEXT("school is shown"), School.bVisible && School.Id == TEXT("quest.today.school_bell"));
    TestEqual(TEXT("its title"), School.Title.ToString(), FString(TEXT("Be at school by the first bell")));
    TestEqual(TEXT("and when"), School.When.ToString(), FString(TEXT("By 10:00")));
    TestTrue(TEXT("a promise shows its visiting hours"), PWQuest::BuildTrackedLine(Log, TEXT("quest.promise.rupa_ghosh.visit_promised")).When.ToString() == TEXT("Visiting 16:00 to 19:30"));
    WorldState Attended = MakeWorldState(3);
    AttendanceDay Day;
    Day.Day = 0;
    Day.Outcome = AttendanceOutcome::OnTime;
    Day.ArrivalMinute = 9 * 60 + 55;
    Day.PeriodsHeld = 6;
    Attended.DayLoop.Attendance.Days.push_back(Day);
    TestFalse(TEXT("a finished objective leaves the HUD"), PWQuest::BuildTrackedLine(MakeQuestLog(Attended, QwAt(2, 11, 0)), TEXT("quest.today.school_bell")).bVisible);

    // The widget, in a real lane world: it reads the clock and the session's tracked id.
    FQwScene Scene;
    if (!TestNotNull(TEXT("hud"), Scene.Hud) || !TestNotNull(TEXT("the lane world"), UPWLaneLifeSubsystem::Get(Scene.Hud))) return false;
    bool bShow = true;
    const TSharedRef<SPWQuestTracker> Tracker = SNew(SPWQuestTracker)
        .WorldContext(TWeakObjectPtr<const UObject>(Scene.Hud))
        .ShowTracker(TAttribute<bool>::CreateLambda([&bShow]() { return bShow; }));
    Tracker->Refresh();
    TestFalse(TEXT("untracked: no line"), Tracker->GetLine().bVisible);
    TestEqual(TEXT("and no card shown"), QwCountTag(Tracker, PWQuest::TrackerTag), 0);

    PWQuest::SetTrackedId(TEXT("quest.today.school_bell"));
    Tracker->Refresh();
    // The lane world starts on a Monday; whether it is a school day depends on the content calendar, which the model reads the same way.
    const bool bSchoolDay = PWQuest::ReadModel(Scene.Hud).FindRow(TEXT("quest.today.school_bell")) != nullptr;
    TestEqual(TEXT("a tracked school bell is on the line on a school day"), Tracker->GetLine().bVisible, bSchoolDay);
    if (bSchoolDay)
    {
        const TArray<FString> Texts = QwShownTexts(Tracker);
        TestTrue(TEXT("the line shows title and time"), QwHas(Texts, TEXT("Be at school by the first bell")) && QwHas(Texts, TEXT("By 10:00")));
        TestEqual(TEXT("as one card"), QwCountTag(Tracker, PWQuest::TrackerTag), 1);
        bShow = false;
        TestEqual(TEXT("hidden under the menus"), QwCountTag(Tracker, PWQuest::TrackerTag), 0);
        bShow = true;
    }
    PWQuest::SetTrackedId(TEXT("quest.nothing"));
    Tracker->Refresh();
    TestFalse(TEXT("a tracked id that is gone leaves the line"), Tracker->GetLine().bVisible);
    TestFalse(TEXT("and with no world the widget shows nothing"), SNew(SPWQuestTracker).WorldContext(TWeakObjectPtr<const UObject>()).ShowTracker(true)->GetLine().bVisible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQwPanelKeysTest, "PerfectWorld.QuestPanel.KeysChooseTrackAndGoBack",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQwPanelKeysTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace PerfectWorld;
    PWQuest::SetTrackedId(FString());
    int32 Closed = 0;
    const FPWQuestModel Model = PWQuest::BuildModel(QwRichLog(), FText::GetEmpty(), FString());
    const TSharedRef<SPWQuestPanel> Panel = SNew(SPWQuestPanel)
        .Model(Model)
        .Space(FVector2D(853.0, 480.0))
        .OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
    Panel->SlatePrepass(1.0f);
    const FGeometry Geometry;

    TArray<FString> Order;
    for (const FPWQuestSectionModel& Section : Model.Sections) for (const FPWQuestRow& Row : Section.Rows) Order.Add(Row.Id);
    if (!TestTrue(TEXT("a long list to move through"), Order.Num() > 20)) return false;
    TestEqual(TEXT("starts on the first"), Panel->GetSelectedId(), Order[0]);
    TestFalse(TEXT("Up at the top is left to the focus navigation"), Panel->OnKeyDown(Geometry, QwKey(EKeys::Up)).IsEventHandled());
    TestEqual(TEXT("and does not move"), Panel->GetSelectedId(), Order[0]);
    for (const FKey& Down : {EKeys::Down, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down})
    {
        const FString Before = Panel->GetSelectedId();
        TestTrue(*(Down.ToString() + TEXT(" is handled")), Panel->OnKeyDown(Geometry, QwKey(Down)).IsEventHandled());
        TestEqual(*(Down.ToString() + TEXT(" chooses the next row")), Panel->GetSelectedId(), Order[Order.IndexOfByKey(Before) + 1]);
    }
    TestEqual(TEXT("three steps down"), Panel->GetSelectedId(), Order[3]);
    Panel->OnKeyDown(Geometry, QwKey(EKeys::Down, true));
    TestEqual(TEXT("a held key repeats the choice"), Panel->GetSelectedId(), Order[4]);
    Panel->OnKeyDown(Geometry, QwKey(EKeys::Up));
    TestEqual(TEXT("and Up goes back"), Panel->GetSelectedId(), Order[3]);

    // Sections: Page Down and RB go to the first row of the next one, Page Up to the previous.
    Panel->OnKeyDown(Geometry, QwKey(EKeys::PageDown));
    TestEqual(TEXT("Page Down jumps to the first promise"), Panel->GetSelectedId(), Model.Sections[1].Rows[0].Id);
    Panel->OnKeyDown(Geometry, QwKey(EKeys::Gamepad_RightShoulder));
    TestEqual(TEXT("RB jumps to the first discovery"), Panel->GetSelectedId(), Model.Sections[2].Rows[0].Id);
    Panel->OnKeyDown(Geometry, QwKey(EKeys::PageDown));
    TestEqual(TEXT("there is no section after the last"), Panel->GetSelectedId(), Model.Sections[2].Rows[0].Id);
    Panel->OnKeyDown(Geometry, QwKey(EKeys::PageUp));
    TestEqual(TEXT("Page Up goes back a section"), Panel->GetSelectedId(), Model.Sections[1].Rows[0].Id);
    for (int32 Step = 0; Step < Order.Num(); ++Step) Panel->OnKeyDown(Geometry, QwKey(EKeys::Down));
    TestEqual(TEXT("the last row is the end of the list"), Panel->GetSelectedId(), Order.Last());
    TestFalse(TEXT("Down at the end is left to the focus navigation, which reaches the buttons"), Panel->OnKeyDown(Geometry, QwKey(EKeys::Down)).IsEventHandled());

    // Tracking: T and X toggle once per press.
    TestTrue(TEXT("T is handled"), Panel->OnKeyDown(Geometry, QwKey(EKeys::T)).IsEventHandled());
    TestEqual(TEXT("and tracks the chosen quest"), PWQuest::GetTrackedId(), Order.Last());
    Panel->OnKeyDown(Geometry, QwKey(EKeys::T, true));
    TestEqual(TEXT("a held T does not toggle again"), PWQuest::GetTrackedId(), Order.Last());
    Panel->OnKeyDown(Geometry, QwKey(EKeys::Gamepad_FaceButton_Left));
    TestTrue(TEXT("the controller's X stops tracking"), PWQuest::GetTrackedId().IsEmpty());
    TestTrue(TEXT("scrolling the detail is handled"), Panel->OnKeyDown(Geometry, QwKey(EKeys::Gamepad_RightStick_Down)).IsEventHandled());

    // Back: every back key, on the first press only.
    int32 Expected = 0;
    for (const FKey& Back : {EKeys::Escape, EKeys::J, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Right, EKeys::Gamepad_LeftShoulder})
    {
        TestTrue(*(Back.ToString() + TEXT(" is handled")), Panel->OnKeyDown(Geometry, QwKey(Back)).IsEventHandled());
        TestEqual(*(Back.ToString() + TEXT(" goes back")), Closed, ++Expected);
        Panel->OnKeyDown(Geometry, QwKey(Back, true));
        TestEqual(*(Back.ToString() + TEXT(" held does not go back twice")), Closed, Expected);
    }
    TestFalse(TEXT("an unrelated key is left alone"), Panel->OnKeyDown(Geometry, QwKey(EKeys::A)).IsEventHandled());
    TestEqual(TEXT("moving and tracking never close"), Closed, Expected);
    TestTrue(TEXT("the Back button is there"), QwClick(Panel, TEXT("Back")));
    TestEqual(TEXT("and goes back"), Closed, Expected + 1);

    // A row can be clicked to choose it.
    TestTrue(TEXT("clicking a row chooses it"), QwClick(Panel, Model.Sections[0].Rows[1].Title.ToString()) && Panel->GetSelectedId() == Model.Sections[0].Rows[1].Id);
    PWQuest::SetTrackedId(FString());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQwPanelFitTest, "PerfectWorld.QuestPanel.FitsEveryWindowAndScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQwPanelFitTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace PerfectWorld;
    PWQuest::SetTrackedId(FString());
    // 853x480 and 1280x720 are 1280x720 and 1920x1080 windows at 150% UI scale; 1280x720 and 1920x1080 are at 100%; 640x360 is the narrow stacked case.
    const FVector2D Sizes[] = {FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0 / 1.5, 1080.0 / 1.5), FVector2D(1920.0, 1080.0), FVector2D(640.0, 360.0)};
    const FPWQuestModel Full = PWQuest::BuildModel(QwRichLog(), FText::FromString(TEXT("Mon 2 Jun · 09:00")), FString());
    const FPWQuestModel Bare = PWQuest::BuildModel(MakeQuestLog(MakeWorldState(4), QwAt(7, 8, 0)), FText::GetEmpty(), FString());
    for (const FVector2D& Size : Sizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
        const float W = static_cast<float>(Size.X);
        const float H = static_cast<float>(Size.Y);
        for (const FPWQuestModel* Model : {&Full, &Bare})
        {
            const FString Which = Name + (Model == &Full ? TEXT(" full") : TEXT(" bare"));
            const TSharedRef<SPWQuestPanel> Panel = SNew(SPWQuestPanel).Model(*Model).Space(Size);
            Panel->SlatePrepass(1.0f);
            const FVector2D Desired(Panel->GetDesiredSize());
            AddInfo(FString::Printf(TEXT("%s: quest window desires %.0f x %.0f of %.0f x %.0f"), *Which, Desired.X, Desired.Y, PWQuest::PanelWidth(W, H), PWQuest::PanelMaxHeight(W, H)));
            TestTrue(*(Which + TEXT(": fits the height; long lists scroll inside")), Desired.Y <= PWQuest::PanelMaxHeight(W, H) + 0.5f);
            TestTrue(*(Which + TEXT(": fits the width")), Desired.X <= FMath::Min(W - 32.0f, 1100.0f) + 0.5f);
            TestEqual(*(Which + TEXT(": two panes from 700 wide")), PWQuest::TwoPane(W, H), W >= 700.0f);
            TestEqual(*(Which + TEXT(": compact below 760")), PWQuest::Compact(W, H), H < 760.0f || W < 760.0f);
            TestEqual(*(Which + TEXT(": the arrangement matches the space")), Panel->IsTwoPane(), PWQuest::TwoPane(W, H));
            const TArray<FString> Texts = QwShownTexts(Panel);
            TestTrue(*(Which + TEXT(": Back and Track stay shown")), QwHas(Texts, TEXT("Back")) && QwHas(Texts, TEXT("Track this")));
            if (PWQuest::TwoPane(W, H))
            {
                TestTrue(*(Which + TEXT(": the list and the detail both have room")),
                         PWQuest::ListWidth(W, H) >= 220.0f && PWQuest::PanelWidth(W, H) - PWQuest::ListWidth(W, H) >= 380.0f);
            }
        }
    }

    // The arrangement follows the space: a window made narrower stacks, and wider side by side again, without losing the choice.
    FVector2D Live(1280.0, 720.0);
    const TSharedRef<SPWQuestPanel> Panel = SNew(SPWQuestPanel).Model(Full).Space(TAttribute<FVector2D>::CreateLambda([&Live]() { return Live; }));
    Panel->SelectRow(Full.Sections[1].Rows[2].Id);
    TestTrue(TEXT("side by side at 1280"), Panel->IsTwoPane() && !Panel->RefreshLayout());
    Live = FVector2D(640.0, 360.0);
    TestTrue(TEXT("narrower re-arranges"), Panel->RefreshLayout() && !Panel->IsTwoPane());
    TestEqual(TEXT("keeping the choice"), Panel->GetSelectedId(), Full.Sections[1].Rows[2].Id);
    TestEqual(TEXT("and every row"), QwCountTag(Panel, PWQuest::RowTag), Full.RowCount());
    Live = FVector2D(1920.0, 1080.0);
    TestTrue(TEXT("wider goes back"), Panel->RefreshLayout() && Panel->IsTwoPane() && !Panel->RefreshLayout());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
