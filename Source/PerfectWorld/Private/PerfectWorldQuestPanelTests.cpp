// The quest window (order F3), part 1: its words (every projection key resolves, unknown keys read neutrally), how rows and the detail
// are formatted, and the session's tracked id and open request. Run in the editor under "PerfectWorld.QuestPanel". The widget tests (sections,
// empty states, tracking, HUD line, keys, fit) are in PerfectWorldQuestPanelWidgetTests.cpp. What needs a real window (keyboard focus, the
// J key through Enhanced Input, the pause menu and HUD hooks) is proved in the editor, not here.

#include "PerfectWorldQuestPanelTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQwWordsTest, "PerfectWorld.QuestPanel.EveryKeyHasWordsAndUnknownKeysReadNeutrally",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQwWordsTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace PerfectWorld;
    // Every item the fullest world can project, plus the generic household agreement, reads as real sentences.
    QuestLogView Generic = MakeQuestLog([] { WorldState World = MakeWorldState(78); QwSay(World, "person.household_adult", {"fact.evening_rota_agreed"}); return World; }(), QwAt(2, 19, 0));
    TSet<FString> Whats;
    int32 Checked = 0;
    const QuestLogView Rich = QwRichLog();
    for (const QuestLogView* Log : {&Rich, &Generic})
    {
        for (const std::vector<QuestItem>* Section : {&Log->Today, &Log->Promises, &Log->Discoveries})
        {
            for (const QuestItem& Item : *Section)
            {
                const FString Name = FString(UTF8_TO_TCHAR(Item.Id.c_str()));
                const FPWQuestRow Row = PWQuest::MakeRow(Item);
                for (const FText& Text : {Row.Title, Row.What, Row.Who, Row.Where, Row.When, Row.StatusLabel})
                {
                    TestFalse(*(Name + TEXT(": has words")), Text.IsEmpty());
                    TestFalse(*(Name + TEXT(": never shows a raw key: ") + Text.ToString()), QwLooksRaw(Text.ToString()));
                }
                TestFalse(*(Name + TEXT(": is not the neutral fallback")), Row.Title.ToString() == PWQuest::TitleText(QwItem("x", "quest.today.nonsense")).ToString());
                if (Item.Section == QuestSection::Promises) Whats.Add(Row.What.ToString());
                ++Checked;
            }
        }
    }
    TestTrue(TEXT("the fullest log has many rows"), Checked > 25);
    TestEqual(TEXT("each of the twelve agreements in the fullest world, and the generic one, has its own sentence"), Whats.Num(), 13);

    const FString NeutralTitle = PWQuest::TitleText(QwItem("x", "quest.today.nonsense")).ToString();
    const FString NeutralWhat = PWQuest::WhatText(QwItem("x", "quest.today.nonsense")).ToString();
    TestFalse(TEXT("the neutral wording is real"), NeutralTitle.IsEmpty() || NeutralWhat.IsEmpty() || QwLooksRaw(NeutralTitle) || QwLooksRaw(NeutralWhat));
    for (const char* Unknown : {"", "quest.", "Quest.Today.School_Bell", "quest.today.school_bell.extra", "fact.debashis_pal.bring_promised", "quest.discover.nowhere"})
    {
        const QuestItem Item = QwItem("x", Unknown, QuestSection::Discoveries);
        TestEqual(*FString::Printf(TEXT("unknown key '%hs' reads the neutral title"), Unknown), PWQuest::TitleText(Item).ToString(), NeutralTitle);
        TestEqual(*FString::Printf(TEXT("and the neutral description for '%hs'"), Unknown), PWQuest::WhatText(Item).ToString(), NeutralWhat);
        TestFalse(*FString::Printf(TEXT("'%hs' shows no raw text anywhere"), Unknown),
                  QwLooksRaw(PWQuest::WhoText(Item).ToString()) || QwLooksRaw(PWQuest::WhereText(Item).ToString()) || QwLooksRaw(PWQuest::WhenText(Item).ToString()));
    }
    QuestItem Stray = QwItem("x", "journal.agreement.someone_new.thing", QuestSection::Promises);
    Stray.PersonId = "person.someone_new";
    TestFalse(TEXT("an unmapped agreement reads a sentence, not its key"), PWQuest::WhatText(Stray).IsEmpty() || QwLooksRaw(PWQuest::WhatText(Stray).ToString()));
    TestEqual(TEXT("the same sentence as any other unmapped agreement"), PWQuest::WhatText(Stray).ToString(),
              PWQuest::WhatText(QwItem("y", "journal.agreement.another.unmapped", QuestSection::Promises)).ToString());
    TestFalse(TEXT("and names nobody it does not know"), QwLooksRaw(PWQuest::TitleText(Stray).ToString()) || QwLooksRaw(PWQuest::WhoText(Stray).ToString()));
    TestEqual(TEXT("an unknown person reads neutrally"), PWQuest::WhoText(Stray).ToString(), FString(TEXT("Someone you know")));
    Stray.PlaceKey = "location.nabapara.nowhere";
    TestFalse(TEXT("an unknown place reads neutrally"), QwLooksRaw(PWQuest::WhereText(Stray).ToString()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQwFormattingTest, "PerfectWorld.QuestPanel.TimesStatusesAndDetailFormat",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQwFormattingTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace PerfectWorld;
    TestEqual(TEXT("the time of day, 24 hour"), PWQuest::TimeText(9 * 60 + 5).ToString(), FString(TEXT("09:05")));
    TestEqual(TEXT("midnight and the last minute"), PWQuest::TimeText(0).ToString() + TEXT(" ") + PWQuest::TimeText(1439).ToString(), FString(TEXT("00:00 23:59")));
    TestEqual(TEXT("two windows"), PWQuest::WindowsText({{600, 750}, {1050, 1230}}).ToString(), FString(TEXT("10:00 to 12:30, 17:30 to 20:30")));

    const QuestLogView Log = QwRichLog();
    const QuestItem* School = QuestLogFind(Log, QuestIdSchoolBell);
    const QuestItem* Rupa = QuestLogFind(Log, "quest.promise.rupa_ghosh.visit_promised");
    const QuestItem* Dishes = QuestLogFind(Log, "quest.today.evening_dishes");
    const QuestItem* Chores = QuestLogFind(Log, QuestIdCommitment);
    if (!TestNotNull(TEXT("school"), School) || !TestNotNull(TEXT("rupa"), Rupa) || !TestNotNull(TEXT("dishes"), Dishes) || !TestNotNull(TEXT("commitment"), Chores)) return false;

    const FPWQuestRow SchoolRow = PWQuest::MakeRow(*School);
    TestEqual(TEXT("school title"), SchoolRow.Title.ToString(), FString(TEXT("Be at school by the first bell")));
    TestEqual(TEXT("school when"), SchoolRow.When.ToString(), FString(TEXT("By 10:00, the first bell.")));
    TestEqual(TEXT("school short when, for the HUD"), PWQuest::ShortWhenText(*School).ToString(), FString(TEXT("By 10:00")));
    TestEqual(TEXT("school where"), SchoolRow.Where.ToString(), FString(TEXT("Nabapara Vidyalaya")));
    TestEqual(TEXT("school who has no one in particular"), SchoolRow.Who.ToString(), FString(TEXT("No one in particular")));
    TestEqual(TEXT("at 09:00 the gates are not open yet: later today"), SchoolRow.StatusLabel.ToString(), FString(TEXT("Later today")));

    const FPWQuestRow RupaRow = PWQuest::MakeRow(*Rupa);
    TestEqual(TEXT("a promise is with its person"), RupaRow.Title.ToString(), FString(TEXT("With Rupa Ghosh")));
    TestEqual(TEXT("and names them"), RupaRow.Who.ToString(), FString(TEXT("Rupa Ghosh")));
    TestEqual(TEXT("and where"), RupaRow.Where.ToString(), FString(TEXT("The tea row house, Nabapara Lane")));
    TestTrue(TEXT("and the visiting hours from the door"), RupaRow.When.ToString().Contains(TEXT("Visiting hours: 16:00 to 19:30.")));
    TestEqual(TEXT("a household promise names the parent in lower case after 'With'"),
              PWQuest::TitleText(*QuestLogFind(Log, "quest.promise.evening_rota_dishes_weekdays")).ToString(), FString(TEXT("With your parent")));
    TestTrue(TEXT("a weekday arrangement says so"), PWQuest::MakeRow(*Dishes).When.ToString().Contains(TEXT("Weekdays only.")));
    TestTrue(TEXT("and when evening begins"), PWQuest::MakeRow(*Dishes).When.ToString().Contains(TEXT("From 18:00, when evening begins.")));
    TestTrue(TEXT("the commitment is in progress"), PWQuest::MakeRow(*Chores).Title.ToString() == TEXT("In progress: Household chores") && PWQuest::MakeRow(*Chores).When.ToString() == TEXT("Now."));

    TSet<FString> Labels;
    for (const QuestSection Section : {QuestSection::Today, QuestSection::Promises, QuestSection::Discoveries})
    {
        for (const QuestStatus Status : {QuestStatus::Open, QuestStatus::Due, QuestStatus::Done, QuestStatus::Missed})
        {
            const FString Label = PWQuest::StatusText(Section, Status).ToString();
            TestFalse(TEXT("every status has a label"), Label.IsEmpty() || QwLooksRaw(Label));
            Labels.Add(Label);
        }
    }
    TestTrue(TEXT("and the labels differ by meaning"), Labels.Num() >= 7);

    const FPWQuestModel Model = PWQuest::BuildModel(Log, FText::FromString(TEXT("Mon 2 Jun · 09:00")), TEXT("quest.today.school_bell"));
    if (!TestEqual(TEXT("three sections"), Model.Sections.Num(), 3)) return false;
    TestTrue(TEXT("in order"), Model.Sections[0].Section == QuestSection::Today && Model.Sections[1].Section == QuestSection::Promises && Model.Sections[2].Section == QuestSection::Discoveries);
    TestEqual(TEXT("headings"), Model.Sections[0].Heading.ToString() + TEXT("/") + Model.Sections[1].Heading.ToString() + TEXT("/") + Model.Sections[2].Heading.ToString(), FString(TEXT("Today/Promises/Discoveries")));
    TestEqual(TEXT("every projected item is a row"), Model.RowCount(), static_cast<int32>(Log.Count()));
    TestEqual(TEXT("the clock words are kept"), Model.Clock.ToString(), FString(TEXT("Mon 2 Jun · 09:00")));
    const FPWQuestRow* Marked = Model.FindRow(TEXT("quest.today.school_bell"));
    TestTrue(TEXT("the tracked id marks its row"), Marked != nullptr && Marked->bTracked);
    int32 TrackedCount = 0;
    for (const FPWQuestSectionModel& Section : Model.Sections) for (const FPWQuestRow& Row : Section.Rows) TrackedCount += Row.bTracked ? 1 : 0;
    TestEqual(TEXT("and only that one"), TrackedCount, 1);
    TestEqual(TEXT("an unknown tracked id marks none"), PWQuest::BuildModel(Log, FText::GetEmpty(), TEXT("quest.nothing")).FirstRow()->bTracked, false);

    const FPWQuestModel Empty = PWQuest::BuildModel(MakeQuestLog(MakeWorldState(1), QwAt(7, 8, 0)), FText::GetEmpty(), FString());
    TestTrue(TEXT("a bare weekend is an available, row-less model with its three sections"), Empty.bAvailable && Empty.Sections.Num() == 3 && Empty.FirstRow() != nullptr);
    TestFalse(TEXT("no world means an unavailable model"), PWQuest::ReadModel(nullptr).bAvailable);
    TestFalse(TEXT("and no tracked line"), PWQuest::ReadTrackedLine(nullptr).bVisible);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQwSessionTest, "PerfectWorld.QuestPanel.TrackingIsPerSessionAndTheOpenRequestIsOneShot",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FQwSessionTest::RunTest(const FString& Parameters)
{
    using namespace QwTests;
    using namespace PerfectWorld;
    PWQuest::SetTrackedId(FString());
    const uint32 Start = PWQuest::TrackedRevision();
    TestTrue(TEXT("tracking turns on"), PWQuest::ToggleTracked(TEXT("a")) && PWQuest::GetTrackedId() == TEXT("a"));
    TestTrue(TEXT("another one replaces it"), PWQuest::ToggleTracked(TEXT("b")) && PWQuest::GetTrackedId() == TEXT("b"));
    TestFalse(TEXT("the same one turns it off"), PWQuest::ToggleTracked(TEXT("b")));
    TestTrue(TEXT("and nothing is tracked"), PWQuest::GetTrackedId().IsEmpty());
    TestEqual(TEXT("each change bumps the revision once"), PWQuest::TrackedRevision(), Start + 3);
    PWQuest::SetTrackedId(FString());
    TestEqual(TEXT("clearing what is clear changes nothing"), PWQuest::TrackedRevision(), Start + 3);

    PWQuest::SetToggleHandler(nullptr);
    TestFalse(TEXT("J does nothing before the HUD installs its handler"), PWQuest::RequestToggle());
    int32 Opened = 0;
    PWQuest::SetToggleHandler([&Opened]() { ++Opened; });
    TestTrue(TEXT("the request reaches the handler"), PWQuest::RequestToggle() && Opened == 1);
    PWQuest::SetToggleHandler(nullptr);
    TestFalse(TEXT("and clearing it stops that"), PWQuest::RequestToggle());

    TestFalse(TEXT("no pending open"), PWQuest::ConsumeOpenOnNextPause());
    PWQuest::SetOpenOnNextPause(true);
    TestTrue(TEXT("the pause menu sees the request once"), PWQuest::ConsumeOpenOnNextPause());
    TestFalse(TEXT("and only once"), PWQuest::ConsumeOpenOnNextPause());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
