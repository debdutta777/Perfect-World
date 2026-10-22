#include "PerfectWorldShopsR7Bindings.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "NeighbourhoodPlan.h"

// Run in the editor under "PerfectWorld.ShopsR7" (Session Frontend or the Unreal MCP AutomationTestToolset). Geometry only: no world.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR74ShopsBindingsTest, "PerfectWorld.ShopsR7.BindingsAndNotices",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR74ShopsBindingsTest::RunTest(const FString&)
{
    using namespace PWLane;
    const auto Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    const struct { const TCHAR* Workplace; int32 Anchors; int32 Routes; } Expected[] = {
        {R74SweetShopId, 5, 4}, {R74PharmacyId, 3, 2}, {R74TeaId, 3, 2}, {R74VendorId, 3, 2}};
    for (const auto& Row : Expected)
    {
        const auto Provider = R74ShopBindings(Plan, FName(Row.Workplace));
        TestEqual(*FString::Printf(TEXT("%s: anchors"), Row.Workplace), Provider.Anchors.Num(), Row.Anchors);
        TestEqual(*FString::Printf(TEXT("%s: routes"), Row.Workplace), Provider.Routes.Num(), Row.Routes);
        TestEqual(*FString::Printf(TEXT("%s: entrances are not doorways"), Row.Workplace), Provider.Doorways.Num(), 0);
    }
    TestTrue(TEXT("an unknown workplace has no provider"), R74ShopBindings(Plan, FName(TEXT("pitch.nabapara.nothing"))).Anchors.IsEmpty());

    int32 Owners = 0;
    for (const auto& Staff : PerfectWorld::ShopStaffR7Definitions())
    {
        const FString Who = UTF8_TO_TCHAR(std::string(Staff.PersonId).c_str());
        const auto Provider = R74ShopBindings(Plan, FName(UTF8_TO_TCHAR(std::string(Staff.WorkplaceId).c_str())));
        const auto* Counter = Provider.Anchors.Find(FName(UTF8_TO_TCHAR(std::string(Staff.CounterId).c_str())));
        const auto* Home = Provider.Anchors.Find(FName(UTF8_TO_TCHAR(std::string(Staff.HomeId).c_str())));
        const auto* ToEntrance = Provider.Routes.Find(R74CounterRouteId(Staff));
        const auto* ToHome = Provider.Routes.Find(R74HomeRouteId(Staff));
        if (!TestTrue(*(Who + TEXT(": counter, home and both routes bind")), Counter != nullptr && Home != nullptr && ToEntrance != nullptr && ToHome != nullptr))
        {
            continue;
        }
        TestTrue(*(Who + TEXT(": the counter anchor is where the core says")), Counter->Feet.GetLocation().Equals(R74StaffFeet(Staff), 0.5));
        TestEqual(*(Who + TEXT(": the keeper faces the para road (+Y)")), FMath::RoundToInt(Counter->Feet.Rotator().Yaw), 90);
        TestTrue(*(Who + TEXT(": home is off-lane and exits at the entrance")), Home->Kind == ENpBindingKind::OffLane &&
            Home->ExitAnchor == FName(UTF8_TO_TCHAR(std::string(Staff.EntranceId).c_str())));
        TestTrue(*(Who + TEXT(": the home leg is an off-lane abstraction, never a visible polyline")), ToHome->bOffLane && ToHome->Points.IsEmpty());
        double Cm = 0.0;
        for (int32 Index = 1; Index < ToEntrance->Points.Num(); ++Index) { Cm += FVector::Dist(ToEntrance->Points[Index - 1], ToEntrance->Points[Index]); }
        TestNearlyEqual(*(Who + TEXT(": counter-to-entrance polyline length is the schedule's")), Cm, static_cast<double>(Staff.EntranceRouteCm), 1.0);
        const auto* PlannedEntrance = PerfectWorld::FindNeighbourhoodFeature(Plan, Staff.EntranceId);
        TestTrue(*(Who + TEXT(": the route ends on the planned entrance")), PlannedEntrance != nullptr && !PlannedEntrance->Geometry.empty() &&
            ToEntrance->Points.Last().Equals(FVector(static_cast<double>(PlannedEntrance->Geometry[0].X),
                static_cast<double>(PlannedEntrance->Geometry[0].Y), static_cast<double>(PlannedEntrance->Geometry[0].Z)), 0.5));
        // A customer in the flap (shops) or in front of the counter (pitches) is inside the 200 cm interaction reach of the keeper's chest.
        const double Reach = FVector::Dist2D(R74CustomerPoint(Staff), R74StaffFeet(Staff));
        TestTrue(*(Who + TEXT(": the customer point is within interaction reach")), Reach <= 190.0);
        const FString Notice = R74BrowseNotice(Staff);
        TestTrue(*(Who + TEXT(": the notice says purchasing is unavailable")), Notice.Contains(TEXT("Purchasing is unavailable")));
        TestTrue(*(Who + TEXT(": the notice posts the hours")), Notice.Contains(R74PostedHoursText(Staff)));
        TestFalse(*(Who + TEXT(": the notice never offers a sale")), Notice.Contains(TEXT("Buy")) || Notice.Contains(TEXT("Price")));
        Owners += R74IsCounterOwner(Staff) ? 1 : 0;
    }
    TestEqual(TEXT("four counter owners, one per workplace"), Owners, 4);
    TestEqual(TEXT("sweet-shop hours exclude the afternoon lull"), R74PostedHoursText(PerfectWorld::ShopStaffR7Definitions()[0]),
        FString(TEXT("07:00-13:30 and 15:30-21:00")));
    return true;
}
#endif
