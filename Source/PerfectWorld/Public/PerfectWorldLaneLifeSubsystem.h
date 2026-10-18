#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Conversation.h"
#include "Doors.h"
#include "TimeContract.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <string>
#include <string_view>

#include "PerfectWorldLaneLifeSubsystem.generated.h"

class APawn;
class APWDoorActor;
class IPWConversationSession;
class SPWClockReadout;
class SWeakWidget;
namespace PWLane { struct FLaneBuilding; }

DECLARE_LOG_CATEGORY_EXTERN(LogPWLaneLife, Log, All);

/** Small conversions between the portable core's UTF-8 string views and Unreal's strings. */
namespace PWLaneLife
{
PERFECTWORLD_API FString ToFString(std::string_view Text);
PERFECTWORLD_API FText ToText(std::string_view Text);
PERFECTWORLD_API FName ToName(std::string_view Text);
PERFECTWORLD_API std::string ToStd(const FName& Name);

/** Minutes since midnight at the start of play: 07:30 on 2 June 2042, the creation start. */
constexpr int32 CampaignStartMinuteOfDay = 7 * 60 + 30;
} // namespace PWLaneLife

/** Fired after the world clock moved (free roam, a paid conversation choice, a reset). Carries the new GameInstant. */
DECLARE_MULTICAST_DELEGATE_OneParam(FPWLaneClockChanged, int64 /*Instant*/);

/** What the core said to a command the subsystem submitted for the player. */
struct FPWLaneCommandOutcome
{
    bool bAccepted = false;
    /** The core's stable rejection code ("door.locked", "time.asleep"), or empty when accepted. */
    FString Code;
    /** Player-facing wording of the refusal; empty when accepted or when the refusal needs no words (a stale click). */
    FText Reason;
};

/** Why the free-roam clock is standing still right now. */
enum class EPWClockHold : uint8
{
    None,
    /** Title, creation or pause menu. */
    Menu,
    Conversation,
    Reading,
    /** The window is in the background. */
    Unfocused
};

/**
 * The Unreal owner of the live lane-life bridge. It holds the ONE portable world (PerfectWorld::WorldSim): the clock, every
 * door and every person's conversation state live there and nowhere else. The portable core decides (may this door open, which
 * line does the person say, what does the choice cost, what time is it); this subsystem submits commands and presents the answers.
 *
 *  Clock   Free roam is tick-driven: real time becomes whole game minutes (TimeContract AdvanceRealTime: 60 real seconds = 60
 *          game minutes) and goes in as time.advance. The clock holds while any front-end menu, the conversation panel or the
 *          reading panel is open, and while the window is unfocused. A Partial advance is resubmitted for the remainder and the
 *          world is settled (time.settle) until it is quiescent.
 *  Doors   door.set through the core's access rules (residency from the lane origin), then the leaf swings; the leaf's real
 *          outcome is reported back with door.settle (Presentation source). A refusal is shown as a short notice.
 *  Talk    The subsystem installs a session factory on UPWConversationPresenter whose sessions run over conversation.open,
 *          conversation.choose and conversation.leave (PerfectWorldLaneLifeConversation.h). A paid choice moves the clock inside
 *          its own command.
 *
 * The player is a resident of the opening home. TODO(care-origin): residency should come from the chosen creation origin.
 */
UCLASS()
class PERFECTWORLD_API UPWLaneLifeSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    /** The subsystem of the world that WorldContext lives in, or null. */
    static UPWLaneLifeSubsystem* Get(const UObject* WorldContext);

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // FTickableGameObject (through UTickableWorldSubsystem). Not ticked while the engine is paused.
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPWLaneLifeSubsystem, STATGROUP_Tickables); }

    // ---- The world and its clock ----

    /** The authoritative world. Read only; every change is a command. */
    const PerfectWorld::WorldState& GetWorldState() const { return Sim->State(); }

    /** The world clock's instant, in game minutes since the campaign epoch (Monday 2 June 2042, 06:00). */
    PerfectWorld::GameInstant GetNow() const { return Sim->State().Clock.Now(); }

    /** Minutes since midnight (0..1439), read from the world clock. */
    int32 GetMinuteOfDay() const;

    /** The clock for the top-right readout, for example "Mon 2 Jun · 07:42". */
    FText GetClockText() const;

    /** None while free roam is running; otherwise why it is standing still. */
    EPWClockHold GetClockHold() const { return ClockHold; }

    FPWLaneClockChanged OnClockChanged;

    /**
     * Starts a fresh world: a new clock at 07:30 on 2 June 2042, every door at its authored state, no conversation memory.
     * Registered doors and scheduled people are brought into step. Loads use AdoptSavedWorld instead.
     */
    void ResetWorld();

    /** Commits a fully validated, detached world; its saved conversation must already be closed. */
    void AdoptSavedWorld(TUniquePtr<PerfectWorld::WorldSim> Prepared);
    /** Validates and closes the saved session before committing any live state. */
    bool LoadWorldSlot(const std::string& Path, PerfectWorld::CharacterCreator& Creation,
        std::optional<PerfectWorld::PlayerPlacement>& Placement, std::string& Message);
    bool HasMovingDoors() const;
    void SettleForSave() { SettleUntilQuiescent(); }
    /** Counts completed presentation resets for restore diagnostics and flow automation. */
    uint64 GetRestorePresentationCount() const { return RestorePresentationCount; }

    // ---- People ----

    /** Bind the built tea shop, register its scheduled proprietor and reconcile at the world clock's instant. */
    bool RegisterTeaShopPresence(const PWLane::FLaneBuilding& Building);

    // ---- Doors ----

    /** Sets the door's start state from the core, or logs an error once if the core has no door with this id. */
    void RegisterDoor(APWDoorActor& Door);

    /** Authored inside half-plane: +1 toward the closed door actor's local +Y, -1 toward -Y. */
    void RegisterDoorInsideSide(const APWDoorActor& Door, double InsideSide);

    /** The core's open flag for this door, or Fallback if the core does not know the id. */
    bool IsDoorOpen(FName DoorId, bool bFallback) const;

    /** The player used a door: door.set through the core, swing the leaf if allowed, show the reason if refused. */
    void RequestDoorToggle(APWDoorActor& Door, APawn* By);

    // ---- Conversation ----

    /** Builds a session over the core for a person (conversation.open), or null when the core refuses. */
    TSharedPtr<IPWConversationSession> MakeConversationSession(FName PersonId, const FText& DisplayName);

    /** The person's conversation state in the world, or null when they have never been spoken to. */
    const PerfectWorld::ConversationState* FindConversation(FName PersonId) const;

    /** conversation.choose for the open session numbered Session. A paid choice moves the clock inside this command. */
    FPWLaneCommandOutcome ChooseConversation(FName PersonId, FName ChoiceId, uint32 Session);

    /** conversation.leave; harmless when nothing is open. */
    void LeaveConversation(FName PersonId);

    /** Shows a short notice through the local player's interaction component. */
    void ShowPlayerNotice(const FText& Message, const APawn* By = nullptr) const;

private:
    // ---- Clock (PerfectWorldLaneLifeClock.cpp) ----
    void BuildWorld();
    /** Submits one command, presents its events and, when the clock moved, tells the people. */
    PerfectWorld::CommandResult SubmitCommand(const PerfectWorld::Command& Command);
    /** time.settle until nothing is due at this instant. */
    void SettleUntilQuiescent();
    /** time.advance by whole minutes, resubmitting the remainder of a Partial advance. */
    void AdvanceClock(int64 Minutes);
    /** Refreshes ClockHold from the menus, panels and window focus; true when the clock must stand still. */
    bool UpdateClockHold();
    /** Leaves a core conversation whose panel is not on screen, so time can move again. */
    void CloseOrphanedConversation();
    void HandleClockMoved();
    void EnsureClockReadout();
    void RemoveClockReadout();
    void PresentEvents(const std::vector<PerfectWorld::DomainEvent>& Events) const;

    // ---- Doors and conversation (PerfectWorldLaneLifeSubsystem.cpp) ----
    void ResolveOriginResidency();
    bool IsResidentOf(const PerfectWorld::DoorDef& Door) const;
    void ShowDoorRefusal(const APawn* By, PerfectWorld::DoorResult Result, const PerfectWorld::DoorDef* Def) const;
    /** APWDoorActor::OnSettled: the leaf's real outcome, reported to the core with door.settle. */
    void HandleDoorSettled(FName DoorId, bool bReachedTarget, bool bIsOpenNow, int8 SwingSide);
    /** APWReadableActor::GlobalReadRequested: open the text in this world's reading presenter. */
    void HandleReadRequested(FName ReadableId, FName ContentId, FText Title);

    TUniquePtr<PerfectWorld::WorldSim> Sim;
    uint64 RestorePresentationCount = 0;
    PerfectWorld::TimeModeState ClockMode;
    EPWClockHold ClockHold = EPWClockHold::Menu;
    /** Real milliseconds below one whole millisecond, carried between ticks. */
    double RealMsFraction = 0.0;
    /** The last time.advance refusal that was logged, so a standing refusal (asleep) is reported once. */
    FString LastClockRefusal;
    PerfectWorld::GameInstant LastBroadcastInstant = -1;

    TSet<FName> ResidentLocations;
    TArray<TWeakObjectPtr<APWDoorActor>> RegisteredDoors;
    /** Door ids already reported as missing from the core, so each is logged once. */
    TSet<FName> ReportedUnknownDoors;

    TMap<FName, double> DoorInsideSides;

    TSharedPtr<SPWClockReadout> ClockReadout;
    TSharedPtr<SWeakWidget> ClockHolder;
};
