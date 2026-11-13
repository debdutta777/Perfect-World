// F5, the calling bell: the geometry the threshold guard and the bell placement rest on, the bell actor, the bell a private door hangs
// for itself, and the guard on every door. Run in the editor under "PerfectWorld.DoorBell". The rules themselves (who is answered, who
// may cross) are the portable core's and are tested by Tests/DoorBellTests.cpp; nothing here re-derives them.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "DoorBell.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldDoorBell.h"

namespace PWDoorBellTest
{
/** A throw-away game world with a physics scene; play never begins, so doors do not register with the lane-life subsystem. */
struct FDbTestWorld
{
    UWorld* World = nullptr;

    FDbTestWorld() { World = UWorld::CreateWorld(EWorldType::Game, false); }
    ~FDbTestWorld()
    {
        if (World != nullptr)
        {
            World->DestroyWorld(false);
        }
    }
    FDbTestWorld(const FDbTestWorld&) = delete;
    FDbTestWorld& operator=(const FDbTestWorld&) = delete;
};

/** A one-way dwelling door at the origin, yaw 0: the closed leaf runs along +X, the dwelling is on the +Y side (swing +1). */
APWDoorActor* DbSpawnDoor(UWorld& World, const TCHAR* Id, const FTransform& At = FTransform::Identity)
{
    APWDoorActor* Door = World.SpawnActor<APWDoorActor>(APWDoorActor::StaticClass(), At);
    if (Door != nullptr)
    {
        Door->Configure(FName(Id), 1.0f, false);
    }
    return Door;
}

/** A wall slab that blocks visibility traces, `FaceY` cm to the outside (-Y) of the hinge line and `Thickness` deep, along X from -200 to 400. */
AActor* DbSpawnWall(UWorld& World, const double OutsideFaceY, const double Thickness)
{
    AActor* Wall = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall, TEXT("Wall"));
    Box->SetMobility(EComponentMobility::Movable);
    Box->SetBoxExtent(FVector(300.0, 0.5 * Thickness, 150.0));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Wall->SetRootComponent(Box);
    Box->RegisterComponent();
    // The wall's face towards the street is at Y = -FaceDepth; its body runs from there towards +Y by Thickness.
    Wall->SetActorLocation(FVector(100.0, -OutsideFaceY + 0.5 * Thickness, 150.0));
    return Wall;
}
} // namespace PWDoorBellTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorBellGeometryTest, "PerfectWorld.DoorBell.ThresholdGeometryReadsInsideAndOutside",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorBellGeometryTest::RunTest(const FString& Parameters)
{
    // Door at (1000, 2000, 300), yawed 90 degrees: its closed leaf runs along world +Y, its local +Y points to world -X.
    const FTransform Door(FRotator(0.0, 90.0, 0.0), FVector(1000.0, 2000.0, 300.0));
    // Dwelling on the local +Y side (world -X): inside is at X < 1000.
    const double Side = 1.0;
    const FVector Outside = PWDoorBell::OutsideDirection(Door, Side);
    TestTrue(TEXT("the outside of a +Y dwelling is world +X for a door yawed 90"), Outside.Equals(FVector(1.0, 0.0, 0.0), 1.0e-4));
    TestTrue(TEXT("the other way round"), PWDoorBell::OutsideDirection(Door, -1.0).Equals(FVector(-1.0, 0.0, 0.0), 1.0e-4));
    TestEqual(TEXT("the bell faces its visitor: yaw 0 for +X"), PWDoorBell::BellYawDegrees(Outside), 0.0, 1.0e-6);
    TestEqual(TEXT("and yaw 180 for -X"), PWDoorBell::BellYawDegrees(FVector(-1.0, 0.0, 0.0)), 180.0, 1.0e-6);

    // A point 40 cm along the leaf, 50 cm outside, at pawn height: outside, near, in the doorway span.
    const FVector OutsidePoint(1050.0, 2040.0, 396.0);
    const PWDoorBell::FThresholdProbe Out = PWDoorBell::ProbeThreshold(Door, Side, OutsidePoint);
    TestEqual(TEXT("across runs along the closed leaf"), Out.Across, 40.0f, 1.0e-3f);
    TestEqual(TEXT("depth is negative on the street side"), Out.Depth, -50.0f, 1.0e-3f);
    TestEqual(TEXT("up is height above the door's floor"), Out.Up, 96.0f, 1.0e-3f);
    TestTrue(TEXT("within reach of the guard"), Out.bNear);
    const PWDoorBell::FThresholdProbe In = PWDoorBell::ProbeThreshold(Door, Side, FVector(950.0, 2040.0, 396.0));
    TestEqual(TEXT("depth is positive inside"), In.Depth, 50.0f, 1.0e-3f);
    // The same points seen with the opposite authored inside side swap their signs.
    TestEqual(TEXT("flipping the inside side flips the depth"), PWDoorBell::ProbeThreshold(Door, -1.0, OutsidePoint).Depth, 50.0f, 1.0e-3f);
    // Far away, or on another floor, is not "near".
    TestFalse(TEXT("20 m away is not near"), PWDoorBell::ProbeThreshold(Door, Side, FVector(3000.0, 2040.0, 396.0)).bNear);
    TestFalse(TEXT("a floor below is not near"), PWDoorBell::ProbeThreshold(Door, Side, FVector(1050.0, 2040.0, 96.0)).bNear);

    // Crossings: only outside -> inside through the doorway, only as a walk.
    const PWDoorBell::FThresholdProbe Step = PWDoorBell::ProbeThreshold(Door, Side, FVector(1020.0, 2040.0, 396.0)); // depth -20
    const PWDoorBell::FThresholdProbe Across = PWDoorBell::ProbeThreshold(Door, Side, FVector(1000.0, 2040.0, 396.0)); // depth 0
    TestFalse(TEXT("approaching the doorway is not yet a crossing"), PWDoorBell::IsInwardCrossing(Out, Step, 30.0f));
    TestTrue(TEXT("passing the hinge line from outside is a crossing"), PWDoorBell::IsInwardCrossing(Step, Across, 20.0f));
    TestTrue(TEXT("so is a big frame step that still counts as a walk"), PWDoorBell::IsInwardCrossing(Out, In, 100.0f));
    TestFalse(TEXT("going out is never a crossing"), PWDoorBell::IsInwardCrossing(In, Out, 100.0f));
    TestFalse(TEXT("a jump of more than a walk (a placement or a load) is not a crossing"), PWDoorBell::IsInwardCrossing(Out, In, 400.0f));
    PWDoorBell::FThresholdProbe BesideTheJamb = In;
    BesideTheJamb.Across = 130.0f; // past the latch jamb: wall, not doorway
    TestFalse(TEXT("only the doorway span counts"), PWDoorBell::IsInwardCrossing(Out, BesideTheJamb, 100.0f));

    // The nominal bell point sits on the latch side, outside the wall face, chest high, and well clear of the leaf.
    const FVector Nominal = PWDoorBell::NominalBellPoint(Door, Side, PWDoorBell::LatchSideAcrossCm);
    TestTrue(TEXT("the bell is on the street side"), FVector::DotProduct(Nominal - Door.GetLocation(), Outside) > 10.0);
    TestEqual(TEXT("chest high above the door's floor"), Nominal.Z - Door.GetLocation().Z, static_cast<double>(PWDoorBell::HeightCm), 1.0e-3);
    TestTrue(TEXT("beside the frame, past the leaf's 90 cm"), FVector::DotProduct(Nominal - Door.GetLocation(), FVector(0.0, 1.0, 0.0)) > 100.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorBellActorTest, "PerfectWorld.DoorBell.TheBellIsAnInteractableThatPulses",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorBellActorTest::RunTest(const FString& Parameters)
{
    using namespace PWDoorBellTest;
    FDbTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    APWDoorBellActor* Bell = Holder.World->SpawnActor<APWDoorBellActor>(APWDoorBellActor::StaticClass(), FTransform::Identity);
    if (!TestNotNull(TEXT("bell"), Bell))
    {
        return false;
    }
    Bell->Configure(FName(TEXT("door.nabapara.tea_row_house_front")));
    TestTrue(TEXT("it remembers its door"), Bell->GetDoorId() == FName(TEXT("door.nabapara.tea_row_house_front")));
    TestTrue(TEXT("it can be asked for a verb with no player and no subsystem"), !Bell->GetInteractionVerb(nullptr).IsEmpty());
    TestEqual(TEXT("its verb names the bell"), Bell->GetInteractionVerb(nullptr).ToString(), FString(TEXT("Ring the bell")));
    TestEqual(TEXT("it outranks a door (10) so the bell is what the prompt offers beside a frame"), Bell->GetInteractionPriority(), 12);
    TestTrue(TEXT("and stays below furniture (15) and people (20)"), Bell->GetInteractionPriority() < 15);
    // The interaction point is on the button, in front of the plate, towards the visitor (+X of the bell).
    const FVector Point = Bell->GetInteractionPoint();
    TestTrue(TEXT("the point is proud of the wall, in front"), Point.X > PWDoorBell::PlateDepthCm && Point.X < PWDoorBell::PlateDepthCm + 3.0f);
    // The label.
    UTextRenderComponent* Label = Bell->FindComponentByClass<UTextRenderComponent>();
    if (TestNotNull(TEXT("a label"), Label))
    {
        TestEqual(TEXT("it says BELL"), Label->Text.ToString(), FString(TEXT("BELL")));
    }
    // The pulse starts and ends, and never leaves the actor ticking.
    TestFalse(TEXT("quiet at rest"), Bell->IsPulsing());
    Bell->Pulse();
    TestTrue(TEXT("pulsing after a ring"), Bell->IsPulsing());
    for (int32 Frame = 0; Frame < 60 && Bell->IsPulsing(); ++Frame)
    {
        Bell->Tick(0.02f);
    }
    TestFalse(TEXT("the pulse ends"), Bell->IsPulsing());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorBellPlacementTest, "PerfectWorld.DoorBell.PrivateDwellingDoorsHangABellBesideTheFrame",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorBellPlacementTest::RunTest(const FString& Parameters)
{
    using namespace PWDoorBellTest;
    FDbTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    // Every private dwelling entrance hangs exactly one bell, named for it, on the street side of its frame.
    for (const std::string_view Entrance : PerfectWorld::DoorBellEntranceIds)
    {
        const FString Id = FString(UTF8_TO_TCHAR(std::string(Entrance).c_str()));
        APWDoorActor* Door = DbSpawnDoor(*Holder.World, *Id);
        if (!TestNotNull(*(Id + TEXT(": door")), Door))
        {
            continue;
        }
        APWDoorBellActor* Bell = PWDoorBell::SpawnBellFor(*Door);
        if (!TestNotNull(*(Id + TEXT(": bell")), Bell))
        {
            continue;
        }
        TestTrue(*(Id + TEXT(": the bell is its door's")), Bell->GetDoorId() == FName(*Id));
        TestTrue(*(Id + TEXT(": the door knows its bell")), Door->GetBellActor() == Bell);
        TestTrue(*(Id + TEXT(": on the street side (a swing +1 door has its dwelling at local +Y)")), Bell->GetActorLocation().Y < 0.0);
        TestTrue(*(Id + TEXT(": the bell faces the street")), Bell->GetActorForwardVector().Equals(FVector(0.0, -1.0, 0.0), 1.0e-3));
        TestTrue(*(Id + TEXT(": a second request hangs no second bell")), PWDoorBell::SpawnBellFor(*Door) == Bell);
        Door->Destroy();
        TestFalse(*(Id + TEXT(": the bell goes with its door")), IsValid(Bell));
    }
    // A public door, a business door and a room door have no bell.
    for (const TCHAR* Id : {TEXT("door.nabapara.tall_flats_front"), TEXT("door.nabapara.stationery_front"), TEXT("door.nabapara.home_bedroom"),
                            TEXT("door.nabapara.home_living"), TEXT("door.unknown.not_in_the_core")})
    {
        APWDoorActor* Door = DbSpawnDoor(*Holder.World, Id);
        if (TestNotNull(Id, Door))
        {
            TestTrue(FString(Id) + TEXT(": no bell"), PWDoorBell::SpawnBellFor(*Door) == nullptr && Door->GetBellActor() == nullptr);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorBellWallTest, "PerfectWorld.DoorBell.TheBellSitsOnTheRealWallFaceAndAvoidsBlockedSpots",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorBellWallTest::RunTest(const FString& Parameters)
{
    using namespace PWDoorBellTest;
    FDbTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    const TCHAR* Id = TEXT("door.nabapara.tea_row_house_front");
    // A thin partition whose street face is only 6 cm from the hinge line: the bell hangs on that face, not at the nominal 12.5 cm.
    DbSpawnWall(*Holder.World, 6.0, 12.0);
    APWDoorActor* Door = DbSpawnDoor(*Holder.World, Id);
    if (!TestNotNull(TEXT("door"), Door))
    {
        return false;
    }
    APWDoorBellActor* Bell = PWDoorBell::SpawnBellFor(*Door);
    if (!TestNotNull(TEXT("bell"), Bell))
    {
        return false;
    }
    TestEqual(TEXT("the bell is on the wall's street face"), Bell->GetActorLocation().Y, -6.0, 0.6);
    TestEqual(TEXT("on the latch side beside the frame"), Bell->GetActorLocation().X, static_cast<double>(PWDoorBell::LatchSideAcrossCm), 0.6);
    TestEqual(TEXT("chest high"), Bell->GetActorLocation().Z, static_cast<double>(PWDoorBell::HeightCm), 0.6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWDoorGuardPresentTest, "PerfectWorld.DoorBell.EveryDoorCarriesAThresholdGuard",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWDoorGuardPresentTest::RunTest(const FString& Parameters)
{
    using namespace PWDoorBellTest;
    FDbTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    APWDoorActor* Door = DbSpawnDoor(*Holder.World, TEXT("door.nabapara.tea_row_house_front"));
    if (TestNotNull(TEXT("door"), Door))
    {
        const UPWDoorGuardComponent* Guard = Door->FindComponentByClass<UPWDoorGuardComponent>();
        if (TestNotNull(TEXT("the door has a threshold guard"), Guard))
        {
            TestEqual(TEXT("it has stopped nobody yet"), Guard->GetBounceCount(), 0);
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
