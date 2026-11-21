#include "Commerce.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::int64_t, 5> R81Funding = {12000, 15000, 30000, 60000, 120000};

bool R81Id(std::string_view id)
{
    if (id.empty() || id.size() > 128) return false;
    for (const char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    return true;
}

bool R81Add(std::int64_t a, std::int64_t b, std::int64_t& out)
{
    if (a < 0 || b < 0 || a > std::numeric_limits<std::int64_t>::max() - b) return false;
    out = a + b;
    return true;
}

bool R81Multiply(std::int64_t a, std::int64_t b, std::int64_t& out)
{
    if (a < 0 || b < 0 || (b != 0 && a > std::numeric_limits<std::int64_t>::max() / b)) return false;
    out = a * b;
    return true;
}

// Operates only on a candidate copy; caller discards it on any failure.
bool R81Append(CommerceState& state, const CommerceReceipt& receipt)
{
    if (state.RecentReceipts.size() == CommerceRecentReceiptLimit)
    {
        const auto& old = state.RecentReceipts.front();
        auto& total = old.Kind == CommerceReceiptKind::Allowance ? state.FoldedCreditsPaise : state.FoldedDebitsPaise;
        if (!R81Add(total, old.TotalPaise, total) ||
            state.FoldedReceiptCount == std::numeric_limits<std::uint64_t>::max()) return false;
        ++state.FoldedReceiptCount;
        state.FoldedThroughRevision = old.Revision;
        state.RecentReceipts.erase(state.RecentReceipts.begin());
    }
    state.RecentReceipts.push_back(receipt);
    return true;
}

CommerceError R81Capacity(const CommerceState& state, std::string_view itemId, std::int32_t quantity)
{
    std::int32_t space = 0;
    bool matching = false;
    for (const auto& stack : state.Inventory)
    {
        if (stack.ItemId.empty()) space += CommerceStackLimit;
        else if (stack.ItemId == itemId)
        {
            matching = true;
            space += CommerceStackLimit - stack.Quantity;
        }
    }
    return space >= quantity ? CommerceError::None : matching ? CommerceError::StackFull : CommerceError::BagFull;
}
} // namespace

std::string_view CommerceErrorCode(CommerceError error) noexcept
{
    switch (error)
    {
    case CommerceError::None: return {};
    case CommerceError::InvalidState: return "commerce.invalid_state";
    case CommerceError::InvalidRank: return "commerce.invalid_rank";
    case CommerceError::InvalidTime: return "commerce.invalid_time";
    case CommerceError::InvalidItem: return "commerce.invalid_item";
    case CommerceError::InvalidQuantity: return "commerce.invalid_quantity";
    case CommerceError::ArithmeticOverflow: return "commerce.arithmetic_overflow";
    case CommerceError::Closed: return "commerce.closed";
    case CommerceError::KeeperAway: return "commerce.keeper_away";
    case CommerceError::NotAtSafePause: return "commerce.not_at_safe_pause";
    case CommerceError::NotAtCounter: return "commerce.not_at_counter";
    case CommerceError::PlayerAsleep: return "commerce.player_asleep";
    case CommerceError::PlayerBusy: return "commerce.player_busy";
    case CommerceError::WorldUnsettled: return "commerce.world_unsettled";
    case CommerceError::InsufficientFunds: return "commerce.insufficient_funds";
    case CommerceError::BagFull: return "commerce.bag_full";
    case CommerceError::StackFull: return "commerce.stack_full";
    case CommerceError::StaleRevision: return "commerce.stale_revision";
    case CommerceError::NotOwned: return "commerce.not_owned";
    case CommerceError::UseUnavailable: return "commerce.use_unavailable";
    case CommerceError::Cooldown: return "commerce.cooldown";
    }
    return "commerce.invalid_error";
}

bool IsCommerceItemValid(const CommerceItem& item)
{
    if (!R81Id(item.Id) || item.Name.empty() || item.Unit.empty() || item.StackLimit != CommerceStackLimit) return false;
    switch (item.Effect)
    {
    case CommerceEffect::None:
    case CommerceEffect::Tea:
    case CommerceEffect::UnsupportedSupply: return item.HungerRelief == 0;
    case CommerceEffect::Food: return item.HungerRelief >= 1000 && item.HungerRelief <= 5000;
    }
    return false;
}

bool IsCommerceStateValid(const CommerceState& state)
{
    const auto& policy = state.Funding;
    if (policy.HouseholdResourcesRank < 0 || policy.HouseholdResourcesRank > 4 ||
        policy.StartingWalletPaise != R81Funding[static_cast<std::size_t>(policy.HouseholdResourcesRank)] ||
        policy.WeeklyAllowancePaise != policy.StartingWalletPaise || state.WalletPaise < 0 ||
        state.InitialisedAt < 0 || state.AllowanceSettledThrough < state.InitialisedAt ||
        state.AllowanceSettledThrough > CommerceMaxInstant ||
        state.NextConsumptionEligibleAt < state.InitialisedAt ||
        state.NextConsumptionEligibleAt > state.AllowanceSettledThrough ||
        state.NextTeaEligibleAt < state.InitialisedAt ||
        state.NextTeaEligibleAt > state.AllowanceSettledThrough + CommerceTeaCooldownMinutes ||
        state.RecentReceipts.size() > CommerceRecentReceiptLimit || state.FoldedCreditsPaise < 0 ||
        state.FoldedDebitsPaise < 0 || state.FoldedThroughRevision > state.TransactionRevision) return false;
    const auto periods = (state.AllowanceSettledThrough - state.InitialisedAt) / CommerceWeekMinutes;
    if (state.NextAllowanceDue != state.InitialisedAt + (periods + 1) * CommerceWeekMinutes) return false;
    std::int64_t expectedCredits = 0;
    if (!R81Multiply(periods, policy.WeeklyAllowancePaise, expectedCredits) ||
        state.FoldedCreditsPaise > expectedCredits || state.FoldedCreditsPaise % policy.WeeklyAllowancePaise != 0) return false;
    auto creditedPeriods = state.FoldedCreditsPaise / policy.WeeklyAllowancePaise;
    if (static_cast<std::uint64_t>(creditedPeriods) > state.FoldedReceiptCount) return false;
    const auto foldedPurchases = state.FoldedReceiptCount - static_cast<std::uint64_t>(creditedPeriods);
    // Every folded purchase debited at least one paise. A credit-only prefix
    // cannot contain debits, even when a forged opening balance reconciles.
    if ((foldedPurchases == 0 && state.FoldedDebitsPaise != 0) ||
        foldedPurchases > static_cast<std::uint64_t>(state.FoldedDebitsPaise)) return false;
    for (const auto& stack : state.Inventory)
        if (stack.Quantity < 0 || stack.Quantity > CommerceStackLimit ||
            (stack.Quantity == 0 ? !stack.ItemId.empty() : !R81Id(stack.ItemId))) return false;
    if (state.FoldedReceiptCount == 0)
    {
        if (state.FoldedThroughRevision != 0 || state.FoldedCreditsPaise != 0 || state.FoldedDebitsPaise != 0) return false;
    }
    else if (state.FoldedThroughRevision == 0 || state.FoldedReceiptCount > state.FoldedThroughRevision ||
             state.RecentReceipts.size() != CommerceRecentReceiptLimit) return false;
    std::int64_t balance = 0;
    std::int64_t credits = state.FoldedCreditsPaise;
    if (!R81Add(policy.StartingWalletPaise, credits, balance) || balance < state.FoldedDebitsPaise) return false;
    balance -= state.FoldedDebitsPaise;
    auto revision = state.FoldedThroughRevision;
    GameInstant at = state.InitialisedAt + creditedPeriods * CommerceWeekMinutes;
    for (const auto& receipt : state.RecentReceipts)
    {
        std::int64_t total = 0;
        if (receipt.Revision <= revision || receipt.Revision > state.TransactionRevision || receipt.At < at ||
            receipt.At > state.AllowanceSettledThrough || receipt.Quantity < 1 || receipt.Quantity > 120 ||
            receipt.UnitPricePaise <= 0 || !R81Multiply(receipt.UnitPricePaise, receipt.Quantity, total) ||
            receipt.TotalPaise != total || receipt.BalanceBeforePaise != balance) return false;
        switch (receipt.Kind)
        {
        case CommerceReceiptKind::Allowance:
            if (!receipt.ShopId.empty() || !receipt.ItemId.empty() || receipt.Quantity != 1 ||
                receipt.UnitPricePaise != policy.WeeklyAllowancePaise || receipt.At <= state.InitialisedAt ||
                creditedPeriods >= periods ||
                receipt.At != state.InitialisedAt + (creditedPeriods + 1) * CommerceWeekMinutes ||
                !R81Add(credits, total, credits) || !R81Add(balance, total, balance)) return false;
            ++creditedPeriods;
            break;
        case CommerceReceiptKind::Purchase:
            if (!R81Id(receipt.ShopId) || !R81Id(receipt.ItemId) || balance < total) return false;
            balance -= total;
            break;
        default: return false;
        }
        if (receipt.BalanceAfterPaise != balance) return false;
        revision = receipt.Revision;
        at = receipt.At;
    }
    return credits == expectedCredits && balance == state.WalletPaise;
}

CommerceStateResult InitialiseCommerce(std::int32_t rank, GameInstant instant)
{
    CommerceStateResult result;
    if (rank < 0 || rank > 4) { result.Error = CommerceError::InvalidRank; return result; }
    if (instant < 0 || instant > CommerceMaxInstant) { result.Error = CommerceError::InvalidTime; return result; }
    const auto amount = R81Funding[static_cast<std::size_t>(rank)];
    result.State.WalletPaise = amount;
    result.State.Funding = {rank, amount, amount};
    result.State.InitialisedAt = instant;
    result.State.AllowanceSettledThrough = instant;
    result.State.NextAllowanceDue = instant + CommerceWeekMinutes;
    result.State.NextTeaEligibleAt = instant;
    result.State.NextConsumptionEligibleAt = instant;
    return result;
}

CommerceStateResult SettleAllowance(const CommerceState& state, GameInstant from, GameInstant to)
{
    CommerceStateResult result{CommerceError::None, state, {}};
    if (!IsCommerceStateValid(state)) { result.Error = CommerceError::InvalidState; return result; }
    if (from != state.AllowanceSettledThrough || to < from || to > CommerceMaxInstant)
    { result.Error = CommerceError::InvalidTime; return result; }
    CommerceState next = state;
    while (next.NextAllowanceDue <= to)
    {
        std::int64_t after = 0;
        if (next.TransactionRevision == std::numeric_limits<std::uint64_t>::max() ||
            !R81Add(next.WalletPaise, next.Funding.WeeklyAllowancePaise, after))
        { result.Error = CommerceError::ArithmeticOverflow; return result; }
        CommerceReceipt receipt{CommerceReceiptKind::Allowance, ++next.TransactionRevision, next.NextAllowanceDue,
            {}, {}, 1, next.Funding.WeeklyAllowancePaise, next.Funding.WeeklyAllowancePaise, next.WalletPaise, after};
        if (!R81Append(next, receipt)) { result.Error = CommerceError::ArithmeticOverflow; return result; }
        next.WalletPaise = after;
        next.NextAllowanceDue += CommerceWeekMinutes; // bounded world horizon + one week
    }
    next.AllowanceSettledThrough = to;
    result.State = std::move(next);
    return result;
}

CommerceQuote QuoteFor(const CommerceCatalogueEntry& entry, const CommerceItem& item,
    std::int32_t quantity, const CommerceState& state)
{
    CommerceQuote quote;
    if (!IsCommerceStateValid(state)) { quote.Error = CommerceError::InvalidState; return quote; }
    if (!IsCommerceItemValid(item) || !R81Id(entry.ShopId) || entry.ItemId != item.Id || entry.UnitPricePaise <= 0)
    { quote.Error = CommerceError::InvalidItem; return quote; }
    if (quantity <= 0 || quantity > 120) { quote.Error = CommerceError::InvalidQuantity; return quote; }
    if (!R81Multiply(entry.UnitPricePaise, quantity, quote.TotalPaise))
    { quote.Error = CommerceError::ArithmeticOverflow; return quote; }
    quote.ShopId = entry.ShopId;
    quote.ItemId = item.Id;
    quote.Quantity = quantity;
    quote.UnitPricePaise = entry.UnitPricePaise;
    quote.WalletBeforePaise = state.WalletPaise;
    quote.WalletAfterPaise = state.WalletPaise - quote.TotalPaise; // both in [0,INT64_MAX]
    quote.ExpectedRevision = state.TransactionRevision;
    return quote;
}

CommerceError ValidatePurchase(const CommerceState& state, const CommerceCatalogueEntry& entry,
    const CommerceItem& item, std::int32_t quantity, std::uint64_t revision, const CommercePurchaseContext& context)
{
    if (!IsCommerceStateValid(state)) return CommerceError::InvalidState;
    if (context.Now != state.AllowanceSettledThrough) return CommerceError::InvalidTime;
    if (revision != state.TransactionRevision) return CommerceError::StaleRevision;
    if (!context.ShopOpen) return CommerceError::Closed;
    if (!context.KeeperPresent) return CommerceError::KeeperAway;
    if (!context.AtSafePause) return CommerceError::NotAtSafePause;
    if (!context.AtCounter) return CommerceError::NotAtCounter;
    if (!context.PlayerAwake) return CommerceError::PlayerAsleep;
    if (!context.PlayerFree) return CommerceError::PlayerBusy;
    if (!context.WorldSettled) return CommerceError::WorldUnsettled;
    const auto quote = QuoteFor(entry, item, quantity, state);
    if (!quote.Ok()) return quote.Error;
    if (state.TransactionRevision == std::numeric_limits<std::uint64_t>::max()) return CommerceError::ArithmeticOverflow;
    if (quote.WalletAfterPaise < 0) return CommerceError::InsufficientFunds;
    return R81Capacity(state, item.Id, quantity);
}

CommerceStateResult ApplyPurchase(const CommerceState& state, const CommerceCatalogueEntry& entry,
    const CommerceItem& item, std::int32_t quantity, std::uint64_t revision, const CommercePurchaseContext& context)
{
    CommerceStateResult result{ValidatePurchase(state, entry, item, quantity, revision, context), state, {}};
    if (!result.Ok()) return result;
    const auto quote = QuoteFor(entry, item, quantity, state);
    CommerceState next = state;
    std::int32_t remaining = quantity;
    for (int pass = 0; pass < 2; ++pass)
        for (auto& stack : next.Inventory)
        {
            if (remaining == 0) break;
            if ((pass == 0 && stack.ItemId != item.Id) || (pass == 1 && !stack.ItemId.empty())) continue;
            const auto delivered = std::min(remaining, CommerceStackLimit - stack.Quantity);
            stack.ItemId = item.Id;
            stack.Quantity += delivered;
            remaining -= delivered;
        }
    const CommerceReceipt receipt{CommerceReceiptKind::Purchase, ++next.TransactionRevision, context.Now,
        entry.ShopId, item.Id, quantity, quote.UnitPricePaise, quote.TotalPaise, state.WalletPaise, quote.WalletAfterPaise};
    if (!R81Append(next, receipt)) { result.Error = CommerceError::ArithmeticOverflow; return result; }
    next.WalletPaise = quote.WalletAfterPaise;
    result.State = std::move(next);
    result.Receipt = receipt;
    return result;
}

CommerceConsumptionResult Consume(const CommerceState& state, const ConditionState& condition,
    const CommerceItem& item, GameInstant now, const CommerceConsumptionContext& context)
{
    CommerceConsumptionResult result{CommerceError::None, state, condition};
    if (!IsCommerceStateValid(state) || !IsConditionValid(condition)) result.Error = CommerceError::InvalidState;
    else if (now != state.AllowanceSettledThrough) result.Error = CommerceError::InvalidTime;
    else if (!context.PlayerAwake) result.Error = CommerceError::PlayerAsleep;
    else if (context.ActivityActive) result.Error = CommerceError::PlayerBusy;
    else if (!context.WorldSettled) result.Error = CommerceError::WorldUnsettled;
    else if (!IsCommerceItemValid(item)) result.Error = CommerceError::InvalidItem;
    else if (item.Effect == CommerceEffect::None || item.Effect == CommerceEffect::UnsupportedSupply)
        result.Error = CommerceError::UseUnavailable;
    else if (now < state.NextConsumptionEligibleAt || (item.Effect == CommerceEffect::Tea && now < state.NextTeaEligibleAt))
        result.Error = CommerceError::Cooldown;
    else if (state.TransactionRevision == std::numeric_limits<std::uint64_t>::max()) result.Error = CommerceError::ArithmeticOverflow;
    if (!result.Ok()) return result;
    auto slot = std::find_if(result.State.Inventory.begin(), result.State.Inventory.end(),
        [&item](const CommerceStack& stack) { return stack.ItemId == item.Id; });
    if (slot == result.State.Inventory.end()) { result.Error = CommerceError::NotOwned; return result; }
    if (--slot->Quantity == 0) slot->ItemId.clear();
    if (item.Effect == CommerceEffect::Tea)
    {
        result.Condition.Energy = std::min(MeterMax, condition.Energy + 2000);
        result.State.NextTeaEligibleAt = now + CommerceTeaCooldownMinutes;
    }
    else result.Condition.Hunger = std::max(0, condition.Hunger - item.HungerRelief);
    result.State.NextConsumptionEligibleAt = now;
    ++result.State.TransactionRevision;
    return result;
}
} // namespace PerfectWorld
