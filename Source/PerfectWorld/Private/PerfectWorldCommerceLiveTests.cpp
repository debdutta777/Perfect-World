#include "PerfectWorldCommerceLive.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldKeyBindings.h"
#include "PerfectWorldCommerceCounter.h"
#include "PerfectWorldInteractionComponent.h"
#include "WorldHash.h"

namespace
{
struct FR88LiveFixture
{
    PerfectWorld::CommerceContent Content;
    PerfectWorld::PersonSchedule Keeper{"person.mahua_saha", "anchor.hardware_store.staff_counter", {
        {PerfectWorld::ScheduleAllWeekdays, 0, 360, "anchor.hardware_store.staff_counter", PerfectWorld::ScheduleDoingKind::Role, "sleeping", 0},
        {PerfectWorld::ScheduleAllWeekdays, 360, 1200, "anchor.hardware_store.staff_counter", PerfectWorld::ScheduleDoingKind::Role, "serving", 0},
        {PerfectWorld::ScheduleAllWeekdays, 1200, 1440, "anchor.hardware_store.staff_counter", PerfectWorld::ScheduleDoingKind::Role, "sleeping", 0}}, {}};
    PerfectWorld::ScheduleTravelTable Travel;
    std::vector<std::string_view> Anchors{"anchor.hardware_store.staff_counter"};
    std::vector<PerfectWorld::CommerceServiceBinding> Bindings;
    std::vector<PerfectWorld::CommerceCatalogueEntry> Entries;
    TUniquePtr<PerfectWorld::WorldSim> Sim;
    TSharedPtr<PWCommerce::FCmLiveCommerceWorld> Bridge;
    bool bAtCounter = true;
    int32 Submitted = 0;
    FR88LiveFixture()
    {
        Content.Items = {{"item.jhalmuri", "Jhalmuri", "portion", 10, PerfectWorld::CommerceEffect::Food, 3000},
            {"item.tea", "Tea", "cup", 10, PerfectWorld::CommerceEffect::Tea, 0}};
        Entries = {{"shop.test", "item.jhalmuri", 4000}, {"shop.test", "item.tea", 2500}};
        PerfectWorld::CommerceShop Shop;
        Shop.Id = "shop.test"; Shop.Name = "Test counter"; Shop.CounterAnchor = "anchor.customer"; Shop.Entries = Entries;
        Content.Shops.push_back(Shop);
        Bindings = {{"shop.test", "person.mahua_saha", "anchor.customer", &Keeper, &Travel, Anchors,
            PerfectWorld::ScheduleAllWeekdays, 360, 1200, {}}};
        Sim = MakeUnique<PerfectWorld::WorldSim>(88, PerfectWorld::MaxEventsPerAdvance, PerfectWorld::DayLoopCatalogues{},
            PerfectWorld::WorldCommerceCatalogues{Content.Items, Entries, Bindings});
        PWCommerce::FCmLiveAccess Access;
        Access.Read = [this]() { return Sim.Get(); };
        Access.Submit = [this](const PerfectWorld::Command& Command) { ++Submitted; return Sim->Submit(Command); };
        Access.Approach = [this](FName) { return bAtCounter ? std::string("anchor.customer") : std::string(); };
        Bridge = MakeShared<PWCommerce::FCmLiveCommerceWorld>(Content, MoveTemp(Access));
    }
    FName Shop() const { return FName(TEXT("shop.test")); }
    FName Food() const { return FName(TEXT("item.jhalmuri")); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR88LivePurchaseTest, "PerfectWorld.CommerceLive.ReducerPurchaseInventoryAndConsume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR88LivePurchaseTest::RunTest(const FString& Parameters)
{
    FR88LiveFixture F;
    const auto Before = F.Bridge->ReadWallet();
    const auto Offer = F.Bridge->BeginService(F.Shop());
    if (!TestTrue(TEXT("live reducer opens service"), Offer.Ok())) return false;
    TestEqual(TEXT("content goods"), Offer.Goods.Num(), 2);
    const auto Quote = F.Bridge->Quote(F.Shop(), F.Food(), 1);
    TestTrue(TEXT("core quote permits one food"), Quote.CanBuy());
    const auto Bought = F.Bridge->Buy(F.Shop(), F.Food(), 1, Quote.Quote.ExpectedRevision);
    TestTrue(TEXT("accepted with reducer receipt"), Bought.bAccepted && Bought.bHasReceipt);
    TestEqual(TEXT("wallet follows core receipt"), F.Bridge->ReadWallet().GetValue(), Bought.Receipt.BalanceAfterPaise);
    TestEqual(TEXT("core debited quote total"), Before.GetValue() - F.Bridge->ReadWallet().GetValue(), Quote.Quote.TotalPaise);
    const auto Hash = PerfectWorld::HashWorldState(F.Sim->State());
    const auto Seq = F.Sim->State().Seq;
    const auto Retry = F.Bridge->Buy(F.Shop(), F.Food(), 1, Quote.Quote.ExpectedRevision);
    TestTrue(TEXT("duplicate keeps precise stale code"), !Retry.bAccepted && Retry.Error == PerfectWorld::CommerceError::StaleRevision);
    TestTrue(TEXT("duplicate is atomic"), PerfectWorld::HashWorldState(F.Sim->State()) == Hash && F.Sim->State().Seq == Seq);
    F.Bridge->EndService(F.Shop());
    const auto Inventory = F.Bridge->ReadInventory();
    TestTrue(TEXT("real inventory and newest receipt"), Inventory.bAvailable && Inventory.Stacks.Num() == 1 && Inventory.Receipts.Num() == 1);
    const auto ReadHash = PerfectWorld::HashWorldState(F.Sim->State());
    F.Bridge->ReadInventory();
    TestTrue(TEXT("read projection leaves state unchanged"), PerfectWorld::HashWorldState(F.Sim->State()) == ReadHash);
    TestTrue(TEXT("consume submits to reducer"), F.Bridge->Consume(F.Food()).bAccepted);
    TestEqual(TEXT("food removed"), F.Bridge->ReadInventory().Stacks.Num(), 0);
    TestTrue(TEXT("second consume is NotOwned"), F.Bridge->Consume(F.Food()).Error == PerfectWorld::CommerceError::NotOwned);
    TestEqual(TEXT("zero game minutes for shopping and use"), F.Sim->State().Clock.Now(), PerfectWorld::GameInstant(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR88LiveRefusalTest, "PerfectWorld.CommerceLive.RefusalsAndSafePause",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR88LiveRefusalTest::RunTest(const FString& Parameters)
{
    FR88LiveFixture F;
    F.bAtCounter = false;
    TestTrue(TEXT("actual approach is required"), F.Bridge->BeginService(F.Shop()).Error == PerfectWorld::CommerceError::NotAtCounter);
    F.bAtCounter = true;
    TestTrue(TEXT("one minute advanced in reducer"), F.Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::AdvanceTime{1}}).Accepted);
    TestTrue(TEXT("safe pause is pending"), F.Bridge->BeginService(F.Shop()).Error == PerfectWorld::CommerceError::NotAtSafePause);
    TestTrue(TEXT("pending retains one request"), F.Sim->CommerceService("shop.test").Requested);
    TestTrue(TEXT("accepted pending is distinguished from a refusal"), F.Bridge->BeginService(F.Shop()).bPending);
    const int32 Submitted = F.Submitted;
    F.Bridge->BeginService(F.Shop());
    TestEqual(TEXT("polling does not resubmit the request"), F.Submitted, Submitted);
    TestTrue(TEXT("world time reaches safe pause"), F.Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::AdvanceTime{2}}).Accepted);
    TestTrue(TEXT("panel offer only when core ready"), F.Bridge->BeginService(F.Shop()).Ok());
    const auto Quote = F.Bridge->Quote(F.Shop(), F.Food(), 10);
    TestTrue(TEXT("unaffordable uses core code"), Quote.Blocked == PerfectWorld::CommerceError::InsufficientFunds);
    const auto Buy = F.Bridge->Buy(F.Shop(), F.Food(), 10, Quote.Quote.ExpectedRevision);
    TestTrue(TEXT("code preserved verbatim"), Buy.Error == PerfectWorld::CommerceError::InsufficientFunds &&
        Buy.Code == UTF8_TO_TCHAR(std::string(PerfectWorld::CommerceErrorCode(Buy.Error)).c_str()));
    F.Bridge->EndService(F.Shop());
    const auto Seq = F.Sim->State().Seq;
    F.Bridge->EndService(F.Shop());
    TestEqual(TEXT("close is idempotent"), F.Sim->State().Seq, Seq);
    F.Bindings[0].CloseMinute = 360;
    TestTrue(TEXT("closed counter uses core code"), F.Bridge->BeginService(F.Shop()).Error == PerfectWorld::CommerceError::Closed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR88InventoryBindingTest, "PerfectWorld.CommerceLive.InventoryBindingStore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR88InventoryBindingTest::RunTest(const FString& Parameters)
{
    FKbBindingSet Keys;
    TestTrue(TEXT("keyboard and controller defaults"), Keys.Get(EKbAction::Inventory, EKbDevice::Keyboard) == EKeys::I &&
        Keys.Get(EKbAction::Inventory, EKbDevice::Gamepad) == EKeys::Gamepad_DPad_Down);
    TestTrue(TEXT("camera and inventory do not share Down"), Keys.Get(EKbAction::ZoomOut, EKbDevice::Gamepad) != EKeys::Gamepad_DPad_Down);
    TestTrue(TEXT("Inventory remaps"), Keys.Set(EKbAction::Inventory, EKbDevice::Keyboard, EKeys::K, false) == EKbSetResult::Applied);
    FKbBindingSet Restored;
    TestTrue(TEXT("binding persists under stable id"), Restored.FromText(Keys.ToText()) && Restored.Get(EKbAction::Inventory, EKbDevice::Keyboard) == EKeys::K);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR88LiveClosureTest, "PerfectWorld.CommerceLive.ClosureRaceAndLifecycleCodes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR88LiveClosureTest::RunTest(const FString& Parameters)
{
    FR88LiveFixture F;
    auto Other = F.Content.Shops.front();
    Other.Id = "shop.other";
    F.Content.Shops.push_back(Other);
    auto Binding = F.Bindings.front();
    Binding.ShopId = "shop.other";
    F.Bindings.push_back(Binding);
    F.Sim = MakeUnique<PerfectWorld::WorldSim>(88, PerfectWorld::MaxEventsPerAdvance,
        PerfectWorld::DayLoopCatalogues{}, PerfectWorld::WorldCommerceCatalogues{F.Content.Items, F.Entries, F.Bindings});
    if (!TestTrue(TEXT("service at first counter"), F.Bridge->BeginService(F.Shop()).Ok())) return false;
    const auto Conflict = F.Bridge->BeginService(FName(TEXT("shop.other")));
    TestTrue(TEXT("non-enum reducer code survives"), !Conflict.Ok() && !Conflict.bPending &&
        Conflict.Code == TEXT("commerce.service_active"));
    const auto Quote = F.Bridge->Quote(F.Shop(), F.Food(), 1);
    F.Bindings[0].CloseMinute = 360;
    const auto Hash = PerfectWorld::HashWorldState(F.Sim->State());
    const auto Seq = F.Sim->State().Seq;
    const auto Result = F.Bridge->Buy(F.Shop(), F.Food(), 1, Quote.Quote.ExpectedRevision);
    TestTrue(TEXT("closure revalidated at confirmation"), !Result.bAccepted &&
        Result.Error == PerfectWorld::CommerceError::Closed);
    TestTrue(TEXT("closure refusal leaves persistent state and sequence"),
        PerfectWorld::HashWorldState(F.Sim->State()) == Hash && F.Sim->State().Seq == Seq);
    F.Bridge->EndService(F.Shop());
    TestFalse(TEXT("closed shop releases its service"), F.Sim->CommerceService("shop.test").Requested);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR88LiveReloadTest, "PerfectWorld.CommerceLive.ReloadAndMissingWorld",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR88LiveReloadTest::RunTest(const FString& Parameters)
{
    FR88LiveFixture F;
    if (!TestTrue(TEXT("service before reload"), F.Bridge->BeginService(F.Shop()).Ok())) return false;
    const auto OldQuote = F.Bridge->Quote(F.Shop(), F.Food(), 1);
    if (!TestTrue(TEXT("purchase before reload"), F.Bridge->Buy(F.Shop(), F.Food(), 1,
        OldQuote.Quote.ExpectedRevision).bAccepted)) return false;
    const auto Snapshot = F.Sim->State();
    F.Sim = MakeUnique<PerfectWorld::WorldSim>(Snapshot, PerfectWorld::MaxEventsPerAdvance,
        PerfectWorld::DayLoopCatalogues{}, PerfectWorld::WorldCommerceCatalogues{F.Content.Items, F.Entries, F.Bindings});
    TestTrue(TEXT("bridge follows replacement world and its wallet"),
        F.Bridge->ReadWallet().GetValue() == Snapshot.Commerce.WalletPaise);
    TestFalse(TEXT("reload discards transient service"), F.Sim->CommerceService("shop.test").Requested);
    TestFalse(TEXT("old service cannot authorize a fresh quote"), F.Bridge->Quote(F.Shop(), F.Food(), 1).CanBuy());
    TestTrue(TEXT("reloaded inventory can be consumed"), F.Bridge->Consume(F.Food()).bAccepted);
    F.Sim.Reset();
    TestFalse(TEXT("no wallet after owner teardown"), F.Bridge->ReadWallet().IsSet());
    TestFalse(TEXT("no inventory after owner teardown"), F.Bridge->ReadInventory().bAvailable);
    TestTrue(TEXT("no service after owner teardown"),
        F.Bridge->BeginService(F.Shop()).Error == PerfectWorld::CommerceError::InvalidState);
    TestFalse(TEXT("no confirmation after owner teardown"),
        F.Bridge->Quote(F.Shop(), F.Food(), 1).CanBuy());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR89CounterReachTest, "PerfectWorld.CommerceLive.CounterReachRetention",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR89CounterReachTest::RunTest(const FString& Parameters)
{
    // A 165 cm character with a 34 cm radius has a 48.5 cm half-axis.
    const FVector R89Bottom(0, 0, 34);
    const FVector R89Top(0, 0, 131);
    const double R89Reach = PWInteraction::ReachCm + PWInteraction::ReachHysteresisCm;
    TestTrue(TEXT("strict acquisition boundary"), PWCommerce::R89WithinCounterReach(R89Bottom, R89Top, FVector(200, 0, 110)));
    TestTrue(TEXT("visible retained Buy beyond strict reach"), PWCommerce::R89WithinCounterReach(R89Bottom, R89Top, FVector(215, 0, 110)));
    TestTrue(TEXT("inclusive retained boundary"), PWCommerce::R89WithinCounterReach(R89Bottom, R89Top, FVector(R89Reach, 0, 110)));
    TestFalse(TEXT("outside retained reach"), PWCommerce::R89WithinCounterReach(R89Bottom, R89Top, FVector(R89Reach + 0.1, 0, 110)));
    TestTrue(TEXT("vertical reach starts at nearest axis endpoint"), PWCommerce::R89WithinCounterReach(R89Bottom, R89Top, R89Top + FVector(0, 0, R89Reach)));
    TestFalse(TEXT("diagonal outside spherical reach"), PWCommerce::R89WithinCounterReach(R89Bottom, R89Top, R89Top + FVector(200, 0, 200)));
    TestTrue(TEXT("non-character point axis fallback"), PWCommerce::R89WithinCounterReach(FVector::ZeroVector, FVector::ZeroVector, FVector(R89Reach, 0, 0)));
    return true;
}
#endif
