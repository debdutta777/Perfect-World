#include "Commerce.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

using namespace PerfectWorld;
namespace
{
int R81Assertions = 0;
const CommerceItem R81Tea{"item.tea", "Tea", "cup", 10, CommerceEffect::Tea, 0};
const CommerceItem R81Food{"item.biscuits", "Biscuits", "pair", 10, CommerceEffect::Food, 1000};
const CommerceItem R81Supply{"item.ors", "ORS", "sachet", 10, CommerceEffect::UnsupportedSupply, 0};
const CommerceConsumptionContext R81Free{true, false, true};

void R81Expect(bool condition, const char* message)
{
    ++R81Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

CommerceState R81New(std::int32_t rank = 0, GameInstant at = 0)
{
    const auto result = InitialiseCommerce(rank, at);
    R81Expect(result.Ok() && IsCommerceStateValid(result.State), "initialisation produces valid state");
    return result.State;
}

CommercePurchaseContext R81Ready(GameInstant at = 0) { return {at, true, true, true, true, true, true, true}; }
CommerceCatalogueEntry R81Entry(const CommerceItem& item, std::int64_t price = 2500)
{ return {"shop.chai", item.Id, price}; }

void R81PurchaseRefusal(const CommerceState& state, const CommerceCatalogueEntry& entry,
    const CommerceItem& item, std::int32_t quantity, std::uint64_t revision,
    const CommercePurchaseContext& context, CommerceError error)
{
    const auto before = state;
    R81Expect(ValidatePurchase(state, entry, item, quantity, revision, context) == error, "precise purchase reason");
    const auto result = ApplyPurchase(state, entry, item, quantity, revision, context);
    R81Expect(result.Error == error && result.State == before && !result.Receipt, "refusal returns unchanged state/no receipt");
    R81Expect(state == before, "purchase never mutates input");
}

void R81UseRefusal(const CommerceState& state, const ConditionState& condition, const CommerceItem& item,
    GameInstant at, const CommerceConsumptionContext& context, CommerceError error)
{
    const auto result = Consume(state, condition, item, at, context);
    R81Expect(result.Error == error && result.State == state && result.Condition == condition, "use refusal is atomic in both slices");
}

void R81FundingAndAllowance()
{
    constexpr std::int64_t amounts[] = {12000, 15000, 30000, 60000, 120000};
    for (int rank = 0; rank <= 4; ++rank)
    {
        const auto initial = R81New(rank, 1399);
        R81Expect(initial.WalletPaise == amounts[rank] && initial.Funding.WeeklyAllowancePaise == amounts[rank], "start/weekly table");
        R81Expect(initial.NextAllowanceDue == 1399 + CommerceWeekMinutes, "relative seven-day boundary");
        auto before = SettleAllowance(initial, 1399, initial.NextAllowanceDue - 1);
        R81Expect(before.Ok() && before.State.WalletPaise == initial.WalletPaise && before.State.TransactionRevision == 0, "no early credit");
        auto due = SettleAllowance(before.State, before.State.AllowanceSettledThrough, initial.NextAllowanceDue);
        R81Expect(due.Ok() && due.State.WalletPaise == 2 * amounts[rank] && due.State.TransactionRevision == 1, "credit exactly at boundary");
        const auto repeated = SettleAllowance(due.State, before.State.AllowanceSettledThrough, initial.NextAllowanceDue);
        R81Expect(repeated.Error == CommerceError::InvalidTime && repeated.State == due.State, "replayed settlement cannot farm credit");
        auto empty = SettleAllowance(due.State, initial.NextAllowanceDue, initial.NextAllowanceDue);
        R81Expect(empty.Ok() && empty.State == due.State, "empty interval is no-op");
        // Both receipt tail and folded totals must equal, not just final wallet.
        const auto bulk = SettleAllowance(initial, 1399, CommerceMaxInstant);
        auto split = initial;
        std::uint32_t seed = 81;
        while (split.AllowanceSettledThrough < CommerceMaxInstant)
        {
            seed = seed * 1664525U + 1013904223U;
            const auto end = std::min(CommerceMaxInstant, split.AllowanceSettledThrough + 1 + seed % 120003U);
            auto step = SettleAllowance(split, split.AllowanceSettledThrough, end);
            R81Expect(step.Ok() && IsCommerceStateValid(step.State), "long split interval stays valid");
            split = step.State;
        }
        R81Expect(bulk.Ok() && bulk.State == split, "100-year split/bulk states are identical");
        const auto periods = (CommerceMaxInstant - 1399) / CommerceWeekMinutes;
        R81Expect(split.WalletPaise == amounts[rank] * (periods + 1) && split.TransactionRevision == static_cast<std::uint64_t>(periods), "all long-skip boundaries counted");
        R81Expect(split.RecentReceipts.size() == 64 && split.FoldedReceiptCount == static_cast<std::uint64_t>(periods - 64), "bounded credit history");
        R81Expect(split.FoldedCreditsPaise == (periods - 64) * amounts[rank] && split.FoldedDebitsPaise == 0, "folded credits reconcile");
    }
    R81Expect(InitialiseCommerce(-1, 0).Error == CommerceError::InvalidRank && InitialiseCommerce(5, 0).Error == CommerceError::InvalidRank, "invalid funding ranks refuse");
    R81Expect(InitialiseCommerce(0, -1).Error == CommerceError::InvalidTime &&
        InitialiseCommerce(0, std::numeric_limits<GameInstant>::max()).Error == CommerceError::InvalidTime, "extreme creation time refuses");
    const auto end = R81New(0, CommerceMaxInstant);
    R81Expect(end.NextAllowanceDue == CommerceMaxInstant + CommerceWeekMinutes, "terminal cursor remains representable");
    const auto initial = R81New();
    auto duplicateBoundary = SettleAllowance(initial, 0, 2 * CommerceWeekMinutes).State;
    duplicateBoundary.RecentReceipts[1].At = CommerceWeekMinutes;
    R81Expect(!IsCommerceStateValid(duplicateBoundary), "duplicate allowance boundary refused despite balanced money");
    for (const auto to : {GameInstant{-1}, CommerceMaxInstant + 1, std::numeric_limits<GameInstant>::max()})
    {
        const auto result = SettleAllowance(initial, 0, to);
        R81Expect(result.Error == CommerceError::InvalidTime && result.State == initial, "invalid settlement atomic");
    }
}

void R81Purchases()
{
    auto state = R81New();
    const auto entry = R81Entry(R81Tea);
    const auto quote = QuoteFor(entry, R81Tea, 2, state);
    R81Expect(quote.Ok() && quote.TotalPaise == 5000 && quote.WalletAfterPaise == 7000 && quote.ExpectedRevision == 0, "portable quote arithmetic");
    const auto bought = ApplyPurchase(state, entry, R81Tea, 2, quote.ExpectedRevision, R81Ready());
    R81Expect(bought.Ok() && bought.Receipt && bought.State.WalletPaise == 7000 && bought.State.Inventory[0] == CommerceStack{R81Tea.Id, 2}, "debit/delivery commit");
    R81Expect(bought.State.TransactionRevision == 1 && bought.State.RecentReceipts.back() == *bought.Receipt && IsCommerceStateValid(bought.State), "revision/receipt commit");
    R81Expect(bought.Receipt->At == 0 && bought.Receipt->UnitPricePaise == 2500 && bought.Receipt->BalanceBeforePaise == 12000, "receipt contains actual price/time/balances");
    R81PurchaseRefusal(bought.State, entry, R81Tea, 2, 0, R81Ready(), CommerceError::StaleRevision);
    const auto retry = ApplyPurchase(bought.State, entry, R81Tea, 1, 1, R81Ready());
    R81Expect(retry.Ok() && retry.State.Inventory[0].Quantity == 3 && retry.State.TransactionRevision == 2, "fresh revision allows distinct purchase");
    for (int flag = 0; flag < 7; ++flag)
    {
        auto context = R81Ready();
        constexpr CommerceError errors[] = {CommerceError::Closed, CommerceError::KeeperAway, CommerceError::NotAtSafePause,
            CommerceError::NotAtCounter, CommerceError::PlayerAsleep, CommerceError::PlayerBusy, CommerceError::WorldUnsettled};
        switch (flag)
        {
        case 0: context.ShopOpen = false; break;
        case 1: context.KeeperPresent = false; break;
        case 2: context.AtSafePause = false; break;
        case 3: context.AtCounter = false; break;
        case 4: context.PlayerAwake = false; break;
        case 5: context.PlayerFree = false; break;
        case 6: context.WorldSettled = false; break;
        }
        R81PurchaseRefusal(state, entry, R81Tea, 1, 0, context, errors[flag]);
    }
    R81PurchaseRefusal(state, entry, R81Tea, 1, 0, R81Ready(1), CommerceError::InvalidTime);
    R81PurchaseRefusal(state, entry, R81Tea, 5, 0, R81Ready(), CommerceError::InsufficientFunds);
    for (const auto quantity : {0, -1, 121, std::numeric_limits<std::int32_t>::max()})
        R81PurchaseRefusal(state, entry, R81Tea, quantity, 0, R81Ready(), CommerceError::InvalidQuantity);
    auto invalid = R81Tea; invalid.Id = "wrong id";
    R81PurchaseRefusal(state, entry, invalid, 1, 0, R81Ready(), CommerceError::InvalidItem);
    R81PurchaseRefusal(state, R81Entry(R81Food), R81Tea, 1, 0, R81Ready(), CommerceError::InvalidItem);
    R81PurchaseRefusal(state, R81Entry(R81Tea, 0), R81Tea, 1, 0, R81Ready(), CommerceError::InvalidItem);
    R81PurchaseRefusal(state, R81Entry(R81Tea, -1), R81Tea, 1, 0, R81Ready(), CommerceError::InvalidItem);
    const auto expensive = R81Entry(R81Tea, std::numeric_limits<std::int64_t>::max());
    R81PurchaseRefusal(state, expensive, R81Tea, 2, 0, R81Ready(), CommerceError::ArithmeticOverflow);
    const auto extremeQuote = QuoteFor(expensive, R81Tea, 1, state);
    R81Expect(extremeQuote.Ok() && extremeQuote.WalletAfterPaise == 12000 - std::numeric_limits<std::int64_t>::max(), "max price quote cannot wrap");
    R81PurchaseRefusal(state, expensive, R81Tea, 1, 0, R81Ready(), CommerceError::InsufficientFunds);
    state.TransactionRevision = std::numeric_limits<std::uint64_t>::max();
    R81PurchaseRefusal(state, entry, R81Tea, 1, state.TransactionRevision, R81Ready(), CommerceError::ArithmeticOverflow);
    const auto overflow = SettleAllowance(state, 0, CommerceWeekMinutes * 3);
    R81Expect(overflow.Error == CommerceError::ArithmeticOverflow && overflow.State == state && !overflow.Receipt, "allowance revision overflow is atomic");
    state.TransactionRevision -= 1;
    const auto lateOverflow = SettleAllowance(state, 0, CommerceWeekMinutes * 2);
    R81Expect(lateOverflow.Error == CommerceError::ArithmeticOverflow && lateOverflow.State == state, "late allowance failure rolls back earlier credits");
    // Equal purchasing rights at every funding rank, including institutional care.
    for (int rank = 0; rank <= 4; ++rank)
        R81Expect(ApplyPurchase(R81New(rank), entry, R81Tea, 1, 0, R81Ready()).Ok(), "rank-independent rights");
}

void R81Capacity()
{
    auto state = R81New(4);
    const auto entry = R81Entry(R81Food, 1);
    auto full = ApplyPurchase(state, entry, R81Food, 120, 0, R81Ready());
    R81Expect(full.Ok(), "twelve stacks can hold 120 units");
    for (const auto& stack : full.State.Inventory) R81Expect(stack.Quantity == 10, "each full stack has ten units");
    R81PurchaseRefusal(full.State, entry, R81Food, 1, 1, R81Ready(), CommerceError::StackFull);
    R81PurchaseRefusal(full.State, R81Entry(R81Tea, 1), R81Tea, 1, 1, R81Ready(), CommerceError::BagFull);
    state.Inventory[0] = {R81Food.Id, 9};
    state.Inventory[2] = {R81Food.Id, 7};
    auto filled = ApplyPurchase(state, entry, R81Food, 16, 0, R81Ready());
    R81Expect(filled.Ok() && filled.State.Inventory[0].Quantity == 10 && filled.State.Inventory[2].Quantity == 10 &&
        filled.State.Inventory[1].Quantity == 10 && filled.State.Inventory[3].Quantity == 2, "partial stacks precede empty slots in stable order");
    auto limited = full.State; limited.Inventory[3].Quantity = 9;
    R81PurchaseRefusal(limited, entry, R81Food, 2, 1, R81Ready(), CommerceError::StackFull);
    R81Expect(ApplyPurchase(limited, entry, R81Food, 1, 1, R81Ready()).Ok(), "exact remaining capacity accepted");
}

void R81Consumption()
{
    auto state = ApplyPurchase(R81New(), R81Entry(R81Tea, 1), R81Tea, 3, 0, R81Ready()).State;
    ConditionState condition{99500, 500, 70000, 80000};
    auto used = Consume(state, condition, R81Tea, 0, R81Free);
    R81Expect(used.Ok() && used.Condition.Energy == MeterMax && used.Condition.Hunger == condition.Hunger &&
        used.Condition.Health == condition.Health && used.Condition.Stress == condition.Stress, "tea clamps only Energy");
    R81Expect(used.State.Inventory[0].Quantity == 2 && used.State.TransactionRevision == 2 &&
        used.State.WalletPaise == state.WalletPaise && used.State.RecentReceipts == state.RecentReceipts && used.State.NextTeaEligibleAt == 60, "tea removes one without money/receipt changes");
    R81UseRefusal(used.State, used.Condition, R81Tea, 0, R81Free, CommerceError::Cooldown);
    auto before = SettleAllowance(used.State, 0, 59).State;
    R81UseRefusal(before, condition, R81Tea, 59, R81Free, CommerceError::Cooldown);
    auto due = SettleAllowance(before, 59, 60).State;
    auto exact = Consume(due, ConditionState{0, 0, 0, 0}, R81Tea, 60, R81Free);
    R81Expect(exact.Ok() && exact.Condition.Energy == 2000 && exact.State.NextTeaEligibleAt == 120, "tea exact cooldown boundary and fixed effect");
    auto otherTea = R81Tea; otherTea.Id = "item.other_tea";
    exact.State.Inventory[1] = {otherTea.Id, 1};
    R81UseRefusal(exact.State, condition, otherTea, 60, R81Free, CommerceError::Cooldown);
    auto food = R81Food; food.HungerRelief = 5000;
    auto fed = ApplyPurchase(R81New(), R81Entry(food, 1), food, 2, 0, R81Ready()).State;
    auto meal = Consume(fed, condition, food, 0, R81Free);
    R81Expect(meal.Ok() && meal.Condition.Hunger == 0 && meal.Condition.Energy == condition.Energy, "food clamps Hunger without energy bonus");
    auto second = Consume(meal.State, meal.Condition, food, 0, R81Free);
    R81Expect(second.Ok() && second.State.Inventory[0] == CommerceStack{} && IsCommerceStateValid(second.State), "food no added cooldown and empty slot cleared");
    R81UseRefusal(second.State, condition, food, 0, R81Free, CommerceError::NotOwned);
    for (const auto amount : {999, 5001, std::numeric_limits<std::int32_t>::max()})
    {
        auto invalid = food; invalid.HungerRelief = amount;
        R81UseRefusal(fed, condition, invalid, 0, R81Free, CommerceError::InvalidItem);
    }
    R81UseRefusal(fed, condition, food, 0, {false, false, true}, CommerceError::PlayerAsleep);
    R81UseRefusal(fed, condition, food, 0, {true, true, true}, CommerceError::PlayerBusy);
    R81UseRefusal(fed, condition, food, 0, {true, false, false}, CommerceError::WorldUnsettled);
    R81UseRefusal(fed, condition, food, 1, R81Free, CommerceError::InvalidTime);
    auto invalidCondition = condition; invalidCondition.Health = -1;
    R81UseRefusal(fed, invalidCondition, food, 0, R81Free, CommerceError::InvalidState);
    auto supply = ApplyPurchase(R81New(), R81Entry(R81Supply, 1), R81Supply, 1, 0, R81Ready()).State;
    R81UseRefusal(supply, condition, R81Supply, 0, R81Free, CommerceError::UseUnavailable);
    auto pen = R81Supply; pen.Id = "item.pen"; pen.Effect = CommerceEffect::None;
    supply.Inventory[1] = {pen.Id, 1};
    R81UseRefusal(supply, condition, pen, 0, R81Free, CommerceError::UseUnavailable);
    fed.TransactionRevision = std::numeric_limits<std::uint64_t>::max();
    R81UseRefusal(fed, condition, food, 0, R81Free, CommerceError::ArithmeticOverflow);
    auto terminal = R81New(0, CommerceMaxInstant); terminal.Inventory[0] = {R81Tea.Id, 1};
    R81Expect(Consume(terminal, condition, R81Tea, CommerceMaxInstant, R81Free).Ok(), "terminal tea eligibility stays representable");
}

void R81FoldingAndDeterminism()
{
    auto state = R81New(4);
    auto replica = state;
    ConditionState condition;
    ConditionState other = condition;
    const auto entry = R81Entry(R81Food, 17);
    for (int i = 0; i < 400; ++i)
    {
        const auto to = state.AllowanceSettledThrough + 113;
        auto elapsed = SettleAllowance(state, state.AllowanceSettledThrough, to);
        auto replay = SettleAllowance(replica, replica.AllowanceSettledThrough, to);
        R81Expect(elapsed.Ok() && replay.Ok(), "deterministic time inputs accepted");
        auto bought = ApplyPurchase(elapsed.State, entry, R81Food, 1, elapsed.State.TransactionRevision, R81Ready(to));
        auto again = ApplyPurchase(replay.State, entry, R81Food, 1, replay.State.TransactionRevision, R81Ready(to));
        R81Expect(bought.Ok() && bought.State == again.State && bought.Receipt == again.Receipt, "purchase determinism including fold");
        auto used = Consume(bought.State, condition, R81Food, to, R81Free);
        auto duplicate = Consume(again.State, other, R81Food, to, R81Free);
        R81Expect(used.Ok() && used.State == duplicate.State && used.Condition == duplicate.Condition && IsCommerceStateValid(used.State), "use determinism including revision gaps");
        state = used.State; condition = used.Condition;
        replica = duplicate.State; other = duplicate.Condition;
        std::int64_t balance = state.Funding.StartingWalletPaise + state.FoldedCreditsPaise - state.FoldedDebitsPaise;
        for (const auto& receipt : state.RecentReceipts)
        {
            R81Expect(receipt.BalanceBeforePaise == balance, "fold preserves receipt-chain opening balance");
            balance += receipt.Kind == CommerceReceiptKind::Allowance ? receipt.TotalPaise : -receipt.TotalPaise;
            R81Expect(receipt.BalanceAfterPaise == balance, "receipt-chain closing balance");
        }
        R81Expect(balance == state.WalletPaise && state.RecentReceipts.size() <= 64, "monetary reconciliation and bound");
    }
    R81Expect(state.FoldedCreditsPaise > 0 && state.FoldedDebitsPaise > 0 && state.FoldedThroughRevision > 0, "mixed credits/debits folded");
    R81PurchaseRefusal(state, entry, R81Food, 1, 0, R81Ready(state.AllowanceSettledThrough), CommerceError::StaleRevision);
    R81PurchaseRefusal(state, entry, R81Food, 1, state.FoldedThroughRevision,
        R81Ready(state.AllowanceSettledThrough), CommerceError::StaleRevision);
    auto corrupt = state; ++corrupt.WalletPaise;
    R81Expect(!IsCommerceStateValid(corrupt), "wallet tamper refused");
    R81PurchaseRefusal(corrupt, entry, R81Food, 1, corrupt.TransactionRevision,
        R81Ready(corrupt.AllowanceSettledThrough), CommerceError::InvalidState);
    R81UseRefusal(corrupt, condition, R81Food, corrupt.AllowanceSettledThrough, R81Free, CommerceError::InvalidState);
    R81Expect(SettleAllowance(corrupt, corrupt.AllowanceSettledThrough, corrupt.AllowanceSettledThrough).Error == CommerceError::InvalidState, "invalid settlement state refused");
    corrupt = state; ++corrupt.FoldedDebitsPaise;
    R81Expect(!IsCommerceStateValid(corrupt), "folded total tamper refused");
    corrupt = state; corrupt.RecentReceipts[1].Revision = corrupt.RecentReceipts[0].Revision;
    R81Expect(!IsCommerceStateValid(corrupt), "duplicate receipt revision refused");
    corrupt = state; ++corrupt.RecentReceipts[0].TotalPaise;
    R81Expect(!IsCommerceStateValid(corrupt), "receipt arithmetic tamper refused");
    corrupt = state; corrupt.Funding.HouseholdResourcesRank = 0;
    R81Expect(!IsCommerceStateValid(corrupt), "funding amount/rank mismatch refused");
    corrupt = state; corrupt.NextAllowanceDue = std::numeric_limits<GameInstant>::max();
    R81Expect(!IsCommerceStateValid(corrupt), "cursor tamper refused");
    corrupt = state; corrupt.Inventory[0] = {R81Food.Id, 11};
    R81Expect(!IsCommerceStateValid(corrupt), "overfull stack refused");
    corrupt = state; corrupt.FoldedCreditsPaise = std::numeric_limits<std::int64_t>::max();
    R81Expect(!IsCommerceStateValid(corrupt), "overflowing folded reconstruction refused");
    corrupt = state; corrupt.RecentReceipts.push_back(corrupt.RecentReceipts.back());
    R81Expect(!IsCommerceStateValid(corrupt), "unbounded history refused");
    corrupt = state; corrupt.RecentReceipts.back().Kind = static_cast<CommerceReceiptKind>(255);
    R81Expect(!IsCommerceStateValid(corrupt), "unknown receipt kind refused");
    corrupt = state; corrupt.AllowanceSettledThrough = std::numeric_limits<GameInstant>::max();
    R81Expect(!IsCommerceStateValid(corrupt), "extreme persisted time refuses without arithmetic overflow");
    corrupt = state; corrupt.Inventory[0] = {R81Food.Id, 0};
    R81Expect(!IsCommerceStateValid(corrupt), "empty slot has canonical empty ID");
    for (int i = 1; i <= static_cast<int>(CommerceError::Cooldown); ++i)
        R81Expect(!CommerceErrorCode(static_cast<CommerceError>(i)).empty(), "each refusal has stable code");
}

void R81FoldedMetadata()
{
    const auto initial = R81New();
    const auto elapsed = SettleAllowance(initial, 0, 66 * CommerceWeekMinutes);
    R81Expect(elapsed.Ok() && elapsed.State.FoldedReceiptCount == 2 &&
        elapsed.State.FoldedThroughRevision == 2, "exact folded prefix after 66 credits");
    auto corrupt = elapsed.State;
    // Balanced money and ordered revisions alone cannot justify a phantom
    // purchase in the prefix: a purchase must have a positive debit.
    ++corrupt.FoldedReceiptCount;
    ++corrupt.FoldedThroughRevision;
    ++corrupt.TransactionRevision;
    for (auto& receipt : corrupt.RecentReceipts) ++receipt.Revision;
    R81Expect(!IsCommerceStateValid(corrupt), "folded purchase count requires a positive debit");
    corrupt = elapsed.State;
    ++corrupt.FoldedDebitsPaise;
    --corrupt.WalletPaise;
    for (auto& receipt : corrupt.RecentReceipts)
    {
        --receipt.BalanceBeforePaise;
        --receipt.BalanceAfterPaise;
    }
    R81Expect(!IsCommerceStateValid(corrupt), "credit-only folded prefix cannot debit money");

    auto state = SettleAllowance(initial, 0, CommerceWeekMinutes).State;
    for (int i = 0; i < 65; ++i)
    {
        const auto bought = ApplyPurchase(state, R81Entry(R81Food, 1), R81Food, 1,
            state.TransactionRevision, R81Ready(CommerceWeekMinutes));
        const auto used = Consume(bought.State, ConditionState{}, R81Food, CommerceWeekMinutes, R81Free);
        R81Expect(bought.Ok() && used.Ok(), "one-paise purchase and use accepted");
        state = used.State;
    }
    R81Expect(state.FoldedReceiptCount == 2 && state.FoldedCreditsPaise == 12000 && state.FoldedDebitsPaise == 1 &&
        IsCommerceStateValid(state), "minimum folded purchase debit is valid");
    corrupt = state;
    for (auto& receipt : corrupt.RecentReceipts) receipt.At = 0;
    R81Expect(!IsCommerceStateValid(corrupt), "recent transactions cannot predate folded allowance credits");
    ++state.FoldedReceiptCount; // revision gaps leave room, but the debit does not
    R81Expect(!IsCommerceStateValid(state), "folded purchase count cannot exceed paise debited");
}
} // namespace

int main()
{
    R81FundingAndAllowance();
    R81Purchases();
    R81Capacity();
    R81Consumption();
    R81FoldingAndDeterminism();
    R81FoldedMetadata();
    std::cout << R81Assertions << " commerce assertions passed\n";
}
