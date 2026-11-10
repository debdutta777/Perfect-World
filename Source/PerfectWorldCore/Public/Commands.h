#pragma once

#include "GameClock.h"
#include "SchoolWeekState.h"
#include "SchoolWork.h"
#include "WeekCompression.h"
#include "WorldCountdown.h"
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

/**
 * S04: open the bound A1 lesson at its bell (SchoolSessions.h OpenSchoolLesson), resolving BindingId through the sim's
 * borrowed lesson catalogue, never from a widget. The skill level, seed and known alternatives are the reducer's. Free:
 * the class activity starts at the current instant. Refused with the contract's school.session.* / dayloop.* codes, or
 * "world.unsettled", "school.invalid_state", "school.conversation_open", "commerce.service_active", "school.paper_open".
 */
struct SchoolLessonOpen
{
    static constexpr std::string_view TypeId = "school.lesson.open";
    std::string BindingId;
    SchoolLessonMode Mode = SchoolLessonMode::Attend;
    SchoolLessonGoal Goal = ModelGoal::UnderstandModel;
};

/** S04: one runner action of the open lesson (ChooseSchoolLesson). Units are within the bell period; no world time passes. */
struct SchoolLessonChoose
{
    static constexpr std::string_view TypeId = "school.lesson.choose";
    std::uint64_t Session = 0;
    SchoolLessonAction Action;
};

/**
 * S04: Record (Cancel false) or leave early (Cancel true, ElapsedMinutes 0..duration-1) through CloseSchoolLesson, then the
 * day's movement is charged through the world clock in the same command. All-or-nothing: an event-capped charge refuses
 * "school.lesson.charge_interrupted" and nothing changes. Completion learning is published once as its DayLoopEvent.
 */
struct SchoolLessonClose
{
    static constexpr std::string_view TypeId = "school.lesson.close";
    std::uint64_t Session = 0;
    bool Cancel = false;
    std::int32_t ElapsedMinutes = 0;
};

/** S04: upsert or remove one standing order (SetSchoolStandingOrder). Free; refused while a lesson or paper is open. */
struct SchoolWeekOrder
{
    static constexpr std::string_view TypeId = "school.week.order";
    SchoolStandingOrder Order;
    bool Remove = false;
};

/**
 * S04: build the school week from the standing orders and `Base` (BuildSchoolWeekPlan), compress it (CompressSchoolWeek)
 * and commit its day loop and lesson sessions together, the time charged through the world clock once. The actual
 * SchoolWeekResult is published as WorldSim::LastSchoolWeekResult for the planner recap.
 */
struct SchoolWeekResolve
{
    static constexpr std::string_view TypeId = "school.week.resolve";
    WeekPlan Base;
};

/**
 * R9-04: the R9-02 supervised tutorial session (TutorialDungeon.h) through WorldSim (WorldAwakening.cpp). Every instant is
 * the world clock's, never a payload's (except TutorialMakeUp's slot); refusals keep the contract's tutorial.* codes. Entry
 * needs a seeded Awakening, an idle settled world and the bound ability recipes; playing, reading and recovering are free.
 * The act or withdrawal that makes the session ReadyToGrant also exits it in the same command: the disclosed hour
 * (TutorialSessionMinutes) is charged once through the world clock and the grant is committed (GrantTutorialAwakening),
 * the live practice profile starting as the grant's StartingProfile. A failure anywhere refuses the whole command.
 */
struct TutorialEnter
{
    static constexpr std::string_view TypeId = "tutorial.enter";
};

/** A COMPLETED contextual action at the current station (CompleteTutorialChoice). */
struct TutorialAct
{
    static constexpr std::string_view TypeId = "tutorial.act";
    std::string StationId;
    std::string ChoiceId;
};

struct TutorialAssist
{
    static constexpr std::string_view TypeId = "tutorial.assist";
};

struct TutorialRecover
{
    static constexpr std::string_view TypeId = "tutorial.recover";
};

/** Safe withdrawal, available at any point of an active session; it still exits and grants. */
struct TutorialWithdraw
{
    static constexpr std::string_view TypeId = "tutorial.withdraw";
};

/** System only (the appointment authority): the student was not there; a make-up is required, never a grant. */
struct TutorialAbsent
{
    static constexpr std::string_view TypeId = "tutorial.absent";
};

/** A supervised make-up slot at `At` (ScheduleTutorialMakeUp); the original appointment and examination never move. */
struct TutorialMakeUp
{
    static constexpr std::string_view TypeId = "tutorial.make_up";
    GameInstant At = 0;
};

/** System only: the exit-and-grant continuation for a loaded ReadyToGrant session. Never a player refusal gate. */
struct TutorialFinish
{
    static constexpr std::string_view TypeId = "tutorial.finish";
};

/**
 * R9-04: confirm optional supervised countdown practice. `Accepted` is the disclosure WorldSim::PreviewCountdownPractice
 * returned; the reducer recomputes it and refuses a stale or altered one (countdown.bad_plan), then charges the minutes
 * through the world clock, records the displaced intentions as a receipt and practises the live profile once. Declining
 * is sending nothing.
 */
struct CountdownPracticeConfirm
{
    static constexpr std::string_view TypeId = "countdown.practice.confirm";
    WorldCountdownPracticeOffer Accepted;
};

// SchoolWorkCommand ("school.work.apply") is SchoolWork.h's delivered command type, routed as is.
using CommandPayload = std::variant<AdvanceTime, ScheduleWorldEvent, SetWorldFlag, GateRegister, GateScheduleOnset, GateContain, GateSeal,
                                    SettleDueEvents, ConversationOpen, ConversationChoose, ConversationLeave, DoorSet, DoorSettle,
                                    SchoolPlanDay, SchoolArrive, CommerceServiceRequest, CommerceServiceClose,
                                    CommerceBuy, InventoryConsume, WorldDayCommand, SchoolLessonOpen, SchoolLessonChoose,
                                    SchoolLessonClose, SchoolWorkCommand, SchoolWeekOrder, SchoolWeekResolve, TutorialEnter,
                                    TutorialAct, TutorialAssist, TutorialRecover, TutorialWithdraw, TutorialAbsent,
                                    TutorialMakeUp, TutorialFinish, CountdownPracticeConfirm>;

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
