#include "Threads.h"

#include "ThreadsInternal.h"

#include <algorithm>
#include <set>

// The rules a thread template must meet (ROADMAP M3-06). Reading the record is in ThreadTemplates.cpp.
// Every message names the thing that is wrong, and each rule has its own wording so a fixture can
// demonstrate it alone.
namespace PerfectWorld::ThreadInternal
{
namespace
{
using Problems = std::vector<std::string>;

constexpr std::array<std::string_view, 12> ThrForbiddenWords{"religion", "religions", "religious", "faith", "caste", "language",
                                                             "languages", "tongue", "stream", "streams", "gender", "background"};
constexpr std::int32_t ThrMaxMilli = 100000;

std::string ThrRange(std::int64_t low, std::int64_t high)
{
    return std::to_string(low) + ".." + std::to_string(high);
}

void ThrRuleExpression(const ThreadTemplate& templ, const ThreadExpression& expression, const std::string& label, Problems& problems)
{
    if (ThreadRoleIndex(templ, expression.By) < 0)
        problems.push_back(label + " expression is by '" + expression.By + "', which is not a role of the template");
    if (expression.Observe.empty())
        problems.push_back(label + " expression has no observable text");
    else if (expression.Observe.size() > ThreadMaxObserveLength)
        problems.push_back(label + " expression text is over " + std::to_string(ThreadMaxObserveLength) + " characters");
    if (expression.Channel == ThreadChannel::Scene && expression.Scene.empty())
        problems.push_back(label + " is a scene but names no scene token");
    if (!expression.Scene.empty() && !IsThreadId(expression.Scene))
        problems.push_back(label + " names the scene '" + expression.Scene + "', which is not a lowercase token");
}

void ThrRuleRoles(const ThreadTemplate& templ, Problems& problems)
{
    if (templ.Roles.size() < ThreadMinRoles || templ.Roles.size() > ThreadMaxRoles)
        problems.push_back("needs " + std::to_string(ThreadMinRoles) + " to " + std::to_string(ThreadMaxRoles) + " roles, found " +
                           std::to_string(templ.Roles.size()));
    std::set<std::string> seen;
    for (const ThreadRole& role : templ.Roles)
    {
        if (!IsThreadId(role.Id))
            problems.push_back("role id '" + role.Id + "' must be lowercase letters, digits and underscores");
        if (!seen.insert(role.Id).second)
            problems.push_back("two roles are both called " + role.Id);
        if (const std::string_view word = ForbiddenWordIn(role.Id); !word.empty())
            problems.push_back("role " + role.Id + " is keyed on '" + std::string(word) + "', which no thread may depend on");
        if (role.Name.empty())
            problems.push_back("role " + role.Id + " has no name");
    }
}

void ThrRuleStages(const ThreadTemplate& templ, Problems& problems)
{
    if (templ.Stages.size() < ThreadMinStages || templ.Stages.size() > ThreadMaxStages)
        problems.push_back("needs " + std::to_string(ThreadMinStages) + " to " + std::to_string(ThreadMaxStages) + " stages, found " +
                           std::to_string(templ.Stages.size()));
    std::set<std::string> seen;
    std::int64_t total = 0;
    std::int32_t previous = 0;
    for (const ThreadStage& stage : templ.Stages)
    {
        const std::string label = "stage " + stage.Id;
        if (!IsThreadId(stage.Id))
            problems.push_back("stage id '" + stage.Id + "' must be lowercase letters, digits and underscores");
        if (!seen.insert(stage.Id).second)
            problems.push_back("two stages are both called " + stage.Id);
        if (stage.Intensity < 1 || stage.Intensity > ThreadMaxIntensity)
            problems.push_back(label + " intensity " + std::to_string(stage.Intensity) + " is outside " + ThrRange(1, ThreadMaxIntensity));
        else if (stage.Intensity <= previous)
            problems.push_back(label + " intensity " + std::to_string(stage.Intensity) + " does not rise above " + std::to_string(previous));
        previous = std::max(previous, stage.Intensity);
        if (stage.DwellDays < 1 || stage.DwellDays > ThreadMaxDwellDays)
            problems.push_back(label + " dwell " + std::to_string(stage.DwellDays) + " days is outside " + ThrRange(1, ThreadMaxDwellDays));
        total += stage.DwellDays;
        ThrRuleExpression(templ, stage.Expression, label, problems);
    }
    if (!templ.Stages.empty() && templ.Stages.back().Intensity != ThreadMaxIntensity)
        problems.push_back("the last stage must reach intensity " + std::to_string(ThreadMaxIntensity) + ", found " +
                           std::to_string(templ.Stages.back().Intensity));
    if (total > ThreadMaxClockDays)
        problems.push_back("the clock runs " + std::to_string(total) + " days in total, over the limit of " + std::to_string(ThreadMaxClockDays));
    if (!templ.Stages.empty() && templ.Stages.front().Expression.Channel == ThreadChannel::Scene)
        problems.push_back("the first stage must not be a scene: a thread begins quietly");
}

bool ThrResolvedBy(const ThreadTemplate& templ, std::string_view resolution)
{
    return std::any_of(templ.Triggers.begin(), templ.Triggers.end(), [&](const ThreadTrigger& trigger) {
        return trigger.Effect == ThreadEffectKind::Resolve && trigger.Resolution == resolution;
    });
}

void ThrRuleResolutions(const ThreadTemplate& templ, Problems& problems)
{
    if (templ.Resolutions.size() < ThreadMinResolutions || templ.Resolutions.size() > ThreadMaxResolutions)
        problems.push_back("needs " + std::to_string(ThreadMinResolutions) + " to " + std::to_string(ThreadMaxResolutions) +
                           " resolutions, found " + std::to_string(templ.Resolutions.size()));
    std::set<std::string> seen;
    for (const ThreadResolution& resolution : templ.Resolutions)
    {
        if (!IsThreadId(resolution.Id))
            problems.push_back("resolution id '" + resolution.Id + "' must be lowercase letters, digits and underscores");
        if (!seen.insert(resolution.Id).second)
            problems.push_back("two resolutions are both called " + resolution.Id);
        ThrRuleExpression(templ, resolution.Expression, "resolution " + resolution.Id, problems);
        if (resolution.Id != templ.DefaultResolution && !ThrResolvedBy(templ, resolution.Id))
            problems.push_back("resolution " + resolution.Id + " is never reached: no trigger resolves it and it is not the default");
    }
    const ThreadResolution* fallback = ThreadFindResolution(templ, templ.DefaultResolution);
    if (fallback == nullptr)
        problems.push_back("defaultResolution '" + templ.DefaultResolution + "' is not one of the resolutions");
    else if (fallback->Outcome == ThreadOutcome::Repaired)
        problems.push_back("the default resolution " + fallback->Id + " is repaired: an untouched thread does not mend itself");
}

// ---- Triggers --------------------------------------------------------------------

void ThrUseRole(const ThreadTemplate& templ, const std::string& role, const std::string& label, std::set<std::string>& used, Problems& problems)
{
    if (ThreadRoleIndex(templ, role) < 0)
        problems.push_back(label + " names the role '" + role + "', which is not a role of the template");
    used.insert(role);
}

void ThrRuleCondition(const ThreadTemplate& templ, const SchoolCalendar* calendar, const ThreadCondition& condition, const std::string& label,
                      std::set<std::string>& used, Problems& problems)
{
    switch (condition.Kind)
    {
    case ThreadConditionKind::DaysOpen:
    case ThreadConditionKind::DaysInStage:
        if (condition.Days < 1 || condition.Days > ThreadMaxConditionDays)
            problems.push_back(label + " days " + std::to_string(condition.Days) + " is outside " + ThrRange(1, ThreadMaxConditionDays));
        break;
    case ThreadConditionKind::Calendar:
        if (templ.CalendarId.empty())
            problems.push_back(label + " is a calendar condition but the template names no calendar");
        else if (calendar == nullptr)
            problems.push_back(label + " cannot be checked: the calendar " + templ.CalendarId + " is not available");
        else if (FindCalendarSpan(*calendar, condition.Span) == nullptr)
            problems.push_back(label + " names the calendar span '" + condition.Span + "', which " + calendar->Id + " does not have");
        break;
    case ThreadConditionKind::Relationship:
        ThrUseRole(templ, condition.RoleA, label, used, problems);
        ThrUseRole(templ, condition.RoleB, label, used, problems);
        if (condition.RoleA == condition.RoleB)
            problems.push_back(label + " compares a role with itself");
        if (condition.Value < -ThrMaxMilli || condition.Value > ThrMaxMilli)
            problems.push_back(label + " value " + std::to_string(condition.Value) + " is outside " + ThrRange(-ThrMaxMilli, ThrMaxMilli));
        break;
    case ThreadConditionKind::Event:
        if (!IsStableIdToken(condition.EventType))
            problems.push_back(label + " event type '" + condition.EventType + "' is not a lowercase token");
        else if (const std::string_view word = ForbiddenWordIn(condition.EventType); !word.empty())
            problems.push_back(label + " event type '" + condition.EventType + "' is keyed on '" + std::string(word) +
                               "', which no thread may depend on");
        if (condition.Parties.empty() || condition.Parties.size() > 3)
            problems.push_back(label + " needs 1 to 3 parties, found " + std::to_string(condition.Parties.size()));
        for (std::size_t i = 0; i < condition.Parties.size(); ++i)
        {
            ThrUseRole(templ, condition.Parties[i], label, used, problems);
            if (std::find(condition.Parties.begin(), condition.Parties.begin() + static_cast<std::ptrdiff_t>(i), condition.Parties[i]) !=
                condition.Parties.begin() + static_cast<std::ptrdiff_t>(i))
                problems.push_back(label + " names the party " + condition.Parties[i] + " twice");
        }
        if (condition.Count < 1 || condition.Count > ThreadMaxEventCount)
            problems.push_back(label + " count " + std::to_string(condition.Count) + " is outside " + ThrRange(1, ThreadMaxEventCount));
        break;
    }
}

void ThrRuleTrigger(const ThreadTemplate& templ, const SchoolCalendar* calendar, const ThreadTrigger& trigger,
                    std::set<std::string>& used, Problems& problems)
{
    const std::string label = "trigger " + trigger.Id;
    const std::int32_t stages = static_cast<std::int32_t>(templ.Stages.size());
    if (trigger.ArmedFrom < 0 || trigger.ArmedUntil >= stages)
        problems.push_back(label + " is armed outside the template's stages");
    else if (trigger.ArmedFrom > trigger.ArmedUntil)
        problems.push_back(label + " is armed until stage " + std::to_string(trigger.ArmedUntil) + ", before it is armed from stage " +
                           std::to_string(trigger.ArmedFrom));
    if (trigger.Conditions.empty() || trigger.Conditions.size() > ThreadMaxConditions)
        problems.push_back(label + " needs 1 to " + std::to_string(ThreadMaxConditions) + " conditions, found " +
                           std::to_string(trigger.Conditions.size()));
    std::size_t number = 0;
    for (const ThreadCondition& condition : trigger.Conditions)
        ThrRuleCondition(templ, calendar, condition, label + " condition " + std::to_string(++number), used, problems);

    if (trigger.Effect == ThreadEffectKind::Resolve)
    {
        if (trigger.Resolution.empty())
            problems.push_back(label + " resolves but names no resolution");
        else if (ThreadFindResolution(templ, trigger.Resolution) == nullptr)
            problems.push_back(label + " names the resolution " + trigger.Resolution + ", which the template does not have");
    }
    else if (!trigger.Resolution.empty())
        problems.push_back(label + " names a resolution but its effect is not resolve");
    if (trigger.Effect == ThreadEffectKind::Express && !trigger.Expression)
        problems.push_back(label + " expresses but has no expression");
    if (trigger.Expression)
    {
        ThrRuleExpression(templ, *trigger.Expression, label, problems);
        used.insert(trigger.Expression->By);
    }
}

bool ThrHasEvent(const ThreadTrigger& trigger)
{
    return std::any_of(trigger.Conditions.begin(), trigger.Conditions.end(),
                       [](const ThreadCondition& condition) { return condition.Kind == ThreadConditionKind::Event; });
}
} // namespace

bool IsThreadId(std::string_view text)
{
    if (text.empty() || text.size() > 64)
        return false;
    return std::all_of(text.begin(), text.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

std::string_view ForbiddenWordIn(std::string_view text)
{
    std::size_t start = 0;
    while (start <= text.size())
    {
        std::size_t end = start;
        while (end < text.size() && text[end] != '_' && text[end] != '.' && text[end] != ':' && text[end] != '-')
            ++end;
        const std::string_view word = text.substr(start, end - start);
        for (const std::string_view forbidden : ThrForbiddenWords)
        {
            if (word == forbidden)
                return forbidden;
        }
        start = end + 1;
    }
    return {};
}

const CalendarSpan* FindCalendarSpan(const SchoolCalendar& calendar, std::string_view id)
{
    const auto find = [&](const auto& list) -> const CalendarSpan* {
        for (const auto& entry : list)
        {
            if (entry.Id == id)
                return &entry;
        }
        return nullptr;
    };
    if (const CalendarSpan* span = find(calendar.Terms))
        return span;
    if (const CalendarSpan* span = find(calendar.Breaks))
        return span;
    if (const CalendarSpan* span = find(calendar.Holidays))
        return span;
    if (const CalendarSpan* span = find(calendar.Assessments))
        return span;
    return find(calendar.Events);
}

std::size_t FirstEventSlot(const ThreadTemplate& templ, std::size_t trigger)
{
    std::size_t slot = 0;
    for (std::size_t i = 0; i < trigger && i < templ.Triggers.size(); ++i)
    {
        for (const ThreadCondition& condition : templ.Triggers[i].Conditions)
            slot += condition.Kind == ThreadConditionKind::Event ? 1 : 0;
    }
    return slot;
}

void ValidateThreadShape(const ThreadTemplate& templ, Problems& problems)
{
    if (templ.Name.empty())
        problems.push_back("the template has no name");
    if (templ.OpensWhen.empty())
        problems.push_back("opensWhen is empty: say what makes the game open this thread");
    ThrRuleRoles(templ, problems);
    ThrRuleStages(templ, problems);
    ThrRuleResolutions(templ, problems);
}

void ValidateThreadTriggers(const ThreadTemplate& templ, const SchoolCalendar* calendar, Problems& problems)
{
    if (templ.Triggers.empty() || templ.Triggers.size() > ThreadMaxTriggers)
        problems.push_back("needs 1 to " + std::to_string(ThreadMaxTriggers) + " triggers, found " + std::to_string(templ.Triggers.size()));
    std::set<std::string> ids;
    std::set<std::string> used;
    bool playerCanChange = false;
    for (const ThreadTrigger& trigger : templ.Triggers)
    {
        if (!IsThreadId(trigger.Id))
            problems.push_back("trigger id '" + trigger.Id + "' must be lowercase letters, digits and underscores");
        if (!ids.insert(trigger.Id).second)
            problems.push_back("two triggers are both called " + trigger.Id);
        ThrRuleTrigger(templ, calendar, trigger, used, problems);
        playerCanChange = playerCanChange ||
                          (ThrHasEvent(trigger) && (trigger.Effect == ThreadEffectKind::Ease || trigger.Effect == ThreadEffectKind::Resolve));
    }
    if (!playerCanChange)
        problems.push_back("no event trigger eases or resolves the thread, so nothing the player does can change its course");
    for (const ThreadStage& stage : templ.Stages)
        used.insert(stage.Expression.By);
    for (const ThreadResolution& resolution : templ.Resolutions)
        used.insert(resolution.Expression.By);
    for (const ThreadRole& role : templ.Roles)
    {
        if (used.count(role.Id) == 0)
            problems.push_back("role " + role.Id + " is never used by an expression or a condition");
    }
}
} // namespace PerfectWorld::ThreadInternal
