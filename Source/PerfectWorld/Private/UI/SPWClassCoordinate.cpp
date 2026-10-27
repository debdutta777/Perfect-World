#include "UI/SPWClassCoordinate.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>
#include <variant>

// The StructureCoordinate lesson. The shared shell, words and bridge are in SPWClassInvestigate.cpp. Nothing here decides anything:
// prices are the core's quotes, the group arithmetic shown is plain addition of numbers the case publishes, and the facts shown are
// only those the player has actually learned.

namespace
{
const PerfectWorld::CoordinateGoal A3CGoals[] = {PerfectWorld::CoordinateGoal::OrganiseWork, PerfectWorld::CoordinateGoal::HearEveryone,
                                                 PerfectWorld::CoordinateGoal::SettleConstraint, PerfectWorld::CoordinateGoal::SubmitCleanly};
const TCHAR* const A3CGoalKeys[] = {TEXT("organise_work"), TEXT("hear_everyone"), TEXT("settle_constraint"), TEXT("submit_cleanly")};
const PerfectWorld::CoordinateResolution A3CResolutions[] = {
    PerfectWorld::CoordinateResolution::Leave, PerfectWorld::CoordinateResolution::Rebalance, PerfectWorld::CoordinateResolution::Reschedule,
    PerfectWorld::CoordinateResolution::TrimScope, PerfectWorld::CoordinateResolution::AskExtension};
const TCHAR* const A3CResolutionKeys[] = {TEXT("leave"), TEXT("rebalance"), TEXT("reschedule"), TEXT("trim_scope"), TEXT("ask_extension")};

FString A3CWord(const std::string_view Name)
{
    return FString(static_cast<int32>(Name.size()), Name.data());
}

bool A3CHas(const std::vector<std::string>& Ids, const std::string& Id)
{
    return std::find(Ids.begin(), Ids.end(), Id) != Ids.end();
}

FText A3CMemberName(const PerfectWorld::CoordinateCaseDef& Def, const std::string& Id)
{
    for (const PerfectWorld::CoordinateMember& Member : Def.Members)
    {
        if (Member.Id != Id) continue;
        const FText Name = PWClassA3::TextFromUtf8(Member.Name);
        return Member.Self ? FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.member.you")), Name) : Name;
    }
    return FText::GetEmpty();
}
} // namespace

namespace PWClassCoordinate
{
FModel BuildModel(const PWClassA3::ILessonBridge& Bridge, const PWClassA3::FFrame& Frame, const FDraft& Draft)
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
    Out.CaseId = PWClassA3::FromUtf8(Def->Id);
    Out.CaseName = PWClassA3::TextFromUtf8(Def->Name);
    Out.CaseBrief = PWClassA3::TextFromUtf8(Def->Brief);
    Out.UnitsLeft = Exposure.UnitsLeft;
    Out.UnitsTotal = Exposure.UnitsTotal;
    Out.Beat = Exposure.Beat;
    Out.Role = PWClassA3::Str(TEXT("phase2.a3.coordinate.role.") + A3CWord(CoordinateRoleNames[static_cast<size_t>(Def->Role)]));
    Out.Scarce = PWClassA3::Str(TEXT("phase2.a3.coordinate.scarce.") + A3CWord(CoordinateScarceNames[static_cast<size_t>(Def->Scarce)]));
    Out.Goal = PWClassA3::Str(TEXT("phase2.a3.coordinate.goal.") + A3CWord(CoordinateGoalNames[static_cast<size_t>(Exposure.Goal)]));
    FText MethodName = PWClassA3::TextFromUtf8(Def->MethodId);
    if (const auto* Family = Catalogue->FindFamily(Def->FamilyId))
    {
        for (const StructureMethod& Method : Family->Methods)
        {
            if (Method.Id == Def->MethodId) MethodName = PWClassA3::TextFromUtf8(Method.Name);
        }
    }
    Out.Method = FText::Format(PWClassA3::Str(Exposure.MethodKnown ? TEXT("phase2.a3.common.method.known") : TEXT("phase2.a3.common.method.unfamiliar")),
                               MethodName);

    const bool bAssigning = Exposure.Beat == CoordinateBeat::Assign;
    const auto AssigneeOf = [&](const std::string& TaskId) -> FString
    {
        if (!bAssigning)
        {
            for (const CoordinateAssignment& Pair : Exposure.Assignment)
            {
                if (Pair.Task == TaskId) return PWClassA3::FromUtf8(Pair.Member);
            }
            return FString();
        }
        const FString* Found = Draft.Find(PWClassA3::FromUtf8(TaskId));
        return Found != nullptr ? *Found : FString();
    };

    TMap<FString, int32> Loads;
    Out.bDraftComplete = !Def->Tasks.empty();
    CoordinateAction FullAssignment;
    FullAssignment.Kind = CoordinateActionKind::AssignWork;
    for (const CoordinateTask& Task : Def->Tasks)
    {
        FTaskRow Row;
        Row.Id = PWClassA3::FromUtf8(Task.Id);
        Row.Name = PWClassA3::TextFromUtf8(Task.Name);
        Row.Load = Task.Load;
        TArray<FText> Suits;
        for (const std::string& MemberId : Task.Fits) Suits.Add(A3CMemberName(*Def, MemberId));
        Row.Suits = FText::Join(INVTEXT(", "), Suits);
        Row.Assignee = AssigneeOf(Task.Id);
        const std::string AssigneeId(TCHAR_TO_UTF8(*Row.Assignee));
        const bool bKnownMember = std::any_of(Def->Members.begin(), Def->Members.end(), [&AssigneeId](const CoordinateMember& M) { return M.Id == AssigneeId; });
        if (!Row.Assignee.IsEmpty() && bKnownMember)
        {
            Row.AssigneeName = A3CMemberName(*Def, AssigneeId);
            Row.bSuited = A3CHas(Task.Fits, AssigneeId);
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
        Row.Id = PWClassA3::FromUtf8(Member.Id);
        Row.Name = A3CMemberName(*Def, Member.Id);
        Row.Capacity = Member.Capacity;
        Row.bSelf = Member.Self;
        Row.bHeard = A3CHas(Exposure.Heard, Member.Id);
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
        for (size_t Index = 0; Index < UE_ARRAY_COUNT(A3CResolutions); ++Index)
        {
            FResolutionRow Row;
            Row.Resolution = A3CResolutions[Index];
            Row.Name = PWClassA3::Str(TEXT("phase2.a3.coordinate.resolution.") + FString(A3CResolutionKeys[Index]));
            Row.Hint = PWClassA3::Str(TEXT("phase2.a3.coordinate.resolution.") + FString(A3CResolutionKeys[Index]) + TEXT(".hint"));
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
                Out.Facts.Add({A3CMemberName(*Def, Fact.Holder), PWClassA3::TextFromUtf8(Fact.Text)});
            }
        }
    }
    return Out;
}
} // namespace PWClassCoordinate

void SPWClassCoordinate::Construct(const FArguments& InArgs)
{
    PWClassA3::FShellArgs Args;
    Args.Bridge = InArgs._Bridge;
    Args.Space = InArgs._Space;
    Args.OnClose = InArgs._OnClose;
    Args.OnLessonClosed = InArgs._OnLessonClosed;
    Args.ReturnFocus = InArgs._ReturnFocus;
    ConstructShell(Args);
}

FText SPWClassCoordinate::ShellSubtitle() const
{
    return PWClassA3::Str(TEXT("phase2.a3.coordinate.subtitle"));
}

int32 SPWClassCoordinate::ShellGoalCount() const
{
    return UE_ARRAY_COUNT(A3CGoals);
}

FText SPWClassCoordinate::ShellGoalName(const int32 Index) const
{
    return PWClassA3::Str(TEXT("phase2.a3.coordinate.goal.") + FString(A3CGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]));
}

FText SPWClassCoordinate::ShellGoalHint(const int32 Index) const
{
    return PWClassA3::Str(TEXT("phase2.a3.coordinate.goal.") + FString(A3CGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]) + TEXT(".hint"));
}

PerfectWorld::SchoolLessonGoal SPWClassCoordinate::ShellGoalAt(const int32 Index) const
{
    return PerfectWorld::SchoolLessonGoal(A3CGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]);
}

void SPWClassCoordinate::ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const
{
    const PerfectWorld::CoordinateExposure& Exposure = View.Coordinate.Exposure;
    bActive = Exposure.Active();
    Left = Exposure.UnitsLeft;
    Total = Exposure.UnitsTotal;
}

void SPWClassCoordinate::ShellFrameChanged()
{
    const PerfectWorld::CoordinateExposure& Exposure = GetFrame().View.Coordinate.Exposure;
    const FString Owner = FString::Printf(TEXT("%llu:%s"), static_cast<unsigned long long>(GetFrame().Session), *PWClassA3::FromUtf8(Exposure.CaseId));
    if (Owner != DraftOwner)
    {
        // A different exercise (or none): the pencilled draft belonged to the last one.
        DraftOwner = Owner;
        Draft.Reset();
    }
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

void SPWClassCoordinate::ShellBuildPlay(const TSharedRef<SVerticalBox>& Into)
{
    using PerfectWorld::CoordinateBeat;
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    if (!Model.bActive)
    {
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.no_exercise")), false);
        return;
    }
    AddHeading(Into, Model.CaseName);
    AddNote(Into, Model.CaseBrief, false);
    for (const FText& Badge : {Model.Role, Model.Scarce, Model.Goal, Model.Method})
    {
        Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
        [
            PWUI::MakeText(Badge, PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
    }
    AddUnitsMeter(Into);

    AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.members_heading")));
    for (const PWClassCoordinate::FMemberRow& Row : Model.Members)
    {
        FText Line = FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.member.line")), Row.Name, FText::AsNumber(Row.Capacity),
                                   FText::AsNumber(Row.DraftLoad));
        if (Row.bOver)
        {
            Line = FText::Format(INVTEXT("{0} ({1})"), Line, PWClassA3::Str(TEXT("phase2.a3.coordinate.member.over")));
        }
        AddNote(Into, Line, !Row.bOver);
    }
    AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.facts_heading")));
    if (Model.Facts.IsEmpty())
    {
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.facts_none")));
    }
    for (const PWClassCoordinate::FFactRow& Fact : Model.Facts)
    {
        AddNote(Into, FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.fact.line")), Fact.Holder, Fact.Text), false);
    }

    switch (Model.Beat)
    {
    case CoordinateBeat::Assign:
    {
        AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.beat.assign")));
        AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.tasks_heading")));
        for (const PWClassCoordinate::FTaskRow& Row : Model.Tasks)
        {
            const FString Id = Row.Id;
            FText Status = PWClassA3::Str(TEXT("phase2.a3.coordinate.task.unassigned"));
            if (!Row.Assignee.IsEmpty())
            {
                Status = FText::Format(PWClassA3::Str(Row.bSuited ? TEXT("phase2.a3.coordinate.task.assigned") : TEXT("phase2.a3.coordinate.task.unsuited")),
                                       Row.AssigneeName);
            }
            const FText Detail = FText::Format(INVTEXT("{0}. {1}"),
                                               FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.task.line")), FText::AsNumber(Row.Load), Row.Suits), Status);
            AddActionRow(Into, TEXT("task.") + Id, Row.Name, Detail, PWClassA3::Str(TEXT("phase2.a3.coordinate.task.cycle")),
                         Row.Assignee.IsEmpty() || Row.bSuited ? ECostTone::Plain : ECostTone::Short, true,
                         [this, Id]() { CycleTaskAssignee(Id); });
        }
        const bool bShort = Model.AssignIssue.Code == "structure.units_short";
        AddActionRow(Into, TEXT("commit"), PWClassA3::Str(TEXT("phase2.a3.coordinate.assign.commit")), FText::GetEmpty(),
                     bShort ? FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.cost.short")), FText::AsNumber(Model.AssignUnits), FText::AsNumber(Model.UnitsLeft))
                            : PWClassA3::CostText(Model.AssignUnits),
                     bShort ? ECostTone::Short : ECostTone::Plain, !Model.bDraftComplete || Model.AssignIssue.Ok(),
                     [this]() { ExecuteCommitAssignment(); });
        break;
    }
    case CoordinateBeat::Exchange:
    {
        AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.beat.exchange")));
        for (const PWClassCoordinate::FMemberRow& Row : Model.Members)
        {
            if (Row.bSelf) continue;
            const FString Id = Row.Id;
            const bool bShort = Row.TalkIssue.Code == "structure.units_short";
            if (Row.bHeard)
            {
                AddActionRow(Into, TEXT("talk.") + Id, FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.exchange.talk")), Row.Name),
                             FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.exchange.heard")), Row.Name), FText::GetEmpty(), ECostTone::Plain,
                             false, [this, Id]() { ExecuteExchange(Id); });
                continue;
            }
            AddActionRow(Into, TEXT("talk.") + Id, FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.exchange.talk")), Row.Name), FText::GetEmpty(),
                         bShort ? FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.cost.short")), FText::AsNumber(Row.TalkUnits), FText::AsNumber(Model.UnitsLeft))
                                : PWClassA3::CostText(Row.TalkUnits),
                         bShort ? ECostTone::Short : (Row.TalkUnits == 0 ? ECostTone::Free : ECostTone::Plain), Row.TalkIssue.Ok(),
                         [this, Id]() { ExecuteExchange(Id); });
        }
        AddActionRow(Into, TEXT("finish_exchange"), PWClassA3::Str(TEXT("phase2.a3.coordinate.exchange.finish")),
                     PWClassA3::Str(TEXT("phase2.a3.coordinate.exchange.finish_hint")), PWClassA3::CostText(0), ECostTone::Free,
                     Model.EndExchangeIssue.Ok(), [this]() { ExecuteEndExchange(); });
        break;
    }
    case CoordinateBeat::Settle:
    {
        AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.beat.settle")));
        for (const PWClassCoordinate::FResolutionRow& Row : Model.Resolutions)
        {
            const PerfectWorld::CoordinateResolution Resolution = Row.Resolution;
            const bool bShort = Row.Issue.Code == "structure.units_short";
            AddActionRow(Into, FString::Printf(TEXT("settle.%d"), static_cast<int32>(Resolution)), Row.Name, Row.Hint,
                         bShort ? FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.cost.short")), FText::AsNumber(Row.Units), FText::AsNumber(Model.UnitsLeft))
                                : PWClassA3::CostText(Row.Units),
                         bShort ? ECostTone::Short : (Row.Units == 0 ? ECostTone::Free : ECostTone::Plain), Row.Issue.Ok(),
                         [this, Resolution]() { ExecuteSettle(Resolution); });
        }
        break;
    }
    case CoordinateBeat::Submit:
    {
        AddHeading(Into, PWClassA3::Str(TEXT("phase2.a3.coordinate.beat.submit")));
        const bool bShort = Model.CheckIssue.Code == "structure.units_short";
        AddActionRow(Into, TEXT("submit.check"), PWClassA3::Str(TEXT("phase2.a3.coordinate.submit.check")),
                     PWClassA3::Str(TEXT("phase2.a3.coordinate.submit.check_hint")),
                     bShort ? FText::Format(PWClassA3::Str(TEXT("phase2.a3.common.cost.short")), FText::AsNumber(Model.CheckUnits), FText::AsNumber(Model.UnitsLeft))
                            : PWClassA3::CostText(Model.CheckUnits),
                     bShort ? ECostTone::Short : (Model.CheckUnits == 0 ? ECostTone::Free : ECostTone::Plain), Model.CheckIssue.Ok(),
                     [this]() { ExecuteSubmit(true); });
        AddActionRow(Into, TEXT("submit.plain"), PWClassA3::Str(TEXT("phase2.a3.coordinate.submit.plain")),
                     PWClassA3::Str(TEXT("phase2.a3.coordinate.submit.plain_hint")), PWClassA3::CostText(Model.PlainUnits), ECostTone::Free,
                     Model.PlainIssue.Ok(), [this]() { ExecuteSubmit(false); });
        break;
    }
    }
    AddNote(Into, PWClassA3::Str(TEXT("phase2.a3.common.leave_note")));
}

TArray<FText> SPWClassCoordinate::ShellOutcomeDetail(const PerfectWorld::SchoolLessonOutcome& Outcome) const
{
    TArray<FText> Lines;
    const auto* Result = std::get_if<PerfectWorld::CoordinateOutcome>(&Outcome);
    if (Result == nullptr)
    {
        return Lines;
    }
    Lines.Add(FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.outcome.counts")), FText::AsNumber(Result->OverloadedMembers),
                            FText::AsNumber(Result->MismatchedTasks), FText::AsNumber(Result->MissedFacts), FText::AsNumber(Result->IdleConversations)));
    const struct { uint8 Bit; const TCHAR* Key; } Failures[] = {
        {PerfectWorld::CoordinateFailOverloaded, TEXT("phase2.a3.coordinate.fail.overloaded")},
        {PerfectWorld::CoordinateFailMismatched, TEXT("phase2.a3.coordinate.fail.mismatched")},
        {PerfectWorld::CoordinateFailUninformed, TEXT("phase2.a3.coordinate.fail.uninformed")},
        {PerfectWorld::CoordinateFailUnsettled, TEXT("phase2.a3.coordinate.fail.unsettled")},
        {PerfectWorld::CoordinateFailSeam, TEXT("phase2.a3.coordinate.fail.seam")},
        {PerfectWorld::CoordinateFailWasted, TEXT("phase2.a3.coordinate.fail.wasted")}};
    for (const auto& Failure : Failures)
    {
        if ((Result->Failures & Failure.Bit) != 0) Lines.Add(PWClassA3::Str(Failure.Key));
    }
    return Lines;
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteCommitAssignment()
{
    if (!Model.bDraftComplete)
    {
        // Nothing is sent: the core would refuse an incomplete assignment, and a pencil draft is not a decision.
        PerfectWorld::SchoolSessionResult Refused;
        Refused.Issue = PerfectWorld::SchoolIssue{"structure.bad_choice", {}};
        ShowBanner(PWClassA3::Str(TEXT("phase2.a3.coordinate.assign.incomplete")), true);
        return Refused;
    }
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::AssignWork;
    for (const PWClassCoordinate::FTaskRow& Row : Model.Tasks)
    {
        Action.Assignment.push_back({std::string(TCHAR_TO_UTF8(*Row.Id)), std::string(TCHAR_TO_UTF8(*Row.Assignee))});
    }
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassA3::Str(TEXT("phase2.a3.coordinate.assign.done")));
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteExchange(const FString& MemberId)
{
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::Exchange;
    Action.Member = std::string(TCHAR_TO_UTF8(*MemberId));
    FText Name = FText::GetEmpty();
    for (const PWClassCoordinate::FMemberRow& Row : Model.Members)
    {
        if (Row.Id == MemberId) Name = Row.Name;
    }
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.exchange.done")), Name));
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
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::Format(PWClassA3::Str(TEXT("phase2.a3.coordinate.settle.done")), Name));
}

PerfectWorld::SchoolSessionResult SPWClassCoordinate::ExecuteSubmit(const bool bCheck)
{
    PerfectWorld::CoordinateAction Action;
    Action.Kind = PerfectWorld::CoordinateActionKind::Submit;
    Action.Check = bCheck;
    return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassA3::Str(TEXT("phase2.a3.coordinate.submit.done")));
}
