#include "PerfectWorldAcceptanceScaleR4Probe.h"

#include "LaneTallFlatsLayout.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFurnitureActor.h"
#include "PerfectWorldPlayerCharacter.h"

// scale_round4's measurements of the built world and of the portable layouts it was built from (R4-06). See the note at the top of
// PerfectWorldAcceptanceScaleR4.cpp for the method and for how the 145 and 195 cm extremes are applied.

namespace PWScaleR4
{
FString MeasureDoors(UPWAcceptanceSubsystem& Sub, FLedger& L)
{
    UWorld* World = Sub.GetPlayWorld();
    int32 Live = 0;
    double Lowest = 1e9;
    for (TActorIterator<APWDoorActor> It(World); It; ++It)
    {
        const APWDoorActor* Door = *It;
        const FProbe Probe(World, Door);
        // Six vertical probes through the aperture, three along the leaf and two either side of its plane: the lowest blocking surface.
        double Head = 1e9;
        for (const double Along : {10.0, 45.0, 80.0})
        {
            for (const double Across : {-10.0, 10.0})
            {
                const FVector From = Door->GetActorLocation() + Door->GetActorForwardVector() * Along + Door->GetActorRightVector() * Across + FVector(0.0, 0.0, 5.0);
                double Distance = 0.0;
                Probe.Up(From, Distance);
                Head = FMath::Min(Head, Distance + 5.0);
            }
        }
        Lowest = FMath::Min(Lowest, Head);
        const FString Id = Door->GetDoorId().ToString();
        L.Expect(Head >= DoorClearCm - 0.5, Id + TEXT(": lintel at ") + Num(Head) + TEXT(" cm, under the 210 cm contract"));
        for (const double Extreme : Extremes)
        {
            L.Expect(Head - Extreme >= HeadMarginCm - 0.5, Id + TEXT(": a ") + Num(Extreme) + TEXT(" cm player has under 15 cm over the head"));
        }
        ++Live;
    }
    L.Expect(Live > 0, TEXT("no live door actors"));
    // Every doorway of every kit frontage, open or hung: 90 by 210 clear, and the 195 cm extreme clears the head strip.
    int32 Openings = 0;
    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        const LI::InteriorLayout Layout = LI::GenerateInteriorLayout(LI::MakeFrontageInput(Row, 15, 300));
        L.Expect(Layout.Ok(), Text(Row.BuildingId) + TEXT(": layout refused"));
        for (const LI::Opening& O : Layout.Openings)
        {
            if (O.Kind == LI::OpeningKind::Window) { continue; }
            ++Openings;
            L.Expect(O.ClearWidthCm >= 90 && O.W1 - O.W0 >= 210, Text(O.Id) + TEXT(": under 90 by 210 clear"));
            L.Expect(O.W1 - BandHighCm >= HeadMarginCm, Text(O.Id) + TEXT(": a 195 cm player has under 15 cm of head room"));
        }
    }
    return FString::Printf(TEXT("%d live doors, lowest lintel %.1f cm; %d kit doorways against 90x210 and the 145/195 extremes"), Live, Lowest, Openings);
}

FString MeasureSeats(UPWAcceptanceSubsystem& Sub, FLedger& L)
{
    UWorld* World = Sub.GetPlayWorld();
    const FProbe Probe(World);
    int32 Seats = 0;
    for (TActorIterator<APWSeatActor> It(World); It; ++It)
    {
        const APWSeatActor* Seat = *It;
        const double Height = Seat->GetSeatHeightCm();
        const FString Id = Seat->GetSeatId().ToString();
        // The lower leg runs about 0.25 H (145 cm) to 0.285 H (195 cm): a seat outside 36 to 55.6 cm leaves feet in the air or knees high.
        L.Expect(Height >= 0.25 * BandLowCm && Height <= 0.285 * BandHighCm, Id + TEXT(": seat height ") + Num(Height) + TEXT(" cm outside 36 to 55.6"));
        double Ceiling = 0.0;
        Probe.Up(Seat->GetActorLocation() + FVector(0.0, 0.0, 5.0), Ceiling);
        Ceiling += 5.0;
        for (const double Extreme : Extremes)
        {
            L.Expect(Ceiling - (Height + 0.52 * Extreme) >= HeadMarginCm, Id + TEXT(": a seated ") + Num(Extreme) + TEXT(" cm player has under 15 cm to the ceiling"));
        }
        L.Expect(PWSeat::SeatedEyeAboveFloor(static_cast<float>(Height)) < Ceiling, Id + TEXT(": the seated eye reaches the ceiling"));
        ++Seats;
    }
    L.Expect(Seats > 0, TEXT("no live seat actors"));
    // Seat-to-table drop in every furnished layout: room for the thigh of a 195 cm player (0.115 H) and within a 145 cm player's elbow (0.23 H).
    int32 Pairs = 0;
    const auto CheckLayout = [&](const LI::InteriorLayout& Layout, const FString& Label)
    {
        for (const LI::Furniture& Chair : Layout.Pieces)
        {
            if (Chair.Kind != LI::FurnitureKind::Chair) { continue; }
            const LI::Furniture* Nearest = nullptr;
            int32 Best = 1 << 20;
            for (const LI::Furniture& Table : Layout.Pieces)
            {
                if (Table.Kind != LI::FurnitureKind::Table) { continue; }
                const int32 Gap = FMath::Max(FMath::Max(Table.Footprint.U0 - Chair.Footprint.U1, Chair.Footprint.U0 - Table.Footprint.U1),
                                             FMath::Max(Table.Footprint.V0 - Chair.Footprint.V1, Chair.Footprint.V0 - Table.Footprint.V1));
                if (Gap < Best) { Best = Gap; Nearest = &Table; }
            }
            if (Nearest == nullptr || Best > 90) { continue; }
            ++Pairs;
            const double Drop = Nearest->H1 - Chair.H1;
            L.Expect(Drop >= 0.115 * BandHighCm + 2.0 && Drop <= 0.23 * BandLowCm, Label + TEXT(": seat-to-table drop ") + Num(Drop) + TEXT(" cm outside 24.4 to 33.4"));
        }
    };
    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        CheckLayout(LI::GenerateInteriorLayout(LI::MakeFrontageInput(Row, 15, 300)), Text(Row.BuildingId));
    }
    CheckLayout(PerfectWorld::LaneTallFlats::GenerateLayout().Upper, TEXT("tall_flats upper"));
    return FString::Printf(TEXT("%d live seats (ceiling, seated head and eye at the extremes), %d seat-table pairs"), Seats, Pairs);
}

namespace
{
/** The anchors the plan names for the two proprietors (Debashis: repair; Mahua: hardware). */
bool IsStaffAnchor(const FString& Tail, const bool bRepair)
{
    const TCHAR* Repair[] = {TEXT("staff_counter"), TEXT("workbench"), TEXT("finishing_table"), TEXT("tool_rack")};
    const TCHAR* Hardware[] = {TEXT("staff_counter"), TEXT("weigh_station"), TEXT("stock_room")};
    if (bRepair) { for (const TCHAR* Name : Repair) { if (Tail == Name) { return true; } } }
    else { for (const TCHAR* Name : Hardware) { if (Tail == Name) { return true; } } }
    return false;
}
} // namespace

FString MeasureWork(UPWAcceptanceSubsystem&, FLedger& L)
{
    int32 Contacts = 0;
    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        const bool bRepair = Row.Furniture == LI::FurnitureSet::Workshop;
        if (!bRepair && Row.Furniture != LI::FurnitureSet::GeneralGoods) { continue; }
        const LI::InteriorLayout Layout = LI::GenerateInteriorLayout(LI::MakeFrontageInput(Row, 15, 300));
        const FString Label = Text(Row.BuildingId);
        if (!Layout.Ok()) { L.Expect(false, Label + TEXT(": layout refused")); continue; }
        const double Staff = bRepair ? 171.0 : 162.0; // Debashis Pal, Mahua Saha
        for (const LI::Anchor& Anchor : Layout.Anchors)
        {
            const FString Full = Text(Anchor.Id);
            const int32 At = Full.Find(TEXT(".anchor."));
            const FString Tail = At == INDEX_NONE ? Full : Full.Mid(At + 8);
            if (Anchor.Kind != LI::AnchorKind::Stand || Anchor.FurnitureId.empty() || !IsStaffAnchor(Tail, bRepair)) { continue; }
            const LI::Furniture* Piece = LI::FindFurniture(Layout, Anchor.FurnitureId);
            const FString Tag = Label.RightChop(18) + TEXT(" ") + Tail;
            if (Piece == nullptr) { L.Expect(false, Tag + TEXT(": serves no piece")); continue; }
            ++Contacts;
            const LI::Rect& R = Piece->Footprint;
            const double Du = Anchor.U < R.U0 ? R.U0 - Anchor.U : (Anchor.U > R.U1 ? Anchor.U - R.U1 : 0);
            const double Dv = Anchor.V < R.V0 ? R.V0 - Anchor.V : (Anchor.V > R.V1 ? Anchor.V - R.V1 : 0);
            const double Reach = FMath::Sqrt(Du * Du + Dv * Dv);
            L.Expect(Reach <= 0.34 * Staff + 10.0, Tag + TEXT(": ") + Num(Reach) + TEXT(" cm to the piece, beyond the arm's reach"));
            if (Piece->Kind == LI::FurnitureKind::Rack)
            {
                L.Expect(Piece->H1 <= 1.15 * Staff, Tag + TEXT(": rack top ") + Num(Piece->H1) + TEXT(" cm beyond a standing reach"));
                continue;
            }
            // A surface: from the resting hand (0.50 H) to shoulder height less 15 cm for the proprietor; from 0.40 H to 0.84 H less 10 cm at the extremes.
            L.Expect(Piece->H1 >= 0.50 * Staff - 10.0 && Piece->H1 <= 0.84 * Staff - 15.0, Tag + TEXT(": surface ") + Num(Piece->H1) + TEXT(" cm outside the proprietor's working band"));
            for (const double Extreme : Extremes)
            {
                L.Expect(Piece->H1 >= 0.40 * Extreme && Piece->H1 <= 0.84 * Extreme - 10.0, Tag + TEXT(": surface ") + Num(Piece->H1) + TEXT(" cm unusable for a ") + Num(Extreme) + TEXT(" cm person"));
            }
        }
    }
    L.Expect(Contacts >= 5, FString::Printf(TEXT("only %d proprietor work contacts found (the plan names seven)"), Contacts));
    return FString::Printf(TEXT("%d proprietor work contacts (anchor reach, surface or rack height for Debashis 171 and Mahua 162) and the extremes"), Contacts);
}

FString MeasureGroundCeilings(UPWAcceptanceSubsystem& Sub, FLedger& L)
{
    const FProbe Probe(Sub.GetPlayWorld());
    int32 Rooms = 0;
    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        const LI::InteriorLayout Layout = LI::GenerateInteriorLayout(LI::MakeFrontageInput(Row, 15, 300));
        for (const LI::Room& Room : Layout.Rooms)
        {
            double Ceiling = 0.0;
            if (!RoomHeadroom(Probe, Row, Room.Area, 15.0, Ceiling)) { continue; } // not built here: the owning order says so
            ++Rooms;
            L.Expect(Ceiling - BandHighCm >= HeadMarginCm, Text(Room.Id) + TEXT(": ceiling ") + Num(Ceiling) + TEXT(" cm leaves a 195 cm player under 15 cm"));
        }
    }
    L.Expect(Rooms > 0, TEXT("no built kit room found to measure"));
    return FString::Printf(TEXT("%d built ground-floor rooms measured for ceiling clearance at 145 and 195 cm"), Rooms);
}

FString MeasureUpstairs(UPWAcceptanceSubsystem& Sub, FLedger& L)
{
    const LI::FrontageEntry* Row = TallFlatsRow();
    const PerfectWorld::LaneTallFlats::Layout Flats = PerfectWorld::LaneTallFlats::GenerateLayout();
    if (Row == nullptr || !Flats.Ok())
    {
        L.Expect(false, TEXT("tall flats row or layout missing"));
        return TEXT("no layout");
    }
    // The core's promise: every stair strip leaves at least 195 + 15 cm over the walked surface, and the upper storey stands 274 clear.
    double MinStrip = 1e9;
    for (const auto& Clear : Flats.Stair.Clearances)
    {
        if (Clear.Kind == PerfectWorld::LaneStair::ClearKind::Strip) { MinStrip = FMath::Min(MinStrip, (Clear.TopMm - Clear.BottomMm) * 0.1); }
    }
    L.Expect(MinStrip >= BandHighCm + HeadMarginCm, TEXT("stair strip clearance ") + Num(MinStrip) + TEXT(" cm is under 210"));
    L.Expect(Flats.Upper.ClearHeightCm >= BandHighCm + HeadMarginCm + 8.0, TEXT("upper storey clear height is under 218 cm (a ceiling fitting hangs 8 cm)"));
    // The built world: every step stands where the layout says, and the head room above it is at least the strip height.
    const FProbe Probe(Sub.GetPlayWorld());
    double LowestOverStep = 1e9;
    int32 Found = 0;
    for (const auto& Step : Flats.Stair.Steps)
    {
        const double Above = Flats.Stair.LowerFloorCm + Step.TopMm * 0.1;
        const FVector Top = RowPoint(*Row, 0.5 * (Step.Plan.U0 + Step.Plan.U1), 0.5 * (Step.Plan.V0 + Step.Plan.V1), Above);
        double Over = 0.0;
        if (!Probe.HeadroomAt(Top.X, Top.Y, Top.Z, Over))
        {
            L.Expect(false, Text(Step.Id) + TEXT(" is not built at the layout's height"));
            continue;
        }
        ++Found;
        LowestOverStep = FMath::Min(LowestOverStep, Over);
    }
    L.Expect(Found == static_cast<int32>(Flats.Stair.Steps.size()) && Found > 0, FString::Printf(TEXT("%d of %d steps found in the world"), Found, static_cast<int32>(Flats.Stair.Steps.size())));
    if (Found > 0)
    {
        L.Expect(LowestOverStep >= MinStrip - 1.0, TEXT("live head room over a step ") + Num(LowestOverStep) + TEXT(" cm is under the layout's ") + Num(MinStrip));
        for (const double Extreme : Extremes)
        {
            L.Expect(LowestOverStep - Extreme >= HeadMarginCm, TEXT("a ") + Num(Extreme) + TEXT(" cm player has under 15 cm over the head on the stair"));
        }
    }
    // The upper dwelling: ceiling above a clear point of every room.
    double LowestUpper = 1e9;
    int32 UpperRooms = 0;
    for (const auto& Room : Flats.Upper.Rooms)
    {
        double Ceiling = 0.0;
        if (!RoomHeadroom(Probe, *Row, Room.Area, Flats.Upper.FinishedFloorCm, Ceiling))
        {
            L.Expect(false, Text(Room.Id) + TEXT(": no built floor at the layout's height"));
            continue;
        }
        ++UpperRooms;
        LowestUpper = FMath::Min(LowestUpper, Ceiling);
    }
    if (UpperRooms > 0)
    {
        for (const double Extreme : Extremes)
        {
            L.Expect(LowestUpper - Extreme >= HeadMarginCm + 8.0, TEXT("upper dwelling: a ") + Num(Extreme) + TEXT(" cm player is within 23 cm of the ceiling or its fittings"));
        }
    }
    return FString::Printf(TEXT("%d steps found, strip %.1f cm, live over stairs %.1f cm, %d upper rooms, lowest ceiling %.1f cm"), Found, MinStrip, LowestOverStep, UpperRooms, LowestUpper);
}
} // namespace PWScaleR4
