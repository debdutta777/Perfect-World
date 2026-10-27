#include "PerfectWorldCommerceText.h"

#include "GameClock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldCommerceText"

namespace PWCommerce
{
namespace
{
/** Indian grouping: the last three digits, then pairs ("1,20,000"). */
FString CmGroupDigits(const uint64 Value)
{
    const FString Digits = LexToString(Value);
    if (Digits.Len() <= 3)
    {
        return Digits;
    }
    FString Head = Digits.Left(Digits.Len() - 3);
    FString Out = TEXT(",") + Digits.Right(3);
    while (Head.Len() > 2)
    {
        Out = TEXT(",") + Head.Right(2) + Out;
        Head = Head.Left(Head.Len() - 2);
    }
    return Head + Out;
}

const TCHAR* const CmMonthNames[12] = {TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"),
                                       TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")};
} // namespace

FString CmFormatPaise(const int64 Paise)
{
    // Negating INT64_MIN overflows; take the magnitude in unsigned arithmetic.
    const bool bNegative = Paise < 0;
    const uint64 Magnitude = bNegative ? (0ull - static_cast<uint64>(Paise)) : static_cast<uint64>(Paise);
    const uint64 Rupees = Magnitude / 100ull;
    const int32 PaisePart = static_cast<int32>(Magnitude % 100ull);
    FString Out = bNegative ? TEXT("-") : TEXT("");
    Out += CmRupeeSign;
    Out += CmGroupDigits(Rupees);
    if (PaisePart != 0)
    {
        Out += FString::Printf(TEXT(".%02d"), PaisePart);
    }
    return Out;
}

FText CmWalletText(const TOptional<int64>& WalletPaise)
{
    if (!WalletPaise.IsSet())
    {
        return FText::GetEmpty();
    }
    return FText::Format(LOCTEXT("WalletFmt", "Wallet {0}"), FText::FromString(CmFormatPaise(WalletPaise.GetValue())));
}

FText CmHudWalletText(const UWorld* World)
{
    const TSharedPtr<ICmCommerceWorld> Bridge = CmBridge::Resolve(World);
    return Bridge.IsValid() ? CmWalletText(Bridge->ReadWallet()) : FText::GetEmpty();
}

FText CmPauseWalletValue(const UWorld* World)
{
    const TSharedPtr<ICmCommerceWorld> Bridge = CmBridge::Resolve(World);
    const TOptional<int64> Wallet = Bridge.IsValid() ? Bridge->ReadWallet() : TOptional<int64>();
    return Wallet.IsSet() ? FText::FromString(CmFormatPaise(Wallet.GetValue())) : LOCTEXT("WalletUnavailable", "Unavailable");
}

FText CmRefusalText(const PerfectWorld::CommerceError Error, const FText& ShopName, const FString& CoreCode)
{
    // R88: lifecycle codes are returned verbatim by WorldSim, outside CommerceError.
    if (CoreCode == TEXT("commerce.service_active"))
        return LOCTEXT("ServiceActive", "Finish at the other counter first.");
    if (CoreCode == TEXT("commerce.service_history_full"))
        return LOCTEXT("ServiceHistoryFull", "The keeper needs to finish this work. Come back after a while.");
    if (CoreCode == TEXT("commerce.service_not_open"))
        return LOCTEXT("ServiceNotOpen", "Step up to the counter again to buy.");
    using PerfectWorld::CommerceError;
    switch (Error)
    {
    case CommerceError::None:
        return FText::GetEmpty();
    case CommerceError::Closed:
        return ShopName.IsEmpty() ? LOCTEXT("ClosedPlain", "The shop is closed right now. Check the posted hours.")
                                  : FText::Format(LOCTEXT("ClosedNamed", "{0} is closed right now. Check the posted hours."), ShopName);
    case CommerceError::KeeperAway:
        return LOCTEXT("KeeperAway", "Nobody is at the counter. The keeper is away right now.");
    case CommerceError::NotAtSafePause:
        return LOCTEXT("FinishingAStep", "The keeper is in the middle of something. Wait a moment, then try again.");
    case CommerceError::NotAtCounter:
        return LOCTEXT("NotAtCounter", "Step up to the counter to buy.");
    case CommerceError::PlayerAsleep:
        return LOCTEXT("PlayerAsleep", "You cannot shop while you are asleep.");
    case CommerceError::PlayerBusy:
        return LOCTEXT("PlayerBusy", "You are busy. Finish what you are doing first.");
    case CommerceError::WorldUnsettled:
        return LOCTEXT("WorldUnsettled", "Things are still moving around you. Try again in a moment.");
    case CommerceError::InsufficientFunds:
        return LOCTEXT("InsufficientFunds", "You do not have enough money for that.");
    case CommerceError::BagFull:
        return LOCTEXT("BagFull", "Your bag is full. There is no free space for this.");
    case CommerceError::StackFull:
        return LOCTEXT("StackFull", "You cannot carry that many. Your bag has no room for the rest.");
    case CommerceError::StaleRevision:
        return LOCTEXT("StaleRevision", "Something changed while you were deciding. Check the price, then confirm again.");
    case CommerceError::InvalidItem:
        return LOCTEXT("InvalidItem", "This counter does not sell that.");
    case CommerceError::InvalidQuantity:
    case CommerceError::ArithmeticOverflow:
        return LOCTEXT("InvalidQuantity", "The counter cannot sell that quantity.");
    case CommerceError::NotOwned:
        return LOCTEXT("NotOwned", "You are not carrying that.");
    case CommerceError::UseUnavailable:
        return LOCTEXT("UseUnavailable", "Keep it in your bag. It has no use yet.");
    case CommerceError::Cooldown:
        return LOCTEXT("Cooldown", "Not yet. You had one a short while ago.");
    case CommerceError::InvalidState:
    case CommerceError::InvalidRank:
    case CommerceError::InvalidTime:
    default:
        return LOCTEXT("PurseUnreadable", "Your purse cannot be read right now. Nothing was changed.");
    }
}

FText CmShortfallText(const int64 WalletAfterPaise)
{
    if (WalletAfterPaise >= 0)
    {
        return CmRefusalText(PerfectWorld::CommerceError::InsufficientFunds);
    }
    const uint64 Short = 0ull - static_cast<uint64>(WalletAfterPaise);
    const int64 Clamped = Short > static_cast<uint64>(MAX_int64) ? MAX_int64 : static_cast<int64>(Short);
    return FText::Format(LOCTEXT("ShortfallFmt", "You do not have enough money. You need {0} more."),
                         FText::FromString(CmFormatPaise(Clamped)));
}

FText CmUseRefusalText(const PerfectWorld::CommerceError Error, const int32 RetryInMinutes)
{
    if (Error == PerfectWorld::CommerceError::Cooldown && RetryInMinutes > 0)
    {
        // A numeric argument (not FText::AsNumber) so the plural rule can pick minute or minutes.
        return FText::Format(LOCTEXT("CooldownWait", "Not yet. You had one a short while ago; wait about {0} {0}|plural(one=minute,other=minutes)."),
                             RetryInMinutes);
    }
    switch (Error)
    {
    case PerfectWorld::CommerceError::PlayerAsleep:
        return LOCTEXT("UseAsleep", "You cannot use anything while you are asleep.");
    case PerfectWorld::CommerceError::PlayerBusy:
        return LOCTEXT("UseBusy", "Finish what you are doing first, then use it.");
    default:
        return CmRefusalText(Error);
    }
}

FText CmInstantText(const PerfectWorld::GameInstant At)
{
    const PerfectWorld::CalendarTime Calendar = PerfectWorld::GameClock::ToCalendar(At);
    const std::string_view Weekday = PerfectWorld::WeekdayName(Calendar.DayOfWeek);
    const FString Day = FString(static_cast<int32>(Weekday.size()), UTF8_TO_TCHAR(std::string(Weekday).c_str())).Left(3);
    const TCHAR* Month = CmMonthNames[FMath::Clamp(Calendar.Month - 1, 0, 11)];
    return FText::FromString(FString::Printf(TEXT("%s %d %s, %02d:%02d"), *Day, Calendar.Day, Month, Calendar.Hour, Calendar.Minute));
}

FText CmItemWithUnit(const FText& Name, const FText& Unit)
{
    return Unit.IsEmpty() ? Name : FText::Format(LOCTEXT("ItemWithUnit", "{0} ({1})"), Name, Unit);
}

FText CmUnitPriceText(const int64 UnitPricePaise, const FText& Unit)
{
    const FText Price = FText::FromString(CmFormatPaise(UnitPricePaise));
    return Unit.IsEmpty() ? Price : FText::Format(LOCTEXT("UnitPriceFmt", "{0} per {1}"), Price, Unit);
}

FText CmReceiptText(const FCmReceiptRow& Row)
{
    const PerfectWorld::CommerceReceipt& R = Row.Receipt;
    const FText When = CmInstantText(R.At);
    const FText Total = FText::FromString(CmFormatPaise(R.TotalPaise));
    const FText Before = FText::FromString(CmFormatPaise(R.BalanceBeforePaise));
    const FText After = FText::FromString(CmFormatPaise(R.BalanceAfterPaise));
    if (R.Kind == PerfectWorld::CommerceReceiptKind::Allowance)
    {
        return FText::Format(LOCTEXT("AllowanceReceipt", "{0}: weekly allowance {1}. Wallet {2}, now {3}."), When, Total, Before, After);
    }
    const FText Item = Row.ItemName.IsEmpty() ? LOCTEXT("SomeGoods", "goods") : Row.ItemName;
    const FText Quantity = FText::AsNumber(R.Quantity);
    if (Row.ShopName.IsEmpty())
    {
        return FText::Format(LOCTEXT("PurchaseReceipt", "{0}: bought {1} × {2} for {3}. Wallet {4}, now {5}."), When, Quantity, Item, Total, Before, After);
    }
    return FText::Format(LOCTEXT("PurchaseReceiptAt", "{0}: bought {1} × {2} at {3} for {4}. Wallet {5}, now {6}."), When, Quantity, Item,
                         Row.ShopName, Total, Before, After);
}

FText CmEffectHint(const PerfectWorld::CommerceEffect Effect)
{
    switch (Effect)
    {
    case PerfectWorld::CommerceEffect::Tea:
        return LOCTEXT("TeaHint", "Gives back some energy. Once an hour.");
    case PerfectWorld::CommerceEffect::Food:
        return LOCTEXT("FoodHint", "Eases your hunger.");
    case PerfectWorld::CommerceEffect::UnsupportedSupply:
        return LOCTEXT("SupplyHint", "Keep it in your bag. It has no use yet.");
    default:
        return FText::GetEmpty();
    }
}
} // namespace PWCommerce

#undef LOCTEXT_NAMESPACE
