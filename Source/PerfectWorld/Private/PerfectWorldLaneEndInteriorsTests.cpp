// Checks for the two lane-end halls (PerfectWorldLaneEndInteriors.h, R6-05): the closing masses are recognised by their exact frame and
// refused when perturbed, the emitted collision realises the portable layout (a floor slab, every solid piece, every window pane, no
// door spec, one light per layout light), the doorless archway, every aisle, standing place and walking route are clear of collision
// for a walking capsule, the posted hours board faces the lane, and the real lane rows replace the sealed ground floor of exactly these
// two buildings. They run the real builders on bare scenes and read back the collision boxes; no world is needed.
// Run in the editor under "PerfectWorld.LaneEnds" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldLaneEndInteriors.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace PWLaneEndTest
{
using namespace PWLane;

constexpr double LetCapsuleRadius = 34.0;
constexpr double LetCapsuleHeight = 165.0;
constexpr LE::EndHall LetHalls[] = {LE::EndHall::CommunityHall, LE::EndHall::MarketHall};

FString LetName(const LE::EndHall Hall)
{
    return UTF8_TO_TCHAR(std::string(LE::GetHallInfo(Hall).BuildingId).c_str());
}

FBox LetWorldBox(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Rect& R, double H0, double H1)
{
    FBox Box(ForceInit);
    for (const int32 U : {R.U0, R.U1})
    {
        for (const int32 V : {R.V0, R.V1})
        {
            Box += KitPoint(B, L, U, V, H0);
            Box += KitPoint(B, L, U, V, H1);
        }
    }
    return Box;
}

bool LetOverlaps(const FBox& A, const FBox& B)
{
    constexpr double Hair = 0.01;
    return A.Min.X < B.Max.X - Hair && B.Min.X < A.Max.X - Hair && A.Min.Y < B.Max.Y - Hair && B.Min.Y < A.Max.Y - Hair &&
           A.Min.Z < B.Max.Z - Hair && B.Min.Z < A.Max.Z - Hair;
}

bool LetAnyBlocking(const TArray<FBox>& Blocking, const FBox& Space)
{
    return Blocking.ContainsByPredicate([&](const FBox& Box) { return LetOverlaps(Space, Box); });
}

/** The walking capsule's swept space at a layout point: a box of its width and height standing 1 cm above the finished floor. */
FBox LetCapsuleAt(const FLaneBuilding& B, const LI::InteriorLayout& L, double U, double V)
{
    const FVector Centre = KitPoint(B, L, U, V, 0.0);
    return FBox(Centre + FVector(-LetCapsuleRadius, -LetCapsuleRadius, 1.0), Centre + FVector(LetCapsuleRadius, LetCapsuleRadius, LetCapsuleHeight));
}
} // namespace PWLaneEndTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWLaneEndsRecognitionTest, "PerfectWorld.LaneEnds.Recognition",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWLaneEndsRecognitionTest::RunTest(const FString& Parameters)
{
    using namespace PWLaneEndTest;
    for (const LE::EndHall Hall : LetHalls)
    {
        const FString Name = LetName(Hall);
        const FLaneBuilding B = MakeLaneEndBuilding(Hall);
        LE::EndHall Found = LE::EndHall::CommunityHall;
        TestTrue(*FString::Printf(TEXT("%s: the closing mass is recognised"), *Name), IsLaneEndHall(B, Found) && Found == Hall);

        const auto Refused = [&](const TCHAR* What, const FLaneBuilding& Perturbed)
        {
            LE::EndHall Scratch;
            TestFalse(*FString::Printf(TEXT("%s: %s is refused"), *Name, What), IsLaneEndHall(Perturbed, Scratch));
        };
        FLaneBuilding P = B;
        P.Width += 100.0;
        Refused(TEXT("another width"), P);
        P = B;
        P.Depth += 100.0;
        Refused(TEXT("another depth"), P);
        P = B;
        P.Storeys += 1;
        Refused(TEXT("another storey count"), P);
        P = B;
        P.Side = 1;
        Refused(TEXT("a side frontage"), P);
        P = B;
        P.Detail = ELaneDetail::Medium;
        Refused(TEXT("another detail level"), P);
        P = B;
        P.Frame.Origin.X += 100.0;
        Refused(TEXT("another position along the lane"), P);
        P = B;
        P.Frame.V = -B.Frame.V;
        Refused(TEXT("a reversed depth axis"), P);
        P = B;
        P.StoreyHeightCm = OlderStoreyHeight;
        Refused(TEXT("another storey height"), P);

        // Each end is its own hall: the other end's frame is not this hall.
        const LE::EndHall Other = Hall == LE::EndHall::CommunityHall ? LE::EndHall::MarketHall : LE::EndHall::CommunityHall;
        LE::EndHall OtherFound = Hall;
        TestTrue(*FString::Printf(TEXT("%s: the other end is the other hall"), *Name), IsLaneEndHall(MakeLaneEndBuilding(Other), OtherFound) && OtherFound == Other);

        // The mass's world footprint is the portable one: X 6400..7300 (east) or -3300..-2400 (west), Y -1300..1300.
        const FVector Near = B.Frame.Pt(0.0, 0.0, 0.0);
        const FVector Far = B.Frame.Pt(B.Width, B.Depth, 0.0);
        const LE::WorldPoint A = LE::HallWorldPoint(Hall, 0, 0);
        const LE::WorldPoint Z = LE::HallWorldPoint(Hall, LE::HallWidthCm, LE::HallDepthCm);
        TestTrue(*FString::Printf(TEXT("%s: the frame agrees with the portable world mapping"), *Name),
                 FMath::IsNearlyEqual(Near.X, static_cast<double>(A.X)) && FMath::IsNearlyEqual(Near.Y, static_cast<double>(A.Y)) &&
                     FMath::IsNearlyEqual(Far.X, static_cast<double>(Z.X)) && FMath::IsNearlyEqual(Far.Y, static_cast<double>(Z.Y)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWLaneEndsCollisionTest, "PerfectWorld.LaneEnds.Collision",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWLaneEndsCollisionTest::RunTest(const FString& Parameters)
{
    using namespace PWLaneEndTest;
    for (const LE::EndHall Hall : LetHalls)
    {
        const FString Name = LetName(Hall);
        const FLaneBuilding B = MakeLaneEndBuilding(Hall);
        const LI::InteriorLayout Layout = LE::GenerateEndLayout(Hall);
        if (!Layout.Ok())
        {
            AddError(FString::Printf(TEXT("%s: layout refused (%hs)"), *Name, std::string(LI::RefusalCode(Layout.Status)).c_str()));
            continue;
        }
        TestTrue(*FString::Printf(TEXT("%s: both core validators pass"), *Name),
                 LE::ValidateEndLayout(Hall, Layout) == LI::Refusal::None && LI::ValidateInteriorLayout(Layout) == LI::Refusal::None);

        FLaneScene Scene;
        BuildLaneEndHall(Scene, B, Hall);
        const TArray<FBox>& Blocking = Scene.Blocks.BlockingBoxes();
        const double FloorAbs = GroundZ + Layout.FinishedFloorCm;

        // The exact set of collision boxes: one floor slab, every solid piece, every window pane (walls are World colour, not listed).
        int32 Expected = 1;
        for (const LI::Furniture& P : Layout.Pieces)
        {
            Expected += P.Solid ? 1 : 0;
        }
        for (const LI::Opening& O : Layout.Openings)
        {
            Expected += O.Kind == LI::OpeningKind::Window ? 1 : 0;
        }
        TestEqual(*FString::Printf(TEXT("%s: collision box count (the street face adds none)"), *Name), Blocking.Num(), Expected);
        TestEqual(*FString::Printf(TEXT("%s: no door spec: the archway has no leaf"), *Name), Scene.Doors.Num(), 0);
        TestEqual(*FString::Printf(TEXT("%s: one room light per layout light"), *Name), Scene.RoomLights.Num(), static_cast<int32>(Layout.Lights.size()));
        TestTrue(*FString::Printf(TEXT("%s: the floor slab tops out at the finished floor"), *Name),
                 Blocking.ContainsByPredicate([&](const FBox& Box) { return FMath::IsNearlyEqual(Box.Max.Z, FloorAbs, 0.1) && Box.Min.Z <= GroundZ + 0.1; }));

        for (const LI::Furniture& P : Layout.Pieces)
        {
            if (!P.Solid)
            {
                continue;
            }
            const FBox Want = LetWorldBox(B, Layout, P.Footprint, P.H0, P.H1);
            TestTrue(*FString::Printf(TEXT("%s: collision box for %hs"), *Name, P.Id.c_str()),
                     Blocking.ContainsByPredicate([&](const FBox& Box) { return Box.Min.Equals(Want.Min, 0.6) && Box.Max.Equals(Want.Max, 0.6); }));
        }

        // The archway: clear of collision over its whole clear width and its full 212 cm height.
        const LI::Opening* Entrance = LE::FindEndEntrance(Layout);
        if (Entrance == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no archway"), *Name));
            continue;
        }
        TestFalse(*FString::Printf(TEXT("%s: the archway is clear of collision"), *Name),
                  LetAnyBlocking(Blocking, LetWorldBox(B, Layout, LI::Rect{Entrance->Plan.U0 + 2, Entrance->Plan.U1 - 2, Entrance->Plan.V0, Entrance->Plan.V1}, 2.0,
                                                       LI::DoorClearHeightCm)));

        // Aisles, standing places and seat approaches stay clear of every collision box.
        for (const LI::Zone& Z : Layout.Zones)
        {
            if (Z.Kind == LI::ZoneKind::Route)
            {
                continue;
            }
            TestFalse(*FString::Printf(TEXT("%s: zone %hs is clear of collision"), *Name, Z.Id.c_str()),
                      LetAnyBlocking(Blocking, LetWorldBox(B, Layout, Z.Area, 2.0, LetCapsuleHeight)));
        }

        // Every authored walking route: a capsule can be walked along it from the archway to the anchor, every 10 cm.
        TestTrue(*FString::Printf(TEXT("%s: routes exist"), *Name), Layout.Routes.size() >= 2);
        for (const LI::Route& R : Layout.Routes)
        {
            for (size_t I = 0; I + 1 < R.Points.size(); ++I)
            {
                const LI::RoutePoint P = R.Points[I];
                const LI::RoutePoint Q = R.Points[I + 1];
                const int32 Steps = FMath::Max(1, (FMath::Abs(Q.U - P.U) + FMath::Abs(Q.V - P.V)) / 10);
                for (int32 Step = 0; Step <= Steps; ++Step)
                {
                    const double U = P.U + (Q.U - P.U) * static_cast<double>(Step) / Steps;
                    const double V = P.V + (Q.V - P.V) * static_cast<double>(Step) / Steps;
                    TestFalse(*FString::Printf(TEXT("%s: route %hs is walkable at leg %d step %d"), *Name, R.Id.c_str(), static_cast<int32>(I), Step),
                              LetAnyBlocking(Blocking, LetCapsuleAt(B, Layout, U, V)));
                }
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWLaneEndsSignTest, "PerfectWorld.LaneEnds.PostedHours",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWLaneEndsSignTest::RunTest(const FString& Parameters)
{
    using namespace PWLaneEndTest;
    TestEqual(TEXT("community hall board text"), LaneEndSignText(LE::EndHall::CommunityHall), FString(TEXT("Community hall hours\n08:00-21:00")));
    TestEqual(TEXT("market hall board text"), LaneEndSignText(LE::EndHall::MarketHall), FString(TEXT("Market hall hours\n06:00-13:00 and 16:00-21:00")));
    for (const LE::EndHall Hall : LetHalls)
    {
        const FString Name = LetName(Hall);
        const LE::HallInfo& Info = LE::GetHallInfo(Hall);
        const FVector At = LaneEndSignLocation(Hall);
        const double Yaw = LaneEndSignYaw(Hall);
        // On the lane's axis, on the lane side of the wall (not inside the building), above the 212 cm archway and below the first-floor band.
        TestTrue(*FString::Printf(TEXT("%s: the board is on the lane axis"), *Name), FMath::Abs(At.Y) < 1.0);
        TestTrue(*FString::Printf(TEXT("%s: the board stands proud of the lane-facing wall"), *Name),
                 (At.X - Info.LaneFaceXCm) * -Info.FaceXSign > 5.0 && (At.X - Info.LaneFaceXCm) * -Info.FaceXSign < 40.0);
        TestTrue(*FString::Printf(TEXT("%s: the board is over the archway and below the first floor"), *Name),
                 At.Z - GroundZ > GroundFloorZ + Info.EntranceHeightCm && At.Z - GroundZ < FloorZ(1));
        // The text faces the lane: toward -V, which is world +X for the west hall and world -X for the east hall.
        TestTrue(*FString::Printf(TEXT("%s: the board faces the lane"), *Name),
                 FMath::IsNearlyEqual(FMath::Abs(Yaw), Info.East ? 180.0 : 0.0, 0.01) || (Info.East && FMath::IsNearlyEqual(Yaw, -180.0, 0.01)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWLaneEndsRowsTest, "PerfectWorld.LaneEnds.RealRows",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWLaneEndsRowsTest::RunTest(const FString& Parameters)
{
    using namespace PWLaneEndTest;
    // The real lane generator: its closing rows build the halls (not a sealed ground floor) and nothing else does.
    FLaneScene Lane;
    BuildLaneRows(Lane);
    const TArray<FBox>& Blocking = Lane.Blocks.BlockingBoxes();
    for (const LE::EndHall Hall : LetHalls)
    {
        const FString Name = LetName(Hall);
        const FLaneBuilding B = MakeLaneEndBuilding(Hall);
        const LI::InteriorLayout Layout = LE::GenerateEndLayout(Hall);
        const FBox Footprint(B.Frame.Pt(0.0, 0.0, -300.0), B.Frame.Pt(B.Width, B.Depth, 100.0));
        const FBox Wide = FBox(FVector(FMath::Min(Footprint.Min.X, Footprint.Max.X), Footprint.Min.Y, Footprint.Min.Z), Footprint.Max);

        int32 Lights = 0;
        for (const FLaneRoomLight& Light : Lane.RoomLights)
        {
            Lights += (Light.Position.X >= Wide.Min.X && Light.Position.X <= Wide.Max.X && Light.Position.Y >= Wide.Min.Y && Light.Position.Y <= Wide.Max.Y) ? 1 : 0;
        }
        TestEqual(*FString::Printf(TEXT("%s: the real rows light the hall"), *Name), Lights, static_cast<int32>(Layout.Lights.size()));
        for (const LI::Furniture& P : Layout.Pieces)
        {
            if (!P.Solid)
            {
                continue;
            }
            const FBox Want = LetWorldBox(B, Layout, P.Footprint, P.H0, P.H1);
            TestTrue(*FString::Printf(TEXT("%s: the real rows build %hs"), *Name, P.Id.c_str()),
                     Blocking.ContainsByPredicate([&](const FBox& Box) { return Box.Min.Equals(Want.Min, 0.6) && Box.Max.Equals(Want.Max, 0.6); }));
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
