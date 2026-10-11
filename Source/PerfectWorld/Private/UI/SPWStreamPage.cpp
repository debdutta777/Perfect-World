#include "UI/SPWStreamPage.h"

#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldStreamPage"

void SPWStreamPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();

    const TSharedRef<SHorizontalBox> Cards = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < Model->Streams.Num(); ++Index)
    {
        Cards->AddSlot().FillWidth(1.0f).Padding(8.0f, 0.0f)[BuildCard(Index)];
    }

    ReasonBox = SNew(SEditableTextBox)
        .Style(&Style.TextBox)
        .Text(FText::FromString(Model->SchoolReason))
        .HintText(LOCTEXT("ReasonHint", "Optional: why this stream?"))
        .MaximumLength(60)
        .OnTextChanged_Lambda([this](const FText& NewText)
        {
            const FString Reason = NewText.ToString();
            const FName StreamId = SelectedStreamId();
            Model->Edit([StreamId, &Reason](UPerfectWorldCharacterCreatorSubsystem& Creator)
            {
                return Creator.SetSchoolStream(StreamId, Reason);
            });
        });

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("Title", "Which stream will you study?"), 32, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 22.0f)
        [
            PWUI::MakeText(LOCTEXT("Subtitle",
                "A stream is a timetable, not a destiny. It sets your first timetable, the course dialogue you hear and the "
                "school activities open to you. Every stream starts with the same Academics XP, costs no points and changes "
                "no stat. A Humanities student can still become an engineer by finishing the prerequisites later."),
                16, Style.TextSecondary, false, true)
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            Cards
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 22.0f, 8.0f, 0.0f)
        [
            SNew(SBorder)
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(22.0f, 16.0f))
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 8.0f)
                [
                    PWUI::MakeText(LOCTEXT("ReasonLabel", "YOUR REASON"), 13, Style.Accent, true)
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    ReasonBox.ToSharedRef()
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SPWStreamPage::BuildCard(int32 Index)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWStreamView& Stream = Model->Streams[Index];
    const FName StreamId = Stream.Id;

    const TSharedRef<SWidget> Content =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(Stream.DisplayName, 30, Style.TextPrimary, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Stream.TimetableSummary, 18, Style.TextSecondary, false, true)
        ];

    return PWUI::MakeContentButton(
        Content, PWUI::EButtonKind::Card,
        FOnClicked::CreateLambda([this, StreamId]()
        {
            const FString Reason = Model->SchoolReason;
            Model->Edit([StreamId, &Reason](UPerfectWorldCharacterCreatorSubsystem& Creator)
            {
                return Creator.SetSchoolStream(StreamId, Reason);
            });
            return FReply::Handled();
        }),
        TAttribute<bool>::CreateLambda([this, Index]()
        {
            return Model->Streams.IsValidIndex(Index) && Model->Streams[Index].bSelected;
        }),
        true, FMargin(26.0f, 24.0f));
}

FName SPWStreamPage::SelectedStreamId() const
{
    for (const FPWStreamView& Stream : Model->Streams)
    {
        if (Stream.bSelected)
        {
            return Stream.Id;
        }
    }
    return Model->Streams.Num() > 0 ? Model->Streams[0].Id : NAME_None;
}

#undef LOCTEXT_NAMESPACE
