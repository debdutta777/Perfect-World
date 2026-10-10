#pragma once

#include "Activities.h"
#include "GameClock.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Practice limits: the anti-grind rules of ROADMAP M2-04
 * (SCHOOL_AND_PROGRESSION "Anti-grind: the rolling practice window";
 * SYSTEMS_CATALOGUE "Attributes, skills and latent growth").
 *
 * A pure, integer-only module over a small plain state (PracticeState). It reads
 * no clock, owns no stats and is not yet a member of WorldState, the reducer,
 * the hash or WorldSave; the integration step calls RecordPractice from the
 * stats owner when `ActivityCompleted` arrives. It depends on Activities.h only
 * for SkillCredit, so that ActivityLearningCredits(def) can be passed straight in.
 *
 * Practice families. A family is a narrow kind of practice: the caller names it
 * with a stable ID ("family.endurance", "family.written_study"). Every activity
 * that is a route to the same practice names the same family, so the variants
 * share one window: "changing running routes does not reset endurance practice".
 * Which activity belongs to which family is content (the activity record will
 * carry it when the activity schema grows the field); this module takes the
 * family as input and never inspects an activity. A skill is not a family: one
 * family can feed several skills, and one skill can be fed by several families.
 *
 * The rolling window (per family). A session at instant T counts toward the
 * window of a proposed session at instant N when 0 <= N - T < PracticeWindowMinutes:
 * it expires exactly 24 hours after it began, so a session proposed at T + 1439
 * minutes still sees it and one at T + 1440 does not. Counting every earlier
 * session in the window, the proposed session earns
 *     none before it  -> Full (1000 permille)
 *     one before it   -> Half (500)
 *     two before it   -> Tenth (100)
 *     three or more   -> None (0): maintenance and output only, no new learning.
 * The rule slides: the window is always the 24 hours before the proposed
 * session, never a calendar day, so there is no midnight to game. A session that
 * earns nothing is still a session and still occupies the window (practising
 * more in a locked window does not unlock it sooner). Only the three most
 * recent sessions per family are kept, which is exact: the count is capped at
 * three, and if the third most recent session has expired every older one has.
 * The credit of each skill is its base milli-XP scaled by the tier (floor).
 * The label shown to the player: Full is "Useful practice", Half is
 * "Consolidation", and Tenth, None, or any session whose credits all came to
 * zero is "Little new learning".
 *
 * The per-skill budget. Whatever the families, one skill earns at most
 * PracticeSkillDailyBudgetMilli per game day (GameClock::DayNumber, midnight to
 * midnight), so twenty barely-different families cannot be cycled around the
 * window. The budget is spent by credit actually granted, after the tier; a
 * credit is cut to what remains. The budget is per skill and independent of the
 * family.
 *
 * Consolidation, "3-in-7". "Three useful sessions on separate days within seven
 * consolidate a technique. Missing Tuesday does not erase Monday." Here the
 * technique is the skill. A session is useful for a skill when its tier is Full
 * or Half and the skill's credit is above zero; a game day counts once however
 * many useful sessions it holds. A skill is consolidated at instant N when at
 * least PracticeConsolidationDays distinct useful days fall in the
 * PracticeConsolidationSpanDays days ending with N's own day (that is, seven
 * calendar days inclusive). It is a rolling status, not a streak and not a
 * counter: a missed or ill day removes nothing already earned, and nothing
 * resets; days simply age out of the window. The session that first makes a
 * skill consolidated reports it in `Consolidates` (the stats owner turns that
 * into a TechniqueConsolidated event and stamps ConsolidatedAt). Consolidation
 * grants no learning by itself: it is the technique recognised, not a bonus
 * that could be farmed. What a consolidated technique unlocks belongs to the
 * stats owner. Only the three most recent useful days per skill are kept,
 * which is exact for the same reason as above.
 *
 * Forecast equals credit. ForecastPractice and RecordPractice run one function;
 * RecordPractice returns the forecast it applied, so the label shown to the
 * player can never differ from what is granted. A refusal changes nothing.
 * Time never runs backwards: a session before the latest recorded one (of any
 * family) is refused.
 */

/** The window is a rolling 24 hours, in minutes. */
constexpr std::int64_t PracticeWindowMinutes = MinutesPerDay;
/** Credit by position in the window, in parts per thousand (SYSTEMS_CATALOGUE: credit 1000/500/100/0). */
constexpr std::int32_t PracticeFullPermille = 1000;
constexpr std::int32_t PracticeHalfPermille = 500;
constexpr std::int32_t PracticeTenthPermille = 100;
/** Sessions kept per family, and useful days kept per skill. */
constexpr std::size_t PracticeKeptSessions = 3;
/**
 * The most milli-XP one skill can earn in one game day. Not documented: the
 * docs say only "a per-skill daily budget" (to be tuned in tuning/growth.json).
 * Chosen at 8 XP so a full school day (five periods, 1200 milli to a subject
 * each) and a study block fit, while a day of cycling families does not.
 */
constexpr std::int32_t PracticeSkillDailyBudgetMilli = 8000;
/** Useful days that consolidate, and the days (inclusive) they must fall within. */
constexpr std::size_t PracticeConsolidationDays = 3;
constexpr std::int64_t PracticeConsolidationSpanDays = 7;
/** Not documented: bounds that keep a state and its text finite. */
constexpr std::size_t PracticeMaxFamilies = 256;
constexpr std::size_t PracticeMaxSkills = 1024;
constexpr std::size_t PracticeMaxIdLength = 128;
constexpr std::int32_t PracticeMaxBaseMilli = 1'000'000;

enum class PracticeTier : std::uint8_t
{
    Full,
    Half,
    Tenth,
    None
};

/** The plain forecast of the docs: "Useful practice / Consolidation / Little new learning". */
enum class PracticeLabel : std::uint8_t
{
    UsefulPractice,
    Consolidation,
    LittleNewLearning
};

/** The tier for a session with `sessionsInWindow` earlier sessions in its family's window. */
[[nodiscard]] PERFECTWORLDCORE_API PracticeTier PracticeTierForCount(std::size_t sessionsInWindow) noexcept;
/** The parts per thousand of the base a tier grants. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t PracticeTierPermille(PracticeTier tier) noexcept;
/** The label text shown to the player, for example "Useful practice". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view PracticeLabelText(PracticeLabel label) noexcept;
/** The tier's stable name, for example "half". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view PracticeTierName(PracticeTier tier) noexcept;

struct PracticeFamilyRecord
{
    std::string FamilyId;
    /** The most recent sessions, oldest first, at most PracticeKeptSessions, never empty. Equal instants are allowed. */
    std::vector<GameInstant> Recent;

    friend bool operator==(const PracticeFamilyRecord&, const PracticeFamilyRecord&) = default;
};

struct PracticeSkillRecord
{
    std::string SkillId;
    /** The game day (GameClock::DayNumber) the budget was last spent on. A different day means nothing spent. */
    std::int64_t BudgetDay = 0;
    std::int32_t BudgetUsedMilli = 0;
    /** The most recent days with a useful session, strictly ascending, at most PracticeKeptSessions. */
    std::vector<std::int64_t> UsefulDays;

    friend bool operator==(const PracticeSkillRecord&, const PracticeSkillRecord&) = default;
};

struct PracticeState
{
    /** Sorted by FamilyId, no duplicates. */
    std::vector<PracticeFamilyRecord> Families;
    /** Sorted by SkillId, no duplicates. */
    std::vector<PracticeSkillRecord> Skills;
    /** The latest instant any session was recorded. */
    GameInstant LastAt = 0;

    friend bool operator==(const PracticeState&, const PracticeState&) = default;
};

/** True when the state is internally consistent (sorted, well-formed IDs, bounded, no budget over the cap). */
[[nodiscard]] PERFECTWORLDCORE_API bool IsPracticeStateValid(const PracticeState& state);

/** One proposed or finished session: the family it practises, what it would teach, and when. */
struct PracticeSession
{
    /** A stable ID of the form [a-z0-9_.]+, such as "family.endurance". */
    std::string FamilyId;
    /** The activity's learning credits (ActivityLearningCredits). At least one, each above zero, no skill twice. */
    std::vector<SkillCredit> Base;
    GameInstant At = 0;
};

enum class PracticeError : std::uint8_t
{
    None = 0,
    InvalidState,
    InvalidSession,
    InvalidTime,
    TooMany
};

/** The stable rejection code, for example "practice.invalid_time". Empty for None. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view PracticeErrorCode(PracticeError error) noexcept;

struct PracticeForecast
{
    PracticeError Error = PracticeError::None;
    PracticeTier Tier = PracticeTier::Full;
    PracticeLabel Label = PracticeLabel::UsefulPractice;
    /** What each skill would be credited, in Base order, zeros included, after the tier and the budget. */
    std::vector<SkillCredit> Credit;
    /** True when the per-skill budget cut a credit below what the tier alone would give. */
    bool BudgetLimited = false;
    /** The skills this session would newly make consolidated, in Base order. */
    std::vector<std::string> Consolidates;

    [[nodiscard]] bool Ok() const noexcept { return Error == PracticeError::None; }

    friend bool operator==(const PracticeForecast&, const PracticeForecast&) = default;
};

/** How many sessions of the family lie in the 24 hours before `now`. Zero for an unknown family. */
[[nodiscard]] PERFECTWORLDCORE_API std::size_t PracticeSessionsInWindow(const PracticeState& state,
                                                                        std::string_view familyId, GameInstant now);
/** What is left of the skill's budget on the game day of `now`. */
[[nodiscard]] PERFECTWORLDCORE_API std::int32_t PracticeBudgetRemaining(const PracticeState& state,
                                                                        std::string_view skillId, GameInstant now);
/** True when at least PracticeConsolidationDays useful days fall in the seven days ending with `now`'s day. */
[[nodiscard]] PERFECTWORLDCORE_API bool PracticeIsConsolidated(const PracticeState& state, std::string_view skillId,
                                                               GameInstant now);

/** What `session` would earn. Changes nothing. */
[[nodiscard]] PERFECTWORLDCORE_API PracticeForecast ForecastPractice(const PracticeState& state,
                                                                     const PracticeSession& session);

/** Applies `session` and returns exactly the forecast it was applied under. A refusal changes nothing. */
PERFECTWORLDCORE_API PracticeForecast RecordPractice(PracticeState& state, const PracticeSession& session);

/**
 * The state as text, integers and stable IDs only, one record per line:
 *   pw.practice/1 / last_at=N / families=N / family=<id>,<t>[,<t>[,<t>]] ... /
 *   skills=N / skill=<id>,<budget_day>,<used>[,<day>...] ... / end
 * Each line ends in a newline, and the same state always gives the same text.
 * Empty for an inconsistent state.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialisePractice(const PracticeState& state);

struct PracticeParse
{
    /** Empty on success; otherwise why the text was refused. */
    std::string Error;
    PracticeState State;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** Reads SerialisePractice's text. Refuses truncation, extra text, malformed numbers and inconsistent state. */
[[nodiscard]] PERFECTWORLDCORE_API PracticeParse DeserialisePractice(std::string_view text);
} // namespace PerfectWorld
