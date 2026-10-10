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

## Checkpoint — Unreal Engine 5.8 and the Unreal MCP server, 2026-09-25

M0-12a, M0-12b and M0-12c are done.

| Check | Result |
|---|---|
| Unreal Engine 5.8 installed (owner, Epic Launcher) | PASS: `UE_5.8/Engine/Build/BatchFiles/Build.bat` and `UnrealEditor.exe` present |
| VS 2026 Build Tools 18.10.2 (owner) | PASS: MSVC 14.50 (compiler 14.50.35739) and 14.51.36231 installed beside VS 2022 17.14 |
| Editor build on 5.8 | PASS: `PerfectWorldEditor Win64 Development`, 22/22 actions, no warnings, MSVC 14.50.35739 (Epic's preferred toolset) |
| Portable suite | PASS: 2/2 test executables (`PerfectWorldCoreTests`, `PerfectWorldClockTests`) |
| Editor opens the project on 5.8 | PASS: launched, and the front end ran in Play In Editor (the Appearance screen was on screen). This is not the M0-13 smoke test |
| Unreal MCP server | PASS: listening on `127.0.0.1:8000` only; `initialize`, `tools/list` and a read-only `list_toolsets` call succeeded; `claude mcp list` shows `unreal` connected |

**What changed.** `EngineAssociation` is 5.8. Both targets moved from `BuildSettingsVersion.V6` to `V7`, because under V6 the editor target changes warning levels it shares with the installed `UnrealEditor` and the build tool refuses it. The `ModelContextProtocol` and `AllToolsets` plugins are enabled for the Editor target only. `Config/DefaultEditorPerProjectUserSettings.ini` auto-starts the server on port 8000.

**Known issue.** `AllToolsets` pulls in `GameFeaturesToolset`, and at startup the editor logs *"Asset Manager settings do not include an entry for assets of type GameFeatureData"* under `LoadErrors`. It is harmless to this project, which uses no game features. The fix is either an asset-manager rule in `DefaultGame.ini` or an explicit toolset list instead of `AllToolsets`; this is for the owner to choose.

**Portability, for a second machine such as a cloud PC.** `Scripts/build_core.py` no longer hardcodes the Visual Studio 2022 generator, so a fresh build folder uses the newest Visual Studio installed. Verified: a fresh folder generated for VS 2026 (MSBuild 18.10) passes 2/2, and the existing VS 2022 folder still passes 2/2. `.mcp.json` registers the `unreal` server for any clone. `BUILD_GUIDE.md` gains a new-machine checklist.

**Not verified.** No M0-13 smoke check has been run. The six built features remain unverified in a launched window, apart from the Appearance screen being visible.

## Checkpoint — cloud PC set up; M0-12 baseline, 2026-09-25 to 2026-09-28

New machine: a Windows 11 cloud PC with an RTX-class GPU (DirectX 12), 19–22 GB RAM, an 80 GB C: and a 420 GB D:. Because C: is small, the repository is at `D:\Dev\PerfectWorld`, Unreal 5.8.3 at `D:\Epic Games\UE_5.8` (launcher at `D:\Epic Games\Launcher`), VS 2026 Build Tools 18.10 at `D:\VS\BuildTools`, and gh, Python 3.13 and CMake under `D:\Tools`.

| Check | Result |
|---|---|
| Rung 1: `python Scripts/build_core.py --build-dir D:/pwcore` | PASS: 2/2 executables; 225,395 creation checks (GAME_PLAN §11 had recorded 225,374) and 16,857 clock checks. Generator Visual Studio 18 2026, MSVC 19.51.36260 |
| Rung 2: `python Scripts/build_unreal.py --engine-root "D:\Epic Games\UE_5.8"` on HEAD `a6b0ac4` | PASS on the second run: exit 0, `Result: Succeeded`, 22/22 actions. The first run failed with `Result: Failed (RulesError)`: "Could not find NetFxSDK install dir". Fixed by adding `Microsoft.Net.Component.4.8.1.SDK` and `.TargetingPack` to the Build Tools |
| Compiler against `BannedVisualCppVersions` | PASS: the build tool used MSVC **14.50.35739** (from the `14.50.35717` folder). Banned for 14.50 is `14.50.0–14.50.35722`; preferred is `14.50.35717–14.50.99999` |
| Warnings | One: "Unable to find Visual Studio SDK. Editor integration will be disabled", which only affects Visual Studio IDE integration |

**Environment findings, now in `BUILD_GUIDE.md`:** the .NET Framework SDK is required; winget `--location` is ignored by the Git and Epic Launcher installers (the launcher needs `msiexec ... INSTALLDIR=`); Epic's install helper quits with `ih_early_exit` when the disk stalls for more than about 45 s, which happened while Steam was downloading to the same disk; the final phase of a 5.8 install (moving and hashing 269,489 files) takes about 15 minutes with no network traffic.

## Checkpoint — M0-13a: the flat front end removed; world scale planned, 2026-09-28

**Intent:** Remove the flat Slate front end, the 2D concept art and the superseded documents on the owner's instruction, keep the portable rules and their Unreal bridge, and record the new world-scale direction.

**Files changed:** deleted `Source/PerfectWorld/{Public,Private}/PerfectWorld{AppearancePanel,CreatorPanel,MenuGameMode}.{h,cpp}`, `Art/kolkata-nabapara.png`, `Art/student-lineup.png`, and `Docs/{ART_DIRECTION,GAME_DESIGN,NEXT_SMALL_STEPS,PRODUCTION_PLAN,REVIEW_DECISIONS,SONNET_DESIGN_REVIEW,TECHNICAL_ARCHITECTURE,MENU_SMOKE_TEST,UNREAL_QUICKSTART}.md`. Changed `Config/DefaultEngine.ini` (no global menu game mode), `Source/PerfectWorld/PerfectWorld.Build.cs` (no Slate), `README.md`, and in `Docs/`: `GAME_PLAN.md` (§3 generated-terrain exception, §4, §6, §7, §8 M0 gate, §10 two decisions, §11), `ROADMAP.md` (M0 rewritten, M0-13a added, front-end steps dropped, M1-04 and M1-23 revised, new §5a world-scale lane M1-25 to M1-31), `BUILD_GUIDE.md`, `SYSTEMS_CATALOGUE.md`, `ASSET_PIPELINE.md`, `CHARACTER_CREATION.md`, `AWAKENING_AND_EXAM.md`, `MASTER_PLAN.md`, `DEVELOPMENT_WORKFLOW.md`, `ART_PROVENANCE.md`. Added `Docs/WORLD_SCALE.md`.

**Exact check:** Rung 1 as above: PASS, 2/2, 225,395 + 16,857 checks. Rung 2 as above after the removal: PASS, exit 0, `Result: Succeeded`, 7/7 actions. A search of `Docs/`, `README.md`, `AGENTS.md`, `Source/` and `Config/` finds no live reference to a removed file (history rows in `ROADMAP.md` and this file keep their original wording).

**Result:** PASS at Rung 2. The owner's decisions are logged in `GAME_PLAN.md` §10. The world-scale build approach in `WORLD_SCALE.md` is Claude's proposal; the GPT-6 critique `AGENTS.md` asks for was **not run**, because Codex is not installed on the cloud PC.

**Remaining limitation:** nothing is on screen: Play In Editor shows an empty Entry map until the 3D front end (M1-23) exists. The deleted screens were never verified in a launched window. Owner questions W1–W4 (`WORLD_SCALE.md` §7) are open.

**Next smallest action:** connect the Unreal MCP server on this machine (the editor must be running), then start the portable lanes: M1-01 (command reducer) and M1-25 (realm coordinates), neither of which needs the editor.

## Checkpoint — W1–W4 working decisions; M1-25 realm coordinates; Unreal MCP over HTTP, 2026-09-28

**World-scale decisions.** The owner delegated W1–W4 ("do it yourself"). Working decisions, recorded in `WORLD_SCALE.md` §7 and open to the owner's override: Kolkata, then Delhi, Mumbai, Bengaluru and Chennai, then twelve foreign cities; no OpenStreetMap import yet; one shared Deep Realm with a home region per nation; stacked floors, floor 1 alone at least ten times Earth's surface.

**Unreal MCP.** The editor started from `D:\Epic Games\UE_5.8` and, after about 7.5 minutes of first-launch shader compilation, answered an MCP `initialize` on `127.0.0.1:8000` (bound to localhost only). `tools/list` returned `list_toolsets`, `describe_toolset` and `call_tool`; read-only calls to `list_toolsets`, `EditorAppToolset.IsPIERunning` (false) and `LogsToolset.GetLogEntries` succeeded over plain JSON-RPC. The Claude session's own `unreal` tools stayed unloaded: the session started before the editor, and only the owner can reconnect it with `/mcp`. The editor later shut down cleanly on its own (log ends in a normal `Exiting.`, no crash), which stopped the listener; it was relaunched. The startup `LoadErrors` line about GameFeatureData appeared again, as recorded above. `claude mcp list` was not run: the Claude Code CLI is not installed on the cloud PC.

**M1-25 — realm coordinates.**

**Intent:** a Deep Realm position that is exact everywhere on a floor ten times Earth's surface, with integers only.

**Files changed:** added `Source/PerfectWorldCore/Public/RealmCoordinate.h`, `Source/PerfectWorldCore/Private/RealmCoordinate.cpp`, `Tests/RealmCoordinateTests.cpp`; `CMakeLists.txt` registers them and the new `PerfectWorldRealmTests` executable.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwcore`.

**Result:** PASS, 3/3 test executables: 225,395 creation checks, 16,857 clock checks and **772,551 realm checks**. The first run failed on a wrong constant in the test itself (71,434.24 km written as 71,434,240,000,000 mm instead of 71,434,240,000 mm); the code was right. Floor 1 is 2,180 sectors of 32.768 km, 71,434.24 km a side, about 5.103 billion km², which is at least ten times Earth. Twelve floors, each a quarter of the one above, the deepest one sector.

**Remaining limitation:** Rung 2 was not run for this step, because the editor was open and locks the module DLLs. The core module compiles into Unreal as well, so the next editor build must include it. Nothing uses realm coordinates yet: the generator (M1-26) and delta store (M1-27) come next.

**Next smallest action:** M1-26, the counter-based realm generator, in the portable lane.

## Checkpoint — India stage one defined; M1-32 data and M1-33 awakened grades, 2026-09-28

The owner asked for India, its cities and dungeon, schools of every level and universities, resources, social classes and an awakening grade and levelling system, "properly". The session's own Unreal MCP tools are now connected (`mcp__unreal__call_tool` answered `IsPIERunning`: false).

**Intent:** define India's first stage as design, data and portable rules before building it in 3D.

**Files changed:** added `Docs/INDIA_STAGE_ONE.md`, `Docs/AWAKENED_GRADES.md`, `Data/India/{nation,cities,institutions,society}.json`, `Source/PerfectWorldCore/Public/AwakenedProgression.h`, `Source/PerfectWorldCore/Private/AwakenedProgression.cpp`, `Tests/AwakenedProgressionTests.cpp`; changed `CMakeLists.txt`, `Docs/ROADMAP.md` (new §5b, M1-32 to M1-35), `Docs/GAME_PLAN.md` (document table, decision log).

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwcore`, and a one-off Python check over `Data/India/` (parse, unique IDs, references, one realm anchor per city).

**Result:** PASS. CTest 4/4: 225,395 creation, 16,857 clock, 772,551 realm and 71 awakened-progression checks. The whole-career simulation reaches S at game day 1,079 at maximum effort (four hours on every dimension every day, every assessment passed at first chance). Two test mistakes were found and fixed on the way (a luxury session that hit the weekly ceiling, and a cramming loop that hit the daily limit first); one code fix: recovery doubles the gain after the weekly ceiling, not before. Data: 4 files, 71 unique IDs, 71 references resolved.

**Remaining limitation:** Rung 2 was not run: the editor is open for MCP work and locks the module DLLs, so `RealmCoordinate` and `AwakenedProgression` have not yet compiled inside Unreal. The data check is a one-off script, not a CTest, until the M1-06 validator exists. Every institution name is fictional but unchecked against real institutions; caste and religion are deliberately not mechanics; all of it awaits the cultural review. Nothing is visible in 3D yet. The GPT-6 critique did not run (Codex not installed).

**Next smallest action:** close the editor once for a Rung 2 build of the two new core files; then M1-06 (content validator over `Data/`), M1-26 (realm generator) and M1-01 (command reducer).

## Checkpoint — M1-06 content registry and validator; India data moved to Data/Content, 2026-09-28

The owner authorised pushing to GitHub for this session ("push to github and continue"); `5971618` was pushed to `origin/main` before this step.

**Intent:** one C++ validator for all content, run by CTest over the real data and over deliberately broken fixtures, so the India data is checked on every build instead of by a one-off script.

**Files changed:** added `Source/PerfectWorldCore/Public/{Json,ContentRegistry}.h`, `Source/PerfectWorldCore/Private/{Json,ContentRegistry,ContentSchemas}.cpp`, `Source/PerfectWorldCore/Private/ContentSchemas.h`, `Tools/ContentCheck/main.cpp` (`pwcontent`), `Tests/ContentRegistryTests.cpp`, `Tests/Fixtures/Content/` (8 cases); moved `Data/India/*.json` to 63 files under `Data/Content/<type>/<id>.json`, the layout `SYSTEMS.md` §5 prescribes (IDs of education stages, school types and university tiers gained type prefixes); changed `CMakeLists.txt`, `.gitattributes` (`*.json` and `*.txt` as LF), `Docs/BUILD_GUIDE.md`, `Docs/INDIA_STAGE_ONE.md`, `Docs/ROADMAP.md` (M1-06 and M1-32 done).

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwcore`; then `pwcontent validate Data/Content` and each fixture by hand to read the messages.

**Result:** PASS, 14/14 CTest entries: the four earlier executables unchanged (225,395 + 16,857 + 772,551 + 71 checks), 37 content-registry unit checks, the real data (63 files, 99 records: nation 1, realm region 1, city 5, institution 23, education stage 10, school type 5, university tier 5, stratum 5, resource 8, and nested district 26, Gate site 10), and eight fixtures each failing for their own reason. Every issue names its file and ID. A fixture checked against the wrong expected reason makes the test fail, as it should.

**Remaining limitation:** the schemas are written in C++ (`ContentSchemas.cpp`) because `SYSTEMS_DATA.md`, which `SYSTEMS.md` names as the schema authority, was never written; the C++ tables are the working schema until it is. The validator does not yet run at game boot or as the editor commandlet (`SYSTEMS.md` §5); it has not been compiled inside Unreal (the editor is open for MCP). No bundle for cooked builds yet.

**Next smallest action:** M1-26 (realm generator), then M1-01 (command reducer); a Rung 2 build at the next convenient editor restart.

## Checkpoint — M1-26 realm generator, 2026-09-28

**Intent:** any Deep Realm chunk's terrain, theme, danger and landmark from the realm seed alone, identical on every machine and in any order.

**Files changed:** added `Source/PerfectWorldCore/Public/RealmGenerator.h`, `Source/PerfectWorldCore/Private/RealmGenerator.cpp`, `Tests/RealmGeneratorTests.cpp`; changed `CMakeLists.txt`, `Docs/WORLD_SCALE.md` §4.3 (havens, danger bands, terrain), `Docs/ROADMAP.md`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwcore`; golden values captured once with `PW_PRINT_GOLDEN=1` and pinned.

**Result:** PASS, 15/15 CTest entries; 20,240 generator checks: golden height at two columns and a fingerprint over six chunks on floors 1, 3 and 12; any generation order gives identical chunks; chunk edges equal the global height field and the step across a boundary is no larger than steps inside; heights within ±560 m everywhere; region (0, 0) is a haven on every floor, its centre is F on floor 1 and E on floor 3, floor 12 is S; floor 1 spans at least four ranks and regions at least five themes; landmarks about one per sixteen 8 × 8 cells, towers only at C or above; chunks off a floor refused. **10,000 chunks in 1.73 s** (limit 5 s). One design fix before the first build: depth now scales to six ranks so the deepest floor is S (one rank per two floors had left floor 12 at A).

**Remaining limitation:** terrain is height only: no caves, water, surface materials or theme-specific shapes yet; nations' Gate anchors are not yet placed in havens (M1-35 and the realm-anchor data); not compiled inside Unreal yet (editor open for MCP); nothing streams or renders (M1-28).

**Next smallest action:** M1-01, the command reducer, so the world state has its single path of change; then M1-27 (chunk deltas).

## Checkpoint — M1-01 command reducer; Claude Code CLI updated, 2026-09-29

**Tooling.** The owner installed the Claude Code CLI (`C:\Users\Administrator\.local\bin\claude.exe`, not on PATH). On the owner's request it was updated with its own updater: `claude update`, exit 0, **2.1.282 → 2.1.284**. The desktop app (Claude 2.9939.2.0, a Microsoft Store package) carries its own Claude Code 2.1.281, which the app manages and updates itself.

**Intent:** one mutation entry point for the world: validate (pure), apply, advance time event by event, commit.

**Files changed:** added `Source/PerfectWorldCore/Public/{Commands,WorldState,WorldReducer}.h`, `Source/PerfectWorldCore/Private/WorldReducer.cpp`, `Tests/WorldReducerTests.cpp`; changed `CMakeLists.txt`, `Docs/ROADMAP.md`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwcore`.

**Result:** PASS, 16/16 CTest entries; **69 reducer checks**: eleven invalid commands each refused with its stable code (`time.non_positive`, `time.too_long`, `schedule.source`, `schedule.bad_id`, `schedule.in_past`, `flag.bad_id`) and the state byte-for-byte unchanged; a three-day advance fires every scheduled event once, in time order, with each midnight's `DayStarted` before an event at that same instant; an event due at the starting instant precedes the midnight after it (a bug found by review before the first build and fixed); the event cap stops an advance as `Partial` and resubmission fires every remaining event exactly once; flags emit a change only when the value changes; 3,000 random commands, valid and invalid, replay to an identical transcript and state. The first run of this step was cut off with no output when the session ended; this rerun is the recorded result.

**Remaining limitation:** only three commands exist; no state hash or replay check (M1-03), no RNG streams (M1-02), no save of the world (M1-05). Not yet compiled inside Unreal (Rung 2 follows now that the editor is closed).

**Next smallest action:** Rung 2 for all core files added since 2026-09-28; M1-02, M1-27, M1-29 and M1-04's portable store in parallel.

## Checkpoint — Rung 2 for every core file since 2026-09-28; core module built without unity, 2026-09-29

**Intent:** compile the seven core files added since the last editor build (realm coordinates, awakened progression, JSON, content registry and schemas, realm generator, world reducer) inside Unreal, and stop Unreal's unity build from merging core files.

**Files changed:** `Source/PerfectWorldCore/PerfectWorldCore.Build.cs` (`bUseUnity = false`). A scan found no duplicate file-private names across `Source/PerfectWorldCore/Private/*.cpp`, but `ContentRegistry.cpp` has `using namespace ContentSchema;` at file scope, which inside a unity blob would leak into every later file; compiling each file alone keeps the Unreal build identical to the CMake build that CTest checks.

**Exact check:** `python Scripts/build_unreal.py --engine-root "D:\Epic Games\UE_5.8"` with the editor closed.

**Result:** PASS: exit 0, `Result: Succeeded`, 10/10 actions (seven compiles, two links, metadata), MSVC 14.50.35739.

**Remaining limitation:** nothing new is exercised at runtime in Unreal; the subsystem still uses only the creation rules.

**Parallel agents.** On the owner's instruction, six Sonnet subagents (the Agent tool's `sonnet` alias) work in separate git worktrees with separate build folders and explicit file ownership: M1-02 RNG streams, M1-03 state hash and replay, M1-04 portable save store, M1-27 chunk deltas, M1-29 city data and district generator, M1-35 Gate sites in the world state. They do not commit; each diff is reviewed, merged and re-verified here. A Claude Code restart on 2026-09-29 stopped the first four before they had written anything; they were resumed from their transcripts.

**Next smallest action:** review and merge each agent's diff; M0-15 packaging script.

## Checkpoint — M0-15 in progress: block test scene and packaging script, 2026-09-29

**Intent:** a first packaged Windows build that boots into a block test scene, before real maps exist.

**Files changed:** added `Source/PerfectWorld/{Public,Private}/PerfectWorldBlockTestGameMode.{h,cpp}` (a temporary game mode that builds an 81 × 81 field of one-metre engine cubes with block towers, a sky atmosphere, a sun and a real-time sky light at start; the engine's default flying pawn), `Scripts/package_windows.py` (RunUAT BuildCookRun, Development, `-ubtargs=-NoUBA`, log in `Saved/Logs/Package.log`); changed `Config/DefaultEngine.ini` (`GlobalDefaultGameMode` set to the test mode until M1-15; the editor's Android-settings churn was discarded) and `Config/DefaultGame.ini` (always cook `/Engine/BasicShapes`).

**Exact check:** `python Scripts/build_unreal.py --engine-root "D:\Epic Games\UE_5.8"`.

**Result:** Rung 2 PASS: exit 0, `Result: Succeeded`, 5/5 actions. **Rung 3 (PIE) and rung 4 (packaging) NOT RUN**: the owner's usage limit was reached while the editor was starting for an MCP check of the scene.

**Remaining limitation:** the scene has not been seen; `package_windows.py` has never run; the test map is built in code in the engine Entry map rather than as `Content/Maps/MAP_Test_Block`.

**Parallel agents stopped.** The six Sonnet agents (M1-02, M1-03, M1-04, M1-27, M1-29, M1-35) were stopped to stay within the usage limit. Partial work is uncommitted in `.claude/worktrees/agent-*`; nothing from them is merged or verified. Resume each from its transcript, then review, merge and run Rung 1 and Rung 2.

**Next smallest action:** Play In Editor via MCP (StartPIE, CaptureViewport, the "Block test scene" log line); then close the editor and run `python Scripts/package_windows.py --engine-root "D:\Epic Games\UE_5.8"`.

## Checkpoint — D: drive wiped and the cloud PC restored, 2026-10-05

**What happened.** On 2026-10-05 the D: drive came back freshly formatted (only `System Volume Information`, created that day). Lost: the D: clone, Unreal Engine 5.8, the Epic launcher, VS 2026 Build Tools, gh/Python/CMake under `D:\Tools`, and all unpushed work: six agent worktrees (M1-03 and M1-04 were finished and verified at Rung 1, the other four partial) and `Source/PerfectWorld/Private/Tests/PerfectWorldCoreParityTests.cpp` (which had compiled at Rung 2 on 2026-09-30, never run). Everything pushed was safe on GitHub (`33128b0`).

**Restored.** gh and CMake with `winget install --force --location D:\Tools\...`; Python 3.13.15 by repairing from a downloaded installer (uninstall failed with 1603 because its files were gone); VS Build Tools 18.10 by recreating `D:\VS\BuildTools`, `setup.exe repair` (cancelled midway by a concurrent MSI install, leaving `tracker.exe` missing) and `setup.exe resume` (exit 0; MSVC 14.50.35717 and 14.51.36231, .NET 4.8.1 SDK confirmed with `vswhere -requires`). GitHub sign-in survived in the Windows credential store. Fresh clone at `D:\Dev\PerfectWorld`; the C: copy fast-forwarded to `33128b0`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwb` (the old `D:/pwcore` folder held a failed compiler detection and is protected from deletion). **Result:** PASS, 16/16, MSVC 19.51.36260.

**Remaining limitation:** Unreal Engine 5.8 is not installed; the owner must reinstall it from the Epic launcher to `D:\Epic Games\UE_5.8`, so Rung 2, PIE and packaging are unavailable until then. The parity tests are re-added unchanged and unbuilt. The six agent steps are being rebuilt by fresh Sonnet 5.5 agents (the API recorded `claude-sonnet-5-5` for this session's subagents). D: may be temporary storage: push every verified step immediately.

## Checkpoint — M1-03 state hash and M1-04 portable save store merged, 2026-10-05

Both rebuilt by Sonnet 5.5 subagents after the D: wipe (the first attempts had passed but were lost), reviewed and merged here. Integration now happens in the C: clone, which survives a D: wipe; build folders stay on D:.

**Intent:** M1-03, a stable world-state hash and command-log replay; M1-04 (portable half), a crash-safe slot store.

**Files changed:** added `Source/PerfectWorldCore/{Public,Private}/WorldHash.{h,cpp}`, `Tests/WorldHashTests.cpp`, `Source/PerfectWorldCore/{Public,Private}/SaveStore.{h,cpp}`, `Tests/SaveStoreTests.cpp`; `CMakeLists.txt`; `Docs/ROADMAP.md`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`.

**Result:** PASS, 18/18. `PASS: 1156337 world hash checks` (golden hashes for a hand-built and an edge-case world, an in-test reference encoder checked against FNV-1a vectors, every single-field edit changes the hash and only its section's hash, 10,000 random sequences replay to identical hashes and results). `PASS: 527 save store checks` (stale temp ignored, corrupt primary falls back to `.bak` with a message, Missing and Corrupt told apart, crash-between-renames state recovers, non-ASCII folder, 100 round trips).

**Remaining limitation:** the hash covers meta, clock and flags; RNG streams (M1-02) and Gate sites (M1-35) must be added as sections when they merge (one line each plus a `WorldHashVersion` bump). The character-creator subsystem still writes slots itself; switching it to `WriteSaveFile`/`ReadSaveFile` needs Rung 2, which waits for Unreal Engine to be reinstalled. Neither file has been compiled inside Unreal.

## Checkpoint — M1-02 RNG streams, M1-27 chunk deltas, M1-35 Gate sites merged; world hash version 2, 2026-10-05

Rebuilt by Sonnet 5.5 subagents after the D: wipe, reviewed and merged here.

**Intent:** in-house deterministic RNG streams (M1-02); sparse, capped, versioned Deep Realm edits (M1-27); Gate sites in the world state with onsets from the clock and NGA containment (M1-35).

**Files changed:** added `Source/PerfectWorldCore/{Public,Private}/{RandomStreams,ChunkDeltaStore,GateSites}.{h,cpp}` and their tests; changed `WorldState.h` (Gate slice and five Gate events), `Commands.h` (four Gate commands), `WorldReducer.cpp` (forwarding and "gates/" routing), `WorldHash.{h,cpp}` (gates section, `WorldHashVersion` 2), `Tests/WorldHashTests.cpp` (reference encoder and re-pinned goldens), `Tests/GateSitesTests.cpp` (hash covers every Gate field), `CMakeLists.txt`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`. **Result:** PASS, 21/21: 1,156,474 hash, 79,559 Gate, 206,396 RNG, 2,247 chunk-delta checks; the 69 reducer checks unchanged.

**Trap found:** `Copy-Item` keeps a file's original modified time, so files copied from a worktree can be older than an existing object file and MSBuild silently links the stale object. The first merged build failed that way ("register emits GateRegistered"); touching the merged files fixed it. Merge by copying, then refresh timestamps, before building.

**Design question for the owner (M1-35):** an onset that fires at a Gate sealed after it was scheduled currently leaves the Gate unchanged and emits `GateOnsetHeld` ("the seal held"). That lets a seal prevent an onset, which `GAME_PLAN.md` §2 says cannot be prevented. The alternative is to count the onset without changing the sealed status.

**Remaining limitation:** RNG streams are not yet part of `WorldState` or the hash; nothing generates onsets from content schedules yet; none of this has been compiled inside Unreal (the engine must be reinstalled).

## Checkpoint — M1-29 city data and district generator merged, 2026-10-05

Rebuilt by a Sonnet 5.5 subagent after the D: wipe, reviewed and merged here.

**Intent:** typed city records read from validated content, and a seeded 64×64 (8 m cell) district builder.

**Files changed:** added `Source/PerfectWorldCore/{Public,Private}/{CityData,DistrictGenerator}.{h,cpp}`, `Tests/DistrictGeneratorTests.cpp`; `CMakeLists.txt` (the test is given `Data/Content`); `Docs/ROADMAP.md`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`. **Result:** PASS, 22/22. `PASS: 9753194 district generator checks`: every cell is street, building or open; one 4-connected street network reaching all four edges; every building touches a street; the same archetype and seed regenerate identically; archetypes differ measurably; the five real cities in `Data/Content/city` all read, Kolkata included, and all their generated districts hold the same guarantees. The validator's dangling-reference and duplicate-ID rejections are the existing `bad_dangling_ref` and `bad_duplicate_id` fixtures.

**Merge note:** the agent's `GateStatus` enum in `CityData.h` clashed with `WorldState.h`'s `GateStatus` (M1-35), so it is renamed `GateSiteStatus` (`GateSiteStatusName`). The content records the authored status of a site; the world state tracks its live status.

**Remaining limitation:** only three layouts are real; riverfront, rail yard, port, campus, industrial, suburb, government quarter and informal settlement borrow one. Nothing streams a district into Unreal yet (M1-30), and none of this has been compiled inside Unreal.

## Checkpoint — Unreal 5.8.3 reinstalled, Rung 2 and engine parity green, M1-04 switch-over, 2026-10-05

**Intent:** re-verify the Unreal side after the D: wipe, then finish M1-04 by moving the character-creator subsystem onto the portable slot store.

**Files changed (commit `faa42fb`):** `Source/PerfectWorld/Private/PerfectWorldCharacterCreatorSubsystem.cpp` (save, load, describe and delete go through `WriteSaveFile`, `ReadSaveFile` and `DeleteSaveFiles` with a validator that parses the creation record; absolute paths via `FPaths::ConvertRelativePathToFull`; a slot whose primary is gone but whose `.bak` survives counts as occupied and reports the recovery); `Source/PerfectWorld/Private/Tests/PerfectWorldCoreParityTests.cpp` (new `PerfectWorld.Core.SaveStoreWorksInEngine`).

**Exact checks:**
- `python Scripts/build_unreal.py --engine-root "D:\Epic Games\UE_5.8"` (run in the D: clone) against engine 5.8.3, CL 58210709, with the MSVC 14.50.35739 toolchain, above the banned 14.50.0–14.50.35722 range: **Succeeded** before and after the change, no compiler warnings. That run included the M1-29 sources, so CityData and DistrictGenerator now compile inside Unreal too.
- Editor opened, Unreal MCP `AutomationTestToolset`: `DiscoverTests`, then `RunTestsByFilter "StartsWith:PerfectWorld.Core"`: **4/4 Success** (`RealmGeneratorMatchesCTest`, `AwakenedCareerMatchesCTest`, `ContentLoadsInEngine`, `SaveStoreWorksInEngine`). The new test writes twice under a Bengali folder name in `Intermediate/`, checks the `.bak`, refuses an invalid record without touching the slot, recovers from a corrupted primary, and deletes.

**Rung reached:** Rung 2 plus editor automation tests. The creator flow itself has not been driven in PIE; it is a flat-front-end subsystem awaiting the 3D creation scene (M1-23).

**Remaining limitation:** M1-05 will change the slot payload to the v6 world save, and the subsystem's validator must then accept v6 records.

## Checkpoint — every district archetype has its own layout (M1-29 follow-up), 2026-10-05

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** `DistrictGenerator.{h,cpp}` (shared helpers moved to `PerfectWorld::DistrictInternal`, a common finishing pass that keeps the street network connected and demotes lots touching no street); new `Private/DistrictLayouts.{h,cpp}` (riverfront, port, rail yard, dispatcher) and `Private/DistrictLayoutsLand.cpp` (campus, industrial, suburb, government quarter, informal settlement, park); `Tests/DistrictGeneratorTests.cpp` and new `Tests/DistrictTestSupport.h`; `CMakeLists.txt`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`. **Result:** PASS, 22/22, `PASS: 37039388 district generator checks`. Every archetype is measured over 200 seeds for its own storey range and a property that tells it apart. Some examples: a 6 to 11 cell river band with promenade, ghat steps and a bridge; a sea at least 800 cells with pier tips; 6 to 8 rail lines with level crossings; a campus quad bigger than any market square; suburb cul-de-sacs; a government boulevard; informal-settlement dead-end lanes. Water appears only in riverfront and port, rail only in rail yards. The real `Data/Content` districts are each built by their own layout, with no fallback left.

**Note:** the shared finishing pass can change the output of the three earlier layouts for a given seed. Nothing saved depends on district output yet. Once a save stores generated-district edits, layout changes need a version field.

## Checkpoint — M1-05 world save v6 (portable core), RNG in the world hash (version 3), 2026-10-05

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/WorldSave.h`, `Private/WorldSave.cpp` (writer, envelope, migration), `Private/WorldSaveReader.cpp`, `Private/WorldSaveInternal.h`, `Tests/WorldSaveTests.cpp`, `Tests/WorldSaveTestSupport.h`, `Tests/WorldHashReference.h` (the reference encoder and goldens, moved out of `WorldHashTests.cpp` to keep it under 500 lines). Changed: `WorldState.h` (`RandomStreams::Snapshot Rng` and `MakeWorldState(seed)`), `WorldReducer.cpp` (the sim starts from `MakeWorldState`), `WorldHash.{h,cpp}` (rng section, `WorldHashVersion` 3), `Tests/WorldHashTests.cpp` (goldens re-pinned to `0x094455d520abf770` and `0xf0312d8420e90da4` from the reference encoder; the agent also re-derived them in a separate Python implementation), `Tests/GateSitesTests.cpp` (five sections), `CMakeLists.txt`.

**Format:** one JSON document: `format "perfectworld.save"`, `schemaVersion 6`, the existing creation object verbatim, `world` (seed, seq, clock with pending onsets, flags, Gates, the four RNG streams), `player` (zone, `posCm`, `yawCdeg`, `pitchCdeg`, camera mode), `hashVersion`, `stateHash`. 64-bit values are `0x` hex strings, there are no floats, every key is required and unknown keys are refused. The loader recomputes the hash and refuses a mismatch. A file without `format` is a v1–v4 creation save. It migrates to a world at 2 Jun 2042 06:00 in `zone.home.<home kit>`, with the seed derived from the file's bytes, so migrating the same file twice gives the same world.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`. **Result:** PASS, 23/23: 19,498 world save, 1,156,739 world hash, 79,559 Gate and 69 reducer checks. The tests cover:
- round trips, including an edge world with INT64 limits and odd bytes in IDs
- every proper prefix of three saves refused
- v1–v4 fixtures migrating
- about 25 targeted corruptions and over 3,000 single-byte flips, each refused or hash-preserving
- 300 random command sequences whose loaded, live and replayed hashes agree
- RNG streams that continue identically after a reload

**Open questions for the owner, raised by the agent:**
1. `ROADMAP.md` makes the world save v6 with v5 from M0-18, while `SYSTEMS.md` §6 calls v5 the first world envelope. The roadmap was followed. `SaveSchemaVersion` is still 4, so no v5 file exists yet.
2. The player's zone, transform and camera are saved beside the world state but are not hashed.
3. An unstarted draft migrates into a world. It could be refused instead.

**Remaining limitation:**
- `WorldSim` cannot yet be constructed from a loaded `WorldState`; M1-15 needs that.
- The subsystem still writes creation-only slots.
- The new files have not been compiled inside Unreal; the next Rung 2 covers them.

## Checkpoint — M2-01 condition module, 2026-10-05

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/Condition.h`, `Private/Condition.cpp`, `Tests/ConditionTests.cpp`; `CMakeLists.txt`.

**Design:**
- Four meters on 0..100000, the catalogue's milli scale: energy and health are high-is-good, hunger and stress high-is-bad.
- One pure `StepCondition` per game minute, and `AdvanceCondition` for N minutes, which stops at a fixed point.
- Meals in four sizes, `AdjustStress`, and activities Sleeping, Resting, LightExertion and HeavyExertion.
- Bounded effects as permille multipliers, never below 500.
- Neglect drains health only down to the floor of 20,000 (`SYSTEMS_CATALOGUE.md`) and flags a care scene. There is no death path, since death comes only from later-life ageing.
- Rates, start values, thresholds and bands not given by the docs are named constants in `Condition.h`; each is marked as chosen.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`. **Result:** PASS, 24/24, `ConditionTests passed (909857 checks)`. The tests cover:
- 30 days awake unfed, stepped by the minute, by the hour and as one jump, with identical results and health exactly at the floor
- 30 days asleep unfed, and 30 days of heavy exertion
- documented recovery rates
- 10,000 random sequences, with every meter in range and health never below the floor
- additivity and determinism

**Remaining limitation:**
- Not yet part of `WorldState`, the reducer or the world hash.
- The catalogue's injuries, `LastIntegratedAt`, and threshold events predicted to the minute are for the integration step.
- Trait recovery bonuses from `CHARACTER_CREATION.md` are not applied yet; they may belong in `Data/` tuning.

## Checkpoint — M0-15 first cooked Windows build (Rung 4), 2026-10-05

**Intent:** run the packaging script for the first time and boot the result.

**Files changed:** `Config/DefaultGame.ini` (an editor-only `GameFeatureData` asset-manager rule); `Docs/DEVELOPMENT_WORKFLOW.md` (the Rung 4 command).

**What happened:**
1. The first run cooked all 503 packages but exited 25 (`Error_UnknownCookFailure`). The Unreal MCP plugin enables GameFeatures in the editor, and GameFeatures logs an error when the asset manager has no `GameFeatureData` rule. An error in the cook log fails the cook.
2. Adding the usual rule fixed the cook, but the packaged game then hit a handled ensure at startup. The game has no GameFeatures module, so the base class failed to load, and symbolicating the callstack added about 16 s to boot.
3. Marking the rule `bIsEditorOnly=True` fixed both.

**Exact check:** `python Scripts/package_windows.py --engine-root "D:\Epic Games\UE_5.8" --output D:\PWBuilds` (in the D: clone, editor closed): **exit 0**, `BUILD SUCCESSFUL`, 902 MB. Then `D:\PWBuilds\Windows\PerfectWorld.exe -windowed`:
- The window "PerfectWorld (64-bit Development PCD3D_SM5)" appeared within 15 s.
- The log shows `Game class is 'PerfectWorldBlockTestGameMode'` and no `Error:` or ensure.
- A screenshot showed the block columns on the grey floor under a sky.
- Closing the window gave `RequestExit(0)` and `LogExit: Exiting.`, with no crash reporter.

**Rung reached:** Rung 4, by boot and clean exit. **Owner check still open:** moving the mouse to look around in the packaged build, and Alt+F4. The scene loads through `/Engine/Maps/Entry` with the test game mode set globally, not a `MAP_Test_Block` asset; M1-15 moves the game mode per map.


## Checkpoint — M1-07 asset registry `pw.asset/1`, 2026-10-05

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New `Public/AssetRegistry.h` and `Private/{AssetInternal.h, AssetRegistry.cpp, AssetReader.cpp, AssetContractReader.cpp, AssetGeometry.cpp, AssetGameplay.cpp}`, all under 500 lines.
- `Tests/AssetRegistryTests.cpp`.
- `Tools/ContentCheck/main.cpp`: a new `pwcontent assets <dir>` subcommand; `validate` is unchanged.
- `Data/Contracts/*.json` (14) and `Data/Assets/<category>/<subcategory>/*.json` (45).
- `Tests/Fixtures/Assets/*` (six bad fixtures).
- `CMakeLists.txt`.

**Content:**
- Contracts for walls, slabs, doors, windows, trim, gates, ground surfaces, beds, seats, desks, storage, fixtures, handheld items and tools.
- Assets:
  - a compact Kolkata flat's shell
  - furniture (a taktaposh, an almirah)
  - street pieces (lane, zebra crossing, pavement, drain cover, lamp post, post box, tea stall)
  - small items (phone, textbooks, a clay bhar cup, tiffin box)
  - one tool (an adjustable spanner)
  - the school gate
  - a Gate-rift frame placeholder

All are voxel-block definitions, not meshes.

**Rules enforced:**
- ID format, and the path must match the ID.
- No drawing-method words in IDs.
- Unknown fields and duplicates refused.
- Integer millimetres only.
- Dimensions within the contract; doors exactly 2100 mm.
- Every cuboid, collision box, anchor, socket and seat point inside the outer envelope; stances and seat exits outside it.
- Seats and camera clearance declared for both first and third person.
- Held items need a grip socket.
- Palette ramps only, with Gate ramps only in `gatekit`.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`. **Result:** PASS, 32/32. `AssetRegistryTests: 316 checks passed`; `pwcontent assets Data`: `PASS: 59 files, 14 contracts, 45 assets`. Each of the six fixtures (anchor outside, missing clearance, door height, unknown family, duplicate ID, unknown field) fails for its stated reason.

**Choices where `ASSET_PIPELINE.md` was silent (owner may revise):**
- The doc's example uses centimetres with decimals; fields here are `*_mm` integers.
- `bottom_center` is the only pivot, so X and Y dimensions must be even.
- The cuboid union must equal the declared dimensions exactly.
- Stance and exit points are floor points outside the asset.
- Contracts carry limits, not overridable defaults.
- Pose, grip and palette-ramp lists are hard-coded until M1-08 defines the tile list.

**Remaining limitation:** nothing in Unreal reads the registry yet; that is M1-09 and M1-10.

## Checkpoint — M0-18 identity and stream, creation schema v5, 2026-10-05

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New `Public/CreationIdentity.h`, `Private/CreationIdentity.cpp` (streams, validators, setters, migration), `Private/CreationIdentityText.cpp` (v5 reading and writing), `Private/CreationText.h` (the shared JSON-subset reader) and `Tests/CreationIdentityTests.cpp`.
- `CharacterCreation.{h,cpp}`.
- `CreationSaveSerialization.cpp`: rewritten from 564 to 363 lines.
- `Tests/WorldSaveTests.cpp`: a v5 fixture and v1–v5 migration.
- The mirror enums in `PerfectWorldCharacterCreatorSubsystem.h`: five new errors before `FileAccess`, and resume points `Identity`, `Stream` and `Review` appended.
- `CMakeLists.txt`.

**Rules:**
- **Age:** the player is 16 on 2 Jun 2042, so dates of birth run from 3 Jun 2025 to 2 Jun 2026 inclusive. Nonexistent dates are refused, never normalised.
- **Pronouns and languages:** free text with byte limits, valid UTF-8 and no control characters. There is no closed list until GAME_PLAN question G is decided.
- **Stream:** `science`, `commerce` or `humanities`, plus an optional reason of up to 240 bytes. The stream changes no cost, rank or home plan, and the test asserts this.
- **Migration:** records older than v5 take the R2 working defaults with `identityMigrated` set. `ConfirmMigratedIdentity` is the one identity edit allowed after life starts.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwi`. **Result:** PASS, 33/33: 1,825 identity, 225,395 creation (10,000 edit sequences), 20,036 world save and 1,156,739 world hash checks, with the goldens unchanged.

**Open questions:**
1. R2 itself.
2. Whether a new draft should resume at `identity` instead of `appearance`.
3. Given name, family name and home city still have no length limits.
4. The subsystem's `SetIdentity` builds a profile without a date of birth, so calling it resets the date to the default. The 3D creation scene (M1-23) needs full identity setters.

## Checkpoint — D: wiped again; toolchain restored; M2-05 identity registry and event log, 2026-10-10

**D: wipe (second, found 2026-10-10):** D: came back empty. GitHub and the C: clone were both at `0d47fa5`, so no committed work was lost. Agent worktrees live on C: and survived.

Restored:
- gh 2.102.0 to `D:\Tools\GitHubCLI` and CMake 4.4.4 to `D:\Tools\CMake`, both from the official GitHub release zips at the paths the machine PATH and git's credential helper expect. gh's sign-in survived in the Windows keyring.
- VS Build Tools 18.10 at `D:\VS\BuildTools`: recreated the folder, then `setup.exe repair` on the stale instance (exit 0). MSVC 14.50.35717 and 14.51.36231 are present, and `Tracker.exe` is back.
- The D: clone, re-cloned at `0d47fa5`. Rung 1 there passes 33/33 with MSVC 19.51.
- Unreal 5.8.3: the launcher, now on C:, still recorded it at `D:\Epic Games\UE_5.8`. A launcher verify re-downloads it to the same D: folder, staging in `D:\Epic Games\UE_5.8\.egstore`.

**M2-05 (built by a Sonnet 5.5 subagent, reviewed and merged here):**
- **Files:** new `Public/{IdentityRegistry,EventLog}.h`, `Private/{IdentityRegistry,EventLog}.cpp`, `Private/IdentityText.h`, `Tests/{IdentityRegistryTests,EventLogTests}.cpp` and `Tests/IdentityTestSupport.h`; `CMakeLists.txt`.
- **Identities:** a generated person is `p_` plus hex16 of a keyed draw on (world seed, cohort token, slot), so the ID does not depend on the order people are referenced. The first reference creates exactly one identity; later references change nothing. A collision is refused.
- **Event log:** append-only (sequence, time, type token, cause sequence, up to 8 parties), with a running FNV-1a chain hash.
- **Formats:** both have strict versioned line formats that refuse truncation, non-canonical numbers and a hash that does not match.
- **Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 34/34, `PerfectWorldIdentityTests passed, 25300 checks`.
- **Remaining:**
  - Not in `WorldState`, the world hash or the save.
  - The catalogue's names, date of birth, nation and facts on an identity are left for crystallisation.
  - Event payloads are not decided.
  - The parse caps (about 1M identities, 16M events) are looser than the catalogue's budget of 25,000 people.

## Checkpoint — M2-02 latent stats and skill catalogue, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New `Public/LatentStats.h`, `Private/LatentStats.cpp`, `Private/ContentFairness.cpp` and `Tests/LatentStatsTests.cpp`.
- `ContentSchemas.{h,cpp}`: the `attribute` and `skill` types and an `IntArray` field kind.
- `ContentRegistry.cpp`: the fairness scan runs on every file.
- Data: `Data/Content/attribute/*.json` (6), `Data/Content/skill/*.json` (10).
- Six bad fixtures.
- `CMakeLists.txt`.
- The pinned counts in `PerfectWorldCoreParityTests.cpp`, now 79 files and 115 records.

**Rules:**
- **Attributes:** Body, Agility, Intelligence, Perception, Willpower and Charisma, on the integer milli scale. They start at 10,000 with a maximum of 100,000; both values are placeholders kept in data.
- **Skill starting XP:** a baseline plus the documented trait packages, +10/+25/+45/+70 by rank, stacked additively. Academics has the same baseline of 100 for everyone, so every stream starts equal.
- **Only input:** the 20 trait ranks. Stream, household, appearance, pronouns, languages, date of birth and names are not inputs; the test checks 60 variants for identical results.
- **Fairness:** the validator refuses any key, or modifier table, that ties a modifier to religion, language, stream or family, in every content type.
- **Balanced Scholar:** Academics 145 XP, Computing 25, Practical Repair 35, Household Tasks, Communication and Creative 10 each, all attributes 10,000.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 41/41. `PASS: 911 latent stats checks`; `pwcontent validate Data/Content`: `PASS: 79 files, 115 records`. The six fixtures fail for their reasons (four fairness cases, a seed naming a non-trait, a missing attribute).

**Open questions:**
1. The attribute scale and the Academics baseline are placeholders.
2. No trait seeds an attribute yet; the docs give none.
3. School subjects are folded into one Academics skill.
4. The seeds live on the skill and attribute records, not in a separate `creation/trait_seeds.json`.

**Remaining:** the changed parity test needs Rung 2 and an editor run once Unreal is back.

## Checkpoint — M0-22 the five creation presets, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** `CharacterCreation.{h,cpp}` (`CreationPreset`, `Presets()`, `FindPreset()`; `ApplyPreset` accepts all five); new `Private/CreationPresets.cpp` (the table) and `Tests/CreationPresetTests.cpp`; `CMakeLists.txt`.

**Presets:** the ranks are copied verbatim from `CHARACTER_CREATION.md` "Verified presets", and each totals exactly 1,000 by the cost table.
- `balanced_scholar`
- `resourceful_maker` (rank 4 in practical experience)
- `athletic_organizer` (rank 4 in athletic aptitude)
- `creative_connector`
- `independent_generalist`, using the doc's valid second composition

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 42/42, `PASS: 865 checks`. The tests cover:
- the table matches the doc trait by trait
- each preset totals 1,000, uses ranks 0 to 4, and has at most four rank-4 traits
- the preset list is in doc order
- apply, review, start, save and reload all behave
- unknown, differently-cased and padded IDs are refused
- a preset leaves household, stream, appearance and identity alone

**For the owner:**
1. Only `balanced_scholar` has an ID in the doc; the other four IDs are the headings in snake_case.
2. **Athletic Organizer leaves Household Resources at rank 0**, so under the 2026-09-20 rule that character also grows up in institutional care. The doc's note names only Independent Generalist. Either the note or the preset should change.

## Checkpoint — M2-03 activities, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:**
- New `Public/Activities.h`, `Private/{Activities.cpp, ActivityRunnerText.cpp, ContentActivitySchemas.cpp, ContentSchemaBuilders.h}`.
- `ContentSchemas.{h,cpp}`: the builders moved out to keep files under 500 lines.
- Tests: `Tests/{ActivitiesTests.cpp, ActivitiesContentTests.cpp, ActivitiesTestSupport.h}`.
- Data: `Data/Content/activity/*.json` (7).
- Three bad fixtures.
- `CMakeLists.txt`.
- The pinned counts in `PerfectWorldCoreParityTests.cpp`, now 86 files and 122 records.

**Schema:** an activity declares:
- a duration (1 to 1,440 minutes)
- a clock mode, interrupt and resolve
- a place
- a condition activity, with an optional meal and stress delta
- an outcome
- an optional learning allowance in milli-XP, split by permille across up to four skills (the split must sum to exactly 1,000)
- attribute evidence, capped at one twentieth of the allowance

The seven authored activities are: study at home, attend class, sleep, eat a meal, household chores, a run, and rest.

**Runner:** start, complete and cancel are pure. The caller passes the time and the condition state, and a refusal changes nothing.
- **Completion** charges the full duration's condition cost and emits learning and evidence credits, keyed by an instance number so they apply exactly once.
- **Cancel** charges only the elapsed minutes and grants nothing else.
- **Save format:** `pw.activity_runner/1` text, which refuses truncation and non-canonical input.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 46/46. `PerfectWorld activities: 1659546 checks passed`; `pwcontent validate Data/Content`: `PASS: 86 files, 122 records`. The tests cover:
- save and load mid-activity for all seven activities, matching an uninterrupted run
- repeat completion, completion after cancel, and cancel after completion all refused
- 3,000 random splits summing exactly
- 60 random 400-operation runs that replay identically with and without save/load

**Open questions:**
1. Should learning be scaled by the condition's focus multiplier here or in M2-04?
2. `interrupt` and `resolve` are validated but not yet enforced.
3. Applying credits to stats (with deduplication by instance) belongs to the later stats-state step.
4. The allowance sizes, about 1.5 to 2 XP an hour, are chosen.

**Remaining:** not in `WorldState`, the reducer, the hash or the save. The parity-test change waits for Rung 2.

## Checkpoint — M2-04 practice limits, 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/PracticeWindow.h`, `Private/PracticeWindow.cpp` (491 lines) and `Tests/PracticeWindowTests.cpp`; `CMakeLists.txt`.

**Rules** (from `SCHOOL_AND_PROGRESSION.md` "Anti-grind: the rolling practice window" and `SYSTEMS_CATALOGUE.md`):
- **Window:** each practice family has a rolling 1,440-minute window. Credit is 1000, 500, 100, then 0 permille by position in the window, and a session leaves the window exactly 24 h later.
- **Route variants:** every activity that is a route to one practice shares that family's window. The family ID is passed by the caller.
- **Per-skill budget:** each skill earns at most 8,000 milli-XP per game day (a placeholder for `tuning/growth.json`).
- **Consolidation:** a skill is consolidated when it has 3 useful days within the 7 days ending today. This is a rolling status, so a missed day only ages out of the span and never resets anything. Consolidation grants no XP.
- **Forecast:** the forecast and the recorded credit come from one function, and the labels are "Useful practice", "Consolidation" and "Little new learning".
- **Save format:** `pw.practice/1` text, which refuses truncation.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 47/47, `PracticeWindowTests passed (458434 checks)`. The tests cover:
- the four tiers, and the window boundaries at 1,439, 1,440 and 1,441 minutes
- shared route limits
- the budget, including its reset at midnight
- consolidation edges
- 10,000 random sequences against an independent oracle, with forecast equal to credit and identical results across save and load

**Open questions:**
1. Zero-credit sessions still occupy the window, so spamming a locked window does not unlock it sooner.
2. Consolidation is per skill, not per family.
3. The activity schema needs a practice-family field at integration time.

## Checkpoint — M2-10 relationships and remembered events (minimal), 2026-10-10

Built by a Sonnet 5.5 subagent, reviewed and merged here.

**Files changed:** new `Public/Relationships.h`, `Private/{Relationships.cpp, RelationshipsText.cpp}` and `Tests/{RelationshipsTests.cpp, RelationshipsSaveTests.cpp, RelationshipsTestSupport.h}`; `CMakeLists.txt`.

**Model** (from `SYSTEMS_CATALOGUE.md` "Relationships"):
- **Pairs:** one undirected record per pair, keyed by (low ID, high ID), holding stage, trust, warmth, respect and owed on −100000..100000, a first-acts bitset, and at most 32 memories.
- **Acts:** callers pass an event-log sequence, the two people (both must be parties to that log entry) and an act kind. Deltas come from a fixed placeholder table of nine kinds.
- **First act only:** only the first act of a kind moves the pair, so generosity farming does nothing. Repeats are remembered at quarter salience.
- **Bereavement:** turns the pair into a frozen memorial that is never deleted.
- **Memory:** salience halves every 180 game days. When the pair is full, the least salient memory is evicted, the older one losing a tie.
- **Fairness:** an event type naming religion, caste, language, stream or background is refused.
- **Save format:** `pw_relationships 1` text with an FNV hash, which refuses truncation and corruption.

**Exact check:** `python Scripts/build_core.py --build-dir D:/pwm`. **Result:** PASS, 48/48, `PerfectWorldRelationshipTests passed, 2198923 checks`. The tests cover:
- 10,000 random sequences with every dimension in range
- replay stability, including save and load midway
- deterministic, bounded eviction (300 farmed gifts never displace first-act memories)
- every truncation refused
- the 8,192-pair limit

**Open questions:**
1. The act table is a placeholder until dialogue and activity data define it.
2. Only the Acquaintance and Memorial stages are produced; the others belong to M5.
3. Should a bereavement memory be pinned against decay?
