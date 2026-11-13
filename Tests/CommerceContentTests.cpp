#include "CommerceContent.h"

#include "Doors.h"
#include "NeighbourhoodPlan.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <system_error>

namespace
{
using namespace PerfectWorld;
int R82Assertions = 0;
void R82Expect(bool value, std::string_view message)
{
    ++R82Assertions;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
std::vector<ContentFile> R82Read(const char* path)
{
    std::vector<ContentFile> files;
    std::error_code error;
    std::filesystem::recursive_directory_iterator it(path, error), end;
    R82Expect(!error, "content directory opens");
    while (it != end)
    {
        if (it->is_regular_file(error) && it->path().extension() == ".json")
        {
            std::ifstream input(it->path(), std::ios::binary);
            R82Expect(input.good(), "content file opens");
            files.push_back({it->path().generic_string(), {std::istreambuf_iterator<char>(input), {}}});
        }
        it.increment(error);
        R82Expect(!error, "directory iteration succeeds");
    }
    return files;
}
std::vector<ContentFile> R82Mutated(std::vector<ContentFile> files, std::string_view id,
    std::string_view from, std::string_view to)
{
    bool found = false;
    for (auto& file : files)
    {
        if (!file.Path.ends_with("/" + std::string(id) + ".json")) continue;
        const auto at = file.Text.find(from);
        R82Expect(at != std::string::npos, "mutation text exists");
        file.Text.replace(at, from.size(), to);
        found = true;
    }
    R82Expect(found, "mutation file exists");
    return files;
}
void R82Refused(const std::vector<ContentFile>& files, std::string_view id, std::string_view from,
    std::string_view to, std::string_view reason)
{
    const auto result = LoadContent(R82Mutated(files, id, from, to));
    R82Expect(!result.Ok(), "bad commerce content is refused");
    R82Expect(std::any_of(result.Issues.begin(), result.Issues.end(), [&](const auto& issue) {
        return issue.Message.find(reason) != std::string::npos;
    }), reason);
    const auto typed = LoadCommerceContent(result.Db);
    R82Expect(!typed.Ok() && typed.Catalogue == CommerceContent{}, "typed loader refuses atomically too");
}
GameInstant R82At(int day, int minute) { return (7 + day) * MinutesPerDay + minute - 360; }

void R82Items(const CommerceContent& content)
{
    struct R82ItemPin { const char* Id; const char* Unit; CommerceEffect Effect; int Relief; };
    const R82ItemPin pins[]{
        {"tea", "cup", CommerceEffect::Tea, 0}, {"biscuits", "pair", CommerceEffect::Food, 1500},
        {"pen", "pen", CommerceEffect::None, 0}, {"notebook", "notebook", CommerceEffect::None, 0},
        {"geometry_box", "box", CommerceEffect::None, 0}, {"rasgulla", "piece", CommerceEffect::Food, 1000},
        {"sandesh", "piece", CommerceEffect::Food, 1000}, {"plasters", "pack of five", CommerceEffect::UnsupportedSupply, 0},
        {"ors", "sachet", CommerceEffect::UnsupportedSupply, 0}, {"jhalmuri", "portion", CommerceEffect::Food, 3500},
        {"phuchka", "plate of six", CommerceEffect::Food, 3000}};
    R82Expect(content.Items.size() == std::size(pins), "eleven items");
    for (const auto& pin : pins)
    {
        const auto* item = FindCommerceItem(content, "item." + std::string(pin.Id));
        R82Expect(item != nullptr && !item->Name.empty() && IsCommerceItemValid(*item), "stable valid named item");
        R82Expect(item->Unit == pin.Unit && item->StackLimit == 10 && item->Effect == pin.Effect &&
            item->HungerRelief == pin.Relief, "display units, stack limits and supported effects are pinned");
    }
    R82Expect(FindCommerceItem(content, "item.missing") == nullptr, "unknown item has no fallback");
}
void R82Shops(const CommerceContent& content)
{
    R82Expect(content.Shops.size() == 5 && FindCommerceShop(content, "shop.hardware_store") == nullptr,
        "five catalogues; hardware has no transactional catalogue");
    struct R82PricePin { const char* Shop; const char* Item; int Price; };
    const R82PricePin prices[]{
        {"shanti_chai", "tea", 2500}, {"shanti_chai", "biscuits", 1000},
        {"stationery", "pen", 2000}, {"stationery", "notebook", 6000}, {"stationery", "geometry_box", 22000},
        {"sweet_shop", "rasgulla", 3000}, {"sweet_shop", "sandesh", 3500},
        {"pharmacy", "plasters", 4000}, {"pharmacy", "ors", 3000},
        {"vendor", "jhalmuri", 4000}, {"vendor", "phuchka", 5000}};
    std::set<std::string> sold;
    for (const auto& pin : prices)
    {
        const auto* shop = FindCommerceShop(content, "shop." + std::string(pin.Shop));
        R82Expect(shop != nullptr, "stable shop id exists");
        const auto* entry = FindCommerceEntry(*shop, "item." + std::string(pin.Item));
        R82Expect(entry != nullptr && entry->ShopId == shop->Id && entry->UnitPricePaise == pin.Price, "integer unit price pinned");
        sold.insert(entry->ItemId);
    }
    std::size_t entries = 0;
    for (const auto& shop : content.Shops)
    {
        entries += shop.Entries.size();
        for (const auto& entry : shop.Entries)
            R82Expect(FindCommerceItem(content, entry.ItemId) != nullptr && entry.UnitPricePaise > 0, "all item references valid");
        R82Expect(!IsCommerceShopOpen(shop, -1) && !IsCommerceShopOpen(shop, CommerceMaxInstant + 1), "invalid time is closed");
        for (int day = 0; day < 7; ++day)
            for (const auto& window : shop.Opening)
            {
                R82Expect(window.Days == 127 && window.StartMinute < window.EndMinute && window.EndMinute <= 1440, "sane daily hours");
                R82Expect(IsCommerceShopOpen(shop, R82At(day, window.StartMinute)), "opening minute included");
                R82Expect(!IsCommerceShopOpen(shop, R82At(day, window.EndMinute)), "closing minute excluded, breaks override");
            }
    }
    R82Expect(entries == 11 && sold.size() == 11, "every item sold exactly once");
    R82Expect(FindCommerceEntry(content.Shops.front(), "item.missing") == nullptr, "unknown entry has no fallback");
    // Existing posted hours outrank the proposal. Compare every minute to Doors.
    for (const auto& pair : {std::pair{"shop.shanti_chai", "door.nabapara.shanti_chai_shutter"},
                             std::pair{"shop.stationery", "door.nabapara.stationery_front"}})
    {
        const auto* shop = FindCommerceShop(content, pair.first);
        const auto doors = AuthoredDoors();
        const auto door = std::find_if(doors.begin(), doors.end(), [&](const auto& d) { return d.Id == pair.second; });
        R82Expect(door != doors.end(), "existing business door registered");
        for (int minute = 0; minute < 1440; ++minute)
        {
            bool open = false;
            for (const auto& w : door->OpeningWindows) open |= minute >= w.OpenMinute && minute < w.CloseMinute;
            R82Expect(IsCommerceShopOpen(*shop, R82At(0, minute)) == open, "catalogue agrees with actual door hours");
        }
    }
}
void R82Staffing(const ContentDb& db, const CommerceContent& content)
{
    constexpr std::array<std::string_view, 2> registered{"person.farida_qadri", "person.anindita_sen"};
    const auto absent = ValidateCommerceKeepers(content, registered);
    R82Expect(absent.size() == 4, "four unmerged Round 7 people reported explicitly");
    for (const auto& issue : absent) R82Expect(issue.Reason == "keeper not registered", "soft unavailability reason");
    const auto* sweet = FindCommerceShop(content, "shop.sweet_shop");
    R82Expect(sweet->Keepers.size() == 2 && sweet->Keepers[0].PersonId == "person.bimal_modak" &&
        sweet->Keepers[1].PersonId == "person.tithi_ghoshal", "primary keeper and assistant identities");
    for (const auto& shop : content.Shops)
        R82Expect(HasRegisteredCommerceKeeper(shop, registered) == (shop.Id == "shop.shanti_chai" || shop.Id == "shop.stationery"),
            "unregistered counters stay unavailable during opening hours");
    constexpr std::array<std::string_view, 6> merged{registered[0], registered[1], "person.bimal_modak", "person.tithi_ghoshal",
        "person.aparna_lahiri", "person.koushik_mondal"};
    R82Expect(ValidateCommerceKeepers(content, merged).empty(), "registered Round 7 people resolve soft diagnostics");
    const std::array<std::string_view, 1> assistant{"person.tithi_ghoshal"};
    R82Expect(HasRegisteredCommerceKeeper(*sweet, assistant), "assistant can be independently registered");
    R82Expect(!HasRegisteredCommerceKeeper(*sweet, {}), "no cast cannot activate a counter");
    for (std::size_t index = 0; index < registered.size(); ++index)
    {
        const auto schedule = LoadNpcSchedule(db, index == 0 ? "npc_schedule.farida_qadri" : "npc_schedule.anindita_sen");
        R82Expect(schedule.Ok() && schedule.Value->Person.PersonId == registered[index], "actual registered schedule resolves keeper");
        const auto& shop = *FindCommerceShop(content, index == 0 ? "shop.shanti_chai" : "shop.stationery");
        for (int day = 0; day < 7; ++day)
        {
            const auto at = R82At(day, index == 0 ? 540 : 600);
            const auto resolved = ResolveSchedule(schedule.Value->Person, schedule.Value->Travel, at);
            const auto decision = ResolveNpcDecisionAt(schedule.Value->Person, schedule.Value->Travel, at);
            R82Expect(IsCommerceShopOpen(shop, at) && resolved.Valid && resolved.Status == ScheduleStatus::Stay &&
                resolved.Zone == shop.CounterAnchor && resolved.Doing == "serving" && decision.Ok(), "existing counters have reachable serving periods every day");
        }
    }
    const auto plan = BuildNeighbourhoodPlan(82);
    for (const auto* name : {"sweet_shop", "pharmacy", "vendor"})
    {
        const auto& shop = *FindCommerceShop(content, "shop." + std::string(name));
        const auto* counter = FindNeighbourhoodFeature(plan, shop.CounterAnchor);
        const auto* entrance = FindNeighbourhoodFeature(plan, "entrance.nabapara." + std::string(name));
        R82Expect(counter != nullptr && entrance != nullptr, "Round 7 counter and entrance anchors exist");
        const auto route = RouteNeighbourhood(plan, entrance->LinkNodeId, counter->LinkNodeId);
        R82Expect(route.Found && route.LengthCm > 0, "spatial approach exists even while keeper is unregistered");
    }
}
void R82Effects(const CommerceContent& content)
{
    for (const auto& item : content.Items)
    {
        auto state = InitialiseCommerce(0, 0).State;
        state.Inventory[0] = {item.Id, 1};
        ConditionState condition;
        condition.Energy = 50000;
        condition.Hunger = 50000;
        const auto used = Consume(state, condition, item, 0, {true, false, true});
        if (item.Effect == CommerceEffect::Tea || item.Effect == CommerceEffect::Food)
        {
            R82Expect(used.Ok() && used.State.Inventory[0] == CommerceStack{}, "authored edible consumes one unit");
            R82Expect(used.Condition.Energy == condition.Energy + (item.Effect == CommerceEffect::Tea ? 2000 : 0) &&
                used.Condition.Hunger == condition.Hunger - item.HungerRelief, "authored effect uses frozen condition rules");
            if (item.Effect == CommerceEffect::Tea) R82Expect(used.State.NextTeaEligibleAt == 60, "authored tea shares persisted cooldown");
        }
        else R82Expect(used.Error == CommerceError::UseUnavailable && used.State == state && used.Condition == condition,
            "stationery and medical supplies are carryable but have no use effect");
    }
}
}

int main(int argc, char** argv)
{
    using namespace PerfectWorld;
    R82Expect(argc == 2, "content directory argument");
    const auto files = R82Read(argv[1]);
    const auto loaded = LoadContent(files);
    for (const auto& issue : loaded.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    R82Expect(loaded.Ok(), "entire authored content loads, including absent Round 7 keepers");
    const auto result = LoadCommerceContent(loaded.Db);
    R82Expect(result.Ok(), "typed commerce content loads");
    const auto& content = result.Catalogue;
    R82Items(content);
    R82Shops(content);
    R82Staffing(loaded.Db, content);
    R82Effects(content);
    constexpr std::array<std::int64_t, 5> amounts{12000, 15000, 30000, 60000, 120000};
    R82Expect(content.Funding.size() == 5, "all five funding ranks");
    for (std::int32_t rank = 0; rank < 5; ++rank)
    {
        const auto* policy = FindCommerceFunding(content, rank);
        R82Expect(policy != nullptr && policy->StartingWalletPaise == amounts[rank] && policy->WeeklyAllowancePaise == amounts[rank] &&
            *policy == InitialiseCommerce(rank, 0).State.Funding, "funding equals frozen rules for every rank");
    }
    R82Expect(FindCommerceFunding(content, -1) == nullptr && FindCommerceFunding(content, 5) == nullptr, "invalid rank has no fallback");
    auto reversed = files;
    std::reverse(reversed.begin(), reversed.end());
    const auto reverseDb = LoadContent(reversed);
    R82Expect(reverseDb.Ok() && LoadCommerceContent(reverseDb.Db).Catalogue == content, "file enumeration order cannot change catalogue");
    R82Expect(LoadCommerceContent(loaded.Db).Catalogue == content, "repeated loads deterministic");
    auto entryOrder = loaded.Db;
    auto& entryData = entryOrder.Records.at("shop.stationery").Data.Object;
    for (auto& [key, value] : entryData) if (key == "entries") std::reverse(value.Array.begin(), value.Array.end());
    R82Expect(LoadCommerceContent(entryOrder).Catalogue == content, "catalogue entries normalize to stable item order");
    auto malformed = loaded.Db;
    auto& malformedItem = malformed.Records.at("item.tea").Data;
    malformedItem.Object.push_back({"unrecognized", JsonValue{}});
    R82Expect(!LoadCommerceContent(malformed).Ok(), "typed boundary rejects unknown fields without registry prevalidation");
    malformedItem.Object.pop_back();
    malformedItem.Object.push_back(malformedItem.Object.front());
    R82Expect(!LoadCommerceContent(malformed).Ok(), "typed boundary rejects duplicate JSON keys");
    CommerceShop weekly = content.Shops.front();
    weekly.Opening = {{2, 600, 660}}; // Monday only, Sunday bit zero.
    R82Expect(IsCommerceShopOpen(weekly, R82At(0, 600)) && !IsCommerceShopOpen(weekly, R82At(6, 600)), "opening weekday mask uses calendar weekday");
    std::vector<ContentFile> commerceFiles;
    for (const auto& file : files)
        if (file.Path.find("/commerce_") != std::string::npos || file.Path.find("/stratum/") != std::string::npos) commerceFiles.push_back(file);
    R82Refused(commerceFiles, "item.tea", "\"stackLimit\": 10", "\"stackLimit\": 9", "outside 10..10");
    R82Refused(commerceFiles, "item.tea", "\"hungerRelief\": 0", "\"hungerRelief\": 1000", "incompatible");
    R82Refused(commerceFiles, "item.biscuits", "\"hungerRelief\": 1500", "\"hungerRelief\": 999", "incompatible");
    R82Refused(commerceFiles, "item.tea", "\"effect\": \"tea\"", "\"effect\": \"healing\"", "unknown value");
    R82Refused(commerceFiles, "item.tea", "\"name\": \"Tea\"", "\"name\": \"\"", "incompatible");
    R82Refused(commerceFiles, "shop.shanti_chai", "\"unitPricePaise\": 2500", "\"unitPricePaise\": 0", "outside 1..");
    R82Refused(commerceFiles, "shop.shanti_chai", "\"unitPricePaise\": 2500", "\"unitPricePaise\": 25.5", "whole number");
    R82Refused(commerceFiles, "shop.shanti_chai", "item.tea", "item.missing", "does not exist");
    R82Refused(commerceFiles, "shop.shanti_chai", "item.tea", "stratum.middle", "needs a commerce_item");
    R82Refused(commerceFiles, "shop.shanti_chai", "\"end\": 690", "\"end\": 360", "nonempty");
    R82Refused(commerceFiles, "shop.shanti_chai", "\"start\": 930", "\"start\": 680", "overlap");
    R82Refused(commerceFiles, "shop.shanti_chai", "\"days\": 127", "\"days\": 0", "outside 1..127");
    R82Refused(commerceFiles, "shop.shanti_chai", "person.farida_qadri", "Farida", "keeper binding");
    R82Refused(commerceFiles, "shop.shanti_chai", "item.biscuits", "item.tea", "duplicate catalogue");
    R82Refused(commerceFiles, "shop.shanti_chai", "\"end\": 690", "\"end\": 1441", "outside 1..1440");
    R82Refused(commerceFiles, "shop.shanti_chai", "\"counterAnchor\": \"anchor.shanti_chai.counter\"", "\"counterAnchor\": \"counter\"", "counter anchor");
    R82Refused(commerceFiles, "commerce_funding.rank_0", "\"weeklyAllowancePaise\": 12000", "\"weeklyAllowancePaise\": 12001", "frozen R8-01");
    R82Refused(commerceFiles, "commerce_funding.rank_0", "stratum.institutional_care", "stratum.middle", "does not match");
    std::erase_if(commerceFiles, [](const auto& file) { return file.Path.ends_with("commerce_funding.rank_0.json"); });
    const auto missingRank = LoadContent(commerceFiles);
    R82Expect(!missingRank.Ok() && !LoadCommerceContent(missingRank.Db).Ok(), "no missing funding rank accepted");
    std::cout << R82Assertions << " commerce content assertions passed; " << files.size() << " files, " << loaded.Db.Count()
        << " records, " << KnownContentTypes().size() << " top-level kinds\n";
}
