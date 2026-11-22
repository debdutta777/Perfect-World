# LL-02 submission 2b handoff

2026-10-17, `game/presence-2b`, `D:/PWWork/worktrees/w-presence2b`. Implementation complete; Unreal compilation and runtime acceptance pending. No commit or Unreal build.

Intent: move Farida onto the generic presenter, bind Anindita to the built stationery shop, and submit Exit from the authored inside of a door.

Completion criterion: no old Farida bridge symbols remain in Source; both shop providers register their scheduled person and reconcile; door toggles distinguish inside from outside. Source review only here; the integrator owns Unreal compilation and runtime acceptance.

Progress:
- Read AGENTS.md, development workflow including section 4, submission 1 handoff, master-plan/system/roadmap/build guidance and relevant schedule/layout/presenter contracts.
- Confirmed stationery frontage X -100, Index 2, side +1, width/depth 700, setback 60, three storeys. Its front-door anchor is already on the footpath; the four schedule segments need no extra local connector.
- No Ruflo tools are exposed in this session. The concurrent presenter's files and all tests remain outside this submission's ownership.

Progress (implementation authored):
- Replaced the Farida-only bridge with RegisterTeaShopPresence and migrated spawn/conversation/clock/reset paths.
- Added NpStationeryBindings: layout anchors, three layout polylines plus the off-lane home segment, footpath/home bindings, front/internal doorway zones and waiting points. KolkataLane registers the provider and Anindita after collision is committed.
- Door specs pass their authored inside side to LaneLife; RequestDoorToggle derives Exit from the pawn position in the closed actor frame. Missing side/pawn and the exact door plane retain Entry.

## Files changed

- `Source/PerfectWorld/Public/PerfectWorldLaneLifeSubsystem.h`: replaces Farida's public/private bridge API and seven fields with RegisterTeaShopPresence; adds RegisterDoorInsideSide and transient authored door-side map.
- `Source/PerfectWorld/Private/PerfectWorldLaneLifeSubsystem.cpp`: removes Farida cleanup and conversation special case; stores authored door sides and derives DoorDirection from the operating pawn's position.
- `Source/PerfectWorld/Private/PerfectWorldLaneLifeClock.cpp`: removes old Farida calls/instant reset; retains existing presenter reconciliation and ResetPresentation hooks.
- `Source/PerfectWorld/Private/PerfectWorldLaneLifeFarida.cpp`: replaces the old controller with the submission-1 tea-shop provider adapter, loading through LoadNpcSchedule. Local segment endpoints are normalized against the schedule's From/To anchors; home stays explicitly off lane.
- `Source/PerfectWorld/Private/PerfectWorldKolkataLane.cpp`: FaSpawnFarida calls RegisterTeaShopPresence without a direct spawn; PbSpawnAnindita recovers the stationery building, registers its provider/person and reconciles; SpawnDoors passes each spec's authored inward side to LaneLife.
- `Source/PerfectWorld/Private/PerfectWorldLaneStationeryShop.h`: declares NpStationeryBindings.
- `Source/PerfectWorld/Private/PerfectWorldLaneStationeryShop.cpp`: implements the provider from GetStationeryAnchors/GetStationeryLayout. Three local routes retain their layout point order, and the fourth route is explicitly off lane. Both front-leaf swing directions and the sales-to-stock doorway have exclusion zones and waiting points.
- `Docs/LL02_SUBMISSION_2B_HANDOFF.md`: added and updated throughout this work.

No other files were edited by this submission. No Config, test, movement, presence, shared binding or yield files were edited. No new implementation files or build configuration changes are needed.

## Removed symbols and test follow-ups

Removed declarations/definitions: PrepareFarida, RegisterFarida, RefreshFaridaForClock, GetFaridaIntent, ResolveFaridaIntent and HandleFaridaBlock.

Removed Farida-only fields: FaridaSchedule, FaridaIntent, Farida, FaridaAnchors, FaridaRoutes, FaridaResolvedInstant and bFaridaBlocked. The portable PerfectWorld::FaridaSchedule type and content constants still exist; only the LaneLife members were removed.

Full Source scan: zero remaining references to the retired bridge methods, intent/anchor/route/instant/block fields. Source and Tests test-file scan: zero tests reference a removed bridge API, so no named test needs a migration in this checkout. Tests were not edited.

For the concurrent test agent/integrator:
- Use presence.FindIntent(FName(TEXT("person.farida_qadri"))) instead of GetFaridaIntent; FindPerson for the actor; RegisterTeaShopPresence on the built tea-shop frame for production registration, or the existing generic provider/explicit-content API in isolated presenter fixtures.
- Any new LaneLife egress fixture that manually spawns/configures a door must also call RegisterDoorInsideSide with its authored closed-frame local-Y sign. Production SpawnDoors already does this. No APWDoorActor or presence/bindings API change is required.
- Useful integration coverage: Anindita's provider resolves all schedule anchors and four segments, keeps the three local route lengths 635/727/1100 cm, and keeps front-door/footpath positions equal. Door command coverage should include outside refusal and inside success at 13:00 and 19:30, a missing pawn/side remaining Entry, and a negative authored inside sign.

## Assumptions and boundaries

- Current lane door specs author PreferredSwing toward the inside/entered room; SpawnDoors passes that sign explicitly. The actor's root stays in the closed frame while only its leaf pivot swings. Direction uses dot(pawn minus hinge, actor local +Y) times the authored sign, strictly greater than zero. No current leaf angle, swing side or core default sign is used to guess inside.
- DoorInsideSides is transient authored geometry: initialized/cleared with the subsystem, retained through ResetWorld alongside existing door actors, and refreshed by each production spawn. Missing pawn/side and a pawn exactly on the door plane use Entry. Future door spawn paths must supply their authored side too.
- Egress bypasses hours/access in Doors.h; collision and leaf-sweep refusal still apply. This does not add automatic door closure, NPC door operation, or an open-door crossing barrier.
- Stationery recovery reuses IsStationeryBuilding on Fronts[1]: X -100, Y 460, Index 2, width/depth 700, setback 60, three storeys, Side 1. Registration occurs only if HasStationeryShop succeeds and after block collision is committed.
- The stationery front-door anchor is already at U250/V-45 on the footpath. location.nabapara.stationery_footpath aliases that transform, and Sen home's ExitAnchor uses it. The schedule's off-lane segment starts at anchor.stationery.front_door; no invented connector or charged distance is added.
- Doorway waiting candidates use front footpath offsets and authored staff anchors. The existing yield system still filters candidates for all zones, occupancy, floor support, player separation and detour distance; no candidate forces relocation.
- Farida keeps the submission-1 adapter's anchor/route contract, including the rest anchor's sit preference. Presenter visibility/collision rules can defer physical arrivals; the portable intent remains authoritative. No new tea-shop doorway zones were added by this adapter.
- The integrator owns STATUS and the build/runtime rungs; this submission records its review here to avoid editing another agent's files.

## Validation actually performed

Read-only PowerShell source audit, run in this worktree, passed:
- `Get-ChildItem Source -Recurse -File | Select-String -Pattern '\b(PrepareFarida|RegisterFarida|RefreshFaridaForClock|GetFaridaIntent|ResolveFaridaIntent|HandleFaridaBlock|FaridaIntent|FaridaAnchors|FaridaRoutes|FaridaResolvedInstant|bFaridaBlocked)\b'`: zero matches.
- The corresponding Source/Tests `*Tests.cpp`/`*Tests.h` scan: zero retired API references.
- Audited each of the seven owned C++ files for fewer than 500 lines and each changed .cpp for its own header first (LaneLife translation units use PerfectWorldLaneLifeSubsystem.h). PASS; largest is LaneLifeSubsystem.cpp at 450 lines.
- Parsed npc_schedule.anindita_sen.json with ConvertFrom-Json and compared all four route IDs with the adapter's route set using Compare-Object: identical.
- Read the complete changed implementations and their public contracts against NpcScheduleContent.h, presence/binding/yield APIs, LaneInteriorLayout.h, the layout builder and Doors.h/Doors.cpp. New file-local helper names use Pb. No Slate widgets/lambdas or text-role code were added.

Git diff/status could not run: git was absent from PATH; the discovered Git executable returned Access is denied. No permission escalation or workaround was attempted.

NOT RUN: CTest (no portable rules changed), UHT/editor compile, Unreal automation, PIE, packaged play. Source audits do not establish compilation or runtime behavior.

## Integrator compilation and play observations

1. Compile PerfectWorldEditor once with the editor closed, including the concurrent test agent's changes. Run PerfectWorld.NpcPresence, PerfectWorld.NpcYield, PerfectWorld.NpcMovement, TeaShop, InteriorKit and Doors automation. No new source registration or Build.cs edits are needed.
2. Farida: use normal clock advancement and a paid conversation choice across a boundary; watch her afternoon transition out of rest (14:36), reach the stove by 14:45 and serve again from 15:30. Confirm one presenter-owned actor, talk availability from generic intent, no duplicate/direct fallback spawn and no visible reconciliation teleport. Look away if the presenter defers an arrival. Reset and confirm one fresh presentation.
3. Anindita: opening play starts at 07:30 stock work. Observe her reach the counter at 08:00, and confirm the front door refuses outside opening before 08:00 and accepts it at 08:00. Observe the stockroom/packing routes through the staff gap, with no route-binding diagnostic.
4. Meal closure: at 13:00 outside opening is refused; Anindita leaves for her meal, is home by 13:23, leaves home at 14:37 and serves again at 15:00. A watched departure may stay at the exit until unseen. At each doorway, approach her, observe a safe sidestep and return after retreat; neither doorway should require pushing past her.
5. After 19:30: enter the stationery shop while open, close the door, remain inside past 19:30, then operate it from the inside with the sweep clear. Opening must succeed as Exit. Close it from outside and attempt to reopen: entry must be refused. Repeat inside egress during the 13:00 meal closure and on the tea shop's opposite local-Y inward sign. Anindita may still be packing after closing; she should reach home at 21:00.
6. Exercise conversation pause/leave and reset with both proprietors, plus the unchanged care worker and household adult. Observe in both cameras. Record actual build/automation/play results in STATUS; no runtime acceptance is claimed here.

Suggested STATUS record: Intent as above; Files changed as listed; Exact check the source-audit commands above; Result PASS for source audits, NOT RUN for Unreal compile/automation/play; Remaining limitation integrator compilation and in-play acceptance; Next smallest action compile once and perform the observations above.
