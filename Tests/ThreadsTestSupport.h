// Shared helpers for the M3-06 thread tests (PerfectWorldThreadTests; main() is in ThreadsTests.cpp).
#pragma once

#include "EventLog.h"
#include "IdentityRegistry.h"
#include "Relationships.h"
#include "SchoolCalendar.h"
#include "Threads.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace PerfectWorld;

namespace ThrTest
{
inline int Checks = 0;

inline void Expect(bool condition, const std::string& message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::int64_t People = 100;
constexpr const char* CalendarId = "calendar.nabapara_2042_43";

/** Everything the tests share: the real content, its templates and calendar, and a registry of npc_0 .. npc_99. */
struct World
{
    std::vector<ContentFile> Files;
    ContentLoadResult Loaded;
    ThreadTemplateSet Templates;
    SchoolCalendarSet Calendars;
    IdentityRegistry Registry;
    const SchoolCalendar* Calendar = nullptr;
};

World& TheWorld();
/** Reads Data/Content from `root` into the world; false when it does not validate. */
bool LoadWorld(const std::string& root);

inline std::string Person(std::int64_t n)
{
    return "npc_" + std::to_string(n);
}

/** Binds the template's role i to Person(first + i). */
inline std::vector<ThreadRoleBinding> Bind(const ThreadTemplate& templ, std::int64_t first)
{
    std::vector<ThreadRoleBinding> bindings;
    for (std::size_t i = 0; i < templ.Roles.size(); ++i)
        bindings.push_back({templ.Roles[i].Id, Person(first + static_cast<std::int64_t>(i))});
    return bindings;
}

inline std::uint64_t Append(EventLog& log, GameInstant at, const std::string& type, const std::vector<std::string>& parties)
{
    EventLogDraft draft;
    draft.At = at;
    draft.Type = type;
    draft.Parties = parties;
    const EventLogAppendResult result = log.Append(draft);
    Expect(result.Ok(), "the test event " + type + " is a valid draft");
    return result.Seq;
}

inline GameInstant DayStart(const char* iso)
{
    const std::optional<CalendarDay> day = ParseCalendarDate(iso);
    Expect(day.has_value(), "a test date parses");
    return CalendarDayStartInstant(day.value_or(0));
}

inline ThreadFacts Facts(const EventLog* log, const RelationshipBook* book, const SchoolCalendar* calendar)
{
    ThreadFacts facts;
    facts.Log = log;
    facts.Relationships = book;
    facts.Calendar = calendar;
    return facts;
}

inline const ThreadTemplate& Template(const char* id)
{
    const ThreadTemplate* templ = TheWorld().Templates.Find(id);
    Expect(templ != nullptr, std::string("the template exists: ") + id);
    static const ThreadTemplate none;
    return templ != nullptr ? *templ : none;
}

inline std::size_t CountEvents(const std::vector<ThreadEvent>& events, ThreadEventKind kind, const std::string& detail = std::string())
{
    std::size_t count = 0;
    for (const ThreadEvent& event : events)
        count += (event.Kind == kind && (detail.empty() || event.Detail == detail)) ? 1 : 0;
    return count;
}

/** A thread set with its facts, so a scenario reads as a story. */
struct Rig
{
    ThreadSet Set;
    EventLog Log;
    RelationshipBook Book;
    const SchoolCalendar* Calendar = nullptr;

    [[nodiscard]] ThreadFacts MakeFacts() const { return Facts(&Log, &Book, Calendar); }

    /** Opens the template with Person(first)... in its roles; returns the instance ID. */
    std::string Open(const char* id, std::int64_t first, GameInstant at)
    {
        const ThreadOpenResult result = ThreadOpen(Set, TheWorld().Templates, id, Bind(Template(id), first), TheWorld().Registry, MakeFacts(), at);
        Expect(result.Ok(), std::string("the scenario opens ") + id);
        return result.Instance;
    }

    std::vector<ThreadEvent> Advance(GameInstant to)
    {
        ThreadAdvanceResult result = ThreadAdvance(Set, TheWorld().Templates, MakeFacts(), to);
        Expect(result.Ok(), "the scenario advances");
        return std::move(result.Events);
    }

    std::uint64_t Log1(GameInstant at, const std::string& type, const std::vector<std::string>& parties)
    {
        return Append(Log, at, type, parties);
    }

    /** One remembered act between two people (an event plus its relationship change). */
    void Act(GameInstant at, std::int64_t actor, std::int64_t other, RelationshipActKind kind)
    {
        const std::uint64_t seq = Append(Log, at, "act_" + std::string(RelationshipActKindName(kind)), {Person(actor), Person(other)});
        Expect(Book.Apply(Log, RelationshipAct{seq, Person(actor), Person(other), kind}).Status != RelationshipStatus::BadParties, "the act applies");
    }
};

inline GameInstant Days(std::int64_t days)
{
    return days * MinutesPerDay;
}

// Defined in ThreadsRunTests.cpp, ThreadsEquivalenceTests.cpp and ThreadsSaveTests.cpp.
void TestSchedule();
void TestTriggers();
void TestCalendarTriggers();
void TestEquivalence();
void TestRandomOperations();
void TestSave();
} // namespace ThrTest
