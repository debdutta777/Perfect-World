#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * Key remapping (order F4). Every action the walking player can be given a key for, the rules for what may be bound where, and
 * the store that keeps the player's choices on disk. Nothing here knows about Slate or Enhanced Input; PerfectWorldKeyBindingsContext
 * turns a binding set into an Enhanced Input mapping context, and UI/SPWControlsSection edits the store.
 *
 * Each action has two slots: one for the keyboard and mouse, one for a gamepad. A slot holds one key or is unbound. Two actions may
 * not share a key on the same device; binding a key that another action holds is reported as a conflict and the caller may ask for
 * a swap. Escape and the controller's menu button belong to Pause and can neither be unbound nor given to another action.
 */

/** Stable order: the numeric value indexes the tables, and the id string (KbInfo) is what the save file stores. */
enum class EKbAction : uint8
{
    MoveForward,
    MoveBack,
    MoveLeft,
    MoveRight,
    Run,
    Jump,
    Interact,
    CycleTarget,
    ToggleCamera,
    ZoomIn,
    ZoomOut,
    Map,
    Quests,
    Pause,
    Inventory, // R88: append, preserving existing numeric action values and saved string ids
    Count
};

enum class EKbDevice : uint8
{
    Keyboard, // keyboard and mouse buttons
    Gamepad,
    Count
};

enum class EKbGroup : uint8
{
    Movement,
    Actions,
    Camera,
    Interface
};

/** What happened to a request to bind. Applied also means "this key may be bound" when returned by Judge. */
enum class EKbSetResult : uint8
{
    Applied,       // the slot now holds the key
    Swapped,       // the slot holds the key and the action that held it took the slot's old key
    Unchanged,     // the slot already held the key
    Conflict,      // another action holds the key on this device; nothing changed (ask, then retry with swap)
    Reserved,      // Escape and the menu button are Pause's
    NotAllowed,    // not a key that can be bound in this slot
    NotRebindable  // the slot is fixed (the move stick, Pause)
};

constexpr int32 KbActionCount = static_cast<int32>(EKbAction::Count);
constexpr int32 KbDeviceCount = static_cast<int32>(EKbDevice::Count);

struct FKbActionInfo
{
    EKbAction Action = EKbAction::Count;
    const TCHAR* Id = TEXT("");
    EKbGroup Group = EKbGroup::Actions;
    FKey DefaultKeyboard;
    FKey DefaultGamepad;
    bool bKeyboardRebindable = true;
    bool bGamepadRebindable = true;
};

/** The table entry for an action (the Count value is not an action). */
const FKbActionInfo& KbInfo(EKbAction Action);

/** The action with this stable id, if any. */
bool KbFindAction(const FString& Id, EKbAction& OutAction);

/** The left and right stick directions Slate synthesises for menu navigation; never bindable. */
bool KbIsStickDirection(const FKey& Key);

/** The mouse wheel as a pair of keys; only the zoom actions take them. */
bool KbIsWheelKey(const FKey& Key);

FText KbActionLabel(EKbAction Action);
FText KbGroupLabel(EKbGroup Group);
FText KbDeviceLabel(EKbDevice Device);
/** Player-facing name of a key: "Left Shift", "A / Cross", "Mouse Wheel Up"; "Unbound" for an invalid key. */
FText KbKeyLabel(const FKey& Key);

/** The bindings of every action: the defaults plus whatever the player changed. A plain value; the rules live here. */
class FKbBindingSet
{
public:
    /** The defaults, equal to the keys the walking player was hard-coded with before remapping existed. */
    FKbBindingSet();

    FKey Get(EKbAction Action, EKbDevice Device) const;
    static FKey DefaultKey(EKbAction Action, EKbDevice Device);

    bool IsDefault(EKbAction Action, EKbDevice Device) const;
    bool IsDefault(EKbAction Action) const;
    bool IsEveryDefault() const;

    /** Whether this key may go in this slot at all, ignoring conflicts. Applied means it may. */
    static EKbSetResult Judge(EKbAction Action, EKbDevice Device, const FKey& Key);

    /** The other action that holds this key on this device, if any. */
    bool FindHolder(EKbAction Except, EKbDevice Device, const FKey& Key, EKbAction& OutHolder) const;

    /**
     * Binds a key. A conflict is refused with Conflict (OutOther names the holder) unless bSwap is true, in which case the holder
     * takes this slot's old key (or becomes unbound if the slot was). A swap the holder cannot take is NotAllowed.
     */
    EKbSetResult Set(EKbAction Action, EKbDevice Device, const FKey& Key, bool bSwap, EKbAction* OutOther = nullptr);
    /** Unbinds a slot. Pause and the fixed slots refuse. */
    EKbSetResult Clear(EKbAction Action, EKbDevice Device);
    /** Puts a slot back to its default; a conflict is reported or swapped as for Set. */
    EKbSetResult ResetSlot(EKbAction Action, EKbDevice Device, bool bSwap, EKbAction* OutOther = nullptr);
    void ResetAll();

    /** The save text: only slots that differ from the default are listed. */
    FString ToText() const;
    /** Reads save text over the defaults; anything unknown, disallowed or in conflict is dropped. False if the text is not a bindings file. */
    bool FromText(const FString& Text);

    bool operator==(const FKbBindingSet& Other) const;
    bool operator!=(const FKbBindingSet& Other) const { return !(*this == Other); }

private:
    void SetSlot(EKbAction Action, EKbDevice Device, const FKey& Key) { Slots[static_cast<int32>(Action)][static_cast<int32>(Device)] = Key; }
    /** Drops overrides that are disallowed or collide until the set is consistent. */
    void Normalize();

    FKey Slots[KbActionCount][KbDeviceCount];
};

/**
 * The player's bindings on disk, and the one place they are changed. Every change is saved at once and announced on OnChanged, which
 * the walking player listens to so its mapping context is rebuilt while the game runs. The file is Saved/PerfectWorld/KeyBindings.txt,
 * never anything under Config/. Tests build their own store on a temporary path.
 */
class FKbBindingStore
{
public:
    explicit FKbBindingStore(const FString& InPath);

    /** The process-wide store, on the default path. */
    static FKbBindingStore& Get();
    static FString DefaultPath();

    DECLARE_MULTICAST_DELEGATE(FChanged);
    FChanged OnChanged;

    /** The current bindings; reads the file on first use. */
    const FKbBindingSet& Current();
    const FString& GetPath() const { return Path; }

    EKbSetResult Rebind(EKbAction Action, EKbDevice Device, const FKey& Key, bool bSwap, EKbAction* OutOther = nullptr);
    EKbSetResult Unbind(EKbAction Action, EKbDevice Device);
    /** Resets both slots of an action; a key another action took meanwhile is swapped back. */
    void ResetAction(EKbAction Action);
    void ResetAll();

    /** Reads the file (the defaults if it is missing or unreadable). Does not broadcast. */
    bool Load();
    /** Writes the file, or removes it when every binding is the default. */
    bool Save() const;

private:
    void Commit(const FKbBindingSet& Updated);

    FString Path;
    FKbBindingSet Bindings;
    bool bLoaded = false;
};
