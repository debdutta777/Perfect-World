#pragma once

#include "Commerce.h"
#include "ContentRegistry.h"

#include <span>

namespace PerfectWorld
{
struct CommerceOpeningWindow
{
    std::int32_t Days = 127; // Sunday bit 0, as in Schedules.h
    std::int32_t StartMinute = 0;
    std::int32_t EndMinute = 0; // exclusive; overnight openings need two windows
    friend bool operator==(const CommerceOpeningWindow&, const CommerceOpeningWindow&) = default;
};
struct CommerceKeeper
{
    std::string PersonId;
    std::string CounterAnchor;
    friend bool operator==(const CommerceKeeper&, const CommerceKeeper&) = default;
};
struct CommerceShop
{
    std::string Id;
    std::string Name;
    std::string CounterAnchor;
    std::vector<CommerceKeeper> Keepers; // authored order, primary then assistant
    std::vector<CommerceOpeningWindow> Opening;
    std::vector<CommerceCatalogueEntry> Entries; // sorted by ItemId on load
    friend bool operator==(const CommerceShop&, const CommerceShop&) = default;
};
struct CommerceContent
{
    std::vector<CommerceItem> Items; // stable ID order
    std::vector<CommerceShop> Shops; // stable ID order
    std::vector<CommerceFundingPolicy> Funding; // rank order
    friend bool operator==(const CommerceContent&, const CommerceContent&) = default;
};
struct CommerceContentResult
{
    CommerceContent Catalogue; // empty on any hard error
    std::vector<ContentIssue> Issues;
    [[nodiscard]] bool Ok() const { return Issues.empty(); }
};

// Strict typed decoding also protects callers with a manually constructed Db.
// Item and stratum references are hard registry refs. Person/anchor IDs are lane
// bindings (as in npc_schedule), with keeper registration validated separately.
[[nodiscard]] PERFECTWORLDCORE_API CommerceContentResult LoadCommerceContent(const ContentDb& db);
[[nodiscard]] PERFECTWORLDCORE_API const CommerceItem* FindCommerceItem(
    const CommerceContent& content, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const CommerceShop* FindCommerceShop(
    const CommerceContent& content, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const CommerceCatalogueEntry* FindCommerceEntry(
    const CommerceShop& shop, std::string_view itemId);
[[nodiscard]] PERFECTWORLDCORE_API const CommerceFundingPolicy* FindCommerceFunding(
    const CommerceContent& content, std::int32_t rank);
[[nodiscard]] PERFECTWORLDCORE_API bool IsCommerceShopOpen(const CommerceShop& shop, GameInstant at);

struct CommerceKeeperUnavailability
{
    std::string ShopId;
    std::string PersonId;
    std::string Reason; // "keeper not registered"; soft, never a load failure
    friend bool operator==(const CommerceKeeperUnavailability&, const CommerceKeeperUnavailability&) = default;
};
// Supply the actual registered cast, never IDs copied from the commerce JSON.
[[nodiscard]] PERFECTWORLDCORE_API std::vector<CommerceKeeperUnavailability> ValidateCommerceKeepers(
    const CommerceContent& content, std::span<const std::string_view> registeredPeople);
[[nodiscard]] PERFECTWORLDCORE_API bool HasRegisteredCommerceKeeper(
    const CommerceShop& shop, std::span<const std::string_view> registeredPeople);
// Registration and opening hours do NOT establish service readiness. R8-03 must
// resolve the registered keeper's schedule, presence and safe pause at confirm.

namespace ContentSchema
{
struct TypeSchema;
const TypeSchema& R82CommerceItemSchema();
const TypeSchema& R82CommerceShopSchema();
const TypeSchema& R82CommerceFundingSchema();
void ValidateCommerceContentSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
}
} // namespace PerfectWorld
