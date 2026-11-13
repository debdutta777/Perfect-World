#pragma once

#include "DayLoop.h"
#include "GameClock.h"
#include "Relationships.h"
#include "WorldState.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The authored content of the ten days (ROADMAP M4-03): the span, the six preparation dimensions,
 * opportunities, obligations and the world event as plain-data definitions, with the tables'
 * accessors and the rules that keep them sound. The behaviour is in Countdown.h.
 */
/** The span, in calendar days (CalendarAwakeningToExamDays). */
constexpr std::int32_t CountdownDays = 10;
/** The order id under which hours left unspent at the end are spent awake. */
inline constexpr std::string_view CountdownPadOrderId = "order.countdown_to_exam_morning";
constexpr std::size_t CountdownMaxCommitments = 16;
constexpr std::size_t CountdownMaxObligations = 8;
constexpr std::size_t CountdownMaxLedgerLines = 256;
constexpr std::size_t CountdownMaxRequests = 64;

/** Each score is on 0..CountdownScoreMax. */
constexpr std::int32_t CountdownScoreMax = 1000;
/** Goodwill before anything happens. */
constexpr std::int32_t CountdownGoodwillStart = 500;
/** The most the day loop's learning credits can add to any one preparation dimension. */
constexpr std::int32_t CountdownDayLoopPrepCap = 300;
/** Two strategies are comparable when their totals are within this many permille of the larger. */
constexpr std::int32_t CountdownComparableSpreadPermille = 250;
/** Two strategies differ when their eight scores are at least this far apart in total (sum of absolute differences). */
constexpr std::int32_t CountdownDistinctDistance = 150;

/** The six preparation dimensions, one per tower floor type (AWAKENING_AND_EXAM.md "Floors are not all combat"). */
enum class CountdownDimension : std::uint8_t
{
    Combat,
    Analysis,
    Endurance,
    Construction,
    Negotiation,
    Logistics
};
constexpr std::size_t CountdownDimensionCount = 6;
using CountdownPoints = std::array<std::int32_t, CountdownDimensionCount>;

/** "combat", "analysis", "endurance", "construction", "negotiation", "logistics". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownDimensionName(CountdownDimension dimension) noexcept;

enum class CountdownOutcome : std::uint8_t
{
    Good,
    Fair,
    Poor
};
constexpr std::size_t CountdownOutcomeCount = 3;
/** "good", "fair", "poor". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownOutcomeName(CountdownOutcome outcome) noexcept;

/** Probability of each outcome in permille, in CountdownOutcome order; always sums to 1000. */
using CountdownOdds = std::array<std::int32_t, CountdownOutcomeCount>;

enum class CountdownBand : std::uint8_t
{
    Unlikely,
    Possible,
    Likely,
    VeryLikely
};
/** "unlikely" (under 250), "possible" (under 500), "likely" (under 800), "very_likely". */
[[nodiscard]] PERFECTWORLDCORE_API CountdownBand CountdownBandOf(std::int32_t permille) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownBandName(CountdownBand band) noexcept;

/** What one outcome does. */
struct CountdownEffect
{
    CountdownPoints Preparation{};
    /** Added to composure. */
    std::int32_t Composure = 0;
    /** Added to goodwill. */
    std::int32_t Goodwill = 0;
    /** Minutes taken off the examination-morning delay (the route consequence). */
    std::int32_t RouteMinutes = 0;

    friend bool operator==(const CountdownEffect&, const CountdownEffect&) = default;
};

/** Which dimension of the relationship counts. */
enum class CountdownRelation : std::uint8_t
{
    Trust,
    Warmth,
    Respect
};

/** The time an opportunity or obligation fills: an activity from the catalogue, or time spent. */
struct CountdownBlockDef
{
    /** Empty for time spent. */
    std::string_view ActivityId;
    std::string_view PlaceKind;
    /** Activity: must equal the catalogue's duration. Spend: the minutes. */
    std::int32_t Minutes = 0;
    DayPassKind Pass = DayPassKind::Wait;
};

struct CountdownOpportunityDef
{
    std::string_view Id;
    std::string_view Name;
    /** At most one opportunity per group may be taken. */
    std::string_view Group;
    /** Calendar day of the span (0 is Awakening Day, 9 the last) and minute of that day. */
    std::int32_t Day = 0;
    std::int32_t StartMinute = 0;
    CountdownBlockDef Block;
    /** The contact's role; empty when anyone may take it. */
    std::string_view Role;
    CountdownRelation Relation = CountdownRelation::Trust;
    /** The relationship needed to be offered it. */
    std::int32_t MinRelation = 0;
    CountdownOdds BaseOdds{};
    /** Permille moved from poor to good per full 10,000 of relationship above the minimum, and the most that can move. */
    std::int32_t SkewStep = 0;
    std::int32_t SkewCap = 0;
    /** In CountdownOutcome order. */
    std::array<CountdownEffect, CountdownOutcomeCount> Effects{};
    /** Offered only once the world event has fired. */
    bool NeedsNews = false;
    /** The act asked of the relationship system when it resolves good or fair with a contact. */
    RelationshipActKind Act = RelationshipActKind::ShareTime;
};

enum class CountdownProposal : std::uint8_t
{
    Honour,
    Share,
    Defer,
    Decline
};
constexpr std::size_t CountdownProposalCount = 4;
/** "honour", "share", "defer", "decline". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view CountdownProposalName(CountdownProposal proposal) noexcept;

struct CountdownObligationDef
{
    std::string_view Id;
    std::string_view Name;
    /** The person owed; the obligation exists only when the context has a contact in this role. */
    std::string_view Role;
    std::int32_t Day = 0;
    std::int32_t StartMinute = 0;
    std::int32_t Minutes = 0;
    DayPassKind Pass = DayPassKind::Walk;
    /** Standing gained when it is honoured. */
    std::int32_t HonourGoodwill = 0;
    /** Standing lost when it is declined (certain, no roll). */
    std::int32_t DeclineGoodwill = 0;
    /** The relationship that shapes whether a share or a deferral is accepted, and the odds. */
    CountdownRelation Relation = CountdownRelation::Trust;
    std::int32_t SkewStep = 0;
    std::int32_t SkewCap = 0;
    /** Deferral: the new slot, the odds (permille) of acceptance, and the standing cost when accepted and when refused. */
    std::int32_t DeferDay = 0;
    std::int32_t DeferMinute = 0;
    std::int32_t DeferBase = 0;
    std::int32_t DeferAcceptedGoodwill = 0;
    std::int32_t DeferRefusedGoodwill = 0;
    /** Sharing: the helper's role and the relationship they must have, the minutes left to the player, the odds, the standing costs. */
    std::string_view HelperRole;
    std::int32_t HelperMinRelation = 0;
    std::int32_t ShareMinutes = 0;
    std::int32_t ShareBase = 0;
    std::int32_t ShareAcceptedGoodwill = 0;
    std::int32_t ShareRefusedGoodwill = 0;
};

/** The one scheduled world event: a Gate break whose onset is fixed. */
struct CountdownWorldEventDef
{
    /** The Gate system's id, e.g. "gate.kolkata_canal_ward9". */
    std::string_view GateId;
    std::string_view NewsId;
    std::string_view Name;
    GateRank Rank = GateRank::E;
    std::int32_t Day = 0;
    std::int32_t Minute = 0;
    /** Minutes the closure would add to the examination-morning journey with nothing done about it. */
    std::int32_t BaseDelayMinutes = 0;
    /** One minute off the delay per this many logistics preparation points, and the most that can take off. */
    std::int32_t LogisticsPointsPerMinute = 1;
    std::int32_t LogisticsMaxMinutes = 0;
    /** Composure lost per this many remaining delay minutes (times two thirds), up to this many points. */
    std::int32_t RouteComposureCap = 0;
};

[[nodiscard]] PERFECTWORLDCORE_API std::span<const CountdownOpportunityDef> CountdownOpportunities() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::span<const CountdownObligationDef> CountdownObligations() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const CountdownWorldEventDef& CountdownWorldEvent() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const CountdownOpportunityDef* CountdownFindOpportunity(std::string_view id) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const CountdownObligationDef* CountdownFindObligation(std::string_view id) noexcept;

/** The span: 18:00 on Awakening Day and 06:00 on examination morning; the instant of a day of the span and minute. */
[[nodiscard]] PERFECTWORLDCORE_API GameInstant CountdownStartInstant() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API GameInstant CountdownEndInstant() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API GameInstant CountdownInstantOf(std::int32_t day, std::int32_t minute) noexcept;

/**
 * Every rule the tables must meet; empty when sound. Ids and roles are fair stable tokens and unique; each
 * window lies inside one calendar day of the span and after its start; odds are non-negative and sum to
 * 1000; effects are in range and only responses change the route; activity blocks match the real catalogue's
 * id, place and duration; the response group starts after the news and has at least two members; a
 * deferral is later than the original slot and a share leaves fewer minutes; the world event lies inside the span.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateCountdownContent(const ActivityCatalogue& activities);
} // namespace PerfectWorld
