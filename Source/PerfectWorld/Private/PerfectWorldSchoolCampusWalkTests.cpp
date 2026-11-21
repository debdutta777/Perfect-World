// R7-03 school campus walking check (PerfectWorldSchoolCampus.h): a flood fill with the 34 cm player capsule over the built campus, from
// the road outside the gate to the classroom, the office, the playground and every clear route the neighbourhood plan draws.
// Run in the editor under "PerfectWorld.SchoolCampus" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldSchoolCampus.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "AmbientCrowd.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "PerfectWorldAwakeningFloor.h"
#include "PerfectWorldSchoolCampusParts.h"
#include "PerfectWorldSchoolTestSupport.h"

namespace
{
using namespace PWSchool;

constexpr double ScTCapsuleRadius = 34.0;
constexpr double ScTStep = 45.0;
constexpr double ScTBody = 165.0;

bool ScWBuild(FScBuilt& Built, FScCampus& Campus)
{
    Campus = ScReadCampus();
    return Campus.bValid && ScBuildCampus(Built, Campus);
}

/** A flood-fill grid over the campus for a standing player: 10 cm cells, every blocking part inflated by the capsule radius. */
struct FScTGrid
{
    static constexpr double Cell = 10.0;
    double X0 = 4900.0;
    double Y0 = 4600.0;
    int32 Nx = 730;
    int32 Ny = 700;
    TBitArray<> Blocked;
    TBitArray<> Seen;

    explicit FScTGrid(const FScBuilt& Built)
    {
        Blocked.Init(false, Nx * Ny);
        Seen.Init(false, Nx * Ny);
        const double Floor = ScGroundZ;
        for (const FScPart& Part : Built.Parts)
        {
            // Only solids the standing body meets: above the 45 cm step and starting below the head.
            if (!ScBlocksPlayer(Part.Colour) || Part.Box.Max.Z <= Floor + ScTStep || Part.Box.Min.Z >= Floor + ScTBody)
            {
                continue;
            }
            AddSolid(Part.Box);
        }
    }

    void AddSolid(const FBox& Box)
    {
        if (!Box.IsValid || Box.Max.Z <= ScGroundZ + ScTStep || Box.Min.Z >= ScGroundZ + ScTBody) return;
        Mark(Box.Min.X - ScTCapsuleRadius, Box.Min.Y - ScTCapsuleRadius,
             Box.Max.X + ScTCapsuleRadius, Box.Max.Y + ScTCapsuleRadius);
    }

    // Read actual spawned components, including the Awakening terminal, movers,
    // pads and any later campus prop. An ISM's aggregate bounds are not a solid.
    int32 AddSpawned(UWorld& World)
    {
        int32 Count = 0;
        for (TActorIterator<AActor> It(&World); It; ++It)
        {
            TArray<UPrimitiveComponent*> Components;
            It->GetComponents(Components);
            for (const auto* Component : Components)
            {
                if (!Component->IsQueryCollisionEnabled() || Component->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block) continue;
                if (const auto* Instances = Cast<UInstancedStaticMeshComponent>(Component))
                {
                    const UStaticMesh* Mesh = Instances->GetStaticMesh();
                    if (Mesh == nullptr) continue;
                    for (int32 I = 0; I < Instances->GetInstanceCount(); ++I)
                    {
                        FTransform Transform;
                        if (Instances->GetInstanceTransform(I, Transform, true))
                        {
                            AddSolid(Mesh->GetBoundingBox().TransformBy(Transform));
                            ++Count;
                        }
                    }
                }
                else
                {
                    AddSolid(Component->CalcBounds(Component->GetComponentTransform()).GetBox());
                    ++Count;
                }
            }
        }
        return Count;
    }

    int32 CellX(double X) const { return FMath::FloorToInt((X - X0) / Cell); }
    int32 CellY(double Y) const { return FMath::FloorToInt((Y - Y0) / Cell); }
    bool In(int32 Ix, int32 Iy) const { return Ix >= 0 && Iy >= 0 && Ix < Nx && Iy < Ny; }
    int32 Index(int32 Ix, int32 Iy) const { return Iy * Nx + Ix; }

    void Mark(double MinX, double MinY, double MaxX, double MaxY)
    {
        for (int32 Iy = FMath::Max(0, CellY(MinY)); Iy <= FMath::Min(Ny - 1, CellY(MaxY)); ++Iy)
        {
            for (int32 Ix = FMath::Max(0, CellX(MinX)); Ix <= FMath::Min(Nx - 1, CellX(MaxX)); ++Ix)
            {
                const double Cx = X0 + (Ix + 0.5) * Cell;
                const double Cy = Y0 + (Iy + 0.5) * Cell;
                if (Cx > MinX && Cx < MaxX && Cy > MinY && Cy < MaxY)
                {
                    Blocked[Index(Ix, Iy)] = true;
                }
            }
        }
    }

    bool IsFree(double X, double Y) const
    {
        const int32 Ix = CellX(X);
        const int32 Iy = CellY(Y);
        return In(Ix, Iy) && !Blocked[Index(Ix, Iy)];
    }

    /** Flood fill (4-connected) from a free point; later Reaches() calls read the result. False when the start is blocked. */
    bool FloodFrom(double X, double Y)
    {
        if (!IsFree(X, Y))
        {
            return false;
        }
        TArray<FIntPoint> Queue;
        Queue.Add(FIntPoint(CellX(X), CellY(Y)));
        Seen[Index(Queue[0].X, Queue[0].Y)] = true;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const FIntPoint At = Queue[Head];
            const FIntPoint Next[4] = {FIntPoint(At.X + 1, At.Y), FIntPoint(At.X - 1, At.Y), FIntPoint(At.X, At.Y + 1), FIntPoint(At.X, At.Y - 1)};
            for (const FIntPoint& N : Next)
            {
                if (In(N.X, N.Y) && !Blocked[Index(N.X, N.Y)] && !Seen[Index(N.X, N.Y)])
                {
                    Seen[Index(N.X, N.Y)] = true;
                    Queue.Add(N);
                }
            }
        }
        return true;
    }

    bool Reaches(double X, double Y) const
    {
        const int32 Ix = CellX(X);
        const int32 Iy = CellY(Y);
        return In(Ix, Iy) && Seen[Index(Ix, Iy)];
    }

    /** Longest run of free cells along X at this Y between two X values, in centimetres. */
    double FreeRunAlongX(double Y, double XA, double XB) const
    {
        int32 Best = 0;
        int32 Run = 0;
        for (int32 Ix = CellX(XA); Ix <= CellX(XB); ++Ix)
        {
            Run = (In(Ix, CellY(Y)) && !Blocked[Index(Ix, CellY(Y))]) ? Run + 1 : 0;
            Best = FMath::Max(Best, Run);
        }
        return Best * Cell;
    }
};

/** Walks a polyline in 5 cm steps and returns the first blocked point, or none. */
bool ScTPolylineClear(const FScTGrid& Grid, const TArray<FVector>& Points, FVector& OutBlockedAt)
{
    for (int32 I = 0; I + 1 < Points.Num(); ++I)
    {
        const double Length = FVector::Dist2D(Points[I], Points[I + 1]);
        const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / 5.0));
        for (int32 S = 0; S <= Steps; ++S)
        {
            const FVector At = FMath::Lerp(Points[I], Points[I + 1], static_cast<double>(S) / Steps);
            if (!Grid.IsFree(At.X, At.Y))
            {
                OutBlockedAt = At;
                return false;
            }
        }
    }
    return true;
}

/** The plan's own route (node to node) as world points. */
bool ScTRoutePoints(const PerfectWorld::NeighbourhoodPlan& Plan, std::string_view From, std::string_view To, TArray<FVector>& OutPoints)
{
    const PerfectWorld::NeighbourhoodRoute Route = PerfectWorld::RouteNeighbourhood(Plan, From, To);
    if (!Route.Found)
    {
        return false;
    }
    OutPoints.Reset();
    for (size_t Leg = 0; Leg < Route.LinkIds.size(); ++Leg)
    {
        const std::string& NodeFrom = Route.NodeIds[Leg];
        for (const PerfectWorld::NeighbourhoodLink& Link : Plan.Links)
        {
            if (Link.Id != Route.LinkIds[Leg])
            {
                continue;
            }
            TArray<FVector> Leg3;
            for (const PerfectWorld::NeighbourhoodPoint& P : Link.Points)
            {
                Leg3.Add(FVector(P.X, P.Y, P.Z));
            }
            if (Link.From != NodeFrom)
            {
                Algo::Reverse(Leg3);
            }
            OutPoints.Append(Leg3);
        }
    }
    return !OutPoints.IsEmpty();
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolCampusReachabilityTest, "PerfectWorld.SchoolCampus.Reachability",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolCampusReachabilityTest::RunTest(const FString& Parameters)
{
    FScBuilt B;
    FScCampus C;
    if (!TestTrue(TEXT("the campus builds"), ScWBuild(B, C)))
    {
        return false;
    }
    FScTGrid Grid(B);
    // The player starts on the para road's north footpath, outside the gate, and walks in.
    const FVector Outside(C.GateX, C.Site.Y0 - 100.0, ScGroundZ);
    if (!TestTrue(TEXT("the point outside the gate is free"), Grid.FloodFrom(Outside.X, Outside.Y)))
    {
        return false;
    }
    TestTrue(TEXT("the 400 cm gate leaves at least 300 cm for the 68 cm capsule"), Grid.FreeRunAlongX(C.Site.Y0 + 15.0, C.GateX - 250.0, C.GateX + 250.0) >= 300.0);
    TestTrue(TEXT("the office doorway admits the capsule"), Grid.FreeRunAlongX(C.Teaching.Y0 + 15.0, C.OfficeDoorX - 90.0, C.OfficeDoorX + 90.0) >= 50.0);
    TestTrue(TEXT("the classroom doorway admits the capsule"), Grid.FreeRunAlongX(C.Teaching.Y0 + 15.0, C.ClassroomDoorX - 90.0, C.ClassroomDoorX + 90.0) >= 50.0);

    const struct { const TCHAR* Name; FVector At; } Targets[] = {
        {TEXT("inside the gate"), FVector(C.GateX, C.Site.Y0 + 150.0, ScGroundZ)},
        {TEXT("the register's stand point"), B.Points.SignInStand},
        {TEXT("the timetable board's stand point"), B.Points.BoardStand},
        {TEXT("the forecourt assembly area"), FVector(C.Forecourt.CentreX() + 500.0, C.Forecourt.CentreY(), ScGroundZ)},
        {TEXT("beside the flag post"), B.Points.FlagPost + FVector(-120.0, 0.0, 0.0)},
        {TEXT("the verandah in front of the office"), FVector(C.OfficeDoorX, C.Teaching.Y0 - 150.0, ScGroundZ)},
        {TEXT("the office, inside"), B.Points.OfficeInside},
        {TEXT("the office, behind the counter"), FVector(C.OfficeDoorX, C.Teaching.Y0 + 560.0, ScGroundZ)},
        {TEXT("the classroom door"), B.Points.ClassroomDoor + FVector(0.0, -60.0, 0.0)},
        {TEXT("the classroom's centre aisle"), B.Points.ClassroomAisle},
        {TEXT("in front of the teacher's table"), B.Points.ClassroomFront},
        {TEXT("the classroom's west margin"), FVector(7110.0, 6900.0, ScGroundZ)},
        {TEXT("the classroom's east margin"), FVector(7890.0, 6900.0, ScGroundZ)},
        {TEXT("the playground centre"), B.Points.Playground},
        {TEXT("the goalmouth, south"), FVector(C.Playground.CentreX(), C.Playground.Y0 + 450.0, ScGroundZ)},
        {TEXT("the goalmouth, north"), FVector(C.Playground.CentreX(), C.Playground.Y1 - 450.0, ScGroundZ)},
        {TEXT("the water tap"), B.Points.WaterTap},
    };
    for (const auto& Target : Targets)
    {
        TestTrue(*FString::Printf(TEXT("%s is free standing room"), Target.Name), Grid.IsFree(Target.At.X, Target.At.Y));
        TestTrue(*FString::Printf(TEXT("%s is reachable on foot from outside the gate"), Target.Name), Grid.Reaches(Target.At.X, Target.At.Y));
    }
    // What is solid stays unreachable: the block's interior behind the two rooms, and the closed strip behind the north wall.
    TestFalse(TEXT("the block's unmodelled interior cannot be entered"), Grid.Reaches(7500.0, 9000.0));
    TestFalse(TEXT("the stair tower cannot be entered"), Grid.Reaches(5750.0, 6700.0));
    TestFalse(TEXT("nor the inside of the guard booth"), Grid.Reaches(C.GateX + 0.5 * C.GateWidth + 250.0, C.Site.Y0 + 180.0));

    // The plan's own walking routes are clear for the capsule: gate to classroom, office and the playground.
    const PerfectWorld::NeighbourhoodPlan Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    for (const char* To : {"walk.nabapara.school_classroom", "walk.nabapara.school_office", "walk.nabapara.school_playground"})
    {
        TArray<FVector> Points;
        if (!TestTrue(*FString::Printf(TEXT("the plan routes the gate to %hs"), To), ScTRoutePoints(Plan, PerfectWorld::NeighbourhoodSchoolGateNodeId, To, Points)))
        {
            continue;
        }
        FVector BlockedAt;
        const bool bClear = ScTPolylineClear(Grid, Points, BlockedAt);
        TestTrue(*FString::Printf(TEXT("the plan's route to %hs is clear for a standing player (blocked at %.0f, %.0f)"), To, BlockedAt.X, BlockedAt.Y), bClear);
    }
    // Through the doorway and along the aisle to the board.
    TArray<FVector> Aisle;
    Aisle.Add(FVector(C.ClassroomDoorX, C.Teaching.Y0 - 30.0, ScGroundZ));
    Aisle.Add(B.Points.ClassroomAisle);
    Aisle.Add(B.Points.ClassroomFront);
    FVector Where;
    TestTrue(TEXT("the straight line from the classroom doorway along the aisle to the board is clear"), ScTPolylineClear(Grid, Aisle, Where));
    TArray<FVector> OfficeWalk;
    OfficeWalk.Add(FVector(C.OfficeDoorX, C.Teaching.Y0 - 30.0, ScGroundZ));
    OfficeWalk.Add(B.Points.OfficeInside);
    OfficeWalk.Add(FVector(C.OfficeDoorX, C.Teaching.Y0 + 560.0, ScGroundZ));
    TestTrue(TEXT("through the office's counter opening to the staff side"), ScTPolylineClear(Grid, OfficeWalk, Where));
    return true;
}

// S06: school_lesson_round9's route to the classroom seat. The first version ran a straight diagonal from the forecourt to the doorway and
// the pawn stopped at (7202, 6214): the verandah pillar at (7250, 6175) (40 cm square on a 48 cm plinth) is inside that line once inflated by the
// 34 cm capsule. The doorway, the verandah walk and the aisle all admit the capsule; only that route was wrong.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolSeatRouteTest, "PerfectWorld.SchoolCampus.SeatRoute",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FScSchoolSeatRouteTest::RunTest(const FString& Parameters)
{
    FScBuilt B;
    FScCampus C;
    if (!TestTrue(TEXT("the campus builds"), ScWBuild(B, C)))
    {
        return false;
    }
    FScTGrid Grid(B);
    PWSchoolTest::FScTestWorld Holder;
    if (!TestNotNull(TEXT("a world for the production campus spawns"), Holder.World)) return false;
    PWLane::FLaneScene Scene;
    TestTrue(TEXT("the production campus spawned"), SpawnSchoolCampus(*Holder.World, Scene).bBuilt);
    PWAwakening::FAwFloorConfig Config;
    Config.bUseHud = false;
    TestTrue(TEXT("the production Awakening floor and terminal spawned"), PWAwakening::SpawnAwakeningFloor(*Holder.World, Config).bBuilt);
    TestTrue(TEXT("spawned pawn blockers are included individually"), Grid.AddSpawned(*Holder.World) > 0);
    // P315: the old test started at waypoint 1 and read only campus block parts.
    // Include the actual approach from the register and every crowd capsule at 09:40.
    TArray<FVector> Route{B.Points.SignInStand};
    Route.Append(ScClassroomSeatRoute(C, B.Points));
    PerfectWorld::ContentDb Db;
    if (!PWSchoolTest::ScLoadSchoolRecords(*this, Db)) return false;
    const auto* Record = Db.Find("calendar.nabapara_2042_43");
    if (!TestNotNull(TEXT("the authored school calendar"), Record)) return false;
    std::vector<std::string> Problems;
    const auto Calendar = PerfectWorld::ParseSchoolCalendarRecord(*Record, Problems);
    if (!TestTrue(TEXT("the calendar parses"), Problems.empty())) return false;
    PerfectWorld::AmbientCrowd Crowd(0x50575F4C414E45ull, std::string(PerfectWorld::NeighbourhoodId),
        PerfectWorld::BuildNeighbourhoodPlan(0), Calendar);
    if (!TestTrue(TEXT("the crowd's public approaches are valid"), Crowd.Problems().empty())) return false;
    // All students' bay approaches must also admit a 34 cm capsule through the
    // real spawned scenery, including the terminal and the verandah pillars.
    for (const auto& Slot : Crowd.RouteSlots())
    {
        if (Slot.Cohort != PerfectWorld::CrowdCohort::Student) continue;
        TArray<FVector> Points;
        for (const auto& Segment : Slot.Segments)
        {
            if (Points.IsEmpty()) Points.Add(FVector(Segment.From.X, Segment.From.Y, Segment.From.Z));
            Points.Add(FVector(Segment.To.X, Segment.To.Y, Segment.To.Z));
        }
        FVector At = FVector::ZeroVector;
        const bool bApproachClear = ScTPolylineClear(Grid, Points, At);
        TestTrue(*FString::Printf(TEXT("pupil slot %u has a clear campus approach (%.0f, %.0f)"), Slot.Slot, At.X, At.Y),
            bApproachClear);
    }
    const auto People = Crowd.CrowdAt(PerfectWorld::CalendarDayStartInstant(0) + 580, {6500, 5850, -185}, 10000);
    int32 Pupils = 0;
    for (const auto& Person : People.People)
    {
        if (Person.Cohort != PerfectWorld::CrowdCohort::Student) continue;
        ++Pupils;
        const FVector Feet(Person.Position.X, Person.Position.Y, Person.Position.Z);
        Grid.AddSolid(FBox(Feet - FVector(34, 34, 0), Feet + FVector(34, 34, 165)));
    }
    TestEqual(TEXT("every pupil in the 09:40 campus sample contributes a blocker"), Pupils, 27);
    FVector BlockedAt = FVector::ZeroVector;
    const bool bRouteClear = ScTPolylineClear(Grid, Route, BlockedAt);
    TestTrue(*FString::Printf(TEXT("the seat route is clear for the 34 cm capsule (blocked at %.0f, %.0f)"), BlockedAt.X, BlockedAt.Y), bRouteClear);
    // The old straight diagonal is not a route.
    TArray<FVector> Old;
    Old.Add(FVector(C.OfficeDoorX, C.Forecourt.Y1 - 200.0, ScGroundZ));
    Old.Add(FVector(C.ClassroomDoorX, C.Teaching.Y0 - 150.0, ScGroundZ));
    FVector OldBlocked = FVector::ZeroVector;
    const bool bOldClear = ScTPolylineClear(Grid, Old, OldBlocked);
    TestFalse(TEXT("the straight diagonal to the doorway runs into a verandah pillar"), bOldClear);
    TestTrue(*FString::Printf(TEXT("it stops near the pillar at X 7250 (blocked at %.0f, %.0f)"), OldBlocked.X, OldBlocked.Y), FMath::Abs(OldBlocked.X - 7250.0) < 120.0);
    // The end of the route is within reach of the seat's interaction point (the aisle at 90 cm up).
    const FVector End = Route.Last();
    const FVector Seat = B.Points.ClassroomAisle + FVector(0.0, 0.0, 90.0);
    TestTrue(*FString::Printf(TEXT("the seat is %.0f cm from where the route ends, inside the 200 cm reach"), FVector::Dist2D(End, Seat)), FVector::Dist2D(End, Seat) <= 200.0);
    TestTrue(TEXT("the route's end is free standing floor"), Grid.IsFree(End.X, End.Y));
    FScTGrid Former(B);
    for (const double Y : {5773.0, 5863.0})
        Former.AddSolid(FBox(FVector(6466, Y - 34, ScGroundZ), FVector(6534, Y + 34, ScGroundZ + 165)));
    TestFalse(TEXT("the packaged waiting-pupil pair obstructs the unchanged route"), ScTPolylineClear(Former, Route, BlockedAt));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
