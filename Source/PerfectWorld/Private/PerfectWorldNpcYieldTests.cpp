#include "PerfectWorldNpcYieldTests.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "PerfectWorldNpcCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpDoorwayYieldTest, "PerfectWorld.NpcYield.StepAsideHysteresis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpDoorwayYieldTest::RunTest(const FString& Parameters)
{
    FNpDoorwayZone Door;
    Door.Id = FName(TEXT("door.test"));
    Door.Bounds = FBox(FVector(-50.0, -50.0, 0.0), FVector(50.0, 50.0, 210.0));
    Door.WaitingFeet = {FVector(0.0, 180.0, 0.0), FVector(0.0, -230.0, 0.0)};
    TArray<FNpDoorwayZone> Doors{Door};
    FNpYieldState State;
    FVector Player(350.0, 0.0, 0.0);
    const auto NpAllowed = [](const FVector&) { return true; };
    TestFalse(TEXT("idle NPC waits until approach"), NpYield::Update(State, Doors, FVector::ZeroVector, 34.0, 81.0, &Player, true, 0.1, NpAllowed));
    Player.X = 290.0;
    TestTrue(TEXT("approach triggers sidestep"), NpYield::Update(State, Doors, FVector::ZeroVector, 34.0, 81.0, &Player, true, 0.1, NpAllowed));
    TestTrue(TEXT("nearest clear point selected"), State.WaitingFeet.Equals(Door.WaitingFeet[0]));
    const FVector Waiting = State.WaitingFeet;
    Player.X = 360.0;
    TestTrue(TEXT("moving out of approach radius does not oscillate"), NpYield::Update(State, Doors, Waiting, 34.0, 81.0, &Player, true, 2.0, NpAllowed));
    Player.X = 450.0;
    TestTrue(TEXT("release delay holds"), NpYield::Update(State, Doors, Waiting, 34.0, 81.0, &Player, true, 0.6, NpAllowed));
    Player.X = 290.0;
    NpYield::Update(State, Doors, Waiting, 34.0, 81.0, &Player, true, 0.1, NpAllowed);
    TestEqual(TEXT("reapproach resets clear timer"), State.ClearSeconds, 0.0);
    Player.X = 450.0;
    NpYield::Update(State, Doors, Waiting, 34.0, 81.0, &Player, true, 0.6, NpAllowed);
    TestFalse(TEXT("returns after full clear second"), NpYield::Update(State, Doors, Waiting, 34.0, 81.0, &Player, true, 0.5, NpAllowed));
    Player.X = 290.0;
    TestFalse(TEXT("NPC outside volume need not yield"), NpYield::Update(State, Doors, Waiting, 34.0, 81.0, &Player, true, 0.1, NpAllowed));
    TestFalse(TEXT("sleep/chat/travel excluded"), NpYield::Update(State, Doors, FVector::ZeroVector, 34.0, 81.0, &Player, false, 0.1, NpAllowed));
    const auto NpOnlyFarther = [](const FVector& Point) { return Point.Y < 0.0; };
    NpYield::Update(State, Doors, FVector::ZeroVector, 34.0, 81.0, &Player, true, 0.1, NpOnlyFarther);
    TestTrue(TEXT("occupied nearest point chooses next safe point"), State.WaitingFeet.Equals(Door.WaitingFeet[1]));
    State = {};
    TestFalse(TEXT("no safe point means no invented destination"), NpYield::Update(State, Doors, FVector::ZeroVector, 34.0, 81.0, &Player, true, 0.1,
        [](const FVector&) { return false; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpDoorwayWalkTest, "PerfectWorld.NpcYield.WalksToWaitingPoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpDoorwayWalkTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("isolated game world"), World)) { return false; }
    APWNpcCharacter* Actor = APWNpcCharacter::SpawnAt(*World, FTransform::Identity,
        FName(TEXT("person.test.yield")), FText::FromString(TEXT("Worker")));
    if (Actor == nullptr) { World->DestroyWorld(false); return false; }
    const FVector Before = Actor->GetActorLocation();
    const FVector WaitingFeet(0.0, 180.0, 0.0);
    Actor->GetNpcMovement()->SetAnchorResolver([WaitingFeet](const FName Id, FTransform& Out)
    {
        Out = FTransform(WaitingFeet);
        return Id == FName(TEXT("anchor.test.wait"));
    });
    FPWNpcIntentView Detour;
    Detour.State = EPWNpcState::Idle;
    Detour.AnchorId = FName(TEXT("anchor.test.wait"));
    Actor->GetNpcMovement()->SetIntent(Detour);
    Actor->GetNpcMovement()->TickPresentation(0.1f);
    TestTrue(TEXT("sidestep requests walking input"), Actor->GetPendingMovementInputVector().Y > 0.0);
    TestTrue(TEXT("presentation does not teleport to wait point"), Actor->GetActorLocation().Equals(Before));
    World->DestroyWorld(false);
    return true;
}

#endif
