#include "UI/SPWStudyDesk.h"

#include "GameClock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldA5"

namespace PWA5
{
bool InteractStudyDesk(const TSharedPtr<IStudyWorld>& World, const FSimpleDelegate& Open, FText& OutReason)
{
    OutReason = LOCTEXT("DeskUnavailable", "The study desk is unavailable right now.");
    if (!World.IsValid() || !Open.IsBound() || !World->CanUseDesk(OutReason)) return false;
    OutReason = FText::GetEmpty();
    Open.ExecuteIfBound();
    return true;
}

FPracticeOffer StudyOffer(const PerfectWorld::ActivityDef& Activity, const FText& Quote, bool bAvailable)
{
    return {Activity.Id, FText::Format(LOCTEXT("PracticeDuration", "{0} ({1} minutes)"),
        FText::FromString(FString(UTF8_TO_TCHAR(Activity.Name.c_str()))), FText::AsNumber(Activity.DurationMinutes)),
        Quote, bAvailable};
}

FText Deadline(const PerfectWorld::GameInstant At)
{
    const auto Date = PerfectWorld::GameClock::ToCalendar(At);
    return FText::FromString(FString::Printf(TEXT("%02d/%02d/%04d %02d:%02d"),
        Date.Day, Date.Month, Date.Year, Date.Hour, Date.Minute));
}

FText IssueText(const PerfectWorld::SchoolIssue& Issue)
{
    if (Issue.Code == "school.work.already_submitted") return LOCTEXT("AlreadySubmitted", "This work has already been submitted. Your academic record is unchanged.");
    if (Issue.Code == "school.work.busy") return LOCTEXT("Busy", "Submit or discard your open paper before beginning another one.");
    if (Issue.Code == "school.work.unavailable") return LOCTEXT("Unavailable", "This work is unavailable here or has not opened yet. Return to the desk when it is available.");
    if (Issue.Code == "school.work.clock_changed") return LOCTEXT("ClockChanged", "The world time changed while this paper was open. Return to the desk to refresh your record.");
    if (Issue.Code == "assessment.already_surveyed") return LOCTEXT("AlreadySurveyed", "The survey is already in your paper. Reading it again costs no units.");
    if (Issue.Code == "assessment.finished") return LOCTEXT("Finished", "All groups are complete. Submit the paper to record your academic marks.");
    if (Issue.Code == "assessment.allocation_short") return LOCTEXT("AllocationShort", "Allocate more units to this group before answering.");
    if (Issue.Code == "assessment.units_short") return LOCTEXT("UnitsShort", "There are not enough unallocated units. Release another allocation or choose a smaller one.");
    if (Issue.Code == "assessment.already_answered") return LOCTEXT("AlreadyAnswered", "This group has already been answered.");
    return LOCTEXT("Refused", "That action is unavailable. Your work has not changed. Refresh the record and review the remaining units.");
}

FText Diagnostics(const PerfectWorld::AssessmentGroupMark& Mark)
{
    using namespace PerfectWorld;
    FString Advice;
    if (Mark.Grade == AssessmentGrade::Unattempted) Advice = TEXT("Unfinished work: practise planning the order and time for each group.");
    if (Mark.Failures & AssessmentFailWrongModel) Advice += TEXT(" Revisit the concept and method for this problem.");
    if (Mark.Failures & AssessmentFailUnsupported) Advice += TEXT(" Support the conclusion with evidence or bound the claim.");
    if (Mark.Failures & AssessmentFailWastedTime) Advice += TEXT(" Review which steps used time without improving the answer.");
    if (Mark.Failures & AssessmentFailUnclear) Advice += TEXT(" Make the explanation and its limits explicit.");
    if (Advice.IsEmpty()) return LOCTEXT("Sound", "Sound method and supported answer. Try a related problem to consolidate it.");
    return FText::FromString(Advice.TrimStartAndEnd());
}
}

void SPWStudyDesk::Construct(const FArguments& InArgs)
{
    World = InArgs._World;
    Space = InArgs._Space;
    OnOpenPaper = InArgs._OnOpenPaper;
    OnClose = InArgs._OnClose;
    const auto Back = PWUI::MakeButton(LOCTEXT("Back", "Back"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]() { OnClose.ExecuteIfBound(); return FReply::Handled(); }));
    BackButton = Back;
    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]() { return FOptionalSize(FMath::Clamp(Space.Get().X - 32.0, 280.0, 760.0)); })
        .MaxDesiredHeight_Lambda([this]() { return FOptionalSize(FMath::Max(120.0, Space.Get().Y - 32.0)); })
        [
            PWUI::MakeBracketPanel(SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(LOCTEXT("DeskTitle", "Study desk"), PWUI::ETextRole::PageHeading, PWUI::FStyle::Get().Accent, true)]
                + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(LOCTEXT("AcademicOnly", "Academic marks are separate from tower rank. Reading and planning cost no time."), 18, PWUI::FStyle::Get().TextSecondary, false, true)]
                + SVerticalBox::Slot().FillHeight(1)[SAssignNew(Body, SBox)]
                + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this]() { return Notice; }), 18, PWUI::FStyle::Get().TextSecondary, false, true)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[Back], FMargin(16))
        ]
    ];
    Refresh();
    PWUI::FocusWhenReady(AsShared(), Back);
}

void SPWStudyDesk::Refresh()
{
    Controls.Reset();
    if (Body.IsValid())
    {
        Body->SetContent(BuildBody());
        if (!FocusKey.IsEmpty())
        {
            const auto* Target = Controls.Find(FocusKey);
            PWUI::FocusWhenReady(AsShared(), Target != nullptr ? *Target : BackButton);
        }
    }
}

TSharedRef<SWidget> SPWStudyDesk::BuildBody()
{
    const auto& Style = PWUI::FStyle::Get();
    const auto Rows = SNew(SVerticalBox);
    PerfectWorld::SchoolAcademicRecordView Record;
    FText Why;
    if (!World.IsValid() || !World->ReadRecord(Record, Why))
    {
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(Why.IsEmpty() ? LOCTEXT("NoRecord", "Your academic record is unavailable.") : Why, 20, Style.TextSecondary, false, true)];
    }
    else
    {
        if (Record.Work.empty()) Rows->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("NoWork", "No work has been issued. You can still practise."), 20, Style.TextSecondary, false, true)];
        for (const auto& Work : Record.Work)
        {
            const std::string Id = Work.Brief.Id;
            const auto Detail = SNew(SVerticalBox);
            Detail->AddSlot().AutoHeight()[PWUI::MakeText(Work.Brief.Kind == PerfectWorld::SchoolWorkKind::Homework
                ? LOCTEXT("HomeworkKind", "Homework") : LOCTEXT("AssessmentKind", "Academic assessment"), 18, Style.Accent, true, true)];
            Detail->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.TitleTextKey, LOCTEXT("WorkTitle", "School work")), 22, Style.TextPrimary, true, true)];
            Detail->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.BriefTextKey, LOCTEXT("BriefFallback", "Read the paper and choose how to use its eighteen units.")), 20, Style.TextPrimary, false, true)];
            Detail->AddSlot().AutoHeight()[PWUI::MakeText(FText::Format(LOCTEXT("Due", "Due: {0}  |  Available: {1}"), PWA5::Deadline(Work.Brief.DueAt), PWA5::Deadline(Work.Brief.AvailableAt)), 18, Style.TextSecondary, false, true)];
            Detail->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.CriteriaTextKey, LOCTEXT("CriteriaFallback", "Five groups carry twenty academic marks each. Feedback explains the method and evidence.")), 18, Style.TextSecondary, false, true)];
            if (Work.Overdue || Work.LateSubmission)
            {
                Detail->AddSlot().AutoHeight()[PWUI::MakeText(Work.LateSubmission ? LOCTEXT("Late", "Submitted late. Diagnostic marks are unchanged by lateness.") : LOCTEXT("Missed", "Deadline missed. You can still complete this work."), 20, Style.Warn, false, true)];
                Detail->AddSlot().AutoHeight()[PWUI::MakeText(World->Text(Work.Brief.RecoveryTextKey, LOCTEXT("RecoveryFallback", "Resume the work when you are ready, or practise a method first.")), 18, Style.TextSecondary, false, true)];
            }
            const FText Label = Work.Marking ? LOCTEXT("ReadFeedback", "Read academic feedback") : Work.CanBegin ? LOCTEXT("OpenBrief", "Open brief") : Work.CanSubmit ? LOCTEXT("ResumePaper", "Resume paper") : LOCTEXT("NotAvailable", "Currently unavailable");
            const bool bCanOpen = OnOpenPaper.IsBound() && (Work.Marking.has_value() || Work.CanBegin || Work.CanSubmit);
            const FString Key = TEXT("work.") + FString(UTF8_TO_TCHAR(Id.c_str()));
            const auto OpenButton = PWUI::MakeButton(Label, PWUI::EButtonKind::Secondary,
                FOnClicked::CreateSPLambda(this, [this, Id, Key]() { FocusKey = Key; OnOpenPaper.ExecuteIfBound(Id); return FReply::Handled(); }), bCanOpen);
            if (bCanOpen) Controls.Add(Key, OpenButton);
            Detail->AddSlot().AutoHeight()[OpenButton];
            Rows->AddSlot().AutoHeight().Padding(0, 10)[PWUI::MakePanel(Detail, FMargin(12))];
        }
        Rows->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("Practice", "Method practice"), 22, Style.Accent, true, true)];
        for (const auto& Offer : World->PracticeOffers())
        {
            const std::string Id = Offer.Id;
            const FString Key = TEXT("practice.") + FString(UTF8_TO_TCHAR(Id.c_str()));
            Rows->AddSlot().AutoHeight()[PWUI::MakeText(Offer.Detail, 18, Style.TextSecondary, false, true)];
            const auto PracticeButton = PWUI::MakeButton(Offer.Label, PWUI::EButtonKind::Secondary,
                FOnClicked::CreateSPLambda(this, [this, Id, Key]()
                {
                    FocusKey = Key;
                    const auto Result = World->StartPractice(Id);
                    Notice = Result.Ok() ? LOCTEXT("PracticeOpened", "Practice opened.") : PWA5::IssueText(Result);
                    Refresh();
                    return FReply::Handled();
                }), Offer.bAvailable);
            if (Offer.bAvailable) Controls.Add(Key, PracticeButton);
            Rows->AddSlot().AutoHeight().Padding(0, 4)[PracticeButton];
        }
    }
    return SNew(SScrollBox).ScrollBarStyle(&Style.ScrollBar).AllowOverscroll(EAllowOverscroll::No)
        + SScrollBox::Slot()[Rows];
}

FReply SPWStudyDesk::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
    {
        if (!Event.IsRepeat()) OnClose.ExecuteIfBound();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

#undef LOCTEXT_NAMESPACE
