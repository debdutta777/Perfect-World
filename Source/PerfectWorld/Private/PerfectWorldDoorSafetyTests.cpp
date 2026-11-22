// Door safety: the leaf must never be rotated through a person or a piece of furniture, even with a very long frame,
// and it must report how its motion ended. Run in the editor (Session Frontend or the AutomationTestToolset) under
// "PerfectWorld.Doors".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldDoorActor.h"

namespace PWDoorSafetyTest
{
/** A throw-away game world with a physics scene; actors are spawned but play never begins, so doors do not register with the subsystem. */
struct FTestWorld
{
    UWorld* World = nullptr;

    FTestWorld() { World = UWorld::CreateWorld(EWorldType::Game, false); }
    ~FTestWorld()
    {
        if (World != nullptr)
        {
            World->DestroyWorld(false);
        }
    }
    FTestWorld(const FTestWorld&) = delete;
    FTestWorld& operator=(const FTestWorld&) = delete;
};

/** Everything OnSettled reported. */
struct FSettledRecord
{
    int32 Count = 0;
    bool bReachedTarget = false;
    bool bIsOpenNow = false;
    int32 SwingSide = 99;
};

/** A one-way door at the world origin, yaw 0, swinging towards +Y; the leaf runs along +X when closed. */
APWDoorActor* SpawnDoor(UWorld& World, const TCHAR* Id, FSettledRecord& Record)
{
    APWDoorActor* Door = World.SpawnActor<APWDoorActor>(APWDoorActor::StaticClass(), FTransform::Identity);
    if (Door != nullptr)
    {
        Door->Configure(FName(Id), 1.0f, false);
        Door->OnSettled.AddLambda([&Record](FName, bool bReached, bool bOpenNow, int8 Side) {
            ++Record.Count;
            Record.bReachedTarget = bReached;
            Record.bIsOpenNow = bOpenNow;
            Record.SwingSide = static_cast<int32>(Side);
        });
    }
    return Door;
}

/** A pawn-blocking box (a stand-in for a person or furniture), centre in world space. */
AActor* SpawnBlocker(UWorld& World, const FVector& Centre)
{
    AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
    if (Actor == nullptr)
    {
        return nullptr;
    }
    UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Blocker"));
    Box->SetMobility(EComponentMobility::Movable);
    Box->SetBoxExtent(FVector(5.0, 5.0, 50.0));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_Pawn);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Actor->SetRootComponent(Box);
    Box->RegisterComponent();
    Actor->SetActorLocation(Centre);
    return Actor;
}

/** A point `Distance` cm along the closed leaf rotated by `AngleDegrees` about the hinge, 100 cm above the floor. */
FVector LeafPoint(const float AngleDegrees, const float Distance)
{
    const double Rad = FMath::DegreesToRadians(static_cast<double>(AngleDegrees));
    return FVector(Distance * FMath::Cos(Rad), Distance * FMath::Sin(Rad), 100.0);
}

/** Ticks in small frames until the door stops moving (or a safety cap). */
void RunUntilStopped(APWDoorActor& Door, const float Frame)
{
    for (int32 I = 0; I < 200 && Door.IsMoving(); ++I)
    {
        Door.Tick(Frame);
    }
}
} // namespace PWDoorSafetyTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorSubStepMathTest, "PerfectWorld.Doors.SubStepsKeepTipWithinFiveCentimetres",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorSubStepMathTest::RunTest(const FString& Parameters)
{
    // Every interval, from a hair to several full swings, is covered by checks no further apart than 5 cm of tip travel.
    const float Intervals[][2] = {{0.0f, 0.1f}, {0.0f, 3.0f}, {0.0f, 95.0f}, {95.0f, 0.0f}, {-95.0f, 95.0f}, {0.0f, 720.0f}, {30.0f, 29.0f}};
    for (const auto& Interval : Intervals)
    {
        const int32 Steps = PWDoor::SubStepCount(Interval[0], Interval[1]);
        TestTrue(TEXT("at least one check"), Steps >= 1);
        const float StepDegrees = FMath::Abs(Interval[1] - Interval[0]) / static_cast<float>(Steps);
        const float TipTravel = PWDoor::LeafWidth * FMath::DegreesToRadians(StepDegrees);
        TestTrue(TEXT("tip travel per check is at most 5 cm"), TipTravel <= PWDoor::MaxTipStepCm + 0.001f);
    }
    // A full opening needs at least 30 checks: 95 degrees at 3.18 degrees each.
    TestTrue(TEXT("full swing is at least 30 checks"), PWDoor::SubStepCount(0.0f, PWDoor::OpenAngleDegrees) >= 30);

    TestEqual(TEXT("closed side"), static_cast<int32>(PWDoor::SwingSideOf(0.0f)), 0);
    TestEqual(TEXT("tiny angle is closed"), static_cast<int32>(PWDoor::SwingSideOf(0.1f)), 0);
    TestEqual(TEXT("positive side"), static_cast<int32>(PWDoor::SwingSideOf(12.0f)), 1);
    TestEqual(TEXT("negative side"), static_cast<int32>(PWDoor::SwingSideOf(-12.0f)), -1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorFreeSwingTest, "PerfectWorld.Doors.FreeSwingSettlesOnceEachWay",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorFreeSwingTest::RunTest(const FString& Parameters)
{
    using namespace PWDoorSafetyTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    FSettledRecord Record;
    APWDoorActor* Door = SpawnDoor(*Holder.World, TEXT("door.test.free"), Record);
    if (!TestNotNull(TEXT("door"), Door))
    {
        return false;
    }

    TestTrue(TEXT("opens when clear"), Door->TryMoveTo(true, nullptr));
    TestFalse(TEXT("a second request while moving is refused"), Door->TryMoveTo(false, nullptr));
    Door->Tick(5.0f); // one enormous frame: still checked, still arrives
    TestEqual(TEXT("settled once"), Record.Count, 1);
    TestTrue(TEXT("reached the target"), Record.bReachedTarget);
    TestTrue(TEXT("open now"), Record.bIsOpenNow);
    TestEqual(TEXT("swing side +1"), Record.SwingSide, 1);
    TestTrue(TEXT("fully open"), FMath::IsNearlyEqual(Door->GetOpenFraction(), 1.0f, 0.001f));
    TestEqual(TEXT("getter side"), static_cast<int32>(Door->GetSwingSide()), 1);
    TestTrue(TEXT("IsOpen"), Door->IsOpen());
    TestFalse(TEXT("not moving"), Door->IsMoving());

    TestTrue(TEXT("closes when clear"), Door->TryMoveTo(false, nullptr));
    RunUntilStopped(*Door, 0.016f);
    TestEqual(TEXT("settled twice in total"), Record.Count, 2);
    TestTrue(TEXT("reached closed"), Record.bReachedTarget);
    TestFalse(TEXT("closed now"), Record.bIsOpenNow);
    TestEqual(TEXT("closed side 0"), Record.SwingSide, 0);
    TestTrue(TEXT("fully closed"), FMath::IsNearlyZero(Door->GetOpenFraction(), 0.001f));
    TestFalse(TEXT("IsOpen false"), Door->IsOpen());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorObstructionTest, "PerfectWorld.Doors.StopsAtLastSafeAngleWhenSomeoneStepsIn",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorObstructionTest::RunTest(const FString& Parameters)
{
    using namespace PWDoorSafetyTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    UWorld& World = *Holder.World;

    // A blocker 50 cm out along the 37 degree line: a 10 cm square whose nearest face the leaf meets at about 35 degrees.
    // The sweep volume (10 cm thick) meets it a little earlier, at about 31 degrees.
    const FVector InTheWay = LeafPoint(37.0f, 50.0f);

    // 1. Already in the way before the door starts: it will not start.
    {
        FSettledRecord Record;
        APWDoorActor* Door = SpawnDoor(World, TEXT("door.test.blocked_at_start"), Record);
        AActor* Blocker = SpawnBlocker(World, InTheWay);
        if (!TestNotNull(TEXT("door and blocker"), Door) || !TestNotNull(TEXT("blocker"), Blocker))
        {
            return false;
        }
        TestFalse(TEXT("will not start into an occupied swing"), Door->TryMoveTo(true, nullptr));
        TestEqual(TEXT("nothing settled"), Record.Count, 0);
        TestFalse(TEXT("still closed"), Door->IsOpen());
        Blocker->Destroy();
        Door->Destroy();
    }

    // 2. Steps in mid-swing, ordinary frames: stops short, counts as open, reports it once.
    {
        FSettledRecord Record;
        APWDoorActor* Door = SpawnDoor(World, TEXT("door.test.mid_swing"), Record);
        if (!TestNotNull(TEXT("door"), Door) || !TestTrue(TEXT("starts when clear"), Door->TryMoveTo(true, nullptr)))
        {
            return false;
        }
        AActor* Blocker = SpawnBlocker(World, InTheWay);
        RunUntilStopped(*Door, 0.05f);
        TestEqual(TEXT("settled once"), Record.Count, 1);
        TestFalse(TEXT("did not reach the target"), Record.bReachedTarget);
        TestTrue(TEXT("part-way counts as open"), Record.bIsOpenNow);
        TestEqual(TEXT("swing side +1"), Record.SwingSide, 1);
        TestTrue(TEXT("IsOpen follows the real angle"), Door->IsOpen());
        TestFalse(TEXT("not moving"), Door->IsMoving());
        const float Angle = Door->GetCurrentAngleDegrees();
        TestTrue(TEXT("opened a good way before stopping"), Angle > 20.0f);
        TestTrue(TEXT("stopped before touching the blocker"), Angle < 34.0f);
        TestTrue(TEXT("open fraction is partial"), Door->GetOpenFraction() > 0.2f && Door->GetOpenFraction() < 0.4f);
        TestFalse(TEXT("cannot be asked to open again"), Door->TryMoveTo(true, nullptr));

        // Closing moves away from the blocker, so it is allowed and finishes normally.
        TestTrue(TEXT("can close from part-way"), Door->TryMoveTo(false, nullptr));
        RunUntilStopped(*Door, 0.05f);
        TestEqual(TEXT("settled twice in total"), Record.Count, 2);
        TestTrue(TEXT("reached closed"), Record.bReachedTarget);
        TestFalse(TEXT("closed now"), Record.bIsOpenNow);
        TestEqual(TEXT("closed side"), Record.SwingSide, 0);
        Blocker->Destroy();
        Door->Destroy();
    }

    // 3. Steps in during one enormous frame: the same stop, not a jump to the end.
    {
        FSettledRecord Record;
        APWDoorActor* Door = SpawnDoor(World, TEXT("door.test.big_frame"), Record);
        if (!TestNotNull(TEXT("door"), Door) || !TestTrue(TEXT("starts when clear"), Door->TryMoveTo(true, nullptr)))
        {
            return false;
        }
        AActor* Blocker = SpawnBlocker(World, InTheWay);
        Door->Tick(5.0f);
        TestEqual(TEXT("settled once"), Record.Count, 1);
        TestFalse(TEXT("did not reach the target"), Record.bReachedTarget);
        TestTrue(TEXT("part-way counts as open"), Record.bIsOpenNow);
        TestTrue(TEXT("stopped before the blocker, not at 95 degrees"), Door->GetCurrentAngleDegrees() < 34.0f);
        Blocker->Destroy();
        Door->Destroy();
    }

    // 4. Someone steps into a closing door: it stops open at the last safe angle.
    {
        FSettledRecord Record;
        APWDoorActor* Door = SpawnDoor(World, TEXT("door.test.closing"), Record);
        if (!TestNotNull(TEXT("door"), Door))
        {
            return false;
        }
        Door->SetOpenImmediate(true, 1);
        TestTrue(TEXT("starts to close"), Door->TryMoveTo(false, nullptr));
        AActor* Blocker = SpawnBlocker(World, LeafPoint(60.0f, 50.0f));
        RunUntilStopped(*Door, 0.05f);
        TestEqual(TEXT("settled once"), Record.Count, 1);
        TestFalse(TEXT("did not reach closed"), Record.bReachedTarget);
        TestTrue(TEXT("still open"), Record.bIsOpenNow);
        TestEqual(TEXT("swing side +1"), Record.SwingSide, 1);
        TestTrue(TEXT("IsOpen"), Door->IsOpen());
        TestTrue(TEXT("stopped on the open side of the blocker"), Door->GetCurrentAngleDegrees() > 60.0f);
        Blocker->Destroy();
        Door->Destroy();
    }

    // 5. The restored swing side of a two-way door survives SetOpenImmediate.
    {
        APWDoorActor* Door = World.SpawnActor<APWDoorActor>(APWDoorActor::StaticClass(), FTransform::Identity);
        if (!TestNotNull(TEXT("door"), Door))
        {
            return false;
        }
        Door->Configure(FName(TEXT("door.test.two_way")), 1.0f, true);
        Door->SetOpenImmediate(true, -1);
        TestEqual(TEXT("restored to the -1 side"), static_cast<int32>(Door->GetSwingSide()), -1);
        Door->SetOpenImmediate(false);
        TestEqual(TEXT("closed side 0"), static_cast<int32>(Door->GetSwingSide()), 0);
        Door->Destroy();
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
