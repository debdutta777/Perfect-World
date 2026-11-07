#include "UI/SPWAcademicAssessment.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldA5Assessment"

void SPWAcademicAssessment::Construct(const FArguments& InArgs)
{
    World = InArgs._World;
    WorkId = InArgs._WorkId;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    const auto Back = PWUI::MakeButton(LOCTEXT("Back", "Back to desk"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); }));
    BackButton = Back;
    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]() { return FOptionalSize(FMath::Clamp(Space.Get().X - 32.0, 280.0, 800.0)); })
        .MaxDesiredHeight_Lambda([this]() { return FOptionalSize(FMath::Max(120.0, Space.Get().Y - 32.0)); })
        [PWUI::MakeBracketPanel(SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(LOCTEXT("Title", "Academic assessment"), PWUI::ETextRole::PageHeading, PWUI::FStyle::Get().Accent, true)]
            + SVerticalBox::Slot().FillHeight(1)[SAssignNew(Body, SBox)]
            + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this]() { return Notice; }), 18, PWUI::FStyle::Get().TextSecondary, false, true)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[Back], FMargin(16))]
    ];
    Refresh();
    PWUI::FocusWhenReady(AsShared(), Back);
}

void SPWAcademicAssessment::Refresh()
{
    Controls.Reset();
    if (Body.IsValid())
    {
        Body->SetContent(BuildBody());
        const auto* Target = Controls.Find(FocusKey);
        if (Target != nullptr) PWUI::FocusWhenReady(AsShared(), *Target);
        else if (!FocusKey.IsEmpty()) PWUI::FocusWhenReady(AsShared(), BackButton);
    }
}

PerfectWorld::SchoolWorkResult SPWAcademicAssessment::Send(PerfectWorld::SchoolWorkCommand Command)
{
    Command.WorkId = WorkId;
    PerfectWorld::SchoolWorkResult Result;
    if (World.IsValid()) Result = World->Apply(Command);
    else Result.Issue.Code = "school.work.unavailable";
    Notice = Result.Ok() ? FText::Format(LOCTEXT("Accepted", "Recorded. Paper units spent: {0}. World minutes charged: {1}."),
        FText::AsNumber(Result.ChargedUnits), FText::AsNumber(Result.WorldMinutes)) : PWA5::IssueText(Result.Issue);
    Refresh();
    return Result;
}

TSharedRef<SWidget> SPWAcademicAssessment::Action(const FText& Label, PerfectWorld::SchoolWorkCommand Command, bool bEnabled)
{
    const FString Key = Label.ToString();
    const auto Button = PWUI::MakeButton(Label, PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this, Command, Key]() { FocusKey = Key; Send(Command); return FReply::Handled(); }), bEnabled);
    if (bEnabled) Controls.Add(Key, Button);
    return Button;
}

TSharedRef<SWidget> SPWAcademicAssessment::BuildBody()
{
    const auto& Style = PWUI::FStyle::Get();
    PerfectWorld::SchoolAcademicRecordView Record;
    FText Why;
    if (!World.IsValid() || !World->ReadRecord(Record, Why))
        return PWUI::MakeText(Why.IsEmpty() ? LOCTEXT("Unavailable", "Your academic record is unavailable.") : Why, 20, Style.TextSecondary, false, true);
    for (const auto& Work : Record.Work)
    {
        if (Work.Brief.Id != WorkId) continue;
        const auto Rows = SNew(SVerticalBox);
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.TitleTextKey, LOCTEXT("Work", "School work")), 22, Style.TextPrimary, true, true)];
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.BriefTextKey, LOCTEXT("Brief", "Choose the order, allocation and ambition for each question group.")), 20, Style.TextPrimary, false, true)];
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.CriteriaTextKey, LOCTEXT("Criteria", "Five groups, twenty academic marks each. Marks describe your work; they are separate from tower rank.")), 18, Style.TextSecondary, false, true)];
        if (Work.Overdue || Work.LateSubmission)
            Rows->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.RecoveryTextKey,
                LOCTEXT("Recovery", "The deadline has passed. Recovery remains available; lateness does not reduce diagnostic marks.")), 18, Style.Warn, false, true)];
        if (Work.Marking) Rows->AddSlot().AutoHeight()[BuildFeedback(*Work.Marking)];
        else if (Work.CanSubmit) Rows->AddSlot().AutoHeight()[BuildPaper(Work)];
        else
        {
            Rows->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("Cost", "This paper reserves 90 minutes: eighteen five-minute units. Submit charges the block once. Cancel discards the draft and charges units already spent. Back keeps the draft open."), 20, Style.TextPrimary, false, true)];
            PerfectWorld::SchoolWorkCommand Begin;
            Begin.Kind = PerfectWorld::SchoolWorkActionKind::Begin;
            Rows->AddSlot().AutoHeight()[Action(LOCTEXT("Begin", "Begin paper (90-minute block)"), Begin, Work.CanBegin)];
        }
        return SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar).AllowOverscroll(EAllowOverscroll::No)
            + SScrollBox::Slot()[Rows];
    }
    return PWUI::MakeText(LOCTEXT("Unknown", "This work is no longer available. Return to the desk to refresh your record."), 20, Style.TextSecondary, false, true);
}

TSharedRef<SWidget> SPWAcademicAssessment::BuildPaper(const PerfectWorld::SchoolWorkView& Work)
{
    using namespace PerfectWorld;
    const auto& Style = PWUI::FStyle::Get();
    const auto Rows = SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight()[PWUI::MakeText(FText::Format(LOCTEXT("Units", "Units left: {0} / 18. Unallocated: {1}."),
        FText::AsNumber(Work.Paper.UnitsLeft), FText::AsNumber(AssessmentUnallocated(Work.Paper))), 20, Style.Accent, true, true)];
    const auto SurveyQuote = QuoteAssessmentSurvey(Work.Paper);
    SchoolWorkCommand Command;
    Command.Kind = SchoolWorkActionKind::Survey;
    Rows->AddSlot().AutoHeight()[Action(Work.Paper.Surveyed ? LOCTEXT("SurveyDone", "Paper surveyed") :
        FText::Format(LOCTEXT("Survey", "Survey paper ({0} unit)"), FText::AsNumber(SurveyQuote.Units)), Command, SurveyQuote.Ok())];
    for (const auto& Line : Work.Survey)
    {
        FText Familiarity = LOCTEXT("Unfamiliar", "Unfamiliar");
        if (Line.Familiarity == AssessmentFamiliarity::Practised) Familiarity = LOCTEXT("Practised", "Practised");
        if (Line.Familiarity == AssessmentFamiliarity::Shaky) Familiarity = LOCTEXT("Shaky", "Shaky: estimate uncertain");
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(FText::Format(LOCTEXT("SurveyLine", "Group {0}: {1} marks. {2}. Estimated work: {3} to {4} units."),
            FText::AsNumber(static_cast<int32>(Line.Group + 1)), FText::AsNumber(Line.Marks), Familiarity,
            FText::AsNumber(Line.EstimateLow), FText::AsNumber(Line.EstimateHigh)), 18, Style.TextSecondary, false, true)];
    }
    // Core vector sizes, never TArray::Num. A stale selection cannot index a smaller paper.
    if (Draft.Group >= Work.Paper.Groups.size()) Draft.Group = 0;
    for (std::size_t Index = 0; Index < Work.Paper.Groups.size(); ++Index)
    {
        const auto& Group = Work.Paper.Groups[Index];
        const FString FocusId = FString::Printf(TEXT("group.%d"), static_cast<int32>(Index));
        const auto GroupButton = PWUI::MakeButton(FText::Format(LOCTEXT("Group", "Group {0}: {1}; allocated {2}"),
            FText::AsNumber(static_cast<int32>(Index + 1)), Group.Answered ? LOCTEXT("Answered", "answered") : LOCTEXT("Open", "open"), FText::AsNumber(Group.Allocated)),
            PWUI::EButtonKind::Chip, FOnClicked::CreateSPLambda(this, [this, Index, FocusId]()
            { FocusKey = FocusId; Draft.Group = Index; Refresh(); return FReply::Handled(); }), true, 20, 0, Draft.Group == Index);
        Controls.Add(FocusId, GroupButton);
        Rows->AddSlot().AutoHeight().Padding(0, 4)[GroupButton];
    }
    if (Draft.Group < Work.Paper.Groups.size())
    {
        const auto& Group = Work.Paper.Groups[Draft.Group];
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(World->GroupPrompt(WorkId, Draft.Group), 20, Style.TextPrimary, false, true)];
        Command.Kind = SchoolWorkActionKind::Allocate;
        Command.Group = Draft.Group;
        Command.Units = Group.Allocated - 1;
        Rows->AddSlot().AutoHeight()[Action(LOCTEXT("Less", "Allocate one fewer unit"), Command, !Group.Answered && Group.Allocated > 0 && !Work.Paper.Finished)];
        Command.Units = Group.Allocated + 1;
        Rows->AddSlot().AutoHeight()[Action(LOCTEXT("More", "Allocate one more unit"), Command, !Group.Answered && AssessmentUnallocated(Work.Paper) > 0 && !Work.Paper.Finished)];
        const auto Toggle = [this, &Rows](const FString& Key, const FText& Label, TFunction<void()> Change)
        {
            const auto Button = PWUI::MakeButton(Label, PWUI::EButtonKind::Chip,
                FOnClicked::CreateSPLambda(this, [this, Change, Key]() { FocusKey = Key; Change(); Refresh(); return FReply::Handled(); }));
            Controls.Add(Key, Button);
            Rows->AddSlot().AutoHeight()[Button];
        };
        Toggle(TEXT("mode"), Draft.Mode == AssessmentMode::Work ? LOCTEXT("WorkMode", "Method: work through") : LOCTEXT("GuessMode", "Method: guess"),
            [this]() { Draft.Mode = Draft.Mode == AssessmentMode::Work ? AssessmentMode::Guess : AssessmentMode::Work; });
        Toggle(TEXT("confirm"), Draft.Confirm ? LOCTEXT("ConfirmOn", "Verification: yes") : LOCTEXT("ConfirmOff", "Verification: no"), [this]() { Draft.Confirm = !Draft.Confirm; });
        Toggle(TEXT("hedge"), Draft.Hedge ? LOCTEXT("HedgeOn", "Bound the claim: yes") : LOCTEXT("HedgeOff", "Bound the claim: no"), [this]() { Draft.Hedge = !Draft.Hedge; });
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("Choices", "Work uses the method; guessing may leave it unsound. Verification re-examines the answer for one unit. Bounding limits the claim, but can waste an otherwise sound answer. Allocate time before committing."), 18, Style.TextSecondary, false, true)];
        const auto Quote = World->Quote(WorkId, Draft);
        Command.Kind = SchoolWorkActionKind::Answer;
        Command.Answer = Draft;
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(FText::Format(LOCTEXT("Quote", "Answer cost: {0} units; units after: {1}."), FText::AsNumber(Quote.Units), FText::AsNumber(Quote.UnitsAfter)), 20, Style.Accent, true, true)];
        if (!Quote.Ok()) Rows->AddSlot().AutoHeight()[PWUI::MakeText(PWA5::IssueText({std::string(AssessmentErrorCode(Quote.Error)), {}}), 18, Style.Warn, false, true)];
        Rows->AddSlot().AutoHeight()[Action(LOCTEXT("Answer", "Commit this group answer"), Command, Quote.Ok())];
    }
    Rows->AddSlot().AutoHeight().Padding(0, 8)[PWUI::MakeText(LOCTEXT("ExitCost", "Submit charges 90 world minutes once, including unanswered groups. Cancel charges spent units only and discards this draft. Back keeps the paper open; other activities remain unavailable until you submit or cancel."), 18, Style.TextSecondary, false, true)];
    Command = {};
    Command.Kind = SchoolWorkActionKind::Submit;
    Rows->AddSlot().AutoHeight()[Action(LOCTEXT("Submit", "Submit paper (90 world minutes)"), Command, Work.CanSubmit)];
    Command.Kind = SchoolWorkActionKind::Cancel;
    Rows->AddSlot().AutoHeight()[Action(FText::Format(LOCTEXT("Cancel", "Discard draft ({0} world minutes)"),
        FText::AsNumber((AssessmentPaperUnits - Work.Paper.UnitsLeft) * AssessmentUnitMinutes)), Command, Work.CanSubmit)];
    return Rows;
}

TSharedRef<SWidget> SPWAcademicAssessment::BuildFeedback(const PerfectWorld::AssessmentMarking& Marking)
{
    const auto& Style = PWUI::FStyle::Get();
    const auto Rows = SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight()[PWUI::MakeText(FText::Format(LOCTEXT("Marks", "Academic marks: {0} / {1}. Separate from tower rank."),
        FText::AsNumber(Marking.Total), FText::AsNumber(Marking.MaxTotal)), 22, Style.Accent, true, true)];
    for (const auto& Group : Marking.Groups)
    {
        Rows->AddSlot().AutoHeight().Padding(0, 8)[PWUI::MakeText(FText::Format(LOCTEXT("GroupMark", "Group {0}: {1} / {2}. {3}"),
            FText::AsNumber(static_cast<int32>(Group.Group + 1)), FText::AsNumber(Group.Awarded), FText::AsNumber(Group.Marks), PWA5::Diagnostics(Group)),
            20, Style.TextPrimary, false, true)];
    }
    return Rows;
}

FReply SPWAcademicAssessment::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        if (!Event.IsRepeat()) OnClose.ExecuteIfBound();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
