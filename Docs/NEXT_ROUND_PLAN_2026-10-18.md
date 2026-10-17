# Next-round plan, 2026-10-18 (gpt-6-astra)

gpt-6-astra's plan after the route fix, LL-03 (2), persistence (A) and Mira merged (integrate at bf50f20), recorded unedited. The owner asked for astra to plan the next step.

**The most valuable next step is to accept a persistent, walkable lane slice:** enter the three new homes through working doors, meet Mira doing her packing shift, save, restart, and resume safely. This connects the owner’s requests to observable behaviour. Another frontage batch would multiply unverified geometry; prepare its furniture now, but gate expansion on LL-05.

The checkpoint history supports the reported automation progress, not packaged acceptance. The inventory and scale audit contain stale status text. I read the requested documents and relevant source; I changed nothing and ran no tests.

For the work orders below, paths expand as follows:

- `G/` = `Source/PerfectWorld/Private/`
- `C/` = `Source/PerfectWorldCore/Private/`
- `CP/` = `Source/PerfectWorldCore/Public/`

**Next round: four work orders**

**NL-01 — Authoritative frontage doors. Owner: Claude deep-work.**

Owns: `CP/Doors.h`, `C/Doors.cpp`, `C/WorldSave.cpp`, `Tests/DoorsTests.cpp`, `Tests/WorldSaveLaneTests.cpp`; new fixtures `Tests/Fixtures/world_v8_doors_10.json` and `Tests/Fixtures/world_v8_doors_11.json`.

Task: migrate the eleven-door roster to **sixteen**: three frontage doors plus the two dwelling doors inside `tall_flats`. Preserve existing IDs/order, open states and swing sides; append explicit defaults. Retain supported ten-door and older standalone migrations. Specify legitimate visitor access for private dwellings and unrestricted exit; a Knock message alone is insufficient.

Acceptance: frozen historical v8 fixtures authenticate against their **stored** door text before defaults alter the world. Tampered fixtures fail; unknown, duplicate and arbitrary partial rosters fail. New saves round-trip. Run focused door/save tests and full CTest.

In-game proof, delivered through NL-04: enter each home by its permitted access route, operate every new leaf, close it behind you, and leave; restart preserves settled states.

Dependency: portable work is independent of persistence B; restart acceptance requires B.

**NL-02 — Mira’s physical working day. Owner: gpt-6.1-sol, after finishing persistence B.**

Owns: `G/PerfectWorldLaneStationeryShop.h`, `G/PerfectWorldLaneStationeryShop.cpp`, `G/PerfectWorldLaneNpcBindings.cpp`, `G/PerfectWorldNpcPresenceSubsystem.cpp`, `G/PerfectWorldNpcPresenceInternal.h`; new `G/PerfectWorldMiraPresenceTests.cpp`.

Task: validate and complete Mira’s bindings. The provider **already exposes the packing-desk anchor**; reuse it. Bind school, personal-time locations and their off-lane routes. Arrival must begin at the shop threshold and follow the interior route; departure must reach that threshold before removal. Cover direct time jumps and loading during work, not just normal Travel transitions. Do not instantiate her at an unseen desk and call that an arrival.

Acceptance: automated Tuesday/Thursday arrival, work and departure; off-day absence; delayed travel across 16:30; conversation pause/resume; no duplicate actor or missing binding. Retain route/presence/yield regressions.

In-game proof: watch her arrive, pack separately from Anindita, answer Talk, resume work and depart without popping or crossing the partition. Restart retains dialogue memory without charging twice.

Dependency: registration belongs to NL-04; restart tests require B.

**NL-03 — Shop furniture readiness. Owner: Claude Sonnet.**

Owns: `CP/LaneInteriorLayout.h`, `C/LaneInteriorLayout.cpp`, `C/LaneInteriorTwoRoom.cpp`, `C/LaneInteriorDetail.h`, `Tests/LaneInteriorLayoutTests.cpp`; new `C/LaneInteriorShopFurniture.cpp` and `G/PerfectWorldShopFurnitureTests.cpp`.

Task: add general-goods and workshop furniture sets using existing block primitives, stable asset IDs, work anchors and reserved circulation. Do not activate another frontage batch.

First deliver a small interface commit containing the furniture enum additions and optional shared-entry unit-door IDs needed by NL-04. Freeze that interface before parallel implementation continues; NL-04 never edits this header.

Acceptance: deterministic, mirrored and narrow-footprint tests; useful work/standing anchors; no furniture in circulation, door sweeps or approach zones. Existing stationery output and routes remain unchanged.

In-game proof: integrator walks an isolated development fixture containing both sets, reaches the work anchors and verifies both cameras. Record these as furniture readiness, not newly completed lane shops.

Dependency: independent of B.

**NL-04 — Door integration and LL-05 evidence. Owner: integrator.**

Owns: `CP/LaneFrontageCatalogue.h`, `C/LaneFrontageCatalogue.cpp`, `C/LaneInteriorShared.cpp`, `Tests/LaneFrontageCatalogueTests.cpp`; `G/PerfectWorldLaneInteriorKit.cpp`, `G/PerfectWorldLaneKitFrontagesTests.cpp`, `G/PerfectWorldKolkataLane.cpp`, `G/PerfectWorldLaneRoutes.h`, `G/PerfectWorldLaneRoutes.cpp`; new `G/PerfectWorldLaneScaleTests.cpp`; `CMakeLists.txt`, `Docs/SCALE_AUDIT.md`, `Docs/LANE_BUILDING_INVENTORY.md`, `Docs/STATUS.md`.

After B relinquishes ownership, also owns `G/PerfectWorldLaneLifeSubsystem.cpp` for the access bridge.

Task: connect NL-01’s IDs, unit leaves and inside signs; register Mira once after provider installation; wire legitimate visiting permission rather than the current hard-coded `IsInvited=false`.

Choose **LL-05** over a separate talkability expansion: faulty clearances would propagate into every remaining house.

Acceptance: measure 165 cm capsule, 155 cm eyes, 34 cm radius, 500 cm road, 15 cm kerb, 300 cm storeys, clear 90×210 doorways and 120 cm circulation. Test connected approaches, waiting spaces, seats, supported height extremes and camera clearance. Record rendering/light costs. Run engine automation and package; retain separate constructed/furnished/access/occupied/walked statuses.

**Merge and package order**

Merge B first, then NL-03’s interface commit, NL-01, NL-02, remaining NL-03, and NL-04. Re-run the merged CTest suite, including Mira, and engine automation; baseline counts are not the expected new totals.

Once the owner’s window is free, walk in short sessions:

1. Both-origin New Life and Continue, blocked-resume fallback, ordinary seating/standing.
2. Anindita’s partition route, yielding, return and after-hours exit.
3. All three homes and five new doors; Mira’s shift; save/restart.
4. Scale routes and furniture fixtures.
5. Dawn title and acknowledgement at 720p/1080p, 150% UI scale, controller, reduced motion and failed start.

Only the integrator builds Unreal. Keep detached steps within ten minutes and bind logs/packages to commit and artifact identity.

**Risk:** VM power loss exceeds the save store’s proven guarantee: flush/rename tests do not prove physical durability. Preserve known-good slots and never report an interrupted run as passed.

## Dispatch (integrator)

- **NL-01 (doors):** Claude deep-work.
- **NL-03 (shop furniture):** Claude Sonnet. It delivers the interface commit first.
- **NL-02 (Mira in Unreal):** gpt-6.1-sol, after persistence (B).
- **NL-04 (door integration and LL-05):** integrator.
