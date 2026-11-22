#pragma once

#include "Commerce.h"
#include "NpcBehaviour.h"

#include <span>

namespace PerfectWorld
{
// R83 adapter for R8-02: borrowed immutable content, never widget readiness or prices.
// Keep these strings, spans, schedules and travel tables alive for the sim/replay.
struct CommerceOpeningOverride
{
    GameInstant From = 0;
    GameInstant To = 0; // exclusive; overrides the weekly window
    bool Open = false;
};

struct CommerceServiceBinding
{
    std::string_view ShopId;
    std::string_view KeeperId;
    std::string_view CounterAnchorId; // player's physical approach anchor
    const PersonSchedule* Schedule = nullptr;
    const ScheduleTravelTable* Travel = nullptr;
    // Staff anchors at which this keeper can actually provide this service.
    std::span<const std::string_view> KeeperAnchors;
    std::uint8_t WeekdayMask = ScheduleAllWeekdays;
    std::int32_t OpenMinute = 0;
    std::int32_t CloseMinute = 0; // exclusive, no overnight window
    std::span<const CommerceOpeningOverride> Overrides;
};

struct WorldCommerceCatalogues
{
    std::span<const CommerceItem> Items;
    std::span<const CommerceCatalogueEntry> Entries;
    std::span<const CommerceServiceBinding> Shops;
};

struct CommerceServiceView
{
    std::string ShopId;
    std::string KeeperId;
    std::string CounterAnchorId;
    bool Requested = false;
    CommercePurchaseContext Context;
    CommerceError Error = CommerceError::None;
    std::optional<GameInstant> ReadyAt;
    [[nodiscard]] bool Ready() const noexcept { return Error == CommerceError::None; }
};
} // namespace PerfectWorld
