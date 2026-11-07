#include "UI/SPWClassCoordinate.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>
#include <variant>

// The StructureCoordinate lesson (order A3) on the shared shell (SPWClassLesson.h). Nothing here decides anything:
// prices are the core's quotes, the group arithmetic shown is plain addition of numbers the case publishes, and the facts shown are
// only those the player has actually learned.

namespace
{
const PerfectWorld::CoordinateGoal ClCGoals[] = {PerfectWorld::CoordinateGoal::OrganiseWork, PerfectWorld::CoordinateGoal::HearEveryone,
                                                 PerfectWorld::CoordinateGoal::SettleConstraint, PerfectWorld::CoordinateGoal::SubmitCleanly};
const TCHAR* const ClCGoalKeys[] = {TEXT("organise_work"), TEXT("hear_everyone"), TEXT("settle_constraint"), TEXT("submit_cleanly")};
const PerfectWorld::CoordinateResolution ClCResolutions[] = {
    PerfectWorld::CoordinateResolution::Leave, PerfectWorld::CoordinateResolution::Rebalance, PerfectWorld::CoordinateResolution::Reschedule,
    PerfectWorld::CoordinateResolution::TrimScope, PerfectWorld::CoordinateResolution::AskExtension};
const TCHAR* const ClCResolutionKeys[] = {TEXT("leave"), TEXT("rebalance"), TEXT("reschedule"), TEXT("trim_scope"), TEXT("ask_extension")};

FString ClCWord(const std::string_view Name)
{
    return FString(static_cast<int32>(Name.size()), Name.data());
}

bool ClCHas(const std::vector<std::string>& Ids, const std::string& Id)
{
    return std::find(Ids.begin(), Ids.end(), Id) != Ids.end();
}

FText ClCMemberName(const PerfectWorld::CoordinateCaseDef& Def, const std::string& Id)
{
    for (const PerfectWorld::CoordinateMember& Member : Def.Members)
    {
        if (Member.Id != Id) continue;
        const FText Name = PWClassLesson::FromUtf8(Member.Name);
        return Member.Self ? FText::Format(PWClassLesson::Text(TEXT("phase2.a3.coordinate.member.you")), Name) : Name;
    }
    return FText::GetEmpty();
}
} // namespace

namespace PWClassCoordinate
{
FModel BuildModel(const PWClassLesson::IBridge& Bridge, const PWClassLesson::FFrame& Frame, const FDraft& Draft)
{
    using namespace PerfectWorld;
    FModel Out;
    const CoordinateCatalogue* Catalogue = Bridge.GetCatalogues().Structures.Coordinate;
    const CoordinateExposure& Exposure = Frame.View.Coordinate.Exposure;
    if (Catalogue == nullptr || !Frame.bHasRun || Frame.bOtherLesson || Frame.bReadyToClose || !Exposure.Active())
    {
        return Out;
    }
    const CoordinateCaseDef* Def = Catalogue->FindCase(Exposure.CaseId);
    if (Def == nullptr)
    {
        return Out;
    }
    Out.bActive = true;
    Out.CaseId = PWClassLesson::ToFString(Def->Id);
    Out.CaseName = PWClassLesson::FromUtf8(Def->Name);
    Out.CaseBrief = PWClassLesson::FromUtf8(Def->Brief);
    Out.UnitsLeft = Exposure.UnitsLeft;
    Out.UnitsTotal = Exposure.UnitsTotal;
    Out.Beat = Exposure.Beat;
    Out.Role = PWClassLesson::Text(TEXT("phase2.a3.coordinate.role.") + ClCWord(CoordinateRoleNames[static_cast<size_t>(Def->Role)]));
    Out.Scarce = PWClassLesson::Text(TEXT("phase2.a3.coordinate.scarce.") + ClCWord(CoordinateScarceNames[static_cast<size_t>(Def->Scarce)]));
    Out.Goal = PWClassLesson::Text(TEXT("phase2.a3.coordinate.goal.") + ClCWord(CoordinateGoalNames[static_cast<size_t>(Exposure.Goal)]));
    FText MethodName = PWClassLesson::FromUtf8(Def->MethodId);
    if (const auto* Family = Catalogue->FindFamily(Def->FamilyId))
    {
        for (const StructureMethod& Method : Family->Methods)
        {
            if (Method.Id == Def->MethodId) MethodName = PWClassLesson::FromUtf8(Method.Name);
        }
    }
    Out.Method = FText::Format(PWClassLesson::Text(Exposure.MethodKnown ? TEXT("phase2.a3.common.method.known") : TEXT("phase2.a3.common.method.unfamiliar")),
                               MethodName);

    const bool bAssigning = Exposure.Beat == CoordinateBeat::Assign;
    const auto AssigneeOf = [&](const std::string& TaskId) -> FString
    {
        if (!bAssigning)
        {
            for (const CoordinateAssignment& Pair : Exposure.Assignment)
            {
                if (Pair.Task == TaskId) return PWClassLesson::ToFString(Pair.Member);
            }
            return FString();
        }
        const FString* Found = Draft.Find(PWClassLesson::ToFString(TaskId));
        return Found != nullptr ? *Found : FString();
    };

    TMap<FString, int32> Loads;
    Out.bDraftComplete = !Def->Tasks.empty();
    CoordinateAction FullAssignment;
    FullAssignment.Kind = CoordinateActionKind::AssignWork;
    for (const CoordinateTask& Task : Def->Tasks)
    {
        FTaskRow Row;
        Row.Id = PWClassLesson::ToFString(Task.Id);
        Row.Name = PWClassLesson::FromUtf8(Task.Name);
        Row.Load = Task.Load;
        TArray<FText> Suits;
        for (const std::string& MemberId : Task.Fits) Suits.Add(ClCMemberName(*Def, MemberId));
        Row.Suits = FText::Join(INVTEXT(", "), Suits);
        Row.Assignee = AssigneeOf(Task.Id);
        const std::string AssigneeId(TCHAR_TO_UTF8(*Row.Assignee));
        const bool bKnownMember = std::any_of(Def->Members.begin(), Def->Members.end(), [&AssigneeId](const CoordinateMember& M) { return M.Id == AssigneeId; });
        if (!Row.Assignee.IsEmpty() && bKnownMember)
        {
            Row.AssigneeName = ClCMemberName(*Def, AssigneeId);
            Row.bSuited = ClCHas(Task.Fits, AssigneeId);
            Loads.FindOrAdd(Row.Assignee) += Task.Load;
            FullAssignment.Assignment.push_back({Task.Id, AssigneeId});
        }
        else
        {
            Row.Assignee.Reset();
            Out.bDraftComplete = false;
        }
        Out.Tasks.Add(MoveTemp(Row));
    }

    const CoordinateAction NoAssignment = [] { CoordinateAction A; A.Kind = CoordinateActionKind::AssignWork; return A; }();
    if (bAssigning)
    {
        // The price is shown before anything is chosen; with a complete draft the core also judges the draft itself.
        const SchoolLessonQuote Price = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(NoAssignment));
        Out.AssignUnits = Price.Units;
        Out.AssignIssue = Price.Issue;
        if (Out.bDraftComplete && Price.Issue.Ok())
        {
            Out.AssignIssue = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(FullAssignment)).Issue;
        }
    }

    for (const CoordinateMember& Member : Def->Members)
    {
        FMemberRow Row;
        Row.Id = PWClassLesson::ToFString(Member.Id);
        Row.Name = ClCMemberName(*Def, Member.Id);
        Row.Capacity = Member.Capacity;
        Row.bSelf = Member.Self;
        Row.bHeard = ClCHas(Exposure.Heard, Member.Id);
        Row.DraftLoad = Loads.FindRef(Row.Id);
        Row.bOver = Row.DraftLoad > Member.Capacity;
        if (Exposure.Beat == CoordinateBeat::Exchange && !Member.Self)
        {
            CoordinateAction Talk;
            Talk.Kind = CoordinateActionKind::Exchange;
            Talk.Member = Member.Id;
            const SchoolLessonQuote Quote = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Talk));
            Row.TalkUnits = Quote.Units;
            Row.TalkIssue = Quote.Issue;
        }
        Out.Members.Add(MoveTemp(Row));
    }

    if (Exposure.Beat == CoordinateBeat::Exchange)
    {
        CoordinateAction End;
        End.Kind = CoordinateActionKind::EndExchange;
        Out.EndExchangeIssue = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(End)).Issue;
    }

    if (Exposure.Beat == CoordinateBeat::Settle)
    {
        for (size_t Index = 0; Index < UE_ARRAY_COUNT(ClCResolutions); ++Index)
        {
            FResolutionRow Row;
            Row.Resolution = ClCResolutions[Index];
            Row.Name = PWClassLesson::Text(TEXT("phase2.a3.coordinate.resolution.") + FString(ClCResolutionKeys[Index]));
            Row.Hint = PWClassLesson::Text(TEXT("phase2.a3.coordinate.resolution.") + FString(ClCResolutionKeys[Index]) + TEXT(".hint"));
            CoordinateAction Settle;
            Settle.Kind = CoordinateActionKind::Settle;
            Settle.Resolution = Row.Resolution;
            const SchoolLessonQuote Quote = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Settle));
            Row.Units = Quote.Units;
            Row.Issue = Quote.Issue;
            Out.Resolutions.Add(MoveTemp(Row));
        }
    }

    if (Exposure.Beat == CoordinateBeat::Submit)
    {
        CoordinateAction Submit;
        Submit.Kind = CoordinateActionKind::Submit;
        Submit.Check = true;
        SchoolLessonQuote Quote = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Submit));
        Out.CheckUnits = Quote.Units;
        Out.CheckIssue = Quote.Issue;
        Submit.Check = false;
        Quote = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Submit));
        Out.PlainUnits = Quote.Units;
        Out.PlainIssue = Quote.Issue;
    }

    for (const std::string& FactId : CoordinateKnownFacts(*Def, Exposure.Heard))
    {
        for (const CoordinateFact& Fact : Def->Facts)
        {
            if (Fact.Id == FactId)
            {
                Out.Facts.Add({ClCMemberName(*Def, Fact.Holder), PWClassLesson::FromUtf8(Fact.Text)});
            }
        }
    }
    return Out;
}
} // namespace PWClassCoordinate

void SPWClassCoordinate::Construct(const FArguments& InArgs)
{
    PWClassLesson::FShellArgs Args;
    Args.Bridge = InArgs._Bridge;
    Args.Space = InArgs._Space;
    Args.OnClose = InArgs._OnClose;
    Args.OnLessonClosed = InArgs._OnLessonClosed;
    Args.ReturnFocus = InArgs._ReturnFocus;
    ConstructShell(Args);
}

int32 SPWClassCoordinate::ShellGoalCount() const
{
    return UE_ARRAY_COUNT(ClCGoals);
}

FText SPWClassCoordinate::ShellGoalName(const int32 Index) const
{
    return PWClassLesson::Text(TEXT("phase2.a3.coordinate.goal.") + FString(ClCGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]));
}

FText SPWClassCoordinate::ShellGoalHint(const int32 Index) const
{
    return PWClassLesson::Text(TEXT("phase2.a3.coordinate.goal.") + FString(ClCGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]) + TEXT(".hint"));
}

PerfectWorld::SchoolLessonGoal SPWClassCoordinate::ShellGoalAt(const int32 Index) const
{
    return PerfectWorld::SchoolLessonGoal(ClCGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]);
}

void SPWClassCoordinate::ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const
{
    const PerfectWorld::CoordinateExposure& Exposure = View.Coordinate.Exposure;
    bActive = Exposure.Active();
    Left = Exposure.UnitsLeft;
    Total = Exposure.UnitsTotal;
}

PWClassLesson::EMastery SPWClassCoordinate::ShellMastery(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const
{
    const PerfectWorld::StructureFamilyRecord* Record = View.Coordinate.FindFamily(FamilyId);
    if (Record == nullptr)
    {
        return PWClassLesson::EMastery::Unseen;
    }
    if (Record->Secure)
    {
        return PWClassLesson::EMastery::Secure;
    }
    return Record->Exposures > 0 ? PWClassLesson::EMastery::Practised : PWClassLesson::EMastery::Unseen;
}

void SPWClassCoordinate::ShellSessionChanged()
{
    // A different exercise (or none): the pencilled draft belonged to the last one, and a resumed screen starts with an empty draft.
    Draft.Reset();
}

void SPWClassCoordinate::ShellFrameChanged()
{
    Model = HasBridge() ? PWClassCoordinate::BuildModel(GetBridge(), GetFrame(), Draft) : PWClassCoordinate::FModel();
}

bool SPWClassCoordinate::SetTaskAssignee(const FString& TaskId, const FString& MemberId)
{
    if (!Model.bActive || Model.Beat != PerfectWorld::CoordinateBeat::Assign)
    {
        return false;
    }
    const bool bTask = Model.Tasks.ContainsByPredicate([&TaskId](const PWClassCoordinate::FTaskRow& Row) { return Row.Id == TaskId; });
    const bool bMember = Model.Members.ContainsByPredicate([&MemberId](const PWClassCoordinate::FMemberRow& Row) { return Row.Id == MemberId; });
    if (!bTask || !bMember)
    {
        return false;
    }
    Draft.Add(TaskId, MemberId);
    RefreshModel();
    return true;
}

bool SPWClassCoordinate::CycleTaskAssignee(const FString& TaskId)
{
    if (!Model.bActive || Model.Members.IsEmpty())
    {
        return false;
    }
    const PWClassCoordinate::FTaskRow* Task = Model.Tasks.FindByPredicate([&TaskId](const PWClassCoordinate::FTaskRow& Row) { return Row.Id == TaskId; });
    if (Task == nullptr)
    {
        return false;
    }
    int32 Current = INDEX_NONE;
    for (int32 Index = 0; Index < Model.Members.Num(); ++Index)
    {
        if (Model.Members[Index].Id == Task->Assignee) Current = Index;
    }
    return SetTaskAssignee(TaskId, Model.Members[(Current + 1) % Model.Members.Num()].Id);
}

void SPWClassCoordinate::ShellBuildCase(const TSharedRef<SVerticalBox>& Into)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    if (!Model.bActive)
    {
        AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.no_exercise")), false);
        return;
    }
    AddHeading(Into, Model.CaseName);
    AddNote(Into, Model.CaseBrief, false);
    for (const FText& Badge : {Model.Role, Model.Scarce, Model.Goal, Model.Method})
    {
        AddLine(Into, Badge, Style.TextSecondary);
    }
    AddUnitsMeter(Into);

    AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.members_heading")));
    for (const PWClassCoordinate::FMemberRow& Row : Model.Members)
    {
        FText Line = FText::Format(PWClassLesson::Text(TEXT("phase2.a3.coordinate.member.line")), Row.Name, FText::AsNumber(Row.Capacity),
                                   FText::AsNumber(Row.DraftLoad));
        if (Row.bOver)
        {
            Line = FText::Format(INVTEXT("{0} ({1})"), Line, PWClassLesson::Text(TEXT("phase2.a3.coordinate.member.over")));
        }
        AddNote(Into, Line, !Row.bOver);
    }
    AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.facts_heading")));
    if (Model.Facts.IsEmpty())
    {
        AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.facts_none")));
    }
    for (const PWClassCoordinate::FFactRow& Fact : Model.Facts)
    {
        AddNote(Into, FText::Format(PWClassLesson::Text(TEXT("phase2.a3.coordinate.fact.line")), Fact.Holder, Fact.Text), false);
    }
}

void SPWClassCoordinate::ShellBuildPlay(const TSharedRef<SVerticalBox>& Into)
{
    using PerfectWorld::CoordinateBeat;
    if (!Model.bActive)
    {
        return;
    }
    switch (Model.Beat)
    {
    case CoordinateBeat::Assign:
    {
        AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.beat.assign")));
        AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.tasks_heading")));
        for (const PWClassCoordinate::FTaskRow& Row : Model.Tasks)
        {
            const FString Id = Row.Id;
            FText Status = PWClassLesson::Text(TEXT("phase2.a3.coordinate.task.unassigned"));
            if (!Row.Assignee.IsEmpty())
            {
                Status = FText::Format(PWClassLesson::Text(Row.bSuited ? TEXT("phase2.a3.coordinate.task.assigned") : TEXT("phase2.a3.coordinate.task.unsuited")),
                                       Row.AssigneeName);
            }
            const FText Detail = FText::Format(INVTEXT("{0}. {1}"),
                                               FText::Format(PWClassLesson::Text(TEXT("phase2.a3.coordinate.task.line")), FText::AsNumber(Row.Load), Row.Suits), Status);
            AddActionRow(Into, TEXT("task.") + Id, Row.Name, Detail, PWClassLesson::Text(TEXT("phase2.a3.coordinate.task.cycle")),
                         Row.Assignee.IsEmpty() || Row.bSuited ? ECostTone::Plain : ECostTone::Short, true,
                         [this, Id]() { CycleTaskAssignee(Id); });
        }
        // The price is the core's quote; a draft that is not complete is never sent, so the button waits for one.
        PerfectWorld::CoordinateAction Priced;
        Priced.Kind = PerfectWorld::CoordinateActionKind::AssignWork;
        PerfectWorld::SchoolLessonQuote Price;
        Price.Units = Model.AssignUnits;
        Price.Issue = Model.AssignIssue;
        AddQuotedRow(Into, TEXT("commit"), PWClassLesson::Text(TEXT("phase2.a3.coordinate.assign.commit")), FText::GetEmpty(), Price,
                     [this]() { ExecuteCommitAssignment(); }, false, Model.bDraftComplete,
                     PWClassLesson::Text(TEXT("phase2.a3.coordinate.assign.incomplete")));
        if (!Model.bDraftComplete)
        {
            // The price of committing is shown before anything is chosen.
            AddLine(Into, PWClassLesson::Text2(TEXT("phase2.a3.coordinate.assign.price"), PWClassLesson::CostText(Model.AssignUnits),
                                               PWClassLesson::Number(Model.UnitsLeft)), PWUI::FStyle::Get().TextSecondary);
        }
        break;
    }
    case CoordinateBeat::Exchange:
    {
        AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.beat.exchange")));
        for (const PWClassCoordinate::FMemberRow& Row : Model.Members)
        {
            if (Row.bSelf) continue;
            const FString Id = Row.Id;
            const FText Title = PWClassLesson::Text1(TEXT("phase2.a3.coordinate.exchange.talk"), Row.Name);
            if (Row.bHeard)
            {
                AddActionRow(Into, TEXT("talk.") + Id, Title, PWClassLesson::Text1(TEXT("phase2.a3.coordinate.exchange.heard"), Row.Name),
                             FText::GetEmpty(), ECostTone::Plain, false, [this, Id]() { ExecuteExchange(Id); });
                continue;
            }
            PerfectWorld::SchoolLessonQuote Quote;
            Quote.Units = Row.TalkUnits;
            Quote.Issue = Row.TalkIssue;
            AddQuotedRow(Into, TEXT("talk.") + Id, Title, FText::GetEmpty(), Quote, [this, Id]() { ExecuteExchange(Id); });
        }
        AddActionRow(Into, TEXT("finish_exchange"), PWClassLesson::Text(TEXT("phase2.a3.coordinate.exchange.finish")),
                     PWClassLesson::Text(TEXT("phase2.a3.coordinate.exchange.finish_hint")), PWClassLesson::CostText(0), ECostTone::Free,
                     Model.EndExchangeIssue.Ok(), [this]() { ExecuteEndExchange(); });
        break;
    }
    case CoordinateBeat::Settle:
    {
        AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.beat.settle")));
        for (const PWClassCoordinate::FResolutionRow& Row : Model.Resolutions)
        {
            const PerfectWorld::CoordinateResolution Resolution = Row.Resolution;
            PerfectWorld::SchoolLessonQuote Quote;
            Quote.Units = Row.Units;
            Quote.Issue = Row.Issue;
            AddQuotedRow(Into, FString::Printf(TEXT("settle.%d"), static_cast<int32>(Resolution)), Row.Name, Row.Hint, Quote,
                         [this, Resolution]() { ExecuteSettle(Resolution); });
        }
        break;
    }
    case CoordinateBeat::Submit:
    {
        AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.coordinate.beat.submit")));
        PerfectWorld::SchoolLessonQuote WithCheck;
        WithCheck.Units = Model.CheckUnits;
        WithCheck.Issue = Model.CheckIssue;
        AddQuotedRow(Into, TEXT("submit.check"), PWClassLesson::Text(TEXT("phase2.a3.coordinate.submit.check")),
                     PWClassLesson::Text(TEXT("phase2.a3.coordinate.submit.check_hint")), WithCheck, [this]() { ExecuteSubmit(true); });
        PerfectWorld::SchoolLessonQuote Plain;
        Plain.Units = Model.PlainUnits;
        Plain.Issue = Model.PlainIssue;
        AddQuotedRow(Into, TEXT("submit.plain"), PWClassLesson::Text(TEXT("phase2.a3.coordinate.submit.plain")),
                     PWClassLesson::Text(TEXT("phase2.a3.coordinate.submit.plain_hint")), Plain, [this]() { ExecuteSubmit(false); });
        break;
    }
    }
    AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.leave_note")));
}

bool SPWClassCoordinate::ShellDescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const
{
    const PerfectWorld::StructureOutcome* Base = nullptr;
    if (const auto* Result = std::get_if<PerfectWorld::CoordinateOutcome>(&Outcome))
    {
        Base = Result;
        OutFacts.Detail.Add(FText::Format(PWClassLesson::Text(TEXT("phase2.a3.coordinate.outcome.counts")), FText::AsNumber(Result->OverloadedMembers),
                                          FText::AsNumber(Result->MismatchedTasks), FText::AsNumber(Result->MissedFacts), FText::AsNumber(Result->IdleConversations)));
    }
    else if (const auto* Plain = std::get_if<PerfectWorld::StructureOutcome>(&Outcome))
    {
        Base = Plain;
    }
    if (Base == nullptr)
    {
        return false;
    }
    OutFacts.Grade = Base->Grade;
    OutFacts.bForeground = Base->Foreground;
    OutFacts.LearningPermille = Base->LearningPermille;
    OutFacts.UnitsSpent = Base->UnitsSpent;
    OutFacts.bBecameSecure = Base->BecameSecure;
    const struct { uint8 Bit; const TCHAR* Key; } Failures[] = {
        {PerfectWorld::CoordinateFailOverloaded, TEXT("phase2.a3.coordinate.fail.overloaded")},
        {PerfectWorld::CoordinateFailMismatched, TEXT("phase2.a3.coordinate.fail.mismatched")},
        {PerfectWorld::CoordinateFailUninformed, TEXT("phase2.a3.coordinate.fail.uninformed")},
        {PerfectWorld::CoordinateFailUnsettled, TEXT("phase2.a3.coordinate.fail.unsettled")},
        {PerfectWorld::CoordinateFailSeam, TEXT("phase2.a3.coordinate.fail.seam")},
        {PerfectWorld::CoordinateFailWasted, TEXT("phase2.a3.coordinate.fail.wasted")}};
    for (const auto& Failure : Failures)
    {
        if ((Base->Failures & Failure.Bit) != 0) OutFacts.Diagnostics.Add(PWClassLesson::Text(Failure.Key));
    }
    return true;
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteCommitAssignment()
{
    if (!Model.bDraftComplete)
    {
        // Nothing is sent: the core would refuse an incomplete assignment, and a pencil draft is not a decision.
        PerfectWorld::SchoolSessionResult Refused;
        Refused.Issue = PerfectWorld::SchoolIssue{"structure.bad_choice", {}};
        ShowBanner(PWClassLesson::Text(TEXT("phase2.a3.coordinate.assign.incomplete")), true);
        return Refused;
    }
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::AssignWork;
    for (const PWClassCoordinate::FTaskRow& Row : Model.Tasks)
    {
        Action.Assignment.push_back({PWClassLesson::ToUtf8(Row.Id), PWClassLesson::ToUtf8(Row.Assignee)});
    }
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassLesson::Text(TEXT("phase2.a3.coordinate.assign.done")));
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteExchange(const FString& MemberId)
{
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::Exchange;
    Action.Member = PWClassLesson::ToUtf8(MemberId);
    FText Name = FText::GetEmpty();
    for (const PWClassCoordinate::FMemberRow& Row : Model.Members)
    {
        if (Row.Id == MemberId) Name = Row.Name;
    }
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassLesson::Text1(TEXT("phase2.a3.coordinate.exchange.done"), Name));
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteEndExchange()
{
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::EndExchange;
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::GetEmpty());
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteSettle(const PerfectWorld::CoordinateResolution Resolution)
{
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::Settle;
    Action.Resolution = Resolution;
    FText Name = FText::GetEmpty();
    for (const PWClassCoordinate::FResolutionRow& Row : Model.Resolutions)
    {
        if (Row.Resolution == Resolution) Name = Row.Name;
    }
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassLesson::Text1(TEXT("phase2.a3.coordinate.settle.done"), Name));
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteSubmit(const bool bCheck)
{
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::Submit;
    Action.Check = bCheck;
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassLesson::Text(TEXT("phase2.a3.coordinate.submit.done")));
}
