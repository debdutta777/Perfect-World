#include "WorldCommerce.h"

#include "WorldReducer.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
Rejection R83Refusal(CommerceError error)
{
    return {std::string(CommerceErrorCode(error)), "The commerce rules refused that action."};
}

const CommerceServiceBinding* R83Shop(const WorldCommerceCatalogues& cats, std::string_view id)
{
    const auto found = std::find_if(cats.Shops.begin(), cats.Shops.end(),
        [id](const auto& shop) { return shop.ShopId == id; });
    return found == cats.Shops.end() ? nullptr : &*found;
}

const CommerceItem* R83Item(const WorldCommerceCatalogues& cats, std::string_view id)
{
    const auto found = std::find_if(cats.Items.begin(), cats.Items.end(),
        [id](const auto& item) { return item.Id == id; });
    return found == cats.Items.end() ? nullptr : &*found;
}

const CommerceCatalogueEntry* R83Entry(const WorldCommerceCatalogues& cats, std::string_view shop, std::string_view item)
{
    const auto found = std::find_if(cats.Entries.begin(), cats.Entries.end(),
        [shop, item](const auto& entry) { return entry.ShopId == shop && entry.ItemId == item; });
    return found == cats.Entries.end() ? nullptr : &*found;
}

bool R83Open(const CommerceServiceBinding& shop, GameInstant at)
{
    if (shop.OpenMinute < 0 || shop.CloseMinute > MinutesPerDay || shop.OpenMinute >= shop.CloseMinute)
        return false;
    for (const auto& exception : shop.Overrides)
        if (at >= exception.From && at < exception.To)
            return exception.Open;
    const auto calendar = GameClock::ToCalendar(at);
    const auto minute = calendar.Hour * 60 + calendar.Minute;
    return (shop.WeekdayMask & ScheduleWeekdayBit(calendar.DayOfWeek)) != 0 &&
        minute >= shop.OpenMinute && minute < shop.CloseMinute;
}

CommerceError R83ContextError(const CommercePurchaseContext& context)
{
    if (!context.ShopOpen) return CommerceError::Closed;
    if (!context.KeeperPresent) return CommerceError::KeeperAway;
    if (!context.AtSafePause) return CommerceError::NotAtSafePause;
    if (!context.AtCounter) return CommerceError::NotAtCounter;
    if (!context.PlayerAwake) return CommerceError::PlayerAsleep;
    if (!context.PlayerFree) return CommerceError::PlayerBusy;
    if (!context.WorldSettled) return CommerceError::WorldUnsettled;
    return CommerceError::None;
}
} // namespace

NpcDecision WorldSim::CommerceKeeperDecision(std::string_view shopId) const
{
    const auto* shop = R83Shop(Commerce_, shopId);
    if (shop == nullptr || shop->Schedule == nullptr || shop->Travel == nullptr ||
        shop->KeeperId.empty() || shop->Schedule->PersonId != shop->KeeperId)
    {
        NpcDecision result;
        result.Error = NpcIntentError::InvalidResolution;
        return result;
    }
    const auto history = ServiceHistory_.find(shop->KeeperId);
    const std::span<const NpcInterruptionWindow> windows = history == ServiceHistory_.end()
        ? std::span<const NpcInterruptionWindow>{} : history->second;
    return ResolveNpcDecisionAt(*shop->Schedule, *shop->Travel, State_.Clock.Now(), windows);
}

CommerceServiceView WorldSim::CommerceService(std::string_view shopId) const
{
    CommerceServiceView view;
    view.ShopId = shopId;
    auto& context = view.Context;
    context.Now = State_.Clock.Now();
    context.PlayerAwake = !State_.DayLoop.Asleep;
    context.PlayerFree = !State_.DayLoop.Runner.Active() && OpenConversationPersonId(State_).empty();
    context.WorldSettled = IsWorldQuiescent(State_) && IsWorldDayInStep(State_) &&
        State_.Commerce.AllowanceSettledThrough == context.Now;
    const auto* shop = R83Shop(Commerce_, shopId);
    if (shop == nullptr)
    {
        view.Error = CommerceError::InvalidItem;
        return view;
    }
    view.KeeperId = shop->KeeperId;
    view.CounterAnchorId = shop->CounterAnchorId;
    view.Requested = ServiceShop_ == shopId;
    context.AtCounter = view.Requested;
    context.ShopOpen = R83Open(*shop, context.Now);
    const auto decision = CommerceKeeperDecision(shopId);
    if (decision.Ok())
    {
        // Resolve the un-interrupted schedule too: Chat must never turn a break,
        // travel or a formerly staffed anchor into permission to sell.
        const auto resolved = ResolveSchedule(*shop->Schedule, *shop->Travel, context.Now);
        const auto base = ResolveNpcIntent(resolved, false, false);
        context.KeeperPresent = base.Ok() && base.Intent.State == NpcState::Work &&
            resolved.Status == ScheduleStatus::Stay && decision.Needs.Priority < NpcNeedPriority::Rest &&
            std::find(shop->KeeperAnchors.begin(), shop->KeeperAnchors.end(), resolved.Zone) != shop->KeeperAnchors.end();
        context.AtSafePause = view.Requested && decision.AcceptedAt.has_value() && !decision.DeferredUntil &&
            !decision.ScheduleChanged && decision.Intent.State == NpcState::Chat &&
            decision.Intent.Availability == PersonAvailability::Awake;
        view.ReadyAt = decision.DeferredUntil;
    }
    view.Error = !view.Requested && context.ShopOpen && context.KeeperPresent
        ? CommerceError::NotAtCounter : R83ContextError(context);
    return view;
}

CommerceQuote WorldSim::QuoteCommerce(std::string_view shopId, std::string_view itemId, std::int32_t quantity) const
{
    const auto* item = R83Item(Commerce_, itemId);
    const auto* entry = R83Entry(Commerce_, shopId, itemId);
    if (item != nullptr && entry != nullptr)
        return QuoteFor(*entry, *item, quantity, State_.Commerce);
    CommerceQuote quote;
    quote.Error = CommerceError::InvalidItem;
    return quote;
}

std::optional<Rejection> WorldSim::ApplyCommerceCommand(const Command& command)
{
    const auto now = State_.Clock.Now();
    if (const auto* request = std::get_if<CommerceServiceRequest>(&command.Payload))
    {
        if (!ServiceShop_.empty())
            return Rejection{"commerce.service_active", "Close the current counter service first."};
        if (!IsCommerceStateValid(State_.Commerce)) return R83Refusal(CommerceError::InvalidState);
        if (now != State_.Commerce.AllowanceSettledThrough) return R83Refusal(CommerceError::InvalidTime);
        const auto* shop = R83Shop(Commerce_, request->ShopId);
        if (shop == nullptr) return R83Refusal(CommerceError::InvalidItem);
        const auto view = CommerceService(request->ShopId);
        if (!view.Context.ShopOpen) return R83Refusal(CommerceError::Closed);
        if (!view.Context.KeeperPresent) return R83Refusal(CommerceError::KeeperAway);
        if (request->CounterAnchorId.empty() || request->CounterAnchorId != shop->CounterAnchorId)
            return R83Refusal(CommerceError::NotAtCounter);
        if (!view.Context.PlayerAwake) return R83Refusal(CommerceError::PlayerAsleep);
        if (!view.Context.PlayerFree) return R83Refusal(CommerceError::PlayerBusy);
        if (!view.Context.WorldSettled) return R83Refusal(CommerceError::WorldUnsettled);
        // Retain closed windows for this resolved segment: reopening cannot erase paused work.
        const auto prior = ServiceHistory_.find(shop->KeeperId);
        auto windows = prior == ServiceHistory_.end() ? std::vector<NpcInterruptionWindow>{} : prior->second;
        const auto block = ResolveSchedule(*shop->Schedule, *shop->Travel, now);
        windows.erase(std::remove_if(windows.begin(), windows.end(), [&block](const auto& window) {
            return window.ReleasedAt && *window.ReleasedAt <= block.SegmentStart;
        }), windows.end());
        // Zero-duration windows have no effect, and need not consume the bounded history.
        windows.erase(std::remove_if(windows.begin(), windows.end(), [](const auto& window) {
            return window.ReleasedAt == window.RequestedAt;
        }), windows.end());
        if (windows.size() >= NpcMaxInterruptionWindows)
            return Rejection{"commerce.service_history_full", "This work segment has too many interruptions."};
        windows.push_back({NpcState::Chat, now, {}});
        const auto decision = ResolveNpcDecisionAt(*shop->Schedule, *shop->Travel, now, windows);
        if (!decision.Ok() || (!decision.DeferredUntil &&
            (!decision.AcceptedAt || decision.ScheduleChanged || decision.Intent.State != NpcState::Chat ||
             decision.Intent.Availability != PersonAvailability::Awake)))
            return R83Refusal(CommerceError::NotAtSafePause);
        ServiceHistory_[std::string(shop->KeeperId)] = std::move(windows);
        ServiceShop_ = request->ShopId;
        return std::nullopt; // Accepted pending request; querying ReadyAt never spends time.
    }
    if (const auto* close = std::get_if<CommerceServiceClose>(&command.Payload))
    {
        if (ServiceShop_.empty() || ServiceShop_ != close->ShopId)
            return Rejection{"commerce.service_not_open", "That counter service is not open."};
        const auto* shop = R83Shop(Commerce_, close->ShopId);
        ServiceHistory_.find(shop->KeeperId)->second.back().ReleasedAt = now;
        ServiceShop_.clear();
        return std::nullopt;
    }
    if (const auto* buy = std::get_if<CommerceBuy>(&command.Payload))
    {
        // Missing IDs are represented by invalid value objects, preserving the frozen
        // rule module's state/revision/service refusal precedence on direct commands.
        CommerceItem missingItem;
        CommerceCatalogueEntry missingEntry;
        const auto* item = R83Item(Commerce_, buy->ItemId);
        const auto* entry = R83Entry(Commerce_, buy->ShopId, buy->ItemId);
        auto context = CommerceService(buy->ShopId).Context;
        auto bought = ApplyPurchase(State_.Commerce, entry ? *entry : missingEntry, item ? *item : missingItem,
            buy->Quantity, buy->ExpectedRevision, context);
        if (!bought.Ok()) return R83Refusal(bought.Error);
        State_.Commerce = std::move(bought.State);
        return std::nullopt;
    }
    if (const auto* use = std::get_if<InventoryConsume>(&command.Payload))
    {
        CommerceItem missingItem;
        const auto* item = R83Item(Commerce_, use->ItemId);
        const CommerceConsumptionContext context{!State_.DayLoop.Asleep,
            State_.DayLoop.Runner.Active() || !OpenConversationPersonId(State_).empty(),
            IsWorldQuiescent(State_) && IsWorldDayInStep(State_)};
        auto used = Consume(State_.Commerce, State_.DayLoop.Condition, item ? *item : missingItem, now, context);
        if (!used.Ok()) return R83Refusal(used.Error);
        State_.Commerce = std::move(used.State);
        State_.DayLoop.Condition = used.Condition;
        return std::nullopt;
    }
    return Rejection{"command.unknown", "Unknown command."};
}

std::optional<Rejection> WorldSim::ApplyWorldDayCommand(const WorldDayCommand& command, CommandResult& result)
{
    if (!ServiceShop_.empty())
        return Rejection{"commerce.service_active", "Close the counter service before a day action."};
    if (!IsWorldQuiescent(State_)) return Rejection{"world.unsettled", "Settle due events first."};
    if (!OpenConversationPersonId(State_).empty())
        return Rejection{"time.conversation_open", "Close the conversation before a day action."};
    if (command.Day.Kind == DayCommandKind::ArriveSchool)
        return Rejection{"day.use_school_arrive", "Use the school arrival command with its bound calendar."};
    auto day = State_.DayLoop;
    const auto applied = ApplyDayCommand(day, School_, command.Day);
    if (!applied.Ok())
        return Rejection{std::string(DayLoopErrorCode(applied.Error)), applied.Detail};
    if (day.Now != State_.Clock.Now())
    {
        ChargeTime(day.Now - State_.Clock.Now(), result);
        if (result.Partial)
            return Rejection{"day.charge_interrupted", "Settle the scheduled time before retrying the day action."};
    }
    State_.DayLoop = std::move(day);
    for (const auto& event : applied.Events)
        result.Events.push_back({0, event.At, event});
    return std::nullopt;
}
} // namespace PerfectWorld
