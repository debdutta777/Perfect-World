// The richer title (R5-06): what the title says about saved lives, and how it behaves. Run in the editor under
// "PerfectWorld.TitleR5". The summary model is exercised on in-memory saves (no slot file is read or written, and the real
// save folder is only read once, by the consistency test). The screen is the real widget, built without a window with the
// saves injected through its test seam: SlatePrepass evaluates bound text and visibility, buttons are clicked with
// SButton::SimulateClick, and window sizes come through SetSpaceForTest (853x480, 1280x720, 1920x1080 and both at 150%).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldPresentationSettings.h"
#include "UI/PWWidgets.h"
#include "UI/SPWTitleScreen.h"
#include "UI/TitleR5Model.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

namespace PWTitleR5Test
{
/** A started life at a game time on 2 June onward: the clock is the core's own, advanced to that calendar reading. */
PerfectWorld::WorldSaveData Tr5MakeSave(const char* Given, const char* Family, const bool bStarted, const int32 Day,
                                        const int32 Hour, const int32 Minute)
{
    PerfectWorld::WorldSaveData Data;
    Data.Creation.Identity.GivenName = Given;
    Data.Creation.Identity.FamilyName = Family;
    Data.Creation.Identity.HomeCity = "Kolkata";
    Data.Creation.HasStarted = bStarted;
    Data.Player.ZoneId = "zone.street.nabapara_lane";
    (void)Data.World.Clock.AdvanceTo(PerfectWorld::GameClock::FromCalendar(2042, 6, Day, Hour, Minute));
    return Data;
}

FDateTime Tr5Written(const int32 Day, const int32 Hour) { return FDateTime(2026, 10, Day, Hour, 0, 0); }

/** Slot 1 (index 1) is the newest and readable, slot 2 is damaged and in the middle, slot 0 is oldest. */
PWTitleR5::FTitleSaves Tr5ThreeSaves()
{
    PWTitleR5::FTitleSaves Saves;
    Saves.Slots.Add(PWTitleR5::SummariseWorldSave(0, Tr5MakeSave("Anika", "Sen", true, 3, 14, 30), Tr5Written(17, 9), false, FString()));
    Saves.Slots.Add(PWTitleR5::SummariseWorldSave(1, Tr5MakeSave("Rohan", "Das", true, 9, 7, 5), Tr5Written(19, 12), false, FString()));
    Saves.Slots.Add(PWTitleR5::SummariseUnreadable(2, Tr5Written(18, 20), FString()));
    Saves.ContinueSlot = 1;
    PWTitleR5::SortNewestFirst(Saves.Slots, Saves.ContinueSlot);
    return Saves;
}

PWTitleR5::FTitleSaves Tr5OneSave(const char* Given = "Anika")
{
    PWTitleR5::FTitleSaves Saves;
    Saves.Slots.Add(PWTitleR5::SummariseWorldSave(1, Tr5MakeSave(Given, "Sen", true, 3, 14, 30), Tr5Written(19, 12), false, FString()));
    Saves.ContinueSlot = 1;
    return Saves;
}

PWTitleR5::FTitleSaves Tr5OnlyDamaged()
{
    PWTitleR5::FTitleSaves Saves;
    Saves.Slots.Add(PWTitleR5::SummariseUnreadable(0, Tr5Written(19, 12), TEXT("It ends in the middle of a line.")));
    Saves.ContinueSlot = 0;
    return Saves;
}

/** The newest save is damaged, an older one is fine: Continue is blocked, Load still has something to offer. */
PWTitleR5::FTitleSaves Tr5NewestDamaged()
{
    PWTitleR5::FTitleSaves Saves;
    Saves.Slots.Add(PWTitleR5::SummariseWorldSave(0, Tr5MakeSave("Anika", "Sen", true, 3, 14, 30), Tr5Written(17, 9), false, FString()));
    Saves.Slots.Add(PWTitleR5::SummariseUnreadable(1, Tr5Written(19, 12), FString()));
    Saves.ContinueSlot = 1;
    PWTitleR5::SortNewestFirst(Saves.Slots, Saves.ContinueSlot);
    return Saves;
}

/** Visits every reachable widget, telling it whether it and everything above it is shown. Only a switcher's active page is walked. */
void Tr5Walk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    if (Widget->GetTypeAsString() == TEXT("SWidgetSwitcher"))
    {
        const TSharedPtr<SWidget> Active = StaticCastSharedRef<SWidgetSwitcher>(Widget)->GetActiveWidget();
        if (Active.IsValid()) Tr5Walk(Active.ToSharedRef(), bShown, Visit);
        return;
    }
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index) Tr5Walk(Children->GetChildAt(Index), bShown, Visit);
}

TArray<FString> Tr5Texts(const TSharedRef<SWidget>& Root)
{
    TArray<FString> Texts;
    Tr5Walk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock")) Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    });
    return Texts;
}

bool Tr5HasPart(const TArray<FString>& Texts, const FString& Part)
{
    return Texts.ContainsByPredicate([&Part](const FString& Text) { return Text.Contains(Part); });
}

struct FTr5Button { TSharedPtr<SButton> Button; FString Label; bool bEnabled = false; };

/** Every shown button in tree order, which is the order the menu reads and keyboard navigation follows. */
TArray<FTr5Button> Tr5Buttons(const TSharedRef<SWidget>& Root)
{
    TArray<FTr5Button> Found;
    Tr5Walk(Root, true, [&Found](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (!bShown || Widget->GetTypeAsString() != TEXT("SButton")) return;
        const TArray<FString> Own = Tr5Texts(Widget);
        Found.Add({StaticCastSharedRef<SButton>(Widget), Own.Num() > 0 ? Own[0] : FString(), Widget->IsEnabled()});
    });
    return Found;
}

/** The labels of the buttons that can take focus, in order. */
TArray<FString> Tr5FocusOrder(const TSharedRef<SWidget>& Root)
{
    TArray<FString> Labels;
    for (const FTr5Button& Entry : Tr5Buttons(Root)) if (Entry.bEnabled) Labels.Add(Entry.Label);
    return Labels;
}

const FTr5Button* Tr5Find(const TArray<FTr5Button>& Buttons, const TCHAR* Label)
{
    return Buttons.FindByPredicate([Label](const FTr5Button& Entry) { return Entry.Label == Label; });
}

TSharedRef<SPWTitleScreen> Tr5Screen(const PWTitleR5::FTitleSaves& Saves, const float Width = 1280.0f, const float Height = 720.0f,
                                     FPWSlotDelegate OnLoadSlot = FPWSlotDelegate())
{
    const TSharedRef<SPWTitleScreen> Screen = SNew(SPWTitleScreen, static_cast<UPerfectWorldCharacterCreatorSubsystem*>(nullptr))
        .Saves(&Saves).OnLoadSlot(OnLoadSlot);
    Screen->SetSpaceForTest(Width, Height);
    Screen->SlatePrepass(1.0f);
    return Screen;
}

bool Tr5Click(const TSharedRef<SPWTitleScreen>& Screen, const TCHAR* Label)
{
    const TArray<FTr5Button> Buttons = Tr5Buttons(Screen);
    const FTr5Button* Entry = Tr5Find(Buttons, Label);
    if (Entry == nullptr || !Entry->bEnabled) return false;
    Entry->Button->SimulateClick();
    Screen->SlatePrepass(1.0f);
    return true;
}

int32 Tr5CountTag(const TSharedRef<SWidget>& Root, const FName Tag)
{
    int32 Count = 0;
    Tr5Walk(Root, true, [&Count, Tag](const TSharedRef<SWidget>& Widget, const bool bShown) { if (bShown && Widget->GetTag() == Tag) ++Count; });
    return Count;
}

int32 Tr5CountType(const TSharedRef<SWidget>& Root, const TCHAR* Type)
{
    int32 Count = 0;
    Tr5Walk(Root, true, [&Count, Type](const TSharedRef<SWidget>& Widget, const bool bShown) { if (bShown && Widget->GetTypeAsString() == Type) ++Count; });
    return Count;
}

struct FTr5Size { float Width; float Height; };
/** 853x480, 1280x720, 1920x1080, and 1280x720 and 1920x1080 at 150% text and UI scale (853x480 and 1280x720 logical units). */
const FTr5Size Tr5Sizes[] = {{853.0f, 480.0f}, {1280.0f, 720.0f}, {1920.0f, 1080.0f}, {1280.0f / 1.5f, 720.0f / 1.5f}, {1920.0f / 1.5f, 1080.0f / 1.5f}};
} // namespace PWTitleR5Test

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5SummaryTextTest, "PerfectWorld.TitleR5.SingleSaveSummaryText",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5SummaryTextTest::RunTest(const FString& Parameters)
{
    using namespace PWTitleR5Test;
    const PWTitleR5::FSlotSummary Ready = PWTitleR5::SummariseWorldSave(1, Tr5MakeSave("Anika", "Sen", true, 3, 14, 30), Tr5Written(19, 12), false, FString());
    TestTrue(TEXT("a started life is Ready and can load"), Ready.State == PWTitleR5::ESlotState::Ready && Ready.CanLoad());
    TestEqual(TEXT("the slot is numbered from one"), Ready.SlotLabel.ToString(), FString(TEXT("Slot 2")));
    TestEqual(TEXT("name"), Ready.Name.ToString(), FString(TEXT("Anika Sen")));
    TestEqual(TEXT("age is the saved age, said as the age the life began at"), Ready.Age.ToString(), FString(TEXT("Age 16 at the start of this life")));
    TestEqual(TEXT("the in-game date and time of day come from the core calendar"), Ready.GameTime.ToString(), FString(TEXT("Tuesday 3 June 2042, 14:30")));
    TestEqual(TEXT("the lane zone is named with the home city"), Ready.Place.ToString(), FString(TEXT("Nabapara Lane, Kolkata")));
    TestTrue(TEXT("the real timestamp says Saved, its month and its year"), Ready.SavedAt.ToString().StartsWith(TEXT("Saved ")) && Ready.SavedAt.ToString().Contains(TEXT("Oct 2026")));
    TestTrue(TEXT("a plain save carries no note"), Ready.Note.IsEmpty());
    TestEqual(TEXT("the saved-at format is fixed"), PWTitleR5::FormatSavedAt(FDateTime(2026, 10, 19, 14, 5, 0)).ToString(), FString(TEXT("Saved 19 Oct 2026, 14:05")));
    TestEqual(TEXT("the first morning reads as the campaign's Monday"), PWTitleR5::FormatGameTime(PerfectWorld::GameClock::FromCalendar(2042, 6, 2, 6, 0)).ToString(), FString(TEXT("Monday 2 June 2042, 06:00")));
    TestTrue(TEXT("Compose carries every line"), Ready.Compose().ToString().Contains(TEXT("Anika Sen")) && Ready.Compose().ToString().Contains(TEXT("14:30")) && Ready.Compose().ToString().Contains(TEXT("Nabapara Lane")));

    const PWTitleR5::FSlotSummary Home = PWTitleR5::SummariseWorldSave(0, [] { PerfectWorld::WorldSaveData D = Tr5MakeSave("Meera", "", true, 2, 6, 0); D.Player.ZoneId = PerfectWorld::OriginHomeZoneId(PerfectWorld::InstitutionalHomeKitId); return D; }(), Tr5Written(1, 1), false, FString());
    TestEqual(TEXT("an opening home zone is named by its kit, and a missing family name is left off"), Home.Place.ToString(), FString(TEXT("Children's home dormitory, Kolkata")));
    TestEqual(TEXT("a given name alone is the name"), Home.Name.ToString(), FString(TEXT("Meera")));
    PerfectWorld::WorldSaveData Unknown = Tr5MakeSave("", "", true, 2, 6, 0);
    Unknown.Player.ZoneId = "zone.somewhere.unbuilt";
    const PWTitleR5::FSlotSummary Elsewhere = PWTitleR5::SummariseWorldSave(0, Unknown, Tr5Written(1, 1), false, FString());
    TestEqual(TEXT("an unnamed life says so"), Elsewhere.Name.ToString(), FString(TEXT("Unnamed")));
    TestEqual(TEXT("an unbuilt zone is not guessed: only the city is shown"), Elsewhere.Place.ToString(), FString(TEXT("Kolkata")));

    const PWTitleR5::FSlotSummary Backup = PWTitleR5::SummariseWorldSave(0, Tr5MakeSave("Anika", "Sen", true, 3, 14, 30), Tr5Written(19, 12), true, FString());
    TestTrue(TEXT("a save read from its backup says so"), Backup.State == PWTitleR5::ESlotState::Recovered && Backup.CanLoad() && !Backup.Note.IsEmpty());
    const PWTitleR5::FSlotSummary Backup2 = PWTitleR5::SummariseWorldSave(0, Tr5MakeSave("Anika", "Sen", true, 3, 14, 30), Tr5Written(19, 12), true, TEXT("The backup was used."));
    TestEqual(TEXT("and uses the reader's own sentence when it has one"), Backup2.Note.ToString(), FString(TEXT("The backup was used.")));

    const PWTitleR5::FSlotSummary Draft = PWTitleR5::SummariseWorldSave(2, Tr5MakeSave("Anika", "Sen", false, 2, 6, 0), Tr5Written(19, 12), false, FString());
    TestTrue(TEXT("an unfinished creation is a Draft that can load"), Draft.State == PWTitleR5::ESlotState::Draft && Draft.CanLoad());
    TestTrue(TEXT("with no game time or place, which a draft does not have"), Draft.GameTime.IsEmpty() && Draft.Place.IsEmpty());
    TestTrue(TEXT("and a note that says creation was not finished"), Draft.Note.ToString().Contains(TEXT("not finished")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5UnreadableTest, "PerfectWorld.TitleR5.CorruptSlotIsHonestAndBlocksContinue",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5UnreadableTest::RunTest(const FString& Parameters)
{
    using namespace PWTitleR5Test;
    const PWTitleR5::FSlotSummary Bad = PWTitleR5::SummariseUnreadable(0, Tr5Written(19, 12), TEXT("It ends in the middle of a line."));
    TestTrue(TEXT("an unreadable slot cannot load"), Bad.State == PWTitleR5::ESlotState::Unreadable && !Bad.CanLoad());
    TestTrue(TEXT("its note says it cannot be read, and why"), Bad.Note.ToString().Contains(TEXT("cannot be read")) && Bad.Note.ToString().Contains(TEXT("middle of a line")));
    TestTrue(TEXT("and no character details are invented"), Bad.Name.IsEmpty() && Bad.GameTime.IsEmpty() && Bad.Place.IsEmpty());
    TestFalse(TEXT("an unreadable slot with no reader sentence still explains itself"), PWTitleR5::SummariseUnreadable(0, Tr5Written(19, 12), FString()).Note.IsEmpty());

    TestTrue(TEXT("no saves: Continue is None"), PWTitleR5::FTitleSaves().GetContinueState() == PWTitleR5::EContinueState::None);
    TestTrue(TEXT("a readable most recent save: Ready"), Tr5OneSave().GetContinueState() == PWTitleR5::EContinueState::Ready);
    TestTrue(TEXT("a damaged most recent save: Blocked"), Tr5OnlyDamaged().GetContinueState() == PWTitleR5::EContinueState::Blocked);
    const PWTitleR5::FTitleSaves Mixed = Tr5NewestDamaged();
    TestTrue(TEXT("damaged newest, readable older: still Blocked, never quietly the older save"), Mixed.GetContinueState() == PWTitleR5::EContinueState::Blocked);
    TestEqual(TEXT("but one slot can still be loaded"), Mixed.LoadableCount(), 1);
    TestEqual(TEXT("and nothing here can"), Tr5OnlyDamaged().LoadableCount(), 0);

    // On screen: Continue is shown but disabled, the error line is present with its own mark, nothing throws.
    const TSharedRef<SPWTitleScreen> Screen = Tr5Screen(Mixed);
    const TArray<FTr5Button> Buttons = Tr5Buttons(Screen);
    const FTr5Button* Continue = Tr5Find(Buttons, TEXT("Continue"));
    if (!TestNotNull(TEXT("Continue is shown even when blocked"), Continue)) return false;
    TestFalse(TEXT("but disabled"), Continue->bEnabled);
    const TArray<FString> Texts = Tr5Texts(Screen);
    TestTrue(TEXT("the error line is honest and marked"), Tr5HasPart(Texts, TEXT("! This save cannot be read.")));
    TestTrue(TEXT("and it points to Load when another save works"), Tr5HasPart(Texts, TEXT("Another save can still be loaded")));
    TestTrue(TEXT("with only damaged saves it says nothing here can load"), Tr5HasPart(Tr5Texts(Tr5Screen(Tr5OnlyDamaged())), TEXT("No save here can be loaded")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5OrderingTest, "PerfectWorld.TitleR5.SeveralSlotsAreOrderedNewestFirst",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5OrderingTest::RunTest(const FString& Parameters)
{
    using namespace PWTitleR5Test;
    const PWTitleR5::FTitleSaves Saves = Tr5ThreeSaves();
    if (!TestEqual(TEXT("three slots"), Saves.Slots.Num(), 3)) return false;
    TestEqual(TEXT("newest first"), Saves.Slots[0].Slot, 1);
    TestEqual(TEXT("then the damaged one, by its time"), Saves.Slots[1].Slot, 2);
    TestEqual(TEXT("then the oldest"), Saves.Slots[2].Slot, 0);
    TestEqual(TEXT("Continue means the newest"), Saves.ContinueTarget() != nullptr ? Saves.ContinueTarget()->Slot : -1, 1);

    TArray<PWTitleR5::FSlotSummary> Ties;
    for (const int32 Slot : {2, 0, 1})
    {
        PWTitleR5::FSlotSummary Entry = PWTitleR5::SummariseUnreadable(Slot, Tr5Written(19, 12), FString());
        Ties.Add(Entry);
    }
    PWTitleR5::SortNewestFirst(Ties, -1);
    TestTrue(TEXT("equal times fall back to slot number"), Ties[0].Slot == 0 && Ties[1].Slot == 1 && Ties[2].Slot == 2);
    PWTitleR5::SortNewestFirst(Ties, 2);
    TestEqual(TEXT("the pinned Continue slot always leads, whatever its time"), Ties[0].Slot, 2);

    // The saved-lives list on screen shows the same order and the same summary, and loads the slot that was chosen.
    TArray<int32> Loaded;
    const TSharedRef<SPWTitleScreen> Screen = Tr5Screen(Saves, 1280.0f, 720.0f, FPWSlotDelegate::CreateLambda([&Loaded](const int32 Slot) { Loaded.Add(Slot); return FText::GetEmpty(); }));
    TestTrue(TEXT("Load is enabled with saves"), Tr5Click(Screen, TEXT("Load")));
    TestEqual(TEXT("the list is the active view"), Screen->GetActiveView(), 2);
    TestEqual(TEXT("one card per slot"), Tr5CountTag(Screen, TEXT("TitleR5SaveCard")), 3);
    const TArray<FString> Texts = Tr5Texts(Screen);
    const int32 Rohan = Texts.IndexOfByPredicate([](const FString& T) { return T == TEXT("Rohan Das"); });
    const int32 Anika = Texts.IndexOfByPredicate([](const FString& T) { return T == TEXT("Anika Sen"); });
    TestTrue(TEXT("newest first on screen"), Rohan != INDEX_NONE && Anika != INDEX_NONE && Rohan < Anika);
    TestTrue(TEXT("the same summary lines appear in the list"), Tr5HasPart(Texts, TEXT("Tuesday 3 June 2042, 14:30")) && Tr5HasPart(Texts, TEXT("Slot 3")));
    TestTrue(TEXT("the damaged slot is listed with its reason"), Tr5HasPart(Texts, TEXT("! This save cannot be read.")));
    const TArray<FString> Order = Tr5FocusOrder(Screen);
    TestTrue(TEXT("the damaged card takes no focus: two cards then Back"), Order.Num() == 3 && Order[2] == TEXT("Back"));
    const TArray<FTr5Button> Buttons = Tr5Buttons(Screen);
    if (Buttons.Num() >= 2 && Buttons[0].bEnabled)
    {
        Buttons[0].Button->SimulateClick();
        TestTrue(TEXT("choosing the first card loads the newest slot"), Loaded.Num() == 1 && Loaded[0] == 1);
    }
    else
    {
        AddError(TEXT("the list should start with an enabled card"));
    }
    const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
    Screen->OnKeyDown(FGeometry(), Escape);
    TestEqual(TEXT("Escape goes back to the menu"), Screen->GetActiveView(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5EmptyStateTest, "PerfectWorld.TitleR5.EmptyStateHidesContinueAndExplainsLoad",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5EmptyStateTest::RunTest(const FString& Parameters)
{
    using namespace PWTitleR5Test;
    const PWTitleR5::FTitleSaves None;
    const TSharedRef<SPWTitleScreen> Screen = Tr5Screen(None);
    const TArray<FTr5Button> Buttons = Tr5Buttons(Screen);
    TestNull(TEXT("Continue is hidden"), Tr5Find(Buttons, TEXT("Continue")));
    const FTr5Button* NewLife = Tr5Find(Buttons, TEXT("New Life"));
    const FTr5Button* Load = Tr5Find(Buttons, TEXT("Load"));
    if (!TestNotNull(TEXT("New Life is the one primary action"), NewLife) || !TestNotNull(TEXT("Load is shown"), Load)) return false;
    TestTrue(TEXT("New Life works"), NewLife->bEnabled);
    TestFalse(TEXT("Load is disabled"), Load->bEnabled);
    TestTrue(TEXT("and says why"), Tr5HasPart(Tr5Texts(Screen), TEXT("No saved lives yet")));
    TestEqual(TEXT("exactly one New Life button"), Buttons.FilterByPredicate([](const FTr5Button& B) { return B.Label == TEXT("New Life"); }).Num(), 1);
    TestFalse(TEXT("no summary card"), Tr5CountTag(Screen, TEXT("TitleR5ContinueCard")) > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5FocusTest, "PerfectWorld.TitleR5.FocusOrderAndStartingFocus",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5FocusTest::RunTest(const FString& Parameters)
{
    using namespace PWTitleR5Test;
    const auto StartLabel = [](const TSharedRef<SPWTitleScreen>& Screen) -> FString
    {
        const TSharedPtr<SWidget> Target = Screen->GetMainFocusTarget();
        const TSharedPtr<SWidget> First = Target.IsValid() ? PWUI::FindFirstFocusable(Target.ToSharedRef()) : nullptr;
        const TArray<FString> Own = First.IsValid() ? Tr5Texts(First.ToSharedRef()) : TArray<FString>();
        return Own.Num() > 0 ? Own[0] : FString();
    };
    const auto Same = [](const TArray<FString>& Actual, std::initializer_list<const TCHAR*> Expected)
    {
        if (Actual.Num() != static_cast<int32>(Expected.size())) return false;
        int32 Index = 0;
        for (const TCHAR* Label : Expected) { if (Actual[Index++] != Label) return false; }
        return true;
    };

    const TSharedRef<SPWTitleScreen> Ready = Tr5Screen(Tr5OneSave());
    TestTrue(TEXT("with a save: Continue, New Life, Load, Settings, Credits, Quit"), Same(Tr5FocusOrder(Ready), {TEXT("Continue"), TEXT("New Life"), TEXT("Load"), TEXT("Settings"), TEXT("Credits"), TEXT("Quit")}));
    TestEqual(TEXT("focus starts on Continue"), StartLabel(Ready), FString(TEXT("Continue")));

    const TSharedRef<SPWTitleScreen> Empty = Tr5Screen(PWTitleR5::FTitleSaves());
    TestTrue(TEXT("no saves: the disabled Load is skipped"), Same(Tr5FocusOrder(Empty), {TEXT("New Life"), TEXT("Settings"), TEXT("Credits"), TEXT("Quit")}));
    TestEqual(TEXT("focus starts on New Life"), StartLabel(Empty), FString(TEXT("New Life")));

    const TSharedRef<SPWTitleScreen> Blocked = Tr5Screen(Tr5NewestDamaged());
    TestTrue(TEXT("a blocked Continue is skipped"), Same(Tr5FocusOrder(Blocked), {TEXT("New Life"), TEXT("Load"), TEXT("Settings"), TEXT("Credits"), TEXT("Quit")}));
    TestEqual(TEXT("focus starts on Load, the way to the save that works"), StartLabel(Blocked), FString(TEXT("Load")));
    const TSharedRef<SPWTitleScreen> Dead = Tr5Screen(Tr5OnlyDamaged());
    TestEqual(TEXT("with nothing loadable focus starts on New Life"), StartLabel(Dead), FString(TEXT("New Life")));

    int32 Continues = 0;
    const PWTitleR5::FTitleSaves OneSave = Tr5OneSave();
    const TSharedRef<SPWTitleScreen> Live = SNew(SPWTitleScreen, static_cast<UPerfectWorldCharacterCreatorSubsystem*>(nullptr))
        .Saves(&OneSave).OnContinue(FPWTryDelegate::CreateLambda([&Continues]() { ++Continues; return FText::GetEmpty(); }));
    Live->SlatePrepass(1.0f);
    TestTrue(TEXT("Continue can be pressed"), Tr5Click(Live, TEXT("Continue")));
    TestEqual(TEXT("and runs the existing Continue path once"), Continues, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5FitTest, "PerfectWorld.TitleR5.SummaryFitsEveryWindowAndScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5FitTest::RunTest(const FString& Parameters)
{
    using namespace PWTitleR5Test;
    const PWTitleR5::FTitleSaves Long = Tr5OneSave("Anindyasundar-Bhattacharyya-Chakraborty-Mukhopadhyay");
    float CompactHeight = 0.0f;
    float FullHeight = 0.0f;
    for (const FTr5Size& Size : Tr5Sizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.Width, Size.Height);
        const SPWTitleScreen::FLayout Layout = SPWTitleScreen::ComputeLayout(Size.Width, Size.Height);
        TestTrue(*(Name + TEXT(": the column, its padding and the margin fit the width")), Layout.LeftPad + Layout.MenuWidth + 24.0f <= Size.Width + 0.01f);
        TestTrue(*(Name + TEXT(": short windows condense the summary")), Layout.bCompactSummary == (Size.Height < 600.0f));

        const TSharedRef<SPWTitleScreen> Screen = Tr5Screen(Long, Size.Width, Size.Height);
        const TSharedPtr<SWidget> Card = [&Screen]() { TSharedPtr<SWidget> Found; Tr5Walk(Screen, true, [&Found](const TSharedRef<SWidget>& W, bool) { if (!Found.IsValid() && W->GetTag() == FName(TEXT("TitleR5ContinueCard"))) Found = W; }); return Found; }();
        if (!TestTrue(*(Name + TEXT(": the summary card is there")), Card.IsValid())) continue;
        const FVector2D Desired(Card->GetDesiredSize());
        AddInfo(FString::Printf(TEXT("%s: summary card %.0f x %.0f in a %.0f unit column"), *Name, Desired.X, Desired.Y, Layout.MenuWidth));
        TestTrue(*(Name + TEXT(": the card is never wider than the menu column")), Desired.X <= Layout.MenuWidth + 0.5f);
        if (Size.Width == 1280.0f) FullHeight = static_cast<float>(Desired.Y);
        if (Size.Width == 853.0f) CompactHeight = static_cast<float>(Desired.Y);

        const TArray<FString> Texts = Tr5Texts(Screen);
        TestTrue(*(Name + TEXT(": every part of the summary is shown")), Tr5HasPart(Texts, TEXT("Anindyasundar")) && Tr5HasPart(Texts, TEXT("Tuesday 3 June 2042, 14:30")) && Tr5HasPart(Texts, TEXT("Nabapara Lane")) && Tr5HasPart(Texts, TEXT("Saved ")) && Tr5HasPart(Texts, TEXT("Age 16")));
        TestTrue(*(Name + TEXT(": a scroll box stands by, so nothing is ever clipped")), Tr5CountType(Screen, TEXT("SScrollBox")) >= 1);
        for (const FTr5Button& Button : Tr5Buttons(Screen))
        {
            TestTrue(*(Name + TEXT(": ") + Button.Label + TEXT(" fits the column")), FVector2D(Button.Button->GetDesiredSize()).X <= Layout.MenuWidth + 0.5f);
        }
        TestEqual(*(Name + TEXT(": six controls can take focus")), Tr5FocusOrder(Screen).Num(), 6);
    }
    TestTrue(TEXT("the condensed summary is no taller than the full one"), CompactHeight <= FullHeight + 0.5f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5MotionTest, "PerfectWorld.TitleR5.ReducedMotionShowsPagesAtOnce",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5MotionTest::RunTest(const FString& Parameters)
{
    using namespace PWTitleR5Test;
    const bool bBefore = UPerfectWorldPresentationSettings::Get().IsReducedMotion();
    UPerfectWorldPresentationSettings::Get().SetReducedMotion(true);
    const TSharedRef<SPWTitleScreen> Screen = Tr5Screen(Tr5ThreeSaves(), 1280.0f, 720.0f, FPWSlotDelegate::CreateLambda([](int32) { return FText::GetEmpty(); }));
    TestTrue(TEXT("Load opens the list"), Tr5Click(Screen, TEXT("Load")));
    Screen->Tick(FGeometry(), 0.0, 0.016f);
    float Opacity = 0.0f;
    Tr5Walk(Screen, true, [&Opacity](const TSharedRef<SWidget>& W, bool) { if (W->GetTypeAsString() == TEXT("SWidgetSwitcher")) Opacity = W->GetRenderOpacity(); });
    TestTrue(TEXT("under reduced motion the new page is fully opaque at once"), FMath::IsNearlyEqual(Opacity, 1.0f));
    UPerfectWorldPresentationSettings::Get().SetReducedMotion(bBefore);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTr5DiskReadTest, "PerfectWorld.TitleR5.ReadTitleSavesAgreesWithTheCreator",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTr5DiskReadTest::RunTest(const FString& Parameters)
{
    // Read-only: whatever the save folder holds, the title's view of it must agree with the creator's own slot rules.
    const PWTitleR5::FTitleSaves Saves = PWTitleR5::ReadTitleSaves();
    TestEqual(TEXT("Continue is the creator's most recent slot"), Saves.ContinueSlot, UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot());
    for (const PWTitleR5::FSlotSummary& Summary : Saves.Slots)
    {
        TestTrue(TEXT("a listed slot is an occupied slot"), Summary.State != PWTitleR5::ESlotState::Empty && UPerfectWorldCharacterCreatorSubsystem::IsSaveSlotOccupied(Summary.Slot));
        TestFalse(TEXT("and has a note or readable details"), Summary.Name.IsEmpty() && Summary.Note.IsEmpty());
    }
    if (Saves.Slots.Num() > 0)
    {
        TestEqual(TEXT("the Continue slot leads the list"), Saves.Slots[0].Slot, Saves.ContinueSlot);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
