#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "LaneFrontageCatalogue.h"
#include "LaneInteriorLayout.h"
#include "PerfectWorldLaneBlocks.h"

class UPWAcceptanceSubsystem;

// Shared by the two scale_round4 files (PerfectWorldAcceptanceScaleR4.cpp: the cast, the steps; ...ScaleR4World.cpp: the measurements of
// the built world and the portable layouts). Named namespace, so nothing here can clash in a unity blob.
namespace PWScaleR4
{
namespace LI = PerfectWorld::LaneInterior;

/** The supported player-height extremes (ASSET_PIPELINE proportions policy 1.45 to 1.95 m). The walking player is a fixed 165 cm. */
inline constexpr double BandLowCm = 145.0;
inline constexpr double BandHighCm = 195.0;
inline constexpr double PlayerBodyCm = 165.0;
inline constexpr double EyeFraction = 155.0 / 165.0; // eye height over body height
inline constexpr double DoorClearCm = 210.0;
inline constexpr double HeadMarginCm = 15.0;
inline constexpr double ProbeMaxCm = 700.0;
inline constexpr double Extremes[] = {BandLowCm, BandHighCm};

/** The violations every measuring step records; the last step fails when any is left. */
struct FLedger
{
    TArray<FString> Violations;
    int32 Checks = 0;
    void Expect(const bool bOk, const FString& What)
    {
        ++Checks;
        if (!bOk) { Violations.Add(What); }
    }
};

inline FString Num(const double Value) { return FString::Printf(TEXT("%.1f"), Value); }
inline FString Text(const std::string_view Id) { return FString(UTF8_TO_TCHAR(std::string(Id).c_str())); }

/** Pawn-channel traces that ignore every pawn, so a person standing in a doorway never reads as a ceiling. */
struct FProbe
{
    UWorld* World = nullptr;
    FCollisionQueryParams Params;

    explicit FProbe(UWorld* InWorld, const AActor* Extra = nullptr) : World(InWorld), Params(FName(TEXT("R4Probe")), false)
    {
        for (TActorIterator<APawn> It(World); It; ++It) { Params.AddIgnoredActor(*It); }
        if (Extra != nullptr) { Params.AddIgnoredActor(Extra); }
    }
    /** Distance up to the first blocking surface; false (and ProbeMaxCm) when nothing is hit. */
    bool Up(const FVector& From, double& OutCm, const double MaxCm = ProbeMaxCm) const
    {
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(Hit, From, From + FVector(0.0, 0.0, MaxCm), ECC_Pawn, Params);
        OutCm = bHit ? Hit.Location.Z - From.Z : MaxCm;
        return bHit;
    }
    /** True when the first blocking surface within MaxCm below From is within 1.5 cm of ExpectedZ (a built floor or step top). */
    bool FloorAt(const FVector& From, const double ExpectedZ, const double MaxCm = 60.0) const
    {
        FHitResult Hit;
        return World->LineTraceSingleByChannel(Hit, From, From - FVector(0.0, 0.0, MaxCm), ECC_Pawn, Params) && !Hit.bStartPenetrating &&
               FMath::Abs(Hit.Location.Z - ExpectedZ) <= 1.5;
    }
    /** The distance from the surface Z up to the first blocking surface at (X, Y), when a floor is built at that Z; false when it is not. */
    bool HeadroomAt(const double X, const double Y, const double FloorZ, double& OutCm) const
    {
        if (!FloorAt(FVector(X, Y, FloorZ + 30.0), FloorZ)) { return false; }
        Up(FVector(X, Y, FloorZ + 2.0), OutCm);
        OutCm += 2.0;
        return true;
    }
};

/** A world point in a catalogue row's frame: U along the frontage (+X), V into the building, above the ground datum. */
inline FVector RowPoint(const LI::FrontageEntry& Row, const double U, const double V, const double AboveGround)
{
    return FVector(Row.OriginXCm + U, Row.Side * (Row.FrontAbsYCm + V), PWLane::GroundZ + AboveGround);
}

/** The first of five sample points in a room (its centre, then its quarter points) that has a built floor; false when none has. */
inline bool RoomHeadroom(const FProbe& Probe, const LI::FrontageEntry& Row, const LI::Rect& Area, const double FloorRelCm, double& OutCm)
{
    const double Fractions[][2] = {{0.5, 0.5}, {0.25, 0.25}, {0.75, 0.25}, {0.25, 0.75}, {0.75, 0.75}};
    for (const auto& F : Fractions)
    {
        const FVector P = RowPoint(Row, Area.U0 + F[0] * Area.Width(), Area.V0 + F[1] * Area.Depth(), FloorRelCm);
        if (Probe.HeadroomAt(P.X, P.Y, P.Z, OutCm)) { return true; }
    }
    return false;
}

inline const LI::FrontageEntry* TallFlatsRow() { return LI::FindFrontageById("building.nabapara.tall_flats"); }

// The built-world measurements (PerfectWorldAcceptanceScaleR4World.cpp). Each records violations in the ledger and returns a summary.
FString MeasureDoors(UPWAcceptanceSubsystem& Sub, FLedger& L);
FString MeasureSeats(UPWAcceptanceSubsystem& Sub, FLedger& L);
FString MeasureWork(UPWAcceptanceSubsystem& Sub, FLedger& L);
FString MeasureGroundCeilings(UPWAcceptanceSubsystem& Sub, FLedger& L);
FString MeasureUpstairs(UPWAcceptanceSubsystem& Sub, FLedger& L);
} // namespace PWScaleR4
