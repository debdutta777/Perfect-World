#include "PerfectWorldAcceptanceSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldFurnitureActor.h"
#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneHomeFurniture.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldPlayerCharacter.h"

// origins_seating_cameras (NR-04): both opening homes, every listed seat, both cameras, in one process.
//
// The life the run starts (a new life, or Continue) is walked first: the family home's table chair and divan, or the care home's
// bed, study bench and dining bench. Each seat gets: walk to its stand point, sit with the interact key's call, a screenshot in first
// person, the camera toggled to third person (the C key's call), a second screenshot, the camera toggled back, and stand up. The
// scenario then goes back to the title, creates the OTHER origin through the real creator (the balanced scholar preset; Household
// Resources 0 for care) in the next empty slot, enters the world and walks that home the same way. Both homes are built in the lane
// at once, so one process can visit both. Seats the current origin does not have (the family home has two) pass as "no seat".
// The only teleports are the engine's own placement of each life at its opening home.
//
// The waypoints through the care home are derived from its layout constants (dormitory door U 120, entry|common doorway U 506 V 435)
// and are not yet measured in a live run: a blocked step reports where it stopped.

namespace PWAcceptance
{
void AppendOriginsSeatingCamerasSteps(TArray<FStepDef>& OutSteps);
}

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

constexpr int32 AoSeatSlots = 3;
constexpr double AoWaypointTolerance = 35.0;
constexpr double AoStandTolerance = 30.0;

PWLane::FLaneFrame AoFamilyFrame()
{
    return PWLane::MakeLaneFrame(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
}

PWLane::FLaneFrame AoCareFrame()
{
    return PWLane::MakeLaneFrame(-1400.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
}

bool AoIsCare(UPWAcceptanceSubsystem& Sub)
{
    const APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
    return Hud != nullptr && Hud->GetCreator() != nullptr && Hud->GetCreator()->GetLaneOrigin().bGrewUpInCare;
}

const PWLane::FHomeSeatSpec* AoSpecIn(const TArray<PWLane::FHomeSeatSpec>& Specs, const TCHAR* Id)
{
    for (const PWLane::FHomeSeatSpec& Spec : Specs)
    {
        if (Spec.Id == FName(Id)) { return &Spec; }
    }
    return nullptr;
}

/** What one seat slot means for the current origin: the seat id, and the points to walk (waypoints then the stand point). */
struct FAoSeat
{
    bool bExists = false;
    FName Id;
    TArray<FVector> Path;
};

FAoSeat AoSeat(const bool bCare, const int32 Slot)
{
    FAoSeat Out;
    if (!bCare)
    {
        const TArray<PWLane::FHomeSeatSpec> Specs = PWLane::HomeSeatSpecs();
        const PWLane::FLaneFrame Frame = AoFamilyFrame();
        const TCHAR* Ids[] = {TEXT("seat.nabapara.home_table_chair"), TEXT("seat.nabapara.home_divan")};
        if (Slot >= static_cast<int32>(UE_ARRAY_COUNT(Ids))) { return Out; }
        const PWLane::FHomeSeatSpec* Spec = AoSpecIn(Specs, Ids[Slot]);
        if (Spec == nullptr) { return Out; }
        Out.bExists = true;
        Out.Id = Spec->Id;
        if (Slot == 0)
        {
            Out.Path.Add(PWLane::HomeWorldPoint(Frame, PWLane::HomeLivingPartitionU, PWLane::HomeLivingDoorV, 0.0)); // the living doorway
        }
        Out.Path.Add(PWLane::HomeWorldPoint(Frame, Spec->StandU, Spec->StandV, 0.0));
        return Out;
    }
    const TArray<PWLane::FHomeSeatSpec> Specs = PWLane::CareSeatSpecs();
    const PWLane::FLaneFrame Frame = AoCareFrame();
    const TCHAR* Ids[] = {TEXT("seat.nabapara.care_bed"), TEXT("seat.nabapara.care_study_bench"), TEXT("seat.nabapara.care_dining_bench")};
    if (Slot >= static_cast<int32>(UE_ARRAY_COUNT(Ids))) { return Out; }
    const PWLane::FHomeSeatSpec* Spec = AoSpecIn(Specs, Ids[Slot]);
    if (Spec == nullptr) { return Out; }
    Out.bExists = true;
    Out.Id = Spec->Id;
    const auto Point = [&Frame](const double U, const double V) { return PWLane::CareWorldPoint(Frame, U, V, 0.0); };
    if (Slot == 1)
    {
        // Bed aisle north to the dormitory door (U 120), through the entry to the entry|common doorway (U 506, V 435), then the study aisle.
        for (const FVector2D& P : {FVector2D(345.0, 560.0), FVector2D(120.0, 560.0), FVector2D(120.0, 470.0), FVector2D(300.0, 440.0),
                                   FVector2D(440.0, 435.0), FVector2D(506.0, 435.0), FVector2D(570.0, 435.0)})
        {
            Out.Path.Add(Point(P.X, P.Y));
        }
    }
    else if (Slot == 2)
    {
        Out.Path.Add(Point(600.0, 410.0));
    }
    Out.Path.Add(Point(Spec->StandU, Spec->StandV));
    return Out;
}

APWSeatActor* AoFindSeat(UWorld* World, const FName Id)
{
    if (World == nullptr) { return nullptr; }
    for (TActorIterator<APWSeatActor> It(World); It; ++It)
    {
        if (It->GetSeatId() == Id) { return *It; }
    }
    return nullptr;
}

FString AoSlotName(const int32 Half, const int32 Slot, const TCHAR* What)
{
    return FString::Printf(TEXT("%c_seat%d_%s"), Half == 0 ? TCHAR('a') : TCHAR('b'), Slot, What);
}

/** Walks the real pawn through the slot's points in order. Passes at once when the current origin has no such seat. */
FStepDef AoWalkStep(const FString& Name, const int32 Slot)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 60.0;
    Def.Run = [Slot](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        const bool bCare = AoIsCare(Sub);
        const FAoSeat Seat = AoSeat(bCare, Slot);
        if (!Seat.bExists)
        {
            return FStepOutcome::Pass(FString::Printf(TEXT("the %s home has no seat in slot %d"), bCare ? TEXT("care") : TEXT("family"), Slot));
        }
        if (Rt.Phase >= Seat.Path.Num())
        {
            return FStepOutcome::Pass(FString::Printf(TEXT("%s: walked %d point(s) to the stand point"), *Seat.Id.ToString(), Seat.Path.Num()));
        }
        const bool bLast = Rt.Phase == Seat.Path.Num() - 1;
        const FStepOutcome Result = Sub.TickWalk(Seat.Path[Rt.Phase], bLast ? AoStandTolerance : AoWaypointTolerance, Rt);
        if (Result.State == PWAcceptance::EStepState::Failed)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("%s, waypoint %d of %d: %s"), *Seat.Id.ToString(), Rt.Phase + 1, Seat.Path.Num(), *Result.Detail));
        }
        if (Result.State == PWAcceptance::EStepState::Passed)
        {
            Rt.NextPhase();
            Rt.LastProbe = 0.0; // the walk's stuck detector starts afresh for the next point
            Rt.StuckSince = 0.0;
        }
        return FStepOutcome::Wait();
    };
    return Def;
}

/** Sits (bSit) or stands up with the interact call, on the slot's seat; passes at once when there is no such seat. */
FStepDef AoSitStep(const FString& Name, const int32 Slot, const bool bSit)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Slot](UPWAcceptanceSubsystem& Sub) -> AActor* { return AoFindSeat(Sub.GetPlayWorld(), AoSeat(AoIsCare(Sub), Slot).Id); },
        [Slot, bSit](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
            const APWSeatActor* Seat = AoFindSeat(Sub.GetPlayWorld(), AoSeat(AoIsCare(Sub), Slot).Id);
            if (Pawn == nullptr || Seat == nullptr) { return false; }
            if (bSit)
            {
                if (Pawn->GetSeatState() != EPWSeatState::Seated || !Pawn->IsSeatedOn(Seat)) { return false; }
                Detail = FString::Printf(TEXT("the pawn is seated on %s"), *Seat->GetSeatId().ToString());
                return true;
            }
            if (Pawn->GetSeatState() != EPWSeatState::Standing) { return false; }
            Detail = FString::Printf(TEXT("the pawn stands up from %s"), *Seat->GetSeatId().ToString());
            return true;
        },
        [Slot](UPWAcceptanceSubsystem& Sub)
        {
            const FAoSeat Seat = AoSeat(AoIsCare(Sub), Slot);
            return !Seat.bExists || AoFindSeat(Sub.GetPlayWorld(), Seat.Id) == nullptr; // nothing to sit on in this slot
        },
        20.0);
}

/** A screenshot, or a pass when the slot has no seat. */
FStepDef AoShotStep(const FString& Name, const int32 Slot)
{
    FStepDef Shot = PWAcceptance::MakeShotStep(Name);
    TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Inner = Shot.Run;
    Shot.Run = [Slot, Inner](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (!AoSeat(AoIsCare(Sub), Slot).bExists) { return FStepOutcome::Pass(TEXT("no seat in this slot for this origin")); }
        return Inner(Sub, Rt);
    };
    return Shot;
}

/** Puts the camera in the wanted mode with the toggle key's call and checks it took. Passes at once when the slot has no seat. */
FStepDef AoCameraStep(const FString& Name, const int32 Slot, const bool bThird)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 6.0;
    Def.Run = [Slot, bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr) { return FStepOutcome::Fail(TEXT("no pawn")); }
        if (!AoSeat(AoIsCare(Sub), Slot).bExists) { return FStepOutcome::Pass(TEXT("no seat in this slot for this origin")); }
        if (Rt.Phase == 0)
        {
            if (Pawn->IsFirstPerson() == bThird) { Pawn->ToggleCameraMode(); }
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.InPhase() < 0.5) { return FStepOutcome::Wait(); }
        if (Pawn->IsFirstPerson() == bThird)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("the camera did not change to %s"), bThird ? TEXT("third person") : TEXT("first person")));
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("camera is %s, pawn seated %d"), bThird ? TEXT("third person") : TEXT("first person"), Pawn->IsSeated()));
    };
    return Def;
}

/** Back to the title, a new life of the other origin in the next empty slot, entered through the real front end. */
FStepDef AoSwitchOriginStep()
{
    FStepDef Def;
    Def.Name = TEXT("switch_to_other_origin");
    Def.TimeoutSeconds = 60.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        UPerfectWorldCharacterCreatorSubsystem* Creator = Hud != nullptr ? Hud->GetCreator() : nullptr;
        if (Hud == nullptr || Pawn == nullptr || Creator == nullptr) { return FStepOutcome::Fail(TEXT("no HUD, pawn or creator")); }
        if (Rt.Phase == 0)
        {
            if (Pawn->IsSeated()) { return FStepOutcome::Fail(TEXT("the pawn is still seated")); }
            Rt.Memo = AoIsCare(Sub) ? TEXT("family") : TEXT("care"); // the origin being switched TO
            Hud->ReturnToTitle();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 1)
        {
            if (Rt.InPhase() < 0.5) { return FStepOutcome::Wait(); }
            const bool bWantCare = Rt.Memo == TEXT("care");
            const int32 Slot = UPerfectWorldCharacterCreatorSubsystem::FindFirstEmptySaveSlot();
            if (Slot < 0) { return FStepOutcome::Fail(TEXT("no empty save slot for the second life (use a clean save directory)")); }
            Hud->BeginNewCreation();
            if (!Creator->ApplyBalancedScholarPreset())
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("the balanced scholar preset was refused: %s"), *Creator->GetLastErrorMessage().ToString()));
            }
            if (bWantCare && !Creator->SetTraitRank(FName(TEXT("household_resources")), 0))
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("Household Resources rank 0 was refused: %s"), *Creator->GetLastErrorMessage().ToString()));
            }
            if (Creator->GetOpeningHome().bGrewUpInCare != bWantCare)
            {
                return FStepOutcome::Fail(FString::Printf(TEXT("the creator's opening home is %s, not the wanted %s"),
                                                          Creator->GetOpeningHome().bGrewUpInCare ? TEXT("care") : TEXT("family"), *Rt.Memo));
            }
            const FText Why = Hud->StartNewLife(Slot);
            if (!Why.IsEmpty()) { return FStepOutcome::Fail(FString::Printf(TEXT("starting the second life was refused: %s"), *Why.ToString())); }
            Rt.Memo += FString::Printf(TEXT(" in slot %d"), Slot);
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        const bool bGrounded = Pawn->GetCharacterMovement() != nullptr && Pawn->GetCharacterMovement()->IsMovingOnGround();
        Rt.Note = FString::Printf(TEXT("menu open %d, grounded %d, started %d"), Hud->IsMenuOpen(), bGrounded, Creator->HasLifeStarted());
        if (Hud->IsMenuOpen() || !bGrounded || Rt.InPhase() < 0.8) { return FStepOutcome::Wait(); }
        const FVector Here = Pawn->GetActorLocation();
        return FStepOutcome::Pass(FString::Printf(TEXT("%s origin life started; in the world at (%.0f, %.0f, %.0f)"), *Rt.Memo, Here.X, Here.Y, Here.Z));
    };
    return Def;
}

void AoAppendHalf(const int32 Half, TArray<FStepDef>& Out)
{
    for (int32 Slot = 0; Slot < AoSeatSlots; ++Slot)
    {
        Out.Add(AoWalkStep(AoSlotName(Half, Slot, TEXT("walk")), Slot));
        Out.Add(AoSitStep(AoSlotName(Half, Slot, TEXT("sit")), Slot, true));
        Out.Add(AoShotStep(AoSlotName(Half, Slot, TEXT("shot_first_person")), Slot));
        Out.Add(AoCameraStep(AoSlotName(Half, Slot, TEXT("camera_third")), Slot, true));
        Out.Add(AoShotStep(AoSlotName(Half, Slot, TEXT("shot_third_person")), Slot));
        Out.Add(AoCameraStep(AoSlotName(Half, Slot, TEXT("camera_first")), Slot, false));
        Out.Add(AoSitStep(AoSlotName(Half, Slot, TEXT("stand")), Slot, false));
    }
}
} // namespace

namespace PWAcceptance
{
void AppendOriginsSeatingCamerasSteps(TArray<FStepDef>& OutSteps)
{
    OutSteps.Add(MakeWorldReadyStep());
    OutSteps.Add(MakeEnterWorldStep());
    AoAppendHalf(0, OutSteps);
    OutSteps.Add(AoSwitchOriginStep());
    AoAppendHalf(1, OutSteps);
}
} // namespace PWAcceptance
