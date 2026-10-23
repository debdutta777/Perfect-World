#pragma once

#include "CoreMinimal.h"

#include "PerfectWorldCommerceBridge.h"

/**
 * R8-05: the words and number formats of the commerce screens (pure; no world, no decisions).
 *
 * Money is integer paise everywhere in the core. It is formatted here and nowhere else: "₹" then rupees with Indian digit grouping,
 * and ".NN" only when the paise are not zero ("₹25", "₹25.50", "₹1,20,000", "-₹5"). The rupee sign is in the project's
 * UI font (Hind Siliguri). A refusal is the core's CommerceError turned into one specific sentence; nothing here picks the error.
 */
namespace PWCommerce
{
/** The rupee sign, U+20B9. */
inline const TCHAR* const CmRupeeSign = TEXT("₹");

/** "₹25", "₹25.50", "-₹5.05". Safe for every int64, including the most negative. */
FString CmFormatPaise(int64 Paise);

/** "Wallet ₹120" for the HUD and the pause card; empty when the wallet is unreadable (the HUD then shows nothing). */
FText CmWalletText(const TOptional<int64>& WalletPaise);

/** The HUD pill's text for a world: "Wallet ₹120" from its commerce seam, or empty when there is none or the wallet is unreadable. */
FText CmHudWalletText(const UWorld* World);

/** The pause card's WALLET value: "₹120", or "Unavailable" when the world has no readable wallet. */
FText CmPauseWalletValue(const UWorld* World);

/** One sentence for a core refusal. ShopName (may be empty) is used where it reads naturally ("Shanti Chai is closed"). None gives empty. */
FText CmRefusalText(PerfectWorld::CommerceError Error, const FText& ShopName = FText::GetEmpty());

/** "Not enough money: you need ₹5 more." from the quote's wallet-after (negative), or the plain refusal when it is not short. */
FText CmShortfallText(int64 WalletAfterPaise);

/** Why an item cannot be used now: the core's code in words, with the wait when the core reported one. */
FText CmUseRefusalText(PerfectWorld::CommerceError Error, int32 RetryInMinutes);

/** "Mon 2 Jun, 09:15" from a game instant. */
FText CmInstantText(PerfectWorld::GameInstant At);

/** The receipt line shown after a purchase and in the history. Purchase and allowance each have their own wording. */
FText CmReceiptText(const FCmReceiptRow& Row);

/** "Tea (cup)": name and unit, so a unit is always visible. Unit may be empty. */
FText CmItemWithUnit(const FText& Name, const FText& Unit);

/** "₹25 per cup". */
FText CmUnitPriceText(int64 UnitPricePaise, const FText& Unit);

/** What the item does, in plain words, for the inventory screen; empty for goods with no use this round. */
FText CmEffectHint(PerfectWorld::CommerceEffect Effect);
} // namespace PWCommerce
