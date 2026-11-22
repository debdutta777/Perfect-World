#include "PerfectWorldAcceptanceScaleR4.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldAcceptanceScaleR4Probe.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcLooks.h"
#include "PerfectWorldPlayerCharacter.h"

// scale_round4 (R4-06; Docs/ROUND4_PLAN_2026-10-19.md). Everything is MEASURED, not assumed: the cast from the authoritative height table
// and the generated blocks (this file), and the doors, seats, work surfaces, ceilings and the tall flats' stair and upper dwelling from
// Pawn-channel traces in the live world and from the portable layouts the world was built from (PerfectWorldAcceptanceScaleR4World.cpp).
//
// The walking player is a fixed 165 cm capsule until the AppearanceRecord has a height, so the SUPPORTED EXTREMES (145 and 195 cm,
// ASSET_PIPELINE proportions policy) are evaluated geometrically: a body of that height (eye 0.939 of height, head top = height)
// against every measured clearance, without changing the player. The real pawn is measured in both cameras at the one declared start,
// the tall flats' top landing. Each measuring step records its violations in a shared ledger and passes, so one run observes
// everything; the last step, r4_verdict, fails when the ledger holds any violation (that blocks the owning order). Absent newcomers are
// not a violation: R4-02 and R4-03 spawn them.
//
// Driver contract: PerfectWorldAcceptanceSubsystem.h (AW-01): FStepDef steps, MakeWorldReadyStep, MakeEnterWorldStep, MakeShotStep.

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
using R4Shared = TSharedRef<PWScaleR4::FLedger>;

FStepDef R4Measure(const FString& Name, const R4Shared& Ledger, TFunction<FString(UPWAcceptanceSubsystem&, PWScaleR4::FLedger&)> Body)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 30.0;
    Def.Run = [Ledger, Body](UPWAcceptanceSubsystem& Sub, FStepRuntime&) -> FStepOutcome
    {
        if (Sub.GetPlayWorld() == nullptr) { return FStepOutcome::Fail(TEXT("no play world")); }
        const int32 ViolationsBefore = Ledger->Violations.Num();
        const int32 ChecksBefore = Ledger->Checks;
        FString Out = Body(Sub, *Ledger);
        Out += FString::Printf(TEXT(" | %d checks, %d violations"), Ledger->Checks - ChecksBefore, Ledger->Violations.Num() - ViolationsBefore);
        for (int32 Index = ViolationsBefore; Index < Ledger->Violations.Num() && Index < ViolationsBefore + 8; ++Index)
        {
            Out += TEXT(" | VIOLATION: ") + Ledger->Violations[Index];
        }
        return FStepOutcome::Pass(Out);
    };
    return Def;
}

struct FR4Person
{
    const TCHAR* Id;
    double HeightCm;
};
// Frozen by R4-06 (R4-02 uses Debashis 171 and Mahua 162).
const FR4Person R4Cast[] = {{TEXT("person.debashis_pal"), 171.0}, {TEXT("person.mahua_saha"), 162.0}, {TEXT("person.purnima_das"), 155.0},
                            {TEXT("person.arun_paul"), 176.0}, {TEXT("person.rekha_bose"), 161.0}};
// The fifteen heights in the cast table at the round-4 baseline (083b0e2); none may move.
const FR4Person R4Baseline[] = {
    {TEXT("person.farida_qadri"), 158.0}, {TEXT("person.mira_sen"), 160.0}, {TEXT("person.ishan_sheikh"), 173.0},
    {TEXT("person.tara_das"), 168.0}, {TEXT("person.amrita_basu"), 163.0}, {TEXT("person.ananya_chatterjee"), 166.0},
    {TEXT("person.amit_roy"), 178.0}, {TEXT("person.care_worker"), 162.0}, {TEXT("person.household_adult"), 168.0},
    {TEXT("person.anindita_sen"), 154.0}, {TEXT("person.leela_dutta"), 176.0}, {TEXT("person.subrata_dey"), 181.0},
    {TEXT("person.rupa_ghosh"), 157.0}, {TEXT("person.nirmal_bose"), 174.0}, {TEXT("person.sahana_mitra"), 164.0}};

FString R4MeasureCast(UPWAcceptanceSubsystem&, PWScaleR4::FLedger& L)
{
    using namespace PWScaleR4;
    FString Summary;
    for (const FR4Person& Person : R4Cast)
    {
        const FString Id = Person.Id;
        const FName Name(Person.Id);
        const PWNpcLooks::FCastRow* Row = PWNpcLooks::FindCast(Name);
        L.Expect(Row != nullptr && FMath::IsNearlyEqual(Row->HeightCm, Person.HeightCm, 0.001), Id + TEXT(": table height is not the frozen value"));
        const double H = PWNpc::StandingHeightCm(Name, EPWNpcOutfit::ParentHousehold);
        L.Expect(FMath::IsNearlyEqual(H, Person.HeightCm, 0.001), Id + TEXT(": StandingHeightCm differs from the frozen value"));
        L.Expect(FMath::IsNearlyEqual(PWNpc::CapsuleHalfHeightCm(Name, EPWNpcOutfit::ShopProprietor), H * 0.5, 0.001), Id + TEXT(": capsule half height is not H/2"));
        TArray<FPWNpcBlock> Blocks;
        PWNpcLooks::BuildBlocks(Name, EPWNpcOutfit::ParentHousehold, H, Blocks);
        double Top = -1e9, Bottom = 1e9, HairTop = -1e9, ReachX = 0.0, ReachY = 0.0, EyeZ = 0.0;
        for (const FPWNpcBlock& B : Blocks)
        {
            const double BlockTop = B.Centre.Z + B.Size.Z * 0.5;
            Top = FMath::Max(Top, BlockTop);
            Bottom = FMath::Min(Bottom, B.Centre.Z - B.Size.Z * 0.5);
            if (B.Centre.Z > 0.85 * H) { HairTop = FMath::Max(HairTop, BlockTop); } // head, hair, wrap and cap
            ReachX = FMath::Max(ReachX, FMath::Abs(B.Centre.X) + B.Size.X * 0.5);
            ReachY = FMath::Max(ReachY, FMath::Abs(B.Centre.Y) + B.Size.Y * 0.5);
            if (B.Tint == FColor(30, 26, 26) && FMath::IsNearlyEqual(B.Size.Z, 2.4) && B.Centre.X > 10.0) { EyeZ = B.Centre.Z; } // AppendFace's eyes
        }
        L.Expect(FMath::Abs(Top - H) <= 0.05, Id + TEXT(": feet-to-head is ") + Num(Top - Bottom) + TEXT(", not the authored height"));
        L.Expect(FMath::Abs(Bottom) <= 0.05, Id + TEXT(": the feet are not on the floor"));
        L.Expect(HairTop <= H + 0.05, Id + TEXT(": hair, wrap or cap rises above the authored height"));
        L.Expect(ReachX <= PWNpc::CapsuleRadius + 0.01 && ReachY <= PWNpc::CapsuleRadius + 0.01, Id + TEXT(": a block leaves the 34 cm capsule"));
        L.Expect(H <= DoorClearCm - HeadMarginCm, Id + TEXT(": under 15 cm of head clearance below a 210 cm door"));
        L.Expect(2.0 * PWNpc::CapsuleRadius <= 90.0, Id + TEXT(": the capsule does not fit a 90 cm doorway"));
        L.Expect(FMath::Abs(EyeZ - 0.925 * H) <= 0.05, Id + TEXT(": the gaze is not at 0.925 H"));
        for (const double Extreme : Extremes) // the eyeline for conversation: their eyes against the eyes of a player at each extreme
        {
            L.Expect(FMath::Abs(EyeZ - Extreme * EyeFraction) < 45.0, Id + TEXT(": eyeline more than 45 cm from a ") + Num(Extreme) + TEXT(" cm player's"));
        }
        Summary += FString::Printf(TEXT("%s %.0fcm eye %.1f reach %.1fx%.1f; "), *Id.RightChop(7), H, EyeZ, ReachX, ReachY);
    }
    for (const FR4Person& New : R4Cast)
    {
        for (const PWNpcLooks::FCastRow& Other : PWNpcLooks::CastTable())
        {
            if (FString(Other.PersonId) == New.Id || PWNpcLooks::LookOf(FName(Other.PersonId)) == PWNpcLooks::ELook::Outfit) { continue; }
            const int32 Differences = PWNpcLooks::SilhouetteDifferences(PWNpcLooks::SilhouetteOf(FName(New.Id), EPWNpcOutfit::ParentHousehold),
                                                                         PWNpcLooks::SilhouetteOf(FName(Other.PersonId), EPWNpcOutfit::ParentHousehold));
            L.Expect(Differences >= 2, FString(New.Id) + TEXT(" and ") + Other.PersonId + TEXT(" differ in fewer than two silhouette attributes"));
        }
    }
    return Summary;
}

FString R4MeasureBaseline(UPWAcceptanceSubsystem&, PWScaleR4::FLedger& L)
{
    using namespace PWScaleR4;
    for (const FR4Person& Person : R4Baseline)
    {
        L.Expect(FMath::IsNearlyEqual(PWNpc::StandingHeightCm(FName(Person.Id), EPWNpcOutfit::HouseholdAdult), Person.HeightCm, 0.001),
                 FString(Person.Id) + TEXT(": height moved since the round-4 baseline"));
    }
    for (const PWNpcLooks::FCastRow& Row : PWNpcLooks::CastTable())
    {
        L.Expect(Row.HeightCm >= BandLowCm && Row.HeightCm <= BandHighCm, FString(Row.PersonId) + TEXT(": outside the supported 145 to 195 cm band"));
    }
    return FString::Printf(TEXT("%d baseline heights unchanged, %d cast rows inside 145-195"), static_cast<int32>(UE_ARRAY_COUNT(R4Baseline)), PWNpcLooks::CastTable().Num());
}

FString R4MeasureLiveCast(UPWAcceptanceSubsystem& Sub, PWScaleR4::FLedger& L)
{
    using namespace PWScaleR4;
    UWorld* World = Sub.GetPlayWorld();
    const FProbe Probe(World);
    int32 Live = 0;
    FString Present;
    for (TActorIterator<APWNpcCharacter> It(World); It; ++It)
    {
        const APWNpcCharacter* Npc = *It;
        const FString Id = Npc->GetPersonId().ToString();
        const double Expected = PWNpc::StandingHeightCm(Npc->GetPersonId(), Npc->GetOutfit());
        const double Capsule = Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0;
        L.Expect(FMath::IsNearlyEqual(Npc->GetStandingHeightCm(), Expected, 0.01) && FMath::IsNearlyEqual(Capsule, Expected, 0.01),
                 Id + TEXT(": live capsule ") + Num(Capsule) + TEXT(" cm differs from the authored ") + Num(Expected));
        const FVector Feet = Npc->GetActorLocation() - FVector(0.0, 0.0, Capsule * 0.5);
        FHitResult Floor;
        if (World->LineTraceSingleByChannel(Floor, Feet + FVector(0.0, 0.0, 20.0), Feet - FVector(0.0, 0.0, 40.0), ECC_Pawn, Probe.Params))
        {
            L.Expect(FMath::Abs(Feet.Z - Floor.Location.Z) <= 6.0, Id + TEXT(": the feet are not on the floor"));
        }
        double Above = 0.0;
        Probe.Up(Feet + FVector(0.0, 0.0, Capsule), Above, 200.0);
        L.Expect(Above >= 5.0, Id + TEXT(": the head is within 5 cm of a ceiling or awning"));
        ++Live;
        for (const FR4Person& Person : R4Cast) { if (Id == Person.Id) { Present += Id.RightChop(7) + TEXT(" "); } }
    }
    return FString::Printf(TEXT("%d live people measured (capsule, feet, head room); round-4 newcomers present: %s(absent ones are spawned by R4-02 and R4-03)"), Live, *Present);
}

FVector R4Landing()
{
    const PWScaleR4::LI::FrontageEntry* Row = PWScaleR4::TallFlatsRow();
    return Row != nullptr ? PWScaleR4::RowPoint(*Row, 550.0, 715.0, 315.0) : FVector::ZeroVector;
}

FStepDef R4StartStep()
{
    FStepDef Def;
    Def.Name = TEXT("r4_declared_start_landing");
    Def.TimeoutSeconds = 20.0;
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr || PWScaleR4::TallFlatsRow() == nullptr) { return FStepOutcome::Fail(TEXT("no pawn or tall flats row")); }
        if (Rt.Phase == 0)
        {
            // The only teleport: the upper landing the tall_flats_upstairs scenario also walks to.
            if (!Pawn->TeleportTo(R4Landing() + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 2.0), FRotator(0.0, 90.0, 0.0))) { return FStepOutcome::Fail(TEXT("declared start refused")); }
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(TEXT("one declared start on the tall flats' upper landing, supported on the floor"));
    };
    return Def;
}

FStepDef R4CameraStep(const R4Shared& Ledger, const EPWCameraMode Mode)
{
    const bool bFirst = Mode == EPWCameraMode::FirstPerson;
    FStepDef Def;
    Def.Name = bFirst ? TEXT("r4_camera_first") : TEXT("r4_camera_third");
    Def.TimeoutSeconds = 20.0;
    Def.Run = [Ledger, Mode, bFirst](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        using namespace PWScaleR4;
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        const APlayerController* Controller = Sub.GetController();
        if (Pawn == nullptr || Controller == nullptr || Controller->PlayerCameraManager == nullptr) { return FStepOutcome::Fail(TEXT("no pawn or camera manager")); }
        if (Rt.Phase == 0)
        {
            Pawn->SetCameraMode(Mode);
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.6) { return FStepOutcome::Wait(); }
        FLedger& L = *Ledger;
        const int32 Before = L.Violations.Num(), ChecksBefore = L.Checks;
        L.Expect(Pawn->GetCameraMode() == Mode, TEXT("the requested camera mode is not active"));
        const double Feet = Pawn->GetActorLocation().Z - PWPlayer::CapsuleHalfHeight;
        L.Expect(FMath::Abs(Feet - R4Landing().Z) <= 6.0, TEXT("the pawn is not standing on the upper floor (feet ") + Num(Feet) + TEXT(")"));
        const FProbe Probe(Sub.GetPlayWorld());
        double Ceiling = 0.0;
        Probe.Up(FVector(Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Feet + 2.0), Ceiling);
        Ceiling += 2.0;
        const double CameraRel = Controller->PlayerCameraManager->GetCameraLocation().Z - Feet;
        L.Expect(Ceiling - PlayerBodyCm >= HeadMarginCm, TEXT("the 165 cm body has under 15 cm to the ceiling"));
        if (bFirst)
        {
            L.Expect(FMath::Abs(CameraRel - PWPlayer::EyeHeightAboveFloor) <= 2.0, TEXT("first-person eye is ") + Num(CameraRel) + TEXT(" cm above the floor, not 155"));
        }
        else
        {
            L.Expect(CameraRel > 20.0 && CameraRel <= Ceiling - 6.0, TEXT("third-person camera ") + Num(CameraRel) + TEXT(" cm up is outside the room (ceiling ") + Num(Ceiling) + TEXT(")"));
        }
        for (const double Extreme : Extremes)
        {
            L.Expect(Ceiling - Extreme >= HeadMarginCm, TEXT("a ") + Num(Extreme) + TEXT(" cm player has under 15 cm to the ceiling on the landing"));
            if (bFirst) { L.Expect(Ceiling - Extreme * EyeFraction >= 20.0, TEXT("a ") + Num(Extreme) + TEXT(" cm player's eye is within 20 cm of the ceiling")); }
        }
        FString Detail = FString::Printf(TEXT("%s camera: feet %.1f, ceiling %.1f above them, camera %.1f above them | %d checks, %d violations"),
                                         bFirst ? TEXT("first-person") : TEXT("third-person"), Feet, Ceiling, CameraRel, L.Checks - ChecksBefore, L.Violations.Num() - Before);
        for (int32 Index = Before; Index < L.Violations.Num() && Index < Before + 8; ++Index) { Detail += TEXT(" | VIOLATION: ") + L.Violations[Index]; }
        return FStepOutcome::Pass(Detail);
    };
    return Def;
}

FStepDef R4VerdictStep(const R4Shared& Ledger)
{
    FStepDef Def;
    Def.Name = TEXT("r4_verdict");
    Def.TimeoutSeconds = 5.0;
    Def.Run = [Ledger](UPWAcceptanceSubsystem&, FStepRuntime&) -> FStepOutcome
    {
        if (Ledger->Violations.Num() == 0) { return FStepOutcome::Pass(FString::Printf(TEXT("%d measurements, no violation"), Ledger->Checks)); }
        return FStepOutcome::Fail(FString::Printf(TEXT("%d violations in %d measurements; the owning order is blocked. First: %s"), Ledger->Violations.Num(), Ledger->Checks, *Ledger->Violations[0]));
    };
    return Def;
}
} // namespace

namespace PWAcceptance
{
void AppendScaleRound4Steps(TArray<FStepDef>& OutSteps)
{
    const R4Shared Ledger = MakeShared<PWScaleR4::FLedger>();
    OutSteps.Add(MakeWorldReadyStep());
    OutSteps.Add(MakeEnterWorldStep());
    OutSteps.Add(R4Measure(TEXT("r4_cast_measured"), Ledger, R4MeasureCast));
    OutSteps.Add(R4Measure(TEXT("r4_cast_baseline_unchanged"), Ledger, R4MeasureBaseline));
    OutSteps.Add(R4Measure(TEXT("r4_cast_live_capsules"), Ledger, R4MeasureLiveCast));
    OutSteps.Add(R4Measure(TEXT("r4_door_clearances"), Ledger, PWScaleR4::MeasureDoors));
    OutSteps.Add(R4Measure(TEXT("r4_seats"), Ledger, PWScaleR4::MeasureSeats));
    OutSteps.Add(R4Measure(TEXT("r4_work_surfaces"), Ledger, PWScaleR4::MeasureWork));
    OutSteps.Add(R4Measure(TEXT("r4_ground_ceilings"), Ledger, PWScaleR4::MeasureGroundCeilings));
    OutSteps.Add(R4Measure(TEXT("r4_upstairs_headroom"), Ledger, PWScaleR4::MeasureUpstairs));
    OutSteps.Add(R4StartStep());
    OutSteps.Add(R4CameraStep(Ledger, EPWCameraMode::FirstPerson));
    OutSteps.Add(MakeShotStep(TEXT("r4_shot_first_person")));
    OutSteps.Add(R4CameraStep(Ledger, EPWCameraMode::ThirdPerson));
    OutSteps.Add(MakeShotStep(TEXT("r4_shot_third_person")));
    OutSteps.Add(R4VerdictStep(Ledger));
}
} // namespace PWAcceptance
