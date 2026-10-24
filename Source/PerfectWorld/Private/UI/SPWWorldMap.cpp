#include "UI/SPWWorldMap.h"

#include "Layout/Clipping.h"
#include "Rendering/DrawElements.h"
#include "UI/PWMapData.h"
#include "UI/PWMapPainter.h"
#include "UI/PWMapPlan.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldWorldMap"

namespace PWMap
{
bool WorldMapCompact(const FVector2f& Space)
{
    return Space.X < 760.0f || Space.Y < 760.0f;
}

float WorldMapPadding(const FVector2f& Space)
{
    return WorldMapCompact(Space) ? 12.0f : 24.0f;
}
} // namespace PWMap

namespace
{
constexpr float MpEdgeMargin = 12.0f;
constexpr float MpSwatchPx = 16.0f;
constexpr float MpLegendTextSize = 14.0f;

/** One legend swatch: a surface or building colour, a door or person mark, or the player marker. It paints through the same painter as the map. */
class SMpLegendSwatch final : public SLeafWidget
{
public:
    enum class EKind : uint8 { Quad, Mark, Player };

    SLATE_BEGIN_ARGS(SMpLegendSwatch) : _Kind(EKind::Quad), _Quad(PWMap::EQuadRole::Road), _Mark(PWMap::EDotRole::Person) {}
        SLATE_ARGUMENT(EKind, Kind)
        SLATE_ARGUMENT(PWMap::EQuadRole, Quad)
        SLATE_ARGUMENT(PWMap::EDotRole, Mark)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Kind = InArgs._Kind;
        Quad = InArgs._Quad;
        Mark = InArgs._Mark;
        SetVisibility(EVisibility::HitTestInvisible);
    }

    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(MpSwatchPx, MpSwatchPx); }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        const FVector2f Centre = AllottedGeometry.GetLocalSize() * 0.5f;
        const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
        switch (Kind)
        {
        case EKind::Quad:
            PWMap::PaintSwatch(Quad, Centre, MpSwatchPx - 4.0f, AllottedGeometry, OutDrawElements, LayerId, Tint);
            return LayerId + 2;
        case EKind::Mark:
            PWMap::PaintMark(Mark, Centre, MpSwatchPx * 0.55f, AllottedGeometry, OutDrawElements, LayerId, Tint);
            return LayerId + 2;
        case EKind::Player:
        {
            PWMap::FPlanArrow Arrow;
            Arrow.bVisible = true;
            Arrow.Position = Centre - FVector2f(0.0f, 2.0f);
            Arrow.AngleDegrees = 0.0f;
            return PWMap::PaintPlayer(Arrow, 0.8f, -1.0f, AllottedGeometry, OutDrawElements, LayerId, Tint) + 1;
        }
        }
        return LayerId;
    }

private:
    EKind Kind = EKind::Quad;
    PWMap::EQuadRole Quad = PWMap::EQuadRole::Road;
    PWMap::EDotRole Mark = PWMap::EDotRole::Person;
};

void MpAddLegendItem(SWrapBox& Row, const TSharedRef<SWidget>& Swatch, const FText& Label)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Row.AddSlot().VAlign(VAlign_Center)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SBox).WidthOverride(MpSwatchPx).HeightOverride(MpSwatchPx)
            [
                Swatch
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            PWUI::MakeText(Label, 14, Style.TextSecondary)
        ]
    ];
}
} // namespace

void SPWWorldMap::Construct(const FArguments& InArgs)
{
    Live = InArgs._Live;
    OnClose = InArgs._OnClose;
    ExtraCloseKeys = InArgs._ExtraCloseKeys;
    if (Live.IsValid())
    {
        Live->Refresh();
    }
    TopBar = BuildTopBar();
    LegendPanel = BuildLegend();

    ChildSlot
    [
        SNew(SBox)
        .Tag(FName(PWMap::WorldMapTag))
        [
            SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top).Padding(MpEdgeMargin)
            [
                TopBar.ToSharedRef()
            ]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(MpEdgeMargin)
            [
                LegendPanel.ToSharedRef()
            ]
        ]
    ];
}

TSharedRef<SWidget> SPWWorldMap::BuildTopBar()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.0, 6.0));
    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeText(LOCTEXT("MapTitle", "Map"), PWUI::ETextRole::SectionHeading, Style.TextPrimary)
    ];
    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeButton(LOCTEXT("CentreOnMe", "Centre on me"), PWUI::EButtonKind::Secondary, FOnClicked::CreateLambda([this]()
        {
            CentreOnPlayer();
            return FReply::Handled();
        }), true, 16)
    ];
    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeButton(LOCTEXT("ZoomOut", "Zoom out"), PWUI::EButtonKind::Secondary, FOnClicked::CreateLambda([this]()
        {
            ZoomBy(1.0 / 1.4);
            return FReply::Handled();
        }), true, 16)
    ];
    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeButton(LOCTEXT("ZoomIn", "Zoom in"), PWUI::EButtonKind::Secondary, FOnClicked::CreateLambda([this]()
        {
            ZoomBy(1.4);
            return FReply::Handled();
        }), true, 16)
    ];
    Row->AddSlot().VAlign(VAlign_Center)
    [
        PWUI::MakeButton(LOCTEXT("ShowLegend", "Legend"), PWUI::EButtonKind::Chip, FOnClicked::CreateLambda([this]()
        {
            SetLegendShown(!bLegendShown);
            return FReply::Handled();
        }), true, 16, 0.0f, TAttribute<bool>::CreateLambda([this]() { return bLegendShown; }))
    ];
    Row->AddSlot().VAlign(VAlign_Center)
    [
        SNew(SBox).Tag(FName(PWMap::WorldMapCloseTag))
        [
            PWUI::MakeButton(LOCTEXT("CloseMap", "Close"), PWUI::EButtonKind::Primary, FOnClicked::CreateLambda([this]()
            {
                RequestClose();
                return FReply::Handled();
            }), true, 16)
        ]
    ];

    // One quiet line under the buttons: what the pointer is on, or else how to move.
    const TWeakPtr<SPWWorldMap> Weak = StaticCastSharedRef<SPWWorldMap>(AsShared());
    const TAttribute<FText> Line = TAttribute<FText>::CreateLambda([Weak]()
    {
        const TSharedPtr<SPWWorldMap> Self = Weak.Pin();
        if (Self.IsValid() && !Self->GetHoverText().IsEmpty())
        {
            return Self->GetHoverText();
        }
        return LOCTEXT("MapHint", "Drag or left stick to move. Wheel or triggers to zoom. L toggles the legend. M, Esc or B closes.");
    });

    return SNew(SBox).Tag(FName(PWMap::WorldMapTopBarTag))
    [
        PWUI::MakePanel(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                Row
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                PWUI::MakeText(Line, PWUI::ETextRole::Meta, Style.TextSecondary, true)
            ],
            FMargin(12.0f, 8.0f))
    ];
}

TSharedRef<SWidget> SPWWorldMap::BuildLegend()
{
    using PWMap::EDotRole;
    using PWMap::EQuadRole;
    const TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(14.0, 6.0));
    const auto Quad = [](EQuadRole Role) -> TSharedRef<SWidget> { return SNew(SMpLegendSwatch).Kind(SMpLegendSwatch::EKind::Quad).Quad(Role); };
    const auto Mark = [](EDotRole Role) -> TSharedRef<SWidget> { return SNew(SMpLegendSwatch).Kind(SMpLegendSwatch::EKind::Mark).Mark(Role); };
    MpAddLegendItem(*Row, SNew(SMpLegendSwatch).Kind(SMpLegendSwatch::EKind::Player), LOCTEXT("LegendYou", "You are here"));
    MpAddLegendItem(*Row, Mark(EDotRole::Person), LOCTEXT("LegendPerson", "Person"));
    MpAddLegendItem(*Row, Quad(EQuadRole::Home), LOCTEXT("LegendHome", "Home"));
    MpAddLegendItem(*Row, Quad(EQuadRole::Shop), LOCTEXT("LegendShop", "Shop"));
    MpAddLegendItem(*Row, Quad(EQuadRole::Workshop), LOCTEXT("LegendWorkshop", "Workshop"));
    MpAddLegendItem(*Row, Quad(EQuadRole::School), LOCTEXT("LegendSchool", "School"));
    MpAddLegendItem(*Row, Quad(EQuadRole::Sealed), LOCTEXT("LegendSealed", "Closed up"));
    MpAddLegendItem(*Row, Quad(EQuadRole::Road), LOCTEXT("LegendRoad", "Road"));
    MpAddLegendItem(*Row, Quad(EQuadRole::Footpath), LOCTEXT("LegendFootpath", "Footpath"));
    MpAddLegendItem(*Row, Mark(EDotRole::DoorOpen), LOCTEXT("LegendDoorOpen", "Door, open to all"));
    MpAddLegendItem(*Row, Mark(EDotRole::DoorClosed), LOCTEXT("LegendDoorClosed", "Door, closed for now"));
    MpAddLegendItem(*Row, Mark(EDotRole::DoorPrivate), LOCTEXT("LegendDoorPrivate", "Door, private (knock)"));
    MpAddLegendItem(*Row, Mark(EDotRole::DoorLocked), LOCTEXT("LegendDoorLocked", "Door, locked"));

    return SNew(SBox).Tag(FName(PWMap::WorldMapLegendTag))
        .Visibility_Lambda([this]() { return bLegendShown ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
        .MaxDesiredWidth_Lambda([this]() { return FOptionalSize(Space.X - 2.0f * MpEdgeMargin); })
        [
            PWUI::MakePanel(Row, FMargin(12.0f, 8.0f))
        ];
}

void SPWWorldMap::ResetView()
{
    View = Fitted;
    PendingZoomLog = 0.0;
    bUserMoved = false;
}

void SPWWorldMap::CentreOnPlayer()
{
    if (!Live.IsValid() || !Live->GetSnapshot().bHasPlayer)
    {
        return;
    }
    PendingZoomLog = 0.0;
    View.CentreCm = Live->GetSnapshot().PlayerCm;
    // From the whole-lane fit, step in to street scale so "centre" shows something; if already closer, keep the scale.
    const double Street = FMath::Min(FMath::Max(Fitted.PixelsPerCm * 2.2, 0.15), Limits.MaxPixelsPerCm);
    View.PixelsPerCm = FMath::Clamp(FMath::Max(View.PixelsPerCm, Street), Limits.MinPixelsPerCm, Limits.MaxPixelsPerCm);
    PerfectWorld::LaneMap::ClampCentre(View, Live->GetScene().Map.BoundsMin, Live->GetScene().Map.BoundsMax, 500.0);
    bUserMoved = true;
}

void SPWWorldMap::ZoomBy(double Factor, bool bImmediate)
{
    ApplyZoom(Factor, Space * 0.5f, bImmediate);
}

void SPWWorldMap::ApplyZoom(double Factor, const FVector2f& AnchorPixel, bool bImmediate)
{
    if (!(Factor > 0.0))
    {
        return;
    }
    bUserMoved = true;
    if (bImmediate || PWUI::IsReducedMotion())
    {
        PerfectWorld::LaneMap::ZoomAbout(View, {Space.X, Space.Y}, {AnchorPixel.X, AnchorPixel.Y}, Factor, Limits);
        return;
    }
    PendingZoomLog += FMath::Loge(Factor);
    PendingZoomAnchor = AnchorPixel;
}

void SPWWorldMap::PanBy(const FVector2f& DeltaPixels)
{
    PerfectWorld::LaneMap::PanByPixels(View, {DeltaPixels.X, DeltaPixels.Y});
    bUserMoved = true;
}

void SPWWorldMap::SetLegendShown(bool bShown)
{
    bLegendShown = bShown;
}

void SPWWorldMap::RequestClose()
{
    OnClose.ExecuteIfBound();
}

int32 SPWWorldMap::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                           int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FVector2f Size = AllottedGeometry.GetLocalSize();
    const FLinearColor Tint = InWidgetStyle.GetColorAndOpacityTint();
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), &Style.BrushSideShade, ESlateDrawEffect::None, Tint);
    int32 Layer = LayerId + 1;

    if (Live.IsValid())
    {
        const FPWMapScene& Scene = Live->GetScene();
        const FPWMapSnapshot& Snapshot = Live->GetSnapshot();
        OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));

        PWMap::FPlanOptions Options;
        Options.DotInsetPx = 6.0f;
        const PWMap::FMapPlan Plan = PWMap::BuildPlan(Scene, Snapshot, View, Size, PerfectWorld::LaneMap::Rect{0.0, 0.0, Size.X, Size.Y}, Options);
        Layer = PWMap::PaintQuads(Plan, AllottedGeometry, OutDrawElements, Layer, Tint, true) + 1;
        if (HoverBuilding != INDEX_NONE)
        {
            for (const PWMap::FPlanQuad& Quad : Plan.Quads)
            {
                if (Quad.bBuilding && Quad.SourceIndex == HoverBuilding)
                {
                    PWMap::PaintQuadOutline(Quad, Style.Accent * Tint, 2.0f, AllottedGeometry, OutDrawElements, Layer);
                }
            }
        }
        ++Layer;

        const float Zoom = static_cast<float>(View.PixelsPerCm / FMath::Max(Fitted.PixelsPerCm, 1.0e-6));
        const float MarkPx = FMath::Clamp(6.0f + (Zoom - 1.0f) * 1.5f, 7.0f, 13.0f);
        Layer = PWMap::PaintDots(Plan, AllottedGeometry, OutDrawElements, Layer, Tint, MarkPx, MarkPx + 1.0f) + 1;

        // Labels: the player first, then the people, then the buildings, each only where it does not cover a better one.
        const FSlateFontInfo Font = PWMap::LabelFont(PWMap::WorldMapCompact(Size) ? 13 : 15);
        struct FMpLabelEntry
        {
            FText Text;
            FVector2f Centre;
        };
        TArray<FMpLabelEntry> Entries;
        std::vector<PerfectWorld::LaneMap::LabelCandidate> Candidates;
        const auto AddLabel = [&](const FText& Text, const FVector2f& Centre, int32 Priority)
        {
            if (Text.IsEmpty())
            {
                return;
            }
            const FVector2f Measured = PWMap::MeasureText(Text, Font);
            Entries.Add({Text, Centre});
            PerfectWorld::LaneMap::LabelCandidate Candidate;
            Candidate.CentrePx = {Centre.X, Centre.Y};
            Candidate.WidthPx = Measured.X + 2.0;
            Candidate.HeightPx = Measured.Y;
            Candidate.Priority = Priority;
            Candidates.push_back(Candidate);
        };
        if (Plan.Player.bVisible)
        {
            AddLabel(LOCTEXT("YouAreHere", "You are here"), Plan.Player.Position + FVector2f(0.0f, 24.0f), 1000);
        }
        for (const PWMap::FPlanDot& Dot : Plan.Dots)
        {
            if (Dot.Role == PWMap::EDotRole::Person && Snapshot.People.IsValidIndex(Dot.SourceIndex))
            {
                AddLabel(Snapshot.People[Dot.SourceIndex].Name, Dot.Position + FVector2f(0.0f, -14.0f), 500);
            }
        }
        for (const PWMap::FPlanQuad& Quad : Plan.Quads)
        {
            if (!Quad.bBuilding || !Scene.BuildingLabels.IsValidIndex(Quad.SourceIndex) || Scene.BuildingLabels[Quad.SourceIndex].IsEmpty())
            {
                continue;
            }
            const bool bHome = Snapshot.PlayerHomeBuilding == Quad.SourceIndex;
            const int32 Priority = Quad.SourceIndex == HoverBuilding ? 800 : (bHome ? 100 : 10);
            AddLabel(bHome ? LOCTEXT("YourHome", "Your home") : Scene.BuildingLabels[Quad.SourceIndex], Quad.Centre, Priority);
        }
        // The bars lie over the map; labels keep out from under them.
        const float TopReserve = TopBar.IsValid() ? TopBar->GetTickSpaceGeometry().GetLocalSize().Y + MpEdgeMargin : 0.0f;
        const float BottomReserve = (LegendPanel.IsValid() && bLegendShown) ? LegendPanel->GetTickSpaceGeometry().GetLocalSize().Y + MpEdgeMargin : 0.0f;
        const PerfectWorld::LaneMap::Rect LabelClip{4.0, static_cast<double>(TopReserve), Size.X - 4.0, Size.Y - static_cast<double>(BottomReserve)};
        const std::vector<bool> Placed = PerfectWorld::LaneMap::PlaceLabels(Candidates, LabelClip, 3.0);
        for (std::size_t Index = 0; Index < Placed.size(); ++Index)
        {
            if (Placed[Index])
            {
                PWMap::PaintLabel(Entries[static_cast<int32>(Index)].Text, Entries[static_cast<int32>(Index)].Centre, Font, AllottedGeometry, OutDrawElements, Layer, Tint);
            }
        }
        Layer += 2;

        // Reduced motion draws a still ring; otherwise the ring breathes out about once a second.
        const float Pulse = PWUI::IsReducedMotion() ? 0.0f : static_cast<float>(FMath::Fmod(Now, 1.25) / 1.25);
        Layer = PWMap::PaintPlayer(Plan.Player, 1.5f, Pulse, AllottedGeometry, OutDrawElements, Layer, Tint) + 1;
        OutDrawElements.PopClip();
    }
    return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, Layer + 1, InWidgetStyle, bParentEnabled);
}

#undef LOCTEXT_NAMESPACE
