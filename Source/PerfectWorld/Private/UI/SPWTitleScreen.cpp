#include "UI/SPWTitleScreen.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Rendering/DrawElements.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "UI/TitleR5Cards.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTitleScreen"

namespace
{
// Title layout constants, in logical (post-scaling) units. Prefixed Ti: the module compiles as unity blobs.
constexpr float TiMenuPreferred = 560.0f;   // the menu column's width where there is room
constexpr float TiMenuMinimum = 280.0f;     // the narrowest it may be compressed to
constexpr float TiScrollPad = 24.0f;        // padding to the right of the menu column
constexpr float TiEdgeMargin = 24.0f;       // from the right and bottom of the screen to the aperture's box
constexpr float TiSurfaceBreath = 16.0f;    // opaque surface beyond the last control
constexpr float TiApertureTargetMin = 340.0f;
constexpr float TiApertureTargetMax = 460.0f;
constexpr float TiApertureSmallest = 220.0f; // below this the aperture is suppressed, not shrunk further
constexpr float TiMoteRoom = 1.5f * TiApertureTargetMin;
constexpr float TiFallbackWidth = 1280.0f;  // used before the first layout pass has given the widget a size
constexpr float TiFallbackHeight = 720.0f;
constexpr float TiCompactHeight = 600.0f;   // below this logical height the save summary joins its lines in pairs
constexpr float TiCardGutter = 26.0f;       // the summary's left indent (18) and right margin (8) inside the menu column

/**
 * The dark reading surface behind the menu. It is nearly opaque to the end of the controls so text keeps at least 4.5:1
 * over a bright sunlit wall (about 0.93 of the background token over a wall of 0.6 luminance gives better than 8:1 for
 * the primary and secondary text colours), then fades across the right of the screen so the lane shows beside the aperture.
 */
class SPWTitleGradient final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTitleGradient) : _SurfaceEdge(640.0f) {}
        SLATE_ATTRIBUTE(float, SurfaceEdge)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs) { SurfaceEdge = InArgs._SurfaceEdge; SetVisibility(EVisibility::HitTestInvisible); }
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
                          FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& WidgetStyle, bool) const override
    {
        const float W = Geometry.GetLocalSize().X;
        const float Edge = FMath::Clamp(SurfaceEdge.Get(640.0f), 1.0f, W * .9f);
        TArray<FSlateGradientStop> Stops;
        Stops.Emplace(FVector2D(0, 0), PWUI::Srgb(16, 26, 29, 250) * WidgetStyle.GetColorAndOpacityTint());
        Stops.Emplace(FVector2D(Edge, 0), PWUI::Srgb(16, 26, 29, 238) * WidgetStyle.GetColorAndOpacityTint());
        Stops.Emplace(FVector2D(Edge + (W - Edge) * .30f, 0), PWUI::Srgb(16, 26, 29, 90) * WidgetStyle.GetColorAndOpacityTint());
        Stops.Emplace(FVector2D(W, 0), FLinearColor::Transparent);
        // Slate names the orientation of the colour bands, not of the fade: a left-to-right fade (stops along X)
        // is Orient_Vertical, as in SComplexGradient. Orient_Horizontal put the stops along Y, off the panel.
        FSlateDrawElement::MakeGradient(Elements, Layer, Geometry.ToPaintGeometry(), Stops, Orient_Vertical);
        return Layer;
    }

private:
    TAttribute<float> SurfaceEdge;
};
} // namespace

SPWTitleScreen::FLayout SPWTitleScreen::ComputeLayout(const float Width, const float Height)
{
    FLayout Layout;
    Layout.LeftPad = Width < 760.0f ? 32.0f : 64.0f;

    // Ornament goes first. The slot is what is left beside a full-width menu, capped at the right third of the screen.
    const float Reach = Layout.LeftPad + TiMenuPreferred + TiScrollPad;
    const float Free = Width - Reach - TiEdgeMargin;
    Layout.ApertureSlot = FMath::Max(0.0f, FMath::Min(Free, Width / 3.0f - TiEdgeMargin));
    const float Tall = FMath::Max(0.0f, Height - 2.0f * TiEdgeMargin);
    const float Target = FMath::Clamp(Height * .42f, TiApertureTargetMin, TiApertureTargetMax);
    const bool bRoomForMotes = Width >= 1440.0f && Layout.ApertureSlot >= TiMoteRoom && Tall >= TiMoteRoom;
    const float Diameter = FMath::Min(Target, FMath::Min(Layout.ApertureSlot, Tall) / (bRoomForMotes ? 1.5f : 1.2f));
    Layout.bAperture = Diameter >= TiApertureSmallest;
    Layout.ApertureDiameter = Layout.bAperture ? Diameter : 0.0f;
    Layout.bMotes = Layout.bAperture && bRoomForMotes;
    Layout.bBrackets = Layout.bAperture;

    // Only then are the controls compressed, and never below a readable column.
    Layout.MenuWidth = FMath::Clamp(Width - Layout.LeftPad - TiScrollPad - TiEdgeMargin, TiMenuMinimum, TiMenuPreferred);
    Layout.SurfaceEdge = Layout.LeftPad + Layout.MenuWidth + TiScrollPad + TiSurfaceBreath;
    Layout.bCompactSummary = Height < TiCompactHeight;
    return Layout;
}

void SPWTitleScreen::SetSpaceForTest(const float Width, const float Height)
{
    SpaceForTest = (Width > 0.0f && Height > 0.0f) ? FVector2D(Width, Height) : FVector2D::ZeroVector;
}

SPWTitleScreen::FLayout SPWTitleScreen::CurrentLayout() const
{
    if (SpaceForTest.X > 0.0 && SpaceForTest.Y > 0.0)
    {
        return ComputeLayout(static_cast<float>(SpaceForTest.X), static_cast<float>(SpaceForTest.Y));
    }
    const FVector2D Size = GetCachedGeometry().GetLocalSize();
    if (Size.X < 1.0 || Size.Y < 1.0)
    {
        return ComputeLayout(TiFallbackWidth, TiFallbackHeight);
    }
    return ComputeLayout(static_cast<float>(Size.X), static_cast<float>(Size.Y));
}

TAttribute<FMargin> SPWTitleScreen::PagePadding(const float Top)
{
    return TAttribute<FMargin>::CreateLambda([this, Top]() { return FMargin(CurrentLayout().LeftPad, Top, TiScrollPad, 24.0f); });
}

// The column keeps its preferred width until the screen is too narrow even with the ornament gone (ComputeLayout).
TSharedRef<SWidget> SPWTitleScreen::MakeRow(const FText& Label, const PWUI::EButtonKind Kind, FOnClicked Click, const bool bEnabled)
{
    return SNew(SBox).WidthOverride_Lambda([this]() { return CurrentLayout().MenuWidth; })
        [SNew(SBox).MinDesiredHeight(44.0f)[PWUI::MakeButton(Label, Kind, Click, bEnabled, 18)]];
}

void SPWTitleScreen::Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator)
{
    (void)InCreator; // The slot summaries come from PWTitleR5, which reads the same slot projection the creator describes.
    PageReveal = PageTransition.AddCurve(0.0f, 0.16f, ECurveEaseFunction::CubicOut);
    PageTransition.JumpToEnd();
    OnContinue = InArgs._OnContinue;
    OnNewGame = InArgs._OnNewGame;
    OnLoad = InArgs._OnLoad;
    OnLoadSlot = InArgs._OnLoadSlot;
    OnSettings = InArgs._OnSettings;
    OnQuit = InArgs._OnQuit;
    Saves = InArgs._Saves != nullptr ? *InArgs._Saves : PWTitleR5::ReadTitleSaves();

    const TSharedRef<SWidget> MainView = BuildMainView();
    const TSharedRef<SWidget> CreditsView = BuildCreditsView();
    const TSharedRef<SWidget> SavesView = BuildSavesView();

    ChildSlot[SNew(SOverlay)
        + SOverlay::Slot()[SNew(SPWTitleGradient).SurfaceEdge_Lambda([this]() { return CurrentLayout().SurfaceEdge; })]
        // The aperture sits in the right third, centred in its box. It is dropped whole when the box is too small for it.
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0, 0, TiEdgeMargin, 0)
        [SNew(SBox)
            .Visibility_Lambda([this]() { return CurrentLayout().bAperture ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            .WidthOverride_Lambda([this]() { return FMath::Max(1.0f, CurrentLayout().ApertureSlot); })
            .HeightOverride_Lambda([this]()
            {
                const FVector2D Size = GetCachedGeometry().GetLocalSize();
                return FMath::Max(1.0f, static_cast<float>(Size.Y < 1.0 ? TiFallbackHeight : Size.Y) - 2.0f * TiEdgeMargin);
            })
            [SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SPWAwakeningSigil).Reveal(true).Rotation(true)
                    .FitRing_Lambda([this]() { return !CurrentLayout().bMotes; })
                    .Diameter_Lambda([this]() { return FMath::Max(1.0f, CurrentLayout().ApertureDiameter); })
                    .Motes_Lambda([this]() { return CurrentLayout().bMotes; })]]]
        + SOverlay::Slot().HAlign(HAlign_Left)
        [SAssignNew(ViewSwitcher, SWidgetSwitcher)
            + SWidgetSwitcher::Slot()[MainView] + SWidgetSwitcher::Slot()[CreditsView] + SWidgetSwitcher::Slot()[SavesView]]];
    PWUI::FocusWhenReady(AsShared(), MainFocusTarget);
}

TSharedRef<SWidget> SPWTitleScreen::BuildMainView()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FSimpleDelegate NewLife = OnNewGame;
    const FSimpleDelegate Load = OnLoad;
    const FSimpleDelegate Settings = OnSettings;
    const FSimpleDelegate Quit = OnQuit;
    const PWTitleR5::EContinueState Continue = Saves.GetContinueState();
    const PWTitleR5::FSlotSummary* Target = Saves.ContinueTarget();
    const bool bHasContinue = Continue != PWTitleR5::EContinueState::None && Target != nullptr;
    const TAttribute<bool> Compact = TAttribute<bool>::CreateLambda([this]() { return CurrentLayout().bCompactSummary; });
    const auto NewLifeClick = [NewLife]() { return FOnClicked::CreateLambda([NewLife]() { NewLife.ExecuteIfBound(); return FReply::Handled(); }); };
    const auto CardWidth = [this]() { return FMath::Max(120.0f, CurrentLayout().MenuWidth - TiCardGutter); };

    // The primary action: Continue when the most recent save can load, a disabled Continue when it cannot, New Life when
    // nothing is saved. A disabled button takes no focus, so focus falls to the first thing that can be done.
    const TSharedRef<SWidget> Primary = MakeRow(
        bHasContinue ? LOCTEXT("Continue", "Continue") : LOCTEXT("NewLifePrimary", "New Life"), PWUI::EButtonKind::Primary,
        bHasContinue ? FOnClicked::CreateLambda([this]() { if (OnContinue.IsBound()) Status = OnContinue.Execute(); return FReply::Handled(); })
                     : NewLifeClick(),
        Continue != PWTitleR5::EContinueState::Blocked);
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Primary];
    TSharedPtr<SWidget> NewLifeRow = Primary;
    if (bHasContinue)
    {
        const TSharedRef<SVerticalBox> Card = SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[PWTitleR5::MakeSummaryBody(*Target, Compact)];
        if (Continue == PWTitleR5::EContinueState::Blocked)
        {
            const FText Hint = Saves.LoadableCount() > 0
                ? LOCTEXT("BlockedOther", "Another save can still be loaded. Choose Load.")
                : LOCTEXT("BlockedNone", "No save here can be loaded. You can still begin a new life.");
            Card->AddSlot().AutoHeight().Padding(0, 8, 0, 0)[PWUI::MakeText(Hint, 15, Style.TextSecondary, false, true)];
        }
        Menu->AddSlot().AutoHeight().Padding(18, 0, 8, 12)
        [SNew(SBox).Tag(TEXT("TitleR5ContinueCard")).WidthOverride_Lambda(CardWidth)[Card]];
        NewLifeRow = MakeRow(LOCTEXT("NewLifeSecondary", "New Life"), PWUI::EButtonKind::TextRow, NewLifeClick());
        Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)[NewLifeRow.ToSharedRef()];
    }
    const TSharedRef<SWidget> LoadRow = MakeRow(LOCTEXT("Load", "Load"), PWUI::EButtonKind::TextRow,
        FOnClicked::CreateLambda([this, Load]() -> FReply
        {
            if (OnLoadSlot.IsBound() && Saves.HasAny()) return ShowSaves();
            Load.ExecuteIfBound();
            return FReply::Handled();
        }), Saves.HasAny());
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)[LoadRow];
    FText LoadNote;
    if (!Saves.HasAny())
    {
        LoadNote = LOCTEXT("NoSaves", "No saved lives yet. Continue and Load appear once you have saved a life.");
    }
    else if (Saves.Slots.Num() > 1)
    {
        LoadNote = FText::Format(LOCTEXT("SaveCount", "{0} saved lives on this device."), FText::AsNumber(Saves.Slots.Num()));
    }
    if (!LoadNote.IsEmpty())
    {
        Menu->AddSlot().AutoHeight().Padding(18, 0, 8, 10)
        [SNew(SBox).Tag(TEXT("TitleR5LoadNote")).WidthOverride_Lambda(CardWidth)[PWUI::MakeText(LoadNote, 14, Style.TextMuted, false, true)]];
    }
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [MakeRow(LOCTEXT("Settings", "Settings"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Settings]() { Settings.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [MakeRow(LOCTEXT("Credits", "Credits"), PWUI::EButtonKind::TextRow, FOnClicked::CreateSP(this, &SPWTitleScreen::ShowCredits))];
    Menu->AddSlot().AutoHeight()
    [MakeRow(LOCTEXT("Quit", "Quit"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Quit]() { Quit.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(12, 8, 0, 0)
    [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Status; }), 16, Style.Danger, false, true)];

    // Where focus starts: Continue when it works, otherwise Load when another save can load, otherwise New Life.
    if (Continue == PWTitleR5::EContinueState::Blocked)
    {
        MainFocusTarget = Saves.LoadableCount() > 0 ? LoadRow : NewLifeRow.ToSharedRef();
    }
    else
    {
        MainFocusTarget = Primary;
    }

    return SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
        + SScrollBox::Slot()
        [SNew(SBox).Padding(PagePadding(34.0f))
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SOverlay)
                // Ornament only where there is room for it; decoration never takes hits or reading order.
                + SOverlay::Slot()
                [SNew(SBox).Visibility_Lambda([this]() { return CurrentLayout().bBrackets ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                    [SNew(SPWCornerBrackets)]]
                + SOverlay::Slot().Padding(24, 16)
                [SNew(SScaleBox).Stretch(EStretch::ScaleToFitX).StretchDirection(EStretchDirection::DownOnly)
                    [PWUI::MakeText(LOCTEXT("TitleMain", "PERFECT WORLD"), PWUI::ETextRole::Title, Style.TextPrimary)]]]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 2, 0, 0)
            [PWUI::MakeText(LOCTEXT("TitleSub", "AWAKENING"), 25, Style.Accent, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 14, 0, 0)
            [PWUI::MakeText(LOCTEXT("LifeLine", "A whole life begins here."), 21, Style.TextPrimary)]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 4, 0, 16)
            [PWUI::MakeText(LOCTEXT("TitlePlace", "Kolkata · 2 June 2042"), 16, Style.TextSecondary)]
            // One short amber rule between the place line and the menu: the only static accent on the page.
            + SVerticalBox::Slot().AutoHeight().Padding(2, 0, 0, 20).HAlign(HAlign_Left)
            [SNew(SBox).WidthOverride(72.0f).HeightOverride(2.0f).Visibility(EVisibility::HitTestInvisible)
                [SNew(SImage).Image(&Style.BrushAccentRule)]]
            + SVerticalBox::Slot().AutoHeight()[Menu]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 18, 0, 0)
            [PWUI::MakeText(LOCTEXT("BuildVersion", "0.1.0 · Development build"), 13, Style.TextSecondary)]]];
}

TSharedRef<SWidget> SPWTitleScreen::BuildCreditsView()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SWidget> Back = MakeRow(LOCTEXT("Back", "Back"), PWUI::EButtonKind::TextRow,
                                             FOnClicked::CreateSP(this, &SPWTitleScreen::ShowMain));
    CreditsFocusTarget = Back;
    return SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
        + SScrollBox::Slot()
        [SNew(SBox).Padding(PagePadding(48.0f))
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 26)[PWUI::MakeText(LOCTEXT("CreditsHeading", "CREDITS"), PWUI::ETextRole::PageHeading, Style.TextPrimary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[PWUI::MakeText(LOCTEXT("CreditsProject", "Perfect World: Awakening"), 22, Style.Accent, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(LOCTEXT("CreditsMadeWith", "Made with Unreal Engine"), 17, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(LOCTEXT("CreditsFont", "Hind Siliguri by Indian Type Foundry"), 17, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 30)[PWUI::MakeText(LOCTEXT("CreditsLicense", "SIL Open Font License 1.1"), 15, Style.TextMuted)]
            + SVerticalBox::Slot().AutoHeight()[Back]]];
}

TSharedRef<SWidget> SPWTitleScreen::BuildSaveCard(const PWTitleR5::FSlotSummary& Summary)
{
    const int32 Slot = Summary.Slot;
    const TAttribute<bool> Compact = TAttribute<bool>::CreateLambda([this]() { return CurrentLayout().bCompactSummary; });
    return SNew(SBox).Tag(TEXT("TitleR5SaveCard")).WidthOverride_Lambda([this]() { return CurrentLayout().MenuWidth; })
        [PWUI::MakeContentButton(
            PWTitleR5::MakeSummaryBody(Summary, Compact), PWUI::EButtonKind::Card,
            FOnClicked::CreateLambda([this, Slot]()
            {
                if (OnLoadSlot.IsBound()) LoadStatus = OnLoadSlot.Execute(Slot);
                return FReply::Handled();
            }),
            false, Summary.CanLoad(), FMargin(20.0f, 14.0f))];
}

// Load, on the title: every saved life as the same summary, newest first. A slot that cannot be read is listed with its
// reason and cannot be chosen, so a damaged file never hides the others and never crashes the list.
TSharedRef<SWidget> SPWTitleScreen::BuildSavesView()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
    TSharedPtr<SWidget> FirstLoadable;
    for (const PWTitleR5::FSlotSummary& Summary : Saves.Slots)
    {
        const TSharedRef<SWidget> Card = BuildSaveCard(Summary);
        if (!FirstLoadable.IsValid() && Summary.CanLoad())
        {
            FirstLoadable = Card;
        }
        List->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Card];
    }
    const TSharedRef<SWidget> Back = MakeRow(LOCTEXT("Back", "Back"), PWUI::EButtonKind::TextRow,
                                             FOnClicked::CreateSP(this, &SPWTitleScreen::ShowMain));
    SavesFocusTarget = FirstLoadable.IsValid() ? FirstLoadable : TSharedPtr<SWidget>(Back);
    return SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
        + SScrollBox::Slot()
        [SNew(SBox).Padding(PagePadding(48.0f))
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(LOCTEXT("SavesHeading", "SAVED LIVES"), PWUI::ETextRole::PageHeading, Style.TextPrimary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)[PWUI::MakeText(LOCTEXT("SavesSub", "Choose a life to continue. The most recent is first."), 16, Style.TextSecondary, false, true)]
            + SVerticalBox::Slot().AutoHeight()[List]
            + SVerticalBox::Slot().AutoHeight().Padding(12, 0, 0, 10)
            [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return LoadStatus; }), 16, Style.Danger, false, true)]
            + SVerticalBox::Slot().AutoHeight()[Back]]];
}

void SPWTitleScreen::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (PWUI::IsReducedMotion()) PageTransition.JumpToEnd();
    ViewSwitcher->SetRenderOpacity(PWUI::MotionLerp(PageReveal.GetLerp()));
}

FReply SPWTitleScreen::SwitchView(const int32 View, const TWeakPtr<SWidget>& Focus)
{
    ActiveView = View;
    ViewSwitcher->SetActiveWidgetIndex(View);
    PageTransition.JumpToEnd();
    PWUI::PlayMotion(PageTransition, AsShared());
    PWUI::CancelPendingFocus(AsShared());
    PWUI::FocusWhenReady(AsShared(), Focus);
    return FReply::Handled();
}

FReply SPWTitleScreen::ShowCredits()
{
    return SwitchView(ViewCredits, CreditsFocusTarget);
}

FReply SPWTitleScreen::ShowSaves()
{
    LoadStatus = FText::GetEmpty();
    return SwitchView(ViewSaves, SavesFocusTarget);
}

FReply SPWTitleScreen::ShowMain()
{
    return SwitchView(ViewMain, MainFocusTarget);
}

FReply SPWTitleScreen::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // A held key auto-repeats; menus react to the first press only.
    if (Event.IsRepeat())
    {
        return FReply::Handled();
    }
    if (ActiveView != ViewMain && (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)) return ShowMain();
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
