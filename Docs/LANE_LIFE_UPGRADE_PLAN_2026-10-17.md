# Lane life upgrade plan, 2026-10-17 (gpt-6-astra)

Written by gpt-6-astra (Codex, high reasoning, read-only review of the repository at `ae98006`), after today's packaged walk: the clock, Farida's tea and the care worker were verified. The owner asked that astra plan this upgrade round first. It is recorded here unedited; the dispatch notes at the end are the integrator's.

I recommend a round centred on **safe circulation, shared NPC presence, reusable interiors, and a visibly richer front end**.

Two source findings change the plan:

- The Fivefold Aperture, creation seals, reduced-motion setting, and morning lighting already exist. Improve their composition rather than commission them again.
- Portable world-save v8 exists, but Unreal still saves through the creator subsystem; `EnterWorld()` calls `PlacePlayerAtOpeningHome()`. The reported Continue obstruction therefore needs reproduction against the exact packaged revision before attributing it to restored coordinates.

This was a read-only review; I made no edits or ran builds/tests. Your latest report takes precedence over STATUSâ€™s older statement that the stationery shop was underway.

I would rank the next steps as follows.

1. **Safe entry and everyday movement.** Resolve door-swing spawn safety, doorway occupation, and ordinary-walk sitting first because these directly obstruct the life already playable.
2. **Generic schedule-driven NPC presence.** Replace Farida-specific lifecycle handling and static scheduled-person spawns with one presenter, so each additional person requires content and spatial bindings.
3. **Parametric interiors and systematic frontage rollout.** A shared shell/furniture kit turns â€œall houses enterableâ€ into a finite inventory rather than repeated bespoke construction.
4. **Front-end composition and entry polish.** A deliberate dawn view, stronger title composition, and restrained successful-start effect will make the existing visual language noticeably richer.
5. **Measured height, clearance, and camera verification.** Verify the existing scale corrections against actual collision and furnished routes before multiplying interiors.
6. **Connect Unreal slots to portable world saves.** Time, doors, conversation memory, and resume placement must survive an application restart before the lane can claim persistent daily life.
7. **Finish the stationery-shop loop.** Merge Aninditaâ€™s content, bind her schedule to a usable counter and stockroom, and demonstrate opening, work, break, and closing.
8. **Add Mira as the first schoolâ€“homeâ€“work example.** Her varied week will exercise the shared system more usefully than adding several stationary conversation targets.

The first five priorities should use the following work orders. Each order is a small series of submissions, **normally two or three implementation files per submission**, rather than one large sol request.

For compact, exact paths below:

- `G/` = `Source/PerfectWorld/Private/`
- `GP/` = `Source/PerfectWorld/Public/`
- `C/` = `Source/PerfectWorldCore/Private/`
- `CP/` = `Source/PerfectWorldCore/Public/`
- `UI/` = `Source/PerfectWorld/Private/UI/`

Files marked **new** do not presently exist.

**LL-01 â€” Safe entry and usable seats**  
**Owner: integrator. Start immediately.**

Owns:

- `G/PerfectWorldBlockTestGameMode.cpp`
- `G/PerfectWorldPlayerCharacterSeat.cpp`
- **New:** `G/PerfectWorldEntrySafety.h`, `.cpp`
- **New:** `G/PerfectWorldEntrySafetyTests.cpp`

Must do:

- Reproduce Continue and both origin starts in the current package; record its revision. The current family-start coordinates already claim clearance from the swing, so test the geometry rather than merely moving the point again.
- Validate placement using the full capsule, floor support, furniture, and door-sweep exclusion volumes; try authored alternatives in a stable order. Never resolve an invalid placement by crossing a wall or changing residency.
- Walk normally to the family chair/divan and care bed/study bench; fix sitting only where that walk reproduces a failure.
- Leave NPC doorway yielding to LL-02, using the same clearance contract.

Acceptance:

- New entry-safety automation covers occupied primary anchors, blocked alternatives, and both origins.
- Existing `PerfectWorld.Doors`, `PerfectWorld.Home`, `PerfectWorld.CareHome`, and `PerfectWorld.Furniture` automation passes.
- In the package, New Life and Continue allow immediate movement and door operation in both cameras.
- From a normal walking start, sit, turn, switch camera, and stand at each listed seat; a blocked stand-up destination produces a safe alternative or clear refusal. **No `BugItGo` for this proof.**

**LL-02 â€” One schedule presenter, including real doorway yielding**  
**Owner: sol. Content-loader changes wait for ANâ€™s merge.**

Owns, in separate bounded submissions:

- `CP/NpcScheduleContent.h`, `C/NpcScheduleContent.cpp`, `Tests/NpcScheduleContentTests.cpp`
- **New:** `GP/PerfectWorldNpcPresenceSubsystem.h`, `G/PerfectWorldNpcPresenceSubsystem.cpp`
- **New:** `G/PerfectWorldLaneNpcBindings.h`, `.cpp`
- `GP/PerfectWorldNpcMovementComponent.h`, `G/PerfectWorldNpcMovementPresent.cpp`
- **New:** `G/PerfectWorldNpcYield.cpp`, `G/PerfectWorldNpcPresenceTests.cpp`, `G/PerfectWorldNpcYieldTests.cpp`

Must do:

- Maintain one presentation record per stable person ID: validated schedule, resolved intent, local/off-lane binding, and optional actor.
- Use `ResolveNpcIntentAt` and the existing movement component. Reconcile on clock changes, reset/load, and conversation completionâ€”including paid choices crossing boundaries.
- Support arrivals and departures after initial world construction. Farida currently has schedule resolution but only a construction-time spawn path.
- Generalise loading without weakening weekly coverage, travel-budget, or authored-person validation. Avoid adding a new presenter branch for every person.
- Bind care desk/common-room routes and off-lane exits; bind Anindita after the shop exists. Missing required bindings produce a specific diagnostic, never a default-position spawn.
- Give doorways exclusion zones and nearby waiting points. An idle worker must also step aside when obstructing passage; todayâ€™s â€œyieldâ€ can simply wait in place.
- Keep avoidance transient. Physical arrival, visibility, and gestures must not award work or alter schedule outcomes.

Acceptance:

- Run `PerfectWorldNpcScheduleContentTests`, `PerfectWorldFaridaScheduleTests`, `PerfectWorldNpcBehaviourTests`, and `PerfectWorldLaneTransactionsTests`.
- New presence automation covers duplicate prevention, weekday/weekend boundaries, initially absent people returning, missing bindings, and paid-conversation jumps.
- Existing `PerfectWorld.NpcMovement` plus new yielding tests pass.
- Packaged observation: Farida returns for afternoon service; care staff change work anchors and depart after duty; Anindita follows her merged schedule once bound.
- Walking toward the care doorway causes a clear sidestep/backtrack into a safe waiting point, without pushing, oscillation, or visible teleportation; conversation remains reachable.

**LL-03 â€” Parametric interior kit and frontage inventory**  
**Owner: Sonnet. Can begin alongside LL-01/02.**

Owns:

- **New:** `CP/LaneInteriorLayout.h`, `C/LaneInteriorLayout.cpp`, `Tests/LaneInteriorLayoutTests.cpp`
- **New:** `C/ContentLaneBuildingSchemas.cpp`
- **New:** `Data/Content/lane_building/lane_building.nabapara.json`
- **New:** `G/PerfectWorldLaneInteriorKit.h`, `.cpp`
- **New:** `G/PerfectWorldLaneInteriorKitTests.cpp`
- **New:** `G/PerfectWorldLaneStationeryShop.h`, `.cpp`

Must do:

- Inventory every near frontage, including the end-closing masses; assign explicit building IDs independent of generated row indices.
- Define compact residence, shop-with-stockroom, and shared-entry variants. Inputs include footprint, orientation, finished-floor datum, storey height, entrance, access policy, furniture set, and reserved stairs.
- Generate wall segments around openings, floors, ceilings, circulation, door sweeps, usable furniture anchors, and room-light specifications.
- Keep layout/clearance decisions portable; Unreal emits block visuals and collision from that result. Furnishings reference stable asset IDs, with explicit physical contracts.
- Use the stationery frontage at approximately **X âˆ’100â€¦600, +Y** as the public variant. Its wrapper should supply shop-specific anchors and dressing, not duplicate shell construction.
- First prove three different footprints/orientations; then complete remaining near ground floors in batches of at most three frontages.

Acceptance:

- Portable tests cover narrow/wide footprints, mirrored orientation, deterministic output, invalid layouts, stable IDs, and disconnected rooms.
- Require **90 Ã— 210 cm clear doorways**, **120 cm circulation**, correct floor-relative placement, and unoccupied swing/seat/standing spaces.
- New Unreal kit automation verifies emitted collision and anchors; existing home/care/tea-shop suites remain green.
- Walk every delivered frontage in both cameras, use an interaction, operate doors, and leave.
- Track **constructed, furnished, access-integrated, occupied, and walked** separately. Every near ground floor must reach the agreed gate before that rollout closes; upper dwellings and roofs remain explicitly unfinished.

**LL-04 â€” Dawn composition and successful entry**  
**Owner: Sonnet. Two sequential submissions; can run alongside world work.**

Owns:

- `UI/SPWTitleScreen.h`, `.cpp`
- **New:** `G/PerfectWorldTitleView.h`, `.cpp`
- `G/PerfectWorldFrontEndHUD.cpp`
- `UI/SPWAwakeningSigil.h`, `.cpp`
- `UI/SPWFrontEndRoot.h`, `.cpp`

Must do:

- First submission: introduce a dedicated title viewpoint over the existing morning lane, independent of the gameplay pawn; recompose the title/menu and existing aperture responsively.
- Second submission: add the short successful-start acknowledgement through the existing root/sigil widgets.
- Trigger it only after successful save **and** successful world placement. A failed start preserves the draft and shows its error.
- Returning to title must not reposition the player, change gameplay time, or overwrite resume state.

Acceptance:

- Packaged checks at 1280Ã—720 and 1920Ã—1080, including 150% text/UI scaling, keyboard, and controller.
- Continue, New Life, Load, Settings, and error states remain readable and operable immediately.
- Toggle reduced motion while animation is running; all decorative motion and camera drift stop immediately and remain stopped after restart.
- Start failure shows no success effect; successful family and care starts receive equivalent treatment.
- Compare captures against the current packageâ€”the delivery must show a composition improvement, not merely prove that a sigil exists.

**LL-05 â€” Scale and circulation evidence**  
**Owner: integrator. Baseline now; final pass waits for LL-01â€“04.**

Owns:

- `G/PerfectWorldLaneRoutes.cpp`, `.h`
- **New:** `G/PerfectWorldLaneScaleTests.cpp`
- `Docs/SCALE_AUDIT.md`
- **New:** `Docs/LANE_BUILDING_INVENTORY.md`

Must do:

- Replace stale â€œnot runâ€ statements with measured results and precise remaining failures.
- Extend route checks beyond sampled footpath width to doorway approaches, room connections, waiting spaces, and seated exits.
- Verify current player dimensions: **165 cm capsule/body, 155 cm eye height, 34 cm radius**; road width **500 cm**, footpath rise **15 cm**, flats **300 cm floor-to-floor**.
- Check actual floor-relative apertures, awnings, furniture, and both cameras. Test clearance with the supported height extremes without silently changing the current player.
- Route geometry corrections back to the owning order; avoid a broad rescale.

Acceptance:

- Existing route diagnostic reports continuous pedestrian routes; new automation measures both width and connectivity.
- Package captures show roadâ†’kerbâ†’thresholdâ†’room traversal, eye level, awning headroom, and seated clearance.
- No camera wall penetration along the tested paths.
- Record measured rendering/light costs after interior expansion.

**Integration ownership and sequencing**

The integrator alone owns these shared integration files:

- `G/PerfectWorldKolkataLane.cpp`
- `G/PerfectWorldLaneBuildings.h`, `.cpp`
- `GP/PerfectWorldLaneLifeSubsystem.h`
- `G/PerfectWorldLaneLifeSubsystem.cpp`, `PerfectWorldLaneLifeClock.cpp`, `PerfectWorldLaneLifeFarida.cpp`
- `CP/Doors.h`, `C/Doors.cpp`, `Tests/DoorsTests.cpp`, `Tests/WorldSaveLaneTests.cpp`
- `C/ContentSchemas.h`, `.cpp`
- `CMakeLists.txt`, `Docs/STATUS.md`

Use those seams to remove Farida-specific/static scheduled spawns, connect generic availability to conversations, register kit buildings, and register new doors/content. Workers submit interface requirements instead of editing these files.

The existing door codec requires the authored door set: **new doors require an explicit migration and old-save fixtures**, not just additional table rows. Also correct the bridgeâ€™s currently hard-coded `DoorDirection::Entry` before accepting private/business interiors; permitted egress must remain possible after closing time.

AN finishes first; LL-02 then consumes its merged interfaces. LL-03â€™s shop binding precedes Aninditaâ€™s packaged presence check. LL-04â€™s start effect follows LL-01. LL-05 closes the round. Only the integrator builds Unreal, with the editor closed and one build at a time. Keep implementation files below 500 lines by splitting responsibilities before additions exceed the limit.

For the front end, retain the earlier **Fivefold Aperture** direction, with these concrete refinements:

| Element | Buildable treatment |
|---|---|
| Backdrop | Existing voxel lane at dawn, framed to show depth, a recognisable shopfront, and warm windows; use its current morning lighting. |
| Main motif | Five equal interrupted sectors, open chevrons, and calibration ticks; keep the centre open. Avoid a literal pentagram or copied religious diagram. |
| Composition | Readable left menu on a dark surface; aperture in the right third, approximately 340â€“460 logical units where space permits; suppress ornament before compressing controls. |
| Colour | Existing blue-green `#101A1D` / `#18262A`, ivory `#F4EEDF`, amber `#E9B568`; reserve cyan for focus. |
| Motion | Retain one 90-second calibration-ring rotation and 800 ms reveal. Successful entry expands all sectors together by about 12% and fades over 450 ms, without delaying control. |
| Restraint | Small stationary creation seals; quiet pause screen; no affinity colours, flashing, magical transformation, or implication that creation is Awakening Day. |

Build this with the existing cached Slate line/box geometry and live voxel scene. Keep decoration hit-test invisible and outside accessibility reading order. No imported art or mesh-dependent gameplay is needed.

Reduced motion shows the complete stationary design immediately. Reading surfaces must preserve contrast over bright walls; target at least **4.5:1 for ordinary text**, with clearly distinguishable focus and error states. Validate enlarged text through reflow and scrolling, rather than shrinking it to fit.

The main risks and exclusions for this round are:

- **Persistence claims:** portable v8 tests do not prove packaged save/load. Priority 6 needs its own bounded bridge order and application-restart test.
- **â€œEvery houseâ€ scope:** completing ground floors advances the request; upper homes, stairs, balconies, and roofs remain part of its eventual completion.
- **NPC travel:** at the current clock rate, 140 cm/s means 140 cm per game minute. Validate authored travel durations and explicit off-lane transitions; do not conceal impossible routes with faster movement or disappearance.
- **Performance:** repeating shadowed room lights across the lane can become expensive. Measure visibility and lighting before finishing rollout.
- **Scope:** no twelve-person expansion, runtime LLM dialogue, new navigation framework, wholesale asset-pipeline rewrite, real-art import, or engine migration.
- **Verification:** preserve the reported 159/159 baseline, rerun relevant checks after changes, and record compilation, automation, packaging, and actual walking separately.

## Dispatch (integrator)

- **LL-01 (safe entry and seats):** integrator.
- **LL-02 (one schedule presenter, with doorway yielding):** gpt-6.1-sol, after Claude Sonnet's Anindita order (AN) merges, since both touch the schedule loader.
- **LL-03 (parametric interior kit):** Claude Sonnet. It replaces the earlier stationery-shop order (SS), and the shop becomes the kit's public variant.
- **LL-04 (dawn title view and start acknowledgement):** Claude Sonnet.
- **LL-05 (scale and circulation evidence):** integrator; the baseline now, the final pass last.
- The integrator alone edits the shared integration files astra lists, and alone runs Unreal builds.
