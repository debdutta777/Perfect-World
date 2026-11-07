#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * R9-06: the Awakening floor's seam to the world (prefix Aw / PWAwakening).
 *
 * The supervised tutorial floor is PRESENTATION ONLY. It never decides conduct, scores a choice, grants anything or reads a clock for
 * hesitation: every decision, every refusal and every result belongs to the portable core's tutorial state machine (R9-02), reached
 * through IAwFloorBridge. The floor shows what the bridge answers (words, phase, consequence) and moves blocks to match.
 *
 * Like PWSchool::IScGateWorld, the seam is installed per world (AwBridge::Install). The integrator installs the real bridge over
 * WorldSim / the tutorial reducer (R9-02, R9-08); tests and the acceptance driver install FAwScriptedBridge, a recording fake that
 * keeps its own tiny state and says so ("scripted") everywhere it appears.
 *
 * IDS THE FLOOR EXPECTS (R9-02 / R9-03 must offer exactly these, or the integrator edits PerfectWorldAwakeningCatalogue.cpp, the only
 * place they are typed). Situations are the plan's three, in floor order:
 *     awakening.situation.stalled_crossing    awakening.situation.shifting_corridor    awakening.situation.shared_delivery
 * Choices, three per situation plus the universal withdrawal offered at every station in every phase:
 *   stalled_crossing:  awakening.choice.help_classmate   awakening.choice.observe_cycle    awakening.choice.wait_together
 *   shifting_corridor: awakening.choice.protect_barrier  awakening.choice.brace_wall       awakening.choice.lead_longer_route
 *   shared_delivery:   awakening.choice.coordinate_carriers  awakening.choice.investigate_marker  awakening.choice.decline_confrontation
 *   any station:       awakening.choice.withdraw_to_safety
 * Helping, protecting, leading and coordinating, observing, declining a fight and withdrawing all map onto the core's six conduct tags
 * and distinct choice ids on the core side; this layer only forwards (situation, choice).
 */
namespace PWAwakening
{
/** Number of situations on the floor (the plan's three). */
constexpr int32 SituationCount = 3;

namespace Ids
{
inline constexpr const TCHAR* StalledCrossing = TEXT("awakening.situation.stalled_crossing");
inline constexpr const TCHAR* ShiftingCorridor = TEXT("awakening.situation.shifting_corridor");
inline constexpr const TCHAR* SharedDelivery = TEXT("awakening.situation.shared_delivery");

inline constexpr const TCHAR* HelpClassmate = TEXT("awakening.choice.help_classmate");
inline constexpr const TCHAR* ObserveCycle = TEXT("awakening.choice.observe_cycle");
inline constexpr const TCHAR* WaitTogether = TEXT("awakening.choice.wait_together");
inline constexpr const TCHAR* ProtectBarrier = TEXT("awakening.choice.protect_barrier");
inline constexpr const TCHAR* BraceWall = TEXT("awakening.choice.brace_wall");
inline constexpr const TCHAR* LeadLongerRoute = TEXT("awakening.choice.lead_longer_route");
inline constexpr const TCHAR* CoordinateCarriers = TEXT("awakening.choice.coordinate_carriers");
inline constexpr const TCHAR* InvestigateMarker = TEXT("awakening.choice.investigate_marker");
inline constexpr const TCHAR* DeclineConfrontation = TEXT("awakening.choice.decline_confrontation");
inline constexpr const TCHAR* WithdrawToSafety = TEXT("awakening.choice.withdraw_to_safety");

/** Reasons the floor passes to NoteAssistance (physical resets only; the core owns what, if anything, they mean). */
inline constexpr const TCHAR* ReasonLeftTheRoute = TEXT("awakening.assist.left_the_route");
inline constexpr const TCHAR* ReasonResetRequested = TEXT("awakening.assist.reset_requested");
inline constexpr const TCHAR* ReasonSupervisorStepped = TEXT("awakening.assist.supervisor_stepped_in");
} // namespace Ids

/** Where a situation stands, as the tutorial state machine reports it. Nothing here is ever a failure. */
enum class EAwPhase : uint8
{
    Locked,
    Available,
    Active,
    Recovering,
    Complete
};

/** The core's answer to "may this life step onto the floor now?" and to actually stepping on. */
struct FAwEnterResult
{
    bool bAccepted = false;
    /** The core's stable code ("awakening.session.begun", "awakening.session.not_due"), or empty. */
    FString Code;
    /** The core's own wording; shown unedited. */
    FString Message;
    bool bResumed = false;
    bool bMakeUp = false;
};

struct FAwFloorStatus
{
    bool bSessionOpen = false;
    bool bSessionComplete = false;
    EAwPhase Phase[SituationCount] = {EAwPhase::Available, EAwPhase::Available, EAwPhase::Available};
};

/** The core's answer to one forwarded choice, unedited. The floor never overrides, softens or reinterprets it. */
struct FAwChoiceResult
{
    bool bAccepted = false;
    /** Stable code, "awakening.choice.accepted", "awakening.choice.already_done" ... */
    FString Code;
    /** The core's wording (an evidence statement such as "You steadied the crossing"), shown as the caption. May be empty. */
    FString Message;
    /** Names the observable consequence ("awakening.consequence.crossing.aligned"); echoed in the log, never branched on. */
    FName Consequence;
    /** True when the supervisor stepped in (a mistake is met with help and a reset, never failure). */
    bool bAssistance = false;
    EAwPhase Phase = EAwPhase::Available;
    bool bSessionComplete = false;
};

/** Everything the floor needs from the world. */
class IAwFloorBridge
{
public:
    virtual ~IAwFloorBridge() = default;

    /** May the player step onto the floor now (appointment due, make-up scheduled)? False with the core's reason. */
    virtual bool CanEnter(FString& OutWhy) = 0;
    /** The player stepped on: begin or resume the supervised session. */
    virtual FAwEnterResult Enter() = 0;
    /** The phases of the three situations, in floor order. */
    virtual FAwFloorStatus GetStatus() = 0;
    /** Forward one (situation, choice). Includes the withdrawal choice. Always answered; never throws, never times out. */
    virtual FAwChoiceResult SubmitChoice(FName Situation, FName Choice) = 0;
    /** The floor reset the player physically (left the route, asked for a reset). Informational: no scoring from frames or positions. */
    virtual void NoteAssistance(FName Situation, FName Reason) = 0;
    /** The player left the floor (complete or not). */
    virtual void Leave() = 0;
};

/** Per-world registry of floor bridges. A world with none installed has no supervised floor (entering is refused politely). */
namespace AwBridge
{
void Install(UWorld* World, TSharedPtr<IAwFloorBridge> Bridge);
void Remove(UWorld* World);
TSharedPtr<IAwFloorBridge> Resolve(UWorld* World);
} // namespace AwBridge

/**
 * The recording fake. Same contract as the real bridge, keeps its own tiny state, validates ids against the floor's catalogue and
 * records every call, so tests can assert that every choice reached the bridge exactly once. NOT the tutorial rules (R9-02).
 */
class FAwScriptedBridge final : public IAwFloorBridge
{
public:
    struct FCall
    {
        FString Kind; // "enter", "submit", "assist", "leave"
        FName Situation;
        FName Choice;
    };

    virtual bool CanEnter(FString& OutWhy) override;
    virtual FAwEnterResult Enter() override;
    virtual FAwFloorStatus GetStatus() override;
    virtual FAwChoiceResult SubmitChoice(FName Situation, FName Choice) override;
    virtual void NoteAssistance(FName Situation, FName Reason) override;
    virtual void Leave() override;

    /** The next accepted choice at this situation returns bAssistance and Phase Recovering (stays open) instead of Complete. */
    void QueueAssistance(FName Situation) { AssistQueue.AddUnique(Situation); }
    /** A fresh session (a replay for acceptance); real bridges never offer this. */
    void ResetSession();
    void SetPhase(int32 SituationIndex, EAwPhase Phase);
    void SetEntryRefusal(const FString& Why) { EntryRefusal = Why; }

    int32 CountCalls(const TCHAR* Kind) const;
    int32 CountSubmits(FName Situation, FName Choice) const;
    const TArray<FCall>& GetCalls() const { return Calls; }
    const FAwFloorStatus& GetState() const { return State; }

private:
    TArray<FCall> Calls;
    TArray<FName> AssistQueue;
    FAwFloorStatus State;
    FString EntryRefusal;
};
} // namespace PWAwakening
