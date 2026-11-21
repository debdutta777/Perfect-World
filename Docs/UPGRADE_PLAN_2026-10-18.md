# Upgrade plan, 2026-10-18 (gpt-6.1-sol)

gpt-6.1-sol's implementation plan for Claude Sonnet subagents, written at the owner's request ("use gpt 6.1 for upgrade plan then sonnet subagents"), recorded unedited. It builds on gpt-6-astra's NL plan (Docs/NEXT_ROUND_PLAN_2026-10-18.md).

The next round should deliver an accepted, persistent lane slice, then establish the next expansion. The reported baseline is **168/168 portable tests and 60/60 engine tests**; persistence B remains uncompiled. No new verification is claimed here.

**Priority across the next two rounds**

1. **Next round: prove access, life and scale together.** Finish B and NL-01/03/04; deliver NL-02 through UP-01/02 below. Astra already covers sixteen doors, legitimate visiting access, Mira’s working day, two furniture sets and LL-05. Close the outstanding packaged entry, seating, route, restart and title checks before activating more frontages.
2. **Next round: extend the owner’s requests visibly.** Add one scheduled resident, enforce talkability for every spawned person, correct person heights, improve creation/review/pause, and prove a stair prototype. Shared schedules, work gestures, avoidance and conversation interruption provide the requested “small AI.”
3. **Following round: expand after acceptance.** Open three frontages: `repair_workshop`, `hardware_store` and `old_house`, using the workshop, general-goods and residence sets. Deliver one connected upper dwelling in `tall_flats`; add a workshop worker/vendor and one talkable passer-by. Add restrained door, interaction and work audio with mute/volume controls and recorded provenance.

That leaves **nine sealed side frontages and two end masses**, plus remaining upper floors. Keep them explicitly queued in batches of at most three; end masses need authored hall/arcade destinations. “ALL houses enterable” includes upper dwellings, stairs and safe permitted roof destinations. Neither locked doors nor decorative entrances count as completion.

**Dispatch contract**

All orders below are for Claude Sonnet. Each checkpoints intent, interface, progress, remaining work and exact commands/results in `D:\PWWork\logs\<UP-ID>_notes.md`, beginning before edits. Use short resumable phases and one fresh Rung 1 directory per order:

`python Scripts/build_core.py --build-dir D:/pwUPxxR1`

Record the complete suite result, including content validation. Engine-only orders still run this regression check. Agents never build Unreal or commit.

The integrator retains `PerfectWorldKolkataLane.cpp`, `PerfectWorldLaneBuildings.*`, all LaneLife subsystem files, ContentSchemas, `Docs/STATUS.md` and shared registration/migration seams. Agents supply CMake append instructions in their notes; the integrator applies them. Parallel orders own disjoint files.

**UP-01 — Mira’s complete spatial bindings**

Own:

- `Source/PerfectWorld/Private/PerfectWorldLaneStationeryShop.cpp`
- `Source/PerfectWorld/Private/PerfectWorldLaneNpcBindings.cpp`
- `Source/PerfectWorld/Private/PerfectWorldMiraBindingsTests.cpp` **(new)**

Reuse the existing packing anchor. Bind every school/personal-time off-lane anchor and route; test connected, reversible threshold-to-desk paths through the partition opening.

Depends on NL-03’s frozen layout interface. Integrator registers Mira once, after provider installation.

Acceptance: existing Mira schedule/conversation/save portable tests; new `PerfectWorld.MiraBindings` automation plus `NpcRoute`. Observe Tuesday/Thursday arrival and packing separately from Anindita.

**UP-02 — Physical arrivals and departures across time jumps**

Own:

- `Source/PerfectWorld/Private/PerfectWorldNpcPresenceSubsystem.cpp`
- `Source/PerfectWorld/Private/PerfectWorldNpcPresenceInternal.h`
- `Source/PerfectWorld/Private/PerfectWorldMiraPresenceTests.cpp` **(new)**

Reconstruct authored routes when reconciliation skips Travel. Arrivals begin at the threshold; departures reach it before removal. Cover direct work-time loads, delayed travel, off-days, conversation release and duplicate prevention.

Depends on UP-01; restart acceptance requires B. Consume UP-04’s height interface after its declaration freezes.

Acceptance: portable NPC/schedule regressions; new `PerfectWorld.MiraPresence`, existing `NpcPresence`, `NpcRoute`, `NpcMovement`, `NpcYield` and `PersistenceFlow`. Observe arrival, interruption, resumed packing and departure without popping or wall crossing.

**UP-03 — Subrata’s first complete resident submission**

Own, all **new**:

- `Data/Content/npc_schedule/npc_schedule.subrata_dey.json`
- `Source/PerfectWorldCore/Private/ConversationSubrata.cpp`
- `Tests/SubrataLifeTests.cpp`
- `Source/PerfectWorld/Private/PerfectWorldSubrataBindings.cpp`

Author seven-day checks, maintenance, noticeboard work, meals, rest and sleep. Add greeting, busy/asleep responses, Leave and one remembered agreement; avoid claiming repairs occurred without recorded consequences. Supply bindings derived from built caretaker/shared-entry layouts.

Depends on NL-01 access and NL-03’s frozen interface. Integrator declares/routes `SubrataConversationCatalogue()`, adds required content references, installs the provider and registers him after construction.

Acceptance: new portable schedule, conversation and world-save tests; integrator adds `PerfectWorld.SubrataPresence`. Observe inspect/work/rest, Talk/Leave, resumed work and remembered agreement after restart.

**UP-04 — Talkability and person-height gate**

Own:

- `Source/PerfectWorld/Public/PerfectWorldNpcCharacter.h`
- `Source/PerfectWorld/Private/PerfectWorldNpcCharacter.cpp`
- `Source/PerfectWorld/Private/PerfectWorldNpcRosterTests.cpp` **(new)**

Expose one person-aware standing-height query used by spawning, clearance checks, body generation and pose calculation. Match authored heights, including Mira 160 cm, Anindita 154 cm and Subrata 181 cm.

Enumerate actual spawned people; require stable identity, reachable interaction, authored conversation or explained unavailability, and Leave. Exercise successful conversations and work resumption.

Acceptance: portable conversation regressions; new `PerfectWorld.NpcRoster`, plus `CareHome`, `TeaShop` and movement tests. Walk every present person in both cameras. Integrator applies any LaneLife routing fixes identified.

**UP-05 — Stair contract and traversable prototype**

Own, all **new**:

- `Source/PerfectWorldCore/Public/LaneStairLayout.h`
- `Source/PerfectWorldCore/Private/LaneStairLayout.cpp`
- `Tests/LaneStairLayoutTests.cpp`
- `Source/PerfectWorld/Private/PerfectWorldLaneStairTests.cpp`

Generate deterministic flights, landings, slab openings, protective barriers and clearance volumes from finished-floor datums. Derive risers to reach landings exactly; reject footprints that cannot fit safely. Test mirrored arrangements and supported height extremes.

Depends on NL-04’s measured scale. Integrator supplies an isolated voxel fixture; production building activation waits for the following round.

Acceptance: new portable layout tests and `PerfectWorld.LaneStair`; walk up/down in both cameras without ceiling contact, snagging or fall-through.

**UP-06 — Creation and pause carry the title’s richness forward**

Own:

- `Source/PerfectWorld/Private/UI/SPWCreationFlow.cpp`
- `Source/PerfectWorld/Private/UI/SPWReviewPage.cpp`
- `Source/PerfectWorld/Private/UI/SPWPauseMenu.cpp`
- `Source/PerfectWorld/Private/PerfectWorldCreationPauseTests.cpp` **(new)**

Build on existing fivefold styling: clearer current-step hierarchy, illustrated first-morning review, and pause context showing actual character/place/time with legible save feedback. Read existing core summaries; preserve transactions and progression.

Depends on B’s settled interfaces.

Acceptance: portable creation/presentation regressions; new `PerfectWorld.CreationPause`, existing `TitleView`, `StartAcknowledgement` and `PersistenceFlow`. Capture 720p/1080p, 150% UI scale, controller focus, reduced motion and failed Save/Start.

**Integration batches**

- **A — Persistent occupied slice:** B, NL-01/03/04, UP-01/02/04. One editor build, full automation, then package. Walk both origins, seating, Anindita’s partition route/yielding, five new doors/private visiting, after-hours exit, Mira and full application restart.
- **B — Resident and presentation:** UP-03/05/06. One build/full automation/package. Walk Subrata’s day, all-person Talk/Leave, stair fixture, creation/pause accessibility and restart regressions.

Do not activate the following round’s frontage batch until Batch A’s packaged gate passes.

**Two risks**

1. **Power-off and clock jumps:** checkpoint notes and reviewable diffs frequently; bind evidence to commit/package identity and process completion. Interrupted runs remain NOT RUN; timestamps alone prove nothing.
2. **Shared-seam drift:** freeze interfaces before consumers implement; integrate migrations/registrations centrally and preserve historical save fixtures. Passing automation cannot substitute for the packaged walk that previously exposed Anindita’s failure.