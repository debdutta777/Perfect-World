// R7-03 school campus (PerfectWorldSchoolCampus.h): the campus read from the neighbourhood plan, its components at the plan's
// coordinates, a walking check with the 34 cm player capsule (gate to classroom, office, playground and every clear route the plan
// draws), and the real spawn in a throw-away world.
// Run in the editor under "PerfectWorld.SchoolCampus" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldSchoolCampus.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldSchoolCampusParts.h"
#include "PerfectWorldSchoolGate.h"
#include "PerfectWorldSchoolTestSupport.h"
#include "PerfectWorldShopDisplayActor.h"

#include <algorithm>

namespace
{
using namespace PWSchool;

/** True when two boxes overlap by more than a hair (touching faces do not count). */
bool ScTOverlap(const FBox& A, const FBox& B)
{
    return A.Min.X < B.Max.X - 0.01 && B.Min.X < A.Max.X - 0.01 && A.Min.Y < B.Max.Y - 0.01 && B.Min.Y < A.Max.Y - 0.01 &&
           A.Min.Z < B.Max.Z - 0.01 && B.Min.Z < A.Max.Z - 0.01;
}

bool ScTBuild(FScBuilt& Built, FScCampus& Campus)
{
    Campus = ScReadCampus();
    return Campus.bValid && ScBuildCampus(Built, Campus);
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolCampusPlanTest, "PerfectWorld.SchoolCampus.PlanExtents",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolCampusPlanTest::RunTest(const FString& Parameters)
{
    const PerfectWorld::NeighbourhoodPlan Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    const FScCampus Campus = ScReadCampus(Plan);
    TestTrue(TEXT("the plan's school is valid"), Campus.bValid);
    TestEqual(TEXT("site X0"), Campus.Site.X0, 5000.0);
    TestEqual(TEXT("site Y0"), Campus.Site.Y0, 4850.0);
    TestEqual(TEXT("site X1"), Campus.Site.X1, 12000.0);
    TestEqual(TEXT("site Y1"), Campus.Site.Y1, 11500.0);
    TestEqual(TEXT("forecourt width"), Campus.Forecourt.Width(), 6000.0);
    TestEqual(TEXT("forecourt depth"), Campus.Forecourt.Depth(), 1200.0);
    TestEqual(TEXT("teaching block width"), Campus.Teaching.Width(), 2500.0);
    TestEqual(TEXT("teaching block depth"), Campus.Teaching.Depth(), 4500.0);
    TestEqual(TEXT("playground width"), Campus.Playground.Width(), 3400.0);
    TestEqual(TEXT("playground depth"), Campus.Playground.Depth(), 4500.0);
    TestEqual(TEXT("gate X"), Campus.GateX, 6500.0);
    TestEqual(TEXT("gate opening"), Campus.GateWidth, 400.0);
    TestEqual(TEXT("office door X"), Campus.OfficeDoorX, 6500.0);
    TestEqual(TEXT("classroom door X"), Campus.ClassroomDoorX, 7500.0);
    const PerfectWorld::NeighbourhoodFeature* Teaching = PerfectWorld::FindNeighbourhoodFeature(Plan, "building.nabapara.school_teaching");
    TestTrue(TEXT("the teaching block is three storeys in the plan"), Teaching != nullptr && Teaching->Storeys == 3);

    // A plan without the school is refused, not defaulted.
    PerfectWorld::NeighbourhoodPlan Broken = Plan;
    Broken.Features.erase(std::remove_if(Broken.Features.begin(), Broken.Features.end(),
                                         [](const PerfectWorld::NeighbourhoodFeature& F) { return F.Id == "school.nabapara.playground"; }),
                          Broken.Features.end());
    TestFalse(TEXT("a plan missing the playground is not a valid campus"), ScReadCampus(Broken).bValid);
    FScBuilt Refused;
    TestFalse(TEXT("an invalid campus builds nothing"), ScBuildCampus(Refused, ScReadCampus(Broken)));
    TestEqual(TEXT("and leaves no parts"), Refused.Parts.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolCampusComponentsTest, "PerfectWorld.SchoolCampus.Components",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolCampusComponentsTest::RunTest(const FString& Parameters)
{
    FScBuilt B;
    FScCampus C;
    if (!TestTrue(TEXT("the campus builds from the plan"), ScBuildCampus(B, ScReadCampus()) && (C = ScReadCampus()).bValid))
    {
        return false;
    }
    const double G = ScGroundZ;
    const auto Need = [&](const TCHAR* Id) -> const FScPart*
    {
        const FScPart* Part = B.FindPart(Id);
        TestNotNull(*FString::Printf(TEXT("part %s exists"), Id), Part);
        return Part;
    };

    // Boundary: five runs exactly on the site rectangle, south run cut at the gate.
    const FScPart* SouthWest = Need(TEXT("wall.south_west"));
    const FScPart* SouthEast = Need(TEXT("wall.south_east"));
    const FScPart* West = Need(TEXT("wall.west"));
    const FScPart* East = Need(TEXT("wall.east"));
    const FScPart* North = Need(TEXT("wall.north"));
    if (SouthWest == nullptr || SouthEast == nullptr || West == nullptr || East == nullptr || North == nullptr)
    {
        return false;
    }
    TestEqual(TEXT("south-west wall starts at the site's west edge"), SouthWest->Box.Min.X, C.Site.X0);
    TestEqual(TEXT("south walls stand on the site's south boundary"), SouthWest->Box.Min.Y, C.Site.Y0);
    TestEqual(TEXT("west wall reaches the north boundary"), West->Box.Max.Y, C.Site.Y1);
    TestEqual(TEXT("east wall stands on the east boundary"), East->Box.Max.X, C.Site.X1);
    TestEqual(TEXT("north wall stands on the north boundary"), North->Box.Max.Y, C.Site.Y1);
    TestEqual(TEXT("wall height"), SouthWest->Box.Max.Z - SouthWest->Box.Min.Z, ScBoundaryWallHeight);
    // The gap in the south boundary is the gate: pillars (100) on either side of the 400 cm opening.
    TestEqual(TEXT("the south-west run ends at the west pillar"), SouthWest->Box.Max.X, C.GateX - 0.5 * C.GateWidth - 100.0);
    TestEqual(TEXT("the south-east run starts at the east pillar"), SouthEast->Box.Min.X, C.GateX + 0.5 * C.GateWidth + 100.0);

    // Gate: pillars, lintel 285 cm up, a signboard between the pillars, English and Bengali.
    const FScPart* PillarW = Need(TEXT("gate.pillar_west"));
    const FScPart* PillarE = Need(TEXT("gate.pillar_east"));
    const FScPart* Lintel = Need(TEXT("gate.lintel"));
    const FScPart* Board = Need(TEXT("gate.board"));
    if (PillarW == nullptr || PillarE == nullptr || Lintel == nullptr || Board == nullptr)
    {
        return false;
    }
    TestEqual(TEXT("west pillar's inner face closes the opening's west edge"), PillarW->Box.Max.X, C.GateX - 0.5 * C.GateWidth);
    TestEqual(TEXT("east pillar's inner face closes the opening's east edge"), PillarE->Box.Min.X, C.GateX + 0.5 * C.GateWidth);
    TestEqual(TEXT("clear height under the lintel"), Lintel->Box.Min.Z - G, 285.0);
    TestTrue(TEXT("the signboard spans between the pillars, above the opening"),
             Board->Box.Min.X >= PillarW->Box.Min.X && Board->Box.Max.X <= PillarE->Box.Max.X && Board->Box.Min.Z >= Lintel->Box.Max.Z - 0.01);
    bool bEnglish = false;
    for (const FScTextSign& Sign : B.Texts)
    {
        bEnglish |= Sign.Id == FName(TEXT("gate.sign_english")) && Sign.Text == TEXT("Nabapara Vidyalaya") &&
                    Sign.Location.X == C.GateX && Sign.Location.Z > Board->Box.Min.Z && Sign.Location.Z < Board->Box.Max.Z;
    }
    TestTrue(TEXT("the English line reads 'Nabapara Vidyalaya', centred on the board"), bEnglish);
    const TArray<const FScPart*> Runs = B.FindParts(TEXT("gate.sign_bengali.run"));
    TestTrue(TEXT("the Bengali line is built of block runs"), Runs.Num() >= 20);
    const FBox Bengali = B.UnionOf(TEXT("gate.sign_bengali.run"));
    TestTrue(TEXT("the Bengali line sits on the board, below the English"),
             Bengali.IsValid && Bengali.Min.X > Board->Box.Min.X && Bengali.Max.X < Board->Box.Max.X && Bengali.Min.Z > Board->Box.Min.Z && Bengali.Max.Z < Board->Box.Max.Z - 40.0);
    TestTrue(TEXT("the Bengali line is centred on the gate"), FMath::Abs(0.5 * (Bengali.Min.X + Bengali.Max.X) - C.GateX) < 20.0);
    TestTrue(TEXT("it stands in front of the board (toward the road)"), Bengali.Max.Y <= Board->Box.Min.Y + 0.01);

    // Guard booth, register and the timetable board.
    TestNotNull(TEXT("guard booth"), B.FindPart(TEXT("booth.body")));
    const FScPart* Stand = Need(TEXT("register.collision"));
    if (Stand != nullptr)
    {
        TestTrue(TEXT("the register's interaction point is on its stand"), B.Points.SignInPoint.X > Stand->Box.Min.X && B.Points.SignInPoint.X < Stand->Box.Max.X &&
                 B.Points.SignInPoint.Y > Stand->Box.Min.Y && B.Points.SignInPoint.Y < Stand->Box.Max.Y && B.Points.SignInPoint.Z <= Stand->Box.Max.Z + 3.0);
        TestTrue(TEXT("the register stands off the 400 cm opening"), Stand->Box.Min.X >= C.GateX + 0.5 * C.GateWidth - 0.01);
    }
    const FScPart* Plate = Need(TEXT("board.plate"));
    if (Plate != nullptr)
    {
        TestTrue(TEXT("the timetable board hangs on the wall's road face, beside the gate"), Plate->Box.Max.X < C.GateX - 0.5 * C.GateWidth - 100.0 && FMath::IsNearlyEqual(Plate->Box.Max.Y, C.Site.Y0));
        TestTrue(TEXT("its interaction point is in front of the plate"), B.Points.BoardPoint.Y < Plate->Box.Min.Y);
    }

    // Forecourt, assembly, flag post.
    const FScPart* Paving = Need(TEXT("paving.forecourt"));
    if (Paving != nullptr)
    {
        TestEqual(TEXT("forecourt paving X0"), Paving->Box.Min.X, C.Forecourt.X0);
        TestEqual(TEXT("forecourt paving Y0"), Paving->Box.Min.Y, C.Forecourt.Y0);
        TestEqual(TEXT("forecourt paving X1"), Paving->Box.Max.X, C.Forecourt.X1);
        TestEqual(TEXT("forecourt paving Y1"), Paving->Box.Max.Y, C.Forecourt.Y1);
        TestEqual(TEXT("paving is level with the footpath datum"), Paving->Box.Max.Z, G);
    }
    TestTrue(TEXT("an assembly area is marked on the forecourt"), B.FindParts(TEXT("assembly.column")).Num() == 5 && B.FindPart(TEXT("assembly.dais")) != nullptr);
    const FScPart* Pole = Need(TEXT("flag.pole"));
    if (Pole != nullptr)
    {
        TestTrue(TEXT("the flag post stands on the forecourt"), C.Forecourt.ContainsXY(Pole->Box.GetCenter().X, Pole->Box.GetCenter().Y));
        TestTrue(TEXT("and is taller than a verandah deck"), Pole->Box.Max.Z - G > 600.0);
    }
    TestTrue(TEXT("the flag has three bands and a wheel"), B.FindPart(TEXT("flag.saffron")) && B.FindPart(TEXT("flag.white")) && B.FindPart(TEXT("flag.green")) && B.FindPart(TEXT("flag.wheel")));

    // Teaching block: three storeys exactly on the plan's footprint, a verandah on every floor, the doorways at the plan's entrances.
    const FScPart* Mass = Need(TEXT("block.upper_mass"));
    if (Mass != nullptr)
    {
        TestEqual(TEXT("block X0"), Mass->Box.Min.X, C.Teaching.X0);
        TestEqual(TEXT("block Y0"), Mass->Box.Min.Y, C.Teaching.Y0);
        TestEqual(TEXT("block X1"), Mass->Box.Max.X, C.Teaching.X1);
        TestEqual(TEXT("block Y1"), Mass->Box.Max.Y, C.Teaching.Y1);
        TestEqual(TEXT("three storeys of 360 cm"), Mass->Box.Max.Z - G, 3.0 * ScStoreyHeight);
    }
    for (int32 Deck = 1; Deck <= 3; ++Deck)
    {
        const FScPart* Slab = Need(*FString::Printf(TEXT("verandah.deck%d"), Deck));
        if (Slab != nullptr)
        {
            TestEqual(*FString::Printf(TEXT("verandah %d runs the whole front"), Deck), Slab->Box.Max.X - Slab->Box.Min.X, C.Teaching.Width());
            TestEqual(*FString::Printf(TEXT("verandah %d is 350 cm deep, in front of the block"), Deck), Slab->Box.Max.Y - Slab->Box.Min.Y, ScVerandahDepth);
            TestEqual(*FString::Printf(TEXT("verandah %d meets the block's face"), Deck), Slab->Box.Max.Y, C.Teaching.Y0);
        }
    }
    TestEqual(TEXT("five pillars carry the verandah"), B.FindParts(TEXT("verandah.pillar")).Num() - B.FindParts(TEXT("verandah.pillar_base")).Num() - B.FindParts(TEXT("verandah.pillar_end")).Num(), 5);
    TestTrue(TEXT("a stair tower and its sign"), B.FindPart(TEXT("stairs.glass")) != nullptr && B.FindPart(TEXT("stairs.sign_plate")) != nullptr);
    TestTrue(TEXT("classroom doors and windows are drawn on the upper floors"), B.FindPart(TEXT("facade.floor2_door_class_side.leaf")) && B.FindPart(TEXT("facade.floor3_door_class_side.leaf")) &&
             B.FindParts(TEXT("facade.floor2_window")).Num() > 0 && B.FindParts(TEXT("facade.floor3_window")).Num() > 0);
    // Doorways: nothing that blocks the player stands in the 130 cm opening up to the door height; the head is there above it.
    for (const double DoorX : {C.OfficeDoorX, C.ClassroomDoorX})
    {
        const FBox Opening(FVector(DoorX - 65.0, C.Teaching.Y0 + 1.0, G + 1.0), FVector(DoorX + 65.0, C.Teaching.Y0 + ScWall - 1.0, G + ScDoorHeight - 1.0));
        for (const FScPart& Part : B.Parts)
        {
            if (ScBlocksPlayer(Part.Colour) && Part.Box.Max.Z > G + 1.0 && ScTOverlap(Part.Box, Opening))
            {
                AddError(FString::Printf(TEXT("%s blocks the doorway at X %.0f"), *Part.Id.ToString(), DoorX));
            }
        }
    }
    TestTrue(TEXT("office door at the plan's office entrance"), C.OfficeDoorX == 6500.0 && B.FindPart(TEXT("block.south_wall.head0")) != nullptr);
    TestTrue(TEXT("classroom door at the plan's classroom entrance"), B.FindPart(TEXT("block.south_wall.head1")) != nullptr);

    // Classroom: forty seats, a teacher's table, a blackboard and fans.
    TestEqual(TEXT("twenty double desks"), B.Classroom.DeskUnits, 20);
    TestEqual(TEXT("forty seats"), B.Classroom.Seats, 40);
    TestEqual(TEXT("four ceiling fans"), B.Classroom.Fans, 4);
    const FBox Room(FVector(ScRoom::PartyX + ScWall, C.Teaching.Y0 + ScWall, G), FVector(C.Teaching.X1 - ScWall, ScRoom::ClassNorth, ScFloorZ(1)));
    for (const FScPart* Desk : B.FindParts(TEXT("class.desk.")))
    {
        TestTrue(*FString::Printf(TEXT("%s is inside the classroom"), *Desk->Id.ToString()),
                 Desk->Box.Min.X >= Room.Min.X && Desk->Box.Max.X <= Room.Max.X && Desk->Box.Min.Y >= Room.Min.Y && Desk->Box.Max.Y <= Room.Max.Y);
    }
    TestTrue(TEXT("teacher's table, chair and blackboard"), B.FindPart(TEXT("class.table.top")) && B.FindPart(TEXT("class.chair.seat")) && B.FindPart(TEXT("class.blackboard")));
    TestTrue(TEXT("two shadowed lights per room at least"), B.IndoorLights.Num() >= 4);

    // Office: accessible counter (low section at 75 cm), two desks.
    const FScPart* Low = Need(TEXT("office.counter_low"));
    if (Low != nullptr)
    {
        TestTrue(TEXT("the low counter is wheelchair height (75 cm)"), FMath::IsNearlyEqual(Low->Box.Max.Z - G, 76.0, 1.0));
    }
    TestTrue(TEXT("two staff desks"), B.FindPart(TEXT("office.desk_a.top")) && B.FindPart(TEXT("office.desk_b.top")));

    // Playground: goalposts inside the plan's playground, trees, a water tap.
    for (const TCHAR* Id : {TEXT("goal.south.post_0"), TEXT("goal.south.post_1"), TEXT("goal.north.post_0"), TEXT("goal.north.post_1")})
    {
        if (const FScPart* Post = Need(Id))
        {
            TestTrue(*FString::Printf(TEXT("%s stands in the playground"), Id), C.Playground.ContainsXY(Post->Box.GetCenter().X, Post->Box.GetCenter().Y));
        }
    }
    TestTrue(TEXT("a few trees"), B.FindParts(TEXT("tree.")).Num() >= 15);
    TestTrue(TEXT("a water tap"), B.FindPart(TEXT("tap.trough")) != nullptr && B.FindParts(TEXT("tap.spout")).Num() == 3);

    // Nothing that blocks the player leaves the site except the gate pillars' proud faces (and nothing touches the road footpath).
    for (const FScPart& Part : B.Parts)
    {
        if (ScBlocksPlayer(Part.Colour) && Part.Box.Min.Y < C.Site.Y0 - 20.0)
        {
            AddError(FString::Printf(TEXT("%s blocks the player south of the boundary (the road side belongs to the street)"), *Part.Id.ToString()));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolCampusSpawnTest, "PerfectWorld.SchoolCampus.Spawn",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolCampusSpawnTest::RunTest(const FString& Parameters)
{
    PWSchoolTest::FScTestWorld Holder;
    if (!TestNotNull(TEXT("a test world"), Holder.World))
    {
        return false;
    }
    PWLane::FLaneScene Scene;
    const FScSpawnResult Spawned = SpawnSchoolCampus(*Holder.World, Scene);
    if (!TestTrue(TEXT("the campus spawned"), Spawned.bBuilt))
    {
        return false;
    }
    FScBuilt Reference;
    FScCampus C;
    TestTrue(TEXT("the reference build"), ScTBuild(Reference, C));
    TestTrue(TEXT("blocks are placed as instances in a few colour components"), Spawned.Instances > 1000 && Spawned.Components > 10);
    TestTrue(TEXT("the blocks actor carries its tag"), Spawned.BlocksActor != nullptr && Spawned.BlocksActor->ActorHasTag(ScBlocksActorTag));
    int32 Counted = 0;
    if (Spawned.BlocksActor != nullptr)
    {
        TArray<UInstancedStaticMeshComponent*> Comps;
        Spawned.BlocksActor->GetComponents(Comps);
        for (const UInstancedStaticMeshComponent* Comp : Comps)
        {
            Counted += Comp->GetInstanceCount();
        }
    }
    TestEqual(TEXT("every built box is an instance"), Counted, Spawned.Instances);

    int32 Signs = 0;
    bool bEnglish = false;
    for (TActorIterator<APWShopHoursSignActor> It(Holder.World); It; ++It)
    {
        if (It->ActorHasTag(FName(TEXT("school.nabapara.sign"))))
        {
            ++Signs;
            bEnglish |= It->GetPostedText() == TEXT("Nabapara Vidyalaya");
        }
    }
    TestEqual(TEXT("every text sign is an actor"), Signs, Reference.Texts.Num());
    TestTrue(TEXT("the English signboard is among them"), bEnglish);
    TestEqual(TEXT("every light spawned"), Spawned.Lights, Reference.IndoorLights.Num() + Reference.OutdoorLights.Num());

    if (TestNotNull(TEXT("the register"), Spawned.Gate))
    {
        TestTrue(TEXT("the register stands at its point"), Spawned.Gate->GetInteractionPoint().Equals(Reference.Points.SignInPoint, 0.1));
        TestEqual(TEXT("its verb"), Spawned.Gate->GetInteractionVerb(nullptr).ToString(), FString(TEXT("Sign in at the gate")));
        TestEqual(TEXT("priority"), Spawned.Gate->GetInteractionPriority(), 15);
    }
    if (TestNotNull(TEXT("the timetable board"), Spawned.Board))
    {
        TestTrue(TEXT("the board's point"), Spawned.Board->GetInteractionPoint().Equals(Reference.Points.BoardPoint, 0.1));
        TestEqual(TEXT("its verb"), Spawned.Board->GetInteractionVerb(nullptr).ToString(), FString(TEXT("Read the timetable")));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
