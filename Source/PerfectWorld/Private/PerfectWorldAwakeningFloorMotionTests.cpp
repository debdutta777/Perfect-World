// R9-06 Awakening floor motion: moving parts glide normally and, under reduced motion, never glide, never cycle by themselves and never
// flicker; they are announced and jump once. Transfers fade normally and are an announced still cut under reduced motion.
// Run in the editor under "PerfectWorld.AwakeningFloor" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldAwakeningTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PWAwakeningTest;

namespace
{
FAwFloorConfig AwTMotionConfig(const bool bReduced)
{
    FAwFloorConfig Config = AwTConfig();
    Config.ReducedMotion = bReduced;
    return Config;
}

APWAwakeningPadActor* AwTMotionPad(const FAwTestFloor& T, const int32 Station, const int32 Index)
{
    return T.Floor()->FindPad(T.Floor()->GetLayout().Stations[Station].Pads[Index].Id);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwReducedMoversTest, "PerfectWorld.AwakeningFloor.ReducedMotionMovers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwReducedMoversTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTMotionConfig(true)))
    {
        return false;
    }
    APWAwakeningMoverActor* Wall = T.Floor()->GetMover(1, false); // the shifting wall: cycles by itself in normal motion
    if (!TestNotNull(TEXT("the wall"), Wall))
    {
        return false;
    }
    const FVector Start = Wall->GetSpec().Poses[0].Centre;
    const FVector End = Wall->GetSpec().Poses[1].Centre;
    TestTrue(TEXT("the wall would cycle in normal motion"), Wall->GetSpec().bAutoCycle);
    TestTrue(TEXT("reduced motion is in force"), Wall->IsReduced());
    Wall->AdvanceTime(AwMoverCycleSeconds * 4.0f);
    TestFalse(TEXT("reduced motion: no cycling by itself"), Wall->IsMoving());
    TestTrue(TEXT("the wall has not moved"), Wall->GetCentre().Equals(Start, 0.01));

    Wall->GoTo(1);
    TestTrue(TEXT("a requested step is pending (announced first)"), Wall->IsMoving());
    TestEqual(TEXT("the pose has not changed yet"), Wall->GetPose(), 0);
    TestEqual(TEXT("but the target is the other pose"), Wall->GetTargetPose(), 1);
    for (int32 Step = 0; Step < 14; ++Step)
    {
        Wall->AdvanceTime(0.1f); // 1.4 s, still inside the announcement
        const FVector Here = Wall->GetCentre();
        TestTrue(TEXT("never an in-between position"), Here.Equals(Start, 0.01) || Here.Equals(End, 0.01));
    }
    TestTrue(TEXT("still at the start before the announcement ends"), Wall->GetCentre().Equals(Start, 0.01));
    Wall->AdvanceTime(0.3f);
    TestTrue(TEXT("one discrete jump"), Wall->GetCentre().Equals(End, 0.01) && Wall->GetPose() == 1 && !Wall->IsMoving());

    // The faulty marker is steady under reduced motion.
    APWAwakeningMoverActor* Marker = T.Floor()->GetMover(2, true);
    if (TestNotNull(TEXT("the marker"), Marker))
    {
        TestTrue(TEXT("it flickers in normal motion"), Marker->IsFlickering());
        for (int32 Step = 0; Step < 30; ++Step)
        {
            Marker->AdvanceTime(0.1f);
            TestTrue(TEXT("steady fault colour, no flicker"), Marker->GetShownColour() == EAwColour::HazardYellow);
        }
    }

    // The advance pad is the untimed way to see it move; reduced motion says so first.
    T.Bridge->Enter();
    T.Floor()->TryEnter(nullptr);
    Wall->SnapTo(0);
    AwTMotionPad(T, 1, 4)->Interact(nullptr);
    TestTrue(TEXT("the advance is announced as coming 'in a moment'"), T.Floor()->GetLastCaption().ToString().Contains(TEXT("in a moment")));
    TestEqual(TEXT("and the pose waits"), Wall->GetPose(), 0);
    TestEqual(TEXT("the advance never reached the bridge as a choice"), T.Bridge->CountCalls(TEXT("submit")), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwNormalMoversTest, "PerfectWorld.AwakeningFloor.NormalMotionMovers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwNormalMoversTest::RunTest(const FString& Parameters)
{
    FAwTestFloor T;
    if (!T.Init(*this, AwTMotionConfig(false)))
    {
        return false;
    }
    APWAwakeningMoverActor* Wall = T.Floor()->GetMover(1, false);
    if (!TestNotNull(TEXT("the wall"), Wall))
    {
        return false;
    }
    const FVector Start = Wall->GetSpec().Poses[0].Centre;
    const FVector End = Wall->GetSpec().Poses[1].Centre;
    TestFalse(TEXT("normal motion"), Wall->IsReduced());
    Wall->GoTo(1);
    Wall->AdvanceTime(AwMoverGlideSeconds * 0.5f);
    const FVector Mid = Wall->GetCentre();
    TestFalse(TEXT("half way through the glide it is neither at the start nor the end"), Mid.Equals(Start, 0.5) || Mid.Equals(End, 0.5));
    TestTrue(TEXT("it is between the two"), Mid.Y < Start.Y && Mid.Y > End.Y);
    Wall->AdvanceTime(AwMoverGlideSeconds);
    TestTrue(TEXT("the glide ends exactly on the pose"), Wall->GetCentre().Equals(End, 0.01) && Wall->GetPose() == 1);

    // Cycling by itself, until held.
    Wall->SnapTo(0);
    Wall->AdvanceTime(AwMoverCycleSeconds + 0.1f);
    TestEqual(TEXT("it begins to cycle on its own"), Wall->GetTargetPose(), 1);
    Wall->SnapTo(0);
    Wall->SetHold(true);
    Wall->AdvanceTime(AwMoverCycleSeconds * 3.0f);
    TestEqual(TEXT("a held wall stays put"), Wall->GetTargetPose(), 0);

    APWAwakeningMoverActor* Marker = T.Floor()->GetMover(2, true);
    if (TestNotNull(TEXT("the marker"), Marker))
    {
        bool bSawInk = false;
        for (int32 Step = 0; Step < 30; ++Step)
        {
            Marker->AdvanceTime(0.1f);
            bSawInk |= Marker->GetShownColour() == EAwColour::Ink;
        }
        TestTrue(TEXT("the marker flickers"), bSawInk);
        Marker->SetFlickering(false);
        TestTrue(TEXT("steadied, it shows its pose colour"), Marker->GetShownColour() == EAwColour::HazardYellow);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwTransferMotionTest, "PerfectWorld.AwakeningFloor.TransfersFadeOrCut",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwTransferMotionTest::RunTest(const FString& Parameters)
{
    // Reduced motion: announced first, held still, one cut, never an alpha above zero.
    {
        FAwFloorConfig Config = AwTMotionConfig(true);
        Config.bInstantTransitions = false;
        FAwTestFloor T;
        if (!T.Init(*this, Config))
        {
            return false;
        }
        APWAwakeningFloorActor* Floor = T.Floor();
        TestTrue(TEXT("entry begins"), Floor->TryEnter(nullptr));
        TestTrue(TEXT("a transfer is in progress"), Floor->IsTransitioning());
        TestFalse(TEXT("the move has not happened yet"), Floor->IsOccupied());
        TestTrue(TEXT("the caption announced it first"), Floor->GetLastCaption().ToString().Contains(TEXT("Welcome")));
        Floor->Tick(0.5f);
        TestEqual(TEXT("no fade at all"), Floor->GetFadeAlpha(), 0.0f);
        TestFalse(TEXT("still waiting"), Floor->IsOccupied());
        Floor->Tick(1.0f);
        TestTrue(TEXT("the cut happened once"), Floor->IsOccupied() && !Floor->IsTransitioning());
        TestEqual(TEXT("and never faded"), Floor->GetFadeAlpha(), 0.0f);
    }
    // Normal motion: fades to black, moves under it, fades back.
    {
        FAwFloorConfig Config = AwTMotionConfig(false);
        Config.bInstantTransitions = false;
        FAwTestFloor T;
        if (!T.Init(*this, Config))
        {
            return false;
        }
        APWAwakeningFloorActor* Floor = T.Floor();
        TestTrue(TEXT("entry begins"), Floor->TryEnter(nullptr));
        Floor->Tick(0.2f);
        TestTrue(TEXT("part way into the fade"), Floor->GetFadeAlpha() > 0.0f && Floor->GetFadeAlpha() < 1.0f);
        TestFalse(TEXT("not yet moved"), Floor->IsOccupied());
        Floor->Tick(0.2f);
        TestTrue(TEXT("moved under black"), Floor->IsOccupied());
        TestEqual(TEXT("fully dark"), Floor->GetFadeAlpha(), 1.0f);
        Floor->Tick(0.2f);
        Floor->Tick(0.2f);
        Floor->Tick(0.2f);
        TestTrue(TEXT("done and clear"), !Floor->IsTransitioning() && Floor->GetFadeAlpha() == 0.0f);
        // Finishing at once (tests, acceptance) never leaves it half way.
        TestTrue(TEXT("a second transfer"), Floor->TryLeave(nullptr));
        Floor->FinishTransition();
        TestTrue(TEXT("finished at once, off the floor"), !Floor->IsOccupied() && !Floor->IsTransitioning() && Floor->GetFadeAlpha() == 0.0f);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
