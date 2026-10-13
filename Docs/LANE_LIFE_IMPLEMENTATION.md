# Lane life plan — implementation sequence (gpt-6.1-sol, high effort, read-only, 2026-10-13)

Companion to `LANE_LIFE_PLAN.md` (gpt-6-astra leads design). Where they differ, the design plan wins on look, tone and staging; this file supplies files, APIs and bounded steps.

**Recommendation:** retain the walking player and completed scale corrections, add an original five-part geometric ornament to the front end, make the near buildings enterable in stages, and connect NPCs to the existing portable simulation. “Small AI” should mean deterministic schedules and behaviour, with authored conversation.

This is a read-only plan for `integrate/ui-phase1-lane-week`. No files were edited, and no builds, tests or gameplay sessions were run. A read-only GPT-6 Astra architecture review was completed and incorporated.

## A. Architecture

### Interpretation and current baseline

| Request | Recommended interpretation |
|---|---|
| More visual front end | Procedural Slate ornament supporting the existing typography, controls and Kolkata view. Use original geometry; a recognizable pentagram would still carry occult associations even when renamed. |
| Workable houses | Every near building eventually has a functional ground floor, entrance, occupants or business, and appropriate access rules. Begin with the player’s home, a care interior, and two shops. Distant skyline buildings remain presentation. |
| Talkable NPCs with small AI | Every instantiated named NPC has a schedule, work behaviour and an authored response. Busy or sleeping people can decline appropriately. Conversation need not imply unrestricted access to private rooms. |
| Walking player | Already implemented. Verify and integrate it; do not replace it again. |
| Scale audit | Preserve completed corrections. Address the remaining floor-reference, clearance, collision and reusable-asset inconsistencies listed in C. |

Important current facts:

- [PerfectWorldPlayerCharacter.h](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Public/PerfectWorldPlayerCharacter.h:28) defines radius **34 cm**, half-height **82.5 cm**, eye height **155 cm**, walking **140 cm/s** and running **400 cm/s**.
- Buildings remain solid colliding cuboids at [PerfectWorldLaneBuildings.cpp:177](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldLaneBuildings.cpp:177). Decorative doors and shutters do not cut openings.
- `IdentityRegistry`, `EventLog`, `Schedules`, `Relationships`, `Threads` and `TimeContract` exist. They are largely standalone.
- `WorldState` already contains `DayLoop`; the world save is **v7**, and `WorldHashVersion` is **4**. Several roadmap descriptions lag behind this.
- The Unreal creator subsystem still saves creation records, rather than the complete world.
- [PerfectWorldFrontEndHUD.cpp:192](C:/Users/Administrator/Documents/GitHub/Perfect-World/Source/PerfectWorld/Private/PerfectWorldFrontEndHUD.cpp:192) deliberately enters play after a failed initial save. Correct that before attaching a successful Start Life effect.

### Portable core ownership

Use existing modules for their existing responsibilities. Add content adapters and integration code around them.

The following notation names exact files:

- `Core/X` = `Source/PerfectWorldCore/Public/X.h` and `Private/X.cpp`.
- Private-only files are explicitly identified.
- Existing files are distinguished from proposed files.

| Proposed files and types | Responsibility | Existing dependencies |
|---|---|---|
| **`Core/Locations`**: `LocationCatalogue`, `LocationDef`, `ZoneDef`, `RouteDef`, `DoorDef`, `LocationState` | Stable locations, building identities, entrances, access policies, integer route points, interaction slots and door state. | `ContentRegistry`, `AssetRegistry`, `GameClock`, `WorldState` |
| **`Core/Interaction`**, **`Public/InteractionCommands.h`**: `Interact`, `EnterZone`, `SetDoorOpen`, `InteractionOffer` | Core eligibility and command validation. Commands reference IDs, verbs and revisions. | `Locations`, `TimeContract`, `Commands`, `DayLoop` |
| **`Private/WorldInteraction.cpp`** | Routes interaction and zone commands through `WorldSim`. | Existing reducer and the modules above |
| **`Core/CastCatalogue`**: `CastCatalogue`, `CastDef` | Typed person definitions: name/text keys, appearance, home, schedule, dialogue set and identity kit. **Does not replace `IdentityRegistry`.** | `ContentRegistry`, `IdentityRegistry`, `Appearance`, `Schedules` |
| **`Core/NpcBehaviour`**: `NpcIntent`, `NpcBehaviourState` | Pure schedule-to-intent selection: work, walk, idle, sleep; conversation temporarily supplies chat intent. Work variants include sweeping, serving and studying. | `Schedules`, `Locations`, `CastCatalogue`, `TimeContract` |
| **`Private/WorldPeople.cpp`** | Registration, schedule progress and social/thread integration. Reuses `RegisterAuthoredIdentity`, `AdvanceSchedulePerson`, `RelationshipBook` and `ThreadSet`. | Existing identity, event, schedule, relationship, thread and calendar modules |
| **`Core/WorldEventRouting`**: `WorldSystemContext`, ordered routing functions | One chronological dispatch path for time advances and command effects. Supplies validated, immutable catalogues to the reducer. | `WorldReducer`, `GameClock`, `DayLoop`, all participating systems |
| **`Core/Conversation`**, **`Public/ConversationCommands.h`**: `ConversationCatalogue`, `ConversationState`, `TalkOpen`, `TalkChoose`, `TalkClose` | Authored nodes/options, conditional selection, declared exchange costs, persistent choices and exactly-once effects. | `TimeContract`, `Locations`, `IdentityRegistry`, `EventLog`, `Relationships`, `Threads`, `NpcBehaviour` |
| **`Private/ConversationText.cpp`**, **`Private/WorldConversation.cpp`** | Conversation codec and reducer routing. | `Conversation`, existing JSON/save/reducer conventions |
| **`Private/WorldLifeSaveInternal.h`**, **`Private/WorldLifeSave.cpp`**, **`Private/WorldLifeHash.cpp`** | Helpers for the new world sections; reuse existing module codecs rather than duplicate their parsers. | Existing `WorldSave*`, `WorldHash`, individual section serializers |

New content-schema files:

- `Private/ContentLocationSchemas.cpp`
- `Private/ContentPeopleSchemas.cpp`
- `Private/ContentNpcScheduleSchemas.cpp`
- `Private/ContentDialogueSchemas.cpp`

Register them through existing `ContentSchemas.h/.cpp`. The existing `ContentScheduleSchemas.cpp` defines **school bell schedules**, not NPC routines.

New portable tests:

- `Tests/LocationsTests.cpp`
- `Tests/InteractionTests.cpp`
- `Tests/CastCatalogueTests.cpp`
- `Tests/NpcBehaviourTests.cpp`
- `Tests/WorldEventRoutingTests.cpp`
- `Tests/WorldPeopleTests.cpp`
- `Tests/ConversationTests.cpp`
- `Tests/ConversationPersistenceTests.cpp`

Extend existing clock, world-save, world-hash and reducer tests where appropriate.

### Unreal ownership

For this table, `Game/X` means `Source/PerfectWorld/Public/X.h` and `Private/X.cpp`. Slate files live under `Source/PerfectWorld/Private/UI`.

| Proposed files/classes | Responsibility and dependencies |
|---|---|
| **`Game/PerfectWorldContentSubsystem` — `UPWContentSubsystem`** | Reads/stages content buffers and invokes the existing portable content/asset validators. Owns immutable catalogues. Depends on Engine and `PerfectWorldCore`. |
| **`Game/PerfectWorldSimulationSubsystem` — `UPWSimSubsystem`** | Sole live `WorldSim` owner and mutation gateway; emits native notifications, feeds elapsed time to the portable time controller, and handles partial advances. |
| **`Game/PerfectWorldSaveSubsystem` — `UPWSaveSubsystem`** | World-slot inspection, transactional start, save/load and placement capture. Uses existing `SaveStore` and `WorldSave`; the creator subsystem remains the draft façade. |
| **`Game/PerfectWorldWorldGameMode` — `APWWorldGameMode`** | Builds the playable layout and restores/spawns at the origin or saved location. Uses the existing walking character/controller and content/simulation subsystems. |
| **`Game/PerfectWorldBlockRuntimeBuilder` — `UPWBlockRuntimeBuilder`** | Builds registered cuboid definitions with engine-cube instances. Reads dimensions, collision and anchors from asset data. |
| **`Game/PerfectWorldAssetResolverSubsystem` — `UPWAssetResolverSubsystem`** | Resolves stable IDs to block definitions; provides the error placeholder. No imported art is required. |
| **`Game/PerfectWorldAssetActor` — `APWAssetActor`** | Registered prop/fixture presenter carrying stable asset and slot IDs. |
| **`Private/PerfectWorldLaneInteriors.h/.cpp` — `PWLane::BuildLaneInterior`** | Constructs wall shells, ceilings, floors and furnished rooms through the block builder. |
| **`Game/PerfectWorldDoorActor` — `APWDoorActor`** | Hinge, leaf, interaction bounds and open/closed collision. Reads core door state; animation completion has no gameplay authority. |
| **`Game/PerfectWorldInteractionComponent` — `UPWInteractionComponent`** | Camera acquisition, occlusion checks, target cycling and command submission. |
| **`UI/SPWInteractionPrompt.h/.cpp` — `SPWInteractionPrompt`** | Verb, target, availability reason and input glyph. Uses existing `PWStyle`/`PWWidgets`. |
| **`Game/PerfectWorldBlockCharacterComponent` — `UPWBlockCharacterComponent`** | Shared player/NPC cuboid display, appearance, procedural poses and face channels. Uses the frozen humanoid presentation contract. |
| **`Game/PerfectWorldNpcCharacter` — `APWNpcCharacter`** | Capsule and visible character; carries `PersonId`, never authoritative relationship or schedule data. |
| **`Game/PerfectWorldNpcPresenter` — `UPWNpcPresenter`** | Actor activation and reconciliation from core views. Keeps actors absent for off-screen people. |
| **`Game/PerfectWorldNpcMovementComponent` — `UPWNpcMovementComponent`** | Follows the authored route, performs local avoidance and presents locomotion. Depends on Engine character movement, not behaviour-tree assets. |
| **`Game/PerfectWorldConversationPresenter` — `UPWConversationPresenter`** | Opens/closes the view, manages input/framing and submits dialogue commands. |
| **`UI/SPWConversationPanel.h/.cpp` — `SPWConversationPanel`** | Speaker, authored lines, choices, disclosed cost and leave action. Reads a core-produced view. |
| **`Game/PerfectWorldPresentationSettingsSubsystem` — `UPWPresentationSettingsSubsystem`** | Persisted reduced-motion and presentation preferences; broadcasts changes to existing consumers. |
| **`UI/PWSigilPainter.h/.cpp` — `FPWSigilGeometry`, `FPWSigilPainter`** | Cached original geometric paths and drawing parameters. |
| **`UI/SPWAwakeningSigil.h/.cpp` — `SPWAwakeningSigil`** | Decorative Slate widget for title, stepper and successful life-start acknowledgement. |

Retain and extend:

- `PerfectWorldPlayerCharacter.*` and `PerfectWorldPlayerCharacterInput.cpp`
- `PerfectWorldPlayerController.*`
- `PerfectWorldFrontEndHUD.*`
- `PerfectWorldLane{Blocks,Buildings,Shops,Props,Barriers}.*`
- `SPWTitleScreen`, `SPWCreationFlow`, `SPWFrontEndRoot`
- `PWStyle` and `PWWidgets`

The chosen implementation needs the project’s existing `Engine`, `PerfectWorldCore`, `Slate`, `SlateCore`, `InputCore` and `EnhancedInput` dependencies. It does **not** initially require `AIModule` or `NavigationSystem`.

### Proposed content files

Initial fixed files:

| Files | Purpose |
|---|---|
| `Data/Content/location/nabapara_lane.json` | Near-building IDs, zones, doors, route graph, work anchors and access policies |
| `Data/Layouts/compact_family_flat.json` | First family-home interior |
| `Data/Layouts/childrens_home_dormitory.json` | Care opening with equivalent presentation attention |
| `Data/Layouts/lane_shop.json` | Reusable enterable shop |
| `Data/Layouts/lane_residence.json` | Reusable near-house ground floor |
| `Data/Presentation/skeleton_humanoid_v1.json` | Frozen skeleton/reference-pose contract |
| `Data/Presentation/appearance_mapping_v1.json` | Existing appearance choices to cuboids |
| `Data/Presentation/character_pose_humanoid_v1.json` | Procedural work, idle, walk and conversation poses |
| `Data/Presentation/lane_identity_kits.json` | Distinguishable silhouettes, clothing signatures and habits |

For each person below, create:

- `Data/Content/person/<PersonId>.json`
- `Data/Content/npc_schedule/<PersonId>.json`
- `Data/Content/dialogue/<PersonId>.json`
- `Data/Text/en/dialogue/<PersonId>.json`

Suggested twelve-person roster:

`npc_guardian_primary`, `npc_ms_basu`, `npc_mira_sen`, `npc_ishan_sheikh`, `npc_tara_das`, `npc_farida_qadri`, `npc_stationer`, `npc_driver`, `npc_neighbour_a`, `npc_neighbour_b`, `npc_gate_guard`, `npc_lane_caretaker`.

The last six are proposed fictional roles requiring authored identities. The driver can initially belong to the existing taxi; an auto-rickshaw is unnecessary for this step.

### Determinism and time integration

The current reducer needs strengthening before social systems are connected:

1. **Process due events at their actual instant.** `WorldReducer.cpp:123` currently advances an event at `Now` to at least `Now + 1`.
2. **Synchronize `DayLoop` at each resolution boundary.** Currently it is synchronized after the complete advance at `WorldReducer.cpp:157`.
3. Merge NPC and thread boundaries globally by time, documented system phase and stable ID. Advancing every NPC completely and then recording their events person-by-person would produce nonchronological history.
4. Keep **command sequence** separate from **event-log sequence**. `DomainEvent.Seq` identifies the command; relationship memories reference the dense `EventLogEntry.Seq`.
5. Preserve unprocessed events when a cap is reached. Complete continuation to a quiescent boundary before exposing saving or another player command.
6. Advance threads to an event instant using pre-effect facts, append the event, apply its relationship effect, then reevaluate same-instant triggers. Test this ordering explicitly.

Schedules remain declarative:

- Reuse `ResolveSchedule`, `NextScheduleBoundary` and `AdvanceSchedulePerson`.
- Validate full-week coverage, travel coverage and unsqueezed ordinary routines.
- Do not accept the missing-leg **60-minute fallback** in authored lane content.
- Do not persist `routine:<index>` as a permanent content identity; use authored action IDs.
- Work progress is resolved from schedule boundaries, never from actor arrival, animation notifies or visibility.

**Physical frame replay is not promised.** Core outcomes are deterministic for the same content and commands. Unreal positions and avoidance are transient presentation. Do not hash Chaos results or award progress when an actor reaches a waypoint.

A paid dialogue must explicitly advance logical exchange progress. `TimeContractResolve.cpp:151` completes `Finish` without assigning the declaration’s cost; charging simply by calling `Finish` would currently charge zero. Reading time must never determine the cost.

### Save-format impact

The current strict reader rejects unknown fields. New persistent state therefore requires explicit versions.

| Checkpoint | Proposed format change |
|---|---|
| Walking, corrected visuals, sigils, settings | No world-save bump. Settings live independently in user preferences. |
| Doors and time modes | **World v8**: location/door state and portable time-mode state |
| People integration | **World v9**: identities, event history, schedule overrides/progress, relationships and threads |
| Conversation | **World v10**: session/node/revision, accepted choices, effects/charges already applied, permanent choice facts |

Reuse existing versioned text envelopes, as v7 already does for `DayLoop`. Bump the world hash whenever its persistent sections change.

Migration requirements:

- Retain support for creation v1–v5 and world v6/v7 onward.
- Initialize old doors from authored defaults.
- Initialize new NPC progress at the loaded clock; do not award invented retrospective work.
- Preserve existing IDs.
- Validate cross-section references, timestamps, memories, thread participants and conversation nodes.
- Record content revision compatibility; changed dialogue nodes or reordered routines require aliases/migration or an explicit incompatibility result.
- Treat old-hash verification honestly; current v6 loading already reports when its hash cannot be recomputed.

Persist important dialogue choices separately from the relationship memory list: that list is bounded and can evict memories.

## B. UE 5.8 implementation choices

### Character and input

Keep the existing native `ACharacter` setup:

- Capsule radius **34**, half-height **82.5**, body height **165**.
- Camera **72.5 cm above capsule centre**, giving **155 cm above the floor**.
- Capsule attachment, controller look, no animated-head attachment.
- Third-person boom **300 cm**, adjustable **120–600**, probe **12 cm**.
- Walking **140 cm/s**, running **400 cm/s**, step height **45 cm**.
- No Blueprint dependency.

Add interaction actions to the existing runtime-created Enhanced Input context: **E/gamepad X** to interact and a separate target-cycle action. Keep V/gamepad Y for camera switching. Remove or suppress movement/interaction contexts while a modal conversation owns input; preserve release consumption and focus restoration.

Enhanced Input supports C++ bindings and runtime context addition/removal. Keep actions and mappings referenced by `UPROPERTY`, and retain the existing logged legacy fallback. [Epic’s Enhanced Input documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine).

### Collision

Use **visual ISMs plus independent simple collision boxes** for new playable interiors.

- Architectural visual instances: no collision.
- Walls, slabs and ceilings: consolidated `UBoxComponent` shapes from layout data.
- Furniture: a few contract-defined boxes per object.
- Doors: separate movable leaf collision.
- Decoration: no collision.
- Real surfaces block Pawn, Visibility and Camera as appropriate.
- Existing world-boundary barriers remain Pawn-only and are excluded from ordinary prop surface tests.

Do not make every trim, shelf item or character cube a physics body. ISMs share component-level collision settings; reduced draw calls do not eliminate collision cost. [Epic’s ISM documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine).

Avoid runtime mesh merging/collision cooking for this lane. Consolidated boxes are simpler to inspect and preserve across an eventual art swap.

### Navigation: choose authored waypoint steering

Use a validated lane graph initially:

- Stable route IDs, integer-centimetre waypoints, entrances and work anchors.
- Fixed tie-breaking for equal routes.
- Routes specify sufficient width, headroom and travel duration.
- NPC locomotion follows schedule segment timing; local sweeps and separation avoid visible collisions.
- NPCs yield rather than physically push the player.
- Off-screen people have no locomotion actors.

At the existing clock rate, an NPC moving at **140 cm/s real time** traverses **140 cm per game minute**. Travel durations must account for this; an 88 m route takes roughly 63 real seconds/game minutes.

Initially, schedule routes must remain traversable independently of player-operated private doors. Where a door controls a route, its closure and alternative route must be a **core decision**. Never let collision silently change attendance or work outcomes.

A runtime NavMesh would require `Dynamic` generation for geometry created during play; `Dynamic Modifiers Only` cannot create missing walkable surfaces. The current lane also explicitly disables navigation relevance at `PerfectWorldLaneBlocks.cpp:171`. Keep NavMesh for later, less constrained layouts. [Epic’s runtime-navigation explanation](https://dev.epicgames.com/documentation/unreal-engine/overview-of-how-to-modify-the-navigation-mesh-in-unreal-engine?lang=en-US).

### NPC and rendering budgets

Start with **four named NPCs**, then reach **twelve**, aligning with M3-12.

Proposed local budget:

| Item | Initial limit |
|---|---|
| Instantiated named NPCs | 12 |
| Moving NPCs at full update | 8 |
| Locomotion | Engine frame update for active movers |
| Steering/avoidance decisions | 10 Hz |
| Cuboid pose updates | 20 Hz nearby; reduced rate beyond 15 m |
| Face/gaze updates | 10 Hz; no continuous off-screen work |
| NPC presentation game-thread cost | ≤1 ms p95, measured |
| New interior lights | Usually 1–2 unshadowed lights per room |
| Sigil Slate CPU cost | ≤0.25 ms p95, measured |

These are proposed subsystem allocations, not measured results. Use the stricter overlapping project budgets until reconciled: block game thread **6 ms**, GPU **7 ms**, draw calls **1,250**, visible triangles **0.5 M**.

Build approximately 35 cuboids per character through shared instance/material batches and the frozen pose contract. Do not duplicate the current six-block player prototype into twelve independent final NPC implementations.

### Slate sigils

Use an original **five-lobed weave**: five equal nodes, interrupted rings, radial connectors and invented marks. Avoid copied runes, sacred diagrams and a conventional pentagram silhouette.

- `MakeLines`: polygonal rings, connectors and ticks.
- `MakeSpline`: optional curved links.
- `MakeBox`: small motes using a solid colour brush.
- Local normalized geometry, cached until size/style changes.
- Rotation only on decorative geometry around a centred render-transform pivot.
- Respect clipping, widget tint and returned paint layer.
- Decorations neither take focus nor intercept input.

UE 5.8 exposes these drawing primitives; its `MakeLines` overloads use `TArray<FVector2f>`. [Epic’s drawing API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/FSlateDrawElement), [MakeLines](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/SlateCore/FSlateDrawElement/MakeLines).

Suggested normal-motion values:

- Title: two rings, periods **60/90 seconds**, up to **12** faint motes.
- Reveal: **600 ms**, without delaying menu availability.
- Stepper: small stationary five-part variants; progress treatment remains separate.
- Successful start: **300 ms** acknowledgement, without a white flash or input lock.

**All five Tattvas receive equal area, weight and timing.** Do not associate Household with Prithvi, Science with Agni, or a trait allocation with an affinity.

Reduced motion must stop rings, motes, reveals, existing page translation/crossfades and the HUD’s title-camera drift. Render a stable complete composition and update states instantly. Start Life is an acknowledgement of a saved commitment, not the player’s January awakening.

## C. Scale findings and exact change locations

Source filenames below are under `Source/PerfectWorld/Private` unless otherwise specified. All source dimensions are centimetres; asset JSON uses millimetres.

### Already corrected — retain and verify

| Item | Current location | Current value |
|---|---|---|
| Player capsule/eyes | `Public/PerfectWorldPlayerCharacter.h:28`; `.cpp:53`, `:78` | Radius 34, half-height 82.5, eye 155 |
| Spawn | `PerfectWorldBlockTestGameMode.cpp:56` | Z **−115.5** initially; road is −200 |
| Road/footpath | `PerfectWorldLaneBlocks.h:18–22` | Road 500 wide, pavement 150 wide before setback, kerb 15 |
| Balcony door | `PerfectWorldLaneBuildings.cpp:41–44` | Leaf 90×210 |
| Rooftop door | `PerfectWorldLaneBuildings.cpp:110` | 90×210 |
| Residence door | `PerfectWorldLaneShops.cpp:144–147` | Leaf 90×210 |
| Near upper windows | `PerfectWorldLaneBuildings.cpp:22–33` | 120×135, sill 75, chhajja 45 |
| Second-row windows | `PerfectWorldLaneBuildings.cpp:210` | 120×135, sill 75 |
| Residence window | `PerfectWorldLaneShops.cpp:162–172` | 120×135, sill 75 above assumed floor |
| Rail/parapet | `PerfectWorldLaneBuildings.cpp:48`, `:80–84` | 100 high |
| Shutter/counter | `PerfectWorldLaneShops.cpp:10–11` | Clear shutter 240; counter top 90 above pavement |
| Poles/wires | `PerfectWorldLaneProps.cpp:8`, `:60`, `:74`, `:87–88` | Pole 900 from road; utility wires 2–2.5 thick |

The settled first-person eye is approximately **world Z −45**, not Z 0. Verify the movement component’s floor-contact offset in play.

### Remaining contract and geometry issues

| Finding and exact location | Required change |
|---|---|
| **Solid building volume**, `PerfectWorldLaneBuildings.cpp:177`; distant equivalents `:199`, `:227` | Replace the near ground-floor volume with shell walls **25 cm** thick, real openings, floor and ceiling. Keep distant massing solid. |
| **Door/window “frames” are filled decorative rectangles**, Buildings `:24`, `:41`; Shops `:144`, `:164` | New shells must use separate jamb/head/sill pieces. Disabling door collision alone cannot create passage through the underlying wall. |
| **Residence step overlaps the doorway baseline**, Shops `:145`, `:148` | Leaf starts at pavement +15, but step top is +28. A solid step would leave **197 cm** headroom. Initial solution: entrance floor stays at +15; change step range to **0→15**. Alternatively raise the entire doorway to +28, consistently. |
| **Ground-floor datum differs from upper floors**, Buildings `:134`, `:170`, `:183`; Shops `:9` | Ground floor is described at road +15, while first upper floor is +300: **285 cm** floor-to-floor. For playable flats use `FloorZ(n)=15+n×300`; building roof datum `15+Storeys×300`. |
| **Older-house height cannot be represented**, LaneBlocks.h `:22`; LaneBuildings.h `:23`; Buildings `:9` | Add per-building `StoreyHeightCm`: **300** for flats, **360** for authored older houses/school. Generic current flats do not need blanket enlargement. |
| **Awning clearance**, Shops `:116`, `:129` | Valance begins at road +206: **191 above pavement**, overlapping a 195 cm character. Set minimum clear underside **210 above pavement**: valance bottom **225**, top **249**, slope edge **247**, retaining wall attachment 285. Check rotated slab underside too. |
| **Collision tied to colour**, LaneBlocks.cpp `:28–35`, `:49`, `:173–191` | Structural concrete floors/ceilings would currently be noncolliding, while any wall-coloured furnishing could collide. New builders must use explicit collision roles/data. |
| **Camera/trace gaps on solid props**, Props `:40`, `:116`, `:143`, `:156`, `:173`; Shops `:59`, `:102` | Pawn-only barriers are inadequate for ordinary solid props under the presentation contract. Add surface collision for Camera/Visibility within **5 cm** of visible geometry; retain Pawn simplification separately. |
| **No usable stairs, corridors or interior ceilings** | Missing features rather than wrong dimensions. New layouts require corridors **≥120**, stairs **17.5 rise/25 tread**, ceilings, fan blades **≥240** above floor. |

The **75 cm plinth** at Buildings `:178` is a decorative band ending 75 above the road; it is not evidence of a raised interior floor. Keep that distinction explicit. Do not add 75 cm entrance rises indiscriminately.

### Approximate props and audit-description discrepancies

These are not all hard contract violations.

| Item/location | Actual result | Recommendation |
|---|---|---|
| Tank, Buildings `:94–97` | Rotated 130 cm square reaches **183.85 cm** across. Body height is **110**, and body plus lid is **120 above its base**—not the audit’s “130 across, 150 tall.” | Correct the audit description. If adopting its approximate 110×120 target, use a **110×110** block body, **110 high**, plus **10 cm** lid; remove the oversized rotated duplicate. |
| Taxi, Props `:127–140` | **436×165** footprint; body roof **146**, taxi marker raises total to **160** | The audit’s 435×165×155 reference is approximate. For an exact target: bumper extrema **±217.5**, glazing top **147**, roof **147→155**; marker becomes a separately documented accessory above 155. |
| Scooter, Props `:149–156` | Visual **190×68×112**; barrier **194×72×115** | For the audit’s 183×69×113 target, use overall constants **183/69/113**; retain 50 cm tyres with axle centres **±66.5**. Recompute collision from final bounds. |
| Scooter wheel comment, Props `:153` | “12 inch” is a rim class; 50 cm is tyre outer diameter | Clarify the comment; the tyre diameter itself is reasonable. |
| Handcart, Props `:162–173` | Deck **180×90**; including handles/wheels, approximately **240×104×92** | Keep. Correct the audit to distinguish deck and overall dimensions. |
| Pot, Shops `:55–59` | Highest foliage is **99 above pavement**; barrier reaches **90** | Audit’s “plant 78 tall” omits the final block. Leaves can remain noncolliding; document trunk/pot collision accurately. |
| Pole, Props `:28` | 900 above road, approximately **885 exposed above pavement** | Document the reference datum. Only enlarge if “900” is explicitly required above pavement. |
| Auto-rickshaw/bicycle | Absent | No existing mismatch. When added, use the task’s proposed auto **265×140×170** and cycle-wheel **70** as authored references. |

### Reusable asset data needs correction before interior use

The lane’s corrected dimensions differ from some existing registered blocks.

| File:line | Finding | Proposed fix |
|---|---|---|
| `Data/Assets/arch/door/wooden_panel.json:15–18`, `:24` | Actual leaf/passage **80×200**, despite nominal 210 cm asset height | Make the door asset a **90×210 leaf**; move jamb/lintel construction into the host shell. Leaf bounds Y **−450→450 mm**, Z **0→2100**; collision centre Z **1050**, half-extents Y **450**, Z **1050**. Update dimensions/envelopes/display name together. |
| `Data/Assets/arch/door/main_grill.json:15–18`, `:24` | Same **80×200** clear-passage problem | Same separation and clear dimensions. Keep grill thickness separate from frame thickness. |
| `Data/Assets/arch/window/louvred_shutter.json:15–19` | Outer **120×135**, inner aperture **100×115** | For the new shell’s 120×135 opening, make the shutter/window insert fill that opening; construct frame and sill separately. Place opening bottom **750 mm above the room floor**. |
| `Data/Assets/prop/street/tea_stall_counter.json:15–17`, `:23`, `:27`, `:30` | Worktop **95 cm**, main collision top **100**, cup socket **95**, versus lane counter **90** | If standardized at 90: body **850 mm**, top **850→900**, stove **900→1050**; counter collision top **900**, separate stove collision; tabletop anchors/socket **900**. Update envelopes/nominal height. |

`bathroom_plank.json:15` already supplies a **90×210** leaf.

Separating frame and leaf avoids weakening `arch.door@1` merely to accommodate an outer frame. Add semantic clearance checks so nominal asset height cannot conceal a shorter passage.

## D. Bounded execution sequence

**Ownership:** routine C++/tests/data work → **GPT-5.6 Sol**, or Claude implementation in a Claude session. Narrative drafts → **Claude Sonnet**, followed by owner editing. Architecture/save review → **GPT-6 Astra**. One integrator owns shared reducer, schema registration, hash/save wiring and HUD integration.

Every implementation step records its actual result in `Docs/STATUS.md`. Build registration and that status entry are the roadmap’s standard extra files. All other rows below own **1–5 files**; repeated batches remain separate steps.

Rungs: **C** = CTest; **U** = Unreal build/automation; **P** = scripted PIE; **K** = launched packaged build and captures.

### Stage 1 — scale and walking player

| Step | Owned files | Completion criterion | Owner / rung |
|---|---|---|---|
| 1.1 | `Docs/SCALE_AUDIT.md` | Distinguishes completed fixes, remaining failures and approximate prop measurements. | Integrator; read-only measurements |
| 1.2 | PlayerCharacter `.h/.cpp`, BlockTestGameMode `.cpp` | Both cameras walk/collide; eye height checked; 20 toggles preserve position/facing. Change code only for reproduced failures. | Sol; U→P |
| 1.3 | LaneShops `.cpp`, LaneBuildings `.h/.cpp`, LaneBlocks `.h` | Door baseline, awning clearance and per-building storey datum corrected. | Sol; U→P |
| 1.4 | LaneProps `.cpp`, LaneBuildings `.cpp` | Chosen prop targets match measured bounds; audit discrepancies resolved. | Sol; U→P |
| 1.5 | Wooden/grill door JSON, window JSON, tea-counter JSON | Registered assets match the chosen clear openings/worktop; asset validator passes. | Sol; C |
| 1.6 | `Tests/AssetRegistryTests.cpp`, asset semantic validator implementation | A deliberately short framed passage is rejected even when nominal height is correct. | Sol; C |
| 1.7 | `Docs/SCALE_AUDIT.md`, `Docs/STATUS.md` | Launched walking package captures road, kerb, door, awning and both cameras. | Integrator + owner; K |

### Stage 2 — front-end sigils

| Step | Owned files | Completion criterion | Owner / rung |
|---|---|---|---|
| 2.1 | PresentationSettingsSubsystem pair, SettingsPanel pair | Reduced motion persists and broadcasts immediately. | Sol; U→P |
| 2.2 | `PWWidgets.h/.cpp`, `PWStyle.h/.cpp` | Shared motion policy/style tokens available; focus stays immediate. | Sol; U |
| 2.3 | SigilPainter pair, AwakeningSigil pair | Static/animated five-part geometry renders without textures, focus or hit-test interference. | Sol; U→P |
| 2.4 | TitleScreen pair, FrontEndHUD `.cpp` | Title ornament integrated; existing title-camera drift freezes under reduced motion. | Sol; U→P |
| 2.5 | CreationFlow pair, FrontEndRoot pair | Step ornaments preserve labels/navigation; transitions become instantaneous under reduced motion. | Sol; U→P |
| 2.6 | HouseholdPage `.cpp`, StreamPage `.cpp`, TraitsPage `.cpp`, RankSelector `.cpp` | Existing page/preview motion also obeys the same preference. | Sol; U→P |
| 2.7 | Relevant existing UI files, maximum five | Owner accepts equal Tattva treatment and composition at 720p/1080p, 150% text, keyboard/controller. | Integrator + owner; K |
| 2.8 | FrontEndHUD `.cpp`, FrontEndRoot pair | Add the successful-start acknowledgement **after stage 3’s transaction is working**; failed save shows no success effect. | Sol; U→P→K |

### Stage 3 — enterable houses and authoritative interactions

| Step | Owned files | Completion criterion | Owner / rung |
|---|---|---|---|
| 3.1 | Locations pair, `ContentLocationSchemas.cpp`, `Tests/LocationsTests.cpp` | Location/door/route definitions validate; IDs and access policies are stable. | Sol; C |
| 3.2 | `ContentSchemas.h/.cpp`, `location/nabapara_lane.json` | New content loads through the existing registry; dangling zones/doors fail. | Integrator; C |
| 3.3 | Interaction pair, InteractionCommands header, `Tests/InteractionTests.cpp` | Valid door command changes state; stale/invalid commands leave it unchanged. | Sol; C |
| 3.4 | `WorldState.h`, `Commands.h`, `WorldReducer.cpp`, `WorldInteraction.cpp`, reducer tests | One reducer-owned door works through the command path. | Integrator; C |
| 3.5 | `WorldHash.h/.cpp`, `WorldLifeHash.cpp`, hash tests | Door/time mutations affect hashes; identical command replays match. | Integrator; C |
| 3.6 | `WorldSave.h`, `WorldLifeSaveInternal.h`, `WorldLifeSave.cpp`, writer, reader | Implement strict v8 serialization/migration. | Integrator; C |
| 3.7 | World-save tests and v1–v7 fixture batches, ≤5 files each | Save/load/continue, truncation and historical migration pass. | Sol; C |
| 3.8 | ContentSubsystem pair, SimulationSubsystem pair | Validated content and one `WorldSim` accessible to presenters. | Sol; U |
| 3.9 | SaveSubsystem pair, creator subsystem pair, save-bridge automation | Existing draft slots remain readable; world slots route through `WorldSaveValidator`. | Sol; C→U |
| 3.10 | FrontEndHUD pair, SaveSubsystem `.cpp`, transactional-start automation | Clone/validate draft, write/read back destination, then publish live world. Failure preserves draft/current slot. | Integrator; U |
| 3.11 | BlockRuntimeBuilder pair, AssetResolverSubsystem pair | Selected existing asset IDs render through the runtime block route. | Sol; U |
| 3.12 | AssetActor pair, LaneBlocks pair, layout automation | Visual instances and contract collision are independent; missing ID is visible/logged. | Sol; U |
| 3.13 | LaneInteriors pair, family layout, care layout | Furnished shells have ceilings, openings and usable clearances. | Sol; U→P |
| 3.14 | LaneBuildings `.h/.cpp`, LaneShops `.cpp`, LaneBarriers `.cpp`, KolkataLane `.cpp` | Selected ground floors replace solid volumes; no old barrier seals an entrance. | Sol; U→P |
| 3.15 | DoorActor pair, InteractionComponent pair | E/gamepad interaction changes core state and door collision in both cameras. | Sol; U→P |
| 3.16 | InteractionPrompt pair, PlayerCharacter `.h`, player-input `.cpp`, HUD `.cpp` | Forgiving selection, fixed priorities/cycling and prompt availability work. | Sol; U→P |
| 3.17 | WorldGameMode pair, `MAP_Nabapara.umap`, HUD `.cpp`, integration automation | Per-map world mode; start at origin home; load restores placement/camera within 1 cm. | Integrator; U→P |
| 3.18 | SimulationSubsystem pair, HUD `.cpp`, time-bridge automation | 60 real seconds add 60 game minutes ±1; title/menu/accessibility pauses add zero. | Sol; C→U→P |
| 3.19 | Shop/residence layouts, lane location data | Two shops and all near ground floors gain reusable interiors, in separate ≤5-file batches. | Sol; C→P |
| 3.20 | Existing layout/verification records, ≤5 files | Packaged home→street→shop walk, open/close/reload, furnished-room camera clearance and collision captures pass. | Integrator + owner; K |

Until the resolver/baker and relevant asset audits pass, describe this as a **runtime-block slice**, not completion of the entire M1 asset-pipeline gate.

### Stage 4 — schedules, behaviour and twelve NPCs

| Step | Owned files | Completion criterion | Owner / rung |
|---|---|---|---|
| 4.1 | `GameClock.h/.cpp`, clock tests | Due-at-now events can be drained without advancing a minute; unprocessed events remain pending. | Sol; C |
| 4.2 | WorldEventRouting pair, reducer `.h/.cpp`, routing tests | Chronological phase order, intermediate day synchronization and capped continuation pass. | Integrator; C |
| 4.3 | CastCatalogue pair, ContentPeopleSchemas, ContentNpcScheduleSchemas, cast tests | Typed people/routines load; full-week/travel and identity-kit checks pass. | Sol; C |
| 4.4 | ContentSchemas pair, first two people/routine files per batch | Register schema, then author **four NPCs** with explicit homes/work anchors. Use separate ≤5-file steps. | Integrator; then Sol; C |
| 4.5 | NpcBehaviour pair, behaviour tests | Work/walk/idle/sleep intent derives identically from the same schedule instant. | Sol; C |
| 4.6 | WorldState header, Commands header, WorldPeople `.cpp`, reducer `.cpp`, people tests | One identity and schedule advances live, off-screen and through saves equivalently. | Integrator; C |
| 4.7 | World-hash files/tests, ≤5 | Add people/social sections and repin hashes. | Integrator; C |
| 4.8 | World-save files, ≤5; persistence tests/fixtures in following batches | v9 preserves identities, schedule cursors/tallies, history, relationships and threads. | Integrator; C |
| 4.9 | WorldPeople `.cpp`, WorldEventRouting `.cpp`, people tests | Event-log/relationship/thread ordering passes simultaneous-event and split-advance tests. | Integrator; C |
| 4.10 | Skeleton, appearance mapping, pose data, identity-kit data | Frozen procedural character contract and first four distinct kits validate. | Sol; C |
| 4.11 | BlockCharacterComponent pair, PlayerCharacter `.h/.cpp`, character automation | Shared block display preserves player scale and builds supported appearances. | Sol; U→P |
| 4.12 | NpcCharacter pair, NpcMovementComponent pair | One NPC walks a validated route and presents a work pose without changing core truth. | Sol; U→P |
| 4.13 | NpcPresenter pair, WorldGameMode `.cpp`, NPC automation | Four actors appear where schedules specify; off-screen progress survives unload/reload. | Sol; U→P |
| 4.14 | Two person files + two schedule files per step | Expand four→twelve; every routine covers the week and has valid routes. | Sol; C |
| 4.15 | Identity-kit data, NPC presentation files, ≤5 | Twelve distinct silhouettes/work habits; owner edits identities and schedules. | Sol + owner; P |
| 4.16 | Performance/capture records, ≤5 | Packaged twelve-NPC route stays within measured budgets; M3-12 recognition playtest recorded separately. | Integrator + owner; K |

Schedule work and social progression must be proven in CTest before an attractive actor performance counts as functional AI.

### Stage 5 — conversation

| Step | Owned files | Completion criterion | Owner / rung |
|---|---|---|---|
| 5.1 | Conversation header/source, commands header, ContentDialogueSchemas, conversation tests | Nodes/options validate; selection uses time/state; stale/duplicate choices reject. | Sol; C |
| 5.2 | ContentSchemas pair, guardian dialogue/text, dialogue fixture | One zero-cost and one declared-cost exchange resolve correctly. | Sol + Sonnet; C |
| 5.3 | WorldConversation `.cpp`, Commands header, reducer `.cpp`, WorldState header, conversation tests | `talk.open/choose/close` run through the reducer; read time is free; exchange progress charges once. | Integrator; C |
| 5.4 | ConversationText `.cpp`, WorldLifeSave `.cpp`, world writer/reader/header | v10 persists session, choices, applied effects and charged progress. | Integrator; C |
| 5.5 | Hash files, ConversationPersistenceTests, ≤5 | Mid-conversation reload equals uninterrupted play; important choices remain after memory eviction. | Sol; C |
| 5.6 | ConversationPresenter pair, ConversationPanel pair | Speaker/choices/cost/leave view works with keyboard/controller and enlarged text. | Sol; U→P |
| 5.7 | HUD pair, PlayerCharacter `.cpp`, NPC presenter `.cpp`, conversation automation | Correct pause/input/focus/framing; leaving restores play; both cameras work. | Integrator; U→P |
| 5.8 | Two dialogue files + two English text files per step | All twelve have morning/work/evening/busy responses and relevant remembered-choice variants. | Sonnet drafts; owner edits; C |
| 5.9 | Existing thread/dialogue data and tests, ≤5 | One classmate recalls a meaningful choice later and after reload; related thread evolves off-screen. | Sol + owner; C→P |
| 5.10 | Verification/capture records, ≤5 | Launched package: talk to every cast member, leave at any line, save/reload, use both cameras and reduced motion. | Integrator + owner; K |

### Safe parallel work

After interfaces are fixed, these can run concurrently with distinct ownership:

- Sigil painter/widget implementation and portable location/interaction work.
- Interior-layout authoring and NPC/dialogue data drafting.
- Block-character component and portable behaviour tests.
- Conversation Slate panel and portable conversation rules.
- Distinct person/dialogue batches.

Sequence all edits to `WorldState`, `Commands`, `WorldReducer`, content registration, world hash/save files and the HUD. No two agents edit the same file, and **only one Unreal build runs at a time**, with the editor closed.

## E. Risks and owner decisions

| Issue | Recommended working assumption / decision needed |
|---|---|
| Ornament identity | Use original interrupted five-part geometry. Owner reviews the visual before broader integration; religious imagery needs the documented cultural review. |
| Extent of workable houses | Every **near ground floor** eventually functional; upper floors and skyline remain later work. Confirm whether upper-floor access is expected in this delivery. |
| Private access | Public shops follow hours; homes support knock/invitation rather than unrestricted entry. |
| Starting origins | Build family and care openings with equal attention. Remaining household-shell mapping still needs the roadmap’s R3 decision. |
| NPC physics versus schedule truth | Route validation and reconciliation must expose visible lag/blockage. A physics obstacle must never silently change work progress. |
| Dialogue coverage | All twelve receive a small, authored response set; deeper arcs follow. “Talkable” does not imply unlimited generated conversation. |
| Content drift | Freeze schedule/dialogue revisions or provide migrations; stable person IDs alone do not preserve changed content semantics. |
| Performance | The reference hardware and conflicting budget tables still need confirmation. Measure the chosen slice before increasing NPC count. |
| Verification | Existing walking code has recorded build/package evidence, but the supplied status says it was not launched. Screenshots alone cannot verify collision, determinism or saving. |

The first reviewable delivery should be **a verified walking player, stationary/animated front-end sigils, one save-persistent door and one genuinely enterable furnished home**. That establishes the foundation for scheduled, talkable neighbours without replacing the core systems already built.