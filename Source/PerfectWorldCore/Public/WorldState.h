#pragma once

#include "AwakeningInputs.h"
#include "Conversation.h"
#include "CharacterCreation.h"
#include "Commerce.h"
#include "DayLoop.h"
#include "Doors.h"
#include "GameClock.h"
#include "LaneJournal.h"
#include "RandomStreams.h"
#include "SchoolSessions.h"
#include "SchoolWeekState.h"
#include "SchoolWork.h"
#include "TowerSession.h"
#include "TutorialDungeon.h"

#include <cstdint>
#include <map>
#include <vector>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/** Gate rank, lowest to highest. The numeric order is the rank order. */
enum class GateRank : std::uint8_t
{
    F,
    E,
    D,
    C,
    B,
    A,
    S
};

/** Content data's "open" is Active here. Contained means the NGA holds it. */
enum class GateStatus : std::uint8_t
{
    Contained,
    Active,
    Sealed
};

/** One Gate site (ROADMAP M1-35). Changed only by the gate system, through commands and scheduled onsets. */
struct GateSiteState
{
    GateRank Rank = GateRank::F;
    GateStatus Status = GateStatus::Contained;
    /** The instant the current status began. */
    GameInstant StatusSince = 0;
    /** Scheduled onsets that have fired for this Gate. */
    std::int32_t OnsetCount = 0;
};

/**
 * P5-03: one booked examination follow-up session (WorldExamFollowup.h). Booked, Arrived, Active and Paused are open;
 * Paid (attendance charged once, including every migrated pre-v14 attendance), Completed (the record's one task
 * completion), Cancelled (by the player), Missed (its slot passed and a new booking replaced it) and Released (a pre-v14
 * reservation released for an unrecorded reason) are terminal. The ordinals are the saved spelling: append only.
 */
enum class P503SessionState : std::uint8_t { Booked, Arrived, Active, Paused, Paid, Completed, Cancelled, Missed, Released };
struct P503FollowupSession
{
    /** Unique and increasing within its record; it distinguishes two sessions booked at the same instant. */
    std::uint64_t Generation = 0;
    GameInstant At = 0;
    P503SessionState State = P503SessionState::Booked;
    friend bool operator==(const P503FollowupSession&, const P503FollowupSession&) = default;
};
/** The immutable once-only task consequence: the chosen rule choice and its records, snapshotted when earned. */
struct P503FollowupCompletion
{
    std::uint64_t Generation = 0;
    std::string TaskId;
    std::string ChoiceId;
    std::vector<std::string> Records;
    GameInstant CompletedAt = 0;
    friend bool operator==(const P503FollowupCompletion&, const P503FollowupCompletion&) = default;
};
/** Follow-up progress for one content appointment offered by one sitting's result. */
struct P503FollowupRecord
{
    /** The examination sitting whose result offered the appointment: stored, never inferred from the live year. */
    std::uint32_t Sitting = 0;
    std::string AppointmentId;
    /** The command freshness token: advances on every accepted transition and never resets on cancel or rebook. */
    std::uint64_t Revision = 0;
    std::vector<P503FollowupSession> Sessions;
    std::optional<P503FollowupCompletion> Completion;
    friend bool operator==(const P503FollowupRecord&, const P503FollowupRecord&) = default;
};

/**
 * The authoritative world (SYSTEMS.md §2). Each member is one save section,
 * mutated only by the reducer through its owning system. This version holds the
 * clock, world flags, Gate sites, the RNG streams and the player's day; systems
 * add their slices as they arrive.
 */
struct WorldState
{
    std::uint64_t WorldSeed = 0;
    /** Accepted commands so far. */
    std::uint64_t Seq = 0;
    GameClock Clock;
    std::map<std::string, bool, std::less<>> Flags;
    /** Gate sites by stable ID (the gate system's slice). */
    std::map<std::string, GateSiteState, std::less<>> Gates;
    /**
     * The persisted state of the four RNG streams (M1-02), indexed by RandomStreamId.
     * WorldSim seeds it from WorldSeed; whatever draws from a stream writes the
     * captured state back here, so a reload continues the sequence instead of re-rolling it.
     */
    RandomStreams::Snapshot Rng = RandomStreams().Capture();
    /**
     * The player's day (M2-12): condition, the running activity, practice, receipts, attendance,
     * the plan and the commitment. It keeps its own Now, but there is one clock: in a valid world
     * DayLoop.Now equals Clock.Now() (IsWorldDayInStep). The world clock is authoritative.
     * AdvanceTime moves both; the day loop's own commands move its Now, and a world that routes
     * them must move Clock to match. A fresh world is a fresh life at the clock's instant.
     */
    DayLoopState DayLoop = NewDayLoopState(0);
    /**
     * The lane's conversation memory, one ConversationState (Conversation.h) per person spoken to, keyed by person id:
     * that person's open session, remembered facts, bOnceEver choices and the current session's paid-choice receipts
     * (ChoiceId, Session). Each state is valid under its person's own catalogue (ConversationCatalogueForPerson), so the
     * household, care and Farida content never share facts or session numbers. At most one state is Open in the world
     * (OpenConversationPersonId). An entry exists once its person has been spoken to. Changed only by the conversation.*
     * commands (WorldConversation.cpp), which charge a paid choice's minutes through the world clock in the same command.
     * Since world v8 each entry is saved in key order as the person id plus its "pw.conversation/2" text and hashed.
     * Unknown people and invalid states are refused. Earlier saves start with no entries. Remembered journal agreements
     * are projected from these current facts, never persisted as a second copy.
     */
    std::map<std::string, ConversationState, std::less<>> Conversations;
    /** Dated successful exchanges, pw.lane_journal/1 in world v9. Older saves have no dated evidence. */
    LaneJournalState Journal;
    /**
     * The lane's doors (Doors.h): one physical entry (open, swing side) per authored door, in table order. Changed
     * only by the door.set and door.settle commands. Since world v8 persisted as "pw.doors/2" and hashed;
     * earlier worlds start from the authored defaults, as here. Historical door rosters migrate explicitly.
     */
    DoorState Doors = MakeInitialDoorState();
    /** R8-03 authoritative slice. R8-04 owns its later save/hash encoding. */
    CommerceState Commerce;
    /**
     * S04: the Phase 2 school slices (SchoolSessions.h, SchoolWork.h, SchoolWeekState.h), changed only by the
     * school.lesson.*, school.work.apply and school.week.* commands (WorldSchool.cpp, WorldSchoolWeek.cpp). Since world
     * v11 saved together as one "pw.school/1" text and hashed as the "school" section. Every earlier world starts with
     * all three empty: attendance and receipts are never evidence of a lesson, a paper or a standing order.
     */
    SchoolSessionsState SchoolSessions;
    SchoolWorkState SchoolWork;
    SchoolWeekState SchoolWeek;
    /**
     * R9-04: the Round 9 Awakening slices, changed only through WorldSim (WorldAwakening.cpp, the countdown boundary in
     * RunTimeTo and the school credits). Awakening (AwakeningInputs.h) is the one latent-stat and growth owner: unseeded
     * until creation confirmation seeds it (SeedAwakeningInputs), and while unseeded nothing grows, opens or grants.
     * Tutorial is the R9-02 supervised session (TutorialDungeon.h). CountdownText is the canonical SerialiseCountdown text
     * of the ten days (Countdown.h includes this header, so the state is held as its text), empty until the 18:00 opening
     * on Awakening Day. LiveAwakenedProfile is the live practice owner, set once from the grant's StartingProfile (which
     * stays an immutable snapshot) and changed only by countdown practice. Since world v12 saved together as one
     * "pw.awakening/1" text and hashed as the "awakening" section; every earlier world loads with all four empty.
     */
    AwakeningState Awakening;
    TutorialDungeonState Tutorial;
    std::string CountdownText;
    std::optional<AwakenedProfile> LiveAwakenedProfile;
    /**
     * P4-02: the national tower examination of 2 February 2043 (WorldExam.h), the one TowerSessionState P4-01 froze: entry
     * choice and final outcome (Participation), the frozen preparation, runner nonces, the active floor with its choice prefix
     * and mission minutes, and every finished floor with its provenance. Changed only by tower.session.apply through WorldSim
     * (WorldExam.cpp). Unset for a new life and for every world saved before v13. Since world v13 saved as one
     * "pw.examination/1" text and hashed as the "examination" section. Results are derived from it, never stored beside it.
     */
    TowerSessionState Examination;
    /**
     * P4-09: the earlier annual sittings (WorldExamSittings.h), one per sitting in order: sitting k is 2 February (2043 + k)
     * 06:00, and Examination belongs to sitting ExaminationEarlier.size(). Each entry is closed (Completed, Ejected,
     * Withdrawn, Deferred, Declined) or Unset (absent from that sitting). Empty for a first sitting and for every older
     * save; written inside the examination section (pw.examination/2 only when non-empty), so no schema or hash change.
     */
    std::vector<TowerSessionState> ExaminationEarlier;
    /**
     * P5-03: examination follow-up bookings, sessions and task completions, sorted by (Sitting, AppointmentId), changed only
     * by exam.followup.apply (WorldExamFollowup.cpp). Saved inside the examination section from world v14
     * (pw.examination/3, only when non-empty, so every world without follow-ups keeps its exact bytes and hash); the
     * reserved p407u.followup.* flags of older saves migrate here on load. Content-free rules: P503FollowupsValidInWorld.
     */
    std::vector<P503FollowupRecord> ExaminationFollowups;
};

/** A fresh world: the clock at the campaign epoch (2 Jun 2042 06:00), no flags or Gates, RNG streams seeded from the world seed, a fresh day. */
inline WorldState MakeWorldState(std::uint64_t worldSeed)
{
    WorldState state;
    state.WorldSeed = worldSeed;
    state.Rng = RandomStreams(worldSeed).Capture();
    state.DayLoop = NewDayLoopState(state.Clock.Now());
    state.Commerce = InitialiseCommerce(0, state.Clock.Now()).State;
    return state;
}

/** Creation was validated by CharacterCreator; its resource rank is always in 0..4. */
inline WorldState MakeWorldState(std::uint64_t worldSeed, const CharacterCreator& creation)
{
    WorldState state = MakeWorldState(worldSeed);
    state.Commerce = InitialiseCommerce(creation.GetTraitRank(HouseholdResourcesTraitId), state.Clock.Now()).State;
    return state;
}

/**
 * True when the player's day is a valid day loop and its clock is the world clock. The save
 * reader requires it, and the reducer keeps it true: a time advance moves both clocks.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsWorldDayInStep(const WorldState& state);

/**
 * True when nothing is due at the current instant (LANE_LIFE_IMPLEMENTATION §A item 5). An event-capped advance can
 * stop with events still due now; submit time.settle (SettleDueEvents) until this holds before saving or before the
 * next player command. The conversation.* and door.* commands are refused ("world.unsettled") until it does.
 */
[[nodiscard]] PERFECTWORLDCORE_API bool IsWorldQuiescent(const WorldState& state);

/**
 * The conversation catalogue that speaks for a person (WorldConversation.cpp): the first of the household, care and
 * Farida catalogues with a node for that person id, or empty for a person none of them knows. Until the composed lane
 * catalogue (CAT order) exists, this is the one person-to-content routing rule; the save order uses it too.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<ConversationCatalogue> ConversationCatalogueForPerson(std::string_view personId) noexcept;

/** The person whose conversation session is open, or empty when none is. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view OpenConversationPersonId(const WorldState& state) noexcept;

/** Things that happened, in the order they happened. Presentation rebuilds from these. */
struct TimeAdvanced
{
    GameInstant From = 0;
    GameInstant To = 0;
};

/** A new game day has begun (midnight). Fires before any scheduled event at the same instant. */
struct DayStarted
{
    std::int64_t Day = 0;
};

struct ScheduledEventFired
{
    std::int64_t Handle = 0;
    std::string EventId;
};

struct WorldFlagChanged
{
    std::string FlagId;
    bool Value = false;
};

struct GateRegistered
{
    std::string GateId;
    GateRank Rank = GateRank::F;
    GateStatus Status = GateStatus::Contained;
};

/** A scheduled onset fired and the Gate is now active. Emitted right after that event's ScheduledEventFired. */
struct GateOnset
{
    std::string GateId;
    GateRank Rank = GateRank::F;
    GateStatus Previous = GateStatus::Contained;
    /** Onsets fired for this Gate, this one included. */
    std::int32_t OnsetCount = 0;
};

/** A scheduled onset fired at a sealed Gate: the seal held and the Gate is unchanged. */
struct GateOnsetHeld
{
    std::string GateId;
};

struct GateContained
{
    std::string GateId;
};

struct GateSealed
{
    std::string GateId;
};

/** A conversation session opened at the world clock's minute of day. */
struct ConversationOpened
{
    std::string PersonId;
    std::string NodeId;
    std::uint32_t Session = 0;
};

/**
 * One authored conversation choice was accepted: its facts are applied and, when CostMinutes > 0, exactly that many
 * minutes follow in the same command's TimeAdvanced (the charge for the receipt (ChoiceId, Session)). Leave emits only
 * ConversationEnded.
 */
struct ConversationChoiceAccepted
{
    std::string PersonId;
    std::string ChoiceId;
    std::uint32_t Session = 0;
    std::int32_t CostMinutes = 0;
    std::string FactSet;
    std::string FactCleared;
    /** Empty when the session ended. */
    std::string NextNodeId;
};

/** The open session ended (Leave, or a choice with no next node). */
struct ConversationEnded
{
    std::string PersonId;
    std::uint32_t Session = 0;
};

/** A door's physical state changed: through access rules (door.set) or a reported settlement (door.settle). */
struct DoorChanged
{
    std::string DoorId;
    bool Open = false;
    std::int8_t SwingSide = 0;
    bool Settlement = false;
};

/** P4-09: what an accepted tower.session.apply did to the examination (WorldExamSittings.h). */
enum class ExamTransitionKind : std::uint8_t
{
    Sat,      // Enter accepted (possibly late, inside the entry window)
    Withdrew, // Withdraw accepted, or time called on a late candidate (TimeCalled)
    Deferred,
    Declined,
    Finished // the Choose that cleared the last floor (Completed) or exhausted the reserve (Ejected)
};

/**
 * P4-09: published once, in the accepted command that makes the transition, at the instant the examination stood at (before
 * any outside time the exit charges, whose TimeAdvanced follows). Refused, duplicate and replayed-from-save commands publish
 * nothing; it is presentation output, never saved or hashed. An Enter whose first step already ends the session publishes
 * Sat and then its exit.
 */
struct ExamTransition
{
    ExamTransitionKind Kind = ExamTransitionKind::Sat;
    /** 0 for 2 February 2043, k for 2 February (2043 + k). */
    std::uint32_t Sitting = 0;
    TowerParticipation Participation = TowerParticipation::Unset;
    /** Minutes after the sitting opened that the candidate entered (Sat and the exits of a sat session); 0 otherwise. */
    std::int64_t LateMinutes = 0;
    /** The outside-world minutes this exit charged (0 for Sat, Deferred and Declined). */
    std::int64_t OutsideMinutes = 0;
    /** True when the late candidate's reduced allowance ran out and the world withdrew the session. */
    bool TimeCalled = false;
};

/** Free school commands forward the existing Planned/ArrivedAtSchool DayLoopEvent, stamped with world sequence. */
using EventPayload = std::variant<TimeAdvanced, DayStarted, ScheduledEventFired, WorldFlagChanged, GateRegistered, GateOnset,
                                  GateOnsetHeld, GateContained, GateSealed, ConversationOpened, ConversationChoiceAccepted,
                                  ConversationEnded, DoorChanged, DayLoopEvent, ExamTransition>;

struct DomainEvent
{
    std::uint64_t Seq = 0;
    GameInstant At = 0;
    EventPayload Payload;
};
} // namespace PerfectWorld
