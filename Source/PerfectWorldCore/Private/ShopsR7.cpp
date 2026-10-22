#include "ShopsR7.h"

#include <array>

namespace PerfectWorld
{
namespace
{
constexpr ShopWindowR7 R74SweetHours[]{{420, 810}, {930, 1260}};
constexpr ShopWindowR7 R74PharmacyHours[]{{540, 840}, {870, 1020}, {1035, 1380}};
constexpr ShopWindowR7 R74TeaHours[]{{300, 600}, {620, 840}, {880, 1020}, {1035, 1260}};
constexpr ShopWindowR7 R74VendorHours[]{{900, 1080}, {1100, 1260}};
constexpr std::array<ShopStaffR7, 5> R74Staff{{
    {"person.bimal_modak", "npc_schedule.bimal_modak", "Bimal Modak", 52, 170,
     "location.nabapara.bimal_home", "location.nabapara.sweet_shop", "entrance.nabapara.sweet_shop",
     "anchor.nabapara.sweet_shop_counter", "anchor.nabapara.sweet_shop_counter", {6800, 3200}, 300, R74SweetHours},
    {"person.tithi_ghoshal", "npc_schedule.tithi_ghoshal", "Tithi Ghoshal", 24, 159,
     "location.nabapara.tithi_home", "location.nabapara.sweet_shop", "entrance.nabapara.sweet_shop",
     "anchor.nabapara.sweet_shop_assistant_counter", "anchor.nabapara.sweet_shop_counter", {6950, 3200}, 450, R74SweetHours},
    {"person.aparna_lahiri", "npc_schedule.aparna_lahiri", "Aparna Lahiri", 39, 172,
     "location.nabapara.aparna_home", "location.nabapara.pharmacy", "entrance.nabapara.pharmacy",
     "anchor.nabapara.pharmacy_counter", "anchor.nabapara.pharmacy_counter", {9200, 3200}, 300, R74PharmacyHours},
    {"person.gouranga_nandi", "npc_schedule.gouranga_nandi", "Gouranga Nandi", 48, 175,
     "location.nabapara.gouranga_home", "pitch.nabapara.tea", "entrance.nabapara.tea",
     "anchor.nabapara.tea_counter", "anchor.nabapara.tea_counter", {11250, 3400}, 100, R74TeaHours},
    {"person.koushik_mondal", "npc_schedule.koushik_mondal", "Koushik Mondal", 32, 177,
     "location.nabapara.koushik_home", "pitch.nabapara.vendor", "entrance.nabapara.vendor",
     "anchor.nabapara.vendor_counter", "anchor.nabapara.vendor_counter", {12250, 3400}, 100, R74VendorHours},
}};
}

std::span<const ShopStaffR7> ShopStaffR7Definitions() noexcept { return R74Staff; }
bool IsShopStaffR7(std::string_view person) noexcept
{
    for (const auto& staff : R74Staff) { if (staff.PersonId == person) { return true; } }
    return false;
}
bool IsShopOpenR7(const ShopStaffR7& staff, int minute) noexcept
{
    for (const auto& window : staff.Opening)
    {
        if (minute >= window.Open && minute < window.Close) { return true; }
    }
    return false;
}

ShopWorkR7 ShopWorkAtR7(const PersonSchedule& person, const ScheduleResolution& block) noexcept
{
    if (block.Status != ScheduleStatus::Stay || block.Kind != ScheduleDoingKind::Role) { return {}; }
    for (const auto& staff : R74Staff)
    {
        if (staff.PersonId != person.PersonId || staff.CounterId != block.Zone) { continue; }
        if (block.Doing == "serving")
        {
            return {NpcWorkPhase::Serve, "work.serve", "npc.pause.counter_clear", NpcDecisionReason::Counter};
        }
        if (block.Doing == "stocking" && person.PersonId == "person.aparna_lahiri")
        {
            return {NpcWorkPhase::StockRestock, "work.stock", "npc.pause.shelf_item_secured", NpcDecisionReason::Stock};
        }
        if (block.Doing != "cooking") { return {}; }
        if (person.PersonId == "person.bimal_modak" || person.PersonId == "person.tithi_ghoshal")
        {
            return {NpcWorkPhase::PrepareSweets, "work.prepare_sweets", "npc.pause.sweet_tray_set_down", NpcDecisionReason::Counter};
        }
        if (person.PersonId == "person.gouranga_nandi")
        {
            return {NpcWorkPhase::BrewTea, "work.brew_tea", "npc.pause.kettle_set_down", NpcDecisionReason::Counter};
        }
        if (person.PersonId == "person.koushik_mondal")
        {
            return {NpcWorkPhase::MixJhalmuri, "work.mix_jhalmuri", "npc.pause.mixing_bowl_set_down", NpcDecisionReason::Counter};
        }
    }
    return {};
}
} // namespace PerfectWorld
