// How people are presented moving and working (PerfectWorldNpcMovementComponent.h): route arithmetic, the catch-up and
// teleport rules, the gesture table, visible waiting when blocked, conversation suspension, and the decline text. The pure
// parts run on plain values; the rest spawn a person in a bare game world (play never begins, so the character movement does
// not tick: the tests drive TickPresentation by hand and read what the component decides). Run in the editor under
// "PerfectWorld.NpcMovement" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "NpcBehaviour.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcMovementComponent.h"
#include "PerfectWorldPresentationSettings.h"

namespace PWNpcMoveTest
{
/** A throw-away game world with a physics scene; actors spawn but play never begins. */
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

TArray<FVector> Line(const double Length)
{
    return {FVector(0.0, 0.0, 0.0), FVector(Length, 0.0, 0.0)};
}

APWNpcCharacter* SpawnPerson(UWorld& World, const FVector& Feet = FVector::ZeroVector)
{
    return APWNpcCharacter::SpawnAt(World, FTransform(FRotator::ZeroRotator, Feet), FName(TEXT("person.test")), FText::FromString(TEXT("Test")));
}

FPWNpcIntentView TravelIntent(const int32 ProgressPermille)
{
    FPWNpcIntentView View;
    View.State = EPWNpcState::Travel;
    View.RouteId = FName(TEXT("route.test"));
    View.ProgressPermille = ProgressPermille;
    View.AnchorId = FName(TEXT("anchor.destination"));
    return View;
}

void Run(UPWNpcMovementComponent& Movement, const int32 Steps, const float Step)
{
    for (int32 I = 0; I < Steps; ++I)
    {
        Movement.TickPresentation(Step);
    }
}

/** A pawn-blocking slab ahead of the origin, wide enough that sidesteps are blocked too. */
AActor* SpawnWall(UWorld& World)
{
    AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
    if (Actor == nullptr)
    {
        return nullptr;
    }
    UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Wall"));
    Box->SetMobility(EComponentMobility::Movable);
    Box->SetBoxExtent(FVector(300.0, 300.0, 100.0));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Actor->SetRootComponent(Box);
    Box->RegisterComponent();
    Actor->SetActorLocation(FVector(350.0, 0.0, 100.0)); // x 50 to 650: past the capsule, across the way
    return Actor;
}
} // namespace PWNpcMoveTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcPolylineTest, "PerfectWorld.NpcMovement.PolylineArithmetic",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcPolylineTest::RunTest(const FString& Parameters)
{
    FPWNpcPolyline Line;
    Line.Set({FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0)});
    TestTrue(TEXT("valid"), Line.IsValid());
    TestNearlyEqual(TEXT("length"), Line.Length(), 2000.0, 0.01);
    TestTrue(TEXT("point on the first leg"), Line.PointAt(500.0).Equals(FVector(500, 0, 0), 0.01));
    TestTrue(TEXT("point on the second leg"), Line.PointAt(1500.0).Equals(FVector(1000, 500, 0), 0.01));
    TestTrue(TEXT("clamped before the start"), Line.PointAt(-50.0).Equals(FVector(0, 0, 0), 0.01));
    TestTrue(TEXT("clamped past the end"), Line.PointAt(9e6).Equals(FVector(1000, 1000, 0), 0.01));
    TestTrue(TEXT("direction on leg two"), Line.DirectionAt(1500.0).Equals(FVector(0, 1, 0), 0.001));

    double Lateral = 0.0;
    TestNearlyEqual(TEXT("projection along"), Line.Project(FVector(520, 40, 0), -1.0, Lateral), 520.0, 0.01);
    TestNearlyEqual(TEXT("projection off the line"), Lateral, 40.0, 0.01);

    FPWNpcPolyline Doubled;
    Doubled.Set({FVector(0, 0, 0), FVector(0, 0, 0.4), FVector(1000, 0, 0)});
    TestEqual(TEXT("a point within 1 cm of the last is dropped"), Doubled.Points.Num(), 2);

    // A route that doubles back: the hint keeps the person on the leg they are on.
    FPWNpcPolyline Back;
    Back.Set({FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 100, 0), FVector(0, 100, 0)});
    TestNearlyEqual(TEXT("hint on the outbound leg"), Back.Project(FVector(500, 50, 0), 100.0, Lateral), 500.0, 0.01);
    TestNearlyEqual(TEXT("hint on the return leg"), Back.Project(FVector(500, 50, 0), 1600.0, Lateral), 1600.0, 0.01);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcCatchUpRulesTest, "PerfectWorld.NpcMovement.CatchUpAndTeleportRules",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcCatchUpRulesTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcMove;
    TestEqual(TEXT("ahead of the core's target: wait"), SpeedForLag(-50.0, 140.0), 0.0);
    TestNearlyEqual(TEXT("on pace: walk at the core's pace"), SpeedForLag(0.0, 140.0), 140.0, 0.01);
    TestNearlyEqual(TEXT("slightly behind: still walking pace"), SpeedForLag(50.0, 140.0), 140.0, 0.01);
    TestNearlyEqual(TEXT("more than 60 cm behind: catch up"), SpeedForLag(61.0, 140.0), 200.0, 0.01);
    TestNearlyEqual(TEXT("far behind: never faster than the modest cap"), SpeedForLag(4000.0, 140.0), 200.0, 0.01);
    TestEqual(TEXT("clock stopped and level: stop"), SpeedForLag(0.0, 0.0), 0.0);
    TestTrue(TEXT("a small lag with a stopped clock closes slowly"), SpeedForLag(20.0, 0.0) < 60.0);

    TestTrue(TEXT("far, unseen, free: the one teleport"), ShouldSnap(501.0, false, false, false));
    TestFalse(TEXT("exactly 5 m is not far enough"), ShouldSnap(500.0, false, false, false));
    TestFalse(TEXT("never in the player's view"), ShouldSnap(9000.0, true, false, false));
    TestFalse(TEXT("never while blocked"), ShouldSnap(9000.0, false, true, false));
    TestFalse(TEXT("never onto an occupied spot"), ShouldSnap(9000.0, false, false, true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcGestureTest, "PerfectWorld.NpcMovement.GesturesAreTablesNotOutcomes",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcGestureTest::RunTest(const FString& Parameters)
{
    for (const PWNpcGesture::FGesture& Row : PWNpcGesture::Table)
    {
        const FName Id(Row.Id);
        TestTrue(*FString::Printf(TEXT("%s has a positive cycle"), Row.Id), PWNpcGesture::CycleSeconds(Id) > 0.0f);
        const FPWNpcPose Start = PWNpcGesture::Work(Id, 0.0f, false);
        const FPWNpcPose Quarter = PWNpcGesture::Work(Id, Row.Cycle * 0.25f, false);
        TestFalse(*FString::Printf(TEXT("%s moves over a stroke"), Row.Id), Start.NearlyEquals(Quarter));
        const FPWNpcPose Again = PWNpcGesture::Work(Id, Row.Cycle, false);
        TestTrue(*FString::Printf(TEXT("%s repeats each cycle"), Row.Id), Start.NearlyEquals(Again, 0.01f));
        TestTrue(*FString::Printf(TEXT("%s with reduced motion holds still"), Row.Id),
                 PWNpcGesture::Work(Id, 0.0f, true).NearlyEquals(PWNpcGesture::Work(Id, 1.3f, true), 0.001f));
        TestTrue(*FString::Printf(TEXT("%s with reduced motion holds the mid-stroke pose"), Row.Id), Start.NearlyEquals(PWNpcGesture::Work(Id, 7.0f, true), 0.001f));
    }
    TestTrue(TEXT("an unknown action gets the generic gesture"),
             PWNpcGesture::Work(FName(TEXT("work.nonsense")), 1.0f, false).NearlyEquals(PWNpcGesture::Work(FName(TEXT("work.activity")), 1.0f, false), 0.001f));
    TestTrue(TEXT("idle sway is gone with reduced motion"), PWNpcGesture::Idle(2.0f, true).NearlyEquals(FPWNpcPose(), 0.001f));
    TestFalse(TEXT("idle sways otherwise"), PWNpcGesture::Idle(1.4f, false).NearlyEquals(FPWNpcPose(), 0.1f));

    // Walking: one swing per 140 cm, none when standing.
    TestTrue(TEXT("a stride repeats every 140 cm"), PWNpcGesture::Walk(10.0f, 1.0f).NearlyEquals(PWNpcGesture::Walk(150.0f, 1.0f), 0.01f));
    TestNearlyEqual(TEXT("leg swing at the quarter stride"), PWNpcGesture::Walk(35.0f, 1.0f).LegPitch[0], 28.0f, 0.01f);
    TestTrue(TEXT("legs swing opposite"), PWNpcGesture::Walk(35.0f, 1.0f).LegPitch[1] < 0.0f);
    TestTrue(TEXT("no speed, no swing"), PWNpcGesture::Walk(35.0f, 0.0f).NearlyEquals(FPWNpcPose(), 0.001f));
    TestTrue(TEXT("yielding raises a hand"), PWNpcGesture::Yielding().ArmPitch[1] > 60.0f);
    TestEqual(TEXT("sleeping lies down"), PWNpcGesture::Sleeping().Lie, 1.0f);
    TestEqual(TEXT("sitting sits"), PWNpcGesture::Sitting().Sit, 1.0f);

    FPWNpcPose Pose;
    Pose.BlendToward(PWNpcGesture::Yielding(), 1.0f, 1.0f);
    TestTrue(TEXT("a full blend lands on the target"), Pose.NearlyEquals(PWNpcGesture::Yielding(), 0.001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcIntentViewTest, "PerfectWorld.NpcMovement.IntentViewCopiesTheCoreIntent",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcIntentViewTest::RunTest(const FString& Parameters)
{
    PerfectWorld::NpcIntent Core;
    Core.State = PerfectWorld::NpcState::Chat;
    Core.AnchorId = "anchor.shanti_chai.counter";
    Core.RouteId = "route:a->b";
    Core.ProgressPermille = 640;
    Core.Availability = PerfectWorld::PersonAvailability::Busy;
    Core.bTalkable = true;
    Core.DeclineReasonKey = PerfectWorld::NpcReasonWorking;
    Core.ResumeState = PerfectWorld::NpcState::Work;
    Core.ResumeWorkActionId = "work.serve";

    const FPWNpcIntentView View = MakeNpcIntentView(Core, true);
    TestTrue(TEXT("state"), View.State == EPWNpcState::Chat);
    TestTrue(TEXT("anchor"), View.AnchorId == FName(TEXT("anchor.shanti_chai.counter")));
    TestTrue(TEXT("route"), View.RouteId == FName(TEXT("route:a->b")));
    TestEqual(TEXT("progress"), View.ProgressPermille, 640);
    TestTrue(TEXT("talkable"), View.bTalkable);
    TestTrue(TEXT("decline key"), View.DeclineReasonKey == FName(TEXT("npc.reason.working")));
    TestTrue(TEXT("resume state"), View.ResumeState == EPWNpcState::Work);
    TestTrue(TEXT("resume action"), View.ResumeWorkActionId == FName(TEXT("work.serve")));
    TestTrue(TEXT("work action empty outside Work"), View.WorkActionId.IsNone());
    TestTrue(TEXT("seat preference is the integrator's"), View.bPreferSit);

    PerfectWorld::NpcIntent Asleep;
    Asleep.State = PerfectWorld::NpcState::Sleep;
    Asleep.bTalkable = false;
    TestTrue(TEXT("sleep"), MakeNpcIntentView(Asleep).State == EPWNpcState::Sleep);
    TestFalse(TEXT("not talkable"), MakeNpcIntentView(Asleep).bTalkable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcTeleportRuleTest, "PerfectWorld.NpcMovement.OnlyFarUnseenPeopleAreMovedAndNearOnesWalk",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcTeleportRuleTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcMoveTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }

    // 4 m behind the core's target: no teleport; they steer towards it (the walk itself needs a ticking world).
    APWNpcCharacter* Near = SpawnPerson(*Holder.World);
    if (!TestNotNull(TEXT("person"), Near))
    {
        return false;
    }
    UPWNpcMovementComponent& NearMove = *Near->GetNpcMovement();
    NearMove.SetIntent(TravelIntent(100));
    NearMove.SetRoute(Line(4000.0));
    TestTrue(TEXT("route taken"), NearMove.HasRoute());
    Run(NearMove, 10, 0.05f);
    TestNearlyEqual(TEXT("still where they were"), Near->GetActorLocation().X, 0.0, 0.01);
    TestTrue(TEXT("steering"), NearMove.IsMoving());
    const double Target = NearMove.GetTravelTarget().X;
    TestTrue(TEXT("the target is the core's point plus a short extrapolation"), Target >= 399.0 && Target <= 400.0 + 140.0 * 1.25 + 1.0);

    // The extrapolation stops 1.25 s after the last core update: a stopped clock does not drag people on.
    Run(NearMove, 60, 0.05f);
    TestTrue(TEXT("extrapolation is capped"), NearMove.GetTravelTarget().X <= 400.0 + 140.0 * 1.25 + 1.0);

    // 10 m behind and unseen (nothing renders here): the one teleport.
    APWNpcCharacter* Far = SpawnPerson(*Holder.World, FVector(0.0, 5000.0, 0.0));
    UPWNpcMovementComponent& FarMove = *Far->GetNpcMovement();
    FarMove.SetIntent(TravelIntent(500));
    TArray<FVector> Route = {FVector(0.0, 5000.0, 0.0), FVector(2000.0, 5000.0, 0.0)};
    FarMove.SetRoute(Route);
    Run(FarMove, 4, 0.05f);
    TestNearlyEqual(TEXT("moved to the route point"), Far->GetActorLocation().X, 1000.0, 30.0);
    TestNearlyEqual(TEXT("on the route line"), Far->GetActorLocation().Y, 5000.0, 0.01);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcBlockedWaitTest, "PerfectWorld.NpcMovement.BlockedPeopleWaitVisiblyAndNeverTeleportThroughWalls",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcBlockedWaitTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcMoveTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    AActor* Wall = SpawnWall(*Holder.World);
    APWNpcCharacter* Npc = SpawnPerson(*Holder.World);
    if (!TestNotNull(TEXT("wall"), Wall) || !TestNotNull(TEXT("person"), Npc))
    {
        return false;
    }
    UPWNpcMovementComponent& Move = *Npc->GetNpcMovement();
    TArray<int32> Events; // 1 for blocked, 0 for cleared
    Move.OnBlockChanged.AddLambda([&Events](const bool bBlocked, FName) { Events.Add(bBlocked ? 1 : 0); });

    FPWNpcIntentView Intent = TravelIntent(300);
    Move.SetIntent(Intent);
    Move.SetRoute(Line(1000.0));
    Run(Move, 10, 0.1f);
    TestTrue(TEXT("after a second the wait is visible"), Move.IsYielding());
    TestTrue(TEXT("presented as Yield"), Move.GetPresentedState() == EPWNpcState::Yield);
    TestFalse(TEXT("not yet reported"), Move.IsPersistentlyBlocked());
    TestFalse(TEXT("not walking into it"), Move.IsMoving());

    Run(Move, 50, 0.1f);
    TestTrue(TEXT("after four seconds the block is persistent"), Move.IsPersistentlyBlocked());
    TestEqual(TEXT("reported once"), Events.Num(), 1);
    TestTrue(TEXT("a raised hand"), Move.GetPose().ArmPitch[1] > 30.0f);

    // The core's target runs 10 m ahead while they are stuck: still no teleport through the wall.
    Intent.ProgressPermille = 1000;
    Move.SetIntent(Intent);
    Run(Move, 10, 0.1f);
    TestNearlyEqual(TEXT("has not moved"), Npc->GetActorLocation().X, 0.0, 0.01);

    // The wall goes; the wait ends, the report clears, and only now may the far, unseen person be placed.
    Wall->SetActorLocation(FVector(0.0, 0.0, -100000.0));
    Run(Move, 6, 0.1f);
    TestFalse(TEXT("no longer yielding"), Move.IsYielding());
    TestFalse(TEXT("no longer blocked"), Move.IsPersistentlyBlocked());
    TestEqual(TEXT("two reports in all"), Events.Num(), 2);
    TestEqual(TEXT("the second says cleared"), Events.Last(), 0);
    TestNearlyEqual(TEXT("then placed at the core's point"), Npc->GetActorLocation().X, 1000.0, 5.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcWorkAndChatTest, "PerfectWorld.NpcMovement.WorkGestureThenChatThenResume",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcWorkAndChatTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcMoveTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    APWNpcCharacter* Npc = SpawnPerson(*Holder.World);
    if (!TestNotNull(TEXT("person"), Npc))
    {
        return false;
    }
    UPWNpcMovementComponent& Move = *Npc->GetNpcMovement();
    Move.SetAnchorResolver([](const FName Id, FTransform& Out) {
        if (Id != FName(TEXT("anchor.test.counter")))
        {
            return false;
        }
        Out = FTransform(FRotator(0.0, 90.0, 0.0), FVector::ZeroVector);
        return true;
    });
    int32 PoseCount = 0;
    Move.OnPose.AddLambda([&PoseCount](const FPWNpcPose&) { ++PoseCount; });

    FPWNpcIntentView Work;
    Work.State = EPWNpcState::Work;
    Work.AnchorId = FName(TEXT("anchor.test.counter"));
    Work.WorkActionId = FName(TEXT("work.serve"));
    Work.bTalkable = true;
    Move.SetIntent(Work);
    Run(Move, 10, 0.05f);
    TestTrue(TEXT("working"), Move.GetPresentedState() == EPWNpcState::Work);
    TestTrue(TEXT("the serving arm is raised"), Move.GetPose().ArmPitch[1] > 5.0f);
    TestTrue(TEXT("poses were published"), PoseCount > 0);
    TestFalse(TEXT("standing at the anchor is not moving"), Move.IsMoving());
    Run(Move, 10, 0.05f);
    TestTrue(TEXT("they turned to the anchor's facing"), Npc->GetActorRotation().Yaw > 30.0);

    // A conversation: hold. Not reduced: the stroke in hand is finished first (at most 0.8 s), then the arms relax.
    Move.SetLocalConversationActive(true);
    TestTrue(TEXT("chatting"), Move.GetPresentedState() == EPWNpcState::Chat);
    Run(Move, 4, 0.05f);
    if (!UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        TestTrue(TEXT("still finishing the stroke"), Move.GetPose().ArmPitch[1] > 12.0f);
    }
    Run(Move, 30, 0.05f);
    TestTrue(TEXT("then at rest"), FMath::Abs(Move.GetPose().ArmPitch[1]) < 8.0f);
    TestFalse(TEXT("not walking away"), Move.IsMoving());

    // The core's Chat intent, then the same Work intent again: the gesture resumes.
    FPWNpcIntentView Chat = Work;
    Chat.State = EPWNpcState::Chat;
    Chat.ResumeState = EPWNpcState::Work;
    Chat.ResumeWorkActionId = Work.WorkActionId;
    Move.SetIntent(Chat);
    Move.SetLocalConversationActive(false);
    TestTrue(TEXT("the core's Chat still holds them"), Move.GetPresentedState() == EPWNpcState::Chat);
    Move.SetIntent(Work);
    TestTrue(TEXT("resumed"), Move.GetPresentedState() == EPWNpcState::Work);
    Run(Move, 20, 0.05f);
    TestTrue(TEXT("the serving arm is raised again"), Move.GetPose().ArmPitch[1] > 5.0f);

    // Sleep: lies down at the anchor.
    FPWNpcIntentView Sleep;
    Sleep.State = EPWNpcState::Sleep;
    Sleep.AnchorId = Work.AnchorId;
    Sleep.bTalkable = false;
    Sleep.DeclineReasonKey = FName(TEXT("npc.reason.asleep"));
    Move.SetIntent(Sleep);
    Run(Move, 80, 0.05f);
    TestTrue(TEXT("lying"), Move.GetPose().Lie > 0.9f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcDeclineTextTest, "PerfectWorld.NpcMovement.NotTalkableKeepsTheVerbAndDeclinesPolitely",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcDeclineTextTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcMoveTest;
    FTestWorld Holder;
    if (!TestNotNull(TEXT("test world"), Holder.World))
    {
        return false;
    }
    APWNpcCharacter* Npc = SpawnPerson(*Holder.World);
    if (!TestNotNull(TEXT("person"), Npc))
    {
        return false;
    }
    TestTrue(TEXT("with no intent they can be talked to"), Npc->GetInteractionVerb(nullptr).ToString().Contains(TEXT("Test")));
    TestTrue(TEXT("and nothing declines"), Npc->GetDeclineText().IsEmpty());

    FPWNpcIntentView Asleep;
    Asleep.State = EPWNpcState::Sleep;
    Asleep.bTalkable = false;
    Asleep.DeclineReasonKey = FName(TEXT("npc.reason.asleep"));
    Npc->GetNpcMovement()->SetIntent(Asleep);
    TestTrue(TEXT("a sleeper is still targeted with the same verb"), Npc->GetInteractionVerb(nullptr).ToString().Contains(TEXT("Talk to Test")));
    TestTrue(TEXT("the notice names them"), Npc->GetDeclineText().ToString().Contains(TEXT("Test")));
    TestTrue(TEXT("and says why"), Npc->GetDeclineText().ToString().Contains(TEXT("asleep")));

    FPWNpcIntentView Walking;
    Walking.State = EPWNpcState::Travel;
    Walking.bTalkable = true;
    Walking.DeclineReasonKey = FName(TEXT("npc.reason.walking"));
    Npc->GetNpcMovement()->SetIntent(Walking);
    TestFalse(TEXT("a walker can still be greeted"), Npc->GetInteractionVerb(nullptr).IsEmpty());
    TestTrue(TEXT("and the polite decline has words"), !Npc->GetDeclineText().IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
