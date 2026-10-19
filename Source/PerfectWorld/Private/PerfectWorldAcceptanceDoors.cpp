#include "PerfectWorldAcceptanceSubsystem.h"

#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameClock.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSubrataBindings.h"

// doors_knock (NR-01): the four private dwelling doors of the lane, knocked at through the REAL interaction path (the E key's
// TryInteract on the door actor) at chosen instants, with the answers recorded and entry checked against what the SCHEDULES say.
//
// The doors: the tea row house (Rupa Ghosh), the two flats of the tall flats (unit A, unit B; reached through the public shared entry
// and passage) and the caretaker's flat (Subrata Dey). For each probe the expected answer is derived from the core, not from the
// presenter: a resident whose schedule home is the door's location, resolved at the knock instant (ResolveNpcIntentAt), is home when
// her intent is a stay there that is idle or work (the lane's own rule, PerfectWorldLaneLifeKnock.cpp); Admitted when home inside
// the door's visiting hours, NotReceiving when home outside them, NoAnswer when nobody is home (including a dwelling that has no
// resident registered yet). The observed answer is the player-facing notice the knock produced. A mismatch is recorded in the ledger
// and the script goes on, so every door is observed; the last step, verdict, fails when the ledger holds any mismatch.
//
// Each probe: the clock is moved forward at the door (the stationery scenario's AdoptSavedWorld method), the player knocks (E on the
// closed door), and presses E again: an admitted knock opens the door (entry), any other answer leaves it shut. The tea row house's
// admitted probe goes on to walk in, close the door from inside, then open it from inside with no knock held (unrestricted exit:
// the core's Exit direction is always Ok) and walk out. The one declared start is outside the row house door; every later move is
// walked (the footpath line Y 340 and the road crossing at the caretaker's flat; the tall flats' public front door is opened by E).

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

constexpr double DkReachSpotCm = 110.0;   // the visitor stands this far in front of the leaf's centre
constexpr double DkInsideSpotCm = 170.0;  // and this far inside it when walking in
constexpr double DkLeafHalfCm = 45.0;
constexpr double DkFootpathY = 340.0;
constexpr double DkRoadFootY = -340.0;
constexpr double DkWalkTolerance = 45.0;

enum class EDkAnswer : uint8 { None, Admitted, NoAnswer, NotReceiving };

const TCHAR* DkAnswerText(const EDkAnswer Answer)
{
    switch (Answer)
    {
    case EDkAnswer::Admitted: return TEXT("admitted");
    case EDkAnswer::NoAnswer: return TEXT("no answer");
    case EDkAnswer::NotReceiving: return TEXT("not receiving");
    default: return TEXT("none");
    }
}

EDkAnswer DkClassify(const FText& Notice)
{
    const FString Text = Notice.ToString();
    if (Text.Contains(TEXT("Someone answers"))) { return EDkAnswer::Admitted; }
    if (Text.Contains(TEXT("Nobody answers"))) { return EDkAnswer::NoAnswer; }
    if (Text.Contains(TEXT("receiving visitors"))) { return EDkAnswer::NotReceiving; }
    return EDkAnswer::None;
}

struct FDkLedger
{
    TArray<FString> Mismatches;
    EDkAnswer LastObserved = EDkAnswer::None;
    int32 Probes = 0;
    int32 Admitted = 0;
    int32 NoAnswer = 0;
    int32 NotReceiving = 0;
    bool bInside = false; // the row house probe was admitted, so the inside steps apply
};

FDkLedger& DkLedger()
{
    static FDkLedger Ledger;
    return Ledger;
}

APWDoorActor* DkFindDoor(UWorld* World, const FName DoorId)
{
    if (World == nullptr) { return nullptr; }
    for (TActorIterator<APWDoorActor> It(World); It; ++It)
    {
        if (It->GetDoorId() == DoorId) { return *It; }
    }
    return nullptr;
}

struct FDkGeo
{
    FVector Centre = FVector::ZeroVector;
    FVector Outside = FVector::ZeroVector;
    FVector Inside = FVector::ZeroVector;
};

/** The visitor and inside spots of a kit door, from the portable layout and the door's own spec (hinge, closed yaw, swing side). */
bool DkDoorGeo(const TCHAR* BuildingId, const TCHAR* DoorId, FDkGeo& Out)
{
    PWLane::FLaneBuilding Building;
    PWLane::FLaneKitFrontage Kit;
    if (!PWLane::MakeSubrataBuilding(BuildingId, Building) || !PWLane::IsKitFrontage(Building, Kit)) { return false; }
    const LI::InteriorLayout Layout = PWLane::MakeKitFrontageLayout(Building, Kit);
    if (!Layout.Ok()) { return false; }
    const std::string Id(TCHAR_TO_UTF8(DoorId));
    for (const LI::Opening& Opening : Layout.Openings)
    {
        if (Opening.DoorId != Id) { continue; }
        const PWLane::FLaneDoorSpec Spec = PWLane::KitDoorSpec(Building, Layout, Opening);
        const FVector Dir = FRotator(0.0, Spec.ClosedYaw, 0.0).Vector();
        const FVector Into = FVector(-Dir.Y, Dir.X, 0.0) * Spec.PreferredSwing;
        Out.Centre = Spec.Hinge + Dir * DkLeafHalfCm;
        Out.Outside = Out.Centre - Into * DkReachSpotCm;
        Out.Inside = Out.Centre + Into * DkInsideSpotCm;
        return true;
    }
    return false;
}

struct FDkDoorRef
{
    const TCHAR* DoorId;
    const TCHAR* BuildingId;
};
constexpr FDkDoorRef DkRowHouse{TEXT("door.nabapara.tea_row_house_front"), TEXT("building.nabapara.tea_row_house")};
constexpr FDkDoorRef DkTallFront{TEXT("door.nabapara.tall_flats_front"), TEXT("building.nabapara.tall_flats")};
constexpr FDkDoorRef DkUnitA{TEXT("door.nabapara.tall_flats_unit_a"), TEXT("building.nabapara.tall_flats")};
constexpr FDkDoorRef DkUnitB{TEXT("door.nabapara.tall_flats_unit_b"), TEXT("building.nabapara.tall_flats")};
constexpr FDkDoorRef DkCaretaker{TEXT("door.nabapara.caretaker_flat_front"), TEXT("building.nabapara.caretaker_flat")};

bool DkSpot(const FDkDoorRef& Door, const bool bInside, FVector& Out, FString& Why)
{
    FDkGeo Geo;
    if (!DkDoorGeo(Door.BuildingId, Door.DoorId, Geo))
    {
        Why = FString::Printf(TEXT("the geometry of %s does not resolve from the kit layout"), Door.DoorId);
        return false;
    }
    Out = bInside ? Geo.Inside : Geo.Outside;
    return true;
}

/** What the schedules say a knock at this door should get now: a resident who is home (idle or at work at the home) and the visiting hours. */
EDkAnswer DkExpected(UWorld* World, const FName DoorId, FString& Facts)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
    const UGameInstance* Instance = World != nullptr ? World->GetGameInstance() : nullptr;
    const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    const PerfectWorld::ContentDb* Db = Content != nullptr ? Content->GetDb() : nullptr;
    const UPWNpcPresenceSubsystem* Presence = World != nullptr ? World->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(std::string(TCHAR_TO_UTF8(*DoorId.ToString())));
    if (Life == nullptr || Db == nullptr || Presence == nullptr || Def == nullptr) { Facts = TEXT("no lane-life, content or presence subsystem"); return EDkAnswer::None; }
    const PerfectWorld::GameInstant Now = Life->GetNow();
    const int32 Minute = PerfectWorld::DoorMinuteOfDay(Now);
    FString Who = TEXT("no resident has a schedule here");
    bool bHome = false;
    for (const PerfectWorld::ContentRecord* Record : Db->OfType("npc_schedule"))
    {
        const PerfectWorld::NpcScheduleResult Loaded = PerfectWorld::LoadNpcSchedule(*Db, Record->Id);
        if (!Loaded.Ok() || Loaded.Value->Person.HomeZone != Def->LocationId) { continue; }
        const auto Intent = PerfectWorld::ResolveNpcIntentAt(Loaded.Value->Person, Loaded.Value->Travel, Now, false, false);
        const bool bHere = Intent.Ok() && Intent.Intent.AnchorId == Def->LocationId &&
                           (Intent.Intent.State == PerfectWorld::NpcState::Idle || Intent.Intent.State == PerfectWorld::NpcState::Work);
        const FName Person(UTF8_TO_TCHAR(Loaded.Value->Person.PersonId.c_str()));
        Who = FString::Printf(TEXT("%s is %s by her schedule (registered with the presenter: %d)"), *Person.ToString(), bHere ? TEXT("home") : TEXT("not home"),
                              Presence->IsRegistered(Person));
        bHome = bHome || bHere;
    }
    Facts = FString::Printf(TEXT("%s at %02d:%02d, visiting window %s"), *Who, Minute / 60, Minute % 60,
                            PerfectWorld::DoorMinutesUntilVisiting(*Def, Minute) == 0 ? TEXT("open") : TEXT("closed"));
    if (!bHome) { return EDkAnswer::NoAnswer; }
    return PerfectWorld::DoorMinutesUntilVisiting(*Def, Minute) == 0 ? EDkAnswer::Admitted : EDkAnswer::NotReceiving;
}

/** A copy of the live world moved to Target by the core's own commands (time.advance, then time.settle), or null with the core's code. */
TUniquePtr<PerfectWorld::WorldSim> DkAdvancedCopy(const PerfectWorld::WorldState& Live, const PerfectWorld::GameInstant Target, FString& Why)
{
    auto Sim = MakeUnique<PerfectWorld::WorldSim>(Live);
    const auto Settle = [&Sim]
    {
        for (int32 Round = 0; Round < 64 && !PerfectWorld::IsWorldQuiescent(Sim->State()); ++Round)
        {
            PerfectWorld::Command Command;
            Command.Source = PerfectWorld::CommandSource::System;
            Command.Payload = PerfectWorld::SettleDueEvents{};
            if (!Sim->Submit(Command).Accepted) { return; }
        }
    };
    Settle();
    for (int32 Round = 0; Round < 64 && Sim->State().Clock.Now() < Target; ++Round)
    {
        PerfectWorld::AdvanceTime Advance;
        Advance.Minutes = Target - Sim->State().Clock.Now();
        PerfectWorld::Command Command;
        Command.Source = PerfectWorld::CommandSource::System;
        Command.Payload = Advance;
        const PerfectWorld::CommandResult Result = Sim->Submit(Command);
        if (!Result.Accepted) { Why = FString::Printf(TEXT("the core refused time.advance: %hs"), Result.Why.Code.c_str()); return nullptr; }
        Settle();
    }
    if (Sim->State().Clock.Now() != Target) { Why = TEXT("the advance did not reach the target instant"); return nullptr; }
    return Sim;
}

/** Moves the clock forward to day (0 = Monday 2 June 2042) at hour:minute, with the player standing at the door and the world at rest. */
FStepDef DkClockStep(const FString& Name, const int32 Day, const int32 Hour, const int32 Minute)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 10.0;
    Def.Run = [Day, Hour, Minute](UPWAcceptanceSubsystem& Sub, FStepRuntime&) -> FStepOutcome
    {
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Life == nullptr || Pawn == nullptr) { return FStepOutcome::Fail(TEXT("no lane-life subsystem or no pawn")); }
        if (Pawn->IsSeated() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() || Life->HasMovingDoors())
        {
            return FStepOutcome::Fail(TEXT("the world is not at rest (seated, in conversation or a door moving), so the clock is not moved"));
        }
        const PerfectWorld::GameInstant Target = PerfectWorld::GameClock::FromCalendar(
            PerfectWorld::CampaignStartYear, PerfectWorld::CampaignStartMonth, PerfectWorld::CampaignStartDay + Day, Hour, Minute);
        const FString Before = Life->GetClockText().ToString();
        if (Life->GetNow() >= Target) { return FStepOutcome::Fail(FString::Printf(TEXT("the clock already stands at %s, at or after the target"), *Before)); }
        FString Why;
        TUniquePtr<PerfectWorld::WorldSim> Moved = DkAdvancedCopy(Life->GetWorldState(), Target, Why);
        if (!Moved.IsValid()) { return FStepOutcome::Fail(Why); }
        Life->AdoptSavedWorld(MoveTemp(Moved));
        if (Life->GetNow() != Target) { return FStepOutcome::Fail(TEXT("the clock is not at the target after adopting the moved world")); }
        return FStepOutcome::Pass(FString::Printf(TEXT("clock moved from %s to %s"), *Before, *Life->GetClockText().ToString()));
    };
    return Def;
}

/** Walks the real pawn to FindTarget's point; Skip (optional) passes the step without walking. */
FStepDef DkWalkStep(const FString& Name, TFunction<bool(FVector&, FString&)> FindTarget, TFunction<bool()> Skip, const double TimeoutSeconds)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = TimeoutSeconds;
    Def.Run = [FindTarget, Skip](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (Skip && Skip()) { return FStepOutcome::Pass(TEXT("skipped: the entry it follows was not admitted")); }
        if (!Rt.bHaveTarget)
        {
            FString Why;
            if (!FindTarget(Rt.Target, Why)) { return FStepOutcome::Fail(Why); }
            Rt.bHaveTarget = true;
        }
        return Sub.TickWalk(Rt.Target, DkWalkTolerance, Rt);
    };
    return Def;
}

FStepDef DkWalkToDoor(const FString& Name, const FDkDoorRef Door, const bool bInside, TFunction<bool()> Skip = nullptr)
{
    return DkWalkStep(Name, [Door, bInside](FVector& Out, FString& Why) { return DkSpot(Door, bInside, Out, Why); }, Skip, 70.0);
}

/** A waypoint on a footpath line, at the X of a door's visitor spot. */
FStepDef DkWalkAlong(const FString& Name, const FDkDoorRef Door, const double LineY)
{
    return DkWalkStep(Name, [Door, LineY](FVector& Out, FString& Why) { if (!DkSpot(Door, false, Out, Why)) { return false; } Out.Y = LineY; return true; }, nullptr, 70.0);
}

FStepDef DkWaitStep(const FString& Name, const double Seconds, TFunction<bool()> Skip)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Seconds + 5.0;
    Def.Run = [Seconds, Skip](UPWAcceptanceSubsystem&, FStepRuntime& Rt) -> FStepOutcome
    {
        if (Skip && Skip()) { return FStepOutcome::Pass(TEXT("nothing to wait for")); }
        return Rt.Elapsed < Seconds ? FStepOutcome::Wait() : FStepOutcome::Pass(TEXT("the last notice has expired"));
    };
    return Def;
}

/** E on the door. bFirst: the knock (records and judges the answer); otherwise the second press (entry, or no entry). */
FStepDef DkKnockStep(const FString& Name, const FDkDoorRef Door, const bool bFirst)
{
    const FName Id(Door.DoorId);
    return PWAcceptance::MakeInteractStep(Name, [Id](UPWAcceptanceSubsystem& Sub) -> AActor* { return DkFindDoor(Sub.GetPlayWorld(), Id); },
        [Id, bFirst](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            FDkLedger& Ledger = DkLedger();
            const UPWInteractionComponent* Interaction = Sub.GetInteraction();
            const APWDoorActor* DoorActor = DkFindDoor(Sub.GetPlayWorld(), Id);
            if (Interaction == nullptr || DoorActor == nullptr) { return false; }
            if (bFirst)
            {
                const EDkAnswer Observed = DkClassify(Interaction->GetNotice());
                if (Observed == EDkAnswer::None) { return false; }
                FString Facts;
                const EDkAnswer Expected = DkExpected(Sub.GetPlayWorld(), Id, Facts);
                Ledger.LastObserved = Observed;
                ++Ledger.Probes;
                (Observed == EDkAnswer::Admitted ? Ledger.Admitted : Observed == EDkAnswer::NoAnswer ? Ledger.NoAnswer : Ledger.NotReceiving) += 1;
                const bool bMatch = Observed == Expected;
                Detail = FString::Printf(TEXT("%s: expected %s, observed %s%s. %s"), *Id.ToString(), DkAnswerText(Expected), DkAnswerText(Observed),
                                         bMatch ? TEXT("") : TEXT(" MISMATCH"), *Facts);
                if (!bMatch) { Ledger.Mismatches.Add(Detail); }
                return true;
            }
            if (Ledger.LastObserved == EDkAnswer::Admitted)
            {
                if (!DoorActor->IsOpen()) { return false; }
                Detail = FString::Printf(TEXT("%s is open: the visitor whose knock was answered was let in"), *Id.ToString());
                return true;
            }
            if (DkClassify(Interaction->GetNotice()) != Ledger.LastObserved) { return false; }
            const bool bShut = !DoorActor->IsOpen();
            Detail = FString::Printf(TEXT("%s knocked again: same answer (%s), door %s"), *Id.ToString(), DkAnswerText(Ledger.LastObserved), bShut ? TEXT("still shut") : TEXT("OPEN"));
            if (!bShut) { Ledger.Mismatches.Add(Detail); }
            return true;
        },
        nullptr, 15.0);
}

/** E on the door to bring it to the wanted state; passes at once when it is already there. */
FStepDef DkSetDoorStep(const FString& Name, const FDkDoorRef Door, const bool bWantOpen, const TCHAR* Say, TFunction<bool()> ExtraSkip = nullptr)
{
    const FName Id(Door.DoorId);
    const FString Text(Say);
    return PWAcceptance::MakeInteractStep(Name, [Id](UPWAcceptanceSubsystem& Sub) -> AActor* { return DkFindDoor(Sub.GetPlayWorld(), Id); },
        [Id, bWantOpen, Text](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWDoorActor* DoorActor = DkFindDoor(Sub.GetPlayWorld(), Id);
            if (DoorActor == nullptr || DoorActor->IsMoving() || DoorActor->IsOpen() != bWantOpen) { return false; }
            Detail = Text;
            return true;
        },
        [Id, bWantOpen, ExtraSkip](UPWAcceptanceSubsystem& Sub)
        {
            if (ExtraSkip && ExtraSkip()) { return true; }
            const APWDoorActor* DoorActor = DkFindDoor(Sub.GetPlayWorld(), Id);
            return DoorActor != nullptr && !DoorActor->IsMoving() && DoorActor->IsOpen() == bWantOpen;
        },
        15.0);
}

FStepDef DkStartStep()
{
    FStepDef Def;
    Def.Name = TEXT("declared_start_outside_row_house");
    Def.TimeoutSeconds = 15.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr) { return FStepOutcome::Fail(TEXT("no pawn")); }
        if (Rt.Phase == 0)
        {
            FDkGeo Geo;
            if (!DkDoorGeo(DkRowHouse.BuildingId, DkRowHouse.DoorId, Geo)) { return FStepOutcome::Fail(TEXT("the row house door geometry does not resolve")); }
            DkLedger() = FDkLedger();
            const FVector Start = Geo.Outside + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 5.0);
            // The one teleport of this scenario: its declared start, on the footpath facing the row house door. Every later move is walked.
            Pawn->TeleportTo(Start, FRotator(0.0, (Geo.Centre - Geo.Outside).Rotation().Yaw, 0.0));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Sub.FaceWorldPoint(Geo.Centre);
            Rt.Memo = FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), Start.X, Start.Y, Start.Z);
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("declared start %s, %.0f cm in front of the row house door; standing on the ground"), *Rt.Memo, DkReachSpotCm));
    };
    return Def;
}

/** One probe: clock, knock, (wait), knock again, and leave the door shut. Door-specific extras go between the second press and the clean-up. */
void DkAddProbe(TArray<FStepDef>& Out, const FString& Tag, const FDkDoorRef Door, const int32 Day, const int32 Hour, const int32 Minute,
                const bool bEnterAndExit)
{
    const auto Admitted = [] { return DkLedger().LastObserved == EDkAnswer::Admitted; };
    Out.Add(DkClockStep(Tag + TEXT("_clock"), Day, Hour, Minute));
    Out.Add(DkKnockStep(Tag + TEXT("_knock"), Door, true));
    Out.Add(PWAcceptance::MakeShotStep(Tag + TEXT("_shot_answer")));
    Out.Add(DkWaitStep(Tag + TEXT("_notice_expires"), 3.0, Admitted));
    Out.Add(DkKnockStep(Tag + TEXT("_knock_again"), Door, false));
    if (bEnterAndExit)
    {
        const auto NotAdmitted = [] { return DkLedger().LastObserved != EDkAnswer::Admitted; };
        Out.Add(DkWalkToDoor(Tag + TEXT("_walk_inside"), Door, true, NotAdmitted));
        Out.Add(DkWaitStep(Tag + TEXT("_settle_inside"), 1.0, NotAdmitted));
        Out.Add(PWAcceptance::MakeShotStep(Tag + TEXT("_shot_inside")));
        Out.Add(DkSetDoorStep(Tag + TEXT("_close_from_inside"), Door, false, TEXT("the door is shut from inside; the knock approach ends with it"), NotAdmitted));
        // Exit is never refused: the core's Exit direction is Ok whatever the knock state, and the knock was forgotten when the door closed.
        Out.Add(DkSetDoorStep(Tag + TEXT("_open_from_inside"), Door, true, TEXT("the door opens from inside with no knock held: exit is unrestricted"), NotAdmitted));
        Out.Add(DkWalkToDoor(Tag + TEXT("_walk_out"), Door, false, NotAdmitted));
    }
    Out.Add(DkSetDoorStep(Tag + TEXT("_shut_door"), Door, false, TEXT("the door is shut again")));
}

FStepDef DkVerdictStep()
{
    FStepDef Def;
    Def.Name = TEXT("verdict");
    Def.TimeoutSeconds = 5.0;
    Def.Run = [](UPWAcceptanceSubsystem&, FStepRuntime&) -> FStepOutcome
    {
        const FDkLedger& Ledger = DkLedger();
        const FString Tally = FString::Printf(TEXT("%d knocks: %d admitted, %d no answer, %d not receiving"), Ledger.Probes, Ledger.Admitted, Ledger.NoAnswer, Ledger.NotReceiving);
        if (!Ledger.Mismatches.IsEmpty()) { return FStepOutcome::Fail(FString::Printf(TEXT("%s; %d mismatches: %s"), *Tally, Ledger.Mismatches.Num(), *FString::Join(Ledger.Mismatches, TEXT(" | ")))); }
        if (Ledger.Admitted == 0 || Ledger.NoAnswer == 0 || Ledger.NotReceiving == 0) { return FStepOutcome::Fail(FString::Printf(TEXT("%s: the run never saw all three answers"), *Tally)); }
        return FStepOutcome::Pass(FString::Printf(TEXT("%s; every answer matched the schedules and the visiting hours"), *Tally));
    };
    return Def;
}
} // namespace

namespace PWAcceptance
{
void AppendDoorsKnockSteps(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(DkStartStep());
    // Tea row house (visiting 16:00-19:30). Monday 16:00 (the clock runs one game minute per real second, so the knocks land before 16:11): Rupa is still walking home (away); 18:30: home, supper, admitted, then in and out;
    // 20:30: home but not receiving.
    DkAddProbe(Out, TEXT("row_house_away_1600"), DkRowHouse, 0, 16, 0, false);
    DkAddProbe(Out, TEXT("row_house_home_1830"), DkRowHouse, 0, 18, 30, true);
    DkAddProbe(Out, TEXT("row_house_late_2030"), DkRowHouse, 0, 20, 30, false);
    // To the tall flats along the +Y footpath; the shared entry is public and opened with E; the two flats' doors are in its passage.
    Out.Add(DkWalkAlong(TEXT("walk_footpath_to_tall_flats"), DkRowHouse, DkFootpathY));
    Out.Add(DkWalkAlong(TEXT("walk_footpath_at_tall_flats"), DkTallFront, DkFootpathY));
    Out.Add(DkWalkToDoor(TEXT("walk_to_tall_flats_entry"), DkTallFront, false));
    Out.Add(DkSetDoorStep(TEXT("open_tall_flats_front"), DkTallFront, true, TEXT("door.nabapara.tall_flats_front (public) is open")));
    Out.Add(DkWalkToDoor(TEXT("walk_into_passage"), DkTallFront, true));
    Out.Add(DkWalkToDoor(TEXT("walk_to_unit_a"), DkUnitA, false));
    DkAddProbe(Out, TEXT("unit_a_evening_1830"), DkUnitA, 1, 18, 30, false);
    DkAddProbe(Out, TEXT("unit_a_late_2115"), DkUnitA, 1, 21, 15, false);
    Out.Add(DkWalkToDoor(TEXT("walk_to_unit_b"), DkUnitB, false));
    DkAddProbe(Out, TEXT("unit_b_evening_1830"), DkUnitB, 2, 18, 30, false);
    DkAddProbe(Out, TEXT("unit_b_late_2115"), DkUnitB, 2, 21, 15, false);
    // Out of the passage and across the lane to the caretaker's flat (visiting 10:00-12:30 and 17:30-20:30), crossing the road clear of the parked taxi.
    Out.Add(DkWalkToDoor(TEXT("walk_back_to_tall_flats_entry"), DkTallFront, true));
    Out.Add(DkWalkAlong(TEXT("walk_out_to_footpath"), DkTallFront, DkFootpathY));
    Out.Add(DkWalkAlong(TEXT("walk_footpath_to_caretaker"), DkCaretaker, DkFootpathY));
    Out.Add(DkWalkAlong(TEXT("walk_across_road"), DkCaretaker, DkRoadFootY));
    Out.Add(DkWalkToDoor(TEXT("walk_to_caretaker_door"), DkCaretaker, false));
    DkAddProbe(Out, TEXT("caretaker_morning_0830"), DkCaretaker, 3, 8, 30, false);
    DkAddProbe(Out, TEXT("caretaker_visiting_1100"), DkCaretaker, 3, 11, 0, false);
    DkAddProbe(Out, TEXT("caretaker_afternoon_1300"), DkCaretaker, 3, 13, 0, false);
    Out.Add(DkVerdictStep());
}
} // namespace PWAcceptance
