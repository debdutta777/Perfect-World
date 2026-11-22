#include "PerfectWorldAwakeningBridge.h"

#include "Engine/World.h"
#include "PerfectWorldAwakeningCatalogue.h"

namespace PWAwakening
{
namespace
{
TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IAwFloorBridge>>& AwFlRegistry()
{
    static TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IAwFloorBridge>> Registry;
    return Registry;
}

void AwFlPruneRegistry()
{
    for (auto It = AwFlRegistry().CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
}
} // namespace

namespace AwBridge
{
void Install(UWorld* World, TSharedPtr<IAwFloorBridge> Bridge)
{
    if (World == nullptr)
    {
        return;
    }
    AwFlPruneRegistry();
    if (Bridge.IsValid())
    {
        AwFlRegistry().Add(World, Bridge);
    }
    else
    {
        AwFlRegistry().Remove(World);
    }
}

void Remove(UWorld* World)
{
    if (World != nullptr)
    {
        AwFlRegistry().Remove(World);
    }
}

TSharedPtr<IAwFloorBridge> Resolve(UWorld* World)
{
    if (World == nullptr)
    {
        return nullptr;
    }
    const TSharedPtr<IAwFloorBridge>* Found = AwFlRegistry().Find(World);
    return Found != nullptr ? *Found : nullptr;
}
} // namespace AwBridge

// ---- The scripted fake ----

bool FAwScriptedBridge::CanEnter(FString& OutWhy)
{
    if (!EntryRefusal.IsEmpty())
    {
        OutWhy = EntryRefusal;
        return false;
    }
    return true;
}

FAwEnterResult FAwScriptedBridge::Enter()
{
    FCall Call;
    Call.Kind = TEXT("enter");
    Calls.Add(Call);
    FAwEnterResult Result;
    if (!EntryRefusal.IsEmpty())
    {
        Result.Code = TEXT("scripted.session.not_due");
        Result.Message = EntryRefusal;
        return Result;
    }
    Result.bAccepted = true;
    Result.Code = TEXT("scripted.session.begun");
    Result.bResumed = State.bSessionOpen;
    State.bSessionOpen = true;
    return Result;
}

FAwFloorStatus FAwScriptedBridge::GetStatus()
{
    return State;
}

FAwChoiceResult FAwScriptedBridge::SubmitChoice(const FName Situation, const FName Choice)
{
    FCall Call;
    Call.Kind = TEXT("submit");
    Call.Situation = Situation;
    Call.Choice = Choice;
    Calls.Add(Call);

    FAwChoiceResult Result;
    const int32 Index = AwSituationIndex(Situation);
    if (!State.bSessionOpen)
    {
        Result.Code = TEXT("scripted.choice.no_session");
        Result.Message = TEXT("The supervised session is not open.");
        return Result;
    }
    if (Index == INDEX_NONE || AwFindChoice(Situation, Choice) == nullptr)
    {
        Result.Code = TEXT("scripted.choice.unknown");
        Result.Message = TEXT("That is not offered here.");
        return Result;
    }
    Result.Phase = State.Phase[Index];
    if (State.Phase[Index] == EAwPhase::Complete)
    {
        Result.Code = TEXT("scripted.choice.already_done");
        Result.Message = TEXT("This situation is already done.");
        return Result;
    }
    Result.bAccepted = true;
    Result.Code = TEXT("scripted.choice.accepted");
    Result.Consequence = FName(*FString::Printf(TEXT("scripted.consequence.%s"), *Choice.ToString()));
    const bool bAssist = AssistQueue.Remove(Situation) > 0 && Choice != AwWithdrawDef().Id;
    if (bAssist)
    {
        Result.bAssistance = true;
        State.Phase[Index] = EAwPhase::Recovering;
    }
    else
    {
        State.Phase[Index] = EAwPhase::Complete;
    }
    Result.Phase = State.Phase[Index];
    bool bAll = true;
    for (const EAwPhase Phase : State.Phase)
    {
        bAll = bAll && Phase == EAwPhase::Complete;
    }
    State.bSessionComplete = bAll;
    Result.bSessionComplete = bAll;
    return Result;
}

void FAwScriptedBridge::NoteAssistance(const FName Situation, const FName Reason)
{
    FCall Call;
    Call.Kind = TEXT("assist");
    Call.Situation = Situation;
    Call.Choice = Reason;
    Calls.Add(Call);
}

void FAwScriptedBridge::Leave()
{
    FCall Call;
    Call.Kind = TEXT("leave");
    Calls.Add(Call);
    State.bSessionOpen = false;
}

void FAwScriptedBridge::ResetSession()
{
    State = FAwFloorStatus();
    AssistQueue.Reset();
}

void FAwScriptedBridge::SetPhase(const int32 SituationIndex, const EAwPhase Phase)
{
    if (SituationIndex >= 0 && SituationIndex < SituationCount)
    {
        State.Phase[SituationIndex] = Phase;
    }
}

int32 FAwScriptedBridge::CountCalls(const TCHAR* Kind) const
{
    int32 Count = 0;
    for (const FCall& Call : Calls)
    {
        Count += Call.Kind == Kind ? 1 : 0;
    }
    return Count;
}

int32 FAwScriptedBridge::CountSubmits(const FName Situation, const FName Choice) const
{
    int32 Count = 0;
    for (const FCall& Call : Calls)
    {
        Count += (Call.Kind == TEXT("submit") && Call.Situation == Situation && Call.Choice == Choice) ? 1 : 0;
    }
    return Count;
}
} // namespace PWAwakening
