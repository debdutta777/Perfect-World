#include "CommerceContent.h"

#include "ContentSchemas.h"

#include <algorithm>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace
{
// Repeat the schema at this boundary, since ContentDb is publicly constructible.
std::string R82Shape(const ContentSchema::TypeSchema& schema, const JsonValue& data)
{
    using ContentSchema::FieldKind;
    if (!data.IsObject()) return "record must be an object";
    std::set<std::string> keys;
    for (const auto& [key, value] : data.Object)
    {
        (void)value;
        if (!keys.insert(key).second) return "duplicate field " + key;
        if (std::none_of(schema.Fields.begin(), schema.Fields.end(), [&](const auto& f) { return f.Name == key; }))
            return "unknown field " + key;
    }
    for (const auto& field : schema.Fields)
    {
        const auto* value = data.Find(field.Name);
        if (value == nullptr)
        {
            if (field.Required) return "missing required field " + field.Name;
            continue;
        }
        bool valid = false;
        switch (field.Kind)
        {
        case FieldKind::String:
        case FieldKind::Ref: valid = value->IsString(); break;
        case FieldKind::Int:
            valid = value->Integer && *value->Integer >= field.Min && *value->Integer <= field.Max; break;
        case FieldKind::Enum:
            valid = value->IsString() && std::find(field.Values.begin(), field.Values.end(), value->String) != field.Values.end();
            break;
        case FieldKind::ObjectArray:
            if (!value->IsArray()) break;
            for (const auto& child : value->Array)
                if (const auto error = R82Shape(*field.Nested, child); !error.empty()) return field.Name + ": " + error;
            valid = true;
            break;
        default: break; // This boundary uses only the kinds above.
        }
        if (!valid) return "invalid field " + field.Name;
    }
    return {};
}
bool R82Token(std::string_view text, std::string_view prefix)
{
    if (!text.starts_with(prefix) || text.size() <= prefix.size() || text.size() > 128) return false;
    return std::all_of(text.begin(), text.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
    });
}
std::int64_t R82Int(const JsonValue& data, std::string_view field) { return *data.Find(field)->Integer; }
const std::string& R82Text(const JsonValue& data, std::string_view field) { return data.Find(field)->String; }
CommerceEffect R82Effect(std::string_view effect)
{
    if (effect == "tea") return CommerceEffect::Tea;
    if (effect == "food") return CommerceEffect::Food;
    if (effect == "unsupported_supply") return CommerceEffect::UnsupportedSupply;
    return CommerceEffect::None;
}
bool R82Registered(std::string_view person, std::span<const std::string_view> people)
{
    return std::find(people.begin(), people.end(), person) != people.end();
}
}

CommerceContentResult LoadCommerceContent(const ContentDb& db)
{
    CommerceContentResult out;
    std::set<std::int32_t> ranks;
    for (const auto* schema : {&ContentSchema::R82CommerceItemSchema(), &ContentSchema::R82CommerceShopSchema(),
                               &ContentSchema::R82CommerceFundingSchema()})
    {
        for (const auto* record : db.OfType(schema->Type))
        {
            const auto issue = [&](std::string message) { out.Issues.push_back({record->File, record->Id, std::move(message)}); };
            const auto& data = record->Data;
            if (const auto error = R82Shape(*schema, data); !error.empty()) { issue(error); continue; }
            if (db.Find(record->Id) != record || !R82Token(record->Id, schema->IdPrefix) || R82Text(data, "id") != record->Id ||
                R82Text(data, "type") != record->Type || !record->ParentId.empty())
            { issue("commerce metadata differs from its registered identity"); continue; }
            if (record->Type == "commerce_item")
            {
                CommerceItem item{record->Id, R82Text(data, "name"), R82Text(data, "unit"),
                    static_cast<std::int32_t>(R82Int(data, "stackLimit")), R82Effect(R82Text(data, "effect")),
                    static_cast<std::int32_t>(R82Int(data, "hungerRelief"))};
                if (!IsCommerceItemValid(item)) issue("item effect and hunger relief are incompatible");
                out.Catalogue.Items.push_back(std::move(item));
            }
            else if (record->Type == "commerce_funding")
            {
                const auto rank = static_cast<std::int32_t>(R82Int(data, "householdResourcesRank"));
                const CommerceFundingPolicy policy{rank, R82Int(data, "startingWalletPaise"), R82Int(data, "weeklyAllowancePaise")};
                const auto frozen = InitialiseCommerce(rank, 0);
                if (!frozen.Ok() || frozen.State.Funding != policy) issue("funding differs from the frozen R8-01 policy");
                if (!ranks.insert(rank).second) issue("duplicate funding rank");
                const auto* stratum = db.Find(R82Text(data, "stratum"));
                const auto* stratumRank = stratum != nullptr ? stratum->Data.Find("householdResourcesRank") : nullptr;
                if (stratum == nullptr || stratum->Type != "stratum" || stratumRank == nullptr || stratumRank->Integer != rank)
                    issue("funding stratum does not match household rank");
                out.Catalogue.Funding.push_back(policy);
            }
            else
            {
                CommerceShop shop{record->Id, R82Text(data, "name"), R82Text(data, "counterAnchor"), {}, {}, {}};
                if (shop.Name.empty() || !R82Token(shop.CounterAnchor, "anchor.")) issue("invalid shop name or counter anchor");
                std::set<std::string> people, anchors, items;
                for (const auto& value : data.Find("keepers")->Array)
                {
                    CommerceKeeper keeper{R82Text(value, "person"), R82Text(value, "counterAnchor")};
                    if (!R82Token(keeper.PersonId, "person.") || !R82Token(keeper.CounterAnchor, "anchor.") ||
                        !people.insert(keeper.PersonId).second || !anchors.insert(keeper.CounterAnchor).second)
                        issue("invalid or duplicate keeper binding");
                    shop.Keepers.push_back(std::move(keeper));
                }
                if (shop.Keepers.empty()) issue("shop needs at least one keeper binding");
                std::pair<std::int32_t, std::int32_t> previous{-1, -1};
                for (const auto& value : data.Find("opening")->Array)
                {
                    CommerceOpeningWindow window{static_cast<std::int32_t>(R82Int(value, "days")),
                        static_cast<std::int32_t>(R82Int(value, "start")), static_cast<std::int32_t>(R82Int(value, "end"))};
                    const std::pair key{window.Days, window.StartMinute};
                    if (window.StartMinute >= window.EndMinute || key <= previous) issue("opening windows must be nonempty and sorted by (days,start)");
                    for (const auto& earlier : shop.Opening)
                        if ((earlier.Days & window.Days) != 0 && earlier.StartMinute < window.EndMinute && window.StartMinute < earlier.EndMinute)
                            issue("opening windows overlap on a shared weekday");
                    shop.Opening.push_back(window);
                    previous = key;
                }
                if (shop.Opening.empty()) issue("shop needs opening windows");
                for (const auto& value : data.Find("entries")->Array)
                {
                    const auto& id = R82Text(value, "item");
                    const auto* target = db.Find(id);
                    if (target == nullptr || target->Type != "commerce_item") issue("catalogue item reference is missing or has the wrong type: " + id);
                    if (!items.insert(id).second) issue("duplicate catalogue item " + id);
                    shop.Entries.push_back({shop.Id, id, R82Int(value, "unitPricePaise")});
                }
                if (shop.Entries.empty()) issue("shop needs catalogue entries");
                std::sort(shop.Entries.begin(), shop.Entries.end(), [](const auto& a, const auto& b) { return a.ItemId < b.ItemId; });
                out.Catalogue.Shops.push_back(std::move(shop));
            }
        }
    }
    if (!out.Catalogue.Items.empty() || !out.Catalogue.Shops.empty() || !out.Catalogue.Funding.empty())
        for (std::int32_t rank = 0; rank < 5; ++rank)
            if (!ranks.contains(rank)) out.Issues.push_back({"commerce_funding", "", "missing funding rank " + std::to_string(rank)});
    std::sort(out.Catalogue.Funding.begin(), out.Catalogue.Funding.end(),
        [](const auto& a, const auto& b) { return a.HouseholdResourcesRank < b.HouseholdResourcesRank; });
    if (!out.Ok()) out.Catalogue = {};
    return out;
}

const CommerceItem* FindCommerceItem(const CommerceContent& content, std::string_view id)
{
    for (const auto& item : content.Items) if (item.Id == id) return &item;
    return nullptr;
}
const CommerceShop* FindCommerceShop(const CommerceContent& content, std::string_view id)
{
    for (const auto& shop : content.Shops) if (shop.Id == id) return &shop;
    return nullptr;
}
const CommerceCatalogueEntry* FindCommerceEntry(const CommerceShop& shop, std::string_view itemId)
{
    for (const auto& entry : shop.Entries) if (entry.ItemId == itemId) return &entry;
    return nullptr;
}
const CommerceFundingPolicy* FindCommerceFunding(const CommerceContent& content, std::int32_t rank)
{
    for (const auto& policy : content.Funding) if (policy.HouseholdResourcesRank == rank) return &policy;
    return nullptr;
}
bool IsCommerceShopOpen(const CommerceShop& shop, GameInstant at)
{
    if (at < 0 || at > CommerceMaxInstant) return false;
    const auto calendar = GameClock::ToCalendar(at);
    const auto bit = 1 << static_cast<int>(calendar.DayOfWeek);
    const auto minute = calendar.Hour * 60 + calendar.Minute;
    for (const auto& window : shop.Opening)
        if ((window.Days & bit) != 0 && minute >= window.StartMinute && minute < window.EndMinute) return true;
    return false;
}
std::vector<CommerceKeeperUnavailability> ValidateCommerceKeepers(
    const CommerceContent& content, std::span<const std::string_view> registeredPeople)
{
    std::vector<CommerceKeeperUnavailability> out;
    for (const auto& shop : content.Shops)
        for (const auto& keeper : shop.Keepers)
            if (!R82Registered(keeper.PersonId, registeredPeople)) out.push_back({shop.Id, keeper.PersonId, "keeper not registered"});
    return out;
}
bool HasRegisteredCommerceKeeper(const CommerceShop& shop, std::span<const std::string_view> registeredPeople)
{
    for (const auto& keeper : shop.Keepers) if (R82Registered(keeper.PersonId, registeredPeople)) return true;
    return false;
}
namespace ContentSchema
{
void ValidateCommerceContentSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    const auto result = LoadCommerceContent(db);
    issues.insert(issues.end(), result.Issues.begin(), result.Issues.end());
}
}
} // namespace PerfectWorld
