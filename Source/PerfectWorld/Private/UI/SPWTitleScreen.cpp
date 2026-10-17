#include "UI/SPWTitleScreen.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Rendering/DrawElements.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
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
    return Layout;
}

SPWTitleScreen::FLayout SPWTitleScreen::CurrentLayout() const
{
    const FVector2D Size = GetCachedGeometry().GetLocalSize();
    if (Size.X < 1.0 || Size.Y < 1.0)
    {
        return ComputeLayout(TiFallbackWidth, TiFallbackHeight);
    }
    return ComputeLayout(static_cast<float>(Size.X), static_cast<float>(Size.Y));
}

void SPWTitleScreen::Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    PageReveal = PageTransition.AddCurve(0.0f, 0.16f, ECurveEaseFunction::CubicOut);
    PageTransition.JumpToEnd();
    OnContinue = InArgs._OnContinue;
    ContinueSlot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    if (ContinueSlot >= 0 && InCreator) ContinueNote = InCreator->DescribeSaveSlot(ContinueSlot);

    const FSimpleDelegate NewLife = InArgs._OnNewGame;
    const FSimpleDelegate Load = InArgs._OnLoad;
    const FSimpleDelegate Settings = InArgs._OnSettings;
    const FSimpleDelegate Quit = InArgs._OnQuit;
    const bool bHasSave = ContinueSlot >= 0;
    // The column keeps its preferred width until the screen is too narrow even with the ornament gone (ComputeLayout).
    const auto Row = [this](const FText& Label, PWUI::EButtonKind Kind, FOnClicked Click) -> TSharedRef<SWidget>
    {
        return SNew(SBox).WidthOverride_Lambda([this]() { return CurrentLayout().MenuWidth; })
            [SNew(SBox).MinDesiredHeight(44.0f)[PWUI::MakeButton(Label, Kind, Click, true, 18)]];
    };
    const auto PagePadding = [this](const float Top)
    {
        return TAttribute<FMargin>::CreateLambda([this, Top]() { return FMargin(CurrentLayout().LeftPad, Top, TiScrollPad, 24.0f); });
    };

    const TSharedRef<SWidget> Primary = Row(
        bHasSave ? LOCTEXT("Continue", "Continue") : LOCTEXT("NewLifePrimary", "New Life"),
        PWUI::EButtonKind::Primary,
        bHasSave ? FOnClicked::CreateLambda([this]() { if (OnContinue.IsBound()) Status = OnContinue.Execute(); return FReply::Handled(); })
                 : FOnClicked::CreateLambda([NewLife]() { NewLife.ExecuteIfBound(); return FReply::Handled(); }));
    MainFocusTarget = Primary;
    TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 6)[Primary];
    if (bHasSave)
    {
        Menu->AddSlot().AutoHeight().Padding(18, 0, 8, 12)[PWUI::MakeText(ContinueNote, 15, Style.TextSecondary, false, true)];
        Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
        [Row(LOCTEXT("NewLifeSecondary", "New Life"), PWUI::EButtonKind::TextRow,
             FOnClicked::CreateLambda([NewLife]() { NewLife.ExecuteIfBound(); return FReply::Handled(); }))];
    }
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [Row(LOCTEXT("Load", "Load"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Load]() { Load.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [Row(LOCTEXT("Settings", "Settings"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Settings]() { Settings.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(0, 0, 0, 2)
    [Row(LOCTEXT("Credits", "Credits"), PWUI::EButtonKind::TextRow, FOnClicked::CreateSP(this, &SPWTitleScreen::ShowCredits))];
    Menu->AddSlot().AutoHeight()
    [Row(LOCTEXT("Quit", "Quit"), PWUI::EButtonKind::TextRow, FOnClicked::CreateLambda([Quit]() { Quit.ExecuteIfBound(); return FReply::Handled(); }))];
    Menu->AddSlot().AutoHeight().Padding(12, 8, 0, 0)
    [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Status; }), 16, Style.Danger, false, true)];

    const TSharedRef<SWidget> MainView = SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
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
            [PWUI::MakeText(LOCTEXT("TitlePlace", "Kolkata \u00b7 2 June 2042"), 16, Style.TextSecondary)]
            // One short amber rule between the place line and the menu: the only static accent on the page.
            + SVerticalBox::Slot().AutoHeight().Padding(2, 0, 0, 20).HAlign(HAlign_Left)
            [SNew(SBox).WidthOverride(72.0f).HeightOverride(2.0f).Visibility(EVisibility::HitTestInvisible)
                [SNew(SImage).Image(&Style.BrushAccentRule)]]
            + SVerticalBox::Slot().AutoHeight()[Menu]
            + SVerticalBox::Slot().AutoHeight().Padding(2, 18, 0, 0)
            [PWUI::MakeText(LOCTEXT("BuildVersion", "0.1.0 · Development build"), 13, Style.TextSecondary)]]];

    const TSharedRef<SWidget> Back = Row(LOCTEXT("Back", "Back"), PWUI::EButtonKind::TextRow,
                                         FOnClicked::CreateSP(this, &SPWTitleScreen::CloseCredits));
    CreditsFocusTarget = Back;
    const TSharedRef<SWidget> CreditsView = SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
        + SScrollBox::Slot()
        [SNew(SBox).Padding(PagePadding(48.0f))
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 26)[PWUI::MakeText(LOCTEXT("CreditsHeading", "CREDITS"), PWUI::ETextRole::PageHeading, Style.TextPrimary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)[PWUI::MakeText(LOCTEXT("CreditsProject", "Perfect World: Awakening"), 22, Style.Accent, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(LOCTEXT("CreditsMadeWith", "Made with Unreal Engine"), 17, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[PWUI::MakeText(LOCTEXT("CreditsFont", "Hind Siliguri by Indian Type Foundry"), 17, Style.TextSecondary)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 30)[PWUI::MakeText(LOCTEXT("CreditsLicense", "SIL Open Font License 1.1"), 15, Style.TextMuted)]
            + SVerticalBox::Slot().AutoHeight()[Back]]];

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
            + SWidgetSwitcher::Slot()[MainView] + SWidgetSwitcher::Slot()[CreditsView]]];
    PWUI::FocusWhenReady(AsShared(), MainFocusTarget);
}

void SPWTitleScreen::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (PWUI::IsReducedMotion()) PageTransition.JumpToEnd();
    ViewSwitcher->SetRenderOpacity(PWUI::MotionLerp(PageReveal.GetLerp()));
}

FReply SPWTitleScreen::ShowCredits()
{
    bShowingCredits = true;
    ViewSwitcher->SetActiveWidgetIndex(1);
    PageTransition.JumpToEnd();
    PWUI::PlayMotion(PageTransition, AsShared());
    PWUI::CancelPendingFocus(AsShared());
    PWUI::FocusWhenReady(AsShared(), CreditsFocusTarget);
    return FReply::Handled();
}

FReply SPWTitleScreen::CloseCredits()
{
    bShowingCredits = false;
    ViewSwitcher->SetActiveWidgetIndex(0);
    PageTransition.JumpToEnd();
    PWUI::PlayMotion(PageTransition, AsShared());
    PWUI::CancelPendingFocus(AsShared());
    PWUI::FocusWhenReady(AsShared(), MainFocusTarget);
    return FReply::Handled();
}

FReply SPWTitleScreen::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // A held key auto-repeats; menus react to the first press only.
    if (Event.IsRepeat())
    {
        return FReply::Handled();
    }
    if (bShowingCredits && (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)) return CloseCredits();
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
