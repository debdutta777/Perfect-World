// Shared helpers of the R4-01 kit stall tests (PerfectWorldLaneKitStallsTests.cpp, PerfectWorldLaneKitStallsFaceTests.cpp).
#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Doors.h"
#include "Engine/World.h"
#include "PerfectWorldLaneKitDisplays.h"
#include "PerfectWorldLaneKitStalls.h"
#include "PerfectWorldProprietorBindings.h"

namespace PWKitStallTest
{
using namespace PWLane;

inline constexpr double StCapsuleWidth = 68.0;
inline constexpr double StWalkBand = 185.0; // the walking height band above the finished floor

inline const TCHAR* const StShops[] = {TEXT("building.nabapara.market_stalls_a"), TEXT("building.nabapara.market_stalls_b"),
                                TEXT("building.nabapara.end_shop"), TEXT("building.nabapara.cloth_merchant"),
                                TEXT("building.nabapara.narrow_shop"), TEXT("building.nabapara.tailor_shop"), TEXT("building.nabapara.corner_stalls")};

struct FStWorld
{
    UWorld* World = nullptr;

    FStWorld() { World = UWorld::CreateWorld(EWorldType::Game, false); }
    ~FStWorld()
    {
        if (World != nullptr)
        {
            World->DestroyWorld(false);
        }
    }
    FStWorld(const FStWorld&) = delete;
    FStWorld& operator=(const FStWorld&) = delete;
};

inline bool StBuild(const TCHAR* Id, FLaneBuilding& B, FLaneKitFrontage& Kit)
{
    return MakeProprietorBuilding(Id, B) && IsKitFrontage(B, Kit);
}

inline FBox StBox(const FLaneBuilding& B, const LI::InteriorLayout& L, double U0, double U1, double V0, double V1, double H0, double H1)
{
    FBox Box(ForceInit);
    for (const double U : {U0, U1})
    {
        for (const double V : {V0, V1})
        {
            Box += KitPoint(B, L, U, V, H0);
            Box += KitPoint(B, L, U, V, H1);
        }
    }
    return Box;
}

inline bool StOverlaps(const FBox& A, const FBox& B)
{
    constexpr double Hair = 0.01;
    return A.Min.X < B.Max.X - Hair && B.Min.X < A.Max.X - Hair && A.Min.Y < B.Max.Y - Hair && B.Min.Y < A.Max.Y - Hair &&
           A.Min.Z < B.Max.Z - Hair && B.Min.Z < A.Max.Z - Hair;
}

inline bool StAnyOverlap(const TArray<FBox>& Boxes, const FBox& Space)
{
    return Boxes.ContainsByPredicate([&](const FBox& Box) { return StOverlaps(Space, Box); });
}

inline FString StExpectedHours(const PerfectWorld::DoorDef& Door)
{
    FString Text;
    for (const PerfectWorld::DoorOpeningWindow& Window : Door.OpeningWindows)
    {
        Text += FString::Printf(TEXT("%s%02d:%02d-%02d:%02d"), Text.IsEmpty() ? TEXT("") : TEXT(" and "), Window.OpenMinute / 60,
                                Window.OpenMinute % 60, Window.CloseMinute / 60, Window.CloseMinute % 60);
    }
    return Text;
}

inline bool StHasEveryRequiredPhrase(const FString& Text, const PerfectWorld::DoorDef& Door)
{
    return Text.Contains(StExpectedHours(Door)) && Text.Contains(TEXT("Unstaffed")) &&
           Text.Contains(TEXT("Purchasing unavailable"));
}

/** Nothing in the text may suggest that something can be bought, paid for or rung up. */
inline bool StImpliesCommerce(const FString& Text)
{
    const FString Lower = Text.ToLower();
    for (const TCHAR* Word : {TEXT("buy"), TEXT("pay"), TEXT("till"), TEXT("price"), TEXT("rupee"), TEXT("cart"), TEXT("checkout"), TEXT("for sale")})
    {
        if (Lower.Contains(Word))
        {
            return true;
        }
    }
    return false;
}

struct FStGap
{
    double Lo = 0.0;
    double Hi = 0.0;
};

/** The free intervals (at least MinWidth wide) of the strip from the kerb to the building front at one X, after removing the blockers in the walking band. */
inline void StFreeGaps(const TArray<const TArray<FBox>*>& Sets, int32 Side, double X, double FrontAbsY, double FloorAbs, double MinWidth, TArray<FStGap>& Out)
{
    constexpr double KerbAbsY = 250.0;
    TArray<FStGap> Blocked;
    for (const TArray<FBox>* Set : Sets)
    {
        for (const FBox& Box : *Set)
        {
            if (X < Box.Min.X || X > Box.Max.X || Box.Max.Z <= FloorAbs + 2.0 || Box.Min.Z >= FloorAbs + StWalkBand)
            {
                continue;
            }
            FStGap Range;
            Range.Lo = Side > 0 ? Box.Min.Y : -Box.Max.Y;
            Range.Hi = Side > 0 ? Box.Max.Y : -Box.Min.Y;
            Blocked.Add(Range);
        }
    }
    Blocked.Sort([](const FStGap& A, const FStGap& B) { return A.Lo < B.Lo; });
    Out.Reset();
    double Cursor = KerbAbsY;
    const auto Close = [&](double Hi)
    {
        if (Hi - Cursor >= MinWidth)
        {
            FStGap Gap;
            Gap.Lo = Cursor;
            Gap.Hi = Hi;
            Out.Add(Gap);
        }
    };
    for (const FStGap& Range : Blocked)
    {
        if (Range.Hi <= Cursor)
        {
            continue;
        }
        Close(FMath::Min(Range.Lo, FrontAbsY));
        Cursor = FMath::Max(Cursor, Range.Hi);
    }
    Close(FrontAbsY);
}
} // namespace PWKitStallTest

#endif // WITH_DEV_AUTOMATION_TESTS
