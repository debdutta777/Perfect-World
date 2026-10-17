// People walk their authored routes (PerfectWorldNpcMovementComponent.h) with the character movement really running: a
// two-room fixture with the stationery shop's numbers (a partition with one off-axis doorway, a packing desk behind it), a
// person in the stock room and the counter on the other side of the wall. The harness runs frames at 120 fps the way the
// engine does: the character movement every frame, the person's movement component whenever its own tick interval has
// elapsed. The person is marked as seen every frame, so the one teleport can never hide a bad walk. Run in the editor
// under "PerfectWorld.NpcRoute".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcMovementComponent.h"

namespace NpRouteTest
{
constexpr double NrFrameSeconds = 1.0 / 120.0;
const FVector NrStock(615.0, 595.0, 0.0);   // the stock room anchor
const FVector NrCounter(250.0, 325.0, 0.0); // the staff side of the counter, straight through the partition from the stock room
const FName NrCounterId(TEXT("anchor.test.counter"));
const FName NrRouteId(TEXT("route:anchor.test.stock_room->anchor.test.counter"));

/** A game world with a physics scene that starts ten seconds in (so nothing counts as just rendered by accident). */
struct FNrWorld
{
    UWorld* World = nullptr;
    TArray<FBox> Obstacles; // the solids a walking body must never touch: partition jambs and the packing desk

    FNrWorld()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (World != nullptr)
        {
            World->TimeSeconds = 10.0;
        }
    }
    ~FNrWorld()
    {
        if (World != nullptr)
        {
            World->DestroyWorld(false);
        }
    }
    FNrWorld(const FNrWorld&) = delete;
    FNrWorld& operator=(const FNrWorld&) = delete;

    void Box(const FVector& Min, const FVector& Max, const bool bObstacle)
    {
        AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
        if (Actor == nullptr)
        {
            return;
        }
        UBoxComponent* Shape = NewObject<UBoxComponent>(Actor, TEXT("Solid"));
        Shape->SetMobility(EComponentMobility::Movable);
        Shape->SetBoxExtent((Max - Min) * 0.5);
        Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Shape->SetCollisionObjectType(ECC_WorldStatic);
        Shape->SetCollisionResponseToAllChannels(ECR_Block);
        Actor->SetRootComponent(Shape);
        Shape->RegisterComponent();
        Actor->SetActorLocation((Min + Max) * 0.5);
        if (bObstacle)
        {
            Obstacles.Add(FBox(Min, Max));
        }
    }

    /** The stationery shop's ground floor in building-local centimetres (LaneInteriorLayout dump 700 700 250). */
    void BuildShop()
    {
        Box(FVector(-100.0, -100.0, -20.0), FVector(800.0, 800.0, 0.0), false);  // floor
        Box(FVector(25.0, 435.0, 0.0), FVector(568.0, 447.0, 274.0), true);      // partition, west of the doorway
        Box(FVector(662.0, 435.0, 0.0), FVector(675.0, 447.0, 274.0), true);     // partition, east of the doorway
        Box(FVector(568.0, 435.0, 212.0), FVector(662.0, 447.0, 274.0), false);  // lintel over the 94 cm doorway
        Box(FVector(395.0, 447.0, 0.0), FVector(555.0, 507.0, 78.0), true);      // packing desk in the stock room
        Box(FVector(25.0, 225.0, 0.0), FVector(555.0, 285.0, 90.0), false);      // the counter, in front of its staff side
        Box(FVector(675.0, 25.0, 0.0), FVector(700.0, 675.0, 274.0), false);     // east wall
        Box(FVector(0.0, 675.0, 0.0), FVector(700.0, 700.0, 274.0), false);      // back wall
    }
};

/** One person and the frame loop. */
struct FNrPerson
{
    APWNpcCharacter* Npc = nullptr;
    UPWNpcMovementComponent* Move = nullptr;
    UCharacterMovementComponent* Body = nullptr;
    double SinceTick = 0.0;
    double MinClearanceCm = 1.0e9; // nearest the capsule ever came to an obstacle
    double Seconds = 0.0;
    double FrameSeconds = NrFrameSeconds; // the game's frame time; the character movement runs once per frame

    bool Spawn(UWorld& World)
    {
        Npc = APWNpcCharacter::SpawnAt(World, FTransform(FRotator(0.0, -90.0, 0.0), NrStock + FVector(0.0, 0.0, 1.0)),
                                       FName(TEXT("person.test.shopkeeper")), FText::FromString(TEXT("Shopkeeper")));
        if (Npc == nullptr)
        {
            return false;
        }
        Move = Npc->GetNpcMovement();
        Body = Npc->GetCharacterMovement();
        Body->SetMovementMode(MOVE_Walking); // play never begins in this world, so nothing settles the movement mode
        Move->SetAnchorResolver([](const FName Id, FTransform& Out) {
            if (Id != NrCounterId)
            {
                return false;
            }
            Out = FTransform(FRotator(0.0, -90.0, 0.0), NrCounter);
            return true;
        });
        return true;
    }

    FVector Feet() const { return Npc->GetActorLocation() - FVector(0.0, 0.0, Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()); }

    void Run(UWorld& World, const TArray<FBox>& Obstacles, const double Duration)
    {
        const double Radius = Npc->GetCapsuleComponent()->GetScaledCapsuleRadius();
        for (double T = 0.0; T < Duration - 1e-9; T += FrameSeconds)
        {
            World.TimeSeconds += FrameSeconds;
            Seconds += FrameSeconds;
            Npc->GetCapsuleComponent()->SetLastRenderTime(static_cast<float>(World.TimeSeconds)); // seen: never teleported
            SinceTick += FrameSeconds;
            const double Interval = Move->GetComponentTickInterval();
            if (Interval <= 0.0 || SinceTick >= Interval - 1e-6)
            {
                Move->TickPresentation(static_cast<float>(SinceTick));
                SinceTick = 0.0;
            }
            Body->TickComponent(static_cast<float>(FrameSeconds), LEVELTICK_All, &Body->PrimaryComponentTick);
            const FVector Centre = Npc->GetActorLocation();
            for (const FBox& Box : Obstacles)
            {
                const FVector Closest = Box.GetClosestPointTo(FVector(Centre.X, Centre.Y, FMath::Clamp(Centre.Z, Box.Min.Z, Box.Max.Z)));
                MinClearanceCm = FMath::Min(MinClearanceCm, FVector::Dist2D(Closest, Centre) - Radius);
            }
        }
    }
};

TArray<FVector> NrRoutePoints()
{
    return {NrStock, FVector(615.0, 325.0, 0.0), NrCounter}; // counter_to_stock_room, walked from the stock room
}

FPWNpcIntentView NrTravel(const int32 Permille)
{
    FPWNpcIntentView View;
    View.State = EPWNpcState::Travel;
    View.RouteId = NrRouteId;
    View.AnchorId = NrCounterId;
    View.ProgressPermille = Permille;
    return View;
}

FPWNpcIntentView NrWork()
{
    FPWNpcIntentView View;
    View.State = EPWNpcState::Work;
    View.AnchorId = NrCounterId;
    View.WorkActionId = FName(TEXT("work.serve"));
    return View;
}

/** How far along the authored route the feet are. */
double NrAlong(const FVector& Feet)
{
    FPWNpcPolyline Line;
    Line.Set(NrRoutePoints());
    double Lateral = 0.0;
    return Line.Project(Feet, -1.0, Lateral);
}
} // namespace NpRouteTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpRoutePaceTest, "PerfectWorld.NpcRoute.KeepsTheSchedulePaceThroughTheDoorway",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpRoutePaceTest::RunTest(const FString& Parameters)
{
    using namespace NpRouteTest;
    FNrWorld Fixture;
    if (!TestNotNull(TEXT("test world"), Fixture.World))
    {
        return false;
    }
    Fixture.BuildShop();
    FNrPerson Person;
    if (!TestTrue(TEXT("person spawned"), Person.Spawn(*Fixture.World)))
    {
        return false;
    }
    // 07:55 to 08:00 at one game minute per real second: the core moves the target a fifth of the 635 cm each second,
    // and at 08:00 the intent is Work at the counter.
    Person.Move->SetIntent(NrTravel(0));
    Person.Move->SetRoute(NrRoutePoints());
    for (int32 Minute = 1; Minute <= 4; ++Minute)
    {
        Person.Run(*Fixture.World, Fixture.Obstacles, 1.0);
        Person.Move->SetIntent(NrTravel(Minute * 200));
    }
    Person.Run(*Fixture.World, Fixture.Obstacles, 1.0);
    const double Along = NrAlong(Person.Feet());
    TestTrue(*FString::Printf(TEXT("keeps the schedule's pace: %.0f of 635 cm walked in the five-second window (needs 450)"), Along), Along >= 450.0);
    Person.Move->SetIntent(NrWork());
    Person.Run(*Fixture.World, Fixture.Obstacles, 8.0);

    const double Left = FVector::Dist2D(Person.Feet(), NrCounter);
    TestTrue(*FString::Printf(TEXT("stands at the counter anchor (%.1f cm off)"), Left), Left <= 20.0);
    TestTrue(*FString::Printf(TEXT("never touched the partition or the desk (closest %.1f cm)"), Person.MinClearanceCm), Person.MinClearanceCm >= 2.0);
    TestFalse(TEXT("never persistently blocked"), Person.Move->IsPersistentlyBlocked());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpRouteLateTest, "PerfectWorld.NpcRoute.LateArrivalFinishesAlongTheRoute",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpRouteLateTest::RunTest(const FString& Parameters)
{
    using namespace NpRouteTest;
    FNrWorld Fixture;
    if (!TestNotNull(TEXT("test world"), Fixture.World))
    {
        return false;
    }
    Fixture.BuildShop();
    FNrPerson Person;
    if (!TestTrue(TEXT("person spawned"), Person.Spawn(*Fixture.World)))
    {
        return false;
    }
    // The travel window passes while the person has barely started (held up, or the clock skipped): the intent becomes
    // Work at the counter with the person still in the stock room. The rest of the way is the authored route, through the
    // doorway, never a straight line through the partition. Run at 30 fps, where input cadence cannot be what goes wrong.
    Person.FrameSeconds = 1.0 / 30.0;
    Person.Move->SetIntent(NrTravel(0));
    Person.Move->SetRoute(NrRoutePoints());
    Person.Run(*Fixture.World, Fixture.Obstacles, 0.2);
    TestTrue(TEXT("still in the stock room when the window closes"), Person.Feet().Y > 500.0);
    Person.Move->SetIntent(NrWork());
    Person.Run(*Fixture.World, Fixture.Obstacles, 10.0);

    const double Left = FVector::Dist2D(Person.Feet(), NrCounter);
    TestTrue(*FString::Printf(TEXT("arrives at the counter anchor (%.1f cm off)"), Left), Left <= 20.0);
    TestTrue(*FString::Printf(TEXT("never touched the partition or the desk (closest %.1f cm)"), Person.MinClearanceCm), Person.MinClearanceCm >= 2.0);
    TestTrue(TEXT("presented as working once there"), Person.Move->GetPresentedState() == EPWNpcState::Work);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
