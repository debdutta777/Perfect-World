// Creation and pause carry the title's richness forward (UP-06): creation's current-step hierarchy, the first-morning review,
// and the pause menu with the real character, place and time and legible Save feedback. Run in the editor under
// "PerfectWorld.CreationPause". The widgets are the real ones, built without a window: SlatePrepass evaluates bound text and
// visibility, buttons are clicked with SButton::SimulateClick, and logical window sizes come through each screen's named
// layout hook (1280x720, 1920x1080 and both at 150% UI scale). No save slot is touched: Save outcomes come from a test hook.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPresentationSettings.h"
#include "UI/PWCreationModel.h"
#include "UI/PWDelegates.h"
#include "UI/SPWCreationFlow.h"
#include "UI/SPWPauseMenu.h"
#include "UI/SPWReviewPage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

// The layout and feedback rules, defined beside each screen's widget and linked by name (no header carries them).
namespace PWCreationPause
{
int32 PauseDensity(float Width, float Height);
float PauseControlWidth(float Width, float Height);
float PauseMaxHeight(float Width, float Height);
FText PauseSaveFeedbackText(bool bSucceeded, const FText& CoreMessage, int32 SuccessCount);
void SetPauseSpaceForTest(float Width, float Height);
void SetPauseSaveHookForTest(TFunction<FText(bool&)> Hook);
FText CreationStepName(int32 Step);
bool CreationRailNumbersOnly(float Width);
bool CreationCompactChrome(float Height);
FText CreationNextLabel(int32 Step);
void SetCreationSpaceForTest(float Width, float Height);
bool ReviewStackTraits(float Width);
float ReviewHeroDiameter(float Width);
FText ReviewFirstMorningStamp();
void SetReviewWidthForTest(float Width);
} // namespace PWCreationPause

namespace PWCreationPauseTest
{
/** A throw-away game world with a game instance, so the creator and the lane clock exist; play never begins. */
struct FCpScene
{
    UWorld* World = nullptr;
    UGameInstance* Instance = nullptr;
    UPerfectWorldCharacterCreatorSubsystem* Creator = nullptr;
    APerfectWorldFrontEndHUD* Hud = nullptr;

    explicit FCpScene(const bool bStartLife)
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Instance = NewObject<UGameInstance>();
        Instance->AddToRoot();
        Instance->Init();
        if (World == nullptr) return;
        World->SetGameInstance(Instance);
        Creator = Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>();
        Hud = World->SpawnActor<APerfectWorldFrontEndHUD>();
        if (Creator == nullptr) return;
        Creator->BeginNewCreation();
        if (bStartLife && Creator->ApplyBalancedScholarPreset()) Creator->StartLife();
    }
    ~FCpScene()
    {
        if (World != nullptr) World->DestroyWorld(false);
        Instance->Shutdown();
        Instance->RemoveFromRoot();
    }
    FCpScene(const FCpScene&) = delete;
};

/** Puts every test hook and the player's motion setting back, whatever a test does. */
struct FCpScoped
{
    explicit FCpScoped(const bool bReducedMotion) : bBefore(UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        UPerfectWorldPresentationSettings::Get().SetReducedMotion(bReducedMotion);
    }
    ~FCpScoped()
    {
        UPerfectWorldPresentationSettings::Get().SetReducedMotion(bBefore);
        PWCreationPause::SetPauseSpaceForTest(0.0f, 0.0f);
        PWCreationPause::SetPauseSaveHookForTest(nullptr);
        PWCreationPause::SetCreationSpaceForTest(0.0f, 0.0f);
        PWCreationPause::SetReviewWidthForTest(0.0f);
    }
    FCpScoped(const FCpScoped&) = delete;
    bool bBefore = false;
};

/** Visits every widget that can be reached, telling it whether it and everything above it is shown. Only a switcher's active page is walked. */
void CpWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    if (Widget->GetTypeAsString() == TEXT("SWidgetSwitcher"))
    {
        const TSharedPtr<SWidget> Active = StaticCastSharedRef<SWidgetSwitcher>(Widget)->GetActiveWidget();
        if (Active.IsValid()) CpWalk(Active.ToSharedRef(), bShown, Visit);
        return;
    }
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index) CpWalk(Children->GetChildAt(Index), bShown, Visit);
}

/** Evaluates bound text and visibility, as a frame would. */
void CpRefresh(const TSharedRef<SWidget>& Root) { Root->SlatePrepass(1.0f); }

TArray<FString> CpShownTexts(const TSharedRef<SWidget>& Root)
{
    TArray<FString> Texts;
    CpWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock"))
        {
            Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        }
    });
    return Texts;
}

bool CpHas(const TArray<FString>& Texts, const FString& Exact) { return Texts.Contains(Exact); }

bool CpHasPart(const TArray<FString>& Texts, const FString& Part)
{
    return Texts.ContainsByPredicate([&Part](const FString& Text) { return Text.Contains(Part); });
}

TSharedPtr<SWidget> CpFindTag(const TSharedRef<SWidget>& Root, const FName Tag)
{
    TSharedPtr<SWidget> Found;
    CpWalk(Root, true, [&Found, Tag](const TSharedRef<SWidget>& Widget, bool)
    {
        if (!Found.IsValid() && Widget->GetTag() == Tag) Found = Widget;
    });
    return Found;
}

struct FCpButton { TSharedPtr<SButton> Button; bool bShown = false; };

/** Every button whose own text is exactly this, in tree order. */
TArray<FCpButton> CpFindButtons(const TSharedRef<SWidget>& Root, const FString& Label)
{
    TArray<FCpButton> Found;
    CpWalk(Root, true, [&Found, &Label](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (Widget->GetTypeAsString() != TEXT("SButton")) return;
        if (CpHas(CpShownTexts(Widget), Label)) Found.Add({StaticCastSharedRef<SButton>(Widget), bShown});
    });
    return Found;
}

/** Clicks the first shown button with this text; false when there is none. */
bool CpClick(const TSharedRef<SWidget>& Root, const FString& Label)
{
    for (const FCpButton& Entry : CpFindButtons(Root, Label))
    {
        if (Entry.bShown) { Entry.Button->SimulateClick(); CpRefresh(Root); return true; }
    }
    return false;
}

bool CpIsShown(const TSharedRef<SWidget>& Root, const FString& Label)
{
    for (const FCpButton& Entry : CpFindButtons(Root, Label)) if (Entry.bShown) return true;
    return false;
}

/** The text of the first N shown buttons, in tree order: on the creation screen these are the step chips. */
TArray<FString> CpFirstButtons(const TSharedRef<SWidget>& Root, const int32 Count)
{
    TArray<FString> Labels;
    CpWalk(Root, true, [&Labels, Count](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Labels.Num() < Count && Widget->GetTypeAsString() == TEXT("SButton"))
        {
            const TArray<FString> Own = CpShownTexts(Widget);
            Labels.Add(Own.Num() > 0 ? Own[0] : FString());
        }
    });
    return Labels;
}

int32 CpCountType(const TSharedRef<SWidget>& Root, const TCHAR* Type)
{
    int32 Count = 0;
    CpWalk(Root, true, [&Count, Type](const TSharedRef<SWidget>& Widget, bool) { if (Widget->GetTypeAsString() == Type) ++Count; });
    return Count;
}

struct FCpSize { float Width; float Height; bool bCompactPause; };
/** 1280x720, 1920x1080, and both at 150% UI scale (853x480 and 1280x720 logical units). */
const FCpSize CpSizes[] = {{1280.0f, 720.0f, true}, {1920.0f, 1080.0f, false}, {1280.0f / 1.5f, 720.0f / 1.5f, true}, {1920.0f / 1.5f, 1080.0f / 1.5f, true}};
} // namespace PWCreationPauseTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpPauseLayoutTest, "PerfectWorld.CreationPause.PauseFitsEveryWindowAndScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCpPauseLayoutTest::RunTest(const FString& Parameters)
{
    using namespace PWCreationPauseTest;
    FCpScoped Scoped(false);
    FCpScene Scene(true);
    if (!TestNotNull(TEXT("creator"), Scene.Creator) || !TestNotNull(TEXT("hud"), Scene.Hud)) return false;

    for (const FCpSize& Size : CpSizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.Width, Size.Height);
        TestEqual(*(Name + TEXT(": density")), PWCreationPause::PauseDensity(Size.Width, Size.Height), Size.bCompactPause ? 1 : 0);
        PWCreationPause::SetPauseSpaceForTest(Size.Width, Size.Height);
        const TSharedRef<SPWPauseMenu> Menu = SNew(SPWPauseMenu).Hud(TWeakObjectPtr<APerfectWorldFrontEndHUD>(Scene.Hud));
        CpRefresh(Menu);

        const TSharedPtr<SWidget> Card = CpFindTag(Menu, TEXT("CpPauseCard"));
        if (!TestTrue(*(Name + TEXT(": the card exists")), Card.IsValid())) continue;
        const FVector2D Desired(Card->GetDesiredSize());
        TestTrue(*(Name + TEXT(": the card fits the height without scrolling")), Desired.Y <= PWCreationPause::PauseMaxHeight(Size.Width, Size.Height));
        TestTrue(*(Name + TEXT(": and the width with a 16 unit margin each side")), Desired.X <= Size.Width - 32.0f);
        const float Control = PWCreationPause::PauseControlWidth(Size.Width, Size.Height);
        TestTrue(*(Name + TEXT(": the controls keep a usable width")), Control >= 200.0f && Control <= 360.0f);

        const TArray<FString> Texts = CpShownTexts(Menu);
        for (const TCHAR* Label : {TEXT("Paused"), TEXT("Resume"), TEXT("Save"), TEXT("Return to Title"), TEXT("Quit")})
        {
            TestTrue(*(Name + TEXT(": shows ") + Label), CpHas(Texts, Label));
        }
        TestTrue(*(Name + TEXT(": the PLACE and TIME labels yield to space before the values do")), CpHas(Texts, TEXT("PLACE")) == !Size.bCompactPause);
        TestTrue(*(Name + TEXT(": the place value is always shown")), CpHasPart(Texts, TEXT("Nabapara Lane")));

        // Even the failure panel (the tallest state) never makes the menu taller than the window: it scrolls instead.
        PWCreationPause::SetPauseSaveHookForTest([](bool& bOk) { bOk = false; return FText::FromString(TEXT("The disk is full. The earlier save is unchanged.")); });
        CpClick(Menu, TEXT("Save"));
        TestTrue(*(Name + TEXT(": with feedback the menu still fits the window")), FVector2D(Menu->GetDesiredSize()).Y <= PWCreationPause::PauseMaxHeight(Size.Width, Size.Height) + 0.5f);
        TestTrue(*(Name + TEXT(": and a scroll box stands by if the card ever outgrows it")), CpCountType(Menu, TEXT("SScrollBox")) == 1);
        PWCreationPause::SetPauseSaveHookForTest(nullptr);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpPauseSaveFeedbackTest, "PerfectWorld.CreationPause.SaveFeedbackShowsSuccessAndFailure",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCpPauseSaveFeedbackTest::RunTest(const FString& Parameters)
{
    using namespace PWCreationPauseTest;
    FCpScoped Scoped(false);
    TestEqual(TEXT("success reads the core's own words"), PWCreationPause::PauseSaveFeedbackText(true, FText::FromString(TEXT("Saved to slot 2.")), 1).ToString(), FString(TEXT("Saved to slot 2.")));
    TestTrue(TEXT("repeating a save says so"), PWCreationPause::PauseSaveFeedbackText(true, FText::FromString(TEXT("Saved to slot 2.")), 3).ToString().Contains(TEXT("3 times")));
    TestTrue(TEXT("failure begins with the words Not saved"), PWCreationPause::PauseSaveFeedbackText(false, FText::FromString(TEXT("Disk full.")), 0).ToString().StartsWith(TEXT("Not saved.")));
    TestTrue(TEXT("and keeps the core's reason"), PWCreationPause::PauseSaveFeedbackText(false, FText::FromString(TEXT("Disk full.")), 0).ToString().Contains(TEXT("Disk full.")));

    PWCreationPause::SetPauseSpaceForTest(1280.0f, 720.0f);
    const TSharedRef<SPWPauseMenu> Menu = SNew(SPWPauseMenu).Hud(TWeakObjectPtr<APerfectWorldFrontEndHUD>());
    CpRefresh(Menu);
    TestTrue(TEXT("before any Save there is no feedback"), !CpHasPart(CpShownTexts(Menu), TEXT("Saved")) && !CpHasPart(CpShownTexts(Menu), TEXT("Not saved")));

    int32 Calls = 0;
    PWCreationPause::SetPauseSaveHookForTest([&Calls](bool& bOk) { ++Calls; bOk = true; return FText::FromString(TEXT("Saved to slot 2.")); });
    TestTrue(TEXT("Save can be pressed"), CpClick(Menu, TEXT("Save")));
    TArray<FString> Texts = CpShownTexts(Menu);
    TestEqual(TEXT("the Save went to the one save path"), Calls, 1);
    TestTrue(TEXT("success is shown in words"), CpHas(Texts, TEXT("Saved to slot 2.")));
    TestTrue(TEXT("and with a check mark, not by colour alone"), CpHas(Texts, TEXT("\u2713")));
    CpClick(Menu, TEXT("Save"));
    TestTrue(TEXT("a second Save is distinguishable from the first"), CpHasPart(CpShownTexts(Menu), TEXT("2 times")));

    PWCreationPause::SetPauseSaveHookForTest([](bool& bOk) { bOk = false; return FText::FromString(TEXT("Stand up and let the doors settle before saving. The earlier save is unchanged.")); });
    CpClick(Menu, TEXT("Save"));
    Texts = CpShownTexts(Menu);
    TestTrue(TEXT("failure says Not saved and why"), CpHasPart(Texts, TEXT("Not saved. Stand up and let the doors settle")));
    TestTrue(TEXT("with its own mark"), CpHas(Texts, TEXT("!")) && !CpHas(Texts, TEXT("\u2713")));
    TestFalse(TEXT("and no stale success beside it"), CpHasPart(Texts, TEXT("Saved to slot")));
    TestTrue(TEXT("Return to Title and Quit are still reachable"), CpIsShown(Menu, TEXT("Return to Title")) && CpIsShown(Menu, TEXT("Quit")));

    // The slot chooser (all slots occupied) reports its own outcome: the pause shows nothing of its own.
    PWCreationPause::SetPauseSaveHookForTest([](bool& bOk) { bOk = false; return FText::GetEmpty(); });
    CpClick(Menu, TEXT("Save"));
    TestFalse(TEXT("a chooser hand-off leaves no feedback"), CpHasPart(CpShownTexts(Menu), TEXT("Not saved")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpPauseContextTest, "PerfectWorld.CreationPause.PauseNamesTheRealCharacterPlaceAndTime",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCpPauseContextTest::RunTest(const FString& Parameters)
{
    using namespace PWCreationPauseTest;
    FCpScoped Scoped(false);
    FCpScene Scene(true);
    if (!TestNotNull(TEXT("creator"), Scene.Creator) || !TestNotNull(TEXT("hud"), Scene.Hud)) return false;
    const FPWCreationSummary Summary = Scene.Creator->GetCreationSummary();
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Scene.Hud);
    if (!TestNotNull(TEXT("the lane clock"), Life)) return false;

    PWCreationPause::SetPauseSpaceForTest(1920.0f, 1080.0f);
    const TSharedRef<SPWPauseMenu> Menu = SNew(SPWPauseMenu).Hud(TWeakObjectPtr<APerfectWorldFrontEndHUD>(Scene.Hud));
    CpRefresh(Menu);
    const TArray<FString> Texts = CpShownTexts(Menu);
    TestTrue(TEXT("the character's own name"), CpHas(Texts, Summary.FullName.IsEmpty() ? FString(TEXT("Your life")) : Summary.FullName.ToString()));
    TestTrue(TEXT("the place, from the lane"), CpHasPart(Texts, TEXT("Nabapara Lane")));
    TestTrue(TEXT("the time, read live from the world clock"), CpHas(Texts, Life->GetClockText().ToString()));
    TestTrue(TEXT("said to be held, with the home from the summary"), CpHasPart(Texts, TEXT("held while the game is paused"))
             && (Summary.HouseholdName.IsEmpty() || CpHasPart(Texts, Summary.HouseholdName.ToString())));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpCreationFlowTest, "PerfectWorld.CreationPause.CreationHierarchyAndProgressionHoldAtEverySize",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCpCreationFlowTest::RunTest(const FString& Parameters)
{
    using namespace PWCreationPauseTest;
    FCpScoped Scoped(false);
    FCpScene Scene(false);
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;

    // The flow's own size is 90% of the window from 1440 up, and the window less 48 by 32 below that.
    const FVector2D FlowSizes[] = {FVector2D(1232, 688), FVector2D(1728, 972), FVector2D(805, 448)};
    for (const FVector2D& Flow : FlowSizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Flow.X, Flow.Y);
        PWCreationPause::SetCreationSpaceForTest(static_cast<float>(Flow.X), static_cast<float>(Flow.Y));
        Scene.Creator->BeginNewCreation();
        const TSharedRef<SPWCreationFlow> Screen = SNew(SPWCreationFlow, Scene.Creator).InitialStep(0);
        CpRefresh(Screen);
        TArray<FString> Texts = CpShownTexts(Screen);
        const bool bNumbers = PWCreationPause::CreationRailNumbersOnly(static_cast<float>(Flow.X));
        TestTrue(*(Name + TEXT(": the header names where the player is")), CpHas(Texts, TEXT("Step 1 of 5")) && CpHas(Texts, TEXT("Household")));
        TestTrue(*(Name + TEXT(": the small New life label yields first when short")), CpHas(Texts, TEXT("New life")) == !PWCreationPause::CreationCompactChrome(static_cast<float>(Flow.Y)));
        const TArray<FString> Chips = CpFirstButtons(Screen, 5);
        TestTrue(*(Name + TEXT(": the current chip reads in full")), Chips.Num() == 5 && Chips[0] == TEXT("1  Household"));
        TestTrue(*(Name + TEXT(": other chips shrink to numbers only when narrow, and keep their names when there is room")),
                 Chips.Num() == 5 && (bNumbers ? (Chips[1] == TEXT("2") && Chips[4] == TEXT("5")) : (Chips[1] == TEXT("2  Traits") && Chips[4] == TEXT("5  Review"))));
        const TSharedPtr<SWidget> Header = CpFindTag(Screen, TEXT("CpStepHeader"));
        TestTrue(*(Name + TEXT(": the header is one short line")), Header.IsValid() && FVector2D(Header->GetDesiredSize()).Y <= 48.0);
        TestTrue(*(Name + TEXT(": the forward button names its destination")), CpIsShown(Screen, TEXT("Next: Traits")));
    }

    // Progression is exactly as before: Next advances, Back retreats, a chip jumps, the resume point follows.
    PWCreationPause::SetCreationSpaceForTest(1232.0f, 688.0f);
    Scene.Creator->BeginNewCreation();
    const TSharedRef<SPWCreationFlow> Screen = SNew(SPWCreationFlow, Scene.Creator).InitialStep(0);
    CpRefresh(Screen);
    const EPWResumePoint Points[] = {EPWResumePoint::Allocation, EPWResumePoint::Identity, EPWResumePoint::Stream, EPWResumePoint::Review};
    for (int32 Step = 1; Step < PWUI::CreationStepCount; ++Step)
    {
        TestTrue(*FString::Printf(TEXT("Next is offered on step %d"), Step), CpClick(Screen, PWCreationPause::CreationNextLabel(Step - 1).ToString()));
        const TArray<FString> Texts = CpShownTexts(Screen);
        TestTrue(*FString::Printf(TEXT("step %d: header"), Step + 1), CpHas(Texts, FString::Printf(TEXT("Step %d of 5"), Step + 1)) && CpHas(Texts, PWCreationPause::CreationStepName(Step).ToString()));
        TestTrue(*FString::Printf(TEXT("step %d: the core remembers where the player got to"), Step + 1), Scene.Creator->GetResumePoint() == Points[Step - 1]);
    }
    TestFalse(TEXT("on Review the forward button is gone"), CpIsShown(Screen, TEXT("Next: Review")));
    TestTrue(TEXT("and Start Life stands in its place"), CpIsShown(Screen, TEXT("Start Life")));
    TestTrue(TEXT("Back retreats one step"), CpClick(Screen, TEXT("Back")) && CpHas(CpShownTexts(Screen), TEXT("Step 4 of 5")));
    TestTrue(TEXT("a chip jumps to its step"), CpClick(Screen, TEXT("1  Household")) && CpHas(CpShownTexts(Screen), TEXT("Step 1 of 5")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpFocusOrderTest, "PerfectWorld.CreationPause.FocusOrderFollowsTheScreenForKeyboardAndController",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCpFocusOrderTest::RunTest(const FString& Parameters)
{
    using namespace PWCreationPauseTest;
    FCpScoped Scoped(false);
    FCpScene Scene(true);
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;

    // Creation: the five step chips, then the page, then Back, then Next - top to bottom, left to right.
    PWCreationPause::SetCreationSpaceForTest(1232.0f, 688.0f);
    Scene.Creator->BeginNewCreation();
    const TSharedRef<SPWCreationFlow> Screen = SNew(SPWCreationFlow, Scene.Creator).InitialStep(0);
    CpRefresh(Screen);
    TArray<FString> Order;
    CpWalk(Screen, true, [&Order](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->IsEnabled() && Widget->SupportsKeyboardFocus())
        {
            const TArray<FString> Own = Widget->GetTypeAsString() == TEXT("SButton") ? CpShownTexts(Widget) : TArray<FString>();
            Order.Add(Own.Num() > 0 ? Own[0] : FString::Printf(TEXT("<%s>"), *Widget->GetTypeAsString()));
        }
    });
    const int32 Chip1 = Order.IndexOfByKey(TEXT("1  Household"));
    const int32 Chip5 = Order.IndexOfByKey(TEXT("5  Review"));
    const int32 Back = Order.IndexOfByKey(TEXT("Back to title"));
    const int32 Next = Order.IndexOfByKey(TEXT("Next: Traits"));
    TestTrue(TEXT("the chips come first, in order"), Chip1 >= 0 && Chip5 == Chip1 + 4);
    TestTrue(TEXT("the page's own controls sit between the chips and the footer"), Back > Chip5 + 1);
    TestTrue(TEXT("Back precedes Next, both last"), Next == Back + 1 && Next == Order.Num() - 1);

    // A controller's B (Gamepad_FaceButton_Right) retreats exactly like Back.
    CpClick(Screen, TEXT("Next: Traits"));
    CpClick(Screen, TEXT("Next: Identity"));
    const FKeyEvent Cancel(EKeys::Gamepad_FaceButton_Right, FModifierKeysState(), 0, false, 0, 0);
    Screen->OnKeyDown(FGeometry(), Cancel);
    CpRefresh(Screen);
    TestTrue(TEXT("controller B goes back one step"), CpHas(CpShownTexts(Screen), TEXT("Step 2 of 5")));

    // Pause: Resume first, then Save, Return to Title and Quit; the feedback panel is never a focus stop.
    PWCreationPause::SetPauseSpaceForTest(1280.0f, 720.0f);
    const TSharedRef<SPWPauseMenu> Menu = SNew(SPWPauseMenu).Hud(TWeakObjectPtr<APerfectWorldFrontEndHUD>(Scene.Hud));
    CpRefresh(Menu);
    TArray<FString> PauseOrder;
    CpWalk(Menu, true, [&PauseOrder](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->IsEnabled() && Widget->SupportsKeyboardFocus() && Widget->GetTypeAsString() == TEXT("SButton"))
        {
            PauseOrder.Add(CpShownTexts(Widget).Num() > 0 ? CpShownTexts(Widget)[0] : FString());
        }
    });
    TestEqual(TEXT("pause focus order"), FString::Join(PauseOrder, TEXT("|")), FString(TEXT("Resume|Save|Return to Title|Quit")));
    PWCreationPause::SetPauseSaveHookForTest([](bool& bOk) { bOk = false; return FText::FromString(TEXT("No.")); });
    CpClick(Menu, TEXT("Save"));
    PauseOrder.Reset();
    CpWalk(Menu, true, [&PauseOrder](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->SupportsKeyboardFocus() && Widget->GetTypeAsString() == TEXT("SButton")) PauseOrder.Add(TEXT("b"));
    });
    TestEqual(TEXT("showing feedback adds no focus stop"), PauseOrder.Num(), 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpReducedMotionTest, "PerfectWorld.CreationPause.ReducedMotionShowsEveryPageAtOnce",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCpReducedMotionTest::RunTest(const FString& Parameters)
{
    using namespace PWCreationPauseTest;
    FCpScoped Scoped(true);
    FCpScene Scene(false);
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;
    PWCreationPause::SetCreationSpaceForTest(1232.0f, 688.0f);
    Scene.Creator->BeginNewCreation();
    const TSharedRef<SPWCreationFlow> Screen = SNew(SPWCreationFlow, Scene.Creator).InitialStep(0);
    CpRefresh(Screen);
    TSharedPtr<SWidget> Body;
    CpWalk(Screen, true, [&Body](const TSharedRef<SWidget>& Widget, bool)
    {
        if (!Body.IsValid() && Widget->GetTypeAsString() == TEXT("SScrollBox")) Body = Widget;
    });
    if (!TestTrue(TEXT("the page body"), Body.IsValid())) return false;
    for (int32 Step = 0; Step < 3; ++Step)
    {
        CpClick(Screen, PWCreationPause::CreationNextLabel(Step).ToString());
        Screen->Tick(FGeometry(), 0.0, 0.016f);
        TestTrue(*FString::Printf(TEXT("a page change under reduced motion is fully opaque at once (step %d)"), Step + 2), FMath::IsNearlyEqual(Body->GetRenderOpacity(), 1.0f));
    }
    // The pause card and review hero add no motion of their own: the sigils honour the same setting themselves.
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCpReviewFirstMorningTest, "PerfectWorld.CreationPause.ReviewTellsTheFirstMorningFromTheSummary",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCpReviewFirstMorningTest::RunTest(const FString& Parameters)
{
    using namespace PWCreationPauseTest;
    FCpScoped Scoped(false);
    FCpScene Scene(false);
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;
    TestEqual(TEXT("the morning is dated from the core's campaign constants"), PWCreationPause::ReviewFirstMorningStamp().ToString(), FString(TEXT("Monday 2 June 2042, 07:30")));
    TestTrue(TEXT("the hero ring grows with the room"), PWCreationPause::ReviewHeroDiameter(1728.0f) == 160.0f
             && PWCreationPause::ReviewHeroDiameter(1232.0f) == 120.0f && PWCreationPause::ReviewHeroDiameter(805.0f) == 88.0f);
    TestTrue(TEXT("the background panel stacks below 1100"), PWCreationPause::ReviewStackTraits(805.0f) && !PWCreationPause::ReviewStackTraits(1232.0f));

    const FPWCreationSummary Summary = Scene.Creator->GetCreationSummary();
    const TSharedRef<FPWCreationModel> Model = MakeShared<FPWCreationModel>(Scene.Creator);
    for (const float Width : {1728.0f, 1232.0f, 805.0f})
    {
        const FString Name = FString::Printf(TEXT("%.0f wide"), Width);
        PWCreationPause::SetReviewWidthForTest(Width);
        TArray<int32> Jumps;
        const TSharedRef<SPWReviewPage> Page = SNew(SPWReviewPage, Model).OnJumpToStep(FPWStepDelegate::CreateLambda([&Jumps](const int32 Step) { Jumps.Add(Step); }));
        CpRefresh(Page);
        const TArray<FString> Texts = CpShownTexts(Page);
        TestTrue(*(Name + TEXT(": the first-morning card and its date")), CpHas(Texts, TEXT("Your first morning")) && CpHas(Texts, TEXT("Monday 2 June 2042, 07:30")));
        TestTrue(*(Name + TEXT(": the household beat")), CpHas(Texts, FString::Printf(TEXT("You wake at %s."), *Summary.HouseholdName.ToString())));
        TestTrue(*(Name + TEXT(": the stream beat")), CpHasPart(Texts, TEXT("Your stream is ")));
        if (Summary.bGrewUpInCare && !Summary.CareNote.IsEmpty()) TestTrue(*(Name + TEXT(": the care note is told")), CpHasPart(Texts, Summary.CareNote.ToString()));
        const TSharedPtr<SWidget> Beside = CpFindTag(Page, TEXT("CpTraitsBeside"));
        const TSharedPtr<SWidget> Below = CpFindTag(Page, TEXT("CpTraitsBelow"));
        const bool bStacked = PWCreationPause::ReviewStackTraits(Width);
        TestTrue(*(Name + TEXT(": the background panel sits beside or below, never both")),
                 Beside.IsValid() && Below.IsValid() && Beside->GetVisibility().IsVisible() != Below->GetVisibility().IsVisible()
                 && Below->GetVisibility().IsVisible() == bStacked);
        // Every Change button still opens the step it opened before: identity, household, stream, then the background.
        const TArray<FCpButton> Changes = CpFindButtons(Page, TEXT("Change"));
        for (const FCpButton& Entry : Changes) if (Entry.bShown) Entry.Button->SimulateClick();
        TestTrue(*(Name + TEXT(": Change buttons jump to the same steps")), Jumps == TArray<int32>({2, 0, 3, 1}));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
