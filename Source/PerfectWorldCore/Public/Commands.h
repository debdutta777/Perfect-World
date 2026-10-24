#pragma once

#include "GameClock.h"
#include "WorldState.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace PerfectWorld
{
/**
 * Commands: the only way anything asks the world to change (SYSTEMS.md §2).
 * Each command's TypeId is the stable text written to replay logs; it never
 * changes once shipped. Systems add their own command headers as they arrive.
 */
enum class CommandSource : std::uint8_t
{
    Player,
    Presentation,
    Performance,
    System,
    Migration,
    Debug
};

/**
 * Move the one clock forward. The only way time passes outside a paid conversation choice (which charges through the
 * same path). It is also the free-roam command: the Unreal clock turns real time into whole minutes with TimeContract's
 * AdvanceRealTime (60 real seconds = 60 game minutes) and submits them here. Events due at the current instant fire
 * first, without a minute passing. Refused while a conversation is open ("time.conversation_open"): reading is free.
 */
struct AdvanceTime
{
    static constexpr std::string_view TypeId = "time.advance";
    std::int64_t Minutes = 1;
};

/** Schedule a world event on the clock. IDs follow "system/kind/key". Systems and debug only. */
struct ScheduleWorldEvent
{
    static constexpr std::string_view TypeId = "world.schedule";
    GameInstant When = 0;
    std::string EventId;
};

/** Set a world flag by stable ID (lowercase letters, digits, '_' and '.'). */
struct SetWorldFlag
{
    static constexpr std::string_view TypeId = "world.flag";
    std::string FlagId;
    bool Value = true;
};

/** Register a Gate site. System and debug only. */
struct GateRegister
{
    static constexpr std::string_view TypeId = "gate.register";
    std::string Id;
    GateRank Rank = GateRank::F;
    GateStatus Status = GateStatus::Contained;
};

/** Schedule a Gate's onset on the clock as "gates/onset/<id>". System and debug only. */
struct GateScheduleOnset
{
    static constexpr std::string_view TypeId = "gate.schedule_onset";
    std::string Id;
    GameInstant When = 0;
};

/** The NGA contains an active Gate. System and debug only. */
struct GateContain
{
    static constexpr std::string_view TypeId = "gate.contain";
    std::string Id;
};

/** Seal a contained Gate for good. System and debug only. */
struct GateSeal
{
    static constexpr std::string_view TypeId = "gate.seal";
    std::string Id;
};

/**
 * Fire the events due at the current instant without moving time, up to the event cap. The continuation after an
 * event-capped advance stopped with events still due now (CommandResult::Partial); repeat until IsWorldQuiescent.
 * Refused ("time.nothing_due") when nothing is due. Any source.
 */
struct SettleDueEvents
{
    static constexpr std::string_view TypeId = "time.settle";
};

/**
 * Open a conversation with a person at the world clock's minute of day (WorldConversation.cpp). The person's catalogue
 * is chosen by id (ConversationCatalogueForPerson: household, care, Farida). Availability is the caller's until people
 * live in the world state: the bridge derives it from the person's schedule at the world clock. One session at a time
 * in the whole world. Refused while any session is open ("conversation.already_open"), the player is asleep or busy with
 * an activity, or the world is unsettled. Codes: "conversation.<ConversationErrorCode>", "conversation.player_asleep",
 * "conversation.activity_running", "world.unsettled". Any source.
 */
struct ConversationOpen
{
    static constexpr std::string_view TypeId = "conversation.open";
    std::string PersonId;
    PersonAvailability Availability = PersonAvailability::Awake;
};

/**
 * Take one choice of the person's open session, atomically: the choice is validated by the core Conversation module
 * against the world's state for that person, its facts are applied, and its CostMinutes are charged through the world
 * clock in the same command, exactly once, keyed by the receipt (ChoiceId, Session) the module records. Session must be
 * the open session's number (WorldState::Conversations[PersonId].SessionCount); an older one is
 * "conversation.stale_session", so a duplicate submitted after Leave and reopen cannot charge again. A duplicate within
 * the session is refused by the module ("conversation.stale_choice" once the node moved on, else
 * "conversation.already_charged"). A paid choice is all-or-nothing: if the charge cannot complete (asleep, activity
 * running, beyond the calendar: "time.*"; or the event cap would stop it short: "conversation.charge_interrupted")
 * nothing changes. "choice.leave" ends the session free.
 */
struct ConversationChoose
{
    static constexpr std::string_view TypeId = "conversation.choose";
    std::string PersonId;
    std::string ChoiceId;
    std::uint32_t Session = 0;
};

/** End the person's open session, free ("conversation.not_open" otherwise). Facts, taken choices and receipts stay. */
struct ConversationLeave
{
    static constexpr std::string_view TypeId = "conversation.leave";
    std::string PersonId;
};

/**
 * Open or close a door through its access rules (Doors.h TrySetDoorOpen) at the world clock's minute of day.
 * Context (direction, residency, invitation) is the caller's until the origin binding lives in the world state; the
 * bridge fills IsResident from ResolveLaneOrigin. Refused "door.<DoorResultCode>", including "door.no_change". Any source.
 */
struct DoorSet
{
    static constexpr std::string_view TypeId = "door.set";
    std::string DoorId;
    bool Open = true;
    DoorUseContext Context;
};

/**
 * Record a leaf's physical outcome (Doors.h ApplyDoorSettlement): an interrupted swing, a partly open leaf, the side
 * it swung to. No access rules apply, so only the presentation, systems and debug may report it ("door.source").
 * Refused "door.<DoorResultCode>" otherwise, including "door.no_change" and "door.invalid_state".
 */
struct DoorSettle
{
    static constexpr std::string_view TypeId = "door.settle";
    std::string DoorId;
    bool IsOpenNow = false;
    std::int8_t SwingSide = 0;
};

/** Free morning intentions, validated by DayLoop against the reducer's borrowed activity catalogue. */
struct SchoolPlanDay
{
    static constexpr std::string_view TypeId = "school.plan_day";
    std::vector<DayPlanItem> Plan;
};

/**
 * Record arrival at the current world instant through DayLoop's ArriveSchool, assuming a stay until dismissal.
 * The school gate submits this after physical approach; placement is presentation context, as for lane commands.
 * Requires a morning plan and the reducer's matching bell schedule/calendar. No time or learning is charged.
 * Forecast period credit is the existing attendance contract; this does not complete classroom activities.
 */
struct SchoolArrive
{
    static constexpr std::string_view TypeId = "school.arrive";
};

/** Physical approach is explicit replay input. Pending requests keep time running until the safe pause. */
struct CommerceServiceRequest
{
    static constexpr std::string_view TypeId = "commerce.service_request";
    std::string ShopId;
    std::string CounterAnchorId;
};

/** Closing/leaving the counter releases its interruption. Reload also starts with no service. */
struct CommerceServiceClose
{
    static constexpr std::string_view TypeId = "commerce.service_close";
    std::string ShopId;
};

struct CommerceBuy
{
    static constexpr std::string_view TypeId = "commerce.buy";
    std::string ShopId;
    std::string ItemId;
    std::int32_t Quantity = 1;
    std::uint64_t ExpectedRevision = 0;
};

struct InventoryConsume
{
    static constexpr std::string_view TypeId = "inventory.consume";
    std::string ItemId;
};

/** Existing portable day rules through WorldSim: sleep, activity completion/cancel and passes share settlement. */
struct WorldDayCommand
{
    static constexpr std::string_view TypeId = "day.apply";
    DayCommand Day;
};

using CommandPayload = std::variant<AdvanceTime, ScheduleWorldEvent, SetWorldFlag, GateRegister, GateScheduleOnset, GateContain, GateSeal,
                                    SettleDueEvents, ConversationOpen, ConversationChoose, ConversationLeave, DoorSet, DoorSettle,
                                    SchoolPlanDay, SchoolArrive, CommerceServiceRequest, CommerceServiceClose,
                                    CommerceBuy, InventoryConsume, WorldDayCommand>;

struct Command
{
    CommandSource Source = CommandSource::Player;
    CommandPayload Payload;
};

inline std::string_view CommandTypeId(const CommandPayload& payload)
{
    return std::visit([](const auto& command) { return std::decay_t<decltype(command)>::TypeId; }, payload);
}
} // namespace PerfectWorld
