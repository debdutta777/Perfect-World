# Lane life round review, 2026-10-18 (gpt-6-astra)

gpt-6-astra's read-only review after LL-02 (1, 2a, 2b), LL-03 (1) and LL-04 (1, 2) merged and the staffed lane was walked, recorded unedited. The owner asked for astra to plan the round.

The round has delivered a reusable foundation, but **none of LL-01–05 has complete acceptance evidence yet**. Stabilise and demonstrate the existing lane before multiplying it. This assessment uses the requested documents and source inspection; I ran no builds or tests.

| Order | Evidence genuinely met | Still open |
|---|---|---|
| **LL-01** | Packaged family entry permits movement; Furniture automation passes. | Both-origin New Life/Continue safety, blocked-placement alternatives, ordinary-walk sitting and safe standing in both cameras. |
| **LL-02** | Generic presentation, three scheduled people, bindings and Exit direction merged; related automation passes. | **Packaged failure:** Anindita cannot reach her counter. Returns, departures, yielding and after-hours egress remain unproved. |
| **LL-03** | Portable kit, layout tests and Unreal automation pass; stationery shop is built and enterable. | Inventory and three-frontage submission are underway. Every delivered frontage’s doors, interactions and two-camera walk remain acceptance gates. |
| **LL-04** | Title view and acknowledgement implemented; headless tests pass. | Visual improvement is claimed, not demonstrated: packaged captures, enlarged text, controller operation, contrast and reduced-motion restart checks remain. |
| **LL-05** | Dimension contracts and earlier geometry changes exist. | Measured circulation, height extremes, camera clearance and rendering costs remain unverified; `SCALE_AUDIT.md` still says “nothing compiled or run.” |

**Next five steps, in order**

Treat each step as separately checkpointed submissions: normally two or three implementation files, with execution windows under ten minutes. Validation can span windows; interrupted work is never recorded as passed. The integrator retains shared-file and Unreal-build ownership.

1. **Accept the route repair and close the immediate usability evidence. Owner: integrator.**

   Files: `PerfectWorldNpcMovementPresent.cpp` and movement tests delivered by the current repair agent; `PerfectWorldBlockTestGameMode.cpp`, `PerfectWorldPlayerCharacterSeat.cpp`, entry-safety helpers/tests; `Docs/STATUS.md`. Review title files only unless a defect reproduces.

   Acceptance, in separate short walks: Anindita traverses the partition opening in both directions, including when delayed across a schedule boundary; yielding and after-hours egress work. Both origins enter safely; ordinary walking reaches each LL-01 seat, with camera switching and safe stand-up. Capture LL-04 at both specified resolutions and 150% scaling, including reduced motion and failed-start behaviour. Do not reassign the repair already underway.

2. **Give the first three kit frontages authoritative doors. Owner: integrator; Claude Sonnet finishes the existing geometry submission.**

   Files: core `Doors.h`, `Doors.cpp`, `Tests/DoorsTests.cpp`, `Tests/WorldSaveLaneTests.cpp`; then `PerfectWorldLaneBuildings.*`, `PerfectWorldKolkataLane.cpp` and the delivered frontage inventory, in a separate integration submission.

   Acceptance: stable building/door IDs; explicit migration from historical **ten-door and eleven-door** states, preserving old values and adding defaults. Old world-v8 fixtures authenticate before migration; tampering remains rejected. Every emitted door registers its inside sign. Walk all three frontages, operate doors, use an interaction and leave after closing. Track constructed, furnished, access-integrated, occupied and walked separately. Private homes need a legitimate knock/invitation route, not permanent refusal.

3. **Connect Unreal slots to world v8. Owner: gpt-6.1-sol; integrator connects shared seams.**

   Files: new `PerfectWorldLaneLifePersistence.*` and persistence automation; separate changes to `PerfectWorldCharacterCreatorSubsystem.cpp`, `PerfectWorldCharacterCreatorSubsystemEntry.cpp`, `PerfectWorldLaneLifeSubsystem.*` and world-entry placement.

   Split storage/validation from restore/placement. Acceptance: a packaged application restart preserves clock, door state/swing, conversation memory and safe resume placement; legacy creator saves remain loadable. Failed or interrupted writes preserve the previous usable slot. Load validates before replacing live state, reconciles NPCs once, and does not reset the restored world. Specify mid-conversation restoration explicitly; paid choices must never charge twice.

4. **Make talkability a roster gate; add Mira next. Owner: Claude Sonnet; integrator owns registration seams.**

   Files: `Data/Content/npc_schedule/npc_schedule.mira_sen.json`, new core `ConversationMira.cpp` and focused tests; separate loader/catalogue registration and Mira binding submissions.

   First audit every currently spawned person: each has a reachable Talk interaction, authored response or explained unavailability, and Leave in both cameras. Then accept Mira’s seven-day schedule: school, Tuesday/Thursday 16:30–18:00 counter work, personal time and sleep; distinct work gestures; safe coexistence with Anindita; conversation pause/resume and remembered dialogue surviving restart.

   **Subrata Dey is next after Mira**, using shared-entry maintenance anchors. Add one person per submission cycle. “Small AI” remains shared schedules, work actions and avoidance; everyone talkable does not mean permanently available.

5. **Close LL-05 before repeating the frontage batch. Owner: integrator.**

   Files: `PerfectWorldLaneRoutes.*`, new `PerfectWorldLaneScaleTests.cpp`, `Docs/SCALE_AUDIT.md`, `Docs/LANE_BUILDING_INVENTORY.md`.

   Acceptance: measure 165 cm body/capsule, 155 cm eyes, 34 cm radius, 500 cm road, 15 cm kerb and 300 cm storeys; verify clear 90×210 doorways and 120 cm circulation. Test supported height extremes, seats, waiting spaces and both cameras. Record connected traversal and rendering/light costs. Repeat the gate for subsequent batches of at most three. Upper dwellings, stairs and roofs remain explicit work toward **ALL houses**.

**Risks**

- `PerfectWorldNpcMovementPresent.cpp` switches non-travel intent to direct anchor chasing; the repair must retain navigable progress across schedule changes.
- `PerfectWorldLaneLifeSubsystem.cpp` controls door operation, but the handoff explicitly excludes NPC door operation and open-door access enforcement.
- Changing `Doors.cpp` affects canonical save text and hashes; migrations need historical fixtures.
- Detached processes survive session disconnection, **not VM power-off**. Timestamp jumps, interrupted binaries and the D-clone index anomaly require revision-bound logs and artifact checks before acceptance.