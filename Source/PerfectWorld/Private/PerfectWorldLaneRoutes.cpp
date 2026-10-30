#include "PerfectWorldLaneRoutes.h"

#include "PerfectWorldLaneShops.h"

namespace PWLane
{
#if !UE_BUILD_SHIPPING
namespace
{
constexpr double SampleStep = 5.0;
constexpr double CapsuleWidth = 68.0;
constexpr double StepUpHeight = 45.0;   // anything lower than this above the footpath is stepped over
constexpr double BodyHeight = 165.0;

struct FInterval
{
    double Lo;
    double Hi;
};

TArray<FInterval> FreeIntervals(double Lo, double Hi, TArray<FInterval>& Blocks)
{
    Blocks.Sort([](const FInterval& A, const FInterval& B) { return A.Lo < B.Lo; });
    TArray<FInterval> Free;
    double Cursor = Lo;
    for (const FInterval& Block : Blocks)
    {
        if (Block.Lo > Cursor)
        {
            FInterval Gap;
            Gap.Lo = Cursor;
            Gap.Hi = FMath::Min(Block.Lo, Hi);
            Free.Add(Gap);
        }
        Cursor = FMath::Max(Cursor, Block.Hi);
        if (Cursor >= Hi)
        {
            break;
        }
    }
    if (Cursor < Hi)
    {
        FInterval Gap;
        Gap.Lo = Cursor;
        Gap.Hi = Hi;
        Free.Add(Gap);
    }
    return Free;
}

bool InAlley(int32 Side, double X)
{
    const double A = Side < 0 ? AlleyMinusA : AlleyPlusA;
    const double B = Side < 0 ? AlleyMinusB : AlleyPlusB;
    return X >= A - 1.0 && X <= B + 1.0;
}
} // namespace

void ReportFootpathRoutes(const FLaneScene& S)
{
    const double WalkBottom = GroundZ + FootpathHeight + StepUpHeight;
    const double WalkTop = GroundZ + FootpathHeight + BodyHeight;
    const TArray<FBox>& Boxes = S.Blocks.BlockingBoxes();

    double WorstWidth = TNumericLimits<double>::Max();
    double WorstX = 0.0;
    int32 WorstSide = 0;
    int32 Breaks = 0;
    int32 Samples = 0;

    for (int32 Side = -1; Side <= 1; Side += 2)
    {
        TArray<FInterval> Reachable;
        const double Sign = static_cast<double>(Side);
        for (double X = LaneX0 + 1.0; X < LaneX1; X += SampleStep)
        {
            if (InAlley(Side, X))
            {
                Reachable.Reset();
                continue;
            }
            const double Front = S.FrontAbsY(Side, X);
            TArray<FInterval> Blocks;
            for (const FBox& Box : Boxes)
            {
                if (X < Box.Min.X || X > Box.Max.X || Box.Max.Z <= WalkBottom || Box.Min.Z >= WalkTop)
                {
                    continue;
                }
                FInterval Block;
                Block.Lo = FMath::Min(Box.Min.Y * Sign, Box.Max.Y * Sign);
                Block.Hi = FMath::Max(Box.Min.Y * Sign, Box.Max.Y * Sign);
                Blocks.Add(Block);
            }
            const TArray<FInterval> Free = FreeIntervals(RoadHalfWidth, Front, Blocks);

            double Widest = 0.0;
            TArray<FInterval> Now;
            for (const FInterval& Interval : Free)
            {
                Widest = FMath::Max(Widest, Interval.Hi - Interval.Lo);
                if (Interval.Hi - Interval.Lo >= MinFootpathRoute)
                {
                    Now.Add(Interval);
                }
            }
            ++Samples;
            if (Widest < WorstWidth)
            {
                WorstWidth = Widest;
                WorstX = X;
                WorstSide = Side;
            }

            TArray<FInterval> Next;
            for (const FInterval& Interval : Now)
            {
                bool bConnected = Reachable.IsEmpty();
                for (const FInterval& Before : Reachable)
                {
                    if (FMath::Min(Interval.Hi, Before.Hi) - FMath::Max(Interval.Lo, Before.Lo) >= CapsuleWidth)
                    {
                        bConnected = true;
                        break;
                    }
                }
                if (bConnected)
                {
                    Next.Add(Interval);
                }
            }
            if (Next.IsEmpty())
            {
                ++Breaks; // no route of 120 cm that continues from the previous step
                Next = Now;
            }
            Reachable = Next;
        }
    }

    if (Breaks > 0 || WorstWidth < MinFootpathRoute)
    {
        UE_LOG(LogTemp, Warning, TEXT("Lane footpath route: %d break(s); narrowest clear width %.0f cm at X %.0f on side %d (need %.0f)."), Breaks,
               WorstWidth, WorstX, WorstSide, MinFootpathRoute);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Lane footpath route: continuous; narrowest clear width %.0f cm at X %.0f on side %d (%d samples)."), WorstWidth,
               WorstX, WorstSide, Samples);
    }
}
#endif
} // namespace PWLane
