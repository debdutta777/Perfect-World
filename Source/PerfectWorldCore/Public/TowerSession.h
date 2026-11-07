#pragma once

#include "SchoolSessions.h"
#include "Tower.h"

namespace PerfectWorld
{
struct CountdownProfile;
inline constexpr std::uint32_t TowerSessionContractVersion = 1;
inline constexpr std::size_t TowerRunnerBeatLimit = 16;
inline constexpr std::size_t TowerRunnerChoiceLimit = 8;
/** A7 authors exactly one scenario for each existing Tower route, with 1..16 beats.
 * All methods allowed by that route share its scoring envelope. These provisional
 * tuning bounds are core authority, never widget-provided metrics. */
struct TowerRunnerChoice
{
    std::string Id;
    std::string TextKey;
    std::int32_t MissionMinutes = 1; // 1..60 before preparation
    std::int32_t Strain = 0; // 0..4; insufficient reserve means terminal defeat
    std::int32_t Recovery = 0; // 0..4 after paying strain
    std::int32_t DisciplineMilli = 0; // 0..1000; authored evidence, not an input command
    /** Must describe non-combat judgment, even for a combat route. */
    std::int32_t CommonJudgmentMilli = 0; // 0..1000
};
struct TowerRunnerBeat
{
    std::string Id;
    std::string PromptTextKey;
    std::vector<std::string> ClueTextKeys;
    std::vector<TowerRunnerChoice> Choices;
};
struct TowerRunnerScenario
{
    std::string Id;
    std::string RouteId;
    std::string TitleTextKey;
    std::vector<TowerRunnerBeat> Beats;
};
struct TowerRunnerCatalogue { std::vector<TowerRunnerScenario> Scenarios; };
enum class TowerParticipation : std::uint8_t { Unset, Entered, Completed, Ejected, Withdrawn, Deferred, Declined };
struct TowerRunnerConditions
{
    std::int32_t Reserve = 4; // base 4 + up to 4 preparation + up to 2 composure
    std::int32_t MinuteDiscount = 0; // 0..2; each choice still costs >=1 minute
    friend bool operator==(const TowerRunnerConditions&, const TowerRunnerConditions&) = default;
};
struct TowerRunnerRun
{
    std::string ScenarioId;
    std::string RouteId;
    TowerMethod Method = TowerMethod::Analysis;
    std::size_t Beat = 0;
    TowerRunnerConditions Conditions;
    std::int32_t DisciplineSum = 0;
    std::int32_t JudgmentSum = 0;
    std::int64_t MissionMinutes = 0;
    std::vector<std::string> Choices;
    friend bool operator==(const TowerRunnerRun&, const TowerRunnerRun&) = default;
};
/** Owned strings: unlike TowerFloorAttempt this is safe to serialize and reload. */
struct TowerSessionAttempt
{
    std::string RouteId;
    TowerMethod Method = TowerMethod::Analysis;
    std::int32_t DisciplineMilli = 0;
    std::int32_t CommonJudgmentMilli = 0;
    std::int64_t MissionMinutes = 0;
    bool Defeated = false;
    /** Completed-floor provenance lets the codec replay authored choices instead of
     * trusting stored numeric scores. Defeat keeps the terminal prefix too. */
    std::string ScenarioId;
    std::vector<std::string> ChoiceIds;
    friend bool operator==(const TowerSessionAttempt&, const TowerSessionAttempt&) = default;
};
/** Immutable copy of the runner-relevant final CountdownProfile fields. This is
 * a snapshot, not another preparation/growth owner; Countdown remains authority.
 * Keeping it independent of Countdown.h permits WorldState to own this slice. */
struct TowerPreparationSnapshot
{
    std::array<std::int32_t, TowerMethodCount> Preparation{};
    std::int32_t Composure = 0;
    std::int32_t RouteDelayMinutes = 0;
    friend bool operator==(const TowerPreparationSnapshot&, const TowerPreparationSnapshot&) = default;
};
struct TowerSessionState
{
    TowerParticipation Participation = TowerParticipation::Unset;
    /** Frozen examination-morning preparation; never recalculate from current stats. */
    TowerPreparationSnapshot Preparation;
    std::uint64_t NextRunner = 1;
    std::uint64_t Runner = 0;
    std::optional<TowerRunnerRun> Active;
    std::vector<TowerSessionAttempt> Attempts;
    /** Route travel delay is mission time, charged at final exit, never moves deadline. */
    friend bool operator==(const TowerSessionState&, const TowerSessionState&) = default;
};
enum class TowerSessionAction : std::uint8_t { Enter, OpenRoute, Choose, Withdraw, Defer, Decline };
struct TowerSessionCommand
{
    static constexpr std::string_view TypeId = "tower.session.apply";
    TowerSessionAction Kind = TowerSessionAction::Enter;
    std::string RouteId;
    TowerMethod Method = TowerMethod::Analysis;
    /** Choose must match both runner nonce and beat, defeating duplicate/stale callbacks. */
    std::uint64_t Runner = 0;
    std::size_t Beat = 0;
    std::string ChoiceId;
};
struct TowerSessionView
{
    TowerParticipation Participation = TowerParticipation::Unset;
    std::int32_t NextFloor = 1;
    std::uint64_t Runner = 0;
    std::size_t Beat = 0;
    TowerRunnerConditions Conditions;
    std::string PromptTextKey;
    std::vector<std::string> ClueTextKeys;
    /** UI receives costs and IDs, never authored scoring answers. */
    struct Choice { std::string Id; std::string TextKey; std::int32_t MissionMinutes = 0; std::int32_t Strain = 0; };
    std::vector<Choice> Choices;
    std::vector<std::string> AvailableRouteIds;
    TowerResult Result;
};
struct TowerSessionResult
{
    SchoolIssue Issue;
    /** Nonzero only on first terminal command. Reducer charges the mission's
     * outside-world cost atomically with terminal state; UI/pause/reading cost zero. */
    std::int64_t OutsideMinutes = 0;
    TowerResult Result;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
[[nodiscard]] PERFECTWORLDCORE_API SchoolIssue ValidateTowerRunnerCatalogue(const TowerRunnerCatalogue& catalogue);
[[nodiscard]] PERFECTWORLDCORE_API TowerRunnerConditions TowerConditionsFromPreparation(const CountdownProfile& preparation,
    TowerMethod method);
/** preparation and now come from authority; Enter requires final preparation and
 * now >= CountdownEndInstant(). Defer/Decline need no awakening/profile. */
PERFECTWORLDCORE_API TowerSessionResult ApplyTowerSession(TowerSessionState& state,
    const TowerRunnerCatalogue& catalogue, const TowerSessionCommand& command,
    const CountdownProfile& preparation, GameInstant now);
[[nodiscard]] PERFECTWORLDCORE_API TowerSessionView QueryTowerSession(const TowerSessionState& state,
    const TowerRunnerCatalogue& catalogue);
} // namespace PerfectWorld
