// The Today journal screen (R4-05): its text rules (key resolution with a neutral fallback, the time of day, row formatting), its empty
// states, its fit at every window size and UI scale, its keys, and its place in the pause menu. Run in the editor under
// "PerfectWorld.JournalPanel". The widgets are the real ones, built without a window: SlatePrepass evaluates bound text and
// visibility and buttons are clicked with SButton::SimulateClick. What needs a real window (focus, and the rows an actual conversation
// produces) is proved by the journal_today acceptance scenario, not here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameClock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "LaneJournal.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/SPWJournalPanel.h"
#include "UI/SPWPauseMenu.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace PWCreationPause
{
void SetPauseSpaceForTest(float Width, float Height);
} // namespace PWCreationPause

namespace
{
/** Visits every reachable widget, telling it whether it and everything above it is shown. */
void JpWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
    {
        JpWalk(Children->GetChildAt(Index), bShown, Visit);
    }
}

TArray<FString> JpShownTexts(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    TArray<FString> Texts;
    JpWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock"))
        {
            Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        }
    });
    return Texts;
}

int32 JpCountTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    Root->SlatePrepass(1.0f);
    int32 Count = 0;
    JpWalk(Root, true, [&Count, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTag() == FName(Tag)) ++Count;
    });
    return Count;
}

bool JpHas(const TArray<FString>& Texts, const FString& Exact) { return Texts.Contains(Exact); }

/** Clicks the first shown button whose text is exactly this. */
bool JpClick(const TSharedRef<SWidget>& Root, const FString& Label)
{
    Root->SlatePrepass(1.0f);
    TSharedPtr<SButton> Target;
    JpWalk(Root, true, [&Target, &Label](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (!Target.IsValid() && bShown && Widget->GetTypeAsString() == TEXT("SButton") && JpHas(JpShownTexts(Widget), Label))
        {
            Target = StaticCastSharedRef<SButton>(Widget);
        }
    });
    if (!Target.IsValid()) return false;
    Target->SimulateClick();
    Root->SlatePrepass(1.0f);
    return true;
}

FKeyEvent JpKey(const FKey Key, const bool bRepeat = false)
{
    return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
}

const TCHAR* const JpSuffixes[] = {
    TEXT("evening_rota_agreed"), TEXT("evening_rota_dishes_weekdays"), TEXT("evening_rota_alternate_days"),
    TEXT("evening_rota_sweep_and_cook"), TEXT("care_jobs_breakfast_weekdays"), TEXT("care_jobs_turns"),
    TEXT("care_jobs_study_room"), TEXT("nirmal_bose.book_evening_agreed"), TEXT("rupa_ghosh.visit_promised"),
    TEXT("sahana_mitra.sewing_evening_agreed"), TEXT("subrata_dey.step_promised"), TEXT("debashis_pal.bring_promised"),
    TEXT("mahua_saha.sort_promised")};

FString JpKeyFor(const TCHAR* Suffix) { return FString(PWJournal::AgreementKeyPrefix) + Suffix; }

/** A model with the given numbers of rows, built through the same formatting the screen uses. */
FPWJournalModel JpModel(const int32 People, const int32 Agreements)
{
    FPWJournalModel Model;
    Model.bAvailable = true;
    Model.Clock = FText::FromString(TEXT("Mon 2 Jun · 09:00"));
    for (int32 Index = 0; Index < People; ++Index)
    {
        FPWJournalPersonRow& Row = Model.People.AddDefaulted_GetRef();
        Row.Name = FText::FromString(FString::Printf(TEXT("Neighbour Number %d With A Fairly Long Name"), Index + 1));
        Row.MetAt = PWJournal::MetAtText(PerfectWorld::GameClock::FromCalendar(2042, 6, 2, 9, Index));
    }
    for (int32 Index = 0; Index < Agreements; ++Index)
    {
        FPWJournalAgreementRow& Row = Model.Agreements.AddDefaulted_GetRef();
        Row.Who = FText::FromString(FString::Printf(TEXT("Person %d"), Index + 1));
        Row.Text = PWJournal::AgreementText(JpKeyFor(JpSuffixes[Index % UE_ARRAY_COUNT(JpSuffixes)]));
    }
    return Model;
}

/** A throw-away game world with a started life, so the lane clock exists; play never begins. */
struct FJpScene
{
    UWorld* World = nullptr;
    UGameInstance* Instance = nullptr;
    APerfectWorldFrontEndHUD* Hud = nullptr;

    FJpScene()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Instance = NewObject<UGameInstance>();
        Instance->AddToRoot();
        Instance->Init();
        if (World == nullptr) return;
        World->SetGameInstance(Instance);
        UPerfectWorldCharacterCreatorSubsystem* Creator = Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>();
        Hud = World->SpawnActor<APerfectWorldFrontEndHUD>();
        if (Creator == nullptr) return;
        Creator->BeginNewCreation();
        if (Creator->ApplyBalancedScholarPreset()) Creator->StartLife();
    }
    ~FJpScene()
    {
        PWCreationPause::SetPauseSpaceForTest(0.0f, 0.0f);
        if (World != nullptr) World->DestroyWorld(false);
        Instance->Shutdown();
        Instance->RemoveFromRoot();
    }
    FJpScene(const FJpScene&) = delete;
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJpKeyResolutionTest, "PerfectWorld.JournalPanel.AgreementKeysResolveAndFallBackNeutrally",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJpKeyResolutionTest::RunTest(const FString& Parameters)
{
    const FString Fallback = PWJournal::AgreementText(JpKeyFor(TEXT("no_such_agreement"))).ToString();
    TestFalse(TEXT("the fallback is a real sentence"), Fallback.IsEmpty());
    TestFalse(TEXT("the fallback never shows a key"), Fallback.Contains(TEXT("journal")) || Fallback.Contains(TEXT("_")));

    TSet<FString> Seen;
    for (const TCHAR* Suffix : JpSuffixes)
    {
        const FString Key = JpKeyFor(Suffix);
        const FString Text = PWJournal::AgreementText(Key).ToString();
        TestFalse(*(Key + TEXT(": has text")), Text.IsEmpty());
        TestFalse(*(Key + TEXT(": is not the fallback")), Text == Fallback);
        TestFalse(*(Key + TEXT(": never shows the raw key or its parts")),
                  Text.Contains(TEXT("journal")) || Text.Contains(TEXT("agreement.")) || Text.Contains(Suffix) || Text.Contains(TEXT("_")));
        TestFalse(*(Key + TEXT(": is its own sentence")), Seen.Contains(Text));
        Seen.Add(Text);
    }
    TestEqual(TEXT("thirteen mapped keys, thirteen different sentences"), Seen.Num(), static_cast<int32>(UE_ARRAY_COUNT(JpSuffixes)));

    for (const TCHAR* Unknown : {TEXT(""), TEXT("journal.agreement."), TEXT("journal.agreement.evening_rota_agreed.extra"), TEXT("fact.debashis_pal.bring_promised"),
                                 TEXT("Journal.Agreement.evening_rota_agreed"), TEXT("journal.agreement.EVENING_ROTA_AGREED")})
    {
        const FText Text = PWJournal::AgreementText(Unknown);
        TestEqual(*FString::Printf(TEXT("unknown key '%s' reads the neutral fallback"), Unknown), Text.ToString(), Fallback);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJpFormattingTest, "PerfectWorld.JournalPanel.RowsFormatTheProjection",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJpFormattingTest::RunTest(const FString& Parameters)
{
    using namespace PerfectWorld;
    TestEqual(TEXT("the time of day, 24 hour"), PWJournal::MetAtText(GameClock::FromCalendar(2042, 6, 2, 8, 47)).ToString(), FString(TEXT("Met at 08:47")));
    TestEqual(TEXT("midnight"), PWJournal::MetAtText(GameClock::FromCalendar(2042, 6, 3, 0, 5)).ToString(), FString(TEXT("Met at 00:05")));
    TestEqual(TEXT("the last minute of a day, on a later date"), PWJournal::MetAtText(GameClock::FromCalendar(2042, 7, 9, 23, 59)).ToString(), FString(TEXT("Met at 23:59")));

    LaneJournalView View;
    View.Day = GameClock::DayNumber(GameClock::FromCalendar(2042, 6, 2, 9, 0));
    LaneJournalPerson Debashis;
    Debashis.PersonId = "person.debashis_pal";
    Debashis.DisplayName = "Debashis Pal";
    Debashis.FirstMetMinute = GameClock::FromCalendar(2042, 6, 2, 9, 3);
    LaneJournalPerson Nameless;
    Nameless.PersonId = "person.someone_new";
    Nameless.FirstMetMinute = GameClock::FromCalendar(2042, 6, 2, 17, 30);
    View.PeopleMetToday.push_back(Debashis);
    View.PeopleMetToday.push_back(Nameless);
    LaneJournalAgreement Promise;
    Promise.PersonId = "person.debashis_pal";
    Promise.AgreementId = "fact.debashis_pal.bring_promised";
    Promise.ShortTextKey = "journal.agreement.debashis_pal.bring_promised";
    LaneJournalAgreement Unmapped;
    Unmapped.PersonId = "person.someone_new";
    Unmapped.AgreementId = "fact.someone_new.thing";
    Unmapped.ShortTextKey = "journal.agreement.someone_new.thing";
    View.AgreementsRemembered.push_back(Promise);
    View.AgreementsRemembered.push_back(Unmapped);

    const FPWJournalModel Model = PWJournal::BuildModel(View, FText::FromString(TEXT("Mon 2 Jun")));
    TestTrue(TEXT("a projection makes an available model"), Model.bAvailable);
    TestEqual(TEXT("the clock words are kept"), Model.Clock.ToString(), FString(TEXT("Mon 2 Jun")));
    if (!TestEqual(TEXT("two people rows"), Model.People.Num(), 2) || !TestEqual(TEXT("two agreement rows"), Model.Agreements.Num(), 2)) return false;
    TestEqual(TEXT("the first person, in the projection's order"), Model.People[0].Name.ToString(), FString(TEXT("Debashis Pal")));
    TestEqual(TEXT("and when"), Model.People[0].MetAt.ToString(), FString(TEXT("Met at 09:03")));
    TestEqual(TEXT("a person without a name reads neutrally"), Model.People[1].Name.ToString(), FString(TEXT("Someone you know")));
    TestEqual(TEXT("and when (afternoon)"), Model.People[1].MetAt.ToString(), FString(TEXT("Met at 17:30")));
    TestEqual(TEXT("the agreement person label is the portable display name"), Model.Agreements[0].Who.ToString(),
              FString(UTF8_TO_TCHAR(std::string(LaneJournalDisplayName("person.debashis_pal")).c_str())));
    TestEqual(TEXT("the agreement's short text"), Model.Agreements[0].Text.ToString(), PWJournal::AgreementText(JpKeyFor(TEXT("debashis_pal.bring_promised"))).ToString());
    TestEqual(TEXT("an unknown person reads neutrally"), Model.Agreements[1].Who.ToString(), FString(TEXT("Someone you know")));
    TestEqual(TEXT("an unmapped key reads the neutral fallback, not the key"), Model.Agreements[1].Text.ToString(), PWJournal::AgreementText(TEXT("")).ToString());
    TestFalse(TEXT("nothing shown is a raw key"), Model.Agreements[1].Text.ToString().Contains(TEXT("someone_new")));

    const FPWJournalModel Empty = PWJournal::BuildModel(LaneJournalView(), FText::GetEmpty());
    TestTrue(TEXT("an empty projection is still an available (empty) model"), Empty.bAvailable && Empty.People.IsEmpty() && Empty.Agreements.IsEmpty());
    TestFalse(TEXT("no world means an unavailable model"), PWJournal::ReadModel(nullptr).bAvailable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJpPanelStatesTest, "PerfectWorld.JournalPanel.EmptyStatesAndRowsAreShown",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJpPanelStatesTest::RunTest(const FString& Parameters)
{
    {
        FPWJournalModel EmptyModel;
        EmptyModel.bAvailable = true;
        EmptyModel.Clock = FText::FromString(TEXT("Mon 2 Jun · 07:30"));
        const TSharedRef<SPWJournalPanel> Panel = SNew(SPWJournalPanel).Model(EmptyModel).Space(FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = JpShownTexts(Panel);
        TestTrue(TEXT("the title"), JpHas(Texts, TEXT("Today")));
        TestTrue(TEXT("the clock"), JpHas(Texts, TEXT("Mon 2 Jun · 07:30")));
        TestTrue(TEXT("both headings stay when empty"), JpHas(Texts, TEXT("People met today")) && JpHas(Texts, TEXT("Agreements remembered")));
        TestEqual(TEXT("the people empty state"), JpCountTag(Panel, PWJournal::EmptyPeopleTag), 1);
        TestEqual(TEXT("the agreements empty state"), JpCountTag(Panel, PWJournal::EmptyAgreementsTag), 1);
        TestEqual(TEXT("no person rows"), JpCountTag(Panel, PWJournal::PersonRowTag), 0);
        TestEqual(TEXT("no agreement rows"), JpCountTag(Panel, PWJournal::AgreementRowTag), 0);
        TestEqual(TEXT("not the unavailable state"), JpCountTag(Panel, PWJournal::UnavailableTag), 0);
        TestTrue(TEXT("Back is offered"), JpHas(Texts, TEXT("Back")));
    }
    {
        const FPWJournalModel Model = JpModel(2, 3);
        const TSharedRef<SPWJournalPanel> Panel = SNew(SPWJournalPanel).Model(Model).Space(FVector2D(1920.0, 1080.0));
        const TArray<FString> Texts = JpShownTexts(Panel);
        TestEqual(TEXT("two person rows"), JpCountTag(Panel, PWJournal::PersonRowTag), 2);
        TestEqual(TEXT("three agreement rows"), JpCountTag(Panel, PWJournal::AgreementRowTag), 3);
        TestEqual(TEXT("no empty states beside rows"), JpCountTag(Panel, PWJournal::EmptyPeopleTag) + JpCountTag(Panel, PWJournal::EmptyAgreementsTag), 0);
        for (const FPWJournalPersonRow& Row : Model.People) TestTrue(*(Row.Name.ToString() + TEXT(" shown")), JpHas(Texts, Row.Name.ToString()) && JpHas(Texts, Row.MetAt.ToString()));
        for (const FPWJournalAgreementRow& Row : Model.Agreements) TestTrue(*(Row.Text.ToString() + TEXT(" shown")), JpHas(Texts, Row.Who.ToString()) && JpHas(Texts, Row.Text.ToString()));
        TestEqual(TEXT("the panel keeps the model it was given"), Panel->GetModel().People.Num(), 2);
    }
    {
        const TSharedRef<SPWJournalPanel> Panel = SNew(SPWJournalPanel).Model(FPWJournalModel()).Space(FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = JpShownTexts(Panel);
        TestEqual(TEXT("an unreadable world says so"), JpCountTag(Panel, PWJournal::UnavailableTag), 1);
        TestEqual(TEXT("and shows no lists"), JpCountTag(Panel, PWJournal::EmptyPeopleTag) + JpCountTag(Panel, PWJournal::PersonRowTag), 0);
        TestTrue(TEXT("Back is still offered"), JpHas(Texts, TEXT("Back")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJpPanelFitTest, "PerfectWorld.JournalPanel.FitsEveryWindowAndScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJpPanelFitTest::RunTest(const FString& Parameters)
{
    // 853x480 and 1280x720 are 1280x720 and 1920x1080 windows at 150% UI scale; 1280x720 and 1920x1080 are at 100%.
    const FVector2D Sizes[] = {FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0 / 1.5, 1080.0 / 1.5), FVector2D(1920.0, 1080.0)};
    const FPWJournalModel Full = JpModel(14, 13); // every conversation person, every mapped agreement
    for (const FVector2D& Size : Sizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
        const float W = static_cast<float>(Size.X);
        const float H = static_cast<float>(Size.Y);
        const TSharedRef<SPWJournalPanel> Panel = SNew(SPWJournalPanel).Model(Full).Space(Size);
        Panel->SlatePrepass(1.0f);
        const FVector2D Desired(Panel->GetDesiredSize());
        AddInfo(FString::Printf(TEXT("%s: Today panel desires %.0f x %.0f of %.0f x %.0f"), *Name, Desired.X, Desired.Y, W - 32.0f, PWJournal::PanelMaxHeight(W, H)));
        TestTrue(*(Name + TEXT(": the panel fits the height; a long list scrolls inside")), Desired.Y <= PWJournal::PanelMaxHeight(W, H) + 0.5f);
        TestTrue(*(Name + TEXT(": the panel fits the width")), Desired.X <= FMath::Min(W - 32.0f, 640.0f) + 0.5f);
        TestEqual(*(Name + TEXT(": compact below 760")), PWJournal::Compact(W, H), H < 760.0f || W < 760.0f);
        TestTrue(*(Name + TEXT(": Back stays shown")), JpHas(JpShownTexts(Panel), TEXT("Back")));
        const TSharedRef<SPWJournalPanel> Empty = SNew(SPWJournalPanel).Model(JpModel(0, 0)).Space(Size);
        Empty->SlatePrepass(1.0f);
        TestTrue(*(Name + TEXT(": the empty panel fits too")), FVector2D(Empty->GetDesiredSize()).Y <= PWJournal::PanelMaxHeight(W, H) + 0.5f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJpPanelKeysTest, "PerfectWorld.JournalPanel.KeysGoBackAndScroll",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJpPanelKeysTest::RunTest(const FString& Parameters)
{
    int32 Closed = 0;
    const TSharedRef<SPWJournalPanel> Panel = SNew(SPWJournalPanel)
        .Model(JpModel(14, 13))
        .Space(FVector2D(853.0, 480.0))
        .OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
    Panel->SlatePrepass(1.0f);
    const FGeometry Geometry;

    TestTrue(TEXT("Escape is handled"), Panel->OnKeyDown(Geometry, JpKey(EKeys::Escape)).IsEventHandled());
    TestEqual(TEXT("Escape goes back"), Closed, 1);
    TestTrue(TEXT("a held Escape is swallowed"), Panel->OnKeyDown(Geometry, JpKey(EKeys::Escape, true)).IsEventHandled());
    TestEqual(TEXT("but goes back only once"), Closed, 1);
    Panel->OnKeyDown(Geometry, JpKey(EKeys::Gamepad_FaceButton_Right));
    TestEqual(TEXT("the controller's B goes back"), Closed, 2);
    Panel->OnKeyDown(Geometry, JpKey(EKeys::Gamepad_Special_Right));
    TestEqual(TEXT("the controller's menu button goes back"), Closed, 3);

    for (const FKey& Key : {EKeys::Up, EKeys::Down, EKeys::PageUp, EKeys::PageDown, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down,
                            EKeys::Gamepad_LeftStick_Up, EKeys::Gamepad_LeftStick_Down, EKeys::Gamepad_RightStick_Up, EKeys::Gamepad_RightStick_Down})
    {
        TestTrue(*(Key.ToString() + TEXT(" scrolls (handled)")), Panel->OnKeyDown(Geometry, JpKey(Key)).IsEventHandled());
    }
    TestEqual(TEXT("scrolling never closes"), Closed, 3);
    TestFalse(TEXT("an unrelated key is left alone"), Panel->OnKeyDown(Geometry, JpKey(EKeys::A)).IsEventHandled());
    TestTrue(TEXT("the scroll offset stays inside the list"), Panel->GetScrollOffset() >= 0.0f && Panel->GetScrollOffset() <= Panel->GetScrollOffsetOfEnd() + 0.5f);

    TestTrue(TEXT("the Back button is there"), JpClick(Panel, TEXT("Back")));
    TestEqual(TEXT("and goes back"), Closed, 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJpPauseMenuTodayTest, "PerfectWorld.JournalPanel.PauseMenuOpensAndClosesToday",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FJpPauseMenuTodayTest::RunTest(const FString& Parameters)
{
    FJpScene Scene;
    if (!TestNotNull(TEXT("hud"), Scene.Hud)) return false;
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Scene.Hud);
    if (!TestNotNull(TEXT("the lane world"), Life)) return false;

    // Today reads the one authoritative world: the same rows the portable projection gives for today.
    const auto Day = PerfectWorld::GameClock::DayNumber(Life->GetWorldState().Clock.Now());
    const PerfectWorld::LaneJournalView Truth = PerfectWorld::LaneJournalToday(Life->GetWorldState(), Day);
    const FPWJournalModel Read = PWJournal::ReadModel(Scene.Hud);
    TestTrue(TEXT("the model is available in a lane world"), Read.bAvailable);
    TestEqual(TEXT("its people are the projection's"), Read.People.Num(), static_cast<int32>(Truth.PeopleMetToday.size()));
    TestEqual(TEXT("its agreements are the projection's"), Read.Agreements.Num(), static_cast<int32>(Truth.AgreementsRemembered.size()));
    TestEqual(TEXT("its clock is the world's own words"), Read.Clock.ToString(), Life->GetClockText().ToString());

    PWCreationPause::SetPauseSpaceForTest(1280.0f, 720.0f);
    const TSharedRef<SPWPauseMenu> Menu = SNew(SPWPauseMenu).Hud(TWeakObjectPtr<APerfectWorldFrontEndHUD>(Scene.Hud));
    for (int32 Round = 0; Round < 2; ++Round)
    {
        const FString Tag = FString::Printf(TEXT("round %d: "), Round + 1);
        TestTrue(*(Tag + TEXT("the menu offers Today")), JpHas(JpShownTexts(Menu), TEXT("Today")) && JpHas(JpShownTexts(Menu), TEXT("Resume")));
        TestEqual(*(Tag + TEXT("no journal yet")), JpCountTag(Menu, PWJournal::PanelTag), 0);
        if (!TestTrue(*(Tag + TEXT("Today opens")), JpClick(Menu, TEXT("Today")))) return false;
        TestEqual(*(Tag + TEXT("the journal is shown")), JpCountTag(Menu, PWJournal::PanelTag), 1);
        TestFalse(*(Tag + TEXT("in place of the menu")), JpHas(JpShownTexts(Menu), TEXT("Resume")));
        // A fresh life has met no one: the empty states stand for what the projection says.
        TestEqual(*(Tag + TEXT("people empty state matches the projection")), JpCountTag(Menu, PWJournal::EmptyPeopleTag), Truth.PeopleMetToday.empty() ? 1 : 0);
        TestEqual(*(Tag + TEXT("agreement empty state matches the projection")), JpCountTag(Menu, PWJournal::EmptyAgreementsTag), Truth.AgreementsRemembered.empty() ? 1 : 0);
        TestEqual(*(Tag + TEXT("person rows match the projection")), JpCountTag(Menu, PWJournal::PersonRowTag), static_cast<int32>(Truth.PeopleMetToday.size()));
        if (Round == 0)
        {
            TestTrue(*(Tag + TEXT("Back returns")), JpClick(Menu, TEXT("Back")));
        }
        else
        {
            TestTrue(*(Tag + TEXT("Escape returns, and does not resume")), Menu->OnKeyDown(FGeometry(), JpKey(EKeys::Escape)).IsEventHandled());
            Menu->SlatePrepass(1.0f);
        }
        TestEqual(*(Tag + TEXT("the journal is gone")), JpCountTag(Menu, PWJournal::PanelTag), 0);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
