# LL-02 submission 1 handoff

2026-10-17, game/npc-presence. Implementation authored in new files only. No commit or Unreal build. Integration and Unreal runtime acceptance remain pending.

## Files created

Under `Source/PerfectWorld/Public/`:

- `PerfectWorldNpcPresenceSubsystem.h`

Under `Source/PerfectWorld/Private/`:

- `PerfectWorldNpcPresenceSubsystem.cpp`
- `PerfectWorldNpcPresenceInternal.h`
- `PerfectWorldLaneNpcBindings.h` and `.cpp`
- `PerfectWorldNpcYield.h` and `.cpp`
- `PerfectWorldNpcPresenceTests.h` and `.cpp`
- `PerfectWorldNpcYieldTests.h` and `.cpp`

This handoff is the twelfth new file. Integrator-owned files, existing movement files, content loaders, configuration, CMake and STATUS were not edited.

## Public API and contracts

`World.GetSubsystem<UPWNpcPresenceSubsystem>()` provides:

```cpp
bool RegisterScheduledPerson(FName PersonId, const FText& DisplayName, EPWNpcOutfit Outfit);
bool RegisterScheduledPersonFromContent(const PerfectWorld::ContentDb& Db, FName PersonId,
    const FText& DisplayName, EPWNpcOutfit Outfit);
void Reconcile(PerfectWorld::GameInstant GameInstantNow);
void ResetPresentation(PerfectWorld::GameInstant GameInstantNow);
void UnregisterPerson(FName PersonId);
APWNpcCharacter* FindPerson(FName PersonId) const;
const PerfectWorld::NpcIntent* FindIntent(FName PersonId) const;
int32 RegisteredPersonCount() const;
bool IsRegistered(FName PersonId) const;
FNpLaneNpcBindings& Bindings();
```

Registration discovers the schedule by its authored `person` field, then calls `LoadNpcSchedule` with that record ID. No person-ID-to-schedule-ID naming convention or presenter branch is required. Duplicate registrations retain the first record and actor. Unmanaged static duplicates are diagnosed and never duplicated or adopted.

Include the private `PerfectWorldLaneNpcBindings.h` in building modules. `RegisterProvider(ProviderId, FNpBindingProvider)` returns false on cross-provider ID conflicts, and replaces the same provider ID atomically. `RemoveProvider`, `FindAnchor`, `FindRoute`, `Doorways` and `Resolve` are available. A provider supplies local feet transforms, explicit off-lane locations with local exit anchors, authored route-segment polylines, explicit off-lane segments, and doorway exclusion boxes with waiting points. Local route points run From to To as declared in schedule content. Optional `bPreferSit` belongs to the anchor.

`Reconcile` resolves the core intent at the full instant, including paid skips and day changes. Ticks present the resolved result, retry arrivals deferred by visibility/occupation, recover late provider registration, retire unseen off-lane actors and update transient doorway detours. The presenter never submits a core command or awards work. FindIntent returns the underlying scheduled intent, even during a physical wait or local conversation.

Travel composes the schedule's directed journey from provider segments. Off-lane portions have no invented geometry; authored segment lengths allocate their share of the journey's progress. The existing movement component receives local progress. Unknown anchors/routes and disconnected local polylines produce specific diagnostics and no new spawn. Visible existing actors hold on a missing binding; unseen ones retire. Visible departures finish a retained local exit route, then remain at the exit until unseen. New arrivals wait until their resolved position is out of view and unoccupied.

## Exact integration edits: clock and conversations

In `PerfectWorldLaneLifeSubsystem.cpp` and `PerfectWorldLaneLifeClock.cpp`, include `PerfectWorldNpcPresenceSubsystem.h`.

In `UPWLaneLifeSubsystem::Initialize`, initialize the presenter dependency before `BuildWorld()`, and reconcile immediately after `BuildWorld()`:

```cpp
UPWNpcPresenceSubsystem* NpPresence = Collection.InitializeDependency<UPWNpcPresenceSubsystem>();
BuildWorld();
if (NpPresence != nullptr) { NpPresence->Reconcile(GetNow()); }
```

In `PerfectWorldLaneLifeClock.cpp`, change the accepted branch of `SubmitCommand` to:

```cpp
if (Result.Accepted)
{
    PresentEvents(Result.Events);
    if (auto* NpPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
    {
        NpPresence->Reconcile(GetNow());
    }
    HandleClockMoved();
}
```

This runs after every accepted command, including a paid ConversationChoose, ConversationLeave, `choice.leave` and an unchanged-time conversation completion. Do not put it behind `Now == LastBroadcastInstant`: leaving a conversation can change presentation without changing time. `LeaveConversation` already uses SubmitCommand and needs no second invocation. Movement's existing local Chat hold prevents movement during the panel; the next tick applies departure after that hold closes.

In `HandleClockMoved`, remove `RefreshFaridaForClock()` and its obsolete Farida-only comment. Retain the existing full-instant guard and OnClockChanged broadcast; SubmitCommand already reconciles first.

In `ResetWorld`, replace `RefreshFaridaForClock()` with:

```cpp
if (auto* NpPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
{
    NpPresence->ResetPresentation(GetNow());
}
```

After a future load assigns the restored WorldSim and applies door states, call the same ResetPresentation block with the restored GetNow. ResetPresentation discards actors and detours, retaining providers and validated schedules; use it under the reset/load transition, before returning world control. Remove `FaridaResolvedInstant = -1` from BuildWorld when retiring the old Farida state.

In `MakeConversationSession`, replace the Farida-specific availability branch with:

```cpp
PerfectWorld::PersonAvailability Availability = PerfectWorld::PersonAvailability::Awake;
if (auto* NpPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
{
    NpPresence->Reconcile(GetNow());
    if (NpPresence->IsRegistered(PersonId))
    {
        const auto* NpIntent = NpPresence->FindIntent(PersonId);
        if (NpIntent == nullptr) { return nullptr; }
        Availability = NpIntent->Availability;
    }
}
```

Keep the existing static household adult's Awake fallback; registered people with no valid intent refuse the session. The actor's movement already applies bTalkable for sleep. No presentation detour changes core availability or business permission.

## Exact integration edits: care home

In `PerfectWorldKolkataLane.cpp`, include `PerfectWorldNpcPresenceSubsystem.h` and `PerfectWorldLaneNpcBindings.h`.

In `SpawnCareHome`, retain CareFrame and the furniture spawn loop. Replace the static care-worker SpawnAt block, including its `At` variable, with:

```cpp
if (auto* NpPresence = World.GetSubsystem<UPWNpcPresenceSubsystem>())
{
    if (!NpPresence->Bindings().RegisterProvider(FName(TEXT("building.nabapara.care_home")), NpCareHomeBindings(CareFrame)))
    {
        UE_LOG(LogTemp, Error, TEXT("Care-home NPC provider conflicts with an existing binding."));
    }
    else
    {
        NpPresence->RegisterScheduledPerson(FName(TEXT("person.care_worker")),
            NSLOCTEXT("PerfectWorldLane", "CareWorker", "Care worker"), EPWNpcOutfit::HouseholdAdult);
        if (auto* NpLife = UPWLaneLifeSubsystem::Get(&World)) { NpPresence->Reconcile(NpLife->GetNow()); }
    }
}
```

Call only on the existing HasCareHome path, after collision is committed. The care adapter registers desk/common/front-door/footpath anchors, home off-lane, four schedule segments and front/common doorway exclusion zones. No LaneBuildings edit is required by this submission.

## Exact integration edits: Farida

Add `bool RegisterTeaShopPresence(const PWLane::FLaneBuilding& Building);` to the LaneLife subsystem's public people API. Replace the Farida-only implementation in `PerfectWorldLaneLifeFarida.cpp` with a provider-registration adapter. Keep the subsystem header first and include the presence subsystem, bindings, content subsystem, TeaShop, GameInstance and World headers. The method body is:

```cpp
auto* NpPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
const auto* NpInstance = GetWorld()->GetGameInstance();
const auto* NpContent = NpInstance != nullptr ? NpInstance->GetSubsystem<UPWContentSubsystem>() : nullptr;
if (NpPresence == nullptr || NpContent == nullptr || NpContent->GetDb() == nullptr) { return false; }
const auto NpLoaded = PerfectWorld::LoadNpcSchedule(*NpContent->GetDb(), PerfectWorld::FaridaScheduleId);
if (!NpLoaded.Ok()) { return false; }
FNpBindingProvider NpProvider;
TArray<PWLane::FTeaShopAnchor> NpAnchors;
PWLane::GetTeaShopAnchors(Building, NpAnchors);
for (const auto& NpAnchor : NpAnchors)
{
    FNpAnchorBinding NpBinding;
    NpBinding.Feet = FTransform(FRotator(0.0, NpAnchor.WorldYawDegrees, 0.0), NpAnchor.World);
    NpBinding.bPreferSit = NpAnchor.Id == FName(TEXT("anchor.shanti_chai.rest"));
    NpProvider.Anchors.Add(NpAnchor.Id, NpBinding);
}
TArray<PWLane::FTeaShopRoute> NpSegments;
PWLane::GetTeaShopRoutes(Building, NpSegments);
for (const auto& NpSegment : NpSegments)
{
    NpProvider.Routes.Add(NpSegment.Id, FNpRouteBinding{NpSegment.World, false});
}
const FName NpFootpath(TEXT("location.nabapara.shanti_chai_footpath"));
const auto* NpFootLine = NpProvider.Routes.Find(FName(TEXT("route.shanti_chai.door_to_footpath")));
if (NpFootLine == nullptr || NpFootLine->Points.IsEmpty()) { return false; }
FNpAnchorBinding NpFootAnchor;
NpFootAnchor.Feet = FTransform(NpFootLine->Points.Last());
NpProvider.Anchors.Add(NpFootpath, NpFootAnchor);
FNpAnchorBinding NpHome;
NpHome.Kind = ENpBindingKind::OffLane;
NpHome.ExitAnchor = NpFootpath;
NpProvider.Anchors.Add(FName(TEXT("location.nabapara.farida_home")), NpHome);
NpProvider.Routes.Add(FName(TEXT("route.farida_home.footpath_to_home")), FNpRouteBinding{{}, true});
// Normalize the existing helpers' door connectors once, preserving authored segment direction.
for (const auto& NpDef : NpLoaded.Value->Routes)
{
    auto* NpLine = NpProvider.Routes.Find(PWLaneLife::ToName(NpDef.Id));
    if (NpLine == nullptr) { return false; }
    if (NpLine->bOffLane) { continue; }
    const auto* NpStart = NpProvider.Anchors.Find(PWLaneLife::ToName(NpDef.From));
    const auto* NpEnd = NpProvider.Anchors.Find(PWLaneLife::ToName(NpDef.To));
    if (NpStart == nullptr || NpEnd == nullptr) { return false; }
    const int32 NpIndex = NpLine->Points.IndexOfByPredicate([NpStart](const FVector& NpPoint)
    {
        return NpPoint.Equals(NpStart->Feet.GetLocation(), 1.0);
    });
    if (NpIndex > 0) { NpLine->Points.RemoveAt(0, NpIndex); }
    else if (NpIndex == INDEX_NONE) { NpLine->Points.Insert(NpStart->Feet.GetLocation(), 0); }
    if (NpLine->Points.IsEmpty() || !NpLine->Points.Last().Equals(NpEnd->Feet.GetLocation(), 1.0))
    {
        NpLine->Points.Add(NpEnd->Feet.GetLocation());
    }
}
if (!NpPresence->Bindings().RegisterProvider(FName(TEXT("building.nabapara.shanti_chai")), MoveTemp(NpProvider))) { return false; }
const bool NpRegistered = NpPresence->RegisterScheduledPerson(FName(TEXT("person.farida_qadri")),
    NSLOCTEXT("PerfectWorldLane", "FaridaName", "Farida"), EPWNpcOutfit::ShopProprietor);
NpPresence->Reconcile(GetNow());
return NpRegistered;
```

In FaSpawnFarida, keep HasTeaShop, frontage recovery and IsTeaShopBuilding. Replace the block from `FTransform Feet` through direct SpawnAt/RegisterFarida with:

```cpp
if (Life == nullptr || !Life->RegisterTeaShopPresence(Building))
{
    UE_LOG(LogTemp, Error, TEXT("Shanti Chai NPC provider/schedule registration failed; no fallback spawn."));
}
return;
```

Remove old PrepareFarida/RegisterFarida/RefreshFaridaForClock/GetFaridaIntent declarations and definitions, ResolveFaridaIntent/HandleFaridaBlock, the seven Farida-only fields, and the Farida actor cleanup block in LaneLife Deinitialize. Migrate any remaining GetFaridaIntent callers to the generic FindIntent. The presenter owns actor cleanup. No duplicate OnClockChanged subscription is necessary.

The shop module can register its doorway zones in its own provider. Anindita's provider, registration and geometry remain with that module; no stationery anchors are guessed here.

## Assumptions and limitations

- AN retains the current NpcSchedule alias/data members, LoadNpcSchedule signature and `person` field. This submission consumes the existing loader without changing its frozen identity/weekly/travel validation.
- One local player; game-world subsystem only. No multiplayer visibility policy.
- Actor spawning is deferred while the target is conservatively in view or overlaps a Pawn-blocking capsule. No timeout overrides that policy. A watched arrival can therefore remain physically absent until the player looks away; its core schedule continues.
- Care desk is a standing point at U180/V314 beside the solid chair, not seated inside the chair at U114/V314. Common room uses CareStaffSpot/CareStaffSpotYaw. Front door is U235/V-45; footpath is U235/V-85, within the 150 cm raised pavement. Authoritative content IDs/times remain unchanged.
- Care route polylines fit the authored segment distance budgets. No navigation framework or autonomous door-opening was added: a closed/blocked door can delay physical presentation without changing outcomes.
- Doorway boxes include approaches and leaf sweep space. They should be compared with LL-01's final clearance geometry before package acceptance. Waiting points are filtered against all registered zones, a full capsule overlap/sweep, floor support and player separation. If none is safe the actor stays put; no pushing, fallback position or teleport.
- Yield starts inside 250 cm of the exclusion box, latches until the player stays beyond 350 cm for one second, then walks back to the latest schedule anchor. Detours are limited to 350 cm and use the existing mover with a temporary Idle target; the core intent remains intact. Dynamic blockers can still delay the walk through existing avoidance.
- Existing movement-component unseen catch-up remains enabled for ordinary schedule presentation. This submission adds no teleport operation. ResetPresentation destroys transient actors under the reset/load transition.
- Provider registration must precede normal registration/reconciliation where possible. Missing bindings recover on ticks, with a specific diagnostic. Provider conflicts return false for the caller to report.
- Complete route-conversation-save integration, Unreal compilation, automation and package walking remain the integrator's work. This submission does not claim that the scene has changed before the hooks above are applied.

## Validation and remaining integration

Ran from this worktree, with TEMP/TMP directed to `.np/tmp` on D:

```text
D:/Tools/CMake/bin/cmake.exe -S . -B .np -G "Visual Studio 18 2026" -A x64
D:/Tools/CMake/bin/cmake.exe --build .np --config Release --target PerfectWorldNpcScheduleContentTests PerfectWorldFaridaScheduleTests PerfectWorldNpcBehaviourTests PerfectWorldLaneTransactionsTests --parallel 4
D:/Tools/CMake/bin/ctest.exe --test-dir .np -C Release --output-on-failure -R "^PerfectWorld(NpcScheduleContent|FaridaSchedule|NpcBehaviour|LaneTransactions)Tests$"
```

PASS: 4/4 targeted portable executables, 8.98 seconds. This is not a full-suite run and does not compile the new Unreal sources. Generated build artifacts were retained under ignored `build/np-submission-1` inside this worktree after execution.

New automation is authored under `PerfectWorld.NpcPresence` (five tests) and `PerfectWorld.NpcYield` (two tests). It covers duplicate actors, weekday/weekend exact boundaries, absence/local arrival/departure/return/reset, missing binding and late provider recovery, a real paid tea command crossing a schedule boundary with Chat hold/no reconciliation teleport, nearest safe yield point/hysteresis, and walking input toward the waiting point. NOT RUN: these groups, existing PerfectWorld.NpcMovement, Unreal build, PIE and packaged observation.

CMake changes: none. All implementation/test additions are in the Unreal module; UBT discovers them. The portable files and four CMake test targets already exist. Build.cs changes: none (Core/CoreUObject/Engine/PerfectWorldCore already supply the dependencies).

Integrator: apply hooks; record the result in STATUS (not edited here); compile Unreal once; run PerfectWorld.NpcPresence, PerfectWorld.NpcYield and PerfectWorld.NpcMovement plus the lane suites; observe afternoon return, care duty anchor changes/departure, and doorway sidestep/return with conversation reachable in both cameras. Only after the shop provider exists, add Anindita's presence acceptance. Persistence bridge completion remains a separate order.
