#include "PerfectWorldTailorBindings.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Misc/AutomationTest.h"
#include "PerfectWorldLaneKitStalls.h"
#include "PerfectWorldNpcLooks.h"
#include "PerfectWorldNpcPilotBridge.h"
#include "PerfectWorldProprietorBindings.h"
#include "Doors.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR66TailorBindingsTest, "PerfectWorld.Tailor.BindingsAndPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR66TailorBindingsTest::RunTest(const FString&)
{
    using namespace PWLane;
    FLaneBuilding Building;
    if (!TestTrue(TEXT("catalogue building resolves"), MakeProprietorBuilding(R66TailorBuilding, Building))) return false;
    const auto Provider = R66TailorBindings(Building);
    TestEqual(TEXT("five local anchors and off-lane home"), Provider.Anchors.Num(), 6);
    TestEqual(TEXT("four real polylines and home route"), Provider.Routes.Num(), 5);
    TestEqual(TEXT("both doorways participate in yield"), Provider.Doorways.Num(), 2);
    const auto* Home = Provider.Anchors.Find(FName(TEXT("location.nabapara.dutta_home")));
    TestTrue(TEXT("off-lane home exits through actual shop threshold"), Home != nullptr && Home->Kind == ENpBindingKind::OffLane &&
        Home->ExitAnchor == FName(TEXT("anchor.tailor_shop.front_door")));
    TestTrue(TEXT("tailor opts into production safe-pause bridge"), R55IsWorkPilot(FName(R66TailorPerson)));
    TestFalse(TEXT("cloth merchant is not implicitly staffed"), R55IsWorkPilot(FName(TEXT("person.cloth_merchant"))));
    Building.Width += 1;
    TestTrue(TEXT("wrong building refuses provider rather than fallback"), R66TailorBindings(Building).Anchors.IsEmpty());
    const auto* Door = PerfectWorld::FindDoor("door.nabapara.tailor_shop_front");
    const auto* Other = PerfectWorld::FindDoor("door.nabapara.cloth_merchant_front");
    if (!TestTrue(TEXT("existing doors"), Door != nullptr && Other != nullptr)) return false;
    TestTrue(TEXT("tailor notice says staffed and unavailable purchasing"), BrowseNoticeText(*Door).Contains(KitPublic::R66TailorStaffedPhrase));
    TestTrue(TEXT("other shop retains original notice"), BrowseNoticeText(*Other).Contains(KitPublic::UnstaffedPhrase));
    TestNearlyEqual(TEXT("frozen authored height"), PWNpc::StandingHeightCm(FName(R66TailorPerson), EPWNpcOutfit::ShopProprietor), 167.0, 0.001);
    for (const TCHAR* Action : {TEXT("work.measure"), TEXT("work.cut"), TEXT("work.stitch"), TEXT("work.press")})
        TestEqual(TEXT("each phase has an authored gesture"), FName(PWNpcGesture::Find(FName(Action)).Id), FName(Action));
    return true;
}
#endif
