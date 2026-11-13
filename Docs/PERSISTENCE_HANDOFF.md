# World persistence submission A

## Submission B

Implemented, pending integrator compilation and execution (2026-10-17). Completion criterion: wire Start/Save/Load/Continue to v8 storage and detached restore, install saved doors and NPC presentation once, safely restore placement or use the origin, and author `PerfectWorld.PersistenceFlow` automation. No commits, Config edits, portable-core edits or Unreal builds. Integrator owns engine compilation, automation execution and packaged restart acceptance. Submission A's older proposals below are historical; this section describes the actual B code.

Implementation: moved slot mutations into the allowed creator Entry translation unit to keep files below 500 lines; added LaneLife adoption; shared entry collision/floor logic in GameMode.cpp through a private persistence declaration (the public GameMode header is outside this submission's ownership). Reads and conversation closure finish before the live commit. Open panels and seated/transitional pawns refuse replacement before mutation. New Creation also refuses those states before discarding the draft.

Files changed (relative to the worktree):

- `Source/PerfectWorld/Public/PerfectWorldCharacterCreatorSubsystem.h`: pending placement and replacement guard API.
- `Source/PerfectWorld/Private/PerfectWorldCharacterCreatorSubsystem.cpp`: world-aware slot descriptions, reset clears pending placement; slot mutations moved out.
- `Source/PerfectWorld/Private/PerfectWorldCharacterCreatorSubsystemEntry.cpp`: Start, Save and Load integration and transient-state guard.
- `Source/PerfectWorld/Public/PerfectWorldLaneLifeSubsystem.h`: validated load/adoption, settlement and reset-count diagnostic APIs.
- `Source/PerfectWorld/Private/PerfectWorldLaneLifeClock.cpp`: detached load transaction, adoption, door restoration and NPC reset.
- `Source/PerfectWorld/Private/PerfectWorldLaneLifeSubsystem.cpp`: corrected the late-door-registration comment; behavior already read saved swings.
- `Source/PerfectWorld/Private/PerfectWorldFrontEndHUD.cpp`: removes Start's ResetWorld, guards New Creation, consumes pending placement, reports fallback.
- `Source/PerfectWorld/Private/PerfectWorldBlockTestGameMode.cpp`: shared floor/capsule/occupancy tests, bounded origin alternatives and saved-entry seam.
- `Source/PerfectWorld/Private/PerfectWorldLaneLifePersistence.h`: declares the engine placement seam; storage implementation and A's tests remain unchanged.
- `Source/PerfectWorld/Private/PerfectWorldLaneLifePersistenceFlowTests.cpp`: four new engine automation cases.
- `Docs/PERSISTENCE_HANDOFF.md`: this record. `Docs/STATUS.md` is outside the allowed edit list; integrator should copy this validation record there.

Exact paths:

| Player path | Behavior |
|---|---|
| Start (`StartNewLife` / `StartLifeAndSaveToSlot`) | Starts a copy of the full draft, builds a new core world (never captures the old lane), advances it from 06:00 to 07:30, captures and transactionally writes v8, then commits the creator and prepared sim. Write failure retains the entire draft, old live world and usable slot. Fresh placement is the core's home-zone zero marker; safe entry resolves it and explicitly sets first person. |
| Pause Save (`SaveActiveGame` / `SaveGameToSlot` / `SaveToSlot`) | Captures the actual lane clock, doors with settled swings, conversation state/receipts, integer capsule-centre position, control yaw/pitch and explicit camera mode. Settles due events through the existing bounded loop before capture. Refuses seated/transitional pawns, moving doors or invalid capture. Failed writes return the storage reason through the existing Save UI; the prior usable slot remains available. Draft saves retain the creator-only format. |
| Load (`LoadSlot` / `LoadFromSlot` / `LoadWorldSlot`) | Guards panels/seating; validates the full primary or recovered backup through A/core migrations and validates a scratch creator. A saved open talk receives only free ConversationLeave on the detached sim, retaining facts, choices, session count and receipts. Only then replaces creator/world and pending placement. No ResetWorld. Applies actor door states/swings without callbacks, resets clock fractions/mode/hold, calls NPC ResetPresentation once at restored time, and broadcasts the clock once. A refused read/validation changes no live world, creator, actors or pending placement. |
| Continue | Selects the most recently written occupied slot using the existing timestamp policy, then calls exactly LoadSlot. No separate reset or save. |

Placement runs AFTER restored doors and NPC presentation. Coordinates resume only in `zone.street.nabapara_lane`, the one currently built zone including interiors. A short floor probe (20 cm above / 30 cm below saved feet), walkable normal, full standing capsule clearance and separate pawn-occupancy query must pass. The centre is grounded 2 cm above the floor; yaw/pitch/camera are applied explicitly and velocity is stopped. Otherwise it calls PlacePlayerAtOpeningHome, trying its canonical point and six bounded 80 cm offsets. Fallback preserves the saved camera, uses safe home facing and logs `Persistence: saved placement ...; used safe origin placement.` Load/Continue also toast that home was used. A supported matching home-zone zero marker uses this same origin path; other home coordinates/unknown zones fall back. No fallback rewrites the file. No safe origin: leave pawn unmoved, menus/input held, report entry failure and retain pending placement for another attempt. At that stage the validated world has already been adopted; this is an entry failure, not a rejected slot read.

Pending placement clears on successful entry, ResetCreation, UI BeginNewCreation and successful draft load. New Start replaces it with the fresh marker, so neither an old camera nor an old placement leaks into a new life. Conversation and reading panels must be closed before Load/Continue/New Creation/Start; no callback-free panel or seat reset was added. Loading a mid-conversation FILE is supported, while replacing a currently open live panel is refused. Old saved-session choice callbacks are rejected by the closed core session without another charge.

Automation authored (not executed): `PerfectWorld.PersistenceFlow.RestoresWorldOnce`, `.RefusalLeavesWorldUntouched`, `.ClosesSavedTalkWithoutCharge`, `.PlacementRestoreAndFallback`. The first uses the production load transaction, a registered door actor and actual care-worker presenter; saves memory/receipts and a negative swing, advances time, flips the door and takes another paid choice, then compares the restored whole-world hash and reset count (+1), actor replacement and clock publication. Refusal damages an isolated slot with no backup. Placement exercises actual collision with flat floors/blockers, both origins/cameras, wrong zone, missing support and no safe origin. Tests use isolated world/slot fixtures; they do not click Slate or overwrite player slots. A's existing tests cover storage interruption, backup recovery and v1-v7 migration.

Validation performed: source review and deterministic first-header / under-500-line audit of all ten touched source files; confirmed four automation registrations and no ResetWorld call in HUD. Inspected the readout binding: it reads GetClockText directly through a weak subsystem pointer, so it observes the adopted sim. Read project instructions, workflow section 4, master/system/roadmap/build/lane documents and A's restore policy. No Ruflo tool is exposed. Git and rg are unavailable on PATH; PowerShell was used for inspection. No CMake/CTest rerun (portable core unchanged), engine compilation, automation, PIE or packaged run. The owner's stated A baseline (60/60, Persistence 5/5) was not rerun or independently claimed here. No verification rung is claimed for B.

Assumptions: synchronous game-thread slot operations; one built lane zone; the existing two family/care interior frames remain authoritative; no persistent seated or partially moving-door pose. New worlds start 07:30; creator-only migrated worlds keep the core's 06:00. Migration flags are preserved through the scratch creator, not inferred from HashVerified. A's physical-power-loss and non-authenticated placement-field limitations still apply.

Integrator acceptance:

1. Compile and run `Automation RunTests PerfectWorld.PersistenceFlow` (four new cases), then `PerfectWorld.Persistence` and the full regression set. Confirm actual discovery/results; these bodies have not compiled here.
2. In a packaged build, Start a life and confirm initial v8 slot/07:30/home/first person. Open one two-way door on each swing side, leaving a recognizable open/closed arrangement. At a suitable time, take the paid evening-jobs topic and agree to dishes on weekdays. Close the talk, stand in a clear supported spot, choose a camera and facing, then pause Save. Record the displayed time and door states/sides.
3. Quit completely, relaunch, Continue. Confirm the recorded time (allow normal free-roam advancement after release), the same door arrangement and swing sides, the recorded supported position/view/camera, and no second paid-time charge. Reopen the household talk: the settled line must remember dishes on weekdays (or next morning, “Good morning. You suggested taking the dishes on weekdays. Shall we keep it that way?”). No start acknowledgement on Continue.
4. Repeat with care origin and the other camera. Exercise unsafe/unsupported placement fallback: origin entry, explicit fallback log/toast and unchanged file. Also exercise corrupt primary with no usable backup while a world is live: player-facing refusal, unchanged time/doors/memory/placement and no NPC reset. With a valid backup, expect recovery instead of refusal. Simulate an unwritable slot and confirm clear Save failure plus prior-slot recovery after restart.

## Submission A historical record

Storage and validation only; integration remains with the integrator. No commits or Unreal builds authorised.

Completion criterion: the three new private files provide capture, atomic slot storage and validated reads, with focused `PerfectWorld.Persistence` automation. Restore and placement seams are supplied here as proposed code, not applied to shared files.

Implemented the three new private files. Reuses `WriteSaveFile` / `ReadSaveFile` with `WorldSaveValidator`; an empty world serialisation refuses before disk access. Reads return a candidate only. Preserves core SourceVersion/HashVerified/Migrated semantics, including unverified historical saves. A retry after backup recovery removes only the invalid primary before invoking the shared writer, protecting the only usable backup from rotation. Automation covers repeated replacement and failed retry after recovery.

Restore policy: close the saved open conversation on a detached WorldSim before committing it. Keep facts, taken choices, session counter and receipts; never replay conversation choices or charge time during restore.

Validation complete for this submission: full portable baseline built; CTest passed 164/164 (63.33 seconds). The storage .cpp compiles standalone with C++20 `/W4 /WX`; all five exact automation bodies pass in the portable harness. Initial VS-generator configuration failed (compiler not found); loading `vcvars64.bat` and using NMake resolved it. Unreal compilation, automation execution and packaged restart are integrator work. No Ruflo routing/shared-memory tool is exposed in the available tool inventory. Git/rg executables are unavailable on this shell; installed user-profile tool directories are denied by the sandbox.

## Delivered API

All functions are in `PWLaneLifePersistence`, declared by `Source/PerfectWorld/Private/PerfectWorldLaneLifePersistence.h`. They contain no engine dependency and run synchronously, with one writer per slot.

```cpp
CaptureResult CaptureWorldSave(const PerfectWorld::CharacterCreationSave& Creation,
    const PerfectWorld::WorldState& World, const PerfectWorld::PlayerPlacement& Player);
PerfectWorld::SaveOperationResult WriteWorldSlot(const std::string& AbsolutePath, std::string_view Text);
ReadResult ReadWorldSlot(const std::string& AbsolutePath);
```

`CaptureResult`: `Data` (optional), `Text`, `Validation`, `Succeeded()`. Failure has no data or text. Capture refuses an unsettled world, empty lane serialisation and any envelope the full core reader refuses (including placement and creation).

`ReadResult`: `Data` (optional), `Status`, `Validation`, `Message`, `Succeeded()`. `Validation.SourceVersion`, `.HashVerified`, `.Migrated` are the **original core result**, before any presentation/restore transformation. A refusal has no data. A recovered backup has its own metadata and a recovery notice. A bad primary with a usable backup succeeds by recovery; a bad file with no usable backup is refused. Read never rewrites files.

Unchanged path convention: pass the absolute UTF-8 form of `GetSaveSlotPath(SlotIndex)`; do not introduce a second set of world filenames. Keep draft writes creator-only; all reads and descriptions accept both creator-only and world envelopes through the new reader. Existing occupancy, deletion and most-recent-slot methods already consider `.bak` and remain applicable.

## Restore contract (integrator acceptance checklist)

- [ ] Hold menu/pause input throughout loading. Read and fully validate a detached candidate before changing Creator, Sim, actors, pawn, camera, panels or residency. Refusal keeps the current life intact and displays `Read.Message`.
- [ ] Restore the candidate creation record into a scratch `CharacterCreator`. Refuse before mutation if that fails. Draft saves return to the creator; do not install their migrated fresh world.
- [ ] Build a detached `WorldSim(candidate.World)`. If one conversation is open, submit **only** `ConversationLeave` to this detached sim. Refuse if Leave fails. Do not use the live subsystem's `SubmitCommand`: it reconciles NPCs for every accepted command.
- [ ] Replace the live Sim with this prepared sim; do not call `BuildWorld`, `ResetWorld`, `BeginWorldFromCreation` or advance to 07:30 on Load/Continue. Core legacy migration starts at 06:00 and must remain at that instant. Only New Life starts fresh at 07:30.
- [ ] Clear clock presentation carry: `ClockMode`, `RealMsFraction`, `LastClockRefusal`, `LastBroadcastInstant`; hold `ClockHold = Menu` until entry succeeds. Refresh residency from the newly installed Creator.
- [ ] Apply each saved door entry to each registered actor with `SetOpenImmediate(Entry.Open, Entry.SwingSide)`. Cancel pre-load animation targets. Future door registration must read the installed Sim as it already does. Do not send door commands or replay old events during restore.
- [ ] After door installation, call `ResetPresentation(GetNow())` **once**. Do not additionally call `Reconcile`, `HandleClockMoved` or `SubmitCommand` in this restore path. Broadcast `OnClockChanged` once, with `LastBroadcastInstant` already set to the restored instant.
- [ ] Close/discard pre-load reading and conversation panels and invalidate their queued callbacks while input remains held. Do this before adoption; never allow a panel's old Leave callback to target the new Sim. The current presenter lacks a callback-free public dismissal seam: add it or require panels closed before Load is offered.
- [ ] Resolve the saved stable zone through the built-zone registry. Position is a standing capsule centre in world centimetres; yaw/pitch are controller angles. Reject unsupported zones instead of teleporting into unbuilt geometry. The zero origin-home position produced by core creator-only migration is an origin-spawn marker; resolve it through the safe home-entry path.
- [ ] Restore standing capsule/movement state, stop velocity, and validate floor support, slope, full capsule clearance and NPC occupancy **after** saved doors and NPCs are installed. Try bounded nearby grounded alternatives, then the resolved origin's safe spawn. If none is safe, keep menus/input held and show a refusal; do not unpause inside a wall. No save rewrite on a fallback.
- [ ] Apply centidegree yaw/pitch and camera mode explicitly (no toggle), dismiss the title camera, flush held inputs and release the menu only after placement succeeds. Both cameras and both origins need packaged evidence.
- [ ] Clear any pending placement after successful entry, reset/new creation or draft load. A later failed load must not accidentally consume a previous pending placement.

**Mid-conversation choice: close on load.** There is no persistent presenter or NPC pose to resume reliably. Free Leave retains facts, taken choices, session count and receipts; paid clock movement already happened atomically in the saved reducer command. Never replay a `ConversationChoose`, reapply choice effects, or charge from a receipt. Old callbacks are discarded; if one reaches the reducer it is rejected while the session is closed, and after reopening its old session number is stale. A repeatable paid topic chosen deliberately in a *new* session is a new transaction, consistent with core rules. `RoundTrip` tests detached Leave, retained receipts and rejection without a second charge. Closing adds one legitimate reducer sequence step; do not expect the post-restore hash to equal the original file hash. `HashVerified` still describes the original read.

## Proposed shared-file code changes (not applied)

These snippets name the integration edits against this checkout. The integrator owns these files. Include their owning headers first; add the necessary includes shown by the types. Split slot operations out of `PerfectWorldCharacterCreatorSubsystem.cpp` if the result would exceed 500 lines (it currently has 460). No Config changes are required.

### LaneLife adoption seam

Add to `PerfectWorldLaneLifeSubsystem.h` public section:

```cpp
// Takes only a WorldSim built from a fully validated save, with its saved talk closed.
void AdoptSavedWorld(TUniquePtr<PerfectWorld::WorldSim> Prepared);
bool HasMovingDoors() const;
```

Implement in the integrator's LaneLife restore translation unit (or Clock.cpp after moving another method to keep it below 500 lines). Include door and NPC subsystem headers:

```cpp
void UPWLaneLifeSubsystem::AdoptSavedWorld(TUniquePtr<PerfectWorld::WorldSim> Prepared)
{
    Sim = MoveTemp(Prepared);
    ClockMode = PerfectWorld::TimeModeState{};
    RealMsFraction = 0.0;
    LastClockRefusal.Reset();
    LastBroadcastInstant = GetNow();
    ClockHold = EPWClockHold::Menu;
    ResolveOriginResidency();
    for (const auto& Weak : RegisteredDoors)
    {
        APWDoorActor* Door = Weak.Get();
        if (Door == nullptr) continue;
        for (const auto& Entry : Sim->State().Doors.Doors)
        {
            if (Entry.Id == PWLaneLife::ToStd(Door->GetDoorId()))
            {
                Door->SetOpenImmediate(Entry.Open, static_cast<int8>(Entry.SwingSide));
                break;
            }
        }
    }
    if (auto* Presence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
        Presence->ResetPresentation(GetNow());
    OnClockChanged.Broadcast(GetNow());
}

bool UPWLaneLifeSubsystem::HasMovingDoors() const
{
    for (const auto& Weak : RegisteredDoors)
        if (const auto* Door = Weak.Get(); Door != nullptr && Door->IsMoving()) return true;
    return false;
}
```

Inspected `SetOpenImmediate`: it cancels movement, disables ticking, installs angle/target and does not raise `OnSettled`, so it fits this restore seam. Do not copy the ResetWorld body with its initial `BuildWorld()` call.

### Creator LoadFromSlot and slot description

Public header adds `#include "WorldSave.h"` before its generated header, plus these non-UFUNCTION accessors and private field:

```cpp
// public:
const std::optional<PerfectWorld::PlayerPlacement>& GetLoadedPlacement() const { return LoadedPlacement; }
void ClearLoadedPlacement() { LoadedPlacement.reset(); }
// private:
std::optional<PerfectWorld::PlayerPlacement> LoadedPlacement;
```

Replace `LoadFromSlot` with this body. Add private persistence header and LaneLife header in its .cpp. The HUD must refuse Load while reading/conversation panels are open until a callback-free discard seam exists.

Also include `PerfectWorldConversationPresenter.h` and `PerfectWorldReadingPresenter.h` for the guard below. Apply the same guard to New Life/adoption, so a panel from a previous life cannot address the fresh world.

```cpp
bool UPerfectWorldCharacterCreatorSubsystem::LoadFromSlot(int32 SlotIndex)
{
    const auto* Talk = UPWConversationPresenter::Get(this);
    const auto* Reading = UPWReadingPresenter::Get(this);
    if ((Talk != nullptr && Talk->IsOpen()) || (Reading != nullptr && Reading->IsOpen()))
        return RecordFileError(FText::FromString(TEXT("Close the conversation or reading panel before loading.")));
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty())
        return RecordFileError(FText::FromString(TEXT("That save slot does not exist.")));
    auto Read = PWLaneLifePersistence::ReadWorldSlot(
        TCHAR_TO_UTF8(*FPaths::ConvertRelativePathToFull(Path)));
    if (!Read.Succeeded())
        return RecordFileError(FText::FromString(ToFString(Read.Message)));
    PerfectWorld::CharacterCreator Scratch;
    if (!RecordResult(Scratch.RestoreSave(Read.Data->Creation))) return false;
    UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(this);
    TUniquePtr<PerfectWorld::WorldSim> Prepared;
    if (Scratch.HasStarted())
    {
        if (Life == nullptr)
            return RecordFileError(FText::FromString(TEXT("The world is not ready to load.")));
        Prepared = MakeUnique<PerfectWorld::WorldSim>(Read.Data->World);
        const std::string Person(PerfectWorld::OpenConversationPersonId(Prepared->State()));
        if (!Person.empty() && !Prepared->Submit({PerfectWorld::CommandSource::Player,
                PerfectWorld::ConversationLeave{Person}}).Accepted)
            return RecordFileError(FText::FromString(TEXT("The saved conversation could not be closed.")));
    }
    // All parsing and rule validation finished before this commit point.
    Creator = MoveTemp(Scratch);
    LoadedPlacement.reset();
    if (Prepared)
    {
        LoadedPlacement = Read.Data->Player;
        Life->AdoptSavedWorld(MoveTemp(Prepared));
    }
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::FromString(ToFString(Read.Message));
    return true;
}
```

For `DescribeSaveSlot`, replace the creation-only `ReadSaveFile` / `DeserializeCreationSave` block with:

```cpp
const auto Read = PWLaneLifePersistence::ReadWorldSlot(ToSlotPathUtf8(Path));
if (Read.Status == PerfectWorld::SaveReadStatus::Missing)
    return FText::FromString(TEXT("Empty"));
if (!Read.Succeeded()) return FText::FromString(TEXT("Unreadable file"));
const PerfectWorld::CharacterCreationSave& Parsed = Read.Data->Creation;
// Keep the existing display-name/household formatting below this point.
```

Clear `LoadedPlacement` in `ResetCreation`, `BeginNewCreation` and successful New Life. Do not clear it as a side effect of reading a slot description. Retain core migration-confirmation flags; do not infer `Migrated` from `HashVerified == false` (v6/v7 and historical door-roster migrations also load unverified).

### SaveToSlot: started lives become world envelopes

Keep the existing out-of-range handling and creator-only draft branch. For a started life replace the creation-only serialisation/write with the following (includes PlayerCharacter, LaneLife and PlayerController). Refuse seated/moving doors rather than saving presentation state v8 cannot represent. Add `bool HasMovingDoors() const` to LaneLife, implemented by querying registered door actors' existing motion state; the door actor currently exposes `IsMoving()`.

```cpp
const auto* Life = UPWLaneLifeSubsystem::Get(this);
const auto* Controller = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
const auto* Pawn = Controller != nullptr ? Cast<APerfectWorldPlayerCharacter>(Controller->GetPawn()) : nullptr;
if (Life == nullptr || Pawn == nullptr || Pawn->IsSeated() || Life->HasMovingDoors())
    return RecordFileError(FText::FromString(TEXT("Stand still and let the doors settle before saving.")));
PerfectWorld::PlayerPlacement Placement;
Placement.ZoneId = "zone.street.nabapara_lane"; // Current built lane is one loaded zone, including its interiors.
const FVector Position = Pawn->GetActorLocation();
if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) || !FMath::IsFinite(Position.Z))
    return RecordFileError(FText::FromString(TEXT("Your position could not be saved.")));
for (int32 Axis = 0; Axis < 3; ++Axis)
{
    if (FMath::Abs(Position[Axis]) > PerfectWorld::WorldSavePositionLimitCm)
        return RecordFileError(FText::FromString(TEXT("Your position is outside the saved world.")));
    Placement.PositionCm[Axis] = FMath::RoundToInt64(Position[Axis]);
}
const FRotator Look = Controller->GetControlRotation();
if (!FMath::IsFinite(Look.Yaw) || !FMath::IsFinite(Look.Pitch))
    return RecordFileError(FText::FromString(TEXT("Your view could not be saved.")));
Placement.YawCdeg = FMath::RoundToInt(FRotator::ClampAxis(Look.Yaw) * 100.0) % 36000;
Placement.PitchCdeg = FMath::RoundToInt(FRotator::NormalizeAxis(Look.Pitch) * 100.0);
Placement.Camera = Pawn->IsFirstPerson() ? PerfectWorld::CameraMode::FirstPerson : PerfectWorld::CameraMode::ThirdPerson;
const auto Capture = PWLaneLifePersistence::CaptureWorldSave(Creator.CaptureSave(), Life->GetWorldState(), Placement);
if (!Capture.Succeeded()) return RecordFileError(FText::FromString(ToFString(Capture.Validation.Message)));
const auto Written = PWLaneLifePersistence::WriteWorldSlot(ToSlotPathUtf8(Path), Capture.Text);
if (!Written.Ok) return RecordFileError(FText::FromString(ToFString(Written.Message)));
// Keep existing success LastError/LastErrorMessage below this point.
```

The explicit lane zone above is a bounded current-content assumption, not a new zone registry. Replace it with the registry lookup as additional zones stream. Settle due events through the existing bounded settlement loop before capture; if still unsettled, capture refuses without touching the slot.

### StartLifeAndSaveToSlot: save a fresh world, never the previous life

Do **not** simply call the new started-life SaveToSlot from the existing method: at that point LaneLife still contains the previous life and the pawn may be at the title hold. Replace that method with this shape, retaining its whole-draft-on-failure behavior:

```cpp
auto Started = Creator; // preserves original draft and allocation history on any failure
if (!RecordResult(Started.StartLife())) return false;
const FString Path = GetSaveSlotPath(SlotIndex);
auto* Life = UPWLaneLifeSubsystem::Get(this);
if (Path.IsEmpty() || Life == nullptr)
    return RecordFileError(FText::FromString(TEXT("The world or save slot is unavailable.")));
PerfectWorld::WorldSaveData Fresh;
const auto Begun = PerfectWorld::BeginWorldFromCreation(Started.CaptureSave(), 0x50575F4C414E45ull, Fresh);
if (!Begun.Succeeded()) return RecordFileError(FText::FromString(ToFString(Begun.Message)));
auto Prepared = MakeUnique<PerfectWorld::WorldSim>(Fresh.World);
if (!Prepared->Submit({PerfectWorld::CommandSource::System,
        PerfectWorld::AdvanceTime{PWLaneLife::CampaignStartMinuteOfDay - 360}}).Accepted)
    return RecordFileError(FText::FromString(TEXT("The new world's clock could not start.")));
// Fresh.Player is the core's origin-home zero-position marker; safe entry resolves it.
const auto Capture = PWLaneLifePersistence::CaptureWorldSave(Started.CaptureSave(), Prepared->State(), Fresh.Player);
if (!Capture.Succeeded()) return RecordFileError(FText::FromString(ToFString(Capture.Validation.Message)));
const auto Written = PWLaneLifePersistence::WriteWorldSlot(
    TCHAR_TO_UTF8(*FPaths::ConvertRelativePathToFull(Path)), Capture.Text);
if (!Written.Ok) return RecordFileError(FText::FromString(ToFString(Written.Message)));
Creator = MoveTemp(Started);
LoadedPlacement.reset();
Life->AdoptSavedWorld(MoveTemp(Prepared));
LastError = EPWCreationError::None;
LastErrorMessage = FText::GetEmpty();
return true;
```

This snippet belongs inside the existing method; add the persistence/LaneLife/Paths includes to Entry.cpp. The initial v8 file uses the same origin marker as `BeginWorldFromCreation`, and is safe to load even if the process stops before first entry. Subsequent world saves contain the actual pawn placement. Keep this marker restriction explicit: only a supported `zone.home.*` with all three coordinates zero may use origin entry. Prefer resolving a concrete safe origin placement *before* writing when integrating the shared entry-safety submission.

### HUD Load/Continue and world-entry changes

`ContinueLatest()` already delegates to `LoadSlot()`; keep that route. In `StartNewLife`, remove the entire `Life->ResetWorld()` block (new-life adoption above already installed the fresh sim). In `LoadSlot`, keep the error path and draft routing; call EnterWorld only after the validated load succeeds. Gate Load/Continue if either presenter is open until callback-free dismissal is implemented.

Replace the unconditional home placement in `EnterWorld` with:

```cpp
auto* Creator = GetCreator();
const auto* Saved = Creator != nullptr && Creator->GetLoadedPlacement()
    ? &*Creator->GetLoadedPlacement() : nullptr;
const bool bEnPlaced = PlayerOwner != nullptr && EnGameMode != nullptr &&
    (Saved != nullptr ? EnGameMode->PlacePlayerAtSavedPlacement(*PlayerOwner, *Saved)
                      : EnGameMode->PlacePlayerAtOpeningHome(*PlayerOwner));
if (!bEnPlaced)
    return LOCTEXT("ResumePlacementFailed", "There is no safe place to resume. Please try loading again.");
if (Creator != nullptr) Creator->ClearLoadedPlacement();
// Existing title dismissal, input release and start-only acknowledgement follow.
```

Add `PlacePlayerAtSavedPlacement(APlayerController&, const PerfectWorld::PlayerPlacement&)` to GameMode and include WorldSave.h. **It is a new integration method, not present in submission A.** Use the existing safe-entry work from review step 1, with the restore checklist above as its required contract. In this checkout `PlacePlayerAtOpeningHome` still teleports without a clearance test; reusing that unmodified is not safe. Its home-coordinate resolution can be extracted and shared for New Life, migrated saves and blocked-resume fallbacks; do not duplicate home coordinate math in persistence.

The saved-placement method's final commit after a safe standing transform is resolved must use:

```cpp
if (!Pawn->SetActorLocationAndRotation(SafeCentre, FRotator(0.0, Saved.YawCdeg / 100.0, 0.0),
        false, nullptr, ETeleportType::TeleportPhysics)) return false;
Pawn->GetCharacterMovement()->StopMovementImmediately();
Player.SetControlRotation(FRotator(Saved.PitchCdeg / 100.0, Saved.YawCdeg / 100.0, 0.0));
Pawn->SetCameraMode(Saved.Camera == PerfectWorld::CameraMode::ThirdPerson
    ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
return true;
```

Here `Pawn` is the standing `APerfectWorldPlayerCharacter*` and `SafeCentre` is the preflight result, not the unchecked saved position. Refuse entry while the *current* pawn is seated/transitional until a callback-free seat-reset seam exists, or add that seam to reset capsule, pose, eye height and movement without replaying seat interaction. Never allow the normal seating tick to overwrite the restored placement. Fallback preserves the saved camera and applies a safe facing; tell the player when an alternate position was used.

## Assumptions and limits

- No shared seams were edited. No commits, Config edits, Unreal builds or editor launches. Storage is implemented; in-game integration and packaged restart acceptance remain undone.
- Slot operations are serialized on the game thread. Concurrent writers and network filesystem semantics are outside the current save-store contract.
- Reused `SaveStore` calls `ofstream::flush()` and closes before read-back/rename. It does **not** call OS durability barriers such as `FlushFileBuffers` or sync directory metadata. The interruption tests cover application failure at protocol boundaries, not a physical power-cut guarantee against drive/cache loss. A stronger hard-power-loss guarantee requires a separate core save-store submission; do not claim one from these tests.
- Core authenticates the world hash, not creation/placement fields. Those fields receive structural/semantic validation; a valid-range edit to them is not cryptographically authenticated. V6/v7 and older hash versions intentionally remain unverified according to the core contract. No bespoke migration, hash or lane rules were added here.
- Initial New Life at 07:30 follows existing LaneLife behavior. Creator-only migration at 06:00 follows the authoritative core migration, without silently moving it to the lane's creation time.
- Generated `build/` outputs are local validation artifacts only. The failed initial configure was preserved under ignored `build/failed-vs-config`.

## Validation record (copy to STATUS during integration)

**Intent:** Add world-v8 capture, slot storage and validated reads without modifying shared integration seams.

**Files changed:** Added `Source/PerfectWorld/Private/PerfectWorldLaneLifePersistence.h`, `.cpp`, `PerfectWorldLaneLifePersistenceTests.cpp`, and this handoff. `Docs/STATUS.md` was left to the integrator under the explicit file-ownership restriction.

**Exact check:** after importing the environment printed by `cmd.exe /c '"D:\VS\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul && set'` into the child PowerShell process:

```powershell
& D:/Tools/CMake/bin/cmake.exe -S . -B build/p -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release
& D:/Tools/CMake/bin/cmake.exe --build build/p --config Release
& D:/Tools/CMake/bin/ctest.exe --test-dir build/p -C Release --output-on-failure -j 2
& cl.exe /nologo /std:c++20 /W4 /WX /EHsc /c /ISource/PerfectWorldCore/Public /ISource/PerfectWorld/Private Source/PerfectWorld/Private/PerfectWorldLaneLifePersistence.cpp /Fobuild/p/Persistence.obj
& cl.exe /nologo /std:c++20 /MD /EHsc /W4 /wd4100 /Ibuild/ps /ISource/PerfectWorldCore/Public /ISource/PerfectWorld/Private build/ps/main.cpp Source/PerfectWorld/Private/PerfectWorldLaneLifePersistence.cpp build/p/PerfectWorldCore.lib /Fobuild/ps/ /Febuild/ps/persistence.exe
& build/ps/persistence.exe
```

The local harness includes the actual persistence automation .cpp and links the real portable core. Only Unreal's test reporting and FString/Paths/Guid facilities are shimmed under ignored `build/ps/Misc`. It executes the five authored bodies, not replacement copies. This does not establish engine compilation or registration. The first harness link used the compiler's default static CRT and failed against the core's dynamic CRT; corrected `/MD` link passed. A due-event fixture initially used a reducer-invalid event ID and failed; corrected to a detached due-now clock fixture, then reran all five successfully.

**Result:** PASS — full baseline CMake build; CTest 164/164 in 63.33 seconds; standalone storage compile; five portable harness bodies (RoundTrip, InterruptedWrite, RefusesDamageWithoutMutation, BackupRecoveryAndRepeatedSave, LegacyCreatorAndWorldVersions); first-header and under-500-line audits. Logs: `build/p/build.log`, `build/p/ctest.log`, `build/ps/results.log`. All validation processes finished. Unreal build / actual `PerfectWorld.Persistence` automation / PIE / packaged restart: NOT RUN, as instructed.

**Remaining limitation:** Shared seams are proposed code only; safe placement and presenter/seat state handling still need integration. Stream flush provides no physical power-cut durability proof. No validation rung above standalone C++ has been claimed for these Unreal-module additions.

**Next smallest action:** Integrator compiles the three files, runs `Automation RunTests PerfectWorld.Persistence`, then wires the adoption and safe placement seams in a separate submission, followed by an application-restart walk.
