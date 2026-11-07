#include "PerfectWorldAwakeningActors.h"

#include "PerfectWorldAwakeningCatalogue.h"

// What the floor does with a pad: forward the choice, show the answer, move the blocks that make it visible, and the gentle resets.
// Nothing here judges a choice: acceptance, phase, assistance and completion all come from the bridge's answer.

#define LOCTEXT_NAMESPACE "PerfectWorldAwakeningChoice"

using namespace PWAwakening;

int32 APWAwakeningFloorActor::GetPadPriority(const FAwPadSpec& Pad) const
{
    switch (Pad.Kind)
    {
    case EAwPadKind::Enter:
    case EAwPadKind::Leave:
        return 24; // people are 20
    case EAwPadKind::Choice:
    case EAwPadKind::Withdraw:
        return 22;
    case EAwPadKind::Advance:
    case EAwPadKind::Reset:
        return 21;
    }
    return 21;
}

FText APWAwakeningFloorActor::GetPadVerb(const FAwPadSpec& Pad) const
{
    if (IsTransitioning())
    {
        return FText::GetEmpty();
    }
    switch (Pad.Kind)
    {
    case EAwPadKind::Enter:
        return bOccupied ? FText::GetEmpty() : LOCTEXT("VerbEnter", "Enter the virtual floor");
    case EAwPadKind::Leave:
        return bOccupied ? LOCTEXT("VerbLeave", "Return to the campus") : FText::GetEmpty();
    default:
        break;
    }
    if (!bOccupied)
    {
        return FText::GetEmpty();
    }
    switch (Pad.Kind)
    {
    case EAwPadKind::Choice:
    case EAwPadKind::Withdraw:
    {
        // The withdrawal is offered in every phase: it is never hidden, locked or conditional.
        const FAwChoiceDef* Def = AwFindChoice(Pad.Situation, Pad.Choice);
        return Def != nullptr ? Def->Verb : FText::GetEmpty();
    }
    case EAwPadKind::Advance:
        return LOCTEXT("VerbAdvance", "Advance this station's moving parts (announced first)");
    case EAwPadKind::Reset:
        return LOCTEXT("VerbReset", "Reset this station (nothing is lost)");
    default:
        break;
    }
    return FText::GetEmpty();
}

void APWAwakeningFloorActor::HandlePad(const FAwPadSpec& Pad, APawn* By)
{
    if (IsTransitioning())
    {
        return;
    }
    switch (Pad.Kind)
    {
    case EAwPadKind::Enter:
        TryEnter(By);
        break;
    case EAwPadKind::Leave:
        TryLeave(By);
        break;
    case EAwPadKind::Choice:
    case EAwPadKind::Withdraw:
        if (bOccupied)
        {
            SubmitChoice(By, Pad.StationIndex, Pad.Choice);
        }
        break;
    case EAwPadKind::Advance:
        if (bOccupied)
        {
            AdvanceStation(Pad.StationIndex);
        }
        break;
    case EAwPadKind::Reset:
        if (bOccupied)
        {
            ResetStation(Pad.StationIndex, By);
        }
        break;
    }
}

void APWAwakeningFloorActor::ApplyEffect(const int32 Station, const FAwEffect& Effect)
{
    APWAwakeningMoverActor* Primary = GetMover(Station, false);
    APWAwakeningMoverActor* Secondary = GetMover(Station, true);
    if (Primary != nullptr)
    {
        if (Effect.bHoldPrimary)
        {
            Primary->SetHold(true);
        }
        if (Effect.PrimaryPose >= 0)
        {
            Primary->GoTo(Effect.PrimaryPose);
        }
    }
    if (Secondary != nullptr && Effect.SecondaryPose >= 0)
    {
        Secondary->GoTo(Effect.SecondaryPose);
        if (Secondary->GetSpec().bFlicker && Effect.SecondaryPose >= 1)
        {
            Secondary->SetFlickering(false);
        }
    }
}

void APWAwakeningFloorActor::ResetMovers(const int32 Station)
{
    for (const bool bSecondary : {false, true})
    {
        if (APWAwakeningMoverActor* Mover = GetMover(Station, bSecondary))
        {
            Mover->SetHold(false);
            Mover->SnapTo(Mover->GetSpec().StartPose);
            Mover->SetFlickering(Mover->GetSpec().bFlicker);
        }
    }
}

FAwChoiceResult APWAwakeningFloorActor::SubmitChoice(APawn* By, const int32 Station, const FName Choice)
{
    FAwChoiceResult Result;
    if (!Layout.Stations.IsValidIndex(Station))
    {
        Result.Code = TEXT("awakening.floor.no_station");
        LastCode = Result.Code;
        return Result;
    }
    const FAwStationSpec& S = Layout.Stations[Station];
    const FAwChoiceDef* Def = AwFindChoice(S.Situation, Choice);
    const bool bWithdraw = Choice == AwWithdrawDef().Id;
    const TSharedPtr<IAwFloorBridge> Bridge = AwBridge::Resolve(GetWorld());
    if (Bridge.IsValid())
    {
        Result = Bridge->SubmitChoice(S.Situation, Choice); // the one place a choice is decided: the core, through the bridge
    }
    else
    {
        Result.Code = TEXT("awakening.floor.no_bridge");
    }
    LastCode = Result.Code;
    if (Bridge.IsValid())
    {
        Phases[Station] = Result.Phase;
        bSessionComplete = bSessionComplete || Result.bSessionComplete;
        RefreshStatusSign(Station);
    }

    FText Caption;
    if (!Result.Message.IsEmpty())
    {
        Caption = FText::FromString(Result.Message);
    }
    else if (Result.bAccepted && Def != nullptr)
    {
        Caption = Def->Caption;
    }
    else
    {
        Caption = LOCTEXT("NotOpen", "That is not open right now. You can ask for help or withdraw to the safe area at any time.");
    }
    if (Result.bAccepted && Def != nullptr)
    {
        ApplyEffect(Station, Def->Effect);
    }
    if (Result.bSessionComplete)
    {
        Caption = FText::Format(LOCTEXT("AllDone", "{0}\nAll three situations are done. You can return to the campus from the return pad."), Caption);
    }

    if (bWithdraw)
    {
        // Safety is never conditional: the move to the safe area happens whatever the bridge answered.
        StartTransfer(Caption, [this, Weak = TWeakObjectPtr<APawn>(By)]() { Place(Weak.Get(), Layout.SafeStand, Layout.SafeYaw); });
    }
    else if (Result.bAccepted && Result.bAssistance)
    {
        // A mistake is met with help and a gentle reset, never failure.
        const FText Help = FText::Format(LOCTEXT("Assisted", "{0}: Let's reset that together. Nothing is lost."), Config.Cast.TeacherName);
        RecoverMove(By, Station, FText::Format(LOCTEXT("TwoLines", "{0}\n{1}"), Caption, Help));
        ResetMovers(Station);
    }
    else
    {
        Announce(Caption);
    }
    return Result;
}

void APWAwakeningFloorActor::RecoverMove(APawn* By, const int32 Station, const FText& Caption)
{
    if (!Layout.Stations.IsValidIndex(Station) || IsTransitioning())
    {
        return;
    }
    const FVector Feet = Layout.Stations[Station].ResetPad;
    StartTransfer(Caption, [this, Weak = TWeakObjectPtr<APawn>(By), Feet]() { Place(Weak.Get(), Feet, 90.0); });
}

void APWAwakeningFloorActor::Recover(APawn* By, const int32 Station, const FName Reason, const bool bTellBridge)
{
    if (!Layout.Stations.IsValidIndex(Station) || IsTransitioning())
    {
        return;
    }
    if (bTellBridge)
    {
        const TSharedPtr<IAwFloorBridge> Bridge = AwBridge::Resolve(GetWorld());
        if (Bridge.IsValid())
        {
            Bridge->NoteAssistance(Layout.Stations[Station].Situation, Reason); // informational; the core decides what, if anything, it means
        }
    }
    RecoverMove(By, Station, FText::Format(LOCTEXT("Recover", "{0}: Let's reset that together. Nothing is lost."), Config.Cast.TeacherName));
}

void APWAwakeningFloorActor::AdvanceStation(const int32 Station)
{
    APWAwakeningMoverActor* Primary = GetMover(Station, false);
    if (Primary == nullptr)
    {
        return;
    }
    // Presentation only: this never reaches the bridge, and it is the untimed way to see a station's parts move.
    const bool bReduced = IsReducedMotion();
    FText Text;
    switch (Station)
    {
    case 0:
        Text = bReduced ? LOCTEXT("Adv0r", "The platform will step to its other position in a moment.") : LOCTEXT("Adv0", "The platform moves to its other position.");
        break;
    case 1:
        Text = bReduced ? LOCTEXT("Adv1r", "The wall will shift to its other position in a moment.") : LOCTEXT("Adv1", "The wall shifts to its other position.");
        break;
    default:
        Text = bReduced ? LOCTEXT("Adv2r", "The crates will move to their other position in a moment.") : LOCTEXT("Adv2", "The crates move to their other position.");
        break;
    }
    Primary->StepNext();
    Announce(Text);
}

void APWAwakeningFloorActor::ResetStation(const int32 Station, APawn* By)
{
    if (!Layout.Stations.IsValidIndex(Station) || IsTransitioning())
    {
        return;
    }
    ResetMovers(Station);
    const TSharedPtr<IAwFloorBridge> Bridge = AwBridge::Resolve(GetWorld());
    if (Bridge.IsValid())
    {
        Bridge->NoteAssistance(Layout.Stations[Station].Situation, FName(Ids::ReasonResetRequested));
    }
    RecoverMove(By, Station, FText::Format(LOCTEXT("ResetDone", "{0}: The station is reset. Nothing is lost."), Config.Cast.TeacherName));
}

#undef LOCTEXT_NAMESPACE
