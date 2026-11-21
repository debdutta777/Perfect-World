// R8-05 buy panel (UI/SPWBuyPanel.h) over the fake bridge: its states and figures, the precise refusals, the guard against an accidental
// confirmation, the single submission, keys, and its fit at every window size and UI scale. Run in the editor under
// "PerfectWorld.CommerceUi.BuyPanel". The widgets are the real ones, built without a window; focus and the real interact key are proved by
// the commerce acceptance scenario (R8-06), not here.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldCommerceTestSupport.h"
#include "UI/PWWidgets.h"
#include "UI/SPWBuyPanel.h"

namespace
{
using namespace PWCommerce;
using namespace PWCommerceTest;
using PerfectWorld::CommerceError;

const FName CmTeaShop(TEXT("shop.shanti_chai"));
const FName CmStationery(TEXT("shop.stationery"));

/** A panel over the fake with a clock the test owns: armed from T+0.5 s. */
TSharedRef<SPWBuyPanel> CmMakePanel(const TSharedRef<FCmFakeCommerce>& Fake, const FName Shop, const FVector2D& Size, double& Now, int32* Closed = nullptr)
{
    const FCmServiceView Offer = Fake->BeginService(Shop);
    const TSharedRef<SPWBuyPanel> Panel = SNew(SPWBuyPanel)
        .Bridge(Fake)
        .Service(Offer)
        .Space(Size)
        .OnClose(FSimpleDelegate::CreateLambda([Closed]() { if (Closed != nullptr) ++(*Closed); }));
    Panel->SetClockForTest([&Now]() { return Now; });
    return Panel;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmBuyStatesTest, "PerfectWorld.CommerceUi.BuyPanel.ShowsGoodsQuantityAndTotals",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmBuyStatesTest::RunTest(const FString& Parameters)
{
    double Now = 0.0;
    const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1); // wallet 150 rupees
    const TSharedRef<SPWBuyPanel> Panel = CmMakePanel(Fake, CmTeaShop, FVector2D(1280.0, 720.0), Now);
    const TArray<FString> Texts = CmShownTexts(Panel);
    TestTrue(TEXT("the shop's name"), CmHas(Texts, TEXT("Shanti Chai")));
    TestTrue(TEXT("the wallet in the header"), CmHas(Texts, FString(TEXT("Wallet ")) + CmRs(15000)));
    TestEqual(TEXT("two goods, two rows"), CmCountTag(Panel, PWBuy::GoodRowTag), 2);
    TestTrue(TEXT("tea with its unit"), CmHas(Texts, TEXT("Tea (cup)")) && CmHas(Texts, CmRs(2500)));
    TestTrue(TEXT("biscuits with their unit"), CmHas(Texts, TEXT("Biscuits (pair)")) && CmHas(Texts, CmRs(1000)));
    TestTrue(TEXT("the quantity, one"), CmHas(Texts, TEXT("Quantity")) && CmHas(Texts, TEXT("1")));
    TestTrue(TEXT("the unit price with its unit"), CmHas(Texts, CmRs(2500) + TEXT(" per cup")));
    TestTrue(TEXT("the total"), CmHas(Texts, TEXT("Total")));
    TestTrue(TEXT("the wallet now, and after"), CmHas(Texts, TEXT("Wallet now")) && CmHas(Texts, TEXT("Wallet after")) && CmHas(Texts, CmRs(12500)));
    TestEqual(TEXT("Confirm and Back are offered"), CmCountTag(Panel, PWBuy::ConfirmTag) + CmCountTag(Panel, PWBuy::BackTag), 2);
    TestEqual(TEXT("no explanation is needed"), CmCountTag(Panel, PWBuy::StatusTag), 0);
    TestEqual(TEXT("no receipt yet"), CmCountTag(Panel, PWBuy::ReceiptTag), 0);
    TestFalse(TEXT("the receipt heading is hidden"), CmHas(Texts, TEXT("Purchase complete")));
    TestTrue(TEXT("Confirm can be pressed (once armed)"), CmIsEnabledTag(Panel, PWBuy::ConfirmTag));
    TestTrue(TEXT("the panel starts on goods"), Panel->GetStage() == PWBuy::EStage::Browsing && Panel->GetSelectedIndex() == 0 && Panel->GetQuantity() == 1);
    TestEqual(TEXT("the quote is the core's"), Panel->GetQuote().Quote.TotalPaise, static_cast<int64>(2500));

    // Focus starts on the first item, never on Confirm: the first focusable widget under the panel is a goods row.
    const TSharedPtr<SWidget> Card = CmFindTag(Panel, PWBuy::PanelTag);
    const TSharedPtr<SWidget> FirstFocus = Card.IsValid() ? PWUI::FindFirstFocusable(Card.ToSharedRef()) : nullptr;
    TestTrue(TEXT("the first focusable control is a goods row"), FirstFocus.IsValid() && FirstFocus->GetTag() == FName(PWBuy::GoodRowTag));

    // Quantity: the stepper asks the bridge again and the figures follow the core's quote.
    const int32 QuotesBefore = Fake->QuoteCalls;
    Panel->StepQuantity(2);
    TestEqual(TEXT("quantity 3"), Panel->GetQuantity(), 3);
    TestTrue(TEXT("a new quote was asked of the bridge"), Fake->QuoteCalls > QuotesBefore);
    const TArray<FString> Three = CmShownTexts(Panel);
    TestTrue(TEXT("total 75, after 75"), CmHas(Three, CmRs(7500)) && Panel->GetQuote().Quote.WalletAfterPaise == 7500);
    TestEqual(TEXT("below one is refused by the stepper"), (Panel->StepQuantity(-9), Panel->GetQuantity()), 1);
    TestEqual(TEXT("the stepper stops at one stack"), (Panel->StepQuantity(99), Panel->GetQuantity()), PWBuy::MaxQuantity);

    // Ten teas cost more than the wallet: the core's quote goes negative and the panel says exactly how much more.
    TestEqual(TEXT("250 rupees"), Panel->GetQuote().Quote.TotalPaise, static_cast<int64>(25000));
    TestTrue(TEXT("the shortfall is explained"), Panel->GetStatusText().ToString() == CmShortfallText(-10000).ToString());
    TestEqual(TEXT("in a status line"), CmCountTag(Panel, PWBuy::StatusTag), 1);
    TestTrue(TEXT("the negative wallet-after is shown"), CmHas(CmShownTexts(Panel), CmRs(-10000)));
    TestFalse(TEXT("Confirm is disabled"), CmIsEnabledTag(Panel, PWBuy::ConfirmTag));
    Now = 5.0;
    TestFalse(TEXT("and does nothing even when armed"), Panel->Confirm());
    TestEqual(TEXT("the bridge was never asked to buy"), Fake->BuyCalls, 0);

    // Choosing another item resets the quantity and quotes that item.
    Panel->SelectGood(1);
    TestEqual(TEXT("biscuits"), Panel->GetSelectedIndex(), 1);
    TestEqual(TEXT("quantity back to one"), Panel->GetQuantity(), 1);
    TestEqual(TEXT("a biscuit pair is 10 rupees"), Panel->GetQuote().Quote.TotalPaise, static_cast<int64>(1000));
    TestEqual(TEXT("no status again"), CmCountTag(Panel, PWBuy::StatusTag), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmBuyRefusalsTest, "PerfectWorld.CommerceUi.BuyPanel.ShowsEachRefusalPrecisely",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmBuyRefusalsTest::RunTest(const FString& Parameters)
{
    struct FCase
    {
        const TCHAR* Name;
        TFunction<void(FCmFakeCommerce&)> Stage;
        const TCHAR* Phrase;
    };
    const FCase Cases[] = {
        {TEXT("closed"), [](FCmFakeCommerce& F) { F.Context.ShopOpen = false; }, TEXT("closed")},
        {TEXT("keeper away"), [](FCmFakeCommerce& F) { F.Context.KeeperPresent = false; }, TEXT("away")},
        {TEXT("finishing a step"), [](FCmFakeCommerce& F) { F.Context.AtSafePause = false; }, TEXT("middle of something")},
        {TEXT("not at the counter"), [](FCmFakeCommerce& F) { F.Context.AtCounter = false; }, TEXT("counter")},
        {TEXT("asleep"), [](FCmFakeCommerce& F) { F.Context.PlayerAwake = false; }, TEXT("asleep")},
        {TEXT("busy"), [](FCmFakeCommerce& F) { F.Context.PlayerFree = false; }, TEXT("busy")},
        {TEXT("unsettled"), [](FCmFakeCommerce& F) { F.Context.WorldSettled = false; }, TEXT("still moving")},
    };
    for (const FCase& Case : Cases)
    {
        double Now = 0.0;
        const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
        const TSharedRef<SPWBuyPanel> Panel = CmMakePanel(Fake, CmTeaShop, FVector2D(1280.0, 720.0), Now);
        TestTrue(*(FString(Case.Name) + TEXT(": the offer was open when the panel appeared")), Panel->GetQuote().CanBuy());
        Case.Stage(*Fake); // the world changes while the player is deciding
        Panel->StepQuantity(1); // the next quote carries the core's new verdict
        const FString Status = Panel->GetStatusText().ToString();
        TestTrue(*(FString(Case.Name) + TEXT(": the status says why (") + Status + TEXT(")")), Status.Contains(Case.Phrase));
        TestEqual(*(FString(Case.Name) + TEXT(": shown in the status line")), CmCountTag(Panel, PWBuy::StatusTag), 1);
        TestFalse(*(FString(Case.Name) + TEXT(": Confirm is disabled")), CmIsEnabledTag(Panel, PWBuy::ConfirmTag));
        Now = 10.0;
        TestFalse(*(FString(Case.Name) + TEXT(": and cannot be forced")), Panel->Confirm());
        TestEqual(*(FString(Case.Name) + TEXT(": nothing was submitted")), Fake->BuyCalls, 0);
        TestEqual(*(FString(Case.Name) + TEXT(": the wallet is untouched")), Fake->State.WalletPaise, static_cast<int64>(15000));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmBuyGuardTest, "PerfectWorld.CommerceUi.BuyPanel.AnAccidentalConfirmCannotBuy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmBuyGuardTest::RunTest(const FString& Parameters)
{
    double Now = 0.0;
    int32 Closed = 0;
    const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
    const TSharedRef<SPWBuyPanel> Panel = CmMakePanel(Fake, CmTeaShop, FVector2D(1280.0, 720.0), Now, &Closed);

    // The moment the panel opens, the key that opened it (or a held confirm key) must not buy.
    TestFalse(TEXT("not armed at once"), Panel->IsConfirmArmed());
    TestTrue(TEXT("the button is enabled for the eye, the guard is behind it"), CmIsEnabledTag(Panel, PWBuy::ConfirmTag));
    TestTrue(TEXT("a press at once is swallowed"), CmClickTag(Panel, PWBuy::ConfirmTag));
    Now = PWBuy::ConfirmArmSeconds * 0.9;
    TestFalse(TEXT("still not armed at 90% of the window"), Panel->IsConfirmArmed());
    TestFalse(TEXT("so Confirm refuses"), Panel->Confirm());
    TestEqual(TEXT("nothing reached the bridge"), Fake->BuyCalls, 0);
    TestEqual(TEXT("nothing was spent"), Fake->State.WalletPaise, static_cast<int64>(15000));
    TestTrue(TEXT("and still goods"), Panel->GetStage() == PWBuy::EStage::Browsing);

    // The keyboard never confirms by itself: Enter is the focused button's business (a real button press), not the panel's.
    TestFalse(TEXT("Enter on the panel is not handled"), Panel->OnKeyDown(FGeometry(), CmKey(EKeys::Enter)).IsEventHandled());
    TestFalse(TEXT("Space on the panel is not handled"), Panel->OnKeyDown(FGeometry(), CmKey(EKeys::SpaceBar)).IsEventHandled());
    TestFalse(TEXT("the controller's A on the panel is not handled"), Panel->OnKeyDown(FGeometry(), CmKey(EKeys::Gamepad_FaceButton_Bottom)).IsEventHandled());
    TestEqual(TEXT("so the bridge still has not been asked"), Fake->BuyCalls, 0);

    // A distinct press after the window: exactly one submission.
    Now = PWBuy::ConfirmArmSeconds + 0.1;
    TestTrue(TEXT("armed"), Panel->IsConfirmArmed() && Panel->CanConfirmNow());
    TestTrue(TEXT("the press submits"), CmClickTag(Panel, PWBuy::ConfirmTag));
    TestEqual(TEXT("one Buy reached the bridge"), Fake->BuyCalls, 1);
    TestEqual(TEXT("the panel counted one submission"), Panel->GetSubmitCount(), 1);
    TestTrue(TEXT("the receipt is shown"), Panel->GetStage() == PWBuy::EStage::Receipt);
    TestEqual(TEXT("the core debited 25 rupees"), Fake->State.WalletPaise, static_cast<int64>(12500));
    TestEqual(TEXT("and delivered a cup of tea"), Fake->State.Inventory[0].Quantity, 1);
    TestEqual(TEXT("the receipt is the core's"), Panel->GetReceipt().Receipt.TotalPaise, static_cast<int64>(2500));
    TestEqual(TEXT("with its revision"), Panel->GetReceipt().Receipt.Revision, static_cast<uint64>(1));
    const TArray<FString> Texts = CmShownTexts(Panel);
    TestTrue(TEXT("the receipt heading"), CmHas(Texts, TEXT("Purchase complete")));
    TestEqual(TEXT("the receipt line"), CmCountTag(Panel, PWBuy::ReceiptTag), 1);
    TestTrue(TEXT("naming the purchase and both balances"), CmAnyContains(Texts, TEXT("bought 1")) && CmAnyContains(Texts, TEXT("Tea")) &&
             CmAnyContains(Texts, CmRs(15000)) && CmAnyContains(Texts, CmRs(12500)));
    TestTrue(TEXT("the header wallet follows"), CmHas(Texts, FString(TEXT("Wallet ")) + CmRs(12500)));
    TestEqual(TEXT("Confirm is gone"), CmCountTag(Panel, PWBuy::ConfirmTag), 0);
    TestEqual(TEXT("Buy more and Done are offered"), CmCountTag(Panel, PWBuy::MoreTag) + CmCountTag(Panel, PWBuy::DoneTag), 2);

    // Repeats of the same press (a held key, a second click) cannot buy again.
    Now += 5.0;
    TestFalse(TEXT("a second Confirm does nothing"), Panel->Confirm());
    TestEqual(TEXT("still one Buy"), Fake->BuyCalls, 1);
    TestEqual(TEXT("still one cup"), Fake->State.Inventory[0].Quantity, 1);

    // Buy more re-arms the guard, then a second purchase works.
    Panel->BuyMore();
    TestTrue(TEXT("back to goods"), Panel->GetStage() == PWBuy::EStage::Browsing && Panel->GetQuantity() == 1);
    TestTrue(TEXT("the wallet now is the new balance"), CmHas(CmShownTexts(Panel), CmRs(12500)) && Panel->GetQuote().Quote.WalletBeforePaise == 12500);
    TestFalse(TEXT("not armed again at once"), Panel->Confirm());
    Now += PWBuy::ConfirmArmSeconds + 0.1;
    TestTrue(TEXT("armed after the window"), Panel->Confirm());
    TestEqual(TEXT("a second purchase went through"), Fake->BuyCalls, 2);
    TestEqual(TEXT("the wallet is now 100 rupees"), Fake->State.WalletPaise, static_cast<int64>(10000));

    TestTrue(TEXT("Done closes"), CmClickTag(Panel, PWBuy::DoneTag));
    TestEqual(TEXT("the owner was told once"), Closed, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmBuyRefusedConfirmTest, "PerfectWorld.CommerceUi.BuyPanel.ARefusedConfirmChangesNothingAndExplains",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmBuyRefusedConfirmTest::RunTest(const FString& Parameters)
{
    double Now = 0.0;
    const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(1);
    const TSharedRef<SPWBuyPanel> Panel = CmMakePanel(Fake, CmTeaShop, FVector2D(1280.0, 720.0), Now);

    // The world refuses the confirmation (the core's own refusal, here forced).
    Now = 1.0;
    Fake->ForceBuyError = CommerceError::NotAtSafePause;
    TestTrue(TEXT("the bridge was asked"), Panel->Confirm());
    TestEqual(TEXT("once"), Fake->BuyCalls, 1);
    TestTrue(TEXT("no receipt, still goods"), Panel->GetStage() == PWBuy::EStage::Browsing);
    TestEqual(TEXT("nothing spent"), Fake->State.WalletPaise, static_cast<int64>(15000));
    TestTrue(TEXT("the reason is on screen"), Panel->GetStatusText().ToString().Contains(TEXT("middle of something")));
    TestFalse(TEXT("the guard re-arms: an immediate second press does nothing"), Panel->Confirm());
    TestEqual(TEXT("still one Buy"), Fake->BuyCalls, 1);

    // A really stale quote: another purchase moves the revision between the quote and the confirmation. The core refuses, the panel
    // explains and takes a fresh quote, and a distinct press then succeeds.
    Now = 3.0;
    const FCmBuyResult Other = Fake->Buy(CmTeaShop, FName(TEXT("item.biscuits")), 1, Fake->State.TransactionRevision);
    TestTrue(TEXT("the other purchase went through"), Other.bAccepted);
    TestTrue(TEXT("the panel's quote is now stale"), Panel->GetQuote().Quote.ExpectedRevision != Fake->State.TransactionRevision);
    const int32 BuysBefore = Fake->BuyCalls;
    TestTrue(TEXT("a confirmation is submitted"), Panel->Confirm());
    TestEqual(TEXT("and refused by the core"), Fake->BuyCalls, BuysBefore + 1);
    TestTrue(TEXT("the panel says something changed"), Panel->GetStatusText().ToString().Contains(TEXT("Something changed")));
    TestEqual(TEXT("only the other purchase was debited"), Fake->State.WalletPaise, static_cast<int64>(14000));
    TestEqual(TEXT("the panel re-quoted at the new revision"), Panel->GetQuote().Quote.ExpectedRevision, Fake->State.TransactionRevision);
    TestTrue(TEXT("and can buy again"), Panel->GetQuote().CanBuy());
    Now = 5.0;
    TestTrue(TEXT("a distinct press buys"), Panel->Confirm());
    TestTrue(TEXT("with a receipt"), Panel->GetStage() == PWBuy::EStage::Receipt);
    TestEqual(TEXT("one tea and one biscuit pair, 35 rupees in all"), Fake->State.WalletPaise, static_cast<int64>(11500));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmBuyKeysTest, "PerfectWorld.CommerceUi.BuyPanel.KeysAdjustQuantityAndGoBack",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmBuyKeysTest::RunTest(const FString& Parameters)
{
    double Now = 0.0;
    int32 Closed = 0;
    const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(4);
    const TSharedRef<SPWBuyPanel> Panel = CmMakePanel(Fake, CmTeaShop, FVector2D(853.0, 480.0), Now, &Closed);
    const FGeometry Geometry;

    for (const FKey& Key : {EKeys::Right, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right, EKeys::Equals, EKeys::Add, EKeys::Gamepad_RightShoulder})
    {
        const int32 Before = Panel->GetQuantity();
        TestTrue(*(Key.ToString() + TEXT(" is handled")), Panel->OnKeyDown(Geometry, CmKey(Key)).IsEventHandled());
        TestEqual(*(Key.ToString() + TEXT(" raises the quantity")), Panel->GetQuantity(), Before + 1);
    }
    for (const FKey& Key : {EKeys::Left, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left, EKeys::Hyphen, EKeys::Subtract, EKeys::Gamepad_LeftShoulder})
    {
        const int32 Before = Panel->GetQuantity();
        TestTrue(*(Key.ToString() + TEXT(" is handled")), Panel->OnKeyDown(Geometry, CmKey(Key)).IsEventHandled());
        TestEqual(*(Key.ToString() + TEXT(" lowers the quantity")), Panel->GetQuantity(), Before - 1);
    }
    TestEqual(TEXT("back at the start"), Panel->GetQuantity(), 1);
    TestTrue(TEXT("the quantity never goes below one"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Left)).IsEventHandled() && Panel->GetQuantity() == 1);
    // Up and Down belong to Slate's navigation between the goods and the controls.
    TestFalse(TEXT("Up is left to navigation"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Up)).IsEventHandled());
    TestFalse(TEXT("Down is left to navigation"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Down)).IsEventHandled());
    TestFalse(TEXT("D-pad down is left to navigation"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Gamepad_DPad_Down)).IsEventHandled());
    TestEqual(TEXT("navigation changes no quantity and buys nothing"), Fake->BuyCalls, 0);

    TestTrue(TEXT("Escape is handled"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Escape)).IsEventHandled());
    TestEqual(TEXT("Escape goes back"), Closed, 1);
    TestTrue(TEXT("a held Escape is swallowed"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Escape, true)).IsEventHandled());
    TestEqual(TEXT("but goes back once"), Closed, 1);
    Panel->OnKeyDown(Geometry, CmKey(EKeys::Gamepad_FaceButton_Right));
    TestEqual(TEXT("the controller's B goes back"), Closed, 2);
    Panel->OnKeyDown(Geometry, CmKey(EKeys::Gamepad_Special_Right));
    TestEqual(TEXT("the controller's menu button goes back"), Closed, 3);
    TestTrue(TEXT("the Back button goes back"), CmClickTag(Panel, PWBuy::BackTag));
    TestEqual(TEXT("and tells its owner"), Closed, 4);

    // On the receipt, sideways keys belong to navigation between Buy more and Done.
    Now = 2.0;
    TestTrue(TEXT("buy"), Panel->Confirm());
    TestFalse(TEXT("Right on the receipt is left to navigation"), Panel->OnKeyDown(Geometry, CmKey(EKeys::Right)).IsEventHandled());
    TestEqual(TEXT("and changes nothing"), Panel->GetQuantity(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmBuyFitTest, "PerfectWorld.CommerceUi.BuyPanel.FitsEveryWindowAndScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmBuyFitTest::RunTest(const FString& Parameters)
{
    // 853x480 and 1280x720 are 1280x720 and 1920x1080 windows at 150% UI scale (and so 150% text); 1280x720 and 1920x1080 are at 100%.
    const FVector2D Sizes[] = {FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0 / 1.5, 1080.0 / 1.5), FVector2D(1920.0, 1080.0)};
    for (const FVector2D& Size : Sizes)
    {
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Size.X, Size.Y);
        const float W = static_cast<float>(Size.X);
        const float H = static_cast<float>(Size.Y);
        double Now = 0.0;

        // The hardest browsing state: a long list, the stepper at ten, and a two-line shortfall explanation showing.
        {
            const TSharedRef<FCmFakeCommerce> Fake = CmFakeWithLongStationery(1, 7);
            const TSharedRef<SPWBuyPanel> Panel = CmMakePanel(Fake, CmStationery, Size, Now);
            Panel->SelectGood(1);
            Panel->StepQuantity(9);
            TestEqual(*(Name + TEXT(": the shortfall is showing")), CmCountTag(Panel, PWBuy::StatusTag), 1);
            TestEqual(*(Name + TEXT(": ten goods in the list")), CmCountTag(Panel, PWBuy::GoodRowTag), 10);
            Panel->SlatePrepass(1.0f);
            const FVector2D Desired(Panel->GetDesiredSize());
            AddInfo(FString::Printf(TEXT("%s: buy panel desires %.0f x %.0f of %.0f x %.0f"), *Name, Desired.X, Desired.Y, W, H));
            TestTrue(*(Name + TEXT(": the panel fits the height")), Desired.Y <= H + 0.5f);
            FGeometry Card, Confirm, Back, Goods;
            if (TestTrue(*(Name + TEXT(": the card, Confirm, Back and the list are laid out")),
                         CmArrangedTag(Panel, Size, PWBuy::PanelTag, Card) && CmArrangedTag(Panel, Size, PWBuy::ConfirmTag, Confirm) &&
                         CmArrangedTag(Panel, Size, PWBuy::BackTag, Back) && CmArrangedTag(Panel, Size, PWBuy::GoodsTag, Goods)))
            {
                const FGeometry Screen = FGeometry::MakeRoot(Size, FSlateLayoutTransform());
                TestTrue(*(Name + TEXT(": the card is inside the screen")), CmInside(Card, Screen));
                TestTrue(*(Name + TEXT(": the card leaves the screen margin")), FVector2D(Card.GetAbsoluteSize()).Y <= PWBuy::PanelMaxHeight(W, H) + 0.5f &&
                         FVector2D(Card.GetAbsoluteSize()).X <= PWBuy::PanelWidth(W, H) + 0.5f);
                TestTrue(*(Name + TEXT(": Confirm is inside the card")), CmInside(Confirm, Card));
                TestTrue(*(Name + TEXT(": Back is inside the card")), CmInside(Back, Card));
                TestTrue(*(Name + TEXT(": at least one row of the list stays visible")), FVector2D(Goods.GetAbsoluteSize()).Y >= 44.0f);
                TestTrue(*(Name + TEXT(": the list is above the buttons")), FVector2D(Goods.GetAbsolutePosition()).Y + FVector2D(Goods.GetAbsoluteSize()).Y <= FVector2D(Confirm.GetAbsolutePosition()).Y + 0.5f);
            }
            TestEqual(*(Name + TEXT(": compact below 760")), PWBuy::Compact(W, H), H < 760.0f || W < 760.0f);
        }

        // The receipt state.
        {
            const TSharedRef<FCmFakeCommerce> Fake = CmFakeWithLongStationery(1, 7);
            const TSharedRef<SPWBuyPanel> Panel = CmMakePanel(Fake, CmStationery, Size, Now);
            Now += 1.0;
            TestTrue(*(Name + TEXT(": a purchase")), Panel->Confirm());
            FGeometry Card, More, Done, Receipt;
            if (TestTrue(*(Name + TEXT(": the receipt state is laid out")),
                         CmArrangedTag(Panel, Size, PWBuy::PanelTag, Card) && CmArrangedTag(Panel, Size, PWBuy::MoreTag, More) &&
                         CmArrangedTag(Panel, Size, PWBuy::DoneTag, Done) && CmArrangedTag(Panel, Size, PWBuy::ReceiptTag, Receipt)))
            {
                TestTrue(*(Name + TEXT(": the receipt is inside the card")), CmInside(Receipt, Card));
                TestTrue(*(Name + TEXT(": Buy more is inside the card")), CmInside(More, Card));
                TestTrue(*(Name + TEXT(": Done is inside the card")), CmInside(Done, Card));
            }
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
