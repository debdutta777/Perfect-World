#include "WorldSaveInternal.h"

#include "CommerceContent.h"

#include <charconv>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// R8-04: the pw.commerce/1 envelope, its place in the world save (schema 10) and the one-time household initialisation of
// every earlier world. The format is documented in WorldSave.h; Commerce.h's R8-04 contract lists the fields.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view R84Allowance = "allowance";
constexpr std::string_view R84Purchase = "purchase";

/** Splits one "\n"-terminated line off the front of `text`. */
bool R84Line(std::string_view& text, std::string_view& line)
{
    const std::size_t end = text.find('\n');
    if (end == std::string_view::npos)
        return false;
    line = text.substr(0, end);
    text.remove_prefix(end + 1);
    return true;
}

/** The next line must be "<key>=<value>". */
bool R84Keyed(std::string_view& text, std::string_view key, std::string_view& value)
{
    std::string_view line;
    if (!R84Line(text, line) || line.size() <= key.size() || !line.starts_with(key) || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Comma-separated fields; exactly `count` of them. IDs never contain commas (Commerce.cpp's ID rule). */
bool R84Fields(std::string_view value, std::size_t count, std::vector<std::string_view>& fields)
{
    fields.clear();
    while (true)
    {
        const std::size_t comma = value.find(',');
        fields.push_back(value.substr(0, comma));
        if (comma == std::string_view::npos)
            break;
        value.remove_prefix(comma + 1);
        if (fields.size() > count)
            return false;
    }
    return fields.size() == count;
}

/** A whole decimal number of type T. Spelling is made canonical by the final reserialisation comparison. */
template <typename T>
bool R84Number(std::string_view text, T& out)
{
    if (text.empty())
        return false;
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

template <typename T>
bool R84KeyedNumber(std::string_view& text, std::string_view key, T& out)
{
    std::string_view value;
    return R84Keyed(text, key, value) && R84Number(value, out);
}

bool R84ParseReceipt(std::string_view value, CommerceReceipt& receipt)
{
    std::vector<std::string_view> f;
    if (!R84Fields(value, 10, f))
        return false;
    if (f[0] == R84Allowance)
        receipt.Kind = CommerceReceiptKind::Allowance;
    else if (f[0] == R84Purchase)
        receipt.Kind = CommerceReceiptKind::Purchase;
    else
        return false;
    receipt.ShopId = std::string(f[3]);
    receipt.ItemId = std::string(f[4]);
    return R84Number(f[1], receipt.Revision) && R84Number(f[2], receipt.At) && R84Number(f[5], receipt.Quantity) &&
           R84Number(f[6], receipt.UnitPricePaise) && R84Number(f[7], receipt.TotalPaise) &&
           R84Number(f[8], receipt.BalanceBeforePaise) && R84Number(f[9], receipt.BalanceAfterPaise);
}

/** Every pw.commerce/1 line before "end", parsed but not yet validated. */
bool R84ParseBody(std::string_view& text, CommerceState& state)
{
    std::string_view line;
    std::string_view value;
    std::vector<std::string_view> f;
    if (!R84Line(text, line) || line != CommerceTextHeader || !R84KeyedNumber(text, "wallet", state.WalletPaise))
        return false;
    for (CommerceStack& stack : state.Inventory)
    {
        if (!R84Keyed(text, "slot", value) || !R84Fields(value, 2, f) || !R84Number(f[1], stack.Quantity))
            return false;
        stack.ItemId = std::string(f[0]);
    }
    if (!R84Keyed(text, "funding", value) || !R84Fields(value, 3, f) || !R84Number(f[0], state.Funding.HouseholdResourcesRank) ||
        !R84Number(f[1], state.Funding.StartingWalletPaise) || !R84Number(f[2], state.Funding.WeeklyAllowancePaise))
        return false;
    std::size_t receipts = 0;
    if (!R84KeyedNumber(text, "initialised", state.InitialisedAt) || !R84KeyedNumber(text, "settled", state.AllowanceSettledThrough) ||
        !R84KeyedNumber(text, "next_allowance", state.NextAllowanceDue) || !R84KeyedNumber(text, "revision", state.TransactionRevision) ||
        !R84KeyedNumber(text, "receipts", receipts) || receipts > CommerceRecentReceiptLimit)
        return false;
    state.RecentReceipts.resize(receipts);
    for (CommerceReceipt& receipt : state.RecentReceipts)
    {
        if (!R84Keyed(text, "receipt", value) || !R84ParseReceipt(value, receipt))
            return false;
    }
    return R84Keyed(text, "folded", value) && R84Fields(value, 4, f) && R84Number(f[0], state.FoldedReceiptCount) &&
           R84Number(f[1], state.FoldedThroughRevision) && R84Number(f[2], state.FoldedCreditsPaise) &&
           R84Number(f[3], state.FoldedDebitsPaise) && R84KeyedNumber(text, "tea_eligible", state.NextTeaEligibleAt) &&
           R84KeyedNumber(text, "consumption_eligible", state.NextConsumptionEligibleAt);
}

/** Funding must be exactly the frozen table row for the life's Household Resources rank (InitialiseCommerce owns the table). */
bool R84FundingIsHousehold(const CommerceState& state, std::int32_t householdRank)
{
    const CommerceStateResult row = InitialiseCommerce(householdRank, 0);
    return row.Ok() && row.State.Funding == state.Funding;
}

/** The creation record's Household Resources rank, or -1 when the record has none. */
std::int32_t R84HouseholdRank(const CharacterCreationSave& creation)
{
    const auto& traits = CharacterCreator::TraitDefinitions();
    for (std::size_t index = 0; index < traits.size(); ++index)
    {
        if (traits[index].Id == HouseholdResourcesTraitId)
            return creation.TraitRanks[index];
    }
    return -1;
}
} // namespace

std::string SerialiseCommerceState(const CommerceState& state)
{
    if (!IsCommerceStateValid(state))
        return {};
    const auto number = [](auto value) { return std::to_string(value); };
    std::string text(CommerceTextHeader);
    text += "\nwallet=" + number(state.WalletPaise) + "\n";
    for (const CommerceStack& stack : state.Inventory)
        text += "slot=" + stack.ItemId + "," + number(stack.Quantity) + "\n";
    text += "funding=" + number(state.Funding.HouseholdResourcesRank) + "," + number(state.Funding.StartingWalletPaise) + "," +
            number(state.Funding.WeeklyAllowancePaise) + "\n";
    text += "initialised=" + number(state.InitialisedAt) + "\nsettled=" + number(state.AllowanceSettledThrough) +
            "\nnext_allowance=" + number(state.NextAllowanceDue) + "\nrevision=" + number(state.TransactionRevision) +
            "\nreceipts=" + number(state.RecentReceipts.size()) + "\n";
    for (const CommerceReceipt& r : state.RecentReceipts)
    {
        text += "receipt=" + std::string(r.Kind == CommerceReceiptKind::Allowance ? R84Allowance : R84Purchase) + "," +
                number(r.Revision) + "," + number(r.At) + "," + r.ShopId + "," + r.ItemId + "," + number(r.Quantity) + "," +
                number(r.UnitPricePaise) + "," + number(r.TotalPaise) + "," + number(r.BalanceBeforePaise) + "," +
                number(r.BalanceAfterPaise) + "\n";
    }
    text += "folded=" + number(state.FoldedReceiptCount) + "," + number(state.FoldedThroughRevision) + "," +
            number(state.FoldedCreditsPaise) + "," + number(state.FoldedDebitsPaise) + "\n";
    text += "tea_eligible=" + number(state.NextTeaEligibleAt) + "\nconsumption_eligible=" + number(state.NextConsumptionEligibleAt) +
            "\nend\n";
    return text;
}

bool DeserialiseCommerceState(std::string_view text, CommerceState& out)
{
    const std::string_view original = text;
    CommerceState parsed;
    if (!R84ParseBody(text, parsed) || text != "end\n" || !IsCommerceStateValid(parsed) || SerialiseCommerceState(parsed) != original)
        return false;
    out = std::move(parsed);
    return true;
}

bool IsCommerceStateInContent(const CommerceState& state, const CommerceContent& content)
{
    for (const CommerceStack& stack : state.Inventory)
    {
        if (!stack.ItemId.empty() && FindCommerceItem(content, stack.ItemId) == nullptr)
            return false;
    }
    for (const CommerceReceipt& receipt : state.RecentReceipts)
    {
        if (receipt.Kind == CommerceReceiptKind::Purchase &&
            (FindCommerceShop(content, receipt.ShopId) == nullptr || FindCommerceItem(content, receipt.ItemId) == nullptr))
            return false;
    }
    return true;
}

std::string WorldSaveWriteCommerce(const WorldState& world, const CharacterCreationSave& creation)
{
    const std::string text = SerialiseCommerceState(world.Commerce);
    if (text.empty() || world.Commerce.AllowanceSettledThrough != world.Clock.Now() ||
        !R84FundingIsHousehold(world.Commerce, R84HouseholdRank(creation)))
        return {};
    return ",\n    \"commerce\": " + WorldSaveQuoted(text);
}

WorldSaveResult WorldSaveReadCommerce(const JsonValue& value, WorldState& world, std::int32_t version, std::int32_t householdRank)
{
    if (version <= WorldSaveLastWithoutCommerceVersion)
    {
        // The household's starting wallet, granted once at the saved instant: no weeks before it are paid.
        const CommerceStateResult begun = InitialiseCommerce(householdRank, world.Clock.Now());
        if (!begun.Ok())
            return WorldSaveInvalid("the saved instant cannot begin the household's allowance.");
        world.Commerce = begun.State;
        return {};
    }
    const JsonValue* commerce = value.Find("commerce");
    CommerceState parsed;
    if (commerce == nullptr || !commerce->IsString())
        return WorldSaveInvalid("the commerce section has the wrong type.");
    if (!DeserialiseCommerceState(commerce->String, parsed))
        return WorldSaveInvalid("the wallet, bag or receipts are damaged or do not add up.");
    if (!R84FundingIsHousehold(parsed, householdRank))
        return WorldSaveInvalid("the allowance does not belong to this life's household.");
    if (parsed.AllowanceSettledThrough != world.Clock.Now())
        return WorldSaveInvalid("the allowance and the world clock disagree about the time.");
    world.Commerce = std::move(parsed);
    return {};
}

WorldSaveResult DeserializeWorldSave(std::string_view text, WorldSaveData& out, const CommerceContent& content)
{
    WorldSaveData data;
    WorldSaveResult result = DeserializeWorldSave(text, data);
    if (!result)
        return result;
    if (!IsCommerceStateInContent(data.World.Commerce, content))
        return WorldSaveInvalid("it holds goods or shops this game does not know.");
    out = std::move(data);
    return result;
}
} // namespace PerfectWorld
