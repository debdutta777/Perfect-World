// R8-04: the commerce hash section (hash version 7), the historical encodings that exclude it, the frozen v9 fixture's
// independent authentication, and snapshot-based replay that keeps a household's funding policy.
#include "WorldHash.h"

#include "CharacterCreation.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace PerfectWorld;

int R84Checks = 0;
void Check(bool condition, const char* message)
{
    ++R84Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#include "WorldHashReference.h"

namespace
{
const CommerceItem R84Tea{"item.tea", "Tea", "cup", 10, CommerceEffect::Tea, 0};
const CommerceItem R84Pen{"item.pen", "Pen", "pen", 10, CommerceEffect::None, 0};

CommercePurchaseContext R84Open(GameInstant now) { return {now, true, true, true, true, true, true, true}; }

/** A long history through the pure rules: weekly allowances, purchases, receipt folding past 64, and tea taken. */
CommerceState R84RichCommerce(std::int32_t rank, int weeks)
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
        const CommerceStateResult bought = ApplyPurchase(state, week % 3 == 0 ? pen : tea, week % 3 == 0 ? R84Pen : R84Tea, 1,
                                                         state.TransactionRevision, R84Open(now));
        Check(bought.Ok(), "a weekly purchase succeeds");
        state = bought.State;
    }
    const CommerceConsumptionResult used = Consume(state, ConditionState{}, R84Tea, now, {true, false, true});
    Check(used.Ok(), "tea is taken");
    return used.State;
}

/** A world in step at the commerce cursor. */
WorldState R84WorldWith(const CommerceState& commerce)
{
    WorldState world = MakeWorldState(84);
    Check(!world.Clock.AdvanceTo(commerce.AllowanceSettledThrough).Rejected, "the clock reaches the cursor");
    world.DayLoop = NewDayLoopState(commerce.AllowanceSettledThrough);
    world.Commerce = commerce;
    Check(IsWorldDayInStep(world) && IsCommerceStateValid(world.Commerce), "a rich world is valid");
    return world;
}

void R84CommerceFieldEdits()
{
    const WorldState base = R84WorldWith(R84RichCommerce(2, 70));
    Check(base.Commerce.FoldedReceiptCount > 0 && base.Commerce.RecentReceipts.size() == CommerceRecentReceiptLimit &&
              base.Commerce.RecentReceipts.front().Kind != base.Commerce.RecentReceipts.back().Kind,
          "the base history folds and mixes receipt kinds");
    CheckAgainstReference(base, "the rich commerce world matches the reference encoder");
    using Edit = std::pair<const char*, std::function<void(CommerceState&)>>;
    const std::vector<Edit> edits = {
        {"wallet", [](CommerceState& c) { c.WalletPaise += 1; }},
        {"slot 0 item", [](CommerceState& c) { c.Inventory[0].ItemId = "item.tez"; }},
        {"slot 0 quantity", [](CommerceState& c) { c.Inventory[0].Quantity += 1; }},
        {"empty slot 11 item", [](CommerceState& c) { c.Inventory[11].ItemId = "item.tea"; }},
        {"empty slot 11 quantity", [](CommerceState& c) { c.Inventory[11].Quantity = 1; }},
        {"slots swapped", [](CommerceState& c) { std::swap(c.Inventory[0], c.Inventory[1]); }},
        {"funding rank", [](CommerceState& c) { c.Funding.HouseholdResourcesRank += 1; }},
        {"funding start", [](CommerceState& c) { c.Funding.StartingWalletPaise += 1; }},
        {"funding weekly", [](CommerceState& c) { c.Funding.WeeklyAllowancePaise += 1; }},
        {"initialised", [](CommerceState& c) { c.InitialisedAt += 1; }},
        {"settled", [](CommerceState& c) { c.AllowanceSettledThrough += 1; }},
        {"next allowance", [](CommerceState& c) { c.NextAllowanceDue += 1; }},
        {"revision", [](CommerceState& c) { c.TransactionRevision += 1; }},
        {"revision high bit", [](CommerceState& c) { c.TransactionRevision ^= 0x8000000000000000ULL; }},
        {"receipt dropped", [](CommerceState& c) { c.RecentReceipts.pop_back(); }},
        {"receipt kind", [](CommerceState& c) { c.RecentReceipts[0].Kind = c.RecentReceipts[0].Kind == CommerceReceiptKind::Allowance ? CommerceReceiptKind::Purchase : CommerceReceiptKind::Allowance; }},
        {"receipt revision", [](CommerceState& c) { c.RecentReceipts[0].Revision += 1; }},
        {"receipt at", [](CommerceState& c) { c.RecentReceipts[0].At += 1; }},
        {"receipt shop", [](CommerceState& c) { c.RecentReceipts.back().ShopId += "x"; }},
        {"receipt item", [](CommerceState& c) { c.RecentReceipts.back().ItemId += "x"; }},
        {"receipt quantity", [](CommerceState& c) { c.RecentReceipts.back().Quantity += 1; }},
        {"receipt unit", [](CommerceState& c) { c.RecentReceipts.back().UnitPricePaise += 1; }},
        {"receipt total", [](CommerceState& c) { c.RecentReceipts.back().TotalPaise += 1; }},
        {"receipt before", [](CommerceState& c) { c.RecentReceipts.back().BalanceBeforePaise += 1; }},
        {"receipt after", [](CommerceState& c) { c.RecentReceipts.back().BalanceAfterPaise += 1; }},
        {"receipts swapped", [](CommerceState& c) { std::swap(c.RecentReceipts[0], c.RecentReceipts[1]); }},
        {"folded count", [](CommerceState& c) { c.FoldedReceiptCount += 1; }},
        {"folded through", [](CommerceState& c) { c.FoldedThroughRevision += 1; }},
        {"folded credits", [](CommerceState& c) { c.FoldedCreditsPaise += 1; }},
        {"folded debits", [](CommerceState& c) { c.FoldedDebitsPaise += 1; }},
        {"tea eligibility", [](CommerceState& c) { c.NextTeaEligibleAt += 1; }},
        {"consumption eligibility", [](CommerceState& c) { c.NextConsumptionEligibleAt += 1; }},
    };
    const auto baseSections = HashWorldSections(base);
    const std::string doors = SerialiseDoorState(base.Doors);
    std::set<std::uint64_t> seen{HashWorldState(base)};
    for (const auto& [name, apply] : edits)
    {
        WorldState edited = base;
        apply(edited.Commerce);
        Check(!(edited.Commerce == base.Commerce), name);
        const auto sections = HashWorldSections(edited);
        Check(sections.size() == baseSections.size() && sections.back().Name == "commerce", name);
        for (std::size_t i = 0; i < sections.size(); ++i)
            Check((sections[i].Hash != baseSections[i].Hash) == (sections[i].Name == "commerce"), name);
        CheckAgainstReference(edited, name);
        Check(HashWorldStateWithDoorsText(edited, doors) == HashWorldState(edited), "the current encoding includes commerce");
        Check(HashWorldStateWithDoorsText(edited, doors, 6) == HashWorldStateWithDoorsText(base, doors, 6) &&
                  HashWorldStateWithDoorsText(edited, doors, 5) == HashWorldStateWithDoorsText(base, doors, 5),
              "historical hash versions 5 and 6 never saw commerce");
        seen.insert(HashWorldState(edited));
    }
    Check(seen.size() == edits.size() + 1, "every distinct commerce edit gives a distinct world hash");
    WorldState journal = base;
    journal.Journal.Encounters.push_back({1, "person.household_adult", 7});
    Check(HashWorldStateWithDoorsText(journal, doors, 6) != HashWorldStateWithDoorsText(base, doors, 6) &&
              HashWorldStateWithDoorsText(journal, doors, 5) == HashWorldStateWithDoorsText(base, doors, 5),
          "hash 6 includes the journal and hash 5 does not");
}

#ifdef PW_COMMERCE_FIXTURE_DIR
/** The frozen v9 writer's checksum, recomputed by the independent reference with its nine hash-6 sections. */
void R84FrozenFixtureAuthentication()
{
    std::ifstream in(std::string(PW_COMMERCE_FIXTURE_DIR) + "/world_v9_doors_29.json", std::ios::binary);
    Check(static_cast<bool>(in), "the frozen v9 fixture is readable");
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    Check(text.size() == 4660 && text.find("\"stateHash\": \"0x49b5048af20be33a\"") != std::string::npos, "the frozen fixture is untouched");
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    Check(result.Succeeded() && result.SourceVersion == 9 && !result.HashVerified && !result.Migrated, "the v9 fixture migrates honestly");
    RefBytes all;
    all.U(6);
    for (const auto& [name, hash] : RefSections(loaded.World))
    {
        if (name == "commerce")
            continue;
        all.S(name);
        all.U(hash);
    }
    Check(RefFnv1a(all.Data) == 0x49b5048af20be33aULL, "the reference's hash-6 encoding reproduces the stored checksum");
    Check(HashWorldStateWithDoorsText(loaded.World, SerialiseDoorState(loaded.World.Doors), 6) == 0x49b5048af20be33aULL,
          "the library's hash-6 encoding agrees");
    Check(HashWorldState(loaded.World) != 0x49b5048af20be33aULL && RefWorld(loaded.World) == HashWorldState(loaded.World),
          "the migrated world hashes differently, as version 7");
}
#endif

// ---- Snapshot replay through the reducer's commerce commands ----

struct R84Shop
{
    std::vector<CommerceItem> Items{R84Tea, R84Pen};
    std::vector<CommerceCatalogueEntry> Entries{{"shop.test", "item.tea", 2500}, {"shop.test", "item.pen", 2000}};
    PersonSchedule Keeper{"person.mahua_saha", "anchor.hardware_store.staff_counter", {
        {ScheduleAllWeekdays, 0, 360, "anchor.hardware_store.staff_counter", ScheduleDoingKind::Role, "sleeping", 0},
        {ScheduleAllWeekdays, 360, 1200, "anchor.hardware_store.staff_counter", ScheduleDoingKind::Role, "serving", 0},
        {ScheduleAllWeekdays, 1200, 1440, "anchor.hardware_store.staff_counter", ScheduleDoingKind::Role, "sleeping", 0}}, {}};
    ScheduleTravelTable Travel;
    std::vector<std::string_view> Anchors{"anchor.hardware_store.staff_counter"};
    std::vector<CommerceServiceBinding> Shops{{"shop.test", "person.mahua_saha", "anchor.customer", &Keeper, &Travel, Anchors,
                                               ScheduleAllWeekdays, 360, 1200, {}}};
    WorldCommerceCatalogues Cats() const { return {Items, Entries, Shops}; }
};

void R84Submit(std::vector<WorldSim*> sims, const CommandPayload& payload)
{
    for (WorldSim* sim : sims)
    {
        const CommandResult result = sim->Submit({CommandSource::Player, payload});
        if (!result.Accepted)
            std::cerr << result.Why.Code << '\n';
        Check(result.Accepted, "the command is accepted");
    }
}

void R84Purchase(std::vector<WorldSim*> sims, const char* item, std::int32_t quantity)
{
    R84Submit(sims, AdvanceTime{1});
    R84Submit(sims, CommerceServiceRequest{"shop.test", "anchor.customer"});
    if (const auto ready = sims.front()->CommerceService("shop.test").ReadyAt)
        R84Submit(sims, AdvanceTime{*ready - sims.front()->State().Clock.Now()});
    Check(sims.front()->CommerceService("shop.test").Ready(), "the counter is ready");
    R84Submit(sims, CommerceBuy{"shop.test", item, quantity, sims.front()->State().Commerce.TransactionRevision});
    R84Submit(sims, CommerceServiceClose{"shop.test"});
}

void R84SnapshotReplay()
{
    const R84Shop shop;
    CharacterCreator creator;
    Check(creator.ApplyPreset("balanced_scholar").Succeeded() && creator.SetHouseholdTemplate("parent_household").Succeeded() &&
              creator.SetIdentity({"Ananya", "Sen", "she/her", "Kolkata", "Bengali"}).Succeeded() && creator.StartLife().Succeeded(),
          "a life is created");
    const std::int32_t rank = creator.GetTraitRank(HouseholdResourcesTraitId);
    Check(rank == 1, "the preset's household resources rank is 1");
    const WorldState initial = MakeWorldState(84, creator);
    WorldSim live(initial, MaxEventsPerAdvance, {}, shop.Cats());
    R84Purchase({&live}, "item.tea", 2);
    R84Submit({&live}, InventoryConsume{"item.tea"});

    WorldSaveData data;
    data.Creation = creator.CaptureSave();
    data.World = live.State();
    data.Player.ZoneId = "zone.street.nabapara_lane";
    WorldSaveData loaded;
    const WorldSaveResult load = DeserializeWorldSave(SerializeWorldSave(data), loaded);
    Check(load.Succeeded() && load.HashVerified && load.SourceVersion == WorldSaveSchemaVersion && loaded.World.Commerce == live.State().Commerce,
          "a mid-life commerce save is certified and exact");
    WorldSim resumed(loaded.World, MaxEventsPerAdvance, {}, shop.Cats());
    for (int day = 0; day < 7; ++day)
        R84Submit({&live, &resumed}, AdvanceTime{MinutesPerDay});
    R84Purchase({&live, &resumed}, "item.pen", 3);
    Check(HashWorldState(live.State()) == HashWorldState(resumed.State()) && live.State().Commerce == resumed.State().Commerce,
          "the resumed life continues exactly as the uninterrupted one");
    const CommerceState& commerce = live.State().Commerce;
    Check(commerce.Funding.HouseholdResourcesRank == rank && commerce.WalletPaise == 15000 - 5000 + 15000 - 6000 &&
              commerce.RecentReceipts.size() == 3 && commerce.RecentReceipts[1].Kind == CommerceReceiptKind::Allowance,
          "rank-1 funding paid one weekly allowance between two purchases");

    ReplayOptions options;
    options.Commerce = shop.Cats();
    options.CheckpointEvery = 4;
    const ReplayResult full = ReplayCommandLogFromSnapshot(initial, live.Log(), options);
    Check(full.Complete && full.Applied == live.Log().size() && full.Hash == HashWorldState(live.State()) && !full.Checkpoints.empty(),
          "snapshot replay from the creation world reproduces the life");
    const ReplayResult tail = ReplayCommandLogFromSnapshot(loaded.World, resumed.Log(), options);
    Check(tail.Complete && tail.Hash == HashWorldState(resumed.State()), "snapshot replay from a loaded save reproduces the continuation");
    const ReplayResult seedOnly = ReplayCommandLog(84, live.Log(), options);
    Check(seedOnly.Hash != HashWorldState(live.State()), "seed-only replay begins a rank-0 household, so it cannot stand in");
    const ReplayResult unbound = ReplayCommandLogFromSnapshot(initial, live.Log());
    Check(!unbound.Complete && unbound.Applied < live.Log().size(), "replay without the commerce binding refuses the first sale");
}
} // namespace

int main()
{
    R84CommerceFieldEdits();
#ifdef PW_COMMERCE_FIXTURE_DIR
    R84FrozenFixtureAuthentication();
#else
    Check(false, "PW_COMMERCE_FIXTURE_DIR must be defined by CMakeLists.txt");
#endif
    R84SnapshotReplay();
    std::cout << "PASS: " << R84Checks << " world hash commerce checks\n";
    return 0;
}
