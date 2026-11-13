#include "PerfectWorldSubrataBindings.h"

#include "Engine/World.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

namespace PWLane
{
namespace
{
// File-local names start with Sb (Subrata bindings): the module builds in unity blobs.

/** One kit frontage as the presenter needs it: the building, its layout and its front entrance (copied, so the struct can move). */
struct FSbSite
{
    FLaneBuilding Building;
    LI::InteriorLayout Layout;
    LI::Opening Entrance;
    double DoorU = 0.0; // centre of the front doorway along the frontage
};

bool SbOpenSite(const FLaneBuilding& Building, const TCHAR* ExpectedId, const int32 ExpectedSide, FSbSite& Out)
{
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(Building, Kit) || Kit.BuildingId != FName(ExpectedId) || Building.Side != ExpectedSide)
    {
        return false;
    }
    Out.Building = Building;
    Out.Layout = MakeKitFrontageLayout(Building, Kit);
    const LI::Opening* Entrance = Out.Layout.Ok() ? KitFrontEntrance(Out.Layout) : nullptr;
    if (Entrance == nullptr)
    {
        return false;
    }
    Out.Entrance = *Entrance;
    Out.DoorU = 0.5 * (Entrance->Plan.U0 + Entrance->Plan.U1);
    return true;
}

FNpAnchorBinding SbLocalAnchor(const FVector& Feet, const double YawDegrees)
{
    FNpAnchorBinding Anchor;
    Anchor.Feet = FTransform(FRotator(0.0, YawDegrees, 0.0), Feet);
    return Anchor;
}

FNpAnchorBinding SbOffLanePlace(const TCHAR* ExitAnchor)
{
    FNpAnchorBinding Place;
    Place.Kind = ENpBindingKind::OffLane;
    Place.ExitAnchor = FName(ExitAnchor);
    return Place;
}

/** The front door's yield zone: the closed leaf plus its sweep and approach on both sides, with two waiting spots on the footpath. */
FNpDoorwayZone SbFrontZone(const FSbSite& Site)
{
    const FLaneBuilding& B = Site.Building;
    const LI::InteriorLayout& L = Site.Layout;
    const LI::Opening& O = Site.Entrance;
    FNpDoorwayZone Zone;
    Zone.Id = FName(UTF8_TO_TCHAR((O.DoorId.empty() ? O.Id : O.DoorId).c_str()));
    // The leaf swings both ways as far as a person is concerned: reserve its sweep and a capsule's approach on each side.
    const double Approach = O.DoorId.empty() ? 65.0 : LI::DoorClearWidthCm + 40.0;
    Zone.Bounds += KitPoint(B, L, O.Plan.U0 - 34.0, O.Plan.V0 - Approach, 0.0);
    Zone.Bounds += KitPoint(B, L, O.Plan.U1 + 34.0, O.Plan.V1 + Approach, 212.0);
    Zone.WaitingFeet = {KitPoint(B, L, Site.DoorU - 150.0, -85.0, 0.0), KitPoint(B, L, Site.DoorU + 150.0, -85.0, 0.0)};
    return Zone;
}

void SbAddDistinct(TArray<FVector>& Points, const FVector& Point)
{
    if (Points.IsEmpty() || !Points.Last().Equals(Point, 0.5))
    {
        Points.Add(Point);
    }
}

double SbYawOf(const FVector& From, const FVector& To)
{
    return (To - From).GetSafeNormal2D().Rotation().Yaw;
}

/** The stand points and the passage routes inside the tall flats; false when the passage cannot hold them. */
bool SbBuildPassage(const FSbSite& Tall, FNpBindingProvider& Out, FVector& OutDoorstep)
{
    const FLaneBuilding& B = Tall.Building;
    const LI::InteriorLayout& L = Tall.Layout;
    const LI::Room* Passage = nullptr;
    for (const LI::Room& Room : L.Rooms)
    {
        if (Room.Kind == LI::RoomKind::Passage) { Passage = &Room; }
    }
    if (Passage == nullptr) { return false; }
    const LI::Rect Area = Passage->Area;
    const int32 Centre = (Area.U0 + Area.U1) / 2;
    const int32 BoardV = Area.V0 + SubrataNoticeboardFrontCm;
    const int32 BoardU = Centre + SubrataNoticeboardSideCm;
    const int32 StairV = Area.V1 - SubrataStairFootBackCm;
    // A capsule (radius 34) must clear both partitions on the centre line and at the noticeboard, and the dead end at the stair.
    const bool bFits = FMath::Abs(Tall.DoorU - Centre) <= 1.0 && Centre - Area.U0 >= 34 && Area.U1 - Centre >= 34 &&
                       Area.U1 - BoardU >= 34 && Area.V0 + 34 <= BoardV && BoardV + 34 <= StairV && StairV + 34 <= Area.V1;
    if (!bFits) { return false; }

    const auto Pt = [&](const int32 U, const int32 V) { return KitPoint(B, L, U, V, 0.0); };
    const FVector Doorstep = Pt(Centre, SubrataDoorstepCm);
    const FVector Board = Pt(BoardU, BoardV);
    const FVector Stair = Pt(Centre, StairV);
    OutDoorstep = Doorstep;
    Out.Anchors.Add(FName(SubrataAnchorFrontDoor), SbLocalAnchor(Doorstep, KitYaw(B, 0, 1)));
    Out.Anchors.Add(FName(SubrataAnchorNoticeboard), SbLocalAnchor(Board, KitYaw(B, 1, 0)));
    Out.Anchors.Add(FName(SubrataAnchorStair), SbLocalAnchor(Stair, KitYaw(B, 0, 1)));

    // Straight in through the doorway on the centre line, then the 20 cm step sideways to the board and back to the line for the stair.
    Out.Routes.Add(FName(SubrataRouteEntryToNoticeboard), FNpRouteBinding{{Doorstep, Pt(Centre, BoardV), Board}, false});
    Out.Routes.Add(FName(SubrataRouteNoticeboardToStair), FNpRouteBinding{{Board, Pt(Centre, BoardV), Stair}, false});
    return true;
}

/**
 * The two long walks. Both are visible polylines whose lengths equal the schedule's, so the presenter's pace (140 cm per game
 * minute) matches the authored travel time. Lines are derived from the two doorsteps and the footpath walking line, then a
 * crossing X is solved so that the home leg is exactly its authored length.
 */
bool SbBuildLane(const FSbSite& Tall, const FSbSite& Home, const FVector& TallDoorstep, FNpBindingProvider& Out)
{
    const FVector HomeDoorstep = KitPoint(Home.Building, Home.Layout, Home.DoorU, SubrataDoorstepCm, 0.0);
    const FVector HomeFoot = KitPoint(Home.Building, Home.Layout, Home.DoorU, SubrataFootpathLineV, 0.0);
    const double TallFootY = KitPoint(Tall.Building, Tall.Layout, Tall.DoorU, SubrataFootpathLineV, 0.0).Y;
    const double FootZ = HomeDoorstep.Z;
    const double RoadZ = GroundZ;
    const double RoadY = SubrataRoadLineY;

    // -Y doorstep -> -Y footpath line (A) -> across the road (B) -> along the road -> up to the +Y footpath line (C) -> west -> +Y doorstep.
    const double A = FMath::Abs(HomeDoorstep.Y - HomeFoot.Y);
    const double B = RoadY - HomeFoot.Y;
    const double C = TallFootY - RoadY;
    const double EastLeg = FMath::Abs(TallDoorstep.Y - TallFootY);
    if (Home.Building.Side != -1 || Tall.Building.Side != 1 || B <= 0.0 || C <= 0.0) { return false; }
    const double LaneRoundX = TallDoorstep.X + (SubrataLaneRoundToEntryCm - EastLeg); // the lane-round threshold, west to east
    const double CrossX = 0.5 * (SubrataHomeToLaneRoundCm - A - B - C + HomeDoorstep.X + LaneRoundX);
    if (LaneRoundX <= TallDoorstep.X || LaneRoundX < LaneX0 || LaneRoundX > LaneX1 || CrossX < HomeDoorstep.X || CrossX < LaneRoundX) { return false; }

    const FVector LaneRound(LaneRoundX, TallFootY, FootZ);
    TArray<FVector> HomeLeg;
    SbAddDistinct(HomeLeg, HomeDoorstep);
    SbAddDistinct(HomeLeg, FVector(HomeDoorstep.X, HomeFoot.Y, FootZ));
    SbAddDistinct(HomeLeg, FVector(CrossX, HomeFoot.Y, FootZ));
    SbAddDistinct(HomeLeg, FVector(CrossX, RoadY, RoadZ));
    SbAddDistinct(HomeLeg, FVector(LaneRoundX, RoadY, RoadZ));
    SbAddDistinct(HomeLeg, LaneRound);
    TArray<FVector> EntryLeg;
    SbAddDistinct(EntryLeg, LaneRound);
    SbAddDistinct(EntryLeg, FVector(TallDoorstep.X, TallFootY, FootZ));
    SbAddDistinct(EntryLeg, TallDoorstep);

    Out.Anchors.Add(FName(SubrataHomeFootpath), SbLocalAnchor(HomeDoorstep, SbYawOf(HomeDoorstep, HomeLeg[1])));
    Out.Anchors.Add(FName(SubrataLaneRoundFootpath), SbLocalAnchor(LaneRound, SbYawOf(EntryLeg[0], EntryLeg[1])));
    // Both places are off-lane. A person appears at, and leaves by, the footpath anchor named here, which is the first or last point of a route.
    Out.Anchors.Add(FName(SubrataPlaceHome), SbOffLanePlace(SubrataHomeFootpath));
    Out.Anchors.Add(FName(SubrataPlaceLaneRound), SbOffLanePlace(SubrataLaneRoundFootpath));
    Out.Routes.Add(FName(SubrataRouteHomeToLaneRound), FNpRouteBinding{MoveTemp(HomeLeg), false});
    Out.Routes.Add(FName(SubrataRouteLaneRoundToEntry), FNpRouteBinding{MoveTemp(EntryLeg), false});
    return true;
}
} // namespace

bool MakeSubrataBuilding(const TCHAR* BuildingId, FLaneBuilding& Out)
{
    const LI::FrontageEntry* Row = LI::FindFrontageById(std::string(TCHAR_TO_UTF8(BuildingId)));
    if (Row == nullptr || (Row->Side != -1 && Row->Side != 1)) { return false; }
    const double Side = static_cast<double>(Row->Side);
    FLaneBuilding Building;
    Building.Width = Row->WidthCm;
    Building.Depth = Row->DepthCm;
    Building.Setback = Row->SetbackCm;
    Building.Storeys = Row->Storeys;
    Building.Side = Row->Side;
    Building.Index = Row->RowIndex;
    Building.Seed = Row->Seed;
    Building.Detail = ELaneDetail::Full;
    Building.Wall = ELaneColour::WallCream;
    Building.Frame = MakeLaneFrame(Row->OriginXCm, Side * Row->FrontAbsYCm, FVector(1.0, 0.0, 0.0), FVector(0.0, Side, 0.0));
    FLaneKitFrontage Kit;
    if (!IsKitFrontage(Building, Kit) || Kit.BuildingId != FName(BuildingId)) { return false; }
    Out = Building;
    return true;
}

FNpBindingProvider NpSubrataBindings(const FLaneBuilding& TallFlats, const FLaneBuilding& CaretakerFlat)
{
    FSbSite Tall;
    FSbSite Home;
    FNpBindingProvider Provider;
    FVector TallDoorstep = FVector::ZeroVector;
    if (!SbOpenSite(TallFlats, SubrataTallFlatsId, 1, Tall) || !SbOpenSite(CaretakerFlat, SubrataCaretakerFlatId, -1, Home) ||
        Tall.Layout.Kind != LI::Variant::SharedEntry || !SbBuildPassage(Tall, Provider, TallDoorstep) ||
        !SbBuildLane(Tall, Home, TallDoorstep, Provider))
    {
        return FNpBindingProvider();
    }
    // Yield zones for the two front doors (the flats' dwelling doorways are open frames or inward leaves of other people's homes).
    Provider.Doorways.Add(SbFrontZone(Tall));
    Provider.Doorways.Add(SbFrontZone(Home));
    return Provider;
}

bool SpawnSubrata(UWorld& World, const FLaneScene& Scene)
{
    if (!HasKitDoor(Scene, SubrataTallFlatsDoorId) || !HasKitDoor(Scene, SubrataCaretakerDoorId))
    {
        UE_LOG(LogTemp, Warning, TEXT("Subrata: the tall flats or the caretaker's flat was not built; he is not placed."));
        return false;
    }
    FLaneBuilding Tall;
    FLaneBuilding Home;
    if (!MakeSubrataBuilding(SubrataTallFlatsId, Tall) || !MakeSubrataBuilding(SubrataCaretakerFlatId, Home))
    {
        UE_LOG(LogTemp, Error, TEXT("Subrata: the catalogue rows of his two buildings did not resolve; no fallback spawn."));
        return false;
    }
    FNpBindingProvider Provider = NpSubrataBindings(Tall, Home);
    if (Provider.Anchors.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Subrata: his bindings could not be derived from the built layouts; no fallback spawn."));
        return false;
    }
    UPWNpcPresenceSubsystem* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Presence == nullptr || !Presence->Bindings().RegisterProvider(FName(SubrataProviderId), MoveTemp(Provider)))
    {
        UE_LOG(LogTemp, Error, TEXT("Subrata: NPC provider registration failed; no fallback spawn."));
        return false;
    }
    // TODO(outfit): his own look (sage workwear, folding clipboard, grey thinning hair) replaces the placeholder outfit; his 181 cm is already authored.
    if (!Presence->RegisterScheduledPerson(FName(SubrataPersonId), NSLOCTEXT("PerfectWorldLane", "SubrataName", "Subrata"), EPWNpcOutfit::HouseholdAdult))
    {
        UE_LOG(LogTemp, Error, TEXT("Subrata's schedule registration failed; no fallback spawn."));
        return false;
    }
    if (const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World)) { Presence->Reconcile(Life->GetNow()); }
    return true;
}
} // namespace PWLane
