#include "PerfectWorldAwakeningHud.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWeakWidget.h"

namespace PWAwakening
{
namespace
{
/** Below the front end (100) and the conversation panel (110), above the interaction notice (41). */
constexpr int32 AwFlHudZOrder = 46;
constexpr float AwFlCaptionBottomMargin = 290.0f;
constexpr float AwFlCueTopMargin = 90.0f;
} // namespace

FAwHud::~FAwHud()
{
    Remove();
}

void FAwHud::SetTextScale(const float InTextScale)
{
    TextScale = FMath::Clamp(InTextScale, 1.0f, 2.0f);
}

void FAwHud::ShowCaption(const FText& Text, const double NowSeconds, const float Seconds)
{
    Caption = Text;
    CaptionVisible = !Text.IsEmpty();
    CaptionUntil = NowSeconds + static_cast<double>(FMath::Max(1.0f, Seconds));
}

void FAwHud::Update(const double NowSeconds)
{
    if (CaptionVisible && NowSeconds >= CaptionUntil)
    {
        CaptionVisible = false;
    }
}

bool FAwHud::Ensure(const float InTextScale)
{
    const float PreviousScale = TextScale;
    SetTextScale(InTextScale);
    if (Holder.IsValid())
    {
        if (FMath::IsNearlyEqual(PreviousScale, TextScale))
        {
            return true;
        }
        Remove(); // font sizes are fixed when the widgets are built, so a new scale rebuilds them
    }
    if (GEngine == nullptr || GEngine->GameViewport == nullptr)
    {
        return false;
    }
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TWeakPtr<FAwHud> Weak = AsShared();
    const auto Size = [Weak](const int32 Base)
    {
        const TSharedPtr<FAwHud> Hud = Weak.Pin();
        return FMath::RoundToInt(static_cast<float>(Base) * (Hud.IsValid() ? Hud->GetTextScale() : 1.0f));
    };

    const TSharedRef<SWidget> Root =
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
            .BorderBackgroundColor_Lambda([Weak]()
            {
                const TSharedPtr<FAwHud> Hud = Weak.Pin();
                return FSlateColor(FLinearColor(0.0f, 0.0f, 0.0f, Hud.IsValid() ? Hud->GetFade() : 0.0f));
            })
            .Visibility_Lambda([Weak]()
            {
                const TSharedPtr<FAwHud> Hud = Weak.Pin();
                return Hud.IsValid() && Hud->GetFade() > 0.001f ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, AwFlCaptionBottomMargin))
        [
            SNew(SBox)
            .MaxDesiredWidth(1000.0f)
            .Visibility_Lambda([Weak]()
            {
                const TSharedPtr<FAwHud> Hud = Weak.Pin();
                return Hud.IsValid() && Hud->IsCaptionVisible() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
            [
                SNew(SBorder)
                .BorderImage(&Style.BrushToast)
                .Padding(FMargin(22.0f, 10.0f))
                [
                    PWUI::MakeText(TAttribute<FText>::CreateLambda([Weak]()
                                   {
                                       const TSharedPtr<FAwHud> Hud = Weak.Pin();
                                       return Hud.IsValid() ? Hud->GetCaption() : FText::GetEmpty();
                                   }),
                                   Size(20), Style.TextPrimary, false, true)
                ]
            ]
        ]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(0.0f, AwFlCueTopMargin, 0.0f, 0.0f))
        [
            SNew(SBox)
            .MaxDesiredWidth(1000.0f)
            .Visibility_Lambda([Weak]()
            {
                const TSharedPtr<FAwHud> Hud = Weak.Pin();
                return Hud.IsValid() && !Hud->GetCue().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
            [
                SNew(SBorder)
                .BorderImage(&Style.BrushToast)
                .Padding(FMargin(20.0f, 8.0f))
                [
                    PWUI::MakeText(TAttribute<FText>::CreateLambda([Weak]()
                                   {
                                       const TSharedPtr<FAwHud> Hud = Weak.Pin();
                                       return Hud.IsValid() ? Hud->GetCue() : FText::GetEmpty();
                                   }),
                                   Size(18), Style.Warn, true, true)
                ]
            ]
        ];
    Content = Root;
    Holder = SNew(SWeakWidget).PossiblyNullContent(Root);
    Holder->SetVisibility(EVisibility::HitTestInvisible);
    GEngine->GameViewport->AddViewportWidgetContent(Holder.ToSharedRef(), AwFlHudZOrder);
    return true;
}

void FAwHud::Remove()
{
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Content.Reset();
}
} // namespace PWAwakening
