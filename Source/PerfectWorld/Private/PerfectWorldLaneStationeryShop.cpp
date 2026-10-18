#include "PerfectWorldLaneStationeryShop.h"

#include "Math/RotationMatrix.h"
#include "PerfectWorldLaneNpcBindings.h"

namespace PWLane
{
namespace
{
// File-local names start with St (stationery): the module builds in unity blobs.
constexpr const char* StBuildingId = "building.nabapara.stationery";
constexpr double StAwningWallZ = 285.0;     // where the awning slope meets the wall
constexpr double StAwningEdgeZ = 247.0;     // the slope's lower edge
constexpr double StValanceBottomZ = FootpathHeight + 210.0; // 210 above the footpath
constexpr double StAwningReach = 90.0;      // the footpath is 210 deep here (60 setback): 120 stays clear of the awning
constexpr double StPlinthTop = 75.0;

struct FStPublicAnchor
{
    const char* Role;
    const TCHAR* Public;
};

constexpr FStPublicAnchor StAnchors[] = {
    {"staff_counter", TEXT("anchor.stationery.counter")},
    {"stock_room", TEXT("anchor.stationery.stock_room")},
    {"packing_desk", TEXT("anchor.stationery.packing_desk")},
    {"front_door", TEXT("anchor.stationery.front_door")},
    {"customer_counter", TEXT("anchor.stationery.customer")},
};

constexpr ELaneColour StBookColours[] = {ELaneColour::GoodsYellow, ELaneColour::GoodsRed,   ELaneColour::SignBlue,
                                         ELaneColour::GoodsGreen,  ELaneColour::GoodsOrange, ELaneColour::ClothTeal,
                                         ELaneColour::ClothMustard, ELaneColour::ClothMagenta};

/** A maps one layout rectangle plus heights above the finished floor to a block. */
void StBlock(FLaneScene& S, const FLaneBuilding& B, ELaneColour C, int32 U0, int32 U1, int32 V0, int32 V1, double H0, double H1)
{
    B.Frame.Add(S.Blocks, C, U0, U1, V0, V1, GroundFloorZ + H0, GroundFloorZ + H1);
}

/** The side a piece is used from: toward its use zone. Returns (sideU, sideV), one of them non-zero. */
bool StFront(const LI::InteriorLayout& L, const LI::Furniture& P, int32& SideU, int32& SideV)
{
    for (const LI::Zone& Z : L.Zones)
    {
        if (Z.OwnerId != P.Id || Z.Kind != LI::ZoneKind::Standing)
        {
            continue;
        }
        const int32 Du = (Z.Area.U0 + Z.Area.U1) - (P.Footprint.U0 + P.Footprint.U1);
        const int32 Dv = (Z.Area.V0 + Z.Area.V1) - (P.Footprint.V0 + P.Footprint.V1);
        SideU = 0;
        SideV = 0;
        if (FMath::Abs(Dv) >= FMath::Abs(Du))
        {
            SideV = Dv >= 0 ? 1 : -1;
        }
        else
        {
            SideU = Du >= 0 ? 1 : -1;
        }
        return true;
    }
    return false;
}

/** Books and boards on the face of a shelf or rack, in a 8 cm layer proud of the solid box (that layer is inside the use zone). */
void StDressShelf(FLaneScene& S, const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Furniture& P, bool bBooks)
{
    int32 SideU = 0;
    int32 SideV = 0;
    if (!StFront(L, P, SideU, SideV))
    {
        return;
    }
    const bool bFrontV = SideV != 0;
    const int32 Lat0 = bFrontV ? P.Footprint.U0 : P.Footprint.V0;
    const int32 Lat1 = bFrontV ? P.Footprint.U1 : P.Footprint.V1;
    const int32 Face = bFrontV ? (SideV > 0 ? P.Footprint.V1 : P.Footprint.V0) : (SideU > 0 ? P.Footprint.U1 : P.Footprint.U0);
    const int32 Sign = bFrontV ? SideV : SideU;
    const auto Put = [&](ELaneColour C, int32 A0, int32 A1, int32 D0, int32 D1, double H0, double H1)
    {
        const int32 Q0 = Sign > 0 ? Face + D0 : Face - D1;
        const int32 Q1 = Sign > 0 ? Face + D1 : Face - D0;
        if (bFrontV)
        {
            StBlock(S, B, C, A0, A1, Q0, Q1, H0, H1);
        }
        else
        {
            StBlock(S, B, C, Q0, Q1, A0, A1, H0, H1);
        }
    };
    const int32 Top = FMath::Min(P.H1, 180);
    for (int32 Board = 44; Board < Top; Board += 44)
    {
        Put(ELaneColour::TrimDark, Lat0 + 2, Lat1 - 2, 0, 8, Board, Board + 3);
    }
    int32 Row = 0;
    for (int32 Base = 3; Base + 20 < Top; Base += 44, ++Row)
    {
        int32 At = Lat0 + 4;
        int32 Index = 0;
        while (At + 8 < Lat1 - 4)
        {
            const uint32 R = LaneHash(P.Footprint.U0 + P.Footprint.V0, Row, Index);
            const int32 Width = 6 + static_cast<int32>(R % 5u);
            const double Height = (bBooks ? 18.0 : 24.0) + static_cast<double>((R >> 8) % (bBooks ? 11u : 17u));
            const int32 Depth = bBooks ? 5 + static_cast<int32>((R >> 16) % 3u) : 8;
            Put(StBookColours[(R >> 4) % UE_ARRAY_COUNT(StBookColours)], At, FMath::Min(At + Width, Lat1 - 4), 0, Depth, Base, Base + Height);
            At += Width + 1;
            ++Index;
        }
    }
}

/** Counter top: a ledger, a bell, a pen cup, a stack of exercise books and a glass case, to either side of the stand points. */
void StDressCounter(FLaneScene& S, const FLaneBuilding& B, const LI::Furniture& P)
{
    const int32 Mid = (P.Footprint.V0 + P.Footprint.V1) / 2;
    const int32 Cu = (P.Footprint.U0 + P.Footprint.U1) / 2;
    const double Top = P.H1;
    StBlock(S, B, ELaneColour::GlassDusk, Cu + 50, Cu + 130, Mid - 18, Mid + 18, Top, Top + 16);
    StBlock(S, B, ELaneColour::GoodsRed, Cu - 110, Cu - 80, Mid - 12, Mid + 12, Top, Top + 4);
    StBlock(S, B, ELaneColour::Chrome, Cu - 70, Cu - 62, Mid - 4, Mid + 4, Top, Top + 7);
    StBlock(S, B, ELaneColour::SignBlue, Cu + 140, Cu + 148, Mid - 4, Mid + 4, Top, Top + 12);
    for (int32 I = 0; I < 4; ++I)
    {
        StBlock(S, B, StBookColours[I], Cu - 170, Cu - 140, Mid - 11 + I, Mid + 11 + I, Top + 3.0 * I, Top + 3.0 * I + 3.0);
    }
}

/** Pens on the display table (standing in cups), and parcels, tape and wrapping paper on the packing desk. */
void StDressTop(FLaneScene& S, const FLaneBuilding& B, const LI::Furniture& P)
{
    const double Top = P.H1;
    if (P.Kind == LI::FurnitureKind::Display)
    {
        for (int32 I = 0; I < 5; ++I)
        {
            const int32 U = P.Footprint.U0 + 8 + I * 14;
            StBlock(S, B, ELaneColour::TrimLight, U, U + 9, P.Footprint.V0 + 8, P.Footprint.V1 - 8, Top, Top + 5);
            StBlock(S, B, StBookColours[(I + 2) % UE_ARRAY_COUNT(StBookColours)], U + 2, U + 5, P.Footprint.V0 + 12, P.Footprint.V0 + 15, Top + 5, Top + 14);
        }
        return;
    }
    const int32 U = P.Footprint.U0;
    const int32 V = P.Footprint.V0;
    StBlock(S, B, ELaneColour::GoodsYellow, U + 12, U + 48, V + 8, V + 34, Top, Top + 14);
    StBlock(S, B, ELaneColour::GoodsYellow, U + 52, U + 82, V + 12, V + 38, Top, Top + 10);
    StBlock(S, B, ELaneColour::ClothMustard, U + 100, U + 140, V + 10, V + 22, Top, Top + 12);
    StBlock(S, B, ELaneColour::TrimDark, U + 100, U + 108, V + 30, V + 38, Top, Top + 4);
}

/** The street face: plinth beside the door, the signboard with letter blocks, and the striped awning on stay rods. */
void StBuildFront(FLaneScene& S, const FLaneBuilding& B)
{
    FLaneBlocks& K = S.Blocks;
    const FLaneFrame& F = B.Frame;
    const double DoorCut = LI::DoorOpeningWidthCm * 0.5 + 5.0;
    F.Add(K, ELaneColour::Plinth, 0.0, StationeryEntranceU - DoorCut, -2.0, 0.0, FootpathHeight, StPlinthTop);
    F.Add(K, ELaneColour::Plinth, StationeryEntranceU + DoorCut, B.Width, -2.0, 0.0, FootpathHeight, StPlinthTop);

    // Signboard (W 238-280) with "STATIONERS" in pale letter blocks centred on the door.
    F.Add(K, ELaneColour::SignBlue, 40.0, B.Width - 40.0, -8.0, 0.0, 238.0, 280.0);
    static const double Letters[] = {20.0, 20.0, 22.0, 20.0, 10.0, 22.0, 22.0, 20.0, 20.0, 20.0};
    double Total = 0.0;
    for (const double L : Letters)
    {
        Total += L + 8.0;
    }
    double U = StationeryEntranceU - 0.5 * Total;
    for (const double L : Letters)
    {
        F.Add(K, ELaneColour::TrimLight, U, U + L, -10.0, -8.0, 250.0, 268.0);
        U += L + 8.0;
    }

    // The awning: alternating stripes on a gentle slope; the valance's lower edge is 210 above the footpath.
    const double Run = StAwningReach - 6.0;
    const FVector Slope = (F.V * -Run + FVector(0.0, 0.0, StAwningEdgeZ - StAwningWallZ)).GetSafeNormal();
    const double SlopeLength = FMath::Sqrt(Run * Run + (StAwningWallZ - StAwningEdgeZ) * (StAwningWallZ - StAwningEdgeZ));
    const FRotator Tilt = FRotationMatrix::MakeFromXY(F.U, Slope).Rotator();
    constexpr double AwningU0 = 20.0;
    const double AwningU1 = B.Width - 20.0;
    const int32 Stripes = FMath::Max(2, FMath::CeilToInt((AwningU1 - AwningU0) / 36.0));
    const double StripeWidth = (AwningU1 - AwningU0) / Stripes;
    for (int32 I = 0; I < Stripes; ++I)
    {
        K.Tilted((I % 2 == 0) ? ELaneColour::AwningRed : ELaneColour::TrimLight,
                 F.Pt(AwningU0 + (I + 0.5) * StripeWidth, -(StAwningReach + 6.0) * 0.5, 0.5 * (StAwningWallZ + StAwningEdgeZ)),
                 FVector(StripeWidth, SlopeLength, 4.0), Tilt);
    }
    F.Add(K, ELaneColour::AwningRed, AwningU0, AwningU1, -StAwningReach - 4.0, -StAwningReach, StValanceBottomZ, StAwningEdgeZ + 2.0);
    for (const double RodU : {AwningU0, AwningU1})
    {
        K.Wire(ELaneColour::TrimDark, F.Pt(RodU, -3.0, 262.0), F.Pt(RodU, -StAwningReach + 3.0, 232.0), 3.0); // all above 230: nothing on the footpath
    }
    if (S.LightSpots.Num() < 40)
    {
        S.LightSpots.Add(F.Pt(StationeryEntranceU, -40.0, 215.0)); // a bulb under the awning, like the open stalls
    }
}
} // namespace

bool IsStationeryBuilding(const FLaneBuilding& B)
{
    return B.Side == 1 && B.Index == 2 && B.Detail == ELaneDetail::Full && FMath::IsNearlyEqual(B.Width, 700.0) &&
           FMath::IsNearlyEqual(B.Depth, 700.0) && FMath::IsNearlyEqual(B.Setback, 60.0) && B.Storeys >= 2 &&
           FMath::IsNearlyEqual(B.Frame.Origin.X, -100.0, 1.0) && FMath::IsNearlyEqual(B.Frame.Origin.Y, 460.0, 1.0);
}

const LI::InteriorLayout& GetStationeryLayout()
{
    static const LI::InteriorLayout Layout = []
    {
        LI::InteriorInput In;
        In.BuildingId = StBuildingId;
        In.Kind = LI::Variant::ShopWithStockroom;
        In.Furniture = LI::FurnitureSet::Stationery;
        In.WidthCm = 700;
        In.DepthCm = 700;
        In.FinishedFloorCm = FMath::RoundToInt(GroundFloorZ);
        In.StoreyHeightCm = FMath::RoundToInt(StoreyHeight);
        In.EntranceCentreU = StationeryEntranceU;
        In.EntranceDoorId = "door.nabapara.stationery_front";
        return LI::GenerateInteriorLayout(In);
    }();
    return Layout;
}

void BuildStationeryShop(FLaneScene& S, const FLaneBuilding& B)
{
    const LI::InteriorLayout& Layout = GetStationeryLayout();
    if (!Layout.Ok())
    {
        UE_LOG(LogTemp, Error, TEXT("Stationery shop layout refused (%hs); the building stays a solid block."), std::string(LI::RefusalCode(Layout.Status)).c_str());
        B.Frame.Add(S.Blocks, B.Wall, 0.0, B.Width, 0.0, B.Depth, 0.0, B.RoofZ());
        return;
    }
    BuildInteriorKit(S, B, Layout);
    StBuildFront(S, B);
    for (const LI::Furniture& P : Layout.Pieces)
    {
        switch (P.Kind)
        {
        case LI::FurnitureKind::Shelf: StDressShelf(S, B, Layout, P, true); break;
        case LI::FurnitureKind::Rack: StDressShelf(S, B, Layout, P, false); break;
        case LI::FurnitureKind::Counter: StDressCounter(S, B, P); break;
        case LI::FurnitureKind::Display:
        case LI::FurnitureKind::Desk: StDressTop(S, B, P); break;
        default: break;
        }
    }
}

bool HasStationeryShop(const FLaneScene& S)
{
    return HasKitDoor(S, StationeryDoorId);
}

void GetStationeryAnchors(const FLaneBuilding& B, TArray<FStationeryAnchor>& Out)
{
    Out.Reset();
    const LI::InteriorLayout& Layout = GetStationeryLayout();
    for (const FStPublicAnchor& Row : StAnchors)
    {
        const LI::Anchor* A = LI::FindAnchor(Layout, std::string(StBuildingId) + ".anchor." + Row.Role);
        if (A == nullptr)
        {
            continue;
        }
        FStationeryAnchor Anchor;
        Anchor.Id = FName(Row.Public);
        Anchor.LayoutId = FName(UTF8_TO_TCHAR(A->Id.c_str()));
        Anchor.Kind = A->Kind;
        Anchor.Local = FVector(A->U, A->V, 0.0);
        Anchor.World = KitPoint(B, Layout, A->U, A->V, 0.0);
        Anchor.YawDegrees = KitYaw(B, A->FaceU, A->FaceV);
        Out.Add(Anchor);
    }
}
FNpBindingProvider NpStationeryBindings(const FLaneBuilding& Building)
{
    FNpBindingProvider PbProvider;
    const LI::InteriorLayout& PbLayout = GetStationeryLayout();
    if (!IsStationeryBuilding(Building) || !PbLayout.Ok()) { return PbProvider; }
    TArray<FStationeryAnchor> PbAnchors;
    GetStationeryAnchors(Building, PbAnchors);
    for (const FStationeryAnchor& PbAnchor : PbAnchors)
    {
        FNpAnchorBinding PbBinding;
        PbBinding.Feet = FTransform(FRotator(0.0, PbAnchor.YawDegrees, 0.0), PbAnchor.World);
        PbBinding.bPreferSit = PbAnchor.Kind == LI::AnchorKind::Seat;
        PbProvider.Anchors.Add(PbAnchor.Id, PbBinding);
    }
    const FName PbFootpath(TEXT("location.nabapara.stationery_footpath"));
    const auto* PbFront = PbProvider.Anchors.Find(FName(StationeryAnchorFrontDoor));
    if (PbFront == nullptr) { return {}; }
    // The layout threshold is already on the footpath at U250/V-45. No extra travel segment is authored.
    const FNpAnchorBinding PbFootAnchor = *PbFront;
    PbProvider.Anchors.Add(PbFootpath, PbFootAnchor);
    // Off-lane places of the Sen household (Anindita's home; Mira's home, school and personal-time destination). Every one
    // enters and leaves the lane at the shop's threshold: the footpath anchor is the front door's transform, so nobody
    // appears or vanishes anywhere else.
    for (const TCHAR* PbOffLane : {TEXT("location.nabapara.sen_home"), TEXT("location.nabapara.school"), TEXT("location.nabapara.mira_out")})
    {
        FNpAnchorBinding PbPlace;
        PbPlace.Kind = ENpBindingKind::OffLane;
        PbPlace.ExitAnchor = PbFootpath;
        PbProvider.Anchors.Add(FName(PbOffLane), PbPlace);
    }

    constexpr const char* PbRouteNames[] = {"counter_to_stock_room", "counter_to_packing_desk", "counter_to_door"};
    for (const char* PbRouteName : PbRouteNames)
    {
        const LI::Route* PbRoute = LI::FindRoute(PbLayout, PbLayout.BuildingId + ".route." + PbRouteName);
        if (PbRoute == nullptr) { return {}; }
        FNpRouteBinding PbLine;
        for (const LI::RoutePoint& PbPoint : PbRoute->Points)
        {
            PbLine.Points.Add(KitPoint(Building, PbLayout, PbPoint.U, PbPoint.V, 0.0));
        }
        const std::string PbPublicId = std::string("route.stationery.") + PbRouteName;
        PbProvider.Routes.Add(FName(UTF8_TO_TCHAR(PbPublicId.c_str())), MoveTemp(PbLine));
    }
    // Off-lane legs: authored lengths only (Anindita's walk home; Mira's school and personal-time walks). They carry no points,
    // so time spent on them is never compressed into the visible walk and the person is absent from the lane meanwhile.
    for (const TCHAR* PbOffLaneRoute : {TEXT("route.sen_home.footpath_to_home"), TEXT("route.mira_school.home_to_school"),
                                        TEXT("route.mira_school.door_to_school"), TEXT("route.mira_out.home_to_out"),
                                        TEXT("route.mira_out.school_to_out")})
    {
        PbProvider.Routes.Add(FName(PbOffLaneRoute), FNpRouteBinding{{}, true});
    }

    for (const LI::Opening& PbOpening : PbLayout.Openings)
    {
        if (PbOpening.Kind == LI::OpeningKind::Window) { continue; }
        FNpDoorwayZone PbZone;
        PbZone.Id = FName(UTF8_TO_TCHAR((PbOpening.DoorId.empty() ? PbOpening.Id : PbOpening.DoorId).c_str()));
        // The front leaf swings both ways. Reserve its sweep on both sides, plus capsule/approach space.
        const double PbApproach = PbOpening.DoorId.empty() ? 65.0 : LI::DoorClearWidthCm + 40.0;
        const double PbV0 = PbOpening.Plan.V0 - PbApproach;
        const double PbV1 = PbOpening.Plan.V1 + PbApproach;
        PbZone.Bounds += KitPoint(Building, PbLayout, PbOpening.Plan.U0 - 34.0, PbV0, 0.0);
        PbZone.Bounds += KitPoint(Building, PbLayout, PbOpening.Plan.U1 + 34.0, PbV1, 212.0);
        if (PbOpening.RoomA == "outside")
        {
            const double PbCentre = (PbOpening.Plan.U0 + PbOpening.Plan.U1) * 0.5;
            PbZone.WaitingFeet = {KitPoint(Building, PbLayout, PbCentre - 150.0, -85.0, 0.0),
                                  KitPoint(Building, PbLayout, PbCentre + 150.0, -85.0, 0.0)};
        }
        else
        {
            // Authored staff work anchors keep waiting away from stockroom furniture and the counter's solid body.
            for (const TCHAR* PbId : {StationeryAnchorCounter, StationeryAnchorStockRoom, StationeryAnchorPackingDesk})
            {
                if (const auto* PbWait = PbProvider.Anchors.Find(FName(PbId)))
                {
                    PbZone.WaitingFeet.Add(PbWait->Feet.GetLocation());
                }
            }
        }
        PbProvider.Doorways.Add(MoveTemp(PbZone));
    }
    return PbProvider;
}
} // namespace PWLane
