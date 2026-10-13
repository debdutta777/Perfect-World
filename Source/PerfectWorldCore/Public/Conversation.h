#pragma once

#include <array>
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
 * Conversation (LANE_LIFE_IMPLEMENTATION rows 5.1 and 5.2, minimal slice; LANE_LIFE_PLAN section B "Conversation UX").
 *
 * The core decides which line a person speaks and what each choice costs; Unreal only shows it. This slice carries ONE
 * person, the household adult of the opening home (the first talkable adult), as an authored constexpr catalogue of nodes.
 *
 * Selection is a pure function of (minute of day, the remembered facts in the ConversationState, the person's availability):
 * SelectConversationNode. The catalogue is written so every combination selects exactly one node (the tests sweep all of
 * them). An Asleep or Busy person selects a polite decline node, which offers only Leave.
 *
 * A session lives in a ConversationState. Choosing applies the choice's declared time cost EXACTLY ONCE and returns it so
 * the integrator can charge it through the time contract (TimeContract.h) later; this module never touches a clock. A
 * costed choice, or one that sets a fact, is "once": it is recorded as taken, is no longer offered, and cannot be chosen
 * again, so leaving and reopening can neither re-charge nor re-award. Zero-cost, fact-free choices (greetings) are
 * repeatable and harmless. Leave is always offered, is free, and ends the session.
 *
 * Remembered facts persist across sessions in the state. The state has a strict text form ("pw.conversation/1"); embedding
 * it in a world save is the integrator's job (this module is standalone, not in WorldState, the reducer or the hash).
 *
 * The person's identity: neither the core nor the content names the household adult of the default Parent household yet,
 * so the role label "Your parent" and the id "person.household_adult" are used until the owner supplies a name.
 *
 * Integer arithmetic only. Nothing here throws.
 */
/** TODO(owner): name the household adult; no authored name exists in the core, the content or INDIA_WORLD_BIBLE. */
constexpr std::string_view HouseholdAdultId = "person.household_adult";
constexpr std::string_view HouseholdAdultLabel = "Your parent";

/** The choice every node offers. */
constexpr std::string_view ConversationLeaveChoiceId = "choice.leave";
constexpr std::string_view FactEveningRotaAgreed = "fact.evening_rota_agreed";
constexpr std::string_view FactEveningRotaKept = "fact.evening_rota_kept";

constexpr std::size_t ConversationMaxAuthoredChoices = 3;
constexpr std::int32_t ConversationMinutesPerDay = 1440;
constexpr std::int32_t ConversationMaxCostMinutes = 60;

enum class PersonAvailability : std::uint8_t
{
    Awake, Busy, Asleep
};
constexpr std::size_t PersonAvailabilityCount = 3;
/** "awake", "busy", "asleep". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view PersonAvailabilityId(PersonAvailability availability) noexcept;

struct ConversationChoiceDef
{
    /** Globally unique, "choice.". */
    std::string_view Id;
    std::string_view Text;
    /** Declared minutes charged once when chosen, 0..60. */
    std::int32_t CostMinutes = 0;
    /** Recorded as taken and then withdrawn. Required whenever the choice costs time or sets a fact. */
    bool bOnce = false;
    /** A fact id set on choosing, or empty. */
    std::string_view SetsFact;
    /** The node the session moves to; empty ends the session. */
    std::string_view NextNode;
};

struct ConversationNodeDef
{
    /** Globally unique, "node.". */
    std::string_view Id;
    std::string_view PersonId;
    /** What the person says. */
    std::string_view Line;
    /** The availability this node serves. */
    PersonAvailability Availability = PersonAvailability::Awake;
    /** The window [FromMinute, ToMinute) of the day it serves; a window with ToMinute <= FromMinute wraps past midnight. */
    std::int32_t FromMinute = 0;
    std::int32_t ToMinute = ConversationMinutesPerDay;
    /** Served only when this fact is set / not set. Empty means no condition. */
    std::string_view RequiresFact;
    std::string_view ForbidsFact;
    /** False for reply nodes, which are reached only through a choice. */
    bool bSelectable = true;
    /** A polite decline (asleep or busy): the session offers Leave only. */
    bool bDeclines = false;
    std::array<ConversationChoiceDef, ConversationMaxAuthoredChoices> Choices{};
    std::size_t ChoiceCount = 0;
};

/** The authored nodes (ConversationContent.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const ConversationNodeDef> ConversationNodes() noexcept;
/** The fact ids a choice may set. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const std::string_view> ConversationFactIds() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const ConversationNodeDef* FindConversationNode(std::string_view nodeId) noexcept;
/** The authored choice with this id, or null (Leave is not authored). */
[[nodiscard]] PERFECTWORLDCORE_API const ConversationChoiceDef* FindConversationChoice(std::string_view choiceId) noexcept;
/** True when the catalogue is well formed (ids, links, costs, once rules, a node for every situation). Tests call it. */
[[nodiscard]] PERFECTWORLDCORE_API bool ValidateConversationCatalogue() noexcept;

enum class ConversationError : std::uint8_t
{
    None, UnknownPerson, InvalidTime, AlreadyOpen, NotOpen, StaleChoice, AlreadyTaken
};
/** "none", "unknown_person", "invalid_time", "already_open", "not_open", "stale_choice", "already_taken". Stable. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ConversationErrorCode(ConversationError error) noexcept;

struct ConversationState
{
    bool Open = false;
    /** Empty unless a session is open. */
    std::string PersonId;
    std::string NodeId;
    /** Ids of the once-choices taken, ever, ascending. */
    std::vector<std::string> TakenChoices;
    /** Remembered fact ids, ascending. */
    std::vector<std::string> Facts;
    friend bool operator==(const ConversationState&, const ConversationState&) = default;
};

/** One row of the choice list the presenter shows. */
struct ConversationChoiceView
{
    std::string_view Id;
    std::string_view Text;
    std::int32_t CostMinutes = 0;
    bool bIsLeave = false;
};

struct ConversationChooseResult
{
    ConversationError Error = ConversationError::None;
    /** Minutes the integrator must charge, once. Zero for a refusal and for Leave. */
    std::int32_t CostMinutes = 0;
    /** The fact this choice newly set, or empty. */
    std::string_view FactSet;
    /** The node the session moved to; empty when it ended. */
    std::string_view NextNodeId;
    /** The session ended (Leave, or a choice with no next node). */
    bool bEnded = false;
    [[nodiscard]] bool Ok() const noexcept { return Error == ConversationError::None; }
};

struct ConversationStateParse
{
    ConversationState State;
    /** Empty on success; otherwise a short sentence and State is left default. */
    std::string Error;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

[[nodiscard]] PERFECTWORLDCORE_API bool HasConversationFact(const ConversationState& state, std::string_view factId) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool WasChoiceTaken(const ConversationState& state, std::string_view choiceId) noexcept;

/**
 * The node a person speaks now, or null for an unknown person or a minute outside 0..1439. Pure: it reads only the
 * state's facts, never whether a session is open.
 */
[[nodiscard]] PERFECTWORLDCORE_API const ConversationNodeDef* SelectConversationNode(std::string_view personId, std::int32_t minuteOfDay,
                                                                                    const ConversationState& state,
                                                                                    PersonAvailability availability) noexcept;

/** Opens a session at the selected node. Refused (state untouched) when one is already open. */
[[nodiscard]] PERFECTWORLDCORE_API ConversationError OpenConversation(ConversationState& state, std::string_view personId,
                                                                      std::int32_t minuteOfDay, PersonAvailability availability);
/** The choices to show for the open session: the authored ones not yet taken (a decline node has none), then Leave. Empty when no session is open. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<ConversationChoiceView> VisibleConversationChoices(const ConversationState& state);
/**
 * Takes a choice of the current node. A choice not offered now is StaleChoice; a once-choice taken before is AlreadyTaken.
 * On success the cost is returned once, the choice is recorded, its fact is set, and the session moves on or ends.
 * Every refusal leaves the state untouched. ConversationLeaveChoiceId ends the session at no cost.
 */
[[nodiscard]] PERFECTWORLDCORE_API ConversationChooseResult ChooseConversationChoice(ConversationState& state, std::string_view choiceId);
/** Ends the open session (free); facts and taken choices stay. NotOpen when none is open. */
[[nodiscard]] PERFECTWORLDCORE_API ConversationError LeaveConversation(ConversationState& state);

/** True when the state could have arisen from the operations above. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsConversationStateValid(const ConversationState& state) noexcept;
/** Strict text form. An inconsistent state serialises to the empty string. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseConversationState(const ConversationState& state);
/** Refuses a missing or wrong header, a truncated text, unknown or repeated or unsorted ids, an inconsistent state, and trailing data. */
[[nodiscard]] PERFECTWORLDCORE_API ConversationStateParse DeserialiseConversationState(std::string_view text);
} // namespace PerfectWorld
