#include "Threads.h"

#include "ThreadsInternal.h"

#include <algorithm>
#include <limits>

// Reading the thread_template record, loading the set and the lookups on a template (ROADMAP M3-06).
// The rules a template must meet are in ThreadTemplateRules.cpp.
namespace PerfectWorld
{
using namespace ThreadInternal;

namespace
{
using ThrProblems = std::vector<std::string>;

std::string ThrText(const JsonValue& object, std::string_view name)
{
    const JsonValue* value = object.Find(name);
    return value != nullptr && value->IsString() ? value->String : std::string();
}

std::int32_t ThrInt(const JsonValue& object, std::string_view name, std::int32_t fallback)
{
    const JsonValue* value = object.Find(name);
    if (value == nullptr || !value->Integer)
        return fallback;
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(*value->Integer, std::numeric_limits<std::int32_t>::min(),
                                                              std::numeric_limits<std::int32_t>::max()));
}

template <typename E, std::size_t N>
E ThrEnum(const JsonValue& object, std::string_view field, const std::array<std::string_view, N>& names, const std::string& label,
          ThrProblems& problems)
{
    const std::string text = ThrText(object, field);
    if (const std::optional<E> value = ParseName<E>(names, text))
        return *value;
    problems.push_back(label + " " + std::string(field) + " '" + text + "' is not a known value");
    return E{};
}

const std::vector<JsonValue>& ThrList(const JsonValue& object, std::string_view name)
{
    static const std::vector<JsonValue> none;
    const JsonValue* value = object.Find(name);
    return value != nullptr && value->IsArray() ? value->Array : none;
}

ThreadExpression ThrReadExpression(const JsonValue* object, const std::string& label, ThrProblems& problems)
{
    ThreadExpression expression;
    if (object == nullptr || !object->IsObject())
    {
        problems.push_back(label + " has no expression");
        return expression;
    }
    expression.Channel = ThrEnum<ThreadChannel>(*object, "channel", ChannelNames, label + " expression", problems);
    expression.By = ThrText(*object, "by");
    expression.Scene = ThrText(*object, "scene");
    expression.Observe = ThrText(*object, "observe");
    return expression;
}

bool ThrNeeds(const JsonValue& object, std::string_view field, const std::string& label, ThrProblems& problems)
{
    if (object.Find(field) != nullptr)
        return true;
    problems.push_back(label + " needs " + std::string(field));
    return false;
}

// The fields a condition of each kind takes besides "kind".
std::vector<std::string_view> ThrAllowedFields(ThreadConditionKind kind)
{
    switch (kind)
    {
    case ThreadConditionKind::DaysOpen:
    case ThreadConditionKind::DaysInStage:
        return {"days"};
    case ThreadConditionKind::Calendar:
        return {"span", "phase"};
    case ThreadConditionKind::Relationship:
        return {"a", "b", "dimension", "op", "value"};
    case ThreadConditionKind::Event:
        return {"event", "parties", "count", "window"};
    }
    return {};
}

ThreadCondition ThrReadCondition(const JsonValue& object, const std::string& label, ThrProblems& problems)
{
    ThreadCondition condition;
    condition.Kind = ThrEnum<ThreadConditionKind>(object, "kind", ConditionNames, label, problems);
    const std::vector<std::string_view> allowed = ThrAllowedFields(condition.Kind);
    for (const auto& [key, value] : object.Object)
    {
        (void)value;
        if (key != "kind" && std::find(allowed.begin(), allowed.end(), key) == allowed.end())
            problems.push_back(label + " (" + std::string(ConditionNames[static_cast<std::size_t>(condition.Kind)]) + ") does not take " + key);
    }
    switch (condition.Kind)
    {
    case ThreadConditionKind::DaysOpen:
    case ThreadConditionKind::DaysInStage:
        if (ThrNeeds(object, "days", label, problems))
            condition.Days = ThrInt(object, "days", 0);
        break;
    case ThreadConditionKind::Calendar:
        condition.Span = ThrText(object, "span");
        if (ThrNeeds(object, "span", label, problems) && ThrNeeds(object, "phase", label, problems))
            condition.When = ThrEnum<ThreadCalendarWhen>(object, "phase", WhenNames, label, problems);
        break;
    case ThreadConditionKind::Relationship:
        condition.RoleA = ThrText(object, "a");
        condition.RoleB = ThrText(object, "b");
        for (const std::string_view field : {"a", "b", "dimension", "op", "value"})
            ThrNeeds(object, field, label, problems);
        condition.Dimension = ThrEnum<ThreadDimension>(object, "dimension", DimensionNames, label, problems);
        condition.Compare = ThrEnum<ThreadCompare>(object, "op", CompareNames, label, problems);
        condition.Value = ThrInt(object, "value", 0);
        break;
    case ThreadConditionKind::Event:
        condition.EventType = ThrText(object, "event");
        for (const JsonValue& party : ThrList(object, "parties"))
            condition.Parties.push_back(party.IsString() ? party.String : std::string());
        ThrNeeds(object, "event", label, problems);
        ThrNeeds(object, "parties", label, problems);
        condition.Count = ThrInt(object, "count", 1);
        if (object.Find("window") != nullptr)
            condition.Window = ThrEnum<ThreadWindow>(object, "window", WindowNames, label, problems);
        break;
    }
    return condition;
}

std::int32_t ThrStageRef(const ThreadTemplate& templ, const JsonValue& object, std::string_view field, std::int32_t fallback,
                         const std::string& label, ThrProblems& problems)
{
    if (object.Find(field) == nullptr)
        return fallback;
    const std::string id = ThrText(object, field);
    const std::int32_t index = ThreadStageIndex(templ, id);
    if (index < 0)
        problems.push_back(label + " " + std::string(field) + " '" + id + "' is not a stage of the template");
    return index < 0 ? fallback : index;
}

ThreadTrigger ThrReadTrigger(const ThreadTemplate& templ, const JsonValue& object, ThrProblems& problems)
{
    ThreadTrigger trigger;
    trigger.Id = ThrText(object, "id");
    const std::string label = "trigger " + trigger.Id;
    const std::int32_t last = templ.Stages.empty() ? 0 : static_cast<std::int32_t>(templ.Stages.size()) - 1;
    trigger.ArmedFrom = ThrStageRef(templ, object, "armedFrom", 0, label, problems);
    trigger.ArmedUntil = ThrStageRef(templ, object, "armedUntil", last, label, problems);
    trigger.Effect = ThrEnum<ThreadEffectKind>(object, "effect", EffectNames, label, problems);
    trigger.Resolution = ThrText(object, "resolution");
    if (const JsonValue* expression = object.Find("expression"))
        trigger.Expression = ThrReadExpression(expression, label, problems);
    std::size_t number = 0;
    for (const JsonValue& entry : ThrList(object, "when"))
        trigger.Conditions.push_back(ThrReadCondition(entry, label + " condition " + std::to_string(++number), problems));
    return trigger;
}
} // namespace

// ---- Lookups ---------------------------------------------------------------------

const ThreadTemplate* ThreadTemplateSet::Find(std::string_view id) const noexcept
{
    const auto it = std::lower_bound(Templates.begin(), Templates.end(), id,
                                     [](const ThreadTemplate& templ, std::string_view key) { return templ.Id < key; });
    return it != Templates.end() && it->Id == id ? &*it : nullptr;
}

std::int32_t ThreadRoleIndex(const ThreadTemplate& templ, std::string_view role)
{
    for (std::size_t i = 0; i < templ.Roles.size(); ++i)
    {
        if (templ.Roles[i].Id == role)
            return static_cast<std::int32_t>(i);
    }
    return -1;
}

std::int32_t ThreadStageIndex(const ThreadTemplate& templ, std::string_view stage)
{
    for (std::size_t i = 0; i < templ.Stages.size(); ++i)
    {
        if (templ.Stages[i].Id == stage)
            return static_cast<std::int32_t>(i);
    }
    return -1;
}

const ThreadResolution* ThreadFindResolution(const ThreadTemplate& templ, std::string_view id)
{
    for (const ThreadResolution& resolution : templ.Resolutions)
    {
        if (resolution.Id == id)
            return &resolution;
    }
    return nullptr;
}

std::size_t ThreadEventSlotCount(const ThreadTemplate& templ)
{
    return FirstEventSlot(templ, templ.Triggers.size());
}

std::string_view ThreadChannelName(ThreadChannel channel)
{
    const std::size_t index = static_cast<std::size_t>(channel);
    return index < ChannelNames.size() ? ChannelNames[index] : std::string_view();
}

std::string_view ThreadOutcomeName(ThreadOutcome outcome)
{
    const std::size_t index = static_cast<std::size_t>(outcome);
    return index < OutcomeNames.size() ? OutcomeNames[index] : std::string_view();
}

// ---- Reading ---------------------------------------------------------------------

ThreadTemplate ParseThreadTemplateRecord(const ContentRecord& record, std::vector<std::string>& problems)
{
    const JsonValue& data = record.Data;
    ThreadTemplate templ;
    templ.Id = record.Id;
    templ.Name = ThrText(data, "name");
    const std::string label = "the template";
    templ.Situation = ThrEnum<ThreadSituation>(data, "situation", SituationNames, label, problems);
    templ.Kind = ThrEnum<ThreadKind>(data, "kind", KindNames, label, problems);
    templ.OpensWhen = ThrText(data, "opensWhen");
    templ.CalendarId = ThrText(data, "calendar");
    templ.DefaultResolution = ThrText(data, "defaultResolution");
    for (const JsonValue& role : ThrList(data, "roles"))
        templ.Roles.push_back({ThrText(role, "id"), ThrText(role, "name")});
    for (const JsonValue& stage : ThrList(data, "stages"))
    {
        ThreadStage out;
        out.Id = ThrText(stage, "id");
        out.Name = ThrText(stage, "name");
        out.Intensity = ThrInt(stage, "intensity", 0);
        out.DwellDays = ThrInt(stage, "dwellDays", 0);
        out.Expression = ThrReadExpression(stage.Find("expression"), "stage " + out.Id, problems);
        templ.Stages.push_back(std::move(out));
    }
    for (const JsonValue& resolution : ThrList(data, "resolutions"))
    {
        ThreadResolution out;
        out.Id = ThrText(resolution, "id");
        out.Name = ThrText(resolution, "name");
        out.Outcome = ThrEnum<ThreadOutcome>(resolution, "outcome", OutcomeNames, "resolution " + out.Id, problems);
        out.Expression = ThrReadExpression(resolution.Find("expression"), "resolution " + out.Id, problems);
        templ.Resolutions.push_back(std::move(out));
    }
    for (const JsonValue& trigger : ThrList(data, "triggers"))
        templ.Triggers.push_back(ThrReadTrigger(templ, trigger, problems));
    return templ;
}

std::vector<std::string> ValidateThreadTemplate(const ThreadTemplate& templ, const SchoolCalendar* calendar)
{
    std::vector<std::string> problems;
    ValidateThreadShape(templ, problems);
    ValidateThreadTriggers(templ, calendar, problems);
    return problems;
}

namespace ThreadInternal
{
std::vector<std::string> ReadAndValidateThread(const ContentDb& content, const ContentRecord& record, ThreadTemplate& out)
{
    std::vector<std::string> problems;
    out = ParseThreadTemplateRecord(record, problems);
    if (!problems.empty())
        return problems;
    SchoolCalendar calendar;
    const SchoolCalendar* calendarPointer = nullptr;
    if (!out.CalendarId.empty())
    {
        const ContentRecord* calendarRecord = content.Find(out.CalendarId);
        if (calendarRecord != nullptr && calendarRecord->Type == "calendar")
        {
            std::vector<std::string> calendarProblems;
            calendar = ParseSchoolCalendarRecord(*calendarRecord, calendarProblems);
            if (calendarProblems.empty())
                calendarPointer = &calendar;
        }
    }
    return ValidateThreadTemplate(out, calendarPointer);
}
} // namespace ThreadInternal

ThreadTemplateResult LoadThreadTemplates(const ContentDb& content)
{
    ThreadTemplateResult result;
    for (const ContentRecord* record : content.OfType("thread_template"))
    {
        ThreadTemplate templ;
        const std::vector<std::string> problems = ReadAndValidateThread(content, *record, templ);
        if (!problems.empty())
        {
            result.Error = record->Id + ": " + problems.front();
            result.Set = {};
            return result;
        }
        result.Set.Templates.push_back(std::move(templ));
    }
    std::sort(result.Set.Templates.begin(), result.Set.Templates.end(),
              [](const ThreadTemplate& a, const ThreadTemplate& b) { return a.Id < b.Id; });
    return result;
}
} // namespace PerfectWorld
