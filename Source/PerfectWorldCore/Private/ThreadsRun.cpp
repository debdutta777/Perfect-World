#include "Threads.h"

#include "ThreadsInternal.h"

#include <algorithm>
#include <limits>

// The escalation clock, the triggers and the evolution of a thread (ROADMAP M3-06). See the comment in
// Threads.h for the rules; this file is their implementation.
//
// A thread changes only at a WAKE: an instant at which something about it can differ. The wakes are
//   - the next midnight (calendar conditions flip there),
//   - the end of the current stage's dwell,
//   - the instant a days-open or days-in-stage condition of an armed trigger becomes true,
//   - the time of the first unread log entry that one of the template's event conditions names.
// None of them depends on how far the caller asked to advance, so one long advance and many short ones
// visit the same wakes and end in the same state.
namespace PerfectWorld
{
using namespace ThreadInternal;

namespace
{
constexpr GameInstant RunNever = std::numeric_limits<GameInstant>::max();

/** Where the first unread relevant log entry is, found once per advance and again after it has been read. */
struct RunRelevantEntry
{
    bool Valid = false;
    bool Found = false;
    GameInstant At = 0;
};

bool RunEntryMatches(const ThreadTemplate& templ, const ThreadInstance& instance, const ThreadCondition& condition, const EventLogEntry& entry)
{
    if (entry.Type != condition.EventType)
        return false;
    for (const std::string& role : condition.Parties)
    {
        const std::string_view person = ThreadPersonInRole(templ, instance, role);
        if (person.empty() || std::find(entry.Parties.begin(), entry.Parties.end(), person) == entry.Parties.end())
            return false;
    }
    return true;
}

bool RunEntryIsRelevant(const ThreadTemplate& templ, const ThreadInstance& instance, const EventLogEntry& entry)
{
    for (const ThreadTrigger& trigger : templ.Triggers)
    {
        for (const ThreadCondition& condition : trigger.Conditions)
        {
            if (condition.Kind == ThreadConditionKind::Event && RunEntryMatches(templ, instance, condition, entry))
                return true;
        }
    }
    return false;
}

/** Reads every unread log entry dated at or before `upTo` into the event counters. */
void RunConsume(const ThreadTemplate& templ, ThreadInstance& instance, const EventLog* log, GameInstant upTo)
{
    if (log == nullptr)
        return;
    const std::vector<EventLogEntry>& entries = log->Entries();
    while (instance.Cursor < entries.size() && entries[instance.Cursor].At <= upTo)
    {
        const EventLogEntry& entry = entries[instance.Cursor];
        std::size_t slot = 0;
        for (const ThreadTrigger& trigger : templ.Triggers)
        {
            for (const ThreadCondition& condition : trigger.Conditions)
            {
                if (condition.Kind != ThreadConditionKind::Event)
                    continue;
                if (RunEntryMatches(templ, instance, condition, entry))
                    instance.Counts[slot] = std::min(instance.Counts[slot] + 1, ThreadMaxEventCount);
                ++slot;
            }
        }
        ++instance.Cursor;
    }
}

void RunFindRelevant(const ThreadTemplate& templ, const ThreadInstance& instance, const EventLog* log, GameInstant to, RunRelevantEntry& found)
{
    found = {};
    found.Valid = true;
    if (log == nullptr)
        return;
    const std::vector<EventLogEntry>& entries = log->Entries();
    for (std::size_t i = instance.Cursor; i < entries.size() && entries[i].At <= to; ++i)
    {
        if (RunEntryIsRelevant(templ, instance, entries[i]))
        {
            found.Found = true;
            found.At = std::max(entries[i].At, instance.Now);
            return;
        }
    }
}

bool RunArmed(const ThreadTrigger& trigger, const ThreadInstance& instance, std::size_t index)
{
    return ((instance.Fired >> index) & 1u) == 0 && instance.Stage >= trigger.ArmedFrom && instance.Stage <= trigger.ArmedUntil;
}

GameInstant RunStageEnd(const ThreadTemplate& templ, const ThreadInstance& instance)
{
    return instance.StageEnteredAt + static_cast<GameInstant>(templ.Stages[static_cast<std::size_t>(instance.Stage)].DwellDays) * MinutesPerDay;
}

/** The earliest wake after the thread's instant and no later than `to`. */
std::optional<GameInstant> RunNextWake(const ThreadTemplate& templ, const ThreadInstance& instance, GameInstant to,
                                       const RunRelevantEntry& relevant)
{
    GameInstant best = RunNever;
    const auto consider = [&](GameInstant candidate) {
        if (candidate <= to && candidate < best)
            best = candidate;
    };
    consider(GameClock::StartOfDay(instance.Now) + MinutesPerDay);
    consider(std::max(instance.Now, RunStageEnd(templ, instance)));
    for (std::size_t i = 0; i < templ.Triggers.size(); ++i)
    {
        if (!RunArmed(templ.Triggers[i], instance, i))
            continue;
        for (const ThreadCondition& condition : templ.Triggers[i].Conditions)
        {
            const GameInstant span = static_cast<GameInstant>(condition.Days) * MinutesPerDay;
            GameInstant flip = RunNever;
            if (condition.Kind == ThreadConditionKind::DaysOpen)
                flip = instance.OpenedAt + span;
            else if (condition.Kind == ThreadConditionKind::DaysInStage)
                flip = instance.StageEnteredAt + span;
            if (flip > instance.Now)
                consider(flip);
        }
    }
    if (relevant.Found)
        consider(relevant.At);
    if (best == RunNever)
        return std::nullopt;
    return best;
}

// ---- Conditions ------------------------------------------------------------------

std::int32_t RunDimension(const RelationshipBook* book, std::string_view a, std::string_view b, ThreadDimension dimension)
{
    const Relationship* pair = book != nullptr ? book->Find(a, b) : nullptr;
    if (pair == nullptr)
        return 0; // strangers: every dimension starts at 0
    switch (dimension)
    {
    case ThreadDimension::Trust:
        return pair->Trust;
    case ThreadDimension::Warmth:
        return pair->Warmth;
    case ThreadDimension::Respect:
        return pair->Respect;
    case ThreadDimension::Owed:
        return RelationshipOwedBy(*pair, a);
    }
    return 0;
}

bool RunCalendarHolds(const ThreadTemplate& templ, const ThreadFacts& facts, const ThreadCondition& condition, GameInstant at)
{
    if (facts.Calendar == nullptr || facts.Calendar->Id != templ.CalendarId)
        return false;
    const CalendarSpan* span = FindCalendarSpan(*facts.Calendar, condition.Span);
    if (span == nullptr)
        return false;
    const CalendarDay day = CalendarDayOfInstant(at);
    switch (condition.When)
    {
    case ThreadCalendarWhen::During:
        return day >= span->First && day <= span->Last;
    case ThreadCalendarWhen::Started:
        return day >= span->First;
    case ThreadCalendarWhen::Ended:
        return day > span->Last;
    }
    return false;
}

bool RunHolds(const ThreadTemplate& templ, const ThreadInstance& instance, const ThreadFacts& facts, const ThreadCondition& condition,
              std::size_t slot, GameInstant at)
{
    switch (condition.Kind)
    {
    case ThreadConditionKind::DaysOpen:
        return (at - instance.OpenedAt) / MinutesPerDay >= condition.Days;
    case ThreadConditionKind::DaysInStage:
        return (at - instance.StageEnteredAt) / MinutesPerDay >= condition.Days;
    case ThreadConditionKind::Calendar:
        return RunCalendarHolds(templ, facts, condition, at);
    case ThreadConditionKind::Relationship:
    {
        const std::string_view a = ThreadPersonInRole(templ, instance, condition.RoleA);
        const std::string_view b = ThreadPersonInRole(templ, instance, condition.RoleB);
        if (a.empty() || b.empty())
            return false;
        const std::int32_t value = RunDimension(facts.Relationships, a, b, condition.Dimension);
        return condition.Compare == ThreadCompare::AtLeast ? value >= condition.Value : value <= condition.Value;
    }
    case ThreadConditionKind::Event:
        return slot < instance.Counts.size() && instance.Counts[slot] >= condition.Count;
    }
    return false;
}

/** The first armed trigger, in template order, whose every condition holds; -1 when none. */
std::int32_t RunFirstDue(const ThreadTemplate& templ, const ThreadInstance& instance, const ThreadFacts& facts, GameInstant at)
{
    for (std::size_t i = 0; i < templ.Triggers.size(); ++i)
    {
        if (!RunArmed(templ.Triggers[i], instance, i))
            continue;
        std::size_t slot = FirstEventSlot(templ, i);
        bool all = true;
        for (const ThreadCondition& condition : templ.Triggers[i].Conditions)
        {
            if (!RunHolds(templ, instance, facts, condition, slot, at))
            {
                all = false;
                break;
            }
            slot += condition.Kind == ThreadConditionKind::Event ? 1 : 0;
        }
        if (all)
            return static_cast<std::int32_t>(i);
    }
    return -1;
}

// ---- Changes ---------------------------------------------------------------------

enum class RunStep : std::uint8_t { None, Changed, Closed };

void RunEnterStage(const ThreadTemplate& templ, ThreadInstance& instance, std::int32_t stage, GameInstant at, ThreadEventKind kind,
                   std::vector<ThreadEvent>& events)
{
    instance.Stage = stage;
    instance.StageEnteredAt = at;
    std::size_t slot = 0;
    for (const ThreadTrigger& trigger : templ.Triggers)
    {
        for (const ThreadCondition& condition : trigger.Conditions)
        {
            if (condition.Kind != ThreadConditionKind::Event)
                continue;
            if (condition.Window == ThreadWindow::SinceStage)
                instance.Counts[slot] = 0;
            ++slot;
        }
    }
    ThreadEvent event;
    event.Kind = kind;
    event.Instance = instance.Id;
    event.At = at;
    event.Stage = stage;
    events.push_back(std::move(event));
    PushExpressed(events, instance, templ.Stages[static_cast<std::size_t>(stage)].Expression, at, -1);
}

RunStep RunCloseWith(ThreadSet& set, std::size_t index, const ThreadTemplate& templ, std::string_view resolution, GameInstant at,
                     std::vector<ThreadEvent>& events)
{
    const ThreadResolution* chosen = ThreadFindResolution(templ, resolution);
    if (chosen == nullptr)
        chosen = ThreadFindResolution(templ, templ.DefaultResolution);
    if (chosen == nullptr)
        return RunStep::None; // unreachable for a validated template
    CloseThread(set, index, *chosen, at, events);
    return RunStep::Closed;
}

/** One stage up; past the last stage the clock is complete and the default resolution closes the thread. */
RunStep RunEscalate(ThreadSet& set, std::size_t index, const ThreadTemplate& templ, GameInstant at, std::vector<ThreadEvent>& events)
{
    ThreadInstance& instance = set.Active[index];
    if (static_cast<std::size_t>(instance.Stage) + 1 < templ.Stages.size())
    {
        RunEnterStage(templ, instance, instance.Stage + 1, at, ThreadEventKind::Escalated, events);
        return RunStep::Changed;
    }
    return RunCloseWith(set, index, templ, templ.DefaultResolution, at, events);
}

RunStep RunFire(ThreadSet& set, std::size_t index, const ThreadTemplate& templ, std::int32_t triggerIndex, GameInstant at,
                std::vector<ThreadEvent>& events)
{
    ThreadInstance& instance = set.Active[index];
    const ThreadTrigger& trigger = templ.Triggers[static_cast<std::size_t>(triggerIndex)];
    instance.Fired |= 1u << static_cast<unsigned>(triggerIndex);
    ThreadEvent fired;
    fired.Kind = ThreadEventKind::TriggerFired;
    fired.Instance = instance.Id;
    fired.At = at;
    fired.Stage = instance.Stage;
    fired.Trigger = triggerIndex;
    fired.Detail = trigger.Id;
    if (trigger.Expression)
    {
        fired.Channel = trigger.Expression->Channel;
        fired.Scene = trigger.Expression->Scene;
    }
    events.push_back(std::move(fired));
    switch (trigger.Effect)
    {
    case ThreadEffectKind::Escalate:
        return RunEscalate(set, index, templ, at, events);
    case ThreadEffectKind::Ease:
        RunEnterStage(templ, instance, std::max(instance.Stage - 1, 0), at, ThreadEventKind::Eased, events);
        return RunStep::Changed;
    case ThreadEffectKind::Resolve:
        return RunCloseWith(set, index, templ, trigger.Resolution, at, events);
    case ThreadEffectKind::Express:
        if (trigger.Expression)
            PushExpressed(events, instance, *trigger.Expression, at, triggerIndex);
        return RunStep::Changed;
    }
    return RunStep::Changed;
}

/** Triggers first, in template order, then the clock; repeated until nothing changes at this instant. True when the thread closed. */
bool RunSettle(ThreadSet& set, std::size_t index, const ThreadTemplate& templ, const ThreadFacts& facts, GameInstant at,
               std::vector<ThreadEvent>& events)
{
    for (std::size_t step = 0; step < ThreadMaxSettleSteps; ++step)
    {
        const ThreadInstance& instance = set.Active[index];
        const std::int32_t due = RunFirstDue(templ, instance, facts, at);
        RunStep result = RunStep::None;
        if (due >= 0)
            result = RunFire(set, index, templ, due, at, events);
        else if (at >= RunStageEnd(templ, instance))
            result = RunEscalate(set, index, templ, at, events);
        if (result == RunStep::Closed)
            return true;
        if (result == RunStep::None)
            return false;
    }
    return false;
}

/** Evolves one thread to `to`. True when it closed (and left Active). */
bool RunAdvanceThread(ThreadSet& set, std::size_t index, const ThreadTemplate& templ, const ThreadFacts& facts, GameInstant to,
                      std::vector<ThreadEvent>& events)
{
    RunRelevantEntry relevant;
    while (true)
    {
        ThreadInstance& instance = set.Active[index];
        if (!relevant.Valid)
            RunFindRelevant(templ, instance, facts.Log, to, relevant);
        const std::optional<GameInstant> wake = RunNextWake(templ, instance, to, relevant);
        if (!wake)
            break;
        instance.Now = std::max(instance.Now, *wake);
        RunConsume(templ, instance, facts.Log, instance.Now);
        if (relevant.Found && instance.Now >= relevant.At)
            relevant.Valid = false;
        if (RunSettle(set, index, templ, facts, instance.Now, events))
            return true;
    }
    ThreadInstance& instance = set.Active[index];
    RunConsume(templ, instance, facts.Log, to);
    instance.Now = to;
    return false;
}
} // namespace

ThreadAdvanceResult ThreadAdvance(ThreadSet& set, const ThreadTemplateSet& templates, const ThreadFacts& facts, GameInstant to)
{
    ThreadAdvanceResult result;
    if (to < 0 || std::any_of(set.Active.begin(), set.Active.end(), [to](const ThreadInstance& live) { return to < live.Now; }))
    {
        result.Status = ThreadStatus::BadTime;
        return result;
    }
    if (!ThreadSetMatchesTemplates(set, templates))
    {
        result.Status = ThreadStatus::BadTemplate;
        return result;
    }
    if (facts.Log != nullptr && !ThreadSetMatchesLog(set, *facts.Log))
    {
        result.Status = ThreadStatus::BadFacts;
        return result;
    }
    for (std::size_t index = 0; index < set.Active.size();)
    {
        const ThreadTemplate* templ = templates.Find(set.Active[index].Template);
        if (!RunAdvanceThread(set, index, *templ, facts, to, result.Events))
            ++index;
    }
    // Time order across threads; ties keep thread order, then emission order.
    std::stable_sort(result.Events.begin(), result.Events.end(), [](const ThreadEvent& a, const ThreadEvent& b) { return a.At < b.At; });
    return result;
}
} // namespace PerfectWorld
