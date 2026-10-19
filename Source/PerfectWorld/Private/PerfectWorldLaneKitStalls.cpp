#include "PerfectWorldLaneKitStalls.h"

#include "Doors.h"
#include "Math/RotationMatrix.h"
#include "PerfectWorldLaneShops.h"

namespace PWLane
{
namespace
{
// File-local names start with Ks (kit stalls): the module builds in unity blobs.
constexpr double KsFoot = FootpathHeight;
constexpr double KsDepth = StallRecessDepth;           // the stall is the generic niche brought forward by this much
constexpr double KsCounterTop = KsFoot + 90.0;         // as in AddOpenStall
constexpr double KsAwningWallZ = 285.0;                // as in AddOpenStall: where the slope meets the wall
constexpr double KsAwningEdgeZ = 247.0;                // the slope's lower edge
constexpr double KsValanceBottomZ = KsFoot + 210.0;    // 225: 210 above the footpath, a full head of clearance
constexpr double KsValanceTopZ = KsAwningEdgeZ + 2.0;
constexpr double KsJamb = 5.0;                         // the kit's trim strip beside a window (IkJamb)
constexpr double KsOverlapSlack = 0.01;
constexpr int32 KsSlots = 5;
constexpr int32 KsShelfRows = 3;
constexpr TCHAR KsBuildingPrefix[] = TEXT("building.nabapara.");

const TCHAR* const KsShopIds[] = {TEXT("building.nabapara.market_stalls_a"), TEXT("building.nabapara.market_stalls_b"),
                                  TEXT("building.nabapara.end_shop")};

ELaneColour KsGoodsColour(uint32 R)
{
    static const ELaneColour Palette[] = {ELaneColour::GoodsYellow, ELaneColour::GoodsOrange, ELaneColour::GoodsRed,
                                          ELaneColour::GoodsGreen, ELaneColour::TrimLight};
    return Palette[R % UE_ARRAY_COUNT(Palette)];
}

int32 KsCrateCount(double U0, double U1)
{
    return FMath::Clamp(FMath::FloorToInt((U1 - U0 - 20.0) / 50.0), 2, 6);
}

FString KsClock(int32 Minute)
{
    return FString::Printf(TEXT("%02d:%02d"), Minute / 60, Minute % 60);
}

bool KsTouches(double A0, double A1, double B0, double B1)
{
    return A1 > B0 && A0 < B1;
}

bool KsFree(const TArray<FLaneSpan>& Windows, double U0, double U1)
{
    for (const FLaneSpan& Window : Windows)
    {
        if (KsTouches(U0, U1, Window.U0, Window.U1))
        {
            return false;
        }
    }
    return true;
}

/** True when [U0, U1] lies inside the frontage between the edge pillars and keeps a pillar's width from every stall already kept. */
bool KsFits(const FLaneBuilding& B, const TArray<FKitStallBay>& Kept, double U0, double U1)
{
    if (U0 < KitPublic::StallEdgeCm - KsOverlapSlack || U1 > B.Width - KitPublic::StallEdgeCm + KsOverlapSlack)
    {
        return false;
    }
    for (const FKitStallBay& Other : Kept)
    {
        if (KsTouches(U0, U1, Other.U0 - KitPublic::StallEdgeCm, Other.U1 + KitPublic::StallEdgeCm))
        {
            return false;
        }
    }
    return true;
}

/**
 * One stall: AddOpenStall's shelves, goods, counter, crates, bulb and awning, with its numbers (V in the niche's frame) and its
 * hashes, moved out of the niche by KsDepth so that its back is the building's front face. Shelf slots that would stand in front of a
 * window are left bare (the window stays a window). Collision: the counter and every crate, the same boxes as AddOpenStall's.
 */
void KsAddStall(FLaneScene& S, const FLaneBuilding& B, const FKitStallBay& Bay, const TArray<FLaneSpan>& Windows)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double U0 = Bay.U0;
    const double U1 = Bay.U1;
    const double Mid = 0.5 * (U0 + U1);
    const uint32 R = Bay.Seed;
    constexpr double Rd = KsDepth;
    const auto Put = [&](ELaneColour Colour, double A0, double A1, double V0, double V1, double Z0, double Z1)
    { F.Add(K, Colour, A0, A1, V0 - KsDepth, V1 - KsDepth, Z0, Z1); };

    const double SlotWidth = (U1 - U0 - 24.0) / KsSlots;
    for (int32 Slot = 0; Slot < KsSlots; ++Slot)
    {
        const double A = U0 + 12.0 + Slot * SlotWidth;
        if (!KsFree(Windows, A, A + SlotWidth))
        {
            continue;
        }
        Put(ELaneColour::ShopInterior, A, A + SlotWidth, Rd - 4.0, Rd, KsFoot, 255.0);
        for (int32 Row = 0; Row < KsShelfRows; ++Row)
        {
            const double Z = 85.0 + Row * 62.0;
            Put(ELaneColour::WoodLight, A, A + SlotWidth, Rd - 30.0, Rd - 4.0, Z, Z + 5.0);
            const uint32 G = LaneHash(static_cast<int32>(R), Row, Slot);
            if (G % 4u == 0u)
            {
                continue;
            }
            Put(KsGoodsColour(G >> 3), A + 3.0, A + SlotWidth - 3.0, Rd - 26.0, Rd - 8.0, Z + 5.0,
                Z + 23.0 + static_cast<double>((G >> 6) % 14u));
        }
    }

    // The counter, its cloth and its crates: the only solid parts, standing 37 to 92 cm out from the wall.
    Put(ELaneColour::WoodLight, U0 + 10.0, U1 - 10.0, 12.0, 59.0, KsFoot, KsCounterTop - 5.0);
    Put(ELaneColour::TrimLight, U0 + 6.0, U1 - 6.0, 8.0, 63.0, KsCounterTop - 5.0, KsCounterTop);
    K.PropCollision(F.Pt(U0 + 6.0, 8.0 - KsDepth, KsFoot), F.Pt(U1 - 6.0, 63.0 - KsDepth, KsCounterTop));
    const int32 Crates = KsCrateCount(U0, U1);
    const double CrateWidth = (U1 - U0 - 20.0) / Crates;
    for (int32 I = 0; I < Crates; ++I)
    {
        const uint32 G = LaneHash(static_cast<int32>(R), 40, I);
        const double A = U0 + 10.0 + I * CrateWidth;
        const double Top = KsCounterTop + 20.0 + static_cast<double>((G >> 5) % 14u);
        Put(KsGoodsColour(G), A + 3.0, A + CrateWidth - 3.0, 15.0, 53.0, KsCounterTop, Top);
        K.PropCollision(F.Pt(A + 3.0, 15.0 - KsDepth, KsCounterTop), F.Pt(A + CrateWidth - 3.0, 53.0 - KsDepth, Top));
    }

    // A bare bulb (the mesh only: the lane's few real stall lights stay with the stalls that still stand in a niche).
    Put(ELaneColour::GlassLit, Mid - 6.0, Mid + 6.0, Rd - 26.0, Rd - 14.0, 232.0, 246.0);

    // The awning, from the building's face, with its stripes and valance as in AddOpenStall. Always on stay rods: a pole on the footpath
    // would leave under 120 cm between the pole and the kerb beside a counter that stands 92 cm out.
    static const ELaneColour Stripe[] = {ELaneColour::AwningRed, ELaneColour::AwningBlue, ELaneColour::AwningGreen};
    const ELaneColour Awning = Stripe[(R >> 2) % UE_ARRAY_COUNT(Stripe)];
    const double Projection = FMath::Min(165.0, 125.0 + B.Setback);
    const double Run = Projection - 6.0;
    const FVector Slope = (F.V * -Run + FVector(0.0, 0.0, KsAwningEdgeZ - KsAwningWallZ)).GetSafeNormal();
    const double SlopeLength = FMath::Sqrt(Run * Run + (KsAwningWallZ - KsAwningEdgeZ) * (KsAwningWallZ - KsAwningEdgeZ));
    const FRotator Tilt = FRotationMatrix::MakeFromXY(F.U, Slope).Rotator();
    const int32 Stripes = FMath::Max(2, FMath::CeilToInt((U1 - U0 + 20.0) / 36.0));
    const double StripeWidth = (U1 - U0 + 20.0) / Stripes;
    for (int32 I = 0; I < Stripes; ++I)
    {
        const double Uc = U0 - 10.0 + (I + 0.5) * StripeWidth;
        K.Tilted((I % 2 == 0) ? Awning : ELaneColour::TrimLight, F.Pt(Uc, -(Projection + 6.0) * 0.5, 0.5 * (KsAwningWallZ + KsAwningEdgeZ)),
                 FVector(StripeWidth, SlopeLength, 4.0), Tilt);
    }
    F.Add(K, Awning, U0 - 10.0, U1 + 10.0, -Projection - 4.0, -Projection, KsValanceBottomZ, KsValanceTopZ);
    for (const double RodU : {U0 - 6.0, U1 + 6.0})
    {
        K.Wire(ELaneColour::TrimDark, F.Pt(RodU, -3.0, 262.0), F.Pt(RodU, -Projection + 3.0, 232.0), 3.0);
    }
}

/** The board on the door canopy: a blue face with a pale rail above and below. The text itself is APWShopHoursSignActor's. */
void KsAddSignBoard(FLaneBlocks& K, const FLaneFrame& F, double DoorU)
{
    const double U0 = DoorU - KitPublic::SignHalfWidthCm;
    const double U1 = DoorU + KitPublic::SignHalfWidthCm;
    constexpr double Face = KitPublic::SignFaceV;
    F.Add(K, ELaneColour::SignBlue, U0, U1, Face, Face + 4.0, KitPublic::SignBottomZ, KitPublic::SignTopZ);
    F.Add(K, ELaneColour::TrimLight, U0 - 2.0, U1 + 2.0, Face - 1.5, Face + 4.0, KitPublic::SignBottomZ, KitPublic::SignBottomZ + 2.0);
    F.Add(K, ELaneColour::TrimLight, U0 - 2.0, U1 + 2.0, Face - 1.5, Face + 4.0, KitPublic::SignTopZ - 2.0, KitPublic::SignTopZ);
}
} // namespace

TArray<FName> UnstaffedBrowseShopIds()
{
    TArray<FName> Ids;
    for (const TCHAR* Id : KsShopIds)
    {
        Ids.Add(FName(Id));
    }
    return Ids;
}

bool IsUnstaffedBrowseShop(FName BuildingId)
{
    for (const TCHAR* Id : KsShopIds)
    {
        if (BuildingId == FName(Id))
        {
            return true;
        }
    }
    return false;
}

void GetGenericStallBays(const FLaneBuilding& B, TArray<FKitStallBay>& OutBays)
{
    OutBays.Reset();
    // The same arithmetic as BuildLaneGroundFloor: mode 1 of (Index + side) mod 3 is the open-stall frontage; bay 0 is always open.
    const int32 Mode = (B.Index + (B.Side > 0 ? 1 : 0)) % 3;
    if (Mode != 1)
    {
        return;
    }
    const double W = B.Width;
    const int32 Bays = FMath::Clamp(FMath::FloorToInt((W - 40.0) / 300.0), 1, 9);
    const double Pillar = 40.0;
    const double BayWidth = (W - Pillar * (Bays + 1)) / Bays;
    for (int32 I = 0; I < Bays; ++I)
    {
        const double U0 = Pillar + I * (BayWidth + Pillar);
        const uint32 R = LaneHash(static_cast<int32>(B.Seed), I, 3);
        if (I == 0 || (R & 1u) != 0u)
        {
            FKitStallBay Bay;
            Bay.U0 = U0;
            Bay.U1 = U0 + BayWidth;
            Bay.Seed = R;
            OutBays.Add(Bay);
        }
    }
}

void PlanKitStalls(const FLaneBuilding& B, double DoorU, TArray<FKitStallBay>& OutBays)
{
    TArray<FKitStallBay> Generic;
    GetGenericStallBays(B, Generic);
    OutBays.Reset();
    const double ClearLo = DoorU - KitPublic::ApproachHalfCm;
    const double ClearHi = DoorU + KitPublic::ApproachHalfCm;
    for (const FKitStallBay& Bay : Generic)
    {
        if (!KsTouches(Bay.U0, Bay.U1, ClearLo, ClearHi))
        {
            OutBays.Add(Bay);
        }
    }
    for (const FKitStallBay& Bay : Generic)
    {
        if (!KsTouches(Bay.U0, Bay.U1, ClearLo, ClearHi))
        {
            continue;
        }
        const double Width = Bay.U1 - Bay.U0;
        FKitStallBay Moved = Bay;
        Moved.bMoved = true;
        // The same stall, whole, on the side of the approach where it fits (east first); then the larger side remnant of it; else none.
        if (KsFits(B, OutBays, ClearHi, ClearHi + Width))
        {
            Moved.U0 = ClearHi;
            Moved.U1 = ClearHi + Width;
            OutBays.Add(Moved);
        }
        else if (KsFits(B, OutBays, ClearLo - Width, ClearLo))
        {
            Moved.U0 = ClearLo - Width;
            Moved.U1 = ClearLo;
            OutBays.Add(Moved);
        }
        else
        {
            const double LeftWidth = ClearLo - Bay.U0;
            const double RightWidth = Bay.U1 - ClearHi;
            const bool bLeft = LeftWidth >= RightWidth;
            const double Remnant = bLeft ? LeftWidth : RightWidth;
            const double R0 = bLeft ? Bay.U0 : ClearHi;
            const double R1 = bLeft ? ClearLo : Bay.U1;
            if (Remnant >= KitPublic::StallMinWidthCm - KsOverlapSlack && KsFits(B, OutBays, R0, R1))
            {
                Moved.U0 = R0;
                Moved.U1 = R1;
                OutBays.Add(Moved);
            }
        }
    }
    OutBays.Sort([](const FKitStallBay& A, const FKitStallBay& Z) { return A.U0 < Z.U0; });
}

int32 KitStallCollisionBoxCount(const FKitStallBay& Bay)
{
    return 1 + KsCrateCount(Bay.U0, Bay.U1);
}

FString PostedHoursText(const PerfectWorld::DoorDef& Door)
{
    if (Door.Access != PerfectWorld::DoorAccess::BusinessHours)
    {
        return FString();
    }
    TArray<FString> Parts;
    if (!Door.OpeningWindows.empty())
    {
        for (const PerfectWorld::DoorOpeningWindow& Window : Door.OpeningWindows)
        {
            Parts.Add(KsClock(Window.OpenMinute) + TEXT("-") + KsClock(Window.CloseMinute));
        }
    }
    else if (Door.CloseMinute > Door.OpenMinute)
    {
        Parts.Add(KsClock(Door.OpenMinute) + TEXT("-") + KsClock(Door.CloseMinute));
    }
    return FString::Join(Parts, TEXT(" and "));
}

FString PostedSignText(const PerfectWorld::DoorDef& Door)
{
    const FString Hours = PostedHoursText(Door);
    if (Hours.IsEmpty())
    {
        return FString();
    }
    return FString(TEXT("Browsing hours\n")) + Hours + TEXT("\n") + KitPublic::UnstaffedPhrase;
}

FString BrowseNoticeText(const PerfectWorld::DoorDef& Door)
{
    const FString Hours = PostedHoursText(Door);
    if (Hours.IsEmpty())
    {
        return FString();
    }
    return FString(TEXT("Everyday goods on the shelves and in the bins.\nBrowsing hours ")) + Hours + TEXT(".\n") + KitPublic::UnstaffedPhrase;
}

FName BrowseActorTag(FName BuildingId)
{
    const FString Name = BuildingId.ToString();
    const FString Prefix(KsBuildingPrefix);
    if (!Name.StartsWith(Prefix) || Name.Len() == Prefix.Len())
    {
        return NAME_None;
    }
    return FName(*(FString(TEXT("interaction.nabapara.")) + Name.RightChop(Prefix.Len()) + TEXT(".browse")));
}

FVector KitSignLocation(const FLaneBuilding& B, double DoorU)
{
    return B.Frame.Pt(DoorU, KitPublic::SignFaceV - 2.0, 0.5 * (KitPublic::SignBottomZ + KitPublic::SignTopZ));
}

double KitSignYaw(const FLaneBuilding& B)
{
    // The text's front is its +X axis; it must face the footpath, which is toward -V.
    const FVector Out = -B.Frame.V;
    return FMath::RadiansToDegrees(FMath::Atan2(Out.Y, Out.X));
}

void BuildKitPublicFace(FLaneScene& S, const FLaneBuilding& B, const FLaneKitFrontage& Frontage)
{
    if (!IsUnstaffedBrowseShop(Frontage.BuildingId))
    {
        return;
    }
    const LI::InteriorLayout Layout = MakeKitFrontageLayout(B, Frontage);
    const LI::Opening* Entrance = Layout.Ok() ? KitFrontEntrance(Layout) : nullptr;
    if (Entrance == nullptr)
    {
        return; // the kit refused this building and left it a solid block: no stall or board on a wall that is not there
    }
    const double DoorU = 0.5 * (Entrance->Plan.U0 + Entrance->Plan.U1);

    TArray<FLaneSpan> Windows;
    for (const LI::Opening& Opening : Layout.Openings)
    {
        if (Opening.Kind == LI::OpeningKind::Window)
        {
            FLaneSpan Span;
            Span.U0 = Opening.Plan.U0 - KsJamb;
            Span.U1 = Opening.Plan.U1 + KsJamb;
            Windows.Add(Span);
        }
    }
    TArray<FKitStallBay> Stalls;
    PlanKitStalls(B, DoorU, Stalls);
    for (const FKitStallBay& Bay : Stalls)
    {
        KsAddStall(S, B, Bay, Windows);
    }

    const PerfectWorld::DoorDef* Door = Frontage.Entry != nullptr ? PerfectWorld::FindDoor(Frontage.Entry->EntranceDoorId) : nullptr;
    if (Door != nullptr && !PostedSignText(*Door).IsEmpty())
    {
        KsAddSignBoard(S.Blocks, B.Frame, DoorU);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Kit frontage %s: its front door has no business hours to post; no hours board is built."),
               *Frontage.BuildingId.ToString());
    }
}
} // namespace PWLane
