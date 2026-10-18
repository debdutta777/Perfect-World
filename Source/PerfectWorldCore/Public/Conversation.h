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
 * Conversation (LANE_LIFE_IMPLEMENTATION rows 5.1 and 5.2; LANE_LIFE_PLAN section B "Conversation UX").
 *
 * The core decides which line a person speaks and what each choice costs; Unreal only shows it. The runtime is general:
 * content arrives as a ConversationCatalogue, a span of nodes (the people's authored lines and choices) and a span of fact
 * definitions. Every function has a form taking the catalogue and a form without it, which uses DefaultConversationCatalogue()
 * (today the household adult; the integrator composes further catalogues later). Nothing in this file names a person or a fact.
 *
 * Selection is a pure function of (person, minute of day, the remembered facts, the person's availability):
 * SelectConversationNode. A catalogue is valid only when every person selects exactly one node in every situation that can
 * arise (each availability, each minute, each consistent set of the facts the person's nodes look at); ValidateConversationCatalogue
 * sweeps them from the catalogue's own data, with no fact combination written down here. An Asleep or Busy person selects a
 * polite decline node, which offers only Leave.
 *
 * Costs. Choosing returns the choice's declared minutes (CostMinutes) EXACTLY ONCE per accepted choice, for the integrator to
 * charge through the time contract; this module never touches a clock. A paid choice may recur (a household can talk about the
 * evening jobs again tomorrow), so duplicates are stopped by a per-session TRANSACTION RECEIPT, not by exhausting the topic:
 * every OpenConversation starts a new session (SessionCount + 1) and a paid choice records a receipt (choice id + session);
 * the same choice is refused (AlreadyCharged) and hidden for the rest of that session, and is offered afresh in the next one.
 * A choice flagged bOnceEver is for the genuinely one-time (it is recorded in TakenChoices and never offered again).
 * Free choices need no receipt and are always repeatable. Leave is always offered, is free, and ends the session.
 *
 * Facts persist across sessions. A fact definition may name a Group (setting one fact clears the others of its group, so
 * an agreement is replaced, not stacked) and a fact it Implies (setting the fact also sets that one). A choice may set a fact,
 * clear a fact, and be shown only while a fact is set or not set.
 *
 * Text form: "pw.conversation/2". "pw.conversation/1" (no session counter, no receipts, every costed choice recorded as taken)
 * is still read and migrated honestly: see DeserialiseConversationState. Embedding the text in a world save is the
 * integrator's job (this module is standalone, not in WorldState, the reducer or the hash).
 *
 * The household adult's identity: neither the core nor the content names the household adult of the default Parent household
 * yet, so the role label "Your parent" and the id "person.household_adult" are used until the owner supplies a name.
 *
 * Integer arithmetic only. Nothing here throws.
 */
/** TODO(owner): name the household adult; no authored name exists in the core, the content or INDIA_WORLD_BIBLE. */
constexpr std::string_view HouseholdAdultId = "person.household_adult";
constexpr std::string_view HouseholdAdultLabel = "Your parent";

/** The choice every node offers. */
constexpr std::string_view ConversationLeaveChoiceId = "choice.leave";
/** The evening jobs, remembered by the household adult. Agreed and Kept date from the first slice. */
constexpr std::string_view FactEveningRotaAgreed = "fact.evening_rota_agreed";
constexpr std::string_view FactEveningRotaKept = "fact.evening_rota_kept";
/** How the jobs are shared: one of these three at most (an exclusive group), each implying Agreed. Agreed alone means an older agreement never put into words. */
constexpr std::string_view FactRotaDishesWeekdays = "fact.evening_rota_dishes_weekdays";
constexpr std::string_view FactRotaAlternateDays = "fact.evening_rota_alternate_days";
constexpr std::string_view FactRotaSweepAndCook = "fact.evening_rota_sweep_and_cook";

constexpr std::size_t ConversationMaxAuthoredChoices = 3;
constexpr std::size_t ConversationMaxNodeFactConditions = 3;
constexpr std::int32_t ConversationMinutesPerDay = 1440;
constexpr std::int32_t ConversationMaxCostMinutes = 60;
/** Most distinct facts one person's nodes may look at (the validity sweep tries every consistent subset). */
constexpr std::size_t ConversationMaxRelevantFacts = 12;

enum class PersonAvailability : std::uint8_t
{
    Awake, Busy, Asleep
};
constexpr std::size_t PersonAvailabilityCount = 3;
/** "awake", "busy", "asleep". */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view PersonAvailabilityId(PersonAvailability availability) noexcept;

struct ConversationChoiceDef
{
    /**
     * "choice.". Unique within its node. The same choice may be offered from several nodes (so one receipt covers it
     * wherever it appears); every offer of an id must then be identical in every field.
     */
    std::string_view Id;
    std::string_view Text;
    /** Declared minutes charged once per accepted choice, 0..60. A paid choice is deduplicated per session by a receipt. */
    std::int32_t CostMinutes = 0;
    /** Genuinely one-time: recorded in TakenChoices and withdrawn forever. Not needed for costs or facts. */
    bool bOnceEver = false;
    /** A fact id set on choosing (clearing its group, setting what it implies), or empty. */
    std::string_view SetsFact;
    /** A fact id cleared on choosing, or empty. It must not be implied by any other fact. */
    std::string_view ClearsFact;
    /** The node the session moves to (a node of the same person); empty ends the session. */
    std::string_view NextNode;
    /** Offered only while this fact is set / not set. Empty means no condition. */
    std::string_view RequiresFact;
    std::string_view ForbidsFact;
};

struct ConversationNodeDef
{
    /** Globally unique, "node.". */
    std::string_view Id;
    /** "person.". */
    std::string_view PersonId;
    /** What the person says. */
    std::string_view Line;
    /** The availability this node serves. */
    PersonAvailability Availability = PersonAvailability::Awake;
    /** The window [FromMinute, ToMinute) of the day it serves; a window with ToMinute <= FromMinute wraps past midnight. */
    std::int32_t FromMinute = 0;
    std::int32_t ToMinute = ConversationMinutesPerDay;
    /** Served only when every non-empty RequiresFacts entry is set and no non-empty ForbidsFacts entry is. */
    std::array<std::string_view, ConversationMaxNodeFactConditions> RequiresFacts{};
    std::array<std::string_view, ConversationMaxNodeFactConditions> ForbidsFacts{};
    /** False for reply nodes, which are reached only through a choice. */
    bool bSelectable = true;
    /** A polite decline (asleep or busy): the session offers Leave only. */
    bool bDeclines = false;
    std::array<ConversationChoiceDef, ConversationMaxAuthoredChoices> Choices{};
    std::size_t ChoiceCount = 0;
};

struct ConversationFactDef
{
    /** "fact.". */
    std::string_view Id;
    /** Facts sharing a non-empty group are exclusive: setting one clears the others. Never the target of an Implies. */
    std::string_view Group;
    /** Also set whenever this fact is set (and required whenever it is present). Empty for none. Chains must not loop. */
    std::string_view Implies;
};

/** The content the runtime reads. Both spans must outlive every use (the authored catalogues are constexpr). */
struct ConversationCatalogue
{
    std::span<const ConversationNodeDef> Nodes;
    std::span<const ConversationFactDef> Facts;
};

/** The catalogue the catalogue-less functions use: today the household adult's (ConversationContent.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue DefaultConversationCatalogue() noexcept;
/** The household adult's catalogue, whatever the default is. */
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue HouseholdConversationCatalogue() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue CareConversationCatalogue() noexcept;
/** Farida Qadri's catalogue (person.farida_qadri, ConversationFarida.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue FaridaConversationCatalogue() noexcept;
/** Anindita Sen's catalogue (person.anindita_sen, ConversationAnindita.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue AninditaConversationCatalogue() noexcept;
/** Mira Sen's catalogue (person.mira_sen, ConversationMira.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue MiraConversationCatalogue() noexcept;
/** Subrata Dey's catalogue (person.subrata_dey, ConversationSubrata.cpp). */
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue SubrataConversationCatalogue() noexcept;
/** The default catalogue's nodes. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const ConversationNodeDef> ConversationNodes() noexcept;

[[nodiscard]] PERFECTWORLDCORE_API const ConversationNodeDef* FindConversationNode(const ConversationCatalogue& catalogue, std::string_view nodeId) noexcept;
/** The authored choice with this id (the first offer of it), or null (Leave is not authored). */
[[nodiscard]] PERFECTWORLDCORE_API const ConversationChoiceDef* FindConversationChoice(const ConversationCatalogue& catalogue, std::string_view choiceId) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const ConversationFactDef* FindConversationFact(const ConversationCatalogue& catalogue, std::string_view factId) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const ConversationNodeDef* FindConversationNode(std::string_view nodeId) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const ConversationChoiceDef* FindConversationChoice(std::string_view choiceId) noexcept;

/**
 * True when the catalogue is well formed: ids and links, costs, shared choice definitions identical, facts known and
 * acyclic and every one settable, reply nodes reachable and of the right person, and a node for every situation of every
 * person (see the file comment). Tests call it for every catalogue they author.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool ValidateConversationCatalogue(const ConversationCatalogue& catalogue) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool ValidateConversationCatalogue() noexcept;

enum class ConversationError : std::uint8_t
{
    None, UnknownPerson, InvalidTime, AlreadyOpen, NotOpen, StaleChoice, AlreadyTaken, AlreadyCharged, SessionLimit
};
/**
 * "none", "unknown_person", "invalid_time", "already_open", "not_open", "stale_choice", "already_taken" (a bOnceEver choice
 * taken before), "already_charged" (a paid choice already charged this session), "session_limit". Stable.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view ConversationErrorCode(ConversationError error) noexcept;

/** A paid choice's per-session transaction receipt: this choice was charged in this session. */
struct ConversationReceipt
{
    std::string ChoiceId;
    std::uint32_t Session = 0;
    friend bool operator==(const ConversationReceipt&, const ConversationReceipt&) = default;
};

struct ConversationState
{
    bool Open = false;
    /** Empty unless a session is open. */
    std::string PersonId;
    std::string NodeId;
    /** Sessions opened, ever; the current (or most recent) session's number. Zero before the first. */
    std::uint32_t SessionCount = 0;
    /** Ids of the bOnceEver choices taken, ever, ascending. */
    std::vector<std::string> TakenChoices;
    /** Receipts of the paid choices charged in the current (or most recent) session; older ones are dropped on opening. Ascending by choice id. */
    std::vector<ConversationReceipt> Receipts;
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
    /** The accepted authored choice's id (empty for a refusal and for Leave). With Session, the key of the charge. */
    std::string_view ChoiceId;
    /** The session the choice was made in (the receipt's session when CostMinutes > 0); zero for a refusal. */
    std::uint32_t Session = 0;
    /** Minutes the integrator must charge, once for this (ChoiceId, Session). Zero for a refusal, a free choice and Leave. */
    std::int32_t CostMinutes = 0;
    /** The fact this choice newly set, or empty. */
    std::string_view FactSet;
    /** The first fact this choice removed (cleared, or replaced within a group), or empty. */
    std::string_view FactCleared;
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
    /** The text was an older version and was migrated; Serialise writes the current version. */
    bool bMigrated = false;
    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

[[nodiscard]] PERFECTWORLDCORE_API bool HasConversationFact(const ConversationState& state, std::string_view factId) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool WasChoiceTaken(const ConversationState& state, std::string_view choiceId) noexcept;
/** True when a receipt shows this choice charged in the state's current session. */
[[nodiscard]] PERFECTWORLDCORE_API bool WasChoiceChargedThisSession(const ConversationState& state, std::string_view choiceId) noexcept;

/** True when a selectable node serves this minute, availability and set of remembered facts (the test SelectConversationNode applies to each node). */
[[nodiscard]] PERFECTWORLDCORE_API bool ConversationNodeServes(const ConversationNodeDef& node, std::int32_t minuteOfDay, const ConversationState& state,
                                                              PersonAvailability availability) noexcept;
/**
 * The node a person speaks now, or null for an unknown person or a minute outside 0..1439. Pure: it reads only the
 * state's facts, never whether a session is open.
 */
[[nodiscard]] PERFECTWORLDCORE_API const ConversationNodeDef* SelectConversationNode(const ConversationCatalogue& catalogue, std::string_view personId,
                                                                                    std::int32_t minuteOfDay, const ConversationState& state,
                                                                                    PersonAvailability availability) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const ConversationNodeDef* SelectConversationNode(std::string_view personId, std::int32_t minuteOfDay,
                                                                                    const ConversationState& state,
                                                                                    PersonAvailability availability) noexcept;

/** Opens a session at the selected node and starts a new session number. Refused (state untouched) when one is already open. */
[[nodiscard]] PERFECTWORLDCORE_API ConversationError OpenConversation(const ConversationCatalogue& catalogue, ConversationState& state,
                                                                      std::string_view personId, std::int32_t minuteOfDay,
                                                                      PersonAvailability availability);
[[nodiscard]] PERFECTWORLDCORE_API ConversationError OpenConversation(ConversationState& state, std::string_view personId,
                                                                      std::int32_t minuteOfDay, PersonAvailability availability);
/**
 * The choices to show for the open session: the authored ones whose conditions hold and that were not taken ever or charged
 * this session (a decline node has none), then Leave. Empty when no session is open.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<ConversationChoiceView> VisibleConversationChoices(const ConversationCatalogue& catalogue,
                                                                                                  const ConversationState& state);
[[nodiscard]] PERFECTWORLDCORE_API std::vector<ConversationChoiceView> VisibleConversationChoices(const ConversationState& state);
/**
 * Takes a choice of the current node. A choice not offered now (absent, or its fact condition fails) is StaleChoice; a
 * bOnceEver choice taken before is AlreadyTaken; a paid choice already charged this session is AlreadyCharged. On success the
 * cost is returned once (with a receipt for the session if it is paid), the choice's facts are applied, and the session moves
 * on or ends. Every refusal leaves the state untouched. ConversationLeaveChoiceId ends the session at no cost.
 */
[[nodiscard]] PERFECTWORLDCORE_API ConversationChooseResult ChooseConversationChoice(const ConversationCatalogue& catalogue, ConversationState& state,
                                                                                    std::string_view choiceId);
[[nodiscard]] PERFECTWORLDCORE_API ConversationChooseResult ChooseConversationChoice(ConversationState& state, std::string_view choiceId);
/** Ends the open session (free); facts, taken choices and receipts stay. NotOpen when none is open. */
[[nodiscard]] PERFECTWORLDCORE_API ConversationError LeaveConversation(ConversationState& state);

/** True when the state could have arisen from the operations above under this catalogue. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsConversationStateValid(const ConversationCatalogue& catalogue, const ConversationState& state) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsConversationStateValid(const ConversationState& state) noexcept;
/** Strict text form, always "pw.conversation/2". An inconsistent state serialises to the empty string. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseConversationState(const ConversationCatalogue& catalogue, const ConversationState& state);
[[nodiscard]] PERFECTWORLDCORE_API std::string SerialiseConversationState(const ConversationState& state);
/**
 * Reads "pw.conversation/2", or migrates "pw.conversation/1": its taken-choice and fact lines must be what /1 could have
 * produced (taken ids talk_rota, keep_rota, revisit_rota only; facts agreed and kept only; each fact with its choice);
 * the facts stay (an agreement is never erased) but the taken list is dropped (costs and rewards are now guarded by
 * receipts and facts), the arrangement stays unnamed (agreed, never put into words), the session counter becomes 1 if a
 * session was open and 0 otherwise, and an open session at a node the catalogue no longer has is closed (leaving is free).
 * Refuses a missing or unknown header, a truncated text, unknown or repeated or unsorted ids, an inconsistent state, and trailing data.
 */
[[nodiscard]] PERFECTWORLDCORE_API ConversationStateParse DeserialiseConversationState(const ConversationCatalogue& catalogue, std::string_view text);
[[nodiscard]] PERFECTWORLDCORE_API ConversationStateParse DeserialiseConversationState(std::string_view text);
} // namespace PerfectWorld
