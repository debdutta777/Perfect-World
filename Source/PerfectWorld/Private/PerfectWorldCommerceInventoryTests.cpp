// R8-05 Inventory screen (UI/SPWInventoryPanel.h) and the HUD wallet pill (UI/SPWClockReadout.h) over the fake bridge: empty and filled
// states, Consume through the bridge with the core's answers, the unavailable state, fit at every window size and scale, and keys.
// Run in the editor under "PerfectWorld.CommerceUi.Inventory". The pause menu's Inventory entry is the integrator's hook (see the order
// report); this file proves the screen the entry opens.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldCommerceTestSupport.h"
#include "UI/SPWClockReadout.h"
#include "UI/SPWInventoryPanel.h"
#include "Widgets/Layout/SBox.h"

namespace
{
using namespace PWCommerce;
using namespace PWCommerceTest;
using PerfectWorld::CommerceError;

const FName CmInvTea(TEXT("item.tea"));
const FName CmInvBiscuits(TEXT("item.biscuits"));
const FName CmInvPlasters(TEXT("item.plasters"));

TSharedRef<SPWInventoryPanel> CmInvMakePanel(const TSharedRef<FCmFakeCommerce>& Fake, const FVector2D& Size, int32* Closed = nullptr)
{
    return SNew(SPWInventoryPanel)
        .Bridge(Fake)
        .Space(Size)
        .OnClose(FSimpleDelegate::CreateLambda([Closed]() { if (Closed != nullptr) ++(*Closed); }));
}

/** A real purchase through the fake (and so the core), for staging a bag. */
bool CmInvBuy(FCmFakeCommerce& Fake, const TCHAR* Shop, const FName Item, const int32 Quantity)
{
    return Fake.Buy(FName(Shop), Item, Quantity, Fake.State.TransactionRevision).bAccepted;
}

int32 CmInvCountEnabledUse(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    int32 Count = 0;
    CmWalk(Root, true, [&Count](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTag() == FName(PWInventory::UseTag) && Widget->IsEnabled()) ++Count;
    });
    return Count;
}

/** A bag with twelve different goods in it (and twelve receipts): the longest screen the Inventory can be. */
TSharedRef<FCmFakeCommerce> CmInvFullBag()
{
    TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(4);
    FCmFakeCommerce::FShop& Filler = Fake->Shops.AddDefaulted_GetRef();
    Filler.Id = FName(TEXT("shop.filler"));
    Filler.Name = FText::FromString(TEXT("Filler Stores"));
    for (int32 Index = 0; Index < 12; ++Index)
    {
        PerfectWorld::CommerceItem Item;
        Item.Id = std::string("item.filler") + std::to_string(Index);
        Item.Name = std::string("Filler Good Number ") + std::to_string(Index + 1);
        Item.Unit = "piece";
        Item.Effect = Index % 3 == 0 ? PerfectWorld::CommerceEffect::UnsupportedSupply : PerfectWorld::CommerceEffect::None;
        Fake->Items.Add(Item);
        PerfectWorld::CommerceCatalogueEntry Entry;
        Entry.ShopId = "shop.filler";
        Entry.ItemId = Item.Id;
        Entry.UnitPricePaise = 1000;
        Filler.Entries.Add(Entry);
        CmInvBuy(*Fake, TEXT("shop.filler"), FName(UTF8_TO_TCHAR(Item.Id.c_str())), 1);
    }
    return Fake;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmInvStatesTest, "PerfectWorld.CommerceUi.Inventory.EmptyFilledAndUnavailableStates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmInvStatesTest::RunTest(const FString& Parameters)
{
    {
        const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
        const TSharedRef<SPWInventoryPanel> Panel = CmInvMakePanel(Fake, FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = CmShownTexts(Panel);
        TestTrue(TEXT("the title and the wallet"), CmHas(Texts, TEXT("Inventory")) && CmHas(Texts, FString(TEXT("Wallet ")) + CmRs(15000)));
        TestTrue(TEXT("the bag count"), CmHas(Texts, TEXT("In your bag: 0 of 12 spaces")));
        TestEqual(TEXT("an empty bag says so"), CmCountTag(Panel, PWInventory::EmptyBagTag), 1);
        TestEqual(TEXT("no receipts says so"), CmCountTag(Panel, PWInventory::EmptyReceiptsTag), 1);
        TestEqual(TEXT("no rows"), CmCountTag(Panel, PWInventory::StackRowTag) + CmCountTag(Panel, PWInventory::ReceiptRowTag), 0);
        TestEqual(TEXT("no Consume buttons"), CmCountTag(Panel, PWInventory::UseTag), 0);
        TestEqual(TEXT("not the unavailable state"), CmCountTag(Panel, PWInventory::UnavailableTag), 0);
        TestEqual(TEXT("Back is offered"), CmCountTag(Panel, PWInventory::BackTag), 1);
        TestTrue(TEXT("both headings stay"), CmHas(Texts, TEXT("Recent receipts")));
    }
    {
        const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
        TestTrue(TEXT("two teas"), CmInvBuy(*Fake, TEXT("shop.shanti_chai"), CmInvTea, 2));
        TestTrue(TEXT("a pair of biscuits"), CmInvBuy(*Fake, TEXT("shop.shanti_chai"), CmInvBiscuits, 1));
        TestTrue(TEXT("a pack of plasters"), CmInvBuy(*Fake, TEXT("shop.stationery"), CmInvPlasters, 1));
        const TSharedRef<SPWInventoryPanel> Panel = CmInvMakePanel(Fake, FVector2D(1920.0, 1080.0));
        const TArray<FString> Texts = CmShownTexts(Panel);
        TestTrue(TEXT("the wallet after the three purchases (50 + 10 + 40 rupees)"), CmHas(Texts, FString(TEXT("Wallet ")) + CmRs(5000)));
        TestTrue(TEXT("three spaces used"), CmHas(Texts, TEXT("In your bag: 3 of 12 spaces")));
        TestEqual(TEXT("three stack rows"), CmCountTag(Panel, PWInventory::StackRowTag), 3);
        TestTrue(TEXT("tea with its unit and count"), CmHas(Texts, TEXT("Tea (cup) × 2")));
        TestTrue(TEXT("biscuits"), CmHas(Texts, TEXT("Biscuits (pair) × 1")));
        TestTrue(TEXT("plasters"), CmHas(Texts, TEXT("Plasters (pack of five) × 1")));
        TestEqual(TEXT("Consume only for tea and food"), CmCountTag(Panel, PWInventory::UseTag), 2);
        TestEqual(TEXT("and both can be used now"), CmInvCountEnabledUse(Panel), 2);
        TestTrue(TEXT("the supply says it has no use yet"), CmAnyContains(Texts, TEXT("no use yet")));
        TestEqual(TEXT("no empty states"), CmCountTag(Panel, PWInventory::EmptyBagTag) + CmCountTag(Panel, PWInventory::EmptyReceiptsTag), 0);
        TestEqual(TEXT("three receipts"), CmCountTag(Panel, PWInventory::ReceiptRowTag), 3);
        TArray<FString> Bought;
        for (const FString& Text : Texts)
        {
            if (Text.Contains(TEXT("bought"))) Bought.Add(Text);
        }
        if (TestEqual(TEXT("three purchase lines"), Bought.Num(), 3))
        {
            TestTrue(TEXT("newest first: plasters"), Bought[0].Contains(TEXT("Plasters")) && Bought[0].Contains(TEXT("Stationery")));
            TestTrue(TEXT("then biscuits"), Bought[1].Contains(TEXT("Biscuits")));
            TestTrue(TEXT("oldest last: two teas"), Bought[2].Contains(TEXT("2 × Tea")) && Bought[2].Contains(TEXT("Shanti Chai")));
        }
        TestEqual(TEXT("the model is the bridge's"), Panel->GetModel().Stacks.Num(), 3);
    }
    {
        const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
        Fake->bStateReadable = false;
        const TSharedRef<SPWInventoryPanel> Panel = CmInvMakePanel(Fake, FVector2D(1280.0, 720.0));
        const TArray<FString> Texts = CmShownTexts(Panel);
        TestEqual(TEXT("an unreadable purse says so"), CmCountTag(Panel, PWInventory::UnavailableTag), 1);
        TestEqual(TEXT("and shows no lists"), CmCountTag(Panel, PWInventory::EmptyBagTag) + CmCountTag(Panel, PWInventory::StackRowTag), 0);
        TestFalse(TEXT("and no wallet"), CmAnyContains(Texts, TEXT("Wallet")));
        TestEqual(TEXT("Back is still offered"), CmCountTag(Panel, PWInventory::BackTag), 1);
    }
    {
        const TSharedRef<SPWInventoryPanel> Panel = SNew(SPWInventoryPanel).Space(FVector2D(1280.0, 720.0));
        TestEqual(TEXT("no bridge at all reads as unavailable"), CmCountTag(Panel, PWInventory::UnavailableTag), 1);
        TestFalse(TEXT("and cannot use anything"), Panel->UseItem(CmInvTea));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmInvConsumeTest, "PerfectWorld.CommerceUi.Inventory.ConsumeGoesThroughTheBridgeAndShowsTheCoresAnswer",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmInvConsumeTest::RunTest(const FString& Parameters)
{
    const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
    CmInvBuy(*Fake, TEXT("shop.shanti_chai"), CmInvTea, 2);
    CmInvBuy(*Fake, TEXT("shop.shanti_chai"), CmInvBiscuits, 1);
    CmInvBuy(*Fake, TEXT("shop.stationery"), CmInvPlasters, 1);
    const TSharedRef<SPWInventoryPanel> Panel = CmInvMakePanel(Fake, FVector2D(1280.0, 720.0));
    const int32 Energy = Fake->Condition.Energy;
    const int32 Hunger = Fake->Condition.Hunger;

    // Tea: the button reaches the core, which adds its fixed energy and removes one cup.
    TestEqual(TEXT("no message before the first use"), CmCountTag(Panel, PWInventory::MessageTag), 0);
    TestTrue(TEXT("tea is used"), CmClickTag(Panel, PWInventory::UseTag));
    TestEqual(TEXT("one Consume reached the bridge"), Fake->ConsumeCalls, 1);
    TestEqual(TEXT("the core added 2000 Energy"), Fake->Condition.Energy, Energy + 2000);
    TestEqual(TEXT("and removed one cup"), Fake->State.Inventory[0].Quantity, 1);
    TestTrue(TEXT("the message says so"), Panel->GetMessage().ToString() == TEXT("Used Tea."));
    TestEqual(TEXT("and is shown"), CmCountTag(Panel, PWInventory::MessageTag), 1);
    TestTrue(TEXT("the screen reloaded: one cup left"), CmHas(CmShownTexts(Panel), TEXT("Tea (cup) × 1")));
    // The shared tea cooldown: the second cup is refused by the core, and the row says how long.
    TestEqual(TEXT("only biscuits can be used now"), CmInvCountEnabledUse(Panel), 1);
    TestTrue(TEXT("the tea row gives the core's wait"), CmAnyContains(CmShownTexts(Panel), TEXT("wait about 60 minutes")));
    TestFalse(TEXT("forcing the second cup"), Panel->UseItem(CmInvTea));
    TestEqual(TEXT("reached the core"), Fake->ConsumeCalls, 2);
    TestEqual(TEXT("and was refused: still one cup"), Fake->State.Inventory[0].Quantity, 1);
    TestEqual(TEXT("energy unchanged"), Fake->Condition.Energy, Energy + 2000);
    TestTrue(TEXT("the refusal says how long"), Panel->GetMessage().ToString().Contains(TEXT("60 minutes")));

    // Food: hunger drops by the authored amount and the emptied slot disappears.
    TestTrue(TEXT("biscuits are used"), Panel->UseItem(CmInvBiscuits));
    TestEqual(TEXT("the core eased hunger by 2000"), Fake->Condition.Hunger, Hunger - 2000);
    TestEqual(TEXT("two rows are left (tea and plasters)"), CmCountTag(Panel, PWInventory::StackRowTag), 2);
    TestFalse(TEXT("the biscuits are gone"), CmAnyContains(CmShownTexts(Panel), TEXT("Biscuits (pair) ×")));

    // A supply has no use: the core refuses and the screen says so; nothing changes.
    const uint64 Revision = Fake->State.TransactionRevision;
    TestFalse(TEXT("plasters cannot be used"), Panel->UseItem(CmInvPlasters));
    TestEqual(TEXT("the core changed nothing"), Fake->State.TransactionRevision, Revision);
    TestTrue(TEXT("the message says it has no use yet"), Panel->GetMessage().ToString().Contains(TEXT("no use yet")));

    // An active activity: the core refuses everything; the rows say so and no Consume button is enabled.
    Fake->UseContext.ActivityActive = true;
    TestFalse(TEXT("refused while busy"), Panel->UseItem(CmInvPlasters));
    TestEqual(TEXT("no Consume button is enabled"), CmInvCountEnabledUse(Panel), 0);
    TestTrue(TEXT("the rows ask the player to finish first"), CmAnyContains(CmShownTexts(Panel), TEXT("Finish what you are doing first")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmInvFitTest, "PerfectWorld.CommerceUi.Inventory.FitsEveryWindowAndScaleAndKeysWork",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmInvFitTest::RunTest(const FString& Parameters)
{
    const FVector2D Sizes[] = {FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0 / 1.5, 1080.0 / 1.5), FVector2D(1920.0, 1080.0)};
    const TSharedRef<FCmFakeCommerce> Full = CmInvFullBag();
    TestEqual(TEXT("twelve different goods"), Full->ReadInventory().Stacks.Num(), 12);
    for (const FVector2D& Size : Sizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
        const float W = static_cast<float>(Size.X);
        const float H = static_cast<float>(Size.Y);
        // The pause menu centres the panel in the screen; so does this wrapper.
        const TSharedRef<SPWInventoryPanel> Panel = CmInvMakePanel(Full, Size);
        const TSharedRef<SWidget> Hosted = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)[Panel];
        TestEqual(*(Name + TEXT(": twelve stack rows")), CmCountTag(Hosted, PWInventory::StackRowTag), 12);
        Hosted->SlatePrepass(1.0f);
        const FVector2D Desired(Panel->GetDesiredSize());
        TestTrue(*(Name + TEXT(": desired height fits")), Desired.Y <= PWInventory::PanelMaxHeight(W, H) + 0.5f);
        TestTrue(*(Name + TEXT(": desired width fits")), Desired.X <= FMath::Min(W - 32.0f, 640.0f) + 0.5f);
        FGeometry Card, Back, Body;
        if (TestTrue(*(Name + TEXT(": the card, the body and Back are laid out")),
                     CmArrangedTag(Hosted, Size, PWInventory::PanelTag, Card) && CmArrangedTag(Hosted, Size, PWInventory::BackTag, Back) &&
                     CmArrangedTag(Hosted, Size, PWInventory::BodyTag, Body)))
        {
            TestTrue(*(Name + TEXT(": the card is inside the screen")), CmInside(Card, FGeometry::MakeRoot(Size, FSlateLayoutTransform())));
            TestTrue(*(Name + TEXT(": Back is inside the card")), CmInside(Back, Card));
            TestTrue(*(Name + TEXT(": the body keeps room to scroll")), FVector2D(Body.GetAbsoluteSize()).Y >= 60.0f);
        }
        TestEqual(*(Name + TEXT(": compact below 760")), PWInventory::Compact(W, H), H < 760.0f || W < 760.0f);
    }

    int32 Closed = 0;
    const TSharedRef<SPWInventoryPanel> Panel = CmInvMakePanel(Full, FVector2D(853.0, 480.0), &Closed);
    Panel->SlatePrepass(1.0f);
    const FGeometry Geometry;
    TestTrue(TEXT("Escape is handled"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Escape)).IsEventHandled());
    TestEqual(TEXT("Escape goes back"), Closed, 1);
    TestTrue(TEXT("a held Escape is swallowed"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Escape, true)).IsEventHandled());
    TestEqual(TEXT("but goes back once"), Closed, 1);
    Panel->OnKeyDown(Geometry, CmKey(EKeys::Gamepad_FaceButton_Right));
    Panel->OnKeyDown(Geometry, CmKey(EKeys::Gamepad_Special_Right));
    TestEqual(TEXT("B and the menu button go back"), Closed, 3);
    for (const FKey& Key : {EKeys::PageUp, EKeys::PageDown, EKeys::Gamepad_RightStick_Up, EKeys::Gamepad_RightStick_Down})
    {
        TestTrue(*(Key.ToString() + TEXT(" scrolls (handled)")), Panel->OnKeyDown(Geometry, CmKey(Key)).IsEventHandled());
    }
    TestFalse(TEXT("Up is left to navigation between the buttons"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Up)).IsEventHandled());
    TestFalse(TEXT("Down is left to navigation"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Gamepad_DPad_Down)).IsEventHandled());
    TestEqual(TEXT("scrolling never closes"), Closed, 3);
    TestTrue(TEXT("the Back button goes back"), CmClickTag(Panel, PWInventory::BackTag));
    TestEqual(TEXT("and tells its owner"), Closed, 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmInvWalletPillTest, "PerfectWorld.CommerceUi.Inventory.TheHudWalletSitsBesideTheClockAndHidesWithIt",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmInvWalletPillTest::RunTest(const FString& Parameters)
{
    const FText Clock = FText::FromString(TEXT("Mon 2 Jun · 07:42"));
    const FText Wallet = FText::FromString(FString(TEXT("Wallet ")) + CmRs(12000));
    {
        const TSharedRef<SPWClockReadout> Readout = SNew(SPWClockReadout).TimeText(Clock).ShowReadout(true).WalletText(Wallet);
        const TArray<FString> Texts = CmShownTexts(Readout);
        TestTrue(TEXT("the clock and the wallet are both shown"), CmHas(Texts, Clock.ToString()) && CmHas(Texts, Wallet.ToString()));
        TestEqual(TEXT("the wallet comes before the clock, to its left"), Texts.IndexOfByKey(Wallet.ToString()), Texts.IndexOfByKey(Clock.ToString()) - 1);
        TestTrue(TEXT("the readout takes no input"), Readout->GetVisibility() == EVisibility::HitTestInvisible);
    }
    {
        const TSharedRef<SPWClockReadout> Readout = SNew(SPWClockReadout).TimeText(Clock).ShowReadout(true);
        const TArray<FString> Texts = CmShownTexts(Readout);
        TestTrue(TEXT("without a wallet only the clock shows"), CmHas(Texts, Clock.ToString()) && !CmAnyContains(Texts, TEXT("Wallet")));
    }
    {
        const TSharedRef<SPWClockReadout> Readout = SNew(SPWClockReadout).TimeText(Clock).ShowReadout(true).WalletText(FText::GetEmpty());
        TestFalse(TEXT("an empty wallet text hides the pill"), CmAnyContains(CmShownTexts(Readout), TEXT("Wallet")));
    }
    {
        const TSharedRef<SPWClockReadout> Readout = SNew(SPWClockReadout).TimeText(Clock).ShowReadout(false).WalletText(Wallet);
        const TArray<FString> Texts = CmShownTexts(Readout);
        TestFalse(TEXT("under a menu neither the clock nor the wallet shows"), CmHas(Texts, Clock.ToString()) || CmHas(Texts, Wallet.ToString()));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
