#include "PerfectWorldLaneLifeConversation.h"

#include "PerfectWorldLaneLifeSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWLaneLifeTalk, Log, All);

FPWCoreConversationSession::FPWCoreConversationSession(const TWeakObjectPtr<UPWLaneLifeSubsystem>& InOwner, const FText& InSpeaker)
    : Owner(InOwner)
    , Speaker(InSpeaker)
{
}

FPWConversationView FPWCoreConversationSession::GetView() const
{
    FPWConversationView View;
    View.Speaker = Speaker;
    View.bCanLeave = true;
    const UPWLaneLifeSubsystem* Life = Owner.Get();
    if (Life == nullptr || !Life->GetConversation().Open)
    {
        return View;
    }
    const PerfectWorld::ConversationState& State = Life->GetConversation();
    if (const PerfectWorld::ConversationNodeDef* Node = PerfectWorld::FindConversationNode(State.NodeId))
    {
        View.Line = PWLaneLife::ToText(Node->Line);
    }
    for (const PerfectWorld::ConversationChoiceView& Choice : PerfectWorld::VisibleConversationChoices(State))
    {
        if (Choice.bIsLeave)
        {
            continue; // the panel's Leave button is the core's Leave choice
        }
        FPWConversationChoice Row;
        Row.Id = PWLaneLife::ToName(Choice.Id);
        Row.Label = PWLaneLife::ToText(Choice.Text);
        Row.CostMinutes = Choice.CostMinutes;
        View.Choices.Add(Row);
    }
    return View;
}

void FPWCoreConversationSession::Choose(const FName ChoiceId)
{
    UPWLaneLifeSubsystem* Life = Owner.Get();
    if (Life == nullptr || !Life->GetConversation().Open)
    {
        return;
    }
    const PerfectWorld::ConversationChooseResult Result =
        PerfectWorld::ChooseConversationChoice(Life->MutableConversation(), PWLaneLife::ToStd(ChoiceId));
    if (!Result.Ok())
    {
        // A stale or already-taken choice: the core left the state untouched, so there is nothing to undo.
        UE_LOG(LogPWLaneLifeTalk, Display, TEXT("Choice %s refused by the core: %s."), *ChoiceId.ToString(),
               *PWLaneLife::ToFString(PerfectWorld::ConversationErrorCode(Result.Error)));
        return;
    }
    // TODO(time-contract): charge the cost through the time contract. There is no clock yet, so it only accumulates.
    Life->AddPendingMinutes(Result.CostMinutes);
}

bool FPWCoreConversationSession::IsFinished() const
{
    const UPWLaneLifeSubsystem* Life = Owner.Get();
    return Life == nullptr || !Life->GetConversation().Open;
}

void FPWCoreConversationSession::Leave()
{
    UPWLaneLifeSubsystem* Life = Owner.Get();
    if (Life != nullptr && Life->GetConversation().Open)
    {
        (void)PerfectWorld::LeaveConversation(Life->MutableConversation());
    }
}
