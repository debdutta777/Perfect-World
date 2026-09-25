# Starter checkpoint — 19 September 2026

## Completed

- C++/Unreal5.7 project structure, portable core, Blueprint subsystem and native Slate creation-menu source.
- Kolkata/West Bengal setting, fictional Nabapara neighborhood and Nabapara Vidyalaya, exactly16-year-old Class XI origin.
-20 trait ledger, nonlinear costs, exact1,000-point confirmation, premium cap, atomic invalid-edit rejection, refunds, default preset, free identity/cosmetic record fields and in-memory origin snapshot validation.
- Detailed documents and two concept-art sheets, plus a small-step development queue.

## Verification evidence

| Check | Result |
|---|---|
| Portable C++20 Release compile | PASS using MSVC19.44.35215 |
| CTest | PASS —1/1 test executable |
| Test assertions | PASS —225,187 checks including10,000 seeded edit sequences |
| Unreal5.7 Editor build | PASS —PerfectWorldEditor Win64 Development, all17 actions completed |
| Unreal reflection and module linking | PASS as part of Editor build |
| Visual Play In Editor/menu interaction | PASS for the desktop mouse smoke checks on20 September; see MENU_SMOKE_TEST.md |
| Cooked Windows game | NOT RUN |
| Android/iOS/console package or device tests | NOT RUN |
| Disk save/load, playable world, school or combat | NOT IMPLEMENTED |

Editor build was run in a separate local validation copy with the same final source. It compiled the Slate menu, Blueprint bridge and portable module successfully. The project delivery contains source and scripts, not a packaged game. Build artifacts stay in the task's work directory. The Desktop copy can be compiled with Scripts/build_unreal.py.

The host initially exposed duplicate Path/PATH environment entries, so the child environment was normalized for MSBuild. Unreal initially used incompatible V5 target defaults; GPT-5.6 Sol updated both targets to V6. The optional Unreal Build Accelerator could not write its global cache under the workspace sandbox, so the build scripts use the standard parallel executor with -NoUBA and a project-local log. No global SDK or compiler installation was changed.

## Model execution record

- **GPT-5.6 Sol:** initial C++ scaffold and design documents were partially written before quota interruption. Later small calls succeeded: portable-core review, UE target-setting correction, and Kolkata naming/world-bible finalization.
- **Claude Sonnet:** design review and production-plan draft completed through the installed signed-in Claude CLI. Returned model label: claude-sonnet-5. Review recommendations were curated in REVIEW_DECISIONS.md.
- **Claude Opus:** requested for source review, but the transfer was blocked by automatic approval review pending specific source-export authorization; no Opus review completed.
- **Main agent:** integrated the work, completed tests and menu, verified compilation and prepared the deliverables. The root model was not silently switched to Sol.
- **Imagegen:** generated original concept art. Generation does not produce playable assets.

No claim is made that model switching avoids shared account limits. Future tasks should default to GPT-5.6 Sol and stay small, with Sonnet and selective Opus as requested.

## Checkpoint — household templates and local save slots (bf91db6)

The five authored household templates from CHARACTER_CREATION.md (parent household, extended family, grandparent household, adult sibling household, appointed guardian) are now implemented in the portable core. Choosing one is free, costs no creation points, and is locked once life begins, the same as identity and cosmetics. Each template names a home kit ID that a future opening scene will build from; no such scene exists yet.

Creation records are now serialized to text (a JSON subset) by stable trait ID rather than by array position, so a renamed or reordered trait fails loudly instead of silently shifting ranks. The save schema moved from version1 to version2. Version1 records still load and migrate to the default household (`parent_household`). A version2 record naming an unknown household is refused outright, and version3+ is refused as unsupported. There are three save slots (`creation_slot_1.json` .. `creation_slot_3.json`) under the project's `Saved/SaveGames/` directory; saves write to a temporary file and move it into place so an interrupted write cannot destroy the previous record.

The Unreal subsystem now exposes household selection and the slot operations (list, describe, save, load, delete) to Blueprint.

| Check | Result |
|---|---|
| Portable CTest | PASS — 1/1 test executable, 225,230 assertions |
| New coverage | household rules, escaped identity round trip (quotes/backslashes), every truncation prefix of a serialized record, a renamed trait, corrupt/garbage text, unknown household, version-1 migration, version-3 rejection, out-of-range slots |
| Unreal PerfectWorldEditor Win64 Development | PASS — compiles |
| Visual Play In Editor smoke test of the household picker or slot buttons | NOT RUN |
| Packaged/cooked game | NOT RUN |
| Android test | NOT RUN |
| Playable room, school or city; art or level for the named home kits | NOT IMPLEMENTED |

## Decisions — 2026-09-20

The owner made four design decisions today that reverse or extend parts of the prior design contract. Full rules and rationale are in the referenced documents; this entry exists so the decision record is visible without opening every file.

1. **Household Resources Rank 0 now means institutional care (a children's home)**, not an ordinary family baseline — a direct reversal of the prior rule that Rank 0 was harm-free and that family hardship could never be a point-allocation outcome. This deliberately couples the point ledger to origin hardship for this one trait. See CHARACTER_CREATION.md ("Household selection") and GAME_DESIGN.md.
2. **Presentation is now a mix of 2D and 3D**, built so it can become fully 3D later. Gameplay, simulation, and world state stay 3D and authoritative; character display is a separate, replaceable layer (2D sprite now, skeletal mesh later) behind an interface, so nothing in the rules may depend on which display mode is active. See TECHNICAL_ARCHITECTURE.md and ART_DIRECTION.md.
3. **The character creator becomes a full 2D/2.5D anime-styled editor** with continuous colour sliders (skin, hair, eye) and discrete pickers for everything else (hair style, face shape, build, uniform variant) in this first version; continuous proportion/morph sliders are deferred until the 3D presentation layer exists. Appearance stays a versioned, engine-neutral data parameter set — never a chosen sprite or mesh reference — so the same saved appearance can drive 2D composition now and a 3D morph rig later. Appearance remains outside the point budget and must never affect point costs or supernatural potential. See CHARACTER_CREATION.md and ART_DIRECTION.md.
4. **New game flow:** Title screen (New Game / Load Game / Settings / Exit) → New Game opens the appearance editor → point allocation including household → play starts in the room the confirmed household template and Household Resources rank determine. See the rewritten queue in NEXT_SMALL_STEPS.md.

None of the code behind these decisions is implemented yet; they are recorded here as the current design intent. Separately: **the step-03 (now step02) save/load and household-picker work has still not been visually smoke-tested in Play In Editor.** That gap predates today's decisions and remains open — see NEXT_SMALL_STEPS.md step02 and the "Visual Play In Editor smoke test of the household picker or slot buttons: NOT RUN" line in the checkpoint above.

## Next checkpoint

The menu font/control polish is done and committed (`aab1e5c`). Next: a visual Play-In-Editor smoke test of the new household picker and the three save-slot buttons (save, load, delete, corrupt-file recovery message), recorded the same way as MENU_SMOKE_TEST.md. After that smoke test passes, proceed to step 04 (one greybox bedroom and a controllable isometric pawn). Do not add city, combat or full school simulation in the same step.

## GitHub checkpoint

Initial commits `f948f1f` and `fee9b4b` are pushed to [https://github.com/debdutta777/Perfect-World](https://github.com/debdutta777/Perfect-World), a private repository. Remote `main` matched local commit `fee9b4baafe04541e0f4dd80761e1f9460566109` when verified. Unreal-generated local configuration changes remain outside these commits.

## Checkpoint — full planning pass and documentation reconciliation, 2026-09-23

The owner completed a full planning pass: `Docs/GAME_PLAN.md` is now the master plan and single entry point, superseding this project's prior document hierarchy wherever they conflict. Four new detail documents were written alongside it — `Docs/SYSTEMS.md`, `Docs/ASSET_PIPELINE.md`, `Docs/DEVELOPMENT_WORKFLOW.md` and `Docs/ROADMAP.md` — and the older documents (`ART_DIRECTION.md`, `TECHNICAL_ARCHITECTURE.md`, `GAME_DESIGN.md`, `NEXT_SMALL_STEPS.md`, `PRODUCTION_PLAN.md`, `MASTER_PLAN.md`, `VISION.md`, `CHARACTER_CREATION.md`, `ART_PROVENANCE.md`, this file, and `README.md`) were reconciled against it: outdated sections updated where a document stays authoritative for its area, and dated superseded notices added where a document is now largely a historical record.

**The owner's decisions of 2026-09-23:**

1. **Voxel block visuals for the whole of development.** Every visual in the game — environments and characters alike — is built from cuboids with low-resolution, nearest-filtered pixel textures until the game is complete. It is a look, not a voxel engine or voxel terrain; the standard Unreal static-mesh pipeline stays in place. Block characters are cuboid parts on the standard Unreal humanoid skeleton, so animations built during development carry over to the final rig unchanged.
2. **Real art integrated only after the game is complete, at milestone M9.** It will come from Blender/FBX/GLB sources and AI-generated sources, arriving through both a drop folder and editor import. `Docs/ASSET_PIPELINE.md` is the authority on the placeholder style, asset IDs, the swap contract and both import routes.
3. **First-person camera by default, switchable to third person.** Any earlier mention of a fixed isometric camera or an isometric pawn is superseded. Third person must be fully supported, not a debug view; some contexts (the appearance editor, authored scenes, mirrors) choose the camera deliberately.
4. **No `Co-Authored-By` trailers on commits** (already recorded above and in `GAME_PLAN.md`'s decision log).

**Honest restatement, unchanged by the planning pass:** the six features listed as built in `GAME_PLAN.md` §11 — character creation rules, the engine-neutral appearance record, three JSON save slots with schema migration and resume points, the game clock, and the front end (title, load, settings, appearance editor, household and points panel) — remain **unverified in a launched window**. Closing that gap is still the first step of any further work. No world, room, character model, NPC, activity or gameplay exists yet. Planning got deeper today; nothing got built.

## Checkpoint — owner answers to §12 A and B, 2026-09-25

**The owner's decisions of 2026-09-25:**

1. **§12 A: the awakening cannot be refused.** It is universal and involuntary. Everything after it, including the examination and all supernatural practice, stays optional. Hanging back or withdrawing inside the tutorial dungeon is conduct and still ends in awakening; a student absent on the day is awakened at a supervised make-up session, whose cost M4-06 specifies.
2. **§12 B: fifty real countries, up from thirty.** Countries, cities and places are real. People are invented, or inspired by real roles and archetypes without being identifiable as anyone real. Institutions, companies, power blocs and incidents stay fictional, and no real-world conflict is depicted directly. `GLOBAL_WORLD.md` §8 holds the rules and a **proposed** roster, which the owner confirms before M3-10 authors it as data.

Propagated to `GAME_PLAN.md` (§1, §4, §7, §8, §10, §12), `GLOBAL_WORLD.md`, `AWAKENING_AND_EXAM.md`, `VISION.md`, `PATHS_AND_PROGRESSION.md`, `ALTERNATE_WORLD.md`, `SYSTEMS.md` (every nation-scaled budget scaled by 5/3; the age-80 save budget is now 9 MB), `SYSTEMS_CATALOGUE.md`, `ROADMAP.md` (M3-10 unblocked pending the roster, M4-06 rewritten), `ROADMAP_LATER.md`, `DEVELOPMENT_WORKFLOW.md`, `NAMING.md`, `GAME_DESIGN.md`, `MASTER_PLAN.md`, `AGENTS.md` and `README.md`. The World Convergence field is now eighty per discipline: fifty guaranteed plus thirty on merit, still capped at three per nation.

**Engine move (M0-12a):** the owner is installing Unreal Engine 5.8 through the Epic Launcher; the download was under way on 2026-09-25. An agent tried twice to install VS 2026 Build Tools with MSVC 14.50, Epic's recommended toolset for 5.8. Both attempts ended at the Windows administrator prompt (installer exit code 1602, cancelled), so nothing was installed. The machine has VS 2022 Build Tools 17.14.13 with MSVC 14.44.35207, which meets 5.8's stated minimum (VS 2022 17.14). 5.7's own configuration lists 14.44.35207 as a banned version, so 5.8's `Windows_SDK.json` must be checked in M0-12b.

No code changed. Nothing was built or run in this checkpoint.
