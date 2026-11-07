// R8-05 commerce words and number formats (PerfectWorldCommerceText.h): price formatting from integer paise, a specific sentence for every
// core refusal, the receipt lines and the wallet readouts. Run in the editor under "PerfectWorld.CommerceUi.Text".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "GameClock.h"
#include "PerfectWorldCommerceTestSupport.h"

namespace
{
using namespace PWCommerce;
using namespace PWCommerceTest;
using PerfectWorld::CommerceError;

const TCHAR* const CmRupee = TEXT("₹");
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmUiPriceFormatTest, "PerfectWorld.CommerceUi.Text.PricesFormatFromPaise",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmUiPriceFormatTest::RunTest(const FString& Parameters)
{
    const auto Rs = [](const TCHAR* Rest) { return FString(CmRupee) + Rest; };
    TestEqual(TEXT("zero"), CmFormatPaise(0), Rs(TEXT("0")));
    TestEqual(TEXT("whole rupees drop the paise"), CmFormatPaise(2500), Rs(TEXT("25")));
    TestEqual(TEXT("tea is 25 rupees"), CmFormatPaise(CmRupees(25)), Rs(TEXT("25")));
    TestEqual(TEXT("half rupees show two digits"), CmFormatPaise(2550), Rs(TEXT("25.50")));
    TestEqual(TEXT("a single paise digit is padded"), CmFormatPaise(2505), Rs(TEXT("25.05")));
    TestEqual(TEXT("under a rupee"), CmFormatPaise(5), Rs(TEXT("0.05")));
    TestEqual(TEXT("the rank 0 wallet"), CmFormatPaise(12000), Rs(TEXT("120")));
    TestEqual(TEXT("999"), CmFormatPaise(99900), Rs(TEXT("999")));
    TestEqual(TEXT("a thousand groups"), CmFormatPaise(120000), Rs(TEXT("1,200")));
    TestEqual(TEXT("Indian grouping: lakh"), CmFormatPaise(CmRupees(120000)), Rs(TEXT("1,20,000")));
    TestEqual(TEXT("Indian grouping: ten lakh"), CmFormatPaise(CmRupees(1000000)), Rs(TEXT("10,00,000")));
    TestEqual(TEXT("a negative wallet-after"), CmFormatPaise(-500), FString(TEXT("-")) + Rs(TEXT("5")));
    TestEqual(TEXT("a negative with paise"), CmFormatPaise(-505), FString(TEXT("-")) + Rs(TEXT("5.05")));
    // The extremes never overflow or crash.
    TestTrue(TEXT("the largest amount formats"), CmFormatPaise(MAX_int64).StartsWith(CmRupee));
    TestTrue(TEXT("the smallest amount formats with a sign"), CmFormatPaise(MIN_int64).StartsWith(FString(TEXT("-")) + CmRupee));
    TestFalse(TEXT("and with digits"), CmFormatPaise(MIN_int64).Contains(TEXT("-9223372036854775808")));

    TestEqual(TEXT("unit price"), CmUnitPriceText(2500, FText::FromString(TEXT("cup"))).ToString(), Rs(TEXT("25 per cup")));
    TestEqual(TEXT("a price with no unit is just the price"), CmUnitPriceText(2500, FText::GetEmpty()).ToString(), Rs(TEXT("25")));
    TestEqual(TEXT("name and unit"), CmItemWithUnit(FText::FromString(TEXT("Tea")), FText::FromString(TEXT("cup"))).ToString(), FString(TEXT("Tea (cup)")));
    TestEqual(TEXT("a name with no unit"), CmItemWithUnit(FText::FromString(TEXT("Tea")), FText::GetEmpty()).ToString(), FString(TEXT("Tea")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmUiRefusalTextTest, "PerfectWorld.CommerceUi.Text.EveryRefusalHasItsOwnWords",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmUiRefusalTextTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("success has no refusal text"), CmRefusalText(CommerceError::None).IsEmpty());
    // Every code the UI can meet: all have words, and the ones a player must tell apart each have different words.
    const CommerceError All[] = {CommerceError::InvalidState, CommerceError::InvalidRank, CommerceError::InvalidTime, CommerceError::InvalidItem,
        CommerceError::InvalidQuantity, CommerceError::ArithmeticOverflow, CommerceError::Closed, CommerceError::KeeperAway, CommerceError::NotAtSafePause,
        CommerceError::NotAtCounter, CommerceError::PlayerAsleep, CommerceError::PlayerBusy, CommerceError::WorldUnsettled, CommerceError::InsufficientFunds,
        CommerceError::BagFull, CommerceError::StackFull, CommerceError::StaleRevision, CommerceError::NotOwned, CommerceError::UseUnavailable, CommerceError::Cooldown};
    for (const CommerceError Error : All)
    {
        const FString Text = CmRefusalText(Error).ToString();
        const FString Code = FString(UTF8_TO_TCHAR(std::string(PerfectWorld::CommerceErrorCode(Error)).c_str()));
        TestFalse(*(Code + TEXT(": has words")), Text.IsEmpty());
        TestFalse(*(Code + TEXT(": never shows a code")), Text.Contains(Code) || Text.Contains(TEXT("commerce.")));
    }
    const CommerceError Distinct[] = {CommerceError::Closed, CommerceError::KeeperAway, CommerceError::NotAtSafePause, CommerceError::NotAtCounter,
        CommerceError::PlayerAsleep, CommerceError::PlayerBusy, CommerceError::WorldUnsettled, CommerceError::InsufficientFunds, CommerceError::BagFull,
        CommerceError::StackFull, CommerceError::StaleRevision, CommerceError::InvalidItem, CommerceError::NotOwned, CommerceError::UseUnavailable,
        CommerceError::Cooldown, CommerceError::InvalidState};
    TSet<FString> Seen;
    for (const CommerceError Error : Distinct)
    {
        const FString Text = CmRefusalText(Error).ToString();
        TestFalse(*(Text + TEXT(": is its own sentence")), Seen.Contains(Text));
        Seen.Add(Text);
    }

    // The five reasons the counter must give precisely.
    const FText Shop = FText::FromString(TEXT("Shanti Chai"));
    TestTrue(TEXT("closed names the shop and says closed"), CmRefusalText(CommerceError::Closed, Shop).ToString().Contains(TEXT("Shanti Chai is closed")));
    TestTrue(TEXT("closed, unnamed"), CmRefusalText(CommerceError::Closed).ToString().Contains(TEXT("closed")));
    TestTrue(TEXT("keeper away"), CmRefusalText(CommerceError::KeeperAway).ToString().Contains(TEXT("away")));
    TestTrue(TEXT("finishing a step"), CmRefusalText(CommerceError::NotAtSafePause).ToString().Contains(TEXT("middle of something")));
    TestTrue(TEXT("not enough money"), CmRefusalText(CommerceError::InsufficientFunds).ToString().Contains(TEXT("enough money")));
    TestTrue(TEXT("bag full"), CmRefusalText(CommerceError::BagFull).ToString().Contains(TEXT("bag is full")));

    TestEqual(TEXT("the shortfall names how much more is needed"), CmShortfallText(-500).ToString(),
              FString(TEXT("You do not have enough money. You need ")) + CmRupee + TEXT("5 more."));
    TestEqual(TEXT("a shortfall of paise"), CmShortfallText(-2550).ToString(), FString(TEXT("You do not have enough money. You need ")) + CmRupee + TEXT("25.50 more."));
    TestEqual(TEXT("no shortfall reads the plain refusal"), CmShortfallText(0).ToString(), CmRefusalText(CommerceError::InsufficientFunds).ToString());
    TestTrue(TEXT("the most negative shortfall does not overflow"), CmShortfallText(MIN_int64).ToString().Contains(CmRupee));

    TestTrue(TEXT("a cooldown with a wait says how long (plural)"), CmUseRefusalText(CommerceError::Cooldown, 45).ToString().Contains(TEXT("45 minutes")));
    TestTrue(TEXT("a cooldown with a wait says how long (singular)"), CmUseRefusalText(CommerceError::Cooldown, 1).ToString().Contains(TEXT("1 minute")));
    TestFalse(TEXT("singular has no plural s"), CmUseRefusalText(CommerceError::Cooldown, 1).ToString().Contains(TEXT("1 minutes")));
    TestEqual(TEXT("a cooldown without a wait reads the plain sentence"), CmUseRefusalText(CommerceError::Cooldown, 0).ToString(), CmRefusalText(CommerceError::Cooldown).ToString());
    TestTrue(TEXT("asleep, using"), CmUseRefusalText(CommerceError::PlayerAsleep, 0).ToString().Contains(TEXT("asleep")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmUiReceiptTextTest, "PerfectWorld.CommerceUi.Text.ReceiptsAndWalletReadouts",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmUiReceiptTextTest::RunTest(const FString& Parameters)
{
    FCmReceiptRow Purchase;
    Purchase.Receipt.Kind = PerfectWorld::CommerceReceiptKind::Purchase;
    Purchase.Receipt.At = PerfectWorld::GameClock::FromCalendar(2042, 6, 2, 9, 15);
    Purchase.Receipt.Quantity = 2;
    Purchase.Receipt.UnitPricePaise = 2500;
    Purchase.Receipt.TotalPaise = 5000;
    Purchase.Receipt.BalanceBeforePaise = 15000;
    Purchase.Receipt.BalanceAfterPaise = 10000;
    Purchase.ItemName = FText::FromString(TEXT("Tea"));
    Purchase.ShopName = FText::FromString(TEXT("Shanti Chai"));
    const FString Line = CmReceiptText(Purchase).ToString();
    TestEqual(TEXT("the purchase line, in full"), Line,
              FString(TEXT("Mon 2 Jun, 09:15: bought 2 × Tea at Shanti Chai for ")) + CmRupee + TEXT("50. Wallet ") + CmRupee + TEXT("150, now ") + CmRupee + TEXT("100."));
    Purchase.ShopName = FText::GetEmpty();
    TestFalse(TEXT("an unnamed shop leaves no gap"), CmReceiptText(Purchase).ToString().Contains(TEXT(" at ")));

    FCmReceiptRow Allowance;
    Allowance.Receipt.Kind = PerfectWorld::CommerceReceiptKind::Allowance;
    Allowance.Receipt.At = PerfectWorld::GameClock::FromCalendar(2042, 6, 9, 6, 0);
    Allowance.Receipt.TotalPaise = 15000;
    Allowance.Receipt.BalanceBeforePaise = 2000;
    Allowance.Receipt.BalanceAfterPaise = 17000;
    const FString AllowanceLine = CmReceiptText(Allowance).ToString();
    TestTrue(TEXT("an allowance reads as one"), AllowanceLine.Contains(TEXT("weekly allowance")) && AllowanceLine.Contains(CmRs(15000)));
    TestTrue(TEXT("and shows both balances"), AllowanceLine.Contains(CmRs(2000)) && AllowanceLine.Contains(CmRs(17000)));
    TestTrue(TEXT("and a date a week later"), AllowanceLine.StartsWith(TEXT("Mon 9 Jun")));

    TestEqual(TEXT("the HUD wallet"), CmWalletText(TOptional<int64>(12000)).ToString(), FString(TEXT("Wallet ")) + CmRs(12000));
    TestTrue(TEXT("an unreadable wallet shows nothing"), CmWalletText(TOptional<int64>()).IsEmpty());

    TestFalse(TEXT("tea has a hint"), CmEffectHint(PerfectWorld::CommerceEffect::Tea).IsEmpty());
    TestFalse(TEXT("food has a hint"), CmEffectHint(PerfectWorld::CommerceEffect::Food).IsEmpty());
    TestTrue(TEXT("a supply says it has no use yet"), CmEffectHint(PerfectWorld::CommerceEffect::UnsupportedSupply).ToString().Contains(TEXT("no use yet")));
    TestTrue(TEXT("a plain good needs no hint"), CmEffectHint(PerfectWorld::CommerceEffect::None).IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCmUiBridgeRegistryTest, "PerfectWorld.CommerceUi.Text.BridgeRegistryAndHudWallet",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCmUiBridgeRegistryTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("a throw-away world"), World)) return false;
    TestFalse(TEXT("a world with no seam offers none"), CmBridge::Resolve(World).IsValid());
    TestTrue(TEXT("and shows no HUD wallet"), CmHudWalletText(World).IsEmpty());
    TestEqual(TEXT("and the pause card says unavailable"), CmPauseWalletValue(World).ToString(), FString(TEXT("Unavailable")));

    const TSharedRef<FCmFakeCommerce> Fake = FCmFakeCommerce::MakeStandard(2);
    CmBridge::Install(World, Fake);
    TestTrue(TEXT("the installed seam resolves"), CmBridge::Resolve(World).IsValid());
    TestEqual(TEXT("the HUD reads the wallet through it"), CmHudWalletText(World).ToString(), FString(TEXT("Wallet ")) + CmRs(30000));
    TestEqual(TEXT("and the pause card"), CmPauseWalletValue(World).ToString(), CmRs(30000));
    Fake->bStateReadable = false;
    TestTrue(TEXT("an unreadable state hides the HUD wallet"), CmHudWalletText(World).IsEmpty());
    TestEqual(TEXT("and the pause card says unavailable"), CmPauseWalletValue(World).ToString(), FString(TEXT("Unavailable")));

    CmBridge::Remove(World);
    TestFalse(TEXT("removing the seam removes it"), CmBridge::Resolve(World).IsValid());
    World->DestroyWorld(false);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
