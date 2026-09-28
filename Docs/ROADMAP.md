# Roadmap

**Document status:** execution plan, 2026-09-23. It answers to `GAME_PLAN.md`, whose milestone gates (§8, as revised the same day after the GPT-6 review) it copies word for word. It **replaces** the old step queue (deleted 2026-09-28) and the build order in `MASTER_PLAN.md` §7 (the mapping is in §1.5). Process lives in `DEVELOPMENT_WORKFLOW.md`, system data and commands in `SYSTEMS.md`, and the block style, asset IDs, contracts, baker, resolver and import routes in `ASSET_PIPELINE.md` (its §12 is the per-milestone pipeline schedule these steps follow). **If this roadmap names a type, file or command differently from `SYSTEMS.md` or `ASSET_PIPELINE.md`, those documents win.** M0–M4 are detailed here. M5–M10 are detailed in [`ROADMAP_LATER.md`](ROADMAP_LATER.md).

**Where things stand (2026-09-28):** the portable rules (M0-01, M0-03, M0-05, M0-06, M0-08, M0-10 rules, M0-11) pass CTest, and both build rungs are green on Unreal 5.8. The flat Slate front end was **deleted** by owner decision (M0-13a), so every step that only built or tested those screens is *dropped*. Work continues on the 3D block game: M1 and the world-scale lane (§5a).

---

## 1. How to read this

### 1.1 Step format

Every milestone opens with its **goal**, its **gate** and the **playable result** it leaves behind. Below that is one table row per step:

| Column | Meaning |
|---|---|
| **ID** | `M<milestone>-<nn>`. From now on IDs are never renumbered. A step added later gets a letter suffix (`M1-07a`), and a dropped step stays in the table marked *dropped* |
| **Deliverable** | The one thing the step produces. If it does not fit one row, split the step before starting (`DEVELOPMENT_WORKFLOW.md` §2) |
| **Touches** | The systems and files the step may change. Paths use the shorthand in §1.2. Registering new files in `CMakeLists.txt`, `.Build.cs` or `.uproject`, and the `STATUS.md` entry, are always allowed and never listed |
| **Pass condition** | An objective check. A step is not done until this holds **at its rung** |
| **Rung** | The highest verification rung required: **CTest**, **Editor** (UHT and compile), **PIE** (Play In Editor), **Cooked** (packaged Windows build). A higher rung includes the lower ones that apply. **+owner** means the owner must perform the check or judge the result |
| **Deps** | Steps that must be done first. `§12 X` is an open owner question in `GAME_PLAN.md`; `R<n>` is a roadmap decision in §3 |
| **Status** | `done <commit>` · `in progress` (with the reason) · `not started` · `blocked` (with the blocker) |

### 1.2 Shorthand

- `Core/Name` means `Source/PerfectWorldCore/Public/Name.h` and `Private/Name.cpp`: portable C++20 with no Unreal includes. Every Core step also adds or extends `Tests/<Name>Tests.cpp`, registered as in the `DEVELOPMENT_WORKFLOW.md` §8 recipe.
- `Game/Name` means `Source/PerfectWorld/Public/PerfectWorldName.h` and `Private/PerfectWorldName.cpp`, using the existing `PerfectWorld` file prefix and `PW` type prefix. `Editor/` is the new editor-only `PerfectWorldEditor` module (`ASSET_PIPELINE.md` §4).
- `Data/...` is validated JSON content (from M1-06). `Layouts` are asset IDs plus transforms placed as `APWAssetActor`s, so maps stay thin and can be diffed. Agents must not maintain large binary level graphs (`MASTER_PLAN.md` §5).
- **Audit 0** means two things hold for the space under test: the coverage report (`-run=PWAssetCoverage`) lists no error-cube IDs, and the validator's hard-reference rule passes (`ASSET_PIPELINE.md` §5 and §9).
- **Budgets** means the `ASSET_PIPELINE.md` §4 budgets, confirmed and first measured in M1-12. The block world may use only half of each.

### 1.3 Working a step cold

1. Read the row, every row in its Deps, and the `SYSTEMS.md` or `ASSET_PIPELINE.md` section for the system it names.
2. Write the intent and the pass condition into `STATUS.md` before writing code.
3. Change only what **Touches** allows. If more is needed, stop and split the step.
4. Run the rung with the exact commands in `DEVELOPMENT_WORKFLOW.md` §3. The CTest build directory must be a short path: `C:/Users/DEBDUTTA/AppData/Local/Temp/pwcore` on the owner's desktop, `D:/pwcore` on the cloud PC. The cooked command is added by M0-15.
5. Record the six-field `STATUS.md` entry. Update this row's Status only when the rung has passed.

### 1.4 Standing rules for every step

- One clock. Portable commands change validated state; actors only present it. No persistent change is made from a frame tick.
- Every save-schema change bumps the version and adds migration tests from every supported older version (`DEVELOPMENT_WORKFLOW.md` §8).
- From M1-10 on, every visual and sound resolves through an asset ID, and gameplay data (collision, anchors, seats, navigation) lives in data, not in meshes.
- No Lumen, Nanite, World Partition or MetaHuman. Keep every file under 500 lines. No `Co-Authored-By` trailers. Agents do not commit; the integrating session does.
- Player-facing text goes through string tables from M1-14 on. Numbers such as cases, durations and thresholds are data unless a step says otherwise.

### 1.5 What this replaces

| Old item | Now |
|---|---|
| Old step queue (deleted 2026-09-28) | Its steps live on as M0-01 to M0-11, M1-20 to M1-22, M2-03, M2-15 to M2-18, M3-11, M3-13 and M10-04 |
| `MASTER_PLAN` §7 A–H | A = dropped with the flat front end (M0-13a) · B = M0-11 (done) · C = M2-01 · D = M2-03, M2-04 · E = M3-05 · F = M1-05 · G = M1-21 · H = M2-24 |

---

## 2. Critical path

```
M0-12 -- M0-13a flat front end removed -- M0-15 first cooked build ==> M0 GATE
M1-01 reducer -- M1-02 RNG -- M1-03 hash --+        <- portable lane: start now, needs no UI
M0-18 identity/stream schema v5 -----------+-- M1-05 world save v6
M0-13a -- M1-04 slot store ----------------+
M1-25 realm coordinate -- M1-26 generator -- M1-27 deltas -- M1-28 streamer -- M1-29 city data
  -- M1-30 hero district -- M1-31 load measurement      <- world-scale lane (§5a)
M1-06 validator -- M1-07 contracts -- M1-08 style kit -- M1-09 baker -- M1-10 resolver
  -- M1-11 block character -- M1-13 controller -- M1-15 world entry -- M1-16 clock
  -- M1-17 interaction -- M1-18 hands -- M1-21 home -- M1-22 street -- M1-24 ==> M1 GATE (owner)
M2-01..M2-12 portable (can start once M1-05 lands) -- M2-13..M2-19 presentation
  -- M2-20 harness -- M2-23 forty-exposure test (owner) -- M2-24 ==> M2 GATE
M3-01 time contract -- M3-02 compression -- M3-03 equivalence test (owner) -- M3-04 repetition gate (owner)
  -- M3-08 content expansion .. M3-18 adult probe (owner) -- M3-20 cultural review -- M3-21 ==> M3 GATE
M4-01..M4-04 portable + M4-05 combat spike (owner) -- M4-06..M4-14 -- M4-15..M4-19 -- M4-20 BLIND TEST ==> M4 GATE
M5 -- M6 -- M7 -- M8 -- M9 -- M10  (ROADMAP_LATER.md)
```

- **Two lanes, always.** The portable lane (CTest only) never waits for the owner. The presentation lane waits on PIE checks and playtests. Keep agents on the portable lane while the owner is playtesting.
- **Portable before presentation.** The clock, reducer, RNG streams and world save come before anything in the world. Condition, activities, practice limits and identity come before the school day. The projection, threads and time contract come before the year. Awakening and tower scoring come before the dungeon and tower maps.
- **Registry before content.** No world content (M1-21 onward) is built until the contracts, style kit, baker and resolver (M1-07 to M1-10) exist.
- **The repetition gate** is M2-23 plus M3-04. No problem-family expansion (M3-08) and no new structure use in M5 or M8 goes ahead until the structure involved has passed forty exposures.

---

## 3. Owner gates and decisions

**Playtests and judgements only the owner can do** (tagged +owner in the tables): M0-12 (approve the commit), M0-15 · M1-08, M1-12, M1-13, M1-18, M1-22, M1-24 · M2-14, M2-18, M2-21, M2-23, M2-24 · M3-03, M3-04, M3-08, M3-12, M3-13, M3-17, M3-18, M3-20, M3-21 · M4-05, M4-07, M4-13, M4-18 to M4-21.

| Decision | Needed before | Question |
|---|---|---|
| ~~W1–W4~~ | M1-25, M1-29 | **Working decisions 2026-09-28 (delegated by the owner):** Kolkata, four Indian cities, then twelve foreign cities · no OpenStreetMap yet · one shared Deep Realm with a home region per nation · stacked floors, floor 1 at least ten Earths (`WORLD_SCALE.md` §7). The owner may override |
| **R1** | M1-23 | Creation screen order. GAME_PLAN lists household, stream, traits, but the deleted flat panel combined household and traits. Proposed: Appearance → Identity → Household and traits → Stream → Review |
| **R2** | M0-18 | Migration defaults for saves older than v5, which have no date of birth or stream. Proposed: 1 January 2026 and `science`, marked as migrated |
| **R3** | M1-21 | The code has five template home kits (`compact_family_flat`, `row_house`, `cooperative_flat`, `rented_flat`, `staff_quarters`) plus the dormitory. GAME_PLAN §7 has three shells. Which templates share which shell? |
| **R4** | M1-12 (asked for in M0) | Target hardware and the budget table (`ASSET_PIPELINE.md` §4 and §12, decision 1) |
| **R5** | M2-23 | Pass thresholds for the forty-exposure test: how much decisions must change, voluntary foreground rate, comprehension score |
| **R6** | M4-16 | Where the blind test starts. Eight testers cannot play 12–20 hours of latent life. A prepared late-year save, or a compressed latent span? |
| **R7** | any time | When to run the Android feasibility spike. The earlier architecture document (deleted 2026-09-28) wanted it early; GAME_PLAN has no Android step before M10 |
| **R8** | M3-20 | A Bengali cultural reviewer booked early enough, given the lead time |
| **R9** | M3-13 | Who writes the presentation bible (GAME_PLAN §3), and where it lives |
| **AP 2–5** | M1-08 to M1-11 | `ASSET_PIPELINE.md` §12 decisions: procedural or Epic mannequin animation; AppearanceRecord gaps (lower garment, casual colour, height; accepting them means a schema bump); forward with MSAA or deferred with TSR; delete the CC0 packs |
| ~~§12 A / B~~ | M4-06 / M3-10 | **Answered 2026-09-25:** the awakening cannot be refused · fifty real countries (`GLOBAL_WORLD.md` §8). The owner still confirms the proposed roster before M3-10 |
| §12 C / D / E | M5-11 / M5-09 / M3-11 | Legacy play · adult relationships · content rating (how Gate casualties appear in news) |
| §12 F / G / H | M8-03 / M10-03 / M10-06 | Starting cities · localisation (string tables are used from M1-14 regardless) · business model |

**Good work for AI agents:** every CTest step, validators, the baker and runtime builder, build and packaging scripts, Unreal bridges, harnesses and log tools, first drafts of data and dialogue. **Needs the owner:** anything judged by feel or look, cultural and dignity review, playtests, the blind test, every R-, AP- and §12 decision, and supplying art at M9. **PIE steps are shared:** an agent implements and runs the Editor build, then the owner, or an agent following a written script and reporting faithfully, runs the check (`DEVELOPMENT_WORKFLOW.md` §3, rung 3).

---

## 4. M0 — Foundation

**Goal:** a green baseline on Unreal 5.8, the flat front end removed, and a first cooked Windows build that boots into the block world. **Gate (GAME_PLAN §8, revised 2026-09-28):** *"Both build rungs green on Unreal 5.8; the flat front end removed (2026-09-28); the packaged game boots into a block test map and quits cleanly. First cooked Windows build."* **Playable result:** a packaged game that opens a block test map. Creation, saving and the title return in 3D during M1.

| ID | Deliverable | Touches | Pass condition | Rung | Deps | Status |
|---|---|---|---|---|---|---|
| M0-01 | Creation ledger (20 traits, 1,000 points, rank-4 cap, Balanced Scholar) and Slate creator | Core/CharacterCreation; Game/CharacterCreatorSubsystem, MenuGameMode | CTest ledger suite; mouse PIE check of 20 Sep (record deleted 2026-09-28) | PIE | — | done `f948f1f`, `fee9b4b` · Slate creator removed by M0-13a; rules kept |
| M0-02 | Larger menu text and controls | Game/CreatorPanel | Compiles; legibility rechecked in M0-13 | Editor | M0-01 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-03 | Household templates, stable-ID JSON save, schema v2 with v1 migration | Core/CharacterCreation, CreationSaveSerialization | CTest round trip, truncation, migration | CTest | M0-01 | done `bf91db6` |
| M0-04 | Household picker and three save-slot buttons | Game/CreatorPanel, CharacterCreatorSubsystem | S14, S20, S21 | PIE | M0-03 | done for the rules and slot files (`bf0636c`); picker and buttons dropped with M0-13a |
| M0-05 | Opening-home plan (template × resources rank) | Core/CharacterCreation | CTest | CTest | M0-03 | done `32bca1b` |
| M0-06 | Household Resources rank 0 means institutional care (rule and creator message) | Core/CharacterCreation; Game/CreatorPanel | CTest passed; message is S19 | PIE | M0-05 | done for the rule (`c081998`, CTest); the creator message dropped with M0-13a |
| M0-07 | Title, Load Game and Settings screens | Game/MenuGameMode | S1–S8, S32 | PIE | M0-04 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-08 | Engine-neutral appearance record, schema v3 | Core/Appearance | CTest | CTest | M0-03 | done `aaf0529` |
| M0-09 | Appearance editor, placed before points | Game/AppearancePanel, MenuGameMode | S9–S13 | PIE | M0-08 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-10 | Resume points, schema v4 | Core/CharacterCreation; Game/MenuGameMode | CTest passed; S21–S24 | PIE | M0-09 | done for the rule (`c8cc9a3`, CTest); screens dropped with M0-13a |
| M0-11 | Game clock (MASTER_PLAN step B) | Core/GameClock | 16,857 assertions pass | CTest | — | done `00e5425` |
| M0-12 | Baseline: both rungs run on HEAD; owner reviews and commits the planning documents; config churn handled per `DEVELOPMENT_WORKFLOW.md` §4; `.gitignore` and `.gitattributes` extended per `ASSET_PIPELINE.md` §11, plus `.claude-flow/` | STATUS.md, .gitignore, .gitattributes | Both test executables pass, with assertion counts recorded; editor build passes; `git status` shows only intentional churn | Editor +owner | — | done 2026-09-28 on the cloud PC · both rungs green on HEAD `a6b0ac4`: 225,395 + 16,857 checks; editor build 22/22 |
| M0-12a | **Install Unreal Engine 5.8** through the Epic Launcher (owner decision 2026-09-25: upgrade for Epic's official Unreal MCP) | — | `C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat` exists | — | — | done 2026-09-25 · owner installed 5.8.0 and VS 2026 Build Tools 18.10.2 |
| M0-12b | Migrate the project to 5.8: check the installed MSVC against 5.8's `Engine/Config/Windows/Windows_SDK.json` (preferred and banned versions), set `EngineAssociation` to 5.8, regenerate, rebuild, fix deprecations. Epic recommends MSVC 14.50 from VS 2026 Build Tools; VS 2022 17.14 is the stated minimum | PerfectWorld.uproject; Source/ | `build_unreal.py` succeeds on 5.8 (it reads the engine version from the project); both CTest executables pass; the editor opens the project | Editor | M0-12a | done 2026-09-25 · targets moved to `BuildSettingsVersion.V7`; editor build 22/22 with MSVC 14.50 (14.50.35739), no warnings; both CTest executables pass; the editor opens the project |
| M0-12c | Enable Epic's **Unreal MCP** plugin with **All Toolsets**; auto-start the server; run `ModelContextProtocol.GenerateClientConfig ClaudeCode` in the editor console | Config/; .mcp.json | `claude mcp list` shows `unreal` connected at `http://127.0.0.1:8000/mcp`; one read-only tool call succeeds. Localhost only: no authentication, never exposed off the machine. Experimental in 5.8 | Editor | M0-12b | done 2026-09-25 · config in `Config/DefaultEditorPerProjectUserSettings.ini`; plugins limited to the Editor target; `claude mcp list` shows `unreal` connected; `list_toolsets` answered. The user-scope registration already existed, so no `.mcp.json` was generated |
| M0-13 | Launched-window smoke test: checks S1–S32 below, in PIE and in a standalone `-game -windowed` window, with a screenshot of each screen | Docs/MENU_SMOKE_TEST.md (new dated section) | Every check has PASS or FAIL recorded; every FAIL has a defect line (D1, D2, ...) with steps to reproduce | PIE +owner | M0-12 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-13a | **Remove the flat Slate front end** (owner decision 2026-09-28): the title, load, settings, appearance and creator panels, the menu game mode, the 2D concept art and nine superseded documents | Game/MenuGameMode, AppearancePanel, CreatorPanel; Config/DefaultEngine.ini; Art/; Docs/ | Both rungs still pass; no code or config refers to the removed classes; no current document links to a removed file | Editor | M0-12 | done 2026-09-28 · editor build 7/7 Succeeded; CTest 2/2 |
| M0-14 | Fix every M0-13 defect, one commit each. Includes the known gap: add Delete per slot on Load Game, with an in-screen confirmation | Game/MenuGameMode, panels, CharacterCreatorSubsystem as each defect needs | Each failed check re-runs PASS; S1, S12, S20, S21 re-run as regression; STATUS marks the six features verified | PIE +owner | M0-13 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-15 | Packaging script and **first cooked Windows build** that boots into a block test map | Scripts/package_windows.py (RunUAT BuildCookRun, Development, `-NoUBA`, log to `Saved/Logs`); rung-4 command added to `DEVELOPMENT_WORKFLOW.md`; Content/Maps/MAP_Test_Block | Script exits 0; the packaged exe opens the test map, the player can look around, and Alt+F4 or the quit key closes it with no crash reporter | Cooked +owner | M0-13a | not started |
| M0-16 | Continue on the title: loads the most recently written slot and goes to its resume point; disabled when no slot exists | Game/MenuGameMode, CharacterCreatorSubsystem | Save slot 3, then slot 1: Continue loads slot 1. With no slots, Continue is disabled. Same result after a relaunch | PIE | M0-14 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-17 | Credits screen: project, engine notice, and every entry in `ART_PROVENANCE.md` | Game/CreditsPanel, MenuGameMode | Opened and closed by mouse, keyboard and controller; every provenance entry is listed | PIE | M0-14 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-18 | Identity and stream in the record, **schema v5**: date of birth (exactly 16 on 2 Jun 2042), pronouns, languages, `SchoolStreamId` and reason, resume points `identity`, `stream` and `review` | Core/CharacterCreation, CreationSaveSerialization | CTest: born 2 Jun 2025 refused; 3 Jun 2025 and 2 Jun 2026 accepted; 3 Jun 2026 refused. The stream changes no cost or stat. v1–v4 fixtures migrate to the R2 defaults. An unknown stream is refused | CTest | M0-12, R2 | not started |
| M0-19 | Identity screen after Appearance | Game/IdentityPanel, MenuGameMode | An invalid date cannot be entered; values survive save/load and Back/Continue | PIE | M0-18, R1 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-20 | Stream screen: Science, Commerce or Humanities, plus a reason. States that it sets a timetable, not a destiny | Game/StreamPanel | The choice survives save/load; points and stats are unchanged | PIE | M0-18, R1 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-21 | Review screen; Confirm moves here from the creator panel | Game/ReviewPanel, CreatorPanel | Shows home, stream, identity, 1,000/1,000 and every nonzero trait in words. Confirm is enabled only when valid. After Confirm, every creation screen is read-only | PIE | M0-19, M0-20 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-22 | The four other verified presets from `CHARACTER_CREATION.md` (only `balanced_scholar` is accepted today), with a preset picker (the picker returns with the 3D creator, M1-23) | Core/CharacterCreation | CTest: each preset totals exactly 1,000 and respects the cap. CTest only until M1-23 | CTest | M0-13a | not started |
| M0-23 | Creation timing check | — | The owner creates a character with a preset in under 8 minutes and from scratch in 20–30; the draft survives Back on every screen | PIE +owner | M0-21, M0-22 | dropped 2026-09-28: flat front end removed (M0-13a) |
| M0-24 | Cooked build of the finished front end | Scripts/package_windows.py | M0-15's checks, plus identity, stream, review, Continue and Credits, pass in the packaged exe | Cooked +owner | M0-16 to M0-23 | dropped 2026-09-28: flat front end removed (M0-13a) |

The M0-13 smoke-test checklist (S1–S32) tested the deleted screens and was removed with them; it is in git history before commit M0-13a if the checks are wanted for the 3D menus.

---

## 5. M1 — Walkable block world

**Goal:** the portable foundation (reducer, RNG, hash, world save, validator), the asset pipeline (contracts, style kit, baker, resolver), then a controllable block character in one furnished home and on one street. **Gate (GAME_PLAN §8):** *"In both camera modes: sit, read, select a small item, use a tool, speak to someone, pass through a furnished room, switch camera."* **Playable result:** confirm a character, wake in a block home, do all seven actions in both camera modes, walk to the school gate, save and resume there.

| ID | Deliverable | Touches | Pass condition | Rung | Deps | Status |
|---|---|---|---|---|---|---|
| M1-01 | Command reducer: `WorldState` and `Reduce(state, command)`, which returns new state and events, or a refusal. `AdvanceTime` is the only way the clock moves | Core/WorldState, Commands, WorldReducer | CTest: a refused command leaves the state identical; the same input gives the same output; `AdvanceTime` fires events in order and reports days crossed | CTest | M0-11 | not started |
| M1-02 | Deterministic RNG streams (`opportunity`, `tower`, `ambience`, `crystallisation`), one fixed algorithm, seeded from a per-life seed | Core/RandomStreams | CTest: golden values for a fixed seed; drawing from one stream never moves another; restoring mid-sequence continues identically | CTest | M1-01 | not started |
| M1-03 | State hash and command-log replay | Core/WorldHash | CTest: 10,000 seeded random command sequences replay to identical hashes; changing any single field changes the hash | CTest | M1-02 | not started |
| M1-04 | Portable slot store: temp write, parse-back check, atomic replace, one `.bak`. The subsystem calls the core only | Core/SaveStore; Game/CharacterCreatorSubsystem | CTest in a temp folder: a stale temp is ignored; a corrupt primary falls back to `.bak` with a message; missing and corrupt are told apart | CTest | M0-13a | not started |
| M1-05 | World save, **schema v6**: the slot holds the creation record plus a world snapshot (clock, RNG states, player zone and transform, camera mode, world flags by stable ID). Saves from v1–v5 start at 2 Jun 2042 06:00 in the origin home | Core/WorldSave | CTest: round trip; every truncation refused; every older fixture migrates; the loaded hash equals the saved hash | CTest | M1-03, M1-04, M0-18 | not started |
| M1-06 | Content registry and a validator executable that CTest runs over `Data/` and over bad fixtures | Core/ContentRegistry; Tools/ContentCheck; Tests/Fixtures | Real data passes. Fixtures with a duplicate ID, a dangling reference, an unknown field and a bad unit each fail, naming the file and ID | CTest | M1-01 | not started |
| M1-07 | Registry schema `pw.asset/1`, contracts for the M1 families, and about 40–50 IDs (home shell, furniture, small items, one tool, street, gate) | Data/Contracts/*.json; Data/Assets/<category>/<subcategory>/*.json; validator rules | Validator passes; every anchor lies inside its bounds; seated and camera clearance are declared for both modes; doors are 2.10 m | CTest | M1-06 | not started |
| M1-08 | Block style kit: the PW-128 palette, about 40 tiles, texel classes and texture group, block materials with face shading, a lighting rig, an anti-aliasing comparison | Art/Blocks/; Content block materials | Each tile and material renders nearest-filtered at its texel class; the owner decides forward/MSAA or deferred/TSR from the comparison captures | PIE +owner | M1-07, AP 4 | not started |
| M1-09 | Baker `-run=PWBakeBlocks` (writes to `/Game/Generated/Blocks/`, stamped with a definition hash) and `UPWBlockRuntimeBuilder`; the packaging script bakes before cooking | Editor/ (new module); Game/BlockRuntimeBuilder; Scripts/package_windows.py | One asset per ID; a second bake changes nothing; bounds within 1 cm of the contract; editing a JSON hot-reloads the runtime block; a stale bake fails the cook | Editor | M1-08 | not started |
| M1-10 | Resolver `UPWAssetResolverSubsystem` with its tiers, the error cube, `APWAssetActor`, the hard-reference validator rule, coverage report v0 | Game/AssetResolverSubsystem, AssetActor; Editor coverage commandlet | Test map: every ID resolves; an unknown ID shows the error cube and logs once; `pw.Assets.ForceTier` switches tiers without changing collision; a deliberate hard reference fails validation | PIE | M1-09 | not started |
| M1-11 | Skeleton freeze and block player: `skeleton.humanoid@1`, parts from `appearance_mapping_v1.json` for all 96 appearance combinations, placeholder animation (AP 2), first-person arms, behind a character-display interface | Game/BlockCharacterComponent, CharacterDisplay; Data/Presentation | All 96 combinations build; idle, walk and sit animate; no gameplay code refers to a mesh | PIE | M1-10, AP 2 | not started |
| M1-12 | Performance budgets confirmed on the target hardware (R4); first cost proxies; p95 frame time recorded with blocks and with `pw.Assets.CostProxy 1` on a synthetic load map | `ASSET_PIPELINE.md` §4 numbers; Content/Maps/MAP_Test_Load | Cooked Development build measured on the target hardware; numbers recorded with a capture procedure someone else can repeat | Cooked +owner | M1-10, R4 | not started |
| M1-13 | Controller in both cameras: Enhanced Input for keyboard/mouse and controller; first person with no cosmetic head motion and a stable horizon; third person with orbit, collision and adjustable distance; a toggle between them | Game/PlayerCharacter, PlayerController; input assets | Walk, look and collide in both modes; 20 toggles leave position and facing unchanged; controller alone works; the owner judges the feel | PIE +owner | M1-11 | not started |
| M1-14 | Comfort and accessibility: FOV, sensitivity, third-person distance, motion blur, roll, shake, subtitles, text scale, remapping, hold or toggle. All player-facing text goes through string tables | Game settings and UI; Config | Every option changes behaviour and survives a relaunch; settings work with a controller alone; no literal player-facing strings in new code | PIE | M1-13 | not started |
| M1-15 | World entry: world game mode, `MAP_Nabapara`, game mode set per map rather than globally. Confirm and in-world loads open the world. Pause menu (Resume, Save, Settings, Quit to Title). Slot rows show name, age, date and location | Game/WorldGameMode, PauseMenu; Config/DefaultEngine.ini; Content/Maps | Confirm spawns the player at the bed. Save from pause, quit, load: same place (within 1 cm) and same camera mode | PIE | M1-05, M1-13 | not started |
| M1-16 | The clock in the world, with a minimal time-mode controller (free roam and paused only; the full contract is M3-01) | Core/TimeModes; Game/SimulationSubsystem, HUD clock | 60 real seconds add 60 game minutes (±1); 30 seconds paused add nothing; code review finds no actor tick changing world state | PIE | M1-15 | not started |
| M1-17 | Contextual interaction and targeting: forgiving selection, fixed priority among overlapping objects, one rule set for both cameras. The verbs open, sit, pick up, use, read and talk each submit a command | Game/InteractionComponent; Core interaction commands | In an overlap test room, priority is identical in both modes over 50 tries; a door's state survives save/load | PIE | M1-10, M1-16 | not started |
| M1-18 | Hands and contact for the gate: seats, reach envelopes, the grips the gate needs, held-item sockets, picking up and putting down a small item, using one tool on a fixture | Game; Data/Assets anchors | In both modes: sit at the desk, pick up a pen, use the tool; the camera stays clear of furniture while seated; the owner accepts the contact | PIE +owner | M1-17 | not started |
| M1-19 | Flat reading view and a phone shell (time and date): screen-aligned, scalable, keyboard and controller navigable; the clock pauses while it is open | Game/PhoneView, ReadingView | Readable at 100–200% text scale; no perspective distortion; the clock does not move while open | PIE | M1-17 | not started |
| M1-20 | Speak to someone: one household adult (a guardian or care worker, depending on origin). Speaker shown, subtitles, framing the player can leave, clock paused | Game/Conversation; Data/Dialogue | In both modes the exchange plays, can be left at any line, and records a choice that survives save/load | PIE | M1-17, M1-11 | not started |
| M1-21 | One furnished block home (the `compact_family_flat` kit) at realistic furniture density, with ceilings, as a layout | Data/Layouts | Audit 0; every room walked in both modes without camera clipping; within budgets | PIE | M1-18, M1-12, R3 | not started |
| M1-22 | The Nabapara lane from the home door to the Nabapara Vidyalaya gate: one crossing, the gate as an interaction point, a route preview | Data/Layouts; navmesh bounds | The walk takes 1–3 real minutes; no stuck spots in either mode; audit 0; within budgets; the owner accepts the layout | PIE +owner | M1-21 | not started |
| M1-23 | **3D creation and title (replaces the deleted flat front end):** New Game / Continue / Load / Settings / Quit, then the block character in an orbit camera with every appearance picker and slider, household and traits, and Confirm, all driving `UPWCharacterCreatorSubsystem` | Game/FrontEnd, CreationStage; Content/Maps/MAP_FrontEnd | Every picker and slider change shows within one frame; 1,000 points and the rank-4 cap enforced by the core; save, quit, load returns to the same resume point; keyboard, mouse and controller alone each work | PIE | M1-11, M1-13 | not started |
| M1-24 | M1 gate run on a cooked build | STATUS.md | In both modes: sit, read, pick up a small item, use the tool, speak to the household adult, pass through the furnished home, switch camera. Bed to gate walked. Save and resume on the street. Audit 0; budgets recorded with blocks and proxies | Cooked +owner | M1-01 to M1-23 | not started |

---

## 5a. World-scale lane (owner decision 2026-09-28)

**Goal:** the machinery for many real cities, fictional places and a Deep Realm ten times Earth's surface (`WORLD_SCALE.md`). The portable steps start now and never wait for the editor. **Playable result:** walk out of Nabapara into a generated Kolkata district, and step through a test Gate onto streamed realm terrain.

| ID | Deliverable | Touches | Pass condition | Rung | Deps | Status |
|---|---|---|---|---|---|---|
| M1-25 | Realm coordinates: floor index, 64-bit chunk coordinate plus local offset, arithmetic, distance, sector of a chunk, and per-floor extents (floor 1 ≥ 10 × 510.1 million km²; deeper floors smaller) | Core/RealmCoordinate | CTest: normalisation round trips at the far corners; distance symmetric and exact across sector boundaries; floor 1's area is at least 5.101 × 10⁹ km²; positions outside a floor are refused; no float ever holds a whole-realm position | CTest | W4 | done 2026-09-28 · 772,551 checks; Rung 1 3/3 |
| M1-26 | Counter-based realm generator: terrain height, region theme and danger rank (F–S) for any chunk from the realm seed alone | Core/RealmGenerator | CTest: golden values for a fixed seed; generating chunks in any order gives identical results; neighbouring chunk edges match; it never touches the life RNG streams; 10,000 chunks generate within the documented time | CTest | M1-25 | not started |
| M1-27 | Chunk delta store: sparse, versioned edits keyed by chunk coordinate, with a cap and revert rule | Core/ChunkDeltaStore | CTest: round trip; an unchanged chunk stores nothing; every truncation refused; the cap reverts the documented deltas | CTest | M1-25 | not started |
| M1-28 | Realm streamer: instanced block chunks built on worker threads in a radius around the player, with re-origin at sector boundaries | Game/RealmStreamer; Content/Maps/MAP_Test_Realm | PIE: walk 5 km in one direction; no hitch above the frame budget; the player's precision stays within 1 cm after 100 re-origins; collision matches the generated data | PIE | M1-26, M1-13 | not started |
| M1-29 | City data and district archetypes: city records (stable ID, nation, districts) and a generated-district builder for at least three archetypes | Core/CityData, DistrictGenerator; Data/Cities | CTest: the validator accepts Kolkata and rejects a dangling nation or duplicate ID; a district regenerates identically from its seed; streets connect with no dead cells | CTest | M1-06 | not started |
| M1-30 | Nabapara as the authored hero district, plus one generated neighbouring Kolkata district, streamed together | Data/Layouts; Data/Cities/kolkata.json; Game/DistrictStreamer | PIE: walk from the school gate into the generated district and back; audit 0; within budgets | PIE +owner | M1-22, M1-28, M1-29 | not started |
| M1-31 | Synthetic-load measurement for the streamers against the budgets | Content/Maps/MAP_Test_Load; `WORLD_SCALE.md` §5 numbers | Numbers recorded on the target hardware with a repeatable capture procedure | Cooked +owner | M1-28, M1-30, R4 | not started |

## 5b. India, stage one (owner instruction 2026-09-28)

**Goal:** India's first five cities, their Gates and dungeon anchors, the education ladder, social strata, resources, and awakened grades, defined as data and portable rules before any of it is built in 3D (`INDIA_STAGE_ONE.md`, `AWAKENED_GRADES.md`).

| ID | Deliverable | Touches | Pass condition | Rung | Deps | Status |
|---|---|---|---|---|---|---|
| M1-32 | India stage-one data: nation, five cities with authored and generated districts and Gate sites, realm anchors, institutions, education stages, school and university types, strata, resources | Data/India/*.json | Every file parses; every ID is unique; every reference resolves; each city has exactly one realm anchor listed by the nation. Formal check moves into the M1-06 validator | CTest (after M1-06) | W1–W4 | done 2026-09-28 as data · 71 IDs, 71 references resolved by a one-off script; the M1-06 validator will make it a CTest |
| M1-33 | Awakened grades F–S, stages 1–10, six power dimensions, practice with daily and weekly ceilings, assessment-only promotion, rest decay | Core/AwakenedProgression | CTest: requirement table climbs; awakening never exceeds F; daily limit and weekly ceiling hold; stage 10 does not promote; every blocking reason reported; grades never decay; maximum-effort S takes over 2.5 game years | CTest | — | done 2026-09-28 · 71 checks; maximum-effort S at day 1,079 |
| M1-34 | Awakening turns the tutorial-dungeon conduct and latent stats into the starting profile (affinity and six ratings) | Core/AwakenedProgression, a new awakening reader | CTest: the same life gives the same profile; conduct moves the profile within documented bounds | CTest | M1-33, M4-01 | not started |
| M1-35 | Gate sites in the world state: onset schedule, rank, status, containment by the NGA | Core/GateSites; Data/India | CTest: onsets fire from the clock exactly once; status changes only through commands | CTest | M1-01, M1-32 | not started |

---

## 6. M2 — One school day

**Goal:** condition, learning, schedules, identity and receipts in the portable core; all four interaction structures prototyped; one full weekday lived in the world. **Gate (GAME_PLAN §8):** *"A full day saved and resumed mid-day. The forty-exposure classroom test passes with abbreviation available."* **Playable result:** wake, a household obligation, the walk to school, two played classes and the rest resolved, a free-block activity, evening, sleep, the day's receipt.

| ID | Deliverable | Touches | Pass condition | Rung | Deps | Status |
|---|---|---|---|---|---|---|
| M2-01 | Condition: energy, hunger, health and stress, with bounded effects. Ordinary neglect never kills | Core/Condition | CTest: 30 days without food or sleep leaves health at or above its floor and the state valid; sleep and meals recover as documented | CTest | M1-05 | not started |
| M2-02 | Latent stats: six attributes and a skill catalogue as data, with starting values from traits | Core/LatentStats; Data/Skills | CTest: Balanced Scholar starts at the documented values; stream and appearance variants are identical; the validator refuses any religion, language, stream or family multiplier | CTest | M1-06, M0-18 | not started |
| M2-03 | Activities: declared duration, clock mode, place, condition cost, outcome, learning allowance split, limited attribute evidence; start, complete and cancel commands | Core/Activities; Data/Activities | CTest: completion applies exactly once, including across a save/load mid-activity; allowance splits sum to one | CTest | M2-01, M2-02 | not started |
| M2-04 | Practice limits: a rolling 24-hour window per practice family (full, ½, ⅒, then nothing), a per-skill budget, 3-in-7 consolidation, and a forecast label | Core/PracticeWindow | CTest: a fourth session within 24 hours teaches nothing; route variants share the limit; a missed day does not reset consolidation; the forecast equals the actual credit | CTest | M2-03 | not started |
| M2-05 | Persistent identity registry and event history: a stable identity at first consequential reference, never changed; an append-only event log recording causes and parties | Core/IdentityRegistry, EventLog | CTest: identities survive saves unchanged; referencing an unmet cohort member creates exactly one identity, deterministically; the log is replay-stable | CTest | M1-03 | not started |
| M2-06 | Schedules and minimal off-screen progress: time ranges, zones, travel and priority exceptions, resolved at event boundaries | Core/Schedules; Data/Schedules | CTest: jumping 16 hours equals stepping boundary by boundary (same hash); unseen classmates still get their scheduled practice | CTest | M2-03, M2-05 | not started |
| M2-07 | Nabapara Vidyalaya bell schedule, one weekday per stream, attendance and lateness | Core/School; Data/School | Validator: no overlaps and a full day for each stream. Arriving after the bell marks the student late | CTest | M2-06 | not started |
| M2-08 | "Model a system" structure with routine abbreviation, and its classroom family: at least 8 compatible cases, varying the unknown, the role and the scarce resource | Core/StructureModel; Data/Problems | CTest: every case can be completed; a correct committed answer is never refused for a low stat; costs show before commit; a secure family resolves by routine at full normal value | CTest | M2-04 | not started |
| M2-09 | The other three structures (explain, investigate, coordinate), with one small family each (3+ cases) | Core/StructureExplain, StructureInvestigate, StructureCoordinate; Data/Problems | The M2-08 properties hold for each structure | CTest | M2-08 | not started |
| M2-10 | Relationships and remembered events (minimal) | Core/Relationships | CTest: a first meaningful act can change a relationship, repeating it cannot; remembered events survive saves | CTest | M2-05 | not started |
| M2-11 | Opportunity receipts: each commitment records the known alternatives it displaced | Core/Receipts | CTest: never lists an alternative the player was not shown; the day's totals match the command log | CTest | M2-03 | not started |
| M2-12 | Day loop and recovery checkpoints: plan, commitments, free blocks, evening, sleep, receipt. An automatic checkpoint at each wake, separate from life slots. Slot summary gains current commitment and last action | Core/DayLoop, SaveStore | CTest: a scripted day ends in the expected state; saving at every command boundary and resuming gives the same final hash; a checkpoint never overwrites a life slot | CTest | M2-06 to M2-11 | not started |
| M2-13 | School interior: gate, corridor, one classroom, yard (the classroom entry in the block interior test suite) | Data/Assets, Data/Layouts | Audit 0; walkable in both modes; within budgets | PIE | M1-22 | not started |
| M2-14 | A teacher and two classmates built from `AppearanceRecord`: expressions, visemes, look-at, identity kits, the full grip set (`ASSET_PIPELINE.md` §12, M2 row), placed by schedule | Game/Npc, FaceComponent; Data/People, Data/Presentation | At 10:00 each is where the schedule says; contact scenarios pass at the proportion extremes; the owner can tell each one apart without a nameplate | PIE +owner | M2-06, M2-13 | not started |
| M2-15 | Structure scene UI: the four structures under shared controls. At most two foreground periods a day; the rest resolve by a chosen focus; routine abbreviation offered | Game/SceneUI | A third foreground period is refused with a reason; each structure plays at a desk in both modes; results reach the receipt | PIE | M2-09, M2-14 | not started |
| M2-16 | Home and free-block activities: study at the desk, the origin's household obligation, one neighbourhood activity, each with its forecast shown before commit | Game; Data/Activities | Each completes once, costs what its data says, and appears in the receipt | PIE | M2-12, M1-18 | not started |
| M2-17 | Phone v1: plan, receipt, messages | Game/PhoneView | Plan and receipt equal the portable state; usable with a controller alone | PIE | M2-12, M1-19 | not started |
| M2-18 | Conversation v1 with a classmate: a remembered choice and a declared clock cost | Game/Conversation; Data/Dialogue | The classmate refers to the choice later that day and after a reload; the owner edits the lines | PIE +owner | M2-10, M2-14 | not started |
| M2-19 | Review screen previews a representative day and concrete trade-offs from the stream timetable and household obligations | Game/ReviewPanel; Core query | Each of the three streams and the care origin shows a different, accurate day | PIE | M2-07, M0-21 | not started |
| M2-20 | Repetition harness and session log: runs exposures of one structure back to back across simulated days, with abbreviation available, logging decisions, voluntary foreground choices and comprehension answers | Game harness mode; Scripts/summarise_sessions.py | A 40-exposure run produces a complete log that the script summarises | PIE | M2-15 | not started |
| M2-21 | Four-structure distinctness check | STATUS.md | The owner, plus any available tester, plays each structure three times and can state how they differ; findings labelled as GAME_PLAN §8 requires | PIE +owner | M2-15 | not started |
| M2-22 | Content matrix and playtime budget started (GAME_PLAN §7) | Data/Matrix/*.csv | The matrix lists every family, role and consequence built so far; the budget uses times measured from the logs | CTest | M2-20 | not started |
| M2-23 | **Forty-exposure classroom test** (repetition gate, structure 1 of 4) | STATUS.md | R5 thresholds met for changing decisions, voluntary foreground participation and comprehension, with abbreviation available | PIE +owner | M2-20, M2-21, R5 | not started |
| M2-24 | M2 gate run on a cooked build | STATUS.md | Wake to sleep on one weekday; save at noon, quit, resume; receipt correct; M2-23 passed | Cooked +owner | M2-01 to M2-23 | not started |

---

## 7. M3 — The Class XI year

**Goal:** the time contract and week compression proven on a short period first, then the whole year: calendar, projection, threads, six origins, a thin fifty-nation layer, and an early probe of adult work. **Gate (GAME_PLAN §8):** *"A short period played manually and compressed yields equivalent state and understanding, then thirty-four weeks. Adult-task probe: one established adult work week built on the shared structures."* **Playable result:** Monday 2 June 2042, 06:00 to the morning of Friday 23 January 2043, in any of six origins.

| ID | Deliverable | Touches | Pass condition | Rung | Deps | Status |
|---|---|---|---|---|---|---|
| M3-01 | **Time contract:** all six clock modes (free roam, conversation, scene, phone and menus, accessibility pause, mission). Every interaction declares its mode, cost, interruption policy and resolution boundary | Core/TimeModes; validator; Game wiring | CTest: a conversation never exceeds its declared cost; the accessibility pause uses no opportunity time; the mission clock caps how fast the outside world advances; content without declarations fails validation. PIE: phone, menus and reading pause the clock | PIE | M2-12 | not started |
| M3-02 | Week compression under the equivalence rule, plus a persistent-state diff tool | Core/WeekResolver; Tools/StateDiff | CTest: the same plan played as scenes and resolved as a routine week gives the same persistent state; at most one interrupt; firsts never auto-resolve; every choice made on the player's behalf is disclosed | CTest | M3-01 | not started |
| M3-03 | Short-period equivalence test: two weeks played manually, then the same plan compressed | STATUS.md | The state diff shows no undisclosed difference; the owner and one tester answer the same comprehension questions equally well | PIE +owner | M3-02 | not started |
| M3-04 | Repetition gate, structures 2–4: forty exposures each | STATUS.md | R5 thresholds met for each; a structure that fails is redesigned before M3-08 | PIE +owner | M2-23, M3-02 | not started |
| M3-05 | Aptitude Projection: evidence per path, naming what has not been observed, with no number the player can farm | Core/Projection | CTest: study-heavy and repair-heavy 8-week personas diverge; a group activity narrows uncertainty without improving outcomes; deterministic | CTest | M2-04 | not started |
| M3-06 | Threads: participants, escalation clock, triggers, expressions; six templates for the year | Core/Threads; Data/Threads | CTest: an untouched thread escalates and resolves on schedule; player action changes its course; save round trip | CTest | M2-10 | not started |
| M3-07 | Assessments: surveying the paper (18 units, five groups) and diagnostic marking across four kinds of failure | Core/Assessment | CTest: a prepared character clears and checks every group; an unprepared one must triage | CTest | M2-09 | not started |
| M3-08 | Problem families expanded to at least 20 (heading for ~40 by the slice), each entered in the content matrix | Data/Problems, Data/Matrix | Validator passes; every case completable; the matrix shows no family repeating another's decision structure and role; the owner accepts each | CTest +owner | M3-04 | not started |
| M3-09 | 2042–43 calendar: terms, assessments, sports day, Independence Day, Durga Puja, Kali Puja and Diwali, winter, Saraswati Puja. Every date flagged unverified | Data/Calendar | Validator: every date is valid and in range; school is closed on declared holidays | CTest | M1-06 | not started |
| M3-10 | Thin fifty-nation model: the `GLOBAL_WORLD.md` §8 roster's IDs and names, a quarterly Country Level, 3–8 notables per nation as persistent identities, a few rankings | Core/Nations; Data/Nations | Validator passes and refuses any nation ID outside the roster; publication fires quarterly on the clock; notables keep their identity across saves | CTest | M2-05, §8 roster confirmed | not started · the owner confirms the roster first |
| M3-11 | News and rankings on the phone; one or two authored hazards whose onset is fixed and whose consequences record their causes | Game/PhoneView; Data/Events | Items appear on schedule; one incident closes a route and reaches the receipts | PIE | M3-10, M2-17, §12 E | not started |
| M3-12 | Named cast grown to ~12 (classmates, household or care staff, teachers, neighbours), with schedules, relationships, threads and off-screen weeks | Data/People | Validator passes; every person has a full-week schedule; the owner edits every draft; identity test: 7 of 8 testers recognise cast members at 12 m (`ASSET_PIPELINE.md` §12) | PIE +owner | M3-06, M2-14 | not started |
| M3-13 | Six origins: three family shells × five templates × four tiers, plus the dormitory; household obligations per origin; presentation-bible rules for poverty and care | Data/Layouts, Data/Assets | Confirming each of the six outcomes spawns the right shell and dressing; audit 0; owner's dignity review | PIE +owner | M1-21, M2-16, R3, R9 | not started |
| M3-14 | Projection terminal: queueing costs time, and the printout is kept on the phone | Game | Two visits a month apart print differently after a changed life | PIE | M3-05, M2-13 | not started |
| M3-15 | Week planner, weekly receipt, scene-day nomination, dropping into any day, and a "since you last played" recap | Game/PhoneView | Plan a week and skip it, read its receipt, drop into a Wednesday; the recap appears after a simulated week away | PIE | M3-02, M2-17 | not started |
| M3-16 | The calendar in the world: festival dressing (block assets through cultural review), closures, weather and night lighting | Data/Assets, Data/Layouts; Game | Durga Puja week shows its dressing and the closure; audit 0 | PIE | M3-09 | not started |
| M3-17 | Year arcs: a friendship that changes, a practical project that matters, a household obligation redesigned | Data; Game | Each arc can be reached and resolved both in a headless run and in play | PIE +owner | M3-06, M3-12 | not started |
| M3-18 | **Adult-task probe:** one established adult work week (the accounts clerk in `ALTERNATE_WORLD.md` §3) built on the shared structures, in a block office, from a separate probe save | Data/Problems, Data/Layouts | The owner and one tester recognise the shared verbs and call it work, not a minigame; findings labelled | PIE +owner | M3-04 | not started |
| M3-19 | Year soak: three scripted personas from 2 Jun 2042 06:00 to 23 Jan 2043 06:00, saved and reloaded every week | Tests/YearSoakTests.cpp | CTest (long-running label): no invalid state; replay hashes identical; ends at the exact final instant; the personas' projections differ | CTest | M3-02 to M3-07 | not started |
| M3-20 | Cultural review of the calendar, festivals, homes, depiction of care, and names | Data flags | Every unverified flag is cleared or corrected by the reviewer | +owner | M3-09, M3-13, M3-16, M3-17, R8 | not started |
| M3-21 | M3 gate run on a cooked build | STATUS.md | M3-03 and M3-18 recorded; the owner lives all 34 weeks and can point to projection changes they caused | Cooked +owner | M3-01 to M3-20 | not started |

---

## 8. M4 — Awakening and examination

**Goal:** the vertical slice, from Awakening Day through the ten days and three tower floors to results that lead somewhere worth playing. **Gate (GAME_PLAN §8):** *"Vertical slice. ≥6 of 8 blind testers explain their own awakening and a sacrificed obligation; each tower route is independently discovered; non-combat routes are substantive; a poor result leads somewhere worth playing."* If the test fails, redesign the projection and scoring, not the content (`MASTER_PLAN.md` §3). **Playable result:** the slice from the chosen start (R6) to the first week after results.

| ID | Deliverable | Touches | Pass condition | Rung | Deps | Status |
|---|---|---|---|---|---|---|
| M4-01 | Conduct ledger: bounded evidence from the dungeon's conduct tests | Core/Conduct | CTest: every recorded action maps to evidence; there is no free-text verdict | CTest | M2-05 | not started |
| M4-02 | Awakening: a Tattva distribution, a capability profile and one derived ability from authored recipes, each with the evidence behind it | Core/Awakening; Data/Abilities | CTest: deterministic; changing only conduct can change the result; stream, household and appearance change nothing; every Tattva can be reached by at least 2 non-combat personas | CTest | M4-01, M3-05 | not started |
| M4-03 | The ten days, from the evening of Awakening Day to 06:00 on exam morning: opportunities shaped by relationships, uncertain but disclosed outcomes, negotiable obligations, one world event | Core/Countdown; Data | CTest: mutually exclusive opportunities are enforced; a reload never rerolls; three scripted strategies finish with different but comparable profiles | CTest | M3-02, M1-02 | not started |
| M4-04 | Tower route matrix and scoring: the mission clock, defeat costing rank, a credential portfolio, national rank against the cohort, offers after any result | Core/Tower; Data/Tower | CTest: the matrix covers every discipline and every non-combat path; an all-non-combat route can reach the top decile; the bottom decile still gets at least 3 actionable offers | CTest | M3-01, M3-05 | not started |
| M4-05 | Combat spike: one tool grammar, evade, a contextual action, one derived ability; custom component versus GAS decided in writing | Game test map; `SYSTEMS.md` decision | The owner judges the feel; the decision is recorded | PIE +owner | M1-13 | not started |
| M4-06 | Tutorial dungeon, built from the Gate kit and its reserved palette ramps, with the player's own classmates and three conduct tests. It can be neither failed nor refused (§12 A). Withdrawal to the safe area is conduct, and a student absent on the day is awakened at a supervised make-up session | Data/Assets, Data/Layouts; Game | Every combination of choices, including withdrawal and absence, ends in awakening; the ledger matches the choices made; audit 0 | PIE | M4-01, M4-05, M3-12, §12 A | not started |
| M4-07 | Awakening scene and profile screen, explaining what conduct contributed without passing a verdict | Game | The values shown equal the portable output; the evidence is named; the owner accepts the tone | PIE +owner | M4-02, M4-06 | not started |
| M4-08 | The ten days in play: a countdown on the phone, preparation in the world, obligations, the news event and its route consequence | Game | Every one of the ten days can be played or planned; receipts show what was sacrificed | PIE | M4-03, M3-15 | not started |
| M4-09 | Tower shell in its own material family: entry, the mission clock, ejection, saves that survive interruption mid-climb | Game; Core/Tower; Data/Assets | Save and quit mid-floor, then resume at the same point; ejection leads to results | PIE | M4-04 | not started |
| M4-10 | Floor 1 (combat and endurance) with three legitimate routes | Data/Layouts; Game | Each route can be completed in both modes and is scored as M4-04 defines | PIE | M4-09 | not started |
| M4-11 | Floor 2 (analysis and construction: a familiar system with one rule changed) with three routes | Data/Layouts; Game | As M4-10 | PIE | M4-09 | not started |
| M4-12 | Floor 3 (negotiation and logistics) with three routes | Data/Layouts; Game | As M4-10 | PIE | M4-09 | not started |
| M4-13 | Results: public rank, credentials, offers and alternatives | Game; Data | Top, middle and bottom results each lead to a playable first week; the owner judges that the bottom one is worth playing | PIE +owner | M4-10 to M4-12 | not started |
| M4-14 | Opting out: deferring or declining the examination, and climbing by non-combat routes only, each lead somewhere worth playing | Core; Data | Each path plays through to the week after results | PIE | M4-13, §12 A | not started |
| M4-15 | Import route A MVP with fixtures, plus the provenance schema and validator (`ASSET_PIPELINE.md` §6, §8, §12) | Editor `-run=PWImportArt`; Scripts/import_art.py; Tests/Fixtures | The fixtures import, fail and report exactly as specified; nothing is left in staging | Editor | M1-10 | not started |
| M4-16 | Slice start (R6) and four checkpoints: awakening morning, the first countdown evening, exam morning, results | Core/SaveStore; Game | Each checkpoint loads into its phase in a cooked build | Cooked | M4-13, R6 | not started |
| M4-17 | Test kit: a local session log (choices, routes, time, sacrifices; no network) and an interview script | Game; Scripts | The log reconstructs a session's key decisions | PIE | M4-08 | not started |
| M4-18 | Performance and stability: M1-12 budgets hold on the benchmark route, in the dungeon and in the tower, with blocks and with proxies; a two-hour run without a crash | STATUS.md | Cooked measurements within budget | Cooked +owner | M4-16 | not started |
| M4-19 | Internal playthroughs: combat-led, non-combat only, and a deliberately poor result | STATUS.md | The owner completes all three, and every blocking issue is fixed | Cooked +owner | M4-17, M4-18 | not started |
| M4-20 | **Blind test**, eight testers | STATUS.md | At least 6 explain their own awakening; at least 6 name an obligation they sacrificed; every tower route is found independently; non-combat routes are judged substantive; poor results still carry on | Cooked +owner | M4-19 | not started |
| M4-21 | Decision: a pass moves on to M5; a failure means redesigning the projection and scoring | `GAME_PLAN.md` §10 | Decision row recorded | +owner | M4-20 | not started |

---

## 9. M5–M10

Full step tables are in [`ROADMAP_LATER.md`](ROADMAP_LATER.md). Gates are copied from GAME_PLAN §8.

| # | Goal | Gate | Steps |
|---|---|---|---|
| M5 | Adult life, plus a skeletal later life and epilogue | *A non-hunter lives 16 → 30 and holds up; an established worker's week holds up; a whole life runs end to end* | 16 |
| M6 | The living nation | *Demotion, re-encounter, cohort replacement and causal attribution hold across decades* | 10 |
| M7 | The world | *The player's standing moves India's rating understandably — and a player with negligible influence still feels the world* | 9 |
| M8 | Breadth | *Complete lives with fresh testers across the scenario matrix: stable ordinary life, homemaking and care, late career switch, hybrid career, summit pursuit, recovery after serious failure* | 10 |
| M9 | Art integration | *Three separate tests: asset coverage and provenance; no unintended change to rules, saves, collision contracts or outcomes; final-presentation acceptance for readability, contact, comfort, performance, accessibility and tone. Includes a scheduled presentation-adjustment period* | 8 |
| M10 | Ship | *Final certification. Performance, accessibility, localisation support and cooked builds began at their earlier milestones* | 7 |

---

## 10. What to do next

1. **M0-12:** confirm both rungs are green on HEAD, and have the owner review and commit the planning documents.
2. **M0-13:** the owner runs smoke-test checks S1–S32 in a launched window and records every result.
3. **M1-01:** an agent builds the command reducer at the same time as step 2. It is portable, needs no UI, and is the first step on the critical path.
4. **M0-14:** fix each defect M0-13 found, one commit each, including Delete on Load Game.
5. **M0-15:** write the packaging script and make the first cooked Windows build. That reaches the M0 gate.

Right after these: M0-18 (schema v5, once R2 is decided) and M1-02 on the portable lane, and M0-16 on the front-end lane. **The owner should answer R4 and AP 2–5 during M0**, because M1-08 to M1-12 need them.
