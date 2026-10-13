#include "UI/SPWSettingsPanel.h"

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "PerfectWorldPresentationSettings.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldSettingsPanel"

namespace
{
// Saved beside the engine's own settings so it travels with them. Nothing reads it yet.
const TCHAR* const SettingsSection = TEXT("PerfectWorld.Input");
const TCHAR* const SensitivityKey = TEXT("MouseSensitivity");
constexpr float SensitivityMin = 0.2f;
constexpr float SensitivityMax = 3.0f;
constexpr float SensitivityStep = 0.1f;
} // namespace

void SPWSettingsPanel::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    OnBack = InArgs._OnBack;

    float Stored = 1.0f;
    if (GConfig != nullptr && GConfig->GetFloat(SettingsSection, SensitivityKey, Stored, GGameUserSettingsIni))
    {
        MouseSensitivity = FMath::Clamp(Stored, SensitivityMin, SensitivityMax);
    }

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]()
        {
            const auto Parent = GetParentWidget();
            const float Width = Parent.IsValid() ? static_cast<float>(Parent->GetCachedGeometry().GetLocalSize().X) : 760.0f;
            return FMath::Clamp(Width - 48.0f, 1.0f, 760.0f);
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            const auto Parent = GetParentWidget();
            const float Height = Parent.IsValid() ? static_cast<float>(Parent->GetCachedGeometry().GetLocalSize().Y) : 720.0f;
            return FMath::Max(1.0f, Height - 32.0f);
        })
        [
            SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar)
            .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
            + SScrollBox::Slot()
            [
            PWUI::MakePanel(
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    PWUI::MakeText(LOCTEXT("Title", "Settings"), 36, Style.TextPrimary, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 24.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(LOCTEXT("DisplayLabel", "DISPLAY"), 13, Style.Accent, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .HAlign(HAlign_Left)
                    [
                        PWUI::MakeButton(
                            TAttribute<FText>::CreateLambda([]()
                            {
                                return IsFullscreen() ? LOCTEXT("Fullscreen", "Fullscreen (click for windowed)")
                                                      : LOCTEXT("Windowed", "Windowed (click for fullscreen)");
                            }),
                            PWUI::EButtonKind::Secondary,
                            FOnClicked::CreateLambda([this]()
                            {
                                ToggleFullscreen();
                                return FReply::Handled();
                            }),
                            true, 20, 360.0f)
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 28.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeButton(
                        TAttribute<FText>::CreateLambda([]()
                        {
                            return PWUI::IsReducedMotion() ? LOCTEXT("ReducedOn", "Reduced motion: On")
                                                           : LOCTEXT("ReducedOff", "Reduced motion: Off");
                        }), PWUI::EButtonKind::Chip,
                        FOnClicked::CreateLambda([]()
                        {
                            UPerfectWorldPresentationSettings::Get().SetReducedMotion(!PWUI::IsReducedMotion());
                            return FReply::Handled();
                        }), true, 20, 0.0f, TAttribute<bool>::CreateLambda([]() { return PWUI::IsReducedMotion(); }))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(LOCTEXT("ReducedNote", "Show motion instantly and keep decorative previews and the title camera still."),
                                   15, Style.TextSecondary, false, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 28.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(LOCTEXT("MouseLabel", "MOUSE SENSITIVITY"), 13, Style.Accent, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        PWUI::MakeButton(INVTEXT("-"), PWUI::EButtonKind::Secondary,
                                         FOnClicked::CreateLambda([this]()
                                         {
                                             SetSensitivity(MouseSensitivity - SensitivityStep, true);
                                             return FReply::Handled();
                                         }),
                                         true, 22, 40.0f)
                    ]
                    + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(14.0f, 0.0f)
                    [
                        SNew(SBox)
                        .HeightOverride(44.0f)
                        [
                            SNew(SSlider)
                            .Style(&Style.Slider)
                            .MinValue(SensitivityMin)
                            .MaxValue(SensitivityMax)
                            .StepSize(SensitivityStep)
                            .MouseUsesStep(true)
                            .IsFocusable(false)
                            .Value_Lambda([this]() { return MouseSensitivity; })
                            .OnValueChanged_Lambda([this](float Value) { SetSensitivity(Value, false); })
                            .OnMouseCaptureEnd(FSimpleDelegate::CreateLambda([this]() { SetSensitivity(MouseSensitivity, true); }))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        PWUI::MakeButton(INVTEXT("+"), PWUI::EButtonKind::Secondary,
                                         FOnClicked::CreateLambda([this]()
                                         {
                                             SetSensitivity(MouseSensitivity + SensitivityStep, true);
                                             return FReply::Handled();
                                         }),
                                         true, 22, 40.0f)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .WidthOverride(70.0f)
                        [
                            SNew(STextBlock)
                            .Text_Lambda([this]()
                            {
                                FNumberFormattingOptions Options;
                                Options.MinimumFractionalDigits = 1;
                                Options.MaximumFractionalDigits = 1;
                                return FText::AsNumber(MouseSensitivity, &Options);
                            })
                            .Font(PWUI::Font(22, true))
                            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
                [
                    PWUI::MakeText(LOCTEXT("MouseNote",
                        "Coming soon: this value is saved now, but the in-world camera does not read it yet."),
                        15, Style.TextMuted, false, true)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 30.0f, 0.0f, 0.0f)
                [
                    SNew(SBox)
                    .HAlign(HAlign_Left)
                    [
                        PWUI::MakeButton(LOCTEXT("Back", "Back"), PWUI::EButtonKind::Secondary,
                                         FOnClicked::CreateLambda([this]()
                                         {
                                             OnBack.ExecuteIfBound();
                                             return FReply::Handled();
                                         }),
                                         true, 20, 140.0f)
                    ]
                ],
                FMargin(36.0f, 30.0f))
            ]
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), AsShared());
}

bool SPWSettingsPanel::IsFullscreen()
{
    const UGameUserSettings* Settings = GEngine != nullptr ? GEngine->GetGameUserSettings() : nullptr;
    return Settings != nullptr && Settings->GetFullscreenMode() != EWindowMode::Windowed;
}

void SPWSettingsPanel::ToggleFullscreen()
{
    UGameUserSettings* Settings = GEngine != nullptr ? GEngine->GetGameUserSettings() : nullptr;
    if (Settings == nullptr)
    {
        return;
    }
    // Borderless fullscreen switches instantly and never changes the monitor's mode.
    Settings->SetFullscreenMode(IsFullscreen() ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
    Settings->ApplySettings(false);
}

void SPWSettingsPanel::SetSensitivity(float Value, bool bSave)
{
    MouseSensitivity = FMath::Clamp(FMath::RoundToFloat(Value / SensitivityStep) * SensitivityStep, SensitivityMin, SensitivityMax);
    if (bSave && GConfig != nullptr)
    {
        GConfig->SetFloat(SettingsSection, SensitivityKey, MouseSensitivity, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}

FReply SPWSettingsPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    // A held key auto-repeats; menus react to the first press only.
    if (InKeyEvent.IsRepeat())
    {
        return FReply::Handled();
    }
    if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        OnBack.ExecuteIfBound();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
