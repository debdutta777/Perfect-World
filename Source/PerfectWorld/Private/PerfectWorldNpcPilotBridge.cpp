#include "PerfectWorldNpcPilotBridge.h"

#include "Engine/World.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPresenceInternal.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

#include <algorithm>

bool R55IsWorkPilot(const FName PersonId)
{
    return PersonId == FName(TEXT("person.debashis_pal")) || PersonId == FName(TEXT("person.mahua_saha")) ||
        PersonId == FName(TEXT("person.satyajit_dutta"));
}

FText R55NpcReasonText(const FName Key)
{
    if (Key == FName(TEXT("npc.reason.working_at_bench")))
        return NSLOCTEXT("PerfectWorldWork", "Bench", "Working at the bench.");
    if (Key == FName(TEXT("npc.reason.working_at_tools")))
        return NSLOCTEXT("PerfectWorldWork", "Tools", "Putting the tools away.");
    if (Key == FName(TEXT("npc.reason.working_at_finishing_table")))
        return NSLOCTEXT("PerfectWorldWork", "Finishing", "Finishing an item at the table.");
    if (Key == FName(TEXT("npc.reason.serving_at_counter")))
        return NSLOCTEXT("PerfectWorldWork", "Counter", "Serving at the counter.");
    if (Key == FName(TEXT("npc.reason.weighing")))
        return NSLOCTEXT("PerfectWorldWork", "Scale", "Weighing an order.");
    if (Key == FName(TEXT("npc.reason.restocking")))
        return NSLOCTEXT("PerfectWorldWork", "Stock", "Restocking the shelves.");
    if (Key == FName(TEXT("npc.reason.meal")))
        return NSLOCTEXT("PerfectWorldWork", "Meal", "Taking a meal break.");
    if (Key == FName(TEXT("npc.reason.rest")))
        return NSLOCTEXT("PerfectWorldWork", "Rest", "Taking a rest.");
    if (Key == FName(TEXT("npc.reason.tea_break")))
        return NSLOCTEXT("PerfectWorldWork", "Tea", "Taking a tea break.");
    if (Key == FName(TEXT("npc.reason.closing_up")))
        return NSLOCTEXT("PerfectWorldWork", "Closing", "Closing up the shop.");
    if (Key == FName(TEXT("npc.reason.next_safe_pause")))
        return NSLOCTEXT("PerfectWorldWork", "SafePause", "Will talk at the next safe pause, within three game minutes.");
    if (Key == FName(TEXT("npc.reason.walking")))
        return NSLOCTEXT("PerfectWorldWork", "Walking", "On the way somewhere.");
    if (Key == FName(TEXT("npc.reason.asleep")))
        return NSLOCTEXT("PerfectWorldWork", "Asleep", "Asleep; please come back later.");
    if (Key == FName(TEXT("npc.reason.blocked")))
        return NSLOCTEXT("PerfectWorldWork", "Yield", "Waiting for the way to clear.");
    if (Key == FName(TEXT("npc.reason.chat")))
        return NSLOCTEXT("PerfectWorldWork", "Chat", "Ready to talk.");
    if (Key == FName(TEXT("npc.reason.available")))
        return NSLOCTEXT("PerfectWorldWork", "Available", "Available to talk.");
    return FText::GetEmpty();
}

bool R55ResolvePilot(FNpPresenceRecord& Record, const PerfectWorld::GameInstant Now)
{
    using namespace PerfectWorld;
    auto& Windows = Record.R55Windows;
    const auto Block = ResolveSchedule(Record.Schedule.Person, Record.Schedule.Travel, Now);
    auto KeepFrom = Block.SegmentStart;
    for (auto It = Windows.rbegin(); It != Windows.rend(); ++It)
    {
        if (!It->ReleasedAt || *It->ReleasedAt > KeepFrom)
        {
            const auto Original = ResolveSchedule(Record.Schedule.Person, Record.Schedule.Travel, It->RequestedAt);
            KeepFrom = std::min(KeepFrom, Original.SegmentStart);
        }
    }
    // Retain the dependency chain too: a spanning released window still needs its original safe-point history.
    Windows.erase(std::remove_if(Windows.begin(), Windows.end(), [KeepFrom](const NpcInterruptionWindow& Window)
    {
        return Window.ReleasedAt && *Window.ReleasedAt <= KeepFrom;
    }), Windows.end());
    const bool bWantsWindow = Record.bR55ChatRequested || Record.bR55RouteBlocked || Record.bR55DoorBlocked;
    const auto Desired = Record.bR55ChatRequested ? NpcState::Chat : NpcState::Yield;
    if (!Windows.empty() && !Windows.back().ReleasedAt && (!bWantsWindow || Windows.back().State != Desired))
    {
        Windows.back().ReleasedAt = Now;
    }
    if (bWantsWindow && (Windows.empty() || Windows.back().ReleasedAt))
    {
        if (Windows.size() >= NpcMaxInterruptionWindows)
        {
            Record.R55Decision = {};
            Record.R55Decision.Error = NpcIntentError::InvalidInterruption;
            return false; // Never discard unfinished-block pauses to make room.
        }
        Windows.push_back({Desired, Now, std::nullopt});
    }
    Record.R55Decision = ResolveNpcDecisionAt(Record.Schedule.Person, Record.Schedule.Travel, Now, Windows);
    return Record.R55Decision.Ok();
}

const PerfectWorld::NpcDecision* UPWNpcPresenceSubsystem::FindPilotDecision(const FName PersonId) const
{
    const auto* Record = People.Find(PersonId);
    return R55IsWorkPilot(PersonId) && Record != nullptr && (*Record)->bIntentValid ? &(*Record)->R55Decision : nullptr;
}

bool UPWNpcPresenceSubsystem::RequestPilotChat(const FName PersonId)
{
    const auto* Entry = People.Find(PersonId);
    if (!R55IsWorkPilot(PersonId) || Entry == nullptr || !bHasNow) { return false; }
    FNpPresenceRecord& Record = **Entry;
    if (!Record.bIntentValid || !Record.Intent.bTalkable) { return false; }
    Record.bR55ChatRequested = true;
    Reconcile(Now);
    if (!Record.R55Decision.Ok())
    {
        ReleasePilotChat(PersonId);
        return false;
    }
    return true;
}

void UPWNpcPresenceSubsystem::ReleasePilotChat(const FName PersonId)
{
    const auto* Entry = People.Find(PersonId);
    if (!R55IsWorkPilot(PersonId) || Entry == nullptr) { return; }
    FNpPresenceRecord& Record = **Entry;
    Record.bR55ChatRequested = false;
    Record.bR55TalkPending = false;
    Record.bR55PanelSeen = false;
    Record.R55TalkPawn.Reset();
    if (auto* Actor = Record.Actor.Get()) { Actor->GetNpcMovement()->SetLocalConversationActive(false); }
    Reconcile(Now);
}

void UPWNpcPresenceSubsystem::ReportPilotObstruction(const FName PersonId, const bool bBlocked, const bool bDoorway)
{
    const auto* Entry = People.Find(PersonId);
    if (!R55IsWorkPilot(PersonId) || Entry == nullptr) { return; }
    FNpPresenceRecord& Record = **Entry;
    bool& Flag = bDoorway ? Record.bR55DoorBlocked : Record.bR55RouteBlocked;
    if (Flag == bBlocked) { return; }
    Flag = bBlocked;
    Reconcile(Now);
}

bool UPWNpcPresenceSubsystem::RequestPilotTalk(const FName PersonId, APawn* By)
{
    if (!R55IsWorkPilot(PersonId) || !People.Contains(PersonId)) { return false; }
    const auto* R55Presenter = UPWConversationPresenter::Get(this);
    if (R55Presenter == nullptr || R55Presenter->IsOpen()) { return true; }
    if (auto* Life = UPWLaneLifeSubsystem::Get(this)) { Reconcile(Life->GetNow()); }
    if (!RequestPilotChat(PersonId))
    {
        if (auto* Life = UPWLaneLifeSubsystem::Get(this))
        {
            const auto* Intent = FindIntent(PersonId);
            const FText Reason = Intent != nullptr ? R55NpcReasonText(FName(UTF8_TO_TCHAR(std::string(Intent->DeclineReasonKey).c_str()))) : FText();
            Life->ShowPlayerNotice(Reason.IsEmpty() ? NSLOCTEXT("PerfectWorldWork", "Unavailable", "Please try again later.") : Reason, By);
        }
        return true;
    }
    FNpPresenceRecord& Record = *People[PersonId];
    Record.R55TalkPawn = By;
    Record.bR55TalkPending = true;
    if (By != nullptr)
    {
        if (auto* Interaction = By->FindComponentByClass<UPWInteractionComponent>())
        {
            Interaction->ShowNotice(R55NpcReasonText(FName(UTF8_TO_TCHAR(std::string(PerfectWorld::NpcDecisionReasonKey(Record.R55Decision.Reason)).c_str()))), 4.0f);
        }
    }
    TickPilotTalk(PersonId, Record);
    return true;
}

void UPWNpcPresenceSubsystem::TickPilotTalk(const FName PersonId, FNpPresenceRecord& Record)
{
    auto* Presenter = UPWConversationPresenter::Get(this);
    if (Record.bR55PanelSeen && (Presenter == nullptr || !Presenter->IsOpen()))
    {
        ReleasePilotChat(PersonId); // Also covers choices which finish without Leave.
        return;
    }
    if (!Record.bR55TalkPending) { return; }
    APawn* By = Record.R55TalkPawn.Get();
    APWNpcCharacter* Actor = Record.Actor.Get();
    if (By == nullptr || Actor == nullptr || Presenter == nullptr ||
        FVector::DistSquared2D(By->GetActorLocation(), Actor->GetActorLocation()) >
            FMath::Square(PWInteraction::ReachCm + PWInteraction::ReachHysteresisCm))
    {
        ReleasePilotChat(PersonId); // Walking away cancels a pending request.
        return;
    }
    if (!Record.R55Decision.Ok() || !Record.Intent.bTalkable)
    {
        if (auto* Life = UPWLaneLifeSubsystem::Get(this))
            Life->ShowPlayerNotice(R55NpcReasonText(FName(UTF8_TO_TCHAR(std::string(PerfectWorld::NpcDecisionReasonKey(Record.R55Decision.Reason)).c_str()))), By);
        ReleasePilotChat(PersonId);
        return;
    }
    if (Record.R55Decision.DeferredUntil || Record.Intent.State != PerfectWorld::NpcState::Chat) { return; }
    Record.bR55TalkPending = false; // The session factory reconciles presence; do not open recursively.
    if (Presenter->IsOpen()) { ReleasePilotChat(PersonId); return; }
    if (Presenter->BeginConversationWith(PersonId, Record.DisplayName))
    {
        Record.bR55PanelSeen = true;
        Actor->GetNpcMovement()->SetLocalConversationActive(true);
    }
    else
    {
        if (auto* Life = UPWLaneLifeSubsystem::Get(this)) { Life->LeaveConversation(PersonId); }
        ReleasePilotChat(PersonId);
    }
}
