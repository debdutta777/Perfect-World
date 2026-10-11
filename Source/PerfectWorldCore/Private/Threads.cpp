#include "Threads.h"

#include "ThreadsInternal.h"

#include <algorithm>
#include <set>

// Opening and closing threads, the queries and the consistency checks (ROADMAP M3-06). The clock and
// the triggers are in ThreadsRun.cpp, the text form in ThreadsText.cpp.
namespace PerfectWorld
{
using namespace ThreadInternal;

namespace ThreadInternal
{
bool ResolvedBefore(const ThreadResolved& a, const ThreadResolved& b)
{
    if (a.At != b.At)
        return a.At < b.At;
    // Ids are "thr_" and a number: a longer one is a larger number.
    return a.Id.size() != b.Id.size() ? a.Id.size() < b.Id.size() : a.Id < b.Id;
}

void PushExpressed(std::vector<ThreadEvent>& events, const ThreadInstance& instance, const ThreadExpression& expression, GameInstant at,
                   std::int32_t trigger)
{
    ThreadEvent event;
    event.Kind = ThreadEventKind::Expressed;
    event.Instance = instance.Id;
    event.At = at;
    event.Stage = instance.Stage;
    event.Trigger = trigger;
    event.Channel = expression.Channel;
    event.Scene = expression.Scene;
    events.push_back(std::move(event));
}

void CloseThread(ThreadSet& set, std::size_t index, const ThreadResolution& resolution, GameInstant at, std::vector<ThreadEvent>& events)
{
    const ThreadInstance& instance = set.Active[index];
    ThreadEvent event;
    event.Kind = ThreadEventKind::Resolved;
    event.Instance = instance.Id;
    event.At = at;
    event.Stage = instance.Stage;
    event.Channel = resolution.Expression.Channel;
    event.Scene = resolution.Expression.Scene;
    event.Detail = resolution.Id;
    events.push_back(std::move(event));
    // The history is kept in (time, instance number) order, whatever order the threads were closed in.
    ThreadResolved record{instance.Id, instance.Template, resolution.Id, at, instance.People};
    const auto after = std::upper_bound(set.Resolved.begin(), set.Resolved.end(), record, ResolvedBefore);
    set.Resolved.insert(after, std::move(record));
    set.Active.erase(set.Active.begin() + static_cast<std::ptrdiff_t>(index));
}
} // namespace ThreadInternal

namespace
{
GameInstant ThrLatestInstant(const ThreadSet& set)
{
    GameInstant latest = 0;
    for (const ThreadInstance& instance : set.Active)
        latest = std::max(latest, instance.Now);
    return latest;
}

/** One person per role in the template's role order, or nothing when the bindings are not exactly one per role. */
std::optional<std::vector<std::string>> ThrBind(const ThreadTemplate& templ, const std::vector<ThreadRoleBinding>& bindings)
{
    if (bindings.size() != templ.Roles.size())
        return std::nullopt;
    std::vector<std::string> people(templ.Roles.size());
    std::vector<bool> filled(templ.Roles.size(), false);
    for (const ThreadRoleBinding& binding : bindings)
    {
        const std::int32_t index = ThreadRoleIndex(templ, binding.Role);
        if (index < 0 || filled[static_cast<std::size_t>(index)])
            return std::nullopt;
        filled[static_cast<std::size_t>(index)] = true;
        people[static_cast<std::size_t>(index)] = binding.Person;
    }
    return people;
}

bool ThrPeopleAreKnownAndDistinct(const std::vector<std::string>& people, const IdentityRegistry& registry)
{
    std::set<std::string> seen;
    for (const std::string& person : people)
    {
        if (FindIdentity(registry, person) == nullptr || !seen.insert(person).second)
            return false;
    }
    return true;
}
} // namespace

ThreadOpenResult ThreadOpen(ThreadSet& set, const ThreadTemplateSet& templates, std::string_view templateId,
                            const std::vector<ThreadRoleBinding>& bindings, const IdentityRegistry& registry, const ThreadFacts& facts,
                            GameInstant at)
{
    ThreadOpenResult result;
    const ThreadTemplate* templ = templates.Find(templateId);
    if (templ == nullptr || templ->Stages.empty())
        return result;
    if (at < 0 || at < ThrLatestInstant(set))
    {
        result.Status = ThreadStatus::BadTime;
        return result;
    }
    if (set.Active.size() >= ThreadMaxActive || set.NextInstance > ThreadMaxInstances)
    {
        result.Status = ThreadStatus::Full;
        return result;
    }
    const std::optional<std::vector<std::string>> people = ThrBind(*templ, bindings);
    if (!people)
    {
        result.Status = ThreadStatus::BadRoles;
        return result;
    }
    if (!ThrPeopleAreKnownAndDistinct(*people, registry))
    {
        result.Status = ThreadStatus::BadPeople;
        return result;
    }
    for (const ThreadInstance& live : set.Active)
    {
        if (live.Template == templ->Id && live.People == *people)
        {
            result.Status = ThreadStatus::Duplicate;
            return result;
        }
    }

    ThreadInstance instance;
    instance.Id = "thr_" + std::to_string(set.NextInstance);
    instance.Template = templ->Id;
    instance.People = *people;
    instance.OpenedAt = at;
    instance.StageEnteredAt = at;
    instance.Now = at;
    instance.Cursor = facts.Log != nullptr ? facts.Log->Size() : 0;
    instance.Counts.assign(ThreadEventSlotCount(*templ), 0);

    ThreadEvent opened;
    opened.Kind = ThreadEventKind::Opened;
    opened.Instance = instance.Id;
    opened.At = at;
    opened.Detail = templ->Id;
    result.Events.push_back(std::move(opened));
    PushExpressed(result.Events, instance, templ->Stages.front().Expression, at, -1);

    result.Status = ThreadStatus::Ok;
    result.Instance = instance.Id;
    ++set.NextInstance;
    set.Active.push_back(std::move(instance));
    return result;
}

ThreadAdvanceResult ThreadClose(ThreadSet& set, const ThreadTemplateSet& templates, std::string_view instance, std::string_view resolution,
                                GameInstant at)
{
    ThreadAdvanceResult result;
    const auto it = std::find_if(set.Active.begin(), set.Active.end(), [&](const ThreadInstance& live) { return live.Id == instance; });
    if (it == set.Active.end())
    {
        result.Status = ThreadStatus::NotFound;
        return result;
    }
    const ThreadTemplate* templ = templates.Find(it->Template);
    if (templ == nullptr)
    {
        result.Status = ThreadStatus::BadTemplate;
        return result;
    }
    const ThreadResolution* chosen = ThreadFindResolution(*templ, resolution);
    if (chosen == nullptr)
    {
        result.Status = ThreadStatus::BadResolution;
        return result;
    }
    if (at < it->Now)
    {
        result.Status = ThreadStatus::BadTime;
        return result;
    }
    CloseThread(set, static_cast<std::size_t>(it - set.Active.begin()), *chosen, at, result.Events);
    return result;
}

const ThreadInstance* ThreadFind(const ThreadSet& set, std::string_view instance)
{
    for (const ThreadInstance& live : set.Active)
    {
        if (live.Id == instance)
            return &live;
    }
    return nullptr;
}

const ThreadExpression* ThreadCurrentExpression(const ThreadTemplate& templ, const ThreadInstance& instance)
{
    if (instance.Stage < 0 || static_cast<std::size_t>(instance.Stage) >= templ.Stages.size())
        return nullptr;
    return &templ.Stages[static_cast<std::size_t>(instance.Stage)].Expression;
}

std::int32_t ThreadIntensity(const ThreadTemplate& templ, const ThreadInstance& instance)
{
    if (instance.Stage < 0 || static_cast<std::size_t>(instance.Stage) >= templ.Stages.size())
        return 0;
    return templ.Stages[static_cast<std::size_t>(instance.Stage)].Intensity;
}

std::string_view ThreadPersonInRole(const ThreadTemplate& templ, const ThreadInstance& instance, std::string_view role)
{
    const std::int32_t index = ThreadRoleIndex(templ, role);
    if (index < 0 || static_cast<std::size_t>(index) >= instance.People.size())
        return {};
    return instance.People[static_cast<std::size_t>(index)];
}

bool ThreadSetMatchesTemplates(const ThreadSet& set, const ThreadTemplateSet& templates)
{
    for (const ThreadInstance& live : set.Active)
    {
        const ThreadTemplate* templ = templates.Find(live.Template);
        if (templ == nullptr || live.Stage < 0 || static_cast<std::size_t>(live.Stage) >= templ->Stages.size())
            return false;
        if (live.People.size() != templ->Roles.size() || live.Counts.size() != ThreadEventSlotCount(*templ))
            return false;
        if (live.Fired >> templ->Triggers.size() != 0 || live.OpenedAt > live.StageEnteredAt || live.StageEnteredAt > live.Now)
            return false;
        if (std::any_of(live.Counts.begin(), live.Counts.end(), [](std::int32_t count) { return count < 0 || count > ThreadMaxEventCount; }))
            return false;
    }
    for (const ThreadResolved& done : set.Resolved)
    {
        const ThreadTemplate* templ = templates.Find(done.Template);
        if (templ == nullptr || ThreadFindResolution(*templ, done.Resolution) == nullptr || done.People.size() != templ->Roles.size())
            return false;
    }
    return true;
}

bool ThreadSetMatchesLog(const ThreadSet& set, const EventLog& log)
{
    return std::all_of(set.Active.begin(), set.Active.end(), [&](const ThreadInstance& live) { return live.Cursor <= log.Size(); });
}
} // namespace PerfectWorld
