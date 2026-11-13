#pragma once

#include "Condition.h"
#include "GameClock.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
// R81 frozen contract for R8-02..05. Integer paise; zero-minute commits; no IO,
// exceptions, catalogue ownership, schedule resolution or persisted codec here.
inline constexpr std::string_view CommerceTextHeader = "pw.commerce/1";
constexpr std::size_t CommerceInventorySlots = 12;
constexpr std::int32_t CommerceStackLimit = 10;
constexpr std::size_t CommerceRecentReceiptLimit = 64;
constexpr GameInstant CommerceWeekMinutes = 7 * MinutesPerDay;
constexpr GameInstant CommerceTeaCooldownMinutes = 60;
constexpr GameInstant CommerceMaxInstant = 100 * 366 * MinutesPerDay;

enum class CommerceEffect : std::uint8_t { None = 0, Tea, Food, UnsupportedSupply };

// R8-02 owns content parsing, stable IDs and lookup. These are value contracts,
// not a hardcoded catalogue. All eleven items use StackLimit=10 this round.
struct CommerceItem
{
    std::string Id;
    std::string Name;
    std::string Unit;
    std::int32_t StackLimit = CommerceStackLimit;
    CommerceEffect Effect = CommerceEffect::None;
    // Food only: 1000..5000 milli Hunger relief. Tea is fixed +2000 Energy.
    std::int32_t HungerRelief = 0;
    friend bool operator==(const CommerceItem&, const CommerceItem&) = default;
};

struct CommerceCatalogueEntry
{
    std::string ShopId;
    std::string ItemId;
    std::int64_t UnitPricePaise = 0; // strictly positive
    friend bool operator==(const CommerceCatalogueEntry&, const CommerceCatalogueEntry&) = default;
};

struct CommerceStack
{
    std::string ItemId; // empty iff Quantity=0; slot order is authoritative
    std::int32_t Quantity = 0;
    friend bool operator==(const CommerceStack&, const CommerceStack&) = default;
};

struct CommerceFundingPolicy
{
    std::int32_t HouseholdResourcesRank = 0;
    std::int64_t StartingWalletPaise = 12000;
    std::int64_t WeeklyAllowancePaise = 12000;
    friend bool operator==(const CommerceFundingPolicy&, const CommerceFundingPolicy&) = default;
};

enum class CommerceReceiptKind : std::uint8_t { Allowance = 0, Purchase };
struct CommerceReceipt
{
    CommerceReceiptKind Kind = CommerceReceiptKind::Allowance;
    std::uint64_t Revision = 0; // successful mutation's revision, never reused
    GameInstant At = 0;
    std::string ShopId; // empty for allowance
    std::string ItemId; // empty for allowance
    std::int32_t Quantity = 0; // 1 for allowance
    std::int64_t UnitPricePaise = 0;
    std::int64_t TotalPaise = 0; // positive credit/debit according to Kind
    std::int64_t BalanceBeforePaise = 0;
    std::int64_t BalanceAfterPaise = 0;
    friend bool operator==(const CommerceReceipt&, const CommerceReceipt&) = default;
};

struct CommerceState
{
    std::int64_t WalletPaise = 12000;
    std::array<CommerceStack, CommerceInventorySlots> Inventory{};
    CommerceFundingPolicy Funding;
    GameInstant InitialisedAt = 0;
    // Settlement requires from == AllowanceSettledThrough. Repeated identical
    // intervals return InvalidTime, never backpay. Empty [t,t] is a valid no-op.
    GameInstant AllowanceSettledThrough = 0;
    GameInstant NextAllowanceDue = CommerceWeekMinutes;
    // Advances once per allowance boundary, purchase or consumption. Settlement
    // with no boundary changes only its cursor, preserving split/bulk equality.
    std::uint64_t TransactionRevision = 0;
    // Chronological oldest-first, last 64 monetary transactions; consumption
    // leaves revision gaps. Initial funding is represented by Funding, no receipt.
    std::vector<CommerceReceipt> RecentReceipts;
    std::uint64_t FoldedReceiptCount = 0;
    std::uint64_t FoldedThroughRevision = 0;
    std::int64_t FoldedCreditsPaise = 0;
    std::int64_t FoldedDebitsPaise = 0;
    GameInstant NextTeaEligibleAt = 0;
    // Food has no added cooldown in this plan. This monotonic floor prevents
    // backdated use; successful use sets it to now, while tea also sets now+60.
    GameInstant NextConsumptionEligibleAt = 0;
    friend bool operator==(const CommerceState&, const CommerceState&) = default;
};

enum class CommerceError : std::uint8_t
{
    None = 0, InvalidState, InvalidRank, InvalidTime, InvalidItem,
    InvalidQuantity, ArithmeticOverflow, Closed, KeeperAway, NotAtSafePause,
    NotAtCounter, PlayerAsleep, PlayerBusy, WorldUnsettled, InsufficientFunds,
    BagFull, StackFull, StaleRevision, NotOwned, UseUnavailable, Cooldown
};
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CommerceErrorCode(CommerceError error) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsCommerceItemValid(const CommerceItem& item);
// Includes financial reconciliation, ordered receipt balances/revisions and
// allowance cursor/policy validation. Content ID existence is R8-02/04's job.
[[nodiscard]] PERFECTWORLDCORE_API bool IsCommerceStateValid(const CommerceState& state);

struct CommerceStateResult
{
    CommerceError Error = CommerceError::None;
    CommerceState State;
    // Purchase only; allowance credits are in State.RecentReceipts.
    std::optional<CommerceReceipt> Receipt;
    [[nodiscard]] bool Ok() const noexcept { return Error == CommerceError::None; }
};

// Invalid rank/time returns Error; never use State unless Ok(). The starting /
// weekly table in paise is 12000,15000,30000,60000,120000 for ranks 0..4.
[[nodiscard]] PERFECTWORLDCORE_API CommerceStateResult InitialiseCommerce(
    std::int32_t householdResourcesRank, GameInstant creationInstant);
// Pure transformation. Due instants in (from,to] credit exactly once. All
// refusals return the original State, no receipt. Long skips are bounded by the
// 100-year world horizon, not MaxSingleAdvanceMinutes. No saturation or wrapping.
[[nodiscard]] PERFECTWORLDCORE_API CommerceStateResult SettleAllowance(
    const CommerceState& state, GameInstant from, GameInstant to);

struct CommerceQuote
{
    CommerceError Error = CommerceError::None;
    std::string ShopId;
    std::string ItemId;
    std::int32_t Quantity = 0;
    std::int64_t UnitPricePaise = 0;
    std::int64_t TotalPaise = 0;
    std::int64_t WalletBeforePaise = 0;
    std::int64_t WalletAfterPaise = 0; // may be negative for an unaffordable quote
    std::uint64_t ExpectedRevision = 0;
    [[nodiscard]] bool Ok() const noexcept { return Error == CommerceError::None; }
};
// Prices come from validated portable content. Quotes don't reserve capacity or
// money and can describe an unaffordable purchase. Validation recomputes them.
[[nodiscard]] PERFECTWORLDCORE_API CommerceQuote QuoteFor(const CommerceCatalogueEntry& entry,
    const CommerceItem& item, std::int32_t quantity, const CommerceState& state);

struct CommercePurchaseContext
{
    GameInstant Now = 0;
    bool ShopOpen = false;
    bool KeeperPresent = false;
    bool AtSafePause = false;
    bool AtCounter = false;
    bool PlayerAwake = false;
    bool PlayerFree = false;
    bool WorldSettled = false;
};
// R8-03 constructs this context afresh from authoritative schedules, interruption
// history, physical approach, player and pending time state at confirmation.
// Never accept UI-provided price/readiness. Now must equal the settled cursor.
// Precedence: state/time, revision, service/player, item/quantity/arithmetic,
// funds, capacity. StackFull means a matching stack exists but total space is
// insufficient; BagFull means no matching stack and no empty slot. Matching
// partial stacks fill first, then empty slots, each in ascending slot order.
[[nodiscard]] PERFECTWORLDCORE_API CommerceError ValidatePurchase(const CommerceState& state,
    const CommerceCatalogueEntry& entry, const CommerceItem& item, std::int32_t quantity,
    std::uint64_t expectedRevision, const CommercePurchaseContext& context);
[[nodiscard]] PERFECTWORLDCORE_API CommerceStateResult ApplyPurchase(const CommerceState& state,
    const CommerceCatalogueEntry& entry, const CommerceItem& item, std::int32_t quantity,
    std::uint64_t expectedRevision, const CommercePurchaseContext& context);

struct CommerceConsumptionContext
{
    bool PlayerAwake = false;
    bool ActivityActive = true;
    bool WorldSettled = false;
};
struct CommerceConsumptionResult
{
    CommerceError Error = CommerceError::None;
    CommerceState State;
    ConditionState Condition;
    [[nodiscard]] bool Ok() const noexcept { return Error == CommerceError::None; }
};
// Removes one unit from the first matching slot, clears an emptied slot, clamps
// the authored effect, increments revision. Tea cooldown is shared across all
// Tea IDs. UnsupportedSupply and None refuse UseUnavailable, no generic healing.
// Active activities refuse, even at their start instant. Caller applies BOTH
// returned slices only on Ok(); refusal returns both inputs unchanged.
[[nodiscard]] PERFECTWORLDCORE_API CommerceConsumptionResult Consume(const CommerceState& state,
    const ConditionState& condition, const CommerceItem& item, GameInstant now,
    const CommerceConsumptionContext& context);

// R8-04 SERIALIZATION/HASH CONTRACT (pw.commerce/1): serialise EVERY field of
// CommerceState in declaration order, including each Inventory slot in order,
// Funding's three fields and RecentReceipts oldest-first with EVERY receipt
// field above. Encode enums by explicit stable names (allowance/purchase).
// No service contexts, quotes, content definitions or result errors persist.
// Authenticate legacy saves first; migrate using InitialiseCommerce(rank,saved
// instant), giving no backpay. On load: IsCommerceStateValid, content ID lookup,
// Funding rank/table equality against validated creation, and settled cursor ==
// world instant. Hash all fields including empty slots and eligibility instants.
// Folded totals + initial funding + recent credits - folded/recent debits ==
// WalletPaise. FoldedThroughRevision and TransactionRevision survive folding:
// no purchase retry can become valid when its old receipt disappears.
} // namespace PerfectWorld
