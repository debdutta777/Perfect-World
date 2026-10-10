#include "Activities.h"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <utility>

namespace PerfectWorld
{
namespace
{
template <typename T, std::size_t N>
bool ActivityLookup(const std::array<std::pair<std::string_view, T>, N>& table, const JsonValue* value, T& out)
{
    if (value == nullptr || !value->IsString())
        return false;
    for (const auto& [name, mapped] : table)
    {
        if (name == value->String)
        {
            out = mapped;
            return true;
        }
    }
    return false;
}

constexpr std::array<std::pair<std::string_view, ActivityClockMode>, 4> ActivityClockNames{{
    {"free_roam", ActivityClockMode::FreeRoam}, {"scene", ActivityClockMode::Scene},
    {"mission", ActivityClockMode::Mission}, {"compressed", ActivityClockMode::Compressed}}};

constexpr std::array<std::pair<std::string_view, ActivityInterrupt>, 3> ActivityInterruptNames{{
    {"none", ActivityInterrupt::None}, {"at_beat", ActivityInterrupt::AtBeat}, {"immediate", ActivityInterrupt::Immediate}}};

constexpr std::array<std::pair<std::string_view, ConditionActivity>, 5> ActivityConditionNames{{
    {"awake", ConditionActivity::Awake}, {"resting", ConditionActivity::Resting}, {"sleeping", ConditionActivity::Sleeping},
    {"light_exertion", ConditionActivity::LightExertion}, {"heavy_exertion", ConditionActivity::HeavyExertion}}};

constexpr std::array<std::pair<std::string_view, MealSize>, 4> ActivityMealNames{{
    {"snack", MealSize::Snack}, {"light", MealSize::Light}, {"full", MealSize::Full}, {"feast", MealSize::Feast}}};

std::optional<std::int32_t> ActivityInt32(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    if (value == nullptr || !value->Integer || *value->Integer < std::numeric_limits<std::int32_t>::min() ||
        *value->Integer > std::numeric_limits<std::int32_t>::max())
        return std::nullopt;
    return static_cast<std::int32_t>(*value->Integer);
}

std::string ActivityText(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsString() ? value->String : std::string();
}

std::optional<Attribute> ActivityAttributeOf(std::string_view contentId)
{
    for (std::size_t i = 0; i < AttributeCount; ++i)
    {
        if (AttributeContentId(static_cast<Attribute>(i)) == contentId)
            return static_cast<Attribute>(i);
    }
    return std::nullopt;
}

std::string ActivityParseLearning(const ContentDb& db, const JsonValue& learning, ActivityDef& def)
{
    const std::optional<std::int32_t> allowance = ActivityInt32(&learning, "allowanceMilliXp");
    const JsonValue* split = learning.Find("split");
    if (!allowance || *allowance < 1 || split == nullptr || !split->IsArray() || split->Array.empty())
        return "learning needs a positive allowanceMilliXp and a non-empty split";
    std::int64_t total = 0;
    for (const JsonValue& entry : split->Array)
    {
        const std::string skill = ActivityText(&entry, "skill");
        const std::optional<std::int32_t> permille = ActivityInt32(&entry, "permille");
        const ContentRecord* record = db.Find(skill);
        if (!permille || *permille < 1 || record == nullptr || record->Type != "skill")
            return "a learning share needs an existing skill and a positive permille";
        if (std::any_of(def.Split.begin(), def.Split.end(), [&](const ActivityShare& s) { return s.SkillId == skill; }))
            return "the learning split names " + skill + " twice";
        total += *permille;
        def.Split.push_back({skill, *permille});
    }
    if (total != ActivitySplitWhole)
        return "the learning split sums to " + std::to_string(total) + ", not " + std::to_string(ActivitySplitWhole);
    def.AllowanceMilliXp = *allowance;
    return {};
}

std::string ActivityParseEvidence(const JsonValue& evidence, ActivityDef& def)
{
    if (!evidence.IsArray())
        return "evidence must be an array";
    std::int64_t total = 0;
    for (const JsonValue& entry : evidence.Array)
    {
        const std::optional<Attribute> attribute = ActivityAttributeOf(ActivityText(&entry, "attribute"));
        const std::optional<std::int32_t> milli = ActivityInt32(&entry, "milli");
        if (!attribute || !milli || *milli < 1)
            return "an evidence entry needs one of the six attributes and a positive milli";
        if (std::any_of(def.Evidence.begin(), def.Evidence.end(), [&](const ActivityEvidenceDef& e) { return e.Attr == *attribute; }))
            return "evidence names an attribute twice";
        total += *milli;
        def.Evidence.push_back({*attribute, *milli});
    }
    if (total > def.AllowanceMilliXp / ActivityEvidenceDivisor)
        return "attribute evidence is above one twentieth of the learning allowance";
    return {};
}

std::string ActivityParse(const ContentDb& db, const ContentRecord& record, ActivityDef& def)
{
    const JsonValue& data = record.Data;
    def.Id = record.Id;
    def.Name = ActivityText(&data, "name");
    def.Place = ActivityText(&data, "place");
    def.Outcome = ActivityText(&data, "outcome");
    const std::optional<std::int32_t> duration = ActivityInt32(&data, "durationMinutes");
    if (def.Name.empty() || def.Place.empty() || def.Outcome.empty() || !duration || *duration < 1 ||
        *duration > MinutesPerDay)
        return "needs a name, place, outcome and a duration of 1 to 1440 minutes";
    def.DurationMinutes = *duration;

    const JsonValue* clock = data.Find("clock");
    if (!ActivityLookup(ActivityClockNames, clock != nullptr ? clock->Find("mode") : nullptr, def.Clock) ||
        !ActivityLookup(ActivityInterruptNames, clock != nullptr ? clock->Find("interrupt") : nullptr, def.Interrupt))
        return "needs a clock with a known mode and interrupt";

    const JsonValue* condition = data.Find("condition");
    if (!ActivityLookup(ActivityConditionNames, condition != nullptr ? condition->Find("activity") : nullptr, def.Condition))
        return "needs a condition with a known activity";
    const JsonValue* meal = condition->Find("meal");
    if (meal != nullptr)
    {
        if (!ActivityLookup(ActivityMealNames, meal, def.Meal))
            return "has an unknown meal size";
        def.HasMeal = true;
    }
    if (condition->Find("stressDelta") != nullptr)
    {
        const std::optional<std::int32_t> delta = ActivityInt32(condition, "stressDelta");
        if (!delta || *delta < -MeterMax || *delta > MeterMax)
            return "has a stressDelta outside the meter range";
        def.StressDelta = *delta;
    }

    if (const JsonValue* learning = data.Find("learning"); learning != nullptr)
    {
        if (std::string error = ActivityParseLearning(db, *learning, def); !error.empty())
            return error;
    }
    if (const JsonValue* evidence = data.Find("evidence"); evidence != nullptr && !(evidence->IsArray() && evidence->Array.empty()))
    {
        if (def.AllowanceMilliXp == 0)
            return "attribute evidence needs a learning allowance";
        if (std::string error = ActivityParseEvidence(*evidence, def); !error.empty())
            return error;
    }
    return {};
}

bool ActivityEndsAfter(const ActivityRunnerState& state, std::int32_t duration, GameInstant now)
{
    return state.StartedAt <= std::numeric_limits<GameInstant>::max() - duration && now >= state.StartedAt + duration;
}

ActivityResult ActivityRefuse(ActivityError error)
{
    ActivityResult result;
    result.Error = error;
    return result;
}

void ActivityClear(ActivityRunnerState& state)
{
    state.AppliedInstance = state.Instance;
    state.ActivityId.clear();
    state.StartedAt = 0;
}
} // namespace

const ActivityDef* ActivityCatalogue::Find(std::string_view id) const noexcept
{
    const auto found = std::lower_bound(Defs.begin(), Defs.end(), id,
                                        [](const ActivityDef& def, std::string_view key) { return def.Id < key; });
    return found != Defs.end() && found->Id == id ? &*found : nullptr;
}

ActivityCatalogueResult LoadActivityCatalogue(const ContentDb& content)
{
    ActivityCatalogueResult result;
    // OfType walks an ordered map, so the definitions come out sorted by ID.
    for (const ContentRecord* record : content.OfType("activity"))
    {
        ActivityDef def;
        if (std::string error = ActivityParse(content, *record, def); !error.empty())
        {
            result.Error = record->Id + ": " + error;
            result.Catalogue = {};
            return result;
        }
        result.Catalogue.Defs.push_back(std::move(def));
    }
    return result;
}

std::vector<std::int32_t> SplitAllowance(std::int64_t allowance, const std::vector<std::int32_t>& permille)
{
    std::int64_t total = 0;
    for (const std::int32_t part : permille)
    {
        if (part < 0)
            return {};
        total += part;
    }
    if (allowance < 0 || allowance > std::numeric_limits<std::int32_t>::max() || total != ActivitySplitWhole)
        return {};

    std::vector<std::int32_t> shares(permille.size());
    std::vector<std::int32_t> remainders(permille.size());
    std::int64_t given = 0;
    for (std::size_t i = 0; i < permille.size(); ++i)
    {
        const std::int64_t exact = allowance * permille[i];
        shares[i] = static_cast<std::int32_t>(exact / ActivitySplitWhole);
        remainders[i] = static_cast<std::int32_t>(exact % ActivitySplitWhole);
        given += shares[i];
    }
    // The leftover is less than the number of shares: one milli each to the largest remainders.
    for (std::int64_t leftover = allowance - given; leftover > 0; --leftover)
    {
        std::size_t best = 0;
        for (std::size_t i = 1; i < remainders.size(); ++i)
        {
            if (remainders[i] > remainders[best])
                best = i;
        }
        ++shares[best];
        remainders[best] = -1;
    }
    return shares;
}

std::vector<SkillCredit> ActivityLearningCredits(const ActivityDef& def)
{
    std::vector<std::int32_t> permille;
    for (const ActivityShare& share : def.Split)
        permille.push_back(share.Permille);
    const std::vector<std::int32_t> shares = SplitAllowance(def.AllowanceMilliXp, permille);
    std::vector<SkillCredit> credits;
    for (std::size_t i = 0; i < shares.size(); ++i)
    {
        if (shares[i] > 0)
            credits.push_back({def.Split[i].SkillId, shares[i]});
    }
    return credits;
}

std::string_view ActivityErrorCode(ActivityError error) noexcept
{
    switch (error)
    {
    case ActivityError::None:
        return {};
    case ActivityError::UnknownActivity:
        return "activity.unknown";
    case ActivityError::Busy:
        return "activity.busy";
    case ActivityError::NotActive:
        return "activity.not_active";
    case ActivityError::NotFinished:
        return "activity.not_finished";
    case ActivityError::Finished:
        return "activity.finished";
    case ActivityError::WrongPlace:
        return "activity.wrong_place";
    case ActivityError::InvalidTime:
        return "activity.invalid_time";
    case ActivityError::InvalidCondition:
        return "activity.invalid_condition";
    case ActivityError::InvalidState:
        return "activity.invalid_state";
    }
    return {};
}

ActivityResult StartActivity(ActivityRunnerState& state, const ActivityCatalogue& catalogue, std::string_view activityId,
                             GameInstant now, std::string_view placeKind)
{
    if (!IsActivityRunnerStateValid(state) || state.Instance == std::numeric_limits<std::uint64_t>::max())
        return ActivityRefuse(ActivityError::InvalidState);
    if (state.Active())
        return ActivityRefuse(ActivityError::Busy);
    const ActivityDef* def = catalogue.Find(activityId);
    if (def == nullptr)
        return ActivityRefuse(ActivityError::UnknownActivity);
    if (now < 0 || now > std::numeric_limits<GameInstant>::max() - def->DurationMinutes)
        return ActivityRefuse(ActivityError::InvalidTime);
    if (def->Place != ActivityAnyPlace && def->Place != placeKind)
        return ActivityRefuse(ActivityError::WrongPlace);

    ++state.Instance;
    state.ActivityId = def->Id;
    state.StartedAt = now;

    ActivityResult result;
    ActivityEvent event;
    event.Kind = ActivityEventKind::Started;
    event.Instance = state.Instance;
    event.ActivityId = def->Id;
    event.At = now;
    event.Minutes = def->DurationMinutes;
    result.Events.push_back(std::move(event));
    return result;
}

ActivityResult CompleteActivity(ActivityRunnerState& state, const ActivityCatalogue& catalogue, ConditionState& condition,
                                GameInstant now)
{
    if (!IsActivityRunnerStateValid(state))
        return ActivityRefuse(ActivityError::InvalidState);
    if (!state.Active())
        return ActivityRefuse(ActivityError::NotActive);
    const ActivityDef* def = catalogue.Find(state.ActivityId);
    if (def == nullptr)
        return ActivityRefuse(ActivityError::UnknownActivity);
    if (!ActivityEndsAfter(state, def->DurationMinutes, now))
        return ActivityRefuse(ActivityError::NotFinished);

    // Everything on a copy first, so a refusal leaves the condition untouched.
    ConditionState next = condition;
    bool applied = AdvanceCondition(next, def->Condition, def->DurationMinutes);
    if (applied && def->HasMeal)
        applied = ApplyMeal(next, def->Meal);
    if (applied && def->StressDelta != 0)
        applied = AdjustStress(next, def->StressDelta);
    if (!applied)
        return ActivityRefuse(ActivityError::InvalidCondition);
    condition = next;

    ActivityResult result;
    ActivityEvent event;
    event.Kind = ActivityEventKind::Completed;
    event.Instance = state.Instance;
    event.ActivityId = def->Id;
    event.At = now;
    event.Minutes = def->DurationMinutes;
    event.Outcome = def->Outcome;
    event.Learning = ActivityLearningCredits(*def);
    for (const ActivityEvidenceDef& evidence : def->Evidence)
        event.Evidence.push_back({evidence.Attr, evidence.Milli});
    result.Events.push_back(std::move(event));
    ActivityClear(state);
    return result;
}

ActivityResult CancelActivity(ActivityRunnerState& state, const ActivityCatalogue& catalogue, ConditionState& condition,
                              GameInstant now)
{
    if (!IsActivityRunnerStateValid(state))
        return ActivityRefuse(ActivityError::InvalidState);
    if (!state.Active())
        return ActivityRefuse(ActivityError::NotActive);
    const ActivityDef* def = catalogue.Find(state.ActivityId);
    if (def == nullptr)
        return ActivityRefuse(ActivityError::UnknownActivity);
    if (now < state.StartedAt)
        return ActivityRefuse(ActivityError::InvalidTime);
    if (ActivityEndsAfter(state, def->DurationMinutes, now))
        return ActivityRefuse(ActivityError::Finished);

    const std::int64_t elapsed = now - state.StartedAt; // less than the duration
    ConditionState next = condition;
    if (!AdvanceCondition(next, def->Condition, elapsed))
        return ActivityRefuse(ActivityError::InvalidCondition);
    condition = next;

    ActivityResult result;
    ActivityEvent event;
    event.Kind = ActivityEventKind::Cancelled;
    event.Instance = state.Instance;
    event.ActivityId = def->Id;
    event.At = now;
    event.Minutes = elapsed;
    result.Events.push_back(std::move(event));
    ActivityClear(state);
    return result;
}
} // namespace PerfectWorld
