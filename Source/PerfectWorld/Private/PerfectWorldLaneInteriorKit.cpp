#include "PerfectWorldLaneInteriorKit.h"

#include "Doors.h"

#include <algorithm>

namespace PWLane
{
namespace
{
// File-local names start with Ik: the module builds in unity blobs, where another .cpp's helper of the same name would collide.
constexpr double IkProud = 1.5;     // trim stands this far proud of the wall face
constexpr double IkJamb = 5.0;      // trim strip width

struct FIkToken
{
    const char* Token;
    ELaneColour Colour;
};

constexpr FIkToken IkTokens[] = {
    {"wood_light", ELaneColour::WoodLight},         {"door_wood", ELaneColour::DoorWood},
    {"cloth_magenta", ELaneColour::ClothMagenta},   {"cloth_mustard", ELaneColour::ClothMustard},
    {"cloth_teal", ELaneColour::ClothTeal},         {"shutter_steel", ELaneColour::ShutterSteel},
    {"shutter_blue", ELaneColour::ShutterBlue},     {"goods_yellow", ELaneColour::GoodsYellow},
    {"goods_orange", ELaneColour::GoodsOrange},     {"goods_red", ELaneColour::GoodsRed},
    {"goods_green", ELaneColour::GoodsGreen},       {"floor_oxide", ELaneColour::FloorOxide},
    {"floor_tile", ELaneColour::FloorTile},         {"plaster_warm", ELaneColour::PlasterWarm},
    {"plaster_pale", ELaneColour::PlasterPale},     {"tile_kitchen", ELaneColour::TileKitchen},
    {"tile_wash", ELaneColour::TileWash},           {"concrete", ELaneColour::Concrete},
    {"chrome", ELaneColour::Chrome},                {"trim_dark", ELaneColour::TrimDark},
    {"trim_light", ELaneColour::TrimLight},         {"pot_terracotta", ELaneColour::PotTerracotta},
};

FName IkName(const std::string& Id)
{
    return FName(UTF8_TO_TCHAR(Id.c_str()));
}

/** Absolute height above the ground datum: a floor-relative height plus the finished floor; a wall that starts at the floor runs down to the ground. */
double IkW(const LI::InteriorLayout& L, int32 Height, bool bWallFoot = false)
{
    return bWallFoot && Height == 0 ? 0.0 : static_cast<double>(L.FinishedFloorCm + Height);
}

bool IkIsPartition(const LI::WallSegment& S)
{
    return S.WallId.find(".wall.partition") != std::string::npos;
}

void IkBox(FLaneBlocks& K, const FLaneFrame& F, ELaneColour C, const LI::Rect& R, double W0, double W1)
{
    F.Add(K, C, R.U0, R.U1, R.V0, R.V1, W0, W1);
}

/** A 1 cm painted skin on the face of a wall segment that looks into a room. */
void IkVeneers(FLaneBlocks& K, const FLaneFrame& F, const LI::InteriorLayout& L, const LI::WallSegment& S)
{
    const double W0 = IkW(L, S.W0);
    const double W1 = IkW(L, S.W1);
    for (const LI::Room& Room : L.Rooms)
    {
        const LI::Rect& P = S.Plan;
        const LI::Rect& R = Room.Area;
        const int32 U0 = std::max(P.U0, R.U0);
        const int32 U1 = std::min(P.U1, R.U1);
        const int32 V0 = std::max(P.V0, R.V0);
        const int32 V1 = std::min(P.V1, R.V1);
        const ELaneColour Colour = KitColour(Room.WallPalette);
        if (P.V1 == R.V0 && U1 > U0)
        {
            IkBox(K, F, Colour, LI::Rect{U0, U1, P.V1 - 1, P.V1}, W0, W1);
        }
        else if (P.V0 == R.V1 && U1 > U0)
        {
            IkBox(K, F, Colour, LI::Rect{U0, U1, P.V0, P.V0 + 1}, W0, W1);
        }
        else if (P.U1 == R.U0 && V1 > V0)
        {
            IkBox(K, F, Colour, LI::Rect{P.U1 - 1, P.U1, V0, V1}, W0, W1);
        }
        else if (P.U0 == R.U1 && V1 > V0)
        {
            IkBox(K, F, Colour, LI::Rect{P.U0, P.U0 + 1, V0, V1}, W0, W1);
        }
    }
}

/** Dark trim strips around one opening; a window also gets a sill, an invisible glass box (nobody walks through it) and visible glass. */
void IkDressOpening(FLaneBlocks& K, const FLaneFrame& F, const LI::InteriorLayout& L, const LI::Opening& O)
{
    const bool bAlongU = O.Plan.Width() > O.Plan.Depth();
    const int32 A0 = bAlongU ? O.Plan.U0 : O.Plan.V0;
    const int32 A1 = bAlongU ? O.Plan.U1 : O.Plan.V1;
    const double P0 = (bAlongU ? O.Plan.V0 : O.Plan.U0) - IkProud;
    const double P1 = (bAlongU ? O.Plan.V1 : O.Plan.U1) + IkProud;
    const double Mid = 0.5 * (P0 + P1);
    const bool bWindow = O.Kind == LI::OpeningKind::Window;
    const double Sill = bWindow ? IkJamb : 0.0;
    const double W0 = IkW(L, O.W0);
    const double W1 = IkW(L, O.W1);
    const auto Put = [&](ELaneColour C, double a0, double a1, double p0, double p1, double w0, double w1)
    {
        if (bAlongU)
        {
            F.Add(K, C, a0, a1, p0, p1, w0, w1);
        }
        else
        {
            F.Add(K, C, p0, p1, a0, a1, w0, w1);
        }
    };
    Put(ELaneColour::TrimDark, A0 - IkJamb, A0, P0, P1, W0 - Sill, W1 + IkJamb);
    Put(ELaneColour::TrimDark, A1, A1 + IkJamb, P0, P1, W0 - Sill, W1 + IkJamb);
    Put(ELaneColour::TrimDark, A0 - IkJamb, A1 + IkJamb, P0, P1, W1, W1 + IkJamb);
    if (!bWindow)
    {
        return;
    }
    Put(ELaneColour::TrimDark, A0 - IkJamb, A1 + IkJamb, P0, P1, W0 - IkJamb, W0);
    Put(ELaneColour::GlassDusk, A0, A1, Mid - 1.0, Mid + 1.0, W0, W1);
    if (bAlongU)
    {
        K.PropCollision(F.Pt(A0, Mid - 1.0, W0), F.Pt(A1, Mid + 1.0, W1));
    }
    else
    {
        K.PropCollision(F.Pt(Mid - 1.0, A0, W0), F.Pt(Mid + 1.0, A1, W1));
    }
}

/** A ceiling fitting and its warm point light (the light is a RoomLight; SpawnRoomLights makes it a shadowed point light with a 6 cm source). */
void IkAddLight(FLaneScene& S, const FLaneFrame& F, const LI::InteriorLayout& L, const LI::LightSpec& Spec)
{
    const double Ceiling = IkW(L, L.ClearHeightCm);
    S.Blocks.Box(ELaneColour::TrimLight, F.Pt(Spec.U - 11.0, Spec.V - 11.0, Ceiling - 4.0), F.Pt(Spec.U + 11.0, Spec.V + 11.0, Ceiling));
    S.Blocks.Box(ELaneColour::GlassLit, F.Pt(Spec.U - 8.0, Spec.V - 8.0, Ceiling - 8.0), F.Pt(Spec.U + 8.0, Spec.V + 8.0, Ceiling - 4.0));
    FLaneRoomLight Light;
    Light.Position = F.Pt(Spec.U, Spec.V, IkW(L, Spec.HeightCm));
    Light.Radius = static_cast<float>(Spec.RadiusCm);
    Light.Lumens = static_cast<float>(Spec.Lumens);
    S.RoomLights.Add(Light);
}
} // namespace

ELaneColour KitColour(const std::string& Token, ELaneColour Fallback)
{
    for (const FIkToken& Row : IkTokens)
    {
        if (Token == Row.Token)
        {
            return Row.Colour;
        }
    }
    return Fallback;
}

LI::InteriorInput MakeKitInput(const FLaneBuilding& B, const char* BuildingId, LI::Variant Kind, LI::FurnitureSet Furniture,
                               int32 EntranceCentreU, const char* EntranceDoorId)
{
    LI::InteriorInput In;
    In.BuildingId = BuildingId;
    In.Kind = Kind;
    In.Furniture = Furniture;
    In.WidthCm = FMath::RoundToInt(B.Width);
    In.DepthCm = FMath::RoundToInt(B.Depth);
    In.FinishedFloorCm = FMath::RoundToInt(GroundFloorZ);
    In.StoreyHeightCm = FMath::RoundToInt(B.StoreyHeightCm);
    In.EntranceCentreU = EntranceCentreU;
    In.EntranceDoorId = EntranceDoorId != nullptr ? EntranceDoorId : "";
    return In;
}

FVector KitPoint(const FLaneBuilding& B, const LI::InteriorLayout& L, double U, double V, double HeightCm)
{
    return B.Frame.Pt(U, V, static_cast<double>(L.FinishedFloorCm) + HeightCm);
}

double KitYaw(const FLaneBuilding& B, int32 FaceU, int32 FaceV)
{
    const FVector Dir = (B.Frame.U * FaceU + B.Frame.V * FaceV).GetSafeNormal();
    return FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
}

void GetKitAnchors(const FLaneBuilding& B, const LI::InteriorLayout& L, TArray<FKitAnchor>& Out)
{
    Out.Reset();
    for (const LI::Anchor& A : L.Anchors)
    {
        FKitAnchor Anchor;
        Anchor.Id = IkName(A.Id);
        Anchor.Kind = A.Kind;
        Anchor.Location = KitPoint(B, L, A.U, A.V, 0.0);
        Anchor.YawDegrees = KitYaw(B, A.FaceU, A.FaceV);
        Out.Add(Anchor);
    }
}

FLaneDoorSpec KitDoorSpec(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Opening& O)
{
    FLaneDoorSpec Spec;
    Spec.Id = IkName(O.DoorId);
    Spec.Hinge = KitPoint(B, L, O.HingeU, O.HingeV, 0.0);
    // A front door sits in a wall that runs along U; a flat door sits in a partition that runs along V (same test as IkDressOpening).
    const bool bAlongU = O.Plan.Width() > O.Plan.Depth();
    // The closed leaf runs from the hinge across the doorway: along U (toward +U when the hinge is on the low jamb, else -U), or
    // along V for a partition door.
    const bool bHingeLow = bAlongU ? O.HingeU == O.Plan.U0 : O.HingeV == O.Plan.V0;
    const FVector& Across = bAlongU ? B.Frame.U : B.Frame.V;
    const FVector Dir = (bHingeLow ? Across : -Across).GetSafeNormal();
    Spec.ClosedYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
    const FVector LocalY(-Dir.Y, Dir.X, 0.0); // the actor's local +Y is its closed direction turned by +90 degrees of yaw
    // The swing side is the dwelling (or passage) side: +V from a front wall (SweepSign), and for a partition door the side its
    // sweep zone lies on along U, which survives a mirrored layout. That sign is also the inside side LaneLife registers.
    FVector Into = B.Frame.V * O.SweepSign;
    if (!bAlongU)
    {
        double SweepSideU = static_cast<double>(O.SweepSign);
        for (const LI::Zone& Z : L.Zones)
        {
            if (Z.Kind == LI::ZoneKind::DoorSweep && Z.OwnerId == O.Id)
            {
                SweepSideU = Z.Area.U0 >= O.Plan.U1 ? 1.0 : -1.0;
                break;
            }
        }
        Into = B.Frame.U * SweepSideU;
    }
    Spec.PreferredSwing = FVector::DotProduct(Into, LocalY) >= 0.0 ? 1.0 : -1.0; // default swing is into the building
    // The authored door decides: a private dwelling leaf is one-way (NL-01), a lane or business door may swing both ways.
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(O.DoorId);
    Spec.bTwoWay = Def != nullptr ? Def->bTwoWay : true;
    return Spec;
}

void BuildInteriorKit(FLaneScene& S, const FLaneBuilding& B, const LI::InteriorLayout& L)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double FloorW = static_cast<double>(L.FinishedFloorCm);
    const double CeilingW = IkW(L, L.ClearHeightCm);

    // Floors, and one collision slab under every room.
    for (const LI::Room& Room : L.Rooms)
    {
        IkBox(K, F, KitColour(Room.FloorPalette, ELaneColour::FloorOxide), Room.Area, 0.0, FloorW);
    }
    K.PropCollision(F.Pt(L.Interior.U0, L.Interior.V0, 0.0), F.Pt(L.Interior.U1, L.Interior.V1, FloorW));

    // Walls: the exterior body is the building's wall colour (a World colour, so it blocks); partitions are plaster with a
    // collision box of their own. Every face that looks into a room gets a 1 cm painted skin.
    for (const LI::WallSegment& Seg : L.Walls)
    {
        const double W0 = IkW(L, Seg.W0, true);
        const double W1 = IkW(L, Seg.W1);
        if (IkIsPartition(Seg))
        {
            IkBox(K, F, ELaneColour::PlasterPale, Seg.Plan, W0, W1);
            K.PropCollision(F.Pt(Seg.Plan.U0, Seg.Plan.V0, W0), F.Pt(Seg.Plan.U1, Seg.Plan.V1, W1));
        }
        else
        {
            IkBox(K, F, B.Wall, Seg.Plan, W0, W1);
        }
        IkVeneers(K, F, L, Seg);
    }
    for (const LI::Opening& O : L.Openings)
    {
        IkDressOpening(K, F, L, O);
    }

    // The ceiling skin, then the solid storeys above it (also closing the top of every wall).
    IkBox(K, F, ELaneColour::CeilingWhite, L.Interior, CeilingW, CeilingW + 1.0);
    if (B.RoofZ() > CeilingW + 1.0)
    {
        F.Add(K, B.Wall, 0.0, B.Width, 0.0, B.Depth, CeilingW + 1.0, B.RoofZ());
    }

    for (const LI::Furniture& Piece : L.Pieces)
    {
        const double W0 = FloorW + Piece.H0;
        const double W1 = FloorW + Piece.H1;
        IkBox(K, F, KitColour(Piece.Palette, ELaneColour::WoodLight), Piece.Footprint, W0, W1);
        if (Piece.Solid)
        {
            K.PropCollision(F.Pt(Piece.Footprint.U0, Piece.Footprint.V0, W0), F.Pt(Piece.Footprint.U1, Piece.Footprint.V1, W1));
        }
    }
    for (const LI::LightSpec& Light : L.Lights)
    {
        IkAddLight(S, F, L, Light);
    }
    for (const LI::Opening& O : L.Openings)
    {
        if (!O.DoorId.empty())
        {
            S.Doors.Add(KitDoorSpec(B, L, O));
        }
    }
}

bool HasKitDoor(const FLaneScene& S, const TCHAR* DoorId)
{
    const FName Id(DoorId);
    return S.Doors.ContainsByPredicate([&](const FLaneDoorSpec& Door) { return Door.Id == Id; });
}
} // namespace PWLane
