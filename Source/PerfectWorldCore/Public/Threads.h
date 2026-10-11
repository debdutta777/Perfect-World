#pragma once

#include "ContentRegistry.h"
#include "EventLog.h"
#include "GameClock.h"
#include "IdentityRegistry.h"
#include "Relationships.h"
#include "SchoolCalendar.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Threads: persistent unresolved tensions that escalate on a clock (ROADMAP M3-06;
 * SYSTEMS_CATALOGUE.md "Threads"; ALTERNATE_WORLD.md section 8 "Where story comes from when
 * there is no hero": "Each has participants, an escalation clock, trigger conditions and a
 * set of authored expressions").
 *
 * TEMPLATES (content type `thread_template`, Data/Content/thread_template). A template names
 * 2 to 4 ROLES (never people), 3 to 6 ordered STAGES (the escalation clock: each stage has an
 * intensity that rises to 1000 and a dwell in days; when the dwell runs out the thread
 * escalates, and when the last stage's dwell runs out it resolves with the template's DEFAULT
 * resolution), TRIGGERS (plain-data conditions with one effect each) and EXPRESSIONS (what the
 * player can observe: a channel, the role that does it, an optional scene token and a line of
 * text). A template never mentions religion, caste, language, stream, gender or family
 * background: role IDs and event types naming those words are refused, and the content
 * fairness rule runs on every record.
 *
 * INSTANCES. ThreadOpen binds every role to a distinct registered identity (IdentityRegistry)
 * and starts stage 0. An instance is plain integers: stage, open and stage-entry instants,
 * the log cursor, a fired-trigger mask and one saturating counter per event condition.
 *
 * EVOLUTION. ThreadAdvance moves every active thread to an instant. A thread is evaluated
 * only at WAKES, the instants at which anything about it can change: the next midnight, the
 * end of the stage dwell, the instant a "days" condition flips, and the time of any unread
 * event-log entry that one of its event conditions names. Between wakes nothing can differ,
 * so advancing in one call equals advancing in any number of smaller calls over the same
 * facts (tested). At a wake the module first reads the log entries up to that instant, then
 * settles: armed triggers fire in template order (each at most ONCE per instance), then the
 * clock escalates if the dwell is over; this repeats until nothing changes at that instant.
 * Facts are read as supplied: the caller advances the threads to an event's instant BEFORE
 * applying that event's relationship change, as it does for every system.
 *
 * Calendar days use SchoolCalendar numbering (day 0 is Monday 2 June 2042). The module is
 * integers only, deterministic, never throws, and not yet wired into WorldState, the reducer,
 * the world hash, the day loop or the save. To integrate: hold a ThreadSet in WorldState, mix
 * HashThreads into the hash, store SerialiseThreads in the save, call ThreadAdvance at event
 * boundaries (phase 40, SYSTEMS_CATALOGUE.md), and turn the returned ThreadEvents into
 * nominations and the week's one interrupt (M3-02 owns that budget).
 *
 * Choices the documents leave open (named here, listed in the M3-06 report):
 *  - an escalation past the last stage resolves with the default resolution;
 *  - an "ease" at stage 0 restarts that stage's dwell;
 *  - events logged at the instant a stage changes belong to the old stage's window;
 *  - the limits below.
 */

constexpr std::uint32_t ThreadFormatVersion = 1;
/** The catalogue's cap on live threads. */
constexpr std::size_t ThreadMaxActive = 40;
/** Instance numbers run 1..this; the resolved history is therefore bounded too. */
constexpr std::uint64_t ThreadMaxInstances = 4096;
constexpr std::size_t ThreadMinRoles = 2;
constexpr std::size_t ThreadMaxRoles = 4;
constexpr std::size_t ThreadMinStages = 3;
constexpr std::size_t ThreadMaxStages = 6;
constexpr std::size_t ThreadMinResolutions = 2;
constexpr std::size_t ThreadMaxResolutions = 6;
constexpr std::size_t ThreadMaxTriggers = 8;
constexpr std::size_t ThreadMaxConditions = 4;
/** Event conditions per template: every trigger may hold several. */
constexpr std::size_t ThreadMaxEventSlots = ThreadMaxTriggers * ThreadMaxConditions;
constexpr std::int32_t ThreadMaxIntensity = 1000;
constexpr std::int32_t ThreadMaxDwellDays = 120;
/** The whole clock, all stages added, fits inside the 265-day year. */
constexpr std::int32_t ThreadMaxClockDays = 200;
constexpr std::int32_t ThreadMaxConditionDays = 365;
/** An event condition counts up to this and saturates. */
constexpr std::int32_t ThreadMaxEventCount = 20;
constexpr std::size_t ThreadMaxObserveLength = 240;
/** The most trigger firings and stage changes at one instant; a guard, never reached by valid templates. */
constexpr std::size_t ThreadMaxSettleSteps = 32;

enum class ThreadSituation : std::uint8_t { School, Home, Neighbourhood, Friendship };
/** The tensions ALTERNATE_WORLD.md section 8 lists. */
enum class ThreadKind : std::uint8_t { Debt, Promise, Grudge, Ambition, Secret, Obligation, Attraction, Rivalry };
/** How an expression reaches the player; Scene may nominate a scene or the week's one interrupt. */
enum class ThreadChannel : std::uint8_t { Ambient, Message, Conversation, Scene };
/** How a resolution feels. A thread left alone never ends Repaired. */
enum class ThreadOutcome : std::uint8_t { Repaired, Settled, Distance, Loss };
enum class ThreadConditionKind : std::uint8_t { DaysOpen, DaysInStage, Calendar, Relationship, Event };
/** During: the day is inside the span. Started: on or after its first day. Ended: after its last day. */
enum class ThreadCalendarWhen : std::uint8_t { During, Started, Ended };
/** Owed: how much role A owes role B. */
enum class ThreadDimension : std::uint8_t { Trust, Warmth, Respect, Owed };
enum class ThreadCompare : std::uint8_t { AtLeast, AtMost };
/** SinceOpen counts entries after the thread opened; SinceStage resets at every stage change. */
enum class ThreadWindow : std::uint8_t { SinceOpen, SinceStage };
enum class ThreadEffectKind : std::uint8_t { Escalate, Ease, Resolve, Express };

PERFECTWORLDCORE_API std::string_view ThreadChannelName(ThreadChannel channel);
PERFECTWORLDCORE_API std::string_view ThreadOutcomeName(ThreadOutcome outcome);

/** What the player can observe. */
struct ThreadExpression
{
    ThreadChannel Channel = ThreadChannel::Ambient;
    /** The role that does or says it. */
    std::string By;
    /** A scene token, required for the Scene channel. */
    std::string Scene;
    /** Plain text a writer replaces; at most ThreadMaxObserveLength characters. */
    std::string Observe;

    friend bool operator==(const ThreadExpression&, const ThreadExpression&) = default;
};

struct ThreadRole
{
    std::string Id;
    std::string Name;

    friend bool operator==(const ThreadRole&, const ThreadRole&) = default;
};

struct ThreadStage
{
    std::string Id;
    std::string Name;
    /** 1..1000, strictly rising; the last stage is 1000. */
    std::int32_t Intensity = 0;
    std::int32_t DwellDays = 1;
    ThreadExpression Expression;

    friend bool operator==(const ThreadStage&, const ThreadStage&) = default;
};

/** One test; the fields a kind does not use stay at their defaults. A trigger holds all of its conditions (AND). */
struct ThreadCondition
{
    ThreadConditionKind Kind = ThreadConditionKind::DaysOpen;
    /** DaysOpen, DaysInStage: whole days at least. */
    std::int32_t Days = 0;
    /** Calendar: an ID from the template's calendar (term, break, holiday, assessment or event). */
    std::string Span;
    ThreadCalendarWhen When = ThreadCalendarWhen::During;
    /** Relationship: the two roles, the dimension, the comparison and the Milli value. */
    std::string RoleA;
    std::string RoleB;
    ThreadDimension Dimension = ThreadDimension::Trust;
    ThreadCompare Compare = ThreadCompare::AtLeast;
    std::int32_t Value = 0;
    /** Event: a log entry of this type whose parties include every listed role, counted at least Count times. */
    std::string EventType;
    std::vector<std::string> Parties;
    std::int32_t Count = 1;
    ThreadWindow Window = ThreadWindow::SinceOpen;

    friend bool operator==(const ThreadCondition&, const ThreadCondition&) = default;
};

struct ThreadTrigger
{
    std::string Id;
    /** Stage indices, inclusive: the trigger can fire only while the thread is in this range. */
    std::int32_t ArmedFrom = 0;
    std::int32_t ArmedUntil = 0;
    std::vector<ThreadCondition> Conditions;
    ThreadEffectKind Effect = ThreadEffectKind::Escalate;
    /** The resolution a Resolve effect closes the thread with. */
    std::string Resolution;
    /** Shown when the trigger fires; required for Express. */
    std::optional<ThreadExpression> Expression;

    friend bool operator==(const ThreadTrigger&, const ThreadTrigger&) = default;
};

struct ThreadResolution
{
    std::string Id;
    std::string Name;
    ThreadOutcome Outcome = ThreadOutcome::Settled;
    ThreadExpression Expression;

    friend bool operator==(const ThreadResolution&, const ThreadResolution&) = default;
};

struct ThreadTemplate
{
    std::string Id;
    std::string Name;
    ThreadSituation Situation = ThreadSituation::School;
    ThreadKind Kind = ThreadKind::Debt;
    /** Prose: the situation that makes the game open this thread. */
    std::string OpensWhen;
    /** The calendar the Calendar conditions name; empty when there are none. */
    std::string CalendarId;
    std::vector<ThreadRole> Roles;
    std::vector<ThreadStage> Stages;
    std::vector<ThreadTrigger> Triggers;
    std::vector<ThreadResolution> Resolutions;
    std::string DefaultResolution;

    friend bool operator==(const ThreadTemplate&, const ThreadTemplate&) = default;
};

struct ThreadTemplateSet
{
    /** Sorted by Id. */
    std::vector<ThreadTemplate> Templates;

    [[nodiscard]] PERFECTWORLDCORE_API const ThreadTemplate* Find(std::string_view id) const noexcept;
};

struct ThreadTemplateResult
{
    /** Empty on success; otherwise the record and what is wrong with it. */
    std::string Error;
    ThreadTemplateSet Set;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** The index of a role, a stage, a resolution; -1 for an ID the template does not have. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ThreadRoleIndex(const ThreadTemplate& templ, std::string_view role);
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ThreadStageIndex(const ThreadTemplate& templ, std::string_view stage);
[[nodiscard]] PERFECTWORLDCORE_API const ThreadResolution* ThreadFindResolution(const ThreadTemplate& templ, std::string_view id);
/** The number of Event conditions across all triggers: the length of an instance's Counts. */
[[nodiscard]] PERFECTWORLDCORE_API std::size_t ThreadEventSlotCount(const ThreadTemplate& templ);

/**
 * Reads one thread_template record. Anything that cannot be read (an unknown enum value, a stage
 * or role the record does not have, a malformed condition) is appended to `problems`, one message
 * each; the result is only meaningful when `problems` stays empty. `calendar` is not looked at.
 */
[[nodiscard]] PERFECTWORLDCORE_API ThreadTemplate ParseThreadTemplateRecord(const ContentRecord& record,
                                                                            std::vector<std::string>& problems);

/**
 * Every rule a parsed template must meet, empty when valid: role, stage and resolution shape and
 * limits, strictly rising intensity ending at 1000, a total clock within ThreadMaxClockDays, the
 * first stage not a scene, expressions by defined roles with observable text and scene tokens,
 * a default resolution that is not Repaired, every resolution reachable, triggers armed inside
 * the stages with well-formed conditions and effects, roles used consistently, calendar
 * conditions naming a span the calendar has (`calendar` may be null only when there are none),
 * and at least one event trigger that eases or resolves (something the player can do).
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateThreadTemplate(const ThreadTemplate& templ,
                                                                                   const SchoolCalendar* calendar);

/** Reads every thread_template record and refuses (never coerces) one that cannot be read or fails validation. */
[[nodiscard]] PERFECTWORLDCORE_API ThreadTemplateResult LoadThreadTemplates(const ContentDb& content);

// ---- Instances -------------------------------------------------------------------

struct ThreadInstance
{
    /** "thr_" and the instance number, 1 upward, never reused. */
    std::string Id;
    std::string Template;
    /** The identity bound to each role, in the template's role order. */
    std::vector<std::string> People;
    std::int32_t Stage = 0;
    GameInstant OpenedAt = 0;
    GameInstant StageEnteredAt = 0;
    /** The instant the thread has been evolved to. */
    GameInstant Now = 0;
    /** Log entries with Seq at or below this have been read. */
    std::uint64_t Cursor = 0;
    /** Bit n is set once trigger n has fired. */
    std::uint32_t Fired = 0;
    /** One per Event condition, in template order; saturates at ThreadMaxEventCount. */
    std::vector<std::int32_t> Counts;

    friend bool operator==(const ThreadInstance&, const ThreadInstance&) = default;
};

struct ThreadResolved
{
    std::string Id;
    std::string Template;
    std::string Resolution;
    GameInstant At = 0;
    std::vector<std::string> People;

    friend bool operator==(const ThreadResolved&, const ThreadResolved&) = default;
};

struct ThreadSet
{
    /** The number the next instance takes. */
    std::uint64_t NextInstance = 1;
    /** In instance order. */
    std::vector<ThreadInstance> Active;
    /** In the order they closed: by time, then instance number. */
    std::vector<ThreadResolved> Resolved;

    friend bool operator==(const ThreadSet&, const ThreadSet&) = default;
};

/** The facts a thread reads. A null pointer reads as empty: no log, no relationships, and calendar conditions never hold. */
struct ThreadFacts
{
    const EventLog* Log = nullptr;
    const RelationshipBook* Relationships = nullptr;
    const SchoolCalendar* Calendar = nullptr;
};

struct ThreadRoleBinding
{
    std::string Role;
    std::string Person;
};

enum class ThreadStatus : std::uint8_t
{
    Ok,
    /** No such template, or a set that does not match the templates. */
    BadTemplate,
    /** The bindings are not exactly one per role. */
    BadRoles,
    /** A bound person is not in the registry, or two roles hold the same person. */
    BadPeople,
    /** The same template is already open for the same people in the same roles. */
    Duplicate,
    /** ThreadMaxActive live threads, or ThreadMaxInstances ever opened. */
    Full,
    /** The instant is negative or earlier than a thread already evolved to. */
    BadTime,
    /** No such live thread. */
    NotFound,
    /** The template has no such resolution. */
    BadResolution,
    /** A thread has read more of the log than the supplied log holds: not the same history. */
    BadFacts,
};

enum class ThreadEventKind : std::uint8_t { Opened, TriggerFired, Escalated, Eased, Expressed, Resolved };

/** What happened, in time order: the catalogue's ThreadOpened, ThreadEscalated, ThreadExpressed and ThreadResolved. */
struct ThreadEvent
{
    ThreadEventKind Kind = ThreadEventKind::Opened;
    std::string Instance;
    GameInstant At = 0;
    /** The stage after the event. */
    std::int32_t Stage = 0;
    /** TriggerFired, and Expressed by a trigger: the trigger index; otherwise -1. */
    std::int32_t Trigger = -1;
    ThreadChannel Channel = ThreadChannel::Ambient;
    std::string Scene;
    /** Opened: the template; TriggerFired: the trigger; Resolved: the resolution; otherwise empty. */
    std::string Detail;

    friend bool operator==(const ThreadEvent&, const ThreadEvent&) = default;
};

struct ThreadOpenResult
{
    ThreadStatus Status = ThreadStatus::BadTemplate;
    std::string Instance;
    std::vector<ThreadEvent> Events;

    [[nodiscard]] bool Ok() const { return Status == ThreadStatus::Ok; }
};

struct ThreadAdvanceResult
{
    ThreadStatus Status = ThreadStatus::Ok;
    std::vector<ThreadEvent> Events;

    [[nodiscard]] bool Ok() const { return Status == ThreadStatus::Ok; }
};

/**
 * Opens a thread at `at`: Opened and Expressed (stage 0). Every role needs a binding to a
 * distinct identity in `registry`; the log entries already in `facts.Log` are not counted.
 * `at` must not be earlier than a live thread's instant. A refused open changes nothing.
 */
PERFECTWORLDCORE_API ThreadOpenResult ThreadOpen(ThreadSet& set, const ThreadTemplateSet& templates, std::string_view templateId,
                                                 const std::vector<ThreadRoleBinding>& bindings, const IdentityRegistry& registry,
                                                 const ThreadFacts& facts, GameInstant at);

/**
 * Evolves every live thread to `to` (see the header comment). Refuses, changing nothing, a `to`
 * before any thread's instant or a set that does not match the templates (ThreadSetMatchesTemplates).
 * Resolved threads leave Active for Resolved.
 */
PERFECTWORLDCORE_API ThreadAdvanceResult ThreadAdvance(ThreadSet& set, const ThreadTemplateSet& templates, const ThreadFacts& facts,
                                                       GameInstant to);

/** Closes a live thread with one of its template's resolutions at `at` (the player's or the game's decision). */
PERFECTWORLDCORE_API ThreadAdvanceResult ThreadClose(ThreadSet& set, const ThreadTemplateSet& templates, std::string_view instance,
                                                     std::string_view resolution, GameInstant at);

[[nodiscard]] PERFECTWORLDCORE_API const ThreadInstance* ThreadFind(const ThreadSet& set, std::string_view instance);
/** The stage's expression, which is what the player can observe now. */
[[nodiscard]] PERFECTWORLDCORE_API const ThreadExpression* ThreadCurrentExpression(const ThreadTemplate& templ, const ThreadInstance& instance);
/** The current stage's intensity, 1..1000. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t ThreadIntensity(const ThreadTemplate& templ, const ThreadInstance& instance);
/** The identity in a role, or empty. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ThreadPersonInRole(const ThreadTemplate& templ, const ThreadInstance& instance,
                                                                      std::string_view role);

/** True when every instance and record fits its template: stage, people, counts, trigger mask and resolution. */
[[nodiscard]] PERFECTWORLDCORE_API bool ThreadSetMatchesTemplates(const ThreadSet& set, const ThreadTemplateSet& templates);
/** True when no thread's cursor is past the end of the log (the same history). */
[[nodiscard]] PERFECTWORLDCORE_API bool ThreadSetMatchesLog(const ThreadSet& set, const EventLog& log);

[[nodiscard]] PERFECTWORLDCORE_API std::uint64_t HashThreads(const ThreadSet& set);

/**
 * Text form, integers in plain decimal, one line per record:
 *
 *   pw_threads 1
 *   next <instance number>
 *   active <n>
 *   thread <id> <template> <stage> <openedAt> <stageEnteredAt> <now> <cursor> <firedMask> <people> <person>... <counts> <count>...
 *   resolved <n>
 *   done <id> <template> <resolution> <at> <people> <person>...
 *   end <u64 hash of the set>
 *
 * Serialising the same set always gives the same bytes.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseThreads(const ThreadSet& set);

/**
 * Strict inverse: nothing for truncation at any byte, unknown fields, wrong counts, unsorted,
 * repeated or out-of-range instances, a person repeated inside a thread, non-canonical numbers,
 * or a hash that does not match. Whether the set fits the templates is ThreadSetMatchesTemplates.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<ThreadSet> ParseThreads(std::string_view text);
} // namespace PerfectWorld
