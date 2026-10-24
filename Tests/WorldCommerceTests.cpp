#include "WorldCommerce.h"

#include "WeekCompression.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace PerfectWorld;

namespace
{
int R83Assertions = 0;
void R83Expect(bool condition, const char* message)
{
    ++R83Assertions;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

struct R83Content
{
    std::vector<CommerceItem> Items{
        {"item.tea", "Tea", "cup", 10, CommerceEffect::Tea, 0},
        {"item.food", "Food", "piece", 10, CommerceEffect::Food, 3000},
        {"item.plaster", "Plasters", "pack", 10, CommerceEffect::UnsupportedSupply, 0},
        {"item.pen", "Pen", "pen", 10, CommerceEffect::None, 0}};
    std::vector<CommerceCatalogueEntry> Entries{
        {"shop.test", "item.tea", 2500}, {"shop.test", "item.food", 1000},
        {"shop.test", "item.plaster", 4000}, {"shop.test", "item.pen", 2000}};
    PersonSchedule Keeper{"person.mahua_saha", "anchor.hardware_store.staff_counter", {
        {ScheduleAllWeekdays, 0, 360, "anchor.hardware_store.staff_counter", ScheduleDoingKind::Role, "sleeping", 0},
        {ScheduleAllWeekdays, 360, 1200, "anchor.hardware_store.staff_counter", ScheduleDoingKind::Role, "serving", 0},
        {ScheduleAllWeekdays, 1200, 1440, "anchor.hardware_store.staff_counter", ScheduleDoingKind::Role, "sleeping", 0}}, {}};
    ScheduleTravelTable Travel;
    std::vector<std::string_view> Anchors{"anchor.hardware_store.staff_counter"};
    std::vector<CommerceServiceBinding> Shops{{"shop.test", "person.mahua_saha", "anchor.customer",
        &Keeper, &Travel, Anchors, ScheduleAllWeekdays, 360, 1200, {}}};
    ActivityCatalogue Activities;
    R83Content()
    {
        ActivityDef activity;
        activity.Id = "activity.rest"; activity.Name = "Rest"; activity.DurationMinutes = 10;
        activity.Place = "home"; activity.Condition = ConditionActivity::Resting; activity.Outcome = "rested";
        Activities.Defs.push_back(activity);
        R83Expect(ValidatePersonSchedule(Keeper).empty() && ValidateNpcSchedule(Keeper).empty(), "fixture schedule valid");
    }
    WorldCommerceCatalogues Cats() const { return {Items, Entries, Shops}; }
    DayLoopCatalogues DayCats() const { return {&Activities, nullptr, nullptr}; }
    WorldSim Sim(WorldState state = MakeWorldState(83), std::size_t cap = MaxEventsPerAdvance) const
    { return WorldSim(std::move(state), cap, DayCats(), Cats()); }
};

CommandResult R83Submit(WorldSim& sim, CommandPayload command)
{
    const auto result = sim.Submit({CommandSource::Player, std::move(command)});
    if (!result.Accepted) std::cerr << result.Why.Code << ' ' << result.Why.Message << '\n';
    R83Expect(result.Accepted, "command accepted");
    R83Expect(IsWorldDayInStep(sim.State()), "day and world stay in step");
    R83Expect(IsCommerceStateValid(sim.State().Commerce), "commerce remains valid");
    R83Expect(sim.State().Commerce.AllowanceSettledThrough == sim.State().Clock.Now(), "allowance cursor follows clock");
    return result;
}

void R83Refused(WorldSim& sim, CommandPayload command, std::string_view code)
{
    const auto before = sim.State();
    const auto logSize = sim.Log().size();
    const auto hash = HashWorldState(before);
    const auto decision = sim.CommerceKeeperDecision("shop.test");
    const auto service = sim.CommerceService("shop.test");
    const auto result = sim.Submit({CommandSource::Player, std::move(command)});
    if (result.Accepted || result.Why.Code != code)
        std::cerr << "wanted " << code << ", got " << result.Why.Code << '\n';
    R83Expect(!result.Accepted && result.Why.Code == code, "stable refusal code");
    R83Expect(result.Events.empty() && !result.Partial && result.From == result.To && result.Seq == before.Seq,
        "refusal is zero minute and emits no sequence or events");
    R83Expect(sim.State().Seq == before.Seq && sim.Log().size() == logSize && HashWorldState(sim.State()) == hash &&
        sim.State().Commerce == before.Commerce && sim.State().DayLoop == before.DayLoop, "full refusal atomicity");
    R83Expect(sim.CommerceKeeperDecision("shop.test") == decision &&
        sim.CommerceService("shop.test").Requested == service.Requested, "refusal preserves transient service history");
}

CommerceBuy R83Buy(const WorldSim& sim, std::string id = "item.tea", std::int32_t quantity = 1)
{ return {"shop.test", std::move(id), quantity, sim.State().Commerce.TransactionRevision}; }

void R83Request(WorldSim& sim)
{ R83Submit(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}); }

void R83Ready(WorldSim& sim)
{
    R83Request(sim);
    const auto view = sim.CommerceService("shop.test");
    if (view.ReadyAt) R83Submit(sim, AdvanceTime{*view.ReadyAt - sim.State().Clock.Now()});
    R83Expect(sim.CommerceService("shop.test").Ready(), "safe service ready");
}

WorldDayCommand R83Day(DayCommandKind kind, std::int64_t minutes = 0)
{
    WorldDayCommand command;
    command.Day.Kind = kind; command.Day.Minutes = minutes;
    return command;
}

WorldState R83At(GameInstant at)
{
    auto state = MakeWorldState(83);
    R83Expect(!state.Clock.AdvanceTo(at).Rejected, "fixture clock moves");
    state.DayLoop = NewDayLoopState(at);
    state.Commerce = SettleAllowance(state.Commerce, 0, at).State;
    return state;
}

void R83ServiceAndReplay()
{
    R83Content content;
    auto sim = content.Sim();
    R83Refused(sim, R83Buy(sim), "commerce.not_at_safe_pause");
    R83Refused(sim, CommerceServiceRequest{"missing", "anchor.customer"}, "commerce.invalid_item");
    R83Refused(sim, CommerceServiceRequest{"shop.test", "wrong"}, "commerce.not_at_counter");
    R83Refused(sim, CommerceServiceClose{"shop.test"}, "commerce.service_not_open");
    R83Submit(sim, AdvanceTime{1});
    R83Request(sim);
    R83Expect(sim.CommerceService("shop.test").ReadyAt == 3, "request waits for real three-minute safe point");
    R83Refused(sim, R83Buy(sim), "commerce.not_at_safe_pause");
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.service_active");
    R83Refused(sim, CommerceServiceClose{"other"}, "commerce.service_not_open");
    R83Submit(sim, AdvanceTime{2});
    R83Refused(sim, AdvanceTime{1}, "time.service_open");
    R83Refused(sim, ConversationOpen{"person.farida_ahmed"}, "commerce.service_active");
    R83Refused(sim, R83Day(DayCommandKind::GoToSleep), "commerce.service_active");
    const auto quote = sim.QuoteCommerce("shop.test", "item.tea", 2);
    R83Expect(quote.Ok() && quote.TotalPaise == 5000 && quote.WalletAfterPaise == 7000, "portable quote uses bound price");
    const auto buy = R83Buy(sim, "item.tea", 2);
    const auto purchased = R83Submit(sim, buy);
    R83Expect(purchased.From == purchased.To && sim.State().Commerce.WalletPaise == 7000, "buy is zero minute");
    R83Refused(sim, buy, "commerce.stale_revision");
    R83Submit(sim, InventoryConsume{"item.tea"});
    R83Refused(sim, InventoryConsume{"item.tea"}, "commerce.cooldown");
    R83Submit(sim, CommerceServiceClose{"shop.test"});
    R83Expect(sim.CommerceKeeperDecision("shop.test").Intent.State == NpcState::Work, "closing resumes work");
    R83Submit(sim, AdvanceTime{1});
    R83Ready(sim);
    R83Submit(sim, CommerceServiceClose{"shop.test"});
    R83Submit(sim, AdvanceTime{60});
    R83Submit(sim, InventoryConsume{"item.tea"});
    auto replay = content.Sim();
    for (const auto& command : sim.Log()) R83Expect(replay.Submit(command).Accepted, "recorded service inputs replay");
    R83Expect(replay.State().Commerce == sim.State().Commerce && replay.State().DayLoop == sim.State().DayLoop &&
        HashWorldState(replay.State()) == HashWorldState(sim.State()) &&
        replay.CommerceKeeperDecision("shop.test") == sim.CommerceKeeperDecision("shop.test"), "deterministic replay including released windows");
    R83Ready(sim);
    auto loaded = content.Sim(sim.State());
    R83Expect(!loaded.CommerceService("shop.test").Requested, "snapshot load invalidates service");
    R83Refused(loaded, R83Buy(loaded), "commerce.not_at_safe_pause");
}

void R83Refusals()
{
    R83Content content;
    auto sim = content.Sim(); R83Ready(sim);
    R83Refused(sim, R83Buy(sim, "unknown"), "commerce.invalid_item");
    R83Refused(sim, R83Buy(sim, "item.tea", 0), "commerce.invalid_quantity");
    R83Refused(sim, R83Buy(sim, "item.tea", -1), "commerce.invalid_quantity");
    R83Refused(sim, R83Buy(sim, "item.tea", 100), "commerce.insufficient_funds");
    R83Expect(sim.QuoteCommerce("shop.test", "unknown", 1).Error == CommerceError::InvalidItem, "unknown quote refused");
    for (const auto& item : content.Items)
        R83Refused(sim, InventoryConsume{item.Id}, item.Effect == CommerceEffect::None || item.Effect == CommerceEffect::UnsupportedSupply
            ? "commerce.use_unavailable" : "commerce.not_owned");
    auto state = sim.State(); state.Commerce.WalletPaise = -1;
    auto invalid = content.Sim(state); R83Refused(invalid, R83Buy(invalid), "commerce.invalid_state");
    state = sim.State(); state.Commerce = InitialiseCommerce(0, 1).State;
    invalid = content.Sim(state); R83Refused(invalid, R83Buy(invalid), "commerce.invalid_time");
    state = sim.State(); state.DayLoop.Asleep = true; state.DayLoop.SleptAt = state.DayLoop.Now;
    state.DayLoop.Phase = DayLoopPhaseOf(state.DayLoop);
    auto asleep = content.Sim(state);
    R83Refused(asleep, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.player_asleep");
    R83Refused(asleep, InventoryConsume{"item.tea"}, "commerce.player_asleep");
    state = MakeWorldState(83);
    R83Expect(ApplyDayCommand(state.DayLoop, content.DayCats(), R83Day(DayCommandKind::PlanDay).Day).Ok(), "fixture plan");
    auto commit = R83Day(DayCommandKind::Commit); commit.Day.ActivityId = "activity.rest"; commit.Day.PlaceKind = "home";
    R83Expect(ApplyDayCommand(state.DayLoop, content.DayCats(), commit.Day).Ok() &&
        ApplyDayCommand(state.DayLoop, content.DayCats(), R83Day(DayCommandKind::Start).Day).Ok(), "fixture activity active");
    auto busy = content.Sim(state);
    R83Refused(busy, InventoryConsume{"item.tea"}, "commerce.player_busy");
    R83Refused(busy, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.player_busy");
    R83Expect(sim.Submit({CommandSource::System, ScheduleWorldEvent{sim.State().Clock.Now(), "test/event/due"}}).Accepted,
        "due event scheduled");
    R83Refused(sim, R83Buy(sim), "commerce.world_unsettled");
    R83Refused(sim, InventoryConsume{"item.tea"}, "commerce.world_unsettled");
    auto unsettled = content.Sim(sim.State());
    R83Refused(unsettled, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.world_unsettled");
}

void R83CapacityAndOverflow()
{
    R83Content content;
    auto state = MakeWorldState(83);
    for (std::size_t i = 0; i < state.Commerce.Inventory.size(); ++i)
        state.Commerce.Inventory[i] = {"item.other" + std::to_string(i), 10};
    auto full = content.Sim(state); R83Ready(full);
    R83Refused(full, R83Buy(full), "commerce.bag_full");
    for (auto& slot : state.Commerce.Inventory) slot = {"item.tea", 10};
    auto stacked = content.Sim(state); R83Ready(stacked);
    R83Refused(stacked, R83Buy(stacked), "commerce.stack_full");
    content.Entries[0].UnitPricePaise = std::numeric_limits<std::int64_t>::max();
    auto expensive = content.Sim(); R83Ready(expensive);
    R83Refused(expensive, R83Buy(expensive, "item.tea", 2), "commerce.arithmetic_overflow");
    content.Entries[0].UnitPricePaise = 2500;
    state = MakeWorldState(83); state.Commerce.TransactionRevision = std::numeric_limits<std::uint64_t>::max();
    auto overflow = content.Sim(state); R83Ready(overflow);
    R83Refused(overflow, R83Buy(overflow), "commerce.arithmetic_overflow");
    R83Submit(overflow, CommerceServiceClose{"shop.test"});
    R83Refused(overflow, AdvanceTime{CommerceWeekMinutes}, "commerce.arithmetic_overflow");
}

void R83ClosingAndHistory()
{
    R83Content content;
    content.Shops[0].CloseMinute = 365;
    auto sim = content.Sim(); R83Submit(sim, AdvanceTime{4}); R83Request(sim);
    R83Submit(sim, AdvanceTime{2});
    R83Refused(sim, R83Buy(sim), "commerce.closed");
    R83Submit(sim, CommerceServiceClose{"shop.test"});
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.closed");
    content.Shops[0].CloseMinute = 1200;
    content.Keeper.Routine[1].EndMinute = 365; content.Keeper.Routine[2].StartMinute = 365;
    sim = content.Sim(); R83Submit(sim, AdvanceTime{4}); R83Request(sim); R83Submit(sim, AdvanceTime{2});
    R83Refused(sim, R83Buy(sim), "commerce.keeper_away");
    content.Keeper.Routine[2].Doing = "break";
    sim = content.Sim(R83At(6));
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.keeper_away");
    content.Keeper.Routine[1].EndMinute = 1200; content.Keeper.Routine[2].StartMinute = 1200;
    sim = content.Sim(); R83Submit(sim, AdvanceTime{1}); R83Request(sim);
    R83Submit(sim, AdvanceTime{1}); R83Submit(sim, CommerceServiceClose{"shop.test"});
    R83Request(sim); R83Expect(sim.CommerceService("shop.test").ReadyAt == 3, "cancel pending request does not earn a safe pause");
    R83Submit(sim, AdvanceTime{1}); R83Submit(sim, CommerceServiceClose{"shop.test"});
    R83Submit(sim, AdvanceTime{1}); R83Request(sim);
    R83Expect(sim.CommerceService("shop.test").ReadyAt == 6, "released history preserves unfinished work");
    R83Submit(sim, AdvanceTime{2});
    sim = content.Sim(); R83Submit(sim, AdvanceTime{1}); R83Request(sim);
    R83Submit(sim, AdvanceTime{4}); R83Submit(sim, CommerceServiceClose{"shop.test"});
    R83Submit(sim, AdvanceTime{1}); R83Request(sim);
    R83Expect(sim.CommerceService("shop.test").ReadyAt == 8, "a positive pause shifts the next work boundary after reopening");
    // A changed work segment must not inherit an earlier segment's accepted pause.
    content.Keeper.Routine[1].EndMinute = 365;
    content.Keeper.Routine.insert(content.Keeper.Routine.begin() + 2,
        {ScheduleAllWeekdays, 365, 1200, "anchor.hardware_store.staff_counter", ScheduleDoingKind::Role, "serving", 0});
    sim = content.Sim(); R83Submit(sim, AdvanceTime{4}); R83Request(sim); R83Submit(sim, AdvanceTime{2});
    R83Refused(sim, R83Buy(sim), "commerce.not_at_safe_pause");
    R83Submit(sim, CommerceServiceClose{"shop.test"}); R83Ready(sim);
    R83Expect(sim.CommerceService("shop.test").Ready(), "new segment can acquire its own safe pause");
}

void R83BoundedHistory()
{
    R83Content content;
    auto sim = content.Sim(); R83Submit(sim, AdvanceTime{1});
    for (std::size_t i = 0; i < NpcMaxInterruptionWindows; ++i)
    {
        R83Request(sim); R83Submit(sim, AdvanceTime{1});
        R83Submit(sim, CommerceServiceClose{"shop.test"}); R83Submit(sim, AdvanceTime{2});
    }
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.service_history_full");
    R83Submit(sim, AdvanceTime{MinutesPerDay - sim.State().Clock.Now()});
    R83Ready(sim);
    R83Expect(sim.CommerceService("shop.test").Ready(), "old segment history is pruned without a permanent service lock");
}

void R83AllowancePaths()
{
    R83Content content;
    const auto before = R83At(CommerceWeekMinutes - 2);
    auto advance = content.Sim(before); R83Submit(advance, AdvanceTime{4});
    R83Expect(advance.State().Commerce.WalletPaise == 24000 && advance.State().Commerce.TransactionRevision == 1, "advance credits once");
    auto sleep = content.Sim(before); R83Submit(sleep, R83Day(DayCommandKind::GoToSleep));
    R83Submit(sleep, R83Day(DayCommandKind::Wake, 4));
    R83Expect(sleep.State().Commerce == advance.State().Commerce, "sleep settles same interval");
    auto activity = content.Sim(before); R83Submit(activity, R83Day(DayCommandKind::PlanDay));
    auto commit = R83Day(DayCommandKind::Commit); commit.Day.ActivityId = "activity.rest"; commit.Day.PlaceKind = "home";
    R83Submit(activity, commit); R83Submit(activity, R83Day(DayCommandKind::Start));
    R83Refused(activity, InventoryConsume{"item.tea"}, "commerce.player_busy");
    auto cancelled = content.Sim(activity.State());
    R83Submit(cancelled, R83Day(DayCommandKind::Cancel, 4));
    R83Expect(cancelled.State().Commerce == advance.State().Commerce, "partial activity cancellation settles once");
    R83Submit(activity, R83Day(DayCommandKind::Complete));
    R83Expect(activity.State().Commerce.TransactionRevision == 1 && activity.State().Commerce.WalletPaise == 24000,
        "activity completion settles once");
    auto passed = content.Sim(before); R83Submit(passed, R83Day(DayCommandKind::PlanDay));
    R83Submit(passed, R83Day(DayCommandKind::Pass, 4));
    R83Expect(passed.State().Commerce == advance.State().Commerce, "day pass settles once");
    auto capped = content.Sim(before, 1);
    for (int i = 0; i < 2; ++i)
        R83Expect(capped.Submit({CommandSource::System, ScheduleWorldEvent{CommerceWeekMinutes, "test/event/due"}}).Accepted, "event scheduled");
    R83Expect(R83Submit(capped, AdvanceTime{4}).Partial, "partial advance stops at boundary");
    R83Expect(capped.State().Commerce.TransactionRevision == 1, "partial path already credits boundary");
    R83Refused(capped, InventoryConsume{"item.tea"}, "commerce.world_unsettled");
    R83Submit(capped, SettleDueEvents{}); R83Submit(capped, AdvanceTime{2});
    R83Expect(capped.State().Commerce == advance.State().Commerce, "settling due events never duplicates allowance");
    auto interrupted = content.Sim(before, 1); R83Submit(interrupted, R83Day(DayCommandKind::GoToSleep));
    for (int i = 0; i < 2; ++i)
        R83Expect(interrupted.Submit({CommandSource::System, ScheduleWorldEvent{CommerceWeekMinutes, "test/event/due"}}).Accepted, "sleep event scheduled");
    R83Refused(interrupted, R83Day(DayCommandKind::Wake, 4), "day.charge_interrupted");
    WeekPlan plan; plan.Days.resize(1);
    WeekEntry spend; spend.Kind = WeekEntryKind::Spend; spend.StartMinute = 300; spend.Minutes = 120;
    plan.Days[0].Entries.push_back(spend);
    const auto start = R83At(CommerceWeekMinutes - 60);
    const auto compressed = CompressWeek(start, plan, content.DayCats());
    const auto stepped = PlayWeekStepByStep(start, plan, content.DayCats());
    R83Expect(compressed.Ok() && stepped.Ok(), "both week paths accepted");
    R83Expect(compressed.World.Commerce == stepped.World.Commerce && compressed.World.Commerce.TransactionRevision == 1 &&
        compressed.World.Commerce.AllowanceSettledThrough == CommerceWeekMinutes + 60, "compression and small steps settle identically");
    for (const auto kind : {WeekEntryKind::Sleep, WeekEntryKind::Activity})
    {
        WeekPlan mixed; mixed.Days.resize(1);
        WeekEntry entry; entry.Kind = kind; entry.StartMinute = 358; entry.Minutes = 4;
        if (kind == WeekEntryKind::Activity) { entry.ActivityId = "activity.rest"; entry.PlaceKind = "home"; }
        mixed.Days[0].Entries.push_back(entry);
        const auto bulk = CompressWeek(before, mixed, content.DayCats());
        const auto small = PlayWeekStepByStep(before, mixed, content.DayCats());
        R83Expect(bulk.Ok() && small.Ok() && bulk.World.Commerce == small.World.Commerce &&
            bulk.World.Commerce.TransactionRevision == 1 && bulk.World.Commerce.AllowanceSettledThrough ==
            before.Clock.Now() + (kind == WeekEntryKind::Sleep ? 4 : 10),
            "compressed sleep and activities settle the actual interval exactly once");
    }
}

void R83ConversationAndMidnight()
{
    R83Content content;
    const GameInstant now = CommerceWeekMinutes + 718;
    auto state = R83At(now);
    state.Commerce = SettleAllowance(InitialiseCommerce(0, 720).State, 720, now).State;
    auto paid = content.Sim(state);
    const std::string person(HouseholdAdultId);
    R83Submit(paid, ConversationOpen{person});
    R83Submit(paid, ConversationChoose{person, "choice.household_adult.talk_rota", 1});
    R83Expect(paid.State().Commerce.WalletPaise == 24000 && paid.State().Commerce.TransactionRevision == 1 &&
        paid.State().Commerce.AllowanceSettledThrough == now + 3, "paid conversation settles crossed allowance once");
    auto capped = content.Sim(state, 1);
    R83Submit(capped, ConversationOpen{person});
    for (int i = 0; i < 2; ++i)
        R83Expect(capped.Submit({CommandSource::System, ScheduleWorldEvent{now + 2, "test/event/due"}}).Accepted,
            "conversation event scheduled");
    R83Refused(capped, ConversationChoose{person, "choice.household_adult.talk_rota", 1}, "conversation.charge_interrupted");
    state = R83At(CommerceWeekMinutes + 1079);
    state.Commerce = SettleAllowance(InitialiseCommerce(0, 1080).State, 1080, state.Clock.Now()).State;
    auto midnight = content.Sim(state);
    const auto moved = R83Submit(midnight, AdvanceTime{2});
    R83Expect(midnight.State().Commerce.TransactionRevision == 1 && moved.Events.size() == 2 &&
        std::holds_alternative<DayStarted>(moved.Events.back().Payload), "midnight and allowance boundary both occur once");
    auto day = content.Sim();
    R83Refused(day, R83Day(DayCommandKind::ArriveSchool), "day.use_school_arrive");
}

void R83ConsumptionAndCreation()
{
    R83Content content;
    auto sim = content.Sim(); R83Ready(sim);
    R83Submit(sim, R83Buy(sim, "item.tea", 2)); R83Submit(sim, R83Buy(sim, "item.food", 2));
    auto state = sim.State(); state.DayLoop.Condition.Energy = MeterMax - 500; state.DayLoop.Condition.Hunger = 4000;
    sim = content.Sim(state);
    const auto before = sim.State().DayLoop.Condition;
    R83Submit(sim, InventoryConsume{"item.tea"});
    R83Expect(sim.State().DayLoop.Condition.Energy == MeterMax && sim.State().DayLoop.Condition.Health == before.Health &&
        sim.State().DayLoop.Condition.Stress == before.Stress, "tea clamps energy without unrelated effects");
    R83Refused(sim, InventoryConsume{"item.tea"}, "commerce.cooldown");
    R83Submit(sim, InventoryConsume{"item.food"}); R83Submit(sim, InventoryConsume{"item.food"});
    R83Expect(sim.State().DayLoop.Condition.Hunger == 0, "food clamps hunger and has no added cooldown");
    R83Submit(sim, AdvanceTime{59}); R83Refused(sim, InventoryConsume{"item.tea"}, "commerce.cooldown");
    R83Submit(sim, AdvanceTime{1}); R83Submit(sim, InventoryConsume{"item.tea"});
    for (int rank = 0; rank <= 4; ++rank)
    {
        CharacterCreator creation;
        R83Expect(creation.ClearAllocation().Succeeded() && creation.SetTraitRank(HouseholdResourcesTraitId, rank).Succeeded(), "creation rank set");
        WorldSim created(83, creation);
        R83Expect(created.State().Commerce == InitialiseCommerce(rank, created.State().Clock.Now()).State, "creation rank selects funding");
    }
}

void R83BindingAndCalendar()
{
    R83Content content;
    auto sim = content.Sim();
    content.Shops[0].Schedule = nullptr;
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.keeper_away");
    content.Shops[0].Schedule = &content.Keeper; content.Shops[0].Travel = nullptr;
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.keeper_away");
    content.Shops[0].Travel = &content.Travel; content.Shops[0].KeeperId = "person.other";
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.keeper_away");
    content.Shops[0].KeeperId = "person.mahua_saha"; content.Shops[0].KeeperAnchors = {};
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.keeper_away");
    content.Shops[0].KeeperAnchors = content.Anchors;
    content.Shops[0].WeekdayMask = ScheduleWeekdayBit(Weekday::Sunday);
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.closed");
    content.Shops[0].WeekdayMask = ScheduleAllWeekdays;
    const std::array<CommerceOpeningOverride, 1> closures{{{3, 6, false}}};
    content.Shops[0].Overrides = closures;
    R83Submit(sim, AdvanceTime{1}); R83Request(sim); R83Submit(sim, AdvanceTime{2});
    R83Refused(sim, R83Buy(sim), "commerce.closed"); // calendar wins at the exact safe point
    R83Submit(sim, CommerceServiceClose{"shop.test"}); R83Submit(sim, AdvanceTime{3}); R83Ready(sim);
    content.Entries[0].UnitPricePaise = 3000;
    R83Submit(sim, R83Buy(sim));
    R83Expect(sim.State().Commerce.RecentReceipts.back().UnitPricePaise == 3000,
        "confirmation recomputes price from the current catalogue binding");
    // A nonpilot busy role has no promised future safe pause. Do not retain an unfulfillable request.
    content.Keeper.PersonId = "person.other"; content.Shops[0].KeeperId = "person.other";
    content.Keeper.Routine[1].Doing = "inspecting";
    sim = content.Sim();
    R83Refused(sim, CommerceServiceRequest{"shop.test", "anchor.customer"}, "commerce.not_at_safe_pause");
    R83Expect(!sim.CommerceService("shop.test").Requested, "unfulfillable request keeps service closed");
}

void R83AdditionalAtomicRefusals()
{
    R83Content content;
    auto sim = content.Sim();
    R83Refused(sim, InventoryConsume{"missing"}, "commerce.invalid_item");
    auto state = MakeWorldState(83); state.Commerce.WalletPaise = -1;
    sim = content.Sim(state); R83Refused(sim, InventoryConsume{"item.tea"}, "commerce.invalid_state");
    R83Refused(sim, AdvanceTime{1}, "commerce.invalid_state");
    state = MakeWorldState(83); state.Commerce = InitialiseCommerce(0, 1).State;
    sim = content.Sim(state); R83Refused(sim, InventoryConsume{"item.tea"}, "commerce.invalid_time");
    R83Refused(sim, AdvanceTime{1}, "commerce.invalid_time");
    state = MakeWorldState(83); state.Commerce.TransactionRevision = std::numeric_limits<std::uint64_t>::max();
    state.Commerce.Inventory[0] = {"item.tea", 1};
    sim = content.Sim(state); R83Refused(sim, InventoryConsume{"item.tea"}, "commerce.arithmetic_overflow");
    // Settlement can fail after a sleep/activity has computed effects; neither slice may survive.
    state = R83At(CommerceWeekMinutes - 2);
    state.Commerce.TransactionRevision = std::numeric_limits<std::uint64_t>::max();
    sim = content.Sim(state); R83Submit(sim, R83Day(DayCommandKind::GoToSleep));
    R83Refused(sim, R83Day(DayCommandKind::Wake, 4), "commerce.arithmetic_overflow");
    sim = content.Sim(state); R83Submit(sim, R83Day(DayCommandKind::PlanDay));
    auto commit = R83Day(DayCommandKind::Commit); commit.Day.ActivityId = "activity.rest"; commit.Day.PlaceKind = "home";
    R83Submit(sim, commit); R83Submit(sim, R83Day(DayCommandKind::Start));
    R83Refused(sim, R83Day(DayCommandKind::Complete), "commerce.arithmetic_overflow");
    R83Refused(sim, R83Day(DayCommandKind::Cancel, 4), "commerce.arithmetic_overflow");
    WeekPlan plan; plan.Days.resize(1);
    WeekEntry entry; entry.Kind = WeekEntryKind::Spend; entry.StartMinute = 358; entry.Minutes = 4;
    plan.Days[0].Entries.push_back(entry);
    const auto compressed = CompressWeek(state, plan, content.DayCats());
    R83Expect(!compressed.Ok() && compressed.World.Commerce == state.Commerce &&
        compressed.World.DayLoop == state.DayLoop && HashWorldState(compressed.World) == HashWorldState(state),
        "compression rolls back its entire input on allowance overflow");
    const GameInstant now = CommerceWeekMinutes + 718;
    state = R83At(now); state.Commerce = SettleAllowance(InitialiseCommerce(0, 720).State, 720, now).State;
    state.Commerce.TransactionRevision = std::numeric_limits<std::uint64_t>::max(); sim = content.Sim(state);
    R83Submit(sim, ConversationOpen{std::string(HouseholdAdultId)});
    R83Refused(sim, ConversationChoose{std::string(HouseholdAdultId), "choice.household_adult.talk_rota", 1},
        "commerce.arithmetic_overflow");
    // A second tea ID shares the same persisted eligibility instant.
    content.Items.push_back({"item.tea_two", "Tea", "cup", 10, CommerceEffect::Tea, 0});
    state = MakeWorldState(83); state.Commerce.Inventory[0] = {"item.tea", 1};
    state.Commerce.Inventory[1] = {"item.tea_two", 1}; sim = content.Sim(state);
    R83Submit(sim, InventoryConsume{"item.tea"});
    R83Refused(sim, InventoryConsume{"item.tea_two"}, "commerce.cooldown");
    sim = content.Sim(sim.State());
    R83Refused(sim, InventoryConsume{"item.tea_two"}, "commerce.cooldown");
}
} // namespace

int main()
{
    R83ServiceAndReplay(); R83Refusals(); R83CapacityAndOverflow(); R83ClosingAndHistory();
    R83BoundedHistory(); R83AllowancePaths(); R83ConversationAndMidnight(); R83ConsumptionAndCreation();
    R83BindingAndCalendar(); R83AdditionalAtomicRefusals();
    std::cout << R83Assertions << " world commerce assertions passed\n";
}
