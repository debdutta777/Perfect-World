// R9-06 Awakening floor in a throw-away world: the spawn, every choice reaching the bridge, the floor deciding nothing itself,
// withdrawal in every phase, gentle assistance and recovery, the refused entry and the text scale.
// Run in the editor under "PerfectWorld.AwakeningFloor" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldAwakeningTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/Character.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldShopDisplayActor.h"
#include "TutorialDungeon.h"

using namespace PWAwakeningTest;

namespace
{
APWAwakeningPadActor* AwTPad(const FAwTestFloor& T, const int32 Station, const int32 Index)
{
    return T.Floor()->FindPad(T.Floor()->GetLayout().Stations[Station].Pads[Index].Id);
}

/** P313: a loaded session whose first exit is refused, then accepted by its supervisor. */
class FP313ExitBridge final : public IAwFloorBridge
{
public:
    FAwFloorStatus Status;
    bool bRefuseExit = true;
    int32 Leaves = 0;
    int32 Withdrawals = 0;

    FP313ExitBridge()
    {
        Status.bSessionOpen = true;
        for (auto& Phase : Status.Phase) Phase = EAwPhase::Complete;
    }
    virtual bool CanEnter(FString&) override { return true; }
    virtual FAwEnterResult Enter() override
    {
        FAwEnterResult Result;
        Result.bAccepted = true;
        Result.bResumed = true;
        return Result;
    }
    virtual FAwFloorStatus GetStatus() override { return Status; }
    virtual FAwChoiceResult SubmitChoice(FName, FName Choice) override
    {
        FAwChoiceResult Result;
        if (Choice != AwWithdrawDef().Id) return Result;
        ++Withdrawals;
        if (!bRefuseExit)
        {
            Status.bSessionOpen = false;
            Status.bSessionComplete = true;
            Result.bAccepted = true;
            Result.bSessionComplete = true;
        }
        return Result;
    }
    virtual void NoteAssistance(FName, FName) override {}
    virtual void Leave() override
    {
        ++Leaves;
        if (!bRefuseExit)
        {
            Status.bSessionOpen = false;
            Status.bSessionComplete = true;
        }
    }
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwSpawnTest, "PerfectWorld.AwakeningFloor.Spawn",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwSpawnTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig()))
    {
        return false;
    }
    const FAwFloorLayout& L = T.Floor()->GetLayout();
    TestEqual(TEXT("every part is an instance"), T.Spawned.Instances, L.Parts.Num());
    TestTrue(TEXT("the blocks actor carries its tag"), T.Spawned.BlocksActor != nullptr && T.Spawned.BlocksActor->ActorHasTag(AwBlocksActorTag));
    TestEqual(TEXT("eighteen choice pads plus the terminal and the return pad"), T.Spawned.Pads, 20);
    TestEqual(TEXT("two movers per station"), T.Spawned.Movers, 6);
    TestEqual(TEXT("labels, signs and one status board per station"), T.Spawned.TextSigns, L.Texts.Num() + SituationCount);
    TestEqual(TEXT("every light"), T.Spawned.Lights, L.Lights.Num());
    TestEqual(TEXT("people only appear when someone steps on the floor"), T.Floor()->GetFigureCount(), 0);

    // Stations at their authored positions: the pads, the movers and the status boards.
    for (int32 S = 0; S < L.Stations.Num(); ++S)
    {
        const FAwStationSpec& Station = L.Stations[S];
        for (const FAwPadSpec& Spec : Station.Pads)
        {
            const APWAwakeningPadActor* Pad = T.Floor()->FindPad(Spec.Id);
            if (TestNotNull(*FString::Printf(TEXT("pad %s exists"), *Spec.Id.ToString()), Pad))
            {
                TestTrue(TEXT("at its authored point"), Pad->GetInteractionPoint().Equals(Spec.Point, 0.1));
                TestTrue(TEXT("in front of people in priority"), Pad->GetInteractionPriority() > 20);
            }
        }
        for (const bool bSecondary : {false, true})
        {
            const APWAwakeningMoverActor* Mover = T.Floor()->GetMover(S, bSecondary);
            const FAwMoverSpec& Spec = bSecondary ? Station.Secondary : Station.Primary;
            if (TestNotNull(TEXT("a mover"), Mover))
            {
                TestTrue(TEXT("at its start pose"), Mover->GetCentre().Equals(Spec.Poses[Spec.StartPose].Centre, 0.1));
                TestEqual(TEXT("start pose reached"), Mover->GetPose(), Spec.StartPose);
            }
        }
        const APWShopHoursSignActor* Status = T.Floor()->GetStatusSign(S);
        if (TestNotNull(TEXT("a status board"), Status))
        {
            TestEqual(TEXT("it starts open"), Status->GetPostedText(), FString(TEXT("[ ] OPEN")));
        }
    }
    const APWAwakeningPadActor* Enter = T.Floor()->FindPad(FName(TEXT("pad.entry")));
    if (TestNotNull(TEXT("the campus terminal"), Enter))
    {
        TestTrue(TEXT("entry discloses its appointment and session duration"),
            Enter->GetInteractionVerb(nullptr).ToString().StartsWith(TEXT("Enter the virtual floor (scheduled appointment; ")) &&
            Enter->GetInteractionVerb(nullptr).ToString().Contains(FString::Printf(TEXT("%lld minutes"), static_cast<long long>(PerfectWorld::TutorialSessionMinutes))));
        TestTrue(TEXT("it stands at the forecourt terminal"), Enter->GetInteractionPoint().Equals(L.EntryPad.Point, 0.1));
    }
    TestTrue(TEXT("choice pads are silent until someone is on the floor"), AwTPad(T, 0, 0)->GetInteractionVerb(nullptr).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwChoicesReachBridgeTest, "PerfectWorld.AwakeningFloor.EveryChoiceReachesTheBridge",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwChoicesReachBridgeTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig()))
    {
        return false;
    }
    APWAwakeningFloorActor* Floor = T.Floor();
    TestTrue(TEXT("entering is accepted"), Floor->TryEnter(nullptr));
    TestTrue(TEXT("the floor is occupied"), Floor->IsOccupied());
    TestEqual(TEXT("the bridge saw one entry"), T.Bridge->CountCalls(TEXT("enter")), 1);
    TestEqual(TEXT("the teacher, two classmates and one classmate per station"), Floor->GetFigureCount(), 6);

    for (int32 S = 0; S < SituationCount; ++S)
    {
        const FAwStationSpec& Station = Floor->GetLayout().Stations[S];
        for (int32 K = 0; K < 4; ++K) // three choices and the withdrawal
        {
            const FAwPadSpec& Spec = Station.Pads[K];
            APWAwakeningPadActor* Pad = AwTPad(T, S, K);
            if (!TestNotNull(TEXT("pad"), Pad))
            {
                continue;
            }
            TestFalse(*FString::Printf(TEXT("%s has a prompt verb on the floor"), *Spec.Id.ToString()), Pad->GetInteractionVerb(nullptr).IsEmpty());
            const int32 Before = T.Bridge->CountSubmits(Station.Situation, Spec.Choice);
            Pad->Interact(nullptr);
            TestEqual(*FString::Printf(TEXT("%s reached the bridge exactly once"), *Spec.Id.ToString()), T.Bridge->CountSubmits(Station.Situation, Spec.Choice), Before + 1);
        }
    }
    int32 Submits = 0;
    for (const FAwScriptedBridge::FCall& Call : T.Bridge->GetCalls())
    {
        Submits += Call.Kind == TEXT("submit") ? 1 : 0;
    }
    TestEqual(TEXT("twelve forwarded choices, nothing else"), Submits, 12);
    TestEqual(TEXT("no assistance was invented"), T.Bridge->CountCalls(TEXT("assist")), 0);
    TestTrue(TEXT("the session is complete by the bridge's say-so"), Floor->IsSessionComplete());
    TestFalse(TEXT("the floor does not leave by itself"), !Floor->IsOccupied());

    // The utility pads never reach the bridge as choices.
    const int32 Calls = T.Bridge->GetCalls().Num();
    AwTPad(T, 0, 4)->Interact(nullptr); // advance
    TestEqual(TEXT("advance forwards nothing"), T.Bridge->GetCalls().Num(), Calls);
    AwTPad(T, 0, 5)->Interact(nullptr); // reset: informational assistance only
    TestEqual(TEXT("reset only informs"), T.Bridge->CountCalls(TEXT("submit")), 12);
    TestEqual(TEXT("... through the assistance call"), T.Bridge->CountCalls(TEXT("assist")), 1);

    const FAwPadSpec LeaveSpec = Floor->GetLayout().LeavePad;
    Floor->FindPad(LeaveSpec.Id)->Interact(nullptr);
    TestFalse(TEXT("the return pad leaves"), Floor->IsOccupied());
    TestEqual(TEXT("the bridge saw one leave"), T.Bridge->CountCalls(TEXT("leave")), 1);
    TestEqual(TEXT("the people go with the session"), Floor->GetFigureCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwEffectsTest, "PerfectWorld.AwakeningFloor.ConsequencesAreObservable",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwEffectsTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig()))
    {
        return false;
    }
    APWAwakeningFloorActor* Floor = T.Floor();
    Floor->TryEnter(nullptr);
    for (int32 S = 0; S < SituationCount; ++S)
    {
        for (int32 K = 0; K < 3; ++K)
        {
            const FAwPadSpec& Spec = Floor->GetLayout().Stations[S].Pads[K];
            const FAwChoiceDef* Def = AwFindChoice(Spec.Situation, Spec.Choice);
            if (!TestNotNull(TEXT("the choice is catalogued"), Def))
            {
                continue;
            }
            T.Bridge->ResetSession();
            T.Bridge->Enter();
            AwTPad(T, S, 5)->Interact(nullptr); // the station's reset pad returns every moving part to its start
            AwTPad(T, S, K)->Interact(nullptr);
            const APWAwakeningMoverActor* Primary = Floor->GetMover(S, false);
            const APWAwakeningMoverActor* Secondary = Floor->GetMover(S, true);
            const FString What = Spec.Id.ToString();
            if (Def->Effect.PrimaryPose >= 0)
            {
                TestEqual(*(What + TEXT(": the primary part is ordered to its pose")), Primary->GetTargetPose(), Def->Effect.PrimaryPose);
            }
            if (Def->Effect.SecondaryPose >= 0)
            {
                TestEqual(*(What + TEXT(": the secondary part is ordered to its pose")), Secondary->GetTargetPose(), Def->Effect.SecondaryPose);
            }
            TestEqual(*(What + TEXT(": a held part stays put")), Primary->IsHeld(), Def->Effect.bHoldPrimary);
            TestEqual(*(What + TEXT(": the board reads done")), Floor->GetStatusSign(S)->GetPostedText(), FString(TEXT("[#] DONE")));
            TestFalse(*(What + TEXT(": a caption says what happened")), Floor->GetLastCaption().IsEmpty());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwFloorDecidesNothingTest, "PerfectWorld.AwakeningFloor.TheFloorDecidesNothing",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwFloorDecidesNothingTest::RunTest(const FString& Parameters)
{
    FAwTestWorld Holder;
    const FAwSpawnResult Spawned = SpawnAwakeningFloor(*Holder.World, AwTConfig());
    if (!TestTrue(TEXT("spawned"), Spawned.bBuilt))
    {
        return false;
    }
    APWAwakeningFloorActor* Floor = Spawned.Floor;

    // No bridge: entering is politely refused and nothing moves.
    TestFalse(TEXT("no bridge: no entry"), Floor->TryEnter(nullptr));
    TestEqual(TEXT("with a stable code"), Floor->GetLastCode(), FString(TEXT("awakening.floor.no_bridge")));
    TestFalse(TEXT("still off the floor"), Floor->IsOccupied());
    TestFalse(TEXT("and a sentence says so"), Floor->GetLastCaption().IsEmpty());

    // A bridge that says no to everything: the floor shows its words and changes nothing.
    const TSharedRef<FAwRefusingBridge> Refusing = MakeShared<FAwRefusingBridge>();
    AwBridge::Install(Holder.World, Refusing);
    TestTrue(TEXT("entry is the bridge's call"), Floor->TryEnter(nullptr));
    const FAwPadSpec Help = Floor->GetLayout().Stations[0].Pads[0];
    Floor->FindPad(Help.Id)->Interact(nullptr);
    TestEqual(TEXT("the choice was forwarded"), Refusing->Submits, 1);
    TestEqual(TEXT("the caption is the core's own"), Floor->GetLastCaption().ToString(), FString(TEXT("The core says no.")));
    TestEqual(TEXT("the platform did not move"), Floor->GetMover(0, false)->GetTargetPose(), Floor->GetLayout().Stations[0].Primary.StartPose);
    TestTrue(TEXT("the phase did not change"), Floor->GetPhase(0) == EAwPhase::Available);
    TestFalse(TEXT("the session is not complete"), Floor->IsSessionComplete());
    TestEqual(TEXT("the board still reads open"), Floor->GetStatusSign(0)->GetPostedText(), FString(TEXT("[ ] OPEN")));

    // The withdrawal is forwarded too, and is refused only by the bridge: the floor never hides it.
    const FAwPadSpec Withdraw = Floor->GetLayout().Stations[1].Pads[3];
    TestFalse(TEXT("the withdrawal is offered"), Floor->FindPad(Withdraw.Id)->GetInteractionVerb(nullptr).IsEmpty());
    Floor->FindPad(Withdraw.Id)->Interact(nullptr);
    TestEqual(TEXT("the withdrawal reached the bridge"), Refusing->Submits, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwWithdrawAlwaysTest, "PerfectWorld.AwakeningFloor.WithdrawalAlwaysAvailable",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwWithdrawAlwaysTest::RunTest(const FString& Parameters)
{
    for (const EAwPhase Phase : {EAwPhase::Locked, EAwPhase::Available, EAwPhase::Active, EAwPhase::Recovering, EAwPhase::Complete})
    {
        FAwTestFloor T;
        if (!T.Init(*this, AwTConfig()))
        {
            return false;
        }
        for (int32 S = 0; S < SituationCount; ++S)
        {
            T.Bridge->SetPhase(S, Phase);
        }
        T.Floor()->TryEnter(nullptr);
        for (int32 S = 0; S < SituationCount; ++S)
        {
            const APWAwakeningPadActor* Pad = AwTPad(T, S, 3);
            TestFalse(*FString::Printf(TEXT("withdrawal at station %d is offered while the situation is phase %d"), S + 1, static_cast<int32>(Phase)),
                      Pad->GetInteractionVerb(nullptr).IsEmpty());
            TestEqual(TEXT("its words"), Pad->GetInteractionVerb(nullptr).ToString(), FString(TEXT("Withdraw to the safe area")));
        }
    }

    // After the whole session is complete, and after a refusal, the pad is still there.
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig()))
    {
        return false;
    }
    T.Floor()->TryEnter(nullptr);
    for (int32 S = 0; S < SituationCount; ++S)
    {
        AwTPad(T, S, 0)->Interact(nullptr);
    }
    TestTrue(TEXT("session complete"), T.Floor()->IsSessionComplete());
    TestFalse(TEXT("withdrawal is still offered"), AwTPad(T, 2, 3)->GetInteractionVerb(nullptr).IsEmpty());

    // A real pawn: withdrawing lands in the safe area, whatever the bridge answered.
    FAwTestFloor U;
    if (!U.Init(*this, AwTConfig()))
    {
        return false;
    }
    ACharacter* Pawn = U.Holder.World->SpawnActor<ACharacter>(U.Floor()->GetLayout().Arrival + FVector(0.0, 0.0, 120.0), FRotator::ZeroRotator);
    if (TestNotNull(TEXT("a test pawn"), Pawn))
    {
        U.Floor()->TryEnter(Pawn);
        AwTPad(U, 1, 3)->Interact(Pawn);
        const FAwFloorLayout& L = U.Floor()->GetLayout();
        TestTrue(TEXT("the pawn stands in the safe area after withdrawing"), L.SafeArea.ExpandBy(FVector(10.0, 10.0, 400.0)).IsInside(Pawn->GetActorLocation()));
        TestTrue(TEXT("at the authored safe stand"), FVector::Dist2D(Pawn->GetActorLocation(), L.SafeStand) < 5.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwAssistanceTest, "PerfectWorld.AwakeningFloor.MistakesAreMetWithHelp",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwAssistanceTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig()))
    {
        return false;
    }
    APWAwakeningFloorActor* Floor = T.Floor();
    Floor->TryEnter(nullptr);

    // The supervisor steps in on the bridge's say-so: help, a reset, the situation stays open. No failure state exists to enter.
    T.Bridge->QueueAssistance(FName(Ids::ShiftingCorridor));
    AwTPad(T, 1, 1)->Interact(nullptr); // brace
    TestTrue(TEXT("the situation is recovering, not failed"), Floor->GetPhase(1) == EAwPhase::Recovering);
    TestTrue(TEXT("the caption offers help"), Floor->GetLastCaption().ToString().Contains(TEXT("reset that together")));
    TestEqual(TEXT("the board says to try again"), Floor->GetStatusSign(1)->GetPostedText(), FString(TEXT("[~] RESET WITH HELP - TRY AGAIN")));
    TestFalse(TEXT("the held wall is released for another attempt"), Floor->GetMover(1, false)->IsHeld());
    TestFalse(TEXT("the session is not over"), Floor->IsSessionComplete());
    AwTPad(T, 1, 1)->Interact(nullptr);
    TestTrue(TEXT("the second attempt completes it"), Floor->GetPhase(1) == EAwPhase::Complete);

    // Leaving the route is a gentle reset: the bridge is told (informational), the player is asked to try again.
    const int32 Before = T.Bridge->CountCalls(TEXT("assist"));
    const FVector InGap = Floor->GetLayout().Stations[0].Hazard.GetCenter() - FVector(0.0, 0.0, 2.0);
    TestTrue(TEXT("with the platform away, the gap is a hazard"), Floor->IsInHazard(0, InGap));
    const FBox& Aligned = Floor->GetLayout().Stations[0].SafeWhenAligned;
    const FVector OnPlatform(Aligned.GetCenter().X, Aligned.GetCenter().Y, Floor->GetLayout().Stations[0].Hazard.GetCenter().Z); // feet at hazard level
    Floor->GetMover(0, false)->SnapTo(0);
    TestFalse(TEXT("with the platform aligned, crossing it is safe"), Floor->IsInHazard(0, OnPlatform));
    Floor->GetMover(0, false)->SnapTo(1);
    TestTrue(TEXT("and away again it is not"), Floor->IsInHazard(0, OnPlatform));
    TestFalse(TEXT("the open floor is not a hazard"), Floor->IsInHazard(0, Floor->GetLayout().Stations[0].Stand));
    Floor->Recover(nullptr, 0, FName(Ids::ReasonLeftTheRoute), true);
    TestEqual(TEXT("the bridge was informed once"), T.Bridge->CountCalls(TEXT("assist")), Before + 1);
    TestTrue(TEXT("with a gentle caption"), Floor->GetLastCaption().ToString().Contains(TEXT("Nothing is lost")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwEntryRefusalTest, "PerfectWorld.AwakeningFloor.EntryAndTextScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwEntryRefusalTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig()))
    {
        return false;
    }
    APWAwakeningFloorActor* Floor = T.Floor();
    T.Bridge->SetEntryRefusal(TEXT("The session is not due yet."));
    TestFalse(TEXT("a refused entry does not move anyone"), Floor->TryEnter(nullptr));
    TestFalse(TEXT("still off the floor"), Floor->IsOccupied());
    TestEqual(TEXT("the core's words are shown"), Floor->GetLastCaption().ToString(), FString(TEXT("The session is not due yet.")));
    TestEqual(TEXT("the bridge was never asked to begin"), T.Bridge->CountCalls(TEXT("enter")), 0);
    TestEqual(TEXT("no people were spawned"), Floor->GetFigureCount(), 0);

    T.Bridge->SetEntryRefusal(FString());
    TestTrue(TEXT("once open, entry works"), Floor->TryEnter(nullptr));
    TestFalse(TEXT("a second entry is ignored"), Floor->TryEnter(nullptr));
    TestTrue(TEXT("the welcome names the supervisor"), Floor->GetLastCaption().ToString().Contains(TEXT("Ms Basu")));
    TestTrue(TEXT("and promises nothing here can hurt"), Floor->GetLastCaption().ToString().Contains(TEXT("nothing here can hurt you")));

    Floor->SetTextScale(1.5f);
    TestTrue(TEXT("text scale 150%"), FMath::IsNearlyEqual(Floor->GetHud().GetTextScale(), 1.5f));
    Floor->SetTextScale(5.0f);
    TestTrue(TEXT("text scale is capped at 200%"), FMath::IsNearlyEqual(Floor->GetHud().GetTextScale(), 2.0f));
    Floor->SetTextScale(0.2f);
    TestTrue(TEXT("and never below 100%"), FMath::IsNearlyEqual(Floor->GetHud().GetTextScale(), 1.0f));

    Floor->GetHud().SetCue(FText::FromString(TEXT("[!!!] GAP")));
    TestTrue(TEXT("a hazard cue is shape and words"), Floor->GetHud().GetCue().ToString().StartsWith(TEXT("[!!!]")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP310FloorTransferRecoveryTest, "PerfectWorld.AwakeningFloor.P310TransferRecovery",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FP310FloorTransferRecoveryTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig())) return false;
    const auto& L = T.Floor()->GetLayout();
    auto* Player = T.Holder.World->SpawnActor<APerfectWorldPlayerCharacter>(
        L.Site.EntryStand + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 3.0), FRotator(0.0, L.Site.EntryYaw, 0.0));
    if (!TestNotNull(TEXT("the real player character"), Player)) return false;
    // This world does not begin play; establish the campus recovery point that BeginPlay normally records.
    Player->P310RememberRecoveryAnchor();
    TestTrue(TEXT("terminal accepts entry"), T.Floor()->TryEnter(Player));
    for (int32 I = 0; I < 10; ++I) Player->Tick(0.1f);
    TestTrue(TEXT("the real player remains in the underground room after ticks"), L.Bounds.IsInside(Player->GetActorLocation()));

    // A genuine fall still recovers, now onto the current floor rather than back home.
    const FVector Arrival = Player->GetActorLocation();
    TestTrue(TEXT("a position below the room is available"), Player->TeleportTo(Arrival - FVector(0.0, 0.0, 3100.0), FRotator::ZeroRotator));
    Player->Tick(0.1f);
    TestTrue(TEXT("fall recovery returns to the room arrival"), Player->GetActorLocation().Equals(Arrival, 0.1));

    TestTrue(TEXT("campus return accepts leaving"), T.Floor()->TryLeave(Player));
    const FVector Campus = Player->GetActorLocation();
    TestTrue(TEXT("campus return reaches the terminal"), FVector::Dist2D(Campus, L.Site.EntryStand) < 5.0);
    TestTrue(TEXT("a position below campus is available"), Player->TeleportTo(Campus - FVector(0.0, 0.0, 3100.0), FRotator::ZeroRotator));
    Player->Tick(0.1f);
    TestTrue(TEXT("return transfer restored campus fall recovery"), Player->GetActorLocation().Equals(Campus, 0.1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP313FloorResumeTest, "PerfectWorld.P313.LoadedFloorRestoresWithdrawalAndReturn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP313FloorResumeTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig())) return false;
    // The session predates this director's occupancy, as it does after Continue.
    T.Bridge->Enter();
    T.Bridge->SetPhase(0, EAwPhase::Complete);
    T.Bridge->SetPhase(1, EAwPhase::Recovering);
    T.Bridge->SetPhase(2, EAwPhase::Locked);
    const auto& Layout = T.Floor()->GetLayout();
    auto* Pawn = T.Holder.World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("saved pawn"), Pawn)) return false;
    Pawn->SetActorLocation(Layout.Stations[1].Stand + FVector(0, 0, 90));
    TestTrue(TEXT("inside saved floor resumes presentation"), T.Floor()->P313ResumeAt(Pawn));
    TestEqual(TEXT("no second enter command"), T.Bridge->CountCalls(TEXT("enter")), 1);
    TestEqual(TEXT("completed conduct preserved"), static_cast<int32>(T.Floor()->GetPhase(0)), static_cast<int32>(EAwPhase::Complete));
    TestEqual(TEXT("pending recovery preserved"), static_cast<int32>(T.Floor()->GetPhase(1)), static_cast<int32>(EAwPhase::Recovering));
    TestFalse(TEXT("restoration happens once"), T.Floor()->P313ResumeAt(Pawn));
    for (int32 Station = 0; Station < SituationCount; ++Station)
    {
        const auto* Withdraw = AwTPad(T, Station, 3);
        TestFalse(TEXT("withdrawal offered in complete, recovering and locked phases"), Withdraw->GetInteractionVerb(Pawn).IsEmpty());
    }
    AwTPad(T, 2, 3)->Interact(Pawn);
    TestEqual(TEXT("locked station still forwards withdrawal"),
        T.Bridge->CountSubmits(Layout.Stations[2].Situation, AwWithdrawDef().Id), 1);
    const auto* Return = T.Floor()->FindPad(Layout.LeavePad.Id);
    TestTrue(TEXT("return explicitly offers withdrawal"), Return->GetInteractionVerb(Pawn).ToString().Contains(TEXT("Withdraw")));
    TestTrue(TEXT("return closes the bridge session"), T.Floor()->TryLeave(Pawn));
    TestFalse(TEXT("no open session remains"), T.Bridge->GetState().bSessionOpen);
    // A session loaded at campus can also be re-entered through the normal terminal.
    T.Bridge->Enter();
    TestTrue(TEXT("terminal advertises resume"), T.Floor()->GetPadVerb(Layout.EntryPad).ToString().Contains(TEXT("Resume")));
    TestTrue(TEXT("normal terminal entry resumes"), T.Floor()->TryEnter(Pawn));
    TestEqual(TEXT("existing completed phase survives re-entry"), static_cast<int32>(T.Floor()->GetPhase(0)), static_cast<int32>(EAwPhase::Complete));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP313FloorExitRetryTest, "PerfectWorld.P313.LoadedFloorKeepsItsExitAfterRefusal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP313FloorExitRetryTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTConfig())) return false;
    const auto Bridge = MakeShared<FP313ExitBridge>();
    AwBridge::Install(T.Holder.World, Bridge);
    auto* Pawn = T.Holder.World->SpawnActor<ACharacter>();
    if (!TestNotNull(TEXT("saved pawn"), Pawn)) return false;
    const auto& Layout = T.Floor()->GetLayout();
    Pawn->SetActorLocation(Layout.Site.EntryStand + FVector(0, 0, 90));
    TestFalse(TEXT("campus pawn is not restored inside the floor"), T.Floor()->P313ResumeAt(Pawn));
    TestFalse(TEXT("restoration needs a pawn"), T.Floor()->P313ResumeAt(nullptr));
    Pawn->SetActorLocation(Layout.Arrival + FVector(0, 0, 90));
    TestTrue(TEXT("loaded exit-ready presentation resumes"), T.Floor()->P313ResumeAt(Pawn));
    TestFalse(TEXT("refused return does not abandon an open session"), T.Floor()->TryLeave(Pawn));
    TestTrue(TEXT("floor retains occupancy for another exit attempt"), T.Floor()->IsOccupied());
    TestEqual(TEXT("one attempted return"), Bridge->Leaves, 1);
    for (int32 Station = 0; Station < SituationCount; ++Station)
        TestFalse(TEXT("completed stations still offer withdrawal after refusal"), AwTPad(T, Station, 3)->GetInteractionVerb(Pawn).IsEmpty());
    AwTPad(T, 0, 3)->Interact(Pawn);
    TestTrue(TEXT("refused withdrawal retains session"), Bridge->Status.bSessionOpen);
    TestTrue(TEXT("refused withdrawal still moves to safety"), FVector::Dist2D(Pawn->GetActorLocation(), Layout.SafeStand) < 5.0);
    Bridge->bRefuseExit = false;
    AwTPad(T, 0, 3)->Interact(Pawn);
    TestEqual(TEXT("withdrawal can be retried"), Bridge->Withdrawals, 2);
    TestFalse(TEXT("supervisor releases the session"), Bridge->Status.bSessionOpen);
    TestTrue(TEXT("completed session can return to campus"), T.Floor()->TryLeave(Pawn));
    TestFalse(TEXT("campus return clears occupancy"), T.Floor()->IsOccupied());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
