#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UStaticMesh;
class UWorld;

/**
 * Shared pieces for the block-built Kolkata lane backdrop (see PerfectWorldKolkataLane.h).
 *
 * Everything is made of the engine cube, scaled and placed as instances. Instances are grouped by colour, so
 * each colour becomes one instanced static mesh component with one dynamic material instance.
 * Units are centimetres. The lane runs along +X, the road centre line is Y = 0, and the ground is GroundZ.
 */
namespace PWLane
{
constexpr double GroundZ = -200.0;        // top of the road; the camera sits 2 m above it
constexpr double RoadHalfWidth = 250.0;   // a 5 m road
constexpr double FootpathOuter = 400.0;   // |Y| of a building front with no setback (footpath is 1.5 m)
constexpr double FootpathHeight = 15.0;
constexpr double StoreyHeight = 300.0;
constexpr double LaneX0 = -2400.0;        // the lane is closed by a building here, behind the start camera
constexpr double LaneX1 = 6400.0;         // and by a building here, at the far end of the view

// Side alleys that break the building rows (the -Y alley is farther down the lane than the +Y one).
constexpr double AlleyMinusA = 4300.0;
constexpr double AlleyMinusB = 4900.0;
constexpr double AlleyPlusA = 2800.0;
constexpr double AlleyPlusB = 3400.0;

/** One entry per colour bucket. The order must match the table in PerfectWorldLaneBlocks.cpp. */
enum class ELaneColour : uint8
{
    WallOchre,
    WallTerracotta,
    WallTeal,
    WallCream,
    WallPink,
    WallButter,
    WallSage,
    WallDustyBlue,
    TrimLight,
    TrimDark,
    Plinth,
    Stain,
    ShutterGreen,
    ShutterSteel,
    ShutterBlue,
    ShutterStripe,
    ShopInterior,
    WoodLight,
    DoorWood,
    GlassDusk,
    GlassLit,
    Concrete,
    Asphalt,
    Footpath,
    Ground,
    Drain,
    RoadPatch,
    Rail,
    TankBlack,
    TankLid,
    PotTerracotta,
    LeafDark,
    LeafLight,
    Trunk,
    WireBlack,
    AwningRed,
    AwningBlue,
    AwningGreen,
    GoodsYellow,
    GoodsOrange,
    GoodsRed,
    GoodsGreen,
    SignBlue,
    SignRed,
    SignGreen,
    ClothMagenta,
    ClothMustard,
    ClothTeal,
    TaxiYellow,
    TaxiGlass,
    Chrome,
    AcUnit,
    Count
};

constexpr int32 ColourCount = static_cast<int32>(ELaneColour::Count);
constexpr int32 WallColourCount = 8; // the first eight colours are the wall palette

/** A small integer hash so the layout is the same every run (same idea as the old block-test hash). */
inline uint32 LaneHash(int32 A, int32 B = 0, int32 C = 0)
{
    uint32 H = static_cast<uint32>(A) * 0x9E3779B1u ^ static_cast<uint32>(B) * 0x85EBCA77u ^ static_cast<uint32>(C) * 0xC2B2AE3Du;
    H ^= H >> 15;
    H *= 0x2C1B3C6Du;
    H ^= H >> 12;
    H *= 0x297A2D39u;
    H ^= H >> 15;
    return H;
}

/** Maps a hash to [0, 1). */
inline double LaneUnit(uint32 H)
{
    return static_cast<double>(H & 0xFFFFFFu) / 16777216.0;
}

/** Collects cube instances by colour, then turns each colour into one instanced mesh component. */
class FLaneBlocks
{
public:
    /** An axis-aligned box between two world-space corners (any order). Boxes thinner than half a centimetre are dropped. */
    void Box(ELaneColour Colour, const FVector& CornerA, const FVector& CornerB);

    /** A rotated box: Size is the full extent along the box's own X, Y and Z. */
    void Tilted(ELaneColour Colour, const FVector& Centre, const FVector& Size, const FRotator& Rotation);

    /** A thin stretched cube from one point to another (overhead wires, bunting lines). */
    void Wire(ELaneColour Colour, const FVector& From, const FVector& To, double Thickness);

    /** A sagging wire made of straight segments; Sag is the drop at the middle. */
    void SaggingWire(ELaneColour Colour, const FVector& From, const FVector& To, double Sag, double Thickness, int32 Segments);

    /** Spawns the actor and one instanced component per used colour. Returns the component count. */
    int32 Commit(UWorld& World, UStaticMesh& Cube, UMaterialInterface* BaseMaterial, int32& OutInstances);

private:
    TArray<FTransform> Instances[ColourCount];
};

/**
 * A local frame for one building or block: U runs along its frontage, V runs from the front face back into
 * the building, W is height above the ground. Mirroring a building for the other side of the lane is just a
 * different V.
 */
struct FLaneFrame
{
    FVector Origin = FVector::ZeroVector;
    FVector U = FVector(1.0, 0.0, 0.0);
    FVector V = FVector(0.0, 1.0, 0.0);

    FVector Pt(double u, double v, double w) const
    {
        return Origin + U * u + V * v + FVector(0.0, 0.0, w);
    }

    void Add(FLaneBlocks& Blocks, ELaneColour Colour, double U0, double U1, double V0, double V1, double W0, double W1) const
    {
        Blocks.Box(Colour, Pt(U0, V0, W0), Pt(U1, V1, W1));
    }
};

inline FLaneFrame MakeLaneFrame(double OriginX, double OriginY, const FVector& U, const FVector& V)
{
    FLaneFrame Frame;
    Frame.Origin = FVector(OriginX, OriginY, GroundZ);
    Frame.U = U;
    Frame.V = V;
    return Frame;
}

/** The front of a first-row building, kept so overhead wires can reach real walls. */
struct FLaneFront
{
    double X0 = 0.0;
    double X1 = 0.0;
    double AbsY = FootpathOuter;
};

/** Everything the generators share while the lane is being laid out. */
struct FLaneScene
{
    FLaneBlocks Blocks;
    TArray<FLaneFront> Fronts[2]; // index 0 is the -Y side, 1 the +Y side
    TArray<FVector> LightSpots;   // warm bulb positions at open stalls, for the few real point lights

    /** |Y| of the building front at this X on the given side (-1 or +1); the default footpath edge if none. */
    double FrontAbsY(int32 Side, double X) const;
};
} // namespace PWLane
