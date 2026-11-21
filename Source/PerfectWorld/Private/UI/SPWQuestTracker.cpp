#include "UI/SPWQuestTracker.h"

#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SPWQuestTracker::Construct(const FArguments& InArgs)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    WorldContext = InArgs._WorldContext;
    ShowTracker = InArgs._ShowTracker;
    const TAttribute<float> TopOffset = InArgs._TopOffset;

    // Nothing here is clickable or focusable: the line can never take input from the world behind it. The root stays shown so that it
    // keeps ticking; only the card inside is collapsed when there is nothing to say.
    SetVisibility(EVisibility::HitTestInvisible);

    ChildSlot
    [
        SNew(SBox)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Top)
        .Padding_Lambda([TopOffset]() { return FMargin(0.0f, TopOffset.Get(76.0f), PWUI::Spacing::Large, 0.0f); })
        [
            SNew(SBox)
            .MaxDesiredWidth(320.0f)
            .Visibility_Lambda([this]()
            {
                return ShowTracker.Get(false) && Line.bVisible ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
            })
            [
                SNew(SBorder)
                .Tag(FName(PWQuest::TrackerTag))
                .BorderImage(&Style.BrushToast)
                .Padding(FMargin(PWUI::Spacing::Compact, PWUI::Spacing::XSmall + 2.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        PWUI::MakeText(TAttribute<FText>::CreateSP(this, &SPWQuestTracker::GetTitleText), 15, Style.TextPrimary, true, true)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                    [
                        PWUI::MakeText(TAttribute<FText>::CreateSP(this, &SPWQuestTracker::GetWhenText), PWUI::ETextRole::Meta, Style.TextSecondary, true)
                    ]
                ]
            ]
        ]
    ];
}

FText SPWQuestTracker::GetTitleText() const
{
    return Line.Title;
}

FText SPWQuestTracker::GetWhenText() const
{
    return Line.When;
}

void SPWQuestTracker::Refresh()
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(WorldContext.Get());
    if (Life == nullptr)
    {
        Line = FPWQuestTrackedLine();
        bRead = false;
        return;
    }
    const int64 Minute = Life->GetNow();
    const uint32 Revision = PWQuest::TrackedRevision();
    if (bRead && Minute == ReadAtMinute && Revision == ReadAtRevision)
    {
        return;
    }
    bRead = true;
    ReadAtMinute = Minute;
    ReadAtRevision = Revision;
    Line = PWQuest::ReadTrackedLine(WorldContext.Get());
}

void SPWQuestTracker::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    Refresh();
}
