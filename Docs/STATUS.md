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
