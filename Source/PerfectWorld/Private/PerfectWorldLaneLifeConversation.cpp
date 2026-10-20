#include "PerfectWorldLaneLifeConversation.h"

#include "PerfectWorldLaneLifeSubsystem.h"
#include "Engine/World.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldNpcPilotBridge.h"
#include "WorldReducer.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWLaneLifeTalk, Log, All);

PerfectWorld::ConversationCatalogue FaCatalogueForPerson(const FName PersonId)
{
    // One routing rule, owned by the core: the world's conversation commands use the same one.
    return PerfectWorld::ConversationCatalogueForPerson(PWLaneLife::ToStd(PersonId)).value_or(PerfectWorld::ConversationCatalogue{});
}

FPWCoreConversationSession::FPWCoreConversationSession(const TWeakObjectPtr<UPWLaneLifeSubsystem>& InOwner,
                                                     const FName InPersonId, const FText& InSpeaker, const uint32 InSessionNumber)
    : Owner(InOwner)
    , PersonId(InPersonId)
    , Speaker(InSpeaker)
    , SessionNumber(InSessionNumber)
{
}

const PerfectWorld::ConversationState* FPWCoreConversationSession::FindOpenState() const
{
    const UPWLaneLifeSubsystem* Life = Owner.Get();
    const PerfectWorld::ConversationState* State = Life != nullptr ? Life->FindConversation(PersonId) : nullptr;
    return State != nullptr && State->Open && State->SessionCount == SessionNumber ? State : nullptr;
}

FPWConversationView FPWCoreConversationSession::GetView() const
{
    FPWConversationView View;
    View.Speaker = Speaker;
    View.bCanLeave = true;
    const PerfectWorld::ConversationState* State = FindOpenState();
    if (State == nullptr)
    {
        return View;
    }
    const PerfectWorld::ConversationCatalogue Catalogue = FaCatalogueForPerson(PersonId);
    if (const PerfectWorld::ConversationNodeDef* Node = PerfectWorld::FindConversationNode(Catalogue, State->NodeId))
    {
        View.Line = PWLaneLife::ToText(Node->Line);
    }
    if (!RefusalNote.IsEmpty())
    {
        View.Line = FText::Format(NSLOCTEXT("PerfectWorldLaneLifeTalk", "LineWithNote", "{0}\n\n({1})"), View.Line, RefusalNote);
    }
    for (const PerfectWorld::ConversationChoiceView& Choice : PerfectWorld::VisibleConversationChoices(Catalogue, *State))
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
    if (Life == nullptr || FindOpenState() == nullptr)
    {
        return;
    }
    // conversation.choose validates the choice, applies its facts and, for a paid one, moves the world clock by its cost in
    // the same command. Nothing is charged here; the clock readout simply reads the new time.
    const FPWLaneCommandOutcome Outcome = Life->ChooseConversation(PersonId, ChoiceId, SessionNumber);
    if (Outcome.bAccepted)
    {
        RefusalNote = FText::GetEmpty();
        if (R55IsWorkPilot(PersonId) && IsFinished())
        {
            if (auto* Presence = Life->GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
            {
                Presence->Reconcile(Life->GetNow());
                Presence->ReleasePilotChat(PersonId);
            }
        }
        return;
    }
    // A refusal changed nothing in the world. A stale or already-taken choice has no words (the panel's refreshed list drops it);
    // anything else is shown under the line, or as a notice when the refusal also ended the session.
    UE_LOG(LogPWLaneLifeTalk, Display, TEXT("Choice %s refused by the core: %s."), *ChoiceId.ToString(), *Outcome.Code);
    RefusalNote = Outcome.Reason;
    if (IsFinished())
    {
        Life->ShowPlayerNotice(Outcome.Reason);
    }
}

bool FPWCoreConversationSession::IsFinished() const
{
    return FindOpenState() == nullptr;
}

void FPWCoreConversationSession::Leave()
{
    UPWLaneLifeSubsystem* Life = Owner.Get();
    if (Life != nullptr && FindOpenState() != nullptr)
    {
        Life->LeaveConversation(PersonId);
    }
    if (Life != nullptr && R55IsWorkPilot(PersonId))
    {
        if (auto* Presence = Life->GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
        {
            Presence->Reconcile(Life->GetNow());
            Presence->ReleasePilotChat(PersonId);
        }
    }
}
