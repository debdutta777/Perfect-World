#pragma once

#include "Conversation.h"
#include "GameClock.h"
#include "Schedules.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * NPC behaviour (LANE_LIFE_IMPLEMENTATION row 4.5; LANE_LIFE_PLAN section B, "What small AI means").
 *
 * The "small AI" of a person is a pure, deterministic mapping from what the portable core already knows to one NpcIntent:
 *
 *   (resolved schedule at an instant, conversation-open flag, blocked flag)  ->  NpcIntent
 *
 * The core decides what a person is doing, where, how available they are and why they decline; Unreal only presents it
 * (route following, avoidance, gestures, animation). There is no clock here, no randomness and no memory: the same
 * resolution and flags always give the same intent, so a loaded scene, a live scene and a scene reached by a long skip
 * agree, and resuming after a conversation is simply asking again without the flag.
 *
 * States, after the plan's table:
 *  - Work   : the schedule places the person in a work role (or an activity block) at an anchor. WorkActionId names the
 *             purposeful action Unreal cycles; it comes from the role, never from animation, so no work "completes" by
 *             an animation event. Meaningful completion is recorded at the schedule boundary (Schedules.h tallies).
 *  - Travel : the schedule has the person walking between two zones. RouteId names the leg, ProgressPermille is how far
 *             along it the person is, and Unreal reconstructs the same journey from it.
 *  - Idle   : a genuine gap or an authored rest. Sit, read or observe.
 *  - Sleep  : asleep or private; the person stays at a valid home anchor and ordinary street talk is unavailable.
 *  - Chat   : an accepted conversation. It suspends the travel or work intent (the suspended state is ResumeState and
 *             the position context is kept) and pauses nothing in the core; Leave resumes by asking again without the flag.
 *  - Yield  : a persistent obstruction reported by presentation (the blocked flag). The person holds and the delay is an
 *             explicit state, never a hidden teleport; any persistent consequence must enter through a bounded core
 *             command, not through this function.
 *
 * Availability follows the Conversation module: Sleep is Asleep, Travel and Yield are Busy but greetable, Work is Busy or
 * Awake by the role's flag, Idle and Chat-of-an-awake-person are Awake. bTalkable says whether the interaction prompt
 * offers a conversation at all (an Asleep person: no). DeclineReasonKey is non-empty when the conversation opened would be
 * the polite decline (a Busy or Asleep person); it is a stable "npc.reason." key, never player-facing text.
 *
 * Roles. A schedule entry whose Doing is a role token is classified by the authored table NpcRoles(); an entry of
 * ScheduleDoingKind::Activity is Work with WorkActionId "work.activity". A role token not in the table is refused, so a
 * typo in authored data is caught by ValidateNpcSchedule rather than becoming silent idling.
 *
 * Standalone: not part of WorldState, the reducer, the hash or the save. Integer arithmetic only. Nothing here throws.
 */

enum class NpcState : std::uint8_t
{
    Work, Travel, Idle, Chat, Sleep, Yield
};
constexpr std::size_t NpcStateCount = 6;
/** "work", "travel", "idle", "chat", "sleep", "yield". Stable. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view NpcStateId(NpcState state) noexcept;

enum class NpcIntentError : std::uint8_t
{
    None,
    /** The resolution is not Valid, a stay has an empty Doing, or a zone is empty / a progress is outside 0..1000. */
    InvalidResolution,
    /** A role token that NpcRoles() does not list. */
    UnknownRole,
    /** A conversation was asked for with someone who is Asleep (street talk is unavailable). */
    ConversationUnavailable
};
/** "none", "invalid_resolution", "unknown_role", "conversation_unavailable". Stable. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view NpcIntentErrorCode(NpcIntentError error) noexcept;

/** How a role token behaves. WorkAvailability matters only when State is Work. */
struct NpcRoleDef
{
    /** The role token as written in a schedule (ScheduleEntry::Doing with Kind Role). */
    std::string_view Role;
    /** Work, Idle or Sleep. */
    NpcState State = NpcState::Idle;
    /** Work only: "work." id of the purposeful action. Empty otherwise. */
    std::string_view WorkActionId;
    /** Work only: Awake (can stop and talk while working, such as serving) or Busy (decline politely, such as sweeping). */
    PersonAvailability WorkAvailability = PersonAvailability::Awake;
};

/** The authored role table, ascending by Role. */
[[nodiscard]] PERFECTWORLDCORE_API std::span<const NpcRoleDef> NpcRoles() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const NpcRoleDef* FindNpcRole(std::string_view role) noexcept;
/** True when the table is well formed (ascending unique tokens, work roles carry an id, others do not). Tests call it. */
[[nodiscard]] PERFECTWORLDCORE_API bool ValidateNpcRoleTable() noexcept;

/** The WorkActionId an Activity-kind schedule entry gets. */
constexpr std::string_view NpcActivityWorkActionId = "work.activity";
constexpr std::string_view NpcReasonAsleep = "npc.reason.asleep";
constexpr std::string_view NpcReasonWalking = "npc.reason.walking";
constexpr std::string_view NpcReasonWorking = "npc.reason.working";
constexpr std::string_view NpcReasonBlocked = "npc.reason.blocked";

struct NpcIntent
{
    NpcState State = NpcState::Idle;
    /**
     * Where the person is (Work, Idle, Sleep) or was heading / holding (Chat, Yield keep the suspended intent's
     * context). For Travel it is the destination zone, so a presenter can always name "where".
     */
    std::string AnchorId;
    /** Travel (and a Chat/Yield that suspended one): "route:<from>-><to>". Empty otherwise. */
    std::string RouteId;
    /** Share of the current schedule segment elapsed, 0..1000. For Travel, how far along the route. */
    std::int32_t ProgressPermille = 0;
    /** The action presentation cycles; empty unless State is Work. */
    std::string_view WorkActionId;
    /** What a conversation sees (feeds SelectConversationNode). */
    PersonAvailability Availability = PersonAvailability::Awake;
    bool bTalkable = true;
    /** Non-empty when talking would only get the polite decline. */
    std::string_view DeclineReasonKey;
    /** For Chat and Yield, the state that is suspended and resumes; otherwise equal to State. */
    NpcState ResumeState = NpcState::Idle;
    /** For Chat and Yield suspending Work, the action that resumes; otherwise equal to WorkActionId. */
    std::string_view ResumeWorkActionId;
    /** The schedule item that placed the person ("routine:<n>" or an exception id); empty for a gap. */
    std::string SourceId;

    friend bool operator==(const NpcIntent&, const NpcIntent&) = default;
};

struct NpcIntentResult
{
    NpcIntentError Error = NpcIntentError::None;
    /** Default-constructed on a refusal. */
    NpcIntent Intent;
    [[nodiscard]] bool Ok() const noexcept { return Error == NpcIntentError::None; }
};

/**
 * Pure. Maps a resolved schedule state to an intent.
 *
 * conversationOpen: the player has an accepted conversation with this person. It turns the intent into Chat (keeping the
 * suspended intent in ResumeState and the position context); refused with ConversationUnavailable for an Asleep person.
 * blocked: presentation reports a persistent obstruction. It turns Work, Travel and Idle into Yield; Sleep ignores it
 * and Chat outranks it (the world is paused while the player talks).
 * Every refusal returns a default intent.
 */
[[nodiscard]] PERFECTWORLDCORE_API NpcIntentResult ResolveNpcIntent(const ScheduleResolution& resolution, bool conversationOpen,
                                                                    bool blocked) noexcept;

/** ResolveSchedule + ResolveNpcIntent. An instant outside the schedule range is InvalidResolution. */
[[nodiscard]] PERFECTWORLDCORE_API NpcIntentResult ResolveNpcIntentAt(const PersonSchedule& person, const ScheduleTravelTable& travel,
                                                                      GameInstant at, bool conversationOpen, bool blocked);

/**
 * The next instant after `after` at which the underlying (flag-free) intent can change identity: this is
 * NextScheduleBoundary, which can also stop at a boundary that changes only the source or the squeezed flag, never later
 * than a real change. Between two such instants the intent differs only in ProgressPermille.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<GameInstant> NextNpcIntentBoundary(const PersonSchedule& person,
                                                                                    const ScheduleTravelTable& travel,
                                                                                    GameInstant after);

/** "" when every routine entry and exception of the person has a known role (Activity entries need only a non-empty ID). */
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateNpcSchedule(const PersonSchedule& person);
} // namespace PerfectWorld
