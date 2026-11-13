// Seats and readable objects (PerfectWorld.Furniture.*): the sit and stand-up glides, the sweeps that refuse an occupied or
// obstructed seat, the seated eye, and the readable's delegate. The last test builds the real opening home, turns its
// collision boxes into pawn-blocking boxes and sits and stands at both authored seats. Run in the editor under
// "PerfectWorld.Furniture".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldFurnitureActor.h"
#include "PerfectWorldHomeLayoutTestSupport.h"
#include "PerfectWorldPlayerCharacter.h"

namespace PWFurnitureTest
{
/** A throw-away game world; actors are spawned but play never begins, and actors are ticked by hand. */
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

/** A box that blocks the Pawn channel. bPerson makes it a Pawn-type object, like another character's capsule. */
AActor* SpawnBlockerBox(UWorld& World, const FVector& Centre, const FVector& HalfExtent, const bool bPerson)
{
    AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
    if (Actor == nullptr)
    {
        return nullptr;
    }
    UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Blocker"));
    Box->SetMobility(EComponentMobility::Movable);
    Box->SetBoxExtent(HalfExtent);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(bPerson ? ECC_Pawn : ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Actor->SetRootComponent(Box);
    Box->RegisterComponent();
    Actor->SetActorLocation(Centre);
    return Actor;
}

APerfectWorldPlayerCharacter* SpawnPlayer(UWorld& World, const FVector& FeetLocation, const double YawDegrees)
{
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Centre = FeetLocation + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 0.5);
    APerfectWorldPlayerCharacter* Player = World.SpawnActor<APerfectWorldPlayerCharacter>(
        APerfectWorldPlayerCharacter::StaticClass(), FTransform(FRotator(0.0, YawDegrees, 0.0), Centre), Spawn);
    if (Player != nullptr)
    {
        Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking); // play never begins here, so the movement component never settles by itself
    }
    return Player;
}

/** A seat at the origin on the floor (Z 0), 45 cm high, facing +X, with its stand point 100 cm ahead. */
APWSeatActor* SpawnTestSeat(UWorld& World)
{
    APWSeatActor* Seat = World.SpawnActor<APWSeatActor>(APWSeatActor::StaticClass(), FTransform::Identity);
    if (Seat != nullptr)
    {
        Seat->Configure(FName(TEXT("seat.test.chair")), FText::FromString(TEXT("Sit on the chair")), FVector::ZeroVector,
                        FVector(1.0, 0.0, 0.0), 45.0f, FVector(100.0, 0.0, 0.0), 45.0f);
    }
    return Seat;
}

void RunGlide(APerfectWorldPlayerCharacter& Player)
{
    for (int32 I = 0; I < 8; ++I)
    {
        Player.Tick(0.1f);
    }
}
} // namespace PWFurnitureTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWFurnitureSeatMathTest, "PerfectWorld.Furniture.SeatedNumbers",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWFurnitureSeatMathTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("smoothstep at 0"), PWSeat::SmoothStep(0.0f), 0.0f);
    TestEqual(TEXT("smoothstep at 1"), PWSeat::SmoothStep(1.0f), 1.0f);
    TestEqual(TEXT("smoothstep at 1/2"), PWSeat::SmoothStep(0.5f), 0.5f);
    TestEqual(TEXT("smoothstep clamps"), PWSeat::SmoothStep(7.0f), 1.0f);

    TestEqual(TEXT("eye over the 45 cm chair"), PWSeat::SeatedEyeAboveFloor(45.0f), 115.0f);
    TestEqual(TEXT("eye over the 42 cm divan"), PWSeat::SeatedEyeAboveFloor(42.0f), 112.0f);
    TestEqual(TEXT("camera height over the seated capsule centre"), PWSeat::EyeRelativeZ, 29.0f);

    // Looking is limited to 105 degrees either side of the facing, across the +-180 wrap too.
    TestTrue(TEXT("a small turn is kept"), FMath::IsNearlyEqual(PWSeat::ClampLookYaw(-170.0f, 170.0f), -170.0f, 0.01f));
    TestTrue(TEXT("turning far left stops at 105"), FMath::IsNearlyEqual(PWSeat::ClampLookYaw(0.0f, 170.0f), 65.0f, 0.01f));
    TestTrue(TEXT("a turn inside the limit is kept"), FMath::IsNearlyEqual(PWSeat::ClampLookYaw(-90.0f, 0.0f), -90.0f, 0.01f));
    TestTrue(TEXT("directly behind is stopped"), FMath::Abs(FMath::FindDeltaAngleDegrees(0.0f, PWSeat::ClampLookYaw(180.0f, 0.0f))) <= 105.01f);
    TestTrue(TEXT("pitch down is limited"), FMath::IsNearlyEqual(PWSeat::ClampLookPitch(300.0f), -60.0f, 0.01f));
    TestTrue(TEXT("pitch up is limited"), FMath::IsNearlyEqual(PWSeat::ClampLookPitch(80.0f), 55.0f, 0.01f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWFurnitureSitStandTest, "PerfectWorld.Furniture.SitAndStandUp",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWFurnitureSitStandTest::RunTest(const FString& Parameters)
{
    using namespace PWFurnitureTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    APWSeatActor* Seat = SpawnTestSeat(*Holder.World);
    APerfectWorldPlayerCharacter* Player = SpawnPlayer(*Holder.World, FVector(100.0, 0.0, 0.0), 180.0);
    if (!TestNotNull(TEXT("seat"), Seat) || !TestNotNull(TEXT("player"), Player))
    {
        return false;
    }
    TestEqual(TEXT("verb"), Seat->GetInteractionVerb(Player).ToString(), FString(TEXT("Sit on the chair")));
    TestEqual(TEXT("priority 15"), Seat->GetInteractionPriority(), 15);
    TestTrue(TEXT("the prompt point is above a chair back"), Seat->GetInteractionPoint().Z > 90.0);

    Seat->Interact(Player);
    TestTrue(TEXT("gliding in"), Player->GetSeatState() == EPWSeatState::Entering);
    TestTrue(TEXT("no verb while gliding"), Seat->GetInteractionVerb(Player).IsEmpty());
    Player->Tick(0.1f);
    TestTrue(TEXT("still gliding after 0.1 s"), Player->GetSeatState() == EPWSeatState::Entering);
    const double MidX = Player->GetActorLocation().X;
    TestTrue(TEXT("moving towards the seat"), MidX < 100.0 && MidX > 0.0);
    RunGlide(*Player);

    TestTrue(TEXT("seated"), Player->GetSeatState() == EPWSeatState::Seated && Player->IsSeatedOn(Seat));
    const FVector Where = Player->GetActorLocation();
    TestTrue(TEXT("on the seat axis"), FMath::IsNearlyZero(Where.X, 0.1) && FMath::IsNearlyZero(Where.Y, 0.1));
    TestTrue(TEXT("capsule rests on the seat top"), FMath::IsNearlyEqual(Where.Z - PWSeat::CapsuleHalfHeight, 45.0, 0.1));
    TestTrue(TEXT("seated capsule"), FMath::IsNearlyEqual(Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), PWSeat::CapsuleHalfHeight, 0.01f));
    TestTrue(TEXT("eye 115 cm above the floor"), FMath::IsNearlyEqual(Player->GetEyeLocation().Z, 115.0, 0.5));
    TestTrue(TEXT("faces the seat's way"), Player->GetActorForwardVector().X > 0.999);
    TestEqual(TEXT("stand-up verb"), Seat->GetInteractionVerb(Player).ToString(), FString(TEXT("Stand up")));
    TestEqual(TEXT("stand-up beats people"), Seat->GetInteractionPriority(), 25);

    // Pressing E on the seat again, and Jump, both stand up; walking does not happen.
    TestFalse(TEXT("a second sit request is refused quietly"), Player->TrySit(Seat, Seat->MakePlacement()) == EPWSitResult::Started);
    Seat->Interact(Player);
    TestTrue(TEXT("gliding out"), Player->GetSeatState() == EPWSeatState::Leaving);
    RunGlide(*Player);
    TestTrue(TEXT("standing again"), Player->GetSeatState() == EPWSeatState::Standing);
    const FVector After = Player->GetActorLocation();
    TestTrue(TEXT("on the stand point"), FMath::IsNearlyEqual(After.X, 100.0, 0.1) && FMath::IsNearlyZero(After.Y, 0.1));
    TestTrue(TEXT("standing capsule"), FMath::IsNearlyEqual(Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), PWPlayer::CapsuleHalfHeight, 0.01f));
    TestTrue(TEXT("standing eye"), FMath::IsNearlyEqual(Player->GetEyeLocation().Z, PWPlayer::EyeHeightAboveFloor + 0.5, 0.5));
    TestEqual(TEXT("sit verb returns"), Seat->GetInteractionVerb(Player).ToString(), FString(TEXT("Sit on the chair")));
    TestEqual(TEXT("priority returns to 15"), Seat->GetInteractionPriority(), 15);

    Seat->Interact(Player);
    RunGlide(*Player);
    TestTrue(TEXT("seated a second time"), Player->GetSeatState() == EPWSeatState::Seated);
    Player->Jump();
    RunGlide(*Player);
    TestTrue(TEXT("jump stands up"), Player->GetSeatState() == EPWSeatState::Standing);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWFurnitureRefusalsTest, "PerfectWorld.Furniture.RefusesOccupiedAndObstructedSeats",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWFurnitureRefusalsTest::RunTest(const FString& Parameters)
{
    using namespace PWFurnitureTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    UWorld& World = *Holder.World;
    APWSeatActor* Seat = SpawnTestSeat(World);
    APerfectWorldPlayerCharacter* Player = SpawnPlayer(World, FVector(100.0, 0.0, 0.0), 180.0);
    if (!TestNotNull(TEXT("seat"), Seat) || !TestNotNull(TEXT("player"), Player))
    {
        return false;
    }
    const FPWSeatPlacement Placement = Seat->MakePlacement();

    // Another person at the seat.
    AActor* Person = SpawnBlockerBox(World, FVector(0.0, 0.0, 90.0), FVector(20.0, 20.0, 50.0), true);
    TestTrue(TEXT("occupied seat refused"), Player->TrySit(Seat, Placement) == EPWSitResult::Occupied);
    TestTrue(TEXT("still standing, not moved"), Player->GetSeatState() == EPWSeatState::Standing && FMath::IsNearlyEqual(Player->GetActorLocation().X, 100.0, 0.01));
    Person->Destroy();

    // A wall across the way, well outside the furniture's own footprint.
    AActor* Wall = SpawnBlockerBox(World, FVector(70.0, 0.0, 70.0), FVector(5.0, 100.0, 100.0), false);
    TestTrue(TEXT("a wall in the way refused"), Player->TrySit(Seat, Placement) == EPWSitResult::Obstructed);
    TestTrue(TEXT("still standing"), Player->GetSeatState() == EPWSeatState::Standing);
    Wall->Destroy();

    // A person in the way, even right beside the seat, is never "the furniture".
    AActor* Walker = SpawnBlockerBox(World, FVector(40.0, 0.0, 70.0), FVector(8.0, 20.0, 40.0), true);
    TestTrue(TEXT("a person in the way refused"), Player->TrySit(Seat, Placement) == EPWSitResult::Obstructed);
    Walker->Destroy();

    // The furniture itself (a chair back right at the seat) is not an obstruction.
    AActor* ChairBack = SpawnBlockerBox(World, FVector(-15.0, 0.0, 70.0), FVector(3.0, 20.0, 25.0), false);
    AActor* Table = SpawnBlockerBox(World, FVector(35.0, 0.0, 70.0), FVector(5.0, 60.0, 30.0), false);
    TestTrue(TEXT("furniture within the footprint does not refuse"), Player->TrySit(Seat, Placement) == EPWSitResult::Started);
    TestTrue(TEXT("a request while gliding is Busy"), Player->TrySit(Seat, Placement) == EPWSitResult::Busy);
    ChairBack->Destroy();
    Table->Destroy();
    RunGlide(*Player);
    TestTrue(TEXT("seated"), Player->GetSeatState() == EPWSeatState::Seated);

    // No room to stand: a solid block over the stand point and its alternatives. The player stays seated.
    AActor* Block = SpawnBlockerBox(World, FVector(130.0, 0.0, 80.0), FVector(60.0, 60.0, 100.0), false);
    TestFalse(TEXT("cannot stand up into a block"), Player->StandUp());
    TestTrue(TEXT("still seated"), Player->GetSeatState() == EPWSeatState::Seated);
    Block->Destroy();
    TestTrue(TEXT("stands up once it is clear"), Player->StandUp());
    RunGlide(*Player);
    TestTrue(TEXT("standing"), Player->GetSeatState() == EPWSeatState::Standing);

    // A different floor level: a player standing 120 cm above the stand point. The first player still stands on that point,
    // so they leave first; a capsule spawned into theirs is pushed out upwards, falls, and is refused as NotReady before the
    // level is ever compared.
    Player->Destroy();
    APerfectWorldPlayerCharacter* Upstairs = SpawnPlayer(World, FVector(100.0, 0.0, 120.0), 0.0);
    if (!TestNotNull(TEXT("upstairs player"), Upstairs))
    {
        return false;
    }
    TestTrue(TEXT("the upstairs player stands where spawned"), Upstairs->GetCharacterMovement()->IsMovingOnGround() &&
                 FMath::IsNearlyEqual(Upstairs->GetActorLocation().Z, 120.0 + PWPlayer::CapsuleHalfHeight + 0.5, 0.5));
    TestEqual(TEXT("another level refused (EPWSitResult)"), static_cast<int32>(Upstairs->TrySit(Seat, Placement)),
              static_cast<int32>(EPWSitResult::WrongLevel));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWFurnitureReadableTest, "PerfectWorld.Furniture.ReadableRaisesTheDelegate",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWFurnitureReadableTest::RunTest(const FString& Parameters)
{
    using namespace PWFurnitureTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    APWReadableActor* Paper = Holder.World->SpawnActor<APWReadableActor>(APWReadableActor::StaticClass(), FTransform::Identity);
    APerfectWorldPlayerCharacter* Player = SpawnPlayer(*Holder.World, FVector(100.0, 0.0, 0.0), 0.0);
    if (!TestNotNull(TEXT("readable"), Paper) || !TestNotNull(TEXT("player"), Player))
    {
        return false;
    }
    Paper->Configure(FName(TEXT("readable.test.paper")), FName(TEXT("text.test.paper")), FText::FromString(TEXT("Read the paper")),
                     FText::FromString(TEXT("Today's paper")), FVector(40.0, 10.0, 78.0), FVector(70.0, 10.0, 0.0));
    TestEqual(TEXT("verb"), Paper->GetInteractionVerb(Player).ToString(), FString(TEXT("Read the paper")));
    TestEqual(TEXT("priority 15"), Paper->GetInteractionPriority(), 15);
    TestTrue(TEXT("prompt point just above the sheet"), FMath::IsNearlyEqual(Paper->GetInteractionPoint().Z, 90.0, 0.01));
    TestTrue(TEXT("stand point"), Paper->GetStandPoint().Equals(FVector(70.0, 10.0, 0.0)));

    Paper->Interact(Player); // nothing bound: logs, does not crash

    struct FSeen
    {
        int32 Count = 0;
        FName Readable;
        FName Content;
        FString Title;
    };
    FSeen Local;
    FSeen Global;
    const FDelegateHandle LocalHandle = Paper->OnReadRequested.AddLambda([&Local](FName Id, FName Content, FText Title) {
        ++Local.Count;
        Local.Readable = Id;
        Local.Content = Content;
        Local.Title = Title.ToString();
    });
    const FDelegateHandle GlobalHandle = APWReadableActor::GlobalReadRequested().AddLambda([&Global](FName Id, FName Content, FText Title) {
        ++Global.Count;
        Global.Readable = Id;
        Global.Content = Content;
        Global.Title = Title.ToString();
    });
    Paper->Interact(Player);
    APWReadableActor::GlobalReadRequested().Remove(GlobalHandle);
    Paper->OnReadRequested.Remove(LocalHandle);

    TestEqual(TEXT("per-actor delegate fired once"), Local.Count, 1);
    TestEqual(TEXT("global delegate fired once"), Global.Count, 1);
    TestEqual(TEXT("readable id"), Local.Readable, FName(TEXT("readable.test.paper")));
    TestEqual(TEXT("content id"), Local.Content, FName(TEXT("text.test.paper")));
    TestEqual(TEXT("title"), Local.Title, FString(TEXT("Today's paper")));
    TestEqual(TEXT("global content id"), Global.Content, FName(TEXT("text.test.paper")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWFurnitureHomeSeatsTest, "PerfectWorld.Furniture.OpeningHomeSeatsAndPaper",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWFurnitureHomeSeatsTest::RunTest(const FString& Parameters)
{
    using namespace PWFurnitureTest;
    PWHomeLayoutTest::FHome Home;
    PWHomeLayoutTest::BuildHome(Home);
    if (!TestTrue(TEXT("the opening home builds"), Home.bBuilt))
    {
        return false;
    }
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    UWorld& World = *Holder.World;

    // The home's real collision boxes (shell and furniture) as pawn-blocking boxes.
    AActor* Shell = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
    USceneComponent* Root = NewObject<USceneComponent>(Shell, TEXT("Root"));
    Shell->SetRootComponent(Root);
    Root->RegisterComponent();
    TArray<FBox> Boxes;
    for (const FBox& Source : Home.Scene.Blocks.BlockingBoxes())
    {
        Boxes.Add(Source);
        UBoxComponent* Box = NewObject<UBoxComponent>(Shell);
        Box->SetupAttachment(Root);
        Box->SetBoxExtent(Source.GetExtent());
        Box->SetRelativeLocation(Source.GetCenter());
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();
    }

    const PWLane::FLaneFrame& Frame = Home.Building.Frame;
    for (const PWLane::FHomeSeatSpec& Spec : PWLane::HomeSeatSpecs())
    {
        const FString Name = Spec.Id.ToString();
        APWSeatActor* Seat = World.SpawnActor<APWSeatActor>(APWSeatActor::StaticClass(), FTransform::Identity);
        if (!TestNotNull(*(Name + TEXT(" seat")), Seat))
        {
            return false;
        }
        Seat->ConfigureFromSpec(Spec, Frame);
        const FVector SeatFloor = PWLane::HomeWorldPoint(Frame, Spec.SeatU, Spec.SeatV, 0.0);
        TestTrue(*(Name + TEXT(" at the spec position")), Seat->GetActorLocation().Equals(SeatFloor, 0.01));
        const FVector Facing = (Frame.U * Spec.FaceU + Frame.V * Spec.FaceV).GetSafeNormal2D();
        TestTrue(*(Name + TEXT(" faces the spec direction")), Seat->GetActorForwardVector().Equals(Facing, 0.001));
        TestEqual(*(Name + TEXT(" verb")), Seat->GetInteractionVerb(nullptr).ToString(), FString(Spec.Verb));
        TestEqual(*(Name + TEXT(" priority")), Seat->GetInteractionPriority(), 15);

        const FVector StandFloor = PWLane::HomeWorldPoint(Frame, Spec.StandU, Spec.StandV, 0.0);
        APerfectWorldPlayerCharacter* Player = SpawnPlayer(World, StandFloor, FMath::RadiansToDegrees(FMath::Atan2(-Facing.Y, -Facing.X)));
        if (!TestNotNull(*(Name + TEXT(" player")), Player))
        {
            return false;
        }
        Seat->Interact(Player);
        RunGlide(*Player);
        TestTrue(*(Name + TEXT(" sits from the stand point")), Player->GetSeatState() == EPWSeatState::Seated);
        const FVector Eye = Player->GetEyeLocation();
        TestTrue(*(Name + TEXT(" eye height")),
                 FMath::IsNearlyEqual(Eye.Z - SeatFloor.Z, static_cast<double>(PWSeat::SeatedEyeAboveFloor(static_cast<float>(Spec.SeatHeightCm))), 0.5));
        // ASSET_PIPELINE.md: the first-person camera stays at least 15 cm from every solid surface in a seat pose.
        double Nearest = 1.0e9;
        for (const FBox& Box : Boxes)
        {
            Nearest = FMath::Min(Nearest, FMath::Sqrt(Box.ComputeSquaredDistanceToPoint(Eye)));
        }
        TestTrue(*(Name + FString::Printf(TEXT(" camera is %.1f cm from the nearest solid (needs 15)"), Nearest)), Nearest >= 15.0);

        Seat->Interact(Player);
        RunGlide(*Player);
        TestTrue(*(Name + TEXT(" stands up")), Player->GetSeatState() == EPWSeatState::Standing);
        TestTrue(*(Name + TEXT(" back on the stand point")), FVector::Dist2D(Player->GetActorLocation(), StandFloor) < 1.0);
        Player->Destroy();
        Seat->Destroy();
    }

    const PWLane::FHomeReadableSpec ReadSpec = PWLane::HomeReadableSpec();
    APWReadableActor* Paper = World.SpawnActor<APWReadableActor>(APWReadableActor::StaticClass(), FTransform::Identity);
    if (!TestNotNull(TEXT("readable"), Paper))
    {
        return false;
    }
    Paper->ConfigureFromSpec(ReadSpec, Frame);
    TestTrue(TEXT("newspaper at the spec position"), Paper->GetActorLocation().Equals(PWLane::HomeWorldPoint(Frame, ReadSpec.U, ReadSpec.V, ReadSpec.HeightCm), 0.01));
    TestEqual(TEXT("newspaper ids"), Paper->GetContentId(), ReadSpec.ContentId);
    TestEqual(TEXT("newspaper verb"), Paper->GetInteractionVerb(nullptr).ToString(), FString(ReadSpec.Verb));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
