#pragma once

#include "CoreMinimal.h"

#include "Commerce.h"

class UWorld;

/**
 * R8-05: the seam between the counter, the buy panel, the HUD wallet and the inventory screen on one side, and the world on the other
 * (prefix Cm / PWCommerce). It follows the school gate's IScGateWorld seam (PerfectWorldSchoolGate.h).
 *
 * Authority. Every rule lives in the portable core (Commerce.h, and R8-03's service/buy/consume commands). Nothing on the Unreal side
 * decides a price, a refusal, an opening hour, whether a keeper is ready, or what a purchase does. The UI asks this interface and shows
 * the answer: the core's CommerceError codes become words in PerfectWorldCommerceText.h, and the core's CommerceQuote/CommerceReceipt
 * values are formatted, never recomputed. A widget never holds a price that it did not just receive from here.
 *
 * Wiring (the integrator's, once R8-03 lands). The default implementation runs over UPWLaneLifeSubsystem and the validated commerce
 * content, and is installed per world with CmBridge::Install (or resolved lazily the way ScGate::Resolve does):
 *   BeginService -> the service-request command for the shop, then the shop's catalogue and a quote for what can be bought now
 *   Quote        -> QuoteFor over the world's catalogue entry and state, plus ValidatePurchase over a freshly rebuilt context
 *   Buy          -> commerce.buy(shop, item, quantity, expectedRevision); the reducer rebuilds the context, nothing here is trusted
 *   EndService   -> the service-close command
 *   ReadInventory / Consume / ReadWallet -> the world's CommerceState, inventory.consume, WalletPaise
 * Tests and the acceptance driver install a fake (PerfectWorldCommerceFake.h).
 */
namespace PWCommerce
{
/** One line of a shop's catalogue, as the world offers it right now. */
struct FCmGood
{
    FName ItemId;
    FText Name;
    /** "cup", "pair", "pack of five"; shown beside the price so a unit is always on screen. */
    FText Unit;
    int64 UnitPricePaise = 0;
    PerfectWorld::CommerceEffect Effect = PerfectWorld::CommerceEffect::None;
};

/** The answer to "may I start buying at this counter?". Error == None means yes, and the panel may open. */
struct FCmServiceView
{
    /** Why the counter cannot serve the player now (Closed, KeeperAway, NotAtSafePause, InsufficientFunds, BagFull, ...). */
    PerfectWorld::CommerceError Error = PerfectWorld::CommerceError::None;
    FName ShopId;
    FText ShopName;
    int64 WalletPaise = 0;
    /** The revision the quotes below were made at; a confirmation carries the quote's own revision, never this one. */
    uint64 Revision = 0;
    TArray<FCmGood> Goods;
    bool Ok() const { return Error == PerfectWorld::CommerceError::None; }
};

/** A portable quote plus the core's verdict on whether it could be bought right now. */
struct FCmQuoteView
{
    PerfectWorld::CommerceQuote Quote;
    /** ValidatePurchase over the current authoritative context. None means a confirmation would be accepted now. */
    PerfectWorld::CommerceError Blocked = PerfectWorld::CommerceError::None;
    bool CanBuy() const { return Quote.Ok() && Blocked == PerfectWorld::CommerceError::None; }
};

/** What a confirmation did. The receipt is the core's own. */
struct FCmBuyResult
{
    bool bAccepted = false;
    PerfectWorld::CommerceError Error = PerfectWorld::CommerceError::None;
    bool bHasReceipt = false;
    PerfectWorld::CommerceReceipt Receipt;
    FText ItemName;
    FText ShopName;
};

struct FCmStackView
{
    FName ItemId;
    FText Name;
    FText Unit;
    int32 Quantity = 0;
    PerfectWorld::CommerceEffect Effect = PerfectWorld::CommerceEffect::None;
    /** The core's answer to "could I use one now?". None means yes; Cooldown carries RetryInMinutes when known. */
    PerfectWorld::CommerceError UseBlocked = PerfectWorld::CommerceError::None;
    int32 RetryInMinutes = 0;
};

struct FCmReceiptRow
{
    PerfectWorld::CommerceReceipt Receipt;
    FText ItemName;
    FText ShopName;
};

struct FCmInventoryView
{
    /** False when no commerce state can be read (no world, or the section is unavailable); the screen says so. */
    bool bAvailable = false;
    int64 WalletPaise = 0;
    int32 SlotCount = static_cast<int32>(PerfectWorld::CommerceInventorySlots);
    /** Occupied slots in the core's authoritative order (empty slots are not listed). */
    TArray<FCmStackView> Stacks;
    /** Newest first. */
    TArray<FCmReceiptRow> Receipts;
};

struct FCmUseResult
{
    bool bAccepted = false;
    PerfectWorld::CommerceError Error = PerfectWorld::CommerceError::None;
    FText ItemName;
};

class ICmCommerceWorld
{
public:
    virtual ~ICmCommerceWorld() = default;

    /** Requests service at a shop's counter (the portable service-request) and returns the offer, or the precise reason there is none. */
    virtual FCmServiceView BeginService(FName ShopId) = 0;
    /** A quote for Quantity of ItemId at this shop, with the core's verdict on buying it now. */
    virtual FCmQuoteView Quote(FName ShopId, FName ItemId, int32 Quantity) = 0;
    /** Submits one purchase; the reducer revalidates everything. Returns the core's answer, unedited. */
    virtual FCmBuyResult Buy(FName ShopId, FName ItemId, int32 Quantity, uint64 ExpectedRevision) = 0;
    /** Closes the service opened by BeginService (the portable service-close). Safe to call when none is open. */
    virtual void EndService(FName ShopId) = 0;

    /** The wallet, inventory and recent receipts for the pause menu's Inventory screen. */
    virtual FCmInventoryView ReadInventory() = 0;
    /** Uses one unit (inventory.consume). The core refuses with a code when it cannot. */
    virtual FCmUseResult Consume(FName ItemId) = 0;
    /** The wallet for the HUD; unset when there is no readable commerce state. */
    virtual TOptional<int64> ReadWallet() = 0;
};

/** Per-world registry of commerce seams. A world with none installed offers no buying (Resolve returns null). */
namespace CmBridge
{
void Install(UWorld* World, TSharedPtr<ICmCommerceWorld> Bridge);
void Remove(UWorld* World);
TSharedPtr<ICmCommerceWorld> Resolve(const UWorld* World);
} // namespace CmBridge
} // namespace PWCommerce
