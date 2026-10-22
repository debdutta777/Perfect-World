#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldLaneBlocks.h"

namespace PWNs
{
using PWLane::ELaneColour;
using PWLane::FLaneBlocks;
using PWLane::FLaneFrame;

constexpr double NsGround = PWLane::GroundZ;                                  // road top, -200
constexpr double NsPath = PWLane::GroundZ + PWLane::FootpathHeight;           // footpath top, -185
constexpr double NsStorey = PWLane::StoreyHeight;                             // 300 floor to floor

/** One axis-aligned box the streets builder emitted; kept so tests can inspect the geometry without a world. */
struct FNsBox
{
    ELaneColour Colour = ELaneColour::Concrete;
    FVector Min = FVector::ZeroVector;
    FVector Max = FVector::ZeroVector;
};

/** An axis-aligned rectangle on the ground plane, in world centimetres (the plan's frame). */
struct FNsRect
{
    double X0 = 0.0;
    double Y0 = 0.0;
    double X1 = 0.0;
    double Y1 = 0.0;

    FNsRect() = default;
    FNsRect(double A, double B, double C, double D)
        : X0(FMath::Min(A, C)), Y0(FMath::Min(B, D)), X1(FMath::Max(A, C)), Y1(FMath::Max(B, D))
    {
    }
    double Width() const { return X1 - X0; }
    double Height() const { return Y1 - Y0; }
    double CentreX() const { return 0.5 * (X0 + X1); }
    double CentreY() const { return 0.5 * (Y0 + Y1); }
    bool IsEmpty() const { return X1 - X0 < 0.5 || Y1 - Y0 < 0.5; }
    /** Positive-area overlap (more than 0.01 cm of depth); tangent rectangles do not overlap. */
    bool Overlaps(const FNsRect& O) const { return X0 < O.X1 - 0.01 && O.X0 < X1 - 0.01 && Y0 < O.Y1 - 0.01 && O.Y0 < Y1 - 0.01; }
    bool Contains(double X, double Y) const { return X >= X0 && X <= X1 && Y >= Y0 && Y <= Y1; }
    FNsRect Grown(double D) const { return FNsRect(X0 - D, Y0 - D, X1 + D, Y1 + D); }
};

/** Pieces of Rect left after removing every cut (axis-aligned subtraction). Pieces under half a centimetre are dropped. */
TArray<FNsRect> NsSubtract(const FNsRect& Rect, const TArray<FNsRect>& Cuts);

/** The sink every streets builder writes through: forwards to the lane's colour-bucketed blocks and optionally logs each box. */
class FNsSink
{
public:
    explicit FNsSink(FLaneBlocks& InBlocks, TArray<FNsBox>* InLog = nullptr) : Blocks(&InBlocks), Log(InLog) {}

    void Box(ELaneColour Colour, const FVector& A, const FVector& B)
    {
        Blocks->Box(Colour, A, B);
        Record(Colour, A, B);
    }
    void Box(ELaneColour Colour, double X0, double Y0, double Z0, double X1, double Y1, double Z1)
    {
        Box(Colour, FVector(X0, Y0, Z0), FVector(X1, Y1, Z1));
    }
    /** A flat slab over a ground rectangle between two absolute heights. */
    void Slab(ELaneColour Colour, const FNsRect& R, double Z0, double Z1) { Box(Colour, R.X0, R.Y0, Z0, R.X1, R.Y1, Z1); }
    /** Invisible collision hugging a visible solid prop (blocks Pawn, Camera, Visibility). */
    void Prop(const FVector& A, const FVector& B)
    {
        Blocks->PropCollision(A, B);
        Record(ELaneColour::PropBlock, A, B);
    }
    void Prop(double X0, double Y0, double Z0, double X1, double Y1, double Z1) { Prop(FVector(X0, Y0, Z0), FVector(X1, Y1, Z1)); }
    /** Invisible Pawn-only boundary. */
    void Barrier(const FVector& A, const FVector& B)
    {
        Blocks->Barrier(A, B);
        Record(ELaneColour::Barrier, A, B);
    }
    void Barrier(double X0, double Y0, double Z0, double X1, double Y1, double Z1) { Barrier(FVector(X0, Y0, Z0), FVector(X1, Y1, Z1)); }
    void Tilted(ELaneColour Colour, const FVector& Centre, const FVector& Size, const FRotator& Rotation)
    {
        Blocks->Tilted(Colour, Centre, Size, Rotation);
    }
    void Wire(ELaneColour Colour, const FVector& From, const FVector& To, double Thickness) { Blocks->Wire(Colour, From, To, Thickness); }
    void SaggingWire(ELaneColour Colour, const FVector& From, const FVector& To, double Sag, double Thickness, int32 Segments)
    {
        Blocks->SaggingWire(Colour, From, To, Sag, Thickness, Segments);
    }

    /** A box in a building frame (U along the frontage, V back into the building, W above GroundZ). */
    void Add(const FLaneFrame& F, ELaneColour Colour, double U0, double U1, double V0, double V1, double W0, double W1)
    {
        Box(Colour, F.Pt(U0, V0, W0), F.Pt(U1, V1, W1));
    }
    /** The frame box as invisible prop collision. */
    void AddProp(const FLaneFrame& F, double U0, double U1, double V0, double V1, double W0, double W1)
    {
        Prop(F.Pt(U0, V0, W0), F.Pt(U1, V1, W1));
    }

    const TArray<FNsBox>* GetLog() const { return Log; }

private:
    void Record(ELaneColour Colour, const FVector& A, const FVector& B)
    {
        if (Log == nullptr)
        {
            return;
        }
        FNsBox Entry;
        Entry.Colour = Colour;
        Entry.Min = FVector(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z));
        Entry.Max = FVector(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z));
        if (Entry.Max.X - Entry.Min.X < 0.5 || Entry.Max.Y - Entry.Min.Y < 0.5 || Entry.Max.Z - Entry.Min.Z < 0.5)
        {
            return; // FLaneBlocks drops boxes this thin, so the log does too
        }
        Log->Add(Entry);
    }

    FLaneBlocks* Blocks;
    TArray<FNsBox>* Log;
};

/** Which way a street face looks: the outward normal, toward the street the building fronts. */
enum class ENsFace : uint8
{
    PlusX,
    MinusX,
    PlusY,
    MinusY
};

/**
 * The frame of a rectangle's street face, as a viewer on the street sees it: U runs to the viewer's right, V runs back into
 * the building, W is height. U x V = +Z always, so lettering along +U reads left to right. OutWidth is the face length and
 * OutDepth the extent along V.
 */
FLaneFrame NsFaceFrame(const FNsRect& R, ENsFace Face, double& OutWidth, double& OutDepth);

/** Frame coordinates (U, V) of a world XY point. */
inline void NsLocal(const FLaneFrame& F, double X, double Y, double& OutU, double& OutV)
{
    const FVector D(X - F.Origin.X, Y - F.Origin.Y, 0.0);
    OutU = D.X * F.U.X + D.Y * F.U.Y;
    OutV = D.X * F.V.X + D.Y * F.V.Y;
}

/** Outward normal as a vector. */
FVector NsNormal(ENsFace Face);

/** The reading hash of a seed and two salts (the lane's LaneHash, kept here so builders stay short). */
inline uint32 NsHash(uint32 Seed, int32 A, int32 B = 0)
{
    return PWLane::LaneHash(static_cast<int32>(Seed), A, B);
}
/** Maps a hash to [0, 1). */
inline double NsUnit(uint32 H) { return PWLane::LaneUnit(H); }
} // namespace PWNs
