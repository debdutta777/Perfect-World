#pragma once

#include "Commands.h"
#include "SchoolWeek.h"
#include "WorldState.h"
#include "WorldCommerce.h"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
struct Rejection
{
    /** Stable, machine-readable, e.g. "time.non_positive". */
    std::string Code;
    std::string Message;
};

struct CommandResult
{
    bool Accepted = false;
    Rejection Why;
    std::uint64_t Seq = 0;
    GameInstant From = 0;
    GameInstant To = 0;
    /**
     * The event cap was reached before the target; submit the remaining time again (To is where it stopped). Events
     * still due at To fire first on that continuation, or through time.settle (SettleDueEvents) until IsWorldQuiescent.
     */
    bool Partial = false;
    std::vector<DomainEvent> Events;
};

/** Scheduled events one advance may fire before it stops and reports Partial. */
constexpr std::size_t MaxEventsPerAdvance = 10'000;

struct SchoolLessonContent;
struct SchoolWorkBriefCatalogue;

/**
 * S04: the school content the school.lesson.*, school.work.apply and school.week.* commands borrow. Immutable, owned by the
 * caller and alive as long as the sim; bind the same content on load and replay. The lesson catalogue resolves a command's
 * BindingId (and gives the week its stream's bindings); the bell schedule and calendar stay the sim's DayLoopCatalogues, so
 * the session catalogues are always {school day catalogues, Structures}. A missing piece refuses "school.missing_content".
 */
struct WorldSchoolCatalogues
{
    AssessmentStructures Structures;
    const SchoolLessonContent* Lessons = nullptr;
    const AssessmentCatalogue* Papers = nullptr;
    /**
     * P3-06: the authored homework briefs (LoadSchoolWorkBriefContent). When bound, every accepted command issues the briefs
     * of this school's calendar and stream whose scheduled boundary lies in the interval the clock actually moved (inclusive
     * at both ends), once each, into State().SchoolWork (IssueScheduledSchoolWork). It needs the day catalogues' calendar
     * and bell schedule and Papers; a refusal refuses the whole command. Unbound (null) issues nothing, as before; Phase 3
     * production must bind it, on load and replay too. Nothing ever issues on a read.
     */
    const SchoolWorkBriefCatalogue* WorkBriefs = nullptr;
};

/**
 * R9-04: what the Awakening commands and the countdown boundary borrow. Recipes are the validated ability recipes the
 * supervised grant resolves with (LoadAbilityRecipes), owned by the caller and alive as long as the sim. Countdown is the
 * trusted countdown context (the player's identity, the relationship book and the people by role), never a presentation
 * payload. The countdown is ROUTED only in a sim whose Countdown.Player is set and whose world's Awakening is seeded: such a
 * sim stops every time path at 18:00 on Awakening Day and opens the countdown there (OpenCountdownAtCurrentTime), and
 * synchronises an open countdown after every accepted command (SynchroniseWorldCountdown). Nested helper sims (week
 * compression, the countdown adapters) bind none of this, so they never open, synchronise or grant.
 */
struct WorldAwakeningCatalogues
{
    const AbilityRecipeSet* Recipes = nullptr;
    CountdownContext Countdown;
};

/** Presentation-free school gate projection. All minute fields are local minutes of today's day. */
struct SchoolArrivalView
{
    SchoolDayKind DayKind = SchoolDayKind::OutsideYear;
    bool HasSchool = false;
    bool CanArrive = false;
    std::int32_t GatesOpenMinute = 0;
    /** Inclusive on-time window: GatesOpenMinute through FirstBellMinute. */
    std::int32_t FirstBellMinute = 0;
    std::int32_t DismissalMinute = 0;
    /** Holiday/break ID when present. No translated presentation text. */
    std::string ClosureId;
    /** Forecast assumes staying until dismissal. Recorded is read solely from DayLoop.Attendance. */
    std::optional<AttendanceForecast> Forecast;
    std::optional<AttendanceDay> Recorded;
    std::string ReasonTextKey;
    Rejection Refusal;
};

/** Pure; requires matching validated bell/calendar content, never falls back to weekdays for missing content. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolArrivalView QuerySchoolArrival(const WorldState& state,
                                                                        const DayLoopCatalogues& school);

/**
 * The command reducer (SYSTEMS.md §2.2, ROADMAP M1-01). Submit is the only
 * mutation entry point: validate (pure; a rejection changes nothing), apply,
 * advance time event by event, commit. Unreal actors submit commands and
 * present the returned events; they never change state themselves.
 */
class PERFECTWORLDCORE_API WorldSim
{
public:
    /**
     * `eventCap` exists for tests; the game always uses MaxEventsPerAdvance.
     * School catalogues are borrowed, immutable and must outlive the sim; bind the same content on load/replay.
     */
    explicit WorldSim(std::uint64_t worldSeed, std::size_t eventCap = MaxEventsPerAdvance,
                      DayLoopCatalogues school = {}, WorldCommerceCatalogues commerce = {},
                      WorldSchoolCatalogues lessons = {}, WorldAwakeningCatalogues awakening = {});
    explicit WorldSim(std::uint64_t worldSeed, const CharacterCreator& creation,
                      std::size_t eventCap = MaxEventsPerAdvance, DayLoopCatalogues school = {},
                      WorldCommerceCatalogues commerce = {}, WorldSchoolCatalogues lessons = {},
                      WorldAwakeningCatalogues awakening = {});
    /**
     * Continue a loaded world (DeserializeWorldSave's World). The state must be in step
     * (IsWorldDayInStep); the command log starts empty, so it holds the commands since the load.
     */
    explicit WorldSim(WorldState state, std::size_t eventCap = MaxEventsPerAdvance, DayLoopCatalogues school = {},
                      WorldCommerceCatalogues commerce = {}, WorldSchoolCatalogues lessons = {},
                      WorldAwakeningCatalogues awakening = {});

    /**
     * The only mutation entry point. Atomic: a rejected command, including a paid conversation choice whose charge
     * could not complete, leaves the state exactly as it was.
     */
    CommandResult Submit(const Command& command);
    const WorldState& State() const noexcept { return State_; }
    /** Accepted commands in order: the replay log (ROADMAP M1-03). */
    const std::vector<Command>& Log() const noexcept { return Log_; }
    [[nodiscard]] SchoolArrivalView SchoolArrivalToday() const { return QuerySchoolArrival(State_, School_); }
    [[nodiscard]] CommerceServiceView CommerceService(std::string_view shopId) const;
    [[nodiscard]] CommerceQuote QuoteCommerce(std::string_view shopId, std::string_view itemId, std::int32_t quantity) const;
    /** Current keeper decision, including released service windows. Presentation uses this for work resumption. */
    [[nodiscard]] NpcDecision CommerceKeeperDecision(std::string_view shopId) const;
    /** S04: {the bound school day catalogues, the borrowed structure catalogues}: what every school read must use. */
    [[nodiscard]] SchoolSessionCatalogues SchoolCatalogues() const { return {School_, Lessons_.Structures}; }
    [[nodiscard]] const WorldSchoolCatalogues& SchoolContent() const noexcept { return Lessons_; }
    /**
     * S04: the actual result of the last ACCEPTED school.lesson.*, school.work.apply and school.week.resolve command, for the
     * panels' summaries and the planner recap. Session output only: never saved, empty after a load, and a refused command
     * leaves the previous value. No result is ever rebuilt from state.
     */
    [[nodiscard]] const std::optional<SchoolSessionResult>& LastSchoolLessonResult() const noexcept { return LastLesson_; }
    [[nodiscard]] const std::optional<SchoolWorkResult>& LastSchoolWorkResult() const noexcept { return LastWork_; }
    [[nodiscard]] const std::optional<SchoolWeekResult>& LastSchoolWeekResult() const noexcept { return LastWeek_; }
    /** R9-04: the bound Awakening content and countdown context. */
    [[nodiscard]] const WorldAwakeningCatalogues& AwakeningContent() const noexcept { return Awakening_; }
    /** R9-04: the stored countdown (ParseCountdown of State().CountdownText), empty before the 18:00 opening. */
    [[nodiscard]] std::optional<CountdownState> Countdown() const;
    /** R9-04: the phone/handoff projection (QueryWorldCountdown) over the stored Awakening and countdown. Pure. */
    [[nodiscard]] WorldCountdownView CountdownView() const;
    /**
     * R9-04: the disclosure for optional supervised practice starting now (PreviewWorldCountdownPractice) with the bound
     * context and the day's catalogues. Reading it changes nothing; confirm by sending CountdownPracticeConfirm with it.
     */
    [[nodiscard]] WorldCountdownPracticeOffer PreviewCountdownPractice(PowerDimension dimension, std::int32_t minutes) const;
    /** R9-04: the Practise result of the last ACCEPTED countdown.practice.confirm. Session output only, never saved. */
    [[nodiscard]] const std::optional<PracticeResult>& LastCountdownPracticeResult() const noexcept { return LastPractice_; }
    /** P37: last accepted routine countdown resolution; session output only, never saved. */
    [[nodiscard]] const std::optional<CountdownResolveResult>& LastCountdownResolveResult() const noexcept { return LastCountdown_; }

private:
    std::optional<Rejection> Validate(const Command& command) const;
    /**
     * A paid conversation choice whose charge the event cap stopped short restores its rollback snapshot.
     * Free school commands stage their day-loop copy before replacing state, so their refusals need no snapshot.
     */
    std::optional<Rejection> Apply(const Command& command, CommandResult& result);
    bool NeedsRollbackSnapshot(const Command& command) const;
    /**
     * Moves the clock to `target` event by event: due-now events first, midnights before same-instant events,
     * the day loop kept in step at every boundary. Stops (Partial) when the event cap is spent before the target,
     * at the instant of the last fired event; everything unfired stays pending (events still due there fire first
     * on the continuation).
     */
    void RunTimeTo(GameInstant target, CommandResult& result);
    /** RunTimeTo(Now + minutes) with its TimeAdvanced placed before the events it caused. Partial when capped. */
    void ChargeTime(std::int64_t minutes, CommandResult& result);
    /** The checks every time-moving command shares (activity running, asleep, calendar end). */
    std::optional<Rejection> ValidateTimeCharge(std::int64_t minutes) const;
    // Conversation and door commands, in WorldConversation.cpp.
    std::optional<Rejection> ValidateLaneCommand(const Command& command) const;
    std::optional<Rejection> ApplyLaneCommand(const Command& command, CommandResult& result);
    // Free planning and arrival only, in WorldSchoolDay.cpp; application commits a successful day-loop copy.
    std::optional<Rejection> ValidateSchoolCommand(const Command& command) const;
    std::optional<Rejection> ApplySchoolCommand(const Command& command, CommandResult& result);
    // WorldCommerce.cpp: stage commerce and service inputs before commit; no persisted service context.
    std::optional<Rejection> ApplyCommerceCommand(const Command& command);
    std::optional<Rejection> ApplyWorldDayCommand(const WorldDayCommand& command, CommandResult& result);
    // S04 school.lesson.* (WorldSchool.cpp) and school.work.apply / school.week.* (WorldSchoolWeek.cpp). Each stages copies,
    // charges time through ChargeTime and commits only on success; Submit's snapshot restores a late refusal.
    std::optional<Rejection> ValidateSchoolPhase2(const Command& command) const;
    std::optional<Rejection> ApplySchoolLessonCommand(const Command& command, CommandResult& result);
    std::optional<Rejection> ApplySchoolWorkCommand(const SchoolWorkCommand& command, CommandResult& result);
    std::optional<Rejection> ApplySchoolWeekCommand(const Command& command, CommandResult& result);
    /** P3-06: issue the bound briefs reached in [from, Now] (WorldSchoolWeek.cpp); nothing when no catalogue is bound. */
    std::optional<Rejection> IssueReachedSchoolWork(GameInstant from);
    // R9-04 tutorial.* and countdown.practice.confirm (WorldAwakening.cpp) and the countdown boundary (WorldCountdownRoute.cpp).
    std::optional<Rejection> ValidateAwakeningCommand(const Command& command) const;
    std::optional<Rejection> ApplyAwakeningCommand(const Command& command, CommandResult& result);
    std::optional<Rejection> ApplyCountdownPractice(const CountdownPracticeConfirm& command, CommandResult& result);
    // P37: the remaining Countdown.h commands, staged with an outer clock charge.
    std::optional<Rejection> ValidateCountdownCommand(const Command& command) const;
    std::optional<Rejection> ApplyCountdownCommand(const Command& command, CommandResult& result);
    /** Derive compulsory supervision boundaries from existing saved appointment/make-up state. */
    [[nodiscard]] GameInstant TutorialTimeBoundary(GameInstant target) const;
    bool StopForTutorialBoundary(GameInstant target, CommandResult& result);
    /** Exit the ReadyToGrant session: charge the disclosed hour once, commit the grant and start the live profile. */
    std::optional<Rejection> FinishTutorial(CommandResult& result);
    /** True when this sim routes the countdown for its world (seeded Awakening and a bound countdown context). */
    [[nodiscard]] bool CountdownRouted() const noexcept;
    /** At exactly 18:00 on Awakening Day with no countdown yet: open it (R95OpenBoundary). A refusal leaves State_ as it was. */
    std::optional<Rejection> OpenCountdownBoundary(CommandResult& result);
    /** After the final day and allowance settlement: synchronise an open countdown (R95SyncSnapshot). */
    std::optional<Rejection> SyncCountdown(CommandResult& result);
    /** R9 growth: credit the completions once to a staged copy of the one owner and commit it (seeded worlds only). */
    std::optional<Rejection> CreditActivities(std::span<const DayLoopEvent> events);

    WorldState State_;
    std::vector<Command> Log_;
    std::size_t EventCap_ = MaxEventsPerAdvance;
    DayLoopCatalogues School_;
    WorldCommerceCatalogues Commerce_;
    WorldSchoolCatalogues Lessons_;
    WorldAwakeningCatalogues Awakening_;
    /** R9-04: a countdown boundary refusal inside RunTimeTo, turned into the command's late refusal by Submit. */
    std::optional<Rejection> Boundary_;
    std::optional<SchoolSessionResult> StagedLesson_, LastLesson_;
    std::optional<SchoolWorkResult> StagedWork_, LastWork_;
    std::optional<SchoolWeekResult> StagedWeek_, LastWeek_;
    std::optional<PracticeResult> StagedPractice_, LastPractice_;
    std::optional<CountdownResolveResult> StagedCountdown_, LastCountdown_;
    std::string ServiceShop_;
    std::map<std::string, std::vector<NpcInterruptionWindow>, std::less<>> ServiceHistory_;
};

/** True for the conversation.* and door.* commands (WorldConversation.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsLaneCommand(const CommandPayload& payload) noexcept;

/** S04: true for school.lesson.*, school.work.apply and school.week.* (WorldSchool.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsSchoolPhase2Command(const CommandPayload& payload) noexcept;

/**
 * S04: why the world cannot take a time-moving or unrelated player command while school play is open: a lesson session
 * ("school.lesson_open") or a started, unsent paper ("school.paper_open"); empty when neither is open.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<Rejection> SchoolPlayOpenRefusal(const WorldState& state);

/** R9-04: true for tutorial.* and countdown.practice.confirm (WorldAwakening.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsAwakeningCommand(const CommandPayload& payload) noexcept;

/** P37: countdown opportunity, negotiation and routine day commands. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsCountdownCommand(const CommandPayload& payload) noexcept;

/**
 * R9-04: "tutorial.active" while the supervised session is Active or ReadyToGrant: the world clock is paused there, so time,
 * the day, conversations, commerce and school play wait until the session exits. Empty otherwise.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<Rejection> TutorialOpenRefusal(const WorldState& state);
} // namespace PerfectWorld
