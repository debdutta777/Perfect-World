#include "UI/SPWTitleScreen.h"

#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTitleScreen"

void SPWTitleScreen::Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    OnContinue = InArgs._OnContinue;

    // Continue picks the newest occupied slot. Work it out once; the title screen is rebuilt each time it opens.
    ContinueSlot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    if (ContinueSlot >= 0 && InCreator != nullptr)
    {
        ContinueNote = FText::Format(LOCTEXT("ContinueNoteFmt", "Slot {0}: {1}"), FText::AsNumber(ContinueSlot + 1),
                                     InCreator->DescribeSaveSlot(ContinueSlot));
    }

    const FSimpleDelegate NewGame = InArgs._OnNewGame;
    const FSimpleDelegate Load = InArgs._OnLoad;
    const FSimpleDelegate Settings = InArgs._OnSettings;
    const FSimpleDelegate Quit = InArgs._OnQuit;

    const auto MenuButton = [](const FText& Label, PWUI::EButtonKind Kind, FOnClicked OnClicked,
                               TAttribute<bool> bEnabled = true) -> TSharedRef<SWidget>
    {
        return SNew(SBox)
            .WidthOverride(380.0f)
            [
                PWUI::MakeButton(Label, Kind, OnClicked, bEnabled, 24, 0.0f)
            ];
    };

    const TSharedRef<STextBlock> StatusText = PWUI::MakeText(
        TAttribute<FText>::CreateLambda([this]() { return Status; }), 16, Style.Danger, true, true);
    const TSharedRef<STextBlock> ContinueText = PWUI::MakeText(
        TAttribute<FText>::CreateLambda([this]() { return ContinueNote; }), 14, Style.TextMuted, false, true);

    ChildSlot
    [
        SNew(SBox)
        .HAlign(HAlign_Left)
        [
            SNew(SBox)
            .WidthOverride(820.0f)
            [
                SNew(SBorder)
                .BorderImage(&Style.BrushSideShade)
                .Padding(FMargin(110.0f, 0.0f, 40.0f, 0.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            // Shrinks (never grows) to the panel's width, so the full title reads at any resolution.
                            SNew(SScaleBox)
                            .Stretch(EStretch::ScaleToFitX)
                            .StretchDirection(EStretchDirection::DownOnly)
                            .HAlign(HAlign_Left)
                            [
                                PWUI::MakeText(LOCTEXT("TitleMain", "PERFECT WORLD"), 84, Style.TextPrimary, true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox)
                            .WidthOverride(140.0f)
                            .HeightOverride(4.0f)
                            [
                                SNew(SImage).Image(&Style.BrushAccentRule)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 14.0f, 0.0f, 0.0f)
                        [
                            PWUI::MakeText(LOCTEXT("TitleSub", "AWAKENING"), 44, Style.Accent, true)
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(6.0f, 12.0f, 0.0f, 54.0f)
                        [
                            PWUI::MakeText(LOCTEXT("TitlePlace", "Kolkata · 2 June 2042"), 22, Style.TextSecondary)
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            MenuButton(LOCTEXT("NewGame", "New Game"), PWUI::EButtonKind::Primary,
                                       FOnClicked::CreateLambda([NewGame]() { NewGame.ExecuteIfBound(); return FReply::Handled(); }))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            MenuButton(LOCTEXT("Continue", "Continue"), PWUI::EButtonKind::Secondary,
                                       FOnClicked::CreateLambda([this]()
                                       {
                                           if (OnContinue.IsBound())
                                           {
                                               Status = OnContinue.Execute();
                                           }
                                           return FReply::Handled();
                                       }),
                                       TAttribute<bool>::CreateLambda([this]() { return ContinueSlot >= 0; }))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            MenuButton(LOCTEXT("Load", "Load"), PWUI::EButtonKind::Secondary,
                                       FOnClicked::CreateLambda([Load]() { Load.ExecuteIfBound(); return FReply::Handled(); }))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            MenuButton(LOCTEXT("Settings", "Settings"), PWUI::EButtonKind::Secondary,
                                       FOnClicked::CreateLambda([Settings]() { Settings.ExecuteIfBound(); return FReply::Handled(); }))
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            MenuButton(LOCTEXT("Quit", "Quit"), PWUI::EButtonKind::Secondary,
                                       FOnClicked::CreateLambda([Quit]() { Quit.ExecuteIfBound(); return FReply::Handled(); }))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 16.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox).WidthOverride(380.0f)[ContinueText]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox).WidthOverride(380.0f)[StatusText]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f, 0.0f, 34.0f)
                    [
                        PWUI::MakeText(LOCTEXT("EarlyBuild", "Early build. Voxel block visuals stand in for final art."), 14, Style.TextMuted)
                    ]
                ]
            ]
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), AsShared());
}

#undef LOCTEXT_NAMESPACE
