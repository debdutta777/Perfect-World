#include "UI/SPWWorldMap.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWMapData.h"
#include "UI/PWWidgets.h"

namespace
{
constexpr float MpStickDeadZone = 0.2f;
constexpr float MpPanPixelsPerSecond = 520.0f;
constexpr double MpWheelZoomPerNotch = 1.2;
constexpr double MpKeyZoomFactor = 1.25;
constexpr double MpTriggerZoomPerSecond = 1.6; // e-fold rate, so a full trigger doubles the scale in about 0.43 s
constexpr double MpZoomEaseRate = 14.0;
constexpr float MpHoverPersonRadiusPx = 14.0f;
constexpr double MpClampMarginCm = 500.0;

float MpDeadZone(float Value)
{
    const float Magnitude = FMath::Abs(Value);
    if (Magnitude < MpStickDeadZone)
    {
        return 0.0f;
    }
    return FMath::Sign(Value) * FMath::Min(1.0f, (Magnitude - MpStickDeadZone) / (1.0f - MpStickDeadZone));
}

bool MpIsCloseKey(const FKey& Key)
{
    return Key == EKeys::Escape || Key == EKeys::M || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right ||
           Key == EKeys::Gamepad_Special_Left || Key == EKeys::Gamepad_Special_Right;
}
} // namespace

FReply SPWWorldMap::HandleKey(const FKey& Key, bool bRepeat)
{
    if (MpIsCloseKey(Key) || ExtraCloseKeys.Contains(Key))
    {
        // A held key auto-repeats; closing reacts to the first press only.
        if (!bRepeat)
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::L)
    {
        if (!bRepeat)
        {
            SetLegendShown(!bLegendShown);
        }
        return FReply::Handled();
    }
    if (Key == EKeys::Home || Key == EKeys::C)
    {
        if (!bRepeat)
        {
            CentreOnPlayer();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::R)
    {
        if (!bRepeat)
        {
            ResetView();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::Equals || Key == EKeys::Add || Key == EKeys::PageUp)
    {
        ZoomBy(MpKeyZoomFactor);
        return FReply::Handled();
    }
    if (Key == EKeys::Hyphen || Key == EKeys::Subtract || Key == EKeys::PageDown)
    {
        ZoomBy(1.0 / MpKeyZoomFactor);
        return FReply::Handled();
    }
    // Arrow keys and W A S D pan while held (Tick applies them); up is +Y.
    if (Key == EKeys::Left || Key == EKeys::A) { KeyPan.X = -1.0f; return FReply::Handled(); }
    if (Key == EKeys::Right || Key == EKeys::D) { KeyPan.X = 1.0f; return FReply::Handled(); }
    if (Key == EKeys::Up || Key == EKeys::W) { KeyPan.Y = 1.0f; return FReply::Handled(); }
    if (Key == EKeys::Down || Key == EKeys::S) { KeyPan.Y = -1.0f; return FReply::Handled(); }
    return FReply::Unhandled();
}

FReply SPWWorldMap::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    return HandleKey(InKeyEvent.GetKey(), InKeyEvent.IsRepeat());
}

FReply SPWWorldMap::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Right || Key == EKeys::D)
    {
        KeyPan.X = 0.0f;
        return FReply::Handled();
    }
    if (Key == EKeys::Up || Key == EKeys::W || Key == EKeys::Down || Key == EKeys::S)
    {
        KeyPan.Y = 0.0f;
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply SPWWorldMap::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
    const FKey Key = InAnalogInputEvent.GetKey();
    const float Value = InAnalogInputEvent.GetAnalogValue();
    // The left stick is handled in full, so Slate does not also turn it into focus movement between the buttons.
    if (Key == EKeys::Gamepad_LeftX)
    {
        StickPan.X = MpDeadZone(Value);
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_LeftY)
    {
        StickPan.Y = MpDeadZone(Value); // up is positive
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_RightTriggerAxis)
    {
        TriggerIn = MpDeadZone(Value);
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_LeftTriggerAxis)
    {
        TriggerOut = MpDeadZone(Value);
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply SPWWorldMap::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button != EKeys::LeftMouseButton && Button != EKeys::MiddleMouseButton && Button != EKeys::RightMouseButton)
    {
        return FReply::Unhandled();
    }
    const FVector2f Pixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    bDragging = true;
    LastDragPixel = Pixel;
    CursorPixel = Pixel;
    bCursorInside = true;
    return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SPWWorldMap::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (!bDragging)
    {
        return FReply::Unhandled();
    }
    bDragging = false;
    return FReply::Handled().ReleaseMouseCapture();
}

FReply SPWWorldMap::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const FVector2f Pixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    CursorPixel = Pixel;
    bCursorInside = true;
    if (bDragging && HasMouseCapture())
    {
        PanBy(Pixel - LastDragPixel);
        LastDragPixel = Pixel;
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

FReply SPWWorldMap::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const float Notches = MouseEvent.GetWheelDelta();
    if (FMath::IsNearlyZero(Notches))
    {
        return FReply::Unhandled();
    }
    const FVector2f Pixel = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    CursorPixel = Pixel;
    bCursorInside = true;
    ApplyZoom(FMath::Pow(MpWheelZoomPerNotch, static_cast<double>(Notches)), Pixel, false);
    return FReply::Handled();
}

void SPWWorldMap::OnMouseLeave(const FPointerEvent& MouseEvent)
{
    bCursorInside = false;
}

void SPWWorldMap::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
    bDragging = false;
}

void SPWWorldMap::OnFocusLost(const FFocusEvent& InFocusEvent)
{
    // A key released while focus was elsewhere would otherwise leave the map drifting.
    KeyPan = FVector2f::ZeroVector;
}

void SPWWorldMap::Refit(const FVector2f& NewSpace)
{
    Space = NewSpace;
    if (!Live.IsValid())
    {
        return;
    }
    const FPWMapScene& Scene = Live->GetScene();
    const float Top = TopBar.IsValid() ? TopBar->GetTickSpaceGeometry().GetLocalSize().Y + 24.0f : 0.0f;
    const float Bottom = (LegendPanel.IsValid() && bLegendShown) ? LegendPanel->GetTickSpaceGeometry().GetLocalSize().Y + 24.0f : 12.0f;
    // Fit the lane in the band between the bars, then slide the result down or up to where that band is.
    const double BandHeight = FMath::Max(80.0, static_cast<double>(NewSpace.Y - Top - Bottom));
    Fitted = PerfectWorld::LaneMap::FitView(Scene.Map.BoundsMin, Scene.Map.BoundsMax, {NewSpace.X, BandHeight}, PWMap::WorldMapPadding(NewSpace),
                                            PerfectWorld::LaneMap::NorthUpHeadingDegrees);
    PerfectWorld::LaneMap::PanByPixels(Fitted, {0.0, static_cast<double>(Top - Bottom) * 0.5});
    Limits = PerfectWorld::LaneMap::ZoomLimitsFor(Fitted);
    if (!bUserMoved || !bHasLayout)
    {
        View = Fitted;
    }
    else
    {
        View.PixelsPerCm = FMath::Clamp(View.PixelsPerCm, Limits.MinPixelsPerCm, Limits.MaxPixelsPerCm);
    }
    bHasLayout = true;
}

void SPWWorldMap::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    Now = InCurrentTime;
    if (!Live.IsValid())
    {
        return;
    }
    Live->Refresh();

    const FVector2f NewSpace = AllottedGeometry.GetLocalSize();
    const float Top = TopBar.IsValid() ? TopBar->GetTickSpaceGeometry().GetLocalSize().Y : 0.0f;
    const float Bottom = (LegendPanel.IsValid() && bLegendShown) ? LegendPanel->GetTickSpaceGeometry().GetLocalSize().Y : 0.0f;
    const FVector2f Bars(Top, Bottom);
    if (NewSpace.X > 1.0f && NewSpace.Y > 1.0f && (!bHasLayout || !NewSpace.Equals(Space, 0.5f) || !Bars.Equals(LastBars, 2.0f)))
    {
        LastBars = Bars;
        Refit(NewSpace);
    }

    // Held keys and the left stick pan; the trigger zooms about the middle of the window.
    FVector2f Pan = StickPan + KeyPan;
    if (Pan.SizeSquared() > 1.0f)
    {
        Pan.Normalize();
    }
    if (!Pan.IsNearlyZero())
    {
        // Pushing right or up moves the view that way, which moves the map the other way.
        PanBy(FVector2f(-Pan.X, Pan.Y) * (MpPanPixelsPerSecond * InDeltaTime));
    }
    const float Trigger = TriggerIn - TriggerOut;
    if (!FMath::IsNearlyZero(Trigger))
    {
        ApplyZoom(FMath::Exp(static_cast<double>(Trigger) * MpTriggerZoomPerSecond * InDeltaTime), Space * 0.5f, true);
    }

    // An eased zoom takes a fraction of what is left each frame; reduced motion applies it whole (ApplyZoom already does, so this is empty then).
    if (!FMath::IsNearlyZero(PendingZoomLog, 1.0e-4))
    {
        const double Take = PendingZoomLog * (1.0 - FMath::Exp(-MpZoomEaseRate * static_cast<double>(InDeltaTime)));
        PerfectWorld::LaneMap::ZoomAbout(View, {Space.X, Space.Y}, {PendingZoomAnchor.X, PendingZoomAnchor.Y}, FMath::Exp(Take), Limits);
        PendingZoomLog -= Take;
    }
    else
    {
        PendingZoomLog = 0.0;
    }
    PerfectWorld::LaneMap::ClampCentre(View, Live->GetScene().Map.BoundsMin, Live->GetScene().Map.BoundsMax, MpClampMarginCm);
    UpdateHover();
}

void SPWWorldMap::UpdateHover()
{
    HoverText = FText::GetEmpty();
    HoverBuilding = INDEX_NONE;
    if (!bCursorInside || !Live.IsValid() || bDragging)
    {
        return;
    }
    const FPWMapScene& Scene = Live->GetScene();
    const FPWMapSnapshot& Snapshot = Live->GetSnapshot();
    const PerfectWorld::LaneMap::Size Widget{Space.X, Space.Y};

    // A person under the pointer wins over the building they stand in.
    float BestDistance = MpHoverPersonRadiusPx;
    for (const FPWMapMarker& Person : Snapshot.People)
    {
        const PerfectWorld::LaneMap::Point Pixel = PerfectWorld::LaneMap::WorldToWidget(View, Widget, Person.WorldCm);
        const float Distance = FVector2f::Distance(CursorPixel, FVector2f(static_cast<float>(Pixel.X), static_cast<float>(Pixel.Y)));
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            HoverText = FText::Format(NSLOCTEXT("PerfectWorldWorldMap", "HoverPerson", "{0}, here now."), Person.Name);
        }
    }
    if (!HoverText.IsEmpty())
    {
        return;
    }
    const PerfectWorld::LaneMap::Point World = PerfectWorld::LaneMap::WidgetToWorld(View, Widget, {CursorPixel.X, CursorPixel.Y});
    if (const PerfectWorld::LaneMap::Building* Building = PerfectWorld::LaneMap::FindBuildingAt(Scene.Map, World))
    {
        HoverBuilding = static_cast<int32>(Building - Scene.Map.Buildings.data());
        HoverText = PWMap::DescribeBuilding(Scene, Snapshot, HoverBuilding);
    }
}
