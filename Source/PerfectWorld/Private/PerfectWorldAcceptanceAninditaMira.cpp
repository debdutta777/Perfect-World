#include "PerfectWorldAcceptanceStationery.h"

#include "Engine/World.h"
#include "LaneInteriorLayout.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "WorldState.h"

// anindita_mira (NR-04): the stationery shop on a Tuesday afternoon, from the customer's side. It uses the shared steps of
// PerfectWorldAcceptanceStationery.h and adds what stationery_shift only sketched:
//   Anindita at the counter, Talk and Leave with her; the player takes the staff gap to the partition doorway and stands in it;
//   Mira arrives at the threshold (she was off the lane), walks in and is held by the player in the doorway (the yield); the player
//   steps aside and she reaches the packing desk; Talk and Leave with her; she resumes packing; the player walks out of her way, the
//   clock moves to 17:55 and she leaves through the front door, walked, not removed.
// The door, the staff gap and the partition doorway are the shop's real ones (the kit layout the build also reads). Every move of the
// player is walked except the one declared start. The clock runs one game minute per real second unless the window is unfocused
// (then it is held, and a step that waits says so).

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

const FName AmAninditaId(TEXT("person.anindita_sen"));
const FName AmMiraId(TEXT("person.mira_sen"));

constexpr double AmWaypointTolerance = 40.0;
constexpr double AmAtAnchorCm = 120.0;
constexpr double AmArrivalMaxFromThresholdCm = 350.0;
constexpr double AmHoldSeconds = 3.0;
constexpr double AmNearCm = 300.0;
constexpr double AmCapsuleOverlapCm = 50.0; // two 34 cm capsules touch at 68; closer than this means they went through each other

/** The shop's floor plan in world space: the walking line the player takes through the staff gap to the partition doorway. */
struct FAmPlan
{
    FVector Customer;     // the customer spot in front of the counter
    FVector FrontOfGap;   // the staff gap's mouth, level with the customer spot
    FVector BehindCounter; // inside the gap, level with the counter anchor
    FVector Doorway;      // the partition doorway's centre
    FVector Corner;       // the corner of Mira's route just inside the stock room
    FVector FrontDoor;
    double DoorwayY = 0.0;
};

bool AmMakePlan(FAmPlan& Out, FString& Why)
{
    using PWAcceptance::Stationery::FindAnchor;
    PWLane::FStationeryAnchor Door, Counter, Customer;
    if (!FindAnchor(PWLane::StationeryAnchorFrontDoor, Door) || !FindAnchor(PWLane::StationeryAnchorCounter, Counter) ||
        !FindAnchor(PWLane::StationeryAnchorCustomer, Customer))
    {
        Why = TEXT("a stationery anchor does not resolve");
        return false;
    }
    // The shop is axis aligned (U = +X, V = +Y), so a building-local point is the front door's world point plus the local difference.
    const auto ToWorld = [&Door](const double U, const double V)
    {
        return Door.World + FVector(U - Door.Local.X, V - Door.Local.Y, 0.0);
    };
    if (FVector::Dist2D(ToWorld(Counter.Local.X, Counter.Local.Y), Counter.World) > 2.0)
    {
        Why = TEXT("the layout-to-world mapping disagrees with the counter anchor by more than 2 cm (the shop is no longer axis aligned)");
        return false;
    }
    const PerfectWorld::LaneInterior::InteriorLayout& Layout = PWLane::GetStationeryLayout();
    const PerfectWorld::LaneInterior::Opening* Partition = nullptr;
    for (const PerfectWorld::LaneInterior::Opening& Opening : Layout.Openings)
    {
        if (Opening.Kind != PerfectWorld::LaneInterior::OpeningKind::Window && Opening.DoorId.empty() && Opening.RoomA != "outside")
        {
            Partition = &Opening;
            break;
        }
    }
    const PerfectWorld::LaneInterior::Route* Route = PerfectWorld::LaneInterior::FindRoute(Layout, Layout.BuildingId + ".route.counter_to_packing_desk");
    if (Partition == nullptr || Route == nullptr || Route->Points.size() < 4)
    {
        Why = TEXT("the layout has no partition doorway or no counter_to_packing_desk route");
        return false;
    }
    const double DoorwayU = (Partition->Plan.U0 + Partition->Plan.U1) * 0.5;
    const double DoorwayV = (Partition->Plan.V0 + Partition->Plan.V1) * 0.5;
    Out.Customer = Customer.World;
    Out.FrontOfGap = ToWorld(Route->Points[1].U, Customer.Local.Y);
    Out.BehindCounter = ToWorld(Route->Points[1].U, Route->Points[1].V);
    Out.Doorway = ToWorld(DoorwayU, DoorwayV);
    Out.Corner = ToWorld(Route->Points[2].U, Route->Points[2].V);
    Out.FrontDoor = Door.World;
    Out.DoorwayY = Out.Doorway.Y;
    return true;
}

FStepDef AmWalkToPlan(const FString& Name, TFunction<FVector(const FAmPlan&)> Pick)
{
    return PWAcceptance::MakeWalkStep(Name,
        [Pick](UPWAcceptanceSubsystem&, FVector& OutTarget, FString& Why)
        {
            FAmPlan Plan;
            if (!AmMakePlan(Plan, Why)) { return false; }
            OutTarget = Pick(Plan);
            return true;
        },
        AmWaypointTolerance, 25.0);
}

APWNpcCharacter* AmFind(UPWAcceptanceSubsystem& Sub, const FName Person)
{
    UWorld* World = Sub.GetPlayWorld();
    const UPWNpcPresenceSubsystem* Presence = World != nullptr ? World->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    return Presence != nullptr ? Presence->FindPerson(Person) : nullptr;
}

FString AmClockNote(UPWAcceptanceSubsystem& Sub)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    if (Life == nullptr) { return FString(); }
    return FString::Printf(TEXT("clock %s, hold %d"), *Life->GetClockText().ToString(), static_cast<int32>(Life->GetClockHold()));
}

bool AmConversationIs(UPWAcceptanceSubsystem& Sub, const FName Person)
{
    UWorld* World = Sub.GetPlayWorld();
    const UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(World);
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
    return Presenter != nullptr && Presenter->IsOpen() && Life != nullptr &&
           FName(UTF8_TO_TCHAR(std::string(PerfectWorld::OpenConversationPersonId(Life->GetWorldState())).c_str())) == Person;
}

FStepDef AmTalkStep(const FString& Name, const FName Person)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Person](UPWAcceptanceSubsystem& Sub) -> AActor* { return AmFind(Sub, Person); },
        [Person](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            if (!AmConversationIs(Sub, Person)) { return false; }
            Detail = FString::Printf(TEXT("the conversation panel is open and the core's open conversation is %s"), *Person.ToString());
            return true;
        },
        nullptr, 20.0);
}

/** The panel's Leave action (what Escape and B call), then the core's conversation closed. */
FStepDef AmLeaveStep(const FString& Name, const FName Person)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 10.0;
    Def.Run = [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(World);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        if (Presenter == nullptr || Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no conversation presenter or lane-life subsystem"));
        }
        if (Rt.Phase == 0)
        {
            if (!AmConversationIs(Sub, Person))
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("no open conversation with %s to leave"), *Person.ToString()));
            }
            Presenter->LeaveConversation();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        const bool bCoreOpen = !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty();
        Rt.Note = FString::Printf(TEXT("panel open %d, core conversation open %d"), Presenter->IsOpen(), bCoreOpen);
        if (Presenter->IsOpen() || bCoreOpen) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("Leave used: the panel is closed and the core has no open conversation (%s)"), *AmClockNote(Sub)));
    };
    return Def;
}

/** After a conversation: the person is back within ToleranceCm of the anchor, no longer in the Chat pose, and has stayed so for 2 s. */
FStepDef AmResumeStep(const FString& Name, const FName Person, const TCHAR* AnchorId)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 40.0;
    const FName Anchor(AnchorId);
    Def.Run = [Person, Anchor](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        PWLane::FStationeryAnchor Where;
        if (!PWAcceptance::Stationery::FindAnchor(*Anchor.ToString(), Where))
        {
            return FStepOutcome::Fail(TEXT("the anchor does not resolve"));
        }
        const APWNpcCharacter* Npc = AmFind(Sub, Person);
        if (Npc == nullptr)
        {
            Rt.Phase = 0;
            Rt.Note = FString::Printf(TEXT("%s is not on the lane; %s"), *Person.ToString(), *AmClockNote(Sub));
            return FStepOutcome::Wait();
        }
        const double Away = FVector::Dist2D(Npc->GetActorLocation(), Where.World);
        const EPWNpcState State = Npc->GetNpcMovement()->GetPresentedState();
        Rt.Note = FString::Printf(TEXT("%s %.0f cm from %s, presented state %d, %s"), *Person.ToString(), Away, *Anchor.ToString(), static_cast<int32>(State), *AmClockNote(Sub));
        if (Away > AmAtAnchorCm || State == EPWNpcState::Chat)
        {
            Rt.Phase = 0;
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 0)
        {
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        return Rt.InPhase() >= 2.0 ? FStepOutcome::Pass(Rt.Note) : FStepOutcome::Wait();
    };
    return Def;
}

/** Mira is first presented near the shop threshold (she walks in; she is not placed inside). */
FStepDef AmArrivalStep()
{
    FStepDef Def;
    Def.Name = TEXT("mira_arrives_at_threshold");
    Def.TimeoutSeconds = 90.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        const APWNpcCharacter* Npc = AmFind(Sub, AmMiraId);
        if (Npc == nullptr)
        {
            Rt.Note = FString::Printf(TEXT("Mira is not on the lane yet; %s"), *AmClockNote(Sub));
            return FStepOutcome::Wait();
        }
        FAmPlan Plan;
        FString Why;
        if (!AmMakePlan(Plan, Why)) { return FStepOutcome::Fail(Why); }
        const double FromDoor = FVector::Dist2D(Npc->GetActorLocation(), Plan.FrontDoor);
        if (FromDoor > AmArrivalMaxFromThresholdCm)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("Mira first appears %.0f cm from the threshold (more than %.0f): she was placed, not walked in; %s"),
                                                      FromDoor, AmArrivalMaxFromThresholdCm, *AmClockNote(Sub)));
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("Mira (%.0f cm tall) first appears %.0f cm from the front-door threshold at (%.0f, %.0f); %s"),
                                                  Npc->GetStandingHeightCm(), FromDoor, Npc->GetActorLocation().X, Npc->GetActorLocation().Y, *AmClockNote(Sub)));
    };
    return Def;
}

/**
 * The player stands in the partition doorway while Mira comes to it. Passes when she has been within 3 m on the shop-floor side for
 * 3 s without getting past or through the player. Fails if she ends up beyond the partition, or inside the player's capsule.
 * Rt.Target scratch: X the closest she came, Y seconds near, Z unused.
 */
FStepDef AmYieldStep()
{
    FStepDef Def;
    Def.Name = TEXT("mira_yields_at_partition");
    Def.TimeoutSeconds = 90.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        FAmPlan Plan;
        FString Why;
        if (Pawn == nullptr || !AmMakePlan(Plan, Why)) { return FStepOutcome::Fail(Pawn == nullptr ? FString(TEXT("no pawn")) : Why); }
        if (!Rt.bHaveTarget)
        {
            Rt.Target = FVector(1.0e9, 0.0, 0.0);
            Rt.bHaveTarget = true;
        }
        if (FVector::Dist2D(Pawn->GetActorLocation(), Plan.Doorway) > 70.0)
        {
            return FStepOutcome::Fail(TEXT("the player is no longer standing in the partition doorway"));
        }
        const APWNpcCharacter* Npc = AmFind(Sub, AmMiraId);
        if (Npc == nullptr)
        {
            Rt.Note = FString::Printf(TEXT("Mira is not on the lane; %s"), *AmClockNote(Sub));
            return FStepOutcome::Wait();
        }
        const FVector At = Npc->GetActorLocation();
        const double Apart = FVector::Dist2D(At, Pawn->GetActorLocation());
        Rt.Target.X = FMath::Min(Rt.Target.X, Apart);
        const EPWNpcState State = Npc->GetNpcMovement()->GetPresentedState();
        Rt.Note = FString::Printf(TEXT("Mira %.0f cm from the player, closest %.0f, near for %.1f s, state %d, yielding %d, y %.0f vs partition %.0f; %s"),
                                  Apart, Rt.Target.X, Rt.Target.Y, static_cast<int32>(State), Npc->GetNpcMovement()->IsYielding(), At.Y, Plan.DoorwayY, *AmClockNote(Sub));
        if (At.Y > Plan.DoorwayY + 30.0)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("Mira is beyond the partition while the player stands in the doorway: %s"), *Rt.Note));
        }
        if (Apart < AmCapsuleOverlapCm)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("Mira is inside the player's capsule: %s"), *Rt.Note));
        }
        if (Apart <= AmNearCm)
        {
            Rt.Target.Y += Rt.Dt;
        }
        if (Rt.Target.Y < AmHoldSeconds) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("held back for %.1f s within %.0f cm of the player in the doorway, never closer than %.0f cm; %s"),
                                                  Rt.Target.Y, AmNearCm, Rt.Target.X, *Rt.Note));
    };
    return Def;
}

/** Mira walks desk to door and leaves the lane: she is seen at least 150 cm from the desk and within 250 cm of the threshold first. */
FStepDef AmDepartureStep()
{
    FStepDef Def;
    Def.Name = TEXT("mira_departs_through_door");
    Def.TimeoutSeconds = 150.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        FAmPlan Plan;
        FString Why;
        PWLane::FStationeryAnchor Desk;
        if (!AmMakePlan(Plan, Why) || !PWAcceptance::Stationery::FindAnchor(PWLane::StationeryAnchorPackingDesk, Desk))
        {
            return FStepOutcome::Fail(Why.IsEmpty() ? FString(TEXT("the desk anchor does not resolve")) : Why);
        }
        if (!Rt.bHaveTarget)
        {
            Rt.Target = FVector(0.0, 1.0e9, 0.0); // X: farthest from the desk, Y: closest to the threshold
            Rt.bHaveTarget = true;
        }
        const APWNpcCharacter* Npc = AmFind(Sub, AmMiraId);
        if (Npc == nullptr)
        {
            if (Rt.Target.X < 150.0)
            {
                // Not yet presented at all, or gone without a walk.
                return Rt.Elapsed < 3.0 ? FStepOutcome::Wait()
                                        : FStepOutcome::Fail(FString::Printf(TEXT("Mira vanished after moving only %.0f cm from the desk: she was removed, not walked out; %s"), Rt.Target.X, *AmClockNote(Sub)));
            }
            if (Rt.Target.Y > 250.0)
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("Mira left the lane but was never seen within 250 cm of the threshold (closest %.0f cm)"), Rt.Target.Y));
            }
            return FStepOutcome::Pass(FString::Printf(TEXT("Mira walked up to %.0f cm from the desk, came within %.0f cm of the threshold and left the lane; %s"),
                                                      Rt.Target.X, Rt.Target.Y, *AmClockNote(Sub)));
        }
        const FVector At = Npc->GetActorLocation();
        Rt.Target.X = FMath::Max(Rt.Target.X, FVector::Dist2D(At, Desk.World));
        Rt.Target.Y = FMath::Min(Rt.Target.Y, FVector::Dist2D(At, Plan.FrontDoor));
        Rt.Note = FString::Printf(TEXT("Mira at (%.0f, %.0f), farthest from the desk %.0f, closest to the threshold %.0f; %s"), At.X, At.Y, Rt.Target.X, Rt.Target.Y, *AmClockNote(Sub));
        return FStepOutcome::Wait();
    };
    return Def;
}

/** The player walks out to the footpath, off to the side of the threshold, so Mira's way out is clear. */
FStepDef AmClearThresholdStep()
{
    return PWAcceptance::MakeWalkStep(TEXT("walk_out_of_mira_way"),
        [](UPWAcceptanceSubsystem&, FVector& OutTarget, FString& Why)
        {
            FAmPlan Plan;
            if (!AmMakePlan(Plan, Why)) { return false; }
            OutTarget = Plan.FrontDoor + FVector(-250.0, -170.0, 0.0);
            return true;
        },
        60.0, 30.0);
}
} // namespace

namespace PWAcceptance
{
void AppendAninditaMiraSteps(TArray<FStepDef>& OutSteps)
{
    using namespace Stationery;
    OutSteps.Add(MakeWorldReadyStep());
    OutSteps.Add(MakeEnterWorldStep());
    OutSteps.Add(DeclaredStartStep());
    OutSteps.Add(WalkToAnchorStep(TEXT("walk_to_shop_threshold"), PWLane::StationeryAnchorFrontDoor, 50.0, 25.0));
    // 16:12: Mira, who walks 40 minutes from school, is still off the lane; she appears at the threshold about 16:24.
    OutSteps.Add(JumpClockStep(TEXT("clock_to_tuesday_1612"), 16, 12));
    OutSteps.Add(OpenDoorStep());
    OutSteps.Add(WalkToAnchorStep(TEXT("walk_to_customer_spot"), PWLane::StationeryAnchorCustomer, 60.0, 30.0));
    OutSteps.Add(FaceAnchorStep(TEXT("face_counter"), PWLane::StationeryAnchorCounter));
    OutSteps.Add(MakeShotStep(TEXT("shot_anindita_at_counter")));
    OutSteps.Add(ObserveStep(TEXT("anindita_at_counter"), TEXT("person.anindita_sen"), PWLane::StationeryAnchorCounter, AmAtAnchorCm));
    OutSteps.Add(AmTalkStep(TEXT("talk_to_anindita"), AmAninditaId));
    OutSteps.Add(MakeShotStep(TEXT("shot_anindita_conversation")));
    OutSteps.Add(AmLeaveStep(TEXT("leave_anindita"), AmAninditaId));
    OutSteps.Add(AmResumeStep(TEXT("anindita_resumes_counter"), AmAninditaId, PWLane::StationeryAnchorCounter));
    // The staff gap to the partition doorway, then stand in it.
    OutSteps.Add(AmWalkToPlan(TEXT("walk_to_staff_gap"), [](const FAmPlan& Plan) { return Plan.FrontOfGap; }));
    OutSteps.Add(AmWalkToPlan(TEXT("walk_behind_counter"), [](const FAmPlan& Plan) { return Plan.BehindCounter; }));
    OutSteps.Add(AmWalkToPlan(TEXT("walk_to_partition_doorway"), [](const FAmPlan& Plan) { return Plan.Doorway; }));
    OutSteps.Add(FaceAnchorStep(TEXT("face_front_door"), PWLane::StationeryAnchorFrontDoor));
    OutSteps.Add(AmArrivalStep());
    OutSteps.Add(MakeShotStep(TEXT("shot_mira_arrival")));
    OutSteps.Add(AmYieldStep());
    OutSteps.Add(MakeShotStep(TEXT("shot_partition_yield")));
    // Step aside: back through the gap to the customer side, out of Mira's line.
    OutSteps.Add(AmWalkToPlan(TEXT("step_back_from_partition"), [](const FAmPlan& Plan) { return Plan.BehindCounter; }));
    OutSteps.Add(AmWalkToPlan(TEXT("step_out_of_staff_gap"), [](const FAmPlan& Plan) { return Plan.FrontOfGap; }));
    OutSteps.Add(WalkToAnchorStep(TEXT("step_aside_to_customer_spot"), PWLane::StationeryAnchorCustomer, 60.0, 30.0));
    {
        FStepDef Reach = ObserveStep(TEXT("mira_reaches_packing_desk"), TEXT("person.mira_sen"), PWLane::StationeryAnchorPackingDesk, AmAtAnchorCm);
        Reach.TimeoutSeconds = 60.0;
        OutSteps.Add(MoveTemp(Reach));
    }
    OutSteps.Add(MakeShotStep(TEXT("shot_mira_packing")));
    // Follow her in and talk.
    OutSteps.Add(AmWalkToPlan(TEXT("walk_to_staff_gap_again"), [](const FAmPlan& Plan) { return Plan.FrontOfGap; }));
    OutSteps.Add(AmWalkToPlan(TEXT("walk_behind_counter_again"), [](const FAmPlan& Plan) { return Plan.BehindCounter; }));
    OutSteps.Add(AmWalkToPlan(TEXT("walk_through_partition"), [](const FAmPlan& Plan) { return Plan.Doorway; }));
    OutSteps.Add(MakeWalkStep(TEXT("walk_up_to_mira"),
        [](UPWAcceptanceSubsystem& Sub, FVector& OutTarget, FString& Why)
        {
            FAmPlan Plan;
            const APWNpcCharacter* Mira = AmFind(Sub, AmMiraId);
            if (!AmMakePlan(Plan, Why)) { return false; }
            if (Mira == nullptr)
            {
                Why = TEXT("Mira is not on the lane");
                return false;
            }
            const FVector At = Mira->GetActorLocation();
            const double Gap = FVector::Dist2D(Plan.Corner, At);
            OutTarget = At + (Plan.Corner - At).GetSafeNormal2D() * FMath::Min(110.0, Gap);
            return true;
        },
        AmWaypointTolerance, 25.0));
    OutSteps.Add(AmTalkStep(TEXT("talk_to_mira"), AmMiraId));
    OutSteps.Add(MakeShotStep(TEXT("shot_mira_conversation")));
    OutSteps.Add(AmLeaveStep(TEXT("leave_mira"), AmMiraId));
    OutSteps.Add(AmResumeStep(TEXT("mira_resumes_packing"), AmMiraId, PWLane::StationeryAnchorPackingDesk));
    OutSteps.Add(MakeShotStep(TEXT("shot_resumed_packing")));
    // Out of her way, then the end of her shift.
    OutSteps.Add(AmWalkToPlan(TEXT("walk_back_to_partition"), [](const FAmPlan& Plan) { return Plan.Doorway; }));
    OutSteps.Add(AmWalkToPlan(TEXT("walk_back_through_gap"), [](const FAmPlan& Plan) { return Plan.BehindCounter; }));
    OutSteps.Add(AmWalkToPlan(TEXT("walk_back_to_gap_mouth"), [](const FAmPlan& Plan) { return Plan.FrontOfGap; }));
    OutSteps.Add(WalkToAnchorStep(TEXT("walk_to_front_door_again"), PWLane::StationeryAnchorFrontDoor, 50.0, 30.0));
    OutSteps.Add(AmClearThresholdStep());
    OutSteps.Add(FaceAnchorStep(TEXT("face_shop_door"), PWLane::StationeryAnchorFrontDoor));
    OutSteps.Add(JumpClockStep(TEXT("clock_to_tuesday_1755"), 17, 55));
    OutSteps.Add(AmDepartureStep());
    OutSteps.Add(MakeShotStep(TEXT("shot_after_departure")));
}
} // namespace PWAcceptance
