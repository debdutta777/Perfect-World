#include "PerfectWorldAcceptanceSubsystem.h"

#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameClock.h"
#include "NpcBehaviour.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"

// stationery_shift: the stationery shop on a Tuesday at 16:40 (Anindita at the counter from 15:00 to 17:00, Mira at the packing desk
// from 16:30 to 18:00 on Tuesdays and Thursdays; npc_schedule.anindita_sen and npc_schedule.mira_sen). A skeleton: it gets to the
// customer's spot through the real door, looks, takes a screenshot and records where the two people stand.

namespace
{
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

constexpr double AwStOutsideCm = 150.0;     // the declared start: this far in front of the threshold anchor, on the footpath
constexpr double AwStThresholdTolerance = 50.0;
constexpr double AwStCustomerTolerance = 60.0;
constexpr double AwStAtAnchorCm = 250.0;    // "at the counter" / "at the packing desk": within this of the anchor, on the floor plan
constexpr int32 AwStMaxRounds = 64;

/** The building exactly as PerfectWorldKolkataLane.cpp recovers it from the recorded frontage (side +1, index 2, X -100 to 600). */
PWLane::FLaneBuilding AwStBuilding()
{
    PWLane::FLaneBuilding Building;
    Building.Frame = PWLane::MakeLaneFrame(-100.0, 460.0, FVector::ForwardVector, FVector::RightVector);
    Building.Width = 700.0;
    Building.Depth = 700.0;
    Building.Setback = 60.0;
    Building.Storeys = 3;
    Building.Side = 1;
    Building.Index = 2;
    return Building;
}

bool AwStAnchor(const TCHAR* PublicId, PWLane::FStationeryAnchor& Out)
{
    const PWLane::FLaneBuilding Building = AwStBuilding();
    if (!PWLane::IsStationeryBuilding(Building))
    {
        return false;
    }
    TArray<PWLane::FStationeryAnchor> Anchors;
    PWLane::GetStationeryAnchors(Building, Anchors);
    for (const PWLane::FStationeryAnchor& Anchor : Anchors)
    {
        if (Anchor.Id == FName(PublicId))
        {
            Out = Anchor;
            return true;
        }
    }
    return false;
}

APWDoorActor* AwStFrontDoor(UWorld* World)
{
    if (World == nullptr)
    {
        return nullptr;
    }
    for (TActorIterator<APWDoorActor> It(World); It; ++It)
    {
        if (It->GetDoorId() == FName(PWLane::StationeryDoorId))
        {
            return *It;
        }
    }
    return nullptr;
}

/** A copy of the live world moved to Target by the core's own commands (time.advance, then time.settle), or null with the core's code. */
TUniquePtr<PerfectWorld::WorldSim> AwStAdvancedCopy(const PerfectWorld::WorldState& Live, const PerfectWorld::GameInstant Target, FString& Why)
{
    auto Sim = MakeUnique<PerfectWorld::WorldSim>(Live);
    const auto Settle = [&Sim]
    {
        for (int32 Round = 0; Round < AwStMaxRounds && !PerfectWorld::IsWorldQuiescent(Sim->State()); ++Round)
        {
            PerfectWorld::Command Command;
            Command.Source = PerfectWorld::CommandSource::System;
            Command.Payload = PerfectWorld::SettleDueEvents{};
            if (!Sim->Submit(Command).Accepted)
            {
                return;
            }
        }
    };
    Settle();
    for (int32 Round = 0; Round < AwStMaxRounds && Sim->State().Clock.Now() < Target; ++Round)
    {
        PerfectWorld::AdvanceTime Advance;
        Advance.Minutes = Target - Sim->State().Clock.Now();
        PerfectWorld::Command Command;
        Command.Source = PerfectWorld::CommandSource::System;
        Command.Payload = Advance;
        const PerfectWorld::CommandResult Result = Sim->Submit(Command);
        if (!Result.Accepted)
        {
            Why = FString::Printf(TEXT("the core refused time.advance: %hs"), Result.Why.Code.c_str());
            return nullptr;
        }
        Settle();
    }
    if (Sim->State().Clock.Now() != Target)
    {
        Why = FString::Printf(TEXT("the advance stopped at instant %lld, not %lld"), static_cast<long long>(Sim->State().Clock.Now()), static_cast<long long>(Target));
        return nullptr;
    }
    return Sim;
}

PWAcceptance::FStepDef AwStJumpClockStep()
{
    PWAcceptance::FStepDef Def;
    Def.Name = TEXT("clock_to_tuesday_1640");
    Def.TimeoutSeconds = 10.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime&) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Life == nullptr || Pawn == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no lane-life subsystem or no pawn"));
        }
        if (Pawn->IsSeated() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() || Life->HasMovingDoors())
        {
            return FStepOutcome::Fail(TEXT("the world is not at rest (seated, in conversation or a door moving), so the clock is not moved"));
        }
        // The next Tuesday after the campaign start (Monday 2 June 2042), through the core's own calendar.
        const PerfectWorld::GameInstant Target = PerfectWorld::GameClock::FromCalendar(
            PerfectWorld::CampaignStartYear, PerfectWorld::CampaignStartMonth, PerfectWorld::CampaignStartDay + 1, 16, 40);
        if (PerfectWorld::GameClock::ToCalendar(Target).DayOfWeek != PerfectWorld::Weekday::Tuesday)
        {
            return FStepOutcome::Fail(TEXT("the calendar says the target day is not a Tuesday"));
        }
        const FString Before = Life->GetClockText().ToString();
        if (Life->GetNow() >= Target)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the clock already stands at %s, at or after the target"), *Before));
        }
        // The lane clock has no public jump. The public seams it does have: a copy of its state, moved by the core's own commands,
        // adopted the way a loaded save is (doors snapped to the core's state, people re-presented at the new instant).
        FString Why;
        TUniquePtr<PerfectWorld::WorldSim> Moved = AwStAdvancedCopy(Life->GetWorldState(), Target, Why);
        if (!Moved.IsValid())
        {
            return FStepOutcome::Fail(Why);
        }
        Life->AdoptSavedWorld(MoveTemp(Moved));
        if (Life->GetNow() != Target)
        {
            return FStepOutcome::Fail(TEXT("the clock is not at the target after adopting the moved world"));
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("clock moved from %s to %s through time.advance and AdoptSavedWorld"), *Before, *Life->GetClockText().ToString()));
    };
    return Def;
}

PWAcceptance::FStepDef AwStDeclaredStartStep()
{
    PWAcceptance::FStepDef Def;
    Def.Name = TEXT("declared_start_outside_shop");
    Def.TimeoutSeconds = 15.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no pawn"));
        }
        if (Rt.Phase == 0)
        {
            PWLane::FStationeryAnchor Door;
            if (!AwStAnchor(PWLane::StationeryAnchorFrontDoor, Door))
            {
                return FStepOutcome::Fail(TEXT("the stationery front-door anchor does not resolve (the building footprint changed)"));
            }
            const FVector Start = Door.World + FVector(0.0, -AwStOutsideCm, PWPlayer::CapsuleHalfHeight + 5.0);
            // The one teleport of this scenario: its declared start, on the footpath facing the shop. Every later move is walked.
            Pawn->TeleportTo(Start, FRotator(0.0, 90.0, 0.0));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Sub.FaceWorldPoint(Door.World);
            Rt.Memo = FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), Start.X, Start.Y, Start.Z);
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround())
        {
            return FStepOutcome::Wait();
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("declared start %s, %.0f cm in front of the shop threshold; standing on the ground"), *Rt.Memo, AwStOutsideCm));
    };
    return Def;
}

PWAcceptance::FStepDef AwStFaceCounterStep()
{
    PWAcceptance::FStepDef Def;
    Def.Name = TEXT("face_counter");
    Def.TimeoutSeconds = 5.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime&) -> FStepOutcome
    {
        PWLane::FStationeryAnchor Counter;
        if (!AwStAnchor(PWLane::StationeryAnchorCounter, Counter))
        {
            return FStepOutcome::Fail(TEXT("the counter anchor does not resolve"));
        }
        Sub.FaceWorldPoint(Counter.World);
        return FStepOutcome::Pass(TEXT("the view is turned to the counter"));
    };
    return Def;
}

/** Waits for the person to be presented, then passes when they stand within AwStAtAnchorCm (XY) of the anchor. */
PWAcceptance::FStepDef AwStObserveStep(const FString& StepName, const TCHAR* PersonId, const TCHAR* AnchorId)
{
    PWAcceptance::FStepDef Def;
    Def.Name = StepName;
    Def.TimeoutSeconds = 12.0;
    const FName Person(PersonId);
    const FName Anchor(AnchorId);
    Def.Run = [Person, Anchor](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        const UPWNpcPresenceSubsystem* Presence = World != nullptr ? World->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        if (Presence == nullptr || Life == nullptr)
        {
            return FStepOutcome::Fail(TEXT("no NPC presence subsystem or lane-life subsystem"));
        }
        PWLane::FStationeryAnchor Where;
        if (!AwStAnchor(*Anchor.ToString(), Where))
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the anchor %s does not resolve"), *Anchor.ToString()));
        }
        const FString Clock = Life->GetClockText().ToString();
        const APWNpcCharacter* Npc = Presence->FindPerson(Person);
        if (Npc == nullptr)
        {
            Rt.Note = FString::Printf(TEXT("%s is not on the lane at %s (registered: %d)"), *Person.ToString(), *Clock, Presence->IsRegistered(Person));
            return FStepOutcome::Wait();
        }
        const FVector At = Npc->GetActorLocation();
        const double Away = FVector::Dist2D(At, Where.World);
        const PerfectWorld::NpcIntent* Intent = Presence->FindIntent(Person);
        const FString IntentText = Intent != nullptr
            ? FString::Printf(TEXT("intent state %d at '%s'"), static_cast<int32>(Intent->State), UTF8_TO_TCHAR(Intent->AnchorId.c_str()))
            : FString(TEXT("no intent"));
        const FString Facts = FString::Printf(TEXT("%s at (%.0f, %.0f, %.0f), %.0f cm from %s (%.0f, %.0f), %s, clock %s"), *Person.ToString(), At.X, At.Y,
                                              At.Z, Away, *Anchor.ToString(), Where.World.X, Where.World.Y, *IntentText, *Clock);
        Rt.Note = Facts;
        if (Away > AwStAtAnchorCm)
        {
            return FStepOutcome::Wait(); // she may still be walking in
        }
        return FStepOutcome::Pass(Facts);
    };
    return Def;
}
} // namespace

namespace PWAcceptance
{
void AppendStationeryShiftSteps(TArray<FStepDef>& OutSteps)
{
    OutSteps.Add(MakeWorldReadyStep());
    OutSteps.Add(MakeEnterWorldStep());
    OutSteps.Add(AwStDeclaredStartStep());
    OutSteps.Add(MakeWalkStep(TEXT("walk_to_shop_threshold"),
        [](UPWAcceptanceSubsystem&, FVector& OutTarget, FString& Why)
        {
            PWLane::FStationeryAnchor Door;
            if (!AwStAnchor(PWLane::StationeryAnchorFrontDoor, Door))
            {
                Why = TEXT("the stationery front-door anchor does not resolve");
                return false;
            }
            OutTarget = Door.World;
            return true;
        },
        AwStThresholdTolerance, 25.0));
    // The clock moves only now, at the door, so the walk from here to the counter keeps the schedule's window (16:30 to 17:00) open.
    OutSteps.Add(AwStJumpClockStep());
    OutSteps.Add(MakeInteractStep(TEXT("open_shop_door"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return AwStFrontDoor(Sub.GetPlayWorld()); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWDoorActor* Door = AwStFrontDoor(Sub.GetPlayWorld());
            if (Door == nullptr || !Door->IsOpen())
            {
                return false;
            }
            Detail = TEXT("door.nabapara.stationery_front is open");
            return true;
        },
        [](UPWAcceptanceSubsystem& Sub)
        {
            const APWDoorActor* Door = AwStFrontDoor(Sub.GetPlayWorld());
            return Door != nullptr && Door->IsOpen();
        },
        12.0));
    OutSteps.Add(MakeWalkStep(TEXT("walk_to_customer_spot"),
        [](UPWAcceptanceSubsystem&, FVector& OutTarget, FString& Why)
        {
            PWLane::FStationeryAnchor Customer;
            if (!AwStAnchor(PWLane::StationeryAnchorCustomer, Customer))
            {
                Why = TEXT("the stationery customer anchor does not resolve");
                return false;
            }
            OutTarget = Customer.World;
            return true;
        },
        AwStCustomerTolerance, 30.0));
    OutSteps.Add(AwStFaceCounterStep());
    OutSteps.Add(MakeShotStep(TEXT("shot_shop_interior")));
    OutSteps.Add(AwStObserveStep(TEXT("anindita_at_counter"), TEXT("person.anindita_sen"), PWLane::StationeryAnchorCounter));
    OutSteps.Add(AwStObserveStep(TEXT("mira_at_packing_desk"), TEXT("person.mira_sen"), PWLane::StationeryAnchorPackingDesk));
}
} // namespace PWAcceptance
