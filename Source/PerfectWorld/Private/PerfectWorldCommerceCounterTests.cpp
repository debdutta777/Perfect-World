// R8-05 counter interaction (PerfectWorldCommerceCounter.h): "Buy" at a staffed counter asks the commerce seam for service and, when the
// core says no, shows the precise reason; when it says yes the paused panel opens (not provable without a viewport, so the release of the
// service when the panel cannot open is what is tested here; the real open is the commerce acceptance scenario's). Run in the editor
// under "PerfectWorld.CommerceUi.Counter".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "PerfectWorldCommerceCounter.h"
#include "PerfectWorldCommerceTestSupport.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldShopDisplayActor.h"

namespace
{
using namespace PWCommerce;
using namespace PWCommerceTest;
using PerfectWorld::CommerceError;

/** A throw-away world with a player (and its interaction component) and the Shanti Chai counter. */
struct FCmCounterFixture
{
    UWorld* World = nullptr;
    APerfectWorldPlayerCharacter* Player = nullptr;
    UPWInteractionComponent* Interaction = nullptr;
    APWCounterBuyActor* Counter = nullptr;

    FCmCounterFixture()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (World == nullptr)
        {
            return;
        }
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Player = World->SpawnActor<APerfectWorldPlayerCharacter>(APerfectWorldPlayerCharacter::StaticClass(), FTransform(FVector(0.0, 0.0, 500.0)), Spawn);
        Interaction = Player != nullptr ? Player->FindComponentByClass<UPWInteractionComponent>() : nullptr;
        Counter = World->SpawnActor<APWCounterBuyActor>(FVector(1200.0, 300.0, 0.0), FRotator::ZeroRotator, Spawn);
        if (Counter != nullptr)
        {
            Counter->Configure(FName(TEXT("shop.shanti_chai")), FName(TEXT("interaction.nabapara.shanti_chai.buy")), FText::GetEmpty(), FVector(1200.0, 300.0, 0.0));
        }
    }
    ~FCmCounterFixture()
    {
        if (World != nullptr)
        {
            CmBridge::Remove(World);
            World->DestroyWorld(false);
        }
    }
    FCmCounterFixture(const FCmCounterFixture&) = delete;
    FCmCounterFixture& operator=(const FCmCounterFixture&) = delete;
    bool Ok() const { return World != nullptr && Player != nullptr && Interaction != nullptr && Counter != nullptr; }
    FString Notice() const { return Interaction->GetNotice().ToString(); }
    void Press()
    {
        Interaction->ShowNotice(FText::GetEmpty());
        Counter->Interact(Player);
    }
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmCounterBasicsTest, "PerfectWorld.CommerceUi.Counter.TheCounterOffersBuyAndNeedsASeam",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmCounterBasicsTest::RunTest(const FString& Parameters)
{
    FCmCounterFixture Fixture;
    if (!TestTrue(TEXT("a world, a player and a counter"), Fixture.Ok())) return false;
    TestEqual(TEXT("the verb is Buy"), Fixture.Counter->GetInteractionVerb(Fixture.Player).ToString(), FString(TEXT("Buy")));
    TestEqual(TEXT("the shop"), Fixture.Counter->GetShopId(), FName(TEXT("shop.shanti_chai")));
    TestTrue(TEXT("the tag"), Fixture.Counter->ActorHasTag(FName(TEXT("interaction.nabapara.shanti_chai.buy"))));
    TestEqual(TEXT("the same priority as the browse shelf (below people)"), Fixture.Counter->GetInteractionPriority(), PWShopDisplay::InteractionPriority);
    TestTrue(TEXT("the point is the counter's"), Fixture.Counter->GetInteractionPoint().Equals(FVector(1200.0, 300.0, 0.0), 0.5));

    // No seam installed: a plain message, no crash, no panel.
    Fixture.Press();
    TestTrue(TEXT("with no commerce seam the player is told nothing can be bought"), Fixture.Notice().Contains(TEXT("Nothing can be bought")));
    TestFalse(TEXT("no panel"), Fixture.Counter->DidOpenPanel());

    // The spawn helper makes one counter per valid spec and skips (and logs) the rest.
    AddExpectedError(TEXT("had no shop id or tag"), EAutomationExpectedErrorFlags::Contains, 2);
    TArray<FCmCounterSpec> Specs;
    FCmCounterSpec Good;
    Good.ShopId = FName(TEXT("shop.stationery"));
    Good.Tag = FName(TEXT("interaction.nabapara.stationery.buy"));
    Good.PointWorld = FVector(5.0, 6.0, 7.0);
    FCmCounterSpec NoTag = Good;
    NoTag.Tag = NAME_None;
    FCmCounterSpec NoShop = Good;
    NoShop.ShopId = NAME_None;
    Specs.Add(Good);
    Specs.Add(NoTag);
    Specs.Add(NoShop);
    TestEqual(TEXT("one counter of three specs"), SpawnCommerceCounters(*Fixture.World, Specs), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmCounterRefusalTest, "PerfectWorld.CommerceUi.Counter.UnavailableShowsThePreciseReason",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmCounterRefusalTest::RunTest(const FString& Parameters)
{
    struct FCase
    {
        const TCHAR* Name;
        TFunction<void(FCmFakeCommerce&)> Stage;
        CommerceError Expected;
        const TCHAR* Phrase;
    };
    const FCase Cases[] = {
        {TEXT("closed"), [](FCmFakeCommerce& F) { F.Context.ShopOpen = false; }, CommerceError::Closed, TEXT("Shanti Chai is closed")},
        {TEXT("keeper away"), [](FCmFakeCommerce& F) { F.Context.KeeperPresent = false; }, CommerceError::KeeperAway, TEXT("keeper is away")},
        {TEXT("finishing a step"), [](FCmFakeCommerce& F) { F.Context.AtSafePause = false; }, CommerceError::NotAtSafePause, TEXT("middle of something")},
        {TEXT("not at the counter"), [](FCmFakeCommerce& F) { F.Context.AtCounter = false; }, CommerceError::NotAtCounter, TEXT("Step up to the counter")},
        {TEXT("asleep"), [](FCmFakeCommerce& F) { F.Context.PlayerAwake = false; }, CommerceError::PlayerAsleep, TEXT("asleep")},
        {TEXT("busy"), [](FCmFakeCommerce& F) { F.Context.PlayerFree = false; }, CommerceError::PlayerBusy, TEXT("busy")},
        {TEXT("unsettled"), [](FCmFakeCommerce& F) { F.Context.WorldSettled = false; }, CommerceError::WorldUnsettled, TEXT("still moving")},
    };
    for (const FCase& Case : Cases)
    {
        FCmCounterFixture Fixture;
        if (!TestTrue(*(FString(Case.Name) + TEXT(": fixture")), Fixture.Ok())) return false;
        const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
        CmBridge::Install(Fixture.World, Fake);
        Case.Stage(*Fake);
        Fixture.Press();
        TestTrue(*(FString(Case.Name) + TEXT(": the core's code")), Fixture.Counter->GetLastError() == Case.Expected);
        TestTrue(*(FString(Case.Name) + TEXT(": the notice says why (") + Fixture.Notice() + TEXT(")")), Fixture.Notice().Contains(Case.Phrase));
        TestFalse(*(FString(Case.Name) + TEXT(": no panel")), Fixture.Counter->DidOpenPanel());
        TestEqual(*(FString(Case.Name) + TEXT(": one request for service")), Fake->BeginCalls, 1);
        TestEqual(*(FString(Case.Name) + TEXT(": nothing bought")), Fake->BuyCalls, 0);
        TestEqual(*(FString(Case.Name) + TEXT(": the wallet is untouched")), Fake->State.WalletPaise, static_cast<int64>(15000));
    }

    // Not enough money: the wallet cannot cover the cheapest thing on the counter.
    {
        FCmCounterFixture Fixture;
        if (!TestTrue(TEXT("fixture"), Fixture.Ok())) return false;
        const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(0); // 120 rupees
        Fixture.Counter->Configure(FName(TEXT("shop.stationery")), NAME_None, FText::GetEmpty(), FVector::ZeroVector);
        TestTrue(TEXT("six pens spend the whole wallet"), Fake->Buy(FName(TEXT("shop.stationery")), FName(TEXT("item.pen")), 6, 0).bAccepted);
        TestEqual(TEXT("nothing left"), Fake->State.WalletPaise, static_cast<int64>(0));
        CmBridge::Install(Fixture.World, Fake);
        Fixture.Press();
        TestTrue(TEXT("the core says insufficient funds"), Fixture.Counter->GetLastError() == CommerceError::InsufficientFunds);
        TestTrue(TEXT("the notice says so"), Fixture.Notice().Contains(TEXT("enough money")));
        TestFalse(TEXT("no panel"), Fixture.Counter->DidOpenPanel());
    }

    // Bag full: twelve different things occupy every slot, so nothing here has a place.
    {
        FCmCounterFixture Fixture;
        if (!TestTrue(TEXT("fixture"), Fixture.Ok())) return false;
        const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(4);
        FCmFakeCommerce::FShop& Filler = Fake->Shops.AddDefaulted_GetRef();
        Filler.Id = FName(TEXT("shop.filler"));
        Filler.Name = FText::FromString(TEXT("Filler Stores"));
        for (int32 Index = 0; Index < 12; ++Index)
        {
            PerfectWorld::CommerceItem Item;
            Item.Id = std::string("item.filler") + std::to_string(Index);
            Item.Name = std::string("Filler ") + std::to_string(Index);
            Item.Unit = "piece";
            Fake->Items.Add(Item);
            PerfectWorld::CommerceCatalogueEntry Entry;
            Entry.ShopId = "shop.filler";
            Entry.ItemId = Item.Id;
            Entry.UnitPricePaise = 1000;
            Filler.Entries.Add(Entry);
            TestTrue(*FString::Printf(TEXT("filler %d fits"), Index), Fake->Buy(FName(TEXT("shop.filler")), FName(UTF8_TO_TCHAR(Item.Id.c_str())), 1, Fake->State.TransactionRevision).bAccepted);
        }
        CmBridge::Install(Fixture.World, Fake);
        Fixture.Press();
        TestTrue(TEXT("the core says the bag is full"), Fixture.Counter->GetLastError() == CommerceError::BagFull);
        TestTrue(TEXT("the notice says so"), Fixture.Notice().Contains(TEXT("bag is full")));
        TestFalse(TEXT("no panel"), Fixture.Counter->DidOpenPanel());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmCounterReleaseTest, "PerfectWorld.CommerceUi.Counter.AnAcceptedServiceIsReleasedWhenThePanelCannotOpen",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmCounterReleaseTest::RunTest(const FString& Parameters)
{
    FCmCounterFixture Fixture;
    if (!TestTrue(TEXT("fixture"), Fixture.Ok())) return false;
    const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
    CmBridge::Install(Fixture.World, Fake);
    // The world offers service; the automation world has no game viewport or controller, so the panel cannot open. The counter must not
    // leave the service held: it ends it, says so, and buys nothing.
    Fixture.Press();
    TestEqual(TEXT("service was requested once"), Fake->BeginCalls, 1);
    TestFalse(TEXT("the panel did not open"), Fixture.Counter->DidOpenPanel());
    TestEqual(TEXT("the accepted service was released once"), Fake->EndCalls, 1);
    TestTrue(TEXT("the player is told"), Fixture.Notice().Contains(TEXT("cannot browse the counter right now")));
    TestTrue(TEXT("the core had said yes"), Fixture.Counter->GetLastError() == CommerceError::None);
    TestEqual(TEXT("nothing was bought"), Fake->BuyCalls, 0);

    // A second press behaves the same: no state leaks between presses.
    Fixture.Press();
    TestEqual(TEXT("requested again"), Fake->BeginCalls, 2);
    TestEqual(TEXT("released again"), Fake->EndCalls, 2);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
