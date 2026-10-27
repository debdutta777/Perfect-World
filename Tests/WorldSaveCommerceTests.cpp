// R8-04: pw.commerce/1 in the world save (schema 10, hash 7): canonical envelope, certified round trips, writer and reader
// refusals, authenticated legacy migration with one-time household initialisation, and content-aware loading.
#include "WorldSave.h"

#include "CommerceContent.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

using namespace PerfectWorld;

int R84SaveChecks = 0;
void Check(bool condition, const char* message)
{
    ++R84SaveChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#include "WorldSaveTestSupport.h"

/** WorldSaveCommerceFixtureTests.cpp: the frozen v9 fixture, its expected migrated result and every earlier fixture. */
void TestWorldSaveCommerceFixtures();

namespace
{
const CommerceItem R84Tea{"item.tea", "Tea", "cup", 10, CommerceEffect::Tea, 0};
const CommerceItem R84Pen{"item.pen", "Pen", "pen", 10, CommerceEffect::None, 0};

/** Weekly allowances and purchases through the pure rules (folding past 64 receipts), then tea taken. */
CommerceState R84History(std::int32_t rank, int weeks)
{
    CommerceState state = InitialiseCommerce(rank, 0).State;
    const CommerceCatalogueEntry tea{"shop.shanti_chai", "item.tea", 2500};
    const CommerceCatalogueEntry pen{"shop.stationery", "item.pen", 2000};
    GameInstant now = 0;
    for (int week = 0; week < weeks; ++week)
    {
        const CommerceStateResult settled = SettleAllowance(state, now, now + CommerceWeekMinutes);
        Check(settled.Ok(), "a week settles");
        state = settled.State;
        now += CommerceWeekMinutes;
        const CommercePurchaseContext open{now, true, true, true, true, true, true, true};
        const CommerceStateResult bought =
            ApplyPurchase(state, week % 3 == 0 ? pen : tea, week % 3 == 0 ? R84Pen : R84Tea, 1, state.TransactionRevision, open);
        Check(bought.Ok(), "a weekly purchase succeeds");
        state = bought.State;
    }
    const CommerceConsumptionResult used = Consume(state, ConditionState{}, R84Tea, now, {true, false, true});
    Check(used.Ok(), "tea is taken");
    return used.State;
}

CharacterCreator R84Creator(const CharacterCreationSave& save)
{
    CharacterCreator creator;
    Check(creator.RestoreSave(save).Succeeded(), "the creation record restores");
    return creator;
}

/** A certified-savable world for `creation` whose commerce is `commerce`, with the clock at its cursor. */
WorldSaveData R84Save(const CharacterCreationSave& creation, const CommerceState& commerce)
{
    WorldSaveData data;
    data.Creation = creation;
    data.World = MakeWorldState(85, R84Creator(creation));
    Check(!data.World.Clock.AdvanceTo(commerce.AllowanceSettledThrough).Rejected, "the clock reaches the cursor");
    data.World.DayLoop = NewDayLoopState(commerce.AllowanceSettledThrough);
    data.World.Commerce = commerce;
    data.Player.ZoneId = "zone.street.nabapara_lane";
    data.Player.PositionCm = {120, -3400, 20};
    return data;
}

std::string R84Quoted(std::string_view text)
{
    std::string out = "\"";
    for (const char c : text)
        out += c == '\n' ? std::string("\\n") : std::string(1, c);
    return out + '"';
}

std::string R84HashText(std::uint64_t hash)
{
    std::string value = "\"0x0000000000000000\"";
    for (int digit = 0; digit < 16; ++digit)
        value[static_cast<std::size_t>(18 - digit)] = "0123456789abcdef"[(hash >> (digit * 4)) & 15];
    return value;
}

/** Today's v10 text rewritten as the historical writer of `version` (9 or 8) produced it, with that version's checksum. */
std::string R84Downgrade(const std::string& current, const WorldState& world, int version)
{
    std::string old = current;
    const std::size_t at = old.find(version == 9 ? ",\n    \"commerce\":" : ",\n    \"journal\":");
    Check(at != std::string::npos, "the section to drop exists");
    old.erase(at, old.find("\n  }", at) - at);
    old = ReplaceOnce(old, "\"schemaVersion\": 10", "\"schemaVersion\": " + std::to_string(version));
    old = ReplaceOnce(old, "\"hashVersion\": 7", version == 9 ? "\"hashVersion\": 6" : "\"hashVersion\": 5");
    const std::uint32_t hashVersion = version == 9 ? 6 : 5;
    old.replace(old.find("\"stateHash\": ") + 13, 20, R84HashText(HashWorldStateWithDoorsText(world, SerialiseDoorState(world.Doors), hashVersion)));
    return old;
}

void R84Envelope()
{
    const CommerceState fresh = InitialiseCommerce(1, 760).State;
    std::string expected = "pw.commerce/1\nwallet=15000\n";
    for (std::size_t slot = 0; slot < CommerceInventorySlots; ++slot)
        expected += "slot=,0\n";
    expected += "funding=1,15000,15000\ninitialised=760\nsettled=760\nnext_allowance=10840\nrevision=0\nreceipts=0\n"
                "folded=0,0,0,0\ntea_eligible=760\nconsumption_eligible=760\nend\n";
    Check(SerialiseCommerceState(fresh) == expected, "a fresh household's envelope has the documented canonical text");
    const CommerceState rich = R84History(2, 70);
    Check(rich.FoldedReceiptCount > 0 && rich.RecentReceipts.size() == CommerceRecentReceiptLimit, "the rich history folds");
    const std::string text = SerialiseCommerceState(rich);
    CommerceState back;
    Check(DeserialiseCommerceState(text, back) && back == rich, "every commerce field round trips exactly");
    Check(text.find("receipt=allowance,") != std::string::npos && text.find("receipt=purchase,") != std::string::npos &&
              text.find("slot=item.tea,") != std::string::npos,
          "receipt kinds are stable names and items are saved by ID");
    const auto refused = [&](const std::string& bad, const char* what)
    {
        CommerceState out = fresh;
        Check(!DeserialiseCommerceState(bad, out) && out == fresh, what);
    };
    for (std::size_t cut = 0; cut < text.size(); ++cut)
        refused(text.substr(0, cut), "every prefix of the envelope is refused");
    refused(text + "x", "trailing bytes are refused");
    refused(ReplaceOnce(expected, "pw.commerce/1", "pw.commerce/2"), "an unknown envelope version is refused");
    refused(ReplaceOnce(expected, "wallet=15000", "wallet=015000"), "a leading zero is refused");
    refused(ReplaceOnce(expected, "wallet=15000", "wallet=+15000"), "a plus sign is refused");
    refused(ReplaceOnce(expected, "wallet=15000", "wallet=15001"), "a wallet that does not reconcile is refused");
    refused(ReplaceOnce(expected, "revision=0", "revision=-0"), "a negative zero is refused");
    refused(ReplaceOnce(expected, "slot=,0\n", "slot=,0\nslot=,0\n"), "a thirteenth slot is refused");
    refused(ReplaceOnce(expected, "slot=,0\n", ""), "an eleventh-slot bag is refused");
    refused(ReplaceOnce(expected, "slot=,0\n", "slot=item.tea,0\n"), "an empty slot naming an item is refused");
    refused(ReplaceOnce(expected, "funding=1,15000,15000", "funding=1,15000,15000,0"), "an extra funding field is refused");
    refused(ReplaceOnce(expected, "next_allowance=10840", "next_allowance=10841"), "an off-week allowance cursor is refused");
    refused(ReplaceOnce(expected, "tea_eligible=760", "tea_eligible=821"), "tea eligibility past the cooldown is refused");
    refused(ReplaceOnce(text, "receipt=allowance,", "receipt=Allowance,"), "an unknown receipt kind is refused");
    refused(ReplaceOnce(text, "receipts=64", "receipts=63"), "a receipt count that disagrees is refused");
    refused(ReplaceOnce(text, "receipts=64", "receipts=65"), "more than 64 recent receipts are refused");
    refused(ReplaceOnce(expected, "end\n", "end"), "a missing final newline is refused");
    CommerceState invalid = rich;
    invalid.WalletPaise += 1;
    Check(SerialiseCommerceState(invalid).empty(), "an unreconciled state is never written");
}

void R84RoundTrip()
{
    const CharacterCreationSave creation = MakeCreation("parent_household");
    Check(R84Creator(creation).GetTraitRank(HouseholdResourcesTraitId) == 1, "the test life has household rank 1");
    const WorldSaveData data = R84Save(creation, R84History(1, 70));
    const std::string text = SerializeWorldSave(data);
    Check(text.find("\"schemaVersion\": 10") != std::string::npos && text.find("\"hashVersion\": 7") != std::string::npos &&
              text.find("\"commerce\": \"pw.commerce/1\\nwallet=") != std::string::npos,
          "the writer emits schema 10, hash 7 and the commerce envelope");
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    Check(result.Succeeded() && result.HashVerified && result.SourceVersion == WorldSaveSchemaVersion && !result.Migrated,
          "a v10 save with commerce is certified");
    Check(loaded.World.Commerce == data.World.Commerce, "wallet, bag, funding, cursor, revision, receipts, folding and eligibility survive");
    CheckSame(data, loaded, "every other section survives");
    Check(SerializeWorldSave(loaded) == text, "resaving is byte-identical");
    for (std::size_t cut = 0; cut < text.size(); cut += 7)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "a truncated v10 save is refused");
    WorldSim original(data.World);
    WorldSim resumed(loaded.World);
    for (int day = 0; day < 15; ++day)
    {
        Check(original.Submit({CommandSource::Player, AdvanceTime{MinutesPerDay}}).Accepted &&
                  resumed.Submit({CommandSource::Player, AdvanceTime{MinutesPerDay}}).Accepted,
              "both lives move on");
    }
    Check(HashWorldState(original.State()) == HashWorldState(resumed.State()) &&
              resumed.State().Commerce.RecentReceipts.back().Kind == CommerceReceiptKind::Allowance &&
              resumed.State().Commerce.TransactionRevision == data.World.Commerce.TransactionRevision + 2,
          "the loaded life keeps paying the weekly allowance on schedule");

    WorldSaveData fresh;
    Check(BeginWorldFromCreation(creation, 77, fresh).Succeeded() && fresh.World.Commerce == InitialiseCommerce(1, 0).State,
          "a new life begins with its household's funding");
    WorldSaveData freshBack;
    Check(DeserializeWorldSave(SerializeWorldSave(fresh), freshBack).HashVerified, "a new life saves certified");
}

void R84WriterAndReaderRefusals()
{
    const CharacterCreationSave creation = MakeCreation("parent_household");
    const WorldSaveData data = R84Save(creation, R84History(1, 70));
    const std::string text = SerializeWorldSave(data);
    WorldSaveData lagging = data;
    lagging.World.Commerce = R84History(1, 69);
    Check(SerializeWorldSave(lagging).empty(), "the writer refuses an allowance cursor behind the clock");
    WorldSaveData otherRank = R84Save(creation, R84History(2, 70));
    Check(SerializeWorldSave(otherRank).empty(), "the writer refuses funding from another household rank");
    WorldSaveData invalid = data;
    invalid.World.Commerce.WalletPaise += 1;
    Check(SerializeWorldSave(invalid).empty(), "the writer refuses commerce that does not reconcile");

    const std::string envelope = R84Quoted(SerialiseCommerceState(data.World.Commerce));
    Check(text.find(envelope) != std::string::npos, "the envelope is stored as one string");
    Check(ErrorOf(ReplaceOnce(text, envelope, R84Quoted(SerialiseCommerceState(R84History(2, 70))))) == WorldSaveError::InvalidWorld,
          "another household's funding is refused");
    Check(ErrorOf(ReplaceOnce(text, envelope, R84Quoted(SerialiseCommerceState(R84History(1, 69))))) == WorldSaveError::InvalidWorld,
          "an allowance cursor away from the clock is refused");
    const std::string wallet = "wallet=" + std::to_string(data.World.Commerce.WalletPaise);
    Check(ErrorOf(ReplaceOnce(text, wallet, "wallet=" + std::to_string(data.World.Commerce.WalletPaise + 100))) == WorldSaveError::InvalidWorld,
          "a forged wallet is refused");
    const std::string revision = "revision=" + std::to_string(data.World.Commerce.TransactionRevision);
    Check(ErrorOf(ReplaceOnce(text, revision, "revision=0" + std::to_string(data.World.Commerce.TransactionRevision))) == WorldSaveError::InvalidWorld,
          "non-canonical commerce text is refused");
    const std::string tea = "tea_eligible=" + std::to_string(data.World.Commerce.NextTeaEligibleAt);
    Check(ErrorOf(ReplaceOnce(text, tea, "tea_eligible=" + std::to_string(data.World.Commerce.NextTeaEligibleAt - 1))) == WorldSaveError::HashMismatch,
          "a valid but altered eligibility instant fails the checksum");
    Check(ErrorOf(ReplaceOnce(text, ",\n    \"commerce\": " + envelope, "")) == WorldSaveError::Malformed, "v10 requires commerce");
    Check(ErrorOf(ReplaceOnce(text, "\"schemaVersion\": 10", "\"schemaVersion\": 9")) == WorldSaveError::Malformed,
          "a v9 record refuses the commerce section");
    Check(ErrorOf(ReplaceOnce(text, "\"schemaVersion\": 10", "\"schemaVersion\": 11")) == WorldSaveError::UnsupportedVersion,
          "a newer schema is refused");
    Check(ErrorOf(ReplaceOnce(text, envelope, "17")) == WorldSaveError::InvalidWorld, "commerce must be text");
}

/** The household ranks 0..4 from verified presets: a swap of two traits keeps the 1,000-point budget. */
CharacterCreationSave R84CreationOfRank(std::int32_t rank)
{
    CharacterCreator creator;
    const char* preset = rank == 0 ? "independent_generalist" : rank == 4 ? "resourceful_maker" : "balanced_scholar";
    Check(creator.ApplyPreset(preset).Succeeded(), "the preset applies");
    const char* lowered = rank == 2 ? "housing_stability" : rank == 3 ? "learning_discipline" : rank == 4 ? "practical_experience" : nullptr;
    if (lowered != nullptr)
        Check(creator.SetTraitRank(lowered, 1).Succeeded() && creator.SetTraitRank(HouseholdResourcesTraitId, rank).Succeeded(),
              "the household rank swaps with another trait");
    Check(creator.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded() && creator.StartLife().Succeeded(), "life starts");
    Check(creator.GetTraitRank(HouseholdResourcesTraitId) == rank, "the life has the requested household rank");
    return creator.CaptureSave();
}

void R84LegacyMigration()
{
    constexpr std::int64_t Table[] = {12000, 15000, 30000, 60000, 120000};
    for (std::int32_t rank = 0; rank <= 4; ++rank)
    {
        const CharacterCreationSave creation = R84CreationOfRank(rank);
        WorldSaveData data = R84Save(creation, R84History(rank, 70));
        Check(SerializeWorldSave(data).size() > 0, "the rank's v10 save is written");
        for (const int version : {9, 8})
        {
            const std::string old = R84Downgrade(SerializeWorldSave(data), data.World, version);
            WorldSaveData loaded;
            const WorldSaveResult result = DeserializeWorldSave(old, loaded);
            Check(result.Succeeded() && result.SourceVersion == version && !result.HashVerified && !result.Migrated,
                  "a historical world authenticates its original checksum and loads honestly unverified");
            const GameInstant saved = data.World.Clock.Now();
            const CommerceState& commerce = loaded.World.Commerce;
            Check(commerce == InitialiseCommerce(rank, saved).State && commerce.WalletPaise == Table[rank] &&
                      commerce.Funding.WeeklyAllowancePaise == Table[rank] && commerce.NextAllowanceDue == saved + CommerceWeekMinutes &&
                      commerce.RecentReceipts.empty() && commerce.TransactionRevision == 0 &&
                      commerce.Inventory == CommerceState{}.Inventory,
                  "the household's starting wallet once, an empty bag and the next allowance a week after the save: no backpay");
            WorldSaveData expected = data;
            expected.World.Commerce = commerce;
            CheckSame(expected, loaded, "everything else in the historical world survives");
            WorldSaveData again;
            Check(DeserializeWorldSave(SerializeWorldSave(loaded), again).HashVerified && again.World.Commerce == commerce,
                  "the migrated world's first save is certified v10");
            Check(ErrorOf(ReplaceOnce(old, "\"seed\": \"0x0000000000000055\"", "\"seed\": \"0x0000000000000056\"")) == WorldSaveError::HashMismatch,
                  "the historical world is authenticated before it migrates");
            const std::string relabelled = ReplaceOnce(old, version == 9 ? "\"hashVersion\": 6" : "\"hashVersion\": 5", "\"hashVersion\": 7");
            WorldSaveData relabelledOut;
            const WorldSaveResult relabelledResult = DeserializeWorldSave(relabelled, relabelledOut);
            Check(relabelledResult.Succeeded() && !relabelledResult.HashVerified, "a historical schema never claims today's certification");
        }
    }
    // A creation-only record (schema 5 and earlier) begins its world with its own household's funding.
    const CharacterCreationSave creation = R84CreationOfRank(3);
    WorldSaveData migrated;
    const WorldSaveResult begun = DeserializeWorldSave(SerializeCreationSave(creation), migrated);
    Check(begun.Succeeded() && begun.Migrated && migrated.World.Commerce == InitialiseCommerce(3, 0).State,
          "a creation-only record begins with the starting wallet of rank 3");
}

void R84ContentAware()
{
    const CharacterCreationSave creation = MakeCreation("parent_household");
    const WorldSaveData data = R84Save(creation, R84History(1, 70));
    const std::string text = SerializeWorldSave(data);
    CommerceContent content;
    content.Items = {R84Pen, R84Tea};
    content.Shops = {CommerceShop{"shop.shanti_chai", "Shanti Chai", "anchor.a", {}, {}, {}},
                     CommerceShop{"shop.stationery", "Stationery", "anchor.b", {}, {}, {}}};
    WorldSaveData loaded;
    Check(DeserializeWorldSave(text, loaded, content).HashVerified && IsCommerceStateInContent(loaded.World.Commerce, content),
          "known goods and shops load with content");
    CommerceContent noPen = content;
    noPen.Items = {R84Tea};
    WorldSaveData refused = Sentinel();
    const WorldSaveResult result = DeserializeWorldSave(text, refused, noPen);
    Check(result.Error == WorldSaveError::InvalidWorld && Untouched(refused) && !result.Message.empty(), "an unknown item is refused atomically");
    CommerceContent noShop = content;
    noShop.Shops.pop_back();
    Check(!IsCommerceStateInContent(data.World.Commerce, noShop), "an unknown shop in a receipt is refused");
    Check(IsCommerceStateInContent(InitialiseCommerce(0, 0).State, CommerceContent{}), "a fresh household names no content");
}
} // namespace

int main()
{
    R84Envelope();
    R84RoundTrip();
    R84WriterAndReaderRefusals();
    R84LegacyMigration();
    R84ContentAware();
    TestWorldSaveCommerceFixtures();
    std::cout << "PASS: " << R84SaveChecks << " world save commerce checks\n";
    return 0;
}
