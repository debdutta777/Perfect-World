// R7-03 school campus walking check (PerfectWorldSchoolCampus.h): a flood fill with the 34 cm player capsule over the built campus, from
// the road outside the gate to the classroom, the office, the playground and every clear route the neighbourhood plan draws.
// Run in the editor under "PerfectWorld.SchoolCampus" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldSchoolCampus.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "PerfectWorldSchoolCampusParts.h"

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
            Mark(Part.Box.Min.X - ScTCapsuleRadius, Part.Box.Min.Y - ScTCapsuleRadius, Part.Box.Max.X + ScTCapsuleRadius, Part.Box.Max.Y + ScTCapsuleRadius);
        }
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

#endif // WITH_DEV_AUTOMATION_TESTS
