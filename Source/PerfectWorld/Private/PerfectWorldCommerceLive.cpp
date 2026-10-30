#include "PerfectWorldCommerceLive.h"

#include "PerfectWorldLaneLifeSubsystem.h"

namespace PWCommerce
{
namespace
{
PerfectWorld::CommerceError R88Error(const PerfectWorld::CommandResult& Result)
{
    if (Result.Accepted) return PerfectWorld::CommerceError::None;
    for (int32 Index = 0; Index <= static_cast<int32>(PerfectWorld::CommerceError::Cooldown); ++Index)
    {
        const auto Error = static_cast<PerfectWorld::CommerceError>(Index);
        if (PerfectWorld::CommerceErrorCode(Error) == Result.Why.Code) return Error;
    }
    return PerfectWorld::CommerceError::InvalidState;
}

PerfectWorld::CommandResult R88Submit(const FCmLiveAccess& Access, PerfectWorld::CommandPayload Payload)
{
    if (Access.Submit) return Access.Submit({PerfectWorld::CommandSource::Player, MoveTemp(Payload)});
    PerfectWorld::CommandResult Result;
    Result.Why.Code = std::string(PerfectWorld::CommerceErrorCode(PerfectWorld::CommerceError::InvalidState));
    return Result;
}

FText R88ItemName(const PerfectWorld::CommerceContent& Data, std::string_view Id)
{
    const auto* Item = PerfectWorld::FindCommerceItem(Data, Id);
    return PWLaneLife::ToText(Item != nullptr ? Item->Name : Id);
}

FText R88ShopName(const PerfectWorld::CommerceContent& Data, std::string_view Id)
{
    const auto* Shop = PerfectWorld::FindCommerceShop(Data, Id);
    return PWLaneLife::ToText(Shop != nullptr ? Shop->Name : Id);
}
}

FCmLiveCommerceWorld::FCmLiveCommerceWorld(const PerfectWorld::CommerceContent& Content, FCmLiveAccess InAccess)
    : Data(Content), Access(MoveTemp(InAccess)) {}

FCmServiceView FCmLiveCommerceWorld::BeginService(const FName ShopId)
{
    FCmServiceView View;
    View.ShopId = ShopId;
    const auto Id = PWLaneLife::ToStd(ShopId);
    View.ShopName = R88ShopName(Data, Id);
    const auto* Sim = Access.Read ? Access.Read() : nullptr;
    if (Sim == nullptr)
    {
        View.Error = PerfectWorld::CommerceError::InvalidState;
        View.Code = PWLaneLife::ToFString(PerfectWorld::CommerceErrorCode(View.Error));
        return View;
    }
    const std::string Anchor = Access.Approach ? Access.Approach(ShopId) : std::string();
    auto Service = Sim->CommerceService(Id);
    if (Service.Requested && Anchor.empty()) EndService(ShopId);
    if (!Service.Requested || Anchor.empty())
    {
        const auto Result = R88Submit(Access, PerfectWorld::CommerceServiceRequest{Id, Anchor});
        View.Code = PWLaneLife::ToFString(Result.Why.Code);
        View.Error = R88Error(Result);
        if (!Result.Accepted) return View;
    }
    Sim = Access.Read();
    if (Sim == nullptr)
    {
        View.Error = PerfectWorld::CommerceError::InvalidState;
        View.Code = PWLaneLife::ToFString(PerfectWorld::CommerceErrorCode(View.Error));
        return View;
    }
    Service = Sim->CommerceService(Id);
    View.Error = Service.Error;
    View.bPending = Service.Requested && Service.Error == PerfectWorld::CommerceError::NotAtSafePause;
    if (!Service.Ready()) View.Code = PWLaneLife::ToFString(PerfectWorld::CommerceErrorCode(Service.Error));
    View.WalletPaise = Sim->State().Commerce.WalletPaise;
    View.Revision = Sim->State().Commerce.TransactionRevision;
    if (!Service.Ready()) return View; // pending request stays live; counter retries while time runs
    const auto* Shop = PerfectWorld::FindCommerceShop(Data, Id);
    if (Shop == nullptr) { View.Error = PerfectWorld::CommerceError::InvalidItem; return View; }
    for (const auto& Entry : Shop->Entries)
    {
        const auto* Item = PerfectWorld::FindCommerceItem(Data, Entry.ItemId);
        if (Item == nullptr) continue;
        View.Goods.Add({PWLaneLife::ToName(Item->Id), PWLaneLife::ToText(Item->Name),
            PWLaneLife::ToText(Item->Unit), Entry.UnitPricePaise, Item->Effect});
    }
    return View;
}

FCmQuoteView FCmLiveCommerceWorld::Quote(const FName ShopId, const FName ItemId, const int32 Quantity)
{
    FCmQuoteView View;
    const auto* Sim = Access.Read ? Access.Read() : nullptr;
    if (Sim == nullptr)
    {
        View.Quote.Error = View.Blocked = PerfectWorld::CommerceError::InvalidState;
        return View;
    }
    const auto Shop = PWLaneLife::ToStd(ShopId), Item = PWLaneLife::ToStd(ItemId);
    View.Quote = Sim->QuoteCommerce(Shop, Item, Quantity);
    const auto* Definition = PerfectWorld::FindCommerceShop(Data, Shop);
    const auto* Entry = Definition != nullptr ? PerfectWorld::FindCommerceEntry(*Definition, Item) : nullptr;
    const auto* Good = PerfectWorld::FindCommerceItem(Data, Item);
    View.Blocked = View.Quote.Error;
    if (View.Quote.Ok() && Entry != nullptr && Good != nullptr)
    {
        auto Context = Sim->CommerceService(Shop).Context;
        Context.AtCounter = Context.AtCounter && Access.Approach && !Access.Approach(ShopId).empty();
        View.Blocked = PerfectWorld::ValidatePurchase(Sim->State().Commerce, *Entry, *Good,
            Quantity, View.Quote.ExpectedRevision, Context);
    }
    return View;
}

FCmBuyResult FCmLiveCommerceWorld::Buy(const FName ShopId, const FName ItemId, const int32 Quantity, const uint64 ExpectedRevision)
{
    FCmBuyResult View;
    // Withdraw the physical approach before the reducer revalidates a confirmation made after walking away.
    if (!Access.Approach || Access.Approach(ShopId).empty()) EndService(ShopId);
    const auto Result = R88Submit(Access, PerfectWorld::CommerceBuy{
        PWLaneLife::ToStd(ShopId), PWLaneLife::ToStd(ItemId), Quantity, ExpectedRevision});
    View.bAccepted = Result.Accepted;
    View.Code = PWLaneLife::ToFString(Result.Why.Code);
    View.Error = R88Error(Result);
    View.ItemName = R88ItemName(Data, PWLaneLife::ToStd(ItemId));
    View.ShopName = R88ShopName(Data, PWLaneLife::ToStd(ShopId));
    const auto* Sim = Access.Read ? Access.Read() : nullptr;
    if (Result.Accepted && Sim != nullptr && !Sim->State().Commerce.RecentReceipts.empty())
    {
        View.Receipt = Sim->State().Commerce.RecentReceipts.back();
        View.bHasReceipt = true;
    }
    return View;
}

void FCmLiveCommerceWorld::EndService(const FName ShopId)
{
    const auto* Sim = Access.Read ? Access.Read() : nullptr;
    const auto Id = PWLaneLife::ToStd(ShopId);
    if (Sim != nullptr && Sim->CommerceService(Id).Requested)
        R88Submit(Access, PerfectWorld::CommerceServiceClose{Id});
}

FCmInventoryView FCmLiveCommerceWorld::ReadInventory()
{
    FCmInventoryView View;
    const auto* Sim = Access.Read ? Access.Read() : nullptr;
    if (Sim == nullptr) return View;
    const auto& World = Sim->State();
    const auto& State = World.Commerce;
    View.bAvailable = true;
    View.WalletPaise = State.WalletPaise;
    const PerfectWorld::CommerceConsumptionContext Context{!World.DayLoop.Asleep,
        World.DayLoop.Runner.Active() || !PerfectWorld::OpenConversationPersonId(World).empty(),
        PerfectWorld::IsWorldQuiescent(World) && PerfectWorld::IsWorldDayInStep(World)};
    for (const auto& Stack : State.Inventory)
    {
        const auto* Item = PerfectWorld::FindCommerceItem(Data, Stack.ItemId);
        if (Item == nullptr || Stack.Quantity == 0) continue;
        FCmStackView Row;
        Row.ItemId = PWLaneLife::ToName(Item->Id);
        Row.Name = PWLaneLife::ToText(Item->Name);
        Row.Unit = PWLaneLife::ToText(Item->Unit);
        Row.Quantity = Stack.Quantity;
        Row.Effect = Item->Effect;
        Row.UseBlocked = PerfectWorld::Consume(State, World.DayLoop.Condition, *Item, World.Clock.Now(), Context).Error;
        if (Row.UseBlocked == PerfectWorld::CommerceError::Cooldown && Item->Effect == PerfectWorld::CommerceEffect::Tea)
            Row.RetryInMinutes = static_cast<int32>(State.NextTeaEligibleAt - World.Clock.Now());
        View.Stacks.Add(MoveTemp(Row));
    }
    for (auto It = State.RecentReceipts.rbegin(); It != State.RecentReceipts.rend(); ++It)
        View.Receipts.Add({*It, R88ItemName(Data, It->ItemId), R88ShopName(Data, It->ShopId)});
    return View;
}

FCmUseResult FCmLiveCommerceWorld::Consume(const FName ItemId)
{
    FCmUseResult View;
    const auto Result = R88Submit(Access, PerfectWorld::InventoryConsume{PWLaneLife::ToStd(ItemId)});
    View.bAccepted = Result.Accepted;
    View.Code = PWLaneLife::ToFString(Result.Why.Code);
    View.Error = R88Error(Result);
    View.ItemName = R88ItemName(Data, PWLaneLife::ToStd(ItemId));
    return View;
}

TOptional<int64> FCmLiveCommerceWorld::ReadWallet()
{
    const auto* Sim = Access.Read ? Access.Read() : nullptr;
    return Sim != nullptr ? TOptional<int64>(Sim->State().Commerce.WalletPaise) : TOptional<int64>();
}
} // namespace PWCommerce
