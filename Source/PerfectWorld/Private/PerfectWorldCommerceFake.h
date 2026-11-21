// R8-05: an in-memory ICmCommerceWorld over the portable commerce rules (Commerce.h), for the automation tests and the acceptance driver.
// It decides nothing itself: every verdict comes from QuoteFor / ValidatePurchase / ApplyPurchase / Consume, exactly as R8-03's reducer
// will use them. What it adds is a tiny authored catalogue, a settable service context (so a test can make the shop closed, the keeper
// away, and so on) and counters of how many times each call was made (so a test can prove a confirmation was submitted once).
#pragma once

#include "PerfectWorldCommerceBridge.h"

namespace PWCommerce
{
class FCmFakeCommerce final : public ICmCommerceWorld
{
public:
    struct FShop
    {
        FName Id;
        FText Name;
        TArray<PerfectWorld::CommerceCatalogueEntry> Entries;
    };

    TArray<FShop> Shops;
    TArray<PerfectWorld::CommerceItem> Items;
    PerfectWorld::CommerceState State;
    PerfectWorld::ConditionState Condition;
    /** What the reducer would rebuild at confirmation. A test flips these to stage a refusal. */
    PerfectWorld::CommercePurchaseContext Context;
    PerfectWorld::CommerceConsumptionContext UseContext;
    /** When false, ReadInventory and ReadWallet report that no commerce state can be read. */
    bool bStateReadable = true;
    /** When set, the next Buy returns this error and changes nothing (a revision race the world resolved against the player). */
    PerfectWorld::CommerceError ForceBuyError = PerfectWorld::CommerceError::None;

    int32 BeginCalls = 0;
    int32 QuoteCalls = 0;
    int32 BuyCalls = 0;
    int32 EndCalls = 0;
    int32 ConsumeCalls = 0;

    /** Shanti Chai (tea, biscuits) and the stationery counter (pen, notebook), a pharmacy-style supply, all open and ready. Rank 0..4 funding. */
    static TSharedRef<FCmFakeCommerce> MakeStandard(const int32 HouseholdRank = 1)
    {
        TSharedRef<FCmFakeCommerce> Fake = MakeShared<FCmFakeCommerce>();
        const PerfectWorld::CommerceStateResult Initial = PerfectWorld::InitialiseCommerce(HouseholdRank, 0);
        Fake->State = Initial.State;
        Fake->Context.Now = Fake->State.AllowanceSettledThrough;
        Fake->Context.ShopOpen = Fake->Context.KeeperPresent = Fake->Context.AtSafePause = Fake->Context.AtCounter = true;
        Fake->Context.PlayerAwake = Fake->Context.PlayerFree = Fake->Context.WorldSettled = true;
        Fake->UseContext.PlayerAwake = true;
        Fake->UseContext.ActivityActive = false;
        Fake->UseContext.WorldSettled = true;

        const auto AddItem = [&Fake](const char* Id, const char* Name, const char* Unit, const PerfectWorld::CommerceEffect Effect, const int32 Relief)
        {
            PerfectWorld::CommerceItem Item;
            Item.Id = Id;
            Item.Name = Name;
            Item.Unit = Unit;
            Item.Effect = Effect;
            Item.HungerRelief = Relief;
            Fake->Items.Add(Item);
        };
        using PerfectWorld::CommerceEffect;
        AddItem("item.tea", "Tea", "cup", CommerceEffect::Tea, 0);
        AddItem("item.biscuits", "Biscuits", "pair", CommerceEffect::Food, 2000);
        AddItem("item.pen", "Pen", "pen", CommerceEffect::None, 0);
        AddItem("item.notebook", "Notebook", "notebook", CommerceEffect::None, 0);
        AddItem("item.plasters", "Plasters", "pack of five", CommerceEffect::UnsupportedSupply, 0);

        FShop& Tea = Fake->Shops.AddDefaulted_GetRef();
        Tea.Id = FName(TEXT("shop.shanti_chai"));
        Tea.Name = FText::FromString(TEXT("Shanti Chai"));
        Tea.Entries.Add(MakeEntry("shop.shanti_chai", "item.tea", 2500));
        Tea.Entries.Add(MakeEntry("shop.shanti_chai", "item.biscuits", 1000));
        FShop& Stationery = Fake->Shops.AddDefaulted_GetRef();
        Stationery.Id = FName(TEXT("shop.stationery"));
        Stationery.Name = FText::FromString(TEXT("Stationery"));
        Stationery.Entries.Add(MakeEntry("shop.stationery", "item.pen", 2000));
        Stationery.Entries.Add(MakeEntry("shop.stationery", "item.notebook", 6000));
        Stationery.Entries.Add(MakeEntry("shop.stationery", "item.plasters", 4000));
        return Fake;
    }

    // ---- ICmCommerceWorld ----

    virtual FCmServiceView BeginService(const FName ShopId) override
    {
        ++BeginCalls;
        FCmServiceView View;
        View.ShopId = ShopId;
        const FShop* Shop = FindShop(ShopId);
        if (Shop == nullptr)
        {
            View.Error = PerfectWorld::CommerceError::InvalidItem;
            return View;
        }
        View.ShopName = Shop->Name;
        View.WalletPaise = State.WalletPaise;
        View.Revision = State.TransactionRevision;
        PerfectWorld::CommerceError FirstError = PerfectWorld::CommerceError::None;
        bool bAnyBuyable = false;
        for (const PerfectWorld::CommerceCatalogueEntry& Entry : Shop->Entries)
        {
            const PerfectWorld::CommerceItem* Item = FindItem(Entry.ItemId);
            if (Item == nullptr)
            {
                continue;
            }
            FCmGood Good;
            Good.ItemId = FName(UTF8_TO_TCHAR(Entry.ItemId.c_str()));
            Good.Name = FText::FromString(UTF8_TO_TCHAR(Item->Name.c_str()));
            Good.Unit = FText::FromString(UTF8_TO_TCHAR(Item->Unit.c_str()));
            Good.UnitPricePaise = Entry.UnitPricePaise;
            Good.Effect = Item->Effect;
            View.Goods.Add(Good);
            const PerfectWorld::CommerceError Verdict = PerfectWorld::ValidatePurchase(State, Entry, *Item, 1, State.TransactionRevision, Context);
            bAnyBuyable |= Verdict == PerfectWorld::CommerceError::None;
            if (FirstError == PerfectWorld::CommerceError::None)
            {
                FirstError = Verdict;
            }
        }
        if (!bAnyBuyable)
        {
            View.Error = FirstError != PerfectWorld::CommerceError::None ? FirstError : PerfectWorld::CommerceError::InvalidItem;
            View.Goods.Reset();
        }
        return View;
    }

    virtual FCmQuoteView Quote(const FName ShopId, const FName ItemId, const int32 Quantity) override
    {
        ++QuoteCalls;
        FCmQuoteView View;
        const PerfectWorld::CommerceCatalogueEntry* Entry = FindEntry(ShopId, ItemId);
        const PerfectWorld::CommerceItem* Item = FindItem(Utf8(ItemId));
        if (Entry == nullptr || Item == nullptr)
        {
            View.Quote.Error = PerfectWorld::CommerceError::InvalidItem;
            View.Blocked = PerfectWorld::CommerceError::InvalidItem;
            return View;
        }
        View.Quote = PerfectWorld::QuoteFor(*Entry, *Item, Quantity, State);
        View.Blocked = View.Quote.Ok()
            ? PerfectWorld::ValidatePurchase(State, *Entry, *Item, Quantity, View.Quote.ExpectedRevision, Context)
            : View.Quote.Error;
        return View;
    }

    virtual FCmBuyResult Buy(const FName ShopId, const FName ItemId, const int32 Quantity, const uint64 ExpectedRevision) override
    {
        ++BuyCalls;
        FCmBuyResult Result;
        const FShop* Shop = FindShop(ShopId);
        const PerfectWorld::CommerceCatalogueEntry* Entry = FindEntry(ShopId, ItemId);
        const PerfectWorld::CommerceItem* Item = FindItem(Utf8(ItemId));
        if (ForceBuyError != PerfectWorld::CommerceError::None)
        {
            Result.Error = ForceBuyError;
            ForceBuyError = PerfectWorld::CommerceError::None;
            return Result;
        }
        if (Shop == nullptr || Entry == nullptr || Item == nullptr)
        {
            Result.Error = PerfectWorld::CommerceError::InvalidItem;
            return Result;
        }
        const PerfectWorld::CommerceStateResult Bought = PerfectWorld::ApplyPurchase(State, *Entry, *Item, Quantity, ExpectedRevision, Context);
        Result.Error = Bought.Error;
        if (!Bought.Ok())
        {
            return Result;
        }
        State = Bought.State;
        Result.bAccepted = true;
        Result.bHasReceipt = Bought.Receipt.has_value();
        if (Bought.Receipt.has_value())
        {
            Result.Receipt = *Bought.Receipt;
        }
        Result.ItemName = FText::FromString(UTF8_TO_TCHAR(Item->Name.c_str()));
        Result.ShopName = Shop->Name;
        return Result;
    }

    virtual void EndService(const FName) override { ++EndCalls; }

    virtual FCmInventoryView ReadInventory() override
    {
        FCmInventoryView View;
        if (!bStateReadable)
        {
            return View;
        }
        View.bAvailable = true;
        View.WalletPaise = State.WalletPaise;
        for (const PerfectWorld::CommerceStack& Stack : State.Inventory)
        {
            const PerfectWorld::CommerceItem* Item = Stack.ItemId.empty() ? nullptr : FindItem(Stack.ItemId);
            if (Item == nullptr)
            {
                continue;
            }
            FCmStackView Row;
            Row.ItemId = FName(UTF8_TO_TCHAR(Item->Id.c_str()));
            Row.Name = FText::FromString(UTF8_TO_TCHAR(Item->Name.c_str()));
            Row.Unit = FText::FromString(UTF8_TO_TCHAR(Item->Unit.c_str()));
            Row.Quantity = Stack.Quantity;
            Row.Effect = Item->Effect;
            const PerfectWorld::CommerceConsumptionResult Trial = PerfectWorld::Consume(State, Condition, *Item, State.AllowanceSettledThrough, UseContext);
            Row.UseBlocked = Trial.Error;
            if (Trial.Error == PerfectWorld::CommerceError::Cooldown && Item->Effect == PerfectWorld::CommerceEffect::Tea)
            {
                Row.RetryInMinutes = static_cast<int32>(FMath::Max<int64>(1, State.NextTeaEligibleAt - State.AllowanceSettledThrough));
            }
            View.Stacks.Add(Row);
        }
        for (int32 Index = static_cast<int32>(State.RecentReceipts.size()) - 1; Index >= 0; --Index)
        {
            FCmReceiptRow Row;
            Row.Receipt = State.RecentReceipts[static_cast<size_t>(Index)];
            const PerfectWorld::CommerceItem* Item = Row.Receipt.ItemId.empty() ? nullptr : FindItem(Row.Receipt.ItemId);
            Row.ItemName = Item != nullptr ? FText::FromString(UTF8_TO_TCHAR(Item->Name.c_str())) : FText::GetEmpty();
            const FName Shop(UTF8_TO_TCHAR(Row.Receipt.ShopId.c_str()));
            const FShop* Found = Row.Receipt.ShopId.empty() ? nullptr : FindShop(Shop);
            Row.ShopName = Found != nullptr ? Found->Name : FText::GetEmpty();
            View.Receipts.Add(Row);
        }
        return View;
    }

    virtual FCmUseResult Consume(const FName ItemId) override
    {
        ++ConsumeCalls;
        FCmUseResult Result;
        const PerfectWorld::CommerceItem* Item = FindItem(Utf8(ItemId));
        if (Item == nullptr)
        {
            Result.Error = PerfectWorld::CommerceError::InvalidItem;
            return Result;
        }
        const PerfectWorld::CommerceConsumptionResult Used = PerfectWorld::Consume(State, Condition, *Item, State.AllowanceSettledThrough, UseContext);
        Result.Error = Used.Error;
        if (Used.Ok())
        {
            State = Used.State;
            Condition = Used.Condition;
            Result.bAccepted = true;
            Result.ItemName = FText::FromString(UTF8_TO_TCHAR(Item->Name.c_str()));
        }
        return Result;
    }

    virtual TOptional<int64> ReadWallet() override
    {
        return bStateReadable ? TOptional<int64>(State.WalletPaise) : TOptional<int64>();
    }

private:
    static std::string Utf8(const FName Name) { return std::string(TCHAR_TO_UTF8(*Name.ToString())); }

    static PerfectWorld::CommerceCatalogueEntry MakeEntry(const char* ShopId, const char* ItemId, const int64 PricePaise)
    {
        PerfectWorld::CommerceCatalogueEntry Entry;
        Entry.ShopId = ShopId;
        Entry.ItemId = ItemId;
        Entry.UnitPricePaise = PricePaise;
        return Entry;
    }

    const FShop* FindShop(const FName ShopId) const
    {
        return Shops.FindByPredicate([ShopId](const FShop& Shop) { return Shop.Id == ShopId; });
    }

    const PerfectWorld::CommerceItem* FindItem(const std::string& Id) const
    {
        return Items.FindByPredicate([&Id](const PerfectWorld::CommerceItem& Item) { return Item.Id == Id; });
    }

    const PerfectWorld::CommerceCatalogueEntry* FindEntry(const FName ShopId, const FName ItemId) const
    {
        const FShop* Shop = FindShop(ShopId);
        const std::string Wanted = Utf8(ItemId);
        return Shop != nullptr
            ? Shop->Entries.FindByPredicate([&Wanted](const PerfectWorld::CommerceCatalogueEntry& Entry) { return Entry.ItemId == Wanted; })
            : nullptr;
    }
};
} // namespace PWCommerce
