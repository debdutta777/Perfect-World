// Checks for the production face of the three unstaffed browsing shops (R4-01; PerfectWorldLaneKitStalls.h): the posted hours are the
// front door's core windows and nothing else, and the stalls the generic ground floor drew are planned clear of the entrance approach.
// Run in the editor under "PerfectWorld.KitStalls" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfectWorldLaneKitStallsTestSupport.h"
#include "PerfectWorldLaneShops.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitStallsPostedHoursTest, "PerfectWorld.KitStalls.PostedHours",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitStallsPostedHoursTest::RunTest(const FString& Parameters)
{
    using namespace PWKitStallTest;
    const TArray<FName> Ids = UnstaffedBrowseShopIds();
    TestEqual(TEXT("three unstaffed browsing shops"), Ids.Num(), 3);
    TestFalse(TEXT("the staffed hardware store is not one of them"), IsUnstaffedBrowseShop(FName(TEXT("building.nabapara.hardware_store"))));
    TestFalse(TEXT("the staffed repair workshop is not one of them"), IsUnstaffedBrowseShop(FName(TEXT("building.nabapara.repair_workshop"))));

    for (const TCHAR* Shop : StShops)
    {
        const FString Name = Shop;
        TestTrue(*FString::Printf(TEXT("%s is an unstaffed browsing shop"), *Name), IsUnstaffedBrowseShop(FName(Shop)));
        const LI::FrontageEntry* Row = LI::FindFrontageById(std::string(TCHAR_TO_UTF8(Shop)));
        const PerfectWorld::DoorDef* Door = Row != nullptr ? PerfectWorld::FindDoor(Row->EntranceDoorId) : nullptr;
        if (Door == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no catalogue row or no authored front door"), *Name));
            continue;
        }
        TestTrue(*FString::Printf(TEXT("%s: the front door is a business-hours door"), *Name), Door->Access == PerfectWorld::DoorAccess::BusinessHours);

        const FString Hours = PostedHoursText(*Door);
        TestEqual(*FString::Printf(TEXT("%s: the posted hours are the door's windows"), *Name), Hours, StExpectedHours(*Door));
        TestEqual(*FString::Printf(TEXT("%s: the policy is 09:00-13:00 and 15:00-19:30"), *Name), Hours, FString(TEXT("09:00-13:00 and 15:00-19:30")));

        const FString Sign = PostedSignText(*Door);
        TArray<FString> Lines;
        Sign.ParseIntoArray(Lines, TEXT("\n"), false);
        TestEqual(*FString::Printf(TEXT("%s: the board has three lines"), *Name), Lines.Num(), 3);
        if (Lines.Num() == 3)
        {
            TestEqual(*FString::Printf(TEXT("%s: the hours line"), *Name), Lines[1], Hours);
            TestEqual(*FString::Printf(TEXT("%s: the unstaffed line is exactly the phrase"), *Name), Lines[2],
                      FString(TEXT("Unstaffed. Purchasing unavailable.")));
        }
        TestTrue(*FString::Printf(TEXT("%s: the board carries every required phrase"), *Name), StHasEveryRequiredPhrase(Sign));
        const FString Notice = BrowseNoticeText(*Door);
        TestTrue(*FString::Printf(TEXT("%s: the browse notice carries every required phrase"), *Name), StHasEveryRequiredPhrase(Notice));
        TestFalse(*FString::Printf(TEXT("%s: the board implies no buying or paying"), *Name), StImpliesCommerce(Sign));
        TestFalse(*FString::Printf(TEXT("%s: the notice implies no buying or paying"), *Name), StImpliesCommerce(Notice));
    }

    // The text follows the DoorDef: a different policy gives a different board, a non-business door posts nothing.
    static constexpr PerfectWorld::DoorOpeningWindow Windows[] = {{485, 545}, {600, 1440}};
    const PerfectWorld::DoorDef Synthetic{"door.test.hours", "location.test.hours", false, PerfectWorld::DoorAccess::BusinessHours, 0, 0, Windows, false, 1};
    TestEqual(TEXT("two windows"), PostedHoursText(Synthetic), FString(TEXT("08:05-09:05 and 10:00-24:00")));
    const PerfectWorld::DoorDef Legacy{"door.test.legacy", "location.test.legacy", false, PerfectWorld::DoorAccess::BusinessHours, 420, 480, {}, false, 1};
    TestEqual(TEXT("the legacy single window"), PostedHoursText(Legacy), FString(TEXT("07:00-08:00")));
    const PerfectWorld::DoorDef Open{"door.test.public", "location.test.public", true, PerfectWorld::DoorAccess::Public, 0, 0, {}, true, 1};
    TestTrue(TEXT("a public door posts no hours"), PostedHoursText(Open).IsEmpty() && PostedSignText(Open).IsEmpty() && BrowseNoticeText(Open).IsEmpty());
    const PerfectWorld::DoorDef* Hardware = PerfectWorld::FindDoor("door.nabapara.hardware_store_front");
    if (Hardware != nullptr)
    {
        TestEqual(TEXT("the hardware store's own hours come from its own door"), PostedHoursText(*Hardware), StExpectedHours(*Hardware));
        TestNotEqual(TEXT("... and differ from the browsing shops'"), PostedHoursText(*Hardware), FString(TEXT("09:00-13:00 and 15:00-19:30")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitStallsPlanTest, "PerfectWorld.KitStalls.StallPlan",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitStallsPlanTest::RunTest(const FString& Parameters)
{
    using namespace PWKitStallTest;
    for (const TCHAR* Shop : StShops)
    {
        const FString Name = Shop;
        FLaneBuilding B;
        FLaneKitFrontage Kit;
        if (!StBuild(Shop, B, Kit))
        {
            AddError(FString::Printf(TEXT("%s: not a recognised kit frontage"), *Name));
            continue;
        }
        const LI::InteriorLayout Layout = MakeKitFrontageLayout(B, Kit);
        const LI::Opening* Entrance = Layout.Ok() ? KitFrontEntrance(Layout) : nullptr;
        if (Entrance == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no layout or no entrance"), *Name));
            continue;
        }
        const double DoorU = 0.5 * (Entrance->Plan.U0 + Entrance->Plan.U1);

        // The stalls this module treats as the generic ones are exactly the niches BuildLaneGroundFloor cuts.
        TArray<FKitStallBay> Generic;
        GetGenericStallBays(B, Generic);
        FLaneScene Scratch;
        TArray<FLaneSpan> Recesses;
        BuildLaneGroundFloor(Scratch, B, Recesses);
        TestEqual(*FString::Printf(TEXT("%s: as many generic stalls as the generic ground floor cuts niches"), *Name), Generic.Num(), Recesses.Num());
        for (int32 I = 0; I < FMath::Min(Generic.Num(), Recesses.Num()); ++I)
        {
            TestTrue(*FString::Printf(TEXT("%s: generic stall %d spans the niche"), *Name, I),
                     FMath::IsNearlyEqual(Generic[I].U0, Recesses[I].U0, 0.01) && FMath::IsNearlyEqual(Generic[I].U1, Recesses[I].U1, 0.01));
        }

        TArray<FKitStallBay> Plan;
        PlanKitStalls(B, DoorU, Plan);
        double PreviousEnd = -1000.0;
        for (const FKitStallBay& Bay : Plan)
        {
            TestTrue(*FString::Printf(TEXT("%s: a stall is inside the edge pillars"), *Name),
                     Bay.U0 >= KitPublic::StallEdgeCm - 0.01 && Bay.U1 <= B.Width - KitPublic::StallEdgeCm + 0.01);
            TestTrue(*FString::Printf(TEXT("%s: a stall is at least 120 wide"), *Name), Bay.U1 - Bay.U0 >= KitPublic::StallMinWidthCm - 0.01);
            TestTrue(*FString::Printf(TEXT("%s: no stall stands within 90 cm of the doorway's centre"), *Name),
                     Bay.U1 <= DoorU - KitPublic::ApproachHalfCm + 0.01 || Bay.U0 >= DoorU + KitPublic::ApproachHalfCm - 0.01);
            TestTrue(*FString::Printf(TEXT("%s: stalls keep a pillar apart, in order"), *Name), Bay.U0 >= PreviousEnd + KitPublic::StallEdgeCm - 0.01);
            PreviousEnd = Bay.U1;
        }

        if (Name.EndsWith(TEXT("market_stalls_a")))
        {
            TestEqual(TEXT("market_stalls_a: two stalls generic"), Generic.Num(), 2);
            if (Plan.Num() == 2)
            {
                TestTrue(TEXT("market_stalls_a: the first stall, which covered the doorway, is trimmed to U 40 to 160 (the 120 cm west of the approach)"),
                         Plan[0].bMoved && FMath::IsNearlyEqual(Plan[0].U0, 40.0, 0.01) && FMath::IsNearlyEqual(Plan[0].U1, 160.0, 0.01));
                TestTrue(TEXT("market_stalls_a: the second stall keeps U 470 to 860"),
                         !Plan[1].bMoved && FMath::IsNearlyEqual(Plan[1].U0, 470.0, 0.01) && FMath::IsNearlyEqual(Plan[1].U1, 860.0, 0.01));
            }
            else
            {
                AddError(FString::Printf(TEXT("market_stalls_a: %d stalls planned, expected 2"), Plan.Num()));
            }
        }
        else if (Name.EndsWith(TEXT("market_stalls_b")))
        {
            TestEqual(TEXT("market_stalls_b: one stall generic"), Generic.Num(), 1);
            if (Plan.Num() == 1)
            {
                TestTrue(TEXT("market_stalls_b: its stall is shifted whole, east of the approach: U 340 to 653.3"),
                         Plan[0].bMoved && FMath::IsNearlyEqual(Plan[0].U0, 340.0, 0.01) && FMath::IsNearlyEqual(Plan[0].U1, 340.0 + 940.0 / 3.0, 0.01));
            }
            else
            {
                AddError(FString::Printf(TEXT("market_stalls_b: %d stalls planned, expected 1"), Plan.Num()));
            }
        }
        else
        {
            TestTrue(TEXT("end_shop: shutter bays, no stalls to keep"), Generic.IsEmpty() && Plan.IsEmpty());
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
