#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Conversation.h"
#include "Doors.h"
#include "FaridaSchedule.h"
#include "NpcBehaviour.h"

#include <string>
#include <string_view>

#include "PerfectWorldLaneLifeSubsystem.generated.h"

class APawn;
class APWDoorActor;
class APWNpcCharacter;
class IPWConversationSession;
namespace PWLane { struct FLaneBuilding; }

/** Small conversions between the portable core's UTF-8 string views and Unreal's strings. */
namespace PWLaneLife
{
PERFECTWORLD_API FString ToFString(std::string_view Text);
PERFECTWORLD_API FText ToText(std::string_view Text);
PERFECTWORLD_API FName ToName(std::string_view Text);
PERFECTWORLD_API std::string ToStd(const FName& Name);

/** Minutes since midnight when play opens: 07:30, until the in-world clock exists (TODO(clock-M1-16)). */
constexpr int32 FixedStartMinuteOfDay = 7 * 60 + 30;
} // namespace PWLaneLife

/**
 * The Unreal owner of the live lane-life bridge: DoorState, per-person ConversationState and Farida's resolved intent.
 * The portable core decides (may this door open, which line does the person say, what does the choice
 * cost); this subsystem keeps the state those functions work on, and Unreal actors and the conversation panel only present
 * the answers. The authoritative world/clock order will take over persistent state and time charging.
 *
 *  Doors   Each APWDoorActor registers at BeginPlay and is set to the core's state for its id. Interact goes through
 *          RequestDoorToggle: the core is asked first (on a copy of the state); only if it allows the move and the leaf can
 *          physically swing is the real state updated. A refusal is shown to the player as a short notice.
 *  Talk    The subsystem installs a session factory on UPWConversationPresenter whose sessions run over the core
 *          conversation functions (PerfectWorldLaneLifeConversation.h).
 *
 * The player is a resident of the opening home. TODO(care-origin): residency should come from the chosen creation origin.
 */
UCLASS()
class PERFECTWORLD_API UPWLaneLifeSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** The subsystem of the world that WorldContext lives in, or null. */
    static UPWLaneLifeSubsystem* Get(const UObject* WorldContext);

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Minutes since midnight (0..1439). There is no in-world clock yet (roadmap M1-16), so this is a fixed morning start,
     * PWLaneLife::FixedStartMinuteOfDay. TODO(clock-M1-16): read the world clock when it exists.
     */
    int32 GetMinuteOfDay() const;

    /** Load the schedule and bind the built shop's world geometry. False means no truthful spawn is available. */
    bool PrepareFarida(const PWLane::FLaneBuilding& Building, FTransform& OutFeet);
    void RegisterFarida(APWNpcCharacter& Person);
    /** TODO(clock-M1-16): call after the authoritative clock changes minute (including jumps/load).
     * Reads GetMinuteOfDay(); repeated calls in the same minute do nothing. */
    void RefreshFaridaForClock();
    const PerfectWorld::NpcIntent* GetFaridaIntent() const { return FaridaIntent ? &*FaridaIntent : nullptr; }

    // ---- Doors ----

    /** Sets the door's start state from the core, or logs an error once if the core has no door with this id. */
    void RegisterDoor(APWDoorActor& Door);

    /** The core's open flag for this door, or Fallback if the core does not know the id. */
    bool IsDoorOpen(FName DoorId, bool bFallback) const;

    /** The player used a door: ask the core, swing the leaf if allowed, show the reason if refused. */
    void RequestDoorToggle(APWDoorActor& Door, APawn* By);

    // ---- Conversation ----

    /** Time the player's choices have committed but nothing has charged yet. TODO(time-contract): charge this through the time contract. */
    int32 GetPendingMinutes() const { return PendingMinutes; }
    void AddPendingMinutes(int32 Minutes) { PendingMinutes += Minutes; }

    /** State is isolated by stable person id. TODO(world-save-v8): persist every person's state with the world. */
    PerfectWorld::ConversationState& MutableConversation(FName PersonId);
    const PerfectWorld::ConversationState& GetConversation(FName PersonId) const;

    /** The live door state. TODO(world-save-v8): persist it with the world. */
    const PerfectWorld::DoorState& GetDoors() const { return Doors; }

    /** Builds a session over the core for a person, or null when the core refuses (unknown person, bad time). */
    TSharedPtr<IPWConversationSession> MakeConversationSession(FName PersonId, const FText& DisplayName);

private:
    void ShowDoorRefusal(const APawn* By, PerfectWorld::DoorResult Result, const PerfectWorld::DoorDef* Def) const;
    /** APWDoorActor::OnSettled: the leaf's real outcome. An interrupted swing overrides the provisional core state. */
    void HandleDoorSettled(FName DoorId, bool bReachedTarget, bool bIsOpenNow, int8 SwingSide);
    /** APWReadableActor::GlobalReadRequested: open the text in this world's reading presenter. */
    void HandleReadRequested(FName ReadableId, FName ContentId, FText Title);
    void ResolveFaridaIntent();
    void HandleFaridaBlock(bool bBlocked, FName RouteId);

    PerfectWorld::DoorState Doors;
    TMap<FName, PerfectWorld::ConversationState> Conversations;
    std::optional<PerfectWorld::FaridaSchedule> FaridaSchedule;
    std::optional<PerfectWorld::NpcIntent> FaridaIntent;
    TWeakObjectPtr<APWNpcCharacter> Farida;
    TMap<FName, FTransform> FaridaAnchors;
    TMap<FName, TArray<FVector>> FaridaRoutes;
    int32 FaridaResolvedMinute = INDEX_NONE;
    bool bFaridaBlocked = false;
    int32 PendingMinutes = 0;

    /** Door ids already reported as missing from the core, so each is logged once. */
    TSet<FName> ReportedUnknownDoors;
};
