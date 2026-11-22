#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "WorldSave.h"

/**
 * What the title screen says about saved lives (R5-06). One summary per occupied slot, read through the same
 * PWLaneLifePersistence::ReadWorldSlot that UPerfectWorldCharacterCreatorSubsystem::DescribeSaveSlot uses, and the
 * continue target is the creator's own FindMostRecentSaveSlot, so this is a presentation of the existing projection and
 * not a second source of truth. Nothing here changes a save or the live creator. MakeWorldSlotSummary supplies the
 * portable facts (including age at the saved instant); this layer only formats them for display.
 */
namespace PWTitleR5
{
enum class ESlotState : uint8
{
    /** No file (or no such slot). Never listed. */
    Empty,
    /** A started life, read from its primary file. */
    Ready,
    /** A started life, read from its backup because the primary was lost or damaged. */
    Recovered,
    /** Character creation was saved before the life began; loading resumes creation. */
    Draft,
    /** A file exists but nothing on disk validates. It cannot be loaded and is never repaired here. */
    Unreadable
};

struct FSlotSummary
{
    /** Zero-based save slot. */
    int32 Slot = -1;
    ESlotState State = ESlotState::Empty;
    /** "Slot 2". */
    FText SlotLabel;
    FText Name;
    /** Current age at the saved instant, projected by the core. */
    FText Age;
    /** The in-game date and time of day from the saved world clock: "Tuesday 3 June 2042, 14:30". */
    FText GameTime;
    /** The authored zone name with the saved home city; city alone for an unknown zone, empty if both are absent. */
    FText Place;
    /** The file's real timestamp in local time: "Saved 19 Oct 2026, 14:05". */
    FText SavedAt;
    /** A sentence that must be shown with the slot: backup recovery, unfinished creation, or why it cannot be read. */
    FText Note;
    FDateTime WrittenUtc = FDateTime::MinValue();

    /** Loading it can work. An unreadable slot never can. */
    bool CanLoad() const { return State != ESlotState::Empty && State != ESlotState::Unreadable; }
    /** Everything shown, in reading order, as one line for tests and accessibility text. */
    FText Compose() const;
};

enum class EContinueState : uint8
{
    /** Nothing is saved: Continue is hidden. */
    None,
    /** The most recent save can load: Continue is offered with its summary. */
    Ready,
    /** The most recent save cannot be read: Continue is disabled and says why. */
    Blocked
};

/** Every occupied slot, newest first, and which one Continue means. */
struct FTitleSaves
{
    TArray<FSlotSummary> Slots;
    /** The slot Continue loads (the creator's FindMostRecentSaveSlot), or -1. */
    int32 ContinueSlot = -1;

    const FSlotSummary* Find(int32 Slot) const;
    const FSlotSummary* ContinueTarget() const { return Find(ContinueSlot); }
    EContinueState GetContinueState() const;
    int32 LoadableCount() const;
    bool HasAny() const { return Slots.Num() > 0; }
};

/** "Saved 19 Oct 2026, 14:05" for a local time. Fixed English month names, like the rest of the front end's save text. */
FText FormatSavedAt(const FDateTime& Local);

/** "Tuesday 3 June 2042, 14:30" for a game clock reading, from the core's own calendar. */
FText FormatGameTime(int64 GameInstantMinutes);

/** The summary of one successfully read slot. WrittenUtc is the file's timestamp as the file system reports it. */
FSlotSummary SummariseWorldSave(int32 Slot, const PerfectWorld::WorldSaveData& Data, const FDateTime& WrittenUtc,
                                bool bRecoveredFromBackup, const FString& Notice);

/** The summary of a slot that exists but cannot be read. Notice is the reader's player-facing sentence, if any. */
FSlotSummary SummariseUnreadable(int32 Slot, const FDateTime& WrittenUtc, const FString& Notice);

/** Newest first; equal times by slot number; Pinned (when it is present) goes first so Continue's slot always leads. */
void SortNewestFirst(TArray<FSlotSummary>& Slots, int32 PinnedSlot);

/** Reads one slot from disk. Never throws or crashes on a missing, truncated or damaged file; Empty when there is none. */
FSlotSummary ReadSlot(int32 Slot);

/** Reads every slot from disk, as the title does when it opens. */
FTitleSaves ReadTitleSaves();
} // namespace PWTitleR5
